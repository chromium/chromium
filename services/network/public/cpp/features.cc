// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "net/base/mime_sniffer.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network::features {

// Enables the Accept-CH support disabler. If this feature is activated, Chrome
// ignore Accept-CH response headers for a site that is specified in the
// following kBlockAcceptClientHintsBlockedSite. This is used to compare Chrome
// performance with a dedicated site.
BASE_FEATURE(kBlockAcceptClientHints, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kBlockAcceptClientHintsBlockedSite{
    &kBlockAcceptClientHints, /*name=*/"BlockedSite", /*default_value=*/""};

bool ShouldBlockAcceptClientHintsFor(const url::Origin& origin) {
  // Check if the Accept-CH support is disabled for a specified site.
  static const bool block_accept_ch =
      base::FeatureList::IsEnabled(features::kBlockAcceptClientHints);
  static const base::NoDestructor<url::Origin> blocked_site(url::Origin::Create(
      GURL(features::kBlockAcceptClientHintsBlockedSite.Get())));
  return block_accept_ch && blocked_site->IsSameOriginWith(origin);
}

BASE_FEATURE(kNetworkErrorLogging, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReporting, base::FEATURE_ENABLED_BY_DEFAULT);

// Based on the field trial parameters, this feature will override the value of
// the maximum number of delayable requests allowed in flight. The number of
// delayable requests allowed in flight will be based on the network's
// effective connection type ranges and the
// corresponding number of delayable requests in flight specified in the
// experiment configuration. Based on field trial parameters, this experiment
// may also throttle delayable requests based on the number of non-delayable
// requests in-flight times a weighting factor.
BASE_FEATURE(kThrottleDelayable, base::FEATURE_ENABLED_BY_DEFAULT);

// When kPriorityRequestsDelayableOnSlowConnections is enabled, HTTP
// requests fetched from a SPDY/QUIC/H2 proxies can be delayed by the
// ResourceScheduler just as HTTP/1.1 resources are. However, requests from such
// servers are not subject to kMaxNumDelayableRequestsPerHostPerClient limit.
BASE_FEATURE(kDelayRequestsOnMultiplexedConnections,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When kPauseBrowserInitiatedHeavyTrafficForP2P is enabled, then a subset of
// the browser initiated traffic may be paused if there is at least one active
// P2P connection and the network is estimated to be congested. This feature is
// intended to throttle only the browser initiated traffic that is expected to
// be heavy (has large request/response sizes) when real time content might be
// streaming over an active P2P connection.
BASE_FEATURE(kPauseBrowserInitiatedHeavyTrafficForP2P,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When kProactivelyThrottleLowPriorityRequests is enabled,
// resource scheduler proactively throttles low priority requests to avoid
// network contention with high priority requests that may arrive soon.
BASE_FEATURE(kProactivelyThrottleLowPriorityRequests,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Cross-Origin Opener Policy (COOP).
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
// https://html.spec.whatwg.org/C/#cross-origin-opener-policy
// Currently this feature is enabled for all platforms except WebView.
BASE_FEATURE(kCrossOriginOpenerPolicy, base::FEATURE_ENABLED_BY_DEFAULT);

// Shift's COOP's default from `unsafe-none` to `same-origin-allow-popups`.
// https://github.com/mikewest/coop-by-default/
BASE_FEATURE(kCrossOriginOpenerPolicyByDefault,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the "noopener-allow-popups" COOP value, which lets a document to
// severe its opener relationship with the document that opened it.
// https://github.com/whatwg/html/pull/10394
BASE_FEATURE(kCoopNoopenerAllowPopups, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or defaults splittup up server (not proxy) entries in the
// HttpAuthCache.
BASE_FEATURE(kSplitAuthCacheByNetworkIsolationKey,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable usage of hardcoded DoH upgrade mapping for use in automatic mode.
BASE_FEATURE(kDnsOverHttpsUpgrade,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// When enabled, the requests in a third party context to domains included in
// the Masked Domain List Component will use the Privacy Proxy to shield the
// client's IP.
BASE_FEATURE(kMaskedDomainList, base::FEATURE_DISABLED_BY_DEFAULT);

// Used to build the MDL component's installer attributes and possibly control
// which release version is retrieved.
// Altering this value via Finch does not have any effect for WebView.
BASE_FEATURE_PARAM(std::string,
                   kMaskedDomainListExperimentalVersion,
                   &kMaskedDomainList,
                   /*name=*/"MaskedDomainListExperimentalVersion",
                   /*default_value=*/"");

// When enabled, the MaskedDomainList will be split into two separate lists,
// one for regular browsing and one for incognito browsing.
BASE_FEATURE_PARAM(bool,
                   kSplitMaskedDomainList,
                   &kMaskedDomainList,
                   /*name=*/"SplitMaskedDomainList",
                   /*default_value=*/false);

// If this feature is enabled, the mDNS responder service responds to queries
// for TXT records associated with
// "Generated-Names._mdns_name_generator._udp.local" with a list of generated
// mDNS names (random UUIDs) in the TXT record data.
BASE_FEATURE(kMdnsResponderGeneratedNameListing,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Treat ORB blocked responses to script-initiated fetches as errors too.
// Complements ORB v0.2, which exempts script-initiated fetches.
// Implementing ORB in Chromium is tracked in https://crbug.com/1178928
BASE_FEATURE(kOpaqueResponseBlockingErrorsForAllFetches,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable support for ACCEPT_CH H2/3 frame as part of Client Hint Reliability.
// See:
// https://tools.ietf.org/html/draft-davidben-http-client-hint-reliability-02#section-4.3
BASE_FEATURE(kAcceptCHFrame, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable offloading the network layer to check enabled client hints.
// See crbug.com/406407746 for details.
BASE_FEATURE(kOffloadAcceptCHFrameCheck, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the network service will consider not-allowed but persisted
// client hints as "enabled" for the purpose of the Accept-CH frame offload
// check.
// See crbug.com/406407746 for details.
BASE_FEATURE_PARAM(bool,
                   kAcceptCHFrameOffloadNotAllowedHints,
                   &kOffloadAcceptCHFrameCheck,
                   "AcceptCHFrameOffloadNotAllowedHints",
                   false);

// Enable offloading the network layer to check enabled client hints even when
// cross origin redirect happens.
// See crbug.com/406407746 for details.
BASE_FEATURE_PARAM(bool,
                   kAcceptCHOffloadWithRedirect,
                   &kOffloadAcceptCHFrameCheck,
                   "AcceptCHOffloadWithRedirect",
                   false);

// Enable offloading the network layer to check enabled client hints for
// subframe requests.
// See crbug.com/406407746 for details.
BASE_FEATURE_PARAM(bool,
                   kAcceptCHOffloadForSubframe,
                   &kOffloadAcceptCHFrameCheck,
                   "AcceptCHOffloadForSubframe",
                   false);

// https://fetch.spec.whatwg.org/#cors-non-wildcard-request-header-name
BASE_FEATURE(kCorsNonWildcardRequestHeadersSupport,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Do not send TLS client certificates in CORS preflight. Omit all client certs
// and continue the handshake without sending one if requested.
BASE_FEATURE(kOmitCorsClientCert, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables support for the `Variants` response header and reduce
// accept-language. https://github.com/Tanych/accept-language
BASE_FEATURE(kReduceAcceptLanguage, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kReduceAcceptLanguageCacheDuration,
                   &kReduceAcceptLanguage,
                   "reduce-accept-language-cache-duration",
                   base::Days(30));

// Enables support for the `Variants` response header and reduce
// Accept-Language HTTP header only.
BASE_FEATURE(kReduceAcceptLanguageHTTP, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled this feature will limit the total number of languages for
// Accept-Language to enhance privacy. See details in
// https://datatracker.ietf.org/doc/html/rfc7231#section-9.7.
BASE_FEATURE(kReduceAcceptLanguageCount, base::FEATURE_ENABLED_BY_DEFAULT);

// When non-zero, the number of Accept-Language of this size will return as the
// HTTP header and JavaScript getter.
BASE_FEATURE_PARAM(int,
                   kMaxAcceptLanguage,
                   &kReduceAcceptLanguageCount,
                   /*name=*/"MaxAcceptLanguage",
                   /*default_value=*/10);

// Enables Local Network Access checks.
// Blocks local network requests without user permission to prevent exploitation
// of vulnerable local devices.
//
// This feature is being built as a replacement for Private Network Access
// (PNA), and if this is on PNA features may stop working.
//
// Spec: https://wicg.github.io/local-network-access/
BASE_FEATURE(kLocalNetworkAccessChecks, base::FEATURE_ENABLED_BY_DEFAULT);

// If true, local network access checks will only be warnings.
BASE_FEATURE_PARAM(bool,
                   kLocalNetworkAccessChecksWarn,
                   &kLocalNetworkAccessChecks,
                   /*name=*/"LocalNetworkAccessChecksWarn",
                   /*default_value=*/false);

// Enables Local Network Access checks for WebRTC.
// Blocks local network requests without user permission to prevent exploitation
// of vulnerable local devices.
//
// Spec: https://wicg.github.io/local-network-access/
BASE_FEATURE(kLocalNetworkAccessChecksWebRTC,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If true, local network access checks will only apply for loopback addresses.
BASE_FEATURE_PARAM(bool,
                   kLocalNetworkAccessChecksWebRTCLoopbackOnly,
                   &kLocalNetworkAccessChecksWebRTC,
                   /*name=*/"LocalNetworkAccessChecksWebRTCLoopbackOnly",
                   /*default_value=*/false);

// Enables Local Network Access checks for WebSockets.
// Blocks local network requests without user permission to prevent exploitation
// of vulnerable local devices.
//
// Spec: https://wicg.github.io/local-network-access/
BASE_FEATURE(kLocalNetworkAccessChecksWebSockets,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Local Network Access checks for WebTransport.
// Blocks local network requests without user permission to prevent exploitation
// of vulnerable local devices.
//
// Spec: https://wicg.github.io/local-network-access/
BASE_FEATURE(kLocalNetworkAccessChecksWebTransport,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, then the network service will parse the Cookie-Indices header.
// This does not currently control changing cache behavior according to the
// value of this header.
BASE_FEATURE(kCookieIndicesHeader, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the compression dictionary transport feature.
// When this feature is enabled, the following will happen:
//   * The network service loads the metadata database.
//   * If there is a matching dictionary for a sending request, it adds the
//     `sec-available-dictionary` header.
//   * And if the `content-encoding` header of the response is `dcb`, it
//     decompresses the response body using the dictionary.
//   * A <link rel=compression-dictionary> HTML tag and a
//     `Link: rel=compression-dictionary` HTTP header will trigger dictionary
//     download.
//   * HTMLLinkElement.relList.supports('compression-dictionary') will return
//     true.
//   * The network service may register a HTTP response as a dictionary if the
//     response header contains a `use-as-dictionary` header.
BASE_FEATURE(kCompressionDictionaryTransport, base::FEATURE_ENABLED_BY_DEFAULT);

// When this feature is enabled, preloaded dictionaries will not be used for
// network requests if the binary has not yet been preloaded.
BASE_FEATURE(kPreloadedDictionaryConditionalUse,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables support for explicitly setting the compression dictionary expiration
// with the `ttl` parameter on the `use-as-dictionary` HTTP response header.
BASE_FEATURE(kCompressionDictionaryTTL, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables support for the `Integrity-Policy` header with script destinations,
// which enables developers to ensure all their external scripts have their
// integrity enforced.
BASE_FEATURE(kIntegrityPolicyScript, base::FEATURE_ENABLED_BY_DEFAULT);

// This feature will reduce TransferSizeUpdated IPC from the network service.
// When enabled, the network service will send the IPC only when DevTools is
// attached or the request is for an ad request.
BASE_FEATURE(kReduceTransferSizeUpdatedIPC, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables content decoding in the renderer process.
// See https://crbug.com/391950057 and this doc for more details.
// https://docs.google.com/document/d/1LwgPlrtQtUhGz_ilTsRun-7o4TuHo9jbXll6FRq-dKk/edit?usp=sharing
BASE_FEATURE(kRendererSideContentDecoding, base::FEATURE_DISABLED_BY_DEFAULT);
// When non-zero, a Mojo data pipe of this size will be used between the
// decoding thread and the data receiving thread.
BASE_FEATURE_PARAM(int,
                   kRendererSideContentDecodingPipeSize,
                   &kRendererSideContentDecoding,
                   /*name=*/"RendererSideContentDecodingPipeSize",
                   /*default_value=*/0);
// Enables renderer-side content decoding for navigations. This is a sub-feature
// of `kRendererSideContentDecoding` and has no effect if that feature is
// disabled.
BASE_FEATURE_PARAM(bool,
                   kRendererSideContentDecodingForNavigation,
                   &kRendererSideContentDecoding,
                   /*name=*/"RendererSideContentDecodingForNavigation",
                   /*default_value=*/true);
// For testing purposes only. If set to true, the creation of the Mojo data pipe
// for the RendererSideContentDecoding feature will be forced to fail,
// simulating an insufficient resources error (net::ERR_INSUFFICIENT_RESOURCES).
BASE_FEATURE_PARAM(
    bool,
    kRendererSideContentDecodingForceMojoFailureForTesting,
    &kRendererSideContentDecoding,
    /*name=*/"RendererSideContentDecodingForceMojoFailureForTesting",
    /*default_value=*/false);

// This feature allows skipping TPCD mitigation checks when the cookie access
// is tagged as being used for advertising purposes. This means that cookies
// will continue to be blocked for cookie accesses on ad requests even if the
// 3PC mitigations would otherwise allow the access.
BASE_FEATURE(kSkipTpcdMitigationsForAds, base::FEATURE_DISABLED_BY_DEFAULT);
// Controls whether we ignore opener heuristic grants for 3PC accesses.
BASE_FEATURE_PARAM(bool,
                   kSkipTpcdMitigationsForAdsHeuristics,
                   &kSkipTpcdMitigationsForAds,
                   /*name=*/"SkipTpcdMitigationsForAdsHeuristics",
                   /*default_value=*/false);
// Controls whether we ignore checks on the metadata allowlist for 3PC cookies.
BASE_FEATURE_PARAM(bool,
                   kSkipTpcdMitigationsForAdsMetadata,
                   &kSkipTpcdMitigationsForAds,
                   /*name=*/"SkipTpcdMitigationsForAdsMetadata",
                   /*default_value=*/false);
// Controls whether we ignore checks on the deprecation trial for 3PC.
BASE_FEATURE_PARAM(bool,
                   kSkipTpcdMitigationsForAdsTrial,
                   &kSkipTpcdMitigationsForAds,
                   /*name=*/"SkipTpcdMitigationsForAdsSupport",
                   /*default_value=*/false);
// Controls whether we ignore checks on the top-level deprecation trial for 3PC.
BASE_FEATURE_PARAM(bool,
                   kSkipTpcdMitigationsForAdsTopLevelTrial,
                   &kSkipTpcdMitigationsForAds,
                   /*name=*/"SkipTpcdMitigationsForAdsTopLevelTrial",
                   /*default_value=*/false);

// Avoids copying ResourceRequest when possible.
BASE_FEATURE(kAvoidResourceRequestCopies, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Document-Isolation-Policy (DIP).
// https://github.com/WICG/document-isolation-policy
BASE_FEATURE(kDocumentIsolationPolicy,
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kConnectionAllowlists, base::FEATURE_DISABLED_BY_DEFAULT);

// This feature enables the Prefetch() method on the NetworkContext, and makes
// the PrefetchMatchingURLLoaderFactory check the match quality.
BASE_FEATURE(kNetworkContextPrefetch, base::FEATURE_DISABLED_BY_DEFAULT);

// This feature makes the matching fetches performed by the Prefetch() actually
// be consumed directly by renderers. When this is disabled, the disk cache
// entry may be reused but the original URLLoader is cancelled. Does nothing
// unless "NetworkContextPrefetch" is also enabled.
BASE_FEATURE(kNetworkContextPrefetchUseMatches,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the CORS URL loader will clone the DevTools connection for a
// resource request only if the request includes a DevTools request id.
BASE_FEATURE(kCloneDevToolsConnectionOnlyIfRequested,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAdAuctionEventRegistration, base::FEATURE_DISABLED_BY_DEFAULT);

// See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
// Changes default Permissions Policy for features join-ad-interest-group and
// run-ad-auction to a more restricted EnableForSelf.
BASE_FEATURE(kAdInterestGroupAPIRestrictedPolicyByDefault,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables unload handler deprecation via Permissions-Policy.
// https://crbug.com/1324111
BASE_FEATURE(kDeprecateUnload, base::FEATURE_DISABLED_BY_DEFAULT);
// If < 100, each user experiences the deprecation on this % of origins.
// Which origins varies per user.
BASE_FEATURE_PARAM(int,
                   kDeprecateUnloadPercent,
                   &kDeprecateUnload,
                   "rollout_percent",
                   100);
// This buckets users, with users in each bucket having a consistent experience
// of the unload deprecation rollout.
BASE_FEATURE_PARAM(int,
                   kDeprecateUnloadBucket,
                   &kDeprecateUnload,
                   "rollout_bucket",
                   0);

// Only used if `kDeprecateUnload` is enabled. The deprecation will only apply
// if the host is on the allow-list.
BASE_FEATURE(kDeprecateUnloadByAllowList, base::FEATURE_DISABLED_BY_DEFAULT);
// A list of hosts for which deprecation of unload is allowed. If it's empty
// the all hosts are allowed.
BASE_FEATURE_PARAM(std::string,
                   kDeprecateUnloadAllowlist,
                   &kDeprecateUnloadByAllowList,
                   "allowlist",
                   "");
// When enabled, a `Sec-Fetch-Frame-Top` header will be emitted on
// outgoing requests.
BASE_FEATURE(kFrameTopHeader, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a `Sec-Fetch-Frame-Ancestors` header will be emitted on
// outgoing requests.
BASE_FEATURE(kFrameAncestorsHeader, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUpdateRequestForCorsRedirect, base::FEATURE_ENABLED_BY_DEFAULT);

// https://github.com/patcg-individual-drafts/topics
// Kill switch for the Topics API.
BASE_FEATURE(kBrowsingTopics, base::FEATURE_ENABLED_BY_DEFAULT);

// Enable the shared storage API. Note that enabling this feature does not
// automatically expose this API to the web, it only allows the element to be
// enabled by the runtime enabled feature, for origin trials.
// https://github.com/pythagoraskitty/shared-storage/blob/main/README.md
BASE_FEATURE(kSharedStorageAPI, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(size_t,
                   kSharedStorageURLSelectionOperationInputURLSizeLimit,
                   &kSharedStorageAPI,
                   "url_selection_operation_input_url_size_limit",
                   8);
BASE_FEATURE_PARAM(int,
                   kMaxSharedStoragePageSize,
                   &kSharedStorageAPI,
                   "MaxSharedStoragePageSize",
                   4096);
BASE_FEATURE_PARAM(int,
                   kMaxSharedStorageCacheSize,
                   &kSharedStorageAPI,
                   "MaxSharedStorageCacheSize",
                   1024);
BASE_FEATURE_PARAM(int,
                   kMaxSharedStorageInitTries,
                   &kSharedStorageAPI,
                   "MaxSharedStorageInitTries",
                   2);
BASE_FEATURE_PARAM(int,
                   kMaxSharedStorageIteratorBatchSize,
                   &kSharedStorageAPI,
                   "MaxSharedStorageIteratorBatchSize",
                   100);
BASE_FEATURE_PARAM(int,
                   kSharedStorageBitBudget,
                   &kSharedStorageAPI,
                   "SharedStorageBitBudget",
                   12);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSharedStorageBudgetInterval,
                   &kSharedStorageAPI,
                   "SharedStorageBudgetInterval",
                   base::Hours(24));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSharedStorageStalePurgeInitialInterval,
                   &kSharedStorageAPI,
                   "SharedStorageStalePurgeInitialInterval",
                   base::Minutes(2));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSharedStorageStalePurgeRecurringInterval,
                   &kSharedStorageAPI,
                   "SharedStorageStalePurgeRecurringInterval",
                   base::Hours(2));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSharedStorageStalenessThreshold,
                   &kSharedStorageAPI,
                   "SharedStorageStalenessThreshold",
                   base::Days(30));
BASE_FEATURE_PARAM(size_t,
                   kSharedStorageMaxAllowedFencedFrameDepthForSelectURL,
                   &kSharedStorageAPI,
                   "SharedStorageMaxAllowedFencedFrameDepthForSelectURL",
                   1);
// NOTE: To preserve user privacy, the
// `kSharedStorageExposeDebugMessageForSettingsStatus` feature param MUST remain
// false by default.
BASE_FEATURE_PARAM(bool,
                   kSharedStorageExposeDebugMessageForSettingsStatus,
                   &kSharedStorageAPI,
                   "ExposeDebugMessageForSettingsStatus",
                   false);

// Enables transactional behavior for sharedStorage.batchUpdate(). This also
// disallows the 'withLock' option for methods within batchUpdate().
// https://wicg.github.io/shared-storage/#batch-update
BASE_FEATURE(kSharedStorageTransactionalBatchUpdate,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for the Interest Group API, i.e. if disabled, the
// API exposure will be disabled regardless of the OT config.
BASE_FEATURE(kInterestGroupStorage, base::FEATURE_ENABLED_BY_DEFAULT);
// TODO(crbug.com/40176812): Adjust these limits in response to usage.
BASE_FEATURE_PARAM(int,
                   kInterestGroupStorageMaxOwners,
                   &kInterestGroupStorage,
                   "max_owners",
                   1000);
BASE_FEATURE_PARAM(int,
                   kInterestGroupStorageMaxStoragePerOwner,
                   &kInterestGroupStorage,
                   "max_storage_per_owner",
                   10 * 1024 * 1024);
BASE_FEATURE_PARAM(int,
                   kInterestGroupStorageMaxGroupsPerOwner,
                   &kInterestGroupStorage,
                   "max_groups_per_owner",
                   2000);
BASE_FEATURE_PARAM(int,
                   kInterestGroupStorageMaxNegativeGroupsPerOwner,
                   &kInterestGroupStorage,
                   "max_negative_groups_per_owner",
                   20000);
BASE_FEATURE_PARAM(int,
                   kInterestGroupStorageMaxOpsBeforeMaintenance,
                   &kInterestGroupStorage,
                   "max_ops_before_maintenance",
                   1000);

BASE_FEATURE(kGetCookiesOnSet, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIncreaseCookieAccessCacheSize, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kCookieAccessCacheSize,
                   &kIncreaseCookieAccessCacheSize,
                   "cookie-access-cache-size",
                   100);

BASE_FEATURE(kPopulatePermissionsPolicyOnRequest,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kProtectedAudienceCorsSafelistKVv2Signals,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStorageAccessHeadersRespectPermissionsPolicy,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeviceBoundSessionAccessObserverSharedRemote,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCSPScriptSrcV2, "ScriptSrcV2", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCSPScriptSrcHashesInV1,
             "ScriptSrcHashesV1",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCacheSharingForPervasiveScripts,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendSameSiteLaxForFedCM, base::FEATURE_DISABLED_BY_DEFAULT);

// This is a newline-delimited list of pervasive script URL Patterns.
BASE_FEATURE_PARAM(std::string,
                   kPervasiveScriptURLPatterns,
                   &kCacheSharingForPervasiveScripts,
                   /*name=*/"url_patterns",
                   /*default_value=*/"");

BASE_FEATURE(kSharedDictionaryCache, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(size_t,
                   kSharedDictionaryCacheSize,
                   &kSharedDictionaryCache,
                   /*name=*/"cache_size",
                   2);
BASE_FEATURE_PARAM(size_t,
                   kSharedDictionaryCacheMaxSizeBytes,
                   &kSharedDictionaryCache,
                   /*name=*/"max_size",
                   1'000'000);

BASE_FEATURE(kNetworkServiceTaskScheduler, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kNetworkServiceTaskSchedulerResourceScheduler,
                   &kNetworkServiceTaskScheduler,
                   "resource_scheduler",
                   true);
BASE_FEATURE_PARAM(bool,
                   kNetworkServiceTaskSchedulerURLLoader,
                   &kNetworkServiceTaskScheduler,
                   "url_loader",
                   true);

BASE_FEATURE(kNetworkServicePerPriorityTaskQueues,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseUnexportableKeyServiceInBrowserProcess,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace network::features
