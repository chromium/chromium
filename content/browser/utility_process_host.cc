// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/utility_process_host.h"

#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_manager/service_manager_context.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/service_manager/child_connection.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "media/base/media_switches.h"
#include "media/webrtc/webrtc_switches.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/sandbox/features.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/sandbox/switches.h"
#include "services/service_manager/zygote/common/zygote_buildflags.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_MACOSX)
#include "components/os_crypt/os_crypt_switches.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_types.h"
#include "services/audio/audio_sandbox_win.h"
#include "services/network/network_sandbox_win.h"
#endif

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "services/service_manager/zygote/common/zygote_handle.h"  // nogncheck
#endif

namespace content {

// NOTE: changes to this class need to be reviewed by the security team.
class UtilitySandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  UtilitySandboxedProcessLauncherDelegate(
      service_manager::SandboxType sandbox_type,
      const base::EnvironmentMap& env,
      const base::CommandLine& cmd_line)
      :
#if defined(OS_POSIX)
        env_(env),
#endif
        sandbox_type_(sandbox_type),
        cmd_line_(cmd_line) {
#if DCHECK_IS_ON()
    bool supported_sandbox_type =
        sandbox_type_ == service_manager::SANDBOX_TYPE_NO_SANDBOX ||
#if defined(OS_WIN)
        sandbox_type_ ==
            service_manager::SANDBOX_TYPE_NO_SANDBOX_AND_ELEVATED_PRIVILEGES ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_XRCOMPOSITING ||
#endif
        sandbox_type_ == service_manager::SANDBOX_TYPE_UTILITY ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_NETWORK ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_CDM ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_PDF_COMPOSITOR ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_PROFILING ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_PPAPI ||
#if defined(OS_CHROMEOS)
        sandbox_type_ == service_manager::SANDBOX_TYPE_IME ||
#endif  // OS_CHROMEOS
        sandbox_type_ == service_manager::SANDBOX_TYPE_AUDIO;
    DCHECK(supported_sandbox_type);
#endif  // DCHECK_IS_ON()
  }

  ~UtilitySandboxedProcessLauncherDelegate() override = default;

#if defined(OS_WIN)
  bool GetAppContainerId(std::string* appcontainer_id) override {
    if (sandbox_type_ == service_manager::SANDBOX_TYPE_XRCOMPOSITING &&
        base::FeatureList::IsEnabled(service_manager::features::kXRSandbox)) {
      *appcontainer_id = base::WideToUTF8(cmd_line_.GetProgram().value());
      return true;
    }
    return false;
  }

  bool DisableDefaultPolicy() override {
    switch (sandbox_type_) {
      case service_manager::SANDBOX_TYPE_AUDIO:
        // Default policy is disabled for audio process to allow audio drivers
        // to read device properties (https://crbug.com/883326).
        return true;
      case service_manager::SANDBOX_TYPE_NETWORK:
        // Default policy is disabled for network process to allow incremental
        // sandbox mitigations to be applied via experiments.
        return true;
      case service_manager::SANDBOX_TYPE_XRCOMPOSITING:
        return base::FeatureList::IsEnabled(
            service_manager::features::kXRSandbox);
      default:
        return false;
    }
  }

  bool ShouldLaunchElevated() override {
    return sandbox_type_ ==
           service_manager::SANDBOX_TYPE_NO_SANDBOX_AND_ELEVATED_PRIVILEGES;
  }

  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    if (sandbox_type_ == service_manager::SANDBOX_TYPE_NETWORK)
      return network::NetworkPreSpawnTarget(policy, cmd_line_);

    if (sandbox_type_ == service_manager::SANDBOX_TYPE_AUDIO)
      return audio::AudioPreSpawnTarget(policy);

    if (sandbox_type_ == service_manager::SANDBOX_TYPE_XRCOMPOSITING &&
        base::FeatureList::IsEnabled(service_manager::features::kXRSandbox)) {
      // There were issues with some mitigations, causing an inability
      // to load OpenVR and Oculus APIs.
      // TODO(https://crbug.com/881919): Try to harden the XR Compositor sandbox
      // to use mitigations and restrict the token.
      policy->SetProcessMitigations(0);
      policy->SetDelayedProcessMitigations(0);

      std::string appcontainer_id;
      if (!GetAppContainerId(&appcontainer_id)) {
        return false;
      }
      sandbox::ResultCode result =
          service_manager::SandboxWin::AddAppContainerProfileToPolicy(
              cmd_line_, sandbox_type_, appcontainer_id, policy);
      if (result != sandbox::SBOX_ALL_OK) {
        return false;
      }

      // Unprotected token/job.
      policy->SetTokenLevel(sandbox::USER_UNPROTECTED,
                            sandbox::USER_UNPROTECTED);
      service_manager::SandboxWin::SetJobLevel(
          cmd_line_, sandbox::JOB_UNPROTECTED, 0, policy);
    }
    return true;
  }
#endif  // OS_WIN

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  service_manager::ZygoteHandle GetZygote() override {
    if (service_manager::IsUnsandboxedSandboxType(sandbox_type_) ||
        sandbox_type_ == service_manager::SANDBOX_TYPE_NETWORK ||
#if defined(OS_CHROMEOS)
        sandbox_type_ == service_manager::SANDBOX_TYPE_IME ||
#endif  // OS_CHROMEOS
        sandbox_type_ == service_manager::SANDBOX_TYPE_AUDIO) {
      return nullptr;
    }
    return service_manager::GetGenericZygote();
  }
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#if defined(OS_POSIX)
  base::EnvironmentMap GetEnvironment() override { return env_; }
#endif  // OS_POSIX

  service_manager::SandboxType GetSandboxType() override {
    return sandbox_type_;
  }

 private:
#if defined(OS_POSIX)
  base::EnvironmentMap env_;
#endif  // OS_WIN
  service_manager::SandboxType sandbox_type_;
  base::CommandLine cmd_line_;
};

UtilityMainThreadFactoryFunction g_utility_main_thread_factory = nullptr;

void UtilityProcessHost::RegisterUtilityMainThreadFactory(
    UtilityMainThreadFactoryFunction create) {
  g_utility_main_thread_factory = create;
}

UtilityProcessHost::UtilityProcessHost()
    : UtilityProcessHost(nullptr /* client */) {}

UtilityProcessHost::UtilityProcessHost(std::unique_ptr<Client> client)
    : sandbox_type_(service_manager::SANDBOX_TYPE_UTILITY),
#if defined(OS_LINUX)
      child_flags_(ChildProcessHost::CHILD_ALLOW_SELF),
#else
      child_flags_(ChildProcessHost::CHILD_NORMAL),
#endif
      started_(false),
      name_(base::ASCIIToUTF16("utility process")),
      client_(std::move(client)) {
  process_.reset(new BrowserChildProcessHostImpl(PROCESS_TYPE_UTILITY, this,
                                                 mojom::kUtilityServiceName));
}

UtilityProcessHost::~UtilityProcessHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (client_ && launch_state_ == LaunchState::kLaunchComplete)
    client_->OnProcessTerminatedNormally();
}

base::WeakPtr<UtilityProcessHost> UtilityProcessHost::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool UtilityProcessHost::Send(IPC::Message* message) {
  if (!StartProcess())
    return false;

  return process_->Send(message);
}

void UtilityProcessHost::SetSandboxType(
    service_manager::SandboxType sandbox_type) {
  DCHECK(sandbox_type != service_manager::SANDBOX_TYPE_INVALID);
  sandbox_type_ = sandbox_type;
}

const ChildProcessData& UtilityProcessHost::GetData() {
  return process_->GetData();
}

#if defined(OS_POSIX)
void UtilityProcessHost::SetEnv(const base::EnvironmentMap& env) {
  env_ = env;
}
#endif

bool UtilityProcessHost::Start() {
  return StartProcess();
}

void UtilityProcessHost::RunService(
    const std::string& service_name,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver,
    service_manager::Service::CreatePackagedServiceInstanceCallback callback) {
  if (launch_state_ == LaunchState::kLaunchFailed) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  process_->GetHost()->RunService(service_name, std::move(receiver));
  if (launch_state_ == LaunchState::kLaunchComplete) {
    std::move(callback).Run(process_->GetProcess().Pid());
  } else {
    DCHECK_EQ(launch_state_, LaunchState::kLaunchInProgress);
    pending_run_service_callbacks_.push_back(std::move(callback));
  }
}

void UtilityProcessHost::SetMetricsName(const std::string& metrics_name) {
  metrics_name_ = metrics_name;
}

void UtilityProcessHost::SetName(const base::string16& name) {
  name_ = name;
}

void UtilityProcessHost::SetServiceIdentity(
    const service_manager::Identity& identity) {
  service_identity_ = identity;
}

void UtilityProcessHost::SetExtraCommandLineSwitches(
    std::vector<std::string> switches) {
  extra_switches_ = std::move(switches);
}

mojom::ChildProcess* UtilityProcessHost::GetChildProcess() {
  return static_cast<ChildProcessHostImpl*>(process_->GetHost())
      ->child_process();
}

bool UtilityProcessHost::StartProcess() {
  if (started_)
    return true;

  started_ = true;
  process_->SetName(name_);
  process_->SetMetricsName(metrics_name_);
  process_->GetHost()->CreateChannelMojo();

  if (RenderProcessHost::run_renderer_in_process()) {
    DCHECK(g_utility_main_thread_factory);
    // See comment in RenderProcessHostImpl::Init() for the background on why we
    // support single process mode this way.
    in_process_thread_.reset(
        g_utility_main_thread_factory(InProcessChildThreadParams(
            base::CreateSingleThreadTaskRunner({BrowserThread::IO}),
            process_->GetInProcessMojoInvitation(),
            process_->child_connection()->service_token())));
    in_process_thread_->Start();
  } else {
    const base::CommandLine& browser_command_line =
        *base::CommandLine::ForCurrentProcess();

    bool has_cmd_prefix =
        browser_command_line.HasSwitch(switches::kUtilityCmdPrefix);

#if defined(OS_ANDROID)
    // readlink("/prof/self/exe") sometimes fails on Android at startup.
    // As a workaround skip calling it here, since the executable name is
    // not needed on Android anyway. See crbug.com/500854.
    std::unique_ptr<base::CommandLine> cmd_line =
        std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
    if (sandbox_type_ == service_manager::SANDBOX_TYPE_NETWORK &&
        base::FeatureList::IsEnabled(features::kWarmUpNetworkProcess)) {
      process_->EnableWarmUpConnection();
    }
#else
    int child_flags = child_flags_;

    // When running under gdb, forking /proc/self/exe ends up forking the gdb
    // executable instead of Chromium. It is almost safe to assume that no
    // updates will happen while a developer is running with
    // |switches::kUtilityCmdPrefix|. See ChildProcessHost::GetChildPath() for
    // a similar case with Valgrind.
    if (has_cmd_prefix)
      child_flags = ChildProcessHost::CHILD_NORMAL;

    base::FilePath exe_path = ChildProcessHost::GetChildPath(child_flags);
    if (exe_path.empty()) {
      NOTREACHED() << "Unable to get utility process binary name.";
      return false;
    }

    std::unique_ptr<base::CommandLine> cmd_line =
        std::make_unique<base::CommandLine>(exe_path);
#endif

    cmd_line->AppendSwitchASCII(switches::kProcessType,
                                switches::kUtilityProcess);
    BrowserChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(cmd_line.get());
    BrowserChildProcessHostImpl::CopyTraceStartupFlags(cmd_line.get());
    std::string locale = GetContentClient()->browser()->GetApplicationLocale();
    cmd_line->AppendSwitchASCII(switches::kLang, locale);

#if defined(OS_WIN)
    cmd_line->AppendArg(switches::kPrefetchArgumentOther);
#endif  // defined(OS_WIN)

    service_manager::SetCommandLineFlagsForSandboxType(cmd_line.get(),
                                                       sandbox_type_);

    // Browser command-line switches to propagate to the utility process.
    static const char* const kSwitchNames[] = {
      network::switches::kForceEffectiveConnectionType,
      network::switches::kHostResolverRules,
      network::switches::kIgnoreCertificateErrorsSPKIList,
      network::switches::kIgnoreUrlFetcherCertRequests,
      network::switches::kLogNetLog,
      network::switches::kNetLogCaptureMode,
      network::switches::kExplicitlyAllowedPorts,
      service_manager::switches::kNoSandbox,
      service_manager::switches::kEnableAudioServiceSandbox,
#if defined(OS_MACOSX)
      service_manager::switches::kEnableSandboxLogging,
      os_crypt::switches::kUseMockKeychain,
#endif
      switches::kDisableTestCerts,
      switches::kEnableLogging,
      switches::kForceTextDirection,
      switches::kForceUIDirection,
      switches::kIgnoreCertificateErrors,
      switches::kLoggingLevel,
      switches::kOverrideUseSoftwareGLForTests,
      switches::kOverrideEnabledCdmInterfaceVersion,
      switches::kProxyServer,
      switches::kDisableAcceleratedMjpegDecode,
      switches::kUseFakeDeviceForMediaStream,
      switches::kUseFakeMjpegDecodeAccelerator,
      switches::kUseFileForFakeVideoCapture,
      switches::kUseMockCertVerifierForTesting,
      switches::kMockCertVerifierDefaultResultForTesting,
      switches::kUtilityStartupDialog,
      switches::kUseGL,
      switches::kV,
      switches::kVModule,
#if defined(OS_ANDROID)
      switches::kEnableReachedCodeProfiler,
#endif
      switches::kEnableExperimentalWebPlatformFeatures,
      // These flags are used by the audio service:
      switches::kAudioBufferSize,
      switches::kAudioServiceQuitTimeoutMs,
      switches::kDisableAudioOutput,
      switches::kFailAudioStreamCreation,
      switches::kForceDisableWebRtcApmInAudioService,
      switches::kMuteAudio,
      switches::kUseFileForFakeAudioCapture,
      switches::kAgcStartupMinVolume,
#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_SOLARIS)
      switches::kAlsaInputDevice,
      switches::kAlsaOutputDevice,
#endif
#if defined(OS_WIN)
      switches::kDisableHighResTimer,
      switches::kEnableExclusiveAudio,
      switches::kForceWaveAudio,
      switches::kTrySupportedChannelLayouts,
      switches::kWaveOutBuffers,
      service_manager::switches::kAddXrAppContainerCaps,
#endif
    };
    cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                               base::size(kSwitchNames));

    network_session_configurator::CopyNetworkSwitches(browser_command_line,
                                                      cmd_line.get());

    if (has_cmd_prefix) {
      // Launch the utility child process with some prefix
      // (usually "xterm -e gdb --args").
      cmd_line->PrependWrapper(browser_command_line.GetSwitchValueNative(
          switches::kUtilityCmdPrefix));
    }

    const bool is_service = service_identity_.has_value();
    if (is_service) {
      GetContentClient()->browser()->AdjustUtilityServiceProcessCommandLine(
          *service_identity_, cmd_line.get());
    }

    for (const auto& extra_switch : extra_switches_)
      cmd_line->AppendSwitch(extra_switch);

    std::unique_ptr<UtilitySandboxedProcessLauncherDelegate> delegate =
        std::make_unique<UtilitySandboxedProcessLauncherDelegate>(
            sandbox_type_, env_, *cmd_line);
    process_->Launch(std::move(delegate), std::move(cmd_line), true);
  }

  return true;
}

bool UtilityProcessHost::OnMessageReceived(const IPC::Message& message) {
  return true;
}

void UtilityProcessHost::OnProcessLaunched() {
  launch_state_ = LaunchState::kLaunchComplete;
  for (auto& callback : pending_run_service_callbacks_)
    std::move(callback).Run(process_->GetProcess().Pid());
  pending_run_service_callbacks_.clear();
  if (client_)
    client_->OnProcessLaunched(process_->GetProcess());
}

void UtilityProcessHost::OnProcessLaunchFailed(int error_code) {
  launch_state_ = LaunchState::kLaunchFailed;
  for (auto& callback : pending_run_service_callbacks_)
    std::move(callback).Run(base::nullopt);
  pending_run_service_callbacks_.clear();
}

void UtilityProcessHost::OnProcessCrashed(int exit_code) {
  if (!client_)
    return;

  // Take ownership of |client_| so the destructor doesn't notify it of
  // termination.
  auto client = std::move(client_);
#if defined(OS_ANDROID)
  // OnProcessCrashed() is always called on Android even in the case of normal
  // process termination. |clean_exit| gives us a reliable indication of whether
  // this was really a crash or just normal termination.
  if (process_->GetTerminationInfo(true /* known_dead */).clean_exit) {
    client->OnProcessTerminatedNormally();
    return;
  }
#endif
  client->OnProcessCrashed();
}

base::Optional<std::string> UtilityProcessHost::GetServiceName() {
  if (!service_identity_)
    return metrics_name_;
  return service_identity_->name();
}

}  // namespace content
