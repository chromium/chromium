// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_host/utility_sandbox_delegate.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_handle_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/sandboxed_process_launcher_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/utility/sandbox_delegate_data.mojom.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
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
//  job_level_(sandbox::JobLevel::kLockdown),
//
//  integrity_level_(sandbox::INTEGRITY_LEVEL_LOW),
//  delayed_integrity_level_(sandbox::INTEGRITY_LEVEL_UNTRUSTED),
bool AudioInitializeConfig(sandbox::TargetConfig* config) {
  // Audio process privilege requirements:
  //  - Lockdown level of USER_NON_ADMIN
  //  - Delayed integrity level of INTEGRITY_LEVEL_LOW
  //
  // For audio streams to create shared memory regions, lockdown level must be
  // at least USER_LIMITED and delayed integrity level INTEGRITY_LEVEL_LOW,
  // otherwise CreateFileMapping() will fail with error code
  // ERROR_ACCESS_DENIED (0x5).
  //
  // For audio input streams to use ISimpleAudioVolume interface, lockdown
  // level must be set to USER_NON_ADMIN, otherwise
  // WASAPIAudioInputStream::Open() will fail with error code E_ACCESSDENIED
  // (0x80070005) when trying to get a reference to ISimpleAudioVolume
  // interface. See
  // https://cs.chromium.org/chromium/src/media/audio/win/audio_low_latency_input_win.cc
  // Use USER_RESTRICTED_NON_ADMIN over USER_NON_ADMIN to prevent failures when
  // AppLocker and similar application whitelisting solutions are in place.
  DCHECK(!config->IsConfigured());

  // Custom default policy allowing audio drivers to read device properties
  // (https://crbug.com/883326).
  auto result = config->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }
  config->SetLockdownDefaultDacl();
  config->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  result = config->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                 sandbox::USER_RESTRICTED_NON_ADMIN);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }

  config->SetDesktop(sandbox::Desktop::kAlternateWinstation);

  return true;
}

// Sets the sandbox policy for the network service process.
bool NetworkInitializeConfig(sandbox::TargetConfig* config) {
  DCHECK(!config->IsConfigured());
  // LPAC sandbox is enabled, so do not use a restricted token.
  auto result = config->SetTokenLevel(sandbox::USER_UNPROTECTED,
                                      sandbox::USER_UNPROTECTED);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }
  // Network Sandbox in LPAC sandbox needs access to its data files. These
  // files are marked on disk with an ACE that permits this access.
  auto lpac_capability =
      GetContentClient()->browser()->GetLPACCapabilityNameForNetworkService();
  if (lpac_capability.empty()) {
    return false;
  }
  auto* app_container = config->GetAppContainer();
  if (!app_container) {
    return false;
  }
  app_container->AddCapability(lpac_capability);

  // Add capability SID for 'network_service' for loopback access for testing.
  // Run 'checkNetIsolation.exe loopbackExempt -a -n=network_service' while
  // elevated to allow network service loopback access.
  // TODO(wfh): Remove this once the socket broker has landed. See
  // https://crbug.com/841001.
  app_container->AddCapabilitySddl(
      L"S-1-15-3-893703388-718787801-2109771152-172907555-2119217564-716812919-"
      L"652991501");

  // All other app container policies are set in
  // SandboxWin::StartSandboxedProcess.
  return true;
}

// Sets the sandbox policy for the print backend service process.
bool PrintBackendInitializeConfig(sandbox::TargetConfig* config) {
  DCHECK(!config->IsConfigured());
  // Print Backend policy lockdown level must be at least USER_LIMITED and
  // delayed integrity level INTEGRITY_LEVEL_LOW, otherwise ::OpenPrinter()
  // will fail with error code ERROR_ACCESS_DENIED (0x5).
  auto result = config->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                      sandbox::USER_LIMITED);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }
  config->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  return true;
}

std::string UtilityAppContainerId(base::CommandLine& cmd_line) {
  return base::WideToUTF8(cmd_line.GetProgram().value());
}

bool IconReaderInitializeConfig(sandbox::TargetConfig* config) {
  DCHECK(!config->IsConfigured());

  auto result = config->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                      sandbox::USER_LOCKDOWN);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }
  config->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_UNTRUSTED);
  result = config->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }
  config->SetLockdownDefaultDacl();
  config->SetDesktop(sandbox::Desktop::kAlternateWinstation);

  sandbox::MitigationFlags flags = config->GetDelayedProcessMitigations();
  flags |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
  result = config->SetDelayedProcessMitigations(flags);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }
  return true;
}

bool OnDeviceModelExecutionInitializeConfig(
    sandbox::TargetConfig* config,
    base::CommandLine& cmd_line,
    sandbox::mojom::Sandbox sandbox_type) {
  DCHECK(!config->IsConfigured());
  // USER_RESTRICTED breaks the Direct3D backend, so for now we can only go as
  // low as USER_LIMITED.
  sandbox::ResultCode result = config->SetTokenLevel(
      sandbox::USER_RESTRICTED_SAME_ACCESS, sandbox::USER_LIMITED);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }
  return true;
}

bool WebNNModelCompilationInitializeConfig(sandbox::TargetConfig* config) {
  DCHECK(!config->IsConfigured());

  // Two-stage WebNN compiler sandbox (see crbug.com/500769395 and
  // content/utility/webnn/webnn_sandbox_init.cc):
  //
  //   1. Process creation: this function. The process runs inside an LPAC
  //      with the capability set configured by
  //      sandbox::policy::SandboxWin::SetupAppContainerProfile() (chrome
  //      install files, registry-read). Token is USER_RESTRICTED_SAME_ACCESS
  //      (initial) / USER_LOCKDOWN (lockdown). Win32k is disabled as a startup
  //      mitigation.
  //   2. Pre-LowerToken: in the child, content/utility/utility_main.cc
  //      calls webnn::PreSandboxInit() to invoke the helper functions
  //      that load third-party execution providers (today the ONNX
  //      Runtime; LiteRT and other backends may follow). LowerToken()
  //      then drops the token to USER_LOCKDOWN and applies the delayed
  //      mitigations (MITIGATION_DYNAMIC_CODE_DISABLE, plus
  //      MITIGATION_FORCE_MS_SIGNED_BINS unless
  //      --allow-third-party-modules is set). No delayed integrity
  //      level is configured: under LPAC the integrity is governed by
  //      the AppContainer token itself, so LowerToken's
  //      SetProcessIntegrityLevel call is a no-op for this process.
  //
  // Pre-launch Code Integrity Guard (MITIGATION_FORCE_MS_SIGNED_BINS as a
  // startup mitigation) is *not* applied here. Doing so would block the
  // loader from mapping chrome.dll and chrome_elf.dll, since neither is
  // Microsoft-signed. Startup CIG is instead wired through
  // ChromeContentBrowserClient::PreSpawnChild(), which pairs it with
  // AllowExtraDll() calls for those two DLLs and lets developers opt out
  // via --allow-third-party-modules (the same switch that disables the
  // delayed MITIGATION_FORCE_MS_SIGNED_BINS in
  // sandbox::policy::SandboxWin).
  //
  // Because LPAC is enabled (see GetAppContainerId() below) the broker
  // skips AddDefaultConfigForSandboxedProcess(), so this function is
  // responsible for the entire per-process configuration. The LPAC
  // capability SIDs and the LPAC's own integrity-level semantics provide
  // access control instead of the default INTEGRITY_LEVEL_LOW /
  // INTEGRITY_LEVEL_UNTRUSTED + kDeviceApi closure that non-LPAC utility
  // processes inherit; the lockdown DACL is re-applied explicitly below.

  // Rely on LPAC (not USER_LIMITED) to limit access to C:\Users\* during
  // stage 2, while keeping USER_LOCKDOWN as the post-LowerToken token
  // level for the tightest restriction (no new handles can be opened).
  // These are also the same values AddDefaultConfigForSandboxedProcess()
  // would have set; we set them explicitly here because the default
  // config is skipped under LPAC.
  sandbox::ResultCode result = config->SetTokenLevel(
      sandbox::USER_RESTRICTED_SAME_ACCESS, sandbox::USER_LOCKDOWN);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }

  // Add MITIGATION_WIN32K_DISABLE to the *startup* mitigation set
  // (AddWin32kLockdownPolicy calls SetProcessMitigations, not
  // SetDelayedProcessMitigations). The broker passes this through
  // PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY to CreateProcess, so the kernel
  // rejects every Win32k syscall from the child from its very first
  // instruction of user code. This is safe because the third-party
  // execution-provider preload helper invoked in stage 2 only needs
  // D3D12 / DirectML / DXCore code paths, none of which require user32
  // or gdi32.
  result = sandbox::policy::SandboxWin::AddWin32kLockdownPolicy(config);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }

  // No UI work is permitted; once Win32k is disabled the per-UI job
  // limits are redundant, so ask for the strictest job level with no UI
  // flags. The broker's GenerateConfigForSandboxedProcess() already
  // sets JobLevel::kLockdown for every sandbox; this is a defensive
  // re-assertion so that any future change to that default does not
  // silently weaken this profile.
  result = sandbox::policy::SandboxWin::SetJobLevel(
      sandbox::mojom::Sandbox::kWebNNModelCompilation,
      sandbox::JobLevel::kLockdown, /*ui_exceptions=*/0, config);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }

  // Restore the default-DACL hardening normally supplied by
  // AddDefaultConfigForSandboxedProcess(), which is skipped under LPAC.
  // This locks down the default DACL of kernel objects this process
  // creates so that other processes can't open handles into them.
  config->SetLockdownDefaultDacl();

  return true;
}

bool XrCompositingInitializeConfig(sandbox::TargetConfig* config,
                                   base::CommandLine& cmd_line,
                                   sandbox::mojom::Sandbox sandbox_type) {
  DCHECK(!config->IsConfigured());
  // TODO(crbug.com/41412553): Try to harden the XR Compositor
  // sandbox to use mitigations and restrict the token.

  // Unprotected token/job.
  auto result = config->SetTokenLevel(sandbox::USER_UNPROTECTED,
                                      sandbox::USER_UNPROTECTED);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }

  result = sandbox::policy::SandboxWin::SetJobLevel(
      sandbox_type, sandbox::JobLevel::kUnprotected, 0, config);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }

  // There were issues with some mitigations, causing an inability
  // to load OpenVR and Oculus APIs.
  result = config->SetProcessMitigations(0);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }

  result = config->SetDelayedProcessMitigations(0);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }

  std::string appcontainer_id = UtilityAppContainerId(cmd_line);
  result = sandbox::policy::SandboxWin::AddAppContainerProfileToConfig(
      cmd_line, sandbox_type, appcontainer_id, config);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }

  return true;
}

bool ScreenAIInitializeConfig(sandbox::TargetConfig* config,
                              sandbox::mojom::Sandbox sandbox_type) {
  DCHECK(!config->IsConfigured());

  auto result = config->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                      sandbox::USER_LOCKDOWN);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }

  result = sandbox::policy::SandboxWin::SetJobLevel(
      sandbox_type, sandbox::JobLevel::kLimitedUser, 0, config);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }

  return true;
}

}  // namespace

std::string UtilitySandboxedProcessLauncherDelegate::GetSandboxTag() {
  return sandbox::policy::SandboxWin::GetSandboxTagForDelegate(
      "utility", GetSandboxType());
}

bool UtilitySandboxedProcessLauncherDelegate::GetAppContainerId(
    std::string* appcontainer_id) {
  if (app_container_disabled_) {
    return false;
  }
  switch (sandbox_type_) {
    case sandbox::mojom::Sandbox::kMediaFoundationCdm:
    case sandbox::mojom::Sandbox::kNetwork:
    case sandbox::mojom::Sandbox::kOnDeviceModelExecution:
    case sandbox::mojom::Sandbox::kProxyResolver:
    case sandbox::mojom::Sandbox::kWebNNModelCompilation:
    case sandbox::mojom::Sandbox::kXrCompositing:
      *appcontainer_id = UtilityAppContainerId(cmd_line_);
      return true;
    case sandbox::mojom::Sandbox::kPrintCompositor:
      if (base::FeatureList::IsEnabled(
              sandbox::policy::features::kPrintCompositorLPAC)) {
        *appcontainer_id = UtilityAppContainerId(cmd_line_);
        return true;
      }
      return false;
    default:
      return false;
  }
}

bool UtilitySandboxedProcessLauncherDelegate::DisableDefaultPolicy() {
  std::string app_container_id;
  // Default policy is always disabled if App Container is enabled.
  if (GetAppContainerId(&app_container_id)) {
    return true;
  }
  switch (sandbox_type_) {
    case sandbox::mojom::Sandbox::kAudio:
      // Default policy is disabled for audio process to allow audio drivers
      // to read device properties (https://crbug.com/883326).
      return true;
    default:
      return false;
  }
}

bool UtilitySandboxedProcessLauncherDelegate::ShouldLaunchElevated() {
  return sandbox_type_ ==
         sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges;
}

bool UtilitySandboxedProcessLauncherDelegate::InitializeConfig(
    sandbox::TargetConfig* config) {
  DCHECK(!config->IsConfigured());
  if (sandbox_type_ == sandbox::mojom::Sandbox::kAudio) {
    if (!AudioInitializeConfig(config)) {
      return false;
    }
  }
  if (sandbox_type_ == sandbox::mojom::Sandbox::kNetwork) {
    if (!NetworkInitializeConfig(config)) {
      return false;
    }
  }
  if (sandbox_type_ == sandbox::mojom::Sandbox::kIconReader) {
    if (!IconReaderInitializeConfig(config)) {
      return false;
    }
  }

  if (sandbox_type_ == sandbox::mojom::Sandbox::kOnDeviceModelExecution) {
    if (!OnDeviceModelExecutionInitializeConfig(config, cmd_line_,
                                                sandbox_type_)) {
      return false;
    }
  }

  if (sandbox_type_ == sandbox::mojom::Sandbox::kWebNNModelCompilation) {
    if (!WebNNModelCompilationInitializeConfig(config)) {
      return false;
    }
  }

  if (sandbox_type_ == sandbox::mojom::Sandbox::kXrCompositing) {
    if (!XrCompositingInitializeConfig(config, cmd_line_, sandbox_type_)) {
      return false;
    }
  }

  if (sandbox_type_ == sandbox::mojom::Sandbox::kScreenAI) {
    if (!ScreenAIInitializeConfig(config, sandbox_type_)) {
      return false;
    }
  }

  if (sandbox_type_ == sandbox::mojom::Sandbox::kSpeechRecognition) {
    auto result = config->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    if (result != sandbox::SBOX_ALL_OK) {
      return false;
    }
    config->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    result = config->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                                   sandbox::USER_LIMITED);
    if (result != sandbox::SBOX_ALL_OK) {
      return false;
    }
  }

  if (sandbox_type_ == sandbox::mojom::Sandbox::kMediaFoundationCdm) {
    auto result = config->SetTokenLevel(sandbox::USER_UNPROTECTED,
                                        sandbox::USER_UNPROTECTED);
    if (result != sandbox::SBOX_ALL_OK) {
      return false;
    }
  }

  if (sandbox_type_ == sandbox::mojom::Sandbox::kProxyResolver) {
    // LPAC sandbox is enabled, so do not use a restricted token.
    auto result = config->SetTokenLevel(sandbox::USER_UNPROTECTED,
                                        sandbox::USER_UNPROTECTED);
    if (result != sandbox::SBOX_ALL_OK) {
      return false;
    }
  }

  if (sandbox_type_ == sandbox::mojom::Sandbox::kService ||
      sandbox_type_ == sandbox::mojom::Sandbox::kServiceWithJit ||
      (sandbox_type_ == sandbox::mojom::Sandbox::kSpeechRecognition &&
       base::FeatureList::IsEnabled(
           sandbox::policy::features::kSpeechRecognitionSandboxHardening))) {
    auto result = sandbox::policy::SandboxWin::AddWin32kLockdownPolicy(config);
    if (result != sandbox::SBOX_ALL_OK) {
      return false;
    }

    config->SetFilterEnvironment(true);
  }

  if (sandbox_type_ == sandbox::mojom::Sandbox::kService) {
    auto delayed_flags = config->GetDelayedProcessMitigations();
    delayed_flags |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
    auto result = config->SetDelayedProcessMitigations(delayed_flags);
    if (result != sandbox::SBOX_ALL_OK) {
      return false;
    }
  }

  if (sandbox_type_ == sandbox::mojom::Sandbox::kPrintBackend) {
    if (!PrintBackendInitializeConfig(config)) {
      return false;
    }
  }

  if (sandbox_type_ == sandbox::mojom::Sandbox::kPrintCompositor &&
      base::FeatureList::IsEnabled(
          sandbox::policy::features::kPrintCompositorLPAC) &&
      !app_container_disabled_) {
    // LPAC sandbox is enabled, so do not use a restricted token.
    auto result = config->SetTokenLevel(sandbox::USER_UNPROTECTED,
                                        sandbox::USER_UNPROTECTED);
    if (result != sandbox::SBOX_ALL_OK) {
      return false;
    }
  }

  return GetContentClient()->browser()->PreSpawnChild(
      config, sandbox_type_,
      ContentBrowserClient::ChildSpawnFlags::kChildSpawnFlagNone);
}

bool UtilitySandboxedProcessLauncherDelegate::ShouldUnsandboxedRunInJob() {
  auto utility_sub_type =
      cmd_line_.GetSwitchValueASCII(switches::kUtilitySubType);
  if (utility_sub_type == network::mojom::NetworkService::Name_) {
    return true;
  }
  return false;
}

bool UtilitySandboxedProcessLauncherDelegate::CetCompatible() {
  // TODO(crbug.com/40803284) can remove once v8 is cet-compatible.
  if (sandbox_type_ == sandbox::mojom::Sandbox::kServiceWithJit) {
    return false;
  }
  auto utility_sub_type =
      cmd_line_.GetSwitchValueASCII(switches::kUtilitySubType);
  return GetContentClient()->browser()->IsUtilityCetCompatible(
      utility_sub_type);
}

bool UtilitySandboxedProcessLauncherDelegate::PreSpawnTarget(
    sandbox::TargetPolicy* policy) {
  AddDelegateData(policy);
  return SandboxedProcessLauncherDelegate::PreSpawnTarget(policy);
}

void UtilitySandboxedProcessLauncherDelegate::SetBootstrapStatusEvent(
    const base::WaitableEvent& event) {
  CHECK(!event_handle_to_inherit_)
      << "SetBootstrapStatusEvent should only be called once.";
  HANDLE dup_handle;
  CHECK(::DuplicateHandle(
      ::GetCurrentProcess(), event.handle(), ::GetCurrentProcess(), &dup_handle,
      EVENT_MODIFY_STATE, /*bInheritHandle=*/TRUE, /*dwOptions=*/0));
  event_handle_to_inherit_.emplace(base::win::ScopedHandle(dup_handle));
}

void UtilitySandboxedProcessLauncherDelegate::AddDelegateData(
    sandbox::TargetPolicy* policy) {
  auto sandbox_config = content::mojom::sandbox::UtilityConfig::New();

  for (const auto& library_path : preload_libraries_) {
    sandbox_config->preload_libraries.push_back(library_path);
  }

  if (event_handle_to_inherit_) {
    sandbox_config->bootstrap_event_handle =
        base::win::HandleToUint32(event_handle_to_inherit_->Get());
    policy->AddHandleToShare(event_handle_to_inherit_->Get());
  }

  std::vector<uint8_t> blob =
      content::mojom::sandbox::UtilityConfig::Serialize(&sandbox_config);
  policy->AddDelegateData(blob);
}
}  // namespace content
