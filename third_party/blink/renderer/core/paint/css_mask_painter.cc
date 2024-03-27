// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/css_mask_painter.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/svg_mask_painter.h"
#include "third_party/blink/renderer/core/style/style_mask_source_image.h"

namespace blink {

namespace {

bool HasSingleInvalidSVGMaskReferenceMaskLayer(const LayoutObject& object,
                                               const FillLayer& first_layer) {
  if (first_layer.Next()) {
    return false;
  }
  const auto* mask_source =
      DynamicTo<StyleMaskSourceImage>(first_layer.GetImage());
  if (!mask_source || !mask_source->HasSVGMask()) {
    return false;
  }
  return !SVGMaskPainter::MaskIsValid(*mask_source, object);
}

}  // namespace

std::optional<gfx::RectF> CSSMaskPainter::MaskBoundingBox(
    const LayoutObject& object,
    const PhysicalOffset& paint_offset) {
  if (!object.IsBoxModelObject() && !object.IsSVGChild())
    return std::nullopt;

  const ComputedStyle& style = object.StyleRef();
  if (!style.HasMask())
    return std::nullopt;

  if (object.IsSVGChild()) {
    // This is a kludge. The spec[1] says that a non-existent <mask>
    // reference should yield an image layer of transparent black.
    //
    // [1] https://drafts.fxtf.org/css-masking/#the-mask-image
    if (HasSingleInvalidSVGMaskReferenceMaskLayer(object, style.MaskLayers())) {
      return std::nullopt;
    }
    // foreignObject handled by the regular box code.
    if (!object.IsSVGForeignObject()) {
      return SVGMaskPainter::ResourceBoundsForSVGChild(object);
    }
  }

  PhysicalRect maximum_mask_region;
  EFillBox maximum_mask_clip = style.MaskLayers().LayersClipMax();
  if (object.IsBox()) {
    if (maximum_mask_clip == EFillBox::kNoClip) {
      maximum_mask_region =
          To<LayoutBox>(object)
              .Layer()
              ->LocalBoundingBoxIncludingSelfPaintingDescendants();
    } else {
      // We could use a tighter rect for padding-box/content-box.
      maximum_mask_region = To<LayoutBox>(object).PhysicalBorderBoxRect();
    }
  } else {
    // For inline elements, depends on the value of box-decoration-break
    // there could be one box in multiple fragments or multiple boxes.
    // Either way here we are only interested in the bounding box of them.
    if (maximum_mask_clip == EFillBox::kNoClip) {
      maximum_mask_region =
          To<LayoutInline>(object)
              .Layer()
              ->LocalBoundingBoxIncludingSelfPaintingDescendants();
    } else {
      // We could use a tighter rect for padding-box/content-box.
      maximum_mask_region = To<LayoutInline>(object).PhysicalLinesBoundingBox();
    }
  }
  if (style.HasMaskBoxImageOutsets())
    maximum_mask_region.Expand(style.MaskBoxImageOutsets());
  maximum_mask_region.offset += paint_offset;
  return gfx::RectF(maximum_mask_region);
}

}  // namespace blink
