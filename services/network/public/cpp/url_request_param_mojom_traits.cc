// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_request_param_mojom_traits.h"

#include "base/notreached.h"
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
  NOTREACHED();
}

net::RequestPriority
EnumTraits<network::mojom::RequestPriority, net::RequestPriority>::FromMojom(
    network::mojom::RequestPriority in) {
  switch (in) {
    case network::mojom::RequestPriority::kThrottled:
      return net::THROTTLED;
    case network::mojom::RequestPriority::kIdle:
      return net::IDLE;
    case network::mojom::RequestPriority::kLowest:
      return net::LOWEST;
    case network::mojom::RequestPriority::kLow:
      return net::LOW;
    case network::mojom::RequestPriority::kMedium:
      return net::MEDIUM;
    case network::mojom::RequestPriority::kHighest:
      return net::HIGHEST;
  }

  NOTREACHED();
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
  NOTREACHED();
}

net::ReferrerPolicy
EnumTraits<network::mojom::URLRequestReferrerPolicy, net::ReferrerPolicy>::
    FromMojom(network::mojom::URLRequestReferrerPolicy in) {
  switch (in) {
    case network::mojom::URLRequestReferrerPolicy::
        kClearReferrerOnTransitionFromSecureToInsecure:
      return net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case network::mojom::URLRequestReferrerPolicy::
        kReduceReferrerGranularityOnTransitionCrossOrigin:
      return net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
    case network::mojom::URLRequestReferrerPolicy::
        kOriginOnlyOnTransitionCrossOrigin:
      return net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
    case network::mojom::URLRequestReferrerPolicy::kNeverClearReferrer:
      return net::ReferrerPolicy::NEVER_CLEAR;
    case network::mojom::URLRequestReferrerPolicy::kOrigin:
      return net::ReferrerPolicy::ORIGIN;
    case network::mojom::URLRequestReferrerPolicy::
        kClearReferrerOnTransitionCrossOrigin:
      return net::ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN;
    case network::mojom::URLRequestReferrerPolicy::
        kOriginClearOnTransitionFromSecureToInsecure:
      return net::ReferrerPolicy::
          ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case network::mojom::URLRequestReferrerPolicy::kNoReferrer:
      return net::ReferrerPolicy::NO_REFERRER;
  }

  NOTREACHED();
}

}  // namespace mojo
