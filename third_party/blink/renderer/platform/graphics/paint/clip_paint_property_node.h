// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CLIP_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CLIP_PAINT_PROPERTY_NODE_H_

#include <algorithm>
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_clip_cache.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class GeometryMapperClipCache;
class PropertyTreeState;

// A clip rect created by a css property such as "overflow" or "clip".
// Along with a reference to the transform space the clip rect is based on,
// and a parent ClipPaintPropertyNode for inherited clips.
//
// The clip tree is rooted at a node with no parent. This root node should
// not be modified.
class PLATFORM_EXPORT ClipPaintPropertyNode
    : public PaintPropertyNode<ClipPaintPropertyNode> {
 public:
  // To make it less verbose and more readable to construct and update a node,
  // a struct with default values is used to represent the state.
  struct State {
    scoped_refptr<const TransformPaintPropertyNode> local_transform_space;
    FloatRoundedRect clip_rect;
    base::Optional<FloatClipRect> clip_rect_excluding_overlay_scrollbars;
    scoped_refptr<const RefCountedPath> clip_path;

    PaintPropertyChangeType ComputeChange(const State& other) const {
      if (local_transform_space != other.local_transform_space ||
          clip_rect != other.clip_rect || clip_path != other.clip_path) {
        return PaintPropertyChangeType::kChangedOnlyValues;
      }
      if (clip_rect_excluding_overlay_scrollbars !=
          other.clip_rect_excluding_overlay_scrollbars) {
        return PaintPropertyChangeType::kChangedOnlyNonRerasterValues;
      }
      return PaintPropertyChangeType::kUnchanged;
    }
  };

  // This node is really a sentinel, and does not represent a real clip space.
  static const ClipPaintPropertyNode& Root();

  static scoped_refptr<ClipPaintPropertyNode> Create(
      const ClipPaintPropertyNode& parent,
      State&& state) {
    return base::AdoptRef(new ClipPaintPropertyNode(
        &parent, std::move(state), false /* is_parent_alias */));
  }
  static scoped_refptr<ClipPaintPropertyNode> CreateAlias(
      const ClipPaintPropertyNode& parent) {
    return base::AdoptRef(new ClipPaintPropertyNode(
        &parent,
        State{nullptr, FloatRoundedRect(LayoutRect::InfiniteIntRect())},
        true /* is_parent_alias */));
  }

  // The empty AnimationState struct is to meet the requirement of
  // ObjectPaintProperties.
  struct AnimationState {};
  PaintPropertyChangeType Update(const ClipPaintPropertyNode& parent,
                                 State&& state,
                                 const AnimationState& = AnimationState()) {
    auto parent_changed = SetParent(&parent);
    auto state_changed = state_.ComputeChange(state);
    if (state_changed != PaintPropertyChangeType::kUnchanged) {
      DCHECK(!IsParentAlias()) << "Changed the state of an alias node.";
      state_ = std::move(state);
      AddChanged(state_changed);
    }
    return std::max(parent_changed, state_changed);
  }

  // Checks if the accumulated clip from |this| to |relative_to_state.Clip()|
  // has changed, at least significance of |change|, in the space of
  // |relative_to_state.Transform()|. We check for changes of not only clip
  // nodes, but also LocalTransformSpace relative to |relative_to_state
  // .Transform()| of the clip nodes. |transform_not_to_check| specifies a
  // transform node that the caller has checked or will check its change in
  // other ways and this function should treat it as unchanged.
  bool Changed(PaintPropertyChangeType change,
               const PropertyTreeState& relative_to_state,
               const TransformPaintPropertyNode* transform_not_to_check) const;

  // Returns the local transform space of this node. Note that the function
  // first unaliases the node, meaning that it walks up the parent chain until
  // it finds a concrete node (not a parent alias) or root. The reason for this
  // is that a parent alias conceptually doesn't have a local transform space,
  // so we just want to return a convenient space which would eliminate extra
  // work. The parent's transform node qualifies as that. Also note, although
  // this is a walk up the parent chain, the only case it would be heavy is if
  // there is a long chain of nested aliases, which is unlikely.
  const TransformPaintPropertyNode& LocalTransformSpace() const {
    // TODO(vmpstr): If this becomes a performance problem, then we should audit
    // the call sites and explicitly unalias clip nodes everywhere. If this is
    // done, then here we can add a DCHECK that we never invoke this function on
    // a parent alias.
    return *Unalias().state_.local_transform_space;
  }
  const FloatRoundedRect& ClipRect() const { return state_.clip_rect; }
  const FloatClipRect ClipRectExcludingOverlayScrollbars() const {
    return state_.clip_rect_excluding_overlay_scrollbars
               ? *state_.clip_rect_excluding_overlay_scrollbars
               : FloatClipRect(state_.clip_rect);
  }

  const RefCountedPath* ClipPath() const { return state_.clip_path.get(); }

  std::unique_ptr<JSONObject> ToJSON() const;

  // Returns memory usage of the clip cache of this node plus ancestors.
  size_t CacheMemoryUsageInBytes() const;

 private:
  friend class PaintPropertyNode<ClipPaintPropertyNode>;

  ClipPaintPropertyNode(const ClipPaintPropertyNode* parent,
                        State&& state,
                        bool is_parent_alias)
      : PaintPropertyNode(parent, is_parent_alias), state_(std::move(state)) {}

  void AddChanged(PaintPropertyChangeType changed) {
    // TODO(crbug.com/814815): This is a workaround of the bug. When the bug is
    // fixed, change the following condition to
    //   DCHECK(!clip_cache_ || !clip_cache_->IsValid());
    DCHECK_NE(PaintPropertyChangeType::kUnchanged, changed);
    if (clip_cache_ && clip_cache_->IsValid()) {
      DLOG(WARNING) << "Clip tree changed without invalidating the cache.";
      GeometryMapperClipCache::ClearCache();
    }
    PaintPropertyNode::AddChanged(changed);
  }

  // For access to GetClipCache();
  friend class GeometryMapper;
  friend class GeometryMapperTest;

  GeometryMapperClipCache& GetClipCache() const {
    return const_cast<ClipPaintPropertyNode*>(this)->GetClipCache();
  }

  GeometryMapperClipCache& GetClipCache() {
    if (!clip_cache_)
      clip_cache_.reset(new GeometryMapperClipCache());
    return *clip_cache_.get();
  }

  State state_;
  std::unique_ptr<GeometryMapperClipCache> clip_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CLIP_PAINT_PROPERTY_NODE_H_
