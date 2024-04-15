// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/launch_native_messaging_host_process.h"

#include <windows.h>

#include <shellapi.h>

#include <cstdint>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "ipc/ipc_channel.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/win/security_descriptor.h"

namespace {

// Windows will use default buffer size when 0 is passed to CreateNamedPipeW().
const uint32_t kBufferSize = 0;
const int kTimeOutMilliseconds = 2000;
const char kChromePipeNamePrefix[] = "\\\\.\\pipe\\chrome_remote_desktop.";

uint32_t CreateNamedPipe(const std::string& pipe_name,
                         const remoting::ScopedSd& security_descriptor,
                         uint32_t open_mode,
                         base::win::ScopedHandle* file_handle) {
  DCHECK(file_handle);

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = security_descriptor.get();
  security_attributes.bInheritHandle = FALSE;

  base::win::ScopedHandle temp_handle(::CreateNamedPipe(
      base::ASCIIToWide(pipe_name).c_str(), open_mode,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS, 1,
      kBufferSize, kBufferSize, kTimeOutMilliseconds, &security_attributes));

  if (!temp_handle.IsValid()) {
    uint32_t error = GetLastError();
    PLOG(ERROR) << "Failed to create named pipe '" << pipe_name << "'";
    return error;
  }

  file_handle->Set(temp_handle.Take());
  return 0;
}

}  // namespace

namespace remoting {

ProcessLaunchResult LaunchNativeMessagingHostProcess(
    const base::FilePath& binary_path,
    intptr_t parent_window_handle,
    bool elevate_process,
    base::win::ScopedHandle* read_handle,
    base::win::ScopedHandle* write_handle) {
  DCHECK(read_handle);
  DCHECK(write_handle);

  if (!base::PathExists(binary_path)) {
    LOG(ERROR) << "Cannot find binary: " << binary_path.value();
    return PROCESS_LAUNCH_RESULT_FAILED;
  }

  // presubmit: allow wstring
  std::wstring user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    LOG(ERROR) << "Failed to query the current user SID.";
    return PROCESS_LAUNCH_RESULT_FAILED;
  }

  // Create a security descriptor that gives full access to the caller and
  // BUILTIN_ADMINISTRATORS and denies access by anyone else.
  // Local admins need access because the privileged host process will run
  // as a local admin which may not be the same user as the current user.
  std::string security_descriptor =
      base::StringPrintf("O:%lsG:%lsD:(A;;GA;;;%ls)(A;;GA;;;BA)",
                         user_sid.c_str(), user_sid.c_str(), user_sid.c_str());

  ScopedSd sd = ConvertSddlToSd(security_descriptor);
  if (!sd) {
    PLOG(ERROR) << "Failed to create a security descriptor.";
    return PROCESS_LAUNCH_RESULT_FAILED;
  }

  std::string input_pipe_name(kChromePipeNamePrefix);
  input_pipe_name.append(IPC::Channel::GenerateUniqueRandomChannelID());
  base::win::ScopedHandle temp_write_handle;
  CreateNamedPipe(input_pipe_name, sd, PIPE_ACCESS_OUTBOUND,
                  &temp_write_handle);
  if (!temp_write_handle.IsValid()) {
    return PROCESS_LAUNCH_RESULT_FAILED;
  }

  std::string output_pipe_name(kChromePipeNamePrefix);
  output_pipe_name.append(IPC::Channel::GenerateUniqueRandomChannelID());
  base::win::ScopedHandle temp_read_handle;
  CreateNamedPipe(output_pipe_name, sd, PIPE_ACCESS_INBOUND, &temp_read_handle);
  if (!temp_read_handle.IsValid()) {
    return PROCESS_LAUNCH_RESULT_FAILED;
  }

  const base::CommandLine* current_command_line =
      base::CommandLine::ForCurrentProcess();

  // Create the child process command line by copying switches from the current
  // command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  DCHECK(!current_command_line->HasSwitch(kElevateSwitchName));
  // Always add the elevation switch when launched via this function.
  command_line.AppendSwitch(kElevateSwitchName);
  command_line.AppendSwitchASCII(kInputSwitchName, input_pipe_name);
  command_line.AppendSwitchASCII(kOutputSwitchName, output_pipe_name);

  for (const auto& switch_data : current_command_line->GetSwitches()) {
    command_line.AppendSwitchNative(switch_data.first, switch_data.second);
  }
  for (const auto& arg : current_command_line->GetArgs()) {
    command_line.AppendArgNative(arg);
  }

  // Get the parameters for the binary to launch.
  base::CommandLine::StringType params = command_line.GetCommandLineString();

  // Launch the child process, requesting elevation if needed.
  SHELLEXECUTEINFO info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  info.hwnd = reinterpret_cast<HWND>(parent_window_handle);
  info.lpFile = binary_path.value().c_str();
  info.lpParameters = params.c_str();
  info.nShow = SW_HIDE;

  if (elevate_process) {
    info.lpVerb = L"runas";
  }

  if (!ShellExecuteEx(&info)) {
    uint32_t error = GetLastError();
    PLOG(ERROR) << "Unable to launch '" << binary_path.value() << "'";
    if (error == ERROR_CANCELLED) {
      return PROCESS_LAUNCH_RESULT_CANCELLED;
    } else {
      return PROCESS_LAUNCH_RESULT_FAILED;
    }
  }

  if (!ConnectNamedPipe(temp_write_handle.Get(), nullptr)) {
    uint32_t error = GetLastError();
    if (error != ERROR_PIPE_CONNECTED) {
      PLOG(ERROR) << "Unable to connect '" << output_pipe_name << "'";
      return PROCESS_LAUNCH_RESULT_FAILED;
    }
  }

  if (!ConnectNamedPipe(temp_read_handle.Get(), nullptr)) {
    uint32_t error = GetLastError();
    if (error != ERROR_PIPE_CONNECTED) {
      PLOG(ERROR) << "Unable to connect '" << input_pipe_name << "'";
      return PROCESS_LAUNCH_RESULT_FAILED;
    }
  }

  read_handle->Set(temp_read_handle.Take());
  write_handle->Set(temp_write_handle.Take());
  return PROCESS_LAUNCH_RESULT_SUCCESS;
}

}  // namespace remoting
