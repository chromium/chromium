// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CSS_MASK_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CSS_MASK_PAINTER_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;
struct PhysicalOffset;

class CORE_EXPORT CSSMaskPainter {
  STATIC_ONLY(CSSMaskPainter);

 public:
  // Returns the bounding box of the computed mask, which could be
  // smaller or bigger than the reference box. Returns nullopt if the
  // there is no mask or the mask is invalid.
  static base::Optional<IntRect> MaskBoundingBox(
      const LayoutObject&,
      const PhysicalOffset& paint_offset);

  // Returns the color filter used to interpret mask pixel values as opaqueness.
  // The return value is undefined if there is no mask or the mask is invalid.
  static ColorFilter MaskColorFilter(const LayoutObject&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CSS_MASK_PAINTER_H_
