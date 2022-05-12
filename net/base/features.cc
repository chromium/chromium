// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/features.h"

#include <vector>

#include "base/feature_list.h"
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

const base::Feature kCookieDomainAttributeEmptyString{
    "CookieDomainAttributeEmptyString", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDnsTransactionDynamicTimeouts{
    "DnsTransactionDynamicTimeouts", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<double> kDnsTransactionTimeoutMultiplier{
    &kDnsTransactionDynamicTimeouts, "DnsTransactionTimeoutMultiplier", 7.5};

const base::FeatureParam<base::TimeDelta> kDnsMinTransactionTimeout{
    &kDnsTransactionDynamicTimeouts, "DnsMinTransactionTimeout",
    base::Seconds(12)};

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
  return base::Milliseconds(kDnsHttpssvcExtraTimeMs.Get());
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

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbInsecureExtraTimeMax{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbInsecureExtraTimeMax",
    base::TimeDelta()};

const base::FeatureParam<int> kUseDnsHttpsSvcbInsecureExtraTimePercent{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbInsecureExtraTimePercent", 0};

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbInsecureExtraTimeMin{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbInsecureExtraTimeMin",
    base::TimeDelta()};

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbSecureExtraTimeMax{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbSecureExtraTimeMax", base::TimeDelta()};

const base::FeatureParam<int> kUseDnsHttpsSvcbSecureExtraTimePercent{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbSecureExtraTimePercent", 0};

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbSecureExtraTimeMin{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbSecureExtraTimeMin", base::TimeDelta()};

const base::FeatureParam<base::TimeDelta> kUseDnsHttpsSvcbExtraTimeAbsolute{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbExtraTimeAbsolute", base::TimeDelta()};

const base::FeatureParam<int> kUseDnsHttpsSvcbExtraTimePercent{
    &kUseDnsHttpsSvcb, "UseDnsHttpsSvcbExtraTimePercent", 0};

const base::Feature kEnableTLS13EarlyData{"EnableTLS13EarlyData",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEncryptedClientHello{"EncryptedClientHello",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNetworkQualityEstimator{"NetworkQualityEstimator",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSplitCacheByIncludeCredentials{
    "SplitCacheByIncludeCredentials", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSplitCacheByNetworkIsolationKey{
    "SplitCacheByNetworkIsolationKey", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSplitHostCacheByNetworkIsolationKey{
    "SplitHostCacheByNetworkIsolationKey", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPartitionConnectionsByNetworkIsolationKey{
    "PartitionConnectionsByNetworkIsolationKey",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kForceIsolationInfoFrameOriginToTopLevelFrame{
    "ForceIsolationInfoFrameOriginToTopLevelFrame",
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

const base::Feature kPermuteTLSExtensions{"PermuteTLSExtensions",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPostQuantumCECPQ2{"PostQuantumCECPQ2",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kPostQuantumCECPQ2SomeDomains{
    "PostQuantumCECPQ2SomeDomains", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<std::string>
    kPostQuantumCECPQ2Prefix(&kPostQuantumCECPQ2SomeDomains, "prefix", "a");

const base::Feature kNetUnusedIdleSocketTimeout{
    "NetUnusedIdleSocketTimeout", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kShortLaxAllowUnsafeThreshold{
    "ShortLaxAllowUnsafeThreshold", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSameSiteDefaultChecksMethodRigorously{
    "SameSiteDefaultChecksMethodRigorously", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
const base::Feature kCertVerifierBuiltinFeature{
    "CertVerifierBuiltin", base::FEATURE_DISABLED_BY_DEFAULT};
#if BUILDFLAG(IS_MAC)
const base::FeatureParam<int> kCertVerifierBuiltinImpl{
    &kCertVerifierBuiltinFeature, "impl", 0};
const base::FeatureParam<int> kCertVerifierBuiltinCacheSize{
    &kCertVerifierBuiltinFeature, "cachesize", 0};
#endif /* BUILDFLAG(IS_MAC) */
#endif

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
// Enables the dual certificate verification trial feature.
// https://crbug.com/649026
const base::Feature kCertDualVerificationTrialFeature{
    "CertDualVerificationTrial", base::FEATURE_DISABLED_BY_DEFAULT};
#if BUILDFLAG(IS_MAC)
const base::FeatureParam<int> kCertDualVerificationTrialImpl{
    &kCertDualVerificationTrialFeature, "impl", 0};
const base::FeatureParam<int> kCertDualVerificationTrialCacheSize{
    &kCertDualVerificationTrialFeature, "cachesize", 0};
#endif /* BUILDFLAG(IS_MAC) */
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
const base::Feature kChromeRootStoreUsed{"ChromeRootStoreUsed",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
#endif /* BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED) */

const base::Feature kTurnOffStreamingMediaCachingOnBattery{
    "TurnOffStreamingMediaCachingOnBattery", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTurnOffStreamingMediaCachingAlways{
    "TurnOffStreamingMediaCachingAlways", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSchemefulSameSite{"SchemefulSameSite",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

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
    base::Seconds(8));

extern const base::FeatureParam<base::TimeDelta> kTimeoutTcpConnectAttemptMax(
    &kTimeoutTcpConnectAttempt,
    "TimeoutTcpConnectAttemptMax",
    base::Seconds(30));

#if BUILDFLAG(ENABLE_REPORTING)
const base::Feature kDocumentReporting{"DocumentReporting",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
const base::Feature kUdpSocketPosixAlwaysUpdateBytesReceived{
    "UdpSocketPosixAlwaysUpdateBytesReceived",
    base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

const base::Feature kCookieSameSiteConsidersRedirectChain{
    "CookieSameSiteConsidersRedirectChain", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSamePartyCookiesConsideredFirstParty{
    "SamePartyCookiesConsideredFirstParty", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPartitionedCookies{"PartitionedCookies",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kPartitionedCookiesBypassOriginTrial{
    "PartitionedCookiesBypassOriginTrial", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNoncedPartitionedCookies{
    "NoncedPartitionedCookies", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kExtraCookieValidityChecks{
    "ExtraCookieValidityChecks", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kRecordRadioWakeupTrigger{
    "RecordRadioWakeupTrigger", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSandboxHttpCache("SandboxHttpCache",
                                      base::FEATURE_DISABLED_BY_DEFAULT);

const base::Feature kClampCookieExpiryTo400Days(
    "ClampCookieExpiryTo400Days",
    base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
const base::Feature kStaticKeyPinningEnforcement(
    "StaticKeyPinningEnforcement",
    base::FEATURE_DISABLED_BY_DEFAULT);
#else
const base::Feature kStaticKeyPinningEnforcement(
    "StaticKeyPinningEnforcement",
    base::FEATURE_ENABLED_BY_DEFAULT);
#endif

}  // namespace features
}  // namespace net
