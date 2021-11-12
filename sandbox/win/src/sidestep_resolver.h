// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SIDESTEP_RESOLVER_H_
#define SANDBOX_WIN_SRC_SIDESTEP_RESOLVER_H_

#include <stddef.h>

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/resolver.h"

namespace sandbox {

// This is the concrete resolver used to perform sidestep interceptions.
class SidestepResolverThunk : public ResolverThunk {
 public:
  SidestepResolverThunk() {}

  SidestepResolverThunk(const SidestepResolverThunk&) = delete;
  SidestepResolverThunk& operator=(const SidestepResolverThunk&) = delete;

  ~SidestepResolverThunk() override {}

  // Implementation of Resolver::Setup.
  NTSTATUS Setup(const void* target_module,
                 const void* interceptor_module,
                 const char* target_name,
                 const char* interceptor_name,
                 const void* interceptor_entry_point,
                 void* thunk_storage,
                 size_t storage_bytes,
                 size_t* storage_used) override;

  // Implementation of Resolver::GetThunkSize.
  size_t GetThunkSize() const override;
};

// This is the concrete resolver used to perform smart sidestep interceptions.
// This means basically a sidestep interception that skips the interceptor when
// the caller resides on the same dll being intercepted. It is intended as
// a helper only, because that determination is not infallible.
class SmartSidestepResolverThunk : public SidestepResolverThunk {
 public:
  SmartSidestepResolverThunk() {}

  SmartSidestepResolverThunk(const SmartSidestepResolverThunk&) = delete;
  SmartSidestepResolverThunk& operator=(const SmartSidestepResolverThunk&) =
      delete;

  ~SmartSidestepResolverThunk() override {}

  // Implementation of Resolver::Setup.
  NTSTATUS Setup(const void* target_module,
                 const void* interceptor_module,
                 const char* target_name,
                 const char* interceptor_name,
                 const void* interceptor_entry_point,
                 void* thunk_storage,
                 size_t storage_bytes,
                 size_t* storage_used) override;

  // Implementation of Resolver::GetThunkSize.
  size_t GetThunkSize() const override;

 private:
  // Performs the actual call to the interceptor if the conditions are correct
  // (as determined by IsInternalCall).
  static void SmartStub();

  // Returns true if return_address is inside the module loaded at base.
  static bool IsInternalCall(const void* base, void* return_address);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SIDESTEP_RESOLVER_H_
