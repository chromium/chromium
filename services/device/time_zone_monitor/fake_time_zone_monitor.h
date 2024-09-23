// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_TIME_ZONE_MONITOR_FAKE_TIME_ZONE_MONITOR_H_
#define SERVICES_DEVICE_TIME_ZONE_MONITOR_FAKE_TIME_ZONE_MONITOR_H_

#include "services/device/time_zone_monitor/time_zone_monitor.h"

namespace device {

// Empty TimeZoneMonitor implementation that doesn't observe time zone changes
// in the OS so it never changes ICU's time zone during its lifetime.
class FakeTimeZoneMonitor : public device::TimeZoneMonitor {
 public:
  FakeTimeZoneMonitor() = default;
  ~FakeTimeZoneMonitor() override = default;
};

}  // namespace device

#endif  // SERVICES_DEVICE_TIME_ZONE_MONITOR_FAKE_TIME_ZONE_MONITOR_H_
