// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_RESOLVER_H_
#define SANDBOX_WIN_SRC_RESOLVER_H_

// Defines ResolverThunk, the interface for classes that perform interceptions.
// For more details see
// http://dev.chromium.org/developers/design-documents/sandbox .

#include <stddef.h>

#include "base/macros.h"
#include "sandbox/win/src/nt_internals.h"

namespace sandbox {

// A resolver is the object in charge of performing the actual interception of
// a function. There should be a concrete implementation of a resolver roughly
// per type of interception.
class ResolverThunk {
 public:
  ResolverThunk() {}
  virtual ~ResolverThunk() {}

  // Performs the actual interception of a function.
  // target_name is an exported function from the module loaded at
  // target_module, and must be replaced by interceptor_name, exported from
  // interceptor_module. interceptor_entry_point can be provided instead of
  // interceptor_name / interceptor_module.
  // thunk_storage must point to a buffer on the child's address space, to hold
  // the patch thunk, and related data. If provided, storage_used will receive
  // the number of bytes used from thunk_storage.
  //
  // Example: (without error checking)
  //
  // size_t size = resolver.GetThunkSize();
  // char* buffer = ::VirtualAllocEx(child_process, nullptr, size,
  //                                 MEM_COMMIT, PAGE_READWRITE);
  // resolver.Setup(ntdll_module, nullptr, L"NtCreateFile", nullptr,
  //                &MyReplacementFunction, buffer, size, nullptr);
  //
  // In general, the idea is to allocate a single big buffer for all
  // interceptions on the same dll, and call Setup n times.
  // WARNING: This means that any data member that is specific to a single
  // interception must be reset within this method.
  virtual NTSTATUS Setup(const void* target_module,
                         const void* interceptor_module,
                         const char* target_name,
                         const char* interceptor_name,
                         const void* interceptor_entry_point,
                         void* thunk_storage,
                         size_t storage_bytes,
                         size_t* storage_used) = 0;

  // Gets the address of function_name inside module (main exe).
  virtual NTSTATUS ResolveInterceptor(const void* module,
                                      const char* function_name,
                                      const void** address);

  // Gets the address of an exported function_name inside module.
  virtual NTSTATUS ResolveTarget(const void* module,
                                 const char* function_name,
                                 void** address);

  // Gets the required buffer size for this type of thunk.
  virtual size_t GetThunkSize() const = 0;

 protected:
  // Performs basic initialization on behalf of a concrete instance of a
  // resolver. That is, parameter validation and resolution of the target
  // and the interceptor into the member variables.
  //
  // target_name is an exported function from the module loaded at
  // target_module, and must be replaced by interceptor_name, exported from
  // interceptor_module. interceptor_entry_point can be provided instead of
  // interceptor_name / interceptor_module.
  // thunk_storage must point to a buffer on the child's address space, to hold
  // the patch thunk, and related data.
  virtual NTSTATUS Init(const void* target_module,
                        const void* interceptor_module,
                        const char* target_name,
                        const char* interceptor_name,
                        const void* interceptor_entry_point,
                        void* thunk_storage,
                        size_t storage_bytes);

  // Gets the required buffer size for the internal part of the thunk.
  size_t GetInternalThunkSize() const;

  // Initializes the internal part of the thunk.
  // interceptor is the function to be called instead of original_function.
  bool SetInternalThunk(void* storage, size_t storage_bytes,
                        const void* original_function, const void* interceptor);

  // Holds the resolved interception target.
  void* target_;
  // Holds the resolved interception interceptor.
  const void* interceptor_;

  DISALLOW_COPY_AND_ASSIGN(ResolverThunk);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_RESOLVER_H_
