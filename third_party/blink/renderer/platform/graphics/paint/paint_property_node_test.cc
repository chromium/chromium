// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

namespace blink {

class PaintPropertyNodeTest : public testing::Test {
 protected:
  template <typename NodeType>
  struct Tree {
    Persistent<const NodeType> root;
    Persistent<NodeType> ancestor;
    Persistent<NodeType> child1;
    Persistent<NodeType> child2;
    Persistent<NodeType> grandchild1;
    Persistent<NodeType> grandchild2;
  };

  void SetUp() override {
    //          root
    //           |
    //        ancestor
    //         /   \
    //     child1   child2
    //       |        |
    // grandchild1 grandchild2

    transform.root = &TransformPaintPropertyNode::Root();
    transform.ancestor = Create2DTranslation(*transform.root, 0, 0);
    transform.child1 = Create2DTranslation(*transform.ancestor, 0, 0);
    transform.child2 = Create2DTranslation(*transform.ancestor, 0, 0);
    transform.grandchild1 = Create2DTranslation(*transform.child1, 0, 0);
    transform.grandchild2 = Create2DTranslation(*transform.child2, 0, 0);

    clip.root = &ClipPaintPropertyNode::Root();
    clip.ancestor =
        CreateClip(*clip.root, *transform.ancestor, FloatRoundedRect());
    clip.child1 =
        CreateClip(*clip.ancestor, *transform.child1, FloatRoundedRect());
    clip.child2 =
        CreateClip(*clip.ancestor, *transform.child2, FloatRoundedRect());
    clip.grandchild1 =
        CreateClip(*clip.child1, *transform.grandchild1, FloatRoundedRect());
    clip.grandchild2 =
        CreateClip(*clip.child2, *transform.grandchild2, FloatRoundedRect());

    effect.root = &EffectPaintPropertyNode::Root();
    effect.ancestor = CreateOpacityEffect(*effect.root, *transform.ancestor,
                                          clip.ancestor.Get(), 0.5);
    effect.child1 = CreateOpacityEffect(*effect.ancestor, *transform.child1,
                                        clip.child1.Get(), 0.5);
    effect.child2 = CreateOpacityEffect(*effect.ancestor, *transform.child2,
                                        clip.child2.Get(), 0.5);
    effect.grandchild1 = CreateOpacityEffect(
        *effect.child1, *transform.grandchild1, clip.grandchild1.Get(), 0.5);
    effect.grandchild2 = CreateOpacityEffect(
        *effect.child2, *transform.grandchild2, clip.grandchild2.Get(), 0.5);
  }

  template <typename NodeType>
  void ResetAllChanged(Tree<NodeType>& tree) {
    tree.grandchild1->ClearChangedToRoot(sequence_number);
    tree.grandchild2->ClearChangedToRoot(sequence_number);
  }

  void ResetAllChanged() {
    sequence_number++;
    ResetAllChanged(transform);
    ResetAllChanged(clip);
    ResetAllChanged(effect);
  }

  template <typename NodeType>
  void ExpectInitialState(const Tree<NodeType>& tree) {
    EXPECT_EQ(PaintPropertyChangeType::kUnchanged, tree.root->NodeChanged());
    EXPECT_EQ(PaintPropertyChangeType::kNodeAddedOrRemoved,
              tree.ancestor->NodeChanged());
    EXPECT_EQ(PaintPropertyChangeType::kNodeAddedOrRemoved,
              tree.child1->NodeChanged());
    EXPECT_EQ(PaintPropertyChangeType::kNodeAddedOrRemoved,
              tree.child2->NodeChanged());
    EXPECT_EQ(PaintPropertyChangeType::kNodeAddedOrRemoved,
              tree.grandchild1->NodeChanged());
    EXPECT_EQ(PaintPropertyChangeType::kNodeAddedOrRemoved,
              tree.grandchild2->NodeChanged());
  }

  template <typename NodeType>
  void ExpectUnchangedState(const Tree<NodeType>& tree) {
    EXPECT_EQ(PaintPropertyChangeType::kUnchanged, tree.root->NodeChanged());
    EXPECT_EQ(PaintPropertyChangeType::kUnchanged,
              tree.ancestor->NodeChanged());
    EXPECT_EQ(PaintPropertyChangeType::kUnchanged, tree.child1->NodeChanged());
    EXPECT_EQ(PaintPropertyChangeType::kUnchanged, tree.child2->NodeChanged());
    EXPECT_EQ(PaintPropertyChangeType::kUnchanged,
              tree.grandchild1->NodeChanged());
    EXPECT_EQ(PaintPropertyChangeType::kUnchanged,
              tree.grandchild2->NodeChanged());
  }

  void ExpectUnchangedState() {
    ExpectUnchangedState(transform);
    ExpectUnchangedState(clip);
    ExpectUnchangedState(effect);
  }

  template <typename NodeType>
  PaintPropertyChangeType NodeChanged(const NodeType& node) {
    return node.NodeChanged();
  }

  Tree<TransformPaintPropertyNode> transform;
  Tree<ClipPaintPropertyNode> clip;
  Tree<EffectPaintPropertyNode> effect;
  int sequence_number = 1;
};

#define STATE(node) PropertyTreeState(*transform.node, *clip.node, *effect.node)
#define EXPECT_CHANGE_EQ(expected_value, node, ...)                           \
  do {                                                                        \
    if (expected_value != PaintPropertyChangeType::kUnchanged) {              \
      for (int change_type = 0;                                               \
           change_type <= static_cast<int>(expected_value); ++change_type) {  \
        SCOPED_TRACE(testing::Message() << "change_type=" << change_type);    \
        EXPECT_TRUE(                                                          \
            node->Changed(static_cast<PaintPropertyChangeType>(change_type),  \
                          ##__VA_ARGS__));                                    \
      }                                                                       \
    }                                                                         \
    for (int change_type = static_cast<int>(expected_value) + 1;              \
         change_type <=                                                       \
         static_cast<int>(PaintPropertyChangeType::kNodeAddedOrRemoved);      \
         ++change_type) {                                                     \
      SCOPED_TRACE(testing::Message() << "change_type=" << change_type);      \
      EXPECT_FALSE(node->Changed(                                             \
          static_cast<PaintPropertyChangeType>(change_type), ##__VA_ARGS__)); \
    }                                                                         \
  } while (false)

TEST_F(PaintPropertyNodeTest, LowestCommonAncestor) {
  EXPECT_EQ(transform.ancestor,
            &transform.ancestor->LowestCommonAncestor(*transform.ancestor));
  EXPECT_EQ(transform.root,
            &transform.root->LowestCommonAncestor(*transform.root));

  EXPECT_EQ(transform.ancestor, &transform.grandchild1->LowestCommonAncestor(
                                    *transform.grandchild2));
  EXPECT_EQ(transform.ancestor,
            &transform.grandchild1->LowestCommonAncestor(*transform.child2));
  EXPECT_EQ(transform.root,
            &transform.grandchild1->LowestCommonAncestor(*transform.root));
  EXPECT_EQ(transform.child1,
            &transform.grandchild1->LowestCommonAncestor(*transform.child1));

  EXPECT_EQ(transform.ancestor, &transform.grandchild2->LowestCommonAncestor(
                                    *transform.grandchild1));
  EXPECT_EQ(transform.ancestor,
            &transform.grandchild2->LowestCommonAncestor(*transform.child1));
  EXPECT_EQ(transform.root,
            &transform.grandchild2->LowestCommonAncestor(*transform.root));
  EXPECT_EQ(transform.child2,
            &transform.grandchild2->LowestCommonAncestor(*transform.child2));

  EXPECT_EQ(transform.ancestor,
            &transform.child1->LowestCommonAncestor(*transform.child2));
  EXPECT_EQ(transform.ancestor,
            &transform.child2->LowestCommonAncestor(*transform.child1));
}

TEST_F(PaintPropertyNodeTest, InitialStateAndReset) {
  ExpectInitialState(transform);
  ResetAllChanged(transform);
  ExpectUnchangedState(transform);
}

TEST_F(PaintPropertyNodeTest, TransformChangeAncestor) {
  ResetAllChanged();
  ExpectUnchangedState();
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
            transform.ancestor->Update(*transform.root,
                                       TransformPaintPropertyNode::State{
                                           {MakeTranslationMatrix(1, 2)}}));

  // Test descendant->Changed(ancestor).
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.ancestor, *transform.root);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, transform.ancestor,
                   *transform.ancestor);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.child1, *transform.root);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, transform.child1,
                   *transform.ancestor);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.grandchild1, *transform.root);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, transform.grandchild1,
                   *transform.ancestor);

  // Test property->Changed(non-ancestor-property). Should combine the changed
  // flags of the two paths to the root.
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.grandchild1, *transform.child2);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.grandchild1, *transform.grandchild2);

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, ClipChangeAncestor) {
  ResetAllChanged();
  ExpectUnchangedState();
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues,
            clip.ancestor->Update(
                *clip.root, ClipPaintPropertyNode::State(
                                *transform.ancestor, gfx::RectF(1, 2, 3, 4),
                                FloatRoundedRect(1, 2, 3, 4))));

  // Test descendant->Changed(ancestor).
  EXPECT_TRUE(clip.ancestor->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(root), nullptr));
  EXPECT_FALSE(clip.ancestor->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(ancestor), nullptr));
  EXPECT_TRUE(clip.child1->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                   STATE(root), nullptr));
  EXPECT_FALSE(clip.child1->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                    STATE(ancestor), nullptr));
  EXPECT_TRUE(clip.grandchild1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(root), nullptr));
  EXPECT_FALSE(clip.grandchild1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(ancestor), nullptr));

  // Test property->Changed(non-ancestor-property).
  // Simply walk to the root.
  EXPECT_TRUE(clip.grandchild1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(child2), nullptr));
  EXPECT_TRUE(
      clip.grandchild1->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                STATE(grandchild2), nullptr));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, EffectChangeAncestor) {
  ResetAllChanged();
  ExpectUnchangedState();
  EffectPaintPropertyNode::State state{transform.ancestor, clip.ancestor};
  state.compositor_element_id = effect.ancestor->GetCompositorElementId();

  // The initial test starts with opacity 0.5, and we're changing it to 0.9
  // here.
  state.opacity = 0.9;
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
            effect.ancestor->Update(*effect.root, std::move(state)));

  // Test descendant->Changed(ancestor).
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   effect.ancestor, STATE(root), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.ancestor,
                   STATE(ancestor), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   effect.child1, STATE(root), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.child1,
                   STATE(ancestor), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   effect.grandchild1, STATE(root), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.grandchild1,
                   STATE(ancestor), nullptr);
  // Test property->Changed(non-ancestor-property).
  // Simply walk to the root.
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   effect.grandchild1, STATE(child2), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   effect.grandchild1, STATE(grandchild2), nullptr);

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, ChangeOpacityDuringCompositedAnimation) {
  ResetAllChanged();
  ExpectUnchangedState();

  EffectPaintPropertyNode::State state{transform.child1, clip.child1};
  state.compositor_element_id = effect.child1->GetCompositorElementId();
  // The initial test starts with opacity 0.5, and we're changing it to 0.9
  // here.
  state.opacity = 0.9;

  EffectPaintPropertyNode::AnimationState animation_state;
  animation_state.is_running_opacity_animation_on_compositor = true;

  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyCompositedValues,
            effect.child1->Update(*effect.ancestor, std::move(state),
                                  animation_state));

  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlyCompositedValues,
                   effect.child1, STATE(root), nullptr);

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, EffectOpacityChangesToOneAndFromOne) {
  ResetAllChanged();
  ExpectUnchangedState();

  {
    EffectPaintPropertyNode::State state{transform.ancestor, clip.ancestor};
    // The initial test starts with opacity 0.5, and we're changing it to 1
    // here.
    state.opacity = 1.f;
    EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues,
              effect.ancestor->Update(*effect.root, std::move(state)));
  }

  // Test descendant->Changed(ancestor).
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlyValues, effect.ancestor,
                   STATE(root), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.ancestor,
                   STATE(ancestor), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlyValues, effect.child1,
                   STATE(root), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.child1,
                   STATE(ancestor), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlyValues,
                   effect.grandchild1, STATE(root), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.grandchild1,
                   STATE(ancestor), nullptr);

  // Test property->Changed(non-ancestor-property).
  // Simply walk to the root.
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlyValues,
                   effect.grandchild1, STATE(child2), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlyValues,
                   effect.grandchild1, STATE(grandchild2), nullptr);

  {
    EffectPaintPropertyNode::State state{transform.ancestor.Get(),
                                         clip.ancestor.Get()};
    state.opacity = 0.7f;
    EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues,
              effect.ancestor->Update(*effect.root, std::move(state)));
  }

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, EffectWillChangeOpacityChangesToAndFromOne) {
  // TODO(crbug.com/1285498): Optimize for will-change: opacity.
  {
    EffectPaintPropertyNode::State state{transform.ancestor, clip.ancestor};
    state.opacity = 0.5f;  // Same as the initial opacity of |effect.ancestor|.
    state.direct_compositing_reasons = CompositingReason::kWillChangeOpacity;
    EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyNonRerasterValues,
              effect.ancestor->Update(*effect.root, std::move(state)));
  }
  {
    EffectPaintPropertyNode::State state{transform.ancestor,
                                         clip.ancestor.Get()};
    // Change only opacity to 1.
    state.opacity = 1.f;
    state.direct_compositing_reasons = CompositingReason::kWillChangeOpacity;
    EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues,
              effect.ancestor->Update(*effect.root, std::move(state)));
  }
  {
    EffectPaintPropertyNode::State state{transform.ancestor.Get(),
                                         clip.ancestor.Get()};
    state.direct_compositing_reasons = CompositingReason::kWillChangeOpacity;
    // Change only opacity to 0.7f.
    state.opacity = 0.7f;
    EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues,
              effect.ancestor->Update(*effect.root, std::move(state)));
  }
}

TEST_F(PaintPropertyNodeTest, EffectAnimatingOpacityChangesToAndFromOne) {
  {
    EffectPaintPropertyNode::State state{transform.ancestor.Get(),
                                         clip.ancestor.Get()};
    state.opacity = 0.5f;  // Same as the initial opacity of |effect.ancestor|.
    state.direct_compositing_reasons |=
        CompositingReason::kActiveOpacityAnimation;
    EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyNonRerasterValues,
              effect.ancestor->Update(*effect.root, std::move(state)));
  }
  {
    EffectPaintPropertyNode::State state1{transform.ancestor.Get(),
                                          clip.ancestor.Get()};
    state1.opacity = 1.f;
    state1.direct_compositing_reasons |=
        CompositingReason::kActiveOpacityAnimation;
    EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
              effect.ancestor->Update(*effect.root, std::move(state1)));
  }
  {
    EffectPaintPropertyNode::State state2{transform.ancestor.Get(),
                                          clip.ancestor.Get()};
    state2.opacity = 0.7f;
    state2.direct_compositing_reasons |=
        CompositingReason::kActiveOpacityAnimation;
    EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
              effect.ancestor->Update(*effect.root, std::move(state2)));
  }
}

TEST_F(PaintPropertyNodeTest, ChangeDirectCompositingReason) {
  ResetAllChanged();
  ExpectUnchangedState();
  {
    TransformPaintPropertyNode::State state;
    state.direct_compositing_reasons = CompositingReason::kWillChangeTransform;
    EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues,
              transform.child1->Update(*transform.ancestor, std::move(state)));
    EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlyValues,
                     transform.child1, *transform.root);
  }

  {
    TransformPaintPropertyNode::State state;
    state.direct_compositing_reasons =
        CompositingReason::kWillChangeTransform |
        CompositingReason::kBackfaceVisibilityHidden;
    EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyNonRerasterValues,
              transform.child1->Update(*transform.ancestor, std::move(state)));
    // The previous change is more significant.
    EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlyValues,
                     transform.child1, *transform.root);
  }

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, ChangeTransformDuringCompositedAnimation) {
  ResetAllChanged();
  ExpectUnchangedState();
  TransformPaintPropertyNode::AnimationState animation_state;
  animation_state.is_running_animation_on_compositor = true;
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyCompositedValues,
            transform.child1->Update(
                *transform.ancestor,
                TransformPaintPropertyNode::State{{MakeScaleMatrix(2)}},
                animation_state));

  EXPECT_FALSE(transform.child1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, *transform.root));
  EXPECT_FALSE(transform.child1->Changed(
      PaintPropertyChangeType::kChangedOnlyNonRerasterValues, *transform.root));
  EXPECT_TRUE(transform.child1->Changed(
      PaintPropertyChangeType::kChangedOnlyCompositedValues, *transform.root));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, ChangeTransformOriginDuringCompositedAnimation) {
  ResetAllChanged();
  ExpectUnchangedState();
  TransformPaintPropertyNode::AnimationState animation_state;
  animation_state.is_running_animation_on_compositor = true;
  EXPECT_EQ(
      PaintPropertyChangeType::kChangedOnlySimpleValues,
      transform.child1->Update(*transform.ancestor,
                               TransformPaintPropertyNode::State{
                                   {gfx::Transform(), gfx::Point3F(1, 2, 3)}},
                               animation_state));

  EXPECT_TRUE(transform.child1->Changed(
      PaintPropertyChangeType::kChangedOnlySimpleValues, *transform.root));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest,
       ChangeTransform2dAxisAlignmentAndOriginDuringCompositedAnimation) {
  ResetAllChanged();
  ExpectUnchangedState();
  TransformPaintPropertyNode::AnimationState animation_state;
  animation_state.is_running_animation_on_compositor = true;
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
            transform.child1->Update(
                *transform.ancestor,
                TransformPaintPropertyNode::State{
                    {MakeRotationMatrix(2), gfx::Point3F(1, 2, 3)}},
                animation_state));

  EXPECT_TRUE(transform.child1->Changed(
      PaintPropertyChangeType::kChangedOnlySimpleValues, *transform.root));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, StickyTranslationChange) {
  ResetAllChanged();
  ExpectUnchangedState();
  TransformPaintPropertyNode::State state;
  state.direct_compositing_reasons = CompositingReason::kStickyPosition;
  // The change affects RequiresCullRectExpansion().
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues,
            transform.child1->Update(*transform.ancestor, std::move(state)));

  // Change sticky translation.
  TransformPaintPropertyNode::State state1{{MakeTranslationMatrix(10, 20)}};
  state1.direct_compositing_reasons = CompositingReason::kStickyPosition;
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyCompositedValues,
            transform.child1->Update(*transform.ancestor, std::move(state1)));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, TransformChangeOneChild) {
  ResetAllChanged();
  ExpectUnchangedState();
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
            transform.child1->Update(*transform.ancestor,
                                     TransformPaintPropertyNode::State{
                                         {MakeTranslationMatrix(1, 2)}}));

  // Test descendant->Changed(ancestor).
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, transform.ancestor,
                   *transform.root);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, transform.ancestor,
                   *transform.ancestor);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.child1, *transform.root);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.child1, *transform.ancestor);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.grandchild1, *transform.ancestor);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, transform.grandchild1,
                   *transform.child1);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, transform.child2,
                   *transform.ancestor);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, transform.grandchild2,
                   *transform.ancestor);

  // Test property->Changed(non-ancestor-property). Need to combine the changed
  // flags of the two paths to the root.
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.child2, *transform.child1);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.child1, *transform.child2);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.child2, *transform.grandchild1);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.child1, *transform.grandchild2);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.grandchild1, *transform.child2);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.grandchild1, *transform.grandchild2);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.grandchild2, *transform.child1);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   transform.grandchild2, *transform.grandchild1);

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, ClipChangeOneChild) {
  ResetAllChanged();
  ExpectUnchangedState();
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues,
            clip.child1->Update(*clip.root,
                                ClipPaintPropertyNode::State(
                                    *transform.ancestor, gfx::RectF(1, 2, 3, 4),
                                    FloatRoundedRect(1, 2, 3, 4))));

  // Test descendant->Changed(ancestor).
  EXPECT_FALSE(clip.ancestor->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(root), nullptr));
  EXPECT_FALSE(clip.ancestor->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(ancestor), nullptr));
  EXPECT_TRUE(clip.child1->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                   STATE(root), nullptr));
  EXPECT_TRUE(clip.child1->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                   STATE(ancestor), nullptr));
  EXPECT_TRUE(clip.grandchild1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(ancestor), nullptr));
  EXPECT_FALSE(clip.grandchild1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(child1), nullptr));
  EXPECT_FALSE(clip.child2->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                    STATE(ancestor), nullptr));
  EXPECT_FALSE(clip.grandchild2->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(ancestor), nullptr));

  // Test property->Changed(PaintPropertyChangeType::kChangedOnlyValues,
  // non-ancestor-property). Simply walk to the root, regardless of
  // relative_to_state's path.
  EXPECT_FALSE(clip.child2->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                    STATE(child1), nullptr));
  EXPECT_TRUE(clip.child1->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                   STATE(child2), nullptr));
  EXPECT_FALSE(clip.child2->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                    STATE(grandchild1), nullptr));
  EXPECT_TRUE(clip.child1->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                   STATE(grandchild2), nullptr));
  EXPECT_TRUE(clip.grandchild1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(child2), nullptr));
  EXPECT_TRUE(
      clip.grandchild1->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                STATE(grandchild2), nullptr));
  EXPECT_FALSE(clip.grandchild2->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(child1), nullptr));
  EXPECT_FALSE(
      clip.grandchild2->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                STATE(grandchild1), nullptr));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, EffectChangeOneChild) {
  ResetAllChanged();
  ExpectUnchangedState();
  EffectPaintPropertyNode::State state{transform.ancestor, clip.ancestor};
  state.opacity = 0.9;
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues,
            effect.child1->Update(*effect.root, std::move(state)));

  // Test descendant->Changed(PaintPropertyChangeType::kChangedOnlyValues,
  // ancestor).
  EXPECT_FALSE(effect.ancestor->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(root), nullptr));
  EXPECT_FALSE(effect.ancestor->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(ancestor), nullptr));
  EXPECT_TRUE(effect.child1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(root), nullptr));
  EXPECT_TRUE(effect.child1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(ancestor), nullptr));
  EXPECT_TRUE(effect.grandchild1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(ancestor), nullptr));
  EXPECT_FALSE(effect.grandchild1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(child1), nullptr));
  EXPECT_FALSE(effect.child2->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(ancestor), nullptr));
  EXPECT_FALSE(effect.grandchild2->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(ancestor), nullptr));

  // Test property->Changed(PaintPropertyChangeType::kChangedOnlyValues,
  // non-ancestor-property). Simply walk to the root, regardless of
  // relative_to_state's path.
  EXPECT_FALSE(effect.child2->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(child1), nullptr));
  EXPECT_TRUE(effect.child1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(child2), nullptr));
  EXPECT_FALSE(
      effect.child2->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                             STATE(grandchild1), nullptr));
  EXPECT_TRUE(
      effect.child1->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                             STATE(grandchild2), nullptr));
  EXPECT_TRUE(effect.grandchild1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(child2), nullptr));
  EXPECT_TRUE(
      effect.grandchild1->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                  STATE(grandchild2), nullptr));
  EXPECT_FALSE(effect.grandchild2->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, STATE(child1), nullptr));
  EXPECT_FALSE(
      effect.grandchild2->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                  STATE(grandchild1), nullptr));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, TransformReparent) {
  ResetAllChanged();
  ExpectUnchangedState();
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues,
            transform.child1->Update(*transform.child2,
                                     TransformPaintPropertyNode::State{
                                         {MakeTranslationMatrix(1, 2)}}));
  EXPECT_FALSE(transform.ancestor->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, *transform.root));
  EXPECT_TRUE(transform.child1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, *transform.ancestor));
  EXPECT_TRUE(transform.child1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, *transform.child2));
  EXPECT_FALSE(transform.child2->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, *transform.ancestor));
  EXPECT_TRUE(transform.grandchild1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, *transform.ancestor));
  EXPECT_FALSE(transform.grandchild1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, *transform.child1));
  EXPECT_TRUE(transform.grandchild1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, *transform.child2));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, ClipLocalTransformSpaceChange) {
  ResetAllChanged();
  ExpectUnchangedState();
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
            transform.child1->Update(*transform.ancestor,
                                     TransformPaintPropertyNode::State{
                                         {MakeTranslationMatrix(1, 2)}}));

  // We check that we detect the change from the transform. However, right now
  // we report simple value change which may be a bit confusing. See
  // crbug.com/948695 for a task to fix this.
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, clip.ancestor,
                   STATE(root), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, clip.ancestor,
                   STATE(ancestor), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   clip.child1, STATE(root), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   clip.child1, STATE(ancestor), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   clip.grandchild1, STATE(ancestor), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, clip.grandchild1,
                   STATE(child1), nullptr);

  // Test with transform_not_to_check.
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, clip.child1,
                   STATE(root), transform.child1.Get());
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, clip.child1,
                   STATE(ancestor), transform.child1.Get());
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   clip.grandchild1, STATE(ancestor), transform.child1.Get());
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   clip.child1, STATE(root), transform.ancestor.Get());
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   clip.child1, STATE(ancestor), transform.ancestor.Get());
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   clip.grandchild1, STATE(ancestor), transform.ancestor.Get());

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, EffectLocalTransformSpaceChange) {
  // Let effect.child1 have pixel-moving filter.
  EffectPaintPropertyNode::State state{transform.child1, clip.child1};
  state.filter.AppendBlurFilter(20);
  effect.child1->Update(*effect.ancestor, std::move(state));

  ResetAllChanged();
  ExpectUnchangedState();
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
            transform.ancestor->Update(*transform.root,
                                       TransformPaintPropertyNode::State{
                                           {MakeTranslationMatrix(1, 2)}}));

  // We check that we detect the change from the transform. However, right now
  // we report simple value change which may be a bit confusing. See
  // crbug.com/948695 for a task to fix this.
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.ancestor,
                   STATE(root), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.ancestor,
                   STATE(ancestor), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   effect.child1, STATE(root), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.child1,
                   STATE(ancestor), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   effect.grandchild1, STATE(root), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.grandchild1,
                   STATE(ancestor), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.grandchild1,
                   STATE(child1), nullptr);

  // Effects without self or ancestor pixel-moving filter are not affected by
  // change of LocalTransformSpace.
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.child2,
                   STATE(root), nullptr);
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.grandchild2,
                   STATE(root), nullptr);

  // Test with transform_not_to_check.
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, effect.child1,
                   STATE(root), transform.child1.Get());
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   effect.child1, STATE(root), transform.ancestor.Get());

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, TransformChange2dAxisAlignment) {
  auto* t = Create2DTranslation(t0(), 10, 20);
  EXPECT_EQ(PaintPropertyChangeType::kNodeAddedOrRemoved, NodeChanged(*t));
  t->ClearChangedToRoot(++sequence_number);
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));

  // Translation doesn't affect 2d axis alignment.
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
            t->Update(t0(), TransformPaintPropertyNode::State{
                                {MakeTranslationMatrix(30, 40)}}));
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues, NodeChanged(*t));
  t->ClearChangedToRoot(++sequence_number);
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));

  // Scale doesn't affect 2d axis alignment.
  auto matrix = MakeScaleMatrix(2, 3, 4);
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
            t->Update(t0(), TransformPaintPropertyNode::State{{matrix}}));
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues, NodeChanged(*t));
  t->ClearChangedToRoot(++sequence_number);
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));

  // Rotation affects 2d axis alignment.
  EXPECT_EQ(t->Matrix(), matrix);
  matrix.Rotate(45);
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues,
            t->Update(t0(), TransformPaintPropertyNode::State{{matrix}}));
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues, NodeChanged(*t));
  t->ClearChangedToRoot(++sequence_number);
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));

  // Changing scale but keeping original rotation doesn't change 2d axis
  // alignment and is treated as simple.
  EXPECT_EQ(t->Matrix(), matrix);
  matrix.Scale3d(3, 4, 5);
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
            t->Update(t0(), TransformPaintPropertyNode::State{{matrix}}));
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues, NodeChanged(*t));
  t->ClearChangedToRoot(++sequence_number);
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));

  // Change rotation again changes 2d axis alignment.
  EXPECT_EQ(t->Matrix(), matrix);
  matrix.Rotate(10);
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues,
            t->Update(t0(), TransformPaintPropertyNode::State{{matrix}}));
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues, NodeChanged(*t));
  t->ClearChangedToRoot(++sequence_number);
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));

  // Reset the transform back to simple translation changes 2d axis alignment.
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues,
            t->Update(t0(), TransformPaintPropertyNode::State{
                                {MakeTranslationMatrix(1, 2)}}));
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues, NodeChanged(*t));
  t->ClearChangedToRoot(++sequence_number);
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));
}

}  // namespace blink
