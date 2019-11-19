// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "chromeos/settings/timezone_settings.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace device {

class TimeZoneMonitorChromeOS
    : public TimeZoneMonitor,
      public chromeos::system::TimezoneSettings::Observer {
 public:
  TimeZoneMonitorChromeOS() : TimeZoneMonitor() {
    chromeos::system::TimezoneSettings::GetInstance()->AddObserver(this);
  }

  ~TimeZoneMonitorChromeOS() override {
    chromeos::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
  }

  // chromeos::system::TimezoneSettings::Observer implementation.
  void TimezoneChanged(const icu::TimeZone& time_zone) override {
    // ICU's default time zone is already set to a new zone. No need to redetect
    // it with detectHostTimeZone() or to update ICU.
    NotifyClients(GetTimeZoneId(time_zone));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeZoneMonitorChromeOS);
};

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  return std::unique_ptr<TimeZoneMonitor>(new TimeZoneMonitorChromeOS());
}

}  // namespace device
