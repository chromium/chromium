// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/win/host_service.h"

#include <windows.h>
#include <sddl.h>
#include <wtsapi32.h>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread.h"
#include "base/win/message_window.h"
#include "base/win/scoped_com_initializer.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/host/branding.h"
#include "remoting/host/daemon_process.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/logging.h"
#include "remoting/host/win/com_security.h"
#include "remoting/host/win/core_resource.h"
#include "remoting/host/win/wts_terminal_observer.h"

namespace remoting {

namespace {

const char kIoThreadName[] = "I/O thread";

// Command line switches:

// "--console" runs the service interactively for debugging purposes.
const char kConsoleSwitchName[] = "console";

// Security descriptor allowing local processes running under SYSTEM or
// LocalService accounts to call COM methods exposed by the daemon.
const wchar_t kComProcessSd[] =
    SDDL_OWNER L":" SDDL_LOCAL_SYSTEM
    SDDL_GROUP L":" SDDL_LOCAL_SYSTEM
    SDDL_DACL L":"
    SDDL_ACE(SDDL_ACCESS_ALLOWED, SDDL_COM_EXECUTE_LOCAL, SDDL_LOCAL_SYSTEM)
    SDDL_ACE(SDDL_ACCESS_ALLOWED, SDDL_COM_EXECUTE_LOCAL, SDDL_LOCAL_SERVICE);

// Appended to |kComProcessSd| to specify that only callers running at medium or
// higher integrity level are allowed to call COM methods exposed by the daemon.
const wchar_t kComProcessMandatoryLabel[] =
    SDDL_SACL L":"
    SDDL_ACE(SDDL_MANDATORY_LABEL, SDDL_NO_EXECUTE_UP, SDDL_ML_MEDIUM);

}  // namespace

HostService* HostService::GetInstance() {
  return base::Singleton<HostService>::get();
}

bool HostService::InitWithCommandLine(const base::CommandLine* command_line) {
  base::CommandLine::StringVector args = command_line->GetArgs();
  if (!args.empty()) {
    LOG(ERROR) << "No positional parameters expected.";
    return false;
  }

  // Run interactively if needed.
  if (run_routine_ == &HostService::RunAsService &&
      command_line->HasSwitch(kConsoleSwitchName)) {
    run_routine_ = &HostService::RunInConsole;
  }

  return true;
}

int HostService::Run() {
  return (this->*run_routine_)();
}

bool HostService::AddWtsTerminalObserver(const std::string& terminal_id,
                                         WtsTerminalObserver* observer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  RegisteredObserver registered_observer;
  registered_observer.terminal_id = terminal_id;
  registered_observer.session_id = kInvalidSessionId;
  registered_observer.observer = observer;

  bool session_id_found = false;
  std::list<RegisteredObserver>::const_iterator i;
  for (i = observers_.begin(); i != observers_.end(); ++i) {
    // Get the attached session ID from another observer watching the same WTS
    // console if any.
    if (i->terminal_id == terminal_id) {
      registered_observer.session_id = i->session_id;
      session_id_found = true;
    }

    // Check that |observer| hasn't been registered already.
    if (i->observer == observer)
      return false;
  }

  // If |terminal_id| is new, enumerate all sessions to see if there is one
  // attached to |terminal_id|.
  if (!session_id_found)
    registered_observer.session_id = LookupSessionId(terminal_id);

  observers_.push_back(registered_observer);

  if (registered_observer.session_id != kInvalidSessionId) {
    observer->OnSessionAttached(registered_observer.session_id);
  }

  return true;
}

void HostService::RemoveWtsTerminalObserver(WtsTerminalObserver* observer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  std::list<RegisteredObserver>::const_iterator i;
  for (i = observers_.begin(); i != observers_.end(); ++i) {
    if (i->observer == observer) {
      observers_.erase(i);
      return;
    }
  }
}

HostService::HostService()
    : run_routine_(&HostService::RunAsService),
      service_status_handle_(0),
      stopped_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {}

HostService::~HostService() {
}

void HostService::OnSessionChange(uint32_t event, uint32_t session_id) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(session_id, kInvalidSessionId);

  // Process only attach/detach notifications.
  if (event != WTS_CONSOLE_CONNECT && event != WTS_CONSOLE_DISCONNECT &&
      event != WTS_REMOTE_CONNECT && event != WTS_REMOTE_DISCONNECT) {
    return;
  }

  // Assuming that notification can arrive later query the current state of
  // |session_id|.
  std::string terminal_id;
  bool attached = LookupTerminalId(session_id, &terminal_id);

  std::list<RegisteredObserver>::iterator i = observers_.begin();
  while (i != observers_.end()) {
    std::list<RegisteredObserver>::iterator next = i;
    ++next;

    // Issue a detach notification if the session was detached from a client or
    // if it is now attached to a different client.
    if (i->session_id == session_id &&
        (!attached || !(i->terminal_id == terminal_id))) {
      i->session_id = kInvalidSessionId;
      i->observer->OnSessionDetached();
      i = next;
      continue;
    }

    // The client currently attached to |session_id| was attached to a different
    // session before. Reconnect it to |session_id|.
    if (attached && i->terminal_id == terminal_id &&
        i->session_id != session_id) {
      WtsTerminalObserver* observer = i->observer;

      if (i->session_id != kInvalidSessionId) {
        i->session_id = kInvalidSessionId;
        i->observer->OnSessionDetached();
      }

      // Verify that OnSessionDetached() above didn't remove |observer|
      // from the list.
      std::list<RegisteredObserver>::iterator j = next;
      --j;
      if (j->observer == observer) {
        j->session_id = session_id;
        observer->OnSessionAttached(session_id);
      }
    }

    i = next;
  }
}

void HostService::CreateLauncher(
    scoped_refptr<AutoThreadTaskRunner> task_runner) {
  // Launch the I/O thread.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner =
      AutoThread::CreateWithType(kIoThreadName, task_runner,
                                 base::MessagePumpType::IO);
  if (!io_task_runner.get()) {
    LOG(FATAL) << "Failed to start the I/O thread";
    return;
  }

  daemon_process_ = DaemonProcess::Create(
      task_runner,
      io_task_runner,
      base::Bind(&HostService::StopDaemonProcess, weak_ptr_));
}

int HostService::RunAsService() {
  SERVICE_TABLE_ENTRYW dispatch_table[] = {
    { const_cast<LPWSTR>(kWindowsServiceName), &HostService::ServiceMain },
    { nullptr, nullptr }
  };

  if (!StartServiceCtrlDispatcherW(dispatch_table)) {
    PLOG(ERROR) << "Failed to connect to the service control manager";
    return kInitializationFailed;
  }

  // Wait until the service thread completely exited to avoid concurrent
  // teardown of objects registered with base::AtExitManager and object
  // destoyed by the service thread.
  stopped_event_.Wait();

  return kSuccessExitCode;
}

void HostService::RunAsServiceImpl() {
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  base::RunLoop run_loop;
  main_task_runner_ = main_task_executor.task_runner();
  weak_ptr_ = weak_factory_.GetWeakPtr();

  // Register the service control handler.
  service_status_handle_ = RegisterServiceCtrlHandlerExW(
      kWindowsServiceName, &HostService::ServiceControlHandler, this);
  if (service_status_handle_ == 0) {
    PLOG(ERROR) << "Failed to register the service control handler";
    return;
  }

  // Report running status of the service.
  SERVICE_STATUS service_status;
  ZeroMemory(&service_status, sizeof(service_status));
  service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  service_status.dwCurrentState = SERVICE_RUNNING;
  service_status.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN |
                                      SERVICE_ACCEPT_STOP |
                                      SERVICE_ACCEPT_SESSIONCHANGE;
  service_status.dwWin32ExitCode = kSuccessExitCode;
  if (!SetServiceStatus(service_status_handle_, &service_status)) {
    PLOG(ERROR)
        << "Failed to report service status to the service control manager";
    return;
  }

  // Initialize COM.
  base::win::ScopedCOMInitializer com_initializer;
  if (!com_initializer.Succeeded())
    return;

  if (!InitializeComSecurity(base::WideToUTF8(kComProcessSd),
                             base::WideToUTF8(kComProcessMandatoryLabel),
                             false)) {
    return;
  }

  CreateLauncher(scoped_refptr<AutoThreadTaskRunner>(
      new AutoThreadTaskRunner(main_task_runner_,
                               run_loop.QuitClosure())));

  // Run the service.
  run_loop.Run();
  weak_factory_.InvalidateWeakPtrs();

  // Tell SCM that the service is stopped.
  service_status.dwCurrentState = SERVICE_STOPPED;
  service_status.dwControlsAccepted = 0;
  if (!SetServiceStatus(service_status_handle_, &service_status)) {
    PLOG(ERROR)
        << "Failed to report service status to the service control manager";
    return;
  }
}

int HostService::RunInConsole() {
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  base::RunLoop run_loop;
  main_task_runner_ = main_task_executor.task_runner();
  weak_ptr_ = weak_factory_.GetWeakPtr();

  int result = kInitializationFailed;

  // Initialize COM.
  base::win::ScopedCOMInitializer com_initializer;
  if (!com_initializer.Succeeded())
    return result;

  if (!InitializeComSecurity(base::WideToUTF8(kComProcessSd),
                             base::WideToUTF8(kComProcessMandatoryLabel),
                             false)) {
    return result;
  }

  // Subscribe to Ctrl-C and other console events.
  if (!SetConsoleCtrlHandler(&HostService::ConsoleControlHandler, TRUE)) {
    PLOG(ERROR) << "Failed to set console control handler";
    return result;
  }

  // Create a window for receiving session change notifications.
  base::win::MessageWindow window;
  if (!window.Create(base::Bind(&HostService::HandleMessage,
                                base::Unretained(this)))) {
    PLOG(ERROR) << "Failed to create the session notification window";
    goto cleanup;
  }

  // Subscribe to session change notifications.
  if (WTSRegisterSessionNotification(window.hwnd(),
                                     NOTIFY_FOR_ALL_SESSIONS) != FALSE) {
    CreateLauncher(scoped_refptr<AutoThreadTaskRunner>(
        new AutoThreadTaskRunner(main_task_runner_,
                                 run_loop.QuitClosure())));

    // Run the service.
    run_loop.Run();

    // Release the control handler.
    stopped_event_.Signal();

    WTSUnRegisterSessionNotification(window.hwnd());
    result = kSuccessExitCode;
  }

cleanup:
  weak_factory_.InvalidateWeakPtrs();

  // Unsubscribe from console events. Ignore the exit code. There is nothing
  // we can do about it now and the program is about to exit anyway. Even if
  // it crashes nothing is going to be broken because of it.
  SetConsoleCtrlHandler(&HostService::ConsoleControlHandler, FALSE);

  return result;
}

void HostService::StopDaemonProcess() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  daemon_process_.reset();
}

bool HostService::HandleMessage(
    UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result) {
  if (message == WM_WTSSESSION_CHANGE) {
    OnSessionChange(wparam, lparam);
    *result = 0;
    return true;
  }

  return false;
}

// static
BOOL WINAPI HostService::ConsoleControlHandler(DWORD event) {
  HostService* self = HostService::GetInstance();
  switch (event) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
      self->main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&HostService::StopDaemonProcess, self->weak_ptr_));
      return TRUE;

    default:
      return FALSE;
  }
}

// static
DWORD WINAPI HostService::ServiceControlHandler(DWORD control,
                                                DWORD event_type,
                                                LPVOID event_data,
                                                LPVOID context) {
  HostService* self = reinterpret_cast<HostService*>(context);
  switch (control) {
    case SERVICE_CONTROL_INTERROGATE:
      return NO_ERROR;

    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
      self->main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&HostService::StopDaemonProcess, self->weak_ptr_));
      return NO_ERROR;

    case SERVICE_CONTROL_SESSIONCHANGE:
      self->main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&HostService::OnSessionChange, self->weak_ptr_,
                         event_type,
                         reinterpret_cast<WTSSESSION_NOTIFICATION*>(event_data)
                             ->dwSessionId));
      return NO_ERROR;

    default:
      return ERROR_CALL_NOT_IMPLEMENTED;
  }
}

// static
VOID WINAPI HostService::ServiceMain(DWORD argc, WCHAR* argv[]) {
  HostService* self = HostService::GetInstance();

  // Run the service.
  self->RunAsServiceImpl();

  // Release the control handler and notify the main thread that it can exit
  // now.
  self->stopped_event_.Signal();
}

int DaemonProcessMain() {
  HostService* service = HostService::GetInstance();
  if (!service->InitWithCommandLine(base::CommandLine::ForCurrentProcess())) {
    return kInvalidCommandLineExitCode;
  }

  return service->Run();
}

}  // namespace remoting
