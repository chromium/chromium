// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_LAYOUT_UNIT_DIFFUSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_LAYOUT_UNIT_DIFFUSER_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

// Occasionally in layout we'll divide a LayoutUnit by some number, (e.g.
// distributing free-space with "justify-content:space-between" as one example).
//
// When we do this, we round down potentially leaving space unused. This can
// result in a "gap" at the end of a layout the last item should be "flush"
// with the end.
//
// To compensate for this we should "diffuse" or "dither" the remainder over
// the number of "buckets" we have. For example if we divided by 7 a "nice" way
// to diffuse would be:
//
// ___X___
// _X___X_
// _X_X_X_
// X_X_X_X
// X_XXX_X
// XXX_XXX
//
// This class works using the core concept of Bresenham's line algorithm:
// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
//
// The line algorithm works by keeping two counters, one for "x" and "y". It
// will move to the next "line" once "x" > "y".
//
// Each time we move to the next "line" indicates we should add part of the
// remainder to the returned value.
class LayoutUnitDiffuser {
  STACK_ALLOCATED();

 public:
  LayoutUnitDiffuser(LayoutUnit size, wtf_size_t buckets)
      : LayoutUnitDiffuser(size / buckets, size.RawValue() % buckets, buckets) {
    DCHECK_GE(size, LayoutUnit());
  }
  LayoutUnitDiffuser() : LayoutUnitDiffuser(LayoutUnit(), 0, 0) {}

  LayoutUnit Next() {
    constexpr LayoutUnit kEpsilon(LayoutUnit::Epsilon());

    // Ensure we never distribute more than the original size.
    if (count_ == 0) {
      return LayoutUnit();
    }
    --count_;

    x_ += dx_;
    if (x_ >= y_) {
      y_ += dy_;
      return base_ + kEpsilon;
    }
    return base_;
  }

 private:
  LayoutUnitDiffuser(LayoutUnit base, uint64_t remainder, uint64_t buckets)
      : base_(base),
        dx_(remainder * 2),
        dy_(buckets * 2),
        x_(0),
        y_(buckets),
        count_(buckets) {
    DCHECK_LE(remainder, std::numeric_limits<uint64_t>::max() / 2);
    DCHECK_LE(remainder, buckets);
  }
  const LayoutUnit base_;
  const uint64_t dx_;
  const uint64_t dy_;
  uint64_t x_;
  uint64_t y_;
  uint64_t count_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_LAYOUT_UNIT_DIFFUSER_H_
