// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_SPARSE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_SPARSE_H_

#include <array>
#include <memory>
#include <utility>

#include "base/dcheck_is_on.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/sparse_vector.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// This implementation of the ObjectPaintProperties interface is backed by
// a SparseVector for storage purposes: child Nodes of this class take up
// zero memory until instantiated.
class CORE_EXPORT ObjectPaintPropertiesSparse : public ObjectPaintProperties {
  USING_FAST_MALLOC(ObjectPaintPropertiesSparse);

 public:
  ObjectPaintPropertiesSparse() = default;
  ObjectPaintPropertiesSparse(const ObjectPaintPropertiesSparse&) = delete;
  ObjectPaintPropertiesSparse(ObjectPaintPropertiesSparse&&) = default;
  ObjectPaintPropertiesSparse& operator=(const ObjectPaintPropertiesSparse&) =
      delete;
  ObjectPaintPropertiesSparse& operator=(ObjectPaintPropertiesSparse&&) =
      default;
  ~ObjectPaintPropertiesSparse() override {
#if DCHECK_IS_ON()
    DCHECK(!is_immutable_);
#endif
  }

// Node definition preprocessor macros.
#define ADD_NODE(type, list, function, field_id)                            \
 public:                                                                    \
  const type##PaintPropertyNode* function() const override {                \
    return GetNode<type##PaintPropertyNode>(list, field_id);                \
  }                                                                         \
                                                                            \
  PaintPropertyChangeType Update##function(                                 \
      const type##PaintPropertyNodeOrAlias& parent,                         \
      type##PaintPropertyNode::State&& state,                               \
      const type##PaintPropertyNode::AnimationState& animation_state =      \
          type##PaintPropertyNode::AnimationState()) override {             \
    return Update<type##PaintPropertyNode, type##PaintPropertyNodeOrAlias>( \
        list, field_id, parent, std::move(state), animation_state);         \
  }                                                                         \
                                                                            \
  bool Clear##function() override { return list.ClearField(field_id); }     \
  // (End of ADD_NODE definition)

#define ADD_ALIAS_NODE(type, list, function, field_id)                        \
 public:                                                                      \
  const type##PaintPropertyNodeOrAlias* function() const override {           \
    return GetNode<type##PaintPropertyNodeAlias>(list, field_id);             \
  }                                                                           \
                                                                              \
  PaintPropertyChangeType Update##function(                                   \
      const type##PaintPropertyNodeOrAlias& parent) override {                \
    return UpdateAlias<type##PaintPropertyNodeAlias>(list, field_id, parent); \
  }                                                                           \
                                                                              \
  bool Clear##function() override { return list.ClearField(field_id); }       \
  // (End of ADD_ALIAS_NODE definition)

#define ADD_TRANSFORM(function, field_id) \
  ADD_NODE(Transform, nodes_, function, field_id)
#define ADD_EFFECT(function, field_id) \
  ADD_NODE(Effect, nodes_, function, field_id)
#define ADD_CLIP(function, field_id) ADD_NODE(Clip, nodes_, function, field_id)

  // Identifier used for indexing into the sparse vector of nodes. NOTE: when
  // adding a new node to this list, make sure to do the following. Update
  // the kMax<NodeType> value to reflect the value you added, and renumber all
  // higher value enums. The HasNodeTypeInRange() method assumes that  all nodes
  // of NodeType are bounded between kMin<NodeType>() and kMax<NodeType>, and
  // there are no other types of nodes in that range.
  enum class NodeId : unsigned {
    // Transforms
    kPaintOffsetTranslation = 0,
    kStickyTranslation = 1,
    kAnchorScrollTranslation = 2,
    kTranslate = 3,
    kRotate = 4,
    kScale = 5,
    kOffset = 6,
    kTransform = 7,
    kPerspective = 8,
    kReplacedContentTransform = 9,
    kScrollTranslation = 10,
    kFirstTransform = kPaintOffsetTranslation,
    kLastTransform = kScrollTranslation,

    // NOTE: the Scroll node is NOT a transform.
    kScroll = 11,

    // Effects
    kEffect = 12,
    kFilter = 13,
    kMask = 14,
    kClipPathMask = 15,
    kVerticalScrollbarEffect = 16,
    kHorizontalScrollbarEffect = 17,
    kScrollCorner = 18,
    kFirstEffect = kEffect,
    kLastEffect = kScrollCorner,

    // Clips
    kPixelMovingFilterClipExpander = 19,
    kClipPathClip = 20,
    kMaskClip = 21,
    kCssClip = 22,
    kCssClipFixedPosition = 23,
    kOverflowControlsClip = 24,
    kBackgroundClip = 25,
    kInnerBorderRadiusClip = 26,
    kOverflowClip = 27,
    kFirstClip = kPixelMovingFilterClipExpander,
    kLastClip = kOverflowClip,

    // Aliases
    kTransformAlias = 28,
    kEffectAlias = 29,
    kClipAlias = 30,

    // Should be updated whenever a higher value NodeType is added.
    kNumFields = kClipAlias + 1
  };

  // Transform implementations.
  bool HasTransformNode() const override {
    return HasNodeTypeInRange(NodeId::kFirstTransform, NodeId::kLastTransform);
  }
  bool HasCSSTransformPropertyNode() const override {
    return Translate() || Rotate() || Scale() || Offset() || Transform();
  }
  std::array<const TransformPaintPropertyNode*, 5>
  AllCSSTransformPropertiesOutsideToInside() const override {
    return {Translate(), Rotate(), Scale(), Offset(), Transform()};
  }

  ADD_TRANSFORM(PaintOffsetTranslation, NodeId::kPaintOffsetTranslation)
  ADD_TRANSFORM(StickyTranslation, NodeId::kStickyTranslation)
  ADD_TRANSFORM(AnchorScrollTranslation, NodeId::kAnchorScrollTranslation)
  ADD_TRANSFORM(Translate, NodeId::kTranslate)
  ADD_TRANSFORM(Rotate, NodeId::kRotate)
  ADD_TRANSFORM(Scale, NodeId::kScale)
  ADD_TRANSFORM(Offset, NodeId::kOffset)
  ADD_TRANSFORM(Transform, NodeId::kTransform)
  ADD_TRANSFORM(Perspective, NodeId::kPerspective)
  ADD_TRANSFORM(ReplacedContentTransform, NodeId::kReplacedContentTransform)
  ADD_TRANSFORM(ScrollTranslation, NodeId::kScrollTranslation)
  using ScrollPaintPropertyNodeOrAlias = ScrollPaintPropertyNode;
  ADD_NODE(Scroll, nodes_, Scroll, NodeId::kScroll)
  ADD_ALIAS_NODE(Transform,
                 nodes_,
                 TransformIsolationNode,
                 NodeId::kTransformAlias)

  // Effect node implementations.
  bool HasEffectNode() const override {
    return HasNodeTypeInRange(NodeId::kFirstEffect, NodeId::kLastEffect);
  }

  ADD_EFFECT(Effect, NodeId::kEffect)
  ADD_EFFECT(Filter, NodeId::kFilter)
  ADD_EFFECT(Mask, NodeId::kMask)
  ADD_EFFECT(ClipPathMask, NodeId::kClipPathMask)
  ADD_EFFECT(VerticalScrollbarEffect, NodeId::kVerticalScrollbarEffect)
  ADD_EFFECT(HorizontalScrollbarEffect, NodeId::kHorizontalScrollbarEffect)
  ADD_EFFECT(ScrollCornerEffect, NodeId::kScrollCorner)
  ADD_ALIAS_NODE(Effect, nodes_, EffectIsolationNode, NodeId::kEffectAlias)

  // Clip node implementations.
  bool HasClipNode() const override {
    return HasNodeTypeInRange(NodeId::kFirstClip, NodeId::kLastClip);
  }
  ADD_CLIP(PixelMovingFilterClipExpander,
           NodeId::kPixelMovingFilterClipExpander)
  ADD_CLIP(ClipPathClip, NodeId::kClipPathClip)
  ADD_CLIP(MaskClip, NodeId::kMaskClip)
  ADD_CLIP(CssClip, NodeId::kCssClip)
  ADD_CLIP(CssClipFixedPosition, NodeId::kCssClipFixedPosition)
  ADD_CLIP(OverflowControlsClip, NodeId::kOverflowControlsClip)
  ADD_CLIP(BackgroundClip, NodeId::kBackgroundClip)
  ADD_CLIP(InnerBorderRadiusClip, NodeId::kInnerBorderRadiusClip)
  ADD_CLIP(OverflowClip, NodeId::kOverflowClip)
  ADD_ALIAS_NODE(Clip, nodes_, ClipIsolationNode, NodeId::kClipAlias)

#undef ADD_CLIP
#undef ADD_EFFECT
#undef ADD_TRANSFORM
#undef ADD_NODE
#undef ADD_ALIAS_NODE

  // Debug-only state change validation method implementations.
#if DCHECK_IS_ON()
  void SetImmutable() const override { is_immutable_ = true; }
  bool IsImmutable() const override { return is_immutable_; }
  void SetMutable() const override { is_immutable_ = false; }

  void Validate() override {
    DCHECK(!ScrollTranslation() || !ReplacedContentTransform())
        << "Replaced elements don't scroll so there should never be both a "
           "scroll translation and a replaced content transform.";
    DCHECK(!ClipPathClip() || !ClipPathMask())
        << "ClipPathClip and ClipPathshould be mutually exclusive.";
    DCHECK((!TransformIsolationNode() && !ClipIsolationNode() &&
            !EffectIsolationNode()) ||
           (TransformIsolationNode() && ClipIsolationNode() &&
            EffectIsolationNode()))
        << "Isolation nodes have to be created for all of transform, clip, and "
           "effect trees.";
  }
#endif

  // Direct update method implementations.
  PaintPropertyChangeType DirectlyUpdateTransformAndOrigin(
      TransformPaintPropertyNode::TransformAndOrigin&& transform_and_origin,
      const TransformPaintPropertyNode::AnimationState& animation_state)
      override {
    CHECK(nodes_.HasField(NodeId::kTransform));
    return GetNode<TransformPaintPropertyNode>(nodes_, NodeId::kTransform)
        ->DirectlyUpdateTransformAndOrigin(std::move(transform_and_origin),
                                           animation_state);
  }
  PaintPropertyChangeType DirectlyUpdateOpacity(
      float opacity,
      const EffectPaintPropertyNode::AnimationState& animation_state) override {
    const bool has_effect = nodes_.HasField(NodeId::kEffect);
    // TODO(yotha): Remove this check once we make sure crbug.com/1370268
    // is fixed.
    DCHECK(has_effect);
    if (!has_effect) {
      return PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
    return GetNode<EffectPaintPropertyNode>(nodes_, NodeId::kEffect)
        ->DirectlyUpdateOpacity(opacity, animation_state);
  }

 private:
  // We have to use a variant to keep track of which subtype of node is
  // instantiated, since the base PaintPropertyNode class is templated and
  // thus doesn't have a reasonable base class for us to use.
  using NodeVariant =
      std::variant<scoped_refptr<TransformPaintPropertyNode>,
                   scoped_refptr<EffectPaintPropertyNode>,
                   scoped_refptr<ClipPaintPropertyNode>,
                   scoped_refptr<TransformPaintPropertyNodeAlias>,
                   scoped_refptr<EffectPaintPropertyNodeAlias>,
                   scoped_refptr<ClipPaintPropertyNodeAlias>,
                   scoped_refptr<ScrollPaintPropertyNode>>;
  using NodeList = SparseVector<NodeId, NodeVariant>;

  template <typename NodeType, typename ParentType>
  PaintPropertyChangeType Update(
      NodeList& nodes,
      NodeId node_id,
      const ParentType& parent,
      NodeType::State&& state,
      const NodeType::AnimationState& animation_state =
          NodeType::AnimationState()) {
    // First, check if we need to add a new node.
    if (!nodes.HasField(node_id)) {
      nodes.SetField(node_id, NodeType::Create(parent, std::move(state)));
#if DCHECK_IS_ON()
      DCHECK(!is_immutable_) << "Sparse node added while immutable.";
#endif
      return PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
    // If not, we just need to update the existing node.
    auto* node = GetNode<NodeType>(nodes, node_id);
    const PaintPropertyChangeType changed =
        node->Update(parent, std::move(state), animation_state);
#if DCHECK_IS_ON()
    DCHECK(!is_immutable_ || changed == PaintPropertyChangeType::kUnchanged)
        << "Value changed while immutable.";
#endif
    return changed;
  }

  template <typename AliasType, typename ParentType>
  PaintPropertyChangeType UpdateAlias(NodeList& nodes,
                                      NodeId node_id,
                                      const ParentType& parent) {
    // First, check if we need to add a new alias.
    if (!nodes.HasField(node_id)) {
      nodes.SetField(node_id, AliasType::Create(parent));
#if DCHECK_IS_ON()
      DCHECK(!is_immutable_) << "Sparse node added while immutable.";
#endif
      return PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
    // If not, we just need to update the existing alias.
    auto* node = GetNode<AliasType>(nodes, node_id);
    DCHECK(node->IsParentAlias());
    const PaintPropertyChangeType changed = node->SetParent(parent);
#if DCHECK_IS_ON()
    DCHECK(!is_immutable_ || changed == PaintPropertyChangeType::kUnchanged)
        << "Parent changed while immutable. New state:\n"
        << *node;
#endif
    return changed;
  }

  template <typename NodeType>
  const NodeType* GetNode(const NodeList& nodes, NodeId node_id) const {
    if (nodes.HasField(node_id)) {
      const NodeVariant& field = nodes.GetField(node_id);
      CHECK(std::holds_alternative<scoped_refptr<NodeType>>(field));
      return std::get<scoped_refptr<NodeType>>(field).get();
    }
    return nullptr;
  }

  template <typename NodeType>
  NodeType* GetNode(NodeList& nodes, NodeId node_id) {
    return const_cast<NodeType*>(
        static_cast<const ObjectPaintPropertiesSparse&>(*this)
            .GetNode<NodeType>(nodes, node_id));
  }

  bool HasNodeTypeInRange(NodeId first_id, NodeId last_id) const {
    for (int i = static_cast<int>(first_id); i < static_cast<int>(last_id);
         ++i) {
      if (nodes_.HasField(static_cast<NodeId>(i))) {
        return true;
      }
    }
    return false;
  }

  NodeList nodes_;

#if DCHECK_IS_ON()
  mutable bool is_immutable_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_SPARSE_H_
