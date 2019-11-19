// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations.h"

#include <stddef.h>
#include <windows.h>
#include <wow64apiset.h>

#include <algorithm>

#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/sandbox_rand.h"
#include "sandbox/win/src/win_utils.h"

namespace {

// API defined in libloaderapi.h >= Win8.
using SetDefaultDllDirectoriesFunction = decltype(&SetDefaultDllDirectories);

// APIs defined in processthreadsapi.h >= Win8.
using SetProcessMitigationPolicyFunction =
    decltype(&SetProcessMitigationPolicy);
using GetProcessMitigationPolicyFunction =
    decltype(&GetProcessMitigationPolicy);
using SetThreadInformationFunction = decltype(&SetThreadInformation);

// Returns a two-element array of mitigation flags supported on this machine.
// - This function is only useful on >= base::win::Version::WIN8.
const ULONG64* GetSupportedMitigations() {
  static ULONG64 mitigations[2] = {};

  // This static variable will only be initialized once.
  if (!mitigations[0] && !mitigations[1]) {
    GetProcessMitigationPolicyFunction get_process_mitigation_policy =
        reinterpret_cast<GetProcessMitigationPolicyFunction>(::GetProcAddress(
            ::GetModuleHandleA("kernel32.dll"), "GetProcessMitigationPolicy"));
    if (get_process_mitigation_policy) {
      // NOTE: the two-element-sized input array is only supported on >= Win10
      // RS2.
      //       If an earlier version, the second element will be left 0.
      size_t mits_size =
          (base::win::GetVersion() >= base::win::Version::WIN10_RS2)
              ? (sizeof(mitigations[0]) * 2)
              : sizeof(mitigations[0]);
      if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                         ProcessMitigationOptionsMask,
                                         &mitigations, mits_size)) {
        NOTREACHED();
      }
    }
  }

  return &mitigations[0];
}

// Returns true if this is 32-bit Chrome running on ARM64 with emulation.
// Needed because ACG does not work with emulated code.
// See
// https://docs.microsoft.com/en-us/windows/uwp/porting/apps-on-arm-troubleshooting-x86.
// See https://crbug.com/977723.
// TODO(wfh): Move this code into base. See https://crbug.com/978257.
bool IsRunning32bitEmulatedOnArm64() {
#if defined(ARCH_CPU_X86)
  using IsWow64Process2Function = decltype(&IsWow64Process2);

  IsWow64Process2Function is_wow64_process2 =
      reinterpret_cast<IsWow64Process2Function>(::GetProcAddress(
          ::GetModuleHandleA("kernel32.dll"), "IsWow64Process2"));
  if (!is_wow64_process2)
    return false;
  USHORT process_machine;
  USHORT native_machine;
  bool retval = is_wow64_process2(::GetCurrentProcess(), &process_machine,
                                  &native_machine);
  if (!retval)
    return false;
  if (native_machine == IMAGE_FILE_MACHINE_ARM64)
    return true;
#endif  // defined(ARCH_CPU_X86)
  return false;
}

}  // namespace

namespace sandbox {

bool ApplyProcessMitigationsToCurrentProcess(MitigationFlags flags) {
  if (!CanSetProcessMitigationsPostStartup(flags))
    return false;

  base::win::Version version = base::win::GetVersion();
  HMODULE module = ::GetModuleHandleA("kernel32.dll");

  if (flags & MITIGATION_DLL_SEARCH_ORDER) {
    SetDefaultDllDirectoriesFunction set_default_dll_directories =
        reinterpret_cast<SetDefaultDllDirectoriesFunction>(
            ::GetProcAddress(module, "SetDefaultDllDirectories"));

    // Check for SetDefaultDllDirectories since it requires KB2533623.
    if (set_default_dll_directories) {
#if defined(COMPONENT_BUILD)
      const DWORD directory_flags = LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
#else
      // In a non-component build, all DLLs will be loaded manually, or via
      // manifest definition, so these flags can be stronger. This prevents DLL
      // planting in the application directory.
      const DWORD directory_flags =
          LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS;
#endif
      if (!set_default_dll_directories(directory_flags) &&
          ERROR_ACCESS_DENIED != ::GetLastError()) {
        return false;
      }
    }
  }

  // Set the heap to terminate on corruption
  if (flags & MITIGATION_HEAP_TERMINATE) {
    if (!::HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption,
                              nullptr, 0) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  if (flags & MITIGATION_HARDEN_TOKEN_IL_POLICY) {
    DWORD error = HardenProcessIntegrityLevelPolicy();
    if ((error != ERROR_SUCCESS) && (error != ERROR_ACCESS_DENIED))
      return false;
  }

#if !defined(_WIN64)  // DEP is always enabled on 64-bit.
  if (flags & MITIGATION_DEP) {
    DWORD dep_flags = PROCESS_DEP_ENABLE;

    if (flags & MITIGATION_DEP_NO_ATL_THUNK)
      dep_flags |= PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION;

    if (!::SetProcessDEPPolicy(dep_flags) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }
#endif

  // This is all we can do in Win7 and below.
  if (version < base::win::Version::WIN8)
    return true;

  SetProcessMitigationPolicyFunction set_process_mitigation_policy =
      reinterpret_cast<SetProcessMitigationPolicyFunction>(
          ::GetProcAddress(module, "SetProcessMitigationPolicy"));
  if (!set_process_mitigation_policy)
    return false;

  // Enable ASLR policies.
  if (flags & MITIGATION_RELOCATE_IMAGE) {
    PROCESS_MITIGATION_ASLR_POLICY policy = {};
    policy.EnableForceRelocateImages = true;
    policy.DisallowStrippedImages =
        (flags & MITIGATION_RELOCATE_IMAGE_REQUIRED) ==
        MITIGATION_RELOCATE_IMAGE_REQUIRED;

    if (!set_process_mitigation_policy(ProcessASLRPolicy, &policy,
                                       sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  // Enable strict handle policies.
  if (flags & MITIGATION_STRICT_HANDLE_CHECKS) {
    PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY policy = {};
    policy.HandleExceptionsPermanentlyEnabled =
        policy.RaiseExceptionOnInvalidHandleReference = true;

    if (!set_process_mitigation_policy(ProcessStrictHandleCheckPolicy, &policy,
                                       sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  // Enable system call policies.
  if (flags & MITIGATION_WIN32K_DISABLE) {
    PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy = {};
    policy.DisallowWin32kSystemCalls = true;

    if (!set_process_mitigation_policy(ProcessSystemCallDisablePolicy, &policy,
                                       sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  // Enable extension point policies.
  if (flags & MITIGATION_EXTENSION_POINT_DISABLE) {
    PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy = {};
    policy.DisableExtensionPoints = true;

    if (!set_process_mitigation_policy(ProcessExtensionPointDisablePolicy,
                                       &policy, sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  if (version < base::win::Version::WIN8_1)
    return true;

  // Enable dynamic code policies.
  if (!IsRunning32bitEmulatedOnArm64() &&
      (flags & MITIGATION_DYNAMIC_CODE_DISABLE)) {
    // Verify caller is not accidentally setting both mutually exclusive
    // policies.
    DCHECK(!(flags & MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT));
    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY policy = {};
    policy.ProhibitDynamicCode = true;

    if (!set_process_mitigation_policy(ProcessDynamicCodePolicy, &policy,
                                       sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  if (version < base::win::Version::WIN10)
    return true;

  // Enable font policies.
  if (flags & MITIGATION_NONSYSTEM_FONT_DISABLE) {
    PROCESS_MITIGATION_FONT_DISABLE_POLICY policy = {};
    policy.DisableNonSystemFonts = true;

    if (!set_process_mitigation_policy(ProcessFontDisablePolicy, &policy,
                                       sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  if (version < base::win::Version::WIN10_TH2)
    return true;

  // Enable binary signing policies.
  if (flags & MITIGATION_FORCE_MS_SIGNED_BINS) {
    PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY policy = {};
    // Allow only MS signed binaries.
    policy.MicrosoftSignedOnly = true;
    // NOTE: there are two other flags available to allow
    // 1) Only Windows Store signed.
    // 2) MS-signed, Win Store signed, and WHQL signed binaries.
    // Support not added at the moment.
    if (!set_process_mitigation_policy(ProcessSignaturePolicy, &policy,
                                       sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  // Enable image load policies.
  if (flags & MITIGATION_IMAGE_LOAD_NO_REMOTE ||
      flags & MITIGATION_IMAGE_LOAD_NO_LOW_LABEL ||
      flags & MITIGATION_IMAGE_LOAD_PREFER_SYS32) {
    PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy = {};
    if (flags & MITIGATION_IMAGE_LOAD_NO_REMOTE)
      policy.NoRemoteImages = true;
    if (flags & MITIGATION_IMAGE_LOAD_NO_LOW_LABEL)
      policy.NoLowMandatoryLabelImages = true;
    // PreferSystem32 is only supported on >= Anniversary.
    if (version >= base::win::Version::WIN10_RS1 &&
        flags & MITIGATION_IMAGE_LOAD_PREFER_SYS32) {
      policy.PreferSystem32Images = true;
    }

    if (!set_process_mitigation_policy(ProcessImageLoadPolicy, &policy,
                                       sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  if (version < base::win::Version::WIN10_RS1)
    return true;

  // Enable dynamic code policies.
  // Per-thread opt-out is only supported on >= Anniversary (RS1).
  if (!IsRunning32bitEmulatedOnArm64() &&
      (flags & MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT)) {
    // Verify caller is not accidentally setting both mutually exclusive
    // policies.
    DCHECK(!(flags & MITIGATION_DYNAMIC_CODE_DISABLE));
    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY policy = {};
    policy.ProhibitDynamicCode = true;
    policy.AllowThreadOptOut = true;

    if (!set_process_mitigation_policy(ProcessDynamicCodePolicy, &policy,
                                       sizeof(policy)) &&
        ERROR_ACCESS_DENIED != ::GetLastError()) {
      return false;
    }
  }

  return true;
}

bool ApplyMitigationsToCurrentThread(MitigationFlags flags) {
  if (!CanSetMitigationsPerThread(flags))
    return false;

  base::win::Version version = base::win::GetVersion();

  if (version < base::win::Version::WIN10_RS1)
    return true;

  // Enable dynamic code per-thread policies.
  if (flags & MITIGATION_DYNAMIC_CODE_OPT_OUT_THIS_THREAD) {
    DWORD thread_policy = THREAD_DYNAMIC_CODE_ALLOW;

    // NOTE: SetThreadInformation API only exists on >= Win8.  Dynamically
    //       get function handle.
    base::ScopedNativeLibrary dll(base::FilePath(L"kernel32.dll"));
    if (!dll.is_valid())
      return false;
    SetThreadInformationFunction set_thread_info_function =
        reinterpret_cast<SetThreadInformationFunction>(
            dll.GetFunctionPointer("SetThreadInformation"));
    if (!set_thread_info_function)
      return false;

    // NOTE: Must use the pseudo-handle here, a thread HANDLE won't work.
    if (!set_thread_info_function(::GetCurrentThread(), ThreadDynamicCodePolicy,
                                  &thread_policy, sizeof(thread_policy))) {
      return false;
    }
  }

  return true;
}

void ConvertProcessMitigationsToPolicy(MitigationFlags flags,
                                       DWORD64* policy_flags,
                                       size_t* size) {
  base::win::Version version = base::win::GetVersion();

  // |policy_flags| is a two-element array of DWORD64s.  Ensure mitigation flags
  // from PROCESS_CREATION_MITIGATION_POLICY2_* go into the second value.  If
  // any flags are set in value 2, update |size| to include both elements.
  DWORD64* policy_value_1 = &policy_flags[0];
  DWORD64* policy_value_2 = &policy_flags[1];
  *policy_value_1 = 0;
  *policy_value_2 = 0;

#if defined(_WIN64)
  *size = sizeof(*policy_flags);
#elif defined(_M_IX86)
  // A 64-bit flags attribute is illegal on 32-bit Win 7.
  if (version < base::win::Version::WIN8)
    *size = sizeof(DWORD);
  else
    *size = sizeof(*policy_flags);
#else
#error This platform is not supported.
#endif

// DEP and SEHOP are not valid for 64-bit Windows
#if !defined(_WIN64)
  if (flags & MITIGATION_DEP) {
    *policy_value_1 |= PROCESS_CREATION_MITIGATION_POLICY_DEP_ENABLE;
    if (!(flags & MITIGATION_DEP_NO_ATL_THUNK))
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_DEP_ATL_THUNK_ENABLE;
  }

  if (flags & MITIGATION_SEHOP)
    *policy_value_1 |= PROCESS_CREATION_MITIGATION_POLICY_SEHOP_ENABLE;
#endif

  // Win 7
  if (version < base::win::Version::WIN8)
    return;

  // Everything >= Win8, do not return before the end of the function where
  // the final policy bitmap is sanity checked against what is supported on this
  // machine.  The API required to do so is only available since Win8.

  // Mitigations >= Win8:
  //----------------------------------------------------------------------------
  if (version >= base::win::Version::WIN8) {
    if (flags & MITIGATION_RELOCATE_IMAGE) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_FORCE_RELOCATE_IMAGES_ALWAYS_ON;
      if (flags & MITIGATION_RELOCATE_IMAGE_REQUIRED) {
        *policy_value_1 |=
            PROCESS_CREATION_MITIGATION_POLICY_FORCE_RELOCATE_IMAGES_ALWAYS_ON_REQ_RELOCS;
      }
    }

    if (flags & MITIGATION_HEAP_TERMINATE) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_HEAP_TERMINATE_ALWAYS_ON;
    }

    if (flags & MITIGATION_BOTTOM_UP_ASLR) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_ON;
    }

    if (flags & MITIGATION_HIGH_ENTROPY_ASLR) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_ON;
    }

    if (flags & MITIGATION_STRICT_HANDLE_CHECKS) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_STRICT_HANDLE_CHECKS_ALWAYS_ON;
    }

    if (flags & MITIGATION_WIN32K_DISABLE) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_WIN32K_SYSTEM_CALL_DISABLE_ALWAYS_ON;
    }

    if (flags & MITIGATION_EXTENSION_POINT_DISABLE) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE_ALWAYS_ON;
    }
  }

  // Mitigations >= Win8.1:
  //----------------------------------------------------------------------------
  if (version >= base::win::Version::WIN8_1) {
    if (flags & MITIGATION_DYNAMIC_CODE_DISABLE) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_PROHIBIT_DYNAMIC_CODE_ALWAYS_ON;
    }
  }

  // Mitigations >= Win10:
  //----------------------------------------------------------------------------
  if (version >= base::win::Version::WIN10) {
    if (flags & MITIGATION_NONSYSTEM_FONT_DISABLE) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_FONT_DISABLE_ALWAYS_ON;
    }
  }

  // Mitigations >= Win10 TH2:
  //----------------------------------------------------------------------------
  if (version >= base::win::Version::WIN10_TH2) {
    if (flags & MITIGATION_FORCE_MS_SIGNED_BINS) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_ON;
    }

    if (flags & MITIGATION_IMAGE_LOAD_NO_REMOTE) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_REMOTE_ALWAYS_ON;
    }

    if (flags & MITIGATION_IMAGE_LOAD_NO_LOW_LABEL) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_LOW_LABEL_ALWAYS_ON;
    }
  }

  // Mitigations >= Win10 RS1 ("Anniversary"):
  //----------------------------------------------------------------------------
  if (version >= base::win::Version::WIN10_RS1) {
    if (flags & MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_PROHIBIT_DYNAMIC_CODE_ALWAYS_ON_ALLOW_OPT_OUT;
    }

    if (flags & MITIGATION_IMAGE_LOAD_PREFER_SYS32) {
      *policy_value_1 |=
          PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON;
    }
  }

  // Mitigations >= Win10 RS3 ("Fall Creator's"):
  //----------------------------------------------------------------------------
  if (version >= base::win::Version::WIN10_RS3) {
    // Note: This mitigation requires not only Win10 1709, but also the January
    //       2018 security updates and any applicable firmware updates from the
    //       OEM device manufacturer.
    // Note: Applying this mitigation attribute on creation will succeed, even
    // if
    //       the underlying hardware does not support the implementation.
    //       Windows just does its best under the hood for the given hardware.
    if (flags & MITIGATION_RESTRICT_INDIRECT_BRANCH_PREDICTION) {
      *policy_value_2 |=
          PROCESS_CREATION_MITIGATION_POLICY2_RESTRICT_INDIRECT_BRANCH_PREDICTION_ALWAYS_ON;
    }
  }

  // When done setting policy flags, sanity check supported policies on this
  // machine, and then update |size|.

  const ULONG64* supported = GetSupportedMitigations();

  *policy_value_1 = *policy_value_1 & supported[0];
  *policy_value_2 = *policy_value_2 & supported[1];

  // Only include the second element in |size| if it is non-zero.  Else,
  // UpdateProcThreadAttribute() will return a failure when setting policies.
  if (*policy_value_2 && version >= base::win::Version::WIN10_RS2) {
    *size = sizeof(*policy_flags) * 2;
  }

  return;
}

MitigationFlags FilterPostStartupProcessMitigations(MitigationFlags flags) {
  base::win::Version version = base::win::GetVersion();

  // Windows 7.
  if (version < base::win::Version::WIN8) {
    return flags & (MITIGATION_BOTTOM_UP_ASLR | MITIGATION_DLL_SEARCH_ORDER |
                    MITIGATION_HEAP_TERMINATE);
  }

  // Windows 8 and above.
  return flags & (MITIGATION_BOTTOM_UP_ASLR | MITIGATION_DLL_SEARCH_ORDER);
}

bool ApplyProcessMitigationsToSuspendedProcess(HANDLE process,
                                               MitigationFlags flags) {
// This is a hack to fake a weak bottom-up ASLR on 32-bit Windows.
#if !defined(_WIN64)
  if (flags & MITIGATION_BOTTOM_UP_ASLR) {
    unsigned int limit;
    GetRandom(&limit);
    char* ptr = 0;
    const size_t kMask64k = 0xFFFF;
    // Random range (512k-16.5mb) in 64k steps.
    const char* end = ptr + ((((limit % 16384) + 512) * 1024) & ~kMask64k);
    while (ptr < end) {
      MEMORY_BASIC_INFORMATION memory_info;
      if (!::VirtualQueryEx(process, ptr, &memory_info, sizeof(memory_info)))
        break;
      size_t size = std::min((memory_info.RegionSize + kMask64k) & ~kMask64k,
                             static_cast<SIZE_T>(end - ptr));
      if (ptr && memory_info.State == MEM_FREE)
        ::VirtualAllocEx(process, ptr, size, MEM_RESERVE, PAGE_NOACCESS);
      ptr += size;
    }
  }
#endif

  return true;
}

MitigationFlags GetAllowedPostStartupProcessMitigations() {
  return MITIGATION_HEAP_TERMINATE |
         MITIGATION_DEP |
         MITIGATION_DEP_NO_ATL_THUNK |
         MITIGATION_RELOCATE_IMAGE |
         MITIGATION_RELOCATE_IMAGE_REQUIRED |
         MITIGATION_BOTTOM_UP_ASLR |
         MITIGATION_STRICT_HANDLE_CHECKS |
         MITIGATION_EXTENSION_POINT_DISABLE |
         MITIGATION_DLL_SEARCH_ORDER |
         MITIGATION_HARDEN_TOKEN_IL_POLICY |
         MITIGATION_WIN32K_DISABLE |
         MITIGATION_DYNAMIC_CODE_DISABLE |
         MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT |
         MITIGATION_FORCE_MS_SIGNED_BINS |
         MITIGATION_NONSYSTEM_FONT_DISABLE |
         MITIGATION_IMAGE_LOAD_NO_REMOTE |
         MITIGATION_IMAGE_LOAD_NO_LOW_LABEL |
         MITIGATION_IMAGE_LOAD_PREFER_SYS32;
}

bool CanSetProcessMitigationsPostStartup(MitigationFlags flags) {
  // All of these mitigations can be enabled after startup.
  return !(flags & ~GetAllowedPostStartupProcessMitigations());
}

bool CanSetProcessMitigationsPreStartup(MitigationFlags flags) {
  // These mitigations cannot be enabled prior to startup.
  return !(flags &
           (MITIGATION_STRICT_HANDLE_CHECKS | MITIGATION_DLL_SEARCH_ORDER));
}

bool CanSetMitigationsPerThread(MitigationFlags flags) {
  // If any flags EXCEPT these are set, fail.
  if (flags & ~(MITIGATION_DYNAMIC_CODE_OPT_OUT_THIS_THREAD))
    return false;

  return true;
}

}  // namespace sandbox
