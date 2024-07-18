// Copyright 2006-2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// For information about interceptions as a whole see
// http://dev.chromium.org/developers/design-documents/sandbox .

#include "sandbox/win/src/interception_agent.h"

#include <windows.h>

#include <stddef.h>

#include "sandbox/win/src/eat_resolver.h"
#include "sandbox/win/src/interception_internal.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace {

// Returns true if target lies between base and base + range.
bool IsWithinRange(const void* base, size_t range, const void* target) {
  const char* end = reinterpret_cast<const char*>(base) + range;
  return reinterpret_cast<const char*>(target) < end;
}

}  // namespace

namespace sandbox {

// The list of intercepted functions back-pointers.
SANDBOX_INTERCEPT OriginalFunctions g_originals;

// Memory buffer mapped from the parent, with the list of interceptions.
SANDBOX_INTERCEPT SharedMemory* g_interceptions = nullptr;

InterceptionAgent* InterceptionAgent::GetInterceptionAgent() {
  static InterceptionAgent* s_singleton = nullptr;
  if (!s_singleton) {
    if (!g_interceptions)
      return nullptr;

    size_t array_bytes = g_interceptions->num_intercepted_dlls * sizeof(void*);
    s_singleton = reinterpret_cast<InterceptionAgent*>(
        new (NT_ALLOC) char[array_bytes + sizeof(InterceptionAgent)]);

    bool success = s_singleton->Init(g_interceptions);
    if (!success) {
      operator delete(s_singleton, NT_ALLOC);
      s_singleton = nullptr;
    }
  }
  return s_singleton;
}

bool InterceptionAgent::Init(SharedMemory* shared_memory) {
  interceptions_ = shared_memory;
  for (size_t i = 0; i < shared_memory->num_intercepted_dlls; i++)
    dlls_[i] = nullptr;
  return true;
}

bool InterceptionAgent::DllMatch(const UNICODE_STRING* full_path,
                                 const UNICODE_STRING* name,
                                 const DllPatchInfo* dll_info) {
  UNICODE_STRING current_name;
  current_name.Length = static_cast<USHORT>(
      GetNtExports()->wcslen(dll_info->dll_name) * sizeof(wchar_t));
  current_name.MaximumLength = current_name.Length;
  current_name.Buffer = const_cast<wchar_t*>(dll_info->dll_name);

  BOOLEAN case_insensitive = TRUE;
  if (full_path && !GetNtExports()->RtlCompareUnicodeString(
                       &current_name, full_path, case_insensitive)) {
    return true;
  }
  return name && !GetNtExports()->RtlCompareUnicodeString(&current_name, name,
                                                          case_insensitive);
}

bool InterceptionAgent::OnDllLoad(const UNICODE_STRING* full_path,
                                  const UNICODE_STRING* name,
                                  void* base_address) {
  DllPatchInfo* dll_info = interceptions_->dll_list;
  size_t i = 0;
  for (; i < interceptions_->num_intercepted_dlls; i++) {
    if (DllMatch(full_path, name, dll_info))
      break;

    dll_info = reinterpret_cast<DllPatchInfo*>(
        reinterpret_cast<char*>(dll_info) + dll_info->record_bytes);
  }

  // Return now if the dll is not in our list of interest.
  if (i == interceptions_->num_intercepted_dlls)
    return true;

  // The dll must be unloaded.
  if (dll_info->unload_module)
    return false;

  // Purify causes this condition to trigger.
  if (dlls_[i])
    return true;

  size_t buffer_bytes = offsetof(DllInterceptionData, thunks) +
                        dll_info->num_functions * sizeof(ThunkData);
  dlls_[i] = reinterpret_cast<DllInterceptionData*>(
      new (NT_PAGE, base_address) char[buffer_bytes]);

  DCHECK_NT(dlls_[i]);
  if (!dlls_[i])
    return true;

  dlls_[i]->data_bytes = buffer_bytes;
  dlls_[i]->num_thunks = 0;
  dlls_[i]->base = base_address;
  dlls_[i]->used_bytes = offsetof(DllInterceptionData, thunks);

  VERIFY(PatchDll(dll_info, dlls_[i]));

  ULONG old_protect;
  SIZE_T real_size = buffer_bytes;
  void* to_protect = dlls_[i];
  VERIFY_SUCCESS(GetNtExports()->ProtectVirtualMemory(
      NtCurrentProcess, &to_protect, &real_size, PAGE_EXECUTE_READ,
      &old_protect));
  return true;
}

void InterceptionAgent::OnDllUnload(void* base_address) {
  for (size_t i = 0; i < interceptions_->num_intercepted_dlls; i++) {
    if (dlls_[i] && dlls_[i]->base == base_address) {
      operator delete(dlls_[i], NT_PAGE);
      dlls_[i] = nullptr;
      break;
    }
  }
}

// TODO(rvargas): We have to deal with prebinded dlls. I see two options: change
// the timestamp of the patched dll, or modify the info on the prebinded dll.
// the first approach messes matching of debug symbols, the second one is more
// complicated.
bool InterceptionAgent::PatchDll(const DllPatchInfo* dll_info,
                                 DllInterceptionData* thunks) {
  DCHECK_NT(thunks);
  DCHECK_NT(dll_info);

  const FunctionInfo* function = reinterpret_cast<const FunctionInfo*>(
      reinterpret_cast<const char*>(dll_info) + dll_info->offset_to_functions);

  for (size_t i = 0; i < dll_info->num_functions; i++) {
    if (!IsWithinRange(dll_info, dll_info->record_bytes, function->function)) {
      NOTREACHED_NT();
      return false;
    }

    ResolverThunk* resolver = GetResolver(function->type);
    if (!resolver)
      return false;

    const char* interceptor =
        function->function + GetNtExports()->strlen(function->function) + 1;

    if (!IsWithinRange(function, function->record_bytes, interceptor) ||
        !IsWithinRange(dll_info, dll_info->record_bytes, interceptor)) {
      NOTREACHED_NT();
      return false;
    }

    NTSTATUS ret = resolver->Setup(
        thunks->base, interceptions_->interceptor_base, function->function,
        interceptor, function->interceptor_address, &thunks->thunks[i],
        sizeof(ThunkData), nullptr);
    if (!NT_SUCCESS(ret)) {
      NOTREACHED_NT();
      return false;
    }

    DCHECK_NT(!g_originals.functions[function->id] ||
              g_originals.functions[function->id] == &thunks->thunks[i]);
    g_originals.functions[function->id] = &thunks->thunks[i];

    thunks->num_thunks++;
    thunks->used_bytes += sizeof(ThunkData);

    function = reinterpret_cast<const FunctionInfo*>(
        reinterpret_cast<const char*>(function) + function->record_bytes);
  }

  return true;
}

// This method is called from within the loader lock
ResolverThunk* InterceptionAgent::GetResolver(InterceptionType type) {
  static EatResolverThunk* eat_resolver = nullptr;

  if (!eat_resolver)
    eat_resolver = new (NT_ALLOC) EatResolverThunk;
  if (type == INTERCEPTION_EAT)
    return eat_resolver;
  NOTREACHED_NT();
  return nullptr;
}

}  // namespace sandbox
