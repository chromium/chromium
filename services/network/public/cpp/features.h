// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_FEATURES_H_
#define SERVICES_NETWORK_PUBLIC_CPP_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace network {
namespace features {

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kNetworkErrorLogging);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kReporting);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kThrottleDelayable);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kDelayRequestsOnMultiplexedConnections);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kPauseBrowserInitiatedHeavyTrafficForP2P);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kCORBProtectionSniffing);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kProactivelyThrottleLowPriorityRequests);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kCrossOriginOpenerPolicy);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCrossOriginOpenerPolicyByDefault);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kCoopRestrictProperties);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kSplitAuthCacheByNetworkIsolationKey);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kDnsOverHttpsUpgrade);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kMdnsResponderGeneratedNameListing);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kOpaqueResponseBlockingV01);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kOpaqueResponseBlockingV02);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kAttributionReportingTriggerAttestation);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kPrivateStateTokens);

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

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kWebSocketReassembleShortMessages);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kAcceptCHFrame);

enum class DataPipeAllocationSize {
  kDefaultSizeOnly,
  kLargerSizeIfPossible,
};

COMPONENT_EXPORT(NETWORK_CPP)
extern uint32_t GetDataPipeDefaultAllocationSize(
    DataPipeAllocationSize = DataPipeAllocationSize::kDefaultSizeOnly);

COMPONENT_EXPORT(NETWORK_CPP)
extern uint32_t GetLoaderChunkSize();

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCorsNonWildcardRequestHeadersSupport);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kBatchSimpleURLLoader);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kNetworkServiceMemoryCache);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kOmitCorsClientCert);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kCacheTransparency);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kPervasivePayloadsList);

COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<std::string>
    kCacheTransparencyPervasivePayloads;

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kReduceAcceptLanguage);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kReduceAcceptLanguageOriginTrial);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kDisableResourceScheduler);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kPrivateNetworkAccessPreflightShortTimeout);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kPrivateNetworkAccessAllowSecureSameOrigin);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kPreconnectInNetworkService);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kPrefetchDNSWithURL);

COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<bool> kPrefetchDNSWithURLAllAnchorElements;

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kPreconnectOnRedirect);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kOutOfProcessSystemDnsResolution);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kAccessControlAllowMethodsInCORSPreflightSpecConformant);

// If enabled, then navigation requests should check the match responses in the
// prefetch cache by using the No-Vary-Search rules if No-Vary-Search header
// is specified in prefetched responses.
// Feature Meta bug: crbug.com/1378072.
// No-Vary-Search explainer:
//   https://github.com/WICG/nav-speculation/blob/main/no-vary-search.md
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kPrefetchNoVarySearch);

// Enables the `inline-speculation-rules` source support in the
// Content-Security-Policy for Prerender2.
// https://crbug.com/1382361
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kPrerender2ContentSecurityPolicyExtensions);

// Enables UMA to track received GetCookiesString IPCs. This feature is enabled
// by default, it is just here to allow some tests to disable it. These tests
// make use of TaskEnvironment::FastForward with very long delays (days) which
// interacts poorly with this metric that is recorded every 30s.
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kGetCookiesStringUma);

}  // namespace features
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FEATURES_H_
