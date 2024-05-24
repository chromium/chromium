// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SCOPED_TIME_ZONE_H_
#define CONTENT_PUBLIC_TEST_SCOPED_TIME_ZONE_H_

#include "base/test/icu_test_util.h"
#include "services/device/time_zone_monitor/fake_time_zone_monitor.h"

namespace content {

// Helper class to temporarily set the time zone of the browser process and
// propagate it to newly spawned child processes.
class ScopedTimeZone {
 public:
  explicit ScopedTimeZone(const char* new_zoneid);
  ScopedTimeZone(const ScopedTimeZone&) = delete;
  ScopedTimeZone& operator=(const ScopedTimeZone&) = delete;
  ~ScopedTimeZone();

 private:
  // Must be created before TimeZoneMonitor. ScopedRestoreDefaultTimezone sets
  // the new time zone in ICU which is then picked by TimeZoneMonitor
  // constructor.
  base::test::ScopedRestoreDefaultTimezone icu_time_zone_;

  device::FakeTimeZoneMonitor time_zone_monitor_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SCOPED_TIME_ZONE_H_
