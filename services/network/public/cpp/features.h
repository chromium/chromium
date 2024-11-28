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
#include "build/build_config.h"

namespace url {
class Origin;
}  // namespace url

namespace network::features {

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kBlockAcceptClientHints);
// Note: Do not use BASE_DECLARE_FEATURE_PARAM macro as this is called only once
// per process to construct a static local instance.
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
BASE_DECLARE_FEATURE_PARAM(int, kMaskedDomainListExperimentGroup);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE_PARAM(std::string, kMaskedDomainListExperimentalVersion);
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
BASE_DECLARE_FEATURE_PARAM(TrustTokenOriginTrialSpec,
                           kTrustTokenOperationsRequiringOriginTrial);
COMPONENT_EXPORT(NETWORK_CPP)

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kAcceptCHFrame);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCorsNonWildcardRequestHeadersSupport);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kOmitCorsClientCert);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kPervasivePayloadsList);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kReduceAcceptLanguage);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kReduceAcceptLanguageCacheDuration);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kPrivateNetworkAccessPreflightShortTimeout);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kLocalNetworkAccessAllowPotentiallyTrustworthySameOrigin);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kPrivateNetworkAccessPermissionPrompt);

// If enabled, then the network service will parse the Cookie-Indices header.
// This does not currently control changing cache behavior according to the
// value of this header.
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCookieIndicesHeader);

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
BASE_DECLARE_FEATURE_PARAM(bool, kSkipTpcdMitigationsForAdsHeuristics);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE_PARAM(bool, kSkipTpcdMitigationsForAdsMetadata);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE_PARAM(bool, kSkipTpcdMitigationsForAdsTrial);
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE_PARAM(bool, kSkipTpcdMitigationsForAdsTopLevelTrial);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kAvoidResourceRequestCopies);

COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kDocumentIsolationPolicy);

// To actually use the prefetch results, it's also necessary to enable
// kNetworkContextPrefetchUseCache, below.
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kNetworkContextPrefetch);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kNetworkContextPrefetchUseMatches);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kTreatNullIPAsPublicAddressSpace);

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kCloneDevToolsConnectionOnlyIfRequested);

// Enables the Storage Access Headers semantics.
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kStorageAccessHeaders);

// Enables the Storage Access Headers Origin Trial.
COMPONENT_EXPORT(NETWORK_CPP) BASE_DECLARE_FEATURE(kStorageAccessHeadersTrial);

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kEnableLockCookieDatabaseByDefault);
#endif  // BUILDFLAG(IS_WIN)

// Should SRI-compliant HTTP Message Signatures be enforced?
// https://wicg.github.io/signature-based-sri/
COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kSRIMessageSignatureEnforcement);

}  // namespace network::features

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FEATURES_H_
