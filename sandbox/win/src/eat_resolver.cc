// Copyright 2006-2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/src/eat_resolver.h"

#include <ntstatus.h>
#include <stddef.h>

#include "base/win/pe_image.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace sandbox {

NTSTATUS EatResolverThunk::Setup(const void* target_module,
                                 const void* interceptor_module,
                                 const char* target_name,
                                 const char* interceptor_name,
                                 const void* interceptor_entry_point,
                                 void* thunk_storage,
                                 size_t storage_bytes,
                                 size_t* storage_used) {
  NTSTATUS ret =
      Init(target_module, interceptor_module, target_name, interceptor_name,
           interceptor_entry_point, thunk_storage, storage_bytes);
  if (!NT_SUCCESS(ret))
    return ret;

  if (!eat_entry_)
    return STATUS_INVALID_PARAMETER;

#if defined(_WIN64)
  // We have two thunks, in order: the return path and the forward path.
  if (!SetInternalThunk(thunk_storage, storage_bytes, nullptr, target_))
    return STATUS_BUFFER_TOO_SMALL;

  size_t thunk_bytes = GetInternalThunkSize();
  storage_bytes -= thunk_bytes;
  thunk_storage = reinterpret_cast<char*>(thunk_storage) + thunk_bytes;
#endif

  if (!SetInternalThunk(thunk_storage, storage_bytes, target_, interceptor_))
    return STATUS_BUFFER_TOO_SMALL;

  AutoProtectMemory memory;
  ret = memory.ChangeProtection(eat_entry_, sizeof(DWORD), PAGE_READWRITE);
  if (!NT_SUCCESS(ret))
    return ret;

  // Perform the patch.
  *eat_entry_ = static_cast<DWORD>(reinterpret_cast<uintptr_t>(thunk_storage)) -
                static_cast<DWORD>(reinterpret_cast<uintptr_t>(target_module));

  if (storage_used)
    *storage_used = GetThunkSize();

  return ret;
}

NTSTATUS EatResolverThunk::ResolveTarget(const void* module,
                                         const char* function_name,
                                         void** address) {
  DCHECK_NT(address);
  if (!module)
    return STATUS_INVALID_PARAMETER;

  base::win::PEImage pe(module);
  if (!pe.VerifyMagic())
    return STATUS_INVALID_IMAGE_FORMAT;

  eat_entry_ = pe.GetExportEntry(function_name);

  if (!eat_entry_)
    return STATUS_PROCEDURE_NOT_FOUND;

  *address = pe.RVAToAddr(*eat_entry_);

  return STATUS_SUCCESS;
}

size_t EatResolverThunk::GetThunkSize() const {
#if defined(_WIN64)
  return GetInternalThunkSize() * 2;
#else
  return GetInternalThunkSize();
#endif
}

}  // namespace sandbox
