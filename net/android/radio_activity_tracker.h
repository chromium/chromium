// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_RADIO_ACTIVITY_TRACKER_H_
#define NET_ANDROID_RADIO_ACTIVITY_TRACKER_H_

#include "base/android/radio_utils.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

struct NetworkTrafficAnnotationTag;

namespace android {

// Tracks radio states and provides helper methods to record network activities
// which may trigger power-consuming radio state changes like wake-ups.
class NET_EXPORT RadioActivityTracker {
 public:
  static RadioActivityTracker& GetInstance();

  RadioActivityTracker(const RadioActivityTracker&) = delete;
  RadioActivityTracker& operator=(const RadioActivityTracker&) = delete;
  RadioActivityTracker(RadioActivityTracker&&) = delete;
  RadioActivityTracker& operator=(RadioActivityTracker&&) = delete;

  // Returns true when a network activity such as creating a network request and
  // resolving a host name could trigger a radio wakeup.
  // TODO(crbug.com/1232623): Consider optimizing this function. This function
  // uses Android's platform APIs which add non-negligible overheads.
  bool ShouldRecordActivityForWakeupTrigger();

  // These override internal members for testing.
  void OverrideRadioActivityForTesting(
      absl::optional<base::android::RadioDataActivity> radio_activity) {
    radio_activity_override_for_testing_ = radio_activity;
  }
  void OverrideRadioTypeForTesting(
      absl::optional<base::android::RadioConnectionType> radio_type) {
    radio_type_override_for_testing_ = radio_type;
  }
  void OverrideLastCheckTimeForTesting(base::TimeTicks last_check_time) {
    last_check_time_ = last_check_time;
  }

 private:
  friend class base::NoDestructor<RadioActivityTracker>;
  RadioActivityTracker();
  ~RadioActivityTracker() = delete;

  // Returns true when RadioUtils is available or any radio states are
  // overridden for testing.
  bool IsRadioUtilsSupported();

  // Contains potentially expensive API calls. Factored out to measure
  // overheads.
  bool ShouldRecordActivityForWakeupTriggerInternal();

  // Updated when ShouldRecordActivityForWakeupTrigger() is called.
  base::android::RadioDataActivity last_radio_data_activity_ =
      base::android::RadioDataActivity::kNone;
  base::TimeTicks last_check_time_;

  // Radio state overrides for testing.
  absl::optional<base::android::RadioDataActivity>
      radio_activity_override_for_testing_;
  absl::optional<base::android::RadioConnectionType>
      radio_type_override_for_testing_;
};

constexpr char kUmaNamePossibleWakeupTriggerTCPWriteAnnotationId[] =
    "Net.Radio.PossibleWakeupTrigger.TCPWriteAnnotationId";
constexpr char kUmaNamePossibleWakeupTriggerUDPWriteAnnotationId[] =
    "Net.Radio.PossibleWakeupTrigger.UDPWriteAnnotationId";

// Records a histogram when writing data to a TCP socket likely wake-ups radio.
NET_EXPORT void MaybeRecordTCPWriteForWakeupTrigger(
    const NetworkTrafficAnnotationTag& traffic_annotation);

// Records a histogram when writing data to a UDP socket likely wake-ups radio.
NET_EXPORT void MaybeRecordUDPWriteForWakeupTrigger(
    const NetworkTrafficAnnotationTag& traffic_annotation);

}  // namespace android
}  // namespace net

#endif  // NET_ANDROID_RADIO_ACTIVITY_TRACKER_H_
