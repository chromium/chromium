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
#include "build/chromeos_buildflags.h"
#include "components/miracle_parameter/common/public/miracle_parameter.h"
#include "net/base/mime_sniffer.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network::features {

// Enables the Accept-CH support disabler. If this feature is activated, Chrome
// ignore Accept-CH response headers for a site that is specified in the
// following kBlockAcceptClientHintsBlockedSite. This is used to compare Chrome
// performance with a dedicated site.
BASE_FEATURE(kBlockAcceptClientHints,
             "BlockAcceptClientHints",
             base::FEATURE_DISABLED_BY_DEFAULT);
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

BASE_FEATURE(kNetworkErrorLogging,
             "NetworkErrorLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReporting, "Reporting", base::FEATURE_ENABLED_BY_DEFAULT);

// Based on the field trial parameters, this feature will override the value of
// the maximum number of delayable requests allowed in flight. The number of
// delayable requests allowed in flight will be based on the network's
// effective connection type ranges and the
// corresponding number of delayable requests in flight specified in the
// experiment configuration. Based on field trial parameters, this experiment
// may also throttle delayable requests based on the number of non-delayable
// requests in-flight times a weighting factor.
BASE_FEATURE(kThrottleDelayable,
             "ThrottleDelayable",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When kPriorityRequestsDelayableOnSlowConnections is enabled, HTTP
// requests fetched from a SPDY/QUIC/H2 proxies can be delayed by the
// ResourceScheduler just as HTTP/1.1 resources are. However, requests from such
// servers are not subject to kMaxNumDelayableRequestsPerHostPerClient limit.
BASE_FEATURE(kDelayRequestsOnMultiplexedConnections,
             "DelayRequestsOnMultiplexedConnections",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When kPauseBrowserInitiatedHeavyTrafficForP2P is enabled, then a subset of
// the browser initiated traffic may be paused if there is at least one active
// P2P connection and the network is estimated to be congested. This feature is
// intended to throttle only the browser initiated traffic that is expected to
// be heavy (has large request/response sizes) when real time content might be
// streaming over an active P2P connection.
BASE_FEATURE(kPauseBrowserInitiatedHeavyTrafficForP2P,
             "PauseBrowserInitiatedHeavyTrafficForP2P",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When kProactivelyThrottleLowPriorityRequests is enabled,
// resource scheduler proactively throttles low priority requests to avoid
// network contention with high priority requests that may arrive soon.
BASE_FEATURE(kProactivelyThrottleLowPriorityRequests,
             "ProactivelyThrottleLowPriorityRequests",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Cross-Origin Opener Policy (COOP).
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
// https://html.spec.whatwg.org/C/#cross-origin-opener-policy
// Currently this feature is enabled for all platforms except WebView.
BASE_FEATURE(kCrossOriginOpenerPolicy,
             "CrossOriginOpenerPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Shift's COOP's default from `unsafe-none` to `same-origin-allow-popups`.
// https://github.com/mikewest/coop-by-default/
BASE_FEATURE(kCrossOriginOpenerPolicyByDefault,
             "CrossOriginOpenerPolicyByDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the "noopener-allow-popups" COOP value, which lets a document to
// severe its opener relationship with the document that opened it.
// https://github.com/whatwg/html/pull/10394
BASE_FEATURE(kCoopNoopenerAllowPopups,
             "CoopNoopenerAllowPopups",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Introduce a new COOP value: restrict-properties. It restricts window
// properties that can be accessed by other pages. This also grants
// crossOriginIsolated if coupled with an appropriate COEP header.
// This used solely for testing the process model and should not be enabled in
// any production code. See https://crbug.com/1221127.
BASE_FEATURE(kCoopRestrictProperties,
             "CoopRestrictProperties",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the origin trial for COOP: restrict-properties. We need a new feature
// because token validation is not possible in the network process. This also
// allows us to keep using CoopRestrictProperties to enable COOP: RP for WPTs.
BASE_FEATURE(kCoopRestrictPropertiesOriginTrial,
             "CoopRestrictPropertiesOriginTrial",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables or defaults splittup up server (not proxy) entries in the
// HttpAuthCache.
BASE_FEATURE(kSplitAuthCacheByNetworkIsolationKey,
             "SplitAuthCacheByNetworkIsolationKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable usage of hardcoded DoH upgrade mapping for use in automatic mode.
BASE_FEATURE(kDnsOverHttpsUpgrade,
             "DnsOverHttpsUpgrade",
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
BASE_FEATURE(kMaskedDomainList,
             "MaskedDomainList",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When set, only resources in the MDL that are part of the experiment group
// will be loaded into the proxy's allow list.
const base::FeatureParam<int> kMaskedDomainListExperimentGroup{
    &kMaskedDomainList, /*name=*/"MaskedDomainListExperimentGroup",
    /*default_value=*/0};

// Used to build the MDL component's installer attributes and possibly control
// which release version is retrieved.
// Altering this value via Finch does not have any effect for WebView.
const base::FeatureParam<std::string> kMaskedDomainListExperimentalVersion{
    &kMaskedDomainList, /*name=*/"MaskedDomainListExperimentalVersion",
    /*default_value=*/""};

// If this feature is enabled, the mDNS responder service responds to queries
// for TXT records associated with
// "Generated-Names._mdns_name_generator._udp.local" with a list of generated
// mDNS names (random UUIDs) in the TXT record data.
BASE_FEATURE(kMdnsResponderGeneratedNameListing,
             "MdnsResponderGeneratedNameListing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Treat ORB blocked responses to script-initiated fetches as errors too.
// Complements ORB v0.2, which exempts script-initiated fetches.
// Implementing ORB in Chromium is tracked in https://crbug.com/1178928
BASE_FEATURE(kOpaqueResponseBlockingErrorsForAllFetches,
             "OpaqueResponseBlockingErrorsForAllFetches",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Gate access to Attribution Reporting cross app and web APIs that allow
// registering with a native attribution API.
BASE_FEATURE(kAttributionReportingCrossAppWeb,
             "AttributionReportingCrossAppWeb",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables preprocessing requests with the Private State Tokens API Fetch flags
// set, and handling their responses, according to the protocol.
// (See https://github.com/WICG/trust-token-api.)
BASE_FEATURE(kPrivateStateTokens,
             "PrivateStateTokens",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Secondary flag used by the FLEDGE ads experiment in the interim before
// PSTs are fully rolled out to stable.
BASE_FEATURE(kFledgePst, "TrustTokens", base::FEATURE_ENABLED_BY_DEFAULT);

// Determines which Trust Tokens operations require the TrustTokens origin trial
// active in order to be used. This is runtime-configurable so that the Trust
// Tokens operations of issuance, redemption, and signing are compatible with
// both standard origin trials and third-party origin trials:
//
// - For standard origin trials, set kOnlyIssuanceRequiresOriginTrial. In Blink,
// all of the interface will be enabled (so long as the base::Feature is!), and
// issuance operations will check at runtime if the origin trial is enabled,
// returning an error if it is not.
// - For third-party origin trials, set kAllOperationsRequireOriginTrial. In
// Blink, the interface will be enabled exactly when the origin trial is present
// in the executing context (so long as the base::Feature is present).
//
// For testing, set kOriginTrialNotRequired. With this option, although all
// operations will still only be available if the base::Feature is enabled, none
// will additionally require that the origin trial be active.
const base::FeatureParam<TrustTokenOriginTrialSpec>::Option
    kTrustTokenOriginTrialParamOptions[] = {
        {TrustTokenOriginTrialSpec::kOriginTrialNotRequired,
         "origin-trial-not-required"},
        {TrustTokenOriginTrialSpec::kAllOperationsRequireOriginTrial,
         "all-operations-require-origin-trial"},
        {TrustTokenOriginTrialSpec::kOnlyIssuanceRequiresOriginTrial,
         "only-issuance-requires-origin-trial"}};
const base::FeatureParam<TrustTokenOriginTrialSpec>
    kTrustTokenOperationsRequiringOriginTrial{
        &kFledgePst, "TrustTokenOperationsRequiringOriginTrial",
        TrustTokenOriginTrialSpec::kOriginTrialNotRequired,
        &kTrustTokenOriginTrialParamOptions};

// Enable support for ACCEPT_CH H2/3 frame as part of Client Hint Reliability.
// See:
// https://tools.ietf.org/html/draft-davidben-http-client-hint-reliability-02#section-4.3
BASE_FEATURE(kAcceptCHFrame, "AcceptCHFrame", base::FEATURE_ENABLED_BY_DEFAULT);

// Enable
BASE_FEATURE(kGetCookiesStringUma,
             "GetCookiesStringUma",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

BASE_FEATURE(kDefaultDataPipeAllocationSizeFeature,
             "DefaultDataPipeAllocationSizeFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLargerDataPipeAllocationSizeFeature,
             "LargerDataPipeAllocationSizeFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNetAdapterMaxBufSizeFeature,
             "NetAdapterMaxBufSizeFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMaxNumConsumedBytesInTaskFeature,
             "MaxNumConsumedBytesInTaskFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The default Mojo ring buffer size, used to send the content body.
MIRACLE_PARAMETER_FOR_INT(GetDefaultDataPipeAllocationSize,
                          kDefaultDataPipeAllocationSizeFeature,
                          "DefaultDataPipeAllocationSize",
                          512 * 1024)

// The larger ring buffer size, used primarily for network::URLLoader loads.
// This value was optimized via Finch: see crbug.com/1041006.
MIRACLE_PARAMETER_FOR_INT(GetLargerDataPipeAllocationSize,
                          kLargerDataPipeAllocationSizeFeature,
                          "LargerDataPipeAllocationSize",
                          2 * 1024 * 1024)

// The max buffer size of NetToMojoPendingBuffer. This buffer size should be
// smaller than the mojo ring buffer size.
MIRACLE_PARAMETER_FOR_INT(GetNetAdapterMaxBufSizeParam,
                          kNetAdapterMaxBufSizeFeature,
                          "NetAdapterMaxBufSize",
                          64 * 1024)

// The maximal number of bytes consumed in a loading task. When there are more
// bytes in the data pipe, they will be consumed in following tasks. Setting too
// small of a number will generate many tasks but setting a too large of a
// number will lead to thread janks. This value was optimized via Finch:
// see crbug.com/1041006.
MIRACLE_PARAMETER_FOR_INT(GetMaxNumConsumedBytesInTask,
                          kMaxNumConsumedBytesInTaskFeature,
                          "MaxNumConsumedBytesInTask",
                          1024 * 1024)

}  // namespace

// static
uint32_t GetDataPipeDefaultAllocationSize(DataPipeAllocationSize option) {
  // The smallest buffer size must be larger than the maximum MIME sniffing
  // chunk size. This is assumed several places in content/browser/loader.
  CHECK_LE(GetDefaultDataPipeAllocationSize(),
           GetLargerDataPipeAllocationSize());
  CHECK_GE(GetDefaultDataPipeAllocationSize(), net::kMaxBytesToSniff)
      << "Smallest data pipe size must be at least as large as a "
         "MIME-type sniffing buffer.";

#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/1306998): ChromeOS experiences a much higher OOM crash
  // rate if the larger data pipe size is used.
  return GetDefaultDataPipeAllocationSize();
#else
  // For low-memory devices, always use the (smaller) default buffer size.
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 512)
    return GetDefaultDataPipeAllocationSize();
  switch (option) {
    case DataPipeAllocationSize::kDefaultSizeOnly:
      return GetDefaultDataPipeAllocationSize();
    case DataPipeAllocationSize::kLargerSizeIfPossible:
      return GetLargerDataPipeAllocationSize();
  }
#endif
}

size_t GetNetAdapterMaxBufSize() {
  return GetNetAdapterMaxBufSizeParam();
}

// static
size_t GetLoaderChunkSize() {
  return GetMaxNumConsumedBytesInTask();
}

// https://fetch.spec.whatwg.org/#cors-non-wildcard-request-header-name
BASE_FEATURE(kCorsNonWildcardRequestHeadersSupport,
             "CorsNonWildcardRequestHeadersSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Do not send TLS client certificates in CORS preflight. Omit all client certs
// and continue the handshake without sending one if requested.
BASE_FEATURE(kOmitCorsClientCert,
             "OmitCorsClientCert",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Load Pervasive Payloads List for Cache Transparency.
BASE_FEATURE(kPervasivePayloadsList,
             "PervasivePayloadsList",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The list of pervasive payloads. A comma separated list starting with a
// version number, followed one or more pairs of URL and checksum. The version
// number is an integer. The URL is the canonical URL as returned by
// GURL::spec(). The checksum is the SHA-256 of the payload and selected headers
// converted to uppercase hexadecimal.
constexpr base::FeatureParam<std::string> kCacheTransparencyPervasivePayloads{
    &kPervasivePayloadsList, "pervasive-payloads", ""};

// Enables support for the `Variants` response header and reduce
// accept-language. https://github.com/Tanych/accept-language
BASE_FEATURE(kReduceAcceptLanguage,
             "ReduceAcceptLanguage",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kReduceAcceptLanguageCacheDuration{
    &kReduceAcceptLanguage, "reduce-accept-language-cache-duration",
    base::Days(30)};

// Reduce PNA preflight response waiting time to 200ms.
// See: https://wicg.github.io/private-network-access/#cors-preflight
BASE_FEATURE(kPrivateNetworkAccessPreflightShortTimeout,
             "PrivateNetworkAccessPreflightShortTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allow potentially trustworthy same origin local network requests without
// preflights.
BASE_FEATURE(kLocalNetworkAccessAllowPotentiallyTrustworthySameOrigin,
             "LocalNetworkAccessAllowPotentiallyTrustworthySameOrigin",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When kPrivateNetworkAccessPermissionPrompt is enabled, public secure websites
// are allowed to access private insecure subresources with user's permission.
BASE_FEATURE(kPrivateNetworkAccessPermissionPrompt,
             "PrivateNetworkAccessPermissionPrompt",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAccessControlAllowMethodsInCORSPreflightSpecConformant,
             "AccessControlAllowMethodsInCORSPreflightSpecConformant",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, then the network service will parse the Cookie-Indices header.
// This does not currently control changing cache behavior according to the
// value of this header.
BASE_FEATURE(kCookieIndicesHeader,
             "CookieIndicesHeader",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the backend of the compression dictionary transport feature.
// When this feature is enabled, the following will happen:
//   * The network service loads the metadata database.
//   * If there is a matching dictionary for a sending request, it adds the
//     `sec-available-dictionary` header.
//   * And if the `content-encoding` header of the response is `dcb`, it
//     decompresses the response body using the dictionary.
BASE_FEATURE(kCompressionDictionaryTransportBackend,
             "CompressionDictionaryTransportBackend",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When both this feature and the kCompressionDictionaryTransportBackend feature
// are enabled, the following will happen:
//   * A <link rel=compression-dictionary> HTML tag and a
//     `Link: rel=compression-dictionary` HTTP header will trigger dictionary
//     download.
//   * HTMLLinkElement.relList.supports('compression-dictionary') will return
//     true.
//   * The network service may register a HTTP response as a dictionary if the
//     response header contains a `use-as-dictionary` header.
// This feature can be enabled by an Origin Trial token in Blink. To propagate
// the enabled state to the network service, Blink sets the
// `shared_dictionary_writer_enabled` flag in resource requests.
BASE_FEATURE(kCompressionDictionaryTransport,
             "CompressionDictionaryTransport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When this feature is enabled, preloaded dictionaries will not be used for
// network requests if the binary has not yet been preloaded.
BASE_FEATURE(kPreloadedDictionaryConditionalUse,
             "PreloadedDictionaryConditionalUse",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVisibilityAwareResourceScheduler,
             "VisibilityAwareResourceScheduler",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSharedZstd, "SharedZstd", base::FEATURE_ENABLED_BY_DEFAULT);

// This feature will reduce TransferSizeUpdated IPC from the network service.
// When enabled, the network service will send the IPC only when DevTools is
// attached or the request is for an ad request.
BASE_FEATURE(kReduceTransferSizeUpdatedIPC,
             "ReduceTransferSizeUpdatedIPC",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature allows skipping TPCD mitigation checks when the cookie access
// is tagged as being used for advertising purposes. This means that cookies
// will continue to be blocked for cookie accesses on ad requests even if the
// 3PC mitigations would otherwise allow the access.
BASE_FEATURE(kSkipTpcdMitigationsForAds,
             "SkipTpcdMitigationsForAds",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Controls whether we ignore opener heuristic grants for 3PC accesses.
const base::FeatureParam<bool> kSkipTpcdMitigationsForAdsHeuristics{
    &kSkipTpcdMitigationsForAds, /*name=*/"SkipTpcdMitigationsForAdsHeuristics",
    /*default_value=*/false};
// Controls whether we ignore checks on the metadata allowlist for 3PC cookies.
const base::FeatureParam<bool> kSkipTpcdMitigationsForAdsMetadata{
    &kSkipTpcdMitigationsForAds, /*name=*/"SkipTpcdMitigationsForAdsMetadata",
    /*default_value=*/false};
// Controls whether we ignore checks on the deprecation trial for 3PC.
const base::FeatureParam<bool> kSkipTpcdMitigationsForAdsTrial{
    &kSkipTpcdMitigationsForAds, /*name=*/"SkipTpcdMitigationsForAdsSupport",
    /*default_value=*/false};
// Controls whether we ignore checks on the top-level deprecation trial for 3PC.
const base::FeatureParam<bool> kSkipTpcdMitigationsForAdsTopLevelTrial{
    &kSkipTpcdMitigationsForAds,
    /*name=*/"SkipTpcdMitigationsForAdsTopLevelTrial",
    /*default_value=*/false};

// Avoids copying ResourceRequest when possible.
BASE_FEATURE(kAvoidResourceRequestCopies,
             "AvoidResourceRequestCopies",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Document-Isolation-Policy (DIP).
// https://github.com/explainers-by-googlers/document-isolation-policy
BASE_FEATURE(kDocumentIsolationPolicy,
             "DocumentIsolationPolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature enables the Prefetch() method on the NetworkContext, and the
// PrefetchMatchingURLLoaderFactory.
BASE_FEATURE(kNetworkContextPrefetch,
             "NetworkContextPrefetch",
             base::FEATURE_DISABLED_BY_DEFAULT);
// How many prefetches should be cached before old ones are evicted. This
// provides rough control over the overall memory used by prefetches.
const base::FeatureParam<int> kNetworkContextPrefetchMaxLoaders{
    &kNetworkContextPrefetch,
    /*name=*/"max_loaders", /*default_value=*/10};

// This feature enables treating 0.0.0.0/8 as the public address space instead
// of private or local. This is a killswitch for a tightening of a loophole in
// Private Network Access. See https://crbug.com/40058874.
BASE_FEATURE(kTreatNullIPAsPublicAddressSpace,
             "TreatNullIPAsPublicAddressSpace",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the CORS URL loader will clone the DevTools connection for a
// resource request only if the request includes a DevTools request id.
BASE_FEATURE(kCloneDevToolsConnectionOnlyIfRequested,
             "CloneDevToolsConnectionOnlyIfRequested",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStorageAccessHeaders,
             "StorageAccessHeaders",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStorageAccessHeadersTrial,
             "StorageAccessHeadersTrial",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace network::features
