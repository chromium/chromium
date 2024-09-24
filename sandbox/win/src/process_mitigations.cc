// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/src/process_mitigations.h"

#include <windows.h>

#include <stddef.h>
#include <wow64apiset.h>

#include <algorithm>
#include <ostream>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/scoped_native_library.h"
#include "base/win/access_token.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/win_utils.h"

// These are missing in 10.0.19551.0 but are in 10.0.19041.0 and 10.0.20226.0.
#ifndef PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_STRICT_MODE
#define PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_STRICT_MODE \
  (0x00000003ui64 << 28)
#define PROCESS_CREATION_MITIGATION_POLICY2_CET_DYNAMIC_APIS_OUT_OF_PROC_ONLY_ALWAYS_OFF \
  (0x00000002ui64 << 48)
#endif

// From insider SDK 10.0.25295.0 and also from MSDN.
// TODO: crbug.com/1414570 Remove after updating SDK
#ifndef PROCESS_CREATION_MITIGATION_POLICY2_FSCTL_SYSTEM_CALL_DISABLE_ALWAYS_ON
#define PROCESS_CREATION_MITIGATION_POLICY2_FSCTL_SYSTEM_CALL_DISABLE_ALWAYS_ON \
  (0x00000001ui64 << 56)
#endif

namespace sandbox {

namespace {

// Returns a two-element array of mitigation flags supported on this machine.
const ULONG64* GetSupportedMitigations() {
  static ULONG64 mitigations[2] = {};

  // This static variable will only be initialized once.
  if (!mitigations[0] && !mitigations[1]) {
    // NOTE: the two-element-sized input array is only supported on >= Win10
    // RS2. If an earlier version, the second element will be left 0.
    size_t mits_size =
        (base::win::GetVersion() >= base::win::Version::WIN10_RS2)
            ? (sizeof(mitigations[0]) * 2)
            : sizeof(mitigations[0]);
    if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                      ProcessMitigationOptionsMask,
                                      &mitigations, mits_size)) {
      NOTREACHED();
    }
  }

  return &mitigations[0];
}

// Returns true if this is 32-bit Chrome running on ARM64 with emulation.
// Needed because ACG does not work with emulated code. This is not needed for
// x64 Chrome running on ARM64 with emulation.
// See
// https://learn.microsoft.com/en-us/windows/arm/apps-on-arm-troubleshooting-x86
// See https://crbug.com/977723.
bool IsRunning32bitEmulatedOnArm64() {
#if defined(ARCH_CPU_X86)
  return base::win::OSInfo::IsRunningEmulatedOnArm64();
#else
  return false;
#endif  // defined(ARCH_CPU_X86)
}

bool SetProcessMitigationPolicyInternal(PROCESS_MITIGATION_POLICY policy,
                                        PVOID lpBuffer,
                                        SIZE_T dwLength) {
  PCHECK(::SetProcessMitigationPolicy(policy, lpBuffer, dwLength))
      << "SetProcessMitigationPolicy failed with Policy: " << policy;

  return true;
}

bool ApplyProcessMitigationsToCurrentProcess(MitigationFlags starting_flags,
                                             MitigationFlags flags,
                                             MitigationFlags& applied_flags) {
  // Check to make sure we have new flags to apply
  MitigationFlags combined_flags = starting_flags | flags;
  if (combined_flags == starting_flags)
    return true;

  base::win::Version version = base::win::GetVersion();

  if (flags & MITIGATION_DLL_SEARCH_ORDER) {
#if defined(COMPONENT_BUILD)
    const DWORD directory_flags = LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
#else
    // In a non-component build, all DLLs will be loaded manually, or via
    // manifest definition, so these flags can be stronger. This prevents DLL
    // planting in the application directory.
    const DWORD directory_flags =
        LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS;
#endif
    if (!::SetDefaultDllDirectories(directory_flags)) {
      return false;
    }

    applied_flags |= MITIGATION_DLL_SEARCH_ORDER;
  }

  // Set the heap to terminate on corruption
  if (flags & MITIGATION_HEAP_TERMINATE) {
    if (!::HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption,
                              nullptr, 0)) {
      return false;
    }

    applied_flags |= MITIGATION_HEAP_TERMINATE;
  }

  if (flags & MITIGATION_HARDEN_TOKEN_IL_POLICY) {
    std::optional<base::win::AccessToken> token =
        base::win::AccessToken::FromCurrentProcess(/*impersonation=*/false,
                                                   READ_CONTROL | WRITE_OWNER);
    if (!token) {
      return false;
    }
    DWORD error = HardenTokenIntegrityLevelPolicy(*token);
    if (error != ERROR_SUCCESS) {
      return false;
    }
    applied_flags |= MITIGATION_HARDEN_TOKEN_IL_POLICY;
  }

#if !defined(_WIN64)  // DEP is always enabled on 64-bit.
  if (flags & MITIGATION_DEP) {
    DWORD dep_flags = PROCESS_DEP_ENABLE;

    if (combined_flags & MITIGATION_DEP_NO_ATL_THUNK)
      dep_flags |= PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION;

    if (!::SetProcessDEPPolicy(dep_flags)) {
      return false;
    }

    applied_flags |=
        combined_flags & (MITIGATION_DEP | MITIGATION_DEP_NO_ATL_THUNK);
  }
#endif

  // Enable ASLR policies.
  if (flags & MITIGATION_RELOCATE_IMAGE) {
    PROCESS_MITIGATION_ASLR_POLICY policy = {};
    policy.EnableForceRelocateImages = true;
    policy.DisallowStrippedImages =
        (combined_flags & MITIGATION_RELOCATE_IMAGE_REQUIRED) ==
        MITIGATION_RELOCATE_IMAGE_REQUIRED;

    policy.EnableBottomUpRandomization =
        (combined_flags & MITIGATION_BOTTOM_UP_ASLR) ==
        MITIGATION_BOTTOM_UP_ASLR;
    policy.EnableHighEntropy =
        (combined_flags & MITIGATION_HIGH_ENTROPY_ASLR) ==
        MITIGATION_HIGH_ENTROPY_ASLR;

    if (!SetProcessMitigationPolicyInternal(ProcessASLRPolicy, &policy,
                                            sizeof(policy))) {
      return false;
    }

    applied_flags |=
        combined_flags &
        (MITIGATION_RELOCATE_IMAGE | MITIGATION_RELOCATE_IMAGE_REQUIRED |
         MITIGATION_BOTTOM_UP_ASLR | MITIGATION_HIGH_ENTROPY_ASLR);
  }

  // Enable strict handle policies.
  if (flags & MITIGATION_STRICT_HANDLE_CHECKS) {
    PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY policy = {};
    policy.HandleExceptionsPermanentlyEnabled =
        policy.RaiseExceptionOnInvalidHandleReference = true;

    if (!SetProcessMitigationPolicyInternal(ProcessStrictHandleCheckPolicy,
                                            &policy, sizeof(policy))) {
      return false;
    }
    applied_flags |= MITIGATION_STRICT_HANDLE_CHECKS;
  }

  // Enable system call policies.
  if (flags & MITIGATION_WIN32K_DISABLE) {
    PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy = {};
    policy.DisallowWin32kSystemCalls = true;

    if (!SetProcessMitigationPolicyInternal(ProcessSystemCallDisablePolicy,
                                            &policy, sizeof(policy))) {
      return false;
    }
    applied_flags |= MITIGATION_WIN32K_DISABLE;
  }

  // Enable extension point policies.
  if (flags & MITIGATION_EXTENSION_POINT_DISABLE) {
    PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy = {};
    policy.DisableExtensionPoints = true;

    if (!SetProcessMitigationPolicyInternal(ProcessExtensionPointDisablePolicy,
                                            &policy, sizeof(policy))) {
      return false;
    }
    applied_flags |= MITIGATION_EXTENSION_POINT_DISABLE;
  }

  // Enable dynamic code policies.
  if (!IsRunning32bitEmulatedOnArm64() &&
      (flags & MITIGATION_DYNAMIC_CODE_DISABLE)) {
    // Verify caller is not accidentally setting both mutually exclusive
    // policies.
    DCHECK(!(flags & MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT));
    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY policy = {};
    policy.ProhibitDynamicCode = true;

    if (!SetProcessMitigationPolicyInternal(ProcessDynamicCodePolicy, &policy,
                                            sizeof(policy))) {
      return false;
    }
    applied_flags |= MITIGATION_DYNAMIC_CODE_DISABLE;
  }

  // Enable font policies.
  if (flags & MITIGATION_NONSYSTEM_FONT_DISABLE) {
    PROCESS_MITIGATION_FONT_DISABLE_POLICY policy = {};
    policy.DisableNonSystemFonts = true;

    if (!SetProcessMitigationPolicyInternal(ProcessFontDisablePolicy, &policy,
                                            sizeof(policy))) {
      return false;
    }
    applied_flags |= MITIGATION_NONSYSTEM_FONT_DISABLE;
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
    if (!SetProcessMitigationPolicyInternal(ProcessSignaturePolicy, &policy,
                                            sizeof(policy))) {
      return false;
    }
    applied_flags |= MITIGATION_FORCE_MS_SIGNED_BINS;
  }

  // Enable image load policies.
  if (flags & MITIGATION_IMAGE_LOAD_NO_REMOTE ||
      flags & MITIGATION_IMAGE_LOAD_NO_LOW_LABEL ||
      flags & MITIGATION_IMAGE_LOAD_PREFER_SYS32) {
    PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy = {};
    if (combined_flags & MITIGATION_IMAGE_LOAD_NO_REMOTE)
      policy.NoRemoteImages = true;
    if (combined_flags & MITIGATION_IMAGE_LOAD_NO_LOW_LABEL)
      policy.NoLowMandatoryLabelImages = true;
    // PreferSystem32 is only supported on >= Anniversary.
    if (version >= base::win::Version::WIN10_RS1 &&
        combined_flags & MITIGATION_IMAGE_LOAD_PREFER_SYS32) {
      policy.PreferSystem32Images = true;
    }

    if (!SetProcessMitigationPolicyInternal(ProcessImageLoadPolicy, &policy,
                                            sizeof(policy))) {
      return false;
    }
    applied_flags |= (combined_flags & (MITIGATION_IMAGE_LOAD_NO_REMOTE |
                                        MITIGATION_IMAGE_LOAD_NO_LOW_LABEL |
                                        MITIGATION_IMAGE_LOAD_PREFER_SYS32));
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

    if (!SetProcessMitigationPolicyInternal(ProcessDynamicCodePolicy, &policy,
                                            sizeof(policy))) {
      return false;
    }

    applied_flags |= MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT;
  }

  return true;
}

}  // namespace

SANDBOX_INTERCEPT MitigationFlags g_current_mitigations = 0;
SANDBOX_INTERCEPT MitigationFlags g_shared_startup_mitigations;

void SetStartingMitigations(MitigationFlags starting_flags) {
  DCHECK_EQ(g_shared_startup_mitigations, uint64_t{0});
  DCHECK_EQ(g_current_mitigations, uint64_t{0});

  g_current_mitigations = starting_flags;
}

bool RatchetDownSecurityMitigations(MitigationFlags additional_flags) {
  DCHECK_EQ(g_shared_startup_mitigations, uint64_t{0});
  if (!CanSetProcessMitigationsPostStartup(additional_flags))
    return false;

  return ApplyProcessMitigationsToCurrentProcess(
      g_current_mitigations, additional_flags, g_current_mitigations);
}

bool LockDownSecurityMitigations(MitigationFlags additional_flags) {
  DCHECK_EQ(g_current_mitigations, uint64_t{0});
  if (!CanSetProcessMitigationsPostStartup(additional_flags))
    return false;

  return ApplyProcessMitigationsToCurrentProcess(
      g_shared_startup_mitigations, additional_flags, g_current_mitigations);
}

bool ApplyMitigationsToCurrentThread(MitigationFlags flags) {
  if (!CanSetMitigationsPerThread(flags))
    return false;

  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return true;

  // Enable dynamic code per-thread policies.
  if (flags & MITIGATION_DYNAMIC_CODE_OPT_OUT_THIS_THREAD) {
    DWORD thread_policy = THREAD_DYNAMIC_CODE_ALLOW;

    // NOTE: Must use the pseudo-handle here, a thread HANDLE won't work.
    if (!::SetThreadInformation(::GetCurrentThread(), ThreadDynamicCodePolicy,
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

#if defined(_WIN64) || defined(_M_IX86)
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

  if (flags & MITIGATION_DYNAMIC_CODE_DISABLE) {
    *policy_value_1 |=
        PROCESS_CREATION_MITIGATION_POLICY_PROHIBIT_DYNAMIC_CODE_ALWAYS_ON;
  }

  if (flags & MITIGATION_NONSYSTEM_FONT_DISABLE) {
    *policy_value_1 |=
        PROCESS_CREATION_MITIGATION_POLICY_FONT_DISABLE_ALWAYS_ON;
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
    //       if the underlying hardware does not support the implementation.
    //       Windows just does its best under the hood for the given hardware.
    if (flags & MITIGATION_RESTRICT_INDIRECT_BRANCH_PREDICTION) {
      *policy_value_2 |=
          PROCESS_CREATION_MITIGATION_POLICY2_RESTRICT_INDIRECT_BRANCH_PREDICTION_ALWAYS_ON;
    }
  }

  // Mitigations >= Win10 20H1
  //----------------------------------------------------------------------------
  if (version >= base::win::Version::WIN10_20H1) {
    if (flags & MITIGATION_CET_DISABLED) {
      *policy_value_2 |=
          PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_ALWAYS_OFF;
    }

    if (flags & MITIGATION_CET_STRICT_MODE) {
      DCHECK(!(flags & MITIGATION_CET_DISABLED))
          << "Cannot enable CET strict mode if CET is disabled.";
      *policy_value_2 |=
          PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_STRICT_MODE;
    }

    if (flags & MITIGATION_CET_ALLOW_DYNAMIC_APIS) {
      DCHECK(!(flags & MITIGATION_CET_DISABLED))
          << "Cannot enable in-process CET apis if CET is disabled.";
      DCHECK(!(flags & MITIGATION_DYNAMIC_CODE_DISABLE))
          << "Cannot enable in-process CET apis if dynamic code is disabled.";
      *policy_value_2 |=
          PROCESS_CREATION_MITIGATION_POLICY2_CET_DYNAMIC_APIS_OUT_OF_PROC_ONLY_ALWAYS_OFF;
    }
  }

  // Mitigations >= Win10 22H2
  //----------------------------------------------------------------------------
  if (version >= base::win::Version::WIN10_22H2) {
    // Note that this mitigation requires not only Win10 22H2, but also a
    // servicing update [TBD].
    if (flags & MITIGATION_FSCTL_DISABLED) {
      *policy_value_2 |=
          PROCESS_CREATION_MITIGATION_POLICY2_FSCTL_SYSTEM_CALL_DISABLE_ALWAYS_ON;
    }
  }

  // This mitigation is supported on systems with no non-architectural core
  // sharing and have enabled support for SMT isolation scheduling.
  if (version >= base::win::Version::WIN11_23H2 &&
      flags & MITIGATION_RESTRICT_CORE_SHARING) {
    *policy_value_2 |=
        PROCESS_CREATION_MITIGATION_POLICY2_RESTRICT_CORE_SHARING_ALWAYS_ON;
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

void ConvertProcessMitigationsToComponentFilter(MitigationFlags flags,
                                                COMPONENT_FILTER* filter) {
  filter->ComponentFlags = 0;
  if (flags & MITIGATION_KTM_COMPONENT) {
    filter->ComponentFlags = COMPONENT_KTM;
  }
}

MitigationFlags FilterPostStartupProcessMitigations(MitigationFlags flags) {
  return flags & (MITIGATION_BOTTOM_UP_ASLR | MITIGATION_DLL_SEARCH_ORDER);
}

bool ApplyProcessMitigationsToSuspendedProcess(HANDLE process,
                                               MitigationFlags flags) {
// This is a hack to fake a weak bottom-up ASLR on 32-bit Windows.
#if !defined(_WIN64)
  if (flags & MITIGATION_BOTTOM_UP_ASLR) {
    char* ptr = 0;
    const size_t kMask64k = 0xFFFF;
    // Random range (512k-16.5mb) in 64k steps.
    auto limit = static_cast<unsigned int>(base::RandInt(512, 512 + 16384 - 1));
    const char* end = ptr + ((limit * 1024) & ~kMask64k);
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
