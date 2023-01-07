// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_TIME_ZONE_MONITOR_TIME_ZONE_MONITOR_ANDROID_H_
#define SERVICES_DEVICE_TIME_ZONE_MONITOR_TIME_ZONE_MONITOR_ANDROID_H_

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include <jni.h>

#include "base/android/scoped_java_ref.h"

namespace device {

class TimeZoneMonitorAndroid : public TimeZoneMonitor {
 public:
  TimeZoneMonitorAndroid();

  TimeZoneMonitorAndroid(const TimeZoneMonitorAndroid&) = delete;
  TimeZoneMonitorAndroid& operator=(const TimeZoneMonitorAndroid&) = delete;

  ~TimeZoneMonitorAndroid() override;

  // Called by the Java implementation when the system time zone changes.
  void TimeZoneChangedFromJava(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

 private:
  // Java provider of system time zone change notifications.
  base::android::ScopedJavaGlobalRef<jobject> impl_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_TIME_ZONE_MONITOR_TIME_ZONE_MONITOR_ANDROID_H_
