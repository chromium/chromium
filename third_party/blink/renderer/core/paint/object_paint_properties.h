// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// This class stores the paint property nodes created by a LayoutObject. The
// object owns each of the property nodes directly and RefPtrs are only used to
// harden against use-after-free bugs. These paint properties are built/updated
// by PaintPropertyTreeBuilder during the PrePaint lifecycle step.
//
// [update & clear implementation note] This class has Update[property](...) and
// Clear[property]() helper functions for efficiently creating and updating
// properties. The update functions returns a 3-state result to indicate whether
// the value or the existence of the node has changed. They use a create-or-
// update pattern of re-using existing properties for efficiency:
// 1. It avoids extra allocations.
// 2. It preserves existing child->parent pointers.
// The clear functions return true if an existing node is removed. Property
// nodes store parent pointers but not child pointers and these return values
// are important for catching property tree structure changes which require
// updating descendant's parent pointers.
class CORE_EXPORT ObjectPaintProperties {
  USING_FAST_MALLOC(ObjectPaintProperties);

 public:
  ObjectPaintProperties() = default;
#if DCHECK_IS_ON()
  ~ObjectPaintProperties() { DCHECK(!is_immutable_); }
#endif

// The following defines 3 functions and one variable:
// - Foo(): a getter for the property.
// - UpdateFoo(): an update function.
// - ClearFoo(): a clear function
// - foo_: the variable itself.
//
// Note that clear* functions return true if the property tree structure
// changes (an existing node was deleted), and false otherwise. See the
// class-level comment ("update & clear implementation note") for details
// about why this is needed for efficient updates.
#define ADD_NODE(type, function, variable)                                   \
 public:                                                                     \
  const type##PaintPropertyNode* function() const { return variable.get(); } \
  PaintPropertyChangeType Update##function(                                  \
      const type##PaintPropertyNode& parent,                                 \
      type##PaintPropertyNode::State&& state,                                \
      const type##PaintPropertyNode::AnimationState& animation_state =       \
          type##PaintPropertyNode::AnimationState()) {                       \
    return Update(variable, parent, std::move(state), animation_state);      \
  }                                                                          \
  bool Clear##function() { return Clear(variable); }                         \
                                                                             \
 private:                                                                    \
  scoped_refptr<type##PaintPropertyNode> variable
  // (End of ADD_NODE definition)

#define ADD_ALIAS_NODE(type, function, variable)                             \
 public:                                                                     \
  const type##PaintPropertyNode* function() const { return variable.get(); } \
  PaintPropertyChangeType Update##function(                                  \
      const type##PaintPropertyNode& parent) {                               \
    return UpdateAlias(variable, parent);                                    \
  }                                                                          \
  bool Clear##function() { return Clear(variable); }                         \
                                                                             \
 private:                                                                    \
  scoped_refptr<type##PaintPropertyNode> variable
  // (End of ADD_ALIAS_NODE definition)

#define ADD_TRANSFORM(function, variable) \
  ADD_NODE(Transform, function, variable)
#define ADD_EFFECT(function, variable) ADD_NODE(Effect, function, variable)
#define ADD_CLIP(function, variable) ADD_NODE(Clip, function, variable)

  // The hierarchy of the transform subtree created by a LayoutObject is as
  // follows:
  // [ PaintOffsetTranslation ]
  // |   Normally paint offset is accumulated without creating a node until
  // |   we see, for example, transform or position:fixed.
  // +-[ StickyTranslation ]
  //  /    This applies the sticky offset induced by position:sticky.
  // |
  // +-[ Transform ]
  //   |   The space created by CSS transform. This is the local border box
  //   |   space.
  //   +-[ Perspective ]
  //     |   The space created by CSS perspective.
  //     +-[ ReplacedContentTransform ]
  //         Additional transform for replaced elements to implement object-fit.
  //         (Replaced elements don't scroll.)
  //     OR
  //     +-[ ScrollTranslation ]
  //         The space created by overflow clip. The translation equals the
  //         offset between the scrolling contents and the scrollable area of
  //         the container, both originated from the top-left corner, so it is
  //         the sum of scroll origin and scroll offset of the ScrollableArea.
  //         To use any content offset based on ScrollOrigin() (e.g. LayoutBox
  //         or InlineBox's PhysicalLocation()) in this space, we should add
  //         ScrollOrigin() to the offset.
  //
  // ... +-[ TransformIsolationNode ]
  //         This serves as a parent to subtree transforms on an element with
  //         paint containment. It induces a PaintOffsetTranslation node and
  //         is the deepest child of any transform tree on the contain: paint
  //         element.
  ADD_TRANSFORM(PaintOffsetTranslation, paint_offset_translation_);
  ADD_TRANSFORM(StickyTranslation, sticky_translation_);
  ADD_TRANSFORM(Transform, transform_);
  ADD_TRANSFORM(Perspective, perspective_);
  ADD_TRANSFORM(ReplacedContentTransform, replaced_content_transform_);
  ADD_TRANSFORM(ScrollTranslation, scroll_translation_);
  ADD_NODE(Scroll, Scroll, scroll_);
  ADD_ALIAS_NODE(Transform, TransformIsolationNode, transform_isolation_node_);

  // The hierarchy of the effect subtree created by a LayoutObject is as
  // follows:
  // [ effect ]
  // |     Isolated group to apply various CSS effects, including opacity,
  // |     mix-blend-mode, backdrop-filter, and for isolation if a mask needs
  // |     to be applied or backdrop-dependent children are present.
  // +-[ filter ]
  // |     Isolated group for CSS filter.
  // +-[ vertical/horizontal scrollbar effect ]
  // |     Overlay Scrollbars on Aura and Android need effect node for fade
  // |     animation.
  // +-[ mask ]
  // |     Isolated group for painting the CSS mask. This node will have
  // |     SkBlendMode::kDstIn and shall paint last, i.e. after masked contents.
  // +-[ clip path ]
  //       Isolated group for painting the CSS clip-path. This node will have
  //       SkBlendMode::kDstIn and shall paint last, i.e. after clipped
  //       contents.
  //
  // ... +-[ effectIsolationNode ]
  //       This serves as a parent to subtree effects on an element with paint
  //       containment, It is the deepest child of any effect tree on the
  //       contain: paint element.
  ADD_EFFECT(Effect, effect_);
  ADD_EFFECT(Filter, filter_);
  ADD_EFFECT(VerticalScrollbarEffect, vertical_scrollbar_effect_);
  ADD_EFFECT(HorizontalScrollbarEffect, horizontal_scrollbar_effect_);
  ADD_EFFECT(Mask, mask_);
  ADD_EFFECT(ClipPath, clip_path_);
  ADD_ALIAS_NODE(Effect, EffectIsolationNode, effect_isolation_node_);

  // The hierarchy of the clip subtree created by a LayoutObject is as follows:
  // [ fragment clip ]
  // |    Clips to a fragment's bounds.
  // |    This is only present for content under a fragmentation container.
  // +-[ clip path clip ]
  //   |  Clip created by path-based CSS clip-path. Only exists if the
  //  /   clip-path is "simple" that can be applied geometrically. This and
  // /    the clip path effect node are mutually exclusive.
  // | NOTE: for composited SPv1 clip path clips, we move clip path clip
  // |       below mask.
  // +-[ mask clip ]
  //   |   Clip created by CSS mask or CSS clip-path. It serves two purposes:
  //   |   1. Cull painting of the masked subtree. Because anything outside of
  //   |      the mask is never visible, it is pointless to paint them.
  //   |   2. Raster clip of the masked subtree. Because the mask implemented
  //   |      as SkBlendMode::kDstIn, pixels outside of mask's bound will be
  //   |      intact when they shall be masked out. This clip ensures no pixels
  //   |      leak out.
  //   +-[ css clip ]
  //     |   Clip created by CSS clip. CSS clip applies to all descendants, this
  //     |   node only applies to containing block descendants. For descendants
  //     |   not contained by this object, use [ css clip fixed position ].
  //     +-[ overflow controls clip ]
  //     |   Clip created by overflow clip to clip overflow controls
  //     |   (scrollbars, resizer, scroll corner) that would overflow the box.
  //     +-[ inner border radius clip]
  //       |   Clip created by a rounded border with overflow clip. This clip is
  //       |   not inset by scrollbars.
  //       +-[ overflow clip ]
  //             Clip created by overflow clip and is inset by the scrollbar.
  //   [ css clip fixed position ]
  //       Clip created by CSS clip. Only exists if the current clip includes
  //       some clip that doesn't apply to our fixed position descendants.
  //
  //  ... +-[ clipIsolationNode ]
  //       This serves as a parent to subtree clips on an element with paint
  //       containment. It is the deepest child of any clip tree on the contain:
  //       paint element.
  ADD_CLIP(FragmentClip, fragment_clip_);
  ADD_CLIP(ClipPathClip, clip_path_clip_);
  ADD_CLIP(MaskClip, mask_clip_);
  ADD_CLIP(CssClip, css_clip_);
  ADD_CLIP(CssClipFixedPosition, css_clip_fixed_position_);
  ADD_CLIP(OverflowControlsClip, overflow_controls_clip_);
  ADD_CLIP(InnerBorderRadiusClip, inner_border_radius_clip_);
  ADD_CLIP(OverflowClip, overflow_clip_);
  ADD_ALIAS_NODE(Clip, ClipIsolationNode, clip_isolation_node_);

#undef ADD_CLIP
#undef ADD_EFFECT
#undef ADD_TRANSFORM
#undef ADD_NODE
#undef ADD_ALIAS_NODE

 public:
#if DCHECK_IS_ON()
  // Used by FindPropertiesNeedingUpdate.h for verifying state doesn't change.
  void SetImmutable() const { is_immutable_ = true; }
  bool IsImmutable() const { return is_immutable_; }
  void SetMutable() const { is_immutable_ = false; }

  void Validate() {
    DCHECK(!ScrollTranslation() || !ReplacedContentTransform())
        << "Replaced elements don't scroll so there should never be both a "
           "scroll translation and a replaced content transform.";
    DCHECK(!ClipPathClip() || !ClipPath())
        << "ClipPathClip and ClipPathshould be mutually exclusive.";
    DCHECK((!TransformIsolationNode() && !ClipIsolationNode() &&
            !EffectIsolationNode()) ||
           (TransformIsolationNode() && ClipIsolationNode() &&
            EffectIsolationNode()))
        << "Isolation nodes have to be created for all of transform, clip, and "
           "effect trees.";
  }
#endif

 private:
  // Return true if the property tree structure changes (an existing node was
  // deleted), and false otherwise. See the class-level comment ("update & clear
  // implementation note") for details about why this is needed for efficiency.
  template <typename PaintPropertyNode>
  bool Clear(scoped_refptr<PaintPropertyNode>& field) {
    if (field) {
      field = nullptr;
      return true;
    }
    return false;
  }

  // Return true if the property tree structure changes (a new node was
  // created), and false otherwise. See the class-level comment ("update & clear
  // implementation note") for details about why this is needed for efficiency.
  template <typename PaintPropertyNode>
  PaintPropertyChangeType Update(
      scoped_refptr<PaintPropertyNode>& field,
      const PaintPropertyNode& parent,
      typename PaintPropertyNode::State&& state,
      const typename PaintPropertyNode::AnimationState& animation_state) {
    if (field) {
      auto changed = field->Update(parent, std::move(state), animation_state);
#if DCHECK_IS_ON()
      DCHECK(!is_immutable_ || changed == PaintPropertyChangeType::kUnchanged)
          << "Value changed while immutable. New state:\n"
          << *field;
#endif
      return changed;
    }
    field = PaintPropertyNode::Create(parent, std::move(state));
#if DCHECK_IS_ON()
    DCHECK(!is_immutable_) << "Node added while immutable. New state:\n"
                           << *field;
#endif
    return PaintPropertyChangeType::kNodeAddedOrRemoved;
  }

  template <typename PaintPropertyNode>
  PaintPropertyChangeType UpdateAlias(scoped_refptr<PaintPropertyNode>& field,
                                      const PaintPropertyNode& parent) {
    if (field) {
      DCHECK(field->IsParentAlias());
      auto changed = field->SetParent(&parent);
#if DCHECK_IS_ON()
      DCHECK(!is_immutable_ || changed == PaintPropertyChangeType::kUnchanged)
          << "Parent changed while immutable. New state:\n"
          << *field;
#endif
      return changed;
    }
    field = PaintPropertyNode::CreateAlias(parent);
#if DCHECK_IS_ON()
    DCHECK(!is_immutable_) << "Node added while immutable. New state:\n"
                           << *field;
#endif
    return PaintPropertyChangeType::kNodeAddedOrRemoved;
  }

#if DCHECK_IS_ON()
  mutable bool is_immutable_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(ObjectPaintProperties);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_H_
