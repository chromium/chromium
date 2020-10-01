// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FEATURES_H_
#define NET_BASE_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/net_buildflags.h"

namespace net {
namespace features {

// Toggles the `Accept-Language` HTTP request header, which
// https://github.com/WICG/lang-client-hint proposes that we deprecate.
NET_EXPORT extern const base::Feature kAcceptLanguageHeader;

// When kCapReferrerToOriginOnCrossOrigin is enabled, HTTP referrers on cross-
// origin requests are restricted to contain at most the source origin.
NET_EXPORT extern const base::Feature kCapReferrerToOriginOnCrossOrigin;

// Enables TLS 1.3 early data.
NET_EXPORT extern const base::Feature kEnableTLS13EarlyData;

// Enables DNS queries for HTTPSSVC or INTEGRITY records, depending on feature
// parameters. These queries will only be made over DoH. HTTPSSVC responses may
// cause us to upgrade the URL to HTTPS and/or to attempt QUIC.
NET_EXPORT extern const base::Feature kDnsHttpssvc;

// Disable H2 reprioritization, in order to measure its impact.
NET_EXPORT extern const base::Feature kAvoidH2Reprioritization;

// Determine which kind of record should be queried: HTTPSSVC or INTEGRITY. No
// more than one of these feature parameters should be enabled at once. In the
// event that both are enabled, |kDnsHttpssvcUseIntegrity| takes priority, and
// |kDnsHttpssvcUseHttpssvc| will be ignored.
NET_EXPORT extern const base::FeatureParam<bool> kDnsHttpssvcUseHttpssvc;
NET_EXPORT extern const base::FeatureParam<bool> kDnsHttpssvcUseIntegrity;

// Enable HTTPSSVC or INTEGRITY to be queried over insecure DNS.
NET_EXPORT extern const base::FeatureParam<bool>
    kDnsHttpssvcEnableQueryOverInsecure;

// If we are still waiting for an HTTPSSVC or INTEGRITY query after all the
// other queries in a DnsTask have completed, we will compute a timeout for the
// remaining query. The timeout will be the min of:
//   (a) |kDnsHttpssvcExtraTimeMs.Get()|
//   (b) |kDnsHttpssvcExtraTimePercent.Get() / 100 * t|, where |t| is the
//       number of milliseconds since the first query began.
NET_EXPORT extern const base::FeatureParam<int> kDnsHttpssvcExtraTimeMs;
NET_EXPORT extern const base::FeatureParam<int> kDnsHttpssvcExtraTimePercent;

// These parameters, respectively, are the list of experimental and control
// domains for which we will query HTTPSSVC or INTEGRITY records. We expect
// valid INTEGRITY results for experiment domains. We expect no INTEGRITY
// results for control domains.
//
// The format of both parameters is a comma-separated list of domains.
// Whitespace around domain names is permitted. Trailing comma is optional.
//
// See helper functions:
// |dns_httpssvc_experiment::GetDnsHttpssvcExperimentDomains| and
// |dns_httpssvc_experiment::GetDnsHttpssvcControlDomains|.
NET_EXPORT extern const base::FeatureParam<std::string>
    kDnsHttpssvcExperimentDomains;
NET_EXPORT extern const base::FeatureParam<std::string>
    kDnsHttpssvcControlDomains;

// This param controls how we determine whether a domain is an experimental or
// control domain. When false, domains must be in |kDnsHttpssvcControlDomains|
// to be considered a control. When true, we ignore |kDnsHttpssvcControlDomains|
// and any non-experiment domain (not in |kDnsHttpssvcExperimentDomains|) is
// considered a control domain.
NET_EXPORT extern const base::FeatureParam<bool>
    kDnsHttpssvcControlDomainWildcard;

namespace dns_httpssvc_experiment {
// Get the value of |kDnsHttpssvcExtraTimeMs|.
NET_EXPORT base::TimeDelta GetExtraTimeAbsolute();
}  // namespace dns_httpssvc_experiment

// Enables optimizing the network quality estimation algorithms in network
// quality estimator (NQE).
NET_EXPORT extern const base::Feature kNetworkQualityEstimator;

// Splits cache entries by the request's NetworkIsolationKey if one is
// available.
NET_EXPORT extern const base::Feature kSplitCacheByNetworkIsolationKey;

// Splits host cache entries by the DNS request's NetworkIsolationKey if one is
// available. Also prevents merging live DNS lookups when there is a NIK
// mismatch.
NET_EXPORT extern const base::Feature kSplitHostCacheByNetworkIsolationKey;

// Partitions connections based on the NetworkIsolationKey associated with a
// request.
NET_EXPORT extern const base::Feature
    kPartitionConnectionsByNetworkIsolationKey;

// Partitions HttpServerProperties based on the NetworkIsolationKey associated
// with a request.
NET_EXPORT extern const base::Feature
    kPartitionHttpServerPropertiesByNetworkIsolationKey;

// Partitions TLS sessions and QUIC server configs based on the
// NetworkIsolationKey associated with a request.
//
// This feature requires kPartitionConnectionsByNetworkIsolationKey to be
// enabled to work.
NET_EXPORT extern const base::Feature
    kPartitionSSLSessionsByNetworkIsolationKey;

// Partitions Expect-CT data by NetworkIsolationKey. This only affects the
// Expect-CT data itself. Regardless of this value, reports will be uploaded
// using the associated NetworkIsolationKey, when one's available.
//
// This feature requires kPartitionConnectionsByNetworkIsolationKey,
// kPartitionHttpServerPropertiesByNetworkIsolationKey, and
// kPartitionConnectionsByNetworkIsolationKey to all be enabled to work.
NET_EXPORT extern const base::Feature
    kPartitionExpectCTStateByNetworkIsolationKey;

// Enables limiting the size of Expect-CT table.
NET_EXPORT extern const base::Feature kExpectCTPruning;

// FeatureParams associated with kExpectCTPruning.

// Expect-CT pruning runs when this many entries are hit.
NET_EXPORT extern const base::FeatureParam<int> kExpectCTPruneMax;
// The Expect-CT pruning logic attempts to reduce entries to at most this many.
NET_EXPORT extern const base::FeatureParam<int> kExpectCTPruneMin;
// Non-transient entries with |enforce| set are safe from being pruned if
// they're less than this many days old, unless the number of entries exceeds
// |kExpectCTMaxEntriesPerNik|.
NET_EXPORT extern const base::FeatureParam<int> kExpectCTSafeFromPruneDays;
// If, after pruning transient, non-enforced, old Expect-CT entries,
// kExpectCTPruneMin is still exceeded, then all NetworkIsolationKeys will be
// capped to this many entries, based on last observation date.
NET_EXPORT extern const base::FeatureParam<int> kExpectCTMaxEntriesPerNik;
// Minimum delay between successive prunings of Expect-CT entries, in seconds.
NET_EXPORT extern const base::FeatureParam<int> kExpectCTPruneDelaySecs;

// Enables sending TLS 1.3 Key Update messages on TLS 1.3 connections in order
// to ensure that this corner of the spec is exercised. This is currently
// disabled by default because we discovered incompatibilities with some
// servers.
NET_EXPORT extern const base::Feature kTLS13KeyUpdate;

// Enables CECPQ2, a post-quantum key-agreement, in TLS 1.3 connections.
NET_EXPORT extern const base::Feature kPostQuantumCECPQ2;

// Changes the timeout after which unused sockets idle sockets are cleaned up.
NET_EXPORT extern const base::Feature kNetUnusedIdleSocketTimeout;

// When enabled, makes cookies without a SameSite attribute behave like
// SameSite=Lax cookies by default, and requires SameSite=None to be specified
// in order to make cookies available in a third-party context. When disabled,
// the default behavior for cookies without a SameSite attribute specified is no
// restriction, i.e., available in a third-party context.
// The "Lax-allow-unsafe" mitigation allows these cookies to be sent on
// top-level cross-site requests with an unsafe (e.g. POST) HTTP method, if the
// cookie is no more than 2 minutes old.
NET_EXPORT extern const base::Feature kSameSiteByDefaultCookies;

// When enabled, cookies without SameSite restrictions that don't specify the
// Secure attribute will be rejected if set from an insecure context, or treated
// as secure if set from a secure context. This ONLY has an effect if
// SameSiteByDefaultCookies is also enabled.
NET_EXPORT extern const base::Feature kCookiesWithoutSameSiteMustBeSecure;

// When enabled, the time threshold for Lax-allow-unsafe cookies will be lowered
// from 2 minutes to 10 seconds. This time threshold refers to the age cutoff
// for which cookies that default into SameSite=Lax, which are newer than the
// threshold, will be sent with any top-level cross-site navigation regardless
// of HTTP method (i.e. allowing unsafe methods). This is a convenience for
// integration tests which may want to test behavior of cookies older than the
// threshold, but which would not be practical to run for 2 minutes.
NET_EXPORT extern const base::Feature kShortLaxAllowUnsafeThreshold;

// When enabled, the SameSite by default feature does not add the
// "Lax-allow-unsafe" behavior. Any cookies that do not specify a SameSite
// attribute will be treated as Lax only, i.e. POST and other unsafe HTTP
// methods will not be allowed at all for top-level cross-site navigations.
// This only has an effect if the cookie defaults to SameSite=Lax.
NET_EXPORT extern const base::Feature kSameSiteDefaultChecksMethodRigorously;

// If this is set and has a non-zero param value, any access to a cookie will be
// granted Legacy access semantics if the last access to a cookie with the same
// (name, domain, path) from a context that is same-site and permits
// HttpOnly access occurred less than (param value) milliseconds ago. The last
// eligible access must have occurred in the current browser session (i.e. it
// does not persist across sessions). This feature does nothing if
// kCookiesWithoutSameSiteMustBeSecure is not enabled.
NET_EXPORT extern const base::Feature
    kRecentHttpSameSiteAccessGrantsLegacyCookieSemantics;
NET_EXPORT extern const base::FeatureParam<int>
    kRecentHttpSameSiteAccessGrantsLegacyCookieSemanticsMilliseconds;

// Recently created cookies are granted legacy access semantics. If this is set
// and has a non-zero integer param value, then for the first (param value)
// milliseconds after the cookie is created, the cookie will behave as if it
// were "legacy" i.e. not handled according to SameSiteByDefaultCookies/
// CookiesWithoutSameSiteMustBeSecure rules.
// This does nothing if SameSiteByDefaultCookies is not enabled.
NET_EXPORT extern const base::Feature
    kRecentCreationTimeGrantsLegacyCookieSemantics;
NET_EXPORT extern const base::FeatureParam<int>
    kRecentCreationTimeGrantsLegacyCookieSemanticsMilliseconds;

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
// When enabled, use the builtin cert verifier instead of the platform verifier.
NET_EXPORT extern const base::Feature kCertVerifierBuiltinFeature;
#endif

NET_EXPORT extern const base::Feature kAppendFrameOriginToNetworkIsolationKey;

// Turns off streaming media caching to disk when on battery power.
NET_EXPORT extern const base::Feature kTurnOffStreamingMediaCachingOnBattery;

// Turns off streaming media caching to disk always.
NET_EXPORT extern const base::Feature kTurnOffStreamingMediaCachingAlways;

// When enabled, sites that use TLS versions below the |version_min_warn|
// threshold are marked with the LEGACY_TLS CertStatus and return an
// ERR_SSL_OBSOLETE_VERSION error. This is used to trigger an interstitial
// warning for these pages.
NET_EXPORT extern const base::Feature kLegacyTLSEnforced;

// When enabled this feature will cause same-site calculations to take into
// account the scheme of the site-for-cookies and the request/response url.
NET_EXPORT extern const base::Feature kSchemefulSameSite;

// When enabled, TLS connections will initially not offer 3DES and SHA-1 but
// enable them on fallback. This is used to improve metrics around usage of
// those algorithms. If disabled, the algorithms will always be offered.
NET_EXPORT extern const base::Feature kTLSLegacyCryptoFallbackForMetrics;

// When enabled, DNS_PROBE_FINISHED_NXDOMAIN error pages may show
// locally-generated suggestions to visit similar domains.
NET_EXPORT extern const base::Feature kUseLookalikesForNavigationSuggestions;

// When enabled, the Network Quality Estimator (NQE) will notify the operating
// system whenever it detects that the current default network may have
// significantly degraded connectivity. Currently only effective on Android.
NET_EXPORT extern const base::Feature kReportPoorConnectivity;

// When enabled, the NQE may preemptively request that the OS activate a mobile
// network when requests on the active Wi-Fi connection are stalled. This can be
// used to warm the radio for a faster transition if/when the OS chooses to drop
// the Wi-Fi connection.
NET_EXPORT extern const base::Feature kPreemptiveMobileNetworkActivation;

// Enables a process-wide limit on "open" UDP sockets. See
// udp_socket_global_limits.h for details on what constitutes an "open" socket.
NET_EXPORT extern const base::Feature kLimitOpenUDPSockets;

// FeatureParams associated with kLimitOpenUDPSockets.

// Sets the maximum allowed open UDP sockets. Provisioning more sockets than
// this will result in a failure (ERR_INSUFFICIENT_RESOURCES).
NET_EXPORT extern const base::FeatureParam<int> kLimitOpenUDPSocketsMax;

// Enables a timeout on individual TCP connect attempts, based on
// the parameter values.
NET_EXPORT extern const base::Feature kTimeoutTcpConnectAttempt;

// FeatureParams associated with kTimeoutTcpConnectAttempt.

// When there is an estimated RTT available, the experimental TCP connect
// attempt timeout is calculated as:
//
//  clamp(kTimeoutTcpConnectAttemptMin,
//        kTimeoutTcpConnectAttemptMax,
//        <Estimated RTT> * kTimeoutTcpConnectAttemptRTTMultiplier);
//
// Otherwise the TCP connect attempt timeout is set to
// kTimeoutTcpConnectAttemptMax.
NET_EXPORT extern const base::FeatureParam<double>
    kTimeoutTcpConnectAttemptRTTMultiplier;
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kTimeoutTcpConnectAttemptMin;
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kTimeoutTcpConnectAttemptMax;

}  // namespace features
}  // namespace net

#endif  // NET_BASE_FEATURES_H_
