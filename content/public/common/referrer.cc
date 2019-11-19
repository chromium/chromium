// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/referrer.h"

#include <string>
#include <type_traits>

#include "base/numerics/safe_conversions.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/enum_utils.h"
#include "net/base/features.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/mojom/referrer.mojom.h"

namespace content {

Referrer::Referrer(const blink::mojom::Referrer& referrer)
    : url(referrer.url), policy(referrer.policy) {}

// static
Referrer Referrer::SanitizeForRequest(const GURL& request,
                                      const Referrer& referrer) {
  blink::mojom::ReferrerPtr sanitized_referrer = SanitizeForRequest(
      request, blink::mojom::Referrer(referrer.url, referrer.policy));
  return Referrer(sanitized_referrer->url, sanitized_referrer->policy);
}

// static
blink::mojom::ReferrerPtr Referrer::SanitizeForRequest(
    const GURL& request,
    const blink::mojom::Referrer& referrer) {
  blink::mojom::ReferrerPtr sanitized_referrer = blink::mojom::Referrer::New(
      referrer.url.GetAsReferrer(), referrer.policy);
  if (sanitized_referrer->policy == network::mojom::ReferrerPolicy::kDefault) {
    if (base::FeatureList::IsEnabled(features::kReducedReferrerGranularity)) {
      sanitized_referrer->policy = network::mojom::ReferrerPolicy::
          kNoReferrerWhenDowngradeOriginWhenCrossOrigin;
    } else {
      sanitized_referrer->policy =
          network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
    }
  }

  if (sanitized_referrer->policy < network::mojom::ReferrerPolicy::kMinValue ||
      sanitized_referrer->policy > network::mojom::ReferrerPolicy::kMaxValue) {
    NOTREACHED();
    sanitized_referrer->policy = network::mojom::ReferrerPolicy::kNever;
  }

  if (!request.SchemeIsHTTPOrHTTPS() ||
      !sanitized_referrer->url.SchemeIsValidForReferrer()) {
    sanitized_referrer->url = GURL();
    return sanitized_referrer;
  }

  bool is_downgrade = sanitized_referrer->url.SchemeIsCryptographic() &&
                      !request.SchemeIsCryptographic();

  switch (sanitized_referrer->policy) {
    case network::mojom::ReferrerPolicy::kDefault:
      NOTREACHED();
      break;
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      if (is_downgrade)
        sanitized_referrer->url = GURL();
      break;
    case network::mojom::ReferrerPolicy::kAlways:
      break;
    case network::mojom::ReferrerPolicy::kNever:
      sanitized_referrer->url = GURL();
      break;
    case network::mojom::ReferrerPolicy::kOrigin:
      sanitized_referrer->url = sanitized_referrer->url.GetOrigin();
      break;
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      if (request.GetOrigin() != sanitized_referrer->url.GetOrigin())
        sanitized_referrer->url = sanitized_referrer->url.GetOrigin();
      break;
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      if (is_downgrade) {
        sanitized_referrer->url = GURL();
      } else {
        sanitized_referrer->url = sanitized_referrer->url.GetOrigin();
      }
      break;
    case network::mojom::ReferrerPolicy::kSameOrigin:
      if (request.GetOrigin() != sanitized_referrer->url.GetOrigin())
        sanitized_referrer->url = GURL();
      break;
    case network::mojom::ReferrerPolicy::
        kNoReferrerWhenDowngradeOriginWhenCrossOrigin:
      if (is_downgrade) {
        sanitized_referrer->url = GURL();
      } else if (request.GetOrigin() != sanitized_referrer->url.GetOrigin()) {
        sanitized_referrer->url = sanitized_referrer->url.GetOrigin();
      }
      break;
  }

  if ((base::FeatureList::IsEnabled(net::features::kCapRefererHeaderLength) &&
       base::saturated_cast<int>(sanitized_referrer->url.spec().length()) >
           net::features::kMaxRefererHeaderLength.Get()) ||
      (base::FeatureList::IsEnabled(
           network::features::kCapReferrerToOriginOnCrossOrigin) &&
       !url::Origin::Create(sanitized_referrer->url)
            .IsSameOriginWith(url::Origin::Create(request)))) {
    sanitized_referrer->url = sanitized_referrer->url.GetOrigin();
  }

  return sanitized_referrer;
}

// static
url::Origin Referrer::SanitizeOriginForRequest(
    const GURL& request,
    const url::Origin& initiator,
    network::mojom::ReferrerPolicy policy) {
  Referrer fake_referrer(initiator.GetURL(), policy);
  Referrer sanitizied_referrer = SanitizeForRequest(request, fake_referrer);
  return url::Origin::Create(sanitizied_referrer.url);
}

// static
net::URLRequest::ReferrerPolicy Referrer::ReferrerPolicyForUrlRequest(
    network::mojom::ReferrerPolicy referrer_policy) {
  switch (referrer_policy) {
    case network::mojom::ReferrerPolicy::kAlways:
      return net::URLRequest::NEVER_CLEAR_REFERRER;
    case network::mojom::ReferrerPolicy::kNever:
      return net::URLRequest::NO_REFERRER;
    case network::mojom::ReferrerPolicy::kOrigin:
      return net::URLRequest::ORIGIN;
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      return net::URLRequest::
          CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
    case network::mojom::ReferrerPolicy::kSameOrigin:
      return net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN;
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      return net::URLRequest::
          ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case network::mojom::ReferrerPolicy::kDefault:
      if (base::FeatureList::IsEnabled(features::kReducedReferrerGranularity)) {
        return net::URLRequest::
            REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
      }
      return net::URLRequest::
          CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case network::mojom::ReferrerPolicy::
        kNoReferrerWhenDowngradeOriginWhenCrossOrigin:
      return net::URLRequest::
          REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  }
  return net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
}

// static
network::mojom::ReferrerPolicy Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
    net::URLRequest::ReferrerPolicy net_policy) {
  switch (net_policy) {
    case net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
    case net::URLRequest::
        REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::
          kNoReferrerWhenDowngradeOriginWhenCrossOrigin;
    case net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin;
    case net::URLRequest::NEVER_CLEAR_REFERRER:
      return network::mojom::ReferrerPolicy::kAlways;
    case net::URLRequest::ORIGIN:
      return network::mojom::ReferrerPolicy::kOrigin;
    case net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kSameOrigin;
    case net::URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::ReferrerPolicy::kStrictOrigin;
    case net::URLRequest::NO_REFERRER:
      return network::mojom::ReferrerPolicy::kNever;
  }
  NOTREACHED();
  return network::mojom::ReferrerPolicy::kDefault;
}

net::URLRequest::ReferrerPolicy Referrer::GetDefaultReferrerPolicy() {
  if (base::FeatureList::IsEnabled(features::kReducedReferrerGranularity)) {
    return net::URLRequest::
        REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  }
  return net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
}

// static
network::mojom::ReferrerPolicy Referrer::ConvertToPolicy(int32_t policy) {
  return mojo::ConvertIntToMojoEnum<network::mojom::ReferrerPolicy>(policy)
      .value_or(network::mojom::ReferrerPolicy::kDefault);
}

}  // namespace content
