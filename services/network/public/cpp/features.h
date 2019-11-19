// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_FEATURES_H_
#define SERVICES_NETWORK_PUBLIC_CPP_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace network {
namespace features {

COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kCapReferrerToOriginOnCrossOrigin;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kExpectCTReporting;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kNetworkErrorLogging;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kNetworkService;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kOutOfBlinkCors;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kReporting;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kThrottleDelayable;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kDelayRequestsOnMultiplexedConnections;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kFetchMetadata;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kFetchMetadataDestination;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kRequestInitiatorSiteLock;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kPauseBrowserInitiatedHeavyTrafficForP2P;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kCORBProtectionSniffing;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kProactivelyThrottleLowPriorityRequests;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kCrossOriginIsolation;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kBlockNonSecureExternalRequests;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kPrefetchMainResourceNetworkIsolationKey;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kSplitAuthCacheByNetworkIsolationKey;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kDnsOverHttpsUpgrade;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kMdnsResponderGeneratedNameListing;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<std::string>
    kDnsOverHttpsUpgradeDisabledProvidersParam;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kDisableKeepaliveFetch;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kOutOfBlinkFrameAncestors;

COMPONENT_EXPORT(NETWORK_CPP)
bool ShouldEnableOutOfBlinkCorsForTesting();

}  // namespace features
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FEATURES_H_
