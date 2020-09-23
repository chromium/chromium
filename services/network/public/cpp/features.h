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
extern const base::Feature kExpectCTReporting;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kNetworkErrorLogging;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kNetworkService;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kReporting;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kThrottleDelayable;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kDelayRequestsOnMultiplexedConnections;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kRequestInitiatorSiteLock;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kPauseBrowserInitiatedHeavyTrafficForP2P;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kCORBProtectionSniffing;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kProactivelyThrottleLowPriorityRequests;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kCrossOriginOpenerPolicy;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kCrossOriginOpenerPolicyReporting;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kCrossOriginOpenerPolicyReportingOriginTrial;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kCrossOriginOpenerPolicyAccessReporting;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kCrossOriginEmbedderPolicy;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kCrossOriginIsolated;
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
extern const base::Feature
    kDeriveOriginFromUrlForNeitherGetNorHeadRequestWhenHavingSpecialAccess;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kEmergencyLegacyCookieAccess;
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kEmergencyLegacyCookieAccessParamName[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<std::string> kEmergencyLegacyCookieAccessParam;

COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kCorbAllowlistAlsoAppliesToOorCors;
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kCorbAllowlistAlsoAppliesToOorCorsParamName[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kRequestInitiatorSiteLockEnfocement;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kCertVerifierService;

COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kTrustTokens;

enum class TrustTokenOriginTrialSpec {
  // See the .cc file for definitions.
  kAllOperationsRequireOriginTrial,
  kOnlyIssuanceRequiresOriginTrial,
  kOriginTrialNotRequired,
};
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<TrustTokenOriginTrialSpec>
    kTrustTokenOperationsRequiringOriginTrial;

COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kOutOfBlinkCSPEE;

COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kWebSocketReassembleShortMessages;

COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kFirstPartySets;

}  // namespace features
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FEATURES_H_
