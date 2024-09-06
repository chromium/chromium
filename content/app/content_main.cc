// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_main.h"

#include <memory>
#include <optional>

#include "base/allocator/partition_alloc_support.h"
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/process/set_process_title.h"
#include "base/profiler/sample_metadata.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/condition_variable.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/app/content_main_runner_impl.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/common/content_switches.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "partition_alloc/buildflags.h"
#include "sandbox/policy/sandbox_type.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/process_startup_helper.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/base/win/atl_module.h"
#include "ui/gfx/switches.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
#include <locale.h>
#include <signal.h>

#include "content/common/shared_file_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include "base/files/scoped_file.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#include "content/app/mac_init.h"
#endif

#if BUILDFLAG(IS_APPLE)
#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "partition_alloc/shim/allocator_shim.h"
#endif
#endif  // BUILDFLAG(IS_MAC)

namespace content {

namespace {

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)

// Setup signal-handling state: resanitize most signals, ignore SIGPIPE.
void SetupSignalHandlers() {
  // Always ignore SIGPIPE.  We check the return value of write().
  CHECK_NE(SIG_ERR, signal(SIGPIPE, SIG_IGN));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableInProcessStackTraces)) {
    // Don't interfere with sanitizer signal handlers.
    return;
  }

  // Sanitise our signal handling state. Signals that were ignored by our
  // parent will also be ignored by us. We also inherit our parent's sigmask.
  sigset_t empty_signal_set;
  CHECK_EQ(0, sigemptyset(&empty_signal_set));
  CHECK_EQ(0, sigprocmask(SIG_SETMASK, &empty_signal_set, nullptr));

  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_DFL;
  static const int signals_to_reset[] = {SIGHUP,  SIGINT,  SIGQUIT, SIGILL,
                                         SIGABRT, SIGFPE,  SIGSEGV, SIGALRM,
                                         SIGTERM, SIGCHLD, SIGBUS,  SIGTRAP};
  for (int signal_to_reset : signals_to_reset)
    CHECK_EQ(0, sigaction(signal_to_reset, &sigact, nullptr));
}

#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)

bool IsSubprocess() {
  auto type = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kProcessType);
  return type == switches::kGpuProcess ||
         type == switches::kPpapiPluginProcess ||
         type == switches::kRendererProcess ||
         type == switches::kUtilityProcess || type == switches::kZygoteProcess;
}

void CommonSubprocessInit() {
#if BUILDFLAG(IS_WIN)
  // HACK: Let Windows know that we have started.  This is needed to suppress
  // the IDC_APPSTARTING cursor from being displayed for a prolonged period
  // while a subprocess is starting.
  if (base::win::IsUser32AndGdi32Available()) {
    PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
    MSG msg;
    PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
  }
#endif

#if !defined(OFFICIAL_BUILD) && BUILDFLAG(IS_WIN)
  base::RouteStdioToConsole(false);
  LoadLibraryA("dbghelp.dll");
#endif
}

void InitTimeTicksAtUnixEpoch() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kTimeTicksAtUnixEpoch)) {
    return;
  }

  std::string time_ticks_at_unix_epoch_as_string =
      command_line->GetSwitchValueASCII(switches::kTimeTicksAtUnixEpoch);

  int64_t time_ticks_at_unix_epoch_delta_micro;
  if (!base::StringToInt64(time_ticks_at_unix_epoch_as_string,
                           &time_ticks_at_unix_epoch_delta_micro)) {
    return;
  }

  base::TimeDelta time_ticks_at_unix_epoch_delta =
      base::Microseconds(time_ticks_at_unix_epoch_delta_micro);

  base::TimeTicks time_ticks_at_unix_epoch =
      base::TimeTicks() + time_ticks_at_unix_epoch_delta;

  base::TimeTicks::SetSharedUnixEpoch(time_ticks_at_unix_epoch);
}

// Apply metadata to samples collected by the StackSamplingProfiler when tracing
// is enabled. This helps distinguish profiles with tracing overhead, e.g. due
// to background tracing, from those without.
class TracingEnabledStateObserver
    : public base::trace_event::TraceLog::EnabledStateObserver {
 public:
  void OnTraceLogEnabled() override {
    apply_sample_metadata_.emplace("TracingEnabled", 1,
                                   base::SampleMetadataScope::kProcess);
  }

  void OnTraceLogDisabled() override { apply_sample_metadata_.reset(); }

 private:
  std::optional<base::ScopedSampleMetadata> apply_sample_metadata_;
};

}  // namespace

ContentMainParams::ContentMainParams(ContentMainDelegate* delegate)
    : delegate(delegate) {}

ContentMainParams::~ContentMainParams() = default;

ContentMainParams::ContentMainParams(ContentMainParams&&) = default;
ContentMainParams& ContentMainParams::operator=(ContentMainParams&&) = default;

// This function must be marked with NO_STACK_PROTECTOR or it may crash on
// return, see the --change-stack-guard-on-fork command line flag.
NO_STACK_PROTECTOR int RunContentProcess(
    ContentMainParams params,
    ContentMainRunner* content_main_runner) {
  base::FeatureList::FailOnFeatureAccessWithoutFeatureList();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Lacros is launched with inherited priority. Revert to normal priority
  // before spawning more processes.
  base::PlatformThread::SetCurrentThreadType(base::ThreadType::kDefault);
#endif
  int exit_code = -1;
#if BUILDFLAG(IS_MAC)
  base::apple::ScopedNSAutoreleasePool autorelease_pool;
#endif

  // A flag to indicate whether Main() has been called before. On Android, we
  // may re-run Main() without restarting the browser process. This flag
  // prevents initializing things more than once.
  static bool is_initialized = false;
#if !BUILDFLAG(IS_ANDROID)
  DCHECK(!is_initialized);
#endif
  if (is_initialized) {
    content_main_runner->ReInitializeParams(std::move(params));
  } else {
    is_initialized = true;
#if BUILDFLAG(IS_APPLE) && PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
    allocator_shim::InitializeAllocatorShim();
#endif
    base::EnableTerminationOnOutOfMemory();
    logging::RegisterAbslAbortHook();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // The various desktop environments set this environment variable that
    // allows the dbus client library to connect directly to the bus. When this
    // variable is not set (test environments like xvfb-run), the dbus client
    // library will fall back to auto-launch mode. Auto-launch is dangerous as
    // it can cause hangs (crbug.com/715658) . This one line disables the dbus
    // auto-launch, by clobbering the DBUS_SESSION_BUS_ADDRESS env variable if
    // not already set. The old auto-launch behavior, if needed, can be restored
    // by setting DBUS_SESSION_BUS_ADDRESS="autolaunch:" before launching
    // chrome.
    const int kNoOverrideIfAlreadySet = 0;
    setenv("DBUS_SESSION_BUS_ADDRESS", "disabled:", kNoOverrideIfAlreadySet);
#endif

#if BUILDFLAG(IS_WIN)
    base::win::RegisterInvalidParamHandler();
    ui::win::CreateATLModuleIfNeeded();
#endif  // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID)
    // On Android, the command line is initialized when library is loaded.
    int argc = 0;
    const char** argv = nullptr;

#if !BUILDFLAG(IS_WIN)
    // argc/argv are ignored on Windows; see command_line.h for details.
    argc = params.argc;
    argv = params.argv;
#endif

    base::CommandLine::Init(argc, argv);

#if BUILDFLAG(IS_POSIX)
    PopulateFileDescriptorStoreFromFdTable();
#endif

    base::EnableTerminationOnHeapCorruption();

    base::SetProcessTitleFromCommandLine(argv);
#endif  // !BUILDFLAG(IS_ANDROID)

    InitTimeTicksAtUnixEpoch();

// On Android setlocale() is not supported, and we don't override the signal
// handlers so we can get a stack trace when crashing.
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
    // Set C library locale to make sure CommandLine can parse
    // argument values in the correct encoding and to make sure
    // generated file names (think downloads) are in the file system's
    // encoding.
    setlocale(LC_ALL, "");
    // For numbers we never want the C library's locale sensitive
    // conversion from number to string because the only thing it
    // changes is the decimal separator which is not good enough for
    // the UI and can be harmful elsewhere. User interface number
    // conversions need to go through ICU. Other conversions need to
    // be locale insensitive so we force the number locale back to the
    // default, "C", locale.
    setlocale(LC_NUMERIC, "C");

    SetupSignalHandlers();
#endif

#if BUILDFLAG(IS_WIN)
    base::win::SetupCRT(*base::CommandLine::ForCurrentProcess());
#endif

#if BUILDFLAG(IS_MAC)
    // We need this pool for all the objects created before we get to the event
    // loop, but we don't want to leave them hanging around until the app quits.
    // Each "main" needs to flush this pool right before it goes into its main
    // event loop to get rid of the cruft. TODO(crbug.com/40260311): This
    // is not safe. Each main loop should create and destroy its own pool; it
    // should not be flushing the pool at the base of the autorelease pool
    // stack.
    params.autorelease_pool = &autorelease_pool;
    InitializeMac();
#endif

#if BUILDFLAG(IS_IOS)
    base::ConditionVariable::InitializeFeatures();
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kEnableViewport);
    command_line->AppendSwitch(switches::kUseMobileUserAgent);
#endif

#if (BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)) && !defined(COMPONENT_BUILD)
    base::subtle::EnableFDOwnershipEnforcement(true);
#endif

    ui::RegisterPathProvider();
    exit_code = content_main_runner->Initialize(std::move(params));

    if (exit_code >= 0) {
      return exit_code;
    }

#if BUILDFLAG(IS_WIN)
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kHeadless)) {
      // When running in headless mode we want stdio routed however if
      // console does not exist we should not create one.
      base::RouteStdioToConsole(/*create_console_if_not_found*/ false);
    } else if (command_line->HasSwitch(switches::kEnableLogging)) {
      // Route stdio to parent console (if any) or create one, do not create a
      // console in children if handles are being passed.
      bool create_console = command_line->GetSwitchValueASCII(
                                switches::kEnableLogging) != "handle";
      base::RouteStdioToConsole(create_console);
    }
#endif

    base::trace_event::TraceLog::GetInstance()->AddOwnedEnabledStateObserver(
        base::WrapUnique(new TracingEnabledStateObserver));

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kTraceToConsole)) {
      base::trace_event::TraceConfig trace_config =
          tracing::GetConfigForTraceToConsole();
      base::trace_event::TraceLog::GetInstance()->SetEnabled(
          trace_config, base::trace_event::TraceLog::RECORDING_MODE);
    }
  }

  if (IsSubprocess())
    CommonSubprocessInit();
  exit_code = content_main_runner->Run();

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  content_main_runner->Shutdown();
#endif

  return exit_code;
}

// This function must be marked with NO_STACK_PROTECTOR or it may crash on
// return, see the --change-stack-guard-on-fork command line flag.
NO_STACK_PROTECTOR int ContentMain(ContentMainParams params) {
  auto runner = ContentMainRunner::Create();
  return RunContentProcess(std::move(params), runner.get());
}

}  // namespace content
