// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_DEVICE_SOURCE_H_
#define CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_DEVICE_SOURCE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/screenlock_monitor/screenlock_monitor_source.h"
#include "content/common/content_export.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wtsapi32.h>
#endif  // OS_WIN

#if defined(OS_CHROMEOS)
#include "components/session_manager/core/session_manager_observer.h"
#endif  // OS_CHROMEOS

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

 private:
#if defined(OS_WIN)
  // Represents a message-only window for screenlock message handling on Win.
  // Only allow ScreenlockMonitor to create it.
  class SessionMessageWindow {
   public:
    SessionMessageWindow();
    ~SessionMessageWindow();

   private:
    bool OnWndProc(UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result);
    void ProcessWTSSessionLockMessage(WPARAM event_id);

    std::unique_ptr<base::win::MessageWindow> window_;

    DISALLOW_COPY_AND_ASSIGN(SessionMessageWindow);
  };

  SessionMessageWindow session_message_window_;
#endif  // OS_WIN

#if defined(OS_MACOSX)
  void StartListeningForScreenlock();
  void StopListeningForScreenlock();
#endif  // OS_MACOSX

#if defined(OS_CHROMEOS)
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
#endif  // OS_CHROMEOS

  DISALLOW_COPY_AND_ASSIGN(ScreenlockMonitorDeviceSource);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_DEVICE_SOURCE_H_
