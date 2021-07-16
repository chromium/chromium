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
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
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

namespace blink {

class LayoutBoxModelObject;

struct CORE_EXPORT PaintInfo {
  STACK_ALLOCATED();

 public:
  PaintInfo(GraphicsContext& context,
            const CullRect& cull_rect,
            PaintPhase phase,
            GlobalPaintFlags global_paint_flags,
            PaintLayerFlags paint_flags,
            const LayoutBoxModelObject* paint_container = nullptr)
      : context(context),
        phase(phase),
        cull_rect_(cull_rect),
        paint_container_(paint_container),
        paint_flags_(paint_flags),
        global_paint_flags_(global_paint_flags) {}

  PaintInfo(GraphicsContext& new_context,
            const PaintInfo& copy_other_fields_from)
      : context(new_context),
        phase(copy_other_fields_from.phase),
        cull_rect_(copy_other_fields_from.cull_rect_),
        paint_container_(copy_other_fields_from.paint_container_),
        fragment_id_(copy_other_fields_from.fragment_id_),
        paint_flags_(copy_other_fields_from.paint_flags_),
        global_paint_flags_(copy_other_fields_from.global_paint_flags_) {
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
  void SetCullRect(const CullRect& cull_rect) { cull_rect_ = cull_rect; }

  bool IntersectsCullRect(
      const PhysicalRect& rect,
      const PhysicalOffset& offset = PhysicalOffset()) const {
    return cull_rect_.Intersects(
        EnclosingIntRect(PhysicalRect(rect.offset + offset, rect.size)));
  }

  void ApplyInfiniteCullRect() { cull_rect_ = CullRect::Infinite(); }

  void TransformCullRect(const TransformPaintPropertyNode& transform) {
    cull_rect_.ApplyTransform(transform);
  }

  // Returns the fragment of the current painting object matching the current
  // layer fragment.
  const FragmentData* LegacyFragmentToPaint(const LayoutObject& object) const {
    if (fragment_id_ == WTF::kNotFound) {
      // We haven't been set up for legacy block fragmentation, so the object
      // better not be fragmented, then.
      DCHECK(!object.FirstFragment().NextFragment());
      return &object.FirstFragment();
    }
    for (const auto* fragment = &object.FirstFragment(); fragment;
         fragment = fragment->NextFragment()) {
      if (fragment->FragmentID() == fragment_id_)
        return fragment;
    }
    // No fragment of the current painting object matches the layer fragment,
    // which means the object should not paint in this fragment.
    return nullptr;
  }

  const FragmentData* FragmentToPaint(const LayoutObject& object) const {
    if (const auto* box = DynamicTo<LayoutBox>(&object)) {
      // We're are looking up FragmentData via LayoutObject, even though the
      // object has NG fragments. This happens with objects that don't support
      // fragment traversal, such as replaced content. We cannot use legacy-
      // based lookup in such cases, as we might not have set a fragment ID to
      // match against. Since we got here, though, it has to mean that we should
      // paint the one and only fragment.
      if (box->PhysicalFragmentCount()) {
        // TODO(mstensho): We should DCHECK that box->PhysicalFragmentCount() is
        // exactly 1 here (i.e. that the object is monolithic), but we are not
        // ready for that yet, as there's code that enters legacy paint
        // functions when we're traversing the fragment tree. See
        // e.g. NGBoxFragmentPainter::RecordScrollHitTestData(), and how it does
        // the job by invoking BoxPainter, which has no concept of
        // fragments. One of the tests that would fail with such a DCHECK here
        // is:
        // virtual/layout_ng_block_frag/fast/multicol/overflow-across-columns.html
        return &box->FirstFragment();
      }
    }
    return LegacyFragmentToPaint(object);
  }

  // Returns the FragmentData of the specified physical fragment. If we're
  // performing fragment traversal, it will map directly to the right
  // FragmentData. Otherwise we'll fall back to matching against the current
  // PaintLayerFragment.
  const FragmentData* FragmentToPaint(
      const NGPhysicalFragment& fragment) const {
    if (fragment_id_ == WTF::kNotFound)
      return fragment.GetFragmentData();
    return LegacyFragmentToPaint(*fragment.GetLayoutObject());
  }

  wtf_size_t FragmentID() const { return fragment_id_; }
  void SetFragmentID(wtf_size_t id) { fragment_id_ = id; }
  void SetIsInFragmentTraversal() { fragment_id_ = WTF::kNotFound; }

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

  // The ID of the fragment that we're currently painting.
  //
  // This is always used in legacy block fragmentation. In NG block
  // fragmentation, it's only used when painting self-painting non-atomic
  // inlines (because we currently have no way of mapping from
  // NGPhysicalFragment to FragmentData in such cases).
  wtf_size_t fragment_id_ = WTF::kNotFound;

  PaintLayerFlags paint_flags_;
  const GlobalPaintFlags global_paint_flags_;

  // For CAP only.
  bool is_painting_scrolling_background_ = false;

  // Used by display-locking.
  bool descendant_painting_blocked_ = false;
};

Image::ImageDecodingMode GetImageDecodingMode(Node*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_INFO_H_
