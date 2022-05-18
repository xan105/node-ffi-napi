/**
 * @file Minimal emulation of POSIX dlopen/dlsym/dlclose on Windows.
 * @license Public domain.
 *
 * This code works fine for the common scenario of loading a
 * specific DLL and calling one (or more) functions within it.
 *
 * No attempt is made to emulate POSIX symbol table semantics.
 * The way Windows thinks about dynamic linking is fundamentally
 * different, and there's no way to emulate the useful aspects of
 * POSIX semantics.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <inttypes.h> /* strtoimax */

#include "win32-dlfcn.h"

/**
 * Win32 error code from last failure.
 */

static DWORD lastError = 0;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert UTF-8 string to Windows UNICODE (UCS-2 LE).
 *
 * Caller must free() the returned string.
 */

static
WCHAR*
UTF8toWCHAR(
    const char* inputString /** UTF-8 string. */
)
{
    int outputSize;
    WCHAR* outputString;

    outputSize = MultiByteToWideChar(CP_UTF8, 0, inputString, -1, NULL, 0);

    if (outputSize == 0)
        return NULL;

    outputString = (WCHAR*) malloc(outputSize * sizeof(WCHAR));

    if (outputString == NULL) {
        SetLastError(ERROR_OUTOFMEMORY);
        return NULL;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, inputString, -1, outputString, outputSize) != outputSize)
    {
        free(outputString);
        return NULL;
    }

    return outputString;
}

static 
bool
str_to_uint16(
  const char* str, uint16_t* res
)
{
  char* end;
  errno = 0;
  
  intmax_t val = strtoimax(str, &end, 10);
  
  if (errno == ERANGE || val < 0 || val > UINT16_MAX || end == str || *end != '\0')
    return false;
  *res = (uint16_t) val;
  return true;
}

/**
 * Open DLL, returning a handle.
 */

void*
dlopen(
    const char* file,   /** DLL filename (UTF-8). */
    int mode            /** mode flags (ignored). */
)
{
    WCHAR* unicodeFilename;
    UINT errorMode;
    void* handle;

    UNREFERENCED_PARAMETER(mode);

    if (file == NULL)
        return (void*) GetModuleHandle(NULL);

    unicodeFilename = UTF8toWCHAR(file);

    if (unicodeFilename == NULL) {
        lastError = GetLastError();
        return NULL;
    }

    errorMode = GetErrorMode();

    /* Have LoadLibrary return NULL on failure; prevent GUI error message. */
    SetErrorMode(errorMode | SEM_FAILCRITICALERRORS);

    handle = (void*) LoadLibraryW(unicodeFilename);

    if (handle == NULL)
        lastError = GetLastError();

    SetErrorMode(errorMode);

    free(unicodeFilename);

    return handle;
}

/**
 * Close DLL.
 */

int
dlclose(
    void* handle        /** Handle from dlopen(). */
)
{
    int rc = 0;

    if (handle != (void*) GetModuleHandle(NULL))
        rc = !FreeLibrary((HMODULE) handle);

    if (rc)
        lastError = GetLastError();

    return rc;
}

/**
 * Look up symbol exported by DLL.
 */

void*
dlsym(
    void* handle,       /** Handle from dlopen(). */
    const char* name    /** Name of exported symbol (ASCII). */
)
{
    uint16_t ordinal;
    void* address;
    
    if (str_to_uint16(name, &ordinal) == true){
      address = (void*) GetProcAddress((HMODULE) handle, (LPCSTR)ordinal);
    } else {
      address = (void*) GetProcAddress((HMODULE) handle, name);
    }

    if (address == NULL)
        lastError = GetLastError();

    return address;
}

/**
 * Return message describing last error.
 */

char*
dlerror(void)
{
    static char errorMessage[64];

    if (lastError != 0) {
        sprintf(errorMessage, "Win32 error %lu", lastError);
        lastError = 0;
        return errorMessage;
    } else {
        return NULL;
    }
}

#ifdef __cplusplus
}
#endif
