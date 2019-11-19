// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/features.h"

namespace net {
namespace features {

const base::Feature kAcceptLanguageHeader{"AcceptLanguageHeader",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCapRefererHeaderLength = {
    "CapRefererHeaderLength", base::FEATURE_ENABLED_BY_DEFAULT};
const base::FeatureParam<int> kMaxRefererHeaderLength = {
    &kCapRefererHeaderLength, "MaxRefererHeaderLength", 4096};

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

const base::Feature kTLS13KeyUpdate{"TLS13KeyUpdate",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPostQuantumCECPQ2{"PostQuantumCECPQ2",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNetUnusedIdleSocketTimeout{
    "NetUnusedIdleSocketTimeout", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRequestEsniDnsRecords{"RequestEsniDnsRecords",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
base::TimeDelta EsniDnsMaxAbsoluteAdditionalWait() {
  DCHECK(base::FeatureList::IsEnabled(kRequestEsniDnsRecords));
  return base::TimeDelta::FromMilliseconds(
      kEsniDnsMaxAbsoluteAdditionalWaitMilliseconds.Get());
}
const base::FeatureParam<int> kEsniDnsMaxAbsoluteAdditionalWaitMilliseconds{
    &kRequestEsniDnsRecords, "EsniDnsMaxAbsoluteAdditionalWaitMilliseconds",
    10};
const base::FeatureParam<int> kEsniDnsMaxRelativeAdditionalWaitPercent{
    &kRequestEsniDnsRecords, "EsniDnsMaxRelativeAdditionalWaitPercent", 5};

const base::Feature kSameSiteByDefaultCookies{
    "SameSiteByDefaultCookies", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCookiesWithoutSameSiteMustBeSecure{
    "CookiesWithoutSameSiteMustBeSecure", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kShortLaxAllowUnsafeThreshold{
    "ShortLaxAllowUnsafeThreshold", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
const base::Feature kCertVerifierBuiltinFeature{
    "CertVerifierBuiltin", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kAppendFrameOriginToNetworkIsolationKey{
    "AppendFrameOriginToNetworkIsolationKey",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseRegistrableDomainInNetworkIsolationKey{
    "UseRegistrableDomainInNetworkIsolationKey",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTurnOffStreamingMediaCaching{
    "TurnOffStreamingMediaCaching", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace net
