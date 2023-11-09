// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INTRINSIC_SIZING_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INTRINSIC_SIZING_INFO_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

struct IntrinsicSizingInfo {
  DISALLOW_NEW();

  static IntrinsicSizingInfo None() {
    return {gfx::SizeF(), gfx::SizeF(), false, false};
  }

  bool IsNone() const {
    return !has_width && !has_height && aspect_ratio.IsEmpty();
  }

  // Because they are using float instead of LayoutUnit, we can't use
  // PhysicalSize here.
  gfx::SizeF size;
  gfx::SizeF aspect_ratio;
  bool has_width = true;
  bool has_height = true;
};

inline float ResolveWidthForRatio(float height,
                                  const gfx::SizeF& natural_ratio) {
  return height * natural_ratio.width() / natural_ratio.height();
}

inline float ResolveHeightForRatio(float width,
                                   const gfx::SizeF& natural_ratio) {
  return width * natural_ratio.height() / natural_ratio.width();
}

// Implements the algorithm at
// https://www.w3.org/TR/css3-images/#default-sizing with a specified size with
// no constraints and a contain constraint.
CORE_EXPORT gfx::SizeF ConcreteObjectSize(
    const IntrinsicSizingInfo& sizing_info,
    const gfx::SizeF& default_object_size);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INTRINSIC_SIZING_INFO_H_
