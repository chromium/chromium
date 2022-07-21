// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/mime_sniffer.h"

namespace network {
namespace features {

// Enables Expect CT reporting, which sends reports for opted-in sites
// that don't serve sufficient Certificate Transparency information.
const base::Feature kExpectCTReporting{"ExpectCTReporting",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNetworkErrorLogging{"NetworkErrorLogging",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kReporting{"Reporting", base::FEATURE_ENABLED_BY_DEFAULT};

// Based on the field trial parameters, this feature will override the value of
// the maximum number of delayable requests allowed in flight. The number of
// delayable requests allowed in flight will be based on the network's
// effective connection type ranges and the
// corresponding number of delayable requests in flight specified in the
// experiment configuration. Based on field trial parameters, this experiment
// may also throttle delayable requests based on the number of non-delayable
// requests in-flight times a weighting factor.
const base::Feature kThrottleDelayable{"ThrottleDelayable",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// When kPriorityRequestsDelayableOnSlowConnections is enabled, HTTP
// requests fetched from a SPDY/QUIC/H2 proxies can be delayed by the
// ResourceScheduler just as HTTP/1.1 resources are. However, requests from such
// servers are not subject to kMaxNumDelayableRequestsPerHostPerClient limit.
const base::Feature kDelayRequestsOnMultiplexedConnections{
    "DelayRequestsOnMultiplexedConnections", base::FEATURE_ENABLED_BY_DEFAULT};

// When kPauseBrowserInitiatedHeavyTrafficForP2P is enabled, then a subset of
// the browser initiated traffic may be paused if there is at least one active
// P2P connection and the network is estimated to be congested. This feature is
// intended to throttle only the browser initiated traffic that is expected to
// be heavy (has large request/response sizes) when real time content might be
// streaming over an active P2P connection.
const base::Feature kPauseBrowserInitiatedHeavyTrafficForP2P{
    "PauseBrowserInitiatedHeavyTrafficForP2P",
    base::FEATURE_ENABLED_BY_DEFAULT};

// When kCORBProtectionSniffing is enabled CORB sniffs additional same-origin
// resources if they look sensitive.
const base::Feature kCORBProtectionSniffing{"CORBProtectionSniffing",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// When kProactivelyThrottleLowPriorityRequests is enabled,
// resource scheduler proactively throttles low priority requests to avoid
// network contention with high priority requests that may arrive soon.
const base::Feature kProactivelyThrottleLowPriorityRequests{
    "ProactivelyThrottleLowPriorityRequests",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Cross-Origin Opener Policy (COOP).
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
// https://html.spec.whatwg.org/C/#cross-origin-opener-policy
// Currently this feature is enabled for all platforms except WebView.
const base::Feature kCrossOriginOpenerPolicy{"CrossOriginOpenerPolicy",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Shift's COOP's default from `unsafe-none` to `same-origin-allow-popups`.
// https://github.com/mikewest/coop-by-default/
const base::Feature kCrossOriginOpenerPolicyByDefault{
    "CrossOriginOpenerPolicyByDefault", base::FEATURE_DISABLED_BY_DEFAULT};

// Introduce a new COOP value: restrict-properties. It restricts window
// properties that can be accessed by other pages. This also grants
// crossOriginIsolated if coupled with an appropriate COEP header.
// This used solely for testing the process model and should not be enabled in
// any production code. See https://crbug.com/1221127.
const base::Feature kCoopRestrictProperties{"CoopRestrictProperties",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or defaults splittup up server (not proxy) entries in the
// HttpAuthCache.
const base::Feature kSplitAuthCacheByNetworkIsolationKey{
    "SplitAuthCacheByNetworkIsolationKey", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable usage of hardcoded DoH upgrade mapping for use in automatic mode.
const base::Feature kDnsOverHttpsUpgrade {
  "DnsOverHttpsUpgrade",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// If this feature is enabled, the mDNS responder service responds to queries
// for TXT records associated with
// "Generated-Names._mdns_name_generator._udp.local" with a list of generated
// mDNS names (random UUIDs) in the TXT record data.
const base::Feature kMdnsResponderGeneratedNameListing{
    "MdnsResponderGeneratedNameListing", base::FEATURE_DISABLED_BY_DEFAULT};

// Switches Cross-Origin Read Blocking (CORB) to use an early implementation of
// Opaque Response Blocking (ORB, aka CORB++) behind the scenes.
//
// This is ORB v0.1 - it doesn't implement the full spec from
// https://github.com/annevk/orb:
// - No Javascript sniffing is done.  Instead the implementation uses all
//   of CORB's confirmation sniffers (for HTML, XML and JSON).
// - Blocking is still done by injecting an empty response rather than erroring
//   out the network request
// - Other differences and more details can be found in
//   //services/network/public/cpp/corb/README.md
//
// Implementing ORB in Chromium is tracked in https://crbug.com/1178928
const base::Feature kOpaqueResponseBlockingV01{
    "OpaqueResponseBlockingV01", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables preprocessing requests with the Trust Tokens API Fetch flags set,
// and handling their responses, according to the protocol.
// (See https://github.com/WICG/trust-token-api.)
const base::Feature kTrustTokens{"TrustTokens",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

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
        &kTrustTokens, "TrustTokenOperationsRequiringOriginTrial",
        TrustTokenOriginTrialSpec::kOriginTrialNotRequired,
        &kTrustTokenOriginTrialParamOptions};

// Determines whether Trust Tokens issuance requests should be diverted, at the
// corresponding issuers' request, to the operating system instead of sent
// to the issuers' servers.
//
// WARNING: If you rename this param, you must update the corresponding flag
// entry in about_flags.cc.
const base::FeatureParam<bool> kPlatformProvidedTrustTokenIssuance{
    &kTrustTokens, "PlatformProvidedTrustTokenIssuance", false};

const base::Feature kWebSocketReassembleShortMessages{
    "WebSocketReassembleShortMessages", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable support for ACCEPT_CH H2/3 frame as part of Client Hint Reliability.
// See:
// https://tools.ietf.org/html/draft-davidben-http-client-hint-reliability-02#section-4.3
const base::Feature kAcceptCHFrame{"AcceptCHFrame",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSCTAuditingRetryReports{"SCTAuditingRetryReports",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSCTAuditingPersistReports{
    "SCTAuditingPersistReports", base::FEATURE_DISABLED_BY_DEFAULT};

namespace {
// The default Mojo ring buffer size, used to send the content body.
static constexpr uint32_t kDefaultDataPipeAllocationSize = 512 * 1024;
// The larger ring buffer size, used primarily for network::URLLoader loads.
// This value was optimized via Finch: see crbug.com/1041006.
static constexpr uint32_t kLargerDataPipeAllocationSize = 2 * 1024 * 1024;

// The maximal number of bytes consumed in a loading task. When there are more
// bytes in the data pipe, they will be consumed in following tasks. Setting too
// small of a number will generate many tasks but setting a too large of a
// number will lead to thread janks. This value was optimized via Finch:
// see crbug.com/1041006.
static constexpr uint32_t kMaxNumConsumedBytesInTask = 1024 * 1024;

// The smallest buffer size must be larger than the maximum MIME sniffing
// chunk size. This is assumed several places in content/browser/loader.
static_assert(kDefaultDataPipeAllocationSize < kLargerDataPipeAllocationSize);
static_assert(kDefaultDataPipeAllocationSize >= net::kMaxBytesToSniff,
              "Smallest data pipe size must be at least as large as a "
              "MIME-type sniffing buffer.");
}  // namespace

// static
uint32_t GetDataPipeDefaultAllocationSize(DataPipeAllocationSize option) {
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/1306998): ChromeOS experiences a much higher OOM crash
  // rate if the larger data pipe size is used.
  return kDefaultDataPipeAllocationSize;
#else
  // For low-memory devices, always use the (smaller) default buffer size.
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 512)
    return kDefaultDataPipeAllocationSize;
  switch (option) {
    case DataPipeAllocationSize::kDefaultSizeOnly:
      return kDefaultDataPipeAllocationSize;
    case DataPipeAllocationSize::kLargerSizeIfPossible:
      return kLargerDataPipeAllocationSize;
  }
#endif
}

// static
uint32_t GetLoaderChunkSize() {
  return kMaxNumConsumedBytesInTask;
}

// Check disk cache to see if the queued requests (especially those don't need
// validation) have already been cached. If yes, start them as they may not
// contend for network.
const base::Feature kCheckCacheForQueuedRequests{
    "CheckCacheForQueuedRequests", base::FEATURE_DISABLED_BY_DEFAULT};

// The time interval before checking the cache for queued request.
constexpr base::FeatureParam<base::TimeDelta> kQueuedRequestsCacheCheckInterval{
    &kCheckCacheForQueuedRequests, "queued_requests_cache_check_interval",
    base::Milliseconds(100)};

// Cache check is only valid for requests queued for long than this threshold.
constexpr base::FeatureParam<base::TimeDelta>
    kQueuedRequestsCacheCheckTimeThreshold{
        &kCheckCacheForQueuedRequests,
        "queued_requests_cache_check_time_threshold", base::Milliseconds(100)};

// https://fetch.spec.whatwg.org/#cors-non-wildcard-request-header-name
const base::Feature kCorsNonWildcardRequestHeadersSupport{
    "CorsNonWildcardRequestHeadersSupport", base::FEATURE_DISABLED_BY_DEFAULT};

// Whether the sync client optimization is used for communication between the
// CorsURLLoader and URLLoader.
const base::Feature kURLLoaderSyncClient{"URLLoaderSyncClient",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Don't wait for database write before responding to
// RestrictedCookieManager::SetCookieFromString.
const base::Feature kFasterSetCookie{"FasterSetCookie",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Allow batching SimpleURLLoaders when the underlying network state is
// inactive.
const base::Feature kBatchSimpleURLLoader{"BatchSimpleURLLoader",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNetworkServiceMemoryCache{
    "NetworkServiceMemoryCache", base::FEATURE_DISABLED_BY_DEFAULT};

// Do not send TLS client certificates in CORS preflight. Omit all client certs
// and continue the handshake without sending one if requested.
const base::Feature kOmitCorsClientCert{"OmitCorsClientCert",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Allow pervasive payloads to use a single-keyed cache.
const base::Feature kCacheTransparency{"CacheTransparency",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Load Pervasive Payloads List for Cache Transparency.
const base::Feature kPervasivePayloadsList{"PervasivePayloadsList",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// The list of pervasive payloads. A comma separated list starting with a
// version number, followed one or more pairs of URL and checksum. The version
// number is an integer. The URL is the canonical URL as returned by
// GURL::spec(). The checksum is the SHA-256 of the payload and selected headers
// converted to uppercase hexadecimal.
constexpr base::FeatureParam<std::string> kCacheTransparencyPervasivePayloads{
    &kPervasivePayloadsList, "pervasive-payloads", ""};

// Enables support for the `Variants` response header and reduce
// accept-language. https://github.com/Tanych/accept-language
const base::Feature kReduceAcceptLanguage{"ReduceAcceptLanguage",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Disable ResourceScheduler.
const base::Feature kDisableResourceScheduler{
    "DisableResourceScheduler", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace network
