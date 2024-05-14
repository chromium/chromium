// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_request_param_mojom_traits.h"

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/base/request_priority.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "services/network/public/mojom/request_priority.mojom.h"

namespace mojo {

network::mojom::RequestPriority
EnumTraits<network::mojom::RequestPriority, net::RequestPriority>::ToMojom(
    net::RequestPriority priority) {
  switch (priority) {
    case net::THROTTLED:
      return network::mojom::RequestPriority::kThrottled;
    case net::IDLE:
      return network::mojom::RequestPriority::kIdle;
    case net::LOWEST:
      return network::mojom::RequestPriority::kLowest;
    case net::LOW:
      return network::mojom::RequestPriority::kLow;
    case net::MEDIUM:
      return network::mojom::RequestPriority::kMedium;
    case net::HIGHEST:
      return network::mojom::RequestPriority::kHighest;
  }
  NOTREACHED_IN_MIGRATION();
  return static_cast<network::mojom::RequestPriority>(priority);
}

bool EnumTraits<network::mojom::RequestPriority, net::RequestPriority>::
    FromMojom(network::mojom::RequestPriority in, net::RequestPriority* out) {
  switch (in) {
    case network::mojom::RequestPriority::kThrottled:
      *out = net::THROTTLED;
      return true;
    case network::mojom::RequestPriority::kIdle:
      *out = net::IDLE;
      return true;
    case network::mojom::RequestPriority::kLowest:
      *out = net::LOWEST;
      return true;
    case network::mojom::RequestPriority::kLow:
      *out = net::LOW;
      return true;
    case network::mojom::RequestPriority::kMedium:
      *out = net::MEDIUM;
      return true;
    case network::mojom::RequestPriority::kHighest:
      *out = net::HIGHEST;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  *out = static_cast<net::RequestPriority>(in);
  return true;
}

network::mojom::URLRequestReferrerPolicy
EnumTraits<network::mojom::URLRequestReferrerPolicy,
           net::ReferrerPolicy>::ToMojom(net::ReferrerPolicy policy) {
  switch (policy) {
    case net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::URLRequestReferrerPolicy::
          kClearReferrerOnTransitionFromSecureToInsecure;
    case net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::URLRequestReferrerPolicy::
          kReduceReferrerGranularityOnTransitionCrossOrigin;
    case net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::URLRequestReferrerPolicy::
          kOriginOnlyOnTransitionCrossOrigin;
    case net::ReferrerPolicy::NEVER_CLEAR:
      return network::mojom::URLRequestReferrerPolicy::kNeverClearReferrer;
    case net::ReferrerPolicy::ORIGIN:
      return network::mojom::URLRequestReferrerPolicy::kOrigin;
    case net::ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::URLRequestReferrerPolicy::
          kClearReferrerOnTransitionCrossOrigin;
    case net::ReferrerPolicy::
        ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::URLRequestReferrerPolicy::
          kOriginClearOnTransitionFromSecureToInsecure;
    case net::ReferrerPolicy::NO_REFERRER:
      return network::mojom::URLRequestReferrerPolicy::kNoReferrer;
  }
  NOTREACHED_IN_MIGRATION();
  return static_cast<network::mojom::URLRequestReferrerPolicy>(policy);
}

bool EnumTraits<network::mojom::URLRequestReferrerPolicy, net::ReferrerPolicy>::
    FromMojom(network::mojom::URLRequestReferrerPolicy in,
              net::ReferrerPolicy* out) {
  switch (in) {
    case network::mojom::URLRequestReferrerPolicy::
        kClearReferrerOnTransitionFromSecureToInsecure:
      *out = net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      return true;
    case network::mojom::URLRequestReferrerPolicy::
        kReduceReferrerGranularityOnTransitionCrossOrigin:
      *out = net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
      return true;
    case network::mojom::URLRequestReferrerPolicy::
        kOriginOnlyOnTransitionCrossOrigin:
      *out = net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
      return true;
    case network::mojom::URLRequestReferrerPolicy::kNeverClearReferrer:
      *out = net::ReferrerPolicy::NEVER_CLEAR;
      return true;
    case network::mojom::URLRequestReferrerPolicy::kOrigin:
      *out = net::ReferrerPolicy::ORIGIN;
      return true;
    case network::mojom::URLRequestReferrerPolicy::
        kClearReferrerOnTransitionCrossOrigin:
      *out = net::ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN;
      return true;
    case network::mojom::URLRequestReferrerPolicy::
        kOriginClearOnTransitionFromSecureToInsecure:
      *out = net::ReferrerPolicy::
          ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
      return true;
    case network::mojom::URLRequestReferrerPolicy::kNoReferrer:
      *out = net::ReferrerPolicy::NO_REFERRER;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
