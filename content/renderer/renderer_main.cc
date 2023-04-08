// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/leak_annotations.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/pending_task.h"
#include "base/process/current_process.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/threading/platform_thread.h"
#include "base/timer/hi_res_timer_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_switches_internal.h"
#include "content/common/partition_alloc_support.h"
#include "content/common/skia_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_main_platform_delegate.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/mojo_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "sandbox/policy/switches.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/webrtc_overrides/init_webrtc.h"  // nogncheck
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/library_loader/library_loader_hooks.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
#include <Carbon/Carbon.h>
#include <signal.h>
#include <unistd.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop/message_pump_mac.h"
#include "third_party/blink/public/web/web_view.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(ARCH_CPU_X86_64)
#include "chromeos/ash/components/memory/userspace_swap/userspace_swap_renderer_initialization_impl.h"
#endif  // defined(X86_64)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/system/core_scheduling.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/renderer/renderer_thread_type_handler.h"
#endif

#if BUILDFLAG(ENABLE_PPAPI)
#include "content/renderer/pepper/pepper_plugin_registry.h"
#endif

#if BUILDFLAG(MOJO_RANDOM_DELAYS_ENABLED)
#include "mojo/public/cpp/bindings/lib/test_random_mojo_delays.h"
#endif

namespace content {
namespace {

// This function provides some ways to test crash and assertion handling
// behavior of the renderer.
void HandleRendererErrorTestParameters(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kWaitForDebugger))
    base::debug::WaitForDebugger(60, true);

  if (command_line.HasSwitch(switches::kRendererStartupDialog))
    WaitForDebugger("Renderer");
}

std::unique_ptr<base::MessagePump> CreateMainThreadMessagePump() {
#if BUILDFLAG(IS_MAC)
  // As long as scrollbars on Mac are painted with Cocoa, the message pump
  // needs to be backed by a Foundation-level loop to process NSTimers. See
  // http://crbug.com/306348#c24 for details.
  return base::MessagePump::Create(base::MessagePumpType::NS_RUNLOOP);
#elif BUILDFLAG(IS_FUCHSIA)
  // Allow FIDL APIs on renderer main thread.
  return base::MessagePump::Create(base::MessagePumpType::IO);
#else
  return base::MessagePump::Create(base::MessagePumpType::DEFAULT);
#endif
}

void LogTimeToStartRunLoop(const base::CommandLine& command_line,
                           base::TimeTicks run_loop_start_time) {
  if (!command_line.HasSwitch(switches::kRendererProcessLaunchTimeTicks))
    return;

  const std::string launch_time_delta_micro_as_string =
      command_line.GetSwitchValueASCII(
          switches::kRendererProcessLaunchTimeTicks);
  int64_t launch_time_delta_micro;
  if (!base::StringToInt64(launch_time_delta_micro_as_string,
                           &launch_time_delta_micro)) {
    return;
  }
  const base::TimeDelta delta = run_loop_start_time.since_origin() -
                                base::Microseconds(launch_time_delta_micro);
  base::UmaHistogramTimes("Renderer.BrowserLaunchToRunLoopStart", delta);
}

}  // namespace

// mainline routine for running as the Renderer process
int RendererMain(MainFunctionParams parameters) {
  // Don't use the TRACE_EVENT0 macro because the tracing infrastructure doesn't
  // expect synchronous events around the main loop of a thread.
  TRACE_EVENT_INSTANT0("startup", "RendererMain", TRACE_EVENT_SCOPE_THREAD);

  base::CurrentProcess::GetInstance().SetProcessType(
      base::CurrentProcessType::PROCESS_RENDERER);
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventRendererProcessSortIndex);

  const base::CommandLine& command_line = *parameters.command_line;

#if BUILDFLAG(IS_MAC)
  base::mac::ScopedNSAutoreleasePool* pool = parameters.autorelease_pool;
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // As the Zygote process starts up earlier than the browser process, it gets
  // its own locale (at login time for Chrome OS). So we have to set the ICU
  // default locale for the renderer process here.
  // ICU locale will be used for fallback font selection, etc.
  if (command_line.HasSwitch(switches::kLang)) {
    const std::string locale =
        command_line.GetSwitchValueASCII(switches::kLang);
    base::i18n::SetICUDefaultLocale(locale);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Turn on core scheduling for ash renderers only if kLacrosOnly is not
  // enabled. If kLacrosOnly is enabled, ash renderers don't run user code. This
  // means they don't need core scheduling. Lacros renderers will get core
  // scheduling in this case.
  if (!command_line.HasSwitch(switches::kAshWebBrowserDisabled)) {
    chromeos::system::EnableCoreSchedulingIfAvailable();
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // Turn on core scheduling for lacros renderers since they run user code in
  // most cases.
  chromeos::system::EnableCoreSchedulingIfAvailable();
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(ARCH_CPU_X86_64)
  using UserspaceSwapInit =
      ash::memory::userspace_swap::UserspaceSwapRendererInitializationImpl;
  absl::optional<UserspaceSwapInit> swap_init;
  if (UserspaceSwapInit::UserspaceSwapSupportedAndEnabled()) {
    swap_init.emplace();

    PLOG_IF(ERROR, !swap_init->PreSandboxSetup())
        << "Unable to complete presandbox userspace swap initialization";
  }
#endif  // defined(ARCH_CPU_X86_64)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (command_line.HasSwitch(switches::kTimeZoneForTesting)) {
    std::string time_zone =
        command_line.GetSwitchValueASCII(switches::kTimeZoneForTesting);
    icu::TimeZone::adoptDefault(
        icu::TimeZone::createTimeZone(icu::UnicodeString(time_zone.c_str())));
  }

  InitializeSkia();

  // This function allows pausing execution using the --renderer-startup-dialog
  // flag allowing us to attach a debugger.
  // Do not move this function down since that would mean we can't easily debug
  // whatever occurs before it.
  HandleRendererErrorTestParameters(command_line);

  RendererMainPlatformDelegate platform(parameters);

  base::PlatformThread::SetName("CrRendererMain");

  // Force main thread initialization. When the implementation is based on a
  // better means of determining which is the main thread, remove.
  RenderThread::IsMainThread();

  blink::Platform::InitializeBlink();
  std::unique_ptr<blink::scheduler::WebThreadScheduler> main_thread_scheduler =
      blink::scheduler::WebThreadScheduler::CreateMainThreadScheduler(
          CreateMainThreadMessagePump());

  platform.PlatformInitialize();

#if BUILDFLAG(ENABLE_PPAPI)
  // Load pepper plugins before engaging the sandbox.
  PepperPluginRegistry::GetInstance();
#endif
  // Initialize WebRTC before engaging the sandbox.
  // NOTE: On linux, this call could already have been made from
  // zygote_main_linux.cc.  However, calling multiple times from the same thread
  // is OK.
  InitializeWebRtcModule();

  {
    content::ContentRendererClient* client = GetContentClient()->renderer();
    bool should_run_loop = true;
    bool need_sandbox =
        !command_line.HasSwitch(sandbox::policy::switches::kNoSandbox);

    if (!need_sandbox) {
      // The post-sandbox actions still need to happen at some point.
      if (client) {
        client->PostSandboxInitialized();
      }
    }

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)
    // Sandbox is enabled before RenderProcess initialization on all platforms,
    // except Windows and Mac.
    // TODO(markus): Check if it is OK to remove ifdefs for Windows and Mac.
    if (need_sandbox) {
      should_run_loop = platform.EnableSandbox();
      need_sandbox = false;
      if (client) {
        client->PostSandboxInitialized();
      }
    }
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // Thread type delegate of the process should be registered before
    // first thread type change in ChildProcess constructor.
    if (base::FeatureList::IsEnabled(
            features::kHandleRendererThreadTypeChangesInBrowser)) {
      RendererThreadTypeHandler::Create();
    }
#elif BUILDFLAG(IS_FUCHSIA)
    // TODO(crbug.com/1431671) Move to using kMainThreadCompositingPriority feature in M112+
    base::PlatformThread::SetCurrentThreadType(base::ThreadType::kCompositing);
#endif

    std::unique_ptr<RenderProcess> render_process = RenderProcessImpl::Create();
    // It's not a memory leak since RenderThread has the same lifetime
    // as a renderer process.
    base::RunLoop run_loop;
    new RenderThreadImpl(run_loop.QuitClosure(),
                         std::move(main_thread_scheduler));

#if BUILDFLAG(IS_CHROMEOS_ASH) && defined(ARCH_CPU_X86_64)
    // Once the sandbox has been entered and initialization of render threads
    // complete we will transfer FDs to the browser, or close them on failure.
    // This should always be called because it will also transfer the errno that
    // prevented the creation of the userfaultfd if applicable.
    if (swap_init) {
      swap_init->TransferFDsOrCleanup(base::BindOnce(
          &RenderThread::BindHostReceiver,
          // Unretained is safe because TransferFDsOrCleanup is synchronous.
          base::Unretained(RenderThread::Get())));

      // No need to leave this around any further.
      swap_init.reset();
    }
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)
    // Startup tracing is usually enabled earlier, but if we forked from a
    // zygote, we can only enable it after mojo IPC support is brought up
    // initialized by RenderThreadImpl, because the mojo broker has to create
    // the tracing SMB on our behalf due to the zygote sandbox.
    if (parameters.zygote_child) {
      tracing::EnableStartupTracingIfNeeded();
      TRACE_EVENT_INSTANT1("startup", "RendererMain", TRACE_EVENT_SCOPE_THREAD,
                           "zygote_child", true);
    }
#endif

    if (need_sandbox) {
      should_run_loop = platform.EnableSandbox();
      if (client) {
        client->PostSandboxInitialized();
      }
    }

#if BUILDFLAG(MOJO_RANDOM_DELAYS_ENABLED)
    mojo::BeginRandomMojoDelays();
#endif

    internal::PartitionAllocSupport::Get()->ReconfigureAfterTaskRunnerInit(
        switches::kRendererProcess);

    base::HighResolutionTimerManager hi_res_timer_manager;

    if (should_run_loop) {
#if BUILDFLAG(IS_MAC)
      if (pool)
        pool->Recycle();
#endif
      TRACE_EVENT_INSTANT0("toplevel", "RendererMain.START_MSG_LOOP",
                           TRACE_EVENT_SCOPE_THREAD);
      const base::TimeTicks run_loop_start_time = base::TimeTicks::Now();
      RenderThreadImpl::current()->set_run_loop_start_time(run_loop_start_time);
      LogTimeToStartRunLoop(command_line, run_loop_start_time);
      run_loop.Run();
    }

#if defined(LEAK_SANITIZER)
    // Run leak detection before RenderProcessImpl goes out of scope. This helps
    // ignore shutdown-only leaks.
    __lsan_do_leak_check();
#endif
  }
  platform.PlatformUninitialize();
  return 0;
}

}  // namespace content
