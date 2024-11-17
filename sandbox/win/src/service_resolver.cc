// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/service_resolver.h"

#include <ntstatus.h>

#include "base/notreached.h"
#include "base/win/pe_image.h"

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
    NOTREACHED();
  }

  return STATUS_SUCCESS;
}

void ServiceResolverThunk::AllowLocalPatches() {
  constexpr wchar_t kNtdllName[] = L"ntdll.dll";
  ntdll_base_ = ::GetModuleHandle(kNtdllName);
}

bool ServiceResolverThunk::WriteProtectedChildMemory(HANDLE child_process,
                                                     void* address,
                                                     const void* buffer,
                                                     size_t length) {
  // First, remove the protections.
  DWORD old_protection;
  if (!::VirtualProtectEx(child_process, address, length, PAGE_WRITECOPY,
                          &old_protection)) {
    return false;
  }

  SIZE_T written;
  bool ok =
      ::WriteProcessMemory(child_process, address, buffer, length, &written) &&
      (length == written);

  // Always attempt to restore the original protection.
  if (!::VirtualProtectEx(child_process, address, length, old_protection,
                          &old_protection)) {
    return false;
  }

  return ok;
}

}  // namespace sandbox
