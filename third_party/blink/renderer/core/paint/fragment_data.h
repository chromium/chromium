// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
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

  FragmentData& LastFragment();
  const FragmentData& LastFragment() const;

  // Physical offset of this fragment's local border box's top-left position
  // from the origin of the transform node of the fragment's property tree
  // state.
  PhysicalOffset PaintOffset() const { return paint_offset_; }
  void SetPaintOffset(const PhysicalOffset& paint_offset) {
    paint_offset_ = paint_offset;
  }

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

  // The pagination offset is the additional factor to add in to map from flow
  // thread coordinates relative to the enclosing pagination layer, to visual
  // coordinates relative to that pagination layer. Not to be used in LayoutNG
  // fragment painting.
  PhysicalOffset LegacyPaginationOffset() const {
    return rare_data_ ? rare_data_->legacy_pagination_offset : PhysicalOffset();
  }
  void SetLegacyPaginationOffset(const PhysicalOffset& pagination_offset) {
    if (rare_data_ || pagination_offset != PhysicalOffset())
      EnsureRareData().legacy_pagination_offset = pagination_offset;
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
  PropertyTreeStateOrAlias LocalBorderBoxProperties() const {
    DCHECK(HasLocalBorderBoxProperties());

    // TODO(chrishtr): this should never happen, but does in practice and
    // we haven't been able to find all of the cases where it happens yet.
    // See crbug.com/1137883. Once we find more of them, remove this.
    if (!rare_data_ || !rare_data_->local_border_box_properties)
      return PropertyTreeState::Root();
    return rare_data_->local_border_box_properties->GetPropertyTreeState();
  }
  bool HasLocalBorderBoxProperties() const {
    return rare_data_ && rare_data_->local_border_box_properties;
  }
  void ClearLocalBorderBoxProperties() {
    if (rare_data_)
      rare_data_->local_border_box_properties = nullptr;
  }
  void SetLocalBorderBoxProperties(const PropertyTreeStateOrAlias& state) {
    EnsureRareData();
    if (!rare_data_->local_border_box_properties) {
      rare_data_->local_border_box_properties =
          std::make_unique<RefCountedPropertyTreeState>(state);
    } else {
      *rare_data_->local_border_box_properties = state;
    }
  }

  void SetCullRect(const CullRect& cull_rect) {
    EnsureRareData().cull_rect_ = cull_rect;
  }
  CullRect GetCullRect() const {
    return rare_data_ ? rare_data_->cull_rect_ : CullRect();
  }
  void SetContentsCullRect(const CullRect& contents_cull_rect) {
    EnsureRareData().contents_cull_rect_ = contents_cull_rect;
  }
  CullRect GetContentsCullRect() const {
    return rare_data_ ? rare_data_->contents_cull_rect_ : CullRect();
  }

  // This is the complete set of property nodes that is inherited
  // from the ancestor before applying any local CSS properties,
  // but includes paint offset transform.
  PropertyTreeStateOrAlias PreEffectProperties() const {
    return PropertyTreeStateOrAlias(PreTransform(), PreClip(), PreEffect());
  }

  // This is the complete set of property nodes that can be used to
  // paint the contents of this fragment. It is similar to
  // |local_border_box_properties_| but includes properties (e.g.,
  // overflow clip, scroll translation) that apply to contents.
  PropertyTreeStateOrAlias ContentsProperties() const {
    return PropertyTreeStateOrAlias(PostScrollTranslation(), PostOverflowClip(),
                                    PostIsolationEffect());
  }

  const TransformPaintPropertyNodeOrAlias& PreTransform() const;
  const TransformPaintPropertyNodeOrAlias& PostScrollTranslation() const;
  const ClipPaintPropertyNodeOrAlias& PreClip() const;
  const ClipPaintPropertyNodeOrAlias& PostOverflowClip() const;
  const EffectPaintPropertyNodeOrAlias& PreEffect() const;
  const EffectPaintPropertyNodeOrAlias& PreFilter() const;
  const EffectPaintPropertyNodeOrAlias& PostIsolationEffect() const;

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
    RareData(const RareData&) = delete;
    RareData& operator=(const RareData&) = delete;
    ~RareData();

    // The following data fields are not fragment specific. Placed here just to
    // avoid separate data structure for them.
    std::unique_ptr<PaintLayer> layer;
    UniqueObjectId unique_id;
    PhysicalRect partial_invalidation_local_rect;
    IntRect partial_invalidation_visual_rect;

    // Fragment specific data.
    PhysicalOffset legacy_pagination_offset;
    LayoutUnit logical_top_in_flow_thread;
    std::unique_ptr<ObjectPaintProperties> paint_properties;
    std::unique_ptr<RefCountedPropertyTreeState> local_border_box_properties;
    bool is_clip_path_cache_valid = false;
    base::Optional<IntRect> clip_path_bounding_box;
    scoped_refptr<const RefCountedPath> clip_path_path;
    CullRect cull_rect_;
    CullRect contents_cull_rect_;
    std::unique_ptr<FragmentData> next_fragment_;
  };

  RareData& EnsureRareData();

  PhysicalOffset paint_offset_;
  std::unique_ptr<RareData> rare_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FRAGMENT_DATA_H_
