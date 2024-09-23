// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"

#include <memory>

#include "base/containers/adapters.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "cc/view_transition/view_transition_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"
#include "third_party/blink/renderer/platform/testing/layer_tree_host_embedder.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/picture_matchers.h"
#include "third_party/blink/renderer/platform/testing/test_paint_artifact.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace blink {
namespace {

using testing::ElementsAre;
using testing::Pointee;

gfx::Transform Translation(SkScalar x, SkScalar y) {
  gfx::Transform transform;
  transform.Translate(x, y);
  return transform;
}

class MockScrollCallbacks : public CompositorScrollCallbacks {
 public:
  MOCK_METHOD3(DidCompositorScroll,
               void(CompositorElementId,
                    const gfx::PointF&,
                    const std::optional<cc::TargetSnapAreaElementIds>&));
  MOCK_METHOD2(DidChangeScrollbarsHidden, void(CompositorElementId, bool));

  base::WeakPtr<MockScrollCallbacks> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockScrollCallbacks> weak_ptr_factory_{this};
};

class PaintArtifactCompositorTest : public testing::Test,
                                    public PaintTestConfigurations {
 protected:
  PaintArtifactCompositorTest()
      : task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        task_runner_current_default_handle_(task_runner_) {}

  void SetUp() override {
    // Delay constructing the compositor until after the feature is set.
    paint_artifact_compositor_ = MakeGarbageCollected<PaintArtifactCompositor>(
        scroll_callbacks_.GetWeakPtr());
    // Prefer lcd-text by default for tests.
    paint_artifact_compositor_->SetLCDTextPreference(
        LCDTextPreference::kStronglyPreferred);

    // Uses a LayerTreeHostClient that will make a LayerTreeFrameSink to allow
    // the compositor to run and submit frames.
    layer_tree_ = std::make_unique<LayerTreeHostEmbedder>(
        &layer_tree_host_client_,
        /*single_thread_client=*/nullptr);
    layer_tree_host_client_.SetLayerTreeHost(layer_tree_->layer_tree_host());
    layer_tree_->layer_tree_host()->SetRootLayer(
        paint_artifact_compositor_->RootLayer());
  }

  void TearDown() override {
    // Make sure we remove all child layers to satisfy destructor
    // child layer element id DCHECK.
    WillBeRemovedFromFrame();
    layer_tree_host_client_.SetLayerTreeHost(nullptr);
  }

  cc::PropertyTrees& GetPropertyTrees() {
    return *layer_tree_->layer_tree_host()->property_trees();
  }

  const cc::TransformNode& GetTransformNode(const cc::Layer* layer) {
    return *GetPropertyTrees().transform_tree().Node(
        layer->transform_tree_index());
  }

  const cc::EffectNode& GetEffectNode(const cc::Layer* layer) {
    return *GetPropertyTrees().effect_tree().Node(layer->effect_tree_index());
  }

  cc::LayerTreeHost& GetLayerTreeHost() {
    return *layer_tree_->layer_tree_host();
  }

  int ElementIdToEffectNodeIndex(CompositorElementId element_id) {
    auto* property_trees = layer_tree_->layer_tree_host()->property_trees();
    const auto* node =
        property_trees->effect_tree().FindNodeFromElementId(element_id);
    return node ? node->id : -1;
  }

  int ElementIdToTransformNodeIndex(CompositorElementId element_id) {
    auto* property_trees = layer_tree_->layer_tree_host()->property_trees();
    const auto* node =
        property_trees->transform_tree().FindNodeFromElementId(element_id);
    return node ? node->id : -1;
  }

  int ElementIdToScrollNodeIndex(CompositorElementId element_id) {
    auto* property_trees = layer_tree_->layer_tree_host()->property_trees();
    const auto* node =
        property_trees->scroll_tree().FindNodeFromElementId(element_id);
    return node ? node->id : -1;
  }

  using ViewportProperties = PaintArtifactCompositor::ViewportProperties;

  void Update(
      const PaintArtifact& artifact,
      const ViewportProperties& viewport_properties = ViewportProperties(),
      const PaintArtifactCompositor::StackScrollTranslationVector&
          scroll_translation_nodes = {}) {
    paint_artifact_compositor_->SetNeedsUpdate();
    paint_artifact_compositor_->Update(artifact, viewport_properties,
                                       scroll_translation_nodes, {});
    layer_tree_->layer_tree_host()->LayoutAndUpdateLayers();
  }

  void WillBeRemovedFromFrame() {
    paint_artifact_compositor_->WillBeRemovedFromFrame();
  }

  cc::Layer* RootLayer() { return paint_artifact_compositor_->RootLayer(); }

  // Returns the |num|th scroll hit test layer.
  cc::Layer* ScrollHitTestLayerAt(size_t num) {
    const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree();
    for (auto& layer : RootLayer()->children()) {
      if (scroll_tree.FindNodeFromElementId(layer->element_id())) {
        if (num == 0)
          return layer.get();
        num--;
      }
    }
    return nullptr;
  }

  // Returns the |num|th non-scrollable content layer.
  cc::Layer* NonScrollHitTestLayerAt(size_t num) {
    const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree();
    for (auto& layer : RootLayer()->children()) {
      if (!scroll_tree.FindNodeFromElementId(layer->element_id())) {
        if (num == 0)
          return layer.get();
        num--;
      }
    }
    return nullptr;
  }

  size_t LayerCount() { return RootLayer()->children().size(); }

  cc::Layer* LayerAt(unsigned index) {
    return RootLayer()->children()[index].get();
  }

  size_t SynthesizedClipLayerCount() {
    return paint_artifact_compositor_->SynthesizedClipLayersForTesting().size();
  }

  cc::Layer* SynthesizedClipLayerAt(unsigned index) {
    return paint_artifact_compositor_->SynthesizedClipLayersForTesting()[index];
  }

  // Return the index of |layer| in the root layer list, or -1 if not found.
  int LayerIndex(const cc::Layer* layer) {
    int i = 0;
    for (auto& child : RootLayer()->children()) {
      if (child.get() == layer)
        return i;
      i++;
    }
    return -1;
  }

  void UpdateWithEffectivelyInvisibleChunk(bool include_preceding_chunk,
                                           bool include_subsequent_chunk) {
    TestPaintArtifact artifact;
    if (include_preceding_chunk)
      artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 10, 10), Color::kBlack);
    artifact.Chunk().EffectivelyInvisible().RectDrawing(
        gfx::Rect(10, 0, 10, 10), Color(255, 0, 0));
    if (include_subsequent_chunk)
      artifact.Chunk().RectDrawing(gfx::Rect(0, 10, 10, 10), Color::kWhite);
    Update(artifact.Build());
  }

  MockScrollCallbacks& ScrollCallbacks() { return scroll_callbacks_; }

  PaintArtifactCompositor& GetPaintArtifactCompositor() {
    return *paint_artifact_compositor_;
  }

  int CcNodeId(const PaintPropertyNode& node) {
    return node.CcNodeId(GetPropertyTrees().sequence_number());
  }

 private:
  MockScrollCallbacks scroll_callbacks_;
  Persistent<PaintArtifactCompositor> paint_artifact_compositor_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
  cc::FakeLayerTreeHostClient layer_tree_host_client_;
  std::unique_ptr<LayerTreeHostEmbedder> layer_tree_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(PaintArtifactCompositorTest);

const auto kNotScrollingOnMain =
    cc::MainThreadScrollingReason::kNotScrollingOnMain;

TEST_P(PaintArtifactCompositorTest, EmptyPaintArtifact) {
  Update(*MakeGarbageCollected<PaintArtifact>());
  EXPECT_TRUE(RootLayer()->children().empty());
}

TEST_P(PaintArtifactCompositorTest, OneChunkWithAnOffset) {
  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(gfx::Rect(50, -50, 100, 100), Color::kWhite);
  Update(artifact.Build());

  ASSERT_EQ(1u, LayerCount());
  const cc::Layer* child = LayerAt(0);
  EXPECT_THAT(
      child->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 100, 100), Color::kWhite)));
  EXPECT_EQ(Translation(50, -50), child->ScreenSpaceTransform());
  EXPECT_EQ(gfx::Size(100, 100), child->bounds());
  EXPECT_FALSE(GetTransformNode(child).transform_changed);
}

TEST_P(PaintArtifactCompositorTest, OneTransform) {
  // A 90 degree clockwise rotation about (100, 100).
  auto* transform =
      CreateTransform(t0(), MakeRotationMatrix(90), gfx::Point3F(100, 100, 0),
                      CompositingReason::k3DTransform);

  TestPaintArtifact artifact;
  artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kGray);
  artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(gfx::Rect(100, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(2u, LayerCount());
  {
    const cc::Layer* layer = LayerAt(0);
    EXPECT_TRUE(GetTransformNode(layer).transform_changed);

    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(100, 0, 100, 100), Color::kBlack));

    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
    EXPECT_EQ(
        gfx::RectF(100, 0, 100, 100),
        layer->ScreenSpaceTransform().MapRect(gfx::RectF(0, 0, 100, 100)));
  }
  {
    const cc::Layer* layer = LayerAt(1);
    EXPECT_FALSE(GetTransformNode(layer).transform_changed);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(gfx::RectF(0, 0, 100, 100), Color::kGray)));
    EXPECT_EQ(gfx::Transform(), layer->ScreenSpaceTransform());
  }
}

TEST_P(PaintArtifactCompositorTest, OneTransformWithAlias) {
  // A 90 degree clockwise rotation about (100, 100).
  auto* real_transform =
      CreateTransform(t0(), MakeRotationMatrix(90), gfx::Point3F(100, 100, 0),
                      CompositingReason::k3DTransform);
  auto* transform = TransformPaintPropertyNodeAlias::Create(*real_transform);

  TestPaintArtifact artifact;
  artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kGray);
  artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(gfx::Rect(100, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(2u, LayerCount());
  {
    const cc::Layer* layer = LayerAt(0);
    EXPECT_TRUE(GetTransformNode(layer).transform_changed);

    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(100, 0, 100, 100), Color::kBlack));

    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
    EXPECT_EQ(
        gfx::RectF(100, 0, 100, 100),
        layer->ScreenSpaceTransform().MapRect(gfx::RectF(0, 0, 100, 100)));
  }
  {
    const cc::Layer* layer = LayerAt(1);
    EXPECT_FALSE(GetTransformNode(layer).transform_changed);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(gfx::RectF(0, 0, 100, 100), Color::kGray)));
    EXPECT_EQ(gfx::Transform(), layer->ScreenSpaceTransform());
  }
}

TEST_P(PaintArtifactCompositorTest, TransformCombining) {
  // A translation by (5, 5) within a 2x scale about (10, 10).
  auto* transform1 =
      CreateTransform(t0(), MakeScaleMatrix(2), gfx::Point3F(10, 10, 0),
                      CompositingReason::k3DTransform);
  auto* transform2 =
      CreateTransform(*transform1, MakeTranslationMatrix(5, 5), gfx::Point3F(),
                      CompositingReason::kWillChangeTransform);

  TestPaintArtifact artifact;
  artifact.Chunk(*transform1, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 300, 200), Color::kWhite);
  artifact.Chunk(*transform2, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 300, 200), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(2u, LayerCount());
  {
    const cc::Layer* layer = LayerAt(0);
    EXPECT_TRUE(GetTransformNode(layer).transform_changed);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(gfx::RectF(0, 0, 300, 200), Color::kWhite)));
    EXPECT_EQ(
        gfx::RectF(-10, -10, 600, 400),
        layer->ScreenSpaceTransform().MapRect(gfx::RectF(0, 0, 300, 200)));
  }
  {
    const cc::Layer* layer = LayerAt(1);
    EXPECT_TRUE(GetTransformNode(layer).transform_changed);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(gfx::RectF(0, 0, 300, 200), Color::kBlack)));
    EXPECT_EQ(gfx::RectF(0, 0, 600, 400), layer->ScreenSpaceTransform().MapRect(
                                              gfx::RectF(0, 0, 300, 200)));
  }
  EXPECT_NE(LayerAt(0)->transform_tree_index(),
            LayerAt(1)->transform_tree_index());
}

TEST_P(PaintArtifactCompositorTest, BackfaceVisibility) {
  TransformPaintPropertyNode::State backface_hidden_state;
  backface_hidden_state.backface_visibility =
      TransformPaintPropertyNode::BackfaceVisibility::kHidden;
  auto* backface_hidden_transform = TransformPaintPropertyNode::Create(
      t0(), std::move(backface_hidden_state));

  auto* backface_inherited_transform = TransformPaintPropertyNode::Create(
      *backface_hidden_transform, TransformPaintPropertyNode::State{});

  TransformPaintPropertyNode::State backface_visible_state;
  backface_visible_state.backface_visibility =
      TransformPaintPropertyNode::BackfaceVisibility::kVisible;
  auto* backface_visible_transform = TransformPaintPropertyNode::Create(
      *backface_hidden_transform, std::move(backface_visible_state));

  TestPaintArtifact artifact;
  artifact.Chunk(*backface_hidden_transform, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 300, 200), Color::kWhite);
  artifact.Chunk(*backface_inherited_transform, c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 100, 100), Color::kBlack);
  artifact.Chunk(*backface_visible_transform, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 300, 200), Color::kDarkGray);
  Update(artifact.Build());

  ASSERT_EQ(2u, LayerCount());
  EXPECT_THAT(
      LayerAt(0)->GetPicture(),
      Pointee(DrawsRectangles(Vector<RectWithColor>{
          RectWithColor(gfx::RectF(0, 0, 300, 200), Color::kWhite),
          RectWithColor(gfx::RectF(100, 100, 100, 100), Color::kBlack)})));
  EXPECT_THAT(
      LayerAt(1)->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 300, 200), Color::kDarkGray)));
}

TEST_P(PaintArtifactCompositorTest, FlattensInheritedTransform) {
  for (bool transform_is_flattened : {true, false}) {
    SCOPED_TRACE(transform_is_flattened);

    // The flattens_inherited_transform bit corresponds to whether the _parent_
    // transform node flattens the transform. This is because Blink's notion of
    // flattening determines whether content within the node's local transform
    // is flattened, while cc's notion applies in the parent's coordinate space.
    auto* transform1 = CreateTransform(t0(), gfx::Transform());
    auto* transform2 =
        CreateTransform(*transform1, MakeRotationMatrix(0, 45, 0));
    TransformPaintPropertyNode::State transform3_state{
        {MakeRotationMatrix(0, 45, 0)}};
    transform3_state.flattens_inherited_transform = transform_is_flattened;
    auto* transform3 = TransformPaintPropertyNode::Create(
        *transform2, std::move(transform3_state));

    TestPaintArtifact artifact;
    artifact.Chunk(*transform3, c0(), e0())
        .RectDrawing(gfx::Rect(0, 0, 300, 200), Color::kWhite);
    Update(artifact.Build());

    ASSERT_EQ(1u, LayerCount());
    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(gfx::RectF(0, 0, 300, 200), Color::kWhite)));

    // The leaf transform node should flatten its inherited transform node
    // if and only if the intermediate rotation transform in the Blink tree
    // flattens.
    const cc::TransformNode* transform_node3 =
        GetPropertyTrees().transform_tree().Node(layer->transform_tree_index());
    EXPECT_EQ(transform_is_flattened,
              transform_node3->flattens_inherited_transform);

    // Given this, we should expect the correct screen space transform for
    // each case. If the transform was flattened, we should see it getting
    // an effective horizontal scale of 1/sqrt(2) each time, thus it gets
    // half as wide. If the transform was not flattened, we should see an
    // empty rectangle (as the total 90 degree rotation makes it
    // perpendicular to the viewport).
    gfx::RectF rect =
        layer->ScreenSpaceTransform().MapRect(gfx::RectF(0, 0, 100, 100));
    if (transform_is_flattened)
      EXPECT_RECTF_EQ(gfx::RectF(0, 0, 50, 100), rect);
    else
      EXPECT_TRUE(rect.IsEmpty());
  }
}

TEST_P(PaintArtifactCompositorTest, FlattensInheritedTransformWithAlias) {
  for (bool transform_is_flattened : {true, false}) {
    SCOPED_TRACE(transform_is_flattened);

    // The flattens_inherited_transform bit corresponds to whether the _parent_
    // transform node flattens the transform. This is because Blink's notion of
    // flattening determines whether content within the node's local transform
    // is flattened, while cc's notion applies in the parent's coordinate space.
    auto* real_transform1 = CreateTransform(t0(), gfx::Transform());
    auto* transform1 =
        TransformPaintPropertyNodeAlias::Create(*real_transform1);
    auto* real_transform2 =
        CreateTransform(*transform1, MakeRotationMatrix(0, 45, 0));
    auto* transform2 =
        TransformPaintPropertyNodeAlias::Create(*real_transform2);
    TransformPaintPropertyNode::State transform3_state{
        {MakeRotationMatrix(0, 45, 0)}};
    transform3_state.flattens_inherited_transform = transform_is_flattened;
    auto* real_transform3 = TransformPaintPropertyNode::Create(
        *transform2, std::move(transform3_state));
    auto* transform3 =
        TransformPaintPropertyNodeAlias::Create(*real_transform3);

    TestPaintArtifact artifact;
    artifact.Chunk(*transform3, c0(), e0())
        .RectDrawing(gfx::Rect(0, 0, 300, 200), Color::kWhite);
    Update(artifact.Build());

    ASSERT_EQ(1u, LayerCount());
    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(gfx::RectF(0, 0, 300, 200), Color::kWhite)));

    // The leaf transform node should flatten its inherited transform node
    // if and only if the intermediate rotation transform in the Blink tree
    // flattens.
    const cc::TransformNode* transform_node3 =
        GetPropertyTrees().transform_tree().Node(layer->transform_tree_index());
    EXPECT_EQ(transform_is_flattened,
              transform_node3->flattens_inherited_transform);

    // Given this, we should expect the correct screen space transform for
    // each case. If the transform was flattened, we should see it getting
    // an effective horizontal scale of 1/sqrt(2) each time, thus it gets
    // half as wide. If the transform was not flattened, we should see an
    // empty rectangle (as the total 90 degree rotation makes it
    // perpendicular to the viewport).
    gfx::RectF rect =
        layer->ScreenSpaceTransform().MapRect(gfx::RectF(0, 0, 100, 100));
    if (transform_is_flattened)
      EXPECT_RECTF_EQ(gfx::RectF(0, 0, 50, 100), rect);
    else
      EXPECT_TRUE(rect.IsEmpty());
  }
}
TEST_P(PaintArtifactCompositorTest, SortingContextID) {
  // Has no 3D rendering context.
  auto* transform1 = CreateTransform(t0(), gfx::Transform());
  // Establishes a 3D rendering context.
  TransformPaintPropertyNode::State transform2_state;
  transform2_state.rendering_context_id = 1;
  transform2_state.direct_compositing_reasons =
      CompositingReason::kWillChangeTransform;
  auto* transform2 = TransformPaintPropertyNode::Create(
      *transform1, std::move(transform2_state));
  // Extends the 3D rendering context of transform2.
  TransformPaintPropertyNode::State transform3_state;
  transform3_state.rendering_context_id = 1;
  transform3_state.direct_compositing_reasons =
      CompositingReason::kWillChangeTransform;
  auto* transform3 = TransformPaintPropertyNode::Create(
      *transform2, std::move(transform3_state));
  // Establishes a 3D rendering context distinct from transform2.
  TransformPaintPropertyNode::State transform4_state;
  transform4_state.rendering_context_id = 2;
  transform4_state.direct_compositing_reasons =
      CompositingReason::kWillChangeTransform;
  auto* transform4 = TransformPaintPropertyNode::Create(
      *transform2, std::move(transform4_state));

  TestPaintArtifact artifact;
  artifact.Chunk(*transform1, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 300, 200), Color::kWhite);
  artifact.Chunk(*transform2, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 300, 200), Color::kLightGray);
  artifact.Chunk(*transform3, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 300, 200), Color::kDarkGray);
  artifact.Chunk(*transform4, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 300, 200), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(4u, LayerCount());

  // The white layer is not 3D sorted.
  const cc::Layer* white_layer = LayerAt(0);
  EXPECT_THAT(
      white_layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 300, 200), Color::kWhite)));
  int white_sorting_context_id =
      GetTransformNode(white_layer).sorting_context_id;
  EXPECT_EQ(0, white_sorting_context_id);

  // The light gray layer is 3D sorted.
  const cc::Layer* light_gray_layer = LayerAt(1);
  EXPECT_THAT(
      light_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 300, 200), Color::kLightGray)));
  int light_gray_sorting_context_id =
      GetTransformNode(light_gray_layer).sorting_context_id;
  EXPECT_NE(0, light_gray_sorting_context_id);

  // The dark gray layer is 3D sorted with the light gray layer, but has a
  // separate transform node.
  const cc::Layer* dark_gray_layer = LayerAt(2);
  EXPECT_THAT(
      dark_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 300, 200), Color::kDarkGray)));
  int dark_gray_sorting_context_id =
      GetTransformNode(dark_gray_layer).sorting_context_id;
  EXPECT_EQ(light_gray_sorting_context_id, dark_gray_sorting_context_id);
  EXPECT_NE(light_gray_layer->transform_tree_index(),
            dark_gray_layer->transform_tree_index());

  // The black layer is 3D sorted, but in a separate context from the previous
  // layers.
  const cc::Layer* black_layer = LayerAt(3);
  EXPECT_THAT(
      black_layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 300, 200), Color::kBlack)));
  int black_sorting_context_id =
      GetTransformNode(black_layer).sorting_context_id;
  EXPECT_NE(0, black_sorting_context_id);
  EXPECT_NE(light_gray_sorting_context_id, black_sorting_context_id);
}

TEST_P(PaintArtifactCompositorTest, OneClip) {
  auto* clip = CreateClip(c0(), t0(), FloatRoundedRect(100, 100, 300, 200));

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clip, e0())
      .RectDrawing(gfx::Rect(220, 80, 300, 200), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(1u, LayerCount());
  const cc::Layer* layer = LayerAt(0);
  // The layer is clipped.
  EXPECT_EQ(gfx::Size(180, 180), layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(220, 100), layer->offset_to_transform_parent());
  EXPECT_THAT(
      layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 300, 180), Color::kBlack)));
  EXPECT_EQ(Translation(220, 100), layer->ScreenSpaceTransform());

  const cc::ClipNode* clip_node =
      GetPropertyTrees().clip_tree().Node(layer->clip_tree_index());
  EXPECT_TRUE(clip_node->AppliesLocalClip());
  EXPECT_EQ(gfx::RectF(100, 100, 300, 200), clip_node->clip);
}

TEST_P(PaintArtifactCompositorTest, OneClipWithAlias) {
  auto* real_clip =
      CreateClip(c0(), t0(), FloatRoundedRect(100, 100, 300, 200));
  auto* clip = ClipPaintPropertyNodeAlias::Create(*real_clip);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clip, e0())
      .RectDrawing(gfx::Rect(220, 80, 300, 200), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(1u, LayerCount());
  const cc::Layer* layer = LayerAt(0);
  // The layer is clipped.
  EXPECT_EQ(gfx::Size(180, 180), layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(220, 100), layer->offset_to_transform_parent());
  EXPECT_THAT(
      layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 300, 180), Color::kBlack)));
  EXPECT_EQ(Translation(220, 100), layer->ScreenSpaceTransform());

  const cc::ClipNode* clip_node =
      GetPropertyTrees().clip_tree().Node(layer->clip_tree_index());
  EXPECT_TRUE(clip_node->AppliesLocalClip());
  EXPECT_EQ(gfx::RectF(100, 100, 300, 200), clip_node->clip);
}

TEST_P(PaintArtifactCompositorTest, NestedClips) {
  auto* transform1 = CreateTransform(t0(), gfx::Transform(), gfx::Point3F(),
                                     CompositingReason::kWillChangeTransform);
  auto* clip1 =
      CreateClip(c0(), *transform1, FloatRoundedRect(100, 100, 700, 700));

  auto* transform2 =
      CreateTransform(*transform1, gfx::Transform(), gfx::Point3F(),
                      CompositingReason::kWillChangeTransform);
  auto* clip2 =
      CreateClip(*clip1, *transform2, FloatRoundedRect(200, 200, 700, 700));

  TestPaintArtifact artifact;
  artifact.Chunk(*transform1, *clip1, e0())
      .RectDrawing(gfx::Rect(300, 350, 100, 100), Color::kWhite);
  artifact.Chunk(*transform2, *clip2, e0())
      .RectDrawing(gfx::Rect(300, 350, 100, 100), Color::kLightGray);
  artifact.Chunk(*transform1, *clip1, e0())
      .RectDrawing(gfx::Rect(300, 350, 100, 100), Color::kDarkGray);
  artifact.Chunk(*transform2, *clip2, e0())
      .RectDrawing(gfx::Rect(300, 350, 100, 100), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(4u, LayerCount());

  const cc::Layer* white_layer = LayerAt(0);
  EXPECT_THAT(
      white_layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 100, 100), Color::kWhite)));
  EXPECT_EQ(Translation(300, 350), white_layer->ScreenSpaceTransform());

  const cc::Layer* light_gray_layer = LayerAt(1);
  EXPECT_THAT(
      light_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 100, 100), Color::kLightGray)));
  EXPECT_EQ(Translation(300, 350), light_gray_layer->ScreenSpaceTransform());

  const cc::Layer* dark_gray_layer = LayerAt(2);
  EXPECT_THAT(
      dark_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 100, 100), Color::kDarkGray)));
  EXPECT_EQ(Translation(300, 350), dark_gray_layer->ScreenSpaceTransform());

  const cc::Layer* black_layer = LayerAt(3);
  EXPECT_THAT(
      black_layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 100, 100), Color::kBlack)));
  EXPECT_EQ(Translation(300, 350), black_layer->ScreenSpaceTransform());

  EXPECT_EQ(white_layer->clip_tree_index(), dark_gray_layer->clip_tree_index());
  const cc::ClipNode* outer_clip =
      GetPropertyTrees().clip_tree().Node(white_layer->clip_tree_index());
  EXPECT_TRUE(outer_clip->AppliesLocalClip());
  EXPECT_EQ(gfx::RectF(100, 100, 700, 700), outer_clip->clip);

  EXPECT_EQ(light_gray_layer->clip_tree_index(),
            black_layer->clip_tree_index());
  const cc::ClipNode* inner_clip =
      GetPropertyTrees().clip_tree().Node(black_layer->clip_tree_index());
  EXPECT_TRUE(inner_clip->AppliesLocalClip());
  EXPECT_EQ(gfx::RectF(200, 200, 700, 700), inner_clip->clip);
  EXPECT_EQ(outer_clip->id, inner_clip->parent_id);
}

TEST_P(PaintArtifactCompositorTest, NestedClipsWithAlias) {
  auto* transform1 = CreateTransform(t0(), gfx::Transform(), gfx::Point3F(),
                                     CompositingReason::kWillChangeTransform);
  auto* real_clip1 =
      CreateClip(c0(), *transform1, FloatRoundedRect(100, 100, 700, 700));
  auto* clip1 = ClipPaintPropertyNodeAlias::Create(*real_clip1);
  auto* transform2 =
      CreateTransform(*transform1, gfx::Transform(), gfx::Point3F(),
                      CompositingReason::kWillChangeTransform);
  auto* real_clip2 =
      CreateClip(*clip1, *transform2, FloatRoundedRect(200, 200, 700, 700));
  auto* clip2 = ClipPaintPropertyNodeAlias::Create(*real_clip2);

  TestPaintArtifact artifact;
  artifact.Chunk(*transform1, *clip1, e0())
      .RectDrawing(gfx::Rect(300, 350, 100, 100), Color::kWhite);
  artifact.Chunk(*transform2, *clip2, e0())
      .RectDrawing(gfx::Rect(300, 350, 100, 100), Color::kLightGray);
  artifact.Chunk(*transform1, *clip1, e0())
      .RectDrawing(gfx::Rect(300, 350, 100, 100), Color::kDarkGray);
  artifact.Chunk(*transform2, *clip2, e0())
      .RectDrawing(gfx::Rect(300, 350, 100, 100), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(4u, LayerCount());

  const cc::Layer* white_layer = LayerAt(0);
  EXPECT_THAT(
      white_layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 100, 100), Color::kWhite)));
  EXPECT_EQ(Translation(300, 350), white_layer->ScreenSpaceTransform());

  const cc::Layer* light_gray_layer = LayerAt(1);
  EXPECT_THAT(
      light_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 100, 100), Color::kLightGray)));
  EXPECT_EQ(Translation(300, 350), light_gray_layer->ScreenSpaceTransform());

  const cc::Layer* dark_gray_layer = LayerAt(2);
  EXPECT_THAT(
      dark_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 100, 100), Color::kDarkGray)));
  EXPECT_EQ(Translation(300, 350), dark_gray_layer->ScreenSpaceTransform());

  const cc::Layer* black_layer = LayerAt(3);
  EXPECT_THAT(
      black_layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 100, 100), Color::kBlack)));
  EXPECT_EQ(Translation(300, 350), black_layer->ScreenSpaceTransform());

  EXPECT_EQ(white_layer->clip_tree_index(), dark_gray_layer->clip_tree_index());
  const cc::ClipNode* outer_clip =
      GetPropertyTrees().clip_tree().Node(white_layer->clip_tree_index());
  EXPECT_TRUE(outer_clip->AppliesLocalClip());
  EXPECT_EQ(gfx::RectF(100, 100, 700, 700), outer_clip->clip);

  EXPECT_EQ(light_gray_layer->clip_tree_index(),
            black_layer->clip_tree_index());
  const cc::ClipNode* inner_clip =
      GetPropertyTrees().clip_tree().Node(black_layer->clip_tree_index());
  EXPECT_TRUE(inner_clip->AppliesLocalClip());
  EXPECT_EQ(gfx::RectF(200, 200, 700, 700), inner_clip->clip);
  EXPECT_EQ(outer_clip->id, inner_clip->parent_id);
}

TEST_P(PaintArtifactCompositorTest, DeeplyNestedClips) {
  HeapVector<Member<ClipPaintPropertyNode>> clips;
  for (unsigned i = 1; i <= 10; i++) {
    clips.push_back(CreateClip(clips.empty() ? c0() : *clips.back(), t0(),
                               FloatRoundedRect(5 * i, 0, 100, 200 - 10 * i)));
  }

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clips.back(), e0())
      .RectDrawing(gfx::Rect(0, 0, 200, 200), Color::kWhite);
  Update(artifact.Build());

  // Check the drawing layer. It's clipped.
  ASSERT_EQ(1u, LayerCount());
  const cc::Layer* drawing_layer = LayerAt(0);
  EXPECT_EQ(gfx::Size(100, 100), drawing_layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(50, 0), drawing_layer->offset_to_transform_parent());
  EXPECT_THAT(
      drawing_layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 150, 200), Color::kWhite)));
  EXPECT_EQ(Translation(50, 0), drawing_layer->ScreenSpaceTransform());

  // Check the clip nodes.
  const cc::ClipNode* clip_node =
      GetPropertyTrees().clip_tree().Node(drawing_layer->clip_tree_index());
  for (const auto& paint_clip_node : base::Reversed(clips)) {
    EXPECT_TRUE(clip_node->AppliesLocalClip());
    EXPECT_EQ(paint_clip_node->PaintClipRect().Rect(), clip_node->clip);
    clip_node = GetPropertyTrees().clip_tree().Node(clip_node->parent_id);
  }
}

TEST_P(PaintArtifactCompositorTest, SiblingClipsWithAlias) {
  auto* real_common_clip =
      CreateClip(c0(), t0(), FloatRoundedRect(0, 0, 80, 60));
  auto* common_clip = ClipPaintPropertyNodeAlias::Create(*real_common_clip);
  auto* real_clip1 =
      CreateClip(*common_clip, t0(), FloatRoundedRect(0, 0, 30, 20));
  auto* clip1 = ClipPaintPropertyNodeAlias::Create(*real_clip1);
  auto* real_clip2 =
      CreateClip(*common_clip, t0(), FloatRoundedRect(40, 0, 40, 60));
  auto* clip2 = ClipPaintPropertyNodeAlias::Create(*real_clip2);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clip1, e0())
      .RectDrawing(gfx::Rect(0, 0, 11, 22), Color::kWhite);
  artifact.Chunk(t0(), *clip2, e0())
      .RectDrawing(gfx::Rect(33, 44, 55, 66), Color::kBlack);
  Update(artifact.Build());

  // The two chunks are merged together.
  ASSERT_EQ(1u, LayerCount());
  const cc::Layer* layer = LayerAt(0);
  EXPECT_THAT(layer->GetPicture(),
              Pointee(DrawsRectangles(Vector<RectWithColor>{
                  // This is the first RectDrawing with real_clip1 applied.
                  RectWithColor(gfx::RectF(0, 0, 11, 20), Color::kWhite),
                  // This is the second RectDrawing with real_clip2 applied.
                  RectWithColor(gfx::RectF(40, 44, 40, 16), Color::kBlack)})));
  EXPECT_EQ(gfx::Transform(), layer->ScreenSpaceTransform());
  const cc::ClipNode* clip_node =
      GetPropertyTrees().clip_tree().Node(layer->clip_tree_index());
  EXPECT_TRUE(clip_node->AppliesLocalClip());
  ASSERT_EQ(gfx::RectF(0, 0, 80, 60), clip_node->clip);
}

TEST_P(PaintArtifactCompositorTest, SiblingClipsWithCompositedTransform) {
  auto* t1 = CreateTransform(t0(), gfx::Transform(), gfx::Point3F(),
                             CompositingReason::kWillChangeTransform);
  auto* t2 = CreateTransform(*t1, MakeTranslationMatrix(1, 2));
  auto* c1 = CreateClip(c0(), t0(), FloatRoundedRect(0, 0, 400, 600));
  auto* c2 = CreateClip(c0(), *t2, FloatRoundedRect(400, 0, 400, 600));

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 640, 480), Color::kWhite);
  artifact.Chunk(t0(), *c2, e0())
      .RectDrawing(gfx::Rect(0, 0, 640, 480), Color::kBlack);
  Update(artifact.Build());

  // We can't merge the two chunks because their clips have unmergeable
  // transforms.
  ASSERT_EQ(2u, LayerCount());
}

TEST_P(PaintArtifactCompositorTest, SiblingTransformsWithAlias) {
  auto* real_common_transform =
      CreateTransform(t0(), MakeTranslationMatrix(5, 6));
  auto* common_transform =
      TransformPaintPropertyNodeAlias::Create(*real_common_transform);
  auto* real_transform1 =
      CreateTransform(*common_transform, MakeScaleMatrix(2));
  auto* transform1 = TransformPaintPropertyNodeAlias::Create(*real_transform1);
  auto* real_transform2 =
      CreateTransform(*common_transform, MakeScaleMatrix(0.5));
  auto* transform2 = TransformPaintPropertyNodeAlias::Create(*real_transform2);

  TestPaintArtifact artifact;
  artifact.Chunk(*transform1, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 111, 222), Color::kWhite);
  artifact.Chunk(*transform2, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 333, 444), Color::kBlack);
  Update(artifact.Build());

  // The two chunks are merged together.
  ASSERT_EQ(1u, LayerCount());
  const cc::Layer* layer = LayerAt(0);
  EXPECT_THAT(
      layer->GetPicture(),
      Pointee(DrawsRectangles(Vector<RectWithColor>{
          RectWithColor(gfx::RectF(0, 0, 222, 444), Color::kWhite),
          RectWithColor(gfx::RectF(0, 0, 166.5, 222), Color::kBlack)})));
  gfx::Transform expected_transform;
  expected_transform.Translate(5, 6);
  EXPECT_EQ(expected_transform, layer->ScreenSpaceTransform());
}

TEST_P(PaintArtifactCompositorTest, SiblingTransformsWithComposited) {
  auto* t1 = CreateTransform(t0(), gfx::Transform(), gfx::Point3F(),
                             CompositingReason::kWillChangeTransform);
  auto* t2 = CreateTransform(*t1, MakeTranslationMatrix(1, 2));
  auto* t3 = CreateTransform(t0(), MakeTranslationMatrix(3, 4));

  TestPaintArtifact artifact;
  artifact.Chunk(*t2, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 640, 480), Color::kWhite);
  artifact.Chunk(*t3, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 640, 480), Color::kBlack);
  Update(artifact.Build());

  // We can't merge the two chunks because their transforms are not mergeable.
  ASSERT_EQ(2u, LayerCount());
}

TEST_P(PaintArtifactCompositorTest, ForeignLayerPassesThrough) {
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  layer->SetIsDrawable(true);
  layer->SetBounds(gfx::Size(400, 300));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk().ForeignLayer(layer, gfx::Point(50, 60));
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);

  ASSERT_EQ(3u, LayerCount());
  EXPECT_EQ(layer, LayerAt(1));
  EXPECT_EQ(gfx::Size(400, 300), layer->bounds());
  EXPECT_EQ(Translation(50, 60), layer->ScreenSpaceTransform());
}

TEST_P(PaintArtifactCompositorTest, EffectTreeConversionWithAlias) {
  Update(TestPaintArtifact()
             .Chunk()
             .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite)
             .Build());
  auto root_element_id = GetPropertyTrees().effect_tree().Node(1)->element_id;

  auto* real_effect1 =
      CreateOpacityEffect(e0(), t0(), &c0(), 0.5, CompositingReason::kAll);
  auto* effect1 = EffectPaintPropertyNodeAlias::Create(*real_effect1);
  auto* real_effect2 =
      CreateOpacityEffect(*effect1, 0.3, CompositingReason::kAll);
  auto* effect2 = EffectPaintPropertyNodeAlias::Create(*real_effect2);
  auto* real_effect3 = CreateOpacityEffect(e0(), 0.2, CompositingReason::kAll);
  auto* effect3 = EffectPaintPropertyNodeAlias::Create(*real_effect3);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *effect2)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  artifact.Chunk(t0(), c0(), *effect1)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  artifact.Chunk(t0(), c0(), *effect3)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  Update(artifact.Build());

  ASSERT_EQ(3u, LayerCount());

  const cc::EffectTree& effect_tree = GetPropertyTrees().effect_tree();
  // Node #0 reserved for null; #1 for root render surface; #2 for e0(),
  // plus 3 nodes for those created by this test.
  ASSERT_EQ(5u, effect_tree.size());

  const cc::EffectNode& converted_root_effect = *effect_tree.Node(1);
  EXPECT_EQ(-1, converted_root_effect.parent_id);
  EXPECT_EQ(root_element_id, converted_root_effect.element_id);

  const cc::EffectNode& converted_effect1 = *effect_tree.Node(2);
  EXPECT_EQ(converted_root_effect.id, converted_effect1.parent_id);
  EXPECT_FLOAT_EQ(0.5, converted_effect1.opacity);
  EXPECT_EQ(real_effect1->GetCompositorElementId(),
            converted_effect1.element_id);

  const cc::EffectNode& converted_effect2 = *effect_tree.Node(3);
  EXPECT_EQ(converted_effect1.id, converted_effect2.parent_id);
  EXPECT_FLOAT_EQ(0.3, converted_effect2.opacity);

  const cc::EffectNode& converted_effect3 = *effect_tree.Node(4);
  EXPECT_EQ(converted_root_effect.id, converted_effect3.parent_id);
  EXPECT_FLOAT_EQ(0.2, converted_effect3.opacity);

  EXPECT_EQ(converted_effect2.id, LayerAt(0)->effect_tree_index());
  EXPECT_EQ(converted_effect1.id, LayerAt(1)->effect_tree_index());
  EXPECT_EQ(converted_effect3.id, LayerAt(2)->effect_tree_index());
}

// Returns a RefCountedPropertyTreeState for composited scrolling paint
// properties with some arbitrary values.
static PropertyTreeState ScrollState1(
    const PropertyTreeState& parent_state = PropertyTreeState::Root(),
    CompositingReasons compositing_reasons =
        CompositingReason::kOverflowScrolling,
    MainThreadScrollingReasons main_thread_reasons = kNotScrollingOnMain) {
  return CreateScrollTranslationState(
      parent_state, 7, 9, gfx::Rect(3, 5, 11, 13), gfx::Size(27, 31),
      compositing_reasons, main_thread_reasons);
}

// Returns a RefCountedPropertyTreeState for composited scrolling paint
// properties with another set of arbitrary values.
static PropertyTreeState ScrollState2(
    const PropertyTreeState& parent_state = PropertyTreeState::Root(),
    CompositingReasons compositing_reasons =
        CompositingReason::kOverflowScrolling,
    MainThreadScrollingReasons main_thread_reasons = kNotScrollingOnMain) {
  return CreateScrollTranslationState(
      parent_state, 39, 31, gfx::Rect(0, 0, 19, 23), gfx::Size(27, 31),
      compositing_reasons, main_thread_reasons);
}

static void CheckCcScrollNode(const ScrollPaintPropertyNode& blink_scroll,
                              const cc::ScrollNode& cc_scroll) {
  EXPECT_EQ(blink_scroll.ContainerRect().size(), cc_scroll.container_bounds);
  EXPECT_EQ(blink_scroll.ContentsRect().size(), cc_scroll.bounds);
  EXPECT_EQ(blink_scroll.UserScrollableHorizontal(),
            cc_scroll.user_scrollable_horizontal);
  EXPECT_EQ(blink_scroll.UserScrollableVertical(),
            cc_scroll.user_scrollable_vertical);
  EXPECT_EQ(blink_scroll.GetCompositorElementId(), cc_scroll.element_id);
  EXPECT_EQ(blink_scroll.GetMainThreadScrollingReasons(),
            cc_scroll.main_thread_scrolling_reasons);
}

TEST_P(PaintArtifactCompositorTest, OneScrollNodeComposited) {
  auto scroll_state = ScrollState1();
  auto& scroll = *scroll_state.Transform().ScrollNode();

  // Scroll node ElementIds are referenced by scroll animations.
  Update(TestPaintArtifact()
             .ScrollHitTestChunk(scroll_state)
             .Chunk(scroll_state)
             .RectDrawing(gfx::Rect(-110, 12, 170, 19), Color::kWhite)
             .Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree();
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(3u, scroll_tree.size());
  const cc::ScrollNode& scroll_node = *scroll_tree.Node(2);
  CheckCcScrollNode(scroll, scroll_node);
  EXPECT_EQ(1, scroll_node.parent_id);
  EXPECT_EQ(scroll_node.element_id, ScrollHitTestLayerAt(0)->element_id());
  EXPECT_EQ(scroll_node.id, ElementIdToScrollNodeIndex(scroll_node.element_id));

  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree();
  const cc::TransformNode& transform_node =
      *transform_tree.Node(scroll_node.transform_id);
  EXPECT_TRUE(transform_node.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-7, -9), transform_node.scroll_offset);
  EXPECT_EQ(kNotScrollingOnMain, scroll_node.main_thread_scrolling_reasons);

  auto* layer = NonScrollHitTestLayerAt(0);
  auto transform_node_index = layer->transform_tree_index();
  EXPECT_EQ(transform_node_index, transform_node.id);
  auto scroll_node_index = layer->scroll_tree_index();
  EXPECT_EQ(scroll_node_index, scroll_node.id);

  // The scrolling contents layer is clipped to the scrolling range.
  EXPECT_EQ(gfx::Size(27, 19), layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(3, 12), layer->offset_to_transform_parent());
  EXPECT_THAT(layer->GetPicture(),
              Pointee(DrawsRectangle(gfx::RectF(0, 0, 57, 19), Color::kWhite)));

  auto* scroll_layer = ScrollHitTestLayerAt(0);
  // The scroll layer should be sized to the container bounds.
  // TODO(pdr): The container bounds will not include scrollbars but the scroll
  // layer should extend below scrollbars.
  EXPECT_EQ(gfx::Size(11, 13), scroll_layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(3, 5), scroll_layer->offset_to_transform_parent());
  EXPECT_EQ(scroll_layer->scroll_tree_index(), scroll_node.id);

  std::optional<cc::TargetSnapAreaElementIds> targets;
  EXPECT_CALL(
      ScrollCallbacks(),
      DidCompositorScroll(scroll_node.element_id, gfx::PointF(1, 2), targets));
  GetPropertyTrees().scroll_tree_mutable().NotifyDidCompositorScroll(
      scroll_node.element_id, gfx::PointF(1, 2), targets);

  EXPECT_CALL(ScrollCallbacks(),
              DidChangeScrollbarsHidden(scroll_node.element_id, true));
  GetPropertyTrees().scroll_tree_mutable().NotifyDidChangeScrollbarsHidden(
      scroll_node.element_id, true);
}

TEST_P(PaintArtifactCompositorTest, OneScrollNodeNonComposited) {
  auto scroll_state =
      ScrollState1(PropertyTreeState::Root(), CompositingReason::kNone);
  Update(TestPaintArtifact().ScrollChunks(scroll_state).Build());
  // Non-composited scrollers still create cc transform and scroll nodes.
  EXPECT_EQ(3u, GetPropertyTrees().scroll_tree().size());
  EXPECT_EQ(3u, GetPropertyTrees().transform_tree().size());
  EXPECT_EQ(1u, LayerCount());
}

TEST_P(PaintArtifactCompositorTest, TransformUnderScrollNode) {
  auto scroll_state = ScrollState1();
  auto* transform =
      CreateTransform(scroll_state.Transform(), gfx::Transform(),
                      gfx::Point3F(), CompositingReason::kWillChangeTransform);

  TestPaintArtifact artifact;
  artifact.Chunk(scroll_state)
      .RectDrawing(gfx::Rect(-20, 4, 60, 8), Color::kBlack)
      .Chunk(*transform, c0(), e0())
      .RectDrawing(gfx::Rect(1, -30, 5, 70), Color::kWhite);
  Update(artifact.Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree();
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(3u, scroll_tree.size());
  const cc::ScrollNode& scroll_node = *scroll_tree.Node(2);

  // Both layers should refer to the same scroll tree node.
  const auto* layer0 = LayerAt(0);
  const auto* layer1 = LayerAt(1);
  EXPECT_EQ(scroll_node.id, layer0->scroll_tree_index());
  EXPECT_EQ(scroll_node.id, layer1->scroll_tree_index());

  // The scrolling layer is clipped to the scrollable range.
  EXPECT_EQ(gfx::Vector2dF(3, 5), layer0->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(27, 7), layer0->bounds());
  EXPECT_THAT(layer0->GetPicture(),
              Pointee(DrawsRectangle(gfx::RectF(0, 0, 37, 7), Color::kBlack)));

  // The layer under the transform without a scroll node is not clipped.
  EXPECT_EQ(gfx::Vector2dF(1, -30), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(5, 70), layer1->bounds());
  EXPECT_THAT(layer1->GetPicture(),
              Pointee(DrawsRectangle(gfx::RectF(0, 0, 5, 70), Color::kWhite)));

  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree();
  const cc::TransformNode& scroll_transform_node =
      *transform_tree.Node(scroll_node.transform_id);
  // The layers have different transform nodes.
  EXPECT_EQ(scroll_transform_node.id, layer0->transform_tree_index());
  EXPECT_NE(scroll_transform_node.id, layer1->transform_tree_index());
}

TEST_P(PaintArtifactCompositorTest, NestedScrollNodes) {
  auto* effect = CreateOpacityEffect(e0(), 0.5);

  auto scroll_state_a = ScrollState1(
      PropertyTreeState(t0(), c0(), *effect),
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
  auto& scroll_a = *scroll_state_a.Transform().ScrollNode();
  auto scroll_state_b = ScrollState2(scroll_state_a);
  auto& scroll_b = *scroll_state_b.Transform().ScrollNode();

  Update(TestPaintArtifact()
             .ScrollChunks(scroll_state_a)
             .ScrollChunks(scroll_state_b)
             .Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree();
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(4u, scroll_tree.size());
  const cc::ScrollNode& scroll_node_a = *scroll_tree.Node(2);
  CheckCcScrollNode(scroll_a, scroll_node_a);
  EXPECT_EQ(1, scroll_node_a.parent_id);
  EXPECT_EQ(scroll_node_a.element_id, ScrollHitTestLayerAt(0)->element_id());
  EXPECT_EQ(scroll_node_a.id,
            ElementIdToScrollNodeIndex(scroll_node_a.element_id));

  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree();
  const cc::TransformNode& transform_node_a =
      *transform_tree.Node(scroll_node_a.transform_id);
  EXPECT_TRUE(transform_node_a.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-7, -9), transform_node_a.scroll_offset);

  const cc::ScrollNode& scroll_node_b = *scroll_tree.Node(3);
  CheckCcScrollNode(scroll_b, scroll_node_b);
  EXPECT_EQ(scroll_node_a.id, scroll_node_b.parent_id);
  EXPECT_EQ(scroll_node_b.element_id, ScrollHitTestLayerAt(1)->element_id());
  EXPECT_EQ(scroll_node_b.id,
            ElementIdToScrollNodeIndex(scroll_node_b.element_id));

  const cc::TransformNode& transform_node_b =
      *transform_tree.Node(scroll_node_b.transform_id);
  EXPECT_TRUE(transform_node_b.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-39, -31), transform_node_b.scroll_offset);
}

TEST_P(PaintArtifactCompositorTest, ScrollHitTestLayerOrder) {
  auto scroll_state = ScrollState1();
  auto& scroll = *scroll_state.Transform().ScrollNode();

  auto* transform =
      CreateTransform(scroll_state.Transform(), MakeTranslationMatrix(5, 5),
                      gfx::Point3F(), CompositingReason::k3DTransform);

  Update(TestPaintArtifact()
             .Chunk(scroll_state)
             .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite)
             .ScrollHitTestChunk(scroll_state)
             .Chunk(*transform, scroll_state.Clip(), scroll_state.Effect())
             .RectDrawing(gfx::Rect(0, 0, 50, 50), Color::kBlack)
             .Build());

  // The first content layer (background) should not have the scrolling element
  // id set.
  EXPECT_EQ(CompositorElementId(), NonScrollHitTestLayerAt(0)->element_id());

  // The scroll layer should be after the first content layer (background).
  EXPECT_LT(LayerIndex(NonScrollHitTestLayerAt(0)),
            LayerIndex(ScrollHitTestLayerAt(0)));
  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree();
  auto* scroll_node =
      scroll_tree.Node(ScrollHitTestLayerAt(0)->scroll_tree_index());
  ASSERT_EQ(scroll.GetCompositorElementId(), scroll_node->element_id);
  EXPECT_EQ(scroll.GetCompositorElementId(),
            ScrollHitTestLayerAt(0)->element_id());
  EXPECT_EQ(RuntimeEnabledFeatures::HitTestOpaquenessEnabled()
                ? cc::HitTestOpaqueness::kOpaque
                : cc::HitTestOpaqueness::kMixed,
            ScrollHitTestLayerAt(0)->hit_test_opaqueness());

  // The second content layer should appear after the first.
  EXPECT_LT(LayerIndex(ScrollHitTestLayerAt(0)),
            LayerIndex(NonScrollHitTestLayerAt(1)));
  EXPECT_EQ(CompositorElementId(), NonScrollHitTestLayerAt(1)->element_id());
}

TEST_P(PaintArtifactCompositorTest, NestedScrollableLayerOrder) {
  auto scroll_state_1 = ScrollState1();
  auto& scroll_1 = *scroll_state_1.Transform().ScrollNode();
  auto scroll_state_2 = ScrollState2(scroll_state_1);
  auto& scroll_2 = *scroll_state_2.Transform().ScrollNode();

  Update(TestPaintArtifact()
             .ScrollHitTestChunk(scroll_state_1)
             .ScrollHitTestChunk(scroll_state_2)
             .Chunk(scroll_state_2)
             .RectDrawing(gfx::Rect(0, 0, 50, 50), Color::kWhite)
             .Build());

  // Two scroll layers should be created for each scroll translation node.
  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree();
  const cc::ClipTree& clip_tree = GetPropertyTrees().clip_tree();
  auto* scroll_1_node =
      scroll_tree.Node(ScrollHitTestLayerAt(0)->scroll_tree_index());
  ASSERT_EQ(scroll_1.GetCompositorElementId(), scroll_1_node->element_id);
  auto* scroll_1_clip_node =
      clip_tree.Node(ScrollHitTestLayerAt(0)->clip_tree_index());
  // The scroll is not under clip_1.
  EXPECT_EQ(gfx::RectF(0, 0, 0, 0), scroll_1_clip_node->clip);

  auto* scroll_2_node =
      scroll_tree.Node(ScrollHitTestLayerAt(1)->scroll_tree_index());
  ASSERT_EQ(scroll_2.GetCompositorElementId(), scroll_2_node->element_id);
  auto* scroll_2_clip_node =
      clip_tree.Node(ScrollHitTestLayerAt(1)->clip_tree_index());
  // The scroll is not under clip_2 but is under the parent clip, clip_1.
  EXPECT_EQ(gfx::RectF(3, 5, 11, 13), scroll_2_clip_node->clip);

  // The first layer should be before the second scroll layer.
  EXPECT_LT(LayerIndex(ScrollHitTestLayerAt(0)),
            LayerIndex(ScrollHitTestLayerAt(1)));

  // The non-scrollable content layer should be after the second scroll layer.
  EXPECT_LT(LayerIndex(ScrollHitTestLayerAt(1)),
            LayerIndex(NonScrollHitTestLayerAt(0)));

  auto expected_hit_test_opaqueness =
      RuntimeEnabledFeatures::HitTestOpaquenessEnabled()
          ? cc::HitTestOpaqueness::kOpaque
          : cc::HitTestOpaqueness::kMixed;
  EXPECT_EQ(expected_hit_test_opaqueness,
            ScrollHitTestLayerAt(0)->hit_test_opaqueness());
  EXPECT_EQ(expected_hit_test_opaqueness,
            ScrollHitTestLayerAt(1)->hit_test_opaqueness());
}

TEST_P(PaintArtifactCompositorTest, AncestorScrollNodes) {
  auto scroll_state_a = ScrollState1();
  auto& scroll_a = *scroll_state_a.Transform().ScrollNode();
  auto scroll_state_b =
      ScrollState2(scroll_state_a, CompositingReason::kNone,
                   cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
  auto& scroll_b = *scroll_state_b.Transform().ScrollNode();

  Update(TestPaintArtifact()
             .ScrollChunks(scroll_state_a)
             .ScrollChunks(scroll_state_b)
             .Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree();
  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree();
  // Node #0 reserved for null; #1 for root render surface. #2 is for scroll_a.

  // Non-composited scrollers still create transform and scroll nodes.
  ASSERT_EQ(4u, scroll_tree.size());
  ASSERT_EQ(4u, transform_tree.size());

  const cc::ScrollNode& scroll_node_a = *scroll_tree.Node(2);
  EXPECT_EQ(1, scroll_node_a.parent_id);
  EXPECT_EQ(scroll_a.GetCompositorElementId(), scroll_node_a.element_id);
  EXPECT_EQ(scroll_node_a.id,
            ElementIdToScrollNodeIndex(scroll_node_a.element_id));
  // The first scrollable layer should be associated with scroll_a.
  EXPECT_EQ(scroll_node_a.element_id, ScrollHitTestLayerAt(0)->element_id());
  EXPECT_TRUE(scroll_node_a.is_composited);

  const cc::TransformNode& transform_node_a =
      *transform_tree.Node(scroll_node_a.transform_id);
  EXPECT_TRUE(transform_node_a.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-7, -9), transform_node_a.scroll_offset);
  EXPECT_EQ(gfx::PointF(-7, -9),
            scroll_tree.current_scroll_offset(scroll_node_a.element_id));

  const cc::ScrollNode& scroll_node_b = *scroll_tree.Node(3);
  EXPECT_EQ(scroll_node_a.id, scroll_node_b.parent_id);
  EXPECT_EQ(scroll_b.GetCompositorElementId(), scroll_node_b.element_id);
  EXPECT_EQ(scroll_node_b.id,
            ElementIdToScrollNodeIndex(scroll_node_b.element_id));
  EXPECT_FALSE(scroll_node_b.is_composited);

  const cc::TransformNode& transform_node_b =
      *transform_tree.Node(scroll_node_b.transform_id);
  EXPECT_TRUE(transform_node_b.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-39, -31), transform_node_b.scroll_offset);
  EXPECT_EQ(gfx::PointF(-39, -31),
            scroll_tree.current_scroll_offset(scroll_node_b.element_id));
}

TEST_P(PaintArtifactCompositorTest, AncestorNonCompositedScrollNode) {
  auto scroll_state_a =
      ScrollState1(PropertyTreeState::Root(), CompositingReason::kNone,
                   cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText);
  auto& scroll_a = *scroll_state_a.Transform().ScrollNode();
  auto scroll_state_b = ScrollState2(scroll_state_a);
  auto& scroll_b = *scroll_state_b.Transform().ScrollNode();

  Update(TestPaintArtifact()
             .ScrollChunks(scroll_state_a)
             .ScrollChunks(scroll_state_b)
             .Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree();
  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree();
  // Node #0 reserved for null; #1 for root render surface. #2 is for scroll_a
  // #3 for scroll_b.
  ASSERT_EQ(4u, scroll_tree.size());
  ASSERT_EQ(4u, transform_tree.size());

  const cc::ScrollNode& scroll_node_a = *scroll_tree.Node(2);
  EXPECT_EQ(1, scroll_node_a.parent_id);
  EXPECT_EQ(scroll_a.GetCompositorElementId(), scroll_node_a.element_id);
  EXPECT_EQ(scroll_node_a.id,
            ElementIdToScrollNodeIndex(scroll_node_a.element_id));
  EXPECT_FALSE(scroll_node_a.is_composited);

  const cc::TransformNode& transform_node_a =
      *transform_tree.Node(scroll_node_a.transform_id);
  EXPECT_TRUE(transform_node_a.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-7, -9), transform_node_a.scroll_offset);
  EXPECT_EQ(gfx::PointF(-7, -9),
            scroll_tree.current_scroll_offset(scroll_node_a.element_id));

  const cc::ScrollNode& scroll_node_b = *scroll_tree.Node(3);
  EXPECT_EQ(scroll_node_a.id, scroll_node_b.parent_id);
  EXPECT_EQ(scroll_b.GetCompositorElementId(), scroll_node_b.element_id);
  EXPECT_EQ(scroll_node_b.id,
            ElementIdToScrollNodeIndex(scroll_node_b.element_id));
  // The first scrollable layer should be associated with scroll_b.
  EXPECT_EQ(scroll_node_b.element_id, ScrollHitTestLayerAt(0)->element_id());
  EXPECT_TRUE(scroll_node_b.is_composited);

  const cc::TransformNode& transform_node_b =
      *transform_tree.Node(scroll_node_b.transform_id);
  EXPECT_TRUE(transform_node_b.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-39, -31), transform_node_b.scroll_offset);
  EXPECT_EQ(gfx::PointF(-39, -31),
            scroll_tree.current_scroll_offset(scroll_node_b.element_id));
}

// If a scroll node is encountered before its parent, ensure the parent scroll
// node is correctly created.
TEST_P(PaintArtifactCompositorTest, AncestorScrollNodesInversedOrder) {
  auto scroll_state_a = ScrollState1();
  auto& scroll_a = *scroll_state_a.Transform().ScrollNode();
  auto scroll_state_b = ScrollState2(scroll_state_a);
  auto& scroll_b = *scroll_state_b.Transform().ScrollNode();

  Update(TestPaintArtifact()
             .ScrollChunks(scroll_state_b)
             .ScrollChunks(scroll_state_a)
             .Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree();
  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree();
  // Node #0 reserved for null; #1 for root render surface. #2 is for scroll_a.
  // #3 is for scroll_b.
  ASSERT_EQ(4u, scroll_tree.size());
  ASSERT_EQ(4u, transform_tree.size());

  const cc::ScrollNode& scroll_node_a = *scroll_tree.Node(2);
  EXPECT_EQ(1, scroll_node_a.parent_id);
  EXPECT_EQ(scroll_a.GetCompositorElementId(), scroll_node_a.element_id);
  EXPECT_EQ(scroll_node_a.id,
            ElementIdToScrollNodeIndex(scroll_node_a.element_id));
  // The second scrollable layer should be associated with scroll_a.
  EXPECT_EQ(scroll_node_a.element_id, ScrollHitTestLayerAt(1)->element_id());
  EXPECT_TRUE(scroll_node_a.is_composited);

  const cc::TransformNode& transform_node_a =
      *transform_tree.Node(scroll_node_a.transform_id);
  EXPECT_TRUE(transform_node_a.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-7, -9), transform_node_a.scroll_offset);
  EXPECT_EQ(gfx::PointF(-7, -9),
            scroll_tree.current_scroll_offset(scroll_node_a.element_id));

  const cc::ScrollNode& scroll_node_b = *scroll_tree.Node(3);
  EXPECT_EQ(scroll_node_a.id, scroll_node_b.parent_id);
  EXPECT_EQ(scroll_b.GetCompositorElementId(), scroll_node_b.element_id);
  EXPECT_EQ(scroll_node_b.id,
            ElementIdToScrollNodeIndex(scroll_node_b.element_id));
  // The first scrollable layer should be associated with scroll_b.
  EXPECT_EQ(scroll_node_b.element_id, ScrollHitTestLayerAt(0)->element_id());
  EXPECT_TRUE(scroll_node_b.is_composited);

  const cc::TransformNode& transform_node_b =
      *transform_tree.Node(scroll_node_b.transform_id);
  EXPECT_TRUE(transform_node_b.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-39, -31), transform_node_b.scroll_offset);
  EXPECT_EQ(gfx::PointF(-39, -31),
            scroll_tree.current_scroll_offset(scroll_node_b.element_id));
}

TEST_P(PaintArtifactCompositorTest,
       DifferentTransformTreeAndScrollTreeHierarchy) {
  auto scroll_state_a = ScrollState1();
  auto& scroll_a = *scroll_state_a.Transform().ScrollNode();
  auto scroll_state_b = ScrollState2(scroll_state_a);
  auto& scroll_b = *scroll_state_b.Transform().ScrollNode();
  // scroll_state_c's has root transform space, while the scroll parent is
  // scroll_b.
  auto scroll_state_c = CreateCompositedScrollTranslationState(
      PropertyTreeState::Root(), scroll_b, 11, 22, gfx::Rect(0, 0, 10, 20),
      gfx::Size(50, 60));
  auto& scroll_c = *scroll_state_c.Transform().ScrollNode();

  Update(TestPaintArtifact()
             .ScrollChunks(scroll_state_a)
             .ScrollChunks(scroll_state_b)
             .ScrollChunks(scroll_state_c)
             .Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree();
  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree();
  // Node #0 reserved for null; #1 for root render surface. #2 is for scroll_a.
  // #3 is for scroll_b. #4 is for scroll_c.
  ASSERT_EQ(5u, scroll_tree.size());
  ASSERT_EQ(5u, transform_tree.size());

  const cc::ScrollNode& scroll_node_a = *scroll_tree.Node(2);
  EXPECT_EQ(1, scroll_node_a.parent_id);
  EXPECT_EQ(scroll_a.GetCompositorElementId(), scroll_node_a.element_id);
  EXPECT_EQ(scroll_node_a.id,
            ElementIdToScrollNodeIndex(scroll_node_a.element_id));
  EXPECT_EQ(scroll_node_a.element_id, ScrollHitTestLayerAt(0)->element_id());

  const cc::TransformNode& transform_node_a =
      *transform_tree.Node(scroll_node_a.transform_id);
  EXPECT_TRUE(transform_node_a.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-7, -9), transform_node_a.scroll_offset);
  EXPECT_EQ(gfx::PointF(-7, -9),
            scroll_tree.current_scroll_offset(scroll_node_a.element_id));

  const cc::ScrollNode& scroll_node_b = *scroll_tree.Node(3);
  EXPECT_EQ(scroll_node_a.id, scroll_node_b.parent_id);
  EXPECT_EQ(scroll_b.GetCompositorElementId(), scroll_node_b.element_id);
  EXPECT_EQ(scroll_node_b.id,
            ElementIdToScrollNodeIndex(scroll_node_b.element_id));
  EXPECT_EQ(scroll_node_b.element_id, ScrollHitTestLayerAt(1)->element_id());

  const cc::TransformNode& transform_node_b =
      *transform_tree.Node(scroll_node_b.transform_id);
  EXPECT_TRUE(transform_node_b.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-39, -31), transform_node_b.scroll_offset);
  EXPECT_EQ(gfx::PointF(-39, -31),
            scroll_tree.current_scroll_offset(scroll_node_b.element_id));

  const cc::ScrollNode& scroll_node_c = *scroll_tree.Node(4);
  EXPECT_EQ(scroll_node_b.id, scroll_node_c.parent_id);
  EXPECT_EQ(scroll_c.GetCompositorElementId(), scroll_node_c.element_id);
  EXPECT_EQ(scroll_node_c.id,
            ElementIdToScrollNodeIndex(scroll_node_c.element_id));
  EXPECT_EQ(scroll_node_c.element_id, ScrollHitTestLayerAt(2)->element_id());

  const cc::TransformNode& transform_node_c =
      *transform_tree.Node(scroll_node_c.transform_id);
  EXPECT_EQ(1, transform_node_c.parent_id);
  EXPECT_TRUE(transform_node_c.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-11, -22), transform_node_c.scroll_offset);
  EXPECT_EQ(gfx::PointF(-11, -22),
            scroll_tree.current_scroll_offset(scroll_node_c.element_id));
}

TEST_P(PaintArtifactCompositorTest,
       DifferentTransformTreeAndScrollTreeHierarchyInversedOrder) {
  auto scroll_state_a = ScrollState1();
  auto& scroll_a = *scroll_state_a.Transform().ScrollNode();
  auto scroll_state_b = ScrollState2(scroll_state_a);
  auto& scroll_b = *scroll_state_b.Transform().ScrollNode();
  // scroll_state_c's has root transform space, while the scroll parent is
  // scroll_b.
  auto scroll_state_c = CreateCompositedScrollTranslationState(
      PropertyTreeState::Root(), scroll_b, 11, 22, gfx::Rect(0, 0, 10, 20),
      gfx::Size(50, 60));
  auto& scroll_c = *scroll_state_c.Transform().ScrollNode();

  Update(TestPaintArtifact()
             .ScrollChunks(scroll_state_c)
             .ScrollChunks(scroll_state_b)
             .ScrollChunks(scroll_state_a)
             .Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree();
  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree();
  // Node #0 reserved for null; #1 for root render surface. #2 is for scroll_a.
  // #3 is for scroll_b. #4 is for scroll_c.
  ASSERT_EQ(5u, scroll_tree.size());
  ASSERT_EQ(5u, transform_tree.size());

  const cc::ScrollNode& scroll_node_a = *scroll_tree.Node(2);
  EXPECT_EQ(1, scroll_node_a.parent_id);
  EXPECT_EQ(scroll_a.GetCompositorElementId(), scroll_node_a.element_id);
  EXPECT_EQ(scroll_node_a.id,
            ElementIdToScrollNodeIndex(scroll_node_a.element_id));
  EXPECT_EQ(scroll_node_a.element_id, ScrollHitTestLayerAt(2)->element_id());

  const cc::TransformNode& transform_node_a =
      *transform_tree.Node(scroll_node_a.transform_id);
  EXPECT_TRUE(transform_node_a.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-7, -9), transform_node_a.scroll_offset);
  EXPECT_EQ(gfx::PointF(-7, -9),
            scroll_tree.current_scroll_offset(scroll_node_a.element_id));

  const cc::ScrollNode& scroll_node_b = *scroll_tree.Node(3);
  EXPECT_EQ(scroll_node_a.id, scroll_node_b.parent_id);
  EXPECT_EQ(scroll_b.GetCompositorElementId(), scroll_node_b.element_id);
  EXPECT_EQ(scroll_node_b.id,
            ElementIdToScrollNodeIndex(scroll_node_b.element_id));
  EXPECT_EQ(scroll_node_b.element_id, ScrollHitTestLayerAt(1)->element_id());

  const cc::TransformNode& transform_node_b =
      *transform_tree.Node(scroll_node_b.transform_id);
  EXPECT_TRUE(transform_node_b.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-39, -31), transform_node_b.scroll_offset);
  EXPECT_EQ(gfx::PointF(-39, -31),
            scroll_tree.current_scroll_offset(scroll_node_b.element_id));

  const cc::ScrollNode& scroll_node_c = *scroll_tree.Node(4);
  EXPECT_EQ(scroll_node_b.id, scroll_node_c.parent_id);
  EXPECT_EQ(scroll_c.GetCompositorElementId(), scroll_node_c.element_id);
  EXPECT_EQ(scroll_node_c.id,
            ElementIdToScrollNodeIndex(scroll_node_c.element_id));
  EXPECT_EQ(scroll_node_c.element_id, ScrollHitTestLayerAt(0)->element_id());

  const cc::TransformNode& transform_node_c =
      *transform_tree.Node(scroll_node_c.transform_id);
  EXPECT_EQ(1, transform_node_c.parent_id);
  EXPECT_TRUE(transform_node_c.local.IsIdentity());
  EXPECT_EQ(gfx::PointF(-11, -22), transform_node_c.scroll_offset);
  EXPECT_EQ(gfx::PointF(-11, -22),
            scroll_tree.current_scroll_offset(scroll_node_c.element_id));
}

TEST_P(PaintArtifactCompositorTest, FixedPositionScrollState) {
  auto scroll_state_a = ScrollState1();
  auto& scroll_a = *scroll_state_a.Transform().ScrollNode();
  auto* fixed_transform = CreateFixedPositionTranslation(
      t0(), 100, 200, scroll_state_a.Transform());
  PropertyTreeState fixed_state(*fixed_transform, scroll_state_a.Clip(),
                                scroll_state_a.Effect());
  // scroll_state_b's has fixed transform space, while the scroll parent is
  // scroll_a.
  auto scroll_state_b = CreateCompositedScrollTranslationState(
      fixed_state, scroll_a, 11, 22, gfx::Rect(10, 20), gfx::Size(50, 60));
  auto& scroll_b = *scroll_state_b.Transform().ScrollNode();

  Update(TestPaintArtifact()
             .ScrollHitTestChunk(scroll_state_a)
             .Chunk(fixed_state)
             .RectDrawing(gfx::Rect(50, 100), Color::kBlack)
             .ScrollHitTestChunk(scroll_state_b)
             .Build());

  auto& scroll_tree = GetPropertyTrees().scroll_tree();
  auto& transform_tree = GetPropertyTrees().transform_tree();
  // Scroll node #0 reserved for null, #1 for root render surface, #2 is for
  // scroll_a, and #3 for scroll_b. Transform ids depend on the order in the
  // painted_scroll_translations_ HashMap so are not deterministic.
  ASSERT_EQ(4u, scroll_tree.size());
  ASSERT_EQ(5u, transform_tree.size());
  ASSERT_EQ(3u, LayerCount());

  auto* scroll_node_a = scroll_tree.Node(2);
  auto* layer_a = LayerAt(0);
  EXPECT_EQ(scroll_a.GetCompositorElementId(), scroll_node_a->element_id);
  EXPECT_EQ(1, scroll_node_a->parent_id);
  EXPECT_EQ(CcNodeId(scroll_state_a.Transform()), scroll_node_a->transform_id);
  EXPECT_EQ(2, layer_a->scroll_tree_index());
  EXPECT_EQ(1, layer_a->transform_tree_index());

  auto* fixed_layer = LayerAt(1);
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_EQ(2, fixed_layer->scroll_tree_index());
  } else {
    EXPECT_EQ(1, fixed_layer->scroll_tree_index());
  }
  EXPECT_EQ(CcNodeId(*fixed_transform), fixed_layer->transform_tree_index());
  auto* fixed_transform_node = transform_tree.Node(3);
  EXPECT_EQ(1, fixed_transform_node->parent_id);

  auto* scroll_node_b = scroll_tree.Node(3);
  auto* layer_b = LayerAt(2);
  EXPECT_EQ(2, scroll_node_b->parent_id);
  EXPECT_EQ(scroll_b.GetCompositorElementId(), scroll_node_b->element_id);
  EXPECT_EQ(CcNodeId(scroll_state_b.Transform()), scroll_node_b->transform_id);
  EXPECT_EQ(3, layer_b->scroll_tree_index());
  EXPECT_EQ(CcNodeId(*fixed_transform), layer_b->transform_tree_index());
}

TEST_P(PaintArtifactCompositorTest, MergeSimpleChunks) {
  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(2u, artifact.GetPaintChunks().size());
  Update(artifact);

  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, MergeClip) {
  auto* clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 20, 50, 60));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), *clip, e0())
      .RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kBlack);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 300, 400), Color::kGray);

  auto& artifact = test_artifact.Build();

  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    // Clip is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(10, 20, 50, 60), Color::kBlack));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 300, 400), Color::kGray));

    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, Merge2DTransform) {
  auto* transform = CreateTransform(t0(), MakeTranslationMatrix(50, 50),
                                    gfx::Point3F(100, 100, 0));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);

  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(50, 50, 100, 100), Color::kBlack));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, Merge2DTransformDirectAncestor) {
  auto* transform = CreateTransform(t0(), gfx::Transform(), gfx::Point3F(),
                                    CompositingReason::k3DTransform);
  auto* transform2 = CreateTransform(*transform, MakeTranslationMatrix(50, 50),
                                     gfx::Point3F(100, 100, 0));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  // The second chunk can merge into the first because it has a descendant
  // state of the first's transform and no direct compositing reason.
  test_artifact.Chunk(*transform2, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(2u, artifact.GetPaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(50, 50, 100, 100), Color::kBlack));

    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, MergeTransformOrigin) {
  auto* transform =
      CreateTransform(t0(), MakeRotationMatrix(45), gfx::Point3F(100, 100, 0));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 42, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(RectWithColor(
        gfx::RectF(29.2893, 0.578644, 141.421, 141.421), Color::kBlack));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 42, 200, 300), Color::kGray));

    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, MergeOpacity) {
  float opacity = 2.0 / 255.0;
  auto* effect = CreateOpacityEffect(e0(), opacity);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    Color semi_transparent_black = Color::FromSkColor4f({0, 0, 0, opacity});
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), semi_transparent_black));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, MergeOpacityWithAlias) {
  float opacity = 2.0 / 255.0;
  auto* real_effect = CreateOpacityEffect(e0(), opacity);
  auto* effect = EffectPaintPropertyNodeAlias::Create(*real_effect);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    Color semi_transparent_black = Color::FromSkColor4f({0, 0, 0, opacity});
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), semi_transparent_black));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, MergeNestedWithAlias) {
  // Tests merging of an opacity effect, inside of a clip, inside of a
  // transform.
  auto* real_transform = CreateTransform(t0(), MakeTranslationMatrix(50, 50),
                                         gfx::Point3F(100, 100, 0));
  auto* transform = TransformPaintPropertyNodeAlias::Create(*real_transform);
  auto* real_clip =
      CreateClip(c0(), *transform, FloatRoundedRect(10, 20, 50, 60));
  auto* clip = ClipPaintPropertyNodeAlias::Create(*real_clip);
  float opacity = 2.0 / 255.0;
  auto* real_effect = CreateOpacityEffect(e0(), *transform, clip, opacity);
  auto* effect = EffectPaintPropertyNodeAlias::Create(*real_effect);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(*transform, *clip, *effect)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    Color semi_transparent_black = Color::FromSkColor4f({0, 0, 0, opacity});
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(60, 70, 50, 60), semi_transparent_black));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, ClipPushedUp) {
  // Tests merging of an element which has a clipapplied to it,
  // but has an ancestor transform of them. This can happen for fixed-
  // or absolute-position elements which escape scroll transforms.
  auto* transform = CreateTransform(t0(), MakeTranslationMatrix(20, 25),
                                    gfx::Point3F(100, 100, 0));
  auto* transform2 = CreateTransform(*transform, MakeTranslationMatrix(20, 25),
                                     gfx::Point3F(100, 100, 0));
  auto* clip = CreateClip(c0(), *transform2, FloatRoundedRect(10, 20, 50, 60));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), *clip, e0())
      .RectDrawing(gfx::Rect(0, 0, 300, 400), Color::kBlack);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    // The two transforms (combined translation of (40, 50)) are applied here,
    // before clipping.
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(50, 70, 50, 60), Color(Color::kBlack)));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, EffectPushedUp) {
  // Tests merging of an element which has an effect applied to it,
  // but has an ancestor transform of them. This can happen for fixed-
  // or absolute-position elements which escape scroll transforms.

  auto* transform = CreateTransform(t0(), MakeTranslationMatrix(20, 25),
                                    gfx::Point3F(100, 100, 0));

  auto* transform2 = CreateTransform(*transform, MakeTranslationMatrix(20, 25),
                                     gfx::Point3F(100, 100, 0));

  float opacity = 2.0 / 255.0;
  auto* effect = CreateOpacityEffect(e0(), *transform2, &c0(), opacity);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(gfx::Rect(0, 0, 300, 400), Color::kBlack);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    Color semi_transparent_black = Color::FromSkColor4f({0, 0, 0, opacity});
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 300, 400), semi_transparent_black));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, EffectAndClipPushedUp) {
  // Tests merging of an element which has an effect applied to it,
  // but has an ancestor transform of them. This can happen for fixed-
  // or absolute-position elements which escape scroll transforms.
  auto* transform = CreateTransform(t0(), MakeTranslationMatrix(20, 25),
                                    gfx::Point3F(100, 100, 0));
  auto* transform2 = CreateTransform(*transform, MakeTranslationMatrix(20, 25),
                                     gfx::Point3F(100, 100, 0));
  auto* clip = CreateClip(c0(), *transform, FloatRoundedRect(10, 20, 50, 60));

  float opacity = 2.0 / 255.0;
  auto* effect = CreateOpacityEffect(e0(), *transform2, clip, opacity);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), *clip, *effect)
      .RectDrawing(gfx::Rect(0, 0, 300, 400), Color::kBlack);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    // The clip is under |transform| but not |transform2|, so only an adjustment
    // of (20, 25) occurs.
    Color semi_transparent_black = Color::FromSkColor4f({0, 0, 0, opacity});
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(30, 45, 50, 60), semi_transparent_black));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, ClipAndEffectNoTransform) {
  // Tests merging of an element which has a clip and effect in the root
  // transform space.
  auto* clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 20, 50, 60));
  float opacity = 2.0 / 255.0;
  auto* effect = CreateOpacityEffect(e0(), t0(), clip, opacity);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), *clip, *effect)
      .RectDrawing(gfx::Rect(0, 0, 300, 400), Color::kBlack);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    Color semi_transparent_black = Color::FromSkColor4f({0, 0, 0, opacity});
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(10, 20, 50, 60), semi_transparent_black));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, TwoClips) {
  // Tests merging of an element which has two clips in the root
  // transform space.
  auto* clip = CreateClip(c0(), t0(), FloatRoundedRect(20, 30, 10, 20));
  auto* clip2 = CreateClip(*clip, t0(), FloatRoundedRect(10, 20, 50, 60));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), *clip2, e0())
      .RectDrawing(gfx::Rect(0, 0, 300, 400), Color::kBlack);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    // The interesction of the two clips is (20, 30, 10, 20).
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(20, 30, 10, 20), Color(Color::kBlack)));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, TwoTransformsClipBetween) {
  auto* transform = CreateTransform(t0(), MakeTranslationMatrix(20, 25),
                                    gfx::Point3F(100, 100, 0));
  auto* clip = CreateClip(c0(), t0(), FloatRoundedRect(0, 0, 50, 60));
  auto* transform2 = CreateTransform(*transform, MakeTranslationMatrix(20, 25),
                                     gfx::Point3F(100, 100, 0));
  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(*transform2, *clip, e0())
      .RectDrawing(gfx::Rect(0, 0, 300, 400), Color::kBlack);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, LayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(40, 50, 10, 10), Color(Color::kBlack)));
    rects_with_color.push_back(
        RectWithColor(gfx::RectF(0, 0, 200, 300), Color::kGray));
    const cc::Layer* layer = LayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, OverlapTransform) {
  auto* transform = CreateTransform(t0(), MakeTranslationMatrix(50, 50),
                                    gfx::Point3F(100, 100, 0),
                                    CompositingReason::k3DTransform);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 200, 300), Color::kGray);

  auto& artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact.GetPaintChunks().size());
  Update(artifact);
  // The third paint chunk overlaps the second but can't merge due to
  // incompatible transform. The second paint chunk can't merge into the first
  // due to a direct compositing reason.
  ASSERT_EQ(3u, LayerCount());
}

EffectPaintPropertyNode* CreateSampleEffectNodeWithElementId() {
  EffectPaintPropertyNode::State state;
  state.local_transform_space = &t0();
  state.output_clip = &c0();
  state.opacity = 2.0 / 255.0;
  state.direct_compositing_reasons = CompositingReason::kActiveOpacityAnimation;
  state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
      2, CompositorElementIdNamespace::kPrimaryEffect);
  return EffectPaintPropertyNode::Create(e0(), std::move(state));
}

TransformPaintPropertyNode* CreateSampleTransformNodeWithElementId() {
  TransformPaintPropertyNode::State state{{MakeRotationMatrix(90)}};
  state.direct_compositing_reasons = CompositingReason::k3DTransform;
  state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
      3, CompositorElementIdNamespace::kPrimaryTransform);
  return TransformPaintPropertyNode::Create(t0(), std::move(state));
}

TEST_P(PaintArtifactCompositorTest, TransformWithElementId) {
  auto* transform = CreateSampleTransformNodeWithElementId();
  TestPaintArtifact artifact;
  artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  EXPECT_EQ(2,
            ElementIdToTransformNodeIndex(transform->GetCompositorElementId()));
}

TEST_P(PaintArtifactCompositorTest, EffectWithElementId) {
  auto* effect = CreateSampleEffectNodeWithElementId();
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(gfx::Rect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  EXPECT_EQ(2, ElementIdToEffectNodeIndex(effect->GetCompositorElementId()));
}

TEST_P(PaintArtifactCompositorTest, NonContiguousEffectWithElementId) {
  auto* effect = CreateSampleEffectNodeWithElementId();
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(gfx::Rect(100, 100, 200, 100), Color::kBlack)
      .Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(100, 100, 300, 100), Color::kBlack)
      .Chunk(t0(), c0(), *effect)
      .RectDrawing(gfx::Rect(100, 100, 400, 100), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(3u, LayerCount());
  EXPECT_EQ(2, LayerAt(0)->effect_tree_index());
  cc::ElementId element_id = effect->GetCompositorElementId();
  EXPECT_EQ(element_id, GetPropertyTrees()
                            .effect_tree()
                            .Node(LayerAt(0)->effect_tree_index())
                            ->element_id);
  EXPECT_EQ(1, LayerAt(1)->effect_tree_index());
  EXPECT_EQ(3, LayerAt(2)->effect_tree_index());
  EXPECT_NE(element_id, GetPropertyTrees()
                            .effect_tree()
                            .Node(LayerAt(2)->effect_tree_index())
                            ->element_id);
}

TEST_P(PaintArtifactCompositorTest, EffectWithElementIdWithAlias) {
  auto* real_effect = CreateSampleEffectNodeWithElementId();
  auto* effect = EffectPaintPropertyNodeAlias::Create(*real_effect);
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(gfx::Rect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  EXPECT_EQ(2,
            ElementIdToEffectNodeIndex(real_effect->GetCompositorElementId()));
}

TEST_P(PaintArtifactCompositorTest, NonCompositedSimpleMask) {
  auto* masked =
      CreateOpacityEffect(e0(), 1.0, CompositingReason::kWillChangeOpacity);
  EffectPaintPropertyNode::State masking_state;
  masking_state.local_transform_space = &t0();
  masking_state.output_clip = &c0();
  masking_state.blend_mode = SkBlendMode::kDstIn;
  auto* masking =
      EffectPaintPropertyNode::Create(*masked, std::move(masking_state));

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *masked)
      .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kGray);
  artifact.Chunk(t0(), c0(), *masking)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());

  const cc::Layer* layer = LayerAt(0);
  EXPECT_THAT(*layer->GetPicture(),
              DrawsRectangles(Vector<RectWithColor>{
                  RectWithColor(gfx::RectF(0, 0, 200, 200), Color::kGray),
                  RectWithColor(gfx::RectF(50, 50, 100, 100), Color::kWhite)}));
  EXPECT_EQ(Translation(100, 100), layer->ScreenSpaceTransform());
  EXPECT_EQ(gfx::Size(200, 200), layer->bounds());
  const cc::EffectNode* masked_group =
      GetPropertyTrees().effect_tree().Node(layer->effect_tree_index());
  EXPECT_FALSE(masked_group->HasRenderSurface());
  EXPECT_EQ(SkBlendMode::kSrcOver, masked_group->blend_mode);
  EXPECT_TRUE(masked_group->filters.IsEmpty());
  // It's the last effect node. |masking| has been decomposited.
  EXPECT_EQ(masked_group, GetPropertyTrees().effect_tree().back());
}

TEST_P(PaintArtifactCompositorTest, CompositedMaskOneChild) {
  auto* masked =
      CreateOpacityEffect(e0(), 1.0, CompositingReason::kWillChangeOpacity);
  EffectPaintPropertyNode::State masking_state;
  masking_state.local_transform_space = &t0();
  masking_state.output_clip = &c0();
  masking_state.blend_mode = SkBlendMode::kDstIn;
  masking_state.direct_compositing_reasons =
      CompositingReason::kWillChangeOpacity;
  auto* masking =
      EffectPaintPropertyNode::Create(*masked, std::move(masking_state));

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *masked)
      .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kGray)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 200, 200))
      .HasText()
      .TextKnownToBeOnOpaqueBackground();
  artifact.Chunk(t0(), c0(), *masking)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(2u, LayerCount());

  // Composited mask doesn't affect opaqueness of the masked layer.
  const cc::Layer* masked_layer = LayerAt(0);
  EXPECT_TRUE(masked_layer->contents_opaque());
  EXPECT_TRUE(masked_layer->contents_opaque_for_text());

  const cc::Layer* masking_layer = LayerAt(1);
  const cc::EffectNode* masking_group =
      GetPropertyTrees().effect_tree().Node(masking_layer->effect_tree_index());

  // Render surface is not needed for one child.
  EXPECT_FALSE(masking_group->HasRenderSurface());
  EXPECT_EQ(SkBlendMode::kDstIn, masking_group->blend_mode);

  // The parent also has a render surface to define the scope of the backdrop
  // of the kDstIn blend mode.
  EXPECT_TRUE(GetPropertyTrees()
                  .effect_tree()
                  .parent(masking_group)
                  ->HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest, NonCompositedMaskClearsOpaqueness) {
  auto* masked =
      CreateOpacityEffect(e0(), 1.0, CompositingReason::kWillChangeOpacity);
  EffectPaintPropertyNode::State masking_state;
  masking_state.local_transform_space = &t0();
  masking_state.output_clip = &c0();
  masking_state.blend_mode = SkBlendMode::kDstIn;
  auto* masking =
      EffectPaintPropertyNode::Create(*masked, std::move(masking_state));

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *masked)
      .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kGray)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 200, 200))
      .HasText()
      .TextKnownToBeOnOpaqueBackground();
  artifact.Chunk(t0(), c0(), *masking)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());

  // Non-composited mask clears opaqueness status of the masked layer.
  const cc::Layer* layer = LayerAt(0);
  EXPECT_EQ(gfx::Size(200, 200), layer->bounds());
  EXPECT_FALSE(layer->contents_opaque());
  EXPECT_FALSE(layer->contents_opaque_for_text());
}

TEST_P(PaintArtifactCompositorTest, CompositedMaskTwoChildren) {
  auto* masked =
      CreateOpacityEffect(e0(), 1.0, CompositingReason::kWillChangeOpacity);
  EffectPaintPropertyNode::State masking_state;
  masking_state.local_transform_space = &t0();
  masking_state.output_clip = &c0();
  masking_state.blend_mode = SkBlendMode::kDstIn;
  auto* masking =
      EffectPaintPropertyNode::Create(*masked, std::move(masking_state));

  auto* child_of_masked =
      CreateOpacityEffect(*masking, 1.0, CompositingReason::kWillChangeOpacity);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *masked)
      .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kGray);
  artifact.Chunk(t0(), c0(), *child_of_masked)
      .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kGray);
  artifact.Chunk(t0(), c0(), *masking)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(3u, LayerCount());

  const cc::Layer* masking_layer = LayerAt(2);
  const cc::EffectNode* masking_group =
      GetPropertyTrees().effect_tree().Node(masking_layer->effect_tree_index());

  // There is a render surface because there are two children.
  EXPECT_TRUE(masking_group->HasRenderSurface());
  EXPECT_EQ(SkBlendMode::kDstIn, masking_group->blend_mode);

  // The parent also has a render surface to define the scope of the backdrop
  // of the kDstIn blend mode.
  EXPECT_TRUE(GetPropertyTrees()
                  .effect_tree()
                  .parent(masking_group)
                  ->HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest, NonCompositedSimpleExoticBlendMode) {
  auto* masked =
      CreateOpacityEffect(e0(), 1.0, CompositingReason::kWillChangeOpacity);
  EffectPaintPropertyNode::State masking_state;
  masking_state.local_transform_space = &t0();
  masking_state.output_clip = &c0();
  masking_state.blend_mode = SkBlendMode::kXor;
  auto* masking =
      EffectPaintPropertyNode::Create(*masked, std::move(masking_state));

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *masked)
      .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kGray);
  artifact.Chunk(t0(), c0(), *masking)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());

  const cc::Layer* layer = LayerAt(0);
  const cc::EffectNode* group =
      GetPropertyTrees().effect_tree().Node(layer->effect_tree_index());
  EXPECT_FALSE(group->HasRenderSurface());
  EXPECT_EQ(SkBlendMode::kSrcOver, group->blend_mode);
  // It's the last effect node. |masking| has been decomposited.
  EXPECT_EQ(group, GetPropertyTrees().effect_tree().back());
}

TEST_P(PaintArtifactCompositorTest, ForcedCompositedExoticBlendMode) {
  auto* masked =
      CreateOpacityEffect(e0(), 1.0, CompositingReason::kWillChangeOpacity);
  EffectPaintPropertyNode::State masking_state;
  masking_state.local_transform_space = &t0();
  masking_state.output_clip = &c0();
  masking_state.blend_mode = SkBlendMode::kXor;
  masking_state.direct_compositing_reasons = CompositingReason::kOverlap;
  auto* masking =
      EffectPaintPropertyNode::Create(*masked, std::move(masking_state));

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *masked)
      .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kGray);
  artifact.Chunk(t0(), c0(), *masking)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(2u, LayerCount());

  const cc::Layer* masking_layer = LayerAt(1);
  const cc::EffectNode* masking_group =
      GetPropertyTrees().effect_tree().Node(masking_layer->effect_tree_index());
  EXPECT_EQ(SkBlendMode::kXor, masking_group->blend_mode);

  // This requires a render surface.
  EXPECT_TRUE(masking_group->HasRenderSurface());
  // The parent also requires a render surface to define the backdrop scope of
  // the blend mode.
  EXPECT_TRUE(GetPropertyTrees()
                  .effect_tree()
                  .parent(masking_group)
                  ->HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest,
       CompositedExoticBlendModeOnTwoOpacityAnimationLayers) {
  auto* masked =
      CreateOpacityEffect(e0(), 1.0, CompositingReason::kWillChangeOpacity);
  auto* masked_child1 = CreateOpacityEffect(
      *masked, 1.0, CompositingReason::kActiveOpacityAnimation);
  auto* masked_child2 = CreateOpacityEffect(
      *masked, 1.0, CompositingReason::kActiveOpacityAnimation);
  EffectPaintPropertyNode::State masking_state;
  masking_state.local_transform_space = &t0();
  masking_state.output_clip = &c0();
  masking_state.blend_mode = SkBlendMode::kXor;
  auto* masking =
      EffectPaintPropertyNode::Create(*masked, std::move(masking_state));

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *masked_child1)
      .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kGray);
  artifact.Chunk(t0(), c0(), *masked_child2)
      .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kBlack);
  artifact.Chunk(t0(), c0(), *masking)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(3u, LayerCount());

  const cc::Layer* masking_layer = LayerAt(2);
  const cc::EffectNode* masking_group =
      GetPropertyTrees().effect_tree().Node(masking_layer->effect_tree_index());
  EXPECT_EQ(SkBlendMode::kXor, masking_group->blend_mode);

  // This requires a render surface.
  EXPECT_TRUE(masking_group->HasRenderSurface());
  // The parent also requires a render surface to define the backdrop scope of
  // the blend mode.
  EXPECT_TRUE(GetPropertyTrees()
                  .effect_tree()
                  .parent(masking_group)
                  ->HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest,
       CompositedExoticBlendModeOnTwo3DTransformLayers) {
  auto* masked =
      CreateOpacityEffect(e0(), 1.0, CompositingReason::kWillChangeOpacity);
  auto* transform1 = CreateTransform(t0(), gfx::Transform(), gfx::Point3F(),
                                     CompositingReason::k3DTransform);
  auto* transform2 = CreateTransform(t0(), gfx::Transform(), gfx::Point3F(),
                                     CompositingReason::k3DTransform);
  EffectPaintPropertyNode::State masking_state;
  masking_state.local_transform_space = &t0();
  masking_state.output_clip = &c0();
  masking_state.blend_mode = SkBlendMode::kXor;
  auto* masking =
      EffectPaintPropertyNode::Create(*masked, std::move(masking_state));

  TestPaintArtifact artifact;
  artifact.Chunk(*transform1, c0(), *masked)
      .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kGray);
  artifact.Chunk(*transform2, c0(), *masked)
      .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kBlack);
  artifact.Chunk(t0(), c0(), *masking)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(3u, LayerCount());

  const cc::Layer* masking_layer = LayerAt(2);
  const cc::EffectNode* masking_group =
      GetPropertyTrees().effect_tree().Node(masking_layer->effect_tree_index());
  EXPECT_EQ(SkBlendMode::kXor, masking_group->blend_mode);

  // This requires a render surface.
  EXPECT_TRUE(masking_group->HasRenderSurface());
  // The parent also requires a render surface to define the backdrop scope of
  // the blend mode.
  EXPECT_TRUE(GetPropertyTrees()
                  .effect_tree()
                  .parent(masking_group)
                  ->HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest, DecompositeExoticBlendModeWithoutBackdrop) {
  auto* parent_effect =
      CreateOpacityEffect(e0(), 1.0, CompositingReason::kWillChangeOpacity);
  EffectPaintPropertyNode::State blend_state1;
  blend_state1.local_transform_space = &t0();
  blend_state1.blend_mode = SkBlendMode::kScreen;
  auto* blend_effect1 =
      EffectPaintPropertyNode::Create(*parent_effect, std::move(blend_state1));
  EffectPaintPropertyNode::State blend_state2;
  blend_state2.local_transform_space = &t0();
  blend_state2.blend_mode = SkBlendMode::kScreen;
  auto* blend_effect2 =
      EffectPaintPropertyNode::Create(*parent_effect, std::move(blend_state2));

  Update(TestPaintArtifact()
             .Chunk(t0(), c0(), *blend_effect1)
             .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kGray)
             .Chunk(t0(), c0(), *blend_effect2)
             .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kBlack)
             .Build());

  ASSERT_EQ(1u, LayerCount());
  const auto* effect =
      GetPropertyTrees().effect_tree().Node(LayerAt(0)->effect_tree_index());
  EXPECT_EQ(1.0f, effect->opacity);
  EXPECT_EQ(SkBlendMode::kSrcOver, effect->blend_mode);
  // Don't need a render surface because all blend effects are decomposited.
  EXPECT_FALSE(effect->HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest,
       DecompositeExoticBlendModeWithNonDrawingLayer) {
  auto* parent_effect =
      CreateOpacityEffect(e0(), 1.0, CompositingReason::kWillChangeOpacity);
  EffectPaintPropertyNode::State blend_state1;
  blend_state1.local_transform_space = &t0();
  blend_state1.blend_mode = SkBlendMode::kScreen;
  auto* blend_effect1 =
      EffectPaintPropertyNode::Create(*parent_effect, std::move(blend_state1));
  EffectPaintPropertyNode::State blend_state2;
  blend_state2.local_transform_space = &t0();
  blend_state2.blend_mode = SkBlendMode::kScreen;
  auto* blend_effect2 =
      EffectPaintPropertyNode::Create(*parent_effect, std::move(blend_state2));
  auto* transform = CreateAnimatingTransform(t0());

  Update(TestPaintArtifact()
             .Chunk(*transform, c0(), *parent_effect)
             .Bounds(gfx::Rect(0, 0, 33, 44))
             .Chunk(t0(), c0(), *blend_effect1)
             .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kGray)
             .Chunk(t0(), c0(), *blend_effect2)
             .RectDrawing(gfx::Rect(200, 200, 200, 200), Color::kBlack)
             .Build());

  ASSERT_EQ(2u, LayerCount());
  // This is the empty layer forced by |transform|.
  EXPECT_EQ(gfx::Size(33, 44), LayerAt(0)->bounds());
  EXPECT_FALSE(LayerAt(0)->draws_content());
  // This is the layer containing the paint chunks with |blend_effect1| and
  // |blend_effect2| decomposited.
  EXPECT_EQ(gfx::Size(300, 300), LayerAt(1)->bounds());
  const auto* effect =
      GetPropertyTrees().effect_tree().Node(LayerAt(1)->effect_tree_index());
  EXPECT_EQ(1.0f, effect->opacity);
  EXPECT_EQ(SkBlendMode::kSrcOver, effect->blend_mode);
  // Don't need a render surface because all blend effects are decomposited.
  EXPECT_FALSE(effect->HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest, UpdateProducesNewSequenceNumber) {
  // A 90 degree clockwise rotation about (100, 100).
  auto* transform =
      CreateTransform(t0(), MakeRotationMatrix(90), gfx::Point3F(100, 100, 0),
                      CompositingReason::k3DTransform);
  auto* clip = CreateClip(c0(), t0(), FloatRoundedRect(100, 100, 300, 200));
  auto* effect = CreateOpacityEffect(e0(), 0.5);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk(*transform, *clip, *effect)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kGray);
  auto& artifact = test_artifact.Build();

  Update(artifact);

  // Two content layers for the differentiated rect drawings and three dummy
  // layers for each of the transform, clip and effect nodes.
  EXPECT_EQ(2u, RootLayer()->children().size());
  int sequence_number = GetPropertyTrees().sequence_number();
  EXPECT_GT(sequence_number, 0);
  for (auto layer : RootLayer()->children()) {
    EXPECT_EQ(sequence_number, layer->property_tree_sequence_number());
  }

  Update(artifact);

  EXPECT_EQ(2u, RootLayer()->children().size());
  sequence_number++;
  EXPECT_EQ(sequence_number, GetPropertyTrees().sequence_number());
  for (auto layer : RootLayer()->children()) {
    EXPECT_EQ(sequence_number, layer->property_tree_sequence_number());
  }

  Update(artifact);

  EXPECT_EQ(2u, RootLayer()->children().size());
  sequence_number++;
  EXPECT_EQ(sequence_number, GetPropertyTrees().sequence_number());
  for (auto layer : RootLayer()->children()) {
    EXPECT_EQ(sequence_number, layer->property_tree_sequence_number());
  }
}

TEST_P(PaintArtifactCompositorTest, DecompositeClip) {
  // A clipped paint chunk that gets merged into a previous layer should
  // only contribute clipped bounds to the layer bound.
  auto* clip = CreateClip(c0(), t0(), FloatRoundedRect(75, 75, 100, 100));

  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(gfx::Rect(50, 50, 100, 100), Color::kGray);
  artifact.Chunk(t0(), *clip, e0())
      .RectDrawing(gfx::Rect(100, 100, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());

  const cc::Layer* layer = LayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 50.f), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(125, 125), layer->bounds());
}

TEST_P(PaintArtifactCompositorTest, DecompositeEffect) {
  // An effect node without direct compositing reason and does not need to
  // group compositing descendants should not be composited and can merge
  // with other chunks.

  auto* effect = CreateOpacityEffect(e0(), 0.5);

  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(gfx::Rect(50, 25, 100, 100), Color::kGray);
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(gfx::Rect(25, 75, 100, 100), Color::kGray);
  artifact.Chunk().RectDrawing(gfx::Rect(75, 75, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());

  const cc::Layer* layer = LayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(25.f, 25.f), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(150, 150), layer->bounds());
  EXPECT_EQ(1, layer->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, DirectlyCompositedEffect) {
  // An effect node with direct compositing shall be composited.
  auto* effect = CreateOpacityEffect(e0(), 0.5f, CompositingReason::kAll);

  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(gfx::Rect(50, 25, 100, 100), Color::kGray);
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(gfx::Rect(25, 75, 100, 100), Color::kGray);
  artifact.Chunk().RectDrawing(gfx::Rect(75, 75, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(3u, LayerCount());

  const cc::Layer* layer1 = LayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 25.f), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer1->bounds());
  EXPECT_EQ(1, layer1->effect_tree_index());

  const cc::Layer* layer2 = LayerAt(1);
  EXPECT_EQ(gfx::Vector2dF(25.f, 75.f), layer2->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer2->bounds());
  const cc::EffectNode* effect_node =
      GetPropertyTrees().effect_tree().Node(layer2->effect_tree_index());
  EXPECT_EQ(1, effect_node->parent_id);
  EXPECT_EQ(0.5f, effect_node->opacity);

  const cc::Layer* layer3 = LayerAt(2);
  EXPECT_EQ(gfx::Vector2dF(75.f, 75.f), layer3->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer3->bounds());
  EXPECT_EQ(1, layer3->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, DecompositeDeepEffect) {
  // A paint chunk may enter multiple level effects with or without compositing
  // reasons. This test verifies we still decomposite effects without a direct
  // reason, but stop at a directly composited effect.
  auto* effect1 = CreateOpacityEffect(e0(), 0.1f);
  auto* effect2 = CreateOpacityEffect(*effect1, 0.2f, CompositingReason::kAll);
  auto* effect3 = CreateOpacityEffect(*effect2, 0.3f);

  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(gfx::Rect(50, 25, 100, 100), Color::kGray);
  artifact.Chunk(t0(), c0(), *effect3)
      .RectDrawing(gfx::Rect(25, 75, 100, 100), Color::kGray);
  artifact.Chunk().RectDrawing(gfx::Rect(75, 75, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(3u, LayerCount());

  const cc::Layer* layer1 = LayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 25.f), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer1->bounds());
  EXPECT_EQ(1, layer1->effect_tree_index());

  const cc::Layer* layer2 = LayerAt(1);
  EXPECT_EQ(gfx::Vector2dF(25.f, 75.f), layer2->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer2->bounds());
  const cc::EffectNode* effect_node2 =
      GetPropertyTrees().effect_tree().Node(layer2->effect_tree_index());
  EXPECT_EQ(0.2f, effect_node2->opacity);
  const cc::EffectNode* effect_node1 =
      GetPropertyTrees().effect_tree().Node(effect_node2->parent_id);
  EXPECT_EQ(1, effect_node1->parent_id);
  EXPECT_EQ(0.1f, effect_node1->opacity);

  const cc::Layer* layer3 = LayerAt(2);
  EXPECT_EQ(gfx::Vector2dF(75.f, 75.f), layer3->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer3->bounds());
  EXPECT_EQ(1, layer3->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, IndirectlyCompositedEffect) {
  // An effect node without direct compositing still needs to be composited
  // for grouping, if some chunks need to be composited.
  auto* effect = CreateOpacityEffect(e0(), 0.5f);
  auto* transform = CreateTransform(t0(), gfx::Transform(), gfx::Point3F(),
                                    CompositingReason::k3DTransform);

  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(gfx::Rect(50, 25, 100, 100), Color::kGray);
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(gfx::Rect(25, 75, 100, 100), Color::kGray);
  artifact.Chunk(*transform, c0(), *effect)
      .RectDrawing(gfx::Rect(75, 75, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(3u, LayerCount());

  const cc::Layer* layer1 = LayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 25.f), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer1->bounds());
  EXPECT_EQ(1, layer1->effect_tree_index());

  const cc::Layer* layer2 = LayerAt(1);
  EXPECT_EQ(gfx::Vector2dF(25.f, 75.f), layer2->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer2->bounds());
  const cc::EffectNode* effect_node =
      GetPropertyTrees().effect_tree().Node(layer2->effect_tree_index());
  EXPECT_EQ(1, effect_node->parent_id);
  EXPECT_EQ(0.5f, effect_node->opacity);

  const cc::Layer* layer3 = LayerAt(2);
  EXPECT_EQ(gfx::Vector2dF(75.f, 75.f), layer3->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer3->bounds());
  EXPECT_EQ(effect_node->id, layer3->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, DecompositedEffectNotMergingDueToOverlap) {
  // This tests an effect that doesn't need to be composited, but needs
  // separate backing due to overlap with a previous composited effect.
  auto* effect1 = CreateOpacityEffect(e0(), 0.1f);
  auto* effect2 = CreateOpacityEffect(e0(), 0.2f);
  auto* transform = CreateTransform(t0(), gfx::Transform(), gfx::Point3F(),
                                    CompositingReason::k3DTransform);
  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(gfx::Rect(0, 0, 40, 40), Color::kGray);
  artifact.Chunk(t0(), c0(), *effect1)
      .RectDrawing(gfx::Rect(50, 0, 40, 40), Color::kGray);
  // This chunk has a transform that must be composited, thus causing effect1
  // to be composited too.
  artifact.Chunk(*transform, c0(), *effect1)
      .RectDrawing(gfx::Rect(100, 0, 40, 40), Color::kGray);
  artifact.Chunk(t0(), c0(), *effect2)
      .RectDrawing(gfx::Rect(100, 50, 40, 40), Color::kGray);
  // This chunk overlaps with the 2nd chunk, but is seemingly safe to merge.
  // However because effect1 gets composited due to a composited transform,
  // we can't merge with effect1 nor skip it to merge with the first chunk.
  artifact.Chunk(t0(), c0(), *effect2)
      .RectDrawing(gfx::Rect(50, 0, 40, 40), Color::kGray);

  Update(artifact.Build());
  ASSERT_EQ(4u, LayerCount());

  const cc::Layer* layer1 = LayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(0.f, 0.f), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(40, 40), layer1->bounds());
  EXPECT_EQ(1, layer1->effect_tree_index());

  const cc::Layer* layer2 = LayerAt(1);
  EXPECT_EQ(gfx::Vector2dF(50.f, 0.f), layer2->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(40, 40), layer2->bounds());
  const cc::EffectNode* effect_node =
      GetPropertyTrees().effect_tree().Node(layer2->effect_tree_index());
  EXPECT_EQ(1, effect_node->parent_id);
  EXPECT_EQ(0.1f, effect_node->opacity);

  const cc::Layer* layer3 = LayerAt(2);
  EXPECT_EQ(gfx::Vector2dF(100.f, 0.f), layer3->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(40, 40), layer3->bounds());
  EXPECT_EQ(effect_node->id, layer3->effect_tree_index());

  const cc::Layer* layer4 = LayerAt(3);
  EXPECT_EQ(gfx::Vector2dF(50.f, 0.f), layer4->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(90, 90), layer4->bounds());
  EXPECT_EQ(1, layer4->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, EffectivelyInvisibleChunk) {
  UpdateWithEffectivelyInvisibleChunk(false, false);
  ASSERT_EQ(1u, LayerCount());
  EXPECT_EQ(gfx::Size(10, 10), LayerAt(0)->bounds());
  EXPECT_FALSE(LayerAt(0)->draws_content());
  EXPECT_FALSE(LayerAt(0)->GetPicture());
}

TEST_P(PaintArtifactCompositorTest, EffectivelyInvisibleSolidColorChunk) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .EffectivelyInvisible()
      .RectDrawing(gfx::Rect(10, 0, 10, 10), Color(255, 0, 0))
      .IsSolidColor();
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_EQ(gfx::Size(10, 10), LayerAt(0)->bounds());
  EXPECT_TRUE(LayerAt(0)->IsSolidColorLayerForTesting());
  EXPECT_FALSE(LayerAt(0)->draws_content());
  EXPECT_FALSE(LayerAt(0)->GetPicture());
}

TEST_P(PaintArtifactCompositorTest,
       EffectivelyInvisibleChunkWithPrecedingChunk) {
  UpdateWithEffectivelyInvisibleChunk(true, false);
  ASSERT_EQ(1u, LayerCount());
  EXPECT_EQ(gfx::Size(20, 10), LayerAt(0)->bounds());
  EXPECT_TRUE(LayerAt(0)->draws_content());
  EXPECT_THAT(LayerAt(0)->GetPicture(),
              Pointee(DrawsRectangles(
                  {RectWithColor(gfx::RectF(0, 0, 10, 10), Color::kBlack)})));
}

TEST_P(PaintArtifactCompositorTest,
       EffectivelyInvisibleChunkWithSubsequentChunk) {
  UpdateWithEffectivelyInvisibleChunk(false, true);
  ASSERT_EQ(1u, LayerCount());
  EXPECT_EQ(gfx::Size(20, 20), LayerAt(0)->bounds());
  EXPECT_TRUE(LayerAt(0)->draws_content());
  EXPECT_THAT(LayerAt(0)->GetPicture(),
              Pointee(DrawsRectangles(
                  {RectWithColor(gfx::RectF(0, 10, 10, 10), Color::kWhite)})));
}

TEST_P(PaintArtifactCompositorTest,
       EffectivelyInvisibleChunkWithPrecedingAndSubsequentChunks) {
  UpdateWithEffectivelyInvisibleChunk(true, true);
  ASSERT_EQ(1u, LayerCount());
  EXPECT_EQ(gfx::Size(20, 20), LayerAt(0)->bounds());
  EXPECT_TRUE(LayerAt(0)->draws_content());
  EXPECT_THAT(LayerAt(0)->GetPicture(),
              Pointee(DrawsRectangles(
                  {RectWithColor(gfx::RectF(0, 0, 10, 10), Color::kBlack),
                   RectWithColor(gfx::RectF(0, 10, 10, 10), Color::kWhite)})));
}

TEST_P(PaintArtifactCompositorTest, UpdateManagesLayerElementIds) {
  auto* transform = CreateAnimatingTransform(t0());
  CompositorElementId element_id = transform->GetCompositorElementId();

  {
    TestPaintArtifact artifact;
    artifact.Chunk(*transform, c0(), e0())
        .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);

    Update(artifact.Build());
    ASSERT_EQ(1u, LayerCount());
    ASSERT_TRUE(GetLayerTreeHost().IsElementInPropertyTrees(
        element_id, cc::ElementListType::ACTIVE));
  }

  {
    TestPaintArtifact artifact;
    ASSERT_TRUE(GetLayerTreeHost().IsElementInPropertyTrees(
        element_id, cc::ElementListType::ACTIVE));
    Update(artifact.Build());
    ASSERT_EQ(0u, LayerCount());
    ASSERT_FALSE(GetLayerTreeHost().IsElementInPropertyTrees(
        element_id, cc::ElementListType::ACTIVE));
  }
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipSimple) {
  // This tests the simplest case that a single layer needs to be clipped
  // by a single composited rounded clip.
  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 5);
  auto* c1 = CreateClip(c0(), t0(), rrect);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //       content0
  // [ mask_isolation_0 ]
  // [        e0        ]
  // One content layer.
  ASSERT_EQ(1u, LayerCount());
  // There is still a "synthesized layer" but it's null.
  ASSERT_EQ(1u, SynthesizedClipLayerCount());
  EXPECT_FALSE(SynthesizedClipLayerAt(0));

  const cc::Layer* content0 = LayerAt(0);

  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);
  EXPECT_TRUE(mask_isolation_0.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_0.mask_filter_info.rounded_corner_bounds());
  EXPECT_FALSE(mask_isolation_0.HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipRotatedNotSupported) {
  // Synthesized clips are not currently supported when rotated (or any
  // transform that is not 2D axis-aligned).
  auto* transform =
      CreateTransform(t0(), MakeRotationMatrix(45), gfx::Point3F(100, 100, 0),
                      CompositingReason::k3DTransform);

  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 5);
  auto* c1 = CreateClip(c0(), *transform, rrect);

  TestPaintArtifact artifact;
  artifact.Chunk(*transform, *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //             clip_mask0
  // content0 [ mask_effect_0 ]
  // [    mask_isolation_0    ]
  // [           e0           ]
  // One content layer.
  ASSERT_EQ(2u, LayerCount());
  ASSERT_EQ(1u, SynthesizedClipLayerCount());

  const cc::Layer* content0 = LayerAt(0);
  const cc::Layer* clip_mask0 = LayerAt(1);

  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);
  EXPECT_TRUE(mask_isolation_0.HasRenderSurface());

  EXPECT_EQ(SynthesizedClipLayerAt(0), clip_mask0);
  EXPECT_TRUE(clip_mask0->draws_content());
  EXPECT_EQ(cc::HitTestOpaqueness::kMixed, clip_mask0->hit_test_opaqueness());
  EXPECT_EQ(gfx::Size(306, 206), clip_mask0->bounds());
  EXPECT_EQ(gfx::Vector2dF(47, 47), clip_mask0->offset_to_transform_parent());
  // c1 should be applied in the clip mask layer.
  EXPECT_EQ(c0_id, clip_mask0->clip_tree_index());
  int mask_effect_0_id = clip_mask0->effect_tree_index();
  const cc::EffectNode& mask_effect_0 =
      *GetPropertyTrees().effect_tree().Node(mask_effect_0_id);
  ASSERT_EQ(mask_isolation_0_id, mask_effect_0.parent_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_0.blend_mode);
  // Render surface is not needed for DstIn controlling only one layer.
  EXPECT_FALSE(mask_effect_0.HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClip90DegRotationSupported) {
  // 90-degree rotations are axis-aligned, and so the synthetic clip is
  // supported.
  auto* transform =
      CreateTransform(t0(), MakeRotationMatrix(90), gfx::Point3F(100, 100, 0),
                      CompositingReason::k3DTransform);

  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 5);
  auto* c1 = CreateClip(c0(), *transform, rrect);

  TestPaintArtifact artifact;
  artifact.Chunk(*transform, *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //       content0
  // [ mask_isolation_0 ]
  // [        e0        ]
  // One content layer.
  ASSERT_EQ(1u, LayerCount());
  // There is still a "synthesized layer" but it's null.
  ASSERT_EQ(1u, SynthesizedClipLayerCount());
  EXPECT_FALSE(SynthesizedClipLayerAt(0));

  const cc::Layer* content0 = LayerAt(0);

  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);
  EXPECT_TRUE(mask_isolation_0.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_0.mask_filter_info.rounded_corner_bounds());
  EXPECT_FALSE(mask_isolation_0.HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest,
       SynthesizedClipShaderBasedBorderRadiusNotSupported2) {
  // This tests the simplest case that a single layer needs to be clipped
  // by a single composited rounded clip. Because the radius is unsymmetric,
  // it falls back to a mask layer.
  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 30, 40);
  auto* c1 = CreateClip(c0(), t0(), rrect);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //             clip_mask0
  // content0 [ mask_effect_0 ]
  // [    mask_isolation_0    ]
  // [           e0           ]
  // One content layer.
  ASSERT_EQ(2u, LayerCount());
  ASSERT_EQ(1u, SynthesizedClipLayerCount());

  const cc::Layer* content0 = LayerAt(0);
  const cc::Layer* clip_mask0 = LayerAt(1);

  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);
  EXPECT_TRUE(mask_isolation_0.HasRenderSurface());

  EXPECT_EQ(SynthesizedClipLayerAt(0), clip_mask0);
  EXPECT_TRUE(clip_mask0->draws_content());
  EXPECT_EQ(cc::HitTestOpaqueness::kMixed, clip_mask0->hit_test_opaqueness());
  EXPECT_EQ(gfx::Size(304, 204), clip_mask0->bounds());
  EXPECT_EQ(gfx::Vector2dF(48, 48), clip_mask0->offset_to_transform_parent());
  EXPECT_EQ(c0_id, clip_mask0->clip_tree_index());
  int mask_effect_0_id = clip_mask0->effect_tree_index();
  const cc::EffectNode& mask_effect_0 =
      *GetPropertyTrees().effect_tree().Node(mask_effect_0_id);
  ASSERT_EQ(mask_isolation_0_id, mask_effect_0.parent_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_0.blend_mode);

  // The masks DrawsContent because it has content that it masks which also
  // DrawsContent.
  EXPECT_TRUE(clip_mask0->draws_content());
}

TEST_P(
    PaintArtifactCompositorTest,
    SynthesizedClipSimpleShaderBasedBorderRadiusNotSupportedMacNonEqualCorners) {
  // Tests that on Mac, we fall back to a mask layer if the corners are not all
  // the same radii.
  gfx::SizeF corner(30, 30);
  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), corner, corner, corner,
                         gfx::SizeF());
  auto* c1 = CreateClip(c0(), t0(), rrect);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

#if BUILDFLAG(IS_MAC)
  ASSERT_EQ(2u, LayerCount());
#else
  ASSERT_EQ(1u, LayerCount());
#endif
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipNested) {
  // This tests the simplest case that a single layer needs to be clipped
  // by a single composited rounded clip.
  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 5);
  auto* c1 = CreateClip(c0(), t0(), rrect);
  auto* c2 = CreateClip(*c1, t0(), rrect);
  auto* c3 = CreateClip(*c2, t0(), rrect);
  auto* t1 = CreateTransform(t0(), gfx::Transform(), gfx::Point3F(),
                             CompositingReason::kWillChangeTransform);
  CompositorFilterOperations filter_operations;
  filter_operations.AppendBlurFilter(5);
  auto* filter = CreateFilterEffect(e0(), t0(), c1, filter_operations);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, *filter)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(*t1, *c3, *filter)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //               content1
  //          [ mask_isolation_2 ]
  // content0 [ mask_isolation_1 ]
  // [          filter           ]
  // [     mask_isolation_0      ]
  // [            e0             ]
  // Two content layers.
  ///
  // mask_isolation_1 will have a render surface. mask_isolation_2 will not
  // because non-leaf synthetic rounded clips must have a render surface.
  // mask_isolation_0 will not because it is a leaf synthetic rounded clip
  // in the render surface created by the filter.

  ASSERT_EQ(2u, LayerCount());
  // There is still a "synthesized layer" but it's null.
  ASSERT_EQ(3u, SynthesizedClipLayerCount());
  EXPECT_FALSE(SynthesizedClipLayerAt(0));
  EXPECT_FALSE(SynthesizedClipLayerAt(1));
  EXPECT_FALSE(SynthesizedClipLayerAt(2));

  const cc::Layer* content0 = LayerAt(0);
  const cc::Layer* content1 = LayerAt(1);

  constexpr int c1_id = 2;
  constexpr int e1_id = 2;

  int c3_id = content1->clip_tree_index();
  const cc::ClipNode& cc_c3 = *GetPropertyTrees().clip_tree().Node(c3_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c3.clip);
  const cc::ClipNode& cc_c2 =
      *GetPropertyTrees().clip_tree().Node(cc_c3.parent_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c2.clip);
  ASSERT_EQ(c1_id, cc_c2.parent_id);
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  EXPECT_EQ(c1_id, content0->clip_tree_index());
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);

  int mask_isolation_2_id = content1->effect_tree_index();
  const cc::EffectNode& mask_isolation_2 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_2_id);
  const cc::EffectNode& mask_isolation_1 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_2.parent_id);
  const cc::EffectNode& cc_filter =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_1.parent_id);
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree().Node(cc_filter.parent_id);

  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);
  EXPECT_TRUE(mask_isolation_0.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_0.mask_filter_info.rounded_corner_bounds());
  EXPECT_FALSE(mask_isolation_0.HasRenderSurface());

  ASSERT_EQ(e1_id, cc_filter.parent_id);
  EXPECT_EQ(cc_filter.id, content0->effect_tree_index());
  EXPECT_EQ(SkBlendMode::kSrcOver, cc_filter.blend_mode);
  EXPECT_FALSE(cc_filter.is_fast_rounded_corner);
  EXPECT_TRUE(cc_filter.HasRenderSurface());

  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_1.blend_mode);
  EXPECT_TRUE(mask_isolation_1.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_1.mask_filter_info.rounded_corner_bounds());
  EXPECT_TRUE(mask_isolation_1.HasRenderSurface());

  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_2.blend_mode);
  EXPECT_TRUE(mask_isolation_2.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_2.mask_filter_info.rounded_corner_bounds());
  EXPECT_FALSE(mask_isolation_2.HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipIsNotDrawable) {
  // This tests the simplist case that a single layer needs to be clipped
  // by a single composited rounded clip.
  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 5);
  auto* c1 = CreateClip(c0(), t0(), rrect);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 0, 0), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //       content0
  // [ mask_isolation_0 ]
  // [        e0        ]
  // One content layer, no clip mask (because layer doesn't draw content).
  ASSERT_EQ(1u, LayerCount());
  ASSERT_EQ(1u, SynthesizedClipLayerCount());
  // There is a synthesized clip", but it has no layer backing.
  ASSERT_EQ(nullptr, SynthesizedClipLayerAt(0));

  const cc::Layer* content0 = LayerAt(0);

  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);
}

TEST_P(PaintArtifactCompositorTest, ReuseSyntheticClip) {
  // This tests the simplist case that a single layer needs to be clipped
  // by a single composited rounded clip.
  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 5);
  auto* c1 = CreateClip(c0(), t0(), rrect);
  auto* c2 = CreateClip(c0(), t0(), rrect);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 0, 0), Color::kBlack);
  Update(artifact.Build());

  const cc::Layer* content0 = LayerAt(0);

  cc::ElementId old_element_id = GetPropertyTrees()
                                     .effect_tree()
                                     .Node(content0->effect_tree_index())
                                     ->element_id;

  TestPaintArtifact repeated_artifact;
  repeated_artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 0, 0), Color::kBlack);
  Update(repeated_artifact.Build());
  const cc::Layer* content1 = LayerAt(0);

  // Check that stable ids are reused across updates.
  EXPECT_EQ(GetPropertyTrees()
                .effect_tree()
                .Node(content1->effect_tree_index())
                ->element_id,
            old_element_id);

  TestPaintArtifact changed_artifact;
  changed_artifact.Chunk(t0(), *c2, e0())
      .RectDrawing(gfx::Rect(0, 0, 0, 0), Color::kBlack);
  Update(changed_artifact.Build());
  const cc::Layer* content2 = LayerAt(0);

  // The new artifact changed the clip node to c2, so the synthetic clip should
  // not be reused.
  EXPECT_NE(GetPropertyTrees()
                .effect_tree()
                .Node(content2->effect_tree_index())
                ->element_id,
            old_element_id);
}

TEST_P(PaintArtifactCompositorTest,
       SynthesizedClipIndirectlyCompositedClipPath) {
  // This tests the case that a clip node needs to be synthesized due to
  // applying clip path to a composited effect.
  auto* c1 = CreateClipPathClip(c0(), t0(), FloatRoundedRect(50, 50, 300, 200));
  auto* e1 = CreateOpacityEffect(e0(), t0(), c1, 1,
                                 CompositingReason::kWillChangeOpacity);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, *e1)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  // content0   clip_mask0
  // [  e1  ][ mask_effect_0 ]
  // [   mask_isolation_0    ]
  // [          e0           ]
  // One content layer, one clip mask.
  ASSERT_EQ(2u, LayerCount());
  ASSERT_EQ(1u, SynthesizedClipLayerCount());

  const cc::Layer* content0 = LayerAt(0);
  const cc::Layer* clip_mask0 = LayerAt(1);

  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int e1_id = content0->effect_tree_index();
  const cc::EffectNode& cc_e1 = *GetPropertyTrees().effect_tree().Node(e1_id);
  EXPECT_EQ(c1_id, cc_e1.clip_id);
  int mask_isolation_0_id = cc_e1.parent_id;
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(c0_id, mask_isolation_0.clip_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);

  EXPECT_EQ(SynthesizedClipLayerAt(0), clip_mask0);
  EXPECT_EQ(gfx::Size(304, 204), clip_mask0->bounds());
  EXPECT_EQ(gfx::Vector2dF(48, 48), clip_mask0->offset_to_transform_parent());
  EXPECT_EQ(c0_id, clip_mask0->clip_tree_index());
  int mask_effect_0_id = clip_mask0->effect_tree_index();
  const cc::EffectNode& mask_effect_0 =
      *GetPropertyTrees().effect_tree().Node(mask_effect_0_id);
  ASSERT_EQ(mask_isolation_0_id, mask_effect_0.parent_id);
  EXPECT_EQ(c0_id, mask_effect_0.clip_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_0.blend_mode);
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipContiguous) {
  // This tests the case that a two back-to-back composited layers having
  // the same composited rounded clip can share the synthesized mask.
  auto* t1 = CreateTransform(t0(), gfx::Transform(), gfx::Point3F(),
                             CompositingReason::kWillChangeTransform);

  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 5);
  auto* c1 = CreateClip(c0(), t0(), rrect);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(*t1, *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //   content0  content1
  // [  mask_isolation_0  ]
  // [         e0         ]
  // Two content layers, one clip mask.
  ASSERT_EQ(2u, LayerCount());
  // There is still a "synthesized layer" but it's null.
  ASSERT_EQ(1u, SynthesizedClipLayerCount());
  EXPECT_FALSE(SynthesizedClipLayerAt(0));

  const cc::Layer* content0 = LayerAt(0);
  const cc::Layer* content1 = LayerAt(1);

  EXPECT_EQ(t0_id, content0->transform_tree_index());
  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);

  int t1_id = content1->transform_tree_index();
  const cc::TransformNode& cc_t1 =
      *GetPropertyTrees().transform_tree().Node(t1_id);
  ASSERT_EQ(t0_id, cc_t1.parent_id);
  EXPECT_EQ(c1_id, content1->clip_tree_index());
  EXPECT_EQ(mask_isolation_0_id, content1->effect_tree_index());

  EXPECT_TRUE(mask_isolation_0.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_0.mask_filter_info.rounded_corner_bounds());
  EXPECT_FALSE(mask_isolation_0.HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipDiscontiguous) {
  // This tests the case that a two composited layers having the same
  // composited rounded clip cannot share the synthesized mask if there is
  // another layer in the middle.
  auto* t1 = CreateTransform(t0(), gfx::Transform(), gfx::Point3F(),
                             CompositingReason::kWillChangeTransform);

  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 5);
  auto* c1 = CreateClip(c0(), t0(), rrect);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(*t1, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //       content0                     content2
  // [ mask_isolation_0 ] content1 [ mask_isolation_1 ]
  // [                       e0                       ]
  // Three content layers.
  ASSERT_EQ(3u, LayerCount());
  // There are still "synthesized layers" but they're null because they use
  // fast rounded corners.
  ASSERT_EQ(2u, SynthesizedClipLayerCount());
  EXPECT_FALSE(SynthesizedClipLayerAt(0));
  EXPECT_FALSE(SynthesizedClipLayerAt(1));

  const cc::Layer* content0 = LayerAt(0);
  const cc::Layer* content1 = LayerAt(1);
  const cc::Layer* content2 = LayerAt(2);

  EXPECT_EQ(t0_id, content0->transform_tree_index());
  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);
  EXPECT_TRUE(mask_isolation_0.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_0.mask_filter_info.rounded_corner_bounds());
  EXPECT_FALSE(mask_isolation_0.HasRenderSurface());

  int t1_id = content1->transform_tree_index();
  const cc::TransformNode& cc_t1 =
      *GetPropertyTrees().transform_tree().Node(t1_id);
  ASSERT_EQ(t0_id, cc_t1.parent_id);
  EXPECT_EQ(c0_id, content1->clip_tree_index());
  EXPECT_EQ(e0_id, content1->effect_tree_index());

  EXPECT_EQ(t0_id, content2->transform_tree_index());
  EXPECT_EQ(c1_id, content2->clip_tree_index());
  int mask_isolation_1_id = content2->effect_tree_index();
  const cc::EffectNode& mask_isolation_1 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_1_id);
  EXPECT_NE(mask_isolation_0_id, mask_isolation_1_id);
  ASSERT_EQ(e0_id, mask_isolation_1.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_1.blend_mode);
  EXPECT_TRUE(mask_isolation_1.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_1.mask_filter_info.rounded_corner_bounds());
  EXPECT_FALSE(mask_isolation_1.HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipAcrossChildEffect) {
  // This tests the case that an effect having the same output clip as the
  // layers before and after it can share the synthesized mask.
  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 5);
  auto* c1 = CreateClip(c0(), t0(), rrect);
  auto* e1 = CreateOpacityEffect(e0(), t0(), c1, 1,
                                 CompositingReason::kWillChangeOpacity);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, *e1)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //          content1
  // content0 [  e1  ] content2
  // [    mask_isolation_0    ]
  // [           e0           ]
  // Three content layers.
  ASSERT_EQ(3u, LayerCount());
  // There is still a "synthesized layer" but it's null.
  ASSERT_EQ(1u, SynthesizedClipLayerCount());
  EXPECT_FALSE(SynthesizedClipLayerAt(0));

  const cc::Layer* content0 = LayerAt(0);
  const cc::Layer* content1 = LayerAt(1);
  const cc::Layer* content2 = LayerAt(2);

  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);
  EXPECT_FALSE(mask_isolation_0.HasRenderSurface());

  EXPECT_EQ(c1_id, content1->clip_tree_index());
  int e1_id = content1->effect_tree_index();
  const cc::EffectNode& cc_e1 = *GetPropertyTrees().effect_tree().Node(e1_id);
  ASSERT_EQ(mask_isolation_0_id, cc_e1.parent_id);

  EXPECT_EQ(c1_id, content2->clip_tree_index());
  EXPECT_EQ(mask_isolation_0_id, content2->effect_tree_index());

  int e2_id = content2->effect_tree_index();
  const cc::EffectNode& cc_e2 = *GetPropertyTrees().effect_tree().Node(e2_id);
  EXPECT_TRUE(cc_e2.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_0.mask_filter_info.rounded_corner_bounds());
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipRespectOutputClip) {
  // This tests the case that a layer cannot share the synthesized mask despite
  // having the same composited rounded clip if it's enclosed by an effect not
  // clipped by the common clip.
  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 5);
  auto* c1 = CreateClip(c0(), t0(), rrect);

  CompositorFilterOperations non_trivial_filter;
  non_trivial_filter.AppendBlurFilter(5);
  auto* e1 = CreateFilterEffect(e0(), non_trivial_filter,
                                CompositingReason::kActiveFilterAnimation);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, *e1)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //                           content1
  //       content0      [ mask_isolation_1 ]      content2
  // [ mask_isolation_0 ][        e1        ][ mask_isolation_2  ]
  // [                            e0                             ]
  // Three content layers.
  ASSERT_EQ(3u, LayerCount());
  // There are still "synthesized layers" but they're null because they use
  // fast rounded corners.
  ASSERT_EQ(3u, SynthesizedClipLayerCount());
  EXPECT_FALSE(SynthesizedClipLayerAt(0));
  EXPECT_FALSE(SynthesizedClipLayerAt(1));
  EXPECT_FALSE(SynthesizedClipLayerAt(2));

  const cc::Layer* content0 = LayerAt(0);
  const cc::Layer* content1 = LayerAt(1);
  const cc::Layer* content2 = LayerAt(2);

  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);
  EXPECT_TRUE(mask_isolation_0.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_0.mask_filter_info.rounded_corner_bounds());
  EXPECT_FALSE(mask_isolation_0.HasRenderSurface());

  EXPECT_EQ(c1_id, content1->clip_tree_index());
  int mask_isolation_1_id = content1->effect_tree_index();
  const cc::EffectNode& mask_isolation_1 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_1_id);
  EXPECT_NE(mask_isolation_0_id, mask_isolation_1_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_1.blend_mode);
  int e1_id = mask_isolation_1.parent_id;
  const cc::EffectNode& cc_e1 = *GetPropertyTrees().effect_tree().Node(e1_id);
  ASSERT_EQ(e0_id, cc_e1.parent_id);
  EXPECT_TRUE(mask_isolation_1.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_1.mask_filter_info.rounded_corner_bounds());
  EXPECT_FALSE(mask_isolation_1.HasRenderSurface());

  EXPECT_EQ(c1_id, content2->clip_tree_index());
  int mask_isolation_2_id = content2->effect_tree_index();
  const cc::EffectNode& mask_isolation_2 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_2_id);
  EXPECT_NE(mask_isolation_0_id, mask_isolation_2_id);
  EXPECT_NE(mask_isolation_1_id, mask_isolation_2_id);
  ASSERT_EQ(e0_id, mask_isolation_2.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_2.blend_mode);
  EXPECT_TRUE(mask_isolation_2.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_2.mask_filter_info.rounded_corner_bounds());
  EXPECT_FALSE(mask_isolation_2.HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipDelegateBlending) {
  // This tests the case that an effect with exotic blending cannot share
  // the synthesized mask with its siblings because its blending has to be
  // applied by the outermost mask.
  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 5);
  auto* c1 = CreateClip(c0(), t0(), rrect);

  EffectPaintPropertyNode::State e1_state;
  e1_state.local_transform_space = &t0();
  e1_state.output_clip = c1;
  e1_state.blend_mode = SkBlendMode::kMultiply;
  e1_state.direct_compositing_reasons = CompositingReason::kWillChangeOpacity;
  auto* e1 = EffectPaintPropertyNode::Create(e0(), std::move(e1_state));

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, *e1)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //                     content1
  //       content0      [  e1  ]                     content2
  // [ mask_isolation_0 ][  mask_isolation_1   ][ mask_isolation_2  ]
  // [                              e0                              ]
  // Three content layers.
  ASSERT_EQ(3u, LayerCount());
  // There are still "synthesized layers" but they're null because they use
  // fast rounded corners.
  ASSERT_EQ(3u, SynthesizedClipLayerCount());
  EXPECT_FALSE(SynthesizedClipLayerAt(0));
  EXPECT_FALSE(SynthesizedClipLayerAt(1));
  EXPECT_FALSE(SynthesizedClipLayerAt(2));

  const cc::Layer* content0 = LayerAt(0);
  const cc::Layer* content1 = LayerAt(1);
  const cc::Layer* content2 = LayerAt(2);

  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);
  EXPECT_TRUE(mask_isolation_0.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_0.mask_filter_info.rounded_corner_bounds());

  EXPECT_EQ(c1_id, content1->clip_tree_index());
  int e1_id = content1->effect_tree_index();
  const cc::EffectNode& cc_e1 = *GetPropertyTrees().effect_tree().Node(e1_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, cc_e1.blend_mode);
  int mask_isolation_1_id = cc_e1.parent_id;
  const cc::EffectNode& mask_isolation_1 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_1_id);
  EXPECT_NE(mask_isolation_0_id, mask_isolation_1_id);
  ASSERT_EQ(e0_id, mask_isolation_1.parent_id);
  EXPECT_EQ(SkBlendMode::kMultiply, mask_isolation_1.blend_mode);
  EXPECT_TRUE(mask_isolation_1.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_1.mask_filter_info.rounded_corner_bounds());

  EXPECT_EQ(c1_id, content2->clip_tree_index());
  int mask_isolation_2_id = content2->effect_tree_index();
  const cc::EffectNode& mask_isolation_2 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_2_id);
  EXPECT_NE(mask_isolation_0_id, mask_isolation_2_id);
  EXPECT_NE(mask_isolation_1_id, mask_isolation_2_id);
  ASSERT_EQ(e0_id, mask_isolation_2.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);
  EXPECT_TRUE(mask_isolation_2.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_2.mask_filter_info.rounded_corner_bounds());
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipDelegateBackdropFilter) {
  // This tests the case that an effect with backdrop filter cannot share
  // the synthesized mask with its siblings because its backdrop filter has to
  // be applied by the outermost mask in the correct transform space.
  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 5);
  auto* c1 = CreateClip(c0(), t0(), rrect);
  auto* c2 = CreateClip(*c1, t0(), FloatRoundedRect(60, 60, 200, 100));

  auto* t1 = Create2DTranslation(t0(), 10, 20);
  CompositorFilterOperations blur_filter;
  blur_filter.AppendBlurFilter(5);
  auto* e1 = CreateBackdropFilterEffect(e0(), *t1, c2, blur_filter, 0.5f);

  TestPaintArtifact artifact;
  artifact.Chunk(*t1, *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(*t1, *c2, *e1)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(*t1, *c1, e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //                           content1
  //       content0      [        e1        ]      content2
  // [ mask_isolation_0 ][ mask_isolation_1 ][ mask_isolation_2  ]
  // [                            e0                             ]
  // Three content layers.
  ASSERT_EQ(3u, LayerCount());
  const cc::Layer* content0 = LayerAt(0);
  const cc::Layer* content1 = LayerAt(1);
  const cc::Layer* content2 = LayerAt(2);

  // Three synthesized layers, all are null because they use fast rounded
  // corners.
  ASSERT_EQ(3u, SynthesizedClipLayerCount());
  EXPECT_FALSE(SynthesizedClipLayerAt(0));
  EXPECT_FALSE(SynthesizedClipLayerAt(1));
  EXPECT_FALSE(SynthesizedClipLayerAt(2));

  int t1_id = content0->transform_tree_index();
  EXPECT_EQ(t0_id, GetPropertyTrees().transform_tree().Node(t1_id)->parent_id);
  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  EXPECT_EQ(t0_id, cc_c1.transform_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(t0_id, mask_isolation_0.transform_id);
  EXPECT_EQ(c0_id, mask_isolation_0.clip_id);
  EXPECT_TRUE(mask_isolation_0.backdrop_filters.IsEmpty());
  EXPECT_TRUE(mask_isolation_0.is_fast_rounded_corner);
  EXPECT_EQ(1.0f, mask_isolation_0.opacity);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_0.mask_filter_info.rounded_corner_bounds());

  EXPECT_EQ(t1_id, content1->transform_tree_index());
  int c2_id = content1->clip_tree_index();
  const cc::ClipNode& cc_c2 = *GetPropertyTrees().clip_tree().Node(c2_id);
  EXPECT_EQ(gfx::RectF(60, 60, 200, 100), cc_c2.clip);
  EXPECT_EQ(c1_id, cc_c2.parent_id);
  EXPECT_EQ(t0_id, cc_c2.transform_id);
  int e1_id = content1->effect_tree_index();
  const cc::EffectNode& cc_e1 = *GetPropertyTrees().effect_tree().Node(e1_id);
  EXPECT_TRUE(cc_e1.backdrop_filters.IsEmpty());
  EXPECT_EQ(1.0f, cc_e1.opacity);
  EXPECT_EQ(t1_id, cc_e1.transform_id);
  EXPECT_EQ(c2_id, cc_e1.clip_id);
  EXPECT_FALSE(cc_e1.backdrop_mask_element_id);

  int mask_isolation_1_id = cc_e1.parent_id;
  const cc::EffectNode& mask_isolation_1 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_1_id);
  EXPECT_NE(mask_isolation_0_id, mask_isolation_1_id);
  ASSERT_EQ(e0_id, mask_isolation_1.parent_id);
  EXPECT_EQ(t1_id, mask_isolation_1.transform_id);
  EXPECT_EQ(c2_id, mask_isolation_1.clip_id);
  EXPECT_FALSE(mask_isolation_1.backdrop_filters.IsEmpty());
  EXPECT_TRUE(mask_isolation_1.is_fast_rounded_corner);
  // Opacity should also be moved to mask_isolation_1.
  EXPECT_EQ(0.5f, mask_isolation_1.opacity);
  EXPECT_EQ(gfx::RRectF(40, 30, 300, 200, 5),
            mask_isolation_1.mask_filter_info.rounded_corner_bounds());

  EXPECT_EQ(t1_id, content2->transform_tree_index());
  EXPECT_EQ(c1_id, content2->clip_tree_index());
  int mask_isolation_2_id = content2->effect_tree_index();
  const cc::EffectNode& mask_isolation_2 =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_2_id);
  EXPECT_NE(mask_isolation_0_id, mask_isolation_2_id);
  EXPECT_NE(mask_isolation_1_id, mask_isolation_2_id);
  ASSERT_EQ(e0_id, mask_isolation_2.parent_id);
  EXPECT_EQ(t0_id, mask_isolation_2.transform_id);
  EXPECT_EQ(c0_id, mask_isolation_2.clip_id);
  EXPECT_TRUE(mask_isolation_2.backdrop_filters.IsEmpty());
  EXPECT_TRUE(mask_isolation_2.is_fast_rounded_corner);
  EXPECT_EQ(1.0f, mask_isolation_2.opacity);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation_2.mask_filter_info.rounded_corner_bounds());
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipMultipleNonBackdropEffects) {
  // This tests the case that multiple non-backdrop effects can share the
  // synthesized mask.
  FloatRoundedRect rrect(gfx::RectF(50, 50, 300, 200), 5);
  auto* c1 = CreateClip(c0(), t0(), rrect);
  auto* c2 = CreateClip(*c1, t0(), FloatRoundedRect(60, 60, 200, 100));

  auto* e1 = CreateOpacityEffect(e0(), t0(), c2, 0.5,
                                 CompositingReason::kWillChangeOpacity);
  auto* e2 = CreateOpacityEffect(e0(), t0(), c1, 0.75,
                                 CompositingReason::kWillChangeOpacity);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c2, *e1)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, *e2)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //  content0  content1  content2
  // [   e1   ][   e2   ]
  // [  mask_isolation  ]
  // [             e0             ]
  // Three content layers.
  ASSERT_EQ(3u, LayerCount());
  const cc::Layer* content0 = LayerAt(0);
  const cc::Layer* content1 = LayerAt(1);
  const cc::Layer* content2 = LayerAt(2);

  // One synthesized layer, which is null because it uses fast rounded corners.
  ASSERT_EQ(1u, SynthesizedClipLayerCount());
  EXPECT_FALSE(SynthesizedClipLayerAt(0));

  int c2_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c2 = *GetPropertyTrees().clip_tree().Node(c2_id);
  int e1_id = content0->effect_tree_index();
  const cc::EffectNode& cc_e1 = *GetPropertyTrees().effect_tree().Node(e1_id);
  int c1_id = content1->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree().Node(c1_id);
  int e2_id = content1->effect_tree_index();
  const cc::EffectNode& cc_e2 = *GetPropertyTrees().effect_tree().Node(e2_id);
  int mask_isolation_id = cc_e1.parent_id;
  const cc::EffectNode& mask_isolation =
      *GetPropertyTrees().effect_tree().Node(mask_isolation_id);

  EXPECT_EQ(c2_id, cc_e1.clip_id);
  EXPECT_EQ(0.5f, cc_e1.opacity);
  EXPECT_EQ(gfx::RectF(60, 60, 200, 100), cc_c2.clip);
  ASSERT_EQ(c1_id, cc_c2.parent_id);

  EXPECT_EQ(c1_id, cc_e2.clip_id);
  EXPECT_EQ(mask_isolation_id, cc_e2.parent_id);
  EXPECT_EQ(0.75f, cc_e2.opacity);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);

  ASSERT_EQ(e0_id, mask_isolation.parent_id);
  EXPECT_EQ(c0_id, mask_isolation.clip_id);
  EXPECT_TRUE(mask_isolation.is_fast_rounded_corner);
  EXPECT_EQ(gfx::RRectF(50, 50, 300, 200, 5),
            mask_isolation.mask_filter_info.rounded_corner_bounds());

  EXPECT_EQ(c0_id, content2->clip_tree_index());
  EXPECT_EQ(e0_id, content2->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, WillBeRemovedFromFrame) {
  auto* effect = CreateSampleEffectNodeWithElementId();
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(gfx::Rect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(1u, LayerCount());
  WillBeRemovedFromFrame();
  // We would need a fake or mock LayerTreeHost to validate that we
  // unregister all element ids, so just check layer count for now.
  EXPECT_EQ(0u, LayerCount());
}

TEST_P(PaintArtifactCompositorTest, SolidColor) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .RectDrawing(gfx::Rect(100, 200, 300, 400), Color::kBlack)
      .IsSolidColor();
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());
  auto* layer = LayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(100, 200), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(300, 400), layer->bounds());
  EXPECT_TRUE(layer->draws_content());
  EXPECT_TRUE(LayerAt(0)->IsSolidColorLayerForTesting());
  EXPECT_EQ(SkColors::kBlack, layer->background_color());
}

TEST_P(PaintArtifactCompositorTest, ContentsNonOpaque) {
  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kBlack);
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_FALSE(LayerAt(0)->contents_opaque());
}

TEST_P(PaintArtifactCompositorTest, ContentsOpaque) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 200, 200));
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_TRUE(LayerAt(0)->contents_opaque());
}

TEST_P(PaintArtifactCompositorTest, ContentsOpaqueUnitedNonOpaque) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .RectDrawing(gfx::Rect(100, 100, 210, 210), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
      .Chunk()
      .RectDrawing(gfx::Rect(200, 200, 200, 200), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(200, 200, 200, 200));
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_EQ(gfx::Size(300, 300), LayerAt(0)->bounds());
  EXPECT_FALSE(LayerAt(0)->contents_opaque());
}

TEST_P(PaintArtifactCompositorTest, ContentsOpaqueUnitedClippedToOpaque) {
  // Almost the same as ContentsOpaqueUnitedNonOpaque, but with a clip which
  // removes the non-opaque part of the layer, making the layer opaque.
  auto* clip1 = CreateClip(c0(), t0(), FloatRoundedRect(175, 175, 100, 100));
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clip1, e0())
      .RectDrawing(gfx::Rect(100, 100, 250, 250), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
      .Chunk(t0(), *clip1, e0())
      .RectDrawing(gfx::Rect(200, 200, 300, 300), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(200, 200, 200, 200));
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_EQ(gfx::Size(100, 100), LayerAt(0)->bounds());
  EXPECT_TRUE(LayerAt(0)->contents_opaque());
}

TEST_P(PaintArtifactCompositorTest, ContentsOpaqueUnitedOpaque1) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .RectDrawing(gfx::Rect(100, 100, 300, 300), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 300, 300))
      .Chunk()
      .RectDrawing(gfx::Rect(200, 200, 200, 200), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(200, 200, 200, 200));
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_EQ(gfx::Size(300, 300), LayerAt(0)->bounds());
  EXPECT_TRUE(LayerAt(0)->contents_opaque());
}

TEST_P(PaintArtifactCompositorTest, ContentsOpaqueUnitedWithRoundedClip) {
  // Almost the same as ContentsOpaqueUnitedOpaque1, but the first layer has a
  // rounded clip.
  auto* clip1 = CreateClip(c0(), t0(),
                           FloatRoundedRect(gfx::RectF(175, 175, 100, 100), 5));
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clip1, e0())
      .RectDrawing(gfx::Rect(100, 100, 210, 210), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 210, 210))
      .Chunk(t0(), c0(), e0())
      .RectDrawing(gfx::Rect(200, 200, 100, 100), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(200, 200, 100, 100));
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_EQ(gfx::Size(125, 125), LayerAt(0)->bounds());
  EXPECT_FALSE(LayerAt(0)->contents_opaque());
}

TEST_P(PaintArtifactCompositorTest, ContentsOpaqueUnitedOpaque2) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .RectDrawing(gfx::Rect(100, 100, 200, 200), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 200, 200))
      .Chunk()
      .RectDrawing(gfx::Rect(100, 100, 300, 300), Color::kBlack)
      .RectKnownToBeOpaque(gfx::Rect(100, 100, 300, 300));
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_EQ(gfx::Size(300, 300), LayerAt(0)->bounds());
  EXPECT_TRUE(LayerAt(0)->contents_opaque());
}

TEST_P(PaintArtifactCompositorTest, DecompositeEffectWithNoOutputClip) {
  // This test verifies effect nodes with no output clip correctly decomposites
  // if there is no compositing reasons.
  auto* clip1 = CreateClip(c0(), t0(), FloatRoundedRect(75, 75, 100, 100));
  auto* effect1 = CreateOpacityEffect(e0(), t0(), nullptr, 0.5);

  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(gfx::Rect(50, 50, 100, 100), Color::kGray);
  artifact.Chunk(t0(), *clip1, *effect1)
      .RectDrawing(gfx::Rect(100, 100, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());

  const cc::Layer* layer = LayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 50.f), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(125, 125), layer->bounds());
  EXPECT_EQ(1, layer->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, CompositedEffectWithNoOutputClip) {
  // This test verifies effect nodes with no output clip but has compositing
  // reason correctly squash children chunks and assign clip node.
  auto* clip1 = CreateClip(c0(), t0(), FloatRoundedRect(75, 75, 100, 100));

  auto* effect1 =
      CreateOpacityEffect(e0(), t0(), nullptr, 0.5, CompositingReason::kAll);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *effect1)
      .RectDrawing(gfx::Rect(50, 50, 100, 100), Color::kGray);
  artifact.Chunk(t0(), *clip1, *effect1)
      .RectDrawing(gfx::Rect(100, 100, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());

  const cc::Layer* layer = LayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 50.f), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(125, 125), layer->bounds());
  EXPECT_EQ(1, layer->clip_tree_index());
  EXPECT_EQ(2, layer->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, LayerRasterInvalidationWithClip) {
  cc::FakeImplTaskRunnerProvider task_runner_provider_;
  cc::TestTaskGraphRunner task_graph_runner_;
  cc::FakeLayerTreeHostImpl host_impl(&task_runner_provider_,
                                      &task_graph_runner_);
  host_impl.EnsureSyncTree();

  // The layer's painting is initially not clipped.
  auto* clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 20, 300, 400));
  TestPaintArtifact artifact1;
  artifact1.Chunk(t0(), *clip, e0())
      .RectDrawing(gfx::Rect(50, 50, 200, 200), Color::kBlack);
  artifact1.Client(0).Validate();
  artifact1.Client(1).Validate();
  Update(artifact1.Build());
  ASSERT_EQ(1u, LayerCount());

  auto* layer = LayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50, 50), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(200, 200), layer->bounds());
  EXPECT_THAT(
      layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 200, 200), Color::kBlack)));

  // The layer's painting overflows the left, top, right edges of the clip.
  auto& artifact2 = TestPaintArtifact()
                        .Chunk(artifact1.Client(0))
                        .Properties(t0(), *clip, e0())
                        .RectDrawing(artifact1.Client(1),
                                     gfx::Rect(0, 0, 400, 200), Color::kBlack)
                        .Build();
  // Simluate commit to the compositor thread.
  // When doing a full commit, we would call
  // layer_tree_host_->ActivateCommitState() and the second argument would come
  // from layer_tree_host_->active_commit_state(); we use pending_commit_state()
  // just to keep the test code simple.
  layer->PushPropertiesTo(
      layer->CreateLayerImpl(host_impl.sync_tree()).get(),
      *const_cast<const cc::LayerTreeHost&>(GetLayerTreeHost())
           .pending_commit_state(),
      const_cast<const cc::LayerTreeHost&>(GetLayerTreeHost())
          .thread_unsafe_commit_state());
  Update(artifact2);
  ASSERT_EQ(1u, LayerCount());
  ASSERT_EQ(layer, LayerAt(0));

  // Invalidate the first chunk because its transform in layer changed.
  EXPECT_EQ(gfx::Rect(0, 0, 300, 180), layer->update_rect());
  EXPECT_EQ(gfx::Vector2dF(10, 20), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(300, 180), layer->bounds());
  EXPECT_THAT(
      layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 390, 180), Color::kBlack)));

  // The layer's painting overflows all edges of the clip.
  auto& artifact3 =
      TestPaintArtifact()
          .Chunk(artifact1.Client(0))
          .Properties(t0(), *clip, e0())
          .RectDrawing(artifact1.Client(1), gfx::Rect(-100, -200, 500, 800),
                       Color::kBlack)
          .Build();
  // Simluate commit to the compositor thread.
  layer->PushPropertiesTo(
      layer->CreateLayerImpl(host_impl.sync_tree()).get(),
      *const_cast<const cc::LayerTreeHost&>(GetLayerTreeHost())
           .pending_commit_state(),
      const_cast<const cc::LayerTreeHost&>(GetLayerTreeHost())
          .thread_unsafe_commit_state());
  Update(artifact3);
  ASSERT_EQ(1u, LayerCount());
  ASSERT_EQ(layer, LayerAt(0));

  // We should not invalidate the layer because the origin didn't change
  // because of the clip.
  EXPECT_EQ(gfx::Rect(), layer->update_rect());
  EXPECT_EQ(gfx::Vector2dF(10, 20), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(300, 400), layer->bounds());
  EXPECT_THAT(
      layer->GetPicture(),
      Pointee(DrawsRectangle(gfx::RectF(0, 0, 390, 580), Color::kBlack)));
}

// Test that PaintArtifactCompositor creates the correct nodes for the visual
// viewport's page scale and scroll layers to support pinch-zooming.
TEST_P(PaintArtifactCompositorTest, CreatesViewportNodes) {
  auto matrix = MakeScaleMatrix(2);
  TransformPaintPropertyNode::State transform_state{{matrix}};
  transform_state.in_subtree_of_page_scale = false;
  const CompositorElementId compositor_element_id =
      CompositorElementIdFromUniqueObjectId(1);
  transform_state.compositor_element_id = compositor_element_id;

  auto* scale_transform_node = TransformPaintPropertyNode::Create(
      TransformPaintPropertyNode::Root(), std::move(transform_state));

  TestPaintArtifact artifact;
  ViewportProperties viewport_properties;
  viewport_properties.page_scale = scale_transform_node;
  Update(artifact.Build(), viewport_properties);

  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree();
  const cc::TransformNode* cc_transform_node =
      transform_tree.FindNodeFromElementId(compositor_element_id);
  EXPECT_TRUE(cc_transform_node);
  EXPECT_EQ(matrix, cc_transform_node->local);
  EXPECT_EQ(gfx::Point3F(), cc_transform_node->origin);
}

// Test that |cc::TransformNode::in_subtree_of_page_scale_layer| is not set on
// the page scale transform node or ancestors, and is set on descendants.
TEST_P(PaintArtifactCompositorTest, InSubtreeOfPageScale) {
  TransformPaintPropertyNode::State ancestor_transform_state;
  ancestor_transform_state.in_subtree_of_page_scale = false;
  auto* ancestor_transform = TransformPaintPropertyNode::Create(
      TransformPaintPropertyNode::Root(), std::move(ancestor_transform_state));

  TransformPaintPropertyNode::State page_scale_transform_state;
  page_scale_transform_state.in_subtree_of_page_scale = false;
  const CompositorElementId page_scale_compositor_element_id =
      CompositorElementIdFromUniqueObjectId(1);
  page_scale_transform_state.compositor_element_id =
      page_scale_compositor_element_id;
  auto* page_scale_transform = TransformPaintPropertyNode::Create(
      *ancestor_transform, std::move(page_scale_transform_state));

  TransformPaintPropertyNode::State descendant_transform_state;
  const CompositorElementId descendant_compositor_element_id =
      CompositorElementIdFromUniqueObjectId(2);
  descendant_transform_state.compositor_element_id =
      descendant_compositor_element_id;
  descendant_transform_state.in_subtree_of_page_scale = true;
  descendant_transform_state.direct_compositing_reasons =
      CompositingReason::kWillChangeTransform;
  auto* descendant_transform = TransformPaintPropertyNode::Create(
      *page_scale_transform, std::move(descendant_transform_state));

  TestPaintArtifact artifact;
  artifact.Chunk(*descendant_transform, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 10, 10), Color::kBlack);
  ViewportProperties viewport_properties;
  viewport_properties.page_scale = page_scale_transform;
  Update(artifact.Build(), viewport_properties);

  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree();
  const auto* cc_page_scale_transform =
      transform_tree.FindNodeFromElementId(page_scale_compositor_element_id);
  // The page scale node is not in a subtree of the page scale layer.
  EXPECT_FALSE(cc_page_scale_transform->in_subtree_of_page_scale_layer);

  // Ancestors of the page scale node are not in a page scale subtree.
  auto cc_ancestor_id = cc_page_scale_transform->parent_id;
  while (cc_ancestor_id != cc::kInvalidPropertyNodeId) {
    const auto* ancestor = transform_tree.Node(cc_ancestor_id);
    EXPECT_FALSE(ancestor->in_subtree_of_page_scale_layer);
    cc_ancestor_id = ancestor->parent_id;
  }

  // Descendants of the page scale node should be in the page scale subtree.
  const auto* cc_descendant_transform =
      transform_tree.FindNodeFromElementId(descendant_compositor_element_id);
  EXPECT_TRUE(cc_descendant_transform->in_subtree_of_page_scale_layer);
}

// Test that PaintArtifactCompositor pushes page scale to the transform tree.
TEST_P(PaintArtifactCompositorTest, ViewportPageScale) {
  // Create a page scale transform node with a page scale factor of 2.0.
  TransformPaintPropertyNode::State transform_state{{MakeScaleMatrix(2)}};
  transform_state.in_subtree_of_page_scale = false;
  transform_state.compositor_element_id =
      CompositorElementIdFromUniqueObjectId(1);
  auto* scale_transform_node = TransformPaintPropertyNode::Create(
      TransformPaintPropertyNode::Root(), std::move(transform_state));

  // Create a viewport scroll node with container size 20x10 and contents size
  // 27x32.
  ScrollPaintPropertyNode::State scroll_state;
  scroll_state.container_rect = gfx::Rect(5, 5, 20, 10);
  scroll_state.contents_size = gfx::Size(27, 32);
  scroll_state.user_scrollable_vertical = true;
  scroll_state.max_scroll_offset_affected_by_page_scale = true;
  auto scroll_element_id = CompositorElementIdFromUniqueObjectId(
      NewUniqueObjectId(), CompositorElementIdNamespace::kScroll);
  scroll_state.compositor_element_id = scroll_element_id;

  auto* scroll = ScrollPaintPropertyNode::Create(
      ScrollPaintPropertyNode::Root(), std::move(scroll_state));
  auto* scroll_translation =
      CreateScrollTranslation(*scale_transform_node, 0, 0, *scroll);

  TestPaintArtifact artifact;
  artifact.Chunk(*scroll_translation, c0(), e0())
      .RectDrawing(gfx::Rect(0, 0, 10, 10), Color::kBlack);
  ViewportProperties viewport_properties;
  viewport_properties.page_scale = scale_transform_node;
  Update(artifact.Build(), viewport_properties);

  cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree_mutable();
  cc::ScrollNode* cc_scroll_node =
      scroll_tree.FindNodeFromElementId(scroll_element_id);
  auto max_scroll_offset = scroll_tree.MaxScrollOffset(cc_scroll_node->id);
  // The max scroll offset should be scaled by the page scale factor (see:
  // |ScrollTree::MaxScrollOffset|). This adjustment scales the contents from
  // 27x32 to 54x64 so the max scroll offset becomes (54-20)/2 x (64-10)/2.
  EXPECT_EQ(gfx::PointF(17, 27), max_scroll_offset);
}

enum {
  kNoRenderSurface = 0,
  kHasRenderSurface = 1 << 0,
};

#define EXPECT_OPACITY(effect_id, expected_opacity, expected_flags)        \
  do {                                                                     \
    const auto* effect = GetPropertyTrees().effect_tree().Node(effect_id); \
    EXPECT_EQ(expected_opacity, effect->opacity);                          \
    EXPECT_EQ(!!((expected_flags)&kHasRenderSurface),                      \
              effect->HasRenderSurface());                                 \
  } while (false)

TEST_P(PaintArtifactCompositorTest, OpacityRenderSurfaces) {
  //            e
  //         /  |  \
  //       a    b    c -- L4
  //     / \   / \    \
  //    aa ab L2 L3   ca          (L = layer)
  //    |   |          |
  //   L0  L1         L5
  auto* e = CreateOpacityEffect(e0(), 0.1f);
  auto* a = CreateOpacityEffect(*e, 0.2f);
  auto* b =
      CreateOpacityEffect(*e, 0.3f, CompositingReason::kWillChangeOpacity);
  auto* c =
      CreateOpacityEffect(*e, 0.4f, CompositingReason::kWillChangeOpacity);
  auto* aa =
      CreateOpacityEffect(*a, 0.5f, CompositingReason::kWillChangeOpacity);
  auto* ab =
      CreateOpacityEffect(*a, 0.6f, CompositingReason::kWillChangeOpacity);
  auto* ca =
      CreateOpacityEffect(*c, 0.7f, CompositingReason::kWillChangeOpacity);
  auto* t = CreateTransform(t0(), MakeRotationMatrix(90), gfx::Point3F(),
                            CompositingReason::k3DTransform);

  TestPaintArtifact artifact;
  gfx::Rect r(150, 150, 100, 100);
  artifact.Chunk(t0(), c0(), *aa).RectDrawing(r, Color::kWhite);
  artifact.Chunk(t0(), c0(), *ab).RectDrawing(r, Color::kWhite);
  artifact.Chunk(t0(), c0(), *b).RectDrawing(r, Color::kWhite);
  artifact.Chunk(*t, c0(), *b).RectDrawing(r, Color::kWhite);
  artifact.Chunk(t0(), c0(), *c).RectDrawing(r, Color::kWhite);
  artifact.Chunk(t0(), c0(), *ca).RectDrawing(r, Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(6u, LayerCount());

  int effect_ids[6];
  for (size_t i = 0; i < LayerCount(); i++)
    effect_ids[i] = LayerAt(i)->effect_tree_index();

  // Effects of layer 0, 1, 5 each has one compositing layer, so don't have
  // render surface.
  EXPECT_OPACITY(effect_ids[0], 0.5f, kNoRenderSurface);
  EXPECT_OPACITY(effect_ids[1], 0.6f, kNoRenderSurface);
  EXPECT_OPACITY(effect_ids[5], 0.7f, kNoRenderSurface);

  // Layer 2 and 3 have the same effect state. The effect has render surface
  // because it has two compositing layers.
  EXPECT_EQ(effect_ids[2], effect_ids[3]);
  EXPECT_OPACITY(effect_ids[2], 0.3f, kHasRenderSurface);

  // Effect |a| has two indirect compositing layers, so has render surface.
  const auto& effect_tree = GetPropertyTrees().effect_tree();
  int id_a = effect_tree.Node(effect_ids[0])->parent_id;
  EXPECT_EQ(id_a, effect_tree.Node(effect_ids[1])->parent_id);
  EXPECT_OPACITY(id_a, 0.2f, kHasRenderSurface);

  // Effect |c| has one direct and one indirect compositing layers, so has
  // render surface.
  EXPECT_OPACITY(effect_ids[4], 0.4f, kHasRenderSurface);

  // |e| has render surface because it has 3 child render surfaces.
  EXPECT_OPACITY(effect_tree.Node(effect_ids[4])->parent_id, 0.1f,
                 kHasRenderSurface);
}

TEST_P(PaintArtifactCompositorTest, OpacityRenderSurfacesWithFilterChildren) {
  auto* opacity = CreateOpacityEffect(e0(), 0.1f);
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(5);
  auto* filter1 = CreateFilterEffect(*opacity, filter,
                                     CompositingReason::kActiveFilterAnimation);
  auto* filter2 = CreateFilterEffect(*opacity, filter,
                                     CompositingReason::kActiveFilterAnimation);

  gfx::Rect r(150, 150, 100, 100);
  Update(TestPaintArtifact()
             .Chunk(t0(), c0(), *filter1)
             .RectDrawing(r, Color::kWhite)
             .Chunk(t0(), c0(), *filter2)
             .RectDrawing(r, Color::kWhite)
             .Build());
  ASSERT_EQ(2u, LayerCount());
  auto filter_id1 = LayerAt(0)->effect_tree_index();
  auto filter_id2 = LayerAt(1)->effect_tree_index();
  EXPECT_OPACITY(filter_id1, 1.f, kHasRenderSurface);
  EXPECT_OPACITY(filter_id2, 1.f, kHasRenderSurface);
  EXPECT_OPACITY(GetPropertyTrees().effect_tree().Node(filter_id1)->parent_id,
                 0.1f, kHasRenderSurface);
}

TEST_P(PaintArtifactCompositorTest, OpacityAnimationRenderSurfaces) {
  // The topologies of the effect tree and layer tree are the same as
  // OpacityRencerSurfaces, except that the layers all have 1.f opacity and
  // active opacity animations.
  //            e
  //         /  |  \
  //       a    b    c -- L4
  //     / \   / \    \
  //    aa ab L2 L3   ca          (L = layer)
  //    |   |          |
  //   L0  L1         L5
  auto* e = CreateAnimatingOpacityEffect(e0());
  auto* a = CreateAnimatingOpacityEffect(*e);
  auto* b = CreateAnimatingOpacityEffect(*e);
  auto* c = CreateAnimatingOpacityEffect(*e);
  auto* aa = CreateAnimatingOpacityEffect(*a);
  auto* ab = CreateAnimatingOpacityEffect(*a);
  auto* ca = CreateAnimatingOpacityEffect(*c);
  auto* t = CreateTransform(t0(), MakeRotationMatrix(90), gfx::Point3F(),
                            CompositingReason::k3DTransform);

  TestPaintArtifact artifact;
  gfx::Rect r(150, 150, 100, 100);
  artifact.Chunk(t0(), c0(), *aa).RectDrawing(r, Color::kWhite);
  artifact.Chunk(t0(), c0(), *ab).RectDrawing(r, Color::kWhite);
  artifact.Chunk(t0(), c0(), *b).RectDrawing(r, Color::kWhite);
  artifact.Chunk(*t, c0(), *b).RectDrawing(r, Color::kWhite);
  artifact.Chunk(t0(), c0(), *c).RectDrawing(r, Color::kWhite);
  artifact.Chunk(t0(), c0(), *ca).RectDrawing(r, Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(6u, LayerCount());

  int effect_ids[6];
  for (size_t i = 0; i < LayerCount(); i++)
    effect_ids[i] = LayerAt(i)->effect_tree_index();

  // Effects of layer 0, 1, 5 each has one compositing layer, so don't have
  // render surface.
  EXPECT_OPACITY(effect_ids[0], 1.f, kNoRenderSurface);
  EXPECT_OPACITY(effect_ids[1], 1.f, kNoRenderSurface);
  EXPECT_OPACITY(effect_ids[5], 1.f, kNoRenderSurface);

  // Layer 2 and 3 have the same effect state. The effect has render surface
  // because it has two compositing layers.
  EXPECT_EQ(effect_ids[2], effect_ids[3]);
  EXPECT_OPACITY(effect_ids[2], 1.f, kHasRenderSurface);

  const auto& effect_tree = GetPropertyTrees().effect_tree();
  int id_a = effect_tree.Node(effect_ids[0])->parent_id;
  EXPECT_EQ(id_a, effect_tree.Node(effect_ids[1])->parent_id);
  EXPECT_OPACITY(id_a, 1.f, kHasRenderSurface);

  // Effect |c| has one direct and one indirect compositing layers, so has
  // render surface.
  EXPECT_OPACITY(effect_ids[4], 1.f, kHasRenderSurface);
  EXPECT_OPACITY(effect_tree.Node(effect_ids[4])->parent_id, 1.f,
                 kHasRenderSurface);
}

TEST_P(PaintArtifactCompositorTest, OpacityRenderSurfacesWithBackdropChildren) {
  // Opacity effect with a single compositing backdrop-filter child. Normally
  // the opacity effect would not get a render surface. However, because
  // backdrop-filter needs to only filter up to the backdrop root, it always
  // gets a render surface.
  auto* e = CreateOpacityEffect(e0(), 0.4f);
  auto* a = CreateOpacityEffect(*e, 0.5f);
  CompositorFilterOperations blur_filter;
  blur_filter.AppendBlurFilter(5);
  auto* bd = CreateBackdropFilterEffect(*a, blur_filter);

  TestPaintArtifact artifact;
  gfx::Rect r(150, 150, 100, 100);
  artifact.Chunk(t0(), c0(), *a).RectDrawing(r, Color::kWhite);
  artifact.Chunk(t0(), c0(), *bd).RectDrawing(r, Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(2u, LayerCount());

  EXPECT_OPACITY(LayerAt(0)->effect_tree_index(), 0.5, kHasRenderSurface);
  EXPECT_OPACITY(LayerAt(1)->effect_tree_index(), 1.0, kHasRenderSurface);
}

TEST_P(PaintArtifactCompositorTest,
       DirectTransformAnimationCausesRenderSurfaceFor2dAxisMisalignedClip) {
  // When a clip is affected by an animated transform, we should get a render
  // surface for the effect node.
  auto* t1 = CreateAnimatingTransform(t0());
  auto* e1 = CreateOpacityEffect(e0(), *t1, nullptr, 1.f);
  auto* c1 = CreateClip(c0(), t0(), FloatRoundedRect(50, 50, 50, 50));
  TestPaintArtifact artifact;
  gfx::Rect r(150, 150, 100, 100);
  artifact.Chunk(t0(), c0(), e0()).RectDrawing(r, Color::kWhite);
  artifact.Chunk(t0(), *c1, *e1).RectDrawing(r, Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(2u, LayerCount());

  const auto* effect =
      GetPropertyTrees().effect_tree().Node(LayerAt(1)->effect_tree_index());
  EXPECT_TRUE(effect->HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest,
       IndirectTransformAnimationCausesRenderSurfaceFor2dAxisMisalignedClip) {
  // When a clip is affected by an animated transform, we should get a render
  // surface for the effect node.
  auto* t1 = CreateAnimatingTransform(t0());
  auto* t2 = Create2DTranslation(*t1, 10, 20);
  auto* e1 = CreateOpacityEffect(e0(), *t2, nullptr, 1.f);
  auto* c1 = CreateClip(c0(), t0(), FloatRoundedRect(50, 50, 50, 50));
  TestPaintArtifact artifact;
  gfx::Rect r(150, 150, 100, 100);
  artifact.Chunk(t0(), c0(), e0()).RectDrawing(r, Color::kWhite);
  artifact.Chunk(t0(), *c1, *e1).RectDrawing(r, Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(2u, LayerCount());

  const auto* effect =
      GetPropertyTrees().effect_tree().Node(LayerAt(1)->effect_tree_index());
  EXPECT_TRUE(effect->HasRenderSurface());
}

TEST_P(PaintArtifactCompositorTest, OpacityIndirectlyAffectingTwoLayers) {
  auto* opacity = CreateOpacityEffect(e0(), 0.5f);
  auto* child_composited_transform = CreateTransform(
      t0(), gfx::Transform(), gfx::Point3F(), CompositingReason::k3DTransform);
  auto* grandchild_composited_transform =
      CreateTransform(*child_composited_transform, gfx::Transform(),
                      gfx::Point3F(), CompositingReason::k3DTransform);
  auto* child_effect = CreateOpacityEffect(*opacity, 1.f);
  auto* grandchild_effect = CreateOpacityEffect(*child_effect, 1.f);

  TestPaintArtifact artifact;
  artifact.Chunk(*child_composited_transform, c0(), *child_effect)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite);
  artifact.Chunk(*grandchild_composited_transform, c0(), *grandchild_effect)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(2u, LayerCount());

  const auto& effect_tree = GetPropertyTrees().effect_tree();
  int layer0_effect_id = LayerAt(0)->effect_tree_index();
  EXPECT_OPACITY(layer0_effect_id, 1.f, kNoRenderSurface);
  int layer1_effect_id = LayerAt(1)->effect_tree_index();
  EXPECT_OPACITY(layer1_effect_id, 1.f, kNoRenderSurface);
  // |opacity| affects both layer0 and layer1 which don't have render surfaces,
  // so it should have a render surface.
  int opacity_id = effect_tree.Node(layer0_effect_id)->parent_id;
  EXPECT_OPACITY(opacity_id, 0.5f, kHasRenderSurface);
}

TEST_P(PaintArtifactCompositorTest, WillChangeOpacityRenderSurfaceWithLayer) {
  auto* opacity =
      CreateOpacityEffect(e0(), 1.f, CompositingReason::kWillChangeOpacity);
  auto* child_composited_transform = CreateTransform(
      t0(), gfx::Transform(), gfx::Point3F(), CompositingReason::k3DTransform);
  auto* child_effect = CreateOpacityEffect(*opacity, 1.f);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *opacity)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(*child_composited_transform, c0(), *child_effect)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(2u, LayerCount());

  int layer0_effect_id = LayerAt(0)->effect_tree_index();
  EXPECT_OPACITY(layer0_effect_id, 1.f, kNoRenderSurface);
  int layer1_effect_id = LayerAt(1)->effect_tree_index();
  // TODO(crbug.com/1285498): Optimize for will-change: opacity.
  // EXPECT_OPACITY(layer1_effect_id, 1.f, kHasRenderSurface);
  EXPECT_OPACITY(layer1_effect_id, 1.f, kNoRenderSurface);
}

TEST_P(PaintArtifactCompositorTest,
       WillChangeOpacityRenderSurfaceWithoutLayer) {
  auto* opacity =
      CreateOpacityEffect(e0(), 1.f, CompositingReason::kWillChangeOpacity);
  auto* child_composited_transform = CreateTransform(
      t0(), gfx::Transform(), gfx::Point3F(), CompositingReason::k3DTransform);
  auto* grandchild_composited_transform =
      CreateTransform(*child_composited_transform, gfx::Transform(),
                      gfx::Point3F(), CompositingReason::k3DTransform);
  auto* child_effect = CreateOpacityEffect(*opacity, 1.f);
  auto* grandchild_effect = CreateOpacityEffect(*child_effect, 1.f);

  TestPaintArtifact artifact;
  artifact.Chunk(*child_composited_transform, c0(), *child_effect)
      .RectDrawing(gfx::Rect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(*grandchild_composited_transform, c0(), *grandchild_effect)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(2u, LayerCount());

  const auto& effect_tree = GetPropertyTrees().effect_tree();
  int layer0_effect_id = LayerAt(0)->effect_tree_index();
  EXPECT_OPACITY(layer0_effect_id, 1.f, kNoRenderSurface);
  int layer1_effect_id = LayerAt(1)->effect_tree_index();
  EXPECT_OPACITY(layer1_effect_id, 1.f, kNoRenderSurface);
  int opacity_id = effect_tree.Node(layer0_effect_id)->parent_id;
  // TODO(crbug.com/1285498): Optimize for will-change: opacity.
  // |opacity| affects both layer0 and layer1 which don't have render surfaces,
  // so it should have a render surface if we have the optimization for
  // will-change:opacity.
  // EXPECT_OPACITY(opacity_id, 1.f, kHasRenderSurface);
  EXPECT_OPACITY(opacity_id, 1.f, kNoRenderSurface);
}

TEST_P(PaintArtifactCompositorTest,
       OpacityIndirectlyAffectingTwoLayersWithOpacityAnimations) {
  auto* opacity = CreateAnimatingOpacityEffect(e0());
  auto* child_composited_effect = CreateAnimatingOpacityEffect(*opacity);
  auto* grandchild_composited_effect =
      CreateAnimatingOpacityEffect(*child_composited_effect);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *child_composited_effect)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite);
  artifact.Chunk(t0(), c0(), *grandchild_composited_effect)
      .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(2u, LayerCount());

  const auto& effect_tree = GetPropertyTrees().effect_tree();
  // layer0's opacity animation needs a render surface because it affects
  // both layer0 and layer1.
  int layer0_effect_id = LayerAt(0)->effect_tree_index();
  EXPECT_OPACITY(layer0_effect_id, 1.f, kHasRenderSurface);
  // layer1's opacity animation doesn't need a render surface because it
  // affects layer1 only.
  int layer1_effect_id = LayerAt(1)->effect_tree_index();
  EXPECT_OPACITY(layer1_effect_id, 1.f, kNoRenderSurface);
  // Though |opacity| affects both layer0 and layer1, layer0's effect has
  // render surface, so |opacity| doesn't need a render surface.
  int opacity_id = effect_tree.Node(layer0_effect_id)->parent_id;
  EXPECT_OPACITY(opacity_id, 1.f, kNoRenderSurface);
}

TEST_P(PaintArtifactCompositorTest, FilterCreatesRenderSurface) {
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(5);
  auto* e1 = CreateFilterEffect(e0(), filter,
                                CompositingReason::kActiveFilterAnimation);
  Update(TestPaintArtifact()
             .Chunk(t0(), c0(), *e1)
             .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite)
             .Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_OPACITY(LayerAt(0)->effect_tree_index(), 1.f, kHasRenderSurface);
}

TEST_P(PaintArtifactCompositorTest, WillChangeFilterCreatesRenderSurface) {
  auto* e1 = CreateFilterEffect(e0(), CompositorFilterOperations(),
                                CompositingReason::kWillChangeFilter);
  Update(TestPaintArtifact()
             .Chunk(t0(), c0(), *e1)
             .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite)
             .Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_OPACITY(LayerAt(0)->effect_tree_index(), 1.f, kHasRenderSurface);
}

TEST_P(PaintArtifactCompositorTest, FilterAnimationCreatesRenderSurface) {
  auto* e1 = CreateAnimatingFilterEffect(e0());
  Update(TestPaintArtifact()
             .Chunk(t0(), c0(), *e1)
             .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite)
             .Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_OPACITY(LayerAt(0)->effect_tree_index(), 1.f, kHasRenderSurface);
}

TEST_P(PaintArtifactCompositorTest, BackdropFilterCreatesRenderSurface) {
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(5);
  auto* e1 = CreateBackdropFilterEffect(e0(), filter);
  Update(TestPaintArtifact()
             .Chunk(t0(), c0(), *e1)
             .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite)
             .Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_OPACITY(LayerAt(0)->effect_tree_index(), 1.f, kHasRenderSurface);
}

TEST_P(PaintArtifactCompositorTest,
       WillChangeBackdropFilterCreatesRenderSurface) {
  auto* e1 =
      CreateBackdropFilterEffect(e0(), CompositorFilterOperations(),
                                 CompositingReason::kWillChangeBackdropFilter);
  Update(TestPaintArtifact()
             .Chunk(t0(), c0(), *e1)
             .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite)
             .Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_OPACITY(LayerAt(0)->effect_tree_index(), 1.f, kHasRenderSurface);
}

TEST_P(PaintArtifactCompositorTest,
       BackdropFilterAnimationCreatesRenderSurface) {
  auto* e1 = CreateAnimatingBackdropFilterEffect(e0());
  Update(TestPaintArtifact()
             .Chunk(t0(), c0(), *e1)
             .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite)
             .Build());
  ASSERT_EQ(1u, LayerCount());
  EXPECT_OPACITY(LayerAt(0)->effect_tree_index(), 1.f, kHasRenderSurface);
}

TEST_P(PaintArtifactCompositorTest, Non2dAxisAlignedClip) {
  auto* rotate = CreateTransform(t0(), MakeRotationMatrix(45));
  auto* clip = CreateClip(c0(), *rotate, FloatRoundedRect(50, 50, 50, 50));
  auto* opacity = CreateOpacityEffect(
      e0(), 0.5f, CompositingReason::kActiveOpacityAnimation);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clip, *opacity)
      .RectDrawing(gfx::Rect(50, 50, 50, 50), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(1u, LayerCount());

  // We should create a synthetic effect node for the non-2d-axis-aligned clip.
  int clip_id = LayerAt(0)->clip_tree_index();
  const auto* cc_clip = GetPropertyTrees().clip_tree().Node(clip_id);
  int effect_id = LayerAt(0)->effect_tree_index();
  const auto* cc_effect = GetPropertyTrees().effect_tree().Node(effect_id);
  EXPECT_OPACITY(effect_id, 1.f, kHasRenderSurface);
  EXPECT_OPACITY(cc_effect->parent_id, 0.5f, kNoRenderSurface);
  EXPECT_EQ(cc_effect->clip_id, cc_clip->parent_id);
}

TEST_P(PaintArtifactCompositorTest, Non2dAxisAlignedRoundedRectClip) {
  auto* rotate = CreateTransform(t0(), MakeRotationMatrix(45));
  FloatRoundedRect rounded_clip(gfx::RectF(50, 50, 50, 50), 5);
  auto* clip = CreateClip(c0(), *rotate, rounded_clip);
  auto* opacity = CreateOpacityEffect(
      e0(), 0.5f, CompositingReason::kActiveOpacityAnimation);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clip, *opacity)
      .RectDrawing(gfx::Rect(50, 50, 50, 50), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(2u, LayerCount());

  // We should create a synthetic effect node for the non-2d-axis-aligned clip.
  auto* masked_layer = LayerAt(0);
  EXPECT_TRUE(masked_layer->draws_content());
  int clip_id = masked_layer->clip_tree_index();
  const auto* cc_clip = GetPropertyTrees().clip_tree().Node(clip_id);
  int effect_id = masked_layer->effect_tree_index();
  const auto* cc_effect = GetPropertyTrees().effect_tree().Node(effect_id);
  EXPECT_OPACITY(effect_id, 1.f, kHasRenderSurface);
  EXPECT_OPACITY(cc_effect->parent_id, 0.5f, kNoRenderSurface);
  // cc_clip should be applied in the clip mask layer.
  EXPECT_EQ(cc_effect->clip_id, cc_clip->parent_id);

  // The mask layer is needed because the masked layer draws content.
  auto* mask_layer = LayerAt(1);
  const auto* cc_mask =
      GetPropertyTrees().effect_tree().Node(mask_layer->effect_tree_index());
  EXPECT_EQ(SkBlendMode::kDstIn, cc_mask->blend_mode);
}

TEST_P(PaintArtifactCompositorTest,
       Non2dAxisAlignedClipUnderLaterRenderSurface) {
  auto* rotate1 = CreateTransform(t0(), MakeRotationMatrix(45), gfx::Point3F(),
                                  CompositingReason::k3DTransform);
  auto* rotate2 =
      CreateTransform(*rotate1, MakeRotationMatrix(-45), gfx::Point3F(),
                      CompositingReason::k3DTransform);
  auto* clip = CreateClip(c0(), *rotate2, FloatRoundedRect(50, 50, 50, 50));
  auto* opacity = CreateOpacityEffect(
      e0(), *rotate1, &c0(), 0.5f, CompositingReason::kActiveOpacityAnimation);

  // This assert ensures the test actually tests the situation. If it fails
  // due to floating-point errors, we should choose other transformation values
  // to make it succeed.
  ASSERT_TRUE(GeometryMapper::SourceToDestinationProjection(t0(), *rotate2)
                  .Preserves2dAxisAlignment());

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *opacity)
      .RectDrawing(gfx::Rect(50, 50, 50, 50), Color::kWhite);
  artifact.Chunk(*rotate1, c0(), *opacity)
      .RectDrawing(gfx::Rect(50, 50, 50, 50), Color::kWhite);
  artifact.Chunk(*rotate2, *clip, *opacity)
      .RectDrawing(gfx::Rect(50, 50, 50, 50), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(3u, LayerCount());

  // We should create a synthetic effect node for the non-2d-axis-aligned clip,
  // though the accumulated transform to the known render surface was identity
  // when the cc clip node was created.
  int clip_id = LayerAt(2)->clip_tree_index();
  const auto* cc_clip = GetPropertyTrees().clip_tree().Node(clip_id);
  int effect_id = LayerAt(2)->effect_tree_index();
  const auto* cc_effect = GetPropertyTrees().effect_tree().Node(effect_id);
  EXPECT_OPACITY(effect_id, 1.f, kHasRenderSurface);
  EXPECT_OPACITY(cc_effect->parent_id, 0.5f, kHasRenderSurface);
  EXPECT_EQ(cc_effect->clip_id, cc_clip->parent_id);
}

static TransformPaintPropertyNode::State Transform3dState(
    const gfx::Transform& transform) {
  TransformPaintPropertyNode::State state{{transform}};
  state.direct_compositing_reasons = CompositingReason::k3DTransform;
  return state;
}

TEST_P(PaintArtifactCompositorTest, TransformChange) {
  auto* t1 = Create2DTranslation(t0(), 10, 20);
  auto* t2 = TransformPaintPropertyNode::Create(
      *t1, Transform3dState(MakeRotationMatrix(45)));
  FakeDisplayItemClient& client =
      *MakeGarbageCollected<FakeDisplayItemClient>();
  client.Validate();
  Update(TestPaintArtifact()
             .Chunk(1)
             .Properties(*t2, c0(), e0())
             .RectDrawing(client, gfx::Rect(100, 100, 200, 100), Color::kBlack)
             .Build());
  ASSERT_EQ(1u, LayerCount());
  auto* layer = static_cast<cc::PictureLayer*>(LayerAt(0));
  auto display_item_list = layer->client()->PaintContentsToDisplayList();

  // Change t1 but not t2.
  layer->ClearSubtreePropertyChangedForTesting();
  t1->Update(
      t0(), TransformPaintPropertyNode::State{{MakeTranslationMatrix(20, 30)}});
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
            t1->NodeChanged());
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, t2->NodeChanged());
  Update(TestPaintArtifact()
             .Chunk(1)
             .Properties(*t2, c0(), e0())
             .RectDrawing(client, gfx::Rect(100, 100, 200, 100), Color::kBlack)
             .Build());

  ASSERT_EQ(1u, LayerCount());
  ASSERT_EQ(layer, LayerAt(0));
  EXPECT_EQ(display_item_list.get(),
            layer->client()->PaintContentsToDisplayList().get());
  EXPECT_TRUE(layer->subtree_property_changed());
  // This is set by cc when propagating ancestor change flag to descendants.
  EXPECT_TRUE(GetTransformNode(layer).transform_changed);
  // This is set by PropertyTreeManager.
  EXPECT_TRUE(GetPropertyTrees()
                  .transform_tree()
                  .Node(GetTransformNode(layer).parent_id)
                  ->transform_changed);

  // Change t2 but not t1.
  layer->ClearSubtreePropertyChangedForTesting();
  t2->Update(*t1, Transform3dState(MakeRotationMatrix(135)));
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, t1->NodeChanged());
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
            t2->NodeChanged());
  Update(TestPaintArtifact()
             .Chunk(1)
             .Properties(*t2, c0(), e0())
             .RectDrawing(client, gfx::Rect(100, 100, 200, 100), Color::kBlack)
             .Build());

  ASSERT_EQ(1u, LayerCount());
  ASSERT_EQ(layer, LayerAt(0));
  EXPECT_EQ(display_item_list.get(),
            layer->client()->PaintContentsToDisplayList().get());
  EXPECT_TRUE(layer->subtree_property_changed());
  EXPECT_TRUE(GetTransformNode(layer).transform_changed);
  EXPECT_FALSE(GetPropertyTrees()
                   .transform_tree()
                   .Node(GetTransformNode(layer).parent_id)
                   ->transform_changed);

  // Change t2 to be 2d translation which will be decomposited.
  layer->ClearSubtreePropertyChangedForTesting();
  t2->Update(*t1, Transform3dState(MakeTranslationMatrix(20, 30)));
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, t1->NodeChanged());
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlyValues, t2->NodeChanged());
  Update(TestPaintArtifact()
             .Chunk(1)
             .Properties(*t2, c0(), e0())
             .RectDrawing(client, gfx::Rect(100, 100, 200, 100), Color::kBlack)
             .Build());

  ASSERT_EQ(1u, LayerCount());
  ASSERT_EQ(layer, LayerAt(0));
  EXPECT_EQ(display_item_list.get(),
            layer->client()->PaintContentsToDisplayList().get());
  // The new transform is decomposited, so there is no transform_changed, but
  // we set subtree_property_changed because offset_from_transform_parent
  // (calculated from the decomposited transforms) changed.
  EXPECT_TRUE(layer->subtree_property_changed());
  EXPECT_FALSE(GetTransformNode(layer).transform_changed);

  // Change no transform nodes, but invalidate client.
  layer->ClearSubtreePropertyChangedForTesting();
  client.Invalidate(PaintInvalidationReason::kBackground);
  Update(TestPaintArtifact()
             .Chunk(1)
             .Properties(*t2, c0(), e0())
             .RectDrawing(client, gfx::Rect(100, 100, 200, 100), Color::kWhite)
             .Build());

  ASSERT_EQ(1u, LayerCount());
  ASSERT_EQ(layer, LayerAt(0));
  EXPECT_NE(display_item_list.get(),
            layer->client()->PaintContentsToDisplayList().get());
}

TEST_P(PaintArtifactCompositorTest, EffectChange) {
  auto* e1 = CreateOpacityEffect(e0(), t0(), nullptr, 0.5f);
  auto* e2 = CreateOpacityEffect(*e1, t0(), nullptr, 0.6f,
                                 CompositingReason::kWillChangeOpacity);

  Update(TestPaintArtifact()
             .Chunk(1)
             .Properties(t0(), c0(), *e2)
             .RectDrawing(gfx::Rect(100, 100, 200, 100), Color::kBlack)
             .Build());
  ASSERT_EQ(1u, LayerCount());
  cc::Layer* layer = LayerAt(0);

  // Change e1 but not e2.
  layer->ClearSubtreePropertyChangedForTesting();
  EffectPaintPropertyNode::State e1_state{&t0()};
  e1_state.opacity = 0.8f;
  e1_state.compositor_element_id = e1->GetCompositorElementId();
  e1->Update(e0(), std::move(e1_state));
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
            e1->NodeChanged());
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, e2->NodeChanged());
  Update(TestPaintArtifact()
             .Chunk(1)
             .Properties(t0(), c0(), *e2)
             .RectDrawing(gfx::Rect(100, 100, 200, 100), Color::kBlack)
             .Build());

  ASSERT_EQ(1u, LayerCount());
  ASSERT_EQ(layer, LayerAt(0));
  // TODO(wangxianzhu): Probably avoid setting this flag on Effect change.
  EXPECT_TRUE(layer->subtree_property_changed());
  // This is set by cc when propagating ancestor change flag to descendants.
  EXPECT_TRUE(GetEffectNode(layer).effect_changed);
  // This is set by PropertyTreeManager.
  EXPECT_TRUE(GetPropertyTrees()
                  .effect_tree()
                  .Node(GetEffectNode(layer).parent_id)
                  ->effect_changed);

  // Change e2 but not e1.
  layer->ClearSubtreePropertyChangedForTesting();
  EffectPaintPropertyNode::State e2_state{&t0()};
  e2_state.opacity = 0.9f;
  e2_state.direct_compositing_reasons = CompositingReason::kWillChangeOpacity;
  e2_state.compositor_element_id = e2->GetCompositorElementId();
  e2->Update(*e1, std::move(e2_state));
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, e1->NodeChanged());
  EXPECT_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues,
            e2->NodeChanged());
  Update(TestPaintArtifact()
             .Chunk(1)
             .Properties(t0(), c0(), *e2)
             .RectDrawing(gfx::Rect(100, 100, 200, 100), Color::kBlack)
             .Build());

  ASSERT_EQ(1u, LayerCount());
  ASSERT_EQ(layer, LayerAt(0));
  // TODO(wangxianzhu): Probably avoid setting this flag on Effect change.
  EXPECT_TRUE(layer->subtree_property_changed());
  EXPECT_TRUE(GetEffectNode(layer).effect_changed);
  EXPECT_FALSE(GetPropertyTrees()
                   .effect_tree()
                   .Node(GetEffectNode(layer).parent_id)
                   ->effect_changed);
}

TEST_P(PaintArtifactCompositorTest, DirectlySetScrollOffset) {
  auto scroll_state = ScrollState1();
  auto& scroll = *scroll_state.Transform().ScrollNode();
  auto scroll_element_id = scroll.GetCompositorElementId();

  Update(TestPaintArtifact().ScrollChunks(scroll_state).Build());

  const auto& scroll_tree = GetPropertyTrees().scroll_tree();
  const auto* scroll_layer = ScrollHitTestLayerAt(0);
  const auto* scroll_node =
      scroll_tree.FindNodeFromElementId(scroll_element_id);
  const auto& transform_tree = GetPropertyTrees().transform_tree();
  const auto* transform_node = transform_tree.Node(scroll_node->transform_id);
  EXPECT_EQ(scroll_element_id, scroll_node->element_id);
  EXPECT_EQ(scroll_element_id, scroll_layer->element_id());
  EXPECT_EQ(scroll_node->id, scroll_layer->scroll_tree_index());
  EXPECT_EQ(gfx::PointF(-7, -9),
            scroll_tree.current_scroll_offset(scroll_element_id));
  EXPECT_EQ(gfx::PointF(-7, -9), transform_node->scroll_offset);

  auto& host = GetLayerTreeHost();
  host.CompositeForTest(base::TimeTicks::Now(), true, base::OnceClosure());
  ASSERT_FALSE(const_cast<const cc::LayerTreeHost&>(host)
                   .pending_commit_state()
                   ->layers_that_should_push_properties.contains(scroll_layer));
  ASSERT_FALSE(host.CommitRequested());
  ASSERT_FALSE(transform_tree.needs_update());

  ASSERT_TRUE(GetPaintArtifactCompositor().DirectlySetScrollOffset(
      scroll_element_id, gfx::PointF(-10, -20)));
  EXPECT_TRUE(const_cast<const cc::LayerTreeHost&>(host)
                  .pending_commit_state()
                  ->layers_that_should_push_properties.contains(scroll_layer));
  EXPECT_TRUE(host.CommitRequested());
  EXPECT_EQ(gfx::PointF(-10, -20),
            scroll_tree.current_scroll_offset(scroll_element_id));
  // DirectlySetScrollOffset doesn't update transform node.
  EXPECT_EQ(gfx::PointF(-7, -9), transform_node->scroll_offset);
  EXPECT_FALSE(transform_tree.needs_update());
}

TEST_P(PaintArtifactCompositorTest, NoCommitRequestForUnchangedScroll) {
  auto& host = GetLayerTreeHost();
  auto scroll_state = ScrollState1();
  auto& scroll_node = *scroll_state.Transform().ScrollNode();
  EXPECT_EQ(PaintPropertyChangeType::kNodeAddedOrRemoved,
            scroll_node.NodeChanged());

  auto* client = MakeGarbageCollected<FakeDisplayItemClient>("client");
  Update(TestPaintArtifact().ScrollHitTestChunk(*client, scroll_state).Build());
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, scroll_node.NodeChanged());
  EXPECT_TRUE(host.CommitRequested());

  host.CompositeForTest(base::TimeTicks::Now(), true, base::OnceClosure());
  EXPECT_FALSE(host.CommitRequested());

  // Update with a paint artifact with the same content.
  Update(TestPaintArtifact().ScrollHitTestChunk(*client, scroll_state).Build());
  // This update should not SetNeedsCommit().
  EXPECT_FALSE(host.CommitRequested());
}

TEST_P(PaintArtifactCompositorTest, AddIndirectlyCompositedScrollNodes) {
  auto scroll_state =
      ScrollState1(PropertyTreeState::Root(), CompositingReason::kNone,
                   cc::MainThreadScrollingReason::kNotScrollingOnMain);
  PaintArtifactCompositor::StackScrollTranslationVector
      scroll_translation_nodes = {&scroll_state.Transform()};

  Update(TestPaintArtifact()
             // Opaque contents make the scroll composited.
             .ScrollChunks(scroll_state, /*contents_opaque=*/true)
             .Build(),
         ViewportProperties(), scroll_translation_nodes);

  const auto& scroll_tree = GetPropertyTrees().scroll_tree();
  auto* scroll_node = scroll_tree.FindNodeFromElementId(
      scroll_state.Transform().ScrollNode()->GetCompositorElementId());
  ASSERT_TRUE(scroll_node);
  EXPECT_TRUE(scroll_node->is_composited);
  EXPECT_EQ(cc::MainThreadScrollingReason::kNotScrollingOnMain,
            scroll_node->main_thread_scrolling_reasons);
  EXPECT_TRUE(scroll_tree.CanRealizeScrollsOnActiveTree(*scroll_node));
  EXPECT_FALSE(scroll_tree.CanRealizeScrollsOnPendingTree(*scroll_node));
  EXPECT_FALSE(scroll_tree.ShouldRealizeScrollsOnMain(*scroll_node));
}

TEST_P(PaintArtifactCompositorTest, AddNonCompositedScrollNodes) {
  auto scroll_state =
      ScrollState1(PropertyTreeState::Root(), CompositingReason::kNone,
                   cc::MainThreadScrollingReason::kNotScrollingOnMain);
  PaintArtifactCompositor::StackScrollTranslationVector
      scroll_translation_nodes = {&scroll_state.Transform()};

  Update(TestPaintArtifact().ScrollChunks(scroll_state).Build(),
         ViewportProperties(), scroll_translation_nodes);

  const auto& scroll_tree = GetPropertyTrees().scroll_tree();
  auto* scroll_node = scroll_tree.FindNodeFromElementId(
      scroll_state.Transform().ScrollNode()->GetCompositorElementId());
  ASSERT_TRUE(scroll_node);
  EXPECT_FALSE(scroll_node->is_composited);
  EXPECT_FALSE(scroll_tree.CanRealizeScrollsOnActiveTree(*scroll_node));
  if (RuntimeEnabledFeatures::RasterInducingScrollEnabled()) {
    EXPECT_EQ(cc::MainThreadScrollingReason::kNotScrollingOnMain,
              scroll_node->main_thread_scrolling_reasons);
    EXPECT_TRUE(scroll_tree.CanRealizeScrollsOnPendingTree(*scroll_node));
    EXPECT_FALSE(scroll_tree.ShouldRealizeScrollsOnMain(*scroll_node));
  } else {
    EXPECT_EQ(cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText,
              scroll_node->main_thread_scrolling_reasons);
    EXPECT_FALSE(scroll_tree.CanRealizeScrollsOnPendingTree(*scroll_node));
    EXPECT_TRUE(scroll_tree.ShouldRealizeScrollsOnMain(*scroll_node));
  }
}

TEST_P(PaintArtifactCompositorTest, AddNonCompositedMainThreadScrollNodes) {
  auto scroll_state = ScrollState1(
      PropertyTreeState::Root(), CompositingReason::kNone,
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
  PaintArtifactCompositor::StackScrollTranslationVector
      scroll_translation_nodes = {&scroll_state.Transform()};

  Update(TestPaintArtifact().ScrollChunks(scroll_state).Build(),
         ViewportProperties(), scroll_translation_nodes);

  const auto& scroll_tree = GetPropertyTrees().scroll_tree();
  auto* scroll_node = scroll_tree.FindNodeFromElementId(
      scroll_state.Transform().ScrollNode()->GetCompositorElementId());
  ASSERT_TRUE(scroll_node);
  EXPECT_FALSE(scroll_node->is_composited);
  if (RuntimeEnabledFeatures::RasterInducingScrollEnabled()) {
    EXPECT_EQ(
        cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
        scroll_node->main_thread_scrolling_reasons);
  } else {
    EXPECT_EQ(
        cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText |
            cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
        scroll_node->main_thread_scrolling_reasons);
  }
  EXPECT_FALSE(scroll_tree.CanRealizeScrollsOnActiveTree(*scroll_node));
  EXPECT_FALSE(scroll_tree.CanRealizeScrollsOnPendingTree(*scroll_node));
  EXPECT_TRUE(scroll_tree.ShouldRealizeScrollsOnMain(*scroll_node));
}

TEST_P(PaintArtifactCompositorTest,
       AddIndirectlyCompositedMainThreadScrollNodes) {
  auto scroll_state = ScrollState1(
      PropertyTreeState::Root(), CompositingReason::kNone,
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
  PaintArtifactCompositor::StackScrollTranslationVector
      scroll_translation_nodes = {&scroll_state.Transform()};

  Update(TestPaintArtifact()
             // Opaque contents make the scroll composited.
             .ScrollChunks(scroll_state, /*contents_opaque=*/true)
             .Build(),
         ViewportProperties(), scroll_translation_nodes);

  const auto& scroll_tree = GetPropertyTrees().scroll_tree();
  auto* scroll_node = scroll_tree.FindNodeFromElementId(
      scroll_state.Transform().ScrollNode()->GetCompositorElementId());
  ASSERT_TRUE(scroll_node);
  // THe scroll node should realize on main thread despite is_composited.
  EXPECT_TRUE(scroll_node->is_composited);
  EXPECT_EQ(cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
            scroll_node->main_thread_scrolling_reasons);
  EXPECT_FALSE(scroll_tree.CanRealizeScrollsOnActiveTree(*scroll_node));
  EXPECT_FALSE(scroll_tree.CanRealizeScrollsOnPendingTree(*scroll_node));
  EXPECT_TRUE(scroll_tree.ShouldRealizeScrollsOnMain(*scroll_node));
}

TEST_P(PaintArtifactCompositorTest, AddUnpaintedNonCompositedScrollNodes) {
  const uint32_t main_thread_scrolling_reason =
      cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText;
  auto scroll_state =
      ScrollState1(PropertyTreeState::Root(), CompositingReason::kNone,
                   main_thread_scrolling_reason);
  PaintArtifactCompositor::StackScrollTranslationVector
      scroll_translation_nodes = {&scroll_state.Transform()};

  Update(TestPaintArtifact().Build(), ViewportProperties(),
         scroll_translation_nodes);

  const auto& scroll_tree = GetPropertyTrees().scroll_tree();
  auto* scroll_node = scroll_tree.FindNodeFromElementId(
      scroll_state.Transform().ScrollNode()->GetCompositorElementId());
  ASSERT_TRUE(scroll_node);
  EXPECT_FALSE(scroll_node->is_composited);
  EXPECT_EQ(scroll_node->transform_id, cc::kInvalidPropertyNodeId);
  EXPECT_EQ(gfx::PointF(-7, -9),
            scroll_tree.current_scroll_offset(scroll_node->element_id));
  EXPECT_FALSE(scroll_tree.CanRealizeScrollsOnActiveTree(*scroll_node));
  EXPECT_FALSE(scroll_tree.CanRealizeScrollsOnPendingTree(*scroll_node));
  EXPECT_FALSE(scroll_tree.ShouldRealizeScrollsOnMain(*scroll_node));
}

TEST_P(PaintArtifactCompositorTest, RepaintIndirectScrollHitTest) {
  auto scroll_state = ScrollState1();
  auto& artifact = TestPaintArtifact().ScrollHitTestChunk(scroll_state).Build();
  auto& host = GetLayerTreeHost();

  Update(artifact);
  EXPECT_TRUE(host.CommitRequested());

  host.CompositeForTest(base::TimeTicks::Now(), true, base::OnceClosure());
  EXPECT_FALSE(host.CommitRequested());

  GetPaintArtifactCompositor().UpdateRepaintedLayers(artifact);
  EXPECT_FALSE(host.CommitRequested());
}

TEST_P(PaintArtifactCompositorTest, ClearChangedStateWithIndirectTransform) {
  // t1 and t2 are siblings.
  auto* t1 = Create2DTranslation(t0(), 1, 1);
  auto* t2 = Create2DTranslation(t0(), 2, 2);
  // c1 and c2 are parent and child, referencing t1 and t2, respectively.
  auto* c1 = CreateClip(c0(), *t1, FloatRoundedRect(1, 1, 1, 1));
  auto* c2 = CreateClip(*c1, *t2, FloatRoundedRect(2, 2, 2, 2));
  EXPECT_EQ(PaintPropertyChangeType::kNodeAddedOrRemoved, t1->NodeChanged());
  EXPECT_EQ(PaintPropertyChangeType::kNodeAddedOrRemoved, t2->NodeChanged());
  EXPECT_EQ(PaintPropertyChangeType::kNodeAddedOrRemoved, c1->NodeChanged());
  EXPECT_EQ(PaintPropertyChangeType::kNodeAddedOrRemoved, c2->NodeChanged());

  auto& artifact = TestPaintArtifact()
                       .Chunk(1)
                       .Properties(*t2, *c2, e0())
                       .RectDrawing(gfx::Rect(2, 2, 2, 2), Color::kBlack)
                       .Build();
  Update(artifact);
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, t1->NodeChanged());
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, t2->NodeChanged());
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, c1->NodeChanged());
  EXPECT_EQ(PaintPropertyChangeType::kUnchanged, c2->NodeChanged());

  GetPaintArtifactCompositor().UpdateRepaintedLayers(artifact);
  // This test passes if no DCHECK occurs.
}

TEST_P(PaintArtifactCompositorTest,
       CompositedPixelMovingFilterWithClipExpander) {
  CompositorFilterOperations filter_op;
  filter_op.AppendBlurFilter(5);
  auto* filter =
      CreateFilterEffect(e0(), filter_op, CompositingReason::kWillChangeFilter);
  auto* clip_expander = CreatePixelMovingFilterClipExpander(c0(), *filter);

  Update(TestPaintArtifact()
             .Chunk(t0(), *clip_expander, *filter)
             .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite)
             .Build());
  ASSERT_EQ(1u, LayerCount());
  const auto* cc_clip_expander =
      GetPropertyTrees().clip_tree().Node(LayerAt(0)->clip_tree_index());
  EXPECT_FALSE(cc_clip_expander->AppliesLocalClip());
  EXPECT_EQ(cc_clip_expander->pixel_moving_filter_id,
            LayerAt(0)->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest,
       NonCompositedPixelMovingFilterWithCompositedClipExpander) {
  CompositorFilterOperations filter_op;
  filter_op.AppendBlurFilter(5);
  auto* filter = CreateFilterEffect(e0(), filter_op);
  auto* clip_expander = CreatePixelMovingFilterClipExpander(c0(), *filter);

  EffectPaintPropertyNode::State mask_state;
  mask_state.local_transform_space = &t0();
  mask_state.output_clip = clip_expander;
  mask_state.blend_mode = SkBlendMode::kDstIn;
  mask_state.direct_compositing_reasons =
      CompositingReason::kBackdropFilterMask;
  auto* mask = EffectPaintPropertyNode::Create(e0(), std::move(mask_state));

  Update(TestPaintArtifact()
             .Chunk(t0(), *clip_expander, *filter)
             .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite)
             .Chunk(t0(), *clip_expander, *mask)
             .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kBlack)
             .Build());
  ASSERT_EQ(2u, LayerCount());
  const auto* cc_clip_expander =
      GetPropertyTrees().clip_tree().Node(LayerAt(0)->clip_tree_index());
  EXPECT_TRUE(cc_clip_expander->AppliesLocalClip());
}

TEST_P(PaintArtifactCompositorTest,
       CreatePictureLayerForSolidColorBackdropFilterMask) {
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(5);
  auto* backdrop_filter = CreateBackdropFilterEffect(e0(), filter);

  EffectPaintPropertyNode::State mask_state;
  mask_state.local_transform_space = &t0();
  mask_state.output_clip = &c0();
  mask_state.blend_mode = SkBlendMode::kDstIn;
  mask_state.direct_compositing_reasons =
      CompositingReason::kBackdropFilterMask;
  auto* mask =
      EffectPaintPropertyNode::Create(*backdrop_filter, std::move(mask_state));

  Update(TestPaintArtifact()
             .Chunk(t0(), c0(), *backdrop_filter)
             .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kWhite)
             .Chunk(t0(), c0(), *mask)
             .RectDrawing(gfx::Rect(150, 150, 100, 100), Color::kBlack)
             .IsSolidColor()
             .Build());
  ASSERT_EQ(2u, LayerCount());
  EXPECT_FALSE(LayerAt(1)->IsSolidColorLayerForTesting());
}

}  // namespace
}  // namespace blink
