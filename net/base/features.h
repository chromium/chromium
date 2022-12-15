// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FEATURES_H_
#define NET_BASE_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/net_buildflags.h"

namespace net::features {

// Enables ALPS extension of TLS 1.3 for HTTP/2, see
// https://vasilvv.github.io/tls-alps/draft-vvv-tls-alps.html and
// https://vasilvv.github.io/httpbis-alps/draft-vvv-httpbis-alps.html.
NET_EXPORT BASE_DECLARE_FEATURE(kAlpsForHttp2);

// Disable H2 reprioritization, in order to measure its impact.
NET_EXPORT BASE_DECLARE_FEATURE(kAvoidH2Reprioritization);

// When kCapReferrerToOriginOnCrossOrigin is enabled, HTTP referrers on cross-
// origin requests are restricted to contain at most the source origin.
NET_EXPORT BASE_DECLARE_FEATURE(kCapReferrerToOriginOnCrossOrigin);

// Support for altering the parameters used for DNS transaction timeout. See
// ResolveContext::SecureTransactionTimeout().
NET_EXPORT BASE_DECLARE_FEATURE(kDnsTransactionDynamicTimeouts);
// Multiplier applied to current fallback periods in determining a transaction
// timeout.
NET_EXPORT extern const base::FeatureParam<double>
    kDnsTransactionTimeoutMultiplier;
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kDnsMinTransactionTimeout;

// Enables querying HTTPS DNS records that will affect results from HostResolver
// and may be used to affect connection behavior. Whether or not those results
// are used (e.g. to connect via ECH) may be controlled by separate features.
NET_EXPORT BASE_DECLARE_FEATURE(kUseDnsHttpsSvcb);

// Param to control whether or not HostResolver, when using Secure DNS, will
// fail the entire connection attempt when receiving an inconclusive response to
// an HTTPS query (anything except transport error, timeout, or SERVFAIL). Used
// to prevent certain downgrade attacks against ECH behavior.
NET_EXPORT extern const base::FeatureParam<bool>
    kUseDnsHttpsSvcbEnforceSecureResponse;

// If we are still waiting for an HTTPS transaction after all the
// other transactions in an insecure DnsTask have completed, we will compute a
// timeout for the remaining transaction. The timeout will be
// `kUseDnsHttpsSvcbInsecureExtraTimePercent.Get() / 100 * t`, where `t` is the
// time delta since the first query began. And the timeout will additionally be
// clamped by:
//   (a) `kUseDnsHttpsSvcbInsecureExtraTimeMin.Get()`
//   (b) `kUseDnsHttpsSvcbInsecureExtraTimeMax.Get()`
//
// Any param is ignored if zero, and if one of min/max is non-zero with a zero
// percent param it will be used as an absolute timeout. If all are zero, there
// is no timeout specific to HTTPS transactions, only the regular DNS query
// timeout and server fallback.
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kUseDnsHttpsSvcbInsecureExtraTimeMax;
NET_EXPORT extern const base::FeatureParam<int>
    kUseDnsHttpsSvcbInsecureExtraTimePercent;
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kUseDnsHttpsSvcbInsecureExtraTimeMin;

// Same as `kUseDnsHttpsSvcbInsecureExtraTime...` except for secure DnsTasks.
//
// If `kUseDnsHttpsSvcbEnforceSecureResponse` is enabled, the timeouts will not
// be used because there is no sense killing a transaction early if that will
// just kill the entire request.
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kUseDnsHttpsSvcbSecureExtraTimeMax;
NET_EXPORT extern const base::FeatureParam<int>
    kUseDnsHttpsSvcbSecureExtraTimePercent;
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kUseDnsHttpsSvcbSecureExtraTimeMin;

// Update protocol using ALPN information in HTTPS DNS records.
NET_EXPORT BASE_DECLARE_FEATURE(kUseDnsHttpsSvcbAlpn);

// Enables TLS 1.3 early data.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableTLS13EarlyData);

// Enables the TLS Encrypted ClientHello feature.
// https://datatracker.ietf.org/doc/html/draft-ietf-tls-esni-13
NET_EXPORT BASE_DECLARE_FEATURE(kEncryptedClientHello);

// Enables optimizing the network quality estimation algorithms in network
// quality estimator (NQE).
NET_EXPORT BASE_DECLARE_FEATURE(kNetworkQualityEstimator);

// Splits cache entries by the request's includeCredentials.
NET_EXPORT BASE_DECLARE_FEATURE(kSplitCacheByIncludeCredentials);

// Splits cache entries by the request's NetworkIsolationKey if one is
// available.
NET_EXPORT BASE_DECLARE_FEATURE(kSplitCacheByNetworkIsolationKey);

// Splits host cache entries by the DNS request's NetworkIsolationKey if one is
// available. Also prevents merging live DNS lookups when there is a NIK
// mismatch.
NET_EXPORT BASE_DECLARE_FEATURE(kSplitHostCacheByNetworkIsolationKey);

// Partitions connections based on the NetworkIsolationKey associated with a
// request.
NET_EXPORT BASE_DECLARE_FEATURE(kPartitionConnectionsByNetworkIsolationKey);

// Forces the `frame_origin` value in IsolationInfo to the `top_level_origin`
// value when an IsolationInfo instance is created. This is to enable
// expirimenting with double keyed network partitions.
NET_EXPORT BASE_DECLARE_FEATURE(kForceIsolationInfoFrameOriginToTopLevelFrame);

// Partitions HttpServerProperties based on the NetworkIsolationKey associated
// with a request.
NET_EXPORT BASE_DECLARE_FEATURE(
    kPartitionHttpServerPropertiesByNetworkIsolationKey);

// Partitions TLS sessions and QUIC server configs based on the
// NetworkIsolationKey associated with a request.
//
// This feature requires kPartitionConnectionsByNetworkIsolationKey to be
// enabled to work.
NET_EXPORT BASE_DECLARE_FEATURE(kPartitionSSLSessionsByNetworkIsolationKey);

// Partitions Network Error Logging and Reporting API data by
// NetworkIsolationKey. Also partitions all reports generated by other consumers
// of the reporting API. Applies the NetworkIsolationKey to reports uploads as
// well.
//
// When disabled, the main entry points of the reporting and NEL services ignore
// NetworkIsolationKey parameters, and they're cleared while loading from the
// cache, but internal objects can be created with them (e.g., endpoints), for
// testing.
NET_EXPORT BASE_DECLARE_FEATURE(kPartitionNelAndReportingByNetworkIsolationKey);

// Creates a <double key + is_cross_site> NetworkAnonymizationKey which is used
// to partition the network state. This double key will have the following
// properties: `top_frame_site` -> the schemeful site of the top level page.
// `frame_site ` -> nullopt
// `is_cross_site` -> true if the `top_frame_site` is cross site when compared
// to the frame site. The frame site will not be stored in this key so the value
// of is_cross_site will be computed at key construction. This feature overrides
// `kEnableDoubleKeyNetworkAnonymizationKey` if both are enabled.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableCrossSiteFlagNetworkAnonymizationKey);

// Creates a double keyed NetworkAnonymizationKey which is used to partition the
// network state. This double key will have the following properties:
// `top_frame_site` -> the schemeful site of the top level page.
// `frame_site ` -> nullopt
// `is_cross_site` -> nullopt
NET_EXPORT BASE_DECLARE_FEATURE(kEnableDoubleKeyNetworkAnonymizationKey);

// Enables sending TLS 1.3 Key Update messages on TLS 1.3 connections in order
// to ensure that this corner of the spec is exercised. This is currently
// disabled by default because we discovered incompatibilities with some
// servers.
NET_EXPORT BASE_DECLARE_FEATURE(kTLS13KeyUpdate);

// Enables permuting TLS extensions in the ClientHello, to reduce the risk of
// non-compliant servers ossifying parts of the ClientHello and interfering with
// deployment of future security improvements.
NET_EXPORT BASE_DECLARE_FEATURE(kPermuteTLSExtensions);

// Enables CECPQ2, a post-quantum key-agreement, in TLS 1.3 connections.
NET_EXPORT BASE_DECLARE_FEATURE(kPostQuantumCECPQ2);

// Enables CECPQ2, a post-quantum key-agreement, in TLS 1.3 connections for a
// subset of domains. (This is intended as Finch kill-switch. For testing
// compatibility with large ClientHello messages, use |kPostQuantumCECPQ2|.)
NET_EXPORT BASE_DECLARE_FEATURE(kPostQuantumCECPQ2SomeDomains);
NET_EXPORT extern const base::FeatureParam<std::string>
    kPostQuantumCECPQ2Prefix;

// Causes SSLClientSocket to force a minimum TLS version of at least TLS 1.2.
NET_EXPORT BASE_DECLARE_FEATURE(kSSLMinVersionAtLeastTLS12);

// Changes the timeout after which unused sockets idle sockets are cleaned up.
NET_EXPORT BASE_DECLARE_FEATURE(kNetUnusedIdleSocketTimeout);

// When enabled, the time threshold for Lax-allow-unsafe cookies will be lowered
// from 2 minutes to 10 seconds. This time threshold refers to the age cutoff
// for which cookies that default into SameSite=Lax, which are newer than the
// threshold, will be sent with any top-level cross-site navigation regardless
// of HTTP method (i.e. allowing unsafe methods). This is a convenience for
// integration tests which may want to test behavior of cookies older than the
// threshold, but which would not be practical to run for 2 minutes.
NET_EXPORT BASE_DECLARE_FEATURE(kShortLaxAllowUnsafeThreshold);

// When enabled, the SameSite by default feature does not add the
// "Lax-allow-unsafe" behavior. Any cookies that do not specify a SameSite
// attribute will be treated as Lax only, i.e. POST and other unsafe HTTP
// methods will not be allowed at all for top-level cross-site navigations.
// This only has an effect if the cookie defaults to SameSite=Lax.
NET_EXPORT BASE_DECLARE_FEATURE(kSameSiteDefaultChecksMethodRigorously);

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
NET_EXPORT BASE_DECLARE_FEATURE(kCertDualVerificationTrialFeature);
#if BUILDFLAG(IS_MAC)
NET_EXPORT extern const base::FeatureParam<int> kCertDualVerificationTrialImpl;
#endif /* BUILDFLAG(IS_MAC) */
#endif /* BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED) */

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
// When enabled, use the Chrome Root Store instead of the system root store
NET_EXPORT BASE_DECLARE_FEATURE(kChromeRootStoreUsed);
#if BUILDFLAG(IS_MAC)
NET_EXPORT extern const base::FeatureParam<int> kChromeRootStoreSysImpl;
#endif /* BUILDFLAG(IS_MAC) */
#endif /* BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED) */

// Turns off streaming media caching to disk when on battery power.
NET_EXPORT BASE_DECLARE_FEATURE(kTurnOffStreamingMediaCachingOnBattery);

// Turns off streaming media caching to disk always.
NET_EXPORT BASE_DECLARE_FEATURE(kTurnOffStreamingMediaCachingAlways);

// When enabled this feature will cause same-site calculations to take into
// account the scheme of the site-for-cookies and the request/response url.
NET_EXPORT BASE_DECLARE_FEATURE(kSchemefulSameSite);

// Enables a process-wide limit on "open" UDP sockets. See
// udp_socket_global_limits.h for details on what constitutes an "open" socket.
NET_EXPORT BASE_DECLARE_FEATURE(kLimitOpenUDPSockets);

// FeatureParams associated with kLimitOpenUDPSockets.

// Sets the maximum allowed open UDP sockets. Provisioning more sockets than
// this will result in a failure (ERR_INSUFFICIENT_RESOURCES).
NET_EXPORT extern const base::FeatureParam<int> kLimitOpenUDPSocketsMax;

// Enables a timeout on individual TCP connect attempts, based on
// the parameter values.
NET_EXPORT BASE_DECLARE_FEATURE(kTimeoutTcpConnectAttempt);

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

#if BUILDFLAG(ENABLE_REPORTING)
// When enabled this feature will allow a new Reporting-Endpoints header to
// configure reporting endpoints for report delivery. This is used to support
// the new Document Reporting spec.
NET_EXPORT BASE_DECLARE_FEATURE(kDocumentReporting);
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
// When enabled, UDPSocketPosix increments the global counter of bytes received
// every time bytes are received, instead of using a timer to batch updates.
// This should reduce the number of wake ups and improve battery consumption.
// TODO(https://crbug.com/1189805): Cleanup the feature after verifying that it
// doesn't negatively affect performance.
NET_EXPORT BASE_DECLARE_FEATURE(kUdpSocketPosixAlwaysUpdateBytesReceived);
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

// When this feature is enabled, redirected requests will be considered
// cross-site for the purpose of SameSite cookies if any redirect hop was
// cross-site to the target URL, even if the original initiator of the
// redirected request was same-site with the target URL (and the
// site-for-cookies).
// See spec changes in https://github.com/httpwg/http-extensions/pull/1348
NET_EXPORT BASE_DECLARE_FEATURE(kCookieSameSiteConsidersRedirectChain);

// When this feature is enabled, the SameParty attribute is enabled. (Note that
// when this feature is disabled, the SameParty attribute is still parsed and
// saved for cookie-sets, but it has no associated semantics (when setting or
// reading cookies).)
NET_EXPORT BASE_DECLARE_FEATURE(kSamePartyAttributeEnabled);

// When enabled, cookies with the SameParty attribute are treated as
// "first-party" when in same-party contexts, for the purposes of third-party
// cookie blocking. (Note that as a consequence, some cookies may be blocked
// while others are allowed on a cross-site, same-party request. Additionally,
// privacy mode is disabled in same-party contexts.)
NET_EXPORT BASE_DECLARE_FEATURE(kSamePartyCookiesConsideredFirstParty);

// When enabled, sites can opt-in to having their cookies partitioned by
// top-level site with the Partitioned attribute. Partitioned cookies will only
// be sent when the browser is on the same top-level site that it was on when
// the cookie was set.
NET_EXPORT BASE_DECLARE_FEATURE(kPartitionedCookies);

// When enabled, then we allow partitioned cookies even if kPartitionedCookies
// is disabled only if the cookie partition key contains a nonce. So far, this
// is used to create temporary cookie jar partitions for fenced and anonymous
// frames.
NET_EXPORT BASE_DECLARE_FEATURE(kNoncedPartitionedCookies);

// Enable recording UMAs for network activities which can wake-up radio on
// Android.
NET_EXPORT BASE_DECLARE_FEATURE(kRecordRadioWakeupTrigger);

// When enabled, cookies cannot have an expiry date further than 400 days in the
// future.
NET_EXPORT BASE_DECLARE_FEATURE(kClampCookieExpiryTo400Days);

// Controls whether static key pinning is enforced.
NET_EXPORT BASE_DECLARE_FEATURE(kStaticKeyPinningEnforcement);

// When enabled, cookies with a non-ASCII domain attribute will be rejected.
NET_EXPORT BASE_DECLARE_FEATURE(kCookieDomainRejectNonASCII);

// Blocks the 'Set-Cookie' request header on outbound fetch requests.
NET_EXPORT BASE_DECLARE_FEATURE(kBlockSetCookieHeader);

NET_EXPORT BASE_DECLARE_FEATURE(kOptimisticBlockfileWrite);

NET_EXPORT BASE_DECLARE_FEATURE(kOptimizeNetworkBuffers);

NET_EXPORT
extern const base::FeatureParam<int> kOptimizeNetworkBuffersBytesReadLimit;

NET_EXPORT extern const base::FeatureParam<int>
    kOptimizeNetworkBuffersMinInputStreamAvailableValueToIgnore;

NET_EXPORT extern const base::FeatureParam<int>
    kOptimizeNetworkBuffersMinInputStreamReadSize;

NET_EXPORT extern const base::FeatureParam<int>
    kOptimizeNetworkBuffersMaxInputStreamBytesToReadWhenAvailableUnknown;

NET_EXPORT extern const base::FeatureParam<int>
    kOptimizeNetworkBuffersFilterSourceStreamBufferSize;

NET_EXPORT extern const base::FeatureParam<bool>
    kOptimizeNetworkBuffersInputStreamCheckAvailable;

// Enable the Storage Access API. https://crbug.com/989663.
NET_EXPORT BASE_DECLARE_FEATURE(kStorageAccessAPI);

// Set the default number of "automatic" implicit storage access grants per
// third party origin that can be granted. This can be overridden via
// experimentation to allow for field trials to validate the default setting.
NET_EXPORT extern const int kStorageAccessAPIDefaultImplicitGrantLimit;
NET_EXPORT extern const base::FeatureParam<int>
    kStorageAccessAPIImplicitGrantLimit;
// Whether the Storage Access API can grant access to storage (even if it is
// unpartitioned). When this feature is disabled, access to storage is only
// granted if the storage is partitioned.
NET_EXPORT extern const base::FeatureParam<bool>
    kStorageAccessAPIGrantsUnpartitionedStorage;
// Whether to auto-grant storage access requests when the top level origin and
// the requesting origin are in the same First-Party Set.
NET_EXPORT extern const base::FeatureParam<bool>
    kStorageAccessAPIAutoGrantInFPS;
// Whether to auto-deny storage access requests when the top level origin and
// the requesting origin are not in the same First-Party Set.
NET_EXPORT extern const base::FeatureParam<bool>
    kStorageAccessAPIAutoDenyOutsideFPS;

NET_EXPORT BASE_DECLARE_FEATURE(kThirdPartyStoragePartitioning);
NET_EXPORT BASE_DECLARE_FEATURE(kSupportPartitionedBlobUrl);

// Whether ALPS parsing is on for any type of frame.
NET_EXPORT BASE_DECLARE_FEATURE(kAlpsParsing);

// Whether ALPS parsing is on for client hint parsing specifically.
NET_EXPORT BASE_DECLARE_FEATURE(kAlpsClientHintParsing);

// Whether to kill the session on Error::kAcceptChMalformed.
NET_EXPORT BASE_DECLARE_FEATURE(kShouldKillSessionOnAcceptChMalformed);

NET_EXPORT BASE_DECLARE_FEATURE(kCaseInsensitiveCookiePrefix);

NET_EXPORT BASE_DECLARE_FEATURE(kEnableWebsocketsOverHttp3);

// Whether to do IPv4 to IPv6 address translation for IPv4 literals.
NET_EXPORT BASE_DECLARE_FEATURE(kUseNAT64ForIPv4Literal);

// Whether to block newly added forbidden headers (https://crbug.com/1362331).
NET_EXPORT BASE_DECLARE_FEATURE(kBlockNewForbiddenHeaders);

#if BUILDFLAG(IS_WIN)
// Whether to probe for SHA-256 on some legacy platform keys, before assuming
// the key requires SHA-1. See SSLPlatformKeyWin for details.
NET_EXPORT BASE_DECLARE_FEATURE(kPlatformKeyProbeSHA256);
#endif

}  // namespace net::features

#endif  // NET_BASE_FEATURES_H_
