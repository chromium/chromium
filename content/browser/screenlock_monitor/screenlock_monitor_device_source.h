// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_DEVICE_SOURCE_H_
#define CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_DEVICE_SOURCE_H_

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/screenlock_monitor/screenlock_monitor_source.h"
#include "content/common/content_export.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <wtsapi32.h>
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include <optional>

#include "components/session_manager/core/session_manager_observer.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include <optional>

#include "chromeos/crosapi/mojom/login_state.mojom.h"  // nogncheck
#include "mojo/public/cpp/bindings/receiver.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN)
namespace gfx {
class SingletonHwndObserver;
}  // namespace gfx
#endif  // BUILDFLAG(IS_WIN)

namespace content {

// A class used to monitor the screenlock state change on each supported
// platform and notify the change event to monitor.
class CONTENT_EXPORT ScreenlockMonitorDeviceSource
    : public ScreenlockMonitorSource {
 public:
  ScreenlockMonitorDeviceSource();

  ScreenlockMonitorDeviceSource(const ScreenlockMonitorDeviceSource&) = delete;
  ScreenlockMonitorDeviceSource& operator=(
      const ScreenlockMonitorDeviceSource&) = delete;

  ~ScreenlockMonitorDeviceSource() override;

#if BUILDFLAG(IS_WIN)
  // Fake session notification registration/unregistration APIs allow us to test
  // receiving and handling messages that look as if they are sent by other
  // sessions, without having to create a session host and a second session.
  using WTSRegisterSessionNotificationFunction = bool (*)(HWND hwnd,
                                                          DWORD flags);
  using WTSUnRegisterSessionNotificationFunction = bool (*)(HWND hwnd);
  static void SetFakeNotificationAPIsForTesting(
      WTSRegisterSessionNotificationFunction register_function,
      WTSUnRegisterSessionNotificationFunction unregister_function);
#endif  // BUILDFLAG(IS_WIN)

 private:
#if BUILDFLAG(IS_WIN)
  // Represents a singleton hwnd for screenlock message handling on Win.
  // Only allow ScreenlockMonitor to create it.
  class SessionMessageWindow {
   public:
    SessionMessageWindow();

    SessionMessageWindow(const SessionMessageWindow&) = delete;
    SessionMessageWindow& operator=(const SessionMessageWindow&) = delete;

    ~SessionMessageWindow();

    static void SetFakeNotificationAPIsForTesting(
        WTSRegisterSessionNotificationFunction register_function,
        WTSUnRegisterSessionNotificationFunction unregister_function);

   private:
    void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    void ProcessWTSSessionLockMessage(WPARAM event_id);

    static WTSRegisterSessionNotificationFunction
        register_session_notification_function_;
    static WTSUnRegisterSessionNotificationFunction
        unregister_session_notification_function_;
    std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;
  };

  SessionMessageWindow session_message_window_;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
  void StartListeningForScreenlock();
  void StopListeningForScreenlock();
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  class ScreenLockListener : public session_manager::SessionManagerObserver {
   public:
    ScreenLockListener();

    ScreenLockListener(const ScreenLockListener&) = delete;
    ScreenLockListener& operator=(const ScreenLockListener&) = delete;

    ~ScreenLockListener() override;

    // session_manager::SessionManagerObserver:
    void OnSessionStateChanged() override;

   private:
    std::optional<ScreenlockEvent> prev_event_;
  };

  ScreenLockListener screenlock_listener_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  class ScreenLockListener
      : public crosapi::mojom::SessionStateChangedEventObserver {
   public:
    ScreenLockListener();

    ScreenLockListener(const ScreenLockListener&) = delete;
    ScreenLockListener& operator=(const ScreenLockListener&) = delete;

    ~ScreenLockListener() override;

    // crosapi::mojom::SessionStateChangedEventObserver:
    void OnSessionStateChanged(crosapi::mojom::SessionState state) override;

   private:
    std::optional<ScreenlockEvent> prev_event_;
    mojo::Receiver<crosapi::mojom::SessionStateChangedEventObserver> receiver_;
  };

  ScreenLockListener screenlock_listener_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_DEVICE_SOURCE_H_
