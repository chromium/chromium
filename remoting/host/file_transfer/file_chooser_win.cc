// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_chooser_win.h"

#include <windows.h>

#include <wtsapi32.h>

#include <utility>
#include <variant>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/scoped_handle.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/base/switches.h"

namespace remoting {

namespace {

using base::win::ScopedHandle;
using protocol::FileTransferResult;
using protocol::MakeFileTransferError;

FileTransferResult<ScopedHandle> GetCurrentUserToken(base::Location from_here) {
  HANDLE user_token = nullptr;
  if (!WTSQueryUserToken(WTS_CURRENT_SESSION, &user_token)) {
    PLOG(ERROR) << "Failed to get current user token";
    return MakeFileTransferError(
        from_here,
        GetLastError() == ERROR_NO_TOKEN
            ? protocol::FileTransfer_Error_Type_NOT_LOGGED_IN
            : protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR,
        GetLastError());
  }
  return ScopedHandle(user_token);
}

FileTransferResult<base::FilePath> GetExePath(base::Location from_here) {
  // The remoting_desktop.exe binary (where this code runs) has extra manifest
  // flags (uiAccess and requireAdministrator) that are undesirable for the
  // file-chooser child process, so remoting_host.exe is used instead.
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_EXE, &path)) {
    LOG(ERROR) << "Failed to get executable path.";
    return MakeFileTransferError(
        from_here, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
  }
  return path.AppendASCII("remoting_host.exe");
}

}  // namespace

FileChooserWindows::LaunchResult::LaunchResult() = default;

FileChooserWindows::LaunchResult::LaunchResult(LaunchResult&&) = default;

FileChooserWindows::LaunchResult& FileChooserWindows::LaunchResult::operator=(
    LaunchResult&&) = default;

FileChooserWindows::LaunchResult::~LaunchResult() {
  if (process.IsValid()) {
    process.Terminate(0, false);
  }
}

FileChooserWindows::FileChooserWindows(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback)
    : callback_(std::move(callback)) {}

FileChooserWindows::~FileChooserWindows() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (process_.IsValid()) {
    process_.Terminate(0, false);
  }
}

void FileChooserWindows::SetLauncherForTesting(LaunchProcessCallback launcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  launcher_for_testing_ = std::move(launcher);
}

void FileChooserWindows::Show() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (launcher_for_testing_) {
    OnProcessLaunched(launcher_for_testing_.Run());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&FileChooserWindows::LaunchChooserProcess),
      base::BindOnce(&FileChooserWindows::OnProcessLaunched,
                     weak_factory_.GetWeakPtr()));
}

void FileChooserWindows::OnProcessLaunched(LaunchResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.status) {
    if (callback_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_),
                                    std::move(result.status.error())));
    }
    return;
  }

  process_ = std::move(result.process);
  file_chooser_.Bind(std::move(result.pending_remote));
  file_chooser_.set_disconnect_handler(base::BindOnce(
      &FileChooserWindows::OnDisconnected, base::Unretained(this)));

  file_chooser_->OpenFile(base::BindOnce(
      &FileChooserWindows::OnFileChooserResult, base::Unretained(this)));
}

void FileChooserWindows::OnFileChooserResult(
    const FileChooser::Result& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "File chooser result received, success=" << result.is_success();
  file_chooser_.reset();
  process_.Close();
  if (callback_) {
    std::move(callback_).Run(result);
  }
}

void FileChooserWindows::OnDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(WARNING) << "File chooser Mojo channel disconnected.";
  file_chooser_.reset();
  process_.Close();
  if (callback_) {
    std::move(callback_).Run(MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
  }
}

// static
FileChooserWindows::LaunchResult FileChooserWindows::LaunchChooserProcess() {
  LaunchResult result;
  base::LaunchOptions launch_options;

  FileTransferResult<ScopedHandle> current_user =
      GetCurrentUserToken(FROM_HERE);
  if (!current_user) {
    LOG(ERROR) << "Failed to query current user token.";
    result.status = current_user.error();
    return result;
  }
  launch_options.as_user = current_user->Get();
  launch_options.force_breakaway_from_job_ = true;
  launch_options.grant_foreground_privilege = true;

  FileTransferResult<base::FilePath> exe_path = GetExePath(FROM_HERE);
  if (!exe_path) {
    LOG(ERROR) << "Failed to get file chooser executable path.";
    result.status = exe_path.error();
    return result;
  }
  base::CommandLine command_line(*exe_path);
  command_line.AppendSwitchASCII(kProcessTypeSwitchName,
                                 kProcessTypeFileChooser);

  mojo::PlatformChannel channel;
  channel.PrepareToPassRemoteEndpoint(&launch_options.handles_to_inherit,
                                      &command_line);

  mojo::OutgoingInvitation invitation;
  invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_SHARE_BROKER);
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(0);

  base::Process process = base::LaunchProcess(command_line, launch_options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch file chooser process.";
    result.status = MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
    return result;
  }
  LOG(INFO) << "Launched file chooser process with PID " << process.Pid();

  channel.RemoteProcessLaunchAttempted();
  mojo::OutgoingInvitation::Send(std::move(invitation), process.Handle(),
                                 channel.TakeLocalEndpoint());

  result.process = std::move(process);
  result.pending_remote =
      mojo::PendingRemote<mojom::FileChooser>(std::move(pipe), 0);
  return result;
}

}  // namespace remoting
