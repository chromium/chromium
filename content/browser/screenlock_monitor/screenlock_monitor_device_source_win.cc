// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screenlock_monitor/screenlock_monitor_device_source.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/win/message_window.h"

namespace content {
namespace {
bool RegisterForSessionNotifications(HWND hwnd, DWORD flag) {
  base::OnceClosure wts_register = base::BindOnce(
      base::IgnoreResult(&::WTSRegisterSessionNotification), hwnd, flag);

  base::ThreadPool::CreateCOMSTATaskRunner({})->PostTask(
      FROM_HERE, std::move(wts_register));

  return true;
}

bool UnregisterFromSessionNotifications(HWND hwnd) {
  return ::WTSUnRegisterSessionNotification(hwnd);
}
}  // namespace

// Set static member function pointers with default (non-fake) APIs.
ScreenlockMonitorDeviceSource::WTSRegisterSessionNotificationFunction
    ScreenlockMonitorDeviceSource::SessionMessageWindow::
        register_session_notification_function_ =
            &RegisterForSessionNotifications;

ScreenlockMonitorDeviceSource::WTSUnRegisterSessionNotificationFunction
    ScreenlockMonitorDeviceSource::SessionMessageWindow::
        unregister_session_notification_function_ =
            &UnregisterFromSessionNotifications;

// static
void ScreenlockMonitorDeviceSource::SetFakeNotificationAPIsForTesting(
    WTSRegisterSessionNotificationFunction register_function,
    WTSUnRegisterSessionNotificationFunction unregister_function) {
  SessionMessageWindow::SetFakeNotificationAPIsForTesting(register_function,
                                                          unregister_function);
}

// static
void ScreenlockMonitorDeviceSource::SessionMessageWindow::
    SetFakeNotificationAPIsForTesting(
        WTSRegisterSessionNotificationFunction register_function,
        WTSUnRegisterSessionNotificationFunction unregister_function) {
  register_session_notification_function_ = register_function;
  unregister_session_notification_function_ = unregister_function;
}

ScreenlockMonitorDeviceSource::SessionMessageWindow::SessionMessageWindow() {
  // Create a window for receiving session change notifications.
  window_ = std::make_unique<base::win::MessageWindow>();
  if (!window_->Create(base::BindRepeating(&SessionMessageWindow::OnWndProc,
                                           base::Unretained(this)))) {
    DLOG(ERROR) << "Failed to create the screenlock monitor window.";
    window_.reset();
    return;
  }

  // Use NOTIFY_FOR_THIS_SESSION so we only receive events from the current
  // session, and not from other users connected to the same session host.
  bool registered = register_session_notification_function_(
      window_->hwnd(), NOTIFY_FOR_THIS_SESSION);
  DCHECK(registered);
}

ScreenlockMonitorDeviceSource::SessionMessageWindow::~SessionMessageWindow() {
  // There should be no race condition between this code and the worker thread.
  // |unregister_session_notification_function_| is only called from destruction
  // as we are in shutdown, which means no other worker threads can be running.
  if (window_) {
    bool unregistered =
        unregister_session_notification_function_(window_->hwnd());
    DCHECK(unregistered);
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
  return false;
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
