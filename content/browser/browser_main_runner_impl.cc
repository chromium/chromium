// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_runner_impl.h"

#include <memory>

#include "base/allocator/partition_alloc_features.h"
#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/leak_annotations.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/synchronization/atomic_flag.h"
#include "base/time/time.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/tracing/startup_tracing_controller.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/gfx/font_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/android/tracing_controller_android.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/base/win/scoped_ole_initializer.h"
#endif

namespace content {
namespace {

base::AtomicFlag& GetExitedMainMessageLoopFlag() {
  static base::NoDestructor<base::AtomicFlag> flag;
  return *flag;
}

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
  if (initialization_started_ && !is_shutdown_) {
    Shutdown();
  }
}

int BrowserMainRunnerImpl::Initialize(MainFunctionParams parameters) {
  SCOPED_UMA_HISTOGRAM_LONG_TIMER(
      "Startup.BrowserMainRunnerImplInitializeLongTime");
  TRACE_EVENT0("startup", "BrowserMainRunnerImpl::Initialize");

  // On Android we normally initialize the browser in a series of UI thread
  // tasks. While this is happening a second request can come from the OS or
  // another application to start the browser. If this happens then we must
  // not run these parts of initialization twice.
  if (!initialization_started_) {
    initialization_started_ = true;

    SkGraphics::Init();

    if (parameters.command_line->HasSwitch(switches::kWaitForDebugger)) {
      base::debug::WaitForDebugger(60, true);
    }

    if (parameters.command_line->HasSwitch(switches::kBrowserStartupDialog)) {
      WaitForDebugger("Browser");
    }

#if BUILDFLAG(IS_WIN)
    base::win::EnableHighDPISupport();
    // Ole must be initialized before starting message pump, so that TSF
    // (Text Services Framework) module can interact with the message pump
    // on Windows 8 Metro mode.
    ole_initializer_ = std::make_unique<ui::ScopedOleInitializer>();
#endif  // BUILDFLAG(IS_WIN)

    gfx::InitializeFonts();

    auto created_main_parts_closure =
        std::move(parameters.created_main_parts_closure);

    main_loop_ = std::make_unique<BrowserMainLoop>(
        std::move(parameters), std::move(scoped_execution_fence_));

    main_loop_->Init();

    if (created_main_parts_closure) {
      std::move(created_main_parts_closure).Run(main_loop_->parts());
    }

    const int early_init_error_code = main_loop_->EarlyInitialization();
    if (early_init_error_code > 0) {
      main_loop_->CreateMessageLoopForEarlyShutdown();
      return early_init_error_code;
    }

    // Must happen before we try to use a message loop or display any UI.
    if (!main_loop_->InitializeToolkit()) {
      main_loop_->CreateMessageLoopForEarlyShutdown();
      return 1;
    }

    main_loop_->PreCreateMainMessageLoop();
    main_loop_->CreateMainMessageLoop();
    main_loop_->PostCreateMainMessageLoop();

    // WARNING: If we get a WM_ENDSESSION, objects created on the stack here
    // are NOT deleted. If you need something to run during WM_ENDSESSION add it
    // to browser_shutdown::Shutdown or BrowserProcess::EndSession.

    ui::InitializeInputMethod();
  }
  main_loop_->CreateStartupTasks();
  int result_code = main_loop_->GetResultCode();
  if (result_code > 0) {
    return result_code;
  }

  // Return -1 to indicate no early termination.
  return -1;
}

#if BUILDFLAG(IS_ANDROID)
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

  // Here and thereafter, `MakeFreeNoOp()` will make `free()` a no-op if
  // 1. The pertinent experiment is enabled and
  // 2. The feature param's value equals the arg fed to
  //    `MakeFreeNoOp()`.
  //
  // For example, clients with the feature param set to
  // `before-preshutdown`, which maps to `kBeforePreShutdown`, will
  // have `free()` become a no-op after this call.
  base::features::MakeFreeNoOp(
      base::features::WhenFreeBecomesNoOp::kBeforePreShutdown);

  main_loop_->PreShutdown();

  base::features::MakeFreeNoOp(base::features::WhenFreeBecomesNoOp::
                                   kBeforeHaltingStartupTracingController);

  // Finalize the startup tracing session if it is still active.
  StartupTracingController::GetInstance().ShutdownAndWaitForStopIfNeeded();

  {
    // The trace event has to stay between profiler creation and destruction.
    TRACE_EVENT0("shutdown", "BrowserMainRunner");
    GetExitedMainMessageLoopFlag().Set();

    base::features::MakeFreeNoOp(
        base::features::WhenFreeBecomesNoOp::kBeforeShutDownThreads);

    main_loop_->ShutdownThreadsAndCleanUp();

    base::features::MakeFreeNoOp(
        base::features::WhenFreeBecomesNoOp::kAfterShutDownThreads);

    ui::ShutdownInputMethod();
#if BUILDFLAG(IS_WIN)
    ole_initializer_.reset(NULL);
#endif
    main_loop_.reset(nullptr);

    is_shutdown_ = true;
  }
}

// static
std::unique_ptr<BrowserMainRunner> BrowserMainRunner::Create() {
  return BrowserMainRunnerImpl::Create();
}

// static
bool BrowserMainRunner::ExitedMainMessageLoop() {
  return GetExitedMainMessageLoopFlag().IsSet();
}

}  // namespace content
