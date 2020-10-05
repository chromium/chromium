// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_native_messaging_host_main.h"

#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "net/base/network_change_notifier.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/breakpad.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/it2me/it2me_native_messaging_host.h"
#include "remoting/host/logging.h"
#include "remoting/host/native_messaging/native_messaging_pipe.h"
#include "remoting/host/native_messaging/pipe_messaging_channel.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/host/resources.h"
#include "remoting/host/switches.h"
#include "remoting/host/usage_stats_consent.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include <gtk/gtk.h>

#include "base/linux_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_APPLE)
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "remoting/host/desktop_capturer_checker.h"
#include "remoting/host/mac/permission_utils.h"
#endif  // defined(OS_APPLE)

#if defined(OS_WIN)
#include <commctrl.h>

#include "remoting/host/switches.h"
#endif  // defined(OS_WIN)

namespace remoting {

namespace {

#if defined(OS_WIN) && defined(OFFICIAL_BUILD)
bool CurrentProcessHasUiAccess() {
  HANDLE process_token;
  OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token);

  DWORD size;
  DWORD uiaccess_value = 0;
  if (!GetTokenInformation(process_token, TokenUIAccess, &uiaccess_value,
                           sizeof(uiaccess_value), &size)) {
    PLOG(ERROR) << "GetTokenInformation() failed";
  }
  CloseHandle(process_token);
  return uiaccess_value != 0;
}
#endif  // defined(OS_WIN) && defined(OFFICIAL_BUILD)

}  // namespace

// Creates a It2MeNativeMessagingHost instance, attaches it to stdin/stdout and
// runs the task executor until It2MeNativeMessagingHost signals shutdown.
int It2MeNativeMessagingHostMain(int argc, char** argv) {
  // This object instance is required by Chrome code (such as
  // SingleThreadTaskExecutor).
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  remoting::InitHostLogging();

#if defined(OS_APPLE)
  // Needed so we don't leak objects when threads are created.
  base::mac::ScopedNSAutoreleasePool pool;
#endif  // defined(OS_APPLE)

#if defined(REMOTING_ENABLE_BREAKPAD)
  // Initialize Breakpad as early as possible. On Mac the command-line needs to
  // be initialized first, so that the preference for crash-reporting can be
  // looked up in the config file.
  if (IsUsageStatsAllowed()) {
    InitializeCrashReporting();
  }
#endif  // defined(REMOTING_ENABLE_BREAKPAD)

#if defined(OS_WIN)
  // Register and initialize common controls.
  INITCOMMONCONTROLSEX info;
  info.dwSize = sizeof(info);
  info.dwICC = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&info);
#endif  // defined(OS_WIN)

  // Required to find the ICU data file, used by some file_util routines.
  base::i18n::InitializeICU();

  mojo::core::Init();

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("It2Me");

  remoting::LoadResources("");

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Required in order for us to run multiple X11 threads.
  XInitThreads();

  // Create an X11EventSource so the global X11 connection
  // (x11::Connection::Get()) can dispatch X events.
  auto event_source =
      std::make_unique<ui::X11EventSource>(x11::Connection::Get());

  // Required for any calls into GTK functions, such as the Disconnect and
  // Continue windows. Calling with nullptr arguments because we don't have
  // any command line arguments for gtk to consume.
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_init();
#else
  gtk_init(nullptr, nullptr);
#endif

  // Need to prime the host OS version value for linux to prevent IO on the
  // network thread. base::GetLinuxDistro() caches the result.
  base::GetLinuxDistro();
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  base::File read_file;
  base::File write_file;
  bool needs_elevation = false;

#if defined(OS_WIN)

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kElevateSwitchName)) {
#if defined(OFFICIAL_BUILD)
    // Unofficial builds won't have 'UiAccess' since it requires signing.
    if (!CurrentProcessHasUiAccess()) {
      LOG(ERROR) << "UiAccess permission missing from elevated It2Me process.";
    }
#endif  // defined(OFFICIAL_BUILD)

    // The UiAccess binary should always have the "input" and "output" switches
    // specified, they represent the name of the named pipes that should be used
    // in place of stdin and stdout.
    DCHECK(command_line->HasSwitch(kInputSwitchName));
    DCHECK(command_line->HasSwitch(kOutputSwitchName));

    // presubmit: allow wstring
    std::wstring input_pipe_name =
        command_line->GetSwitchValueNative(kInputSwitchName);
    // presubmit: allow wstring
    std::wstring output_pipe_name =
        command_line->GetSwitchValueNative(kOutputSwitchName);

    // A NULL SECURITY_ATTRIBUTES signifies that the handle can't be inherited.
    read_file =
        base::File(CreateFile(input_pipe_name.c_str(), GENERIC_READ, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!read_file.IsValid()) {
      PLOG(ERROR) << "CreateFile failed on '" << input_pipe_name << "'";
      return kInitializationFailed;
    }

    write_file = base::File(CreateFile(output_pipe_name.c_str(), GENERIC_WRITE,
                                       0, nullptr, OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!write_file.IsValid()) {
      PLOG(ERROR) << "CreateFile failed on '" << output_pipe_name << "'";
      return kInitializationFailed;
    }
  } else {
    needs_elevation = true;

    // GetStdHandle() returns pseudo-handles for stdin and stdout even if
    // the hosting executable specifies "Windows" subsystem. However the
    // returned  handles are invalid in that case unless standard input and
    // output are redirected to a pipe or file.
    read_file = base::File(GetStdHandle(STD_INPUT_HANDLE));
    write_file = base::File(GetStdHandle(STD_OUTPUT_HANDLE));

    // After the native messaging channel starts the native messaging reader
    // will keep doing blocking read operations on the input named pipe.
    // If any other thread tries to perform any operation on STDIN, it will also
    // block because the input named pipe is synchronous (non-overlapped).
    // It is pretty common for a DLL to query the device info (GetFileType) of
    // the STD* handles at startup. So any LoadLibrary request can potentially
    // be blocked. To prevent that from happening we close STDIN and STDOUT
    // handles as soon as we retrieve the corresponding file handles.
    SetStdHandle(STD_INPUT_HANDLE, nullptr);
    SetStdHandle(STD_OUTPUT_HANDLE, nullptr);
  }
#elif defined(OS_POSIX)
  // The files are automatically closed.
  read_file = base::File(STDIN_FILENO);
  write_file = base::File(STDOUT_FILENO);
#else
#error Not implemented.
#endif

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  base::RunLoop run_loop;

#if defined(OS_APPLE)
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(kCheckAccessibilityPermissionSwitchName)) {
    return mac::CanInjectInput() ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if (cmd_line->HasSwitch(kCheckScreenRecordingPermissionSwitchName)) {
    // Trigger screen-capture, even if CanRecordScreen() returns true. It uses a
    // heuristic that might not be 100% reliable, but it is critically
    // important to add the host bundle to the list of apps under
    // Security & Privacy -> Screen Recording.
    if (base::mac::IsAtLeastOS10_15()) {
      DesktopCapturerChecker().TriggerSingleCapture();
    }
    return mac::CanRecordScreen() ? EXIT_SUCCESS : EXIT_FAILURE;
  }
#endif  // defined(OS_APPLE)

  // NetworkChangeNotifier must be initialized after SingleThreadTaskExecutor.
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier(
      net::NetworkChangeNotifier::CreateIfNeeded());

  std::unique_ptr<It2MeHostFactory> factory(new It2MeHostFactory());

  std::unique_ptr<NativeMessagingPipe> native_messaging_pipe(
      new NativeMessagingPipe());

  // Set up the native messaging channel.
  std::unique_ptr<extensions::NativeMessagingChannel> channel(
      new PipeMessagingChannel(std::move(read_file), std::move(write_file)));

#if defined(OS_POSIX)
  PipeMessagingChannel::ReopenStdinStdout();
#endif  // defined(OS_POSIX)

  std::unique_ptr<ChromotingHostContext> context =
      ChromotingHostContext::Create(new remoting::AutoThreadTaskRunner(
          main_task_executor.task_runner(), run_loop.QuitClosure()));
  std::unique_ptr<PolicyWatcher> policy_watcher =
      PolicyWatcher::CreateWithTaskRunner(context->file_task_runner());
  std::unique_ptr<extensions::NativeMessageHost> host(
      new It2MeNativeMessagingHost(needs_elevation, std::move(policy_watcher),
                                   std::move(context), std::move(factory)));

  host->Start(native_messaging_pipe.get());

  native_messaging_pipe->Start(std::move(host), std::move(channel));

  // Run the loop until channel is alive.
  run_loop.Run();

  // Block until tasks blocking shutdown have completed their execution.
  base::ThreadPoolInstance::Get()->Shutdown();

  return kSuccessExitCode;
}

}  // namespace remoting
