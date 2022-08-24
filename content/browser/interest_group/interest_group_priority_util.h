// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PRIORITY_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PRIORITY_UTIL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace blink {
struct AuctionConfig;
struct InterestGroup;
}  // namespace blink

namespace content {

// Calculates the priority of `interest_group` given `auction_config` using the
// provided priority vector.
//
// `priority_vector` is either the field of that name from `interest_group`, or
// the priority vector received as part of the trusted bidding signals fetch. It
// must not be empty.
//
// `first_dot_product_priority` is the result of calling
// CalculateInterestGroupPriority() using the interest group's priority vector,
// if present, and should only be passed in when `priority_vector` is the
// priority vector received from a trusted bidding server.
CONTENT_EXPORT double CalculateInterestGroupPriority(
    const blink::AuctionConfig& auction_config,
    const blink::InterestGroup& interest_group,
    const base::flat_map<std::string, double>& priority_vector,
    absl::optional<double> first_dot_product_priority = absl::nullopt);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PRIORITY_UTIL_H_
