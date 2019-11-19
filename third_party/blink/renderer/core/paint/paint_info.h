/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_INFO_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
// TODO(jchaffraix): Once we unify PaintBehavior and PaintLayerFlags, we should
// move PaintLayerFlags to PaintPhase and rename it. Thus removing the need for
// this #include
// "third_party/blink/renderer/core/paint/paint_layer_painting_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painting_info.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

#include <limits>

namespace blink {

class LayoutBoxModelObject;

struct CORE_EXPORT PaintInfo {
  USING_FAST_MALLOC(PaintInfo);

 public:
  PaintInfo(GraphicsContext& context,
            const IntRect& cull_rect,
            PaintPhase phase,
            GlobalPaintFlags global_paint_flags,
            PaintLayerFlags paint_flags,
            const LayoutBoxModelObject* paint_container = nullptr,
            LayoutUnit fragment_logical_top_in_flow_thread = LayoutUnit())
      : context(context),
        phase(phase),
        cull_rect_(cull_rect),
        paint_container_(paint_container),
        fragment_logical_top_in_flow_thread_(
            fragment_logical_top_in_flow_thread),
        paint_flags_(paint_flags),
        global_paint_flags_(global_paint_flags),
        is_painting_scrolling_background_(false),
        descendant_painting_blocked_(false) {}

  PaintInfo(GraphicsContext& new_context,
            const PaintInfo& copy_other_fields_from)
      : context(new_context),
        phase(copy_other_fields_from.phase),
        cull_rect_(copy_other_fields_from.cull_rect_),
        paint_container_(copy_other_fields_from.paint_container_),
        fragment_logical_top_in_flow_thread_(
            copy_other_fields_from.fragment_logical_top_in_flow_thread_),
        paint_flags_(copy_other_fields_from.paint_flags_),
        global_paint_flags_(copy_other_fields_from.global_paint_flags_),
        is_painting_scrolling_background_(false),
        descendant_painting_blocked_(false) {
    // We should never pass is_painting_scrolling_background_ other PaintInfo.
    DCHECK(!copy_other_fields_from.is_painting_scrolling_background_);
  }

  // Creates a PaintInfo for painting descendants. See comments about the paint
  // phases in PaintPhase.h for details.
  PaintInfo ForDescendants() const {
    PaintInfo result(*this);

    // We should never start to paint descendant when the flag is set.
    DCHECK(!result.is_painting_scrolling_background_);

    if (phase == PaintPhase::kDescendantOutlinesOnly)
      result.phase = PaintPhase::kOutline;
    else if (phase == PaintPhase::kDescendantBlockBackgroundsOnly)
      result.phase = PaintPhase::kBlockBackground;
    return result;
  }

  bool IsRenderingClipPathAsMaskImage() const {
    return paint_flags_ & kPaintLayerPaintingRenderingClipPathAsMask;
  }
  bool IsRenderingResourceSubtree() const {
    return paint_flags_ & kPaintLayerPaintingRenderingResourceSubtree;
  }

  // TODO(wangxianzhu): Rename this function to SkipBackground() for CAP.
  bool SkipRootBackground() const {
    return paint_flags_ & kPaintLayerPaintingSkipRootBackground;
  }
  void SetSkipsBackground(bool b) {
    DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
    if (b)
      paint_flags_ |= kPaintLayerPaintingSkipRootBackground;
    else
      paint_flags_ &= ~kPaintLayerPaintingSkipRootBackground;
  }

  bool IsPrinting() const { return global_paint_flags_ & kGlobalPaintPrinting; }
  bool ShouldAddUrlMetadata() const {
    return global_paint_flags_ & kGlobalPaintAddUrlMetadata;
  }

  DisplayItem::Type DisplayItemTypeForClipping() const {
    return DisplayItem::PaintPhaseToClipType(phase);
  }

  const LayoutBoxModelObject* PaintContainer() const {
    return paint_container_;
  }

  GlobalPaintFlags GetGlobalPaintFlags() const { return global_paint_flags_; }

  PaintLayerFlags PaintFlags() const { return paint_flags_; }

  const CullRect& GetCullRect() const { return cull_rect_; }

  bool IntersectsCullRect(
      const PhysicalRect& rect,
      const PhysicalOffset& offset = PhysicalOffset()) const {
    return cull_rect_.Intersects(rect.ToLayoutRect(), offset.ToLayoutPoint());
  }

  void ApplyInfiniteCullRect() { cull_rect_ = CullRect::Infinite(); }

  void TransformCullRect(const TransformPaintPropertyNode& transform) {
    cull_rect_.ApplyTransform(transform);
  }

  // Returns the fragment of the current painting object matching the current
  // layer fragment.
  const FragmentData* FragmentToPaint(const LayoutObject& object) const {
    for (const auto* fragment = &object.FirstFragment(); fragment;
         fragment = fragment->NextFragment()) {
      if (fragment->LogicalTopInFlowThread() ==
          fragment_logical_top_in_flow_thread_)
        return fragment;
    }
    // No fragment of the current painting object matches the layer fragment,
    // which means the object should not paint in this fragment.
    return nullptr;
  }

  void SetFragmentLogicalTopInFlowThread(LayoutUnit fragment_logical_top) {
    fragment_logical_top_in_flow_thread_ = fragment_logical_top;
  }

  bool IsPaintingScrollingBackground() const {
    DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
    return is_painting_scrolling_background_;
  }
  void SetIsPaintingScrollingBackground(bool b) {
    DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
    is_painting_scrolling_background_ = b;
  }

  bool DescendantPaintingBlocked() const {
    return descendant_painting_blocked_;
  }
  void SetDescendantPaintingBlocked(bool blocked) {
    descendant_painting_blocked_ = blocked;
  }

  // FIXME: Introduce setters/getters at some point. Requires a lot of changes
  // throughout paint/.
  GraphicsContext& context;
  PaintPhase phase;

 private:
  CullRect cull_rect_;

  // The box model object that originates the current painting.
  const LayoutBoxModelObject* paint_container_;

  // The logical top of the current fragment of the self-painting PaintLayer
  // which initiated the current painting, in the containing flow thread.
  LayoutUnit fragment_logical_top_in_flow_thread_;

  PaintLayerFlags paint_flags_;
  const GlobalPaintFlags global_paint_flags_;

  // For CAP only.
  bool is_painting_scrolling_background_ : 1;

  // Used by display-locking.
  bool descendant_painting_blocked_ : 1;
};

Image::ImageDecodingMode GetImageDecodingMode(Node*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_INFO_H_
