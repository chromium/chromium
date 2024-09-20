// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_FEATURES_H_
#define SERVICES_NETWORK_PUBLIC_CPP_FEATURES_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace url {
class Origin;
}  // namespace url

namespace network::features {

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kBlockAcceptClientHints);
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<std::string> kBlockAcceptClientHintsBlockedSite;
COMPONENT_EXPORT(NETWORK_CPP)
bool ShouldBlockAcceptClientHintsFor(const url::Origin& origin);

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
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kCoopNoopenerAllowPopups);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kCoopRestrictProperties);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCoopRestrictPropertiesOriginTrial);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kSplitAuthCacheByNetworkIsolationKey);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kDnsOverHttpsUpgrade);
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kMaskedDomainList);
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<int> kMaskedDomainListExperimentGroup;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<std::string>
    kMaskedDomainListExperimentalVersion;
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kMdnsResponderGeneratedNameListing);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kOpaqueResponseBlockingErrorsForAllFetches);

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

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kAcceptCHFrame);

enum class DataPipeAllocationSize {
  kDefaultSizeOnly,
  kLargerSizeIfPossible,
};

COMPONENT_EXPORT(NETWORK_CPP)
extern uint32_t GetDataPipeDefaultAllocationSize(
    DataPipeAllocationSize = DataPipeAllocationSize::kDefaultSizeOnly);

COMPONENT_EXPORT(NETWORK_CPP)
extern size_t GetNetAdapterMaxBufSize();

COMPONENT_EXPORT(NETWORK_CPP)
extern size_t GetLoaderChunkSize();

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCorsNonWildcardRequestHeadersSupport);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kOmitCorsClientCert);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kPervasivePayloadsList);

COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<std::string>
    kCacheTransparencyPervasivePayloads;

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kReduceAcceptLanguage);
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<base::TimeDelta>
    kReduceAcceptLanguageCacheDuration;

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kPrivateNetworkAccessPreflightShortTimeout);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kLocalNetworkAccessAllowPotentiallyTrustworthySameOrigin);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kPrivateNetworkAccessPermissionPrompt);

COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<bool> kPrefetchDNSWithURLAllAnchorElements;

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kAccessControlAllowMethodsInCORSPreflightSpecConformant);

// If enabled, then the network service will parse the Cookie-Indices header.
// This does not currently control changing cache behavior according to the
// value of this header.
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCookieIndicesHeader);

// Enables UMA to track received GetCookiesString IPCs. This feature is enabled
// by default, it is just here to allow some tests to disable it. These tests
// make use of TaskEnvironment::FastForward with very long delays (days) which
// interacts poorly with this metric that is recorded every 30s.
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kGetCookiesStringUma);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCompressionDictionaryTransportBackend);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCompressionDictionaryTransport);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kPreloadedDictionaryConditionalUse);

// Enables visibility aware network service resource scheduler. When enabled,
// request may be prioritized or de-prioritized based on the visibility of
// requestors.
// TODO(crbug.com/40066382): Remove this feature.
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kVisibilityAwareResourceScheduler);

// Enables Compression Dictionary Transport with Zstandard (aka Shared Zstd).
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kSharedZstd);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kReduceTransferSizeUpdatedIPC);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kSkipTpcdMitigationsForAds);
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<bool> kSkipTpcdMitigationsForAdsHeuristics;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<bool> kSkipTpcdMitigationsForAdsMetadata;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<bool> kSkipTpcdMitigationsForAdsTrial;
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<bool> kSkipTpcdMitigationsForAdsTopLevelTrial;

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kAvoidResourceRequestCopies);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kDocumentIsolationPolicy);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kNetworkContextPrefetch);

COMPONENT_EXPORT(NETWORK_CPP)
extern const base::FeatureParam<int> kNetworkContextPrefetchMaxLoaders;

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kTreatNullIPAsPublicAddressSpace);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCloneDevToolsConnectionOnlyIfRequested);

// Enables the Storage Access Headers semantics.
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kStorageAccessHeaders);

// Enables the Storage Access Headers Origin Trial.
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kStorageAccessHeadersTrial);

}  // namespace network::features

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FEATURES_H_
