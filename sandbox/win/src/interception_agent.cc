// Copyright 2006-2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For information about interceptions as a whole see
// http://dev.chromium.org/developers/design-documents/sandbox .

#include "sandbox/win/src/interception_agent.h"

#include <windows.h>

#include <stddef.h>

#include <type_traits>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "sandbox/win/src/eat_resolver.h"
#include "sandbox/win/src/interception_internal.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace {

// Returns true if target lies between base and base + range.
bool IsWithinRange(const void* base, size_t range, const void* target) {
  const char* end = UNSAFE_TODO(reinterpret_cast<const char*>(base) + range);
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
  if (!s_singleton && g_interceptions) {
    s_singleton = new (NT_ALLOC) InterceptionAgent(g_interceptions);
  }
  return s_singleton;
}

InterceptionAgent::InterceptionAgent(SharedMemory* shared_memory)
    : interceptions_(shared_memory) {
  size_t n = interceptions_->num_intercepted_dlls;
  DllData* dlls = new (NT_ALLOC) DllData[n];
  DCHECK_NT(dlls);
  if (!dlls) {
    return;
  }
  // SAFETY: Allocated `n` elements above.
  dlls_ = UNSAFE_BUFFERS(base::span(dlls, n));
}

bool InterceptionAgent::DllMatch(const UNICODE_STRING* full_path,
                                 const UNICODE_STRING* name,
                                 const DllPatchInfo* dll_info) {
  UNICODE_STRING current_name;
  current_name.Length = static_cast<USHORT>(UNSAFE_TODO(
      GetNtExports()->wcslen(dll_info->dll_name) * sizeof(wchar_t)));
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
  for (; i < dlls_.size(); i++) {
    if (DllMatch(full_path, name, dll_info))
      break;

    dll_info = reinterpret_cast<DllPatchInfo*>(UNSAFE_TODO(
        reinterpret_cast<char*>(dll_info) + dll_info->record_bytes));
  }

  // Return now if the dll is not in our list of interest.
  if (i == dlls_.size()) {
    return true;
  }

  // The dll must be unloaded.
  if (dll_info->unload_module)
    return false;

  // Purify causes this condition to trigger.
  if (dlls_[i].base) {
    return true;
  }

  // `OnDllUnload` frees this array with the scalar `operator delete`, which
  // requires the returned pointer to be the start of the allocation. This holds
  // only if the compiler doesn't prepend an array cookie, i.e. if `ThunkData`
  // is trivially destructible.
  static_assert(std::is_trivially_destructible_v<ThunkData>);
  size_t n = dll_info->num_functions;
  ThunkData* thunks = new (NT_PAGE, base_address) ThunkData[n];
  DCHECK_NT(thunks);
  if (!thunks) {
    return true;
  }

  // SAFETY: Allocating `n` elements for a span of size `n`.
  dlls_[i] = {
      .base = base_address,
      .thunks = UNSAFE_BUFFERS(base::span(thunks, n)),
  };

  VERIFY(PatchDll(dll_info, dlls_[i]));

  ULONG old_protect;
  SIZE_T real_size = dlls_[i].thunks.size_bytes();
  void* to_protect = dlls_[i].thunks.data();
  VERIFY_SUCCESS(GetNtExports()->ProtectVirtualMemory(
      NtCurrentProcess, &to_protect, &real_size, PAGE_EXECUTE_READ,
      &old_protect));
  return true;
}

void InterceptionAgent::OnDllUnload(void* base_address) {
  for (DllData& dll : dlls_) {
    if (dll.base && dll.base == base_address) {
      operator delete(dll.thunks.data(), NT_PAGE);
      dll = {};
      break;
    }
  }
}

// TODO(rvargas): We have to deal with prebinded dlls. I see two options: change
// the timestamp of the patched dll, or modify the info on the prebinded dll.
// the first approach messes matching of debug symbols, the second one is more
// complicated.
bool InterceptionAgent::PatchDll(const DllPatchInfo* dll_info, DllData& dll) {
  DCHECK_NT(dll_info);

  const FunctionInfo* function = reinterpret_cast<const FunctionInfo*>(
      UNSAFE_TODO(reinterpret_cast<const char*>(dll_info) +
                  dll_info->offset_to_functions));

  for (ThunkData& thunk : dll.thunks) {
    if (!IsWithinRange(dll_info, dll_info->record_bytes, function->function)) {
      NOTREACHED_NT();
      return false;
    }

    ResolverThunk* resolver = GetResolver(function->type);
    if (!resolver)
      return false;

    NTSTATUS ret = resolver->Setup(dll.base, function->function,
                                   function->interceptor_address, &thunk,
                                   sizeof(thunk), nullptr);
    if (!NT_SUCCESS(ret)) {
      NOTREACHED_NT();
      return false;
    }

    DCHECK_NT(!UNSAFE_TODO(g_originals.functions[function->id]) ||
              UNSAFE_TODO(g_originals.functions[function->id]) == &thunk);
    UNSAFE_TODO(g_originals.functions[function->id]) = &thunk;

    function = reinterpret_cast<const FunctionInfo*>(UNSAFE_TODO(
        reinterpret_cast<const char*>(function) + function->record_bytes));
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
