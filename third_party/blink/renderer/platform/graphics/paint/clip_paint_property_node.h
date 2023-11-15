// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CLIP_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CLIP_PAINT_PROPERTY_NODE_H_

#include <algorithm>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_clip_cache.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class EffectPaintPropertyNode;
class GeometryMapperClipCache;
class PropertyTreeState;

// A clip rect created by a css property such as "overflow" or "clip".
// Along with a reference to the transform space the clip rect is based on,
// and a parent ClipPaintPropertyNode for inherited clips.
//
// The clip tree is rooted at a node with no parent. This root node should
// not be modified.
class ClipPaintPropertyNode;

class PLATFORM_EXPORT ClipPaintPropertyNodeOrAlias
    : public PaintPropertyNode<ClipPaintPropertyNodeOrAlias,
                               ClipPaintPropertyNode> {
 public:
  // Checks if the accumulated clip from |this| to |relative_to_state.Clip()|
  // has changed, at least significance of |change|, in the space of
  // |relative_to_state.Transform()|. We check for changes of not only clip
  // nodes, but also LocalTransformSpace relative to |relative_to_state
  // .Transform()| of the clip nodes. |transform_not_to_check| specifies a
  // transform node that the caller has checked or will check its change in
  // other ways and this function should treat it as unchanged.
  bool Changed(
      PaintPropertyChangeType change,
      const PropertyTreeState& relative_to_state,
      const TransformPaintPropertyNodeOrAlias* transform_not_to_check) const;
  bool ChangedExceptScroll(
      PaintPropertyChangeType change,
      const PropertyTreeState& relative_to_state,
      const TransformPaintPropertyNodeOrAlias* transform_not_to_check) const;

  void ClearChangedToRoot(int sequence_number) const;

  void AddChanged(PaintPropertyChangeType changed) {
    DCHECK_NE(PaintPropertyChangeType::kUnchanged, changed);
    GeometryMapperClipCache::ClearCache();
    PaintPropertyNode::AddChanged(changed);
  }

 protected:
  using PaintPropertyNode::PaintPropertyNode;

 private:
  template <bool (TransformPaintPropertyNodeOrAlias::*ChangedMethod)(
      PaintPropertyChangeType,
      const TransformPaintPropertyNodeOrAlias&) const>
  bool ChangedInternal(
      PaintPropertyChangeType change,
      const PropertyTreeState& relative_to_state,
      const TransformPaintPropertyNodeOrAlias* transform_not_to_check) const;
};

class ClipPaintPropertyNodeAlias : public ClipPaintPropertyNodeOrAlias {
 public:
  static scoped_refptr<ClipPaintPropertyNodeAlias> Create(
      const ClipPaintPropertyNodeOrAlias& parent) {
    return base::AdoptRef(new ClipPaintPropertyNodeAlias(parent));
  }

 private:
  explicit ClipPaintPropertyNodeAlias(
      const ClipPaintPropertyNodeOrAlias& parent)
      : ClipPaintPropertyNodeOrAlias(parent, kParentAlias) {}
};

class PLATFORM_EXPORT ClipPaintPropertyNode
    : public ClipPaintPropertyNodeOrAlias {
 public:
  // To make it less verbose and more readable to construct and update a node,
  // a struct with default values is used to represent the state.
  struct PLATFORM_EXPORT State {
    DISALLOW_NEW();

   public:
    State(scoped_refptr<const TransformPaintPropertyNodeOrAlias>
              local_transform_space,
          const gfx::RectF& layout_clip_rect,
          const FloatRoundedRect& paint_clip_rect)
        : local_transform_space(std::move(local_transform_space)) {
      SetClipRect(layout_clip_rect, paint_clip_rect);
    }
    State(scoped_refptr<const TransformPaintPropertyNodeOrAlias>
              local_transform_space,
          scoped_refptr<const EffectPaintPropertyNode> pixel_moving_filter)
        : local_transform_space(std::move(local_transform_space)),
          pixel_moving_filter(std::move(pixel_moving_filter)) {
      DCHECK(layout_clip_rect_.IsInfinite());
      paint_clip_rect_ = FloatRoundedRect(layout_clip_rect_.Rect());
    }

    scoped_refptr<const TransformPaintPropertyNodeOrAlias>
        local_transform_space;
    absl::optional<FloatClipRect> layout_clip_rect_excluding_overlay_scrollbars;
    absl::optional<Path> clip_path;
    // If this is not nullptr, this clip node will generate a cc clip node to
    // expand clip rect for a pixel-moving filter.
    scoped_refptr<const EffectPaintPropertyNode> pixel_moving_filter;

    void SetClipRect(const gfx::RectF& layout_clip_rect_arg,
                     const FloatRoundedRect& paint_clip_rect_arg) {
      layout_clip_rect_.SetRect(layout_clip_rect_arg);
      if (paint_clip_rect_arg.IsRounded())
        layout_clip_rect_.SetHasRadius();
      paint_clip_rect_ = paint_clip_rect_arg;
    }

    PaintPropertyChangeType ComputeChange(const State& other) const;

    bool ClipPathEquals(const absl::optional<Path>& p) const {
      return (!clip_path && !p) || (clip_path && p && *clip_path == *p);
    }

   private:
    friend class ClipPaintPropertyNode;
    FloatClipRect layout_clip_rect_;
    FloatRoundedRect paint_clip_rect_;
  };

  // This node is really a sentinel, and does not represent a real clip space.
  static const ClipPaintPropertyNode& Root();

  static scoped_refptr<ClipPaintPropertyNode> Create(
      const ClipPaintPropertyNodeOrAlias& parent,
      State&& state) {
    return base::AdoptRef(new ClipPaintPropertyNode(&parent, std::move(state)));
  }

  // The empty AnimationState struct is to meet the requirement of
  // ObjectPaintProperties.
  struct AnimationState {
    STACK_ALLOCATED();
  };
  PaintPropertyChangeType Update(const ClipPaintPropertyNodeOrAlias& parent,
                                 State&& state,
                                 const AnimationState& = AnimationState()) {
    auto parent_changed = SetParent(parent);
    auto state_changed = state_.ComputeChange(state);
    if (state_changed != PaintPropertyChangeType::kUnchanged) {
      state_ = std::move(state);
      AddChanged(state_changed);
    }
    return std::max(parent_changed, state_changed);
  }

  const ClipPaintPropertyNode& Unalias() const = delete;
  bool IsParentAlias() const = delete;

  const TransformPaintPropertyNodeOrAlias& LocalTransformSpace() const {
    return *state_.local_transform_space;
  }
  // The clip rect for painting and compositing. It may be pixel snapped, or
  // not (e.g. for SVG).
  const FloatRoundedRect& PaintClipRect() const {
    return state_.paint_clip_rect_;
  }
  // The clip rect used for GeometryMapper to map in layout coordinates.
  const FloatClipRect& LayoutClipRect() const {
    return state_.layout_clip_rect_;
  }
  const FloatClipRect& LayoutClipRectExcludingOverlayScrollbars() const {
    return state_.layout_clip_rect_excluding_overlay_scrollbars
               ? *state_.layout_clip_rect_excluding_overlay_scrollbars
               : state_.layout_clip_rect_;
  }

  const absl::optional<Path>& ClipPath() const { return state_.clip_path; }
  bool ClipPathEquals(const absl::optional<Path>& p) const {
    return state_.ClipPathEquals(p);
  }

  const EffectPaintPropertyNode* PixelMovingFilter() const {
    return state_.pixel_moving_filter.get();
  }

  const ClipPaintPropertyNode* NearestPixelMovingFilterClip() const {
    return GetClipCache().NearestPixelMovingFilterClip();
  }

  std::unique_ptr<JSONObject> ToJSON() const;

 private:
  friend class PaintPropertyNode<ClipPaintPropertyNodeOrAlias,
                                 ClipPaintPropertyNode>;

  ClipPaintPropertyNode(const ClipPaintPropertyNodeOrAlias* parent,
                        State&& state)
      : ClipPaintPropertyNodeOrAlias(parent), state_(std::move(state)) {}

  // For access to GetClipCache();
  friend class GeometryMapper;
  friend class GeometryMapperClipCache;
  friend class GeometryMapperTest;

  GeometryMapperClipCache& GetClipCache() const {
    if (!clip_cache_)
      clip_cache_ = std::make_unique<GeometryMapperClipCache>();
    clip_cache_->UpdateIfNeeded(*this);
    return *clip_cache_;
  }

  State state_;
  mutable std::unique_ptr<GeometryMapperClipCache> clip_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CLIP_PAINT_PROPERTY_NODE_H_
