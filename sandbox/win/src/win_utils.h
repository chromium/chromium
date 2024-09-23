// Copyright 2006-2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_WIN_UTILS_H_
#define SANDBOX_WIN_SRC_WIN_UTILS_H_

#include <stdint.h>
#include <stdlib.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/win/windows_types.h"

namespace sandbox {

// Prefix for path used by NT calls.
const wchar_t kNTPrefix[] = L"\\??\\";

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

// Resolves a handle to an nt path or nullopt if the path cannot be resolved.
std::optional<std::wstring> GetPathFromHandle(HANDLE handle);

// Resolves a win32 path to an nt path using GetPathFromHandle. The path must
// exist. Returns the path if the translation was successful.
std::optional<std::wstring> GetNtPathFromWin32Path(const std::wstring& path);

// Resolves a handle to its type name. Returns the typename if successful.
std::optional<std::wstring> GetTypeNameFromHandle(HANDLE handle);

// Allocates |local_buffer.size()| in child (PAGE_READWRITE) and copies data
// from |local_buffer| in this process into |child|. |remote_buffer|
// contains the address in the chile.  If a zero byte copy is
// requested |true| is returned and no allocation or copying is
// attempted.  Returns false if allocation or copying fails. If
// copying fails, the allocation will be reversed.
bool CopyToChildMemory(HANDLE child,
                       base::span<uint8_t> local_buffer,
                       void** remote_buffer);

// Returns true if the provided path points to a pipe using a native path.
bool IsPipe(const std::wstring& path);

// Converts a NTSTATUS code to a Win32 error code.
DWORD GetLastErrorFromNtStatus(NTSTATUS status);

// Returns the address of the main exe module in memory taking in account
// address space layout randomization. This uses the process' PEB to extract
// the base address. This should only be called on new, suspended processes.
void* GetProcessBaseAddress(HANDLE process);

// Returns true if the string contains a NUL ('\0') character.
bool ContainsNulCharacter(std::wstring_view str);

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_WIN_UTILS_H_
