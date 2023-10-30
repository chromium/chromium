// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_mask_painter.h"

#include "cc/paint/color_filter.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {

AffineTransform MaskToContentTransform(const LayoutSVGResourceMasker& masker,
                                       const gfx::RectF& reference_box,
                                       float zoom) {
  AffineTransform content_transformation;
  if (masker.MaskContentUnits() ==
      SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    content_transformation.Translate(reference_box.x(), reference_box.y());
    content_transformation.ScaleNonUniform(reference_box.width(),
                                           reference_box.height());
  } else if (zoom != 1) {
    content_transformation.Scale(zoom);
  }
  return content_transformation;
}

LayoutSVGResourceMasker* ResolveElementReference(SVGResource* mask_resource,
                                                 SVGResourceClient& client) {
  auto* masker =
      GetSVGResourceAsType<LayoutSVGResourceMasker>(client, mask_resource);
  if (!masker) {
    return nullptr;
  }
  if (DisplayLockUtilities::LockedAncestorPreventingLayout(*masker)) {
    return nullptr;
  }
  SECURITY_DCHECK(!masker->SelfNeedsFullLayout());
  masker->ClearInvalidationMask();
  return masker;
}

void PaintSVGMask(LayoutSVGResourceMasker* masker,
                  const LayoutObject& layout_object,
                  GraphicsContext& context) {
  const ComputedStyle& style = layout_object.StyleRef();
  const gfx::RectF reference_box =
      SVGResources::ReferenceBoxForEffects(layout_object);
  const AffineTransform content_transformation = MaskToContentTransform(
      *masker, reference_box,
      layout_object.IsSVGForeignObject() ? style.EffectiveZoom() : 1);
  SubtreeContentTransformScope content_transform_scope(content_transformation);
  PaintRecord record = masker->CreatePaintRecord();

  context.Save();
  context.ConcatCTM(content_transformation);
  bool needs_luminance_layer =
      masker->StyleRef().MaskType() == EMaskType::kLuminance;
  if (needs_luminance_layer) {
    context.BeginLayer(cc::ColorFilter::MakeLuma());
  }
  context.DrawRecord(std::move(record));
  if (needs_luminance_layer) {
    context.EndLayer();
  }
  context.Restore();
}

}  // namespace

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

  PaintSVGMask(masker, layout_object, context);
}

PaintRecord SVGMaskPainter::PaintResource(SVGResource* mask_resource,
                                          SVGResourceClient& client,
                                          const gfx::RectF& reference_box,
                                          float zoom) {
  auto* masker = ResolveElementReference(mask_resource, client);
  if (!masker) {
    return PaintRecord();
  }

  const AffineTransform content_transformation =
      MaskToContentTransform(*masker, reference_box, zoom);
  SubtreeContentTransformScope content_transform_scope(content_transformation);
  PaintRecord record = masker->CreatePaintRecord();
  if (record.empty()) {
    return record;
  }
  gfx::RectF bounds = masker->ResourceBoundingBox(reference_box, zoom);
  AffineTransform origin =
      AffineTransform::Translation(-bounds.x(), -bounds.y());

  PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording();
  canvas->concat(AffineTransformToSkM44(origin * content_transformation));
  canvas->drawPicture(std::move(record));
  return recorder.finishRecordingAsPicture();
}

gfx::RectF SVGMaskPainter::ResourceBounds(SVGResource* mask_resource,
                                          SVGResourceClient& client,
                                          const gfx::RectF& reference_box,
                                          float zoom) {
  auto* masker = ResolveElementReference(mask_resource, client);
  if (!masker) {
    return gfx::RectF();
  }
  return masker->ResourceBoundingBox(reference_box, zoom);
}

EMaskType SVGMaskPainter::MaskType(SVGResource* mask_resource,
                                   SVGResourceClient& client) {
  auto* masker = ResolveElementReference(mask_resource, client);
  if (!masker) {
    return EMaskType::kAlpha;
  }
  return masker->StyleRef().MaskType();
}

}  // namespace blink
