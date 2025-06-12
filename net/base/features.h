// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FEATURES_H_
#define NET_BASE_FEATURES_H_

#include <stddef.h>

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

// If enabled, HostResolver will use the new HostResolverCache that separately
// caches by DNS type, unlike the old HostCache that always cached by merged
// request results. May enable related behavior such as separately sorting DNS
// results after each transaction rather than sorting collectively after all
// transactions complete.
NET_EXPORT BASE_DECLARE_FEATURE(kUseHostResolverCache);

// Enables the Happy Eyeballs v3, where we use intermediate DNS resolution
// results to make connection attempts as soon as possible.
NET_EXPORT BASE_DECLARE_FEATURE(kHappyEyeballsV3);

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

// The maximum age in seconds of observations to be used for calculating the
// HTTP RTT from the historical data.
// Negative value means infinite. i.e. all data are used.
NET_EXPORT extern const base::FeatureParam<int> kRecentHTTPThresholdInSeconds;

// The maximum age in seconds of observations to be used for calculating the
// transport RTT from the historical data.
// Negative value means infinite. i.e. all data are used.
NET_EXPORT extern const base::FeatureParam<int>
    kRecentTransportThresholdInSeconds;

// The maximum age in seconds of observations to be used for calculating the
// end to end RTT from the historical data.
// Negative value means infinite. i.e. all data are used.
NET_EXPORT extern const base::FeatureParam<int>
    kRecentEndToEndThresholdInSeconds;

// Number of observations received after which the effective connection type
// should be recomputed.
NET_EXPORT extern const base::FeatureParam<int>
    kCountNewObservationsReceivedComputeEct;

// Maximum number of observations that can be held in a single
// ObservationBuffer.
NET_EXPORT extern const base::FeatureParam<int> kObservationBufferSize;

// Minimum duration between two consecutive computations of effective
// connection type. Set to non-zero value as a performance optimization.
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kEffectiveConnectionTypeRecomputationInterval;

// Splits cache entries by the request's includeCredentials.
NET_EXPORT BASE_DECLARE_FEATURE(kSplitCacheByIncludeCredentials);

// Splits cache entries by the request's NetworkIsolationKey if one is
// available.
NET_EXPORT BASE_DECLARE_FEATURE(kSplitCacheByNetworkIsolationKey);

// This flag incorporates a boolean into the cache key that is true for
// renderer-initiated main frame navigations when the request initiator site is
// cross-site to the URL being navigated to. This provides protections against
// certain cross-site leak attacks involving cross-site navigations.
NET_EXPORT BASE_DECLARE_FEATURE(
    kSplitCacheByCrossSiteMainFrameNavigationBoolean);

// Splits the generated code cache by the request's NetworkIsolationKey if one
// is available. Note that this feature is also gated behind
// `net::HttpCache::IsSplitCacheEnabled()`.
NET_EXPORT BASE_DECLARE_FEATURE(kSplitCodeCacheByNetworkIsolationKey);

// Partitions connections and other network states based on the
// NetworkAnonymizationKey associated with a request.
// See https://github.com/MattMenke2/Explainer---Partition-Network-State.
NET_EXPORT BASE_DECLARE_FEATURE(kPartitionConnectionsByNetworkIsolationKey);

// Changes the interval between two search engine preconnect attempts.
NET_EXPORT BASE_DECLARE_FEATURE(kSearchEnginePreconnectInterval);

// Enables a more efficient SearchEnginePreconnector
NET_EXPORT BASE_DECLARE_FEATURE(kSearchEnginePreconnect2);

// The idle timeout for the SearchEnginePreconnector2 feature.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kIdleTimeoutInSeconds);

// The maximum time for the SearchEnginePreconnector2 to be considered as short.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kShortSessionThreshold);

// The maximum time to backoff when attempting preconnect retry for
// SearchEnginePreconnector2.
NET_EXPORT extern const base::FeatureParam<int> kMaxPreconnectRetryInterval;

// The interval between two QUIC ping requests for the periodic PING for
// SearchEnginePreconnector2.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kPingIntervalInSeconds);

// The QUIC connection options which will be sent to the server in order to
// enable certain QUIC features. This should be set using `QuicTag`s (32-bit
// value represented in ASCII equivalent e.g. EXMP). If we want to set
// multiple features, then the values should be separated with a comma
// (e.g. "ABCD,EFGH").
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(std::string, kQuicConnectionOptions);

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

// When this feature is enabled, redirected requests will be considered
// cross-site for the purpose of SameSite cookies if any redirect hop was
// cross-site to the target URL, even if the original initiator of the
// redirected request was same-site with the target URL (and the
// site-for-cookies).
// See spec changes in https://github.com/httpwg/http-extensions/pull/1348
NET_EXPORT BASE_DECLARE_FEATURE(kCookieSameSiteConsidersRedirectChain);

// When this feature is enabled, servers can include an
// allow-same-site-none-cookies value that notifies the browser that same-site
// SameSite=None cookies should be allowed in sandboxed contexts with 3PC
// restrictions.
NET_EXPORT BASE_DECLARE_FEATURE(kAllowSameSiteNoneCookiesInSandbox);

// When this feature is enabled, the network service will wait until First-Party
// Sets are initialized before issuing requests that use the HTTP cache or
// cookies.
NET_EXPORT BASE_DECLARE_FEATURE(kWaitForFirstPartySetsInit);

// Controls the maximum time duration an outermost frame navigation should be
// deferred by RWS initialization.
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kWaitForFirstPartySetsInitNavigationThrottleTimeout;

// When enabled, requestStorageAccessFor will require storage access permissions
// granted by StorageAccessApi or StorageAccessHeaders to send cookies on
// requests allowed because of requestStorageAccessFor instead of cors.
NET_EXPORT BASE_DECLARE_FEATURE(kRequestStorageAccessNoCorsRequired);

// When enabled, the Storage Access API follows the Same Origin Policy when
// including cookies on network requests. (I.e., a cross-site cookie is only
// included via the Storage Access API if the request's URL's origin [not site]
// has opted into receiving cross-site cookies.)
NET_EXPORT
BASE_DECLARE_FEATURE(kStorageAccessApiFollowsSameOriginPolicy);

// Controls whether static key pinning is enforced.
NET_EXPORT BASE_DECLARE_FEATURE(kStaticKeyPinningEnforcement);

// When enabled, cookies with a non-ASCII domain attribute will be rejected.
NET_EXPORT BASE_DECLARE_FEATURE(kCookieDomainRejectNonASCII);

NET_EXPORT BASE_DECLARE_FEATURE(kThirdPartyStoragePartitioning);

// Controls consideration of top-level 3PCD origin trial settings.
NET_EXPORT BASE_DECLARE_FEATURE(kTopLevelTpcdOriginTrial);

// Feature to enable consideration of 3PC deprecation trial settings.
NET_EXPORT BASE_DECLARE_FEATURE(kTpcdTrialSettings);

// Feature to enable consideration of top-level 3PC deprecation trial settings.
NET_EXPORT BASE_DECLARE_FEATURE(kTopLevelTpcdTrialSettings);

// Whether to enable the use of 3PC based on 3PCD metadata grants delivered via
// component updater.
NET_EXPORT BASE_DECLARE_FEATURE(kTpcdMetadataGrants);

// Whether to enable staged rollback of the TPCD Metadata Entries.
NET_EXPORT BASE_DECLARE_FEATURE(kTpcdMetadataStageControl);

// Whether ALPS parsing is on for any type of frame.
NET_EXPORT BASE_DECLARE_FEATURE(kAlpsParsing);

// Whether ALPS parsing is on for client hint parsing specifically.
NET_EXPORT BASE_DECLARE_FEATURE(kAlpsClientHintParsing);

// Whether to kill the session on Error::kAcceptChMalformed.
NET_EXPORT BASE_DECLARE_FEATURE(kShouldKillSessionOnAcceptChMalformed);

NET_EXPORT BASE_DECLARE_FEATURE(kEnableWebsocketsOverHttp3);

#if BUILDFLAG(IS_WIN)
// Whether or not to use the GetNetworkConnectivityHint API on modern Windows
// versions for the Network Change Notifier.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableGetNetworkConnectivityHintAPI);

// Whether or not to enable TCP port randomization via SO_RANDOMIZE_PORT on
// Windows for versions >= kTcpPortRandomizationWinVersionMinimum.
// See crbug.com/40744069 for more details.
NET_EXPORT BASE_DECLARE_FEATURE(kTcpPortRandomizationWin);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                      kTcpPortRandomizationWinVersionMinimum);

// Whether to use a TCP socket implementation which uses an IO completion
// handler to be notified of completed reads and writes, instead of an event.
NET_EXPORT BASE_DECLARE_FEATURE(kTcpSocketIoCompletionPortWin);
#endif

// Avoid creating cache entries for transactions that are most likely no-store.
NET_EXPORT BASE_DECLARE_FEATURE(kAvoidEntryCreationForNoStore);
NET_EXPORT extern const base::FeatureParam<int>
    kAvoidEntryCreationForNoStoreCacheSize;

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

// Enables the Probabilistic Reveal Tokens feature.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableProbabilisticRevealTokens);

// Sets the name of the probabilistic reveal token issuer server.
NET_EXPORT extern const base::FeatureParam<std::string>
    kProbabilisticRevealTokenServer;

// Sets the path of the probabilistic reveal token server URL used for issuing
// tokens.
NET_EXPORT extern const base::FeatureParam<std::string>
    kProbabilisticRevealTokenServerPath;

// If true, the probabilistic reveal token registration check will be skipped
// and we will consider every domain as being eligible to receive PRTs. In order
// for PRTs to be attached to requests, the
// `ProbabilisticRevealTokensAddHeaderToProxiedRequests` flag must also be true.
NET_EXPORT extern const base::FeatureParam<bool>
    kBypassProbabilisticRevealTokenRegistry;

// If true, the standard probabilistic reveal token registry will be ignored and
// the custom registry will be used instead. The custom registry can be set with
// the `CustomProbabilisticRevealTokenRegistry` flag. This will only be used if
// `BypassProbabilisticRevealTokenRegistry` is false. This is intended to be
// used for developer testing only.
NET_EXPORT extern const base::FeatureParam<bool>
    kUseCustomProbabilisticRevealTokenRegistry;

// A comma-separated list of domains (eTLD+1) which will be considered eligible
// to receive PRTs. This will override the default PRT registry and will only be
// used if `UseCustomProbabilisticRevealTokenRegistry` is true and
// `BypassProbabilisticRevealTokenRegistry` is false. This is intended to be
// used for developer testing only.
NET_EXPORT extern const base::FeatureParam<std::string>
    kCustomProbabilisticRevealTokenRegistry;

// If true, probabilistic reveal tokens will only be enabled in Incognito mode.
NET_EXPORT extern const base::FeatureParam<bool>
    kProbabilisticRevealTokensOnlyInIncognito;

// If true, probabilistic reveal tokens will only be fetched. PRTs will not be
// randomized at request time or attached to any requests. This is intended to
// be used for measuring issuer server load before the feature is fully enabled.
NET_EXPORT extern const base::FeatureParam<bool>
    kProbabilisticRevealTokenFetchOnly;

// If true, probabilistic reveal tokens can be attached to non-proxied requests
// as well. PRTs will still only be attached to requests if the
// `ProbabilisticRevealTokensAddHeaderToProxiedRequests` flag is true and the
// request is being sent to a registered domain, but this flag can be used in
//  combination with `BypassProbabilisticRevealTokenRegistry` or
// `CustomProbabilisticRevealTokenRegistry`. This is intended to be used for
// developer testing only.
NET_EXPORT extern const base::FeatureParam<bool>
    kEnableProbabilisticRevealTokensForNonProxiedRequests;

// If true, probabilistic reveal tokens header will be added to proxied
// requests.
NET_EXPORT extern const base::FeatureParam<bool>
    kProbabilisticRevealTokensAddHeaderToProxiedRequests;

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

// Fetches of the IP Protection proxy list will have a random time in the range
// of plus or minus this delta added to their interval.
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kIpPrivacyProxyListFetchIntervalFuzz;

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

// If true, pass OAuth token to Phosphor in GetProxyConfig API for IP
// Protection. This is used by E2E tests to ensure a stable geo for tokens
// and proxy config.
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

// Backoff time applied when fetching tokens from the IP Protection auth
// token server encounters an error indicating that the primary account is not
// eligible (e.g., user is signed in but not eligible for IP protection) or
// a 403 (FORBIDDEN) status code (e.g., quota exceeded).
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kIpPrivacyTryGetAuthTokensNotEligibleBackoff;

// Backoff time applied when fetching tokens from the IP Protection auth
// token server encounters a transient error, such as a failure to fetch
// an OAuth token for a primary account or a network issue.
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kIpPrivacyTryGetAuthTokensTransientBackoff;

// Backoff time applied when fetching tokens from the IP Protection auth
// token server encounters a 400 (BAD REQUEST) or 401 (UNAUTHORIZED) status code
// which suggests a bug.
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kIpPrivacyTryGetAuthTokensBugBackoff;

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

// Fallback to direct when connections to IP protection proxies fail. This
// defaults to true and is intended for development of the QUIC functionality.
NET_EXPORT extern const base::FeatureParam<bool> kIpPrivacyFallbackToDirect;

// Identifier for an experiment arm, to be sent to IP Protection proxies and the
// token server in the `Ip-Protection-Debug-Experiment-Arm` header. The default
// value, 0, is not sent.
NET_EXPORT extern const base::FeatureParam<int> kIpPrivacyDebugExperimentArm;

// When enabled and an IP protection delegate can be be created in the
// `NetworkContext`, a `IpProtectionProxyDelegate` will ALWAYS be created even
// for `NetworkContexts` that do not participate in IP protection. This is
// necessary for the WebView traffic experiment. By default, this feature param
// is false and will not create a delegate when IP protection is not enabled.
// Further, this also prevents the unnecessary instantiation of the
// `IpProtectionCore` for a `NetworkContext` that does not participate in IP
// protection.
NET_EXPORT extern const base::FeatureParam<bool> kIpPrivacyAlwaysCreateCore;

// Enables IP protection in incognito mode only. The default value of this
// feature is false, which maintains the existing behavior when
// `kEnableIpProtectionProxy` is enabled, IPP is enabled in both regular and
// incognito browsing sessions. When set to true, the main profile Network
// Context won't proxy traffic using IP Protection.
NET_EXPORT extern const base::FeatureParam<bool> kIpPrivacyOnlyInIncognito;

// Enables the ability to detect when a user has requests being actively
// proxied by IP Protection and thus allowing the user to made aware and offer
// the ability to bypass IP Protection via the User Bypass UX.
NET_EXPORT extern const base::FeatureParam<bool> kIpPrivacyEnableUserBypass;

// If true, IP Protection will be disabled by default for enterprise users.
// Otherwise, IP Protection will be enabled by default for enterprise users (but
// can still be opted out of via enterprise policy). This is intended to be used
// as a kill-switch in case significant enterprise breakage is encountered
// during the IP Protection rollout. Note that this has no effect unless the
// `kEnableIpProtectionProxy` feature is enabled.
// TODO(https://crbug.com/41496985): Remove this feature a few milestones after
// launch assuming no major enterprise breakage is encountered.
NET_EXPORT extern const base::FeatureParam<bool>
    kIpPrivacyDisableForEnterpriseByDefault;

// Maximum report body size (KB) to include in serialized reports. Bodies
// exceeding this are omitted when kExcludeLargeBodyReports is enabled.  Use
// Reporting.ReportBodySize UMA histogram to monitor report body sizes and
// inform this value.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t, kMaxReportBodySizeKB);

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
// TODO(crbug.com/40220507): remove this.
NET_EXPORT BASE_DECLARE_FEATURE(kAddressTrackerLinuxIsProxied);
#endif  // BUILDFLAG(IS_LINUX)

// Enables binding of cookies to the port that originally set them by default.
NET_EXPORT BASE_DECLARE_FEATURE(kEnablePortBoundCookies);

// Enables binding of cookies to the scheme that originally set them. Also
// enables domain cookie shadowing protection.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableSchemeBoundCookies);

// Disallows cookies to have non ascii values in their name or value.
NET_EXPORT BASE_DECLARE_FEATURE(kDisallowNonAsciiCookies);

// Enables expiration duration limit (3 hours) for cookies on insecure websites.
// This feature is a no-op unless kEnableSchemeBoundCookies is enabled.
NET_EXPORT BASE_DECLARE_FEATURE(kTimeLimitedInsecureCookies);

// Enables enabling third-party cookie blocking from the command line.
NET_EXPORT BASE_DECLARE_FEATURE(kForceThirdPartyCookieBlocking);

// Enables Early Hints on HTTP/1.1.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableEarlyHintsOnHttp11);

// Enables draft-07 version of WebTransport over HTTP/3.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableWebTransportDraft07);

NET_EXPORT BASE_DECLARE_FEATURE(kThirdPartyPartitionedStorageAllowedByDefault);

// Enables a more efficient implementation of SpdyHeadersToHttpResponse().
NET_EXPORT BASE_DECLARE_FEATURE(kSpdyHeadersToHttpResponseUseBuilder);

// Enables using the new ALPS codepoint to negotiate application settings for
// HTTP2.
NET_EXPORT BASE_DECLARE_FEATURE(kUseNewAlpsCodepointHttp2);

// Enables using the new ALPS codepoint to negotiate application settings for
// QUIC.
NET_EXPORT BASE_DECLARE_FEATURE(kUseNewAlpsCodepointQUIC);

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
// This feature will enable the browser to persist Device Bound Session data
// across restarts. This feature is only valid if `kDeviceBoundSessions` is
// enabled.
NET_EXPORT BASE_DECLARE_FEATURE(kPersistDeviceBoundSessions);
// This feature will enable the Device Bound Session Credentials
// protocol on all pages, ignoring the requirements for Origin Trial
// headers. This is required because we cannot properly add the origin
// trial header due to the circumstances outlined in
// https://crbug.com/40860522. An EmbeddedTestServer cannot reliably be
// started on one origin due to port randomization, an Origin Trial
// cannot be generated dynamically, and a URLLoaderInterceptor will mock
// the exact code we need to test.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDeviceBoundSessionsForceEnableForTesting);
// This feature enables the Device Bound Session Credentials refresh quota.
// This behavior is expected by default; disabling it should only be for
// testing purposes.
NET_EXPORT BASE_DECLARE_FEATURE(kDeviceBoundSessionsRefreshQuota);

// When enabled, all proxies in a proxy chain are partitioned by the NAK for the
// endpoint of the connection. When disabled, proxies carrying tunnels to other
// proxies (i.e., all proxies but the last one in the ProxyChain) are not
// partitioned, allowing greater connection re-use.
NET_EXPORT BASE_DECLARE_FEATURE(kPartitionProxyChains);

// Enables more checks when creating a SpdySession for proxy. These checks are
// already applied to non-proxy SpdySession creations.
// TODO(crbug.com/343519247): Remove this once we are sure that these checks are
// not causing any problems.
NET_EXPORT BASE_DECLARE_FEATURE(kSpdySessionForProxyAdditionalChecks);

// When this feature is enabled, Chromium can use stored shared dictionaries
// even when the connection is using HTTP/1 for non-localhost requests.
NET_EXPORT BASE_DECLARE_FEATURE(kCompressionDictionaryTransportOverHttp1);

// When this feature is enabled, Chromium can use stored shared dictionaries
// even when the connection is using HTTP/2 for non-localhost requests.
NET_EXPORT BASE_DECLARE_FEATURE(kCompressionDictionaryTransportOverHttp2);

// When this feature is enabled, Chromium will use stored shared dictionaries
// only if the request URL is a localhost URL or the transport layer is using a
// certificate rooted at a standard CA root.
NET_EXPORT BASE_DECLARE_FEATURE(
    kCompressionDictionaryTransportRequireKnownRootCert);

// Enables enterprises to use the Reporting API to collect 3PCD-related
// issues from sites used in their organization.
NET_EXPORT BASE_DECLARE_FEATURE(kReportingApiEnableEnterpriseCookieIssues);

// Use the simdutf library to base64 decode data: URLs.
NET_EXPORT BASE_DECLARE_FEATURE(kSimdutfBase64Support);

// Further optimize parsing data: URLs.
NET_EXPORT BASE_DECLARE_FEATURE(kFurtherOptimizeParsingDataUrls);

// Enables support for codepoints defined in draft-ietf-tls-tls13-pkcs1, which
// enable RSA keys to be used with client certificates even if they do not
// support RSA-PSS.
NET_EXPORT BASE_DECLARE_FEATURE(kLegacyPKCS1ForTLS13);

// Keep whitespace for non-base64 encoded data: URLs.
NET_EXPORT BASE_DECLARE_FEATURE(kKeepWhitespaceForDataUrls);

// If enabled, unrecognized keys in a No-Vary-Search header will be ignored.
// Otherwise, unrecognized keys are treated as if the header was invalid.
NET_EXPORT BASE_DECLARE_FEATURE(kNoVarySearchIgnoreUnrecognizedKeys);

// If enabled, then a cookie entry containing both encrypted and plaintext
// values is considered invalid, and the entire eTLD group will be dropped.
NET_EXPORT BASE_DECLARE_FEATURE(kEncryptedAndPlaintextValuesAreInvalid);

// Kill switch for Static CT Log (aka Tiled Log aka Sunlight)
// enforcements in Certificate Transparency policy checks. If disabled, SCTs
// from Static CT Logs will simply be ignored.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableStaticCTAPIEnforcement);

// Finch experiment to select a disk cache backend.
enum class DiskCacheBackend {
  kSimple,
  kBlockfile,
};
NET_EXPORT BASE_DECLARE_FEATURE(kDiskCacheBackendExperiment);
NET_EXPORT extern const base::FeatureParam<DiskCacheBackend>
    kDiskCacheBackendParam;

// If enabled, ignore Strict-Transport-Security for [*.]localhost hosts.
NET_EXPORT BASE_DECLARE_FEATURE(kIgnoreHSTSForLocalhost);

// If enabled, main frame navigation resources will be prioritized in Simple
// Cache. So they will be less likely to be evicted.
NET_EXPORT BASE_DECLARE_FEATURE(kSimpleCachePrioritizedCaching);
// This is a factor by which we divide the size of an entry that has the
// HINT_HIGH_PRIORITY flag set to prioritize it for eviction to be less likely
// evicted.
NET_EXPORT extern const base::FeatureParam<int>
    kSimpleCachePrioritizedCachingPrioritizationFactor;
// The period of time that the entry with HINT_HIGH_PRIORITY flag is considered
// prioritized.
NET_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSimpleCachePrioritizedCachingPrioritizationPeriod;

#if BUILDFLAG(USE_NSS_CERTS)
// If enabled, use new implementation of client cert path building.
NET_EXPORT BASE_DECLARE_FEATURE(kNewClientCertPathBuilding);
#endif  // BUILDFLAG(USE_NSS_CERTS)

// When enabled HSTS upgrades will only apply to top-level navigations.
NET_EXPORT BASE_DECLARE_FEATURE(kHstsTopLevelNavigationsOnly);

// Whether or not to apply No-Vary-Search processing in the HTTP disk cache.
NET_EXPORT BASE_DECLARE_FEATURE(kHttpCacheNoVarySearch);

NET_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t,
                                      kHttpCacheNoVarySearchCacheMaxEntries);

// Enables sending the CORS Origin header on the POST request for Reporting API
// report uploads.
NET_EXPORT BASE_DECLARE_FEATURE(kReportingApiCorsOriginHeader);

// Enables exclusion of reports having large body during serialized reports.
// When enabled, report bodies exceeding kMaxReportBodySizeKB are omitted. This
// helps prevent excessively large reports json stringification.
NET_EXPORT BASE_DECLARE_FEATURE(kExcludeLargeBodyReports);

// Enables the Related Website Partition API, allowing members of a Related
// Website Set to access partitioned non-cookie storage. See
// https://github.com/explainers-by-googlers/related-website-partition-api.
NET_EXPORT BASE_DECLARE_FEATURE(kRelatedWebsitePartitionAPI);

#if BUILDFLAG(IS_ANDROID)
// If enabled, Android OS's certificate verification (CertVerifyProcAndroid) is
// done using the certificate transparency aware API.
NET_EXPORT BASE_DECLARE_FEATURE(kUseCertTransparencyAwareApiForOsCertVerify);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables a special interstitial for self signed cert errors in local network
// URLs.
NET_EXPORT BASE_DECLARE_FEATURE(kSelfSignedLocalNetworkInterstitial);

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
// If enabled, server certificates that successfully verify and that identify
// as QWACs will be verified against the 1-QWAC specification as well.
NET_EXPORT BASE_DECLARE_FEATURE(kVerifyQWACs);
#endif

#if BUILDFLAG(IS_MAC)
// If enabled, includes deprecated APIs for looking up client certificates on
// macOS. This is disabled by default and is available as an emergency kill
// switch.
// TODO(crbug.com/40233280): This will reach stable in M137 (May 2025). Remove
// this flag sometime after August 2025.
NET_EXPORT BASE_DECLARE_FEATURE(kIncludeDeprecatedClientCertLookup);
#endif

// Finch-controlled list of ports that should be blocked due to ongoing abuse.
NET_EXPORT BASE_DECLARE_FEATURE(kRestrictAbusePorts);
NET_EXPORT extern const base::FeatureParam<std::string>
    kPortsToRestrictForAbuse;
NET_EXPORT extern const base::FeatureParam<std::string>
    kPortsToRestrictForAbuseMonitorOnly;

// Finch-controlled list of ports that should be blocked on localhost.
NET_EXPORT BASE_DECLARE_FEATURE(kRestrictAbusePortsOnLocalhost);

}  // namespace net::features

#endif  // NET_BASE_FEATURES_H_
