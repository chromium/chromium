// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_LARGEST_CONTENTFUL_PAINT_TYPE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_LARGEST_CONTENTFUL_PAINT_TYPE_H_

#include "third_party/blink/public/mojom/performance/largest_contentful_paint_type.mojom-forward.h"

namespace blink::mojom {

inline constexpr LargestContentfulPaintType operator&(
    LargestContentfulPaintType a,
    LargestContentfulPaintType b) {
  return static_cast<LargestContentfulPaintType>(static_cast<uint32_t>(a) &
                                                 static_cast<uint32_t>(b));
}

inline constexpr LargestContentfulPaintType operator|(
    LargestContentfulPaintType a,
    LargestContentfulPaintType b) {
  return static_cast<LargestContentfulPaintType>(static_cast<uint32_t>(a) |
                                                 static_cast<uint32_t>(b));
}

inline LargestContentfulPaintType& operator&=(LargestContentfulPaintType& a,
                                              LargestContentfulPaintType b) {
  return a = a & b;
}

inline LargestContentfulPaintType& operator|=(LargestContentfulPaintType& a,
                                              LargestContentfulPaintType b) {
  return a = a | b;
}

inline constexpr uint64_t LargestContentfulPaintTypeToUKMFlags(
    LargestContentfulPaintType type) {
  return static_cast<uint64_t>(type);
}

}  // namespace blink::mojom

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_LARGEST_CONTENTFUL_PAINT_TYPE_H_
