// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/css_mask_painter.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"

namespace blink {

absl::optional<gfx::RectF> CSSMaskPainter::MaskBoundingBox(
    const LayoutObject& object,
    const PhysicalOffset& paint_offset) {
  if (!object.IsBoxModelObject() && !object.IsSVGChild())
    return absl::nullopt;

  const ComputedStyle& style = object.StyleRef();
  if (object.IsSVG()) {
    if (SVGResourceClient* client = SVGResources::GetClient(object)) {
      auto* masker = GetSVGResourceAsType<LayoutSVGResourceMasker>(
          *client, style.MaskerResource());
      if (masker) {
        const gfx::RectF reference_box =
            SVGResources::ReferenceBoxForEffects(object);
        const float reference_box_zoom =
            object.IsSVGForeignObject() ? object.StyleRef().EffectiveZoom() : 1;
        return masker->ResourceBoundingBox(reference_box, reference_box_zoom);
      }
    }
  }

  if (object.IsSVGChild() && !object.IsSVGForeignObject()) {
    return absl::nullopt;
  }

  if (!style.HasMask())
    return absl::nullopt;

  PhysicalRect maximum_mask_region;
  // For HTML/CSS objects, the extent of the mask is known as "mask
  // painting area", which is determined by CSS mask-clip property.
  // We don't implement mask-clip:margin-box or no-clip currently,
  // so the maximum we can get is border-box.
  if (object.IsBox()) {
    maximum_mask_region = To<LayoutBox>(object).PhysicalBorderBoxRect();
  } else {
    // For inline elements, depends on the value of box-decoration-break
    // there could be one box in multiple fragments or multiple boxes.
    // Either way here we are only interested in the bounding box of them.
    maximum_mask_region = To<LayoutInline>(object).PhysicalLinesBoundingBox();
  }
  if (style.HasMaskBoxImageOutsets())
    maximum_mask_region.Expand(style.MaskBoxImageOutsets());
  maximum_mask_region.offset += paint_offset;
  return gfx::RectF(maximum_mask_region);
}

}  // namespace blink
