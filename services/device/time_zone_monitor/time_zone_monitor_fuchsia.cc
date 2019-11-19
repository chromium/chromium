// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include <memory>

#include "base/logging.h"

namespace device {
namespace {

// TODO(fuchsia): Implement this. crbug.com/750934
class TimeZoneMonitorFuchsia : public TimeZoneMonitor {
 public:
  TimeZoneMonitorFuchsia() = default;
  ~TimeZoneMonitorFuchsia() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeZoneMonitorFuchsia);
};

}  // namespace

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  // TODO(https://crbug.com/750934): Implement a real TimeZoneMonitor.

  return std::make_unique<TimeZoneMonitorFuchsia>();
}

}  // namespace device
