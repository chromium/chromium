// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRANSFORM_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRANSFORM_PAINT_PROPERTY_NODE_H_

#include <algorithm>
#include "cc/trees/sticky_position_constraint.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_clip_cache.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_transform_cache.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

using CompositorStickyConstraint = cc::StickyPositionConstraint;

// A transform (e.g., created by css "transform" or "perspective", or for
// internal positioning such as paint offset or scrolling) along with a
// reference to the parent TransformPaintPropertyNode. The scroll tree is
// referenced by transform nodes and a transform node with an associated scroll
// node will be a 2d transform for scroll offset.
//
// The transform tree is rooted at a node with no parent. This root node should
// not be modified.
class PLATFORM_EXPORT TransformPaintPropertyNode
    : public PaintPropertyNode<TransformPaintPropertyNode> {
 public:
  enum class BackfaceVisibility : unsigned char {
    // backface-visibility is not inherited per the css spec. However, for an
    // element that don't create a new plane, for now we let the element
    // inherit the parent backface-visibility.
    kInherited,
    // backface-visibility: hidden for the new plane.
    kHidden,
    // backface-visibility: visible for the new plane.
    kVisible,
  };

  // Stores a transform and origin with an optimization for the identity and
  // 2d translation cases that avoids allocating a full matrix and origin.
  class TransformAndOrigin {
    DISALLOW_NEW();

   public:
    TransformAndOrigin() {}
    // These constructors are not explicit so that we can use FloatSize or
    // TransformationMatrix directly in the initialization list of State.
    TransformAndOrigin(const FloatSize& translation_2d)
        : translation_2d_(translation_2d) {}
    // This should be used for arbitrary matrix only. If the caller knows that
    // the transform is identity or a 2d translation, the translation_2d version
    // should be used instead.
    TransformAndOrigin(const TransformationMatrix& matrix,
                       const FloatPoint3D& origin = FloatPoint3D(),
                       bool disable_optimization = false) {
      if (!disable_optimization && matrix.IsIdentityOr2DTranslation())
        translation_2d_ = matrix.To2DTranslation();
      else
        matrix_and_origin_.reset(new MatrixAndOrigin{matrix, origin});
    }

    bool IsIdentityOr2DTranslation() const { return !matrix_and_origin_; }
    bool IsIdentity() const {
      return !matrix_and_origin_ && translation_2d_.IsZero();
    }
    const FloatSize& Translation2D() const {
      DCHECK(IsIdentityOr2DTranslation());
      return translation_2d_;
    }
    const TransformationMatrix& Matrix() const {
      DCHECK(matrix_and_origin_);
      return matrix_and_origin_->matrix;
    }
    TransformationMatrix SlowMatrix() const {
      return matrix_and_origin_
                 ? matrix_and_origin_->matrix
                 : TransformationMatrix().Translate(translation_2d_.Width(),
                                                    translation_2d_.Height());
    }
    FloatPoint3D Origin() const {
      return matrix_and_origin_ ? matrix_and_origin_->origin : FloatPoint3D();
    }
    bool TransformEquals(const TransformAndOrigin& other) const {
      return translation_2d_ == other.translation_2d_ &&
             ((!matrix_and_origin_ && !other.matrix_and_origin_) ||
              (matrix_and_origin_ && other.matrix_and_origin_ &&
               matrix_and_origin_->matrix == other.matrix_and_origin_->matrix));
    }

    bool ChangePreserves2dAxisAlignment(const TransformAndOrigin& other) const {
      if (IsIdentityOr2DTranslation() && other.IsIdentityOr2DTranslation())
        return true;
      if (IsIdentityOr2DTranslation())
        return other.Matrix().Preserves2dAxisAlignment();
      if (other.IsIdentityOr2DTranslation())
        return Matrix().Preserves2dAxisAlignment();
      // TODO(crbug.com/960481): Consider more rare corner cases.
      return (Matrix().Inverse() * other.Matrix()).Preserves2dAxisAlignment();
    }

   private:
    struct MatrixAndOrigin {
      TransformationMatrix matrix;
      FloatPoint3D origin;
    };
    FloatSize translation_2d_;
    std::unique_ptr<MatrixAndOrigin> matrix_and_origin_;
  };

  struct AnimationState {
    AnimationState() {}
    bool is_running_animation_on_compositor = false;
  };

  // To make it less verbose and more readable to construct and update a node,
  // a struct with default values is used to represent the state.
  struct State {
    TransformAndOrigin transform_and_origin;
    scoped_refptr<const ScrollPaintPropertyNode> scroll;
    // Use bitfield packing instead of separate bools to save space.
    struct Flags {
      bool flattens_inherited_transform : 1;
      bool affected_by_outer_viewport_bounds_delta : 1;
      bool in_subtree_of_page_scale : 1;
      bool animation_is_axis_aligned : 1;
    } flags = {false, false, true, false};
    BackfaceVisibility backface_visibility = BackfaceVisibility::kInherited;
    unsigned rendering_context_id = 0;
    CompositingReasons direct_compositing_reasons = CompositingReason::kNone;
    CompositorElementId compositor_element_id;
    std::unique_ptr<CompositorStickyConstraint> sticky_constraint;

    PaintPropertyChangeType ComputeChange(
        const State& other,
        const AnimationState& animation_state) const {
      if (flags.flattens_inherited_transform !=
              other.flags.flattens_inherited_transform ||
          flags.affected_by_outer_viewport_bounds_delta !=
              other.flags.affected_by_outer_viewport_bounds_delta ||
          flags.in_subtree_of_page_scale !=
              other.flags.in_subtree_of_page_scale ||
          flags.animation_is_axis_aligned !=
              other.flags.animation_is_axis_aligned ||
          backface_visibility != other.backface_visibility ||
          rendering_context_id != other.rendering_context_id ||
          compositor_element_id != other.compositor_element_id ||
          scroll != other.scroll || !StickyConstraintEquals(other)) {
        return PaintPropertyChangeType::kChangedOnlyValues;
      }

      bool matrix_changed =
          !transform_and_origin.TransformEquals(other.transform_and_origin);
      bool origin_changed =
          transform_and_origin.Origin() != other.transform_and_origin.Origin();
      bool transform_changed = matrix_changed || origin_changed;

      bool transform_has_simple_change = true;
      if (!transform_changed) {
        transform_has_simple_change = false;
      } else if (animation_state.is_running_animation_on_compositor) {
        transform_has_simple_change = false;
      } else if (matrix_changed &&
                 !transform_and_origin.ChangePreserves2dAxisAlignment(
                     other.transform_and_origin)) {
        // An additional cc::EffectNode may be required if
        // blink::TransformPaintPropertyNode is not axis-aligned (see:
        // PropertyTreeManager::NeedsSyntheticEffect). Changes to axis alignment
        // are therefore treated as non-simple. We do not need to check origin
        // because axis alignment is not affected by transform origin.
        transform_has_simple_change = false;
      }

      // If the transform changed, and it's not simple then we need to report
      // values change.
      if (transform_changed && !transform_has_simple_change &&
          !animation_state.is_running_animation_on_compositor) {
        return PaintPropertyChangeType::kChangedOnlyValues;
      }

      bool non_reraster_values_changed =
          direct_compositing_reasons != other.direct_compositing_reasons;
      // Both simple value change and non-reraster change is upgraded to value
      // change.
      if (non_reraster_values_changed && transform_has_simple_change)
        return PaintPropertyChangeType::kChangedOnlyValues;
      if (non_reraster_values_changed)
        return PaintPropertyChangeType::kChangedOnlyNonRerasterValues;
      if (transform_has_simple_change)
        return PaintPropertyChangeType::kChangedOnlySimpleValues;
      // At this point, our transform change isn't simple, and the above checks
      // didn't return a values change, so it must mean that we're running a
      // compositor animation here.
      if (transform_changed) {
        DCHECK(animation_state.is_running_animation_on_compositor);
        return PaintPropertyChangeType::kChangedOnlyCompositedValues;
      }
      return PaintPropertyChangeType::kUnchanged;
    }

    bool StickyConstraintEquals(const State& other) const {
      if (!sticky_constraint && !other.sticky_constraint)
        return true;
      return sticky_constraint && other.sticky_constraint &&
             *sticky_constraint == *other.sticky_constraint;
    }
  };

  // This node is really a sentinel, and does not represent a real transform
  // space.
  static const TransformPaintPropertyNode& Root();

  static scoped_refptr<TransformPaintPropertyNode> Create(
      const TransformPaintPropertyNode& parent,
      State&& state) {
    return base::AdoptRef(new TransformPaintPropertyNode(
        &parent, std::move(state), false /* is_parent_alias */));
  }
  static scoped_refptr<TransformPaintPropertyNode> CreateAlias(
      const TransformPaintPropertyNode& parent) {
    return base::AdoptRef(new TransformPaintPropertyNode(
        &parent, State{}, true /* is_parent_alias */));
  }

  PaintPropertyChangeType Update(
      const TransformPaintPropertyNode& parent,
      State&& state,
      const AnimationState& animation_state = AnimationState()) {
    auto parent_changed = SetParent(&parent);
    auto state_changed = state_.ComputeChange(state, animation_state);
    if (state_changed != PaintPropertyChangeType::kUnchanged) {
      DCHECK(!IsParentAlias()) << "Changed the state of an alias node.";
      state_ = std::move(state);
      AddChanged(state_changed);
      Validate();
    }
    return std::max(parent_changed, state_changed);
  }

  // If |relative_to_node| is an ancestor of |this|, returns true if any node is
  // marked changed, at least significance of |change|, along the path from
  // |this| to |relative_to_node| (not included). Otherwise returns the combined
  // changed status of the paths from |this| and |relative_to_node| to the root.
  bool Changed(PaintPropertyChangeType change,
               const TransformPaintPropertyNode& relative_to_node) const;

  bool IsIdentityOr2DTranslation() const {
    return state_.transform_and_origin.IsIdentityOr2DTranslation();
  }
  bool IsIdentity() const { return state_.transform_and_origin.IsIdentity(); }
  // Only available when IsIdentityOr2DTranslation() is true.
  const FloatSize& Translation2D() const {
    return state_.transform_and_origin.Translation2D();
  }
  // Only available when IsIdentityOr2DTranslation() is false.
  const TransformationMatrix& Matrix() const {
    return state_.transform_and_origin.Matrix();
  }
  TransformationMatrix MatrixWithOriginApplied() const {
    return TransformationMatrix(Matrix()).ApplyTransformOrigin(Origin());
  }
  // The slow version always return meaningful TransformationMatrix regardless
  // of IsIdentityOr2DTranslation(). Should be used only in contexts that are
  // not performance sensitive.
  TransformationMatrix SlowMatrix() const {
    return state_.transform_and_origin.SlowMatrix();
  }
  FloatPoint3D Origin() const { return state_.transform_and_origin.Origin(); }

  // The associated scroll node, or nullptr otherwise.
  const ScrollPaintPropertyNode* ScrollNode() const {
    return state_.scroll.get();
  }

  // If true, this node is translated by the viewport bounds delta, which is
  // used to keep bottom-fixed elements appear fixed to the bottom of the
  // screen in the presence of URL bar movement.
  bool IsAffectedByOuterViewportBoundsDelta() const {
    return state_.flags.affected_by_outer_viewport_bounds_delta;
  }

  // If true, this node is a descendant of the page scale transform. This is
  // important for avoiding raster during pinch-zoom (see: crbug.com/951861).
  bool IsInSubtreeOfPageScale() const {
    return state_.flags.in_subtree_of_page_scale;
  }

  const CompositorStickyConstraint* GetStickyConstraint() const {
    return state_.sticky_constraint.get();
  }

  // If this is a scroll offset translation (i.e., has an associated scroll
  // node), returns this. Otherwise, returns the transform node that this node
  // scrolls with respect to. This can require a full ancestor traversal.
  const TransformPaintPropertyNode& NearestScrollTranslationNode() const;

  // If true, content with this transform node (or its descendant) appears in
  // the plane of its parent. This is implemented by flattening the total
  // accumulated transform from its ancestors.
  bool FlattensInheritedTransform() const {
    return state_.flags.flattens_inherited_transform;
  }

  // Returns the local BackfaceVisibility value set on this node. To be used
  // for testing only; use |BackfaceVisibilitySameAsParent()| or
  // |IsBackfaceHidden()| for production code.
  BackfaceVisibility GetBackfaceVisibilityForTesting() const {
    return state_.backface_visibility;
  }

  // Returns true if the backface visibility for this node is the same as that
  // of its parent. This will be true for the Root node.
  bool BackfaceVisibilitySameAsParent() const {
    if (IsRoot())
      return true;
    if (state_.backface_visibility == BackfaceVisibility::kInherited)
      return true;
    if (state_.backface_visibility ==
        Parent()->Unalias().state_.backface_visibility)
      return true;
    return IsBackfaceHidden() == Parent()->Unalias().IsBackfaceHidden();
  }

  // Returns true if the flattens inherited transform setting for this node is
  // the same as that of its parent. This will be true for the Root node.
  bool FlattensInheritedTransformSameAsParent() const {
    if (IsRoot())
      return true;
    return state_.flags.flattens_inherited_transform ==
           Parent()->Unalias().state_.flags.flattens_inherited_transform;
  }

  // Returns the first non-inherited BackefaceVisibility value along the
  // transform node ancestor chain, including this node's value if it is
  // non-inherited. TODO(wangxianzhu): Let PaintPropertyTreeBuilder calculate
  // the value instead of walking up the tree.
  bool IsBackfaceHidden() const {
    const auto* node = this;
    while (node &&
           node->state_.backface_visibility == BackfaceVisibility::kInherited)
      node = node->Parent();
    return node &&
           node->state_.backface_visibility == BackfaceVisibility::kHidden;
  }

  bool HasDirectCompositingReasons() const {
    return DirectCompositingReasons() != CompositingReason::kNone;
  }

  bool HasDirectCompositingReasonsOtherThan3dTransform() const {
    return DirectCompositingReasons() & ~CompositingReason::k3DTransform;
  }

  // TODO(crbug.com/900241): Use HaveActiveTransformAnimation() instead of this
  // function when we can track animations for each property type.
  bool RequiresCompositingForAnimation() const {
    return DirectCompositingReasons() &
           CompositingReason::kComboActiveAnimation;
  }
  bool HasActiveTransformAnimation() const {
    return state_.direct_compositing_reasons &
           CompositingReason::kActiveTransformAnimation;
  }
  bool TransformAnimationIsAxisAligned() const {
    return state_.flags.animation_is_axis_aligned;
  }

  bool RequiresCompositingForRootScroller() const {
    return state_.direct_compositing_reasons & CompositingReason::kRootScroller;
  }

  const CompositorElementId& GetCompositorElementId() const {
    return state_.compositor_element_id;
  }

  // Content whose transform nodes have a common rendering context ID are 3D
  // sorted. If this is 0, content will not be 3D sorted.
  unsigned RenderingContextId() const { return state_.rendering_context_id; }
  bool HasRenderingContext() const { return state_.rendering_context_id; }

  std::unique_ptr<JSONObject> ToJSON() const;

  // Returns memory usage of the transform cache of this node plus ancestors.
  size_t CacheMemoryUsageInBytes() const;

 private:
  friend class PaintPropertyNode<TransformPaintPropertyNode>;

  TransformPaintPropertyNode(const TransformPaintPropertyNode* parent,
                             State&& state,
                             bool is_parent_alias)
      : PaintPropertyNode(parent, is_parent_alias), state_(std::move(state)) {
    Validate();
  }

  CompositingReasons DirectCompositingReasons() const {
    DCHECK(!Parent() || !IsParentAlias());
    return state_.direct_compositing_reasons;
  }

  void Validate() const {
#if DCHECK_IS_ON()
    if (IsParentAlias())
      DCHECK(IsIdentity());
    if (state_.scroll) {
      // If there is an associated scroll node, this can only be a 2d
      // translation for scroll offset.
      DCHECK(IsIdentityOr2DTranslation());
      // The scroll compositor element id should be stored on the scroll node.
      DCHECK(!state_.compositor_element_id);
    }
#endif
  }

  void AddChanged(PaintPropertyChangeType changed) {
    // TODO(crbug.com/814815): This is a workaround of the bug. When the bug is
    // fixed, change the following condition to
    //   DCHECK(!transform_cache_ || !transform_cache_->IsValid());
    DCHECK_NE(PaintPropertyChangeType::kUnchanged, changed);
    if (transform_cache_ && transform_cache_->IsValid()) {
      DLOG(WARNING) << "Transform tree changed without invalidating the cache.";
      GeometryMapperTransformCache::ClearCache();
      GeometryMapperClipCache::ClearCache();
    }
    PaintPropertyNode::AddChanged(changed);
  }

  // For access to GetTransformCache() and SetCachedTransform.
  friend class GeometryMapper;
  friend class GeometryMapperTest;
  friend class GeometryMapperTransformCache;
  friend class GeometryMapperTransformCacheTest;

  const GeometryMapperTransformCache& GetTransformCache() const {
    if (!transform_cache_)
      transform_cache_.reset(new GeometryMapperTransformCache);
    transform_cache_->UpdateIfNeeded(*this);
    return *transform_cache_;
  }
  void UpdateScreenTransform() const {
    DCHECK(transform_cache_);
    transform_cache_->UpdateScreenTransform(*this);
  }

  State state_;
  mutable std::unique_ptr<GeometryMapperTransformCache> transform_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRANSFORM_PAINT_PROPERTY_NODE_H_
