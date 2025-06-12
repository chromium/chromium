// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/features.h"

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "net/base/cronet_buildflags.h"
#include "net/net_buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace net::features {

BASE_FEATURE(kAlpsForHttp2, "AlpsForHttp2", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAvoidH2Reprioritization,
             "AvoidH2Reprioritization",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCapReferrerToOriginOnCrossOrigin,
             "CapReferrerToOriginOnCrossOrigin",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAsyncDns,
             "AsyncDns",
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kDnsTransactionDynamicTimeouts,
             "DnsTransactionDynamicTimeouts",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<double> kDnsTransactionTimeoutMultiplier{
    &kDnsTransactionDynamicTimeouts, "DnsTransactionTimeoutMultiplier", 7.5};

const base::FeatureParam<base::TimeDelta> kDnsMinTransactionTimeout{
    &kDnsTransactionDynamicTimeouts, "DnsMinTransactionTimeout",
    base::Seconds(12)};

BASE_FEATURE(kUseDnsHttpsSvcb,
             "UseDnsHttpsSvcb",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool> kUseDnsHttpsSvcbEnforceSecureResponse{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbEnforceSecureResponse", false};

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbInsecureExtraTimeMax{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbInsecureExtraTimeMax",
    base::Milliseconds(50)};

const base::FeatureParam<int> kUseDnsHttpsSvcbInsecureExtraTimePercent{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbInsecureExtraTimePercent", 20};

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbInsecureExtraTimeMin{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbInsecureExtraTimeMin",
    base::Milliseconds(5)};

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbSecureExtraTimeMax{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbSecureExtraTimeMax",
    base::Milliseconds(50)};

const base::FeatureParam<int> kUseDnsHttpsSvcbSecureExtraTimePercent{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbSecureExtraTimePercent", 20};

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbSecureExtraTimeMin{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbSecureExtraTimeMin",
    base::Milliseconds(5)};

BASE_FEATURE(kUseHostResolverCache,
             "UseHostResolverCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHappyEyeballsV3,
             "HappyEyeballsV3",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kAlternativePortForGloballyReachableCheck{
    &kUseAlternativePortForGloballyReachableCheck,
    "AlternativePortForGloballyReachableCheck", 443};

BASE_FEATURE(kUseAlternativePortForGloballyReachableCheck,
             "UseAlternativePortForGloballyReachableCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableIPv6ReachabilityOverride,
             "EnableIPv6ReachabilityOverride",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableTLS13EarlyData,
             "EnableTLS13EarlyData",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNetworkQualityEstimator,
             "NetworkQualityEstimator",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kRecentHTTPThresholdInSeconds{
    &kNetworkQualityEstimator, "RecentHTTPThresholdInSeconds", -1};
const base::FeatureParam<int> kRecentTransportThresholdInSeconds{
    &kNetworkQualityEstimator, "RecentTransportThresholdInSeconds", -1};
const base::FeatureParam<int> kRecentEndToEndThresholdInSeconds{
    &kNetworkQualityEstimator, "RecentEndToEndThresholdInSeconds", -1};
const base::FeatureParam<int> kCountNewObservationsReceivedComputeEct{
    &kNetworkQualityEstimator, "CountNewObservationsReceivedComputeEct", 50};
const base::FeatureParam<int> kObservationBufferSize{
    &kNetworkQualityEstimator, "ObservationBufferSize", 300};
const base::FeatureParam<base::TimeDelta>
    kEffectiveConnectionTypeRecomputationInterval{
        &kNetworkQualityEstimator,
        "EffectiveConnectionTypeRecomputationInterval", base::Seconds(10)};

BASE_FEATURE(kSplitCacheByIncludeCredentials,
             "SplitCacheByIncludeCredentials",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSplitCacheByNetworkIsolationKey,
             "SplitCacheByNetworkIsolationKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Note: Use of this feature is gated on the HTTP cache itself being
// partitioned, which is controlled by the kSplitCacheByNetworkIsolationKey
// feature.
BASE_FEATURE(kSplitCacheByCrossSiteMainFrameNavigationBoolean,
             "SplitCacheByCrossSiteMainFrameNavigationBoolean",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSplitCodeCacheByNetworkIsolationKey,
             "SplitCodeCacheByNetworkIsolationKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPartitionConnectionsByNetworkIsolationKey,
             "PartitionConnectionsByNetworkIsolationKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSearchEnginePreconnectInterval,
             "SearchEnginePreconnectInterval",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSearchEnginePreconnect2,
             "SearchEnginePreconnect2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kIdleTimeoutInSeconds,
                   &kSearchEnginePreconnect2,
                   "IdleTimeoutInSeconds",
                   120);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kShortSessionThreshold,
                   &kSearchEnginePreconnect2,
                   "MaxShortSessionThreshold",
                   base::Seconds(30));

extern const base::FeatureParam<int> kMaxPreconnectRetryInterval(
    &kSearchEnginePreconnect2,
    "MaxPreconnectRetryInterval",
    30);

BASE_FEATURE_PARAM(int,
                   kPingIntervalInSeconds,
                   &kSearchEnginePreconnect2,
                   "PingIntervalInSeconds",
                   30);

BASE_FEATURE_PARAM(std::string,
                   kQuicConnectionOptions,
                   &kSearchEnginePreconnect2,
                   "QuicConnectionOptions",
                   "");

BASE_FEATURE(kShortLaxAllowUnsafeThreshold,
             "ShortLaxAllowUnsafeThreshold",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSameSiteDefaultChecksMethodRigorously,
             "SameSiteDefaultChecksMethodRigorously",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLimitOpenUDPSockets,
             "LimitOpenUDPSockets",
             base::FEATURE_ENABLED_BY_DEFAULT);

extern const base::FeatureParam<int> kLimitOpenUDPSocketsMax(
    &kLimitOpenUDPSockets,
    "LimitOpenUDPSocketsMax",
    6000);

BASE_FEATURE(kTimeoutTcpConnectAttempt,
             "TimeoutTcpConnectAttempt",
             base::FEATURE_DISABLED_BY_DEFAULT);

extern const base::FeatureParam<double> kTimeoutTcpConnectAttemptRTTMultiplier(
    &kTimeoutTcpConnectAttempt,
    "TimeoutTcpConnectAttemptRTTMultiplier",
    5.0);

extern const base::FeatureParam<base::TimeDelta> kTimeoutTcpConnectAttemptMin(
    &kTimeoutTcpConnectAttempt,
    "TimeoutTcpConnectAttemptMin",
    base::Seconds(8));

extern const base::FeatureParam<base::TimeDelta> kTimeoutTcpConnectAttemptMax(
    &kTimeoutTcpConnectAttempt,
    "TimeoutTcpConnectAttemptMax",
    base::Seconds(30));

BASE_FEATURE(kCookieSameSiteConsidersRedirectChain,
             "CookieSameSiteConsidersRedirectChain",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowSameSiteNoneCookiesInSandbox,
             "AllowSameSiteNoneCookiesInSandbox",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWaitForFirstPartySetsInit,
             "WaitForFirstPartySetsInit",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the maximum time duration an outermost frame navigation should be
// deferred by RWS initialization.
extern const base::FeatureParam<base::TimeDelta>
    kWaitForFirstPartySetsInitNavigationThrottleTimeout{
        &kWaitForFirstPartySetsInit,
        "kWaitForFirstPartySetsInitNavigationThrottleTimeout",
        base::Seconds(0)};

BASE_FEATURE(kRequestStorageAccessNoCorsRequired,
             "RequestStorageAccessNoCorsRequired",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStorageAccessApiFollowsSameOriginPolicy,
             "StorageAccessApiFollowsSameOriginPolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStaticKeyPinningEnforcement,
             "StaticKeyPinningEnforcement",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCookieDomainRejectNonASCII,
             "CookieDomainRejectNonASCII",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables partitioning of third party storage (IndexedDB, CacheStorage, etc.)
// by the top level site to reduce fingerprinting.
BASE_FEATURE(kThirdPartyStoragePartitioning,
             "ThirdPartyStoragePartitioning",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTopLevelTpcdOriginTrial,
             "TopLevelTpcdOriginTrial",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTpcdTrialSettings,
             "TpcdSupportSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTopLevelTpcdTrialSettings,
             "TopLevelTpcdSupportSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTpcdMetadataGrants,
             "TpcdMetadataGrants",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTpcdMetadataStageControl,
             "TpcdMetadataStageControl",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAlpsParsing, "AlpsParsing", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAlpsClientHintParsing,
             "AlpsClientHintParsing",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShouldKillSessionOnAcceptChMalformed,
             "ShouldKillSessionOnAcceptChMalformed",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableWebsocketsOverHttp3,
             "EnableWebsocketsOverHttp3",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Disabled because of https://crbug.com/1489696.
BASE_FEATURE(kEnableGetNetworkConnectivityHintAPI,
             "EnableGetNetworkConnectivityHintAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTcpPortRandomizationWin,
             "TcpPortRandomizationWin",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kTcpPortRandomizationWinVersionMinimum,
                   &kTcpPortRandomizationWin,
                   "TcpPortRandomizationWinVersionMinimum",
                   static_cast<int>(base::win::Version::WIN10_20H1));

BASE_FEATURE(kTcpSocketIoCompletionPortWin,
             "TcpSocketIoCompletionPortWin",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kAvoidEntryCreationForNoStore,
             "AvoidEntryCreationForNoStore",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kAvoidEntryCreationForNoStoreCacheSize{
    &kAvoidEntryCreationForNoStore, "AvoidEntryCreationForNoStoreCacheSize",
    1000};

// A flag for new Kerberos feature, that suggests new UI
// when Kerberos authentication in browser fails on ChromeOS.
// b/260522530
#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kKerberosInBrowserRedirect,
             "KerberosInBrowserRedirect",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// A flag to use asynchronous session creation for new QUIC sessions.
BASE_FEATURE(kAsyncQuicSession,
             "AsyncQuicSession",
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// A flag to make multiport context creation asynchronous.
BASE_FEATURE(kAsyncMultiPortPath,
             "AsyncMultiPortPath",
#if !BUILDFLAG(CRONET_BUILD) && (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID))
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Probabilistic reveal tokens configuration settings
BASE_FEATURE(kEnableProbabilisticRevealTokens,
             "EnableProbabilisticRevealTokens",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kProbabilisticRevealTokenServer{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"ProbabilisticRevealTokenServer",
    /*default_value=*/"https://aaftokenissuer.pa.googleapis.com"};

const base::FeatureParam<std::string> kProbabilisticRevealTokenServerPath{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"ProbabilisticRevealTokenServerPath",
    /*default_value=*/"/v1/issueprts"};

const base::FeatureParam<bool> kBypassProbabilisticRevealTokenRegistry{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"BypassProbabilisticRevealTokenRegistry",
    /*default_value=*/false};

const base::FeatureParam<bool> kUseCustomProbabilisticRevealTokenRegistry{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"UseCustomProbabilisticRevealTokenRegistry",
    /*default_value=*/false};

const base::FeatureParam<std::string> kCustomProbabilisticRevealTokenRegistry{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"CustomProbabilisticRevealTokenRegistry",
    /*default_value=*/""};

const base::FeatureParam<bool> kProbabilisticRevealTokensOnlyInIncognito{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"ProbabilisticRevealTokensOnlyInIncognito",
    /*default_value=*/false};

const base::FeatureParam<bool> kProbabilisticRevealTokenFetchOnly{
    &kEnableProbabilisticRevealTokens,
    /*name=*/"ProbabilisticRevealTokenFetchOnly",
    /*default_value=*/false};

const base::FeatureParam<bool>
    kEnableProbabilisticRevealTokensForNonProxiedRequests{
        &kEnableProbabilisticRevealTokens,
        /*name=*/"EnableProbabilisticRevealTokensForNonProxiedRequests",
        /*default_value=*/false};

const base::FeatureParam<bool>
    kProbabilisticRevealTokensAddHeaderToProxiedRequests{
        &kEnableProbabilisticRevealTokens,
        /*name=*/"ProbabilisticRevealTokensAddHeaderToProxiedRequests",
        /*default_value=*/false};

// IP protection experiment configuration settings
BASE_FEATURE(kEnableIpProtectionProxy,
             "EnableIpPrivacyProxy",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kIpPrivacyTokenServer{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyTokenServer",
    /*default_value=*/"https://prod.ipprotectionauth.goog"};

const base::FeatureParam<std::string> kIpPrivacyTokenServerGetInitialDataPath{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyTokenServerGetInitialDataPath",
    /*default_value=*/"/v1/ipblinding/getInitialData"};

const base::FeatureParam<std::string> kIpPrivacyTokenServerGetTokensPath{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyTokenServerGetTokensPath",
    /*default_value=*/"/v1/ipblinding/auth"};

const base::FeatureParam<std::string> kIpPrivacyTokenServerGetProxyConfigPath{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyTokenServerGetProxyConfigPath",
    /*default_value=*/"/v1/ipblinding/getProxyConfig"};

const base::FeatureParam<int> kIpPrivacyAuthTokenCacheBatchSize{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyAuthTokenCacheBatchSize",
    /*default_value=*/64};

const base::FeatureParam<int> kIpPrivacyAuthTokenCacheLowWaterMark{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyAuthTokenCacheLowWaterMark",
    /*default_value=*/16};

const base::FeatureParam<base::TimeDelta> kIpPrivacyProxyListFetchInterval{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyProxyListFetchInterval",
    /*default_value=*/base::Hours(1)};

const base::FeatureParam<base::TimeDelta> kIpPrivacyProxyListMinFetchInterval{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyProxyMinListFetchInterval",
    /*default_value=*/base::Minutes(1)};

const base::FeatureParam<base::TimeDelta> kIpPrivacyProxyListFetchIntervalFuzz{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyProxyListFetchIntervalFuzz",
    /*default_value=*/base::Minutes(30)};

const base::FeatureParam<bool> kIpPrivacyDirectOnly{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyDirectOnly",
    /*default_value=*/false};

const base::FeatureParam<bool> kIpPrivacyIncludeOAuthTokenInGetProxyConfig{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyIncludeOAuthTokenInGetProxyConfig",
    /*default_value=*/false};

const base::FeatureParam<std::string> kIpPrivacyProxyAHostnameOverride{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyProxyAHostnameOverride",
    /*default_value=*/""};

const base::FeatureParam<std::string> kIpPrivacyProxyBHostnameOverride{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyProxyBHostnameOverride",
    /*default_value=*/""};

const base::FeatureParam<bool> kIpPrivacyAddHeaderToProxiedRequests{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyAddHeaderToProxiedRequests",
    /*default_value=*/false};

const base::FeatureParam<base::TimeDelta> kIpPrivacyExpirationFuzz{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyExpirationFuzz",
    /*default_value=*/base::Minutes(15)};

const base::FeatureParam<base::TimeDelta>
    kIpPrivacyTryGetAuthTokensNotEligibleBackoff{
        &kEnableIpProtectionProxy,
        /*name=*/"IpPrivacyTryGetAuthTokensNotEligibleBackoff",
        /*default_value=*/base::Hours(1)};

const base::FeatureParam<base::TimeDelta>
    kIpPrivacyTryGetAuthTokensTransientBackoff{
        &kEnableIpProtectionProxy,
        /*name=*/"IpPrivacyTryGetAuthTokensTransientBackoff",
        /*default_value=*/base::Seconds(5)};

const base::FeatureParam<base::TimeDelta> kIpPrivacyTryGetAuthTokensBugBackoff{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyTryGetAuthTokensBugBackoff",
    /*default_value=*/base::Minutes(10)};

const base::FeatureParam<bool> kIpPrivacyRestrictTopLevelSiteSchemes{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyRestrictTopLevelSiteSchemes",
    /*default_value=*/true};

const base::FeatureParam<bool> kIpPrivacyUseQuicProxies{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyUseQuicProxies",
    /*default_value=*/false};

const base::FeatureParam<bool> kIpPrivacyUseQuicProxiesOnly{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyUseQuicProxiesOnly",
    /*default_value=*/false};

const base::FeatureParam<bool> kIpPrivacyFallbackToDirect{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyFallbackToDirect",
    /*default_value=*/true};

const base::FeatureParam<int> kIpPrivacyDebugExperimentArm{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyDebugExperimentArm",
    /*default_value=*/0};

const base::FeatureParam<bool> kIpPrivacyAlwaysCreateCore{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyAlwaysCreateCore",
    /*default_value=*/false};

const base::FeatureParam<bool> kIpPrivacyOnlyInIncognito{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyOnlyInIncognito",
    /*default_value=*/false};

const base::FeatureParam<bool> kIpPrivacyEnableUserBypass{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyEnableUserBypass",
    /*default_value=*/false};

const base::FeatureParam<bool> kIpPrivacyDisableForEnterpriseByDefault{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyDisableForEnterpriseByDefault",
    /*default_value=*/false};

BASE_FEATURE(kExcludeLargeBodyReports,
             "ExcludeLargeReportBodies",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kMaxReportBodySizeKB,
                   &kExcludeLargeBodyReports,
                   "max_report_body_size_kb",
                   1024);

BASE_FEATURE(kRelatedWebsitePartitionAPI,
             "RelatedWebsitePartitionAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Network-change migration requires NetworkHandle support, which are currently
// only supported on Android (see
// NetworkChangeNotifier::AreNetworkHandlesSupported).
#if BUILDFLAG(IS_ANDROID)
inline constexpr auto kMigrateSessionsOnNetworkChangeV2Default =
    base::FEATURE_ENABLED_BY_DEFAULT;
#else   // !BUILDFLAG(IS_ANDROID)
inline constexpr auto kMigrateSessionsOnNetworkChangeV2Default =
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif  // BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMigrateSessionsOnNetworkChangeV2,
             "MigrateSessionsOnNetworkChangeV2",
             kMigrateSessionsOnNetworkChangeV2Default);

BASE_FEATURE(kDisableBlackholeOnNoNewNetwork,
             "DisableBlackHoleOnNoNewNetwork",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX)
BASE_FEATURE(kAddressTrackerLinuxIsProxied,
             "AddressTrackerLinuxIsProxied",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX)

// Enables binding of cookies to the port that originally set them by default.
BASE_FEATURE(kEnablePortBoundCookies,
             "EnablePortBoundCookies",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables binding of cookies to the scheme that originally set them.
NET_EXPORT BASE_DECLARE_FEATURE(kEnableSchemeBoundCookies);
BASE_FEATURE(kEnableSchemeBoundCookies,
             "EnableSchemeBoundCookies",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disallows cookies to have non ascii values in their name or value.
NET_EXPORT BASE_DECLARE_FEATURE(kDisallowNonAsciiCookies);
BASE_FEATURE(kDisallowNonAsciiCookies,
             "DisallowNonAsciiCookies",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTimeLimitedInsecureCookies,
             "TimeLimitedInsecureCookies",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable third-party cookie blocking from the command line.
BASE_FEATURE(kForceThirdPartyCookieBlocking,
             "ForceThirdPartyCookieBlockingEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableEarlyHintsOnHttp11,
             "EnableEarlyHintsOnHttp11",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableWebTransportDraft07,
             "EnableWebTransportDraft07",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, partitioned storage will be allowed even if third-party cookies
// are disabled by default. Partitioned storage will not be allowed if
// third-party cookies are disabled due to a specific rule.
BASE_FEATURE(kThirdPartyPartitionedStorageAllowedByDefault,
             "ThirdPartyPartitionedStorageAllowedByDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSpdyHeadersToHttpResponseUseBuilder,
             "SpdyHeadersToHttpResponseUseBuilder",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseNewAlpsCodepointHttp2,
             "UseNewAlpsCodepointHttp2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseNewAlpsCodepointQUIC,
             "UseNewAlpsCodepointQUIC",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTruncateBodyToContentLength,
             "TruncateBodyToContentLength",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kReduceIPAddressChangeNotification,
             "ReduceIPAddressChangeNotification",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

BASE_FEATURE(kDeviceBoundSessions,
             "DeviceBoundSessions",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPersistDeviceBoundSessions,
             "PersistDeviceBoundSessions",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kDeviceBoundSessionsForceEnableForTesting,
                   &kDeviceBoundSessions,
                   "ForceEnableForTesting",
                   false);
BASE_FEATURE(kDeviceBoundSessionsRefreshQuota,
             "DeviceBoundSessionsRefreshQuota",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPartitionProxyChains,
             "PartitionProxyChains",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSpdySessionForProxyAdditionalChecks,
             "SpdySessionForProxyAdditionalChecks",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCompressionDictionaryTransportOverHttp1,
             "CompressionDictionaryTransportOverHttp1",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCompressionDictionaryTransportOverHttp2,
             "CompressionDictionaryTransportOverHttp2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCompressionDictionaryTransportRequireKnownRootCert,
             "CompressionDictionaryTransportRequireKnownRootCert",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReportingApiEnableEnterpriseCookieIssues,
             "ReportingApiEnableEnterpriseCookieIssues",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSimdutfBase64Support,
             "SimdutfBase64Support",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFurtherOptimizeParsingDataUrls,
             "FurtherOptimizeParsingDataUrls",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/347047841): Remove this flag when we branch for M131 or later,
// if we haven't had to turn this off.
BASE_FEATURE(kLegacyPKCS1ForTLS13,
             "LegacyPKCS1ForTLS13",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kKeepWhitespaceForDataUrls,
             "KeepWhitespaceForDataUrls",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNoVarySearchIgnoreUnrecognizedKeys,
             "NoVarySearchIgnoreUnrecognizedKeys",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEncryptedAndPlaintextValuesAreInvalid,
             "EncryptedAndPlaintextValuesAreInvalid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableStaticCTAPIEnforcement,
             "EnableStaticCTAPIEnforcement",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDiskCacheBackendExperiment,
             "DiskCacheBackendExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<DiskCacheBackend>::Option
    kDiskCacheBackendOptions[] = {
        {DiskCacheBackend::kSimple, "simple"},
        {DiskCacheBackend::kBlockfile, "blockfile"},
};
const base::FeatureParam<DiskCacheBackend> kDiskCacheBackendParam{
    &kDiskCacheBackendExperiment, "backend", DiskCacheBackend::kBlockfile,
    &kDiskCacheBackendOptions};

BASE_FEATURE(kIgnoreHSTSForLocalhost,
             "IgnoreHSTSForLocalhost",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSimpleCachePrioritizedCaching,
             "SimpleCachePrioritizedCaching",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int>
    kSimpleCachePrioritizedCachingPrioritizationFactor{
        &kSimpleCachePrioritizedCaching,
        /*name=*/"SimpleCachePrioritizedCachingPrioritizationFactor",
        /*default_value=*/10};

const base::FeatureParam<base::TimeDelta>
    kSimpleCachePrioritizedCachingPrioritizationPeriod{
        &kSimpleCachePrioritizedCaching,
        /*name=*/"SimpleCachePrioritizedCachingPrioritizationPeriod",
        /*default_value=*/base::Days(1)};

#if BUILDFLAG(USE_NSS_CERTS)
// TODO(crbug.com/390333881): Remove this flag after a few milestones.
BASE_FEATURE(kNewClientCertPathBuilding,
             "NewClientCertPathBuilding",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(USE_NSS_CERTS)

BASE_FEATURE(kHstsTopLevelNavigationsOnly,
             "HstsTopLevelNavigationsOnly",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHttpCacheNoVarySearch,
             "HttpCacheNoVarySearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kHttpCacheNoVarySearchCacheMaxEntries,
                   &kHttpCacheNoVarySearch,
                   "max_entries",
                   1000);

BASE_FEATURE(kReportingApiCorsOriginHeader,
             "ReportingApiCorsOriginHeader",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kUseCertTransparencyAwareApiForOsCertVerify,
             "UseCertTransparencyAwareApiForOsCertVerify",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSelfSignedLocalNetworkInterstitial,
             "SelfSignedLocalNetworkInterstitial",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
BASE_FEATURE(kVerifyQWACs, "VerifyQWACs", base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kIncludeDeprecatedClientCertLookup,
             "IncludeDeprecatedClientCertLookup",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kRestrictAbusePorts,
             "RestrictAbusePorts",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRestrictAbusePortsOnLocalhost,
             "RestrictAbusePortsOnLocalhost",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace net::features
