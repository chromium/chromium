// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PMCOLOR_UTILS_H_
#define SKIA_EXT_PMCOLOR_UTILS_H_

#include <cstdint>

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/private/chromium/SkPMColor.h"

namespace skia {

// Interpret c as 4 x 8 bit channels and scale each of them by alpha/255
// (approximately).
inline uint32_t ScaleChannelsByAlpha(uint32_t c, unsigned alpha) {
  // approximate rgb * alpha / 255 with
  //             rgb * (alpha + 1) / 256
  // because >> 8 is faster than / 255
  const unsigned scale = alpha + 1;

  static constexpr uint32_t kMask = 0x00FF00FF;

  // These variables imply that the passed in color is RGBA, but because
  // each channel is independent, it also works for BGRA.
  uint32_t rb = ((c & kMask) * scale) >> 8;
  uint32_t ag = ((c >> 8) & kMask) * scale;
  return (rb & kMask) | (ag & ~kMask);
}

// Blend two premultiplied colors together
inline SkPMColor BlendSrcOver(SkPMColor src, SkPMColor dst) {
  uint32_t scale = 256 - SkPMColorGetA(src);

  static constexpr uint32_t kMask = 0x00FF00FF;
  uint32_t rb = (((dst & kMask) * scale) >> 8) & kMask;
  uint32_t ag = (((dst >> 8) & kMask) * scale) & ~kMask;

  rb += (src & kMask);
  ag += (src & ~kMask);

  // Color channels (but not alpha) can overflow, so we have to saturate to 0xFF
  // in each lane.
  return std::min(rb & 0x000001FF, 0x000000FFU) |
         std::min(ag & 0x0001FF00, 0x0000FF00U) |
         std::min(rb & 0x01FF0000, 0x00FF0000U) | (ag & 0xFF000000);
}

}  // namespace skia

#endif  // SKIA_EXT_PMCOLOR_UTILS_H_
