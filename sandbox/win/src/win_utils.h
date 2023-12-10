// Copyright 2006-2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_WIN_UTILS_H_
#define SANDBOX_WIN_SRC_WIN_UTILS_H_

#include <stdlib.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <optional>
#include "base/win/windows_types.h"

namespace sandbox {

// Prefix for path used by NT calls.
const wchar_t kNTPrefix[] = L"\\??\\";
const size_t kNTPrefixLen = std::size(kNTPrefix) - 1;

const wchar_t kNTDevicePrefix[] = L"\\Device\\";
const size_t kNTDevicePrefixLen = std::size(kNTDevicePrefix) - 1;

// List of handles mapped to their kernel object type name.
using ProcessHandleMap = std::map<std::wstring, std::vector<HANDLE>>;

// Basic implementation of a singleton which calls the destructor
// when the exe is shutting down or the DLL is being unloaded.
template <typename Derived>
class SingletonBase {
 public:
  static Derived* GetInstance() {
    static Derived* instance = nullptr;
    if (!instance) {
      instance = new Derived();
      // Microsoft CRT extension. In an exe this this called after
      // winmain returns, in a dll is called in DLL_PROCESS_DETACH
      _onexit(OnExit);
    }
    return instance;
  }

 private:
  // this is the function that gets called by the CRT when the
  // process is shutting down.
  static int __cdecl OnExit() {
    delete GetInstance();
    return 0;
  }
};

// Convert a short path (C:\path~1 or \\??\\c:\path~1) to the long version of
// the path. If the path is not a valid filesystem path, the function returns
// false and argument is not modified.
// - If passing in a short native device path (\Device\HarddiskVolumeX\path~1),
//   a drive letter string (c:\) must also be provided.
bool ConvertToLongPath(std::wstring* path,
                       const std::wstring* drive_letter = nullptr);

// Returns ERROR_SUCCESS if the path contains a reparse point,
// ERROR_NOT_A_REPARSE_POINT if there's no reparse point in this path, or an
// error code when the function fails.
// This function is not smart. It looks for each element in the path and
// returns true if any of them is a reparse point.
DWORD IsReparsePoint(const std::wstring& full_path);

// Returns true if the handle corresponds to the object pointed by this path.
bool SameObject(HANDLE handle, const wchar_t* full_path);

// Resolves a handle to an nt path or nullopt if the path cannot be resolved.
std::optional<std::wstring> GetPathFromHandle(HANDLE handle);

// Resolves a win32 path to an nt path using GetPathFromHandle. The path must
// exist. Returns the path if the translation was successful.
std::optional<std::wstring> GetNtPathFromWin32Path(const std::wstring& path);

// Resolves a handle to its type name. Returns the typename if successful.
std::optional<std::wstring> GetTypeNameFromHandle(HANDLE handle);

// Resolves a user-readable registry path to a system-readable registry path.
// For example, HKEY_LOCAL_MACHINE\\Software\\microsoft is translated to
// \\registry\\machine\\software\\microsoft. Returns nullopt if the path
// cannot be resolved.
std::optional<std::wstring> ResolveRegistryName(std::wstring name);

// Allocates |buffer_bytes| in child (PAGE_READWRITE) and copies data
// from |local_buffer| in this process into |child|. |remote_buffer|
// contains the address in the chile.  If a zero byte copy is
// requested |true| is returned and no allocation or copying is
// attempted.  Returns false if allocation or copying fails. If
// copying fails, the allocation will be reversed.
bool CopyToChildMemory(HANDLE child,
                       const void* local_buffer,
                       size_t buffer_bytes,
                       void** remote_buffer);

// Returns true if the provided path points to a pipe.
bool IsPipe(const std::wstring& path);

// Converts a NTSTATUS code to a Win32 error code.
DWORD GetLastErrorFromNtStatus(NTSTATUS status);

// Returns the address of the main exe module in memory taking in account
// address space layout randomization. This uses the process' PEB to extract
// the base address. This should only be called on new, suspended processes.
void* GetProcessBaseAddress(HANDLE process);

// Returns a map of handles open in the current process. The map is keyed by the
// kernel object type name. If querying the handles fails an empty optional
// value is returned. Note that unless all threads are suspended in the process
// the valid handles could change between the return of the list and when you
// use them.
std::optional<ProcessHandleMap> GetCurrentProcessHandles();

}  // namespace sandbox

// Resolves a function name in NTDLL to a function pointer. The second parameter
// is a pointer to the function pointer.
void ResolveNTFunctionPtr(const char* name, void* ptr);

#endif  // SANDBOX_WIN_SRC_WIN_UTILS_H_
