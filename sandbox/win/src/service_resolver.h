// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SERVICE_RESOLVER_H_
#define SANDBOX_WIN_SRC_SERVICE_RESOLVER_H_

#include <stddef.h>

#include "sandbox/win/src/nt_internals.h"
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
  virtual void AllowLocalPatches();

  // Verifies that the function specified by |target_name| in |target_module| is
  // a service and copies the data from that function into |thunk_storage|. If
  // |storage_bytes| is too small, then the method fails.
  virtual NTSTATUS CopyThunk(const void* target_module,
                             const char* target_name,
                             BYTE* thunk_storage,
                             size_t storage_bytes,
                             size_t* storage_used);

 protected:
  // The unit test will use this member to allow local patch on a buffer.
  HMODULE ntdll_base_;

  // Handle of the child process.
  HANDLE process_;

 private:
  // Returns true if the code pointer by target_ corresponds to the expected
  // type of function. Saves that code on the first part of the thunk pointed
  // by local_thunk (should be directly accessible from the parent).
  virtual bool IsFunctionAService(void* local_thunk) const;

  // Performs the actual patch of target_.
  // local_thunk must be already fully initialized, and the first part must
  // contain the original code. The real type of this buffer is ServiceFullThunk
  // (yes, private). remote_thunk (real type ServiceFullThunk), must be
  // allocated on the child, and will contain the thunk data, after this call.
  // Returns the apropriate status code.
  virtual NTSTATUS PerformPatch(void* local_thunk, void* remote_thunk);

  // Provides basically the same functionality as IsFunctionAService but it
  // continues even if it does not recognize the function code. remote_thunk
  // is the address of our memory on the child.
  bool SaveOriginalFunction(void* local_thunk, void* remote_thunk);

  // true if we are allowed to patch already-patched functions.
  bool relaxed_;
  ULONG relative_jump_;
};

// This is the concrete resolver used to perform service-call type functions
// inside ntdll.dll on WOW64 (32 bit ntdll on 64 bit Vista).
class Wow64ResolverThunk : public ServiceResolverThunk {
 public:
  // The service resolver needs a child process to write to.
  Wow64ResolverThunk(HANDLE process, bool relaxed)
      : ServiceResolverThunk(process, relaxed) {}

  Wow64ResolverThunk(const Wow64ResolverThunk&) = delete;
  Wow64ResolverThunk& operator=(const Wow64ResolverThunk&) = delete;

  ~Wow64ResolverThunk() override {}

 private:
  bool IsFunctionAService(void* local_thunk) const override;
};

// This is the concrete resolver used to perform service-call type functions
// inside ntdll.dll on WOW64 for Windows 8.
class Wow64W8ResolverThunk : public ServiceResolverThunk {
 public:
  // The service resolver needs a child process to write to.
  Wow64W8ResolverThunk(HANDLE process, bool relaxed)
      : ServiceResolverThunk(process, relaxed) {}

  Wow64W8ResolverThunk(const Wow64W8ResolverThunk&) = delete;
  Wow64W8ResolverThunk& operator=(const Wow64W8ResolverThunk&) = delete;

  ~Wow64W8ResolverThunk() override {}

 private:
  bool IsFunctionAService(void* local_thunk) const override;
};

// This is the concrete resolver used to perform service-call type functions
// inside ntdll.dll on Windows 8.
class Win8ResolverThunk : public ServiceResolverThunk {
 public:
  // The service resolver needs a child process to write to.
  Win8ResolverThunk(HANDLE process, bool relaxed)
      : ServiceResolverThunk(process, relaxed) {}

  Win8ResolverThunk(const Win8ResolverThunk&) = delete;
  Win8ResolverThunk& operator=(const Win8ResolverThunk&) = delete;

  ~Win8ResolverThunk() override {}

 private:
  bool IsFunctionAService(void* local_thunk) const override;
};

// This is the concrete resolver used to perform service-call type functions
// inside ntdll.dll on WOW64 for Windows 10.
class Wow64W10ResolverThunk : public ServiceResolverThunk {
 public:
  // The service resolver needs a child process to write to.
  Wow64W10ResolverThunk(HANDLE process, bool relaxed)
      : ServiceResolverThunk(process, relaxed) {}

  Wow64W10ResolverThunk(const Wow64W10ResolverThunk&) = delete;
  Wow64W10ResolverThunk& operator=(const Wow64W10ResolverThunk&) = delete;

  ~Wow64W10ResolverThunk() override {}

 private:
  bool IsFunctionAService(void* local_thunk) const override;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SERVICE_RESOLVER_H_
