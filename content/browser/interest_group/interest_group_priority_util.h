// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PRIORITY_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PRIORITY_UTIL_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace blink {
struct AuctionConfig;
}  // namespace blink

namespace content {

struct StorageInterestGroup;

// Calculates the priority of `storage_interest_group` given `auction_config`
// using the provided priority vector.
//
// `auction_start_time` is the time the auction started. The same value should
// be used for all calls within a single auction, to ensure consistency between
// information passed to different bidders.
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
    const StorageInterestGroup& storage_interest_group,
    const base::Time auction_start_time,
    const base::flat_map<std::string, double>& priority_vector,
    std::optional<double> first_dot_product_priority = std::nullopt);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PRIORITY_UTIL_H_
