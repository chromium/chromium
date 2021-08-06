// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/radio_monitor_android.h"

#include "base/metrics/histogram_functions.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace network {

// static
RadioMonitorAndroid& RadioMonitorAndroid::GetInstance() {
  static base::NoDestructor<RadioMonitorAndroid> s_instance;
  return *s_instance;
}

RadioMonitorAndroid::RadioMonitorAndroid() = default;

void RadioMonitorAndroid::MaybeRecordURLLoaderAnnotationId(
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(base::FeatureList::IsEnabled(features::kRecordRadioWakeupTrigger));
  if (!ShouldRecordRadioWakeupTrigger())
    return;

  base::UmaHistogramSparse(kUmaNamePossibleWakeupTriggerURLLoader,
                           traffic_annotation.unique_id_hash_code);
}

void RadioMonitorAndroid::MaybeRecordResolveHost(
    const mojom::ResolveHostParametersPtr& parameters) {
  DCHECK(base::FeatureList::IsEnabled(features::kRecordRadioWakeupTrigger));
  if (!ShouldRecordRadioWakeupTrigger())
    return;

  mojom::ResolveHostParameters::Purpose purpose =
      parameters ? parameters->purpose
                 : mojom::ResolveHostParameters::Purpose::kUnspecified;
  base::UmaHistogramEnumeration(kUmaNamePossibleWakeupTriggerResolveHost,
                                purpose);
}

bool RadioMonitorAndroid::IsRadioUtilsSupported() {
  return base::android::RadioUtils::IsSupported() ||
         radio_activity_override_for_testing_.has_value() ||
         radio_type_override_for_testing_.has_value();
}

bool RadioMonitorAndroid::ShouldRecordRadioWakeupTrigger() {
  if (!IsRadioUtilsSupported())
    return false;

  base::android::RadioConnectionType radio_type =
      radio_type_override_for_testing_.value_or(
          base::android::RadioUtils::GetConnectionType());
  if (radio_type != base::android::RadioConnectionType::kCell)
    return false;

  absl::optional<base::android::RadioDataActivity> radio_activity =
      radio_activity_override_for_testing_.has_value()
          ? radio_activity_override_for_testing_
          : base::android::RadioUtils::GetCellDataActivity();

  return radio_activity.has_value() &&
         *radio_activity == base::android::RadioDataActivity::kDormant;
}

}  // namespace network
