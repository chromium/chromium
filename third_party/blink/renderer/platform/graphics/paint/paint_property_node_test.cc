// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by node BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

namespace blink {

class PaintPropertyNodeTest : public testing::Test {
 protected:
  template <typename NodeType>
  struct Tree {
    const NodeType* root;
    scoped_refptr<NodeType> ancestor;
    scoped_refptr<NodeType> child1;
    scoped_refptr<NodeType> child2;
    scoped_refptr<NodeType> grandchild1;
    scoped_refptr<NodeType> grandchild2;
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
    transform.ancestor =
        CreateTransform(*transform.root, TransformationMatrix());
    transform.child1 =
        CreateTransform(*transform.ancestor, TransformationMatrix());
    transform.child2 =
        CreateTransform(*transform.ancestor, TransformationMatrix());
    transform.grandchild1 =
        CreateTransform(*transform.child1, TransformationMatrix());
    transform.grandchild2 =
        CreateTransform(*transform.child2, TransformationMatrix());

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
                                          clip.ancestor.get(), 0.5);
    effect.child1 = CreateOpacityEffect(*effect.ancestor, *transform.child1,
                                        clip.child1.get(), 0.5);
    effect.child2 = CreateOpacityEffect(*effect.ancestor, *transform.child2,
                                        clip.child2.get(), 0.5);
    effect.grandchild1 = CreateOpacityEffect(
        *effect.child1, *transform.grandchild1, clip.grandchild1.get(), 0.5);
    effect.grandchild2 = CreateOpacityEffect(
        *effect.child2, *transform.grandchild2, clip.grandchild2.get(), 0.5);
  }

  template <typename NodeType>
  void ResetAllChanged(Tree<NodeType>& tree) {
    tree.grandchild1->ClearChangedToRoot();
    tree.grandchild2->ClearChangedToRoot();
  }

  void ResetAllChanged() {
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
            &LowestCommonAncestor(*transform.ancestor, *transform.ancestor));
  EXPECT_EQ(transform.root,
            &LowestCommonAncestor(*transform.root, *transform.root));

  EXPECT_EQ(transform.ancestor, &LowestCommonAncestor(*transform.grandchild1,
                                                      *transform.grandchild2));
  EXPECT_EQ(transform.ancestor,
            &LowestCommonAncestor(*transform.grandchild1, *transform.child2));
  EXPECT_EQ(transform.root,
            &LowestCommonAncestor(*transform.grandchild1, *transform.root));
  EXPECT_EQ(transform.child1,
            &LowestCommonAncestor(*transform.grandchild1, *transform.child1));

  EXPECT_EQ(transform.ancestor, &LowestCommonAncestor(*transform.grandchild2,
                                                      *transform.grandchild1));
  EXPECT_EQ(transform.ancestor,
            &LowestCommonAncestor(*transform.grandchild2, *transform.child1));
  EXPECT_EQ(transform.root,
            &LowestCommonAncestor(*transform.grandchild2, *transform.root));
  EXPECT_EQ(transform.child2,
            &LowestCommonAncestor(*transform.grandchild2, *transform.child2));

  EXPECT_EQ(transform.ancestor,
            &LowestCommonAncestor(*transform.child1, *transform.child2));
  EXPECT_EQ(transform.ancestor,
            &LowestCommonAncestor(*transform.child2, *transform.child1));
}

TEST_F(PaintPropertyNodeTest, InitialStateAndReset) {
  ExpectInitialState(transform);
  ResetAllChanged(transform);
  ExpectUnchangedState(transform);
}

TEST_F(PaintPropertyNodeTest, TransformChangeAncestor) {
  ResetAllChanged();
  ExpectUnchangedState();
  transform.ancestor->Update(
      *transform.root, TransformPaintPropertyNode::State{FloatSize(1, 2)});

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
  clip.ancestor->Update(
      *clip.root, ClipPaintPropertyNode::State{transform.ancestor.get(),
                                               FloatRoundedRect(1, 2, 3, 4)});

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
  EffectPaintPropertyNode::State state{transform.ancestor.get(),
                                       clip.ancestor.get()};
  state.compositor_element_id = effect.ancestor->GetCompositorElementId();

  // The initial test starts with opacity 0.5, and we're changing it to 0.9
  // here.
  state.opacity = 0.9;
  effect.ancestor->Update(*effect.root, std::move(state));

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

TEST_F(PaintPropertyNodeTest, EffectOpacityChangesToOne) {
  ResetAllChanged();
  ExpectUnchangedState();
  EffectPaintPropertyNode::State state{transform.ancestor.get(),
                                       clip.ancestor.get()};
  // The initial test starts with opacity 0.5, and we're changing it to 1
  // here.
  state.opacity = 1.f;
  effect.ancestor->Update(*effect.root, std::move(state));

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

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, ChangeDirectCompositingReason) {
  ResetAllChanged();
  ExpectUnchangedState();
  TransformPaintPropertyNode::State state;
  state.direct_compositing_reasons =
      CompositingReason::kActiveTransformAnimation;
  transform.child1->Update(*transform.ancestor, std::move(state));

  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlyNonRerasterValues,
                   transform.child1, *transform.root);

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, ChangeTransformDuringCompositedAnimation) {
  ResetAllChanged();
  ExpectUnchangedState();
  TransformPaintPropertyNode::AnimationState animation_state;
  animation_state.is_running_animation_on_compositor = true;
  transform.child1->Update(
      *transform.ancestor,
      TransformPaintPropertyNode::State{TransformationMatrix().Scale(2)},
      animation_state);

  EXPECT_FALSE(transform.child1->Changed(
      PaintPropertyChangeType::kChangedOnlyValues, *transform.root));
  EXPECT_FALSE(transform.child1->Changed(
      PaintPropertyChangeType::kChangedOnlyNonRerasterValues, *transform.root));
  EXPECT_TRUE(transform.child1->Changed(
      PaintPropertyChangeType::kChangedOnlyCompositedValues, *transform.root));

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, TransformChangeOneChild) {
  ResetAllChanged();
  ExpectUnchangedState();
  transform.child1->Update(*transform.ancestor,
                           TransformPaintPropertyNode::State{FloatSize(1, 2)});

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
  clip.child1->Update(
      *clip.root, ClipPaintPropertyNode::State{transform.ancestor.get(),
                                               FloatRoundedRect(1, 2, 3, 4)});

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
  EffectPaintPropertyNode::State state{transform.ancestor.get(),
                                       clip.ancestor.get()};
  state.opacity = 0.9;
  effect.child1->Update(*effect.root, std::move(state));

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
  transform.child1->Update(*transform.child2,
                           TransformPaintPropertyNode::State{FloatSize(1, 2)});
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
  transform.child1->Update(*transform.ancestor,
                           TransformPaintPropertyNode::State{FloatSize(1, 2)});

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
                   STATE(root), transform.child1.get());
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kUnchanged, clip.child1,
                   STATE(ancestor), transform.child1.get());
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   clip.grandchild1, STATE(ancestor), transform.child1.get());
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   clip.child1, STATE(root), transform.ancestor.get());
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   clip.child1, STATE(ancestor), transform.ancestor.get());
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   clip.grandchild1, STATE(ancestor), transform.ancestor.get());

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, EffectLocalTransformSpaceChange) {
  // Let effect.child1 have pixel-moving filter.
  EffectPaintPropertyNode::State state{transform.child1.get(),
                                       clip.child1.get()};
  state.filter.AppendBlurFilter(20);
  effect.child1->Update(*effect.ancestor, std::move(state));

  ResetAllChanged();
  ExpectUnchangedState();
  transform.ancestor->Update(
      *transform.root, TransformPaintPropertyNode::State{FloatSize(1, 2)});

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
                   STATE(root), transform.child1.get());
  EXPECT_CHANGE_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
                   effect.child1, STATE(root), transform.ancestor.get());

  ResetAllChanged();
  ExpectUnchangedState();
}

TEST_F(PaintPropertyNodeTest, TransformChange2dAxisAlignment) {
  auto t = Create2DTranslation(t0(), 10, 20);
  EXPECT_EQ(PaintPropertyChangeType::kNodeAddedOrRemoved, NodeChanged(*t));
  t->ClearChangedToRoot();
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));

  // Translation doesn't affect 2d axis alignment.
  t->Update(t0(), TransformPaintPropertyNode::State{FloatSize(30, 40)});
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues, NodeChanged(*t));
  t->ClearChangedToRoot();
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));

  // Scale doesn't affect 2d axis alignment.
  t->Update(t0(), TransformPaintPropertyNode::State{
                      TransformationMatrix().Scale3d(2, 3, 4)});
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues, NodeChanged(*t));
  t->ClearChangedToRoot();
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));

  // Rotation affects 2d axis alignment.
  t->Update(t0(), TransformPaintPropertyNode::State{
                      TransformationMatrix(t->Matrix()).Rotate(45)});
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues, NodeChanged(*t));
  t->ClearChangedToRoot();
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));

  // Changing scale but keeping original rotation doesn't change 2d axis
  // alignment and is treated as simple.
  t->Update(t0(), TransformPaintPropertyNode::State{
                      TransformationMatrix(t->Matrix()).Scale3d(3, 4, 5)});
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues, NodeChanged(*t));
  t->ClearChangedToRoot();
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));

  // Change rotation rotation again changes 2d axis alignment.
  t->Update(t0(), TransformPaintPropertyNode::State{
                      TransformationMatrix(t->Matrix()).Rotate(10)});
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues, NodeChanged(*t));
  t->ClearChangedToRoot();
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));

  // Reset the transform back to simple translation changes 2d axis alignment.
  t->Update(t0(), TransformPaintPropertyNode::State{FloatSize(1, 2)});
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues, NodeChanged(*t));
  t->ClearChangedToRoot();
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, NodeChanged(*t));
}

}  // namespace blink
