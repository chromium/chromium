// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_DEVICE_SOURCE_H_
#define CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_DEVICE_SOURCE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/screenlock_monitor/screenlock_monitor_source.h"
#include "content/common/content_export.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wtsapi32.h>
#endif  // OS_WIN

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/session_manager/core/session_manager_observer.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
namespace base {
namespace win {
class MessageWindow;
}
}  // namespace base
#endif  // OS_WIN

namespace content {

// A class used to monitor the screenlock state change on each supported
// platform and notify the change event to monitor.
class CONTENT_EXPORT ScreenlockMonitorDeviceSource
    : public ScreenlockMonitorSource {
 public:
  ScreenlockMonitorDeviceSource();
  ~ScreenlockMonitorDeviceSource() override;

#if defined(OS_WIN)
  // Fake session notification registration/unregistration APIs allow us to test
  // receiving and handling messages that look as if they are sent by other
  // sessions, without having to create a session host and a second session.
  using WTSRegisterSessionNotificationFunction = bool (*)(HWND hwnd,
                                                          DWORD flags);
  using WTSUnRegisterSessionNotificationFunction = bool (*)(HWND hwnd);
  static void SetFakeNotificationAPIsForTesting(
      WTSRegisterSessionNotificationFunction register_function,
      WTSUnRegisterSessionNotificationFunction unregister_function);
#endif  // defined(OS_WIN)

 private:
#if defined(OS_WIN)
  // Represents a message-only window for screenlock message handling on Win.
  // Only allow ScreenlockMonitor to create it.
  class SessionMessageWindow {
   public:
    SessionMessageWindow();
    ~SessionMessageWindow();

    static void SetFakeNotificationAPIsForTesting(
        WTSRegisterSessionNotificationFunction register_function,
        WTSUnRegisterSessionNotificationFunction unregister_function);

   private:
    bool OnWndProc(UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result);
    void ProcessWTSSessionLockMessage(WPARAM event_id);

    static WTSRegisterSessionNotificationFunction
        register_session_notification_function_;
    static WTSUnRegisterSessionNotificationFunction
        unregister_session_notification_function_;
    std::unique_ptr<base::win::MessageWindow> window_;

    DISALLOW_COPY_AND_ASSIGN(SessionMessageWindow);
  };

  SessionMessageWindow session_message_window_;
#endif  // OS_WIN

#if defined(OS_MAC)
  void StartListeningForScreenlock();
  void StopListeningForScreenlock();
#endif  // OS_MAC

#if BUILDFLAG(IS_CHROMEOS_ASH)
  class ScreenLockListener : public session_manager::SessionManagerObserver {
   public:
    ScreenLockListener();
    ~ScreenLockListener() override;

    // session_manager::SessionManagerObserver:
    void OnSessionStateChanged() override;

   private:
    DISALLOW_COPY_AND_ASSIGN(ScreenLockListener);
  };

  ScreenLockListener screenlock_listener_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  DISALLOW_COPY_AND_ASSIGN(ScreenlockMonitorDeviceSource);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_DEVICE_SOURCE_H_
