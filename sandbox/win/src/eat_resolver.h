// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_EAT_RESOLVER_H_
#define SANDBOX_WIN_SRC_EAT_RESOLVER_H_

#include <stddef.h>

#include "base/memory/raw_ptr_exclusion.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/resolver.h"

namespace sandbox {

// This is the concrete resolver used to perform exports table interceptions.
class EatResolverThunk : public ResolverThunk {
 public:
  EatResolverThunk() : eat_entry_(nullptr) {}

  EatResolverThunk(const EatResolverThunk&) = delete;
  EatResolverThunk& operator=(const EatResolverThunk&) = delete;

  ~EatResolverThunk() override {}

  // Implementation of Resolver::Setup.
  NTSTATUS Setup(const void* target_module,
                 const void* interceptor_module,
                 const char* target_name,
                 const char* interceptor_name,
                 const void* interceptor_entry_point,
                 void* thunk_storage,
                 size_t storage_bytes,
                 size_t* storage_used) override;

  // Implementation of Resolver::ResolveTarget.
  NTSTATUS ResolveTarget(const void* module,
                         const char* function_name,
                         void** address) override;

  // Implementation of Resolver::GetThunkSize.
  size_t GetThunkSize() const override;

 private:
  // The entry to patch.
  // RAW_PTR_EXCLUSION: Accessed too early during the process startup to support
  // raw_ptr<T>.
  RAW_PTR_EXCLUSION DWORD* eat_entry_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_EAT_RESOLVER_H_
