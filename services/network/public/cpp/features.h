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

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kBlockAcceptClientHints);
// Note: Do not use BASE_DECLARE_FEATURE_PARAM macro as this is called only once
// per process to construct a static local instance.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
extern const base::FeatureParam<std::string> kBlockAcceptClientHintsBlockedSite;
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
bool ShouldBlockAcceptClientHintsFor(const url::Origin& origin);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kNetworkErrorLogging);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kReporting);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kThrottleDelayable);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kDelayRequestsOnMultiplexedConnections);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kPauseBrowserInitiatedHeavyTrafficForP2P);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kProactivelyThrottleLowPriorityRequests);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kCrossOriginOpenerPolicy);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kCrossOriginOpenerPolicyByDefault);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kCoopNoopenerAllowPopups);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kSplitAuthCacheByNetworkIsolationKey);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kDnsOverHttpsUpgrade);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kMaskedDomainList);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(std::string, kMaskedDomainListExperimentalVersion);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kSplitMaskedDomainList);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kMdnsResponderGeneratedNameListing);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kOpaqueResponseBlockingErrorsForAllFetches);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kAcceptCHFrame);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kOffloadAcceptCHFrameCheck);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kAcceptCHFrameOffloadNotAllowedHints);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kAcceptCHOffloadWithRedirect);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kAcceptCHOffloadForSubframe);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kCorsNonWildcardRequestHeadersSupport);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kOmitCorsClientCert);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kReduceAcceptLanguage);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kReduceAcceptLanguageHTTP);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kReduceAcceptLanguageCacheDuration);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kReduceAcceptLanguageCount);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kMaxAcceptLanguage);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kLocalNetworkAccessChecks);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kLocalNetworkAccessChecksWarn);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kLocalNetworkAccessChecksWebRTC);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kLocalNetworkAccessChecksWebRTCLoopbackOnly);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kLocalNetworkAccessChecksWebSockets);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kLocalNetworkAccessChecksWebTransport);

// If enabled, then the network service will parse the Cookie-Indices header.
// This does not currently control changing cache behavior according to the
// value of this header.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kCookieIndicesHeader);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kCompressionDictionaryTransport);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kPreloadedDictionaryConditionalUse);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kCompressionDictionaryTTL);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kIntegrityPolicyScript);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kReduceTransferSizeUpdatedIPC);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kRendererSideContentDecoding);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kRendererSideContentDecodingPipeSize);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kRendererSideContentDecodingForNavigation);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    bool,
    kRendererSideContentDecodingForceMojoFailureForTesting);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kSkipTpcdMitigationsForAds);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kSkipTpcdMitigationsForAdsHeuristics);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kSkipTpcdMitigationsForAdsMetadata);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kSkipTpcdMitigationsForAdsTrial);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kSkipTpcdMitigationsForAdsTopLevelTrial);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kAvoidResourceRequestCopies);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kDocumentIsolationPolicy);

// Should connection allowlists be enforced?
// https://github.com/mikewest/anti-exfil
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kConnectionAllowlists);

// To actually use the prefetch results, it's also necessary to enable
// kNetworkContextPrefetchUseCache, below.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kNetworkContextPrefetch);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kNetworkContextPrefetchUseMatches);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kCloneDevToolsConnectionOnlyIfRequested);

// Should Sec-Ad-Auction-Event-Recording-Eligible be sent on requests made
// with attributionsrc, and should Ad-Auction-Register-Event responses on
// those requests be processed?
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kAdAuctionEventRegistration);

// Runtime flag that changes default Permissions Policy for features
// join-ad-interest-group and run-ad-auction to a more restricted EnableForSelf.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kAdInterestGroupAPIRestrictedPolicyByDefault);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kDeprecateUnload);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kDeprecateUnloadByAllowList);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kDeprecateUnloadPercent);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kDeprecateUnloadBucket);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(std::string, kDeprecateUnloadAllowlist);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kFrameTopHeader);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kFrameAncestorsHeader);

// Updates the request body, headers, and referrer policy for CORS
// redirects, following 4.4. HTTP-redirect fetch:
// https://fetch.spec.whatwg.org/#http-redirect-fetch
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kUpdateRequestForCorsRedirect);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kBrowsingTopics);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kSharedStorageAPI);
// Maximum number of URLs allowed to be included in the input parameter for
// runURLSelectionOperation().
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    size_t,
    kSharedStorageURLSelectionOperationInputURLSizeLimit);
// Maximum database page size in bytes. Must be a power of two between
// 512 and 65536, inclusive.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kMaxSharedStoragePageSize);
// Maximum database in-memory cache size, in pages.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kMaxSharedStorageCacheSize);
// Maximum number of tries to initialize the database.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kMaxSharedStorageInitTries);
// Maximum number of keys or key-value pairs returned in each batch by
// the async `keys()` and `entries()` iterators, respectively.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kMaxSharedStorageIteratorBatchSize);
// Maximum number of bits of entropy allowed per origin to output via the Shared
// Storage API.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kSharedStorageBitBudget);
// Interval over which `kSharedStorageBitBudget` is defined.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kSharedStorageBudgetInterval);
// Initial interval from service startup after which
// SharedStorageManager first checks for any stale entries, purging any that it
// finds.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kSharedStorageStalePurgeInitialInterval);
// Second and subsequent intervals from service startup after
// which SharedStorageManager checks for any stale entries, purging any that it
// finds.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kSharedStorageStalePurgeRecurringInterval);
// Length of time between last key write access and key expiration. When an
// entry's data is older than this threshold, it will be auto-purged.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kSharedStorageStalenessThreshold);
// Maximum depth of fenced frame where sharedStorage.selectURL() is allowed to
// be invoked. The depth of a fenced frame is the number of the fenced frame
// boundaries above that frame (i.e. the outermost main frame's frame tree has
// fenced frame depth 0, a topmost fenced frame tree embedded in the outermost
// main frame has fenced frame depth 1, etc).
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(
    size_t,
    kSharedStorageMaxAllowedFencedFrameDepthForSelectURL);
// If enabled, sends additional details in the error message for the
// rejected promise when shared storage is disabled, for local troubleshooting
// and use in testing.
//
// NOTE: To preserve user privacy, this feature param MUST remain false by
// default.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool,
                           kSharedStorageExposeDebugMessageForSettingsStatus);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kSharedStorageTransactionalBatchUpdate);

// Backend storage + kill switch for Interest Group API origin trials.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kInterestGroupStorage);
// Backend storage + kill switch for Interest Group API origin trials.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kInterestGroupStorageMaxOwners);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kInterestGroupStorageMaxStoragePerOwner);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kInterestGroupStorageMaxGroupsPerOwner);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kInterestGroupStorageMaxNegativeGroupsPerOwner);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kInterestGroupStorageMaxOpsBeforeMaintenance);

// When enabled, returns the output of GetCookiesString when calling
// SetCookiesString, so that it can be cached in the renderer to avoid an IPC
// on subsequent Get requests.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kGetCookiesOnSet);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kIncreaseCookieAccessCacheSize);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(int, kCookieAccessCacheSize);

// If enabled, permissions policies relevant to a request are populated on
// `network:ResourceRequest`.
//
// Note: Policies are not guaranteed to be added on every path. If
// `PermissionsPolicy` on the request is nullopt, you need to set it somewhere.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kPopulatePermissionsPolicyOnRequest);

// Enables CORS safelisting the Protected Audience Trusted Key-Value
// Content-Type.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kProtectedAudienceCorsSafelistKVv2Signals);

// If enabled and `kPopulatePermissionsPolicyOnRequest` is also enabled, storage
// access headers will respect the "storage-access" permissions policy when
// calculating storage access status.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kStorageAccessHeadersRespectPermissionsPolicy);

// When enabled, a shared remote Mojo interface of
// DeviceBoundSessionAccessObserver is used to reduce Clone() IPC.
// See https://crbug.com/407680127 for more details.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kDeviceBoundSessionAccessObserverSharedRemote);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kCSPScriptSrcV2);

// When enabled, allowlisting script urls and scripts used in eval via hashes
// will be supported in script-src.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kCSPScriptSrcHashesInV1);

// When enabled, fetches for "pervasive" scripts that match one of the
// configured patterns will use a shared, single-keyed cache.
// See https://chromestatus.com/feature/5202380930678784
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kCacheSharingForPervasiveScripts);

// When enabled, sends SameSite=Lax cookies for FedCM requests in addition to
// SameSite=None.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kSendSameSiteLaxForFedCM);

// newline-delimited list of URL patterns for "pervasive" scripts.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(std::string, kPervasiveScriptURLPatterns);

// When enabled, disk-based shared dictionaries will use a memory cache to
// keep frequently used dictionaries in memory.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kSharedDictionaryCache);

COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(size_t, kSharedDictionaryCacheSize);

// Maximum size of dictionaries that are allowed to be stored in the cache.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(size_t, kSharedDictionaryCacheMaxSizeBytes);

// When enabled, Network Service Task Scheduler is enabled on the Network
// Service's IO Thread.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kNetworkServiceTaskScheduler);

// These parameters control whether the Network Service Task Scheduler is used
// for specific classes.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kNetworkServiceTaskSchedulerResourceScheduler);
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE_PARAM(bool, kNetworkServiceTaskSchedulerURLLoader);

// When enabled, Network Service Task Scheduler supports
// per-net::RequestrPriority task queues for each RequestPriority variant.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kNetworkServicePerPriorityTaskQueues);

// When enabled the browser process will start an UnexportableKeyService proxy
// service that can be used by other processes. It will also make the network
// process start using the proxy. This is needed for example when implementing
// DBSC for macOS, since access to the Secure Enclave requires higher privileges
// than what the network process has.
COMPONENT_EXPORT(NETWORK_CPP_FLAGS_AND_SWITCHES)
BASE_DECLARE_FEATURE(kUseUnexportableKeyServiceInBrowserProcess);

}  // namespace network::features

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FEATURES_H_
