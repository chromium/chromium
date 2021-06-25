// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/features.h"

#include <vector>

#include "build/build_config.h"

namespace net {
namespace features {

const base::Feature kAcceptLanguageHeader{"AcceptLanguageHeader",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAlpsForHttp2{"AlpsForHttp2",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAvoidH2Reprioritization{"AvoidH2Reprioritization",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCapReferrerToOriginOnCrossOrigin{
    "CapReferrerToOriginOnCrossOrigin", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDnsTransactionDynamicTimeouts{
    "DnsTransactionDynamicTimeouts", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<double> kDnsTransactionTimeoutMultiplier{
    &kDnsTransactionDynamicTimeouts, "DnsTransactionTimeoutMultiplier", 7.5};

const base::FeatureParam<base::TimeDelta> kDnsMinTransactionTimeout{
    &kDnsTransactionDynamicTimeouts, "DnsMinTransactionTimeout",
    base::TimeDelta::FromSeconds(12)};

const base::Feature kDnsHttpssvc{"DnsHttpssvc",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<bool> kDnsHttpssvcUseHttpssvc{
    &kDnsHttpssvc, "DnsHttpssvcUseHttpssvc", false};

const base::FeatureParam<bool> kDnsHttpssvcUseIntegrity{
    &kDnsHttpssvc, "DnsHttpssvcUseIntegrity", false};

const base::FeatureParam<bool> kDnsHttpssvcEnableQueryOverInsecure{
    &kDnsHttpssvc, "DnsHttpssvcEnableQueryOverInsecure", false};

const base::FeatureParam<int> kDnsHttpssvcExtraTimeMs{
    &kDnsHttpssvc, "DnsHttpssvcExtraTimeMs", 10};

const base::FeatureParam<int> kDnsHttpssvcExtraTimePercent{
    &kDnsHttpssvc, "DnsHttpssvcExtraTimePercent", 5};

const base::FeatureParam<std::string> kDnsHttpssvcExperimentDomains{
    &kDnsHttpssvc, "DnsHttpssvcExperimentDomains", ""};

const base::FeatureParam<std::string> kDnsHttpssvcControlDomains{
    &kDnsHttpssvc, "DnsHttpssvcControlDomains", ""};

const base::FeatureParam<bool> kDnsHttpssvcControlDomainWildcard{
    &kDnsHttpssvc, "DnsHttpssvcControlDomainWildcard", false};

namespace dns_httpssvc_experiment {
base::TimeDelta GetExtraTimeAbsolute() {
  DCHECK(base::FeatureList::IsEnabled(features::kDnsHttpssvc));
  return base::TimeDelta::FromMilliseconds(kDnsHttpssvcExtraTimeMs.Get());
}
}  // namespace dns_httpssvc_experiment

const base::Feature kUseDnsHttpsSvcb{"UseDnsHttpsSvcb",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<bool> kUseDnsHttpsSvcbHttpUpgrade{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbHttpUpgrade", false};

const base::FeatureParam<bool> kUseDnsHttpsSvcbEnforceSecureResponse{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbEnforceSecureResponse", false};

const base::FeatureParam<bool> kUseDnsHttpsSvcbEnableInsecure{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbEnableInsecure", false};

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbExtraTimeAbsolute{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbExtraTimeAbsolute", base::TimeDelta()};

const base::FeatureParam<int> kUseDnsHttpsSvcbExtraTimePercent{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbExtraTimePercent", 0};

const base::Feature kEnableTLS13EarlyData{"EnableTLS13EarlyData",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNetworkQualityEstimator{"NetworkQualityEstimator",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSplitCacheByNetworkIsolationKey{
    "SplitCacheByNetworkIsolationKey", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSplitHostCacheByNetworkIsolationKey{
    "SplitHostCacheByNetworkIsolationKey", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPartitionConnectionsByNetworkIsolationKey{
    "PartitionConnectionsByNetworkIsolationKey",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPartitionHttpServerPropertiesByNetworkIsolationKey{
    "PartitionHttpServerPropertiesByNetworkIsolationKey",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPartitionSSLSessionsByNetworkIsolationKey{
    "PartitionSSLSessionsByNetworkIsolationKey",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPartitionExpectCTStateByNetworkIsolationKey{
    "PartitionExpectCTStateByNetworkIsolationKey",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPartitionNelAndReportingByNetworkIsolationKey{
    "PartitionNelAndReportingByNetworkIsolationKey",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kExpectCTPruning{"ExpectCTPruning",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

NET_EXPORT extern const base::FeatureParam<int>
    kExpectCTPruneMax(&kExpectCTPruning, "ExpectCTPruneMax", 2000);
NET_EXPORT extern const base::FeatureParam<int>
    kExpectCTPruneMin(&kExpectCTPruning, "ExpectCTPruneMin", 1800);
NET_EXPORT extern const base::FeatureParam<int> kExpectCTSafeFromPruneDays(
    &kExpectCTPruning,
    "ExpectCTSafeFromPruneDays",
    40);
NET_EXPORT extern const base::FeatureParam<int> kExpectCTMaxEntriesPerNik(
    &kExpectCTPruning,
    "ExpectCTMaxEntriesPerNik",
    20);
NET_EXPORT extern const base::FeatureParam<int>
    kExpectCTPruneDelaySecs(&kExpectCTPruning, "ExpectCTPruneDelaySecs", 60);

const base::Feature kTLS13KeyUpdate{"TLS13KeyUpdate",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPostQuantumCECPQ2{"PostQuantumCECPQ2",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kPostQuantumCECPQ2SomeDomains{
    "PostQuantumCECPQ2SomeDomains", base::FEATURE_ENABLED_BY_DEFAULT};
const base::FeatureParam<std::string> kPostQuantumCECPQ2InitialLetters(
    &kPostQuantumCECPQ2SomeDomains,
    "InitialLetters",
    "ag");

const base::Feature kNetUnusedIdleSocketTimeout{
    "NetUnusedIdleSocketTimeout", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSameSiteByDefaultCookies{"SameSiteByDefaultCookies",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCookiesWithoutSameSiteMustBeSecure{
    "CookiesWithoutSameSiteMustBeSecure", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kShortLaxAllowUnsafeThreshold{
    "ShortLaxAllowUnsafeThreshold", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSameSiteDefaultChecksMethodRigorously{
    "SameSiteDefaultChecksMethodRigorously", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
const base::Feature kCertVerifierBuiltinFeature{
    "CertVerifierBuiltin", base::FEATURE_DISABLED_BY_DEFAULT};
#if defined(OS_MAC)
const base::FeatureParam<int> kCertVerifierBuiltinImpl{
    &kCertVerifierBuiltinFeature, "impl", 0};
const base::FeatureParam<int> kCertVerifierBuiltinCacheSize{
    &kCertVerifierBuiltinFeature, "cachesize", 0};
#endif /* defined(OS_MAC) */
#endif

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
// Enables the dual certificate verification trial feature.
// https://crbug.com/649026
const base::Feature kCertDualVerificationTrialFeature{
    "CertDualVerificationTrial", base::FEATURE_DISABLED_BY_DEFAULT};
#if defined(OS_MAC)
const base::FeatureParam<int> kCertDualVerificationTrialImpl{
    &kCertDualVerificationTrialFeature, "impl", 0};
const base::FeatureParam<int> kCertDualVerificationTrialCacheSize{
    &kCertDualVerificationTrialFeature, "cachesize", 0};
#endif /* defined(OS_MAC) */
#endif

const base::Feature kTurnOffStreamingMediaCachingOnBattery{
    "TurnOffStreamingMediaCachingOnBattery", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTurnOffStreamingMediaCachingAlways{
    "TurnOffStreamingMediaCachingAlways", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLegacyTLSEnforced{"LegacyTLSEnforced",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSchemefulSameSite{"SchemefulSameSite",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTLSLegacyCryptoFallbackForMetrics{
    "TLSLegacyCryptoFallbackForMetrics", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUseLookalikesForNavigationSuggestions{
    "UseLookalikesForNavigationSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kReportPoorConnectivity{"ReportPoorConnectivity",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPreemptiveMobileNetworkActivation{
    "PreemptiveMobileNetworkActivation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLimitOpenUDPSockets{"LimitOpenUDPSockets",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

extern const base::FeatureParam<int> kLimitOpenUDPSocketsMax(
    &kLimitOpenUDPSockets,
    "LimitOpenUDPSocketsMax",
    6000);

const base::Feature kTimeoutTcpConnectAttempt{
    "TimeoutTcpConnectAttempt", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::FeatureParam<double> kTimeoutTcpConnectAttemptRTTMultiplier(
    &kTimeoutTcpConnectAttempt,
    "TimeoutTcpConnectAttemptRTTMultiplier",
    5.0);

extern const base::FeatureParam<base::TimeDelta> kTimeoutTcpConnectAttemptMin(
    &kTimeoutTcpConnectAttempt,
    "TimeoutTcpConnectAttemptMin",
    base::TimeDelta::FromSeconds(8));

extern const base::FeatureParam<base::TimeDelta> kTimeoutTcpConnectAttemptMax(
    &kTimeoutTcpConnectAttempt,
    "TimeoutTcpConnectAttemptMax",
    base::TimeDelta::FromSeconds(30));

constexpr base::Feature kFirstPartySets{"FirstPartySets",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<bool> kFirstPartySetsIsDogfooder{
    &kFirstPartySets, "FirstPartySetsIsDogfooder", false};

const base::Feature kSameSiteCookiesBugfix1166211{
    "SameSiteCookiesBugfix1166211", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNoCookieChangeNotificationOnLoad{
    "NoCookieChangeNotificationOnLoad", base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(ENABLE_REPORTING)
const base::Feature kDocumentReporting{"DocumentReporting",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
const base::Feature kUdpSocketPosixAlwaysUpdateBytesReceived{
    "UdpSocketPosixAlwaysUpdateBytesReceived",
    base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_POSIX) || defined(OS_FUCHSIA)

const base::Feature kCookieSameSiteConsidersRedirectChain{
    "CookieSameSiteConsidersRedirectChain", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSamePartyCookiesConsideredFirstParty{
    "SamePartyCookiesConsideredFirstParty", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace net
