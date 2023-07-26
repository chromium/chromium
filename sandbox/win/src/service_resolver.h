// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SERVICE_RESOLVER_H_
#define SANDBOX_WIN_SRC_SERVICE_RESOLVER_H_

#include <stddef.h>

#include "base/win/windows_types.h"
#include "sandbox/win/src/resolver.h"

namespace sandbox {

// This is the concrete resolver used to perform service-call type functions
// inside ntdll.dll.
class [[clang::lto_visibility_public]] ServiceResolverThunk
    : public ResolverThunk {
 public:
  // The service resolver needs a child process to write to.
  ServiceResolverThunk(HANDLE process, bool relaxed)
      : ntdll_base_(nullptr),
        process_(process),
        relaxed_(relaxed),
        relative_jump_(0) {}

  ServiceResolverThunk(const ServiceResolverThunk&) = delete;
  ServiceResolverThunk& operator=(const ServiceResolverThunk&) = delete;

  ~ServiceResolverThunk() override {}

  // Implementation of Resolver::Setup.
  NTSTATUS Setup(const void* target_module,
                 const void* interceptor_module,
                 const char* target_name,
                 const char* interceptor_name,
                 const void* interceptor_entry_point,
                 void* thunk_storage,
                 size_t storage_bytes,
                 size_t* storage_used) override;

  // Implementation of Resolver::ResolveInterceptor.
  NTSTATUS ResolveInterceptor(const void* module,
                              const char* function_name,
                              const void** address) override;

  // Implementation of Resolver::ResolveTarget.
  NTSTATUS ResolveTarget(const void* module,
                         const char* function_name,
                         void** address) override;

  // Implementation of Resolver::GetThunkSize.
  size_t GetThunkSize() const override;

  // Call this to set up ntdll_base_ which will allow for local patches.
  void AllowLocalPatches();

  // Verifies that the function specified by |target_name| in |target_module| is
  // a service and copies the data from that function into |thunk_storage|. If
  // |storage_bytes| is too small, then the method fails.
  NTSTATUS CopyThunk(const void* target_module,
                     const char* target_name,
                     BYTE* thunk_storage,
                     size_t storage_bytes,
                     size_t* storage_used);

  // Checks if a target was patched correctly for a jump. This is only for use
  // in testing in 32-bit builds. Will always return true on 64-bit builds. Set
  // |thunk_storage| to the same pointer passed to Setup().
  bool VerifyJumpTargetForTesting(void* thunk_storage) const;

 private:
  // The unit test will use this member to allow local patch on a buffer.
  HMODULE ntdll_base_;

  // Handle of the child process.
  HANDLE process_;

  // Writes |length| bytes from the provided |buffer| into the address space of
  // |child_process|, at the specified |address|, preserving the original write
  // protection attributes. Returns true on success.
  static bool WriteProtectedChildMemory(HANDLE child_process,
                                        void* address,
                                        const void* buffer,
                                        size_t length);

  // Returns true if the code pointer by target_ corresponds to the expected
  // type of function. Saves that code on the first part of the thunk pointed
  // by local_thunk (should be directly accessible from the parent).
  bool IsFunctionAService(void* local_thunk) const;

  // Performs the actual patch of target_.
  // local_thunk must be already fully initialized, and the first part must
  // contain the original code. The real type of this buffer is ServiceFullThunk
  // (yes, private). remote_thunk (real type ServiceFullThunk), must be
  // allocated on the child, and will contain the thunk data, after this call.
  // Returns the appropriate status code.
  NTSTATUS PerformPatch(void* local_thunk, void* remote_thunk);

  // Provides basically the same functionality as IsFunctionAService but it
  // continues even if it does not recognize the function code. remote_thunk
  // is the address of our memory on the child.
  bool SaveOriginalFunction(void* local_thunk, void* remote_thunk);

  // true if we are allowed to patch already-patched functions.
  bool relaxed_;
  ULONG relative_jump_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SERVICE_RESOLVER_H_
