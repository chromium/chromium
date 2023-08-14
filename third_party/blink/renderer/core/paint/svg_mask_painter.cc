// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_mask_painter.h"

#include "cc/paint/color_filter.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

void SVGMaskPainter::Paint(GraphicsContext& context,
                           const LayoutObject& layout_object,
                           const DisplayItemClient& display_item_client) {
  const auto* properties = layout_object.FirstFragment().PaintProperties();
  // TODO(crbug.com/814815): This condition should be a DCHECK, but for now
  // we may paint the object for filters during PrePaint before the
  // properties are ready.
  if (!properties || !properties->Mask())
    return;

  DCHECK(properties->MaskClip());
  PropertyTreeStateOrAlias property_tree_state(
      properties->Mask()->LocalTransformSpace(), *properties->MaskClip(),
      *properties->Mask());
  ScopedPaintChunkProperties scoped_paint_chunk_properties(
      context.GetPaintController(), property_tree_state, display_item_client,
      DisplayItem::kSVGMask);

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, display_item_client,
                                                  DisplayItem::kSVGMask))
    return;

  // TODO(fs): Should clip this with the bounds of the mask's PaintRecord.
  gfx::RectF visual_rect = properties->MaskClip()->PaintClipRect().Rect();
  DrawingRecorder recorder(context, display_item_client, DisplayItem::kSVGMask,
                           gfx::ToEnclosingRect(visual_rect));

  SVGResourceClient* client = SVGResources::GetClient(layout_object);
  const ComputedStyle& style = layout_object.StyleRef();
  auto* masker = GetSVGResourceAsType<LayoutSVGResourceMasker>(
      *client, style.MaskerResource());
  DCHECK(masker);
  if (DisplayLockUtilities::LockedAncestorPreventingLayout(*masker))
    return;
  SECURITY_DCHECK(!masker->SelfNeedsFullLayout());
  masker->ClearInvalidationMask();

  gfx::RectF reference_box =
      SVGResources::ReferenceBoxForEffects(layout_object);
  AffineTransform content_transformation;
  if (masker->MaskContentUnits() ==
      SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    content_transformation.Translate(reference_box.x(), reference_box.y());
    content_transformation.ScaleNonUniform(reference_box.width(),
                                           reference_box.height());
  } else if (layout_object.IsSVGForeignObject()) {
    content_transformation.Scale(style.EffectiveZoom());
  }

  PaintRecord record =
      masker->CreatePaintRecord(content_transformation, context);

  context.Save();
  context.ConcatCTM(content_transformation);
  bool needs_luminance_layer =
      masker->StyleRef().MaskType() == EMaskType::kLuminance;
  if (needs_luminance_layer) {
    context.BeginLayer(cc::ColorFilter::MakeLuma());
  }
  context.DrawRecord(std::move(record));
  if (needs_luminance_layer)
    context.EndLayer();
  context.Restore();
}

}  // namespace blink
