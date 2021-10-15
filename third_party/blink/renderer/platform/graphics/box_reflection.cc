// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/box_reflection.h"

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/skia/include/core/SkMatrix.h"

#include <utility>

namespace blink {

BoxReflection::BoxReflection(ReflectionDirection direction, float offset)
    : BoxReflection(direction, offset, nullptr, FloatRect()) {}

BoxReflection::BoxReflection(ReflectionDirection direction,
                             float offset,
                             sk_sp<PaintRecord> mask,
                             const FloatRect& mask_bounds)
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
      NOTREACHED();
      flip_matrix.reset();
      break;
  }
  return flip_matrix;
}

FloatRect BoxReflection::MapRect(const FloatRect& rect) const {
  SkRect reflection(rect);
  ReflectionMatrix().mapRect(&reflection);
  FloatRect result = rect;
  result.Union(reflection);
  return result;
}

}  // namespace blink
