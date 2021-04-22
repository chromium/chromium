// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_USE_COUNTER_USE_COUNTER_FEATURE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_USE_COUNTER_USE_COUNTER_FEATURE_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom-shared.h"

namespace blink {

// `UseCounterFeature` is a union of enum feature types that UseCounter can
// count, e.g. mojom::WebFeature.
struct BLINK_COMMON_EXPORT UseCounterFeature {
  mojom::UseCounterFeatureType type;
  uint32_t value;
};

bool BLINK_COMMON_EXPORT operator==(const UseCounterFeature& lhs,
                                    const UseCounterFeature& rhs);
bool BLINK_COMMON_EXPORT operator<(const UseCounterFeature& lhs,
                                   const UseCounterFeature& rhs);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_USE_COUNTER_USE_COUNTER_FEATURE_H_
