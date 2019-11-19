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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/paint/svg_mask_painter.h"

namespace blink {

ScopedSVGPaintState::~ScopedSVGPaintState() {
  if (filter_) {
    DCHECK(SVGResourcesCache::CachedResourcesForLayoutObject(object_));
    DCHECK(
        SVGResourcesCache::CachedResourcesForLayoutObject(object_)->Filter() ==
        filter_);
    DCHECK(filter_recording_context_);
    SVGFilterPainter(*filter_).FinishEffect(object_,
                                            *filter_recording_context_);

    // Reset the paint info after the filter effect has been completed.
    filter_paint_info_ = nullptr;
  }

  if (masker_) {
    DCHECK(SVGResourcesCache::CachedResourcesForLayoutObject(object_));
    DCHECK(
        SVGResourcesCache::CachedResourcesForLayoutObject(object_)->Masker() ==
        masker_);
    SVGMaskPainter(*masker_).FinishEffect(object_, GetPaintInfo().context);
  }
}

bool ScopedSVGPaintState::ApplyClipMaskAndFilterIfNecessary() {
#if DCHECK_IS_ON()
  DCHECK(!apply_clip_mask_and_filter_if_necessary_called_);
  apply_clip_mask_and_filter_if_necessary_called_ = true;
#endif
  // In CAP we should early exit once the paint property state has been
  // applied, because all meta (non-drawing) display items are ignored in
  // CAP. However we can't simply omit them because there are still
  // non-composited painting (e.g. SVG filters in particular) that rely on
  // these meta display items.
  ApplyPaintPropertyState();

  // When rendering clip paths as masks, only geometric operations should be
  // included so skip non-geometric operations such as compositing, masking, and
  // filtering.
  if (GetPaintInfo().IsRenderingClipPathAsMaskImage()) {
    DCHECK(!object_.IsSVGRoot());
    ApplyClipIfNecessary();
    return true;
  }

  // LayoutSVGRoot and LayoutSVGForeignObject always have a self-painting
  // PaintLayer (hence comments below about PaintLayerPainter).
  bool is_svg_root_or_foreign_object =
      object_.IsSVGRoot() || object_.IsSVGForeignObject();
  if (is_svg_root_or_foreign_object) {
    // PaintLayerPainter takes care of opacity and blend mode.
    DCHECK(object_.HasLayer() || !(object_.StyleRef().HasOpacity() ||
                                   object_.StyleRef().HasBlendMode() ||
                                   object_.StyleRef().ClipPath()));
  } else {
    ApplyClipIfNecessary();
  }

  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(object_);

  if (!ApplyMaskIfNecessary(resources))
    return false;

  if (is_svg_root_or_foreign_object) {
    // PaintLayerPainter takes care of filter.
    DCHECK(object_.HasLayer() || !object_.StyleRef().HasFilter());
  } else if (!ApplyFilterIfNecessary(resources)) {
    return false;
  }

  return true;
}

void ScopedSVGPaintState::ApplyPaintPropertyState() {
  // SVGRoot works like normal CSS replaced element and its effects are
  // applied as stacking context effect by PaintLayerPainter.
  if (object_.IsSVGRoot())
    return;

  const auto* fragment = GetPaintInfo().FragmentToPaint(object_);
  if (!fragment)
    return;
  const auto* properties = fragment->PaintProperties();
  // MaskClip() implies Effect(), thus we don't need to check MaskClip().
  if (!properties || (!properties->Effect() && !properties->ClipPathClip()))
    return;

  auto& paint_controller = GetPaintInfo().context.GetPaintController();
  PropertyTreeState state = paint_controller.CurrentPaintChunkProperties();
  if (const auto* effect = properties->Effect())
    state.SetEffect(*effect);
  if (const auto* mask_clip = properties->MaskClip())
    state.SetClip(*mask_clip);
  else if (const auto* clip_path_clip = properties->ClipPathClip())
    state.SetClip(*clip_path_clip);
  scoped_paint_chunk_properties_.emplace(
      paint_controller, state, object_,
      DisplayItem::PaintPhaseToSVGEffectType(GetPaintInfo().phase));
}

void ScopedSVGPaintState::ApplyClipIfNecessary() {
  if (object_.StyleRef().ClipPath()) {
    clip_path_clipper_.emplace(GetPaintInfo().context, object_,
                               PhysicalOffset());
  }
}

bool ScopedSVGPaintState::ApplyMaskIfNecessary(SVGResources* resources) {
  if (LayoutSVGResourceMasker* masker =
          resources ? resources->Masker() : nullptr) {
    if (!SVGMaskPainter(*masker).PrepareEffect(object_, GetPaintInfo().context))
      return false;
    masker_ = masker;
  }
  return true;
}

static bool HasReferenceFilterOnly(const ComputedStyle& style) {
  if (!style.HasFilter())
    return false;
  const FilterOperations& operations = style.Filter();
  if (operations.size() != 1)
    return false;
  return operations.at(0)->GetType() == FilterOperation::REFERENCE;
}

bool ScopedSVGPaintState::ApplyFilterIfNecessary(SVGResources* resources) {
  if (!resources)
    return !HasReferenceFilterOnly(object_.StyleRef());

  LayoutSVGResourceFilter* filter = resources->Filter();
  if (!filter)
    return true;
  filter_recording_context_ =
      std::make_unique<SVGFilterRecordingContext>(GetPaintInfo().context);
  filter_ = filter;
  GraphicsContext* filter_context = SVGFilterPainter(*filter).PrepareEffect(
      object_, *filter_recording_context_);
  if (!filter_context)
    return false;

  // Because the filter needs to cache its contents we replace the context
  // during filtering with the filter's context.
  filter_paint_info_ =
      std::make_unique<PaintInfo>(*filter_context, paint_info_);

  // Because we cache the filter contents and do not invalidate on paint
  // invalidation rect changes, we need to paint the entire filter region
  // so elements outside the initial paint (due to scrolling, etc) paint.
  filter_paint_info_->ApplyInfiniteCullRect();
  return true;
}

}  // namespace blink
