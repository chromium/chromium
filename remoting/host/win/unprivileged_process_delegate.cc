// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/win/unprivileged_process_delegate.h"

// clang-format off
#include <windows.h>  // Must be in front of other Windows header files.
// clang-format on

#include <sddl.h>

#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/scoped_handle.h"
#include "base/win/sid.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "remoting/base/breakpad_utils.h"
#include "remoting/base/typed_buffer.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/security_descriptor.h"
#include "remoting/host/win/window_station_and_desktop.h"

using base::win::ScopedHandle;
using base::win::Sid;

namespace remoting {

namespace {

// The security descriptors below are used to lock down access to the worker
// process launched by UnprivilegedProcessDelegate. UnprivilegedProcessDelegate
// assumes that it runs under SYSTEM. The worker process is launched under
// a different account and attaches to a newly created window station. If UAC is
// supported by the OS, the worker process is started at low integrity level.
// UnprivilegedProcessDelegate replaces the first printf parameter in
// the strings below by the logon SID assigned to the worker process.

// Security descriptor of the desktop the worker process attaches to. It gives
// SYSTEM and the logon SID full access to the desktop.
const char kDesktopSdFormat[] = "O:SYG:SYD:(A;;0xf01ff;;;SY)(A;;0xf01ff;;;%s)";

// Mandatory label specifying low integrity level.
const char kLowIntegrityMandatoryLabel[] = "S:(ML;CIOI;NW;;;LW)";

// Security descriptor of the window station the worker process attaches to. It
// gives SYSTEM and the logon SID full access the window station. The child
// containers and objects inherit ACE giving SYSTEM and the logon SID full
// access to them as well.
const char kWindowStationSdFormat[] =
    "O:SYG:SYD:(A;CIOIIO;GA;;;SY)"
    "(A;CIOIIO;GA;;;%s)(A;NP;0xf037f;;;SY)(A;NP;0xf037f;;;%s)";

// Security descriptor of the worker process. It gives access SYSTEM full access
// to the process. It gives READ_CONTROL, SYNCHRONIZE, PROCESS_QUERY_INFORMATION
// and PROCESS_TERMINATE rights to the built-in administrators group.  It also
// gives PROCESS_QUERY_LIMITED_INFORMATION to the authenticated users group.
const char kWorkerProcessSd[] =
    "O:SYG:SYD:(A;;GA;;;SY)(A;;0x120401;;;BA)(A;;0x1000;;;AU)";

// Security descriptor of the worker process threads. It gives access SYSTEM
// full access to the threads. It gives READ_CONTROL, SYNCHRONIZE,
// THREAD_QUERY_INFORMATION and THREAD_TERMINATE rights to the built-in
// administrators group.
const char kWorkerThreadSd[] = "O:SYG:SYD:(A;;GA;;;SY)(A;;0x120801;;;BA)";

// Creates a token with limited access that will be used to run the worker
// process.
bool CreateRestrictedToken(ScopedHandle* token_out) {
  // Create a token representing LocalService account.
  HANDLE temp_handle;
  if (!LogonUser(L"LocalService", L"NT AUTHORITY", nullptr,
                 LOGON32_LOGON_SERVICE, LOGON32_PROVIDER_DEFAULT,
                 &temp_handle)) {
    return false;
  }
  ScopedHandle token(temp_handle);

  // The DISABLE_MAX_PRIVILEGE flag removes all privileges except for
  // "SeChangeNotifyPrivilege" which is needed to access the machine certificate
  // (including its private key) in the "Local Machine" cert store. This is
  // needed for HTTPS client third-party authentication . But the presence of
  // "SeChangeNotifyPrivilege" also allows it to open and manipulate objects
  // owned by the same user. This risk is only mitigated by setting the
  // process integrity level to Low.
  if (!::CreateRestrictedToken(token.Get(), DISABLE_MAX_PRIVILEGE, 0, nullptr,
                               0, nullptr, 0, nullptr, &temp_handle)) {
    PLOG(ERROR) << "Failed to get the restricted token";
    return false;
  }

  ScopedHandle restricted_token(temp_handle);
  std::optional<Sid> sid = Sid::FromIntegrityLevel(SECURITY_MANDATORY_LOW_RID);
  if (!sid) {
    LOG(ERROR) << "Failed to get integrity level SID";
    return false;
  }

  TOKEN_MANDATORY_LABEL label = {};
  label.Label.Attributes = SE_GROUP_INTEGRITY;
  label.Label.Sid = sid->GetPSID();

  if (!SetTokenInformation(restricted_token.Get(), TokenIntegrityLevel, &label,
                           sizeof(label))) {
    PLOG(ERROR) << "Failed to set low integrity level";
    return false;
  }

  *token_out = std::move(restricted_token);
  return true;
}

// Creates a window station with a given name and the default desktop giving
// the complete access to |logon_sid|.
bool CreateWindowStationAndDesktop(ScopedSid logon_sid,
                                   WindowStationAndDesktop* handles_out) {
  // Convert the logon SID into a string.
  std::string logon_sid_string = ConvertSidToString(logon_sid.get());
  if (logon_sid_string.empty()) {
    PLOG(ERROR) << "Failed to convert a SID to string";
    return false;
  }

  // Format the security descriptors in SDDL form.
  std::string desktop_sddl =
      base::StringPrintf(kDesktopSdFormat, logon_sid_string.c_str()) +
      kLowIntegrityMandatoryLabel;
  std::string window_station_sddl =
      base::StringPrintf(kWindowStationSdFormat, logon_sid_string.c_str(),
                         logon_sid_string.c_str()) +
      kLowIntegrityMandatoryLabel;

  // Create the desktop and window station security descriptors.
  ScopedSd desktop_sd = ConvertSddlToSd(desktop_sddl);
  ScopedSd window_station_sd = ConvertSddlToSd(window_station_sddl);
  if (!desktop_sd || !window_station_sd) {
    PLOG(ERROR) << "Failed to create a security descriptor.";
    return false;
  }

  // GetProcessWindowStation() returns the current handle which does not need to
  // be freed.
  HWINSTA current_window_station = GetProcessWindowStation();

  // Generate a unique window station name.
  std::string window_station_name = base::StringPrintf(
      "chromoting-%" CrPRIdPid "-%d", base::GetCurrentProcId(),
      base::RandInt(1, std::numeric_limits<int>::max()));

  // Make sure that a new window station will be created instead of opening
  // an existing one.
  DWORD window_station_flags = CWF_CREATE_ONLY;

  // Request full access because this handle will be inherited by the worker
  // process which needs full access in order to attach to the window station.
  DWORD desired_access =
      WINSTA_ALL_ACCESS | DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER;

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = window_station_sd.get();
  security_attributes.bInheritHandle = TRUE;

  WindowStationAndDesktop handles;
  handles.SetWindowStation(CreateWindowStation(
      base::UTF8ToWide(window_station_name).c_str(), window_station_flags,
      desired_access, &security_attributes));
  if (!handles.window_station()) {
    PLOG(ERROR) << "CreateWindowStation() failed";
    return false;
  }

  // Switch to the new window station and create a desktop on it.
  if (!SetProcessWindowStation(handles.window_station())) {
    PLOG(ERROR) << "SetProcessWindowStation() failed";
    return false;
  }

  desired_access =
      DESKTOP_READOBJECTS | DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU |
      DESKTOP_HOOKCONTROL | DESKTOP_JOURNALRECORD | DESKTOP_JOURNALPLAYBACK |
      DESKTOP_ENUMERATE | DESKTOP_WRITEOBJECTS | DESKTOP_SWITCHDESKTOP |
      DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER;

  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = desktop_sd.get();
  security_attributes.bInheritHandle = TRUE;

  // The default desktop of the interactive window station is called "Default".
  // Name the created desktop the same way in case any code relies on that.
  // The desktop name should not make any difference though.
  handles.SetDesktop(CreateDesktop(L"Default", nullptr, nullptr, 0,
                                   desired_access, &security_attributes));

  // Switch back to the original window station.
  if (!SetProcessWindowStation(current_window_station)) {
    PLOG(ERROR) << "SetProcessWindowStation() failed";
    return false;
  }

  if (!handles.desktop()) {
    PLOG(ERROR) << "CreateDesktop() failed";
    return false;
  }

  handles.Swap(*handles_out);
  return true;
}

}  // namespace

UnprivilegedProcessDelegate::UnprivilegedProcessDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    std::unique_ptr<base::CommandLine> target_command)
    : io_task_runner_(io_task_runner),
      target_command_(std::move(target_command)),
      event_handler_(nullptr) {}

UnprivilegedProcessDelegate::~UnprivilegedProcessDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!channel_);
  DCHECK(!worker_process_.IsValid());
}

void UnprivilegedProcessDelegate::LaunchProcess(
    WorkerProcessLauncher* event_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!event_handler_);

  event_handler_ = event_handler;

  // Create a restricted token that will be used to run the worker process.
  ScopedHandle token;
  if (!CreateRestrictedToken(&token)) {
    PLOG(ERROR) << "Failed to create a restricted LocalService token";
    ReportFatalError();
    return;
  }

  // Determine our logon SID, so we can grant it access to our window station
  // and desktop.
  ScopedSid logon_sid = GetLogonSid(token.Get());
  if (!logon_sid) {
    PLOG(ERROR) << "Failed to retrieve the logon SID";
    ReportFatalError();
    return;
  }

  // Create the process and thread security descriptors.
  ScopedSd process_sd = ConvertSddlToSd(kWorkerProcessSd);
  ScopedSd thread_sd = ConvertSddlToSd(kWorkerThreadSd);
  if (!process_sd || !thread_sd) {
    PLOG(ERROR) << "Failed to create a security descriptor";
    ReportFatalError();
    return;
  }

  SECURITY_ATTRIBUTES process_attributes;
  process_attributes.nLength = sizeof(process_attributes);
  process_attributes.lpSecurityDescriptor = process_sd.get();
  process_attributes.bInheritHandle = FALSE;

  SECURITY_ATTRIBUTES thread_attributes;
  thread_attributes.nLength = sizeof(thread_attributes);
  thread_attributes.lpSecurityDescriptor = thread_sd.get();
  thread_attributes.bInheritHandle = FALSE;

  // Create our own window station and desktop accessible by |logon_sid|.
  WindowStationAndDesktop handles;
  if (!CreateWindowStationAndDesktop(std::move(logon_sid), &handles)) {
    PLOG(ERROR) << "Failed to create a window station and desktop";
    ReportFatalError();
    return;
  }

  mojo::OutgoingInvitation invitation;
  std::string message_pipe_token = base::NumberToString(base::RandUint64());
  std::unique_ptr<IPC::ChannelProxy> server = IPC::ChannelProxy::Create(
      invitation.AttachMessagePipe(message_pipe_token).release(),
      IPC::Channel::MODE_SERVER, this, io_task_runner_,
      base::SingleThreadTaskRunner::GetCurrentDefault());
  base::CommandLine command_line(target_command_->argv());
  command_line.AppendSwitchASCII(kMojoPipeToken, message_pipe_token);

  base::HandlesToInheritVector handles_to_inherit = {
      handles.desktop(),
      handles.window_station(),
  };

  // Create a handle for crash server pipe and provide it to the child process.
  // A named pipe will not exist if the user has not opted into crash reporting.
  ScopedHandle crash_server_pipe;
  if (IsUsageStatsAllowed()) {
    crash_server_pipe = GetClientHandleForCrashServerPipe();
    if (crash_server_pipe.get()) {
      // In order to pass the handle on the command line we need to convert it
      // to a string. The handle is a pointer and the conversion utilities don't
      // provide helpers for pointer -> string conversions so we cast to a
      // 64-bit value to make this conversion insensitive to the bitness of the
      // binary. Since the client and server will be the same bitness, the
      // child process will cast this value back to the correct bitness.
      command_line.AppendSwitchASCII(
          kCrashServerPipeHandle,
          base::NumberToString(
              reinterpret_cast<uint64_t>(crash_server_pipe.get())));
      handles_to_inherit.push_back(crash_server_pipe.get());
    }
  }

  mojo::PlatformChannel channel;
  channel.PrepareToPassRemoteEndpoint(&handles_to_inherit, &command_line);

  // Try to launch the worker process. The launched process inherits
  // the window station, desktop, and pipe handles, created above.
  ScopedHandle worker_process;
  ScopedHandle worker_thread;
  if (!LaunchProcessWithToken(
          command_line.GetProgram(), command_line.GetCommandLineString(),
          token.Get(), &process_attributes, &thread_attributes,
          handles_to_inherit, /* creation_flags= */ 0,
          /* thread_attributes= */ nullptr, &worker_process, &worker_thread)) {
    ReportFatalError();
    return;
  }

  mojo::OutgoingInvitation::Send(std::move(invitation), worker_process.Get(),
                                 channel.TakeLocalEndpoint());

  channel_ = std::move(server);

  ReportProcessLaunched(std::move(worker_process));
}

void UnprivilegedProcessDelegate::GetRemoteAssociatedInterface(
    mojo::GenericPendingAssociatedReceiver receiver) {
  channel_->GetRemoteAssociatedInterface(std::move(receiver));
}

void UnprivilegedProcessDelegate::CloseChannel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  worker_process_control_.reset();
  channel_.reset();
}

void UnprivilegedProcessDelegate::CrashProcess(const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (worker_process_control_) {
    worker_process_control_->CrashProcess(
        location.function_name(), location.file_name(), location.line_number());
  }
}

void UnprivilegedProcessDelegate::KillProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CloseChannel();
  event_handler_ = nullptr;

  if (worker_process_.IsValid()) {
    TerminateProcess(worker_process_.Get(), CONTROL_C_EXIT);
    worker_process_.Close();
  }
}

bool UnprivilegedProcessDelegate::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED() << "Received unexpected IPC type: " << message.type();
}

void UnprivilegedProcessDelegate::OnChannelConnected(int32_t peer_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DWORD pid = GetProcessId(worker_process_.Get());
  if (pid != static_cast<DWORD>(peer_pid)) {
    LOG(ERROR) << "The actual client PID " << pid
               << " does not match the one reported by the client: "
               << peer_pid;
    ReportFatalError();
    return;
  }

  channel_->GetRemoteAssociatedInterface(&worker_process_control_);

  event_handler_->OnChannelConnected(peer_pid);
}

void UnprivilegedProcessDelegate::OnChannelError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  event_handler_->OnChannelError();
}

void UnprivilegedProcessDelegate::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  event_handler_->OnAssociatedInterfaceRequest(interface_name,
                                               std::move(handle));
}

void UnprivilegedProcessDelegate::ReportFatalError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CloseChannel();

  WorkerProcessLauncher* event_handler = event_handler_;
  event_handler_ = nullptr;
  event_handler->OnFatalError();
}

void UnprivilegedProcessDelegate::ReportProcessLaunched(
    base::win::ScopedHandle worker_process) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!worker_process_.IsValid());

  worker_process_ = std::move(worker_process);

  // Report a handle that can be used to wait for the worker process completion,
  // query information about the process and duplicate handles.
  DWORD desired_access =
      SYNCHRONIZE | PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION;
  HANDLE temp_handle;
  if (!DuplicateHandle(GetCurrentProcess(), worker_process_.Get(),
                       GetCurrentProcess(), &temp_handle, desired_access, FALSE,
                       0)) {
    PLOG(ERROR) << "Failed to duplicate a handle";
    ReportFatalError();
    return;
  }
  ScopedHandle limited_handle(temp_handle);

  event_handler_->OnProcessLaunched(std::move(limited_handle));
}

}  // namespace remoting
