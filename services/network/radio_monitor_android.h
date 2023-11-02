// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_RADIO_MONITOR_ANDROID_H_
#define SERVICES_NETWORK_RADIO_MONITOR_ANDROID_H_

#include "base/component_export.h"
#include "services/network/public/mojom/host_resolver.mojom-forward.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

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

// Records UMAs when a network request initiated by a URLLoader likely
// wake-ups radio.
COMPONENT_EXPORT(NETWORK_SERVICE)
void MaybeRecordURLLoaderCreationForWakeupTrigger(
    const ResourceRequest& request,
    const net::NetworkTrafficAnnotationTag& traffic_annotation);

// Records a host resolve request when the request likely wake-ups radio.
COMPONENT_EXPORT(NETWORK_SERVICE)
void MaybeRecordResolveHostForWakeupTrigger(
    const mojom::ResolveHostParametersPtr& parameters);

}  // namespace network

#endif  // SERVICES_NETWORK_RADIO_MONITOR_ANDROID_H_
