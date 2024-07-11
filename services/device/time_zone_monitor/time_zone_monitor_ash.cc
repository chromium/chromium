// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace device {

class TimeZoneMonitorAsh : public TimeZoneMonitor,
                           public ash::system::TimezoneSettings::Observer {
 public:
  TimeZoneMonitorAsh() : TimeZoneMonitor() {
    ash::system::TimezoneSettings::GetInstance()->AddObserver(this);
  }

  TimeZoneMonitorAsh(const TimeZoneMonitorAsh&) = delete;
  TimeZoneMonitorAsh& operator=(const TimeZoneMonitorAsh&) = delete;

  ~TimeZoneMonitorAsh() override {
    ash::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
  }

  // ash::system::TimezoneSettings::Observer implementation.
  void TimezoneChanged(const icu::TimeZone& time_zone) override {
    UpdateIcuAndNotifyClients(base::WrapUnique(time_zone.clone()));
  }
};

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  return std::make_unique<TimeZoneMonitorAsh>();
}

}  // namespace device
