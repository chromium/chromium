// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/features.h"

#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "net/base/cronet_buildflags.h"
#include "net/net_buildflags.h"

namespace net::features {

BASE_FEATURE(kAlpsForHttp2, "AlpsForHttp2", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAvoidH2Reprioritization,
             "AvoidH2Reprioritization",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCapReferrerToOriginOnCrossOrigin,
             "CapReferrerToOriginOnCrossOrigin",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kUseDnsHttpsSvcbAlpn,
             "UseDnsHttpsSvcbAlpn",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kAlternativePortForGloballyReachableCheck{
    &kUseAlternativePortForGloballyReachableCheck,
    "AlternativePortForGloballyReachableCheck", 443};

BASE_FEATURE(kUseAlternativePortForGloballyReachableCheck,
             "UseAlternativePortForGloballyReachableCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSHA1ServerSignature,
             "SHA1ServerSignature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableTLS13EarlyData,
             "EnableTLS13EarlyData",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEncryptedClientHello,
             "EncryptedClientHello",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEncryptedClientHelloQuic,
             "EncryptedClientHelloQuic",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/795089): Enable this feature.
BASE_FEATURE(kRSAKeyUsageForLocalAnchors,
             "RSAKeyUsageForLocalAnchors",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNetworkQualityEstimator,
             "NetworkQualityEstimator",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSplitCacheByIncludeCredentials,
             "SplitCacheByIncludeCredentials",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSplitCacheByNetworkIsolationKey,
             "SplitCacheByNetworkIsolationKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSplitCodeCacheByNetworkIsolationKey,
             "SplitCodeCacheByNetworkIsolationKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSplitHostCacheByNetworkIsolationKey,
             "SplitHostCacheByNetworkIsolationKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPartitionConnectionsByNetworkIsolationKey,
             "PartitionConnectionsByNetworkIsolationKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPartitionHttpServerPropertiesByNetworkIsolationKey,
             "PartitionHttpServerPropertiesByNetworkIsolationKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPartitionSSLSessionsByNetworkIsolationKey,
             "PartitionSSLSessionsByNetworkIsolationKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPartitionNelAndReportingByNetworkIsolationKey,
             "PartitionNelAndReportingByNetworkIsolationKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableCrossSiteFlagNetworkIsolationKey,
             "EnableCrossSiteFlagNetworkIsolationKey",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableFrameSiteSharedOpaqueNetworkIsolationKey,
             "EnableFrameSiteSharedOpaqueNetworkIsolationKey",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kHttpCacheKeyingExperimentControlGroup,
             "HttpCacheKeyingExperimentControlGroup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTLS13KeyUpdate,
             "TLS13KeyUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPermuteTLSExtensions,
             "PermuteTLSExtensions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPostQuantumKyber,
             "PostQuantumKyber",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNetUnusedIdleSocketTimeout,
             "NetUnusedIdleSocketTimeout",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShortLaxAllowUnsafeThreshold,
             "ShortLaxAllowUnsafeThreshold",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSameSiteDefaultChecksMethodRigorously,
             "SameSiteDefaultChecksMethodRigorously",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
// Enables the dual certificate verification trial feature.
// https://crbug.com/649026
BASE_FEATURE(kCertDualVerificationTrialFeature,
             "CertDualVerificationTrial",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
BASE_FEATURE(kChromeRootStoreUsed,
             "ChromeRootStoreUsed",
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(USE_NSS_CERTS) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kTrustStoreTrustedLeafSupport,
             "TrustStoreTrustedLeafSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kTurnOffStreamingMediaCachingOnBattery,
             "TurnOffStreamingMediaCachingOnBattery",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTurnOffStreamingMediaCachingAlways,
             "TurnOffStreamingMediaCachingAlways",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSchemefulSameSite,
             "SchemefulSameSite",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

#if BUILDFLAG(ENABLE_REPORTING)
BASE_FEATURE(kDocumentReporting,
             "DocumentReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
BASE_FEATURE(kUdpSocketPosixAlwaysUpdateBytesReceived,
             "UdpSocketPosixAlwaysUpdateBytesReceived",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

BASE_FEATURE(kCookieSameSiteConsidersRedirectChain,
             "CookieSameSiteConsidersRedirectChain",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWaitForFirstPartySetsInit,
             "WaitForFirstPartySetsInit",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPartitionedCookies,
             "PartitionedCookies",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNoncedPartitionedCookies,
             "NoncedPartitionedCookies",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBlockTruncatedCookies,
             "BlockTruncatedCookies",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
             base::FEATURE_DISABLED_BY_DEFAULT);
// Whether to use the new code paths needed to support partitioning Blob URLs.
// This exists as a kill-switch in case an issue is identified with the Blob
// URL implementation that causes breakage.
BASE_FEATURE(kSupportPartitionedBlobUrl,
             "SupportPartitionedBlobUrl",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTpcdSupportSettings,
             "TpcdSupportSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTpcdMetadataGrants,
             "TpcdMetadataGrants",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAlpsParsing, "AlpsParsing", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAlpsClientHintParsing,
             "AlpsClientHintParsing",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShouldKillSessionOnAcceptChMalformed,
             "ShouldKillSessionOnAcceptChMalformed",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCaseInsensitiveCookiePrefix,
             "CaseInsensitiveCookiePrefix",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableWebsocketsOverHttp3,
             "EnableWebsocketsOverHttp3",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseNAT64ForIPv4Literal,
             "UseNAT64ForIPv4Literal",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBlockNewForbiddenHeaders,
             "BlockNewForbiddenHeaders",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kPlatformKeyProbeSHA256,
             "PlatformKeyProbeSHA256",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disabled because of https://crbug.com/1489696.
BASE_FEATURE(kEnableGetNetworkConnectivityHintAPI,
             "EnableGetNetworkConnectivityHintAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Prefetch to follow normal semantics instead of 5-minute rule
// https://crbug.com/1345207
BASE_FEATURE(kPrefetchFollowsNormalCacheSemantics,
             "PrefetchFollowsNormalCacheSemantics",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// IP protection experiment configuration settings
BASE_FEATURE(kEnableIpProtectionProxy,
             "EnableIpPrivacyProxy",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kIpPrivacyProxyServer{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyProxyServer",
    /*default_value=*/""};

const base::FeatureParam<std::string> kIpPrivacyTokenServer{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyTokenServer",
    /*default_value=*/"https://autopush-phosphor-pa.sandbox.googleapis.com"};

const base::FeatureParam<std::string> kIpPrivacyTokenServerGetInitialDataPath{
    &kEnableIpProtectionProxy,
    /*name=*/"IpPrivacyTokenServerGetInitialDataPath",
    /*default_value=*/"/v1/getInitialData"};

const base::FeatureParam<std::string> kIpPrivacyTokenServerGetTokensPath{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyTokenServerGetTokensPath",
    /*default_value=*/"/v1/authWithHeaderCreds"};

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

const base::FeatureParam<bool> kIpPrivacyDirectOnly{
    &kEnableIpProtectionProxy, /*name=*/"IpPrivacyDirectOnly",
    /*default_value=*/false};

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

// Enable third-party cookie blocking from the command line.
BASE_FEATURE(kForceThirdPartyCookieBlocking,
             "ForceThirdPartyCookieBlockingEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If the HTTP Cache Transaction write lock should be acquired async with
// sending the HTTP request.
BASE_FEATURE(kAsyncCacheLock,
             "AsyncCacheLock",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableEarlyHintsOnHttp11,
             "EnableEarlyHintsOnHttp11",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableWebTransportDraft07,
             "EnableWebTransportDraft07",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kZstdContentEncoding,
             "ZstdContentEncoding",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDigestAuthEnableSecureAlgorithms,
             "DigestAuthEnableSecureAlgorithms",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, partitioned storage will be allowed even if third-party cookies
// are disabled by default. Partitioned storage will not be allowed if
// third-party cookies are disabled due to a specific rule.
// TODO(crbug.com/1468277): Default enable when UI work is complete.
BASE_FEATURE(kThirdPartyPartitionedStorageAllowedByDefault,
             "ThirdPartyPartitionedStorageAllowedByDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCookieDeprecationFacilitatedTestingLabels,
             "CookieDeprecationFacilitatedTestingLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace net::features
