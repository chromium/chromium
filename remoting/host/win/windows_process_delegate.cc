// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/windows_process_delegate.h"

#include <Windows.h>

#include "base/logging.h"
#include "remoting/host/base/host_exit_codes.h"

namespace remoting {

WindowsProcessDelegate::WindowsProcessDelegate() = default;
WindowsProcessDelegate::~WindowsProcessDelegate() = default;

void WindowsProcessDelegate::WatchProcess(
    base::win::ScopedHandle worker_process) {
  DCHECK(event_handler_);
  DCHECK(!process_watcher_.GetWatchedObject());
  DCHECK(!worker_process_.is_valid());

  if (!process_watcher_.StartWatchingOnce(worker_process.Get(), this)) {
    LOG(ERROR) << "Failed to watch worker process";
    event_handler_->OnFatalError();
    return;
  }

  worker_process_ = std::move(worker_process);
}

void WindowsProcessDelegate::OnObjectSignaled(HANDLE object) {
  DCHECK(worker_process_.Get() == object);
  DCHECK(event_handler_);

  DWORD exit_code = CONTROL_C_EXIT;
  if (!::GetExitCodeProcess(worker_process_.Get(), &exit_code)) {
    PLOG(INFO) << "Failed to query the exit code of the worker process";
    exit_code = CONTROL_C_EXIT;
  }

  worker_process_.Close();
  process_watcher_.StopWatching();

  event_handler_->OnProcessExited(exit_code);
}

}  // namespace remoting
