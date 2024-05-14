// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/box_reflection.h"

#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

#include <utility>

namespace blink {

BoxReflection::BoxReflection(ReflectionDirection direction, float offset)
    : BoxReflection(direction, offset, PaintRecord(), gfx::RectF()) {}

BoxReflection::BoxReflection(ReflectionDirection direction,
                             float offset,
                             PaintRecord mask,
                             const gfx::RectF& mask_bounds)
    : direction_(direction),
      offset_(offset),
      mask_(std::move(mask)),
      mask_bounds_(mask_bounds) {}

BoxReflection::BoxReflection(const BoxReflection& reflection) = default;

BoxReflection::~BoxReflection() = default;

SkMatrix BoxReflection::ReflectionMatrix() const {
  SkMatrix flip_matrix;
  switch (direction_) {
    case kVerticalReflection:
      flip_matrix.setScale(1, -1);
      flip_matrix.postTranslate(0, offset_);
      break;
    case kHorizontalReflection:
      flip_matrix.setScale(-1, 1);
      flip_matrix.postTranslate(offset_, 0);
      break;
    default:
      // MSVC requires that SkMatrix be initialized in this unreachable case.
      NOTREACHED_IN_MIGRATION();
      flip_matrix.reset();
      break;
  }
  return flip_matrix;
}

gfx::RectF BoxReflection::MapRect(const gfx::RectF& rect) const {
  SkRect reflection = gfx::RectFToSkRect(rect);
  ReflectionMatrix().mapRect(&reflection);
  return gfx::UnionRects(rect, gfx::SkRectToRectF(reflection));
}

}  // namespace blink
