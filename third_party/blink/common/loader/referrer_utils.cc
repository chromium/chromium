// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/referrer_utils.h"

#include <atomic>

#include "base/command_line.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"

namespace blink {

network::mojom::ReferrerPolicy ReferrerUtils::NetToMojoReferrerPolicy(
    net::ReferrerPolicy net_policy) {
  switch (net_policy) {
    case net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
    case net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin;
    case net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin;
    case net::ReferrerPolicy::NEVER_CLEAR:
      return network::mojom::ReferrerPolicy::kAlways;
    case net::ReferrerPolicy::ORIGIN:
      return network::mojom::ReferrerPolicy::kOrigin;
    case net::ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kSameOrigin;
    case net::ReferrerPolicy::
        ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::ReferrerPolicy::kStrictOrigin;
    case net::ReferrerPolicy::NO_REFERRER:
      return network::mojom::ReferrerPolicy::kNever;
  }
  NOTREACHED_IN_MIGRATION();
  return network::mojom::ReferrerPolicy::kDefault;
}

net::ReferrerPolicy ReferrerUtils::GetDefaultNetReferrerPolicy() {
  return net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
}

network::mojom::ReferrerPolicy ReferrerUtils::MojoReferrerPolicyResolveDefault(
    network::mojom::ReferrerPolicy referrer_policy) {
  if (referrer_policy == network::mojom::ReferrerPolicy::kDefault)
    return NetToMojoReferrerPolicy(GetDefaultNetReferrerPolicy());
  return referrer_policy;
}

}  // namespace blink
