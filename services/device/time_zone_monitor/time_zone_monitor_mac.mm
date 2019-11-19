// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <memory>

#include "base/macros.h"
#include "services/device/time_zone_monitor/time_zone_monitor.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace device {

class TimeZoneMonitorMac : public TimeZoneMonitor {
 public:
  TimeZoneMonitorMac() : TimeZoneMonitor() {
    NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
    notification_observer_ =
        [nc addObserverForName:NSSystemTimeZoneDidChangeNotification
                        object:nil
                         queue:nil
                    usingBlock:^(NSNotification* notification) {
                      UpdateIcuAndNotifyClients(DetectHostTimeZoneFromIcu());
                    }];
  }

  ~TimeZoneMonitorMac() override {
    NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
    [nc removeObserver:notification_observer_];
  }

 private:
  id notification_observer_;

  DISALLOW_COPY_AND_ASSIGN(TimeZoneMonitorMac);
};

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  return std::unique_ptr<TimeZoneMonitor>(new TimeZoneMonitorMac());
}

}  // namespace device
