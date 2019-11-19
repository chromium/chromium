// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/features.h"

#include "build/build_config.h"

namespace network {
namespace features {

// When kCapReferrerToOriginOnCrossOrigin is enabled, HTTP referrers on cross-
// origin requests are restricted to contain at most the source origin.
const base::Feature kCapReferrerToOriginOnCrossOrigin{
    "CapReferrerToOriginOnCrossOrigin", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Expect CT reporting, which sends reports for opted-in sites
// that don't serve sufficient Certificate Transparency information.
const base::Feature kExpectCTReporting{"ExpectCTReporting",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNetworkErrorLogging{"NetworkErrorLogging",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
// Enables the network service.
const base::Feature kNetworkService {
#if defined(OS_ANDROID)
  "NetworkService",
#else
  "NetworkServiceNotSupported",
#endif
      base::FEATURE_ENABLED_BY_DEFAULT
};

// Out of Blink CORS will be launched at m79. The flag will be enabled by
// default around m81 after the feature rolled out over the finch successfully
// at m79. Both mode will be maintained at least until m81, or around m83+ for
// enterprise supports.
const base::Feature kOutOfBlinkCors{"OutOfBlinkCors",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

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

// Implementation of https://mikewest.github.io/sec-metadata/
const base::Feature kFetchMetadata{"FetchMetadata",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// The `Sec-Fetch-Dest` header is split out from the main "FetchMetadata"
// feature so we can ship the broader feature without this specifific bit
// while we continue discussion.
const base::Feature kFetchMetadataDestination{
    "FetchMetadataDestination", base::FEATURE_DISABLED_BY_DEFAULT};

// When kRequestInitiatorSiteLock is enabled, then CORB, CORP and Sec-Fetch-Site
// will validate network::ResourceRequest::request_initiator against
// network::mojom::URLLoaderFactoryParams::request_initiator_site_lock.
const base::Feature kRequestInitiatorSiteLock{"RequestInitiatorSiteLock",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// When kPauseBrowserInitiatedHeavyTrafficForP2P is enabled, then a subset of
// the browser initiated traffic may be paused if there is at least one active
// P2P connection and the network is estimated to be congested. This feature is
// intended to throttle only the browser initiated traffic that is expected to
// be heavy (has large request/response sizes) when real time content might be
// streaming over an active P2P connection.
const base::Feature kPauseBrowserInitiatedHeavyTrafficForP2P{
    "PauseBrowserInitiatedHeavyTrafficForP2P",
    base::FEATURE_DISABLED_BY_DEFAULT};

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

// This is for Cross-Origin-Opener-Policy (COOP) and
// Cross-Origin-Embedder-Policy (COEP).
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
// https://github.com/mikewest/corpp
const base::Feature kCrossOriginIsolation{"CrossOriginIsolation",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// When kBlockNonSecureExternalRequests is enabled, requests initiated from a
// pubic network may only target a private network if the initiating context
// is secure.
//
// https://wicg.github.io/cors-rfc1918/#integration-fetch
const base::Feature kBlockNonSecureExternalRequests{
    "BlockNonSecureExternalRequests", base::FEATURE_DISABLED_BY_DEFAULT};

// When kPrefetchMainResourceNetworkIsolationKey is enabled, cross-origin
// prefetch requests for main-resources, as well as their preload response
// headers, will use a special NetworkIsolationKey allowing them to be reusable
// from a cross-origin context when the HTTP cache is partitioned by the
// NetworkIsolationKey.
const base::Feature kPrefetchMainResourceNetworkIsolationKey{
    "PrefetchMainResourceNetworkIsolationKey",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or defaults splittup up server (not proxy) entries in the
// HttpAuthCache.
const base::Feature kSplitAuthCacheByNetworkIsolationKey{
    "SplitAuthCacheByNetworkIsolationKey", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable usage of hardcoded DoH upgrade mapping for use in automatic mode.
const base::Feature kDnsOverHttpsUpgrade {
  "DnsOverHttpsUpgrade",
#if defined(OS_CHROMEOS) || defined(OS_MACOSX) || defined(OS_ANDROID) || \
    defined(OS_WIN)
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

// Provides a mechanism to disable DoH upgrades for some subset of the hardcoded
// upgrade mapping. Separate multiple provider ids with commas. See the
// mapping in net/dns/dns_util.cc for provider ids.
const base::FeatureParam<std::string>
    kDnsOverHttpsUpgradeDisabledProvidersParam{&kDnsOverHttpsUpgrade,
                                               "DisabledProviders", ""};

// Disable special treatment on requests with keepalive set (see
// https://fetch.spec.whatwg.org/#request-keepalive-flag). This is introduced
// for investigation on the memory usage, and should not be enabled widely.
const base::Feature kDisableKeepaliveFetch{"DisableKeepaliveFetch",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// When kOutOfBlinkFrameAncestors is enabled, the frame-ancestors
// directive is parsed from the Content-Security-Policy header in the network
// service and enforced in the browser.
const base::Feature kOutOfBlinkFrameAncestors{
    "OutOfBlinkFrameAncestors", base::FEATURE_DISABLED_BY_DEFAULT};

bool ShouldEnableOutOfBlinkCorsForTesting() {
  return base::FeatureList::IsEnabled(features::kOutOfBlinkCors);
}

}  // namespace features
}  // namespace network
