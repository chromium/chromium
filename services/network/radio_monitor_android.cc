// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/radio_monitor_android.h"

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "net/android/radio_activity_tracker.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace network {

void MaybeRecordURLLoaderCreationForWakeupTrigger(
    const ResourceRequest& request,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  if (!net::android::RadioActivityTracker::GetInstance()
           .ShouldRecordActivityForWakeupTrigger())
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

void MaybeRecordResolveHostForWakeupTrigger(
    const mojom::ResolveHostParametersPtr& parameters) {
  if (!net::android::RadioActivityTracker::GetInstance()
           .ShouldRecordActivityForWakeupTrigger())
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

}  // namespace network
