// Copyright 2006-2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/resolver.h"

#include <windows.h>

#include <ntstatus.h>
#include <stddef.h>
#include <winternl.h>

#include "base/win/pe_image.h"

namespace sandbox {

NTSTATUS ResolverThunk::Init(const void* target_module,
                             const char* target_name,
                             const void* interceptor_entry_point,
                             void* thunk_storage,
                             size_t storage_bytes) {
  if (!thunk_storage || 0 == storage_bytes || !target_module || !target_name ||
      !interceptor_entry_point) {
    return STATUS_INVALID_PARAMETER;
  }

  if (storage_bytes < GetThunkSize())
    return STATUS_BUFFER_TOO_SMALL;

  NTSTATUS ret = STATUS_SUCCESS;
  ret = ResolveTarget(target_module, target_name, &target_);
  if (!NT_SUCCESS(ret))
    return ret;

  interceptor_ = interceptor_entry_point;

  return ret;
}

}  // namespace sandbox
