// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/message_loop/message_pump_type.h"
#include "base/optional.h"
#include "base/power_monitor/power_monitor.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/platform_thread.h"
#include "base/timer/hi_res_timer_manager.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/child/child_process.h"
#include "content/common/content_switches_internal.h"
#include "content/common/partition_alloc_support.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/utility/content_utility_client.h"
#include "content/utility/utility_thread_impl.h"
#include "sandbox/policy/sandbox.h"
#include "services/tracing/public/cpp/trace_startup.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "content/utility/speech/speech_recognition_sandbox_hook_linux.h"
#include "printing/sandbox/print_backend_sandbox_hook_linux.h"
#include "sandbox/policy/linux/sandbox_linux.h"
#include "services/audio/audio_sandbox_hook_linux.h"
#include "services/network/network_sandbox_hook_linux.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/services/ime/ime_sandbox_hook.h"
#include "chromeos/services/tts/tts_sandbox_hook.h"
#endif

#if defined(OS_MAC)
#include "base/message_loop/message_pump_mac.h"
#endif

#if defined(OS_WIN)
#include "base/rand_util.h"
#include "sandbox/win/src/sandbox.h"

sandbox::TargetServices* g_utility_target_services = nullptr;
#endif

namespace content {

// Mainline routine for running as the utility process.
int UtilityMain(const MainFunctionParams& parameters) {
  base::MessagePumpType message_pump_type =
      parameters.command_line.HasSwitch(switches::kMessageLoopTypeUi)
          ? base::MessagePumpType::UI
          : base::MessagePumpType::DEFAULT;

#if defined(OS_MAC)
  auto sandbox_type =
      sandbox::policy::SandboxTypeFromCommandLine(parameters.command_line);
  if (sandbox_type != sandbox::policy::SandboxType::kNoSandbox) {
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

#if defined(OS_FUCHSIA)
  // On Fuchsia always use IO threads to allow FIDL calls.
  if (message_pump_type == base::MessagePumpType::DEFAULT)
    message_pump_type = base::MessagePumpType::IO;
#endif  // defined(OS_FUCHSIA)

  // The main task executor of the utility process.
  base::SingleThreadTaskExecutor main_thread_task_executor(message_pump_type);
  base::PlatformThread::SetName("CrUtilityMain");

  if (parameters.command_line.HasSwitch(switches::kUtilityStartupDialog))
    WaitForDebugger("Utility");

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Initializes the sandbox before any threads are created.
  // TODO(jorgelo): move this after GTK initialization when we enable a strict
  // Seccomp-BPF policy.
  auto sandbox_type =
      sandbox::policy::SandboxTypeFromCommandLine(parameters.command_line);
  if (parameters.zygote_child ||
      sandbox_type == sandbox::policy::SandboxType::kNetwork ||
#if BUILDFLAG(IS_CHROMEOS_ASH)
      sandbox_type == sandbox::policy::SandboxType::kIme ||
      sandbox_type == sandbox::policy::SandboxType::kTts ||
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      sandbox_type == sandbox::policy::SandboxType::kPrintBackend ||
      sandbox_type == sandbox::policy::SandboxType::kAudio ||
      sandbox_type == sandbox::policy::SandboxType::kSpeechRecognition) {
    sandbox::policy::SandboxLinux::PreSandboxHook pre_sandbox_hook;
    if (sandbox_type == sandbox::policy::SandboxType::kNetwork)
      pre_sandbox_hook = base::BindOnce(&network::NetworkPreSandboxHook);
    else if (sandbox_type == sandbox::policy::SandboxType::kPrintBackend)
      pre_sandbox_hook = base::BindOnce(&printing::PrintBackendPreSandboxHook);
    else if (sandbox_type == sandbox::policy::SandboxType::kAudio)
      pre_sandbox_hook = base::BindOnce(&audio::AudioPreSandboxHook);
    else if (sandbox_type == sandbox::policy::SandboxType::kSpeechRecognition)
      pre_sandbox_hook =
          base::BindOnce(&speech::SpeechRecognitionPreSandboxHook);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    else if (sandbox_type == sandbox::policy::SandboxType::kIme)
      pre_sandbox_hook = base::BindOnce(&chromeos::ime::ImePreSandboxHook);
    else if (sandbox_type == sandbox::policy::SandboxType::kTts)
      pre_sandbox_hook = base::BindOnce(&chromeos::tts::TtsPreSandboxHook);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    sandbox::policy::Sandbox::Initialize(
        sandbox_type, std::move(pre_sandbox_hook),
        sandbox::policy::SandboxLinux::Options());
  }
#elif defined(OS_WIN)
  g_utility_target_services = parameters.sandbox_info->target_services;
#endif

  ChildProcess utility_process;
  GetContentClient()->utility()->PostIOThreadCreated(
      utility_process.io_task_runner());
  base::RunLoop run_loop;
  utility_process.set_main_thread(
      new UtilityThreadImpl(run_loop.QuitClosure()));

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MAC)
  // Startup tracing is usually enabled earlier, but if we forked from a zygote,
  // we can only enable it after mojo IPC support is brought up initialized by
  // UtilityThreadImpl, because the mojo broker has to create the tracing SMB on
  // our behalf due to the zygote sandbox.
  if (parameters.zygote_child)
    tracing::EnableStartupTracingIfNeeded();
#endif  // OS_POSIX && !OS_ANDROID && !OS_MAC

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
  base::Optional<base::HighResolutionTimerManager> hi_res_timer_manager;
  if (base::PowerMonitor::IsInitialized()) {
    hi_res_timer_manager.emplace();
  }

#if defined(OS_WIN)
  auto sandbox_type =
      sandbox::policy::SandboxTypeFromCommandLine(parameters.command_line);

  // https://crbug.com/1076771 https://crbug.com/1075487 Premature unload of
  // shell32 caused process to crash during process shutdown.
  HMODULE shell32_pin = ::LoadLibrary(L"shell32.dll");
  UNREFERENCED_PARAMETER(shell32_pin);

  if (!sandbox::policy::IsUnsandboxedSandboxType(sandbox_type) &&
      sandbox_type != sandbox::policy::SandboxType::kCdm) {
    if (!g_utility_target_services)
      return false;
    char buffer;
    // Ensure RtlGenRandom is warm before the token is lowered; otherwise,
    // base::RandBytes() will CHECK fail when v8 is initialized.
    base::RandBytes(&buffer, sizeof(buffer));

    if (sandbox_type == sandbox::policy::SandboxType::kNetwork) {
      // Network service process needs FWPUCLNT.DLL to be loaded before sandbox
      // lockdown otherwise getaddrinfo fails.
      HMODULE fwpuclnt_pin = ::LoadLibrary(L"FWPUCLNT.DLL");
      UNREFERENCED_PARAMETER(fwpuclnt_pin);
      // Network service process needs urlmon.dll to be loaded before sandbox
      // lockdown otherwise CoInternetCreateSecurityManager fails.
      HMODULE urlmon_pin = ::LoadLibrary(L"urlmon.dll");
      UNREFERENCED_PARAMETER(urlmon_pin);
    }
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
