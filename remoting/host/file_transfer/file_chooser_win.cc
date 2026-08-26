// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_chooser.h"

#include <windows.h>

#include <wtsapi32.h>

#include <utility>
#include <variant>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/scoped_handle.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/mojom/desktop_session.mojom.h"

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

class FileChooserWindows : public FileChooser {
 public:
  FileChooserWindows(scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                     ResultCallback callback);

  FileChooserWindows(const FileChooserWindows&) = delete;
  FileChooserWindows& operator=(const FileChooserWindows&) = delete;

  ~FileChooserWindows() override;

  // FileChooser implementation.
  void Show() override;

 private:
  FileTransferResult<std::monostate> LaunchChooserProcess();
  void OnFileChooserResult(const FileChooser::Result& result);
  void OnDisconnected();

  ResultCallback callback_;
  base::Process process_;
  mojo::Remote<mojom::FileChooser> file_chooser_;
};

FileChooserWindows::FileChooserWindows(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback)
    : callback_(std::move(callback)) {}

FileChooserWindows::~FileChooserWindows() {
  if (process_.IsValid()) {
    process_.Terminate(0, false);
  }
}

void FileChooserWindows::Show() {
  FileTransferResult<std::monostate> result = LaunchChooserProcess();

  if (!result) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), std::move(result.error())));
  }
}

void FileChooserWindows::OnFileChooserResult(
    const FileChooser::Result& result) {
  file_chooser_.reset();
  process_.Close();
  if (callback_) {
    std::move(callback_).Run(result);
  }
}

void FileChooserWindows::OnDisconnected() {
  file_chooser_.reset();
  process_.Close();
  if (callback_) {
    std::move(callback_).Run(MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
  }
}

FileTransferResult<std::monostate> FileChooserWindows::LaunchChooserProcess() {
  base::LaunchOptions launch_options;

  FileTransferResult<ScopedHandle> current_user =
      GetCurrentUserToken(FROM_HERE);
  if (!current_user) {
    return current_user.error();
  }
  launch_options.as_user = current_user->Get();

  FileTransferResult<base::FilePath> exe_path = GetExePath(FROM_HERE);
  if (!exe_path) {
    return exe_path.error();
  }
  base::CommandLine command_line(*exe_path);
  command_line.AppendSwitchASCII(kProcessTypeSwitchName,
                                 kProcessTypeFileChooser);

  mojo::PlatformChannel channel;
  channel.PrepareToPassRemoteEndpoint(&launch_options.handles_to_inherit,
                                      &command_line);

  mojo::OutgoingInvitation invitation;
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(0);
  file_chooser_.Bind(
      mojo::PendingRemote<mojom::FileChooser>(std::move(pipe), 0));
  file_chooser_.set_disconnect_handler(base::BindOnce(
      &FileChooserWindows::OnDisconnected, base::Unretained(this)));

  process_ = base::LaunchProcess(command_line, launch_options);
  if (!process_.IsValid()) {
    LOG(ERROR) << "Failed to launch process.";
    file_chooser_.reset();
    return MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
  }

  channel.RemoteProcessLaunchAttempted();
  mojo::OutgoingInvitation::Send(std::move(invitation), process_.Handle(),
                                 channel.TakeLocalEndpoint());

  file_chooser_->OpenFile(base::BindOnce(
      &FileChooserWindows::OnFileChooserResult, base::Unretained(this)));

  return kSuccessTag;
}

}  // namespace

std::unique_ptr<FileChooser> FileChooser::Create(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback) {
  return std::make_unique<FileChooserWindows>(std::move(ui_task_runner),
                                              std::move(callback));
}

}  // namespace remoting
