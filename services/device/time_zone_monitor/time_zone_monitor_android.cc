// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/time_zone_monitor/time_zone_monitor_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/timezone_utils.h"  // nogncheck
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "services/device/time_zone_monitor/time_zone_monitor_jni_headers/TimeZoneMonitor_jni.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

using base::android::JavaParamRef;

namespace device {

TimeZoneMonitorAndroid::TimeZoneMonitorAndroid() : TimeZoneMonitor() {
  impl_.Reset(
      Java_TimeZoneMonitor_getInstance(base::android::AttachCurrentThread(),
                                       reinterpret_cast<intptr_t>(this)));
}

TimeZoneMonitorAndroid::~TimeZoneMonitorAndroid() {
  Java_TimeZoneMonitor_stop(base::android::AttachCurrentThread(), impl_);
}

void TimeZoneMonitorAndroid::TimeZoneChangedFromJava(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  // See base/i18n/icu_util.cc:InitializeIcuTimeZone() for more information.
  base::string16 zone_id = base::android::GetDefaultTimeZoneId();
  std::unique_ptr<icu::TimeZone> new_zone(icu::TimeZone::createTimeZone(
      icu::UnicodeString(FALSE, zone_id.data(), zone_id.length())));
  UpdateIcuAndNotifyClients(std::move(new_zone));
}

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  return std::unique_ptr<TimeZoneMonitor>(new TimeZoneMonitorAndroid());
}

}  // namespace device
