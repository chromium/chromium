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
#include "net/disk_cache/buildflags.h"
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

// Enables partial support for Structured DNS Errors
// (draft-ietf-dnsop-structured-dns-error). When enabled, the Chrome DNS
// resolver will indicate support for structured extended errors in outgoing DNS
// requests, render EDNS error codes on the error page, and populate filtering
// details when provided as a structured error
// (draft-nottingham-public-resolver-errors).
NET_EXPORT BASE_DECLARE_FEATURE(kUseStructuredDnsErrors);

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

// If enabled, avoids aborting connections in response to adding or removing an
// IPv6 temporary address.
NET_EXPORT BASE_DECLARE_FEATURE(kMaintainConnectionsOnIpv6TempAddrChange);

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

// Splits the generated code cache by the request's NetworkIsolationKey if one
// is available. Note that this feature is also gated behind
// `net::HttpCache::IsSplitCacheEnabled()`.
NET_EXPORT BASE_DECLARE_FEATURE(kSplitCodeCacheByNetworkIsolationKey);

// Partitions connections and other network states based on the
// NetworkAnonymizationKey associated with a request.
// See https://github.com/MattMenke2/Explainer---Partition-Network-State.
NET_EXPORT BASE_DECLARE_FEATURE(kPartitionConnectionsByNetworkIsolationKey);

// "__Http-" prefix for cookies.
// https://github.com/httpwg/http-extensions/pull/3110
NET_EXPORT BASE_DECLARE_FEATURE(kPrefixCookieHttp);

// "__HostHttp-" prefix for cookies.
// https://github.com/httpwg/http-extensions/issues/3111
NET_EXPORT BASE_DECLARE_FEATURE(kPrefixCookieHostHttp);

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

// Whether to fallback to the old preconnect interval when the device is in low
// power mode.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kFallbackInLowPowerMode);

// Enables Connection Keep-Alive feature for Http2
NET_EXPORT BASE_DECLARE_FEATURE(kConnectionKeepAliveForHttp2);

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

// Controls whether static key pinning is enforced.
NET_EXPORT BASE_DECLARE_FEATURE(kStaticKeyPinningEnforcement);

// When enabled, cookies with a non-ASCII domain attribute will be rejected.
NET_EXPORT BASE_DECLARE_FEATURE(kCookieDomainRejectNonASCII);

NET_EXPORT BASE_DECLARE_FEATURE(kThirdPartyStoragePartitioning);

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
// This was launched in M141, but the finch flag was kept around in case it
// ever causes issues (as some may take time to detect due to rarity).
NET_EXPORT BASE_DECLARE_FEATURE(kTcpPortRandomizationWin);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                      kTcpPortRandomizationWinVersionMinimum);

// Whether or not TCP port reuse timing metrics are recorded.
// See crbug.com/40744069 for more details.
NET_EXPORT BASE_DECLARE_FEATURE(kTcpPortReuseMetricsWin);

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

// Maximum report body size (KB) to include in serialized reports. Bodies
// exceeding this are omitted when kExcludeLargeBodyReports is enabled.  Use
// Reporting.ReportBodySize UMA histogram to monitor report body sizes and
// inform this value.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t, kMaxReportBodySizeKB);

// Whether QuicParams::migrate_sessions_on_network_change_v2 defaults to true or
// false. This is needed as a workaround to set this value to true on Android
// but not on WebView (until crbug.com/1430082 has been fixed).
NET_EXPORT BASE_DECLARE_FEATURE(kMigrateSessionsOnNetworkChangeV2);

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

// Enables a smarter throttling strategy based in the server's IP.
NET_EXPORT BASE_DECLARE_FEATURE(kWebTransportFineGrainedThrottling);

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

// Uses the Network framework path monitor instead of SCNetworkReachability for
// connection type change detection on macOS.
NET_EXPORT BASE_DECLARE_FEATURE(kUseNetworkPathMonitorForNetworkChangeNotifier);
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
    kDeviceBoundSessionsRequireOriginTrialTokens);
// This feature enables the Device Bound Session Credentials refresh quota.
// This behavior is expected by default; disabling it should only be for
// testing purposes.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kDeviceBoundSessionsRefreshQuota);
// This feature controls whether DBSC checks the .well-known for subdomain
// registration.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDeviceBoundSessionsCheckSubdomainRegistration);
// This feature controls the database schema version for stored sessions.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kDeviceBoundSessionsSchemaVersion);

// This feature controls whether DBSC allows federated sessions.
NET_EXPORT BASE_DECLARE_FEATURE(kDeviceBoundSessionsFederatedRegistration);
// This param controls whether DBSC checks the .well-known for federated
// sessions.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kDeviceBoundSessionsFederatedRegistrationCheckWellKnown);

// This feature controls whether to proactively trigger Device
// Bound Session refreshes when a cookie is soon to expire.
NET_EXPORT BASE_DECLARE_FEATURE(kDeviceBoundSessionProactiveRefresh);
// This controls the threshold for proactive refrehshes.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kDeviceBoundSessionProactiveRefreshThreshold);

// This feature controls whether DBSC has a signing quota instead of a refresh
// quota, and has associated signing caching for refreshes.
NET_EXPORT BASE_DECLARE_FEATURE(kDeviceBoundSessionSigningQuotaAndCaching);

// Enables more checks when creating a SpdySession for proxy. These checks are
// already applied to non-proxy SpdySession creations.
// TODO(crbug.com/343519247): Remove this once we are sure that these checks are
// not causing any problems.
NET_EXPORT BASE_DECLARE_FEATURE(kSpdySessionForProxyAdditionalChecks);

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

// If enabled, unrecognized keys in a No-Vary-Search header will be ignored.
// Otherwise, unrecognized keys are treated as if the header was invalid.
NET_EXPORT BASE_DECLARE_FEATURE(kNoVarySearchIgnoreUnrecognizedKeys);

// Enables enforcement of One-RFC6962 policy for Certificate Transparency. When
// disabled, Chrome does not distinguish between SCTs based on log type.
NET_EXPORT BASE_DECLARE_FEATURE(kEnforceOneRfc6962CtPolicy);

// Finch experiment to select a disk cache backend.
enum class DiskCacheBackend {
  kDefault,
  kSimple,
  kBlockfile,
#if BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND)
  kSql,
#endif  // ENABLE_DISK_CACHE_SQL_BACKEND
};
NET_EXPORT BASE_DECLARE_FEATURE(kDiskCacheBackendExperiment);
NET_EXPORT extern const base::FeatureParam<DiskCacheBackend>
    kDiskCacheBackendParam;

#if BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND)
// If the number of pages recorded in the WAL file of the SQL disk cache's DB
// exceeds this value, a checkpoint is executed on committing data.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                      kSqlDiskCacheForceCheckpointThreshold);
// If the number of pages recorded in the WAL file of the SQL disk cache's DB
// exceeds this value and the browser is idle, a checkpoint is executed.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                      kSqlDiskCacheIdleCheckpointThreshold);
// While the memory usage for the buffer doesn't exceed the number of bytes
// specified by this param, the SQL backend executes optimistic writes.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                      kSqlDiskCacheOptimisticWriteBufferSize);
// Disables synchronous writes in the WAL file of the SQL disk cache's DB.
// This is faster but less safe.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kSqlDiskCacheSynchronousOff);
// The number of shards for the SQL disk cache.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kSqlDiskCacheShardCount);
// Loads the in-memory index on initialization.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kSqlDiskCacheLoadIndexOnInit);
#endif  // ENABLE_DISK_CACHE_SQL_BACKEND

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

// When enabled HSTS upgrades will only apply to top-level navigations.
NET_EXPORT BASE_DECLARE_FEATURE(kHstsTopLevelNavigationsOnly);

#if BUILDFLAG(IS_WIN)
// Whether or not to flush on MappedFile::Flush().
NET_EXPORT BASE_DECLARE_FEATURE(kHttpCacheMappedFileFlushWin);
#endif

// Whether or not to apply No-Vary-Search processing in the HTTP disk cache.
NET_EXPORT BASE_DECLARE_FEATURE(kHttpCacheNoVarySearch);

NET_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t,
                                      kHttpCacheNoVarySearchCacheMaxEntries);

// Whether persistence is enabled in on-the-record profiles. True by default.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                      kHttpCacheNoVarySearchPersistenceEnabled);

// If true, don't erase the NoVarySearchCache entry when simple cache in-memory
// hints indicate that the disk cache entry is not usable.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                      kHttpCacheNoVarySearchKeepNotSuitable);

// Whether to use the new implementation of
// HttpNoVarySearchData::AreEquivalent().
NET_EXPORT BASE_DECLARE_FEATURE(kHttpNoVarySearchDataUseNewAreEquivalent);

// Whether to skip opening the http cache entry which was marked as "unusable"
// from the "Cache-Control" header point of view.
NET_EXPORT BASE_DECLARE_FEATURE(kHttpCacheSkipUnusableEntry);

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

// Finch-controlled list of ports that should be blocked due to ongoing abuse.
NET_EXPORT BASE_DECLARE_FEATURE(kRestrictAbusePorts);
NET_EXPORT extern const base::FeatureParam<std::string>
    kPortsToRestrictForAbuse;
NET_EXPORT extern const base::FeatureParam<std::string>
    kPortsToRestrictForAbuseMonitorOnly;

// Finch-controlled list of ports that should be blocked on localhost.
NET_EXPORT BASE_DECLARE_FEATURE(kRestrictAbusePortsOnLocalhost);

// Enables TLS Trust Anchor IDs
// (https://tlswg.org/tls-trust-anchor-ids/draft-ietf-tls-trust-anchor-ids.html),
// a TLS extension to help the server serve a certificate that the client will
// trust.
NET_EXPORT BASE_DECLARE_FEATURE(kTLSTrustAnchorIDs);

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
// Enables support for Merkle Tree Certificates. `kTLSTrustAnchorIDs` must also
// be enabled for this to be useful.
NET_EXPORT BASE_DECLARE_FEATURE(kVerifyMTCs);
#endif

// Indicates if the client is participating in the TCP socket pool limit
// randomization trial. The params below define the bounds for the probability.
// function we use when calculating the chance the state should flip between
// capped and uncapped.
// See crbug.com/415691664 for more details.
NET_EXPORT BASE_DECLARE_FEATURE(kTcpSocketPoolLimitRandomization);
// The base of an exponent when calculating the probability.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(double,
                                      kTcpSocketPoolLimitRandomizationBase);
// The maximum amount of additional sockets to allow use of.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                      kTcpSocketPoolLimitRandomizationCapacity);
// The minimum probability allowed to be returned.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(double,
                                      kTcpSocketPoolLimitRandomizationMinimum);
// The percentage of noise to add/subtract from the probability.
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(double,
                                      kTcpSocketPoolLimitRandomizationNoise);

// These parameters control whether the Network Service Task Scheduler is used
// for specific classes.
NET_EXPORT BASE_DECLARE_FEATURE(kNetTaskScheduler);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                      kNetTaskSchedulerHttpProxyConnectJob);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                      kNetTaskSchedulerHttpStreamFactoryJob);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(
    bool,
    kNetTaskSchedulerHttpStreamFactoryJobController);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                      kNetTaskSchedulerURLRequestErrorJob);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kNetTaskSchedulerURLRequestHttpJob);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kNetTaskSchedulerURLRequestJob);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                      kNetTaskSchedulerURLRequestRedirectJob);

NET_EXPORT BASE_DECLARE_FEATURE(kNetTaskScheduler2);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kNetTaskSchedulerHttpCache);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                      kNetTaskSchedulerHttpCacheTransaction);

// If enabled, we will add an additional delay to the main job in
// HttpStreamFactoryJobController.
NET_EXPORT BASE_DECLARE_FEATURE(kAdditionalDelayMainJob);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kAdditionalDelay);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                      kDelayMainJobWithAvailableSpdySession);

// If enabled, we will extend the quic handshake timeout.
NET_EXPORT BASE_DECLARE_FEATURE(kExtendQuicHandshakeTimeout);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kQuicHandshakeTimeout);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                                      kMaxIdleTimeBeforeCryptoHandshake);

// If enabled, we will use a longer idle timeout.
NET_EXPORT BASE_DECLARE_FEATURE(kQuicLongerIdleConnectionTimeout);

// If enabled, we will use QUIC with a smaller MTU.
NET_EXPORT BASE_DECLARE_FEATURE(kLowerQuicMaxPacketSize);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t, kQuicMaxPacketSize);

// When enabled, races QUIC connection attempts for the specified hostnames
// even when there is no available ALPN information.
NET_EXPORT BASE_DECLARE_FEATURE(kConfigureQuicHints);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(std::string, kQuicHintHostPortPairs);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(std::string,
                                      kWildcardQuicHintHostPortPairs);

// When enabled, the browser checks if a navigation URL is in any navigation
// entry. If so, it sets the
// `IS_MAIN_FRAME_ORIGIN_RECENTLY_ACCESSED` load flag.
// Note that this flag is only set for metric collection.
NET_EXPORT BASE_DECLARE_FEATURE(kUpdateIsMainFrameOriginRecentlyAccessed);
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t, kRecentlyAccessedOriginCacheSize);

// When enabled, the browser tries QUIC by default.
NET_EXPORT BASE_DECLARE_FEATURE(kTryQuicByDefault);

// The QUIC connection options which will be sent to the server in order to
// enable certain QUIC features. This should be set using `QuicTag`s (32-bit
// value represented in ASCII equivalent e.g. EXMP). To set multiple features,
// separate the values with a comma (e.g. "ABCD,EFGH").
NET_EXPORT BASE_DECLARE_FEATURE_PARAM(std::string, kQuicOptions);

NET_EXPORT BASE_DECLARE_FEATURE(kDnsResponseDiscardPartialQuestions);

// When enabled, users can make Secure DNS in AUTOMATIC mode fallback to a
// well-known DoH provider before using insecure DNS.
NET_EXPORT BASE_DECLARE_FEATURE(kAddAutomaticWithDohFallbackMode);

// If true, a CONNECT-UDP response is not needed to start sending datagrams.
NET_EXPORT BASE_DECLARE_FEATURE(
    kUseQuicProxiesWithoutWaitingForConnectResponse);

// If enabled, the configured bootstrap IP addresses of DoH providers will
// be randomized for better load balancing of the initial DoH URL lookups.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableBootstrapIPRandomizationForDoh);

}  // namespace net::features

#endif  // NET_BASE_FEATURES_H_
