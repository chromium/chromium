// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/css_mask_painter.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"

namespace blink {

base::Optional<IntRect> CSSMaskPainter::MaskBoundingBox(
    const LayoutObject& object,
    const PhysicalOffset& paint_offset) {
  if (!object.IsBoxModelObject() && !object.IsSVGChild())
    return base::nullopt;

  if (object.IsSVG()) {
    SVGResources* resources =
        SVGResourcesCache::CachedResourcesForLayoutObject(object);
    LayoutSVGResourceMasker* masker = resources ? resources->Masker() : nullptr;
    if (masker) {
      return EnclosingIntRect(
          masker->ResourceBoundingBox(object.ObjectBoundingBox()));
    }
  }

  if (object.IsSVGChild() && !object.IsSVGForeignObject())
    return base::nullopt;

  const ComputedStyle& style = object.StyleRef();
  if (!style.HasMask())
    return base::nullopt;

  PhysicalRect maximum_mask_region;
  // For HTML/CSS objects, the extent of the mask is known as "mask
  // painting area", which is determined by CSS mask-clip property.
  // We don't implement mask-clip:margin-box or no-clip currently,
  // so the maximum we can get is border-box.
  if (object.IsBox()) {
    maximum_mask_region = ToLayoutBox(object).PhysicalBorderBoxRect();
  } else {
    // For inline elements, depends on the value of box-decoration-break
    // there could be one box in multiple fragments or multiple boxes.
    // Either way here we are only interested in the bounding box of them.
    DCHECK(object.IsLayoutInline());
    maximum_mask_region = ToLayoutInline(object).PhysicalLinesBoundingBox();
  }
  if (style.HasMaskBoxImageOutsets())
    maximum_mask_region.Expand(style.MaskBoxImageOutsets());
  maximum_mask_region.offset += paint_offset;
  return PixelSnappedIntRect(maximum_mask_region);
}

ColorFilter CSSMaskPainter::MaskColorFilter(const LayoutObject& object) {
  if (!object.IsSVGChild())
    return kColorFilterNone;

  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(object);
  LayoutSVGResourceMasker* masker = resources ? resources->Masker() : nullptr;
  if (!masker)
    return kColorFilterNone;
  return masker->StyleRef().SvgStyle().MaskType() == MT_LUMINANCE
             ? kColorFilterLuminanceToAlpha
             : kColorFilterNone;
}

}  // namespace blink
