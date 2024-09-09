// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/referrer_policy.h"

#include <string>

#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace net {

std::optional<ReferrerPolicy> ReferrerPolicyFromHeader(
    std::string_view referrer_policy_header_value) {
  using enum ReferrerPolicy;
  const auto policy_tokens =
      base::SplitStringPiece(referrer_policy_header_value, ",",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Map from lower-cased token to ReferrerPolicy. It's good for compile speed
  // to keep this sorted.
  static constexpr auto kTokenToReferrerPolicy =
      base::MakeFixedFlatMap<std::string_view, ReferrerPolicy>(
          {{"no-referrer", NO_REFERRER},
           {"no-referrer-when-downgrade",
            CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE},
           {"origin", ORIGIN},
           {"origin-when-cross-origin", ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN},
           {"same-origin", CLEAR_ON_TRANSITION_CROSS_ORIGIN},
           {"strict-origin",
            ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE},
           {"strict-origin-when-cross-origin",
            REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN},
           {"unsafe-url", NEVER_CLEAR}});

  // Per https://w3c.github.io/webappsec-referrer-policy/#unknown-policy-values,
  // use the last recognized policy value, and ignore unknown policies.
  for (const auto& token : base::Reversed(policy_tokens)) {
    const std::string lowered_token = base::ToLowerASCII(token);
    const auto it = kTokenToReferrerPolicy.find(lowered_token);
    if (it != kTokenToReferrerPolicy.end()) {
      return it->second;
    }
  }

  return std::nullopt;
}

}  // namespace net
