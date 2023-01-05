// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/service_resolver.h"

#include <ntstatus.h>

#include "base/win/pe_image.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace sandbox {

NTSTATUS ServiceResolverThunk::ResolveInterceptor(
    const void* interceptor_module,
    const char* interceptor_name,
    const void** address) {
  // After all, we are using a locally mapped version of the exe, so the
  // action is the same as for a target function.
  return ResolveTarget(interceptor_module, interceptor_name,
                       const_cast<void**>(address));
}

// In this case all the work is done from the parent, so resolve is
// just a simple GetProcAddress.
NTSTATUS ServiceResolverThunk::ResolveTarget(const void* module,
                                             const char* function_name,
                                             void** address) {
  if (!module)
    return STATUS_UNSUCCESSFUL;

  base::win::PEImage module_image(module);
  *address =
      reinterpret_cast<void*>(module_image.GetProcAddress(function_name));

  if (!*address) {
    NOTREACHED_NT();
    return STATUS_UNSUCCESSFUL;
  }

  return STATUS_SUCCESS;
}

void ServiceResolverThunk::AllowLocalPatches() {
  ntdll_base_ = ::GetModuleHandle(kNtdllName);
}

}  // namespace sandbox
