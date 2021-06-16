// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_runner_impl.h"

#include <memory>

#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/leak_annotations.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/run_loop.h"
#include "base/synchronization/atomic_flag.h"
#include "base/time/time.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/notification_service_impl.h"
#include "content/browser/tracing/startup_tracing_controller.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/gfx/font_util.h"

#if defined(OS_ANDROID)
#include "content/browser/android/tracing_controller_android.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/base/win/scoped_ole_initializer.h"
#endif

namespace content {
namespace {

base::LazyInstance<base::AtomicFlag>::Leaky g_exited_main_message_loop;

}  // namespace

// static
std::unique_ptr<BrowserMainRunnerImpl> BrowserMainRunnerImpl::Create() {
  return std::make_unique<BrowserMainRunnerImpl>();
}

BrowserMainRunnerImpl::BrowserMainRunnerImpl()
    : initialization_started_(false),
      is_shutdown_(false),
      scoped_execution_fence_(
          std::make_unique<base::ThreadPoolInstance::ScopedExecutionFence>()) {}

BrowserMainRunnerImpl::~BrowserMainRunnerImpl() {
  if (initialization_started_ && !is_shutdown_)
    Shutdown();
}

int BrowserMainRunnerImpl::Initialize(const MainFunctionParams& parameters) {
  SCOPED_UMA_HISTOGRAM_LONG_TIMER(
      "Startup.BrowserMainRunnerImplInitializeLongTime");
  TRACE_EVENT0("startup", "BrowserMainRunnerImpl::Initialize");

  // On Android we normally initialize the browser in a series of UI thread
  // tasks. While this is happening a second request can come from the OS or
  // another application to start the browser. If this happens then we must
  // not run these parts of initialization twice.
  if (!initialization_started_) {
    initialization_started_ = true;

    const base::TimeTicks start_time_step1 = base::TimeTicks::Now();

    SkGraphics::Init();

    if (parameters.command_line.HasSwitch(switches::kWaitForDebugger))
      base::debug::WaitForDebugger(60, true);

    if (parameters.command_line.HasSwitch(switches::kBrowserStartupDialog))
      WaitForDebugger("Browser");

    notification_service_ = std::make_unique<NotificationServiceImpl>();

#if defined(OS_WIN)
    // Ole must be initialized before starting message pump, so that TSF
    // (Text Services Framework) module can interact with the message pump
    // on Windows 8 Metro mode.
    ole_initializer_ = std::make_unique<ui::ScopedOleInitializer>();
#endif  // OS_WIN

    gfx::InitializeFonts();

    main_loop_ = std::make_unique<BrowserMainLoop>(
        parameters, std::move(scoped_execution_fence_));

    main_loop_->Init();

    if (parameters.created_main_parts_closure) {
      std::move(*parameters.created_main_parts_closure)
          .Run(main_loop_->parts());
      delete parameters.created_main_parts_closure;
    }

    const int early_init_error_code = main_loop_->EarlyInitialization();
    if (early_init_error_code > 0)
      return early_init_error_code;

    // Must happen before we try to use a message loop or display any UI.
    if (!main_loop_->InitializeToolkit())
      return 1;

    main_loop_->PreCreateMainMessageLoop();
    main_loop_->CreateMainMessageLoop();
    main_loop_->PostCreateMainMessageLoop();

    // WARNING: If we get a WM_ENDSESSION, objects created on the stack here
    // are NOT deleted. If you need something to run during WM_ENDSESSION add it
    // to browser_shutdown::Shutdown or BrowserProcess::EndSession.

    ui::InitializeInputMethod();
    UMA_HISTOGRAM_TIMES("Startup.BrowserMainRunnerImplInitializeStep1Time",
                        base::TimeTicks::Now() - start_time_step1);
  }
  const base::TimeTicks start_time_step2 = base::TimeTicks::Now();
  main_loop_->CreateStartupTasks();
  int result_code = main_loop_->GetResultCode();
  if (result_code > 0)
    return result_code;

  UMA_HISTOGRAM_TIMES("Startup.BrowserMainRunnerImplInitializeStep2Time",
                      base::TimeTicks::Now() - start_time_step2);

  // Return -1 to indicate no early termination.
  return -1;
}

#if defined(OS_ANDROID)
void BrowserMainRunnerImpl::SynchronouslyFlushStartupTasks() {
  main_loop_->SynchronouslyFlushStartupTasks();
}
#endif

int BrowserMainRunnerImpl::Run() {
  DCHECK(initialization_started_);
  DCHECK(!is_shutdown_);
  main_loop_->RunMainMessageLoop();
  return main_loop_->GetResultCode();
}

void BrowserMainRunnerImpl::Shutdown() {
  DCHECK(initialization_started_);
  DCHECK(!is_shutdown_);

#ifdef LEAK_SANITIZER
  // Invoke leak detection now, to avoid dealing with shutdown-only leaks.
  // Normally this will have already happened in
  // BroserProcessImpl::ReleaseModule(), so this call has no effect. This is
  // only for processes which do not instantiate a BrowserProcess.
  // If leaks are found, the process will exit here.
  __lsan_do_leak_check();
#endif

  main_loop_->PreShutdown();

  // Finalize the startup tracing session if it is still active.
  StartupTracingController::GetInstance().ShutdownAndWaitForStopIfNeeded();

  {
    // The trace event has to stay between profiler creation and destruction.
    TRACE_EVENT0("shutdown", "BrowserMainRunner");
    g_exited_main_message_loop.Get().Set();

    main_loop_->ShutdownThreadsAndCleanUp();

    ui::ShutdownInputMethod();
#if defined(OS_WIN)
    ole_initializer_.reset(NULL);
#endif
#if defined(OS_ANDROID)
    // Forcefully terminates the RunLoop inside MessagePumpForUI, ensuring
    // proper shutdown for content_browsertests. Shutdown() is not used by
    // the actual browser.
    if (base::RunLoop::IsRunningOnCurrentThread())
      base::RunLoop::QuitCurrentDeprecated();
#endif
    main_loop_.reset(nullptr);

    notification_service_.reset(nullptr);

    is_shutdown_ = true;
  }
}

// static
std::unique_ptr<BrowserMainRunner> BrowserMainRunner::Create() {
  return BrowserMainRunnerImpl::Create();
}

// static
bool BrowserMainRunner::ExitedMainMessageLoop() {
  return g_exited_main_message_loop.IsCreated() &&
         g_exited_main_message_loop.Get().IsSet();
}

}  // namespace content
