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
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kProactivelyThrottleLowPriorityRequests);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kCrossOriginOpenerPolicy);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCrossOriginOpenerPolicyByDefault);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kCoopRestrictProperties);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCoopRestrictPropertiesOriginTrial);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kSplitAuthCacheByNetworkIsolationKey);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kDnsOverHttpsUpgrade);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kMaskedDomainList);
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<std::string>
    kMaskedDomainListExperimentalVersion;
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kMdnsResponderGeneratedNameListing);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kOpaqueResponseBlockingV02);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kOpaqueResponseBlockingErrorsForAllFetches);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kAttributionReportingReportVerification);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kAttributionReportingCrossAppWeb);

// Both flags need to be checked for required PST components as they are being
// used in different experiments.
//
// kFledgePst is the original flag used in the OT and respects
// the TrustTrialOriginTrialSpec. It will be deprecated in favor of
// kPrivateStateTokens when the experiment is over.
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kPrivateStateTokens);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kFledgePst);

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
extern uint32_t GetNetAdapterMaxBufSize();

COMPONENT_EXPORT(NETWORK_CPP)
extern uint32_t GetLoaderChunkSize();

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCorsNonWildcardRequestHeadersSupport);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kNetworkServiceMemoryCache);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kOmitCorsClientCert);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kCacheTransparency);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kPervasivePayloadsList);

COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<std::string>
    kCacheTransparencyPervasivePayloads;

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kReduceAcceptLanguage);
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<base::TimeDelta>
    kReduceAcceptLanguageCacheDuration;

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kReduceAcceptLanguageOriginTrial);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kPrivateNetworkAccessPreflightShortTimeout);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kLocalNetworkAccessAllowPotentiallyTrustworthySameOrigin);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kPrivateNetworkAccessPermissionPrompt);

COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<bool> kPrefetchDNSWithURLAllAnchorElements;

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

// Enables UMA to track received GetCookiesString IPCs. This feature is enabled
// by default, it is just here to allow some tests to disable it. These tests
// make use of TaskEnvironment::FastForward with very long delays (days) which
// interacts poorly with this metric that is recorded every 30s.
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kGetCookiesStringUma);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCompressionDictionaryTransportBackend);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCompressionDictionaryTransport);

// Enables visibility aware network service resource scheduler. When enabled,
// request may be prioritized or de-prioritized based on the visibility of
// requestors.
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kVisibilityAwareResourceScheduler);

// Enables Compression Dictionary Transport with Zstandard (aka Shared Zstd).
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kSharedZstd);

}  // namespace features
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FEATURES_H_
