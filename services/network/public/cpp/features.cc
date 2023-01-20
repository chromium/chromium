// Copyright 2018 The Chromium Authors
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

namespace network::features {

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

// When kCORBProtectionSniffing is enabled CORB sniffs additional same-origin
// resources if they look sensitive.
BASE_FEATURE(kCORBProtectionSniffing,
             "CORBProtectionSniffing",
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

// Introduce a new COOP value: restrict-properties. It restricts window
// properties that can be accessed by other pages. This also grants
// crossOriginIsolated if coupled with an appropriate COEP header.
// This used solely for testing the process model and should not be enabled in
// any production code. See https://crbug.com/1221127.
BASE_FEATURE(kCoopRestrictProperties,
             "CoopRestrictProperties",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// If this feature is enabled, the mDNS responder service responds to queries
// for TXT records associated with
// "Generated-Names._mdns_name_generator._udp.local" with a list of generated
// mDNS names (random UUIDs) in the TXT record data.
BASE_FEATURE(kMdnsResponderGeneratedNameListing,
             "MdnsResponderGeneratedNameListing",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE(kOpaqueResponseBlockingV01,
             "OpaqueResponseBlockingV01",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables ORB blocked responses being treated as errors (according to the spec)
// rather than the current, CORB-style handling of injecting an empty response.
// This is ORB v0.2.
// This should only be enabled when ORB v0.1 is, too.
BASE_FEATURE(kOpaqueResponseBlockingV02,
             "OpaqueResponseBlockingV02",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables preprocessing requests with the Private State Tokens API Fetch flags
// set, and handling their responses, according to the protocol.
// (See https://github.com/WICG/trust-token-api.)
BASE_FEATURE(kPrivateStateTokens,
             "TrustTokens",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
        &kPrivateStateTokens, "TrustTokenOperationsRequiringOriginTrial",
        TrustTokenOriginTrialSpec::kOriginTrialNotRequired,
        &kTrustTokenOriginTrialParamOptions};

BASE_FEATURE(kWebSocketReassembleShortMessages,
             "WebSocketReassembleShortMessages",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable support for ACCEPT_CH H2/3 frame as part of Client Hint Reliability.
// See:
// https://tools.ietf.org/html/draft-davidben-http-client-hint-reliability-02#section-4.3
BASE_FEATURE(kAcceptCHFrame, "AcceptCHFrame", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSCTAuditingRetryReports,
             "SCTAuditingRetryReports",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSCTAuditingPersistReports,
             "SCTAuditingPersistReports",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// https://fetch.spec.whatwg.org/#cors-non-wildcard-request-header-name
BASE_FEATURE(kCorsNonWildcardRequestHeadersSupport,
             "CorsNonWildcardRequestHeadersSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow batching SimpleURLLoaders when the underlying network state is
// inactive.
BASE_FEATURE(kBatchSimpleURLLoader,
             "BatchSimpleURLLoader",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNetworkServiceMemoryCache,
             "NetworkServiceMemoryCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Do not send TLS client certificates in CORS preflight. Omit all client certs
// and continue the handshake without sending one if requested.
BASE_FEATURE(kOmitCorsClientCert,
             "OmitCorsClientCert",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow pervasive payloads to use a single-keyed cache.
BASE_FEATURE(kCacheTransparency,
             "CacheTransparency",
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

// Gate access to ReduceAcceptLanguage origin trial major code. Currently, All
// ReduceAcceptLanguage feature codes are guarded by the feature flag
// kReduceAcceptLanguage. This feature flag is useful on control major code
// which required to do origin trial. It allows Chrome developers to mitigate
// issues when exposed codes cause impacts.
BASE_FEATURE(kReduceAcceptLanguageOriginTrial,
             "ReduceAcceptLanguageOriginTrial",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disable ResourceScheduler.
BASE_FEATURE(kDisableResourceScheduler,
             "DisableResourceScheduler",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Reduce PNA preflight response waiting time to 200ms.
// See: https://wicg.github.io/private-network-access/#cors-preflight
BASE_FEATURE(kPrivateNetworkAccessPreflightShortTimeout,
             "PrivateNetworkAccessPreflightReduceTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Handle the Link header DNS prefetches and preconnects in the network
// service instead of through the renderer process.
BASE_FEATURE(kPreconnectInNetworkService,
             "PreconnectInNetworkService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When prefetching a DNS record ensures that the scheme and port are taken
// into account so that the cache (which is keyed by scheme and port) works
// for subsequent queries.
BASE_FEATURE(kPrefetchDNSWithURL,
             "PrefetchDNSWithURL",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool> kPrefetchDNSWithURLAllAnchorElements{
    &kPrefetchDNSWithURL, "prefetch_dns_all_anchor_elements", true};

// Preconnect to a new origin right when a redirect starts.
BASE_FEATURE(kPreconnectOnRedirect,
             "PreconnectOnRedirect",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables out-of-process system DNS resolution so getaddrinfo() never runs in
// the network service sandbox. System DNS resolution will instead be brokered
// out over Mojo, likely to run in the browser process.
BASE_FEATURE(kOutOfProcessSystemDnsResolution,
             "OutOfProcessSystemDnsResolution",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAccessControlAllowMethodsInCORSPreflightSpecConformant,
             "AccessControlAllowMethodsInCORSPreflightSpecConformant",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchNoVarySearch,
             "PrefetchNoVarySearch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrerender2ContentSecurityPolicyExtensions,
             "Prerender2ContentSecurityPolicyExtensions",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace network::features
