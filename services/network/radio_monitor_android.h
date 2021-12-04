// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_RADIO_MONITOR_ANDROID_H_
#define SERVICES_NETWORK_RADIO_MONITOR_ANDROID_H_

#include "base/android/radio_utils.h"
#include "base/component_export.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/host_resolver.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

struct ResourceRequest;

constexpr char kUmaNamePossibleWakeupTriggerURLLoaderAnnotationId[] =
    "Network.Radio.PossibleWakeupTrigger.URLLoaderAnnotationId2";
constexpr char kUmaNamePossibleWakeupTriggerURLLoaderRequestDestination[] =
    "Network.Radio.PossibleWakeupTrigger.URLLoaderRequestDestination";
constexpr char kUmaNamePossibleWakeupTriggerURLLoaderRequestPriority[] =
    "Network.Radio.PossibleWakeupTrigger.URLLoaderRequestPriority";
constexpr char kUmaNamePossibleWakeupTriggerURLLoaderRequestIsPrefetch[] =
    "Network.Radio.PossibleWakeupTrigger.URLLoaderRequestIsPrefetch";
constexpr char kUmaNamePossibleWakeupTriggerResolveHost[] =
    "Network.Radio.PossibleWakeupTrigger.ResolveHostPurpose2";

// Checks radio states and records histograms when network activities may
// trigger power-consuming radio state changes like wake-ups.
class COMPONENT_EXPORT(NETWORK_SERVICE) RadioMonitorAndroid {
 public:
  static RadioMonitorAndroid& GetInstance();

  RadioMonitorAndroid(const RadioMonitorAndroid&) = delete;
  RadioMonitorAndroid& operator=(const RadioMonitorAndroid&) = delete;
  RadioMonitorAndroid(RadioMonitorAndroid&&) = delete;
  RadioMonitorAndroid& operator=(RadioMonitorAndroid&&) = delete;

  // Records UMAs when a network request initiated by a URLLoader likely
  // wake-ups radio.
  void MaybeRecordURLLoader(
      const ResourceRequest& request,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Records a host resolve request when the request likely wake-ups radio.
  void MaybeRecordResolveHost(
      const mojom::ResolveHostParametersPtr& parameters);

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
  friend class base::NoDestructor<RadioMonitorAndroid>;
  RadioMonitorAndroid();
  ~RadioMonitorAndroid() = delete;

  // Returns true when RadioUtils is available or any radio states are
  // overridden for testing.
  bool IsRadioUtilsSupported();

  // Returns true when an incoming network event such as creating URLLoader and
  // resolving a host name could trigger a radio wakeup.
  // TODO(crbug.com/1232623): Consider optimizing this function. This function
  // uses Android's platform APIs which add non-negligible overheads.
  bool ShouldRecordRadioWakeupTrigger();
  // Contains maybe-expensive API calls.
  bool ShouldRecordRadioWakeupTriggerInternal();

  // Updated when ShouldRecordRadioWakeupTrigger() is called.
  base::android::RadioDataActivity last_radio_data_activity_ =
      base::android::RadioDataActivity::kNone;
  base::TimeTicks last_check_time_;

  // Radio state overrides for testing.
  absl::optional<base::android::RadioDataActivity>
      radio_activity_override_for_testing_;
  absl::optional<base::android::RadioConnectionType>
      radio_type_override_for_testing_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_RADIO_MONITOR_ANDROID_H_
