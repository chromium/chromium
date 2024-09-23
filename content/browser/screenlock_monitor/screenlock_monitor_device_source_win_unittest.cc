// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screenlock_monitor/screenlock_monitor_device_source.h"

#include "base/test/task_environment.h"
#include "content/browser/screenlock_monitor/screenlock_monitor.h"
#include "content/browser/screenlock_monitor/screenlock_monitor_source.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// TODO(crbug.com/40164163): These global state variables will likely lead to
// issues if multiple tests are run in parallel. Use caution if adding more
// tests to this file until crbug.com/1166275 is resolved.
static DWORD g_flag;
static HWND g_hwnd;

// These functions mock the Windows OS component that notifies subscribers
// when a session changes state (i.e. when it locks or unlocks).
// This is the mock for the WTSRegisterSessionNotification API.
bool FakeRegister(HWND hwnd, DWORD flag) {
  // Ensure only valid flags for WTSRegisterSessionNotification are used.
  if (flag != NOTIFY_FOR_ALL_SESSIONS && flag != NOTIFY_FOR_THIS_SESSION)
    return false;

  g_flag = flag;
  g_hwnd = hwnd;
  return true;
}

// Mocks the WTSUnRegisterSessionNotification API.
bool FakeUnregister(HWND hwnd) {
  if (hwnd != g_hwnd)
    return false;

  return true;
}

// If the recipient registered with the |NOTIFY_FOR_THIS_SESSION| flag, then
// we should only send events from this session.
bool ShouldSendEvent(DWORD session_id) {
  if (g_flag == NOTIFY_FOR_ALL_SESSIONS)
    return true;

  DWORD current_session_id;
  EXPECT_TRUE(ProcessIdToSessionId(GetCurrentProcessId(), &current_session_id));
  return session_id == current_session_id;
}

void GenerateFakeLockEvent(DWORD session_id) {
  if (!ShouldSendEvent(session_id))
    return;

  SendMessage(g_hwnd, WM_WTSSESSION_CHANGE, WTS_SESSION_LOCK, session_id);
}

void GenerateFakeUnlockEvent(DWORD session_id) {
  if (!ShouldSendEvent(session_id))
    return;

  SendMessage(g_hwnd, WM_WTSSESSION_CHANGE, WTS_SESSION_UNLOCK, session_id);
}
}  // namespace

class ScreenlockMonitorTestObserver : public ScreenlockObserver {
 public:
  ScreenlockMonitorTestObserver() : is_screen_locked_(false) {}
  ~ScreenlockMonitorTestObserver() override = default;

  // ScreenlockObserver callbacks.
  void OnScreenLocked() override { is_screen_locked_ = true; }
  void OnScreenUnlocked() override { is_screen_locked_ = false; }

  bool IsScreenLocked() { return is_screen_locked_; }

 private:
  bool is_screen_locked_;
};

TEST(ScreenlockMonitorDeviceSourceWinTest, FakeSessionNotifications) {
  content::BrowserTaskEnvironment task_environment;
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ScreenlockMonitorDeviceSource::SetFakeNotificationAPIsForTesting(
      &FakeRegister, &FakeUnregister);
  ScreenlockMonitorDeviceSource* screenlock_monitor_source =
      new ScreenlockMonitorDeviceSource();
  auto screenlock_monitor = std::make_unique<ScreenlockMonitor>(
      std::unique_ptr<ScreenlockMonitorSource>(screenlock_monitor_source));
  ScreenlockMonitorTestObserver observer;
  screenlock_monitor->AddObserver(&observer);

  DWORD session_id;
  EXPECT_TRUE(ProcessIdToSessionId(GetCurrentProcessId(), &session_id));

  // |screenlock_monitor_source_| should ignore events from other sessions.
  GenerateFakeLockEvent(session_id + 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(observer.IsScreenLocked());

  GenerateFakeLockEvent(session_id);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.IsScreenLocked());

  GenerateFakeUnlockEvent(session_id + 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.IsScreenLocked());

  GenerateFakeUnlockEvent(session_id);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(observer.IsScreenLocked());
}

// CAUTION: adding another test here will likely cause issues. See the comment
// at the top of this file, or crbug.com/1166275

}  // namespace content
