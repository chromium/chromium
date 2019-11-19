// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/ref_counted_property_tree_state.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PaintLayer;

// Represents the data for a particular fragment of a LayoutObject.
// See README.md.
class CORE_EXPORT FragmentData {
  USING_FAST_MALLOC(FragmentData);

 public:
  FragmentData* NextFragment() const {
    return rare_data_ ? rare_data_->next_fragment_.get() : nullptr;
  }
  FragmentData& EnsureNextFragment();
  void ClearNextFragment() { DestroyTail(); }

  // Visual offset of this fragment's top-left position from the
  // "paint offset root" which is the containing root PaintLayer of the root
  // LocalFrameView, or PaintLayer with a transform, whichever is nearer along
  // the containing block chain.
  PhysicalOffset PaintOffset() const { return paint_offset_; }
  void SetPaintOffset(const PhysicalOffset& paint_offset) {
    paint_offset_ = paint_offset;
  }

  // The visual rect computed by the latest paint invalidation.
  // This rect does *not* account for composited scrolling. See LayoutObject::
  // AdjustVisualRectForCompositedScrolling().
  // It's location may be different from PaintOffset when there is visual (ink)
  // overflow to the top and/or the left.
  IntRect VisualRect() const { return visual_rect_; }
  void SetVisualRect(const IntRect& rect) { visual_rect_ = rect; }

  // An id for this object that is unique for the lifetime of the WebView.
  UniqueObjectId UniqueId() const {
    DCHECK(rare_data_);
    return rare_data_->unique_id;
  }

  // The PaintLayer associated with this LayoutBoxModelObject. This can be null
  // depending on the return value of LayoutBoxModelObject::LayerTypeRequired().
  PaintLayer* Layer() const {
    return rare_data_ ? rare_data_->layer.get() : nullptr;
  }
  void SetLayer(std::unique_ptr<PaintLayer>);

  // Visual rect of the selection on this object, in the same coordinate space
  // as DisplayItemClient::VisualRect().
  IntRect SelectionVisualRect() const {
    return rare_data_ ? rare_data_->selection_visual_rect : IntRect();
  }
  void SetSelectionVisualRect(const IntRect& r) {
    if (rare_data_ || !r.IsEmpty())
      EnsureRareData().selection_visual_rect = r;
  }

  // Covers the sub-rectangles of the object that need to be re-rastered, in the
  // object's local coordinate space.  During PrePaint, the rect mapped into
  // visual rect space will be added into PartialInvalidationVisualRect(), and
  // cleared.
  PhysicalRect PartialInvalidationLocalRect() const {
    return rare_data_ ? rare_data_->partial_invalidation_local_rect
                      : PhysicalRect();
  }
  // LayoutObject::InvalidatePaintRectangle() calls this method to accumulate
  // the sub-rectangles needing re-rasterization.
  void SetPartialInvalidationLocalRect(const PhysicalRect& r) {
    if (rare_data_ || !r.IsEmpty())
      EnsureRareData().partial_invalidation_local_rect = r;
  }

  // Covers the sub-rectangles of the object that need to be re-rastered, in
  // visual rect space (see VisualRect()). It will be cleared after the raster
  // invalidation is issued after paint.
  IntRect PartialInvalidationVisualRect() const {
    return rare_data_ ? rare_data_->partial_invalidation_visual_rect
                      : IntRect();
  }
  void SetPartialInvalidationVisualRect(const IntRect& r) {
    if (rare_data_ || !r.IsEmpty())
      EnsureRareData().partial_invalidation_visual_rect = r;
  }

  LayoutUnit LogicalTopInFlowThread() const {
    return rare_data_ ? rare_data_->logical_top_in_flow_thread : LayoutUnit();
  }
  void SetLogicalTopInFlowThread(LayoutUnit top) {
    if (rare_data_ || top)
      EnsureRareData().logical_top_in_flow_thread = top;
  }

  // The pagination offset is the additional factor to add in to map
  // from flow thread coordinates relative to the enclosing pagination
  // layer, to visual coordiantes relative to that pagination layer.
  PhysicalOffset PaginationOffset() const {
    return rare_data_ ? rare_data_->pagination_offset : PhysicalOffset();
  }
  void SetPaginationOffset(const PhysicalOffset& pagination_offset) {
    if (rare_data_ || pagination_offset != PhysicalOffset())
      EnsureRareData().pagination_offset = pagination_offset;
  }

  bool IsClipPathCacheValid() const {
    return rare_data_ && rare_data_->is_clip_path_cache_valid;
  }
  void InvalidateClipPathCache();

  base::Optional<IntRect> ClipPathBoundingBox() const {
    DCHECK(IsClipPathCacheValid());
    return rare_data_ ? rare_data_->clip_path_bounding_box : base::nullopt;
  }
  const RefCountedPath* ClipPathPath() const {
    DCHECK(IsClipPathCacheValid());
    return rare_data_ ? rare_data_->clip_path_path.get() : nullptr;
  }
  void SetClipPathCache(const IntRect& bounding_box,
                        scoped_refptr<const RefCountedPath>);
  void ClearClipPathCache() {
    if (rare_data_) {
      rare_data_->is_clip_path_cache_valid = true;
      rare_data_->clip_path_bounding_box = base::nullopt;
      rare_data_->clip_path_path = nullptr;
    }
  }

  // Holds references to the paint property nodes created by this object.
  const ObjectPaintProperties* PaintProperties() const {
    return rare_data_ ? rare_data_->paint_properties.get() : nullptr;
  }
  ObjectPaintProperties* PaintProperties() {
    return rare_data_ ? rare_data_->paint_properties.get() : nullptr;
  }
  ObjectPaintProperties& EnsurePaintProperties() {
    EnsureRareData();
    if (!rare_data_->paint_properties)
      rare_data_->paint_properties = std::make_unique<ObjectPaintProperties>();
    return *rare_data_->paint_properties;
  }
  void ClearPaintProperties() {
    if (rare_data_)
      rare_data_->paint_properties = nullptr;
  }
  void EnsureId() { EnsureRareData(); }

  // This is a complete set of property nodes that should be used as a
  // starting point to paint a LayoutObject. This data is cached because some
  // properties inherit from the containing block chain instead of the
  // painting parent and cannot be derived in O(1) during the paint walk.
  // LocalBorderBoxProperties() includes fragment clip.
  //
  // For example: <div style='opacity: 0.3;'/>
  //   The div's local border box properties would have an opacity 0.3 effect
  //   node. Even though the div has no transform, its local border box
  //   properties would have a transform node that points to the div's
  //   ancestor transform space.
  PropertyTreeState LocalBorderBoxProperties() const {
    DCHECK(HasLocalBorderBoxProperties());
    return rare_data_->local_border_box_properties->GetPropertyTreeState();
  }
  bool HasLocalBorderBoxProperties() const {
    return rare_data_ && rare_data_->local_border_box_properties;
  }
  void ClearLocalBorderBoxProperties() {
    if (rare_data_)
      rare_data_->local_border_box_properties = nullptr;
  }
  void SetLocalBorderBoxProperties(const PropertyTreeState& state) {
    EnsureRareData();
    if (!rare_data_->local_border_box_properties) {
      rare_data_->local_border_box_properties =
          std::make_unique<RefCountedPropertyTreeState>(state);
    } else {
      *rare_data_->local_border_box_properties = state;
    }
  }

  // This is the complete set of property nodes that is inherited
  // from the ancestor before applying any local CSS properties,
  // but includes paint offset transform.
  PropertyTreeState PreEffectProperties() const {
    return PropertyTreeState(PreTransform(), PreClip(), PreEffect());
  }

  // This is the complete set of property nodes that can be used to
  // paint the contents of this fragment. It is similar to
  // |local_border_box_properties_| but includes properties (e.g.,
  // overflow clip, scroll translation) that apply to contents.
  PropertyTreeState ContentsProperties() const {
    return PropertyTreeState(PostScrollTranslation(), PostOverflowClip(),
                             PostIsolationEffect());
  }

  // This is the complete set of property nodes that can be used to
  // paint mask-based clip-path.
  PropertyTreeState ClipPathProperties() const {
    DCHECK(rare_data_);
    const auto* properties = rare_data_->paint_properties.get();
    DCHECK(properties);
    DCHECK(properties->MaskClip());
    DCHECK(properties->ClipPath());
    return PropertyTreeState(properties->MaskClip()->LocalTransformSpace(),
                             *properties->MaskClip(), *properties->ClipPath());
  }

  const TransformPaintPropertyNode& PreTransform() const;
  const TransformPaintPropertyNode& PostScrollTranslation() const;
  const ClipPaintPropertyNode& PreClip() const;
  const ClipPaintPropertyNode& PostOverflowClip() const;
  const EffectPaintPropertyNode& PreEffect() const;
  const EffectPaintPropertyNode& PreFilter() const;
  const EffectPaintPropertyNode& PostIsolationEffect() const;

  // Map a rect from |this|'s local border box space to |fragment|'s local
  // border box space. Both fragments must have local border box properties.
  void MapRectToFragment(const FragmentData& fragment, IntRect&) const;

  ~FragmentData() {
    if (NextFragment())
      DestroyTail();
  }

 private:
  friend class FragmentDataTest;

  // We could let the compiler generate code to automatically destroy the
  // next_fragment_ chain, but the code would cause stack overflow in some
  // cases (e.g. fast/multicol/infinitely-tall-content-in-outer-crash.html).
  // This function destroy the next_fragment_ chain non-recursively.
  void DestroyTail();

  // Contains rare data that that is not needed on all fragments.
  struct CORE_EXPORT RareData {
    USING_FAST_MALLOC(RareData);

   public:
    RareData();
    ~RareData();

    // The following data fields are not fragment specific. Placed here just to
    // avoid separate data structure for them.
    std::unique_ptr<PaintLayer> layer;
    UniqueObjectId unique_id;
    IntRect selection_visual_rect;
    PhysicalRect partial_invalidation_local_rect;
    IntRect partial_invalidation_visual_rect;

    // Fragment specific data.
    PhysicalOffset pagination_offset;
    LayoutUnit logical_top_in_flow_thread;
    std::unique_ptr<ObjectPaintProperties> paint_properties;
    std::unique_ptr<RefCountedPropertyTreeState> local_border_box_properties;
    bool is_clip_path_cache_valid = false;
    base::Optional<IntRect> clip_path_bounding_box;
    scoped_refptr<const RefCountedPath> clip_path_path;
    std::unique_ptr<FragmentData> next_fragment_;

    DISALLOW_COPY_AND_ASSIGN(RareData);
  };

  RareData& EnsureRareData();

  IntRect visual_rect_;
  PhysicalOffset paint_offset_;

  std::unique_ptr<RareData> rare_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_H_
