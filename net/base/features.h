// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FEATURES_H_
#define NET_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "net/base/net_export.h"
#include "net/net_buildflags.h"

namespace net {
namespace features {

// Toggles the `Accept-Language` HTTP request header, which
// https://github.com/WICG/lang-client-hint proposes that we deprecate.
NET_EXPORT extern const base::Feature kAcceptLanguageHeader;

// Caps the length of the `referer` header to 4k, which should be enough for
// anyone.
NET_EXPORT extern const base::Feature kCapRefererHeaderLength;
NET_EXPORT extern const base::FeatureParam<int> kMaxRefererHeaderLength;

// Enables TLS 1.3 early data.
NET_EXPORT extern const base::Feature kEnableTLS13EarlyData;

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

// Enables sending TLS 1.3 Key Update messages on TLS 1.3 connections in order
// to ensure that this corner of the spec is exercised. This is currently
// disabled by default because we discovered incompatibilities with some
// servers.
NET_EXPORT extern const base::Feature kTLS13KeyUpdate;

// Enables CECPQ2, a post-quantum key-agreement, in TLS 1.3 connections.
NET_EXPORT extern const base::Feature kPostQuantumCECPQ2;

// Changes the timeout after which unused sockets idle sockets are cleaned up.
NET_EXPORT extern const base::Feature kNetUnusedIdleSocketTimeout;

// Enables the built-in resolver requesting ESNI (TLS 1.3 Encrypted
// Server Name Indication) records alongside IPv4 and IPv6 address records
// during DNS over HTTPS (DoH) host resolution.
NET_EXPORT extern const base::Feature kRequestEsniDnsRecords;
// Returns a TimeDelta of value kEsniDnsMaxAbsoluteAdditionalWaitMilliseconds
// milliseconds (see immediately below).
NET_EXPORT base::TimeDelta EsniDnsMaxAbsoluteAdditionalWait();
// The following two parameters specify the amount of extra time to wait for a
// long-running ESNI DNS transaction after the successful conclusion of
// concurrent A and AAAA transactions. This timeout will have value
// min{kEsniDnsMaxAbsoluteAdditionalWaitMilliseconds,
//     (100% + kEsniDnsMaxRelativeAdditionalWaitPercent)
//       * max{time elapsed for the concurrent A query,
//             time elapsed for the concurrent AAAA query}}.
NET_EXPORT extern const base::FeatureParam<int>
    kEsniDnsMaxAbsoluteAdditionalWaitMilliseconds;
NET_EXPORT extern const base::FeatureParam<int>
    kEsniDnsMaxRelativeAdditionalWaitPercent;

// When enabled, makes cookies without a SameSite attribute behave like
// SameSite=Lax cookies by default, and requires SameSite=None to be specified
// in order to make cookies available in a third-party context. When disabled,
// the default behavior for cookies without a SameSite attribute specified is no
// restriction, i.e., available in a third-party context.
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

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
// When enabled, use the builtin cert verifier instead of the platform verifier.
NET_EXPORT extern const base::Feature kCertVerifierBuiltinFeature;
#endif

NET_EXPORT extern const base::Feature kAppendFrameOriginToNetworkIsolationKey;

NET_EXPORT extern const base::Feature
    kUseRegistrableDomainInNetworkIsolationKey;

// Turns off streaming media caching to disk.
NET_EXPORT extern const base::Feature kTurnOffStreamingMediaCaching;

}  // namespace features
}  // namespace net

#endif  // NET_BASE_FEATURES_H_
