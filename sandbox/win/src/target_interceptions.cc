// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/target_interceptions.h"

#include "base/win/static_constants.h"
#include "sandbox/win/src/interception_agent.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace sandbox {

SANDBOX_INTERCEPT NtExports g_nt;

const char KERNEL32_DLL_NAME[] = "kernel32.dll";

enum SectionLoadState {
  kBeforeKernel32,
  kAfterKernel32,
};

// Hooks NtMapViewOfSection to detect the load of DLLs. If hot patching is
// required for this dll, this functions patches it.
NTSTATUS WINAPI
TargetNtMapViewOfSection(NtMapViewOfSectionFunction orig_MapViewOfSection,
                         HANDLE section,
                         HANDLE process,
                         PVOID* base,
                         ULONG_PTR zero_bits,
                         SIZE_T commit_size,
                         PLARGE_INTEGER offset,
                         PSIZE_T view_size,
                         SECTION_INHERIT inherit,
                         ULONG allocation_type,
                         ULONG protect) {
  NTSTATUS ret = orig_MapViewOfSection(section, process, base, zero_bits,
                                       commit_size, offset, view_size, inherit,
                                       allocation_type, protect);
  static SectionLoadState s_state = kBeforeKernel32;

  do {
    if (!NT_SUCCESS(ret))
      break;

    if (!IsSameProcess(process))
      break;

    // Only check for verifier.dll or kernel32.dll loading if we haven't moved
    // past that state yet.
    if (s_state == kBeforeKernel32) {
      const char* ansi_module_name =
          GetAnsiImageInfoFromModule(reinterpret_cast<HMODULE>(*base));

      // _strnicmp below may hit read access violations for some sections. We
      // find what looks like a valid export directory for a PE module but the
      // pointer to the module name will be pointing to invalid memory.
      __try {
        // Don't initialize the heap if verifier.dll is being loaded. This
        // indicates Application Verifier is enabled and we should wait until
        // the next module is loaded.
        if (ansi_module_name &&
            (g_nt._strnicmp(
                 ansi_module_name, base::win::kApplicationVerifierDllName,
                 g_nt.strlen(base::win::kApplicationVerifierDllName) + 1) == 0))
          break;

        if (ansi_module_name &&
            (g_nt._strnicmp(ansi_module_name, KERNEL32_DLL_NAME,
                            sizeof(KERNEL32_DLL_NAME)) == 0)) {
          s_state = kAfterKernel32;
        }
      } __except (EXCEPTION_EXECUTE_HANDLER) {
      }
    }

    if (!InitHeap())
      break;

    if (!IsValidImageSection(section, base, offset, view_size))
      break;

    UINT image_flags;
    UNICODE_STRING* module_name =
        GetImageInfoFromModule(reinterpret_cast<HMODULE>(*base), &image_flags);
    UNICODE_STRING* file_name = GetBackingFilePath(*base);

    if ((!module_name) && (image_flags & MODULE_HAS_CODE)) {
      // If the module has no exports we retrieve the module name from the
      // full path of the mapped section.
      module_name = ExtractModuleName(file_name);
    }

    InterceptionAgent* agent = InterceptionAgent::GetInterceptionAgent();

    if (agent) {
      if (!agent->OnDllLoad(file_name, module_name, *base)) {
        // Interception agent is demanding to un-map the module.
        g_nt.UnmapViewOfSection(process, *base);
        *base = nullptr;
        ret = STATUS_UNSUCCESSFUL;
      }
    }

    if (module_name)
      operator delete(module_name, NT_ALLOC);

    if (file_name)
      operator delete(file_name, NT_ALLOC);

  } while (false);

  return ret;
}

NTSTATUS WINAPI
TargetNtUnmapViewOfSection(NtUnmapViewOfSectionFunction orig_UnmapViewOfSection,
                           HANDLE process,
                           PVOID base) {
  NTSTATUS ret = orig_UnmapViewOfSection(process, base);

  if (!NT_SUCCESS(ret))
    return ret;

  if (!IsSameProcess(process))
    return ret;

  InterceptionAgent* agent = InterceptionAgent::GetInterceptionAgent();

  if (agent)
    agent->OnDllUnload(base);

  return ret;
}

}  // namespace sandbox
