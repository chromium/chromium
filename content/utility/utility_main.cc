// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/allocator/partition_alloc_support.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_functions.h"
#include "base/power_monitor/power_monitor.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/hi_res_timer_manager.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/child/child_process.h"
#include "content/common/content_switches_internal.h"
#include "content/common/features.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/utility/content_utility_client.h"
#include "content/utility/utility_thread_impl.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox.h"
#include "sandbox/policy/sandbox_type.h"
#include "services/on_device_model/on_device_model_service.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/video_effects/public/cpp/buildflags.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/file_descriptor_store.h"
#include "base/files/file_util.h"
#include "base/pickle.h"
#include "content/child/sandboxed_process_thread_type_handler.h"
#include "content/common/gpu_pre_sandbox_hook_linux.h"
#include "content/public/common/content_descriptor_keys.h"
#include "content/utility/speech/speech_recognition_sandbox_hook_linux.h"
#include "gpu/config/gpu_info_collector.h"
#include "media/gpu/sandbox/hardware_video_encoding_sandbox_hook_linux.h"
#include "sandbox/policy/linux/sandbox_linux.h"
#include "services/audio/audio_sandbox_hook_linux.h"
#include "services/network/network_sandbox_hook_linux.h"
// gn check is not smart enough to realize that this include only applies to
// Linux/ChromeOS and the BUILD.gn dependencies correctly account for that.
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"  //nogncheck

#if BUILDFLAG(ENABLE_PRINTING)
#include "printing/sandbox/print_backend_sandbox_hook_linux.h"
#endif
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/gpu/sandbox/hardware_video_decoding_sandbox_hook_linux.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS) && BUILDFLAG(IS_LINUX)
#include "services/video_effects/video_effects_sandbox_hook_linux.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_VIDEO_EFFECTS) && BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/services/ime/ime_sandbox_hook.h"
#include "chromeos/services/tts/tts_sandbox_hook.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/ash/services/libassistant/libassistant_sandbox_hook.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if (BUILDFLAG(ENABLE_SCREEN_AI_SERVICE) && \
     (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)))
#include "services/screen_ai/public/cpp/utilities.h"  // nogncheck
#include "services/screen_ai/sandbox/screen_ai_sandbox_hook_linux.h"  // nogncheck
#endif

#if BUILDFLAG(IS_MAC)
#include "base/message_loop/message_pump_apple.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/native_library.h"
#include "base/rand_util.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "content/utility/sandbox_delegate_data.mojom.h"
#include "sandbox/policy/win/sandbox_warmup.h"
#include "sandbox/win/src/sandbox.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
sandbox::TargetServices* g_utility_target_services = nullptr;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX)
#include "components/services/on_device_translation/sandbox_hook.h"
#endif  // BUILDFLAG(IS_LINUX)
namespace content {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
std::vector<std::string> GetNetworkContextsParentDirectories() {
  base::MemoryMappedFile::Region region;
  base::ScopedFD read_pipe_fd = base::FileDescriptorStore::GetInstance().TakeFD(
      kNetworkContextParentDirsDescriptor, &region);
  DCHECK(region == base::MemoryMappedFile::Region::kWholeFile);

  std::string dirs_str;
  if (!base::ReadStreamToString(fdopen(read_pipe_fd.get(), "r"), &dirs_str)) {
    LOG(FATAL) << "Failed to read network context parents dirs from pipe.";
  }

  base::Pickle dirs_pickle =
      base::Pickle::WithUnownedBuffer(base::as_byte_span(dirs_str));
  base::PickleIterator dirs_pickle_iter(dirs_pickle);

  std::vector<std::string> dirs;
  std::string dir;
  while (dirs_pickle_iter.ReadString(&dir)) {
    dirs.push_back(dir);
  }

  CHECK(dirs_pickle_iter.ReachedEnd());

  return dirs;
}

bool ShouldUseAmdGpuPolicy(sandbox::mojom::Sandbox sandbox_type) {
  const bool obtain_gpu_info =
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
      sandbox_type == sandbox::mojom::Sandbox::kHardwareVideoDecoding ||
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
      sandbox_type == sandbox::mojom::Sandbox::kHardwareVideoEncoding;

  if (obtain_gpu_info) {
    // The kHardwareVideoDecoding and kHardwareVideoEncoding sandboxes need to
    // know the GPU type in order to select the right policy.
    gpu::GPUInfo gpu_info{};
    gpu::CollectBasicGraphicsInfo(&gpu_info);
    return angle::IsAMD(gpu_info.active_gpu().vendor_id);
  }

  return false;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
// Handle pre-lockdown sandbox hooks
bool PreLockdownSandboxHook(base::span<const uint8_t> delegate_blob) {
  // TODO(crbug.com/40265190) Migrate other settable things to delegate_data.
  CHECK(!delegate_blob.empty());
  content::mojom::sandbox::UtilityConfigPtr sandbox_config;
  if (!content::mojom::sandbox::UtilityConfig::Deserialize(
          delegate_blob.data(), delegate_blob.size(), &sandbox_config)) {
    NOTREACHED();
  }
  if (!sandbox_config->preload_libraries.empty()) {
    for (const auto& library_path : sandbox_config->preload_libraries) {
      CHECK(library_path.IsAbsolute());
      base::NativeLibraryLoadError lib_error;
      HMODULE h_mod = base::LoadNativeLibrary(library_path, &lib_error);
      // We deliberately "leak" `h_mod` so that the module stays loaded.
      if (!h_mod) {
        base::UmaHistogramSparse(
            "Process.Sandbox.PreloadLibraryFailed.ErrorCode", lib_error.code);
        // The browser should not request libraries that do not exist, so crash
        // on failure.
        wchar_t dll_name[MAX_PATH];
        base::wcslcpy(dll_name, library_path.value().c_str(), MAX_PATH);
        base::debug::Alias(dll_name);
        base::debug::Alias(&lib_error);
        NOTREACHED();
      }
    }
  }
  return true;
}
#endif  // BUILDFLAG(IS_WIN)

void SetUtilityThreadName(const std::string& utility_sub_type) {
  // Typical utility sub-types are audio.mojom.AudioService or
  // proxy_resolver.mojom.ProxyResolverFactory. Using the full sub-type as part
  // of the thread name is too verbose so we take the text in front of the first
  // period and use that as a prefix. This give us thread names like
  // audio.CrUtilityMain and proxy_resolver.CrUtilityMain. If there is no period
  // then the entire utility_sub_type string will be put in front.
  auto first_period = utility_sub_type.find('.');
  base::PlatformThread::SetName(utility_sub_type.substr(0, first_period) +
                                ".CrUtilityMain");
}

}  // namespace

// Mainline routine for running as the utility process.
int UtilityMain(MainFunctionParams parameters) {
  base::MessagePumpType message_pump_type =
      parameters.command_line->HasSwitch(switches::kMessageLoopTypeUi)
          ? base::MessagePumpType::UI
          : base::MessagePumpType::DEFAULT;

#if BUILDFLAG(IS_MAC)
  auto sandbox_type =
      sandbox::policy::SandboxTypeFromCommandLine(*parameters.command_line);
  if (sandbox_type != sandbox::mojom::Sandbox::kNoSandbox) {
    // On Mac, the TYPE_UI pump for the main thread is an NSApplication loop.
    // In a sandboxed utility process, NSApp attempts to acquire more Mach
    // resources than a restrictive sandbox policy should allow. Services that
    // require a TYPE_UI pump generally just need a NS/CFRunLoop to pump system
    // work sources, so choose that pump type instead. A NSRunLoop MessagePump
    // is used for TYPE_UI MessageLoops on non-main threads.
    base::MessagePump::OverrideMessagePumpForUIFactory(
        []() -> std::unique_ptr<base::MessagePump> {
          return std::make_unique<base::MessagePumpNSRunLoop>();
        });
  }
#endif

#if BUILDFLAG(IS_FUCHSIA)
  // On Fuchsia always use IO threads to allow FIDL calls.
  if (message_pump_type == base::MessagePumpType::DEFAULT)
    message_pump_type = base::MessagePumpType::IO;
#endif  // BUILDFLAG(IS_FUCHSIA)

  // The main task executor of the utility process.
  base::SingleThreadTaskExecutor main_thread_task_executor(message_pump_type);
  const std::string utility_sub_type =
      parameters.command_line->GetSwitchValueASCII(switches::kUtilitySubType);
  SetUtilityThreadName(utility_sub_type);

  if (parameters.command_line->HasSwitch(switches::kUtilityStartupDialog)) {
    auto dialog_match = parameters.command_line->GetSwitchValueASCII(
        switches::kUtilityStartupDialog);
    if (dialog_match.empty() || dialog_match == utility_sub_type) {
      WaitForDebugger(utility_sub_type.empty() ? "Utility" : utility_sub_type);
    }
  }

  if (utility_sub_type == on_device_model::mojom::OnDeviceModelService::Name_) {
    CHECK(on_device_model::OnDeviceModelService::PreSandboxInit());
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Thread type delegate of the process should be registered before first
  // thread type change in ChildProcess constructor. It also needs to be
  // registered before the process has multiple threads, which may race with
  // application of the sandbox.
  if (base::FeatureList::IsEnabled(
          features::kHandleChildThreadTypeChangesInBrowser) ||
      base::FeatureList::IsEnabled(features::kSchedQoSOnResourcedForChrome)) {
    SandboxedProcessThreadTypeHandler::Create();
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Initializes the sandbox before any threads are created.
  // TODO(jorgelo): move this after GTK initialization when we enable a strict
  // Seccomp-BPF policy.
  auto sandbox_type =
      sandbox::policy::SandboxTypeFromCommandLine(*parameters.command_line);
  sandbox::policy::SandboxLinux::Options sandbox_options;
  sandbox::policy::SandboxLinux::PreSandboxHook pre_sandbox_hook;
  switch (sandbox_type) {
    case sandbox::mojom::Sandbox::kNetwork:
      pre_sandbox_hook = base::BindOnce(&network::NetworkPreSandboxHook,
                                        GetNetworkContextsParentDirectories());
      break;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case sandbox::mojom::Sandbox::kPrintBackend:
      pre_sandbox_hook = base::BindOnce(&printing::PrintBackendPreSandboxHook);
      break;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
    case sandbox::mojom::Sandbox::kAudio:
      pre_sandbox_hook = base::BindOnce(&audio::AudioPreSandboxHook);
      break;
    case sandbox::mojom::Sandbox::kOnDeviceModelExecution:
      on_device_model::OnDeviceModelService::AddSandboxLinuxOptions(
          sandbox_options);
      pre_sandbox_hook = base::BindOnce(&GpuPreSandboxHook);
      break;
    case sandbox::mojom::Sandbox::kSpeechRecognition:
      pre_sandbox_hook =
          base::BindOnce(&speech::SpeechRecognitionPreSandboxHook);
      break;
#if BUILDFLAG(IS_LINUX)
    case sandbox::mojom::Sandbox::kOnDeviceTranslation:
      pre_sandbox_hook = base::BindOnce(
          &on_device_translation::OnDeviceTranslationSandboxHook);
      break;
#endif  // BUILDFLAG(IS_LINUX)
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    case sandbox::mojom::Sandbox::kScreenAI:
      pre_sandbox_hook =
          base::BindOnce(&screen_ai::ScreenAIPreSandboxHook,
                         parameters.command_line->GetSwitchValuePath(
                             screen_ai::GetBinaryPathSwitch()));
      break;
#endif
#if BUILDFLAG(IS_LINUX)
    case sandbox::mojom::Sandbox::kVideoEffects:
      pre_sandbox_hook =
          base::BindOnce(&video_effects::VideoEffectsPreSandboxHook);
      break;
#endif  // BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
    case sandbox::mojom::Sandbox::kHardwareVideoDecoding:
      pre_sandbox_hook =
          base::BindOnce(&media::HardwareVideoDecodingPreSandboxHook);
      break;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
    case sandbox::mojom::Sandbox::kHardwareVideoEncoding:
      pre_sandbox_hook =
          base::BindOnce(&media::HardwareVideoEncodingPreSandboxHook);
      break;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case sandbox::mojom::Sandbox::kIme:
      pre_sandbox_hook = base::BindOnce(&ash::ime::ImePreSandboxHook);
      break;
    case sandbox::mojom::Sandbox::kTts:
      pre_sandbox_hook = base::BindOnce(&chromeos::tts::TtsPreSandboxHook);
      break;
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
    case sandbox::mojom::Sandbox::kLibassistant:
      pre_sandbox_hook =
          base::BindOnce(&ash::libassistant::LibassistantPreSandboxHook);
      break;
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    default:
      break;
  }
  if (!sandbox::policy::IsUnsandboxedSandboxType(sandbox_type) &&
      (parameters.zygote_child || !pre_sandbox_hook.is_null())) {
    sandbox_options.use_amd_specific_policies =
        ShouldUseAmdGpuPolicy(sandbox_type);
    sandbox::policy::Sandbox::Initialize(
        sandbox_type, std::move(pre_sandbox_hook), sandbox_options);
  }

  // Start the HangWatcher now that the sandbox is engaged, if it hasn't
  // already been started.
  if (base::HangWatcher::IsEnabled() &&
      !base::HangWatcher::GetInstance()->IsStarted()) {
    DCHECK(parameters.hang_watcher_not_started_time.has_value());
    base::TimeDelta uncovered_hang_watcher_time =
        base::TimeTicks::Now() -
        parameters.hang_watcher_not_started_time.value();
    base::UmaHistogramTimes("HangWatcher.UtilityProcess.UncoveredStartupTime",
                            uncovered_hang_watcher_time);
    base::HangWatcher::GetInstance()->Start();
  }

#elif BUILDFLAG(IS_WIN)
  std::optional<base::win::ScopedCOMInitializer> scoped_com_initializer;
  if (message_pump_type == base::MessagePumpType::UI &&
      base::FeatureList::IsEnabled(
          features::kUtilityWithUiPumpInitializesCom)) {
    scoped_com_initializer.emplace();
    CHECK(scoped_com_initializer->Succeeded());
  }

  g_utility_target_services = parameters.sandbox_info->target_services;

  // Call hooks with data provided by UtilitySandboxedProcessLauncherDelegate.
  // Must happen before IO thread to preempt any mojo services starting.
  if (g_utility_target_services) {
    auto delegate_data = g_utility_target_services->GetDelegateData();
    if (delegate_data.has_value() && !delegate_data->empty()) {
      PreLockdownSandboxHook(delegate_data.value());
    }
  }
#endif

  ChildProcess utility_process(base::ThreadType::kDefault);
  GetContentClient()->utility()->PostIOThreadCreated(
      utility_process.io_task_runner());
  base::RunLoop run_loop;
  utility_process.set_main_thread(
      new UtilityThreadImpl(run_loop.QuitClosure()));

  // Mojo IPC support is brought up by UtilityThreadImpl, so startup tracing
  // is enabled here if it needs to start after mojo init (normally so the mojo
  // broker can bypass the sandbox to allocate startup tracing's SMB).
  if (parameters.needs_startup_tracing_after_mojo_init) {
    tracing::EnableStartupTracingIfNeeded();
  }

  // Both utility process and service utility process would come
  // here, but the later is launched without connection to service manager, so
  // there has no base::PowerMonitor be created(See ChildThreadImpl::Init()).
  // As base::PowerMonitor is necessary to base::HighResolutionTimerManager, for
  // such case we just disable base::HighResolutionTimerManager for now.
  // Note that disabling base::HighResolutionTimerManager means high resolution
  // timer is always disabled no matter on battery or not, but it should have
  // no any bad influence because currently service utility process is not using
  // any high resolution timer.
  // TODO(leonhsl): Once http://crbug.com/646833 got resolved, re-enable
  // base::HighResolutionTimerManager here for future possible usage of high
  // resolution timer in service utility process.
  std::optional<base::HighResolutionTimerManager> hi_res_timer_manager;
  if (base::PowerMonitor::GetInstance()->IsInitialized()) {
    hi_res_timer_manager.emplace();
  }

#if BUILDFLAG(IS_WIN)
  auto sandbox_type =
      sandbox::policy::SandboxTypeFromCommandLine(*parameters.command_line);
  DVLOG(1) << "Sandbox type: " << static_cast<int>(sandbox_type);

  // https://crbug.com/1076771 https://crbug.com/1075487 Premature unload of
  // shell32 caused process to crash during process shutdown. See also a
  // separate fix for https://crbug.com/1139752. Fixed in Windows 11.
  if (base::win::GetVersion() < base::win::Version::WIN11) {
    HMODULE shell32_pin = ::LoadLibrary(L"shell32.dll");
    UNREFERENCED_PARAMETER(shell32_pin);
  }

  // Not all utility processes require DPI awareness as this context only
  // pertains to certain workloads & impacted system API calls (e.g. UX
  // scaling or per-monitor windowing). We do not blanket apply DPI awareness
  // as utility processes running within a kService sandbox with the Win32K
  // Lockdown policy applied may crash when calling EnableHighDPISupport. See
  // crbug.com/978133.
  if (sandbox_type == sandbox::mojom::Sandbox::kMediaFoundationCdm) {
    // The Media Foundation Utility Process needs to be marked as DPI aware so
    // the Media Engine & CDM can correctly identify the target monitor for
    // video output. This is required to ensure that the proper monitor is
    // queried for hardware capabilities & any settings are applied to the
    // correct monitor.
    base::win::EnableHighDPISupport();
  }

  if (!sandbox::policy::IsUnsandboxedSandboxType(sandbox_type) &&
      sandbox_type != sandbox::mojom::Sandbox::kCdm &&
      sandbox_type != sandbox::mojom::Sandbox::kMediaFoundationCdm) {
    if (!g_utility_target_services)
      return false;

    sandbox::policy::WarmupRandomnessInfrastructure();

    g_utility_target_services->LowerToken();
  }
#endif

  base::allocator::PartitionAllocSupport::Get()->ReconfigureAfterTaskRunnerInit(
      switches::kUtilityProcess);

  run_loop.Run();

  if (utility_sub_type == on_device_model::mojom::OnDeviceModelService::Name_) {
    CHECK(on_device_model::OnDeviceModelService::Shutdown());
  }

#if defined(LEAK_SANITIZER)
  // Invoke LeakSanitizer before shutting down the utility thread, to avoid
  // reporting shutdown-only leaks.
  __lsan_do_leak_check();
#endif

  return 0;
}

}  // namespace content
