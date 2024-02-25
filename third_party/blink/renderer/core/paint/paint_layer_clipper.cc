/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@dbaron.org>
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

static bool HasNonVisibleOverflow(const PaintLayer& layer) {
  if (const auto* box = layer.GetLayoutBox())
    return box->ShouldClipOverflowAlongEitherAxis();
  return false;
}

bool ClipRectsContext::ShouldRespectRootLayerClip() const {
  return respect_overflow_clip == kRespectOverflowClip;
}

PaintLayerClipper::PaintLayerClipper(const PaintLayer* layer) : layer_(layer) {}

void PaintLayerClipper::CalculateRects(const ClipRectsContext& context,
                                       const FragmentData& fragment_data,
                                       PhysicalOffset& layer_offset,
                                       ClipRect& background_rect,
                                       ClipRect& foreground_rect) const {
  DCHECK(fragment_data.HasLocalBorderBoxProperties());
  // TODO(chrishtr): find the root cause of not having a fragment and fix it.
  if (!fragment_data.HasLocalBorderBoxProperties()) {
    return;
  }

  layer_offset = context.sub_pixel_accumulation;
  if (layer_ == context.root_layer) {
    DCHECK_EQ(&fragment_data, context.root_fragment);
  } else {
    layer_offset += fragment_data.PaintOffset();
    auto projection = GeometryMapper::SourceToDestinationProjection(
        fragment_data.PreTransform(),
        context.root_fragment->LocalBorderBoxProperties().Transform());
    layer_offset = PhysicalOffset::FromPointFRound(
        projection.MapPoint(gfx::PointF(layer_offset)));
    layer_offset -= context.root_fragment->PaintOffset();
  }

  CalculateBackgroundClipRectInternal(context, fragment_data,
                                      kRespectOverflowClip, background_rect);

  foreground_rect.Reset();

  if (ShouldClipOverflowAlongEitherAxis(context)) {
    LayoutBoxModelObject& layout_object = layer_->GetLayoutObject();
    foreground_rect =
        To<LayoutBox>(layout_object)
            .OverflowClipRect(layer_offset,
                              context.overlay_scrollbar_clip_behavior);
    if (layout_object.StyleRef().HasBorderRadius())
      foreground_rect.SetHasRadius(true);
    foreground_rect.Intersect(background_rect);
  } else {
    foreground_rect = background_rect;
  }
}

void PaintLayerClipper::CalculateBackgroundClipRectInternal(
    const ClipRectsContext& context,
    const FragmentData& fragment_data,
    ShouldRespectOverflowClipType should_apply_self_overflow_clip,
    ClipRect& output) const {
  output.Reset();
  bool is_clipping_root = layer_ == context.root_layer;
  if (is_clipping_root && !context.ShouldRespectRootLayerClip())
    return;

  auto source_property_tree_state = fragment_data.LocalBorderBoxProperties();
  auto destination_property_tree_state =
      context.root_fragment->LocalBorderBoxProperties();
  if (context.ShouldRespectRootLayerClip()) {
    destination_property_tree_state.SetClip(context.root_fragment->PreClip());
    destination_property_tree_state.SetEffect(
        context.root_fragment->PreEffect());
  } else {
    destination_property_tree_state.SetClip(
        context.root_fragment->ContentsClip());
  }

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
      HasNonVisibleOverflow(*layer_)) {
    // Implement the following special case: if computing clip rects with
    // respect to the root, don't exclude overlay scrollbars for the background
    // rect if layer_ is the same as the root.
    OverlayScrollbarClipBehavior clip_behavior =
        context.overlay_scrollbar_clip_behavior;

    if (is_clipping_root)
      clip_behavior = kIgnoreOverlayScrollbarSize;

    FloatClipRect clip_rect(gfx::RectF(LocalVisualRect(context)));
    clip_rect.Move(gfx::Vector2dF(fragment_data.PaintOffset()));

    GeometryMapper::LocalToAncestorVisualRect(source_property_tree_state,
                                              destination_property_tree_state,
                                              clip_rect, clip_behavior);
    output.SetRect(clip_rect);
  } else if (&source_property_tree_state.Clip() !=
             &destination_property_tree_state.Clip()) {
    const FloatClipRect& clipped_rect_in_root_layer_space =
        GeometryMapper::LocalToAncestorClipRect(
            source_property_tree_state, destination_property_tree_state,
            context.overlay_scrollbar_clip_behavior);
    output.SetRect(clipped_rect_in_root_layer_space);
  }

  if (!output.IsInfinite()) {
    // TODO(chrishtr): generalize to multiple fragments.
    output.Move(-context.root_fragment->PaintOffset());
    output.Move(context.sub_pixel_accumulation);
  }
}

PhysicalRect PaintLayerClipper::LocalVisualRect(
    const ClipRectsContext& context) const {
  const LayoutObject& layout_object = layer_->GetLayoutObject();
  // The LayoutView or Global Root Scroller is special since its overflow
  // clipping rect may be larger than its box rect (crbug.com/492871).
  bool affected_by_url_bar = layout_object.IsGlobalRootScroller();
  PhysicalRect layer_bounds_with_visual_overflow =
      affected_by_url_bar ? layout_object.View()->ViewRect()
                          : To<LayoutBox>(layout_object).VisualOverflowRect();
  return layer_bounds_with_visual_overflow;
}

void PaintLayerClipper::CalculateBackgroundClipRect(
    const ClipRectsContext& context,
    ClipRect& output) const {
  const auto& fragment_data = layer_->GetLayoutObject().FirstFragment();
  DCHECK(fragment_data.HasLocalBorderBoxProperties());
  // TODO(chrishtr): find the root cause of not having a fragment and fix it.
  if (!fragment_data.HasLocalBorderBoxProperties()) {
    return;
  }

  CalculateBackgroundClipRectInternal(context, fragment_data,
                                      kIgnoreOverflowClip, output);
}

bool PaintLayerClipper::ShouldClipOverflowAlongEitherAxis(
    const ClipRectsContext& context) const {
  if (layer_ == context.root_layer && !context.ShouldRespectRootLayerClip())
    return false;
  // Embedded objects with border radius need to compute clip rects when
  // painting child mask layers. We do not have access to paint phases here,
  // so always claim to clip and ignore it later when painting the foreground
  // phases.
  return HasNonVisibleOverflow(*layer_) ||
         (layer_->GetLayoutObject().IsLayoutEmbeddedContent() &&
          layer_->GetLayoutObject().StyleRef().HasBorderRadius());
}

}  // namespace blink
