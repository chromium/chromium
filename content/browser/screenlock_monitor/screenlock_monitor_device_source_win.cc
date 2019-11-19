// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screenlock_monitor/screenlock_monitor_device_source.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "base/win/message_window.h"

namespace content {

ScreenlockMonitorDeviceSource::SessionMessageWindow::SessionMessageWindow() {
  // Create a window for receiving session change notifications.
  window_.reset(new base::win::MessageWindow());
  if (!window_->Create(base::BindRepeating(&SessionMessageWindow::OnWndProc,
                                           base::Unretained(this)))) {
    DLOG(ERROR) << "Failed to create the screenlock monitor window.";
    window_.reset();
    return;
  }

  base::OnceClosure wts_register =
      base::BindOnce(base::IgnoreResult(&::WTSRegisterSessionNotification),
                     window_->hwnd(), NOTIFY_FOR_ALL_SESSIONS);

  base::CreateCOMSTATaskRunner({base::ThreadPool()})
      ->PostTask(FROM_HERE, std::move(wts_register));
}

ScreenlockMonitorDeviceSource::SessionMessageWindow::~SessionMessageWindow() {
  // There should be no race condition between this code and the worker thread.
  // WTSUnRegisterSessionNotification is only called from destruction as we are
  // in shutdown, which means no other worker threads can be running.
  if (window_) {
    ::WTSUnRegisterSessionNotification(window_->hwnd());
    window_.reset();
  }
}

bool ScreenlockMonitorDeviceSource::SessionMessageWindow::OnWndProc(
    UINT message,
    WPARAM wparam,
    LPARAM lparam,
    LRESULT* result) {
  if (message == WM_WTSSESSION_CHANGE) {
    ProcessWTSSessionLockMessage(wparam);
  }
  return true;
}

void ScreenlockMonitorDeviceSource::SessionMessageWindow::
    ProcessWTSSessionLockMessage(WPARAM event_id) {
  ScreenlockEvent screenlock_event;
  switch (event_id) {
    case WTS_SESSION_LOCK:
      screenlock_event = SCREEN_LOCK_EVENT;
      break;
    case WTS_SESSION_UNLOCK:
      screenlock_event = SCREEN_UNLOCK_EVENT;
      break;
    default:
      return;
  }

  ProcessScreenlockEvent(screenlock_event);
}

}  // namespace content
