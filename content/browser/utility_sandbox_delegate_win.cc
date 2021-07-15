// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/utility_sandbox_delegate.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "media/mojo/mojom/cdm_service.mojom.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_types.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {
namespace {
// Audio:
//  lockdown_level_(sandbox::USER_LOCKDOWN),
//  initial_level_(sandbox::USER_RESTRICTED_SAME_ACCESS),
//
//  job_level_(sandbox::JOB_LOCKDOWN),
//
//  integrity_level_(sandbox::INTEGRITY_LEVEL_LOW),
//  delayed_integrity_level_(sandbox::INTEGRITY_LEVEL_UNTRUSTED),
bool AudioPreSpawnTarget(sandbox::TargetPolicy* policy) {
  // Audio process privilege requirements:
  //  - Lockdown level of USER_NON_ADMIN
  //  - Delayed integrity level of INTEGRITY_LEVEL_LOW
  //
  // For audio streams to create shared memory regions, lockdown level must be
  // at least USER_LIMITED and delayed integrity level INTEGRITY_LEVEL_LOW,
  // otherwise CreateFileMapping() will fail with error code ERROR_ACCESS_DENIED
  // (0x5).
  //
  // For audio input streams to use ISimpleAudioVolume interface, lockdown
  // level must be set to USER_NON_ADMIN, otherwise
  // WASAPIAudioInputStream::Open() will fail with error code E_ACCESSDENIED
  // (0x80070005) when trying to get a reference to ISimpleAudioVolume
  // interface. See
  // https://cs.chromium.org/chromium/src/media/audio/win/audio_low_latency_input_win.cc
  // Use USER_RESTRICTED_NON_ADMIN over USER_NON_ADMIN to prevent failures when
  // AppLocker and similar application whitelisting solutions are in place.
  policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                        sandbox::USER_RESTRICTED_NON_ADMIN);
  policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);

  // Custom default policy allowing audio drivers to read device properties
  // (https://crbug.com/883326).
  policy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  policy->SetLockdownDefaultDacl();
  policy->SetAlternateDesktop(true);

  return true;
}

// Sets the sandbox policy for the network service process.
bool NetworkPreSpawnTarget(sandbox::TargetPolicy* policy) {
  if (sandbox::policy::features::IsNetworkServiceSandboxLPACEnabled()) {
    // LPAC sandbox is enabled, so do not use a restricted token.
    if (sandbox::SBOX_ALL_OK !=
        policy->SetTokenLevel(sandbox::USER_UNPROTECTED,
                              sandbox::USER_UNPROTECTED)) {
      return false;
    }

    // Network Sandbox in LPAC sandbox needs access to its data files. These
    // files are marked on disk with an ACE that permits this access.
    auto lpacCapability =
        GetContentClient()->browser()->GetLPACCapabilityNameForNetworkService();
    if (lpacCapability.empty())
      return false;
    auto app_container = policy->GetAppContainer();
    if (!app_container)
      return false;
    app_container->AddCapability(lpacCapability.c_str());

    // All other app container policies are set in
    // SandboxWin::StartSandboxedProcess.
    return true;
  }

  // USER_LIMITED is as tight as this sandbox can be, because
  // DNS running in-process is blocked by USER_RESTRICTED and
  // below as it can't connect to the service.
  if (policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                            sandbox::USER_LIMITED) != sandbox::SBOX_ALL_OK)
    return false;

  auto permitted_paths =
      GetContentClient()->browser()->GetNetworkContextsParentDirectory();

  for (auto permitted_path : permitted_paths) {
    permitted_path = permitted_path.Append(FILE_PATH_LITERAL("*"));
    if (policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                        sandbox::TargetPolicy::FILES_ALLOW_ANY,
                        permitted_path.value().c_str()) !=
        sandbox::SBOX_ALL_OK) {
      return false;
    }
  }

  // DNS needs to read policies which are ACLed NT AUTHORITY\Authenticated Users
  // Allow ReadKey unlike the rest of HKLM which is BUILTIN\Users Allow ReadKey.
  if (policy->AddRule(
          sandbox::TargetPolicy::SUBSYS_REGISTRY,
          sandbox::TargetPolicy::REG_ALLOW_READONLY,
          L"HKEY_LOCAL_MACHINE\\Software\\Policies\\Microsoft\\*") !=
      sandbox::SBOX_ALL_OK) {
    return false;
  }

  // Needed for ::GetAdaptersAddresses calls in //net.
  if (policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                      sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                      L"\\DEVICE\\NETBT_TCPIP\\*") != sandbox::SBOX_ALL_OK) {
    return false;
  }
  return true;
}

// Sets the sandbox policy for the print backend service process.
bool PrintBackendPreSpawnTarget(sandbox::TargetPolicy* policy) {
  // Print Backend policy lockdown level must be at least USER_LIMITED and
  // delayed integrity level INTEGRITY_LEVEL_LOW, otherwise ::OpenPrinter()
  // will fail with error code ERROR_ACCESS_DENIED (0x5).
  policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                        sandbox::USER_LIMITED);
  policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  return true;
}
}  // namespace

bool UtilitySandboxedProcessLauncherDelegate::GetAppContainerId(
    std::string* appcontainer_id) {
  if (sandbox_type_ == sandbox::policy::SandboxType::kNetwork) {
    DCHECK(sandbox::policy::features::IsNetworkServiceSandboxLPACEnabled());
    *appcontainer_id = base::WideToUTF8(cmd_line_.GetProgram().value());
    return true;
  }

  if ((sandbox_type_ == sandbox::policy::SandboxType::kXrCompositing &&
       base::FeatureList::IsEnabled(sandbox::policy::features::kXRSandbox)) ||
      sandbox_type_ == sandbox::policy::SandboxType::kMediaFoundationCdm) {
    *appcontainer_id = base::WideToUTF8(cmd_line_.GetProgram().value());
    return true;
  }
  return false;
}

bool UtilitySandboxedProcessLauncherDelegate::DisableDefaultPolicy() {
  switch (sandbox_type_) {
    case sandbox::policy::SandboxType::kAudio:
      // Default policy is disabled for audio process to allow audio drivers
      // to read device properties (https://crbug.com/883326).
      return true;
    case sandbox::policy::SandboxType::kXrCompositing:
      return base::FeatureList::IsEnabled(
          sandbox::policy::features::kXRSandbox);
    case sandbox::policy::SandboxType::kMediaFoundationCdm:
      // Default policy is disabled for MF Cdm process to allow the application
      // of specific LPAC sandbox policies.
      return true;
    case sandbox::policy::SandboxType::kNetwork:
      // If LPAC is enabled for network sandbox then LPAC-specific policy is set
      // elsewhere.
      return sandbox::policy::features::IsNetworkServiceSandboxLPACEnabled();
    default:
      return false;
  }
}

bool UtilitySandboxedProcessLauncherDelegate::ShouldLaunchElevated() {
  return sandbox_type_ ==
         sandbox::policy::SandboxType::kNoSandboxAndElevatedPrivileges;
}

bool UtilitySandboxedProcessLauncherDelegate::PreSpawnTarget(
    sandbox::TargetPolicy* policy) {
  if (sandbox_type_ == sandbox::policy::SandboxType::kNetwork) {
    if (!NetworkPreSpawnTarget(policy))
      return false;
  }

  if (sandbox_type_ == sandbox::policy::SandboxType::kAudio) {
    if (!AudioPreSpawnTarget(policy))
      return false;
  }

  if (sandbox_type_ == sandbox::policy::SandboxType::kProxyResolver) {
    sandbox::MitigationFlags flags = policy->GetDelayedProcessMitigations();
    flags |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
    if (sandbox::SBOX_ALL_OK != policy->SetDelayedProcessMitigations(flags))
      return false;
  }

  if (sandbox_type_ == sandbox::policy::SandboxType::kSpeechRecognition) {
    policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    policy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                          sandbox::USER_LIMITED);
  }

  if (sandbox_type_ == sandbox::policy::SandboxType::kIconReader) {
    policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                          sandbox::USER_LOCKDOWN);
    policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);
    policy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    policy->SetLockdownDefaultDacl();
    policy->SetAlternateDesktop(true);

    sandbox::MitigationFlags flags = policy->GetDelayedProcessMitigations();
    flags |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
    if (sandbox::SBOX_ALL_OK != policy->SetDelayedProcessMitigations(flags))
      return false;

    // Allow file read. These should match IconLoader::GroupForFilepath().
    policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                    sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                    L"\\??\\*.exe");
    policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                    sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                    L"\\??\\*.dll");
    policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                    sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                    L"\\??\\*.ico");
  }

  if (sandbox_type_ == sandbox::policy::SandboxType::kXrCompositing &&
      base::FeatureList::IsEnabled(sandbox::policy::features::kXRSandbox)) {
    // There were issues with some mitigations, causing an inability
    // to load OpenVR and Oculus APIs.
    // TODO(https://crbug.com/881919): Try to harden the XR Compositor
    // sandbox to use mitigations and restrict the token.
    policy->SetProcessMitigations(0);
    policy->SetDelayedProcessMitigations(0);

    std::string appcontainer_id;
    if (!GetAppContainerId(&appcontainer_id)) {
      return false;
    }
    sandbox::ResultCode result =
        sandbox::policy::SandboxWin::AddAppContainerProfileToPolicy(
            cmd_line_, sandbox_type_, appcontainer_id, policy);
    if (result != sandbox::SBOX_ALL_OK) {
      return false;
    }

    // Unprotected token/job.
    policy->SetTokenLevel(sandbox::USER_UNPROTECTED, sandbox::USER_UNPROTECTED);
    sandbox::policy::SandboxWin::SetJobLevel(
        cmd_line_, sandbox::JOB_UNPROTECTED, 0, policy);
  }

  if (sandbox_type_ == sandbox::policy::SandboxType::kMediaFoundationCdm) {
    policy->SetTokenLevel(sandbox::USER_UNPROTECTED, sandbox::USER_UNPROTECTED);
  }

  if (sandbox_type_ == sandbox::policy::SandboxType::kService) {
    auto result = sandbox::policy::SandboxWin::AddWin32kLockdownPolicy(policy);
    if (result != sandbox::SBOX_ALL_OK)
      return false;

    auto delayed_flags = policy->GetDelayedProcessMitigations();
    delayed_flags |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
    result = policy->SetDelayedProcessMitigations(delayed_flags);
    if (result != sandbox::SBOX_ALL_OK)
      return false;
  }

#if BUILDFLAG(ENABLE_PRINTING)
  if (sandbox_type_ == sandbox::policy::SandboxType::kPrintBackend) {
    if (!PrintBackendPreSpawnTarget(policy))
      return false;
  }
#endif

  return GetContentClient()->browser()->PreSpawnChild(
      policy, sandbox_type_, ContentBrowserClient::ChildSpawnFlags::NONE);
}

bool UtilitySandboxedProcessLauncherDelegate::ShouldUnsandboxedRunInJob() {
  auto utility_sub_type =
      cmd_line_.GetSwitchValueASCII(switches::kUtilitySubType);
  if (utility_sub_type == network::mojom::NetworkService::Name_)
    return true;
  return false;
}

bool UtilitySandboxedProcessLauncherDelegate::CetCompatible() {
  auto utility_sub_type =
      cmd_line_.GetSwitchValueASCII(switches::kUtilitySubType);
  return GetContentClient()->browser()->IsUtilityCetCompatible(
      utility_sub_type);
}
}  // namespace content
