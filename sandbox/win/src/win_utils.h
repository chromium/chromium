// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_WIN_UTILS_H_
#define SANDBOX_SRC_WIN_UTILS_H_

#include <stddef.h>
#include <windows.h>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/stl_util.h"
#include "sandbox/win/src/nt_internals.h"

namespace sandbox {

// Prefix for path used by NT calls.
const wchar_t kNTPrefix[] = L"\\??\\";
const size_t kNTPrefixLen = base::size(kNTPrefix) - 1;

const wchar_t kNTDevicePrefix[] = L"\\Device\\";
const size_t kNTDevicePrefixLen = base::size(kNTDevicePrefix) - 1;

// Automatically acquires and releases a lock when the object is
// is destroyed.
class AutoLock {
 public:
  // Acquires the lock.
  explicit AutoLock(CRITICAL_SECTION* lock) : lock_(lock) {
    ::EnterCriticalSection(lock);
  }

  // Releases the lock;
  ~AutoLock() { ::LeaveCriticalSection(lock_); }

 private:
  CRITICAL_SECTION* lock_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(AutoLock);
};

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

// Function object which invokes LocalFree on its parameter, which must be
// a pointer. Can be used to store LocalAlloc pointers in std::unique_ptr:
//
// std::unique_ptr<int, sandbox::LocalFreeDeleter> foo_ptr(
//     static_cast<int*>(LocalAlloc(LMEM_FIXED, sizeof(int))));
struct LocalFreeDeleter {
  inline void operator()(void* ptr) const { ::LocalFree(ptr); }
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

// Resolves a handle to an nt path. Returns true if the handle can be resolved.
bool GetPathFromHandle(HANDLE handle, std::wstring* path);

// Resolves a win32 path to an nt path using GetPathFromHandle. The path must
// exist. Returs true if the translation was succesful.
bool GetNtPathFromWin32Path(const std::wstring& path, std::wstring* nt_path);

// Translates a reserved key name to its handle.
// For example "HKEY_LOCAL_MACHINE" returns HKEY_LOCAL_MACHINE.
// Returns nullptr if the name does not represent any reserved key name.
HKEY GetReservedKeyFromName(const std::wstring& name);

// Resolves a user-readable registry path to a system-readable registry path.
// For example, HKEY_LOCAL_MACHINE\\Software\\microsoft is translated to
// \\registry\\machine\\software\\microsoft. Returns false if the path
// cannot be resolved.
bool ResolveRegistryName(std::wstring name, std::wstring* resolved_name);

// Writes |length| bytes from the provided |buffer| into the address space of
// |child_process|, at the specified |address|, preserving the original write
// protection attributes. Returns true on success.
bool WriteProtectedChildMemory(HANDLE child_process,
                               void* address,
                               const void* buffer,
                               size_t length);

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

// Calls GetTokenInformation with the desired |info_class| and returns a
// |buffer| and the Win32 error code.
DWORD GetTokenInformation(HANDLE token,
                          TOKEN_INFORMATION_CLASS info_class,
                          std::unique_ptr<BYTE[]>* buffer);

}  // namespace sandbox

// Resolves a function name in NTDLL to a function pointer. The second parameter
// is a pointer to the function pointer.
void ResolveNTFunctionPtr(const char* name, void* ptr);

#endif  // SANDBOX_SRC_WIN_UTILS_H_
