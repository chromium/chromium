/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "third_party/blink/renderer/core/paint/paint_layer_clipper.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

namespace blink {

static bool HasOverflowClip(
    const PaintLayer& layer) {
  if (!layer.GetLayoutObject().IsBox())
    return false;
  const LayoutBox& box = ToLayoutBox(layer.GetLayoutObject());
  return box.ShouldClipOverflow();
}

bool ClipRectsContext::ShouldRespectRootLayerClip() const {
  return respect_overflow_clip == kRespectOverflowClip;
}

static void AdjustClipRectsForChildren(
    const LayoutBoxModelObject& layout_object,
    ClipRects& clip_rects) {
  EPosition position = layout_object.StyleRef().GetPosition();
  // A fixed object is essentially the root of its containing block hierarchy,
  // so when we encounter such an object, we reset our clip rects to the
  // fixedClipRect.
  if (position == EPosition::kFixed) {
    clip_rects.SetPosClipRect(clip_rects.FixedClipRect());
    clip_rects.SetOverflowClipRect(clip_rects.FixedClipRect());
    clip_rects.SetFixed(true);
  } else if (position == EPosition::kRelative) {
    clip_rects.SetPosClipRect(clip_rects.OverflowClipRect());
  } else if (position == EPosition::kAbsolute) {
    clip_rects.SetOverflowClipRect(clip_rects.PosClipRect());
  }
}

static void ApplyClipRects(const ClipRectsContext& context,
                           const LayoutBoxModelObject& layout_object,
                           LayoutPoint offset,
                           ClipRects& clip_rects) {
  DCHECK(layout_object.IsBox());
  const LayoutBox& box = *ToLayoutBox(&layout_object);

  DCHECK(box.ShouldClipOverflow() || box.HasClip());
  LayoutView* view = box.View();
  DCHECK(view);

  if (box.ShouldClipOverflow()) {
    ClipRect new_overflow_clip =
        box.OverflowClipRect(offset, context.overlay_scrollbar_clip_behavior);
    new_overflow_clip.SetHasRadius(box.StyleRef().HasBorderRadius());
    clip_rects.SetOverflowClipRect(
        Intersection(new_overflow_clip, clip_rects.OverflowClipRect()));
    if (box.IsPositioned())
      clip_rects.SetPosClipRect(
          Intersection(new_overflow_clip, clip_rects.PosClipRect()));
    if (box.CanContainFixedPositionObjects())
      clip_rects.SetFixedClipRect(
          Intersection(new_overflow_clip, clip_rects.FixedClipRect()));
    if (box.ShouldApplyPaintContainment())
      clip_rects.SetPosClipRect(
          Intersection(new_overflow_clip, clip_rects.PosClipRect()));
  }
  if (box.HasClip()) {
    LayoutRect new_clip = box.ClipRect(offset);
    clip_rects.SetPosClipRect(Intersection(new_clip, clip_rects.PosClipRect()));
    clip_rects.SetOverflowClipRect(
        Intersection(new_clip, clip_rects.OverflowClipRect()));
    clip_rects.SetFixedClipRect(
        Intersection(new_clip, clip_rects.FixedClipRect()));
  }
}

PaintLayerClipper::PaintLayerClipper(const PaintLayer& layer,
                                     bool usegeometry_mapper)
    : layer_(layer), use_geometry_mapper_(usegeometry_mapper) {}

ClipRects* PaintLayerClipper::ClipRectsIfCached(
    const ClipRectsContext& context) const {
  DCHECK(context.UsesCache());
  if (!layer_.GetClipRectsCache())
    return nullptr;
  ClipRectsCache::Entry& entry =
      layer_.GetClipRectsCache()->Get(context.CacheSlot());
  // FIXME: We used to ASSERT that we always got a consistent root layer.
  // We should add a test that has an inconsistent root. See
  // http://crbug.com/366118 for an example.
  if (context.root_layer != entry.root)
    return nullptr;
#if DCHECK_IS_ON()
  DCHECK(entry.overlay_scrollbar_clip_behavior ==
         context.overlay_scrollbar_clip_behavior);
#endif
  return entry.clip_rects.get();
}

ClipRects& PaintLayerClipper::StoreClipRectsInCache(
    const ClipRectsContext& context,
    ClipRects* parent_clip_rects,
    const ClipRects& clip_rects) const {
  ClipRectsCache::Entry& entry =
      layer_.EnsureClipRectsCache().Get(context.CacheSlot());
  entry.root = context.root_layer;
#if DCHECK_IS_ON()
  entry.overlay_scrollbar_clip_behavior =
      context.overlay_scrollbar_clip_behavior;
#endif
  if (parent_clip_rects) {
    // If our clip rects match the clip rects of our parent, we share storage.
    if (clip_rects == *parent_clip_rects) {
      entry.clip_rects = parent_clip_rects;
      return *parent_clip_rects;
    }
  }
  entry.clip_rects = ClipRects::Create(clip_rects);
  return *entry.clip_rects;
}

ClipRects& PaintLayerClipper::GetClipRects(
    const ClipRectsContext& context) const {
  DCHECK(!use_geometry_mapper_);
  if (ClipRects* result = ClipRectsIfCached(context))
    return *result;
  // Note that it's important that we call getClipRects on our parent
  // before we call calculateClipRects so that calculateClipRects will hit
  // the cache.
  ClipRects* parent_clip_rects = nullptr;
  if (context.root_layer != &layer_ && layer_.Parent()) {
    parent_clip_rects =
        &PaintLayerClipper(*layer_.Parent(), false).GetClipRects(context);
  }
  scoped_refptr<ClipRects> clip_rects = ClipRects::Create();
  CalculateClipRects(context, *clip_rects);
  return StoreClipRectsInCache(context, parent_clip_rects, *clip_rects);
}

void PaintLayerClipper::ClearCache(ClipRectsCacheSlot cache_slot) {
  if (cache_slot == kNumberOfClipRectsCacheSlots)
    layer_.ClearClipRectsCache();
  else if (ClipRectsCache* cache = layer_.GetClipRectsCache())
    cache->Clear(cache_slot);
}

void PaintLayerClipper::ClearClipRectsIncludingDescendants() {
  ClearClipRectsIncludingDescendants(kNumberOfClipRectsCacheSlots);
}

void PaintLayerClipper::ClearClipRectsIncludingDescendants(
    ClipRectsCacheSlot cache_slot) {
  std::stack<const PaintLayer*> layers;
  layers.push(&layer_);

  while (!layers.empty()) {
    const PaintLayer* current_layer = layers.top();
    layers.pop();
    PaintLayerClipper(*current_layer, use_geometry_mapper_)
        .ClearCache(cache_slot);
    for (const PaintLayer* layer = current_layer->FirstChild(); layer;
         layer = layer->NextSibling())
      layers.push(layer);
  }
}

LayoutRect PaintLayerClipper::LocalClipRect(
    const PaintLayer& clipping_root_layer) const {
  ClipRectsContext context(
      &clipping_root_layer,
      &clipping_root_layer.GetLayoutObject().FirstFragment(),
      kPaintingClipRects);
  if (use_geometry_mapper_) {
    ClipRect clip_rect;
    CalculateBackgroundClipRectWithGeometryMapper(
        context, layer_.GetLayoutObject().FirstFragment(), kRespectOverflowClip,
        clip_rect);

    if (clip_rect.IsInfinite())
      return clip_rect.Rect();

    LayoutRect premapped_rect = clip_rect.Rect();
    // The rect now needs to be transformed to the local space of this
    // PaintLayer.
    // TODO(chrishtr): not correct for fragmentation.
    premapped_rect.MoveBy(context.root_fragment->PaintOffset());

    const auto* clip_root_layer_transform =
        context.root_fragment->LocalBorderBoxProperties().Transform();
    const auto* layer_transform = layer_.GetLayoutObject()
                                      .FirstFragment()
                                      .LocalBorderBoxProperties()
                                      .Transform();
    FloatRect clipped_rect_in_local_space(premapped_rect);
    GeometryMapper::SourceToDestinationRect(clip_root_layer_transform,
                                            layer_transform,
                                            clipped_rect_in_local_space);
    // TODO(chrishtr): not correct for fragmentation.
    clipped_rect_in_local_space.MoveBy(
        -FloatPoint(layer_.GetLayoutObject().FirstFragment().PaintOffset()));

    return LayoutRect(clipped_rect_in_local_space);
  }

  LayoutRect layer_bounds;
  ClipRect background_rect, foreground_rect;
  CalculateRects(context, nullptr, nullptr, layer_bounds, background_rect,
                 foreground_rect);

  if (background_rect.IsInfinite())
    return background_rect.Rect();

  LayoutRect clip_rect = background_rect.Rect();
  LayoutPoint clipping_root_offset;
  layer_.ConvertToLayerCoords(&clipping_root_layer, clipping_root_offset);
  clip_rect.MoveBy(-clipping_root_offset);

  return clip_rect;
}

void PaintLayerClipper::CalculateRectsWithGeometryMapper(
    const ClipRectsContext& context,
    const FragmentData& fragment_data,
    const LayoutRect* paint_dirty_rect,
    LayoutRect& layer_bounds,
    ClipRect& background_rect,
    ClipRect& foreground_rect,
    const LayoutPoint* offset_from_root) const {
  layer_bounds.SetSize(LayoutSize(layer_.PixelSnappedSize()));
  if (offset_from_root) {
    layer_bounds.SetLocation(*offset_from_root);
  } else {
    layer_bounds.SetLocation(LayoutPoint(context.sub_pixel_accumulation));
    layer_bounds.MoveBy(fragment_data.PaintOffset());
    GeometryMapper::SourceToDestinationRect(
        fragment_data.PreTransform(),
        context.root_fragment->LocalBorderBoxProperties().Transform(),
        layer_bounds);
    layer_bounds.MoveBy(-context.root_fragment->PaintOffset());
  }

  CalculateBackgroundClipRectWithGeometryMapper(
      context, fragment_data, kRespectOverflowClip, background_rect);

  foreground_rect.Reset();
  if (paint_dirty_rect)
    background_rect.Intersect(*paint_dirty_rect);

  if (ShouldClipOverflow(context)) {
    LayoutBoxModelObject& layout_object = layer_.GetLayoutObject();
    foreground_rect =
        ToLayoutBox(layout_object)
            .OverflowClipRect(layer_bounds.Location(),
                              context.overlay_scrollbar_clip_behavior);
    if (layout_object.StyleRef().HasBorderRadius())
      foreground_rect.SetHasRadius(true);
    foreground_rect.Intersect(background_rect);
  } else {
    foreground_rect = background_rect;
  }
}

void PaintLayerClipper::CalculateRects(
    const ClipRectsContext& context,
    const FragmentData* fragment_data,
    const LayoutRect* paint_dirty_rect,
    LayoutRect& layer_bounds,
    ClipRect& background_rect,
    ClipRect& foreground_rect,
    const LayoutPoint* offset_from_root) const {
  // This feature is not supported for this method.
  DCHECK(context.respect_overflow_clip != kIgnoreOverflowClipAndScroll);
  if (use_geometry_mapper_) {
    DCHECK(fragment_data);
    DCHECK(fragment_data->HasLocalBorderBoxProperties());
    // TODO(chrishtr): find the root cause of not having a fragment and fix it.
    if (!fragment_data->HasLocalBorderBoxProperties())
      return;
    CalculateRectsWithGeometryMapper(context, *fragment_data, paint_dirty_rect,
                                     layer_bounds, background_rect,
                                     foreground_rect, offset_from_root);
    return;
  }
  DCHECK(!fragment_data);

  bool is_clipping_root = &layer_ == context.root_layer;
  LayoutBoxModelObject& layout_object = layer_.GetLayoutObject();

  if (!is_clipping_root && layer_.Parent()) {
    CalculateBackgroundClipRect(context, background_rect);
    background_rect.Move(context.sub_pixel_accumulation);
  }
  if (paint_dirty_rect)
    background_rect.Intersect(*paint_dirty_rect);

  foreground_rect = background_rect;

  LayoutPoint offset(context.sub_pixel_accumulation);
  if (offset_from_root)
    offset = *offset_from_root;
  else
    layer_.ConvertToLayerCoords(context.root_layer, offset);
  layer_bounds = LayoutRect(offset, LayoutSize(layer_.PixelSnappedSize()));

  // Update the clip rects that will be passed to child layers.
  if (ShouldClipOverflow(context)) {
    LayoutRect overflow_and_clip_rect =
        ToLayoutBox(layout_object)
            .OverflowClipRect(offset, context.overlay_scrollbar_clip_behavior);
    foreground_rect.Intersect(overflow_and_clip_rect);
    if (layout_object.StyleRef().HasBorderRadius())
      foreground_rect.SetHasRadius(true);

    // FIXME: Does not do the right thing with columns yet, since we don't yet
    // factor in the individual column boxes as overflow.

    LayoutRect layer_bounds_with_visual_overflow = LocalVisualRect(context);
    layer_bounds_with_visual_overflow.MoveBy(offset);
    background_rect.Intersect(layer_bounds_with_visual_overflow);
  }

  // CSS clip (different than clipping due to overflow) can clip to any box,
  // even if it falls outside of the border box.
  if (layout_object.HasClip()) {
    // Clip applies to *us* as well, so go ahead and update the damageRect.
    LayoutRect new_pos_clip = ToLayoutBox(layout_object).ClipRect(offset);
    background_rect.Intersect(new_pos_clip);
    foreground_rect.Intersect(new_pos_clip);
  }
}

void PaintLayerClipper::CalculateClipRects(const ClipRectsContext& context,
                                           ClipRects& clip_rects) const {
  const LayoutBoxModelObject& layout_object = layer_.GetLayoutObject();
  bool is_clipping_root = &layer_ == context.root_layer;

  if (is_clipping_root && !context.ShouldRespectRootLayerClip()) {
    clip_rects.Reset(LayoutRect(LayoutRect::InfiniteIntRect()));
    if (layout_object.StyleRef().GetPosition() == EPosition::kFixed)
      clip_rects.SetFixed(true);
    return;
  }

  // For transformed layers, the root layer was shifted to be us, so there is no
  // need to examine the parent. We want to cache clip rects with us as the
  // root.
  PaintLayer* parent_layer = !is_clipping_root ? layer_.Parent() : nullptr;
  // Ensure that our parent's clip has been calculated so that we can examine
  // the values.
  if (parent_layer) {
    PaintLayerClipper(*parent_layer, use_geometry_mapper_)
        .GetOrCalculateClipRects(context, clip_rects);
  } else {
    clip_rects.Reset(LayoutRect(LayoutRect::InfiniteIntRect()));
  }

  AdjustClipRectsForChildren(layout_object, clip_rects);

  // Computing paint offset is expensive, skip the computation if the object
  // is known to have no clip. This check is redundant otherwise.
  if (HasOverflowClip(layer_) || layout_object.HasClip()) {
    // This offset cannot use convertToLayerCoords, because sometimes our
    // rootLayer may be across some transformed layer boundary, for example, in
    // the PaintLayerCompositor overlapMap, where clipRects are needed in view
    // space.
    LayoutPoint offset(layout_object.LocalToAncestorPoint(
        FloatPoint(), &context.root_layer->GetLayoutObject()));
    if (context.respect_overflow_clip == kIgnoreOverflowClipAndScroll &&
        context.root_layer->GetScrollableArea() &&
        layer_.IsAffectedByScrollOf(context.root_layer)) {
      offset.Move(LayoutSize(
          context.root_layer->GetScrollableArea()->GetScrollOffset()));
    }

    ApplyClipRects(context, layout_object, offset, clip_rects);
  }
}

static ClipRect BackgroundClipRectForPosition(const ClipRects& parent_rects,
                                              EPosition position) {
  if (position == EPosition::kFixed)
    return parent_rects.FixedClipRect();

  if (position == EPosition::kAbsolute)
    return parent_rects.PosClipRect();

  return parent_rects.OverflowClipRect();
}

void PaintLayerClipper::CalculateBackgroundClipRectWithGeometryMapper(
    const ClipRectsContext& context,
    const FragmentData& fragment_data,
    ShouldRespectOverflowClipType should_apply_self_overflow_clip,
    ClipRect& output) const {
  DCHECK(use_geometry_mapper_);

  output.Reset();
  bool is_clipping_root = &layer_ == context.root_layer;
  if (is_clipping_root && !context.ShouldRespectRootLayerClip())
    return;

  PropertyTreeState source_property_tree_state(nullptr, nullptr, nullptr);
  PropertyTreeState destination_property_tree_state(nullptr, nullptr, nullptr);
  InitializeCommonClipRectState(context, fragment_data,
                                source_property_tree_state,
                                destination_property_tree_state);

  // The background rect applies all clips *above* m_layer, but not the overflow
  // clip of m_layer. It also applies a clip to the total painting bounds
  // of m_layer, because nothing in m_layer or its children within the clip can
  // paint outside of those bounds.
  // The total painting bounds includes any visual overflow (such as shadow) and
  // filter bounds.
  //
  // TODO(chrishtr): sourceToDestinationVisualRect and
  // sourceToDestinationClipRect may not compute tight results in the presence
  // of transforms. Tight results are required for most use cases of these
  // rects, so we should add methods to GeometryMapper that guarantee there
  // are tight results, or else signal an error.
  if ((should_apply_self_overflow_clip == kRespectOverflowClip) &&
      HasOverflowClip(layer_)) {
    // Implement the following special case: if computing clip rects with
    // respect to the root, don't exclude overlay scrollbars for the background
    // rect if layer_ is the same as the root.
    OverlayScrollbarClipBehavior clip_behavior =
        context.overlay_scrollbar_clip_behavior;

    if (is_clipping_root)
      clip_behavior = kIgnorePlatformOverlayScrollbarSize;

    FloatClipRect clip_rect(FloatRect(LocalVisualRect(context)));
    clip_rect.MoveBy(FloatPoint(fragment_data.PaintOffset()));

    GeometryMapper::LocalToAncestorVisualRect(source_property_tree_state,
                                              destination_property_tree_state,
                                              clip_rect, clip_behavior);
    output.SetRect(clip_rect);
  } else if (source_property_tree_state.Clip() !=
             destination_property_tree_state.Clip()) {
    const FloatClipRect& clipped_rect_in_root_layer_space =
        GeometryMapper::LocalToAncestorClipRect(
            source_property_tree_state, destination_property_tree_state,
            context.overlay_scrollbar_clip_behavior);
    output.SetRect(clipped_rect_in_root_layer_space);
  }

  if (!output.IsInfinite()) {
    // TODO(chrishtr): generalize to multiple fragments.
    output.MoveBy(-context.root_fragment->PaintOffset());
    output.Move(context.sub_pixel_accumulation);
  }
}

void PaintLayerClipper::InitializeCommonClipRectState(
    const ClipRectsContext& context,
    const FragmentData& fragment_data,
    PropertyTreeState& source_property_tree_state,
    PropertyTreeState& destination_property_tree_state) const {
  DCHECK(use_geometry_mapper_);
  DCHECK(fragment_data.HasLocalBorderBoxProperties());
  source_property_tree_state = fragment_data.LocalBorderBoxProperties();

  DCHECK(context.root_fragment->HasLocalBorderBoxProperties());
  destination_property_tree_state =
      context.root_fragment->LocalBorderBoxProperties();

  if (context.ShouldRespectRootLayerClip()) {
    destination_property_tree_state.SetClip(context.root_fragment->PreClip());
  } else {
    destination_property_tree_state.SetClip(
        context.root_fragment->PostOverflowClip());
  }
}

LayoutRect PaintLayerClipper::LocalVisualRect(
    const ClipRectsContext& context) const {
  const LayoutObject& layout_object = layer_.GetLayoutObject();
  // The LayoutView is special since its overflow clipping rect may be larger
  // than its box rect (crbug.com/492871).
  LayoutRect layer_bounds_with_visual_overflow =
      layout_object.IsLayoutView()
          ? ToLayoutView(layout_object).ViewRect()
          : ToLayoutBox(layout_object).VisualOverflowRect();
  ToLayoutBox(layout_object)
      .FlipForWritingMode(
          // PaintLayer are in physical coordinates, so the overflow has to be
          // flipped.
          layer_bounds_with_visual_overflow);
  // At this point layer_bounds_with_visual_overflow only includes the visual
  // overflow induced by paint, prior to applying filters. This function is
  // expected the return the final visual rect after filtering.
  if (layer_.PaintsWithFilters() &&
      // If we use GeometryMapper to map to an ancestor layer, GeometryMapper
      // will handle filter effects.
      (!use_geometry_mapper_ || context.root_layer == &layer_)) {
    layer_bounds_with_visual_overflow =
        layer_.MapLayoutRectForFilter(layer_bounds_with_visual_overflow);
  }
  return layer_bounds_with_visual_overflow;
}

void PaintLayerClipper::CalculateBackgroundClipRect(
    const ClipRectsContext& context,
    ClipRect& output) const {
  if (use_geometry_mapper_) {
    const auto& fragment_data = layer_.GetLayoutObject().FirstFragment();
    DCHECK(fragment_data.HasLocalBorderBoxProperties());
    // TODO(chrishtr): find the root cause of not having a fragment and fix it.
    if (!fragment_data.HasLocalBorderBoxProperties())
      return;

    CalculateBackgroundClipRectWithGeometryMapper(context, fragment_data,
                                                  kIgnoreOverflowClip, output);
    return;
  }
  DCHECK(layer_.Parent());
  LayoutView* layout_view = layer_.GetLayoutObject().View();
  DCHECK(layout_view);

  scoped_refptr<ClipRects> parent_clip_rects = ClipRects::Create();
  if (&layer_ == context.root_layer) {
    parent_clip_rects->Reset(LayoutRect(LayoutRect::InfiniteIntRect()));
  } else {
    PaintLayerClipper(*layer_.Parent(), use_geometry_mapper_)
        .GetOrCalculateClipRects(context, *parent_clip_rects);
  }

  output = BackgroundClipRectForPosition(
      *parent_clip_rects, layer_.GetLayoutObject().StyleRef().GetPosition());
  output.Move(context.sub_pixel_accumulation);

  // Note: infinite clipRects should not be scrolled here, otherwise they will
  // accidentally no longer be considered infinite.
  if (parent_clip_rects->Fixed() &&
      &context.root_layer->GetLayoutObject() == layout_view &&
      output != LayoutRect(LayoutRect::InfiniteIntRect()))
    output.Move(LayoutSize(layout_view->OffsetForFixedPosition()));
}

void PaintLayerClipper::GetOrCalculateClipRects(const ClipRectsContext& context,
                                                ClipRects& clip_rects) const {
  DCHECK(!use_geometry_mapper_);

  if (context.UsesCache())
    clip_rects = GetClipRects(context);
  else
    CalculateClipRects(context, clip_rects);
}

bool PaintLayerClipper::ShouldClipOverflow(
    const ClipRectsContext& context) const {
  if (&layer_ == context.root_layer && !context.ShouldRespectRootLayerClip())
    return false;
  // Embedded objects with border radius need to compute clip rects when
  // painting child mask layers. We do not have access to paint phases here,
  // so always claim to clip and ignore it later when painting the foreground
  // phases.
  return HasOverflowClip(layer_) ||
         (layer_.GetLayoutObject().IsLayoutEmbeddedContent() &&
          layer_.GetLayoutObject().StyleRef().HasBorderRadius());
}

}  // namespace blink
