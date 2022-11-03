// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/message_loop/message_pump_type.h"
#include "base/power_monitor/power_monitor.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/platform_thread.h"
#include "base/timer/hi_res_timer_manager.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "content/child/child_process.h"
#include "content/common/content_switches_internal.h"
#include "content/common/partition_alloc_support.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/utility/content_utility_client.h"
#include "content/utility/utility_thread_impl.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox.h"
#include "sandbox/policy/sandbox_type.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/utility/speech/speech_recognition_sandbox_hook_linux.h"
#include "gpu/config/gpu_info_collector.h"
#include "media/gpu/sandbox/hardware_video_encoding_sandbox_hook_linux.h"
// gn check is not smart enough to realize that this include only applies to
// Linux/ChromeOS and the BUILD.gn dependencies correctly account for that.
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"  //nogncheck

#if BUILDFLAG(ENABLE_PRINTING)
#include "printing/sandbox/print_backend_sandbox_hook_linux.h"
#endif
#include "sandbox/policy/linux/sandbox_linux.h"
#include "services/audio/audio_sandbox_hook_linux.h"
#include "services/network/network_sandbox_hook_linux.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/gpu/sandbox/hardware_video_decoding_sandbox_hook_linux.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)

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
#include "components/services/screen_ai/sandbox/screen_ai_sandbox_hook_linux.h"  // nogncheck
#endif

#if BUILDFLAG(IS_MAC)
#include "base/message_loop/message_pump_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/rand_util.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/sandbox.h"

sandbox::TargetServices* g_utility_target_services = nullptr;
#endif

namespace content {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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

  if (parameters.command_line->HasSwitch(switches::kTimeZoneForTesting)) {
    std::string time_zone = parameters.command_line->GetSwitchValueASCII(
        switches::kTimeZoneForTesting);
    icu::TimeZone::adoptDefault(
        icu::TimeZone::createTimeZone(icu::UnicodeString(time_zone.c_str())));
  }

  // The main task executor of the utility process.
  base::SingleThreadTaskExecutor main_thread_task_executor(message_pump_type);
  base::PlatformThread::SetName("CrUtilityMain");

  if (parameters.command_line->HasSwitch(switches::kUtilityStartupDialog)) {
    const std::string utility_sub_type =
        parameters.command_line->GetSwitchValueASCII(switches::kUtilitySubType);
    auto dialog_match = parameters.command_line->GetSwitchValueASCII(
        switches::kUtilityStartupDialog);
    if (dialog_match.empty() || dialog_match == utility_sub_type) {
      WaitForDebugger(utility_sub_type.empty() ? "Utility" : utility_sub_type);
    }
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Initializes the sandbox before any threads are created.
  // TODO(jorgelo): move this after GTK initialization when we enable a strict
  // Seccomp-BPF policy.
  auto sandbox_type =
      sandbox::policy::SandboxTypeFromCommandLine(*parameters.command_line);
  sandbox::policy::SandboxLinux::PreSandboxHook pre_sandbox_hook;
  switch (sandbox_type) {
    case sandbox::mojom::Sandbox::kNetwork:
      pre_sandbox_hook = base::BindOnce(&network::NetworkPreSandboxHook);
      break;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case sandbox::mojom::Sandbox::kPrintBackend:
      pre_sandbox_hook = base::BindOnce(&printing::PrintBackendPreSandboxHook);
      break;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
    case sandbox::mojom::Sandbox::kAudio:
      pre_sandbox_hook = base::BindOnce(&audio::AudioPreSandboxHook);
      break;
    case sandbox::mojom::Sandbox::kSpeechRecognition:
      pre_sandbox_hook =
          base::BindOnce(&speech::SpeechRecognitionPreSandboxHook);
      break;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    case sandbox::mojom::Sandbox::kScreenAI:
      pre_sandbox_hook = base::BindOnce(&screen_ai::ScreenAIPreSandboxHook);
      break;
#endif
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
    sandbox::policy::SandboxLinux::Options sandbox_options;
    sandbox_options.use_amd_specific_policies =
        ShouldUseAmdGpuPolicy(sandbox_type);
    sandbox::policy::Sandbox::Initialize(
        sandbox_type, std::move(pre_sandbox_hook), sandbox_options);
  }
#elif BUILDFLAG(IS_WIN)
  g_utility_target_services = parameters.sandbox_info->target_services;
#endif

  ChildProcess utility_process(base::ThreadType::kDefault);
  GetContentClient()->utility()->PostIOThreadCreated(
      utility_process.io_task_runner());
  base::RunLoop run_loop;
  utility_process.set_main_thread(
      new UtilityThreadImpl(run_loop.QuitClosure()));

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)
  // Startup tracing is usually enabled earlier, but if we forked from a zygote,
  // we can only enable it after mojo IPC support is brought up initialized by
  // UtilityThreadImpl, because the mojo broker has to create the tracing SMB on
  // our behalf due to the zygote sandbox.
  if (parameters.zygote_child)
    tracing::EnableStartupTracingIfNeeded();
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)

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
  absl::optional<base::HighResolutionTimerManager> hi_res_timer_manager;
  if (base::PowerMonitor::IsInitialized()) {
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
      sandbox_type != sandbox::mojom::Sandbox::kMediaFoundationCdm &&
      sandbox_type != sandbox::mojom::Sandbox::kWindowsSystemProxyResolver) {
    if (!g_utility_target_services)
      return false;
    char buffer;
    // Ensure RtlGenRandom is warm before the token is lowered; otherwise,
    // base::RandBytes() will CHECK fail when v8 is initialized.
    base::RandBytes(&buffer, sizeof(buffer));

    g_utility_target_services->LowerToken();
  }
#endif

  internal::PartitionAllocSupport::Get()->ReconfigureAfterTaskRunnerInit(
      switches::kUtilityProcess);

  run_loop.Run();

#if defined(LEAK_SANITIZER)
  // Invoke LeakSanitizer before shutting down the utility thread, to avoid
  // reporting shutdown-only leaks.
  __lsan_do_leak_check();
#endif

  return 0;
}

}  // namespace content
