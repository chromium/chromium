// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/src/target_interceptions.h"

#include <ntstatus.h>

#include "base/win/static_constants.h"
#include "sandbox/win/src/interception_agent.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace sandbox {

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

    if (s_state == kBeforeKernel32) {
      const char* ansi_module_name =
          GetAnsiImageInfoFromModule(reinterpret_cast<HMODULE>(*base));

      // _strnicmp below may hit read access violations for some sections. We
      // find what looks like a valid export directory for a PE module but the
      // pointer to the module name will be pointing to invalid memory.
      __try {
        if (ansi_module_name &&
            (GetNtExports()->_strnicmp(ansi_module_name, KERNEL32_DLL_NAME,
                                       sizeof(KERNEL32_DLL_NAME)) == 0)) {
          s_state = kAfterKernel32;
        }
      } __except (EXCEPTION_EXECUTE_HANDLER) {
      }
    }

    // Assume the heap may not be initialized before kernel32 loads, which is
    // the case when AppVerifier is enabled.
    if (s_state == kBeforeKernel32) {
      break;
    }

    if (!InitHeap())
      break;

    if (!IsValidImageSection(section, base, offset, view_size))
      break;

    bool has_code = false;
    ScopedUnicodeString image_name =
        GetImageInfoFromModule(reinterpret_cast<HMODULE>(*base), &has_code);
    ScopedUnicodeString file_name = GetBackingFilePath(*base);
    std::wstring_view module_name = image_name.str();

    if (module_name.empty() && has_code) {
      // If the module has no exports we retrieve the module name from the
      // full path of the mapped section.
      module_name = ExtractModuleName(file_name.str());
    }

    InterceptionAgent* agent = InterceptionAgent::GetInterceptionAgent();

    if (agent) {
      if (!agent->OnDllLoad(file_name.str(), module_name, *base)) {
        // Interception agent is demanding to un-map the module.
        GetNtExports()->UnmapViewOfSection(process, *base);
        *base = nullptr;
        ret = STATUS_UNSUCCESSFUL;
      }
    }
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
