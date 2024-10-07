// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/utility_process_host.h"

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/child_process_host_impl.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/utility_sandbox_delegate.h"
#include "content/common/features.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_descriptor_keys.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/webrtc/webrtc_features.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "services/network/public/mojom/network_service.mojom.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "components/os_crypt/sync/os_crypt_switches.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
#include "content/browser/v8_snapshot_files.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/pickle.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "components/app_launch_prefetch/app_launch_prefetch.h"
#include "media/capture/capture_switches.h"
#include "services/audio/public/mojom/audio_service.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_MAC)
#include "base/task/sequenced_task_runner.h"
#include "components/viz/host/gpu_client.h"
#include "media/capture/capture_switches.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#endif

namespace content {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
base::ScopedFD PassNetworkContextParentDirs(
    std::vector<base::FilePath> network_context_parent_dirs) {
  base::Pickle pickle;
  for (const base::FilePath& dir : network_context_parent_dirs) {
    pickle.WriteString(dir.value());
  }

  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  if (!base::CreatePipe(&read_fd, &write_fd)) {
    PLOG(ERROR) << "Failed to create thepipe necessary to properly sandbox the "
                   "network service.";
    return base::ScopedFD();
  }
  if (!base::WriteFileDescriptor(write_fd.get(), pickle)) {
    PLOG(ERROR) << "Failed to write to the pipe which is necessary to properly "
                   "sandbox the network service.";
    return base::ScopedFD();
  }

  return read_fd;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
base::CommandLine::StringViewType UtilityToAppLaunchPrefetchArg(
    const std::string& utility_type) {
  // Set the default prefetch type for utility processes.
  app_launch_prefetch::SubprocessType prefetch_type =
      app_launch_prefetch::SubprocessType::kUtilityOther;

  if (utility_type == network::mojom::NetworkService::Name_) {
    prefetch_type = app_launch_prefetch::SubprocessType::kUtilityNetworkService;
  } else if (utility_type == storage::mojom::StorageService::Name_) {
    prefetch_type = app_launch_prefetch::SubprocessType::kUtilityStorage;
  } else if (utility_type == audio::mojom::AudioService::Name_) {
    prefetch_type = app_launch_prefetch::SubprocessType::kUtilityAudio;
  }
  return app_launch_prefetch::GetPrefetchSwitch(prefetch_type);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

UtilityMainThreadFactoryFunction g_utility_main_thread_factory = nullptr;

void UtilityProcessHost::RegisterUtilityMainThreadFactory(
    UtilityMainThreadFactoryFunction create) {
  g_utility_main_thread_factory = create;
}

UtilityProcessHost::UtilityProcessHost()
    : UtilityProcessHost(nullptr /* client */) {}

UtilityProcessHost::UtilityProcessHost(std::unique_ptr<Client> client)
    : sandbox_type_(sandbox::mojom::Sandbox::kUtility),
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      child_flags_(ChildProcessHost::CHILD_ALLOW_SELF),
#else
      child_flags_(ChildProcessHost::CHILD_NORMAL),
#endif
      started_(false),
      name_(u"utility process"),
      file_data_(std::make_unique<ChildProcessLauncherFileData>()),
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_MAC)
      allowed_gpu_(false),
      gpu_client_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
#endif
      client_(std::move(client)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  process_ = std::make_unique<BrowserChildProcessHostImpl>(
      PROCESS_TYPE_UTILITY, this, ChildProcessHost::IpcMode::kNormal);
}

UtilityProcessHost::~UtilityProcessHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (client_ && launch_state_ == LaunchState::kLaunchComplete)
    client_->OnProcessTerminatedNormally();
}

base::WeakPtr<UtilityProcessHost> UtilityProcessHost::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void UtilityProcessHost::SetSandboxType(sandbox::mojom::Sandbox sandbox_type) {
  sandbox_type_ = sandbox_type;
}

const ChildProcessData& UtilityProcessHost::GetData() {
  return process_->GetData();
}

#if BUILDFLAG(IS_POSIX)
void UtilityProcessHost::SetEnv(const base::EnvironmentMap& env) {
  env_ = env;
}
#endif

bool UtilityProcessHost::Start() {
  return StartProcess();
}

void UtilityProcessHost::SetMetricsName(const std::string& metrics_name) {
  metrics_name_ = metrics_name;
}

void UtilityProcessHost::SetName(const std::u16string& name) {
  name_ = name;
}

void UtilityProcessHost::SetExtraCommandLineSwitches(
    std::vector<std::string> switches) {
  extra_switches_ = std::move(switches);
}

#if BUILDFLAG(IS_WIN)
void UtilityProcessHost::SetPreloadLibraries(
    const std::vector<base::FilePath>& preloads) {
  preload_libraries_ = preloads;
}
#endif  // BUILDFLAG(IS_WIN)

void UtilityProcessHost::SetAllowGpuClient() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_MAC)
  allowed_gpu_ = true;
#endif
}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
void UtilityProcessHost::AddFileToPreload(
    std::string key,
    absl::variant<base::FilePath, base::ScopedFD> file) {
  DCHECK_EQ(file_data_->files_to_preload.count(key), 0u);
  file_data_->files_to_preload.insert({std::move(key), std::move(file)});
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

#if BUILDFLAG(USE_ZYGOTE)
void UtilityProcessHost::SetZygoteForTesting(ZygoteCommunication* handle) {
  zygote_for_testing_ = handle;
}
#endif  // BUILDFLAG(USE_ZYGOTE)

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

  if (RenderProcessHost::run_renderer_in_process()) {
    DCHECK(g_utility_main_thread_factory);
    // See comment in RenderProcessHostImpl::Init() for the background on why we
    // support single process mode this way.
    in_process_thread_.reset(g_utility_main_thread_factory(
        InProcessChildThreadParams(GetIOThreadTaskRunner({}),
                                   process_->GetInProcessMojoInvitation())));
    in_process_thread_->Start();
  } else {
    const base::CommandLine& browser_command_line =
        *base::CommandLine::ForCurrentProcess();

    bool has_cmd_prefix =
        browser_command_line.HasSwitch(switches::kUtilityCmdPrefix);

#if BUILDFLAG(IS_ANDROID)
    // readlink("/prof/self/exe") sometimes fails on Android at startup.
    // As a workaround skip calling it here, since the executable name is
    // not needed on Android anyway. See crbug.com/500854.
    std::unique_ptr<base::CommandLine> cmd_line =
        std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
    if (metrics_name_ == network::mojom::NetworkService::Name_ &&
        base::FeatureList::IsEnabled(features::kWarmUpNetworkProcess)) {
      process_->EnableWarmUpConnection();
    }
#else  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_MAC)
    if (sandbox_type_ == sandbox::mojom::Sandbox::kServiceWithJit)
      DCHECK_EQ(child_flags_, ChildProcessHost::CHILD_RENDERER);
#endif  // BUILDFLAG(IS_MAC)
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
      NOTREACHED_IN_MIGRATION() << "Unable to get utility process binary name.";
      return false;
    }

    std::unique_ptr<base::CommandLine> cmd_line =
        std::make_unique<base::CommandLine>(exe_path);
#endif  // BUILDFLAG(IS_ANDROID)

    cmd_line->AppendSwitchASCII(switches::kProcessType,
                                switches::kUtilityProcess);
    // Specify the type of utility process for debugging/profiling purposes.
    cmd_line->AppendSwitchASCII(switches::kUtilitySubType, metrics_name_);
    std::string locale = GetContentClient()->browser()->GetApplicationLocale();
    cmd_line->AppendSwitchASCII(switches::kLang, locale);

#if BUILDFLAG(IS_WIN)
    cmd_line->AppendArgNative(UtilityToAppLaunchPrefetchArg(metrics_name_));
#endif  // BUILDFLAG(IS_WIN)

    sandbox::policy::SetCommandLineFlagsForSandboxType(cmd_line.get(),
                                                       sandbox_type_);

    // Browser command-line switches to propagate to the utility process.
    static const char* const kSwitchNames[] = {
        network::switches::kAdditionalTrustTokenKeyCommitments,
        network::switches::kForceEffectiveConnectionType,
        network::switches::kHostResolverRules,
        network::switches::kIgnoreCertificateErrorsSPKIList,
        network::switches::kTestThirdPartyCookiePhaseout,
        network::switches::kDisableSharedDictionaryStorageCleanupForTesting,
        sandbox::policy::switches::kNoSandbox,
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
        switches::kDisableDevShmUsage,
#endif
#if BUILDFLAG(IS_MAC)
        sandbox::policy::switches::kDisableMetalShaderCache,
        sandbox::policy::switches::kEnableSandboxLogging,
#endif
        switches::kEnableBackgroundThreadPool,
        switches::kEnableExperimentalCookieFeatures,
        switches::kForceTextDirection,
        switches::kForceUIDirection,
        switches::kIgnoreCertificateErrors,
        switches::kOverrideUseSoftwareGLForTests,
        switches::kOverrideEnabledCdmInterfaceVersion,
        switches::kDisableAcceleratedMjpegDecode,
        switches::kUseFakeDeviceForMediaStream,
        switches::kUseFakeMjpegDecodeAccelerator,
        switches::kUseFileForFakeVideoCapture,
        switches::kUseMockCertVerifierForTesting,
        switches::kMockCertVerifierDefaultResultForTesting,
        switches::kUtilityStartupDialog,
        switches::kUseANGLE,
        switches::kUseGL,
        switches::kEnableExperimentalWebPlatformFeatures,
        // These flags are used by the audio service:
        switches::kAudioBufferSize,
        switches::kDisableAudioInput,
        switches::kDisableAudioOutput,
        switches::kFailAudioStreamCreation,
        switches::kMuteAudio,
        switches::kUseFileForFakeAudioCapture,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FREEBSD) || \
    BUILDFLAG(IS_SOLARIS)
        switches::kAlsaInputDevice,
        switches::kAlsaOutputDevice,
#endif
#if BUILDFLAG(USE_CRAS)
        switches::kUseCras,
#endif
#if BUILDFLAG(IS_WIN)
        switches::kDisableHighResTimer,
        switches::kEnableExclusiveAudio,
        switches::kForceWaveAudio,
        switches::kRaiseTimerFrequency,
        switches::kTrySupportedChannelLayouts,
        switches::kWaveOutBuffers,
        switches::kWebXrForceRuntime,
        sandbox::policy::switches::kAddXrAppContainerCaps,
#endif
        network::switches::kIpAddressSpaceOverrides,
#if BUILDFLAG(IS_CHROMEOS)
        switches::kSchedulerBoostUrgent,
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        switches::kEnableResourcesFileSharing,
#endif
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
        switches::kHardwareVideoDecodeFrameRate,
#endif
    };
    cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames);

    network_session_configurator::CopyNetworkSwitches(browser_command_line,
                                                      cmd_line.get());

    if (has_cmd_prefix) {
      // Launch the utility child process with some prefix
      // (usually "xterm -e gdb --args").
      cmd_line->PrependWrapper(browser_command_line.GetSwitchValueNative(
          switches::kUtilityCmdPrefix));
    }

    for (const auto& extra_switch : extra_switches_)
      cmd_line->AppendSwitch(extra_switch);

#if BUILDFLAG(IS_WIN)
    if (media::IsMediaFoundationD3D11VideoCaptureEnabled()) {
      // MediaFoundationD3D11VideoCapture requires Gpu memory buffers,
      // which are unavailable if the GPU process isn't running or if
      // D3D shared images are not supported.
      if (!GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled() &&
          GpuDataManagerImpl::GetInstance()->GetGPUInfo().shared_image_d3d) {
        cmd_line->AppendSwitch(switches::kVideoCaptureUseGpuMemoryBuffer);
      }
    }
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
    file_data_->files_to_preload.merge(GetV8SnapshotFilesToPreload(*cmd_line));
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // The network service should have access to the parent directories
    // necessary for its usage.
    if (sandbox_type_ == sandbox::mojom::Sandbox::kNetwork) {
      std::vector<base::FilePath> network_context_parent_dirs =
          GetContentClient()->browser()->GetNetworkContextsParentDirectory();
      file_data_->files_to_preload[kNetworkContextParentDirsDescriptor] =
          PassNetworkContextParentDirs(std::move(network_context_parent_dirs));
    }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
    // Pass `kVideoCaptureUseGpuMemoryBuffer` flag to video capture service only
    // when the video capture use GPU memory buffer enabled.
    if (metrics_name_ == video_capture::mojom::VideoCaptureService::Name_) {
      bool pass_gpu_buffer_flag =
          switches::IsVideoCaptureUseGpuMemoryBufferEnabled();
#if BUILDFLAG(IS_LINUX)
      // Check if NV12 GPU memory buffer supported at the same time.
      pass_gpu_buffer_flag =
          pass_gpu_buffer_flag &&
          GpuDataManagerImpl::GetInstance()->IsGpuMemoryBufferNV12Supported();
#endif  // BUILDFLAG(IS_LINUX)
      if (pass_gpu_buffer_flag) {
        cmd_line->AppendSwitch(switches::kVideoCaptureUseGpuMemoryBuffer);
      }
    }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH) ||
        // BUILDFLAG(IS_MAC)

    std::unique_ptr<UtilitySandboxedProcessLauncherDelegate> delegate =
        std::make_unique<UtilitySandboxedProcessLauncherDelegate>(
            sandbox_type_, env_, *cmd_line);

#if BUILDFLAG(IS_WIN)
    if (!preload_libraries_.empty()) {
      delegate->SetPreloadLibraries(preload_libraries_);
    }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(USE_ZYGOTE)
    if (zygote_for_testing_.has_value()) {
      delegate->SetZygote(zygote_for_testing_.value());
    }
#endif  // BUILDFLAG(USE_ZYGOTE)

    process_->LaunchWithFileData(std::move(delegate), std::move(cmd_line),
                                 std::move(file_data_), true);
  }

  return true;
}

void UtilityProcessHost::OnProcessLaunched() {
  launch_state_ = LaunchState::kLaunchComplete;
  if (client_)
    client_->OnProcessLaunched(process_->GetProcess());
}

void UtilityProcessHost::OnProcessLaunchFailed(int error_code) {
  launch_state_ = LaunchState::kLaunchFailed;
}

void UtilityProcessHost::OnProcessCrashed(int exit_code) {
  if (!client_)
    return;

  // Take ownership of |client_| so the destructor doesn't notify it of
  // termination.
  auto client = std::move(client_);
  client->OnProcessCrashed();
}

std::optional<std::string> UtilityProcessHost::GetServiceName() {
  return metrics_name_;
}

}  // namespace content
