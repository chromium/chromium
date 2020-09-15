// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/embedder/main.h"

#include "base/allocator/buildflags.h"
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/activity_tracker.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/shared_memory_hooks.h"
#include "base/message_loop/message_pump_type.h"
#include "base/optional.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "sandbox/policy/sandbox_type.h"
#include "services/service_manager/embedder/main_delegate.h"
#include "services/service_manager/embedder/set_process_title.h"
#include "services/service_manager/embedder/shared_file_util.h"
#include "services/service_manager/embedder/switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include <windows.h>

#include "base/win/process_startup_helper.h"
#include "base/win/win_util.h"
#include "ui/base/win/atl_module.h"
#endif

#if defined(OS_POSIX) && !defined(OS_ANDROID)
#include <locale.h>
#include <signal.h>

#include "base/file_descriptor_store.h"
#include "base/posix/global_descriptors.h"
#endif

#if defined(OS_MAC)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "services/service_manager/embedder/mac_init.h"

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "base/allocator/allocator_shim.h"
#endif
#endif  // defined(OS_MAC)

namespace service_manager {

namespace {

// Maximum message size allowed to be read from a Mojo message pipe in any
// service manager embedder process.
constexpr size_t kMaximumMojoMessageSize = 128 * 1024 * 1024;

#if defined(OS_POSIX) && !defined(OS_ANDROID)

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
  CHECK_EQ(0, sigprocmask(SIG_SETMASK, &empty_signal_set, NULL));

  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_DFL;
  static const int signals_to_reset[] = {
      SIGHUP,  SIGINT,  SIGQUIT, SIGILL, SIGABRT, SIGFPE, SIGSEGV,
      SIGALRM, SIGTERM, SIGCHLD, SIGBUS, SIGTRAP};
  for (unsigned i = 0; i < base::size(signals_to_reset); i++) {
    CHECK_EQ(0, sigaction(signals_to_reset[i], &sigact, NULL));
  }
}

void PopulateFDsFromCommandLine() {
  const std::string& shared_file_param =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSharedFiles);
  if (shared_file_param.empty())
    return;

  base::Optional<std::map<int, std::string>> shared_file_descriptors =
      service_manager::ParseSharedFileSwitchValue(shared_file_param);
  if (!shared_file_descriptors)
    return;

  for (const auto& descriptor : *shared_file_descriptors) {
    base::MemoryMappedFile::Region region;
    const std::string& key = descriptor.second;
    base::ScopedFD fd = base::GlobalDescriptors::GetInstance()->TakeFD(
        descriptor.first, &region);
    base::FileDescriptorStore::GetInstance().Set(key, std::move(fd), region);
  }
}

#endif  // defined(OS_POSIX) && !defined(OS_ANDROID)

void CommonSubprocessInit() {
#if defined(OS_WIN)
  // HACK: Let Windows know that we have started.  This is needed to suppress
  // the IDC_APPSTARTING cursor from being displayed for a prolonged period
  // while a subprocess is starting.
  if (base::win::IsUser32AndGdi32Available()) {
    PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
    MSG msg;
    PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
  }
#endif

#if !defined(OFFICIAL_BUILD) && defined(OS_WIN)
  base::RouteStdioToConsole(false);
  LoadLibraryA("dbghelp.dll");
#endif
}

}  // namespace

MainParams::MainParams(MainDelegate* delegate) : delegate(delegate) {}

MainParams::~MainParams() {}

int Main(const MainParams& params) {
  MainDelegate* delegate = params.delegate;
  DCHECK(delegate);

  int exit_code = -1;
  base::debug::GlobalActivityTracker* tracker = nullptr;
#if defined(OS_MAC)
  std::unique_ptr<base::mac::ScopedNSAutoreleasePool> autorelease_pool;
#endif

  // A flag to indicate whether Main() has been called before. On Android, we
  // may re-run Main() without restarting the browser process. This flag
  // prevents initializing things more than once.
  static bool is_initialized = false;
#if !defined(OS_ANDROID)
  DCHECK(!is_initialized);
#endif
  if (!is_initialized) {
    is_initialized = true;
#if defined(OS_MAC) && BUILDFLAG(USE_ALLOCATOR_SHIM)
    base::allocator::InitializeAllocatorShim();
#endif
    base::EnableTerminationOnOutOfMemory();

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
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

#if defined(OS_WIN)
    base::win::RegisterInvalidParamHandler();
    ui::win::CreateATLModuleIfNeeded();
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
    // On Android, the command line is initialized when library is loaded.
    int argc = 0;
    const char** argv = nullptr;

#if !defined(OS_WIN)
    // argc/argv are ignored on Windows; see command_line.h for details.
    argc = params.argc;
    argv = params.argv;
#endif

    base::CommandLine::Init(argc, argv);

#if defined(OS_POSIX)
    PopulateFDsFromCommandLine();
#endif

    base::EnableTerminationOnHeapCorruption();

    SetProcessTitleFromCommandLine(argv);
#endif  // !defined(OS_ANDROID)

// On Android setlocale() is not supported, and we don't override the signal
// handlers so we can get a stack trace when crashing.
#if defined(OS_POSIX) && !defined(OS_ANDROID)
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

#if defined(OS_WIN)
    base::win::SetupCRT(*base::CommandLine::ForCurrentProcess());
#endif

    MainDelegate::InitializeParams init_params;

#if defined(OS_MAC)
    // We need this pool for all the objects created before we get to the event
    // loop, but we don't want to leave them hanging around until the app quits.
    // Each "main" needs to flush this pool right before it goes into its main
    // event loop to get rid of the cruft.
    autorelease_pool = std::make_unique<base::mac::ScopedNSAutoreleasePool>();
    init_params.autorelease_pool = autorelease_pool.get();
    InitializeMac();
#endif

    mojo::core::Configuration mojo_config;
    mojo_config.max_message_num_bytes = kMaximumMojoMessageSize;
    delegate->InitializeMojo(&mojo_config);

    ui::RegisterPathProvider();

    tracker = base::debug::GlobalActivityTracker::Get();
    exit_code = delegate->Initialize(init_params);
    if (exit_code >= 0) {
      if (tracker) {
        tracker->SetProcessPhase(
            base::debug::GlobalActivityTracker::PROCESS_LAUNCH_FAILED);
        tracker->process_data().SetInt("exit-code", exit_code);
      }
      return exit_code;
    }

    // Note #1: the installed shared memory hooks require a live instance of
    // mojo::core::ScopedIPCSupport to function, which is instantiated below by
    // the process type's main function. However, some implementations of the
    // service_manager::MainDelegate::Initialize() delegate method allocate
    // shared memory, so the hooks cannot be installed before the Initialize()
    // call above, or the shared memory allocation will simply fail.
    //
    // Note #2: some platforms can directly allocated shared memory in a
    // sandboxed process. The defines below must be in sync with the
    // implementation of mojo::NodeController::CreateSharedBuffer().
#if !defined(OS_MAC) && !defined(OS_NACL_SFI) && !defined(OS_FUCHSIA)
    if (sandbox::policy::IsUnsandboxedSandboxType(
            sandbox::policy::SandboxTypeFromCommandLine(
                *base::CommandLine::ForCurrentProcess()))) {
      // Unsandboxed processes don't need shared memory brokering... because
      // they're not sandboxed.
    } else if (mojo_config.force_direct_shared_memory_allocation) {
      // Don't bother with hooks if direct shared memory allocation has been
      // requested.
    } else {
      // Sanity check, since installing the shared memory hooks in a broker
      // process will lead to infinite recursion.
      DCHECK(!mojo_config.is_broker_process);
      // Otherwise, this is a sandboxed process that will need brokering to
      // allocate shared memory.
      mojo::SharedMemoryUtils::InstallBaseHooks();
    }
#endif  // !defined(OS_MAC) && !defined(OS_NACL_SFI) && !defined(OS_FUCHSIA)

#if defined(OS_WIN)
    // Route stdio to parent console (if any) or create one.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableLogging)) {
      base::RouteStdioToConsole(true);
    }
#endif

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kTraceToConsole)) {
      base::trace_event::TraceConfig trace_config =
          tracing::GetConfigForTraceToConsole();
      base::trace_event::TraceLog::GetInstance()->SetEnabled(
          trace_config, base::trace_event::TraceLog::RECORDING_MODE);
    }
  }

  if (delegate->IsEmbedderSubprocess())
    CommonSubprocessInit();
  exit_code = delegate->RunEmbedderProcess();

  if (tracker) {
    if (exit_code == 0) {
      tracker->SetProcessPhaseIfEnabled(
          base::debug::GlobalActivityTracker::PROCESS_EXITED_CLEANLY);
    } else {
      tracker->SetProcessPhaseIfEnabled(
          base::debug::GlobalActivityTracker::PROCESS_EXITED_WITH_CODE);
      tracker->process_data().SetInt("exit-code", exit_code);
    }
  }

#if defined(OS_MAC)
  autorelease_pool.reset();
#endif

  delegate->ShutDownEmbedderProcess();

  return exit_code;
}

}  // namespace service_manager
