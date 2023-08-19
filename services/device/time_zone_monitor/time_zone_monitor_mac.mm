// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <memory>

#import "base/task/sequenced_task_runner.h"
#include "services/device/time_zone_monitor/time_zone_monitor.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace device {

class TimeZoneMonitorMac : public TimeZoneMonitor {
 public:
  TimeZoneMonitorMac() {
    NSNotificationCenter* nc = NSNotificationCenter.defaultCenter;
    notification_observer_ =
        [nc addObserverForName:NSSystemTimeZoneDidChangeNotification
                        object:nil
                         queue:nil
                    usingBlock:^(NSNotification* notification) {
                      UpdateIcuAndNotifyClients(DetectHostTimeZoneFromIcu());
                    }];
  }

  TimeZoneMonitorMac(const TimeZoneMonitorMac&) = delete;
  TimeZoneMonitorMac& operator=(const TimeZoneMonitorMac&) = delete;

  ~TimeZoneMonitorMac() override {
    [NSNotificationCenter.defaultCenter removeObserver:notification_observer_];
  }

 private:
  id __strong notification_observer_;
};

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  return std::unique_ptr<TimeZoneMonitor>(new TimeZoneMonitorMac());
}

}  // namespace device
