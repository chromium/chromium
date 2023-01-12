// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include <windows.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace device {

class TimeZoneMonitorWin : public TimeZoneMonitor {
 public:
  TimeZoneMonitorWin()
      : TimeZoneMonitor(),
        singleton_hwnd_observer_(
            base::BindRepeating(&TimeZoneMonitorWin::OnWndProc,
                                base::Unretained(this))),
        current_platform_timezone_(GetPlatformTimeZone()) {}
  TimeZoneMonitorWin(const TimeZoneMonitorWin&) = delete;
  TimeZoneMonitorWin& operator=(const TimeZoneMonitorWin&) = delete;

  ~TimeZoneMonitorWin() override = default;

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
      constexpr auto kMinimalPostTaskDelay = base::Milliseconds(1);
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&TimeZoneMonitorWin::OnWmTimechangeReceived,
                         weak_ptr_factory_.GetWeakPtr()),
          kMinimalPostTaskDelay);
    }
  }

  // Returns the platform specific string for the time zone. Do not rely on the
  // ICU library since it's taking into account other sources for time zone like
  // the TZ environment. This avoid loading the ICU library if not required.
  std::string GetPlatformTimeZone() {
    std::string timezone;
    TIME_ZONE_INFORMATION time_zone_information;
    if (::GetTimeZoneInformation(&time_zone_information) !=
        TIME_ZONE_ID_INVALID) {
      // StandardName field may be empty.
      timezone = base::WideToUTF8(time_zone_information.StandardName);
    }
    return timezone;
  }

  void OnWmTimechangeReceived() {
    TRACE_EVENT0("device", "TimeZoneMonitorWin::OnTimechangeReceived");

    // Only dispatch time zone notifications when the platform time zone has
    // changed. Windows API is sending WM_TIMECHANGE messages each time a
    // time property has changed which is common during a power suspend/resume
    // transition even if the time zone stayed the same. As a good example, any
    // NTP update may trigger a WM_TIMECHANGE message.
    const std::string timezone = GetPlatformTimeZone();
    if (timezone.empty() || current_platform_timezone_ != timezone) {
      UpdateIcuAndNotifyClients(DetectHostTimeZoneFromIcu());
      current_platform_timezone_ = timezone;
    }

    pending_update_notification_tasks_ = false;
  }

  gfx::SingletonHwndObserver singleton_hwnd_observer_;
  bool pending_update_notification_tasks_ = false;
  std::string current_platform_timezone_;
  base::WeakPtrFactory<TimeZoneMonitorWin> weak_ptr_factory_{this};
};

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  return std::make_unique<TimeZoneMonitorWin>();
}

}  // namespace device
