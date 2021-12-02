// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/radio_monitor_android.h"

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace network {

namespace {

// The minimum interval for recording possible radio wake-ups. It's unlikely
// that radio state transitions happen in seconds.
constexpr static base::TimeDelta
    kMinimumRecordIntervalForPossibleWakeupTrigger = base::Seconds(1);

}  // namespace

// static
RadioMonitorAndroid& RadioMonitorAndroid::GetInstance() {
  static base::NoDestructor<RadioMonitorAndroid> s_instance;
  return *s_instance;
}

RadioMonitorAndroid::RadioMonitorAndroid() = default;

void RadioMonitorAndroid::MaybeRecordURLLoader(
    const ResourceRequest& request,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(base::FeatureList::IsEnabled(features::kRecordRadioWakeupTrigger));
  if (!ShouldRecordRadioWakeupTrigger())
    return;

  TRACE_EVENT_INSTANT1("loading", "RadioMonitorAndroid::URLLoaderWakeupRadio",
                       TRACE_EVENT_SCOPE_THREAD, "traffic_annotation",
                       traffic_annotation.unique_id_hash_code);

  base::UmaHistogramEnumeration(
      kUmaNamePossibleWakeupTriggerURLLoaderRequestDestination,
      request.destination);
  base::UmaHistogramEnumeration(
      kUmaNamePossibleWakeupTriggerURLLoaderRequestPriority, request.priority,
      static_cast<net::RequestPriority>(
          net::RequestPrioritySize::NUM_PRIORITIES));
  base::UmaHistogramBoolean(
      kUmaNamePossibleWakeupTriggerURLLoaderRequestIsPrefetch,
      request.load_flags & net::LOAD_PREFETCH);
  base::UmaHistogramSparse(kUmaNamePossibleWakeupTriggerURLLoaderAnnotationId,
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

  TRACE_EVENT_INSTANT1("loading",
                       "RadioMonitorAndroid::HostResolverWakeupRadio",
                       TRACE_EVENT_SCOPE_THREAD, "purpose", purpose);

  base::UmaHistogramEnumeration(kUmaNamePossibleWakeupTriggerResolveHost,
                                purpose);
}

bool RadioMonitorAndroid::IsRadioUtilsSupported() {
  return base::android::RadioUtils::IsSupported() ||
         radio_activity_override_for_testing_.has_value() ||
         radio_type_override_for_testing_.has_value();
}

bool RadioMonitorAndroid::ShouldRecordRadioWakeupTrigger() {
  // Check recording interval first to reduce overheads of calling Android's
  // platform APIs.
  base::TimeTicks now = base::TimeTicks::Now();
  if (!last_record_time_.is_null() &&
      now - last_record_time_ < kMinimumRecordIntervalForPossibleWakeupTrigger)
    return false;

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

  if (!radio_activity.has_value())
    return false;

  // When the last activity was dormant, don't treat this event as a wakeup
  // trigger since there could be state transition delay and startup latency.
  bool should_record =
      *radio_activity == base::android::RadioDataActivity::kDormant &&
      last_radio_data_activity_ != base::android::RadioDataActivity::kDormant;

  last_radio_data_activity_ = *radio_activity;
  last_record_time_ = now;

  return should_record;
}

}  // namespace network
