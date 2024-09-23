// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_chooser.h"

#include <windows.h>

#include <wtsapi32.h>

#include <cstdlib>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "mojo/public/cpp/bindings/message.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/file_transfer/file_chooser_common_win.h"
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

FileTransferResult<std::pair<ScopedHandle, ScopedHandle>> MakePipe(
    base::Location from_here) {
  HANDLE pipe_read = nullptr;
  HANDLE pipe_write = nullptr;

  // Create the pipe for the child process's STDOUT.
  // LaunchProcess will take care of making the write end inheritable.
  if (!CreatePipe(&pipe_read, &pipe_write, nullptr,
                  kFileChooserPipeBufferSize)) {
    PLOG(ERROR) << "Failed to create pipe";
    return MakeFileTransferError(
        from_here, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR,
        GetLastError());
  }

  ScopedHandle scoped_pipe_read(pipe_read);
  ScopedHandle scoped_pipe_write(pipe_write);

  // If the pipe buffer ends up smaller than expected, we want to fail rather
  // hang, so set the pipe to be non-blocking. (Note that despite the name,
  // SetNamedPipeHandleState is documented to be usable with anonymous pipes as
  // well.
  DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
  if (!SetNamedPipeHandleState(pipe_write, &mode, nullptr, nullptr)) {
    PLOG(ERROR) << "Failed to set pipe to non-blocking mode";
    return MakeFileTransferError(
        from_here, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR,
        GetLastError());
  }

  return std::make_pair(std::move(scoped_pipe_read),
                        std::move(scoped_pipe_write));
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

class FileChooserWindows : public FileChooser,
                           public base::win::ObjectWatcher::Delegate {
 public:
  FileChooserWindows(scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                     ResultCallback callback);

  FileChooserWindows(const FileChooserWindows&) = delete;
  FileChooserWindows& operator=(const FileChooserWindows&) = delete;

  ~FileChooserWindows() override;

  // FileChooser implementation.
  void Show() override;

  // ObjectWatcher::Delegate implementation.
  void OnObjectSignaled(HANDLE object) override;

 private:
  FileTransferResult<absl::monostate> LaunchChooserProcess();

  ResultCallback callback_;
  base::Process process_;
  base::win::ObjectWatcher object_watcher_;
  ScopedHandle pipe_read_;
};

FileChooserWindows::FileChooserWindows(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback)
    : callback_(std::move(callback)) {}

void FileChooserWindows::Show() {
  FileTransferResult<absl::monostate> result = LaunchChooserProcess();

  if (!result) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), std::move(result.error())));
  }
}

void FileChooserWindows::OnObjectSignaled(HANDLE object) {
  int exit_code;
  // Shouldn't block since process handle is already signaled.
  if (!process_.WaitForExit(&exit_code)) {
    // Currently, WaitForExit returns immediately if GetExitCodeProcess fails,
    // so GetLastError should still be relevant.
    PLOG(ERROR) << "Failed to check exit status";
    process_.Close();
    std::move(callback_).Run(MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }
  if (exit_code != ERROR_SUCCESS) {
    LOG(ERROR) << "Error running dialog process:" << exit_code;
    process_.Close();
    std::move(callback_).Run(MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }
  process_.Close();

  std::vector<uint8_t> response_bytes(kFileChooserPipeBufferSize);
  DWORD bytes_read;
  if (!PeekNamedPipe(pipe_read_.Get(), response_bytes.data(),
                     response_bytes.size(), &bytes_read, nullptr, nullptr)) {
    PLOG(ERROR) << "Failed to read response from pipe";
    std::move(callback_).Run(MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR,
        GetLastError()));
    return;
  }

  mojo::Message serialized_message(
      base::span<uint8_t>(response_bytes.begin(), bytes_read),
      base::span<mojo::ScopedHandle>());

  FileChooser::Result result;
  if (!mojom::FileChooserResult::DeserializeFromMessage(
          std::move(serialized_message), &result)) {
    LOG(ERROR) << "Failed to deserialize response.";
    std::move(callback_).Run(MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }

  std::move(callback_).Run(std::move(result));
}

FileTransferResult<absl::monostate> FileChooserWindows::LaunchChooserProcess() {
  base::LaunchOptions launch_options;

  FileTransferResult<ScopedHandle> current_user =
      GetCurrentUserToken(FROM_HERE);
  if (!current_user) {
    return current_user.error();
  }
  launch_options.as_user = current_user->Get();

  FileTransferResult<std::pair<ScopedHandle, ScopedHandle>> pipe_pair =
      MakePipe(FROM_HERE);
  if (!pipe_pair) {
    return pipe_pair.error();
  }
  pipe_read_ = std::move(pipe_pair->first);
  launch_options.handles_to_inherit.push_back(pipe_pair->second.Get());
  launch_options.stdin_handle = INVALID_HANDLE_VALUE;
  launch_options.stdout_handle = pipe_pair->second.Get();
  launch_options.stderr_handle = INVALID_HANDLE_VALUE;

  FileTransferResult<base::FilePath> exe_path = GetExePath(FROM_HERE);
  if (!exe_path) {
    return exe_path.error();
  }
  base::CommandLine command_line(*exe_path);
  command_line.AppendSwitchASCII(kProcessTypeSwitchName,
                                 kProcessTypeFileChooser);

  process_ = base::LaunchProcess(command_line, launch_options);
  if (!process_.IsValid()) {
    LOG(ERROR) << "Failed to launch process.";
    return MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
  }

  if (!object_watcher_.StartWatchingOnce(process_.Handle(), this)) {
    LOG(ERROR) << "Failed to wait for file chooser";
    process_.Terminate(0, false);
    process_.Close();
    return MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
  }

  return kSuccessTag;
}

FileChooserWindows::~FileChooserWindows() {
  if (process_.IsValid()) {
    process_.Terminate(0, false);
  }
}

}  // namespace

std::unique_ptr<FileChooser> FileChooser::Create(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback) {
  return std::make_unique<FileChooserWindows>(std::move(ui_task_runner),
                                              std::move(callback));
}

}  // namespace remoting
