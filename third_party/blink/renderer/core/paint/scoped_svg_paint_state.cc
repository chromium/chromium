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

#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

ScopedSVGPaintState::~ScopedSVGPaintState() {
  if (should_paint_clip_path_as_mask_image_) {
    ClipPathClipper::PaintClipPathAsMaskImage(GetPaintInfo().context, object_,
                                              display_item_client_,
                                              PhysicalOffset());
  }
}

bool ScopedSVGPaintState::ApplyEffects() {
#if DCHECK_IS_ON()
  DCHECK(!apply_effects_called_);
  apply_effects_called_ = true;
#endif

  const auto* properties = object_.FirstFragment().PaintProperties();
  if (properties)
    ApplyPaintPropertyState(*properties);

  // When rendering clip paths as masks, only geometric operations should be
  // included so skip non-geometric operations such as compositing, masking,
  // and filtering.
  if (paint_info_.IsRenderingClipPathAsMaskImage()) {
    DCHECK(!object_.IsSVGRoot());
    if (properties && properties->ClipPathMask())
      should_paint_clip_path_as_mask_image_ = true;
    return true;
  }

  // LayoutSVGRoot and LayoutSVGForeignObject always have a self-painting
  // PaintLayer (hence comments below about PaintLayerPainter).
  bool is_svg_root_or_foreign_object =
      object_.IsSVGRoot() || object_.IsSVGForeignObject();
  if (is_svg_root_or_foreign_object) {
    // PaintLayerPainter takes care of clip path.
    DCHECK(object_.HasLayer() || !properties || !properties->ClipPathMask());
  } else if (properties && properties->ClipPathMask()) {
    should_paint_clip_path_as_mask_image_ = true;
  }

  ApplyMaskIfNecessary();
  // TODO(fs): Change return value to void.
  return true;
}

void ScopedSVGPaintState::ApplyPaintPropertyState(
    const ObjectPaintProperties& properties) {
  // SVGRoot works like normal CSS replaced element and its effects are
  // applied as stacking context effect by PaintLayerPainter.
  if (object_.IsSVGRoot())
    return;

  auto& paint_controller = paint_info_.context.GetPaintController();
  auto state = paint_controller.CurrentPaintChunkProperties();
  if (const auto* filter = properties.Filter())
    state.SetEffect(*filter);
  else if (const auto* effect = properties.Effect())
    state.SetEffect(*effect);

  if (const auto* mask_clip = properties.MaskClip())
    state.SetClip(*mask_clip);
  else if (const auto* clip_path_clip = properties.ClipPathClip())
    state.SetClip(*clip_path_clip);
  scoped_paint_chunk_properties_.emplace(
      paint_controller, state, display_item_client_,
      DisplayItem::PaintPhaseToSVGEffectType(paint_info_.phase));
}

void ScopedSVGPaintState::ApplyMaskIfNecessary() {
  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(object_);
  if (resources && resources->Masker())
    mask_painter_.emplace(paint_info_.context, object_, display_item_client_);
}

}  // namespace blink
