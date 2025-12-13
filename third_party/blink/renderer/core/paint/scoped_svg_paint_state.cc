/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/svg_mask_painter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

ScopedSVGTransformState::ScopedSVGTransformState(const PaintInfo& paint_info,
                                                 const LayoutObject& object)
    : content_paint_info_(paint_info) {
  DCHECK(object.IsSVGChild());

  const auto* fragment = &object.FirstFragment();
  const auto* properties = fragment->PaintProperties();
  if (!properties) {
    return;
  }

  // TODO(https://crbug.com/40208169): Also consider Translate, Rotate,
  // Scale, and Offset.
  if (const auto* transform_node = properties->Transform()) {
    transform_property_scope_.emplace(
        paint_info.context.GetPaintController(), *transform_node, object,
        DisplayItem::PaintPhaseToSVGTransformType(paint_info.phase));
    if (auto* context_paints = paint_info.GetSvgContextPaints()) {
      transformed_context_paints_.emplace(
          context_paints->fill, context_paints->stroke,
          context_paints->transform *
              AffineTransform::FromTransform(transform_node->Matrix()));
      content_paint_info_.SetSvgContextPaints(
          base::OptionalToPtr(transformed_context_paints_));
    }
  }
}

ScopedSVGPaintState::ScopedSVGPaintState(const LayoutObject& object,
                                         const PaintInfo& paint_info,
                                         PaintBehavior paint_behavior)
    : ScopedSVGPaintState(object, paint_info, object, paint_behavior) {}

ScopedSVGPaintState::ScopedSVGPaintState(
    const LayoutObject& object,
    const PaintInfo& paint_info,
    const DisplayItemClient& display_item_client,
    PaintBehavior paint_behavior)
    : object_(object),
      paint_info_(paint_info),
      display_item_client_(display_item_client),
      paint_behavior_(paint_behavior) {
  if (paint_info.phase == PaintPhase::kForeground) {
    ApplyEffects();
  }
}

ScopedSVGPaintState::~ScopedSVGPaintState() {
  // Paint mask before clip path as mask because if both exist, the ClipPathMask
  // effect node is a child of the Mask node (see object_paint_properties.h for
  // the node hierarchy), to ensure the clip-path mask will be applied to the
  // mask to create an intersection of the masks, then the intersection will be
  // applied to the masked content.
  if (should_paint_mask_)
    SVGMaskPainter::Paint(paint_info_.context, object_, display_item_client_);

  if (should_paint_clip_path_as_mask_image_) {
    ClipPathClipper::PaintClipPathAsMaskImage(paint_info_.context, object_,
                                              display_item_client_);
  }
}

void ScopedSVGPaintState::ApplyEffects() {
  // LayoutSVGRoot works like a normal CSS replaced element and its effects are
  // applied as stacking context effects by PaintLayerPainter.
  DCHECK(!object_.IsSVGRoot());
#if DCHECK_IS_ON()
  DCHECK(!apply_effects_called_);
  apply_effects_called_ = true;
#endif

  const auto* properties = object_.FirstFragment().PaintProperties();
  if (!properties) {
    return;
  }
  ApplyPaintPropertyState(*properties);

  // When rendering clip paths as masks, only geometric operations should be
  // included so skip non-geometric operations such as compositing, masking,
  // and filtering.
  if (paint_info_.IsRenderingClipPathAsMaskImage()) {
    if (properties->ClipPathMask())
      should_paint_clip_path_as_mask_image_ = true;
    return;
  }

  // LayoutSVGForeignObject always have a self-painting PaintLayer, and thus
  // PaintLayerPainter takes care of clip path and mask.
  if (object_.IsSVGForeignObject()) {
    DCHECK(object_.HasLayer() || !properties->ClipPathMask());
    return;
  }

  if (properties->ClipPathMask()) {
    should_paint_clip_path_as_mask_image_ = true;
  }
  if (properties->Mask()) {
    should_paint_mask_ = true;
  }
}

void ScopedSVGPaintState::ApplyPaintPropertyState(
    const ObjectPaintProperties& properties) {
  auto& paint_controller = paint_info_.context.GetPaintController();
  auto state = paint_controller.CurrentPaintChunkProperties();
  if (const auto* filter = properties.Filter()) {
    state.SetEffect(*filter);
  } else if (const auto* effect = properties.Effect()) {
    state.SetEffect(*effect);
  }
  if (const auto* filter_clip = properties.PixelMovingFilterClipExpander()) {
    state.SetClip(*filter_clip);
  } else if (const auto* mask_clip = properties.MaskClip()) {
    state.SetClip(*mask_clip);
  } else if (const auto* clip_path_clip = properties.ClipPathClip()) {
    state.SetClip(*clip_path_clip);
  }

  scoped_paint_chunk_properties_.emplace(
      paint_controller, state, display_item_client_,
      DisplayItem::PaintPhaseToSVGEffectType(paint_info_.phase));

  // If SVG element has no content other than a reference filter, we still need
  // to ensure a paint chunk is created so that the reference filter paints.
  CHECK(paint_info_.phase == PaintPhase::kForeground);
  if ((paint_behavior_.Has(PaintComponent::kReferenceFilter)) &&
      RuntimeEnabledFeatures::SvgFilterPaintsForHiddenContentEnabled() &&
      properties.Filter() && properties.Filter()->HasReferenceFilter()) {
    paint_info_.context.GetPaintController().EnsureChunk();
  }
}

ScopedSVGPaintState::PaintBehavior ScopedSVGPaintState::ComputePaintBehavior(
    const LayoutObject& object,
    const PaintInfo& paint_info,
    bool has_content) {
  DCHECK(object.IsSVG());

  // If we have content (shapes for leaves, children for containers, etc.),
  // paint everything.
  if (has_content) {
    return PaintBehavior::All();
  }

  // No content - check if we have reference filter that still needs painted. We
  // must ensure a paint chunk is created for reference filters, even if there
  // is no content to paint.
  if (paint_info.phase == PaintPhase::kForeground &&
      RuntimeEnabledFeatures::SvgFilterPaintsForHiddenContentEnabled() &&
      !paint_info.IsRenderingClipPathAsMaskImage() &&
      object.StyleRef().Filter().HasReferenceFilter()) {
    return {PaintComponent::kReferenceFilter};
  }

  // No content and no reference filter - can skip painting entirely.
  return {};
}

}  // namespace blink
