// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include <windows.h>

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace device {

class TimeZoneMonitorWin : public TimeZoneMonitor {
 public:
  TimeZoneMonitorWin()
      : TimeZoneMonitor(),
        singleton_hwnd_observer_(new gfx::SingletonHwndObserver(
            base::BindRepeating(&TimeZoneMonitorWin::OnWndProc,
                                base::Unretained(this)))) {}
  TimeZoneMonitorWin(const TimeZoneMonitorWin&) = delete;
  TimeZoneMonitorWin& operator=(const TimeZoneMonitorWin&) = delete;

  ~TimeZoneMonitorWin() override {}

 private:
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_TIMECHANGE && !pending_update_notification_tasks_) {
      // Traces show that in some cases there are multiple WM_TIMECHANGE while
      // performing a power resume. Only sending one is enough
      // (http://crbug.com/1074036).
      pending_update_notification_tasks_ = true;

      // The notifications are sent through a delayed task to avoid running
      // the observers code while the computer is still suspended. The thread
      // controller is not dispatching delayed tasks uuntil the power resume
      // signal is received.
      constexpr auto kMinimalPostTaskDelay =
          base::TimeDelta::FromMilliseconds(1);
      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&TimeZoneMonitorWin::OnWmTimechangeReceived,
                         weak_ptr_factory_.GetWeakPtr()),
          kMinimalPostTaskDelay);
    }
  }

  void OnWmTimechangeReceived() {
    TRACE_EVENT0("device", "TimeZoneMonitorWin::OnTimechangeReceived");
    UpdateIcuAndNotifyClients(DetectHostTimeZoneFromIcu());
    pending_update_notification_tasks_ = false;
  }

  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;
  bool pending_update_notification_tasks_ = false;
  base::WeakPtrFactory<TimeZoneMonitorWin> weak_ptr_factory_{this};
};

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  return std::unique_ptr<TimeZoneMonitor>(new TimeZoneMonitorWin());
}

}  // namespace device
