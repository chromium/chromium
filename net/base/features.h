// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FEATURES_H_
#define NET_BASE_FEATURES_H_

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/crypto_buildflags.h"
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

// Enables the built-in DNS resolver.
NET_EXPORT BASE_DECLARE_FEATURE(kAsyncDns);

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

// If enabled, HostResolver will use the new HostResolverCache that separately
// caches by DNS type, unlike the old HostCache that always cached by merged
// request results. May enable related behavior such as separately sorting DNS
// results after each transaction rather than sorting collectively after all
// transactions complete.
NET_EXPORT BASE_DECLARE_FEATURE(kUseHostResolverCache);

// Enables the DNS ServiceEndpointRequest API, which provides intermediate
// service endpoints in the middle of a DNS transaction so that clients of this
// API can attempt connections as soon as candidate endpoints are available.
NET_EXPORT BASE_DECLARE_FEATURE(kUseServiceEndpointRequest);

// If the `kUseAlternativePortForGloballyReachableCheck` flag is enabled, the
// globally reachable check will use the port number specified by
// `kAlternativePortForGloballyReachableCheck` flag. Otherwise, the globally
// reachable check will use 443 port.
NET_EXPORT extern const base::FeatureParam<int>
    kAlternativePortForGloballyReachableCheck;
NET_EXPORT BASE_DECLARE_FEATURE(kUseAlternativePortForGloballyReachableCheck);

// If enabled, overrides IPv6 reachability probe results based on the system's
// IP addresses.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableIPv6ReachabilityOverride);

// Enables TLS 1.3 early data.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableTLS13EarlyData);

// Enables optimizing the network quality estimation algorithms in network
// quality estimator (NQE).
NET_EXPORT BASE_DECLARE_FEATURE(kNetworkQualityEstimator);

// Splits cache entries by the request's includeCredentials.
NET_EXPORT BASE_DECLARE_FEATURE(kSplitCacheByIncludeCredentials);

// Splits cache entries by the request's NetworkIsolationKey if one is
// available.
NET_EXPORT BASE_DECLARE_FEATURE(kSplitCacheByNetworkIsolationKey);

// Splits the generated code cache by the request's NetworkIsolationKey if one
// is available. Note that this feature is also gated behind
// `net::HttpCache::IsSplitCacheEnabled()`.
NET_EXPORT BASE_DECLARE_FEATURE(kSplitCodeCacheByNetworkIsolationKey);

// Splits host cache entries by the DNS request's NetworkAnonymizationKey if one
// is available. Also prevents merging live DNS lookups when there is a NAK
// mismatch.
NET_EXPORT BASE_DECLARE_FEATURE(kSplitHostCacheByNetworkIsolationKey);

// Partitions connections based on the NetworkAnonymizationKey associated with a
// request.
NET_EXPORT BASE_DECLARE_FEATURE(kPartitionConnectionsByNetworkIsolationKey);

// Partitions HttpServerProperties based on the NetworkAnonymizationKey
// associated with a request.
NET_EXPORT BASE_DECLARE_FEATURE(
    kPartitionHttpServerPropertiesByNetworkIsolationKey);

// Partitions TLS sessions and QUIC server configs based on the
// NetworkAnonymizationKey associated with a request.
//
// This feature requires kPartitionConnectionsByNetworkIsolationKey to be
// enabled to work.
NET_EXPORT BASE_DECLARE_FEATURE(kPartitionSSLSessionsByNetworkIsolationKey);

// Partitions Network Error Logging and Reporting API data by
// NetworkAnonymizationKey. Also partitions all reports generated by other
// consumers of the reporting API. Applies the NetworkAnonymizationKey to
// reports uploads as well.
//
// When disabled, the main entry points of the reporting and NEL services ignore
// NetworkAnonymizationKey parameters, and they're cleared while loading from
// the cache, but internal objects can be created with them (e.g., endpoints),
// for testing.
NET_EXPORT BASE_DECLARE_FEATURE(kPartitionNelAndReportingByNetworkIsolationKey);

// Creates a <double key + is_cross_site> NetworkIsolationKey which is used
// to partition the HTTP cache. This key will have the following properties:
// `top_frame_site_` -> the schemeful site of the top level page.
// `frame_site_` -> std::nullopt.
// `is_cross_site_` -> a boolean indicating whether the frame site is
// schemefully cross-site from the top-level site.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableCrossSiteFlagNetworkIsolationKey);
NET_EXPORT BASE_DECLARE_FEATURE(
    kEnableFrameSiteSharedOpaqueNetworkIsolationKey);
NET_EXPORT BASE_DECLARE_FEATURE(kHttpCacheKeyingExperimentControlGroup);

// Enables sending TLS 1.3 Key Update messages on TLS 1.3 connections in order
// to ensure that this corner of the spec is exercised. This is currently
// disabled by default because we discovered incompatibilities with some
// servers.
NET_EXPORT BASE_DECLARE_FEATURE(kTLS13KeyUpdate);

// Enables permuting TLS extensions in the ClientHello, to reduce the risk of
// non-compliant servers ossifying parts of the ClientHello and interfering with
// deployment of future security improvements.
NET_EXPORT BASE_DECLARE_FEATURE(kPermuteTLSExtensions);

// Enables Kyber-based post-quantum key-agreements in TLS 1.3 connections.
NET_EXPORT BASE_DECLARE_FEATURE(kPostQuantumKyber);

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

// When enabled, bssl::TrustStore implementations will use TRUSTED_LEAF,
// TRUSTED_ANCHOR_OR_LEAF, and TRUSTED_ANCHOR as appropriate. When disabled,
// bssl::TrustStore implementation will only use TRUSTED_ANCHOR.
// TODO(https://crbug.com/1403034): remove this a few milestones after the
// trusted leaf support has been launched on all relevant platforms.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(USE_NSS_CERTS) || BUILDFLAG(IS_WIN)
NET_EXPORT BASE_DECLARE_FEATURE(kTrustStoreTrustedLeafSupport);
#endif

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

// When this feature is enabled, the network service will wait until First-Party
// Sets are initialized before issuing requests that use the HTTP cache or
// cookies.
NET_EXPORT BASE_DECLARE_FEATURE(kWaitForFirstPartySetsInit);

// Controls the maximum time duration an outermost frame navigation should be
// deferred by RWS initialization.
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kWaitForFirstPartySetsInitNavigationThrottleTimeout;

// When enabled, a cross-site ancestor chain bit is included in the partition
// key in partitioned cookies.
NET_EXPORT BASE_DECLARE_FEATURE(kAncestorChainBitEnabledInPartitionedCookies);

// When enabled, cookie-related code will treat cookies containing '\0', '\r',
// and '\n' as invalid and reject the cookie.
NET_EXPORT BASE_DECLARE_FEATURE(kBlockTruncatedCookies);

// Controls whether static key pinning is enforced.
NET_EXPORT BASE_DECLARE_FEATURE(kStaticKeyPinningEnforcement);

// When enabled, cookies with a non-ASCII domain attribute will be rejected.
NET_EXPORT BASE_DECLARE_FEATURE(kCookieDomainRejectNonASCII);

NET_EXPORT BASE_DECLARE_FEATURE(kThirdPartyStoragePartitioning);
NET_EXPORT BASE_DECLARE_FEATURE(kSupportPartitionedBlobUrl);

// Feature to enable consideration of 3PC deprecation trial settings.
NET_EXPORT BASE_DECLARE_FEATURE(kTpcdTrialSettings);

// Feature to enable consideration of top-level 3PC deprecation trial settings.
NET_EXPORT BASE_DECLARE_FEATURE(kTopLevelTpcdTrialSettings);

// Whether to enable the use of 3PC based on 3PCD metadata grants delivered via
// component updater.
NET_EXPORT BASE_DECLARE_FEATURE(kTpcdMetadataGrants);

// Whether to enable staged rollback of the TPCD Metadata Entries.
NET_EXPORT BASE_DECLARE_FEATURE(kTpcdMetadataStagedRollback);

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

// Whether or not to use the GetNetworkConnectivityHint API on modern Windows
// versions for the Network Change Notifier.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableGetNetworkConnectivityHintAPI);
#endif

// Prefetch to follow normal semantics instead of 5-minute rule
// https://crbug.com/1345207
NET_EXPORT BASE_DECLARE_FEATURE(kPrefetchFollowsNormalCacheSemantics);

// A flag for new Kerberos feature, that suggests new UI
// when Kerberos authentication in browser fails on ChromeOS.
// b/260522530
#if BUILDFLAG(IS_CHROMEOS)
NET_EXPORT BASE_DECLARE_FEATURE(kKerberosInBrowserRedirect);
#endif

// A flag to use asynchronous session creation for new QUIC sessions.
NET_EXPORT BASE_DECLARE_FEATURE(kAsyncQuicSession);

// A flag to make multiport context creation asynchronous.
NET_EXPORT BASE_DECLARE_FEATURE(kAsyncMultiPortPath);

// Enables custom proxy configuration for the IP Protection experimental proxy.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableIpProtectionProxy);

// Sets the name of the IP protection auth token server.
NET_EXPORT extern const base::FeatureParam<std::string> kIpPrivacyTokenServer;

// Sets the path component of the IP protection auth token server URL used for
// getting initial token signing data.
NET_EXPORT extern const base::FeatureParam<std::string>
    kIpPrivacyTokenServerGetInitialDataPath;

// Sets the path component of the IP protection auth token server URL used for
// getting blind-signed tokens.
NET_EXPORT extern const base::FeatureParam<std::string>
    kIpPrivacyTokenServerGetTokensPath;

// Sets the path component of the IP protection auth token server URL used for
// getting proxy configuration.
NET_EXPORT extern const base::FeatureParam<std::string>
    kIpPrivacyTokenServerGetProxyConfigPath;

// Sets the batch size to fetch new auth tokens for IP protection.
NET_EXPORT extern const base::FeatureParam<int>
    kIpPrivacyAuthTokenCacheBatchSize;

// Sets the cache low-water-mark for auth tokens for IP protection.
NET_EXPORT extern const base::FeatureParam<int>
    kIpPrivacyAuthTokenCacheLowWaterMark;

// Sets the normal time between fetches of the IP protection proxy list.
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kIpPrivacyProxyListFetchInterval;

// Sets the minimum time between fetches of the IP protection proxy list, such
// as when a re-fetch is forced due to an error.
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kIpPrivacyProxyListMinFetchInterval;

// Overrides the ProxyA hostname normally set by the proxylist fetch.
NET_EXPORT extern const base::FeatureParam<std::string>
    kIpPrivacyProxyAHostnameOverride;

// Overrides the ProxyB hostname normally set by the proxylist fetch.
NET_EXPORT extern const base::FeatureParam<std::string>
    kIpPrivacyProxyBHostnameOverride;

// Controls whether IP Protection _proxying_ is bypassed by not including any
// of the proxies in the proxy list. This supports experimental comparison of
// connections that _would_ have been proxied, but were not.
NET_EXPORT extern const base::FeatureParam<bool> kIpPrivacyDirectOnly;

// The PSK added to connections to proxyB with `Proxy-Authorization: Preshared
// $PSK`.
NET_EXPORT extern const base::FeatureParam<std::string> kIpPrivacyProxyBPsk;

// If true, pass OAuth token to Phosphor in GetProxyConfig API for IP
// Protection.
NET_EXPORT extern const base::FeatureParam<bool>
    kIpPrivacyIncludeOAuthTokenInGetProxyConfig;

// Controls whether a header ("IP-Protection: 1") should be added to proxied
// network requests.
NET_EXPORT extern const base::FeatureParam<bool>
    kIpPrivacyAddHeaderToProxiedRequests;

// Token expirations will have a random time between 5 seconds and this delta
// subtracted from their expiration, in order to even out the load on the token
// servers.
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kIpPrivacyExpirationFuzz;

// If true, only proxy traffic when the top-level site uses the http:// or
// https:// schemes. This prevents attempts to proxy from top-level sites with
// chrome://, chrome-extension://, or other non-standard schemes, in addition to
// top-level sites using less common schemes like blob:// and data://.
NET_EXPORT extern const base::FeatureParam<bool>
    kIpPrivacyRestrictTopLevelSiteSchemes;

// If true, IP protection will attempt to use QUIC to connect to proxies,
// falling back to HTTPS.  If false, it will only use HTTPs.
NET_EXPORT extern const base::FeatureParam<bool> kIpPrivacyUseQuicProxies;

// If true, IP protection will only use QUIC to connect to proxies, with no
// fallback to HTTPS. This is intended for development of the QUIC
// functionality.
NET_EXPORT extern const base::FeatureParam<bool> kIpPrivacyUseQuicProxiesOnly;

// Truncate IP protection proxy chains to a single proxy. This is intended for
// development of the QUIC functionality.
NET_EXPORT extern const base::FeatureParam<bool> kIpPrivacyUseSingleProxy;

// Send all traffic to this host via IP Protection proxies, regardless of MDL,
// 1P/3P, or token availability. This is intended for development of the QUIC
// functionality.
NET_EXPORT extern const base::FeatureParam<std::string> kIpPrivacyAlwaysProxy;

// Whether QuicParams::migrate_sessions_on_network_change_v2 defaults to true or
// false. This is needed as a workaround to set this value to true on Android
// but not on WebView (until crbug.com/1430082 has been fixed).
NET_EXPORT BASE_DECLARE_FEATURE(kMigrateSessionsOnNetworkChangeV2);

// Enables whether blackhole detector should be disabled during connection
// migration and there is no available network.
NET_EXPORT BASE_DECLARE_FEATURE(kDisableBlackholeOnNoNewNetwork);

#if BUILDFLAG(IS_LINUX)
// AddressTrackerLinux will not run inside the network service in this
// configuration, which will improve the Linux network service sandbox.
// TODO(crbug.com/1312226): remove this.
NET_EXPORT BASE_DECLARE_FEATURE(kAddressTrackerLinuxIsProxied);
#endif  // BUILDFLAG(IS_LINUX)

// Enables binding of cookies to the port that originally set them by default.
NET_EXPORT BASE_DECLARE_FEATURE(kEnablePortBoundCookies);

// Enables binding of cookies to the scheme that originally set them. Also
// enables domain cookie shadowing protection.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableSchemeBoundCookies);

// Enables expiration duration limit (3 hours) for cookies on insecure websites.
// This feature is a no-op unless kEnableSchemeBoundCookies is enabled.
NET_EXPORT BASE_DECLARE_FEATURE(kTimeLimitedInsecureCookies);

// Enables enabling third-party cookie blocking from the command line.
NET_EXPORT BASE_DECLARE_FEATURE(kForceThirdPartyCookieBlocking);

// Enables an exception for third-party cookie blocking when the request is
// same-site with the top-level document, opted into CORS, but embedded in a
// cross-site context.
NET_EXPORT BASE_DECLARE_FEATURE(kThirdPartyCookieTopLevelSiteCorsException);

// Enables Early Hints on HTTP/1.1.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableEarlyHintsOnHttp11);

// Enables draft-07 version of WebTransport over HTTP/3.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableWebTransportDraft07);

// Enables Zstandard Content-Encoding support.
NET_EXPORT BASE_DECLARE_FEATURE(kZstdContentEncoding);

NET_EXPORT BASE_DECLARE_FEATURE(kThirdPartyPartitionedStorageAllowedByDefault);

// Enables the HTTP extensible priorities "priority" header.
// RFC 9218
NET_EXPORT BASE_DECLARE_FEATURE(kPriorityHeader);

// Enables a more efficient implementation of SpdyHeadersToHttpResponse().
NET_EXPORT BASE_DECLARE_FEATURE(kSpdyHeadersToHttpResponseUseBuilder);

// Enables receiving ECN bit by sockets in Chrome.
NET_EXPORT BASE_DECLARE_FEATURE(kReceiveEcn);

// Enables using the new ALPS codepoint to negotiate application settings for
// HTTP2.
NET_EXPORT BASE_DECLARE_FEATURE(kUseNewAlpsCodepointHttp2);

// Enables using the new ALPS codepoint to negotiate application settings for
// QUIC.
NET_EXPORT BASE_DECLARE_FEATURE(kUseNewAlpsCodepointQUIC);

// Treat HTTP header `Expires: "0"` as expired value according section 5.3 on
// RFC 9111.
// TODO(https://crbug.com/853508): Remove after the bug fix will go well for a
// while on stable channels.
NET_EXPORT BASE_DECLARE_FEATURE(kTreatHTTPExpiresHeaderValueZeroAsExpired);

// Enables truncating the response body to the content length.
NET_EXPORT BASE_DECLARE_FEATURE(kTruncateBodyToContentLength);

#if BUILDFLAG(IS_MAC)
// Reduces the frequency of IP address change notifications that result in
// TCP and QUIC connection resets.
NET_EXPORT BASE_DECLARE_FEATURE(kReduceIPAddressChangeNotification);
#endif  // BUILDFLAG(IS_MAC)

// This feature will enable the Device Bound Session Credentials protocol to let
// the server assert sessions (and cookies) are bound to a specific device.
NET_EXPORT BASE_DECLARE_FEATURE(kDeviceBoundSessions);

}  // namespace net::features

#endif  // NET_BASE_FEATURES_H_
