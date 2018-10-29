// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/referrer.h"

#include <string>

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "services/network/loader_util.h"

namespace content {

// static
Referrer Referrer::SanitizeForRequest(const GURL& request,
                                      const Referrer& referrer) {
  Referrer sanitized_referrer(referrer.url.GetAsReferrer(), referrer.policy);
  if (sanitized_referrer.policy == network::mojom::ReferrerPolicy::kDefault) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kReducedReferrerGranularity)) {
      sanitized_referrer.policy = network::mojom::ReferrerPolicy::
          kNoReferrerWhenDowngradeOriginWhenCrossOrigin;
    } else {
      sanitized_referrer.policy =
          network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
    }
  }

  if (sanitized_referrer.policy < network::mojom::ReferrerPolicy::kMinValue ||
      sanitized_referrer.policy > network::mojom::ReferrerPolicy::kMaxValue) {
    NOTREACHED();
    sanitized_referrer.policy = network::mojom::ReferrerPolicy::kNever;
  }

  if (!request.SchemeIsHTTPOrHTTPS() ||
      !sanitized_referrer.url.SchemeIsValidForReferrer()) {
    sanitized_referrer.url = GURL();
    return sanitized_referrer;
  }

  bool is_downgrade = sanitized_referrer.url.SchemeIsCryptographic() &&
                      !request.SchemeIsCryptographic();

  switch (sanitized_referrer.policy) {
    case network::mojom::ReferrerPolicy::kDefault:
      NOTREACHED();
      break;
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      if (is_downgrade)
        sanitized_referrer.url = GURL();
      break;
    case network::mojom::ReferrerPolicy::kAlways:
      break;
    case network::mojom::ReferrerPolicy::kNever:
      sanitized_referrer.url = GURL();
      break;
    case network::mojom::ReferrerPolicy::kOrigin:
      sanitized_referrer.url = sanitized_referrer.url.GetOrigin();
      break;
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      if (request.GetOrigin() != sanitized_referrer.url.GetOrigin())
        sanitized_referrer.url = sanitized_referrer.url.GetOrigin();
      break;
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      if (is_downgrade) {
        sanitized_referrer.url = GURL();
      } else {
        sanitized_referrer.url = sanitized_referrer.url.GetOrigin();
      }
      break;
    case network::mojom::ReferrerPolicy::kSameOrigin:
      if (request.GetOrigin() != sanitized_referrer.url.GetOrigin())
        sanitized_referrer.url = GURL();
      break;
    case network::mojom::ReferrerPolicy::
        kNoReferrerWhenDowngradeOriginWhenCrossOrigin:
      if (is_downgrade) {
        sanitized_referrer.url = GURL();
      } else if (request.GetOrigin() != sanitized_referrer.url.GetOrigin()) {
        sanitized_referrer.url = sanitized_referrer.url.GetOrigin();
      }
      break;
  }
  return sanitized_referrer;
}

// static
void Referrer::SetReferrerForRequest(net::URLRequest* request,
                                     const Referrer& referrer) {
  request->SetReferrer(network::ComputeReferrer(referrer.url));
  request->set_referrer_policy(ReferrerPolicyForUrlRequest(referrer.policy));
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
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kReducedReferrerGranularity)) {
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
    case net::URLRequest::MAX_REFERRER_POLICY:
      NOTREACHED();
      return network::mojom::ReferrerPolicy::kDefault;
  }
  NOTREACHED();
  return network::mojom::ReferrerPolicy::kDefault;
}

net::URLRequest::ReferrerPolicy Referrer::GetDefaultReferrerPolicy() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kReducedReferrerGranularity)) {
    return net::URLRequest::
        REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  }
  return net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
}

}  // namespace content
