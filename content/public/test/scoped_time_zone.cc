// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/scoped_time_zone.h"

#include "services/device/device_service.h"

namespace content {

ScopedTimeZone::ScopedTimeZone(const char* new_zoneid)
    : icu_time_zone_(new_zoneid) {
  device::DeviceService::OverrideTimeZoneMonitorBinderForTesting(
      base::BindRepeating(&device::FakeTimeZoneMonitor::Bind,
                          base::Unretained(&time_zone_monitor_)));
}

ScopedTimeZone::~ScopedTimeZone() {
  device::DeviceService::OverrideTimeZoneMonitorBinderForTesting(
      base::NullCallback());
}

}  // namespace content
