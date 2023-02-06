// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_UTILS_H_

#include <stdint.h>

#include <string>

#include "third_party/blink/public/common/interest_group/interest_group.h"

namespace blink {

// Helper function that converts a size string into its corresponding value and
// units. Accepts measurements in pixels (px) and screen widths (sw).
// Examples of allowed inputs: "200px" "200 px" "50sw" "25         sw"
BLINK_COMMON_EXPORT std::tuple<double, blink::InterestGroup::Size::LengthUnit>
ParseInterestGroupSizeString(const base::StringPiece input);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_UTILS_H_
