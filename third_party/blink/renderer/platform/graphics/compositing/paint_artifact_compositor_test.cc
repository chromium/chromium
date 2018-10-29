// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"
#include "third_party/blink/renderer/platform/testing/layer_tree_host_embedder.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/picture_matchers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/test_paint_artifact.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using testing::Pointee;

PaintChunk::Id DefaultId() {
  DEFINE_STATIC_LOCAL(FakeDisplayItemClient, fake_client, ());
  return PaintChunk::Id(fake_client, DisplayItem::kDrawingFirst);
}

PaintChunk DefaultChunk() {
  return PaintChunk(0, 1, DefaultId(), PropertyTreeState::Root());
}

gfx::Transform Translation(SkMScalar x, SkMScalar y) {
  gfx::Transform transform;
  transform.Translate(x, y);
  return transform;
}

class FakeScrollClient {
 public:
  FakeScrollClient() : did_scroll_count(0) {}

  void DidScroll(const gfx::ScrollOffset& offset, const CompositorElementId&) {
    did_scroll_count++;
    last_scroll_offset = offset;
  };

  gfx::ScrollOffset last_scroll_offset;
  unsigned did_scroll_count;
};

class PaintArtifactCompositorTest : public testing::Test,
                                    public PaintTestConfigurations {
 protected:
  PaintArtifactCompositorTest()
      : task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        task_runner_handle_(task_runner_) {}

  void SetUp() override {
    // Delay constructing the compositor until after the feature is set.
    paint_artifact_compositor_ =
        PaintArtifactCompositor::Create(WTF::BindRepeating(
            &FakeScrollClient::DidScroll, WTF::Unretained(&scroll_client_)));
    paint_artifact_compositor_->EnableExtraDataForTesting();

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
  }

  cc::PropertyTrees& GetPropertyTrees() {
    return *layer_tree_->layer_tree_host()->property_trees();
  }

  const cc::TransformNode& GetTransformNode(const cc::Layer* layer) {
    auto* property_trees = layer_tree_->layer_tree_host()->property_trees();
    return *property_trees->transform_tree.Node(layer->transform_tree_index());
  }

  const cc::LayerTreeHost& GetLayerTreeHost() {
    return *layer_tree_->layer_tree_host();
  }

  int ElementIdToEffectNodeIndex(CompositorElementId element_id) {
    auto* property_trees = layer_tree_->layer_tree_host()->property_trees();
    return property_trees->element_id_to_effect_node_index[element_id];
  }

  int ElementIdToTransformNodeIndex(CompositorElementId element_id) {
    auto* property_trees = layer_tree_->layer_tree_host()->property_trees();
    return property_trees->element_id_to_transform_node_index[element_id];
  }

  int ElementIdToScrollNodeIndex(CompositorElementId element_id) {
    auto* property_trees = layer_tree_->layer_tree_host()->property_trees();
    return property_trees->element_id_to_scroll_node_index[element_id];
  }

  void Update(scoped_refptr<const PaintArtifact> artifact) {
    CompositorElementIdSet element_ids;
    Update(artifact, element_ids);
  }

  void Update(scoped_refptr<const PaintArtifact> artifact,
              CompositorElementIdSet& element_ids) {
    // Pass nullptr for the visual viewport paint property nodes since we're
    // really just checking the internals of PaintArtifactCompositor.
    Update(artifact, element_ids, nullptr);
  }

  void Update(scoped_refptr<const PaintArtifact> artifact,
              CompositorElementIdSet& element_ids,
              TransformPaintPropertyNode* viewport_scale_transform_node) {
    paint_artifact_compositor_->Update(artifact, element_ids,
                                       viewport_scale_transform_node);
    layer_tree_->layer_tree_host()->LayoutAndUpdateLayers();
  }

  void WillBeRemovedFromFrame() {
    paint_artifact_compositor_->WillBeRemovedFromFrame();
  }

  cc::Layer* RootLayer() { return paint_artifact_compositor_->RootLayer(); }

  // SlimmingPaintV2 creates scroll hit test display items (which create scroll
  // hit test layers in PaintArtifactCompositor) whereas in
  // BlinkGenPropertyTrees, scrollable foreign layers are created in
  // ScrollingCoordinator and passed to PaintArtifactCompositor. This function
  // is used to create a chunk representing the scrollable layer in either of
  // these modes.
  void CreateScrollableChunk(TestPaintArtifact& artifact,
                             const TransformPaintPropertyNode& scroll_offset,
                             const ClipPaintPropertyNode& clip,
                             const EffectPaintPropertyNode& effect) {
    if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
      // Create a foreign layer for scrolling, roughly matching the layer
      // created by ScrollingCoordinator.
      const auto* scroll_node = scroll_offset.ScrollNode();
      scoped_refptr<cc::Layer> layer = cc::Layer::Create();
      auto rect = scroll_node->ContainerRect();
      layer->SetScrollable(gfx::Size(rect.Width(), rect.Height()));
      layer->SetBounds(gfx::Size(rect.Width(), rect.Height()));
      layer->SetElementId(scroll_node->GetCompositorElementId());
      layer->set_did_scroll_callback(
          paint_artifact_compositor_->scroll_callback_);
      artifact.Chunk(scroll_offset, clip, effect)
          .ForeignLayer(FloatPoint(rect.Location()), rect.Size(), layer);
      return;
    }
    // Scroll hit test layers are marked as scrollable for hit testing but are
    // in the unscrolled transform space (scroll offset's parent).
    artifact.Chunk(*scroll_offset.Parent(), clip, effect)
        .ScrollHitTest(scroll_offset);
  }

  // Returns the |num|th scrollable layer. In SlimmingPaintV2, this will be a
  // scroll hit test layer, whereas in BlinkGenPropertyTrees this will be a
  // content layer.
  cc::Layer* ScrollableLayerAt(size_t num) {
    if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
      for (size_t content_layer_index = 0;
           content_layer_index < ContentLayerCount(); content_layer_index++) {
        auto* content_layer = ContentLayerAt(content_layer_index);
        if (content_layer->scrollable()) {
          if (num == 0)
            return content_layer;
          num--;
        }
      }
      return nullptr;
    }
    return paint_artifact_compositor_->GetExtraDataForTesting()
        ->scroll_hit_test_layers[num]
        .get();
  }

  // Returns the |num|th non-scrollable layer. In SlimmingPaintV2, content
  // layers are not scrollable so this is the |num|th content layer. In
  // BlinkGenPropertyTrees, content layers are scrollable and non-scrollable, so
  // this will return the |num|th content layer that is not scrollable.
  cc::Layer* NonScrollableLayerAt(size_t num) {
    for (size_t content_layer_index = 0;
         content_layer_index < ContentLayerCount(); content_layer_index++) {
      auto* content_layer = ContentLayerAt(content_layer_index);
      if (!content_layer->scrollable()) {
        if (num == 0)
          return content_layer;
        num--;
      }
    }
    return nullptr;
  }

  size_t ContentLayerCount() {
    return paint_artifact_compositor_->GetExtraDataForTesting()
        ->content_layers.size();
  }

  cc::Layer* ContentLayerAt(unsigned index) {
    return paint_artifact_compositor_->GetExtraDataForTesting()
        ->content_layers[index]
        .get();
  }

  CompositorElementId ScrollElementId(unsigned id) {
    return CompositorElementIdFromUniqueObjectId(
        id, CompositorElementIdNamespace::kScroll);
  }

  size_t SynthesizedClipLayerCount() {
    return paint_artifact_compositor_->GetExtraDataForTesting()
        ->synthesized_clip_layers.size();
  }

  cc::Layer* SynthesizedClipLayerAt(unsigned index) {
    return paint_artifact_compositor_->GetExtraDataForTesting()
        ->synthesized_clip_layers[index]
        .get();
  }

  // Return the index of |layer| in the root layer list, or -1 if not found.
  int LayerIndex(const cc::Layer* layer) {
    for (size_t i = 0; i < RootLayer()->children().size(); ++i) {
      if (RootLayer()->children()[i] == layer)
        return i;
    }
    return -1;
  }

  void AddSimpleRectChunk(TestPaintArtifact& artifact) {
    artifact.Chunk().RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);
  }

  void UpdateWithArtifactWithOpacity(float opacity,
                                     bool include_preceding_chunk,
                                     bool include_subsequent_chunk) {
    TestPaintArtifact artifact;
    if (include_preceding_chunk)
      AddSimpleRectChunk(artifact);
    auto effect = CreateOpacityEffect(e0(), opacity);
    artifact.Chunk(t0(), c0(), *effect)
        .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
    if (include_subsequent_chunk)
      AddSimpleRectChunk(artifact);
    Update(artifact.Build());
  }

  using PendingLayer = PaintArtifactCompositor::PendingLayer;

  bool MightOverlap(const PendingLayer& a, const PendingLayer& b) {
    return PaintArtifactCompositor::MightOverlap(a, b);
  }

  FakeScrollClient& ScrollClient() { return scroll_client_; }

 private:
  FakeScrollClient scroll_client_;
  std::unique_ptr<PaintArtifactCompositor> paint_artifact_compositor_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  cc::FakeLayerTreeHostClient layer_tree_host_client_;
  std::unique_ptr<LayerTreeHostEmbedder> layer_tree_;
};

INSTANTIATE_LAYER_LIST_TEST_CASE_P(PaintArtifactCompositorTest);

const auto kNotScrollingOnMain = MainThreadScrollingReason::kNotScrollingOnMain;

TEST_P(PaintArtifactCompositorTest, EmptyPaintArtifact) {
  Update(PaintArtifact::Empty());
  EXPECT_TRUE(RootLayer()->children().empty());
}

TEST_P(PaintArtifactCompositorTest, OneChunkWithAnOffset) {
  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(FloatRect(50, -50, 100, 100), Color::kWhite);
  Update(artifact.Build());

  ASSERT_EQ(1u, ContentLayerCount());
  const cc::Layer* child = ContentLayerAt(0);
  EXPECT_THAT(
      child->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kWhite)));
  EXPECT_EQ(Translation(50, -50), child->ScreenSpaceTransform());
  EXPECT_EQ(gfx::Size(100, 100), child->bounds());
}

TEST_P(PaintArtifactCompositorTest, OneTransform) {
  // A 90 degree clockwise rotation about (100, 100).
  auto transform = CreateTransform(t0(), TransformationMatrix().Rotate(90),
                                   FloatPoint3D(100, 100, 0),
                                   CompositingReason::k3DTransform);

  TestPaintArtifact artifact;
  artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kGray);
  artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(2u, ContentLayerCount());
  {
    const cc::Layer* layer = ContentLayerAt(0);

    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(FloatRect(100, 100, 200, 100), Color::kBlack));

    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
    gfx::RectF mapped_rect(0, 0, 100, 100);
    layer->ScreenSpaceTransform().TransformRect(&mapped_rect);
    EXPECT_EQ(gfx::RectF(100, 0, 100, 100), mapped_rect);
  }
  {
    const cc::Layer* layer = ContentLayerAt(1);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kGray)));
    EXPECT_EQ(gfx::Transform(), layer->ScreenSpaceTransform());
  }
}

TEST_P(PaintArtifactCompositorTest, OneTransformWithAlias) {
  // A 90 degree clockwise rotation about (100, 100).
  auto real_transform = CreateTransform(t0(), TransformationMatrix().Rotate(90),
                                        FloatPoint3D(100, 100, 0),
                                        CompositingReason::k3DTransform);
  auto transform = TransformPaintPropertyNode::CreateAlias(*real_transform);

  TestPaintArtifact artifact;
  artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kGray);
  artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(2u, ContentLayerCount());
  {
    const cc::Layer* layer = ContentLayerAt(0);

    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(FloatRect(100, 100, 200, 100), Color::kBlack));

    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
    gfx::RectF mapped_rect(0, 0, 100, 100);
    layer->ScreenSpaceTransform().TransformRect(&mapped_rect);
    EXPECT_EQ(gfx::RectF(100, 0, 100, 100), mapped_rect);
  }
  {
    const cc::Layer* layer = ContentLayerAt(1);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kGray)));
    EXPECT_EQ(gfx::Transform(), layer->ScreenSpaceTransform());
  }
}

TEST_P(PaintArtifactCompositorTest, TransformCombining) {
  // A translation by (5, 5) within a 2x scale about (10, 10).
  auto transform1 =
      CreateTransform(t0(), TransformationMatrix().Scale(2),
                      FloatPoint3D(10, 10, 0), CompositingReason::k3DTransform);
  auto transform2 =
      CreateTransform(*transform1, TransformationMatrix().Translate(5, 5),
                      FloatPoint3D(), CompositingReason::k3DTransform);

  TestPaintArtifact artifact;
  artifact.Chunk(*transform1, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kWhite);
  artifact.Chunk(*transform2, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(2u, ContentLayerCount());
  {
    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kWhite)));
    gfx::RectF mapped_rect(0, 0, 300, 200);
    layer->ScreenSpaceTransform().TransformRect(&mapped_rect);
    EXPECT_EQ(gfx::RectF(-10, -10, 600, 400), mapped_rect);
  }
  {
    const cc::Layer* layer = ContentLayerAt(1);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kBlack)));
    gfx::RectF mapped_rect(0, 0, 300, 200);
    layer->ScreenSpaceTransform().TransformRect(&mapped_rect);
    EXPECT_EQ(gfx::RectF(0, 0, 600, 400), mapped_rect);
  }
  EXPECT_NE(ContentLayerAt(0)->transform_tree_index(),
            ContentLayerAt(1)->transform_tree_index());
}

TEST_P(PaintArtifactCompositorTest, BackfaceVisibility) {
  TransformPaintPropertyNode::State backface_hidden_state;
  backface_hidden_state.backface_visibility =
      TransformPaintPropertyNode::BackfaceVisibility::kHidden;
  auto backface_hidden_transform = TransformPaintPropertyNode::Create(
      t0(), std::move(backface_hidden_state));

  auto backface_inherited_transform = TransformPaintPropertyNode::Create(
      *backface_hidden_transform, TransformPaintPropertyNode::State{});

  TransformPaintPropertyNode::State backface_visible_state;
  backface_visible_state.backface_visibility =
      TransformPaintPropertyNode::BackfaceVisibility::kVisible;
  auto backface_visible_transform = TransformPaintPropertyNode::Create(
      *backface_hidden_transform, std::move(backface_visible_state));

  TestPaintArtifact artifact;
  artifact.Chunk(*backface_hidden_transform, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kWhite);
  artifact.Chunk(*backface_inherited_transform, c0(), e0())
      .RectDrawing(FloatRect(100, 100, 100, 100), Color::kBlack);
  artifact.Chunk(*backface_visible_transform, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kDarkGray);
  Update(artifact.Build());

  ASSERT_EQ(2u, ContentLayerCount());
  EXPECT_THAT(
      ContentLayerAt(0)->GetPicture(),
      Pointee(DrawsRectangles(Vector<RectWithColor>{
          RectWithColor(FloatRect(0, 0, 300, 200), Color::kWhite),
          RectWithColor(FloatRect(100, 100, 100, 100), Color::kBlack)})));
  EXPECT_THAT(
      ContentLayerAt(1)->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kDarkGray)));
}

TEST_P(PaintArtifactCompositorTest, FlattensInheritedTransform) {
  for (bool transform_is_flattened : {true, false}) {
    SCOPED_TRACE(transform_is_flattened);

    // The flattens_inherited_transform bit corresponds to whether the _parent_
    // transform node flattens the transform. This is because Blink's notion of
    // flattening determines whether content within the node's local transform
    // is flattened, while cc's notion applies in the parent's coordinate space.
    auto transform1 = CreateTransform(t0(), TransformationMatrix());
    auto transform2 =
        CreateTransform(*transform1, TransformationMatrix().Rotate3d(0, 45, 0));
    TransformPaintPropertyNode::State transform3_state;
    transform3_state.matrix = TransformationMatrix().Rotate3d(0, 45, 0);
    transform3_state.flattens_inherited_transform = transform_is_flattened;
    auto transform3 = TransformPaintPropertyNode::Create(
        *transform2, std::move(transform3_state));

    TestPaintArtifact artifact;
    artifact.Chunk(*transform3, c0(), e0())
        .RectDrawing(FloatRect(0, 0, 300, 200), Color::kWhite);
    Update(artifact.Build());

    ASSERT_EQ(1u, ContentLayerCount());
    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kWhite)));

    // The leaf transform node should flatten its inherited transform node
    // if and only if the intermediate rotation transform in the Blink tree
    // flattens.
    const cc::TransformNode* transform_node3 =
        GetPropertyTrees().transform_tree.Node(layer->transform_tree_index());
    EXPECT_EQ(transform_is_flattened,
              transform_node3->flattens_inherited_transform);

    // Given this, we should expect the correct screen space transform for
    // each case. If the transform was flattened, we should see it getting
    // an effective horizontal scale of 1/sqrt(2) each time, thus it gets
    // half as wide. If the transform was not flattened, we should see an
    // empty rectangle (as the total 90 degree rotation makes it
    // perpendicular to the viewport).
    gfx::RectF rect(0, 0, 100, 100);
    layer->ScreenSpaceTransform().TransformRect(&rect);
    if (transform_is_flattened)
      EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 50, 100), rect);
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
    auto real_transform1 = CreateTransform(t0(), TransformationMatrix());
    auto transform1 = TransformPaintPropertyNode::CreateAlias(*real_transform1);
    auto real_transform2 =
        CreateTransform(*transform1, TransformationMatrix().Rotate3d(0, 45, 0));
    auto transform2 = TransformPaintPropertyNode::CreateAlias(*real_transform2);
    TransformPaintPropertyNode::State transform3_state;
    transform3_state.matrix = TransformationMatrix().Rotate3d(0, 45, 0);
    transform3_state.flattens_inherited_transform = transform_is_flattened;
    auto real_transform3 = TransformPaintPropertyNode::Create(
        *transform2, std::move(transform3_state));
    auto transform3 = TransformPaintPropertyNode::CreateAlias(*real_transform3);

    TestPaintArtifact artifact;
    artifact.Chunk(*transform3, c0(), e0())
        .RectDrawing(FloatRect(0, 0, 300, 200), Color::kWhite);
    Update(artifact.Build());

    ASSERT_EQ(1u, ContentLayerCount());
    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(
        layer->GetPicture(),
        Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kWhite)));

    // The leaf transform node should flatten its inherited transform node
    // if and only if the intermediate rotation transform in the Blink tree
    // flattens.
    const cc::TransformNode* transform_node3 =
        GetPropertyTrees().transform_tree.Node(layer->transform_tree_index());
    EXPECT_EQ(transform_is_flattened,
              transform_node3->flattens_inherited_transform);

    // Given this, we should expect the correct screen space transform for
    // each case. If the transform was flattened, we should see it getting
    // an effective horizontal scale of 1/sqrt(2) each time, thus it gets
    // half as wide. If the transform was not flattened, we should see an
    // empty rectangle (as the total 90 degree rotation makes it
    // perpendicular to the viewport).
    gfx::RectF rect(0, 0, 100, 100);
    layer->ScreenSpaceTransform().TransformRect(&rect);
    if (transform_is_flattened)
      EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 50, 100), rect);
    else
      EXPECT_TRUE(rect.IsEmpty());
  }
}
TEST_P(PaintArtifactCompositorTest, SortingContextID) {
  // Has no 3D rendering context.
  auto transform1 = CreateTransform(t0(), TransformationMatrix());
  // Establishes a 3D rendering context.
  TransformPaintPropertyNode::State transform2_3_state;
  transform2_3_state.rendering_context_id = 1;
  transform2_3_state.direct_compositing_reasons =
      CompositingReason::k3DTransform;
  auto transform2 = TransformPaintPropertyNode::Create(
      *transform1, std::move(transform2_3_state));
  // Extends the 3D rendering context of transform2.
  auto transform3 = TransformPaintPropertyNode::Create(
      *transform2, std::move(transform2_3_state));
  // Establishes a 3D rendering context distinct from transform2.
  TransformPaintPropertyNode::State transform4_state;
  transform4_state.rendering_context_id = 2;
  transform4_state.direct_compositing_reasons = CompositingReason::k3DTransform;
  auto transform4 = TransformPaintPropertyNode::Create(
      *transform2, std::move(transform4_state));

  TestPaintArtifact artifact;
  artifact.Chunk(*transform1, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kWhite);
  artifact.Chunk(*transform2, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kLightGray);
  artifact.Chunk(*transform3, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kDarkGray);
  artifact.Chunk(*transform4, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 300, 200), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(4u, ContentLayerCount());

  // The white layer is not 3D sorted.
  const cc::Layer* white_layer = ContentLayerAt(0);
  EXPECT_THAT(
      white_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kWhite)));
  int white_sorting_context_id =
      GetTransformNode(white_layer).sorting_context_id;
  EXPECT_EQ(white_layer->sorting_context_id(), white_sorting_context_id);
  EXPECT_EQ(0, white_sorting_context_id);

  // The light gray layer is 3D sorted.
  const cc::Layer* light_gray_layer = ContentLayerAt(1);
  EXPECT_THAT(
      light_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kLightGray)));
  int light_gray_sorting_context_id =
      GetTransformNode(light_gray_layer).sorting_context_id;
  EXPECT_NE(0, light_gray_sorting_context_id);

  // The dark gray layer is 3D sorted with the light gray layer, but has a
  // separate transform node.
  const cc::Layer* dark_gray_layer = ContentLayerAt(2);
  EXPECT_THAT(
      dark_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kDarkGray)));
  int dark_gray_sorting_context_id =
      GetTransformNode(dark_gray_layer).sorting_context_id;
  EXPECT_EQ(light_gray_sorting_context_id, dark_gray_sorting_context_id);
  EXPECT_NE(light_gray_layer->transform_tree_index(),
            dark_gray_layer->transform_tree_index());

  // The black layer is 3D sorted, but in a separate context from the previous
  // layers.
  const cc::Layer* black_layer = ContentLayerAt(3);
  EXPECT_THAT(
      black_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 300, 200), Color::kBlack)));
  int black_sorting_context_id =
      GetTransformNode(black_layer).sorting_context_id;
  EXPECT_NE(0, black_sorting_context_id);
  EXPECT_NE(light_gray_sorting_context_id, black_sorting_context_id);
}

TEST_P(PaintArtifactCompositorTest, OneClip) {
  auto clip = CreateClip(c0(), &t0(), FloatRoundedRect(100, 100, 300, 200));

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clip, e0())
      .RectDrawing(FloatRect(220, 80, 300, 200), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(1u, ContentLayerCount());
  const cc::Layer* layer = ContentLayerAt(0);
  // The layer is clipped.
  EXPECT_EQ(gfx::Size(180, 180), layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(220, 100), layer->offset_to_transform_parent());
  EXPECT_THAT(
      layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 300, 180), Color::kBlack)));
  EXPECT_EQ(Translation(220, 100), layer->ScreenSpaceTransform());

  const cc::ClipNode* clip_node =
      GetPropertyTrees().clip_tree.Node(layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, clip_node->clip_type);
  EXPECT_EQ(gfx::RectF(100, 100, 300, 200), clip_node->clip);
}

TEST_P(PaintArtifactCompositorTest, OneClipWithAlias) {
  auto real_clip =
      CreateClip(c0(), &t0(), FloatRoundedRect(100, 100, 300, 200));
  auto clip = ClipPaintPropertyNode::CreateAlias(*real_clip);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clip, e0())
      .RectDrawing(FloatRect(220, 80, 300, 200), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(1u, ContentLayerCount());
  const cc::Layer* layer = ContentLayerAt(0);
  // The layer is clipped.
  EXPECT_EQ(gfx::Size(180, 180), layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(220, 100), layer->offset_to_transform_parent());
  EXPECT_THAT(
      layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 300, 180), Color::kBlack)));
  EXPECT_EQ(Translation(220, 100), layer->ScreenSpaceTransform());

  const cc::ClipNode* clip_node =
      GetPropertyTrees().clip_tree.Node(layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, clip_node->clip_type);
  EXPECT_EQ(gfx::RectF(100, 100, 300, 200), clip_node->clip);
}

TEST_P(PaintArtifactCompositorTest, NestedClips) {
  auto clip1 = CreateClip(c0(), &t0(), FloatRoundedRect(100, 100, 700, 700),
                          CompositingReason::kOverflowScrollingTouch);
  auto clip2 = CreateClip(*clip1, &t0(), FloatRoundedRect(200, 200, 700, 700),
                          CompositingReason::kOverflowScrollingTouch);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clip1, e0())
      .RectDrawing(FloatRect(300, 350, 100, 100), Color::kWhite);
  artifact.Chunk(t0(), *clip2, e0())
      .RectDrawing(FloatRect(300, 350, 100, 100), Color::kLightGray);
  artifact.Chunk(t0(), *clip1, e0())
      .RectDrawing(FloatRect(300, 350, 100, 100), Color::kDarkGray);
  artifact.Chunk(t0(), *clip2, e0())
      .RectDrawing(FloatRect(300, 350, 100, 100), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(4u, ContentLayerCount());

  const cc::Layer* white_layer = ContentLayerAt(0);
  EXPECT_THAT(
      white_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kWhite)));
  EXPECT_EQ(Translation(300, 350), white_layer->ScreenSpaceTransform());

  const cc::Layer* light_gray_layer = ContentLayerAt(1);
  EXPECT_THAT(
      light_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kLightGray)));
  EXPECT_EQ(Translation(300, 350), light_gray_layer->ScreenSpaceTransform());

  const cc::Layer* dark_gray_layer = ContentLayerAt(2);
  EXPECT_THAT(
      dark_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kDarkGray)));
  EXPECT_EQ(Translation(300, 350), dark_gray_layer->ScreenSpaceTransform());

  const cc::Layer* black_layer = ContentLayerAt(3);
  EXPECT_THAT(
      black_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kBlack)));
  EXPECT_EQ(Translation(300, 350), black_layer->ScreenSpaceTransform());

  EXPECT_EQ(white_layer->clip_tree_index(), dark_gray_layer->clip_tree_index());
  const cc::ClipNode* outer_clip =
      GetPropertyTrees().clip_tree.Node(white_layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, outer_clip->clip_type);
  EXPECT_EQ(gfx::RectF(100, 100, 700, 700), outer_clip->clip);

  EXPECT_EQ(light_gray_layer->clip_tree_index(),
            black_layer->clip_tree_index());
  const cc::ClipNode* inner_clip =
      GetPropertyTrees().clip_tree.Node(black_layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, inner_clip->clip_type);
  EXPECT_EQ(gfx::RectF(200, 200, 700, 700), inner_clip->clip);
  EXPECT_EQ(outer_clip->id, inner_clip->parent_id);
}

TEST_P(PaintArtifactCompositorTest, NestedClipsWithAlias) {
  auto real_clip1 =
      CreateClip(c0(), &t0(), FloatRoundedRect(100, 100, 700, 700),
                 CompositingReason::kOverflowScrollingTouch);
  auto clip1 = ClipPaintPropertyNode::CreateAlias(*real_clip1);
  auto real_clip2 =
      CreateClip(*clip1, &t0(), FloatRoundedRect(200, 200, 700, 700),
                 CompositingReason::kOverflowScrollingTouch);
  auto clip2 = ClipPaintPropertyNode::CreateAlias(*real_clip2);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clip1, e0())
      .RectDrawing(FloatRect(300, 350, 100, 100), Color::kWhite);
  artifact.Chunk(t0(), *clip2, e0())
      .RectDrawing(FloatRect(300, 350, 100, 100), Color::kLightGray);
  artifact.Chunk(t0(), *clip1, e0())
      .RectDrawing(FloatRect(300, 350, 100, 100), Color::kDarkGray);
  artifact.Chunk(t0(), *clip2, e0())
      .RectDrawing(FloatRect(300, 350, 100, 100), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(4u, ContentLayerCount());

  const cc::Layer* white_layer = ContentLayerAt(0);
  EXPECT_THAT(
      white_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kWhite)));
  EXPECT_EQ(Translation(300, 350), white_layer->ScreenSpaceTransform());

  const cc::Layer* light_gray_layer = ContentLayerAt(1);
  EXPECT_THAT(
      light_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kLightGray)));
  EXPECT_EQ(Translation(300, 350), light_gray_layer->ScreenSpaceTransform());

  const cc::Layer* dark_gray_layer = ContentLayerAt(2);
  EXPECT_THAT(
      dark_gray_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kDarkGray)));
  EXPECT_EQ(Translation(300, 350), dark_gray_layer->ScreenSpaceTransform());

  const cc::Layer* black_layer = ContentLayerAt(3);
  EXPECT_THAT(
      black_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kBlack)));
  EXPECT_EQ(Translation(300, 350), black_layer->ScreenSpaceTransform());

  EXPECT_EQ(white_layer->clip_tree_index(), dark_gray_layer->clip_tree_index());
  const cc::ClipNode* outer_clip =
      GetPropertyTrees().clip_tree.Node(white_layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, outer_clip->clip_type);
  EXPECT_EQ(gfx::RectF(100, 100, 700, 700), outer_clip->clip);

  EXPECT_EQ(light_gray_layer->clip_tree_index(),
            black_layer->clip_tree_index());
  const cc::ClipNode* inner_clip =
      GetPropertyTrees().clip_tree.Node(black_layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, inner_clip->clip_type);
  EXPECT_EQ(gfx::RectF(200, 200, 700, 700), inner_clip->clip);
  EXPECT_EQ(outer_clip->id, inner_clip->parent_id);
}

TEST_P(PaintArtifactCompositorTest, DeeplyNestedClips) {
  Vector<scoped_refptr<ClipPaintPropertyNode>> clips;
  for (unsigned i = 1; i <= 10; i++) {
    clips.push_back(CreateClip(clips.IsEmpty() ? c0() : *clips.back(), &t0(),
                               FloatRoundedRect(5 * i, 0, 100, 200 - 10 * i)));
  }

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clips.back(), e0())
      .RectDrawing(FloatRect(0, 0, 200, 200), Color::kWhite);
  Update(artifact.Build());

  // Check the drawing layer. It's clipped.
  ASSERT_EQ(1u, ContentLayerCount());
  const cc::Layer* drawing_layer = ContentLayerAt(0);
  EXPECT_EQ(gfx::Size(100, 100), drawing_layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(50, 0), drawing_layer->offset_to_transform_parent());
  EXPECT_THAT(
      drawing_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 150, 200), Color::kWhite)));
  EXPECT_EQ(Translation(50, 0), drawing_layer->ScreenSpaceTransform());

  // Check the clip nodes.
  const cc::ClipNode* clip_node =
      GetPropertyTrees().clip_tree.Node(drawing_layer->clip_tree_index());
  for (auto it = clips.rbegin(); it != clips.rend(); ++it) {
    const ClipPaintPropertyNode* paint_clip_node = it->get();
    EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, clip_node->clip_type);
    EXPECT_EQ(paint_clip_node->ClipRect().Rect(), clip_node->clip);
    clip_node = GetPropertyTrees().clip_tree.Node(clip_node->parent_id);
  }
}

TEST_P(PaintArtifactCompositorTest, SiblingClipsWithAlias) {
  auto real_common_clip =
      CreateClip(c0(), &t0(), FloatRoundedRect(0, 0, 800, 600));
  auto common_clip = ClipPaintPropertyNode::CreateAlias(*real_common_clip);
  auto real_clip1 =
      CreateClip(*common_clip, &t0(), FloatRoundedRect(0, 0, 400, 600));
  auto clip1 = ClipPaintPropertyNode::CreateAlias(*real_clip1);
  auto real_clip2 =
      CreateClip(*common_clip, &t0(), FloatRoundedRect(400, 0, 400, 600));
  auto clip2 = ClipPaintPropertyNode::CreateAlias(*real_clip2);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *clip1, e0())
      .RectDrawing(FloatRect(0, 0, 640, 480), Color::kWhite);
  artifact.Chunk(t0(), *clip2, e0())
      .RectDrawing(FloatRect(0, 0, 640, 480), Color::kBlack);
  Update(artifact.Build());
  ASSERT_EQ(2u, ContentLayerCount());

  const cc::Layer* white_layer = ContentLayerAt(0);
  EXPECT_THAT(
      white_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 640, 480), Color::kWhite)));
  EXPECT_EQ(gfx::Transform(), white_layer->ScreenSpaceTransform());
  const cc::ClipNode* white_clip =
      GetPropertyTrees().clip_tree.Node(white_layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, white_clip->clip_type);
  ASSERT_EQ(gfx::RectF(0, 0, 400, 600), white_clip->clip);

  const cc::Layer* black_layer = ContentLayerAt(1);
  // The layer is clipped.
  EXPECT_EQ(gfx::Size(240, 480), black_layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(400, 0), black_layer->offset_to_transform_parent());
  EXPECT_THAT(
      black_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 240, 480), Color::kBlack)));
  EXPECT_EQ(Translation(400, 0), black_layer->ScreenSpaceTransform());
  const cc::ClipNode* black_clip =
      GetPropertyTrees().clip_tree.Node(black_layer->clip_tree_index());
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP, black_clip->clip_type);
  ASSERT_EQ(gfx::RectF(400, 0, 400, 600), black_clip->clip);

  EXPECT_EQ(white_clip->parent_id, black_clip->parent_id);
  const cc::ClipNode* common_clip_node =
      GetPropertyTrees().clip_tree.Node(white_clip->parent_id);
  EXPECT_EQ(cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP,
            common_clip_node->clip_type);
  ASSERT_EQ(gfx::RectF(0, 0, 800, 600), common_clip_node->clip);
}

TEST_P(PaintArtifactCompositorTest, ForeignLayerPassesThrough) {
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  layer->SetIsDrawable(true);
  layer->SetBounds(gfx::Size(400, 300));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk().ForeignLayer(FloatPoint(50, 60), IntSize(400, 300),
                                     layer);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);

  ASSERT_EQ(3u, ContentLayerCount());
  EXPECT_EQ(layer, ContentLayerAt(1));
  EXPECT_EQ(gfx::Size(400, 300), layer->bounds());
  EXPECT_EQ(Translation(50, 60), layer->ScreenSpaceTransform());
}

TEST_P(PaintArtifactCompositorTest, EffectTreeConversionWithAlias) {
  EffectPaintPropertyNode::State effect1_state;
  effect1_state.local_transform_space = &t0();
  effect1_state.output_clip = &c0();
  effect1_state.opacity = 0.5;
  effect1_state.direct_compositing_reasons = CompositingReason::kAll;
  effect1_state.compositor_element_id = CompositorElementId(2);
  auto real_effect1 =
      EffectPaintPropertyNode::Create(e0(), std::move(effect1_state));
  auto effect1 = EffectPaintPropertyNode::CreateAlias(*real_effect1);
  auto real_effect2 =
      CreateOpacityEffect(*effect1, 0.3, CompositingReason::kAll);
  auto effect2 = EffectPaintPropertyNode::CreateAlias(*real_effect2);
  auto real_effect3 = CreateOpacityEffect(e0(), 0.2, CompositingReason::kAll);
  auto effect3 = EffectPaintPropertyNode::CreateAlias(*real_effect3);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *effect2)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  artifact.Chunk(t0(), c0(), *effect1)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  artifact.Chunk(t0(), c0(), *effect3)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  Update(artifact.Build());

  ASSERT_EQ(3u, ContentLayerCount());

  const cc::EffectTree& effect_tree = GetPropertyTrees().effect_tree;
  // Node #0 reserved for null; #1 for root render surface; #2 for
  // e0(), plus 3 nodes for those created by
  // this test.
  ASSERT_EQ(5u, effect_tree.size());

  const cc::EffectNode& converted_root_effect = *effect_tree.Node(1);
  EXPECT_EQ(-1, converted_root_effect.parent_id);
  EXPECT_EQ(CompositorElementIdFromUniqueObjectId(1).GetInternalValue(),
            converted_root_effect.stable_id);

  const cc::EffectNode& converted_effect1 = *effect_tree.Node(2);
  EXPECT_EQ(converted_root_effect.id, converted_effect1.parent_id);
  EXPECT_FLOAT_EQ(0.5, converted_effect1.opacity);
  EXPECT_EQ(2u, converted_effect1.stable_id);

  const cc::EffectNode& converted_effect2 = *effect_tree.Node(3);
  EXPECT_EQ(converted_effect1.id, converted_effect2.parent_id);
  EXPECT_FLOAT_EQ(0.3, converted_effect2.opacity);

  const cc::EffectNode& converted_effect3 = *effect_tree.Node(4);
  EXPECT_EQ(converted_root_effect.id, converted_effect3.parent_id);
  EXPECT_FLOAT_EQ(0.2, converted_effect3.opacity);

  EXPECT_EQ(converted_effect2.id, ContentLayerAt(0)->effect_tree_index());
  EXPECT_EQ(converted_effect1.id, ContentLayerAt(1)->effect_tree_index());
  EXPECT_EQ(converted_effect3.id, ContentLayerAt(2)->effect_tree_index());
}

// Returns a ScrollPaintPropertyNode::State with some arbitrary values.
static ScrollPaintPropertyNode::State ScrollState1() {
  ScrollPaintPropertyNode::State state;
  state.container_rect = IntRect(3, 5, 11, 13);
  state.contents_size = IntSize(27, 31);
  state.user_scrollable_horizontal = true;
  return state;
}

// Returns a ScrollPaintPropertyNode::State with another set arbitrary values.
static ScrollPaintPropertyNode::State ScrollState2() {
  ScrollPaintPropertyNode::State state;
  state.container_rect = IntRect(0, 0, 19, 23);
  state.contents_size = IntSize(29, 31);
  state.user_scrollable_horizontal = true;
  return state;
}

static scoped_refptr<ScrollPaintPropertyNode> CreateScroll(
    const ScrollPaintPropertyNode& parent,
    const ScrollPaintPropertyNode::State& state_arg,
    MainThreadScrollingReasons main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNotScrollingOnMain,
    CompositorElementId scroll_element_id = CompositorElementId()) {
  ScrollPaintPropertyNode::State state = state_arg;
  state.main_thread_scrolling_reasons = main_thread_scrolling_reasons;
  state.compositor_element_id = scroll_element_id;
  return ScrollPaintPropertyNode::Create(parent, std::move(state));
}

static void CheckCcScrollNode(const ScrollPaintPropertyNode& blink_scroll,
                              const cc::ScrollNode& cc_scroll) {
  EXPECT_EQ(static_cast<gfx::Size>(blink_scroll.ContainerRect().Size()),
            cc_scroll.container_bounds);
  EXPECT_EQ(static_cast<gfx::Size>(blink_scroll.ContentsSize()),
            cc_scroll.bounds);
  EXPECT_EQ(blink_scroll.UserScrollableHorizontal(),
            cc_scroll.user_scrollable_horizontal);
  EXPECT_EQ(blink_scroll.UserScrollableVertical(),
            cc_scroll.user_scrollable_vertical);
  EXPECT_EQ(blink_scroll.GetCompositorElementId(), cc_scroll.element_id);
  EXPECT_EQ(blink_scroll.GetMainThreadScrollingReasons(),
            cc_scroll.main_thread_scrolling_reasons);
}

TEST_P(PaintArtifactCompositorTest, OneScrollNode) {
  CompositorElementId scroll_element_id = ScrollElementId(2);
  auto scroll = CreateScroll(ScrollPaintPropertyNode::Root(), ScrollState1(),
                             kNotScrollingOnMain, scroll_element_id);
  auto scroll_translation = CreateScrollTranslation(t0(), 7, 9, *scroll);

  TestPaintArtifact artifact;
  CreateScrollableChunk(artifact, *scroll_translation, c0(), e0());
  artifact.Chunk(*scroll_translation, c0(), e0())
      .RectDrawing(FloatRect(-110, 12, 170, 19), Color::kWhite);
  Update(artifact.Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree;
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(3u, scroll_tree.size());
  const cc::ScrollNode& scroll_node = *scroll_tree.Node(2);
  CheckCcScrollNode(*scroll, scroll_node);
  EXPECT_EQ(1, scroll_node.parent_id);
  EXPECT_EQ(scroll_element_id, ScrollableLayerAt(0)->element_id());
  EXPECT_EQ(scroll_node.id, ElementIdToScrollNodeIndex(scroll_element_id));

  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree;
  const cc::TransformNode& transform_node =
      *transform_tree.Node(scroll_node.transform_id);
  EXPECT_TRUE(transform_node.local.IsIdentity());
  EXPECT_EQ(gfx::ScrollOffset(-7, -9), transform_node.scroll_offset);
  EXPECT_EQ(kNotScrollingOnMain, scroll_node.main_thread_scrolling_reasons);

  auto* layer = NonScrollableLayerAt(0);
  auto transform_node_index = layer->transform_tree_index();
  EXPECT_EQ(transform_node_index, transform_node.id);
  auto scroll_node_index = layer->scroll_tree_index();
  EXPECT_EQ(scroll_node_index, scroll_node.id);

  // The scrolling contents layer is clipped to the scrolling range.
  EXPECT_EQ(gfx::Size(27, 19), layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(0, 12), layer->offset_to_transform_parent());
  EXPECT_THAT(layer->GetPicture(),
              Pointee(DrawsRectangle(FloatRect(0, 0, 60, 19), Color::kWhite)));

  auto* scroll_layer = ScrollableLayerAt(0);
  EXPECT_TRUE(scroll_layer->scrollable());
  // The scroll layer should be sized to the container bounds.
  // TODO(pdr): The container bounds will not include scrollbars but the scroll
  // layer should extend below scrollbars.
  EXPECT_EQ(gfx::Size(11, 13), scroll_layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(3, 5), scroll_layer->offset_to_transform_parent());
  EXPECT_EQ(scroll_layer->scroll_tree_index(), scroll_node.id);

  EXPECT_EQ(0u, ScrollClient().did_scroll_count);
  scroll_layer->SetScrollOffsetFromImplSide(gfx::ScrollOffset(1, 2));
  EXPECT_EQ(1u, ScrollClient().did_scroll_count);
  EXPECT_EQ(gfx::ScrollOffset(1, 2), ScrollClient().last_scroll_offset);
}

TEST_P(PaintArtifactCompositorTest, TransformUnderScrollNode) {
  auto scroll = CreateScroll(ScrollPaintPropertyNode::Root(), ScrollState1());
  auto scroll_translation = CreateScrollTranslation(t0(), 7, 9, *scroll);

  auto transform =
      CreateTransform(*scroll_translation, TransformationMatrix(),
                      FloatPoint3D(), CompositingReason::k3DTransform);

  TestPaintArtifact artifact;
  artifact.Chunk(*scroll_translation, c0(), e0())
      .RectDrawing(FloatRect(-20, 4, 60, 8), Color::kBlack)
      .Chunk(*transform, c0(), e0())
      .RectDrawing(FloatRect(1, -30, 5, 70), Color::kWhite);
  Update(artifact.Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree;
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(3u, scroll_tree.size());
  const cc::ScrollNode& scroll_node = *scroll_tree.Node(2);

  // Both layers should refer to the same scroll tree node.
  const auto* layer0 = ContentLayerAt(0);
  const auto* layer1 = ContentLayerAt(1);
  EXPECT_EQ(scroll_node.id, layer0->scroll_tree_index());
  EXPECT_EQ(scroll_node.id, layer1->scroll_tree_index());

  // The scrolling layer is clipped to the scrollable range.
  EXPECT_EQ(gfx::Vector2dF(0, 4), layer0->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(27, 8), layer0->bounds());
  EXPECT_THAT(layer0->GetPicture(),
              Pointee(DrawsRectangle(FloatRect(0, 0, 40, 8), Color::kBlack)));

  // The layer under the transform without a scroll node is not clipped.
  EXPECT_EQ(gfx::Vector2dF(1, -30), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(5, 70), layer1->bounds());
  EXPECT_THAT(layer1->GetPicture(),
              Pointee(DrawsRectangle(FloatRect(0, 0, 5, 70), Color::kWhite)));

  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree;
  const cc::TransformNode& scroll_transform_node =
      *transform_tree.Node(scroll_node.transform_id);
  // The layers have different transform nodes.
  EXPECT_EQ(scroll_transform_node.id, layer0->transform_tree_index());
  EXPECT_NE(scroll_transform_node.id, layer1->transform_tree_index());
}

TEST_P(PaintArtifactCompositorTest, NestedScrollNodes) {
  auto effect = CreateOpacityEffect(e0(), 0.5);

  CompositorElementId scroll_element_id_a = ScrollElementId(2);
  auto scroll_a = CreateScroll(
      ScrollPaintPropertyNode::Root(), ScrollState1(),
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
      scroll_element_id_a);
  auto scroll_translation_a = CreateScrollTranslation(
      t0(), 11, 13, *scroll_a, CompositingReason::kLayerForScrollingContents);

  CompositorElementId scroll_element_id_b = ScrollElementId(3);
  auto scroll_b = CreateScroll(*scroll_a, ScrollState2(), kNotScrollingOnMain,
                               scroll_element_id_b);
  auto scroll_translation_b =
      CreateScrollTranslation(*scroll_translation_a, 37, 41, *scroll_b);
  TestPaintArtifact artifact;
  artifact.Chunk(*scroll_translation_a, c0(), *effect)
      .RectDrawing(FloatRect(7, 11, 13, 17), Color::kWhite);
  CreateScrollableChunk(artifact, *scroll_translation_a, c0(), *effect);
  artifact.Chunk(*scroll_translation_b, c0(), *effect)
      .RectDrawing(FloatRect(1, 2, 3, 5), Color::kWhite);
  CreateScrollableChunk(artifact, *scroll_translation_b, c0(), *effect);
  Update(artifact.Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree;
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(4u, scroll_tree.size());
  const cc::ScrollNode& scroll_node_a = *scroll_tree.Node(2);
  CheckCcScrollNode(*scroll_a, scroll_node_a);
  EXPECT_EQ(1, scroll_node_a.parent_id);
  EXPECT_EQ(scroll_element_id_a, ScrollableLayerAt(0)->element_id());
  EXPECT_EQ(scroll_node_a.id, ElementIdToScrollNodeIndex(scroll_element_id_a));

  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree;
  const cc::TransformNode& transform_node_a =
      *transform_tree.Node(scroll_node_a.transform_id);
  EXPECT_TRUE(transform_node_a.local.IsIdentity());
  EXPECT_EQ(gfx::ScrollOffset(-11, -13), transform_node_a.scroll_offset);

  const cc::ScrollNode& scroll_node_b = *scroll_tree.Node(3);
  CheckCcScrollNode(*scroll_b, scroll_node_b);
  EXPECT_EQ(scroll_node_a.id, scroll_node_b.parent_id);
  EXPECT_EQ(scroll_element_id_b, ScrollableLayerAt(1)->element_id());
  EXPECT_EQ(scroll_node_b.id, ElementIdToScrollNodeIndex(scroll_element_id_b));

  const cc::TransformNode& transform_node_b =
      *transform_tree.Node(scroll_node_b.transform_id);
  EXPECT_TRUE(transform_node_b.local.IsIdentity());
  EXPECT_EQ(gfx::ScrollOffset(-37, -41), transform_node_b.scroll_offset);
}

TEST_P(PaintArtifactCompositorTest, ScrollHitTestLayerOrder) {
  auto clip = CreateClip(c0(), &t0(), FloatRoundedRect(0, 0, 100, 100));

  CompositorElementId scroll_element_id = ScrollElementId(2);
  auto scroll = CreateScroll(ScrollPaintPropertyNode::Root(), ScrollState1(),
                             kNotScrollingOnMain, scroll_element_id);
  auto scroll_translation = CreateScrollTranslation(
      t0(), 7, 9, *scroll, CompositingReason::kWillChangeCompositingHint);

  auto transform = CreateTransform(
      *scroll_translation, TransformationMatrix().Translate(5, 5),
      FloatPoint3D(), CompositingReason::k3DTransform);

  TestPaintArtifact artifact;
  artifact.Chunk(*scroll_translation, *clip, e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  CreateScrollableChunk(artifact, *scroll_translation, *clip, e0());
  artifact.Chunk(*transform, *clip, e0())
      .RectDrawing(FloatRect(0, 0, 50, 50), Color::kBlack);
  Update(artifact.Build());

  // The first content layer (background) should not have the scrolling element
  // id set.
  EXPECT_EQ(CompositorElementId(), NonScrollableLayerAt(0)->element_id());

  // The scroll layer should be after the first content layer (background).
  EXPECT_LT(LayerIndex(NonScrollableLayerAt(0)),
            LayerIndex(ScrollableLayerAt(0)));
  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree;
  auto* scroll_node =
      scroll_tree.Node(ScrollableLayerAt(0)->scroll_tree_index());
  ASSERT_EQ(scroll_element_id, scroll_node->element_id);
  EXPECT_EQ(scroll_element_id, ScrollableLayerAt(0)->element_id());

  // The second content layer should appear after the first.
  EXPECT_LT(LayerIndex(ScrollableLayerAt(0)),
            LayerIndex(NonScrollableLayerAt(1)));
  EXPECT_EQ(CompositorElementId(), NonScrollableLayerAt(1)->element_id());
}

TEST_P(PaintArtifactCompositorTest, NestedScrollableLayerOrder) {
  auto clip_1 = CreateClip(c0(), &t0(), FloatRoundedRect(0, 0, 100, 100));
  CompositorElementId scroll_1_element_id = ScrollElementId(1);
  auto scroll_1 = CreateScroll(ScrollPaintPropertyNode::Root(), ScrollState1(),
                               kNotScrollingOnMain, scroll_1_element_id);
  auto scroll_translation_1 = CreateScrollTranslation(
      t0(), 7, 9, *scroll_1, CompositingReason::kWillChangeCompositingHint);

  auto clip_2 = CreateClip(*clip_1, scroll_translation_1.get(),
                           FloatRoundedRect(0, 0, 50, 50));
  CompositorElementId scroll_2_element_id = ScrollElementId(2);
  auto scroll_2 = CreateScroll(ScrollPaintPropertyNode::Root(), ScrollState2(),
                               kNotScrollingOnMain, scroll_2_element_id);
  auto scroll_translation_2 = CreateScrollTranslation(
      t0(), 0, 0, *scroll_2, CompositingReason::kWillChangeCompositingHint);

  TestPaintArtifact artifact;
  CreateScrollableChunk(artifact, *scroll_translation_1, *clip_1->Parent(),
                        e0());
  CreateScrollableChunk(artifact, *scroll_translation_2, *clip_2->Parent(),
                        e0());
  artifact.Chunk(*scroll_translation_2, *clip_2, e0())
      .RectDrawing(FloatRect(0, 0, 50, 50), Color::kWhite);
  Update(artifact.Build());

  // Two scroll layers should be created for each scroll translation node.
  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree;
  const cc::ClipTree& clip_tree = GetPropertyTrees().clip_tree;
  auto* scroll_1_node =
      scroll_tree.Node(ScrollableLayerAt(0)->scroll_tree_index());
  ASSERT_EQ(scroll_1_element_id, scroll_1_node->element_id);
  auto* scroll_1_clip_node =
      clip_tree.Node(ScrollableLayerAt(0)->clip_tree_index());
  // The scroll is not under clip_1.
  EXPECT_EQ(gfx::RectF(0, 0, 0, 0), scroll_1_clip_node->clip);

  auto* scroll_2_node =
      scroll_tree.Node(ScrollableLayerAt(1)->scroll_tree_index());
  ASSERT_EQ(scroll_2_element_id, scroll_2_node->element_id);
  auto* scroll_2_clip_node =
      clip_tree.Node(ScrollableLayerAt(1)->clip_tree_index());
  // The scroll is not under clip_2 but is under the parent clip, clip_1.
  EXPECT_EQ(gfx::RectF(0, 0, 100, 100), scroll_2_clip_node->clip);

  // The first layer should be before the second scroll layer.
  EXPECT_LT(LayerIndex(ScrollableLayerAt(0)), LayerIndex(ScrollableLayerAt(1)));

  // The non-scrollable content layer should be after the second scroll layer.
  EXPECT_LT(LayerIndex(ScrollableLayerAt(1)),
            LayerIndex(NonScrollableLayerAt(0)));
}

// If a scroll node is encountered before its parent, ensure the parent scroll
// node is correctly created.
TEST_P(PaintArtifactCompositorTest, AncestorScrollNodes) {
  CompositorElementId scroll_element_id_a = ScrollElementId(2);
  auto scroll_a = CreateScroll(ScrollPaintPropertyNode::Root(), ScrollState1(),
                               kNotScrollingOnMain, scroll_element_id_a);
  auto scroll_translation_a = CreateScrollTranslation(
      t0(), 11, 13, *scroll_a, CompositingReason::kLayerForScrollingContents);

  CompositorElementId scroll_element_id_b = ScrollElementId(3);
  auto scroll_b = CreateScroll(*scroll_a, ScrollState2(), kNotScrollingOnMain,
                               scroll_element_id_b);
  auto scroll_translation_b =
      CreateScrollTranslation(*scroll_translation_a, 37, 41, *scroll_b);

  TestPaintArtifact artifact;
  CreateScrollableChunk(artifact, *scroll_translation_b, c0(), e0());
  CreateScrollableChunk(artifact, *scroll_translation_a, c0(), e0());
  Update(artifact.Build());

  const cc::ScrollTree& scroll_tree = GetPropertyTrees().scroll_tree;
  // Node #0 reserved for null; #1 for root render surface.
  ASSERT_EQ(4u, scroll_tree.size());

  const cc::ScrollNode& scroll_node_a = *scroll_tree.Node(2);
  EXPECT_EQ(1, scroll_node_a.parent_id);
  EXPECT_EQ(scroll_element_id_a, scroll_node_a.element_id);
  EXPECT_EQ(scroll_node_a.id, ElementIdToScrollNodeIndex(scroll_element_id_a));
  // The second scrollable layer should be associated with the first scroll node
  // (a).
  EXPECT_EQ(scroll_element_id_a, ScrollableLayerAt(1)->element_id());

  const cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree;
  const cc::TransformNode& transform_node_a =
      *transform_tree.Node(scroll_node_a.transform_id);
  EXPECT_TRUE(transform_node_a.local.IsIdentity());
  EXPECT_EQ(gfx::ScrollOffset(-11, -13), transform_node_a.scroll_offset);

  const cc::ScrollNode& scroll_node_b = *scroll_tree.Node(3);
  EXPECT_EQ(scroll_node_a.id, scroll_node_b.parent_id);
  EXPECT_EQ(scroll_element_id_b, scroll_node_b.element_id);
  EXPECT_EQ(scroll_node_b.id, ElementIdToScrollNodeIndex(scroll_element_id_b));
  // The first scrollable layer should be associated with the second scroll node
  // (b).
  EXPECT_EQ(scroll_element_id_b, ScrollableLayerAt(0)->element_id());

  const cc::TransformNode& transform_node_b =
      *transform_tree.Node(scroll_node_b.transform_id);
  EXPECT_TRUE(transform_node_b.local.IsIdentity());
  EXPECT_EQ(gfx::ScrollOffset(-37, -41), transform_node_b.scroll_offset);
}

TEST_P(PaintArtifactCompositorTest, MergeSimpleChunks) {
  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(2u, artifact->PaintChunks().size());
  Update(artifact);

  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, MergeClip) {
  auto clip = CreateClip(c0(), &t0(), FloatRoundedRect(10, 20, 50, 60));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), *clip, e0())
      .RectDrawing(FloatRect(0, 0, 200, 300), Color::kBlack);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 300, 400), Color::kGray);

  auto artifact = test_artifact.Build();

  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // Clip is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(FloatRect(10, 20, 50, 60), Color::kBlack));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 300, 400), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, Merge2DTransform) {
  auto transform =
      CreateTransform(t0(), TransformationMatrix().Translate(50, 50),
                      FloatPoint3D(100, 100, 0));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);

  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(FloatRect(50, 50, 100, 100), Color::kBlack));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, Merge2DTransformDirectAncestor) {
  auto transform = CreateTransform(t0(), TransformationMatrix(), FloatPoint3D(),
                                   CompositingReason::k3DTransform);
  auto transform2 =
      CreateTransform(*transform, TransformationMatrix().Translate(50, 50),
                      FloatPoint3D(100, 100, 0));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  // The second chunk can merge into the first because it has a descendant
  // state of the first's transform and no direct compositing reason.
  test_artifact.Chunk(*transform2, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(2u, artifact->PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(FloatRect(50, 50, 100, 100), Color::kBlack));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, MergeTransformOrigin) {
  auto transform = CreateTransform(t0(), TransformationMatrix().Rotate(45),
                                   FloatPoint3D(100, 100, 0));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 42, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(RectWithColor(
        FloatRect(29.2893, 0.578644, 141.421, 141.421), Color::kBlack));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 42, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, MergeOpacity) {
  float opacity = 2.0 / 255.0;
  auto effect = CreateOpacityEffect(e0(), opacity);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100),
                      Color(Color::kBlack).CombineWithAlpha(opacity).Rgb()));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, MergeOpacityWithAlias) {
  float opacity = 2.0 / 255.0;
  auto real_effect = CreateOpacityEffect(e0(), opacity);
  auto effect = EffectPaintPropertyNode::CreateAlias(*real_effect);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100),
                      Color(Color::kBlack).CombineWithAlpha(opacity).Rgb()));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, MergeNestedWithAlias) {
  // Tests merging of an opacity effect, inside of a clip, inside of a
  // transform.
  auto real_transform =
      CreateTransform(t0(), TransformationMatrix().Translate(50, 50),
                      FloatPoint3D(100, 100, 0));
  auto transform = TransformPaintPropertyNode::CreateAlias(*real_transform);
  auto real_clip =
      CreateClip(c0(), transform.get(), FloatRoundedRect(10, 20, 50, 60));
  auto clip = ClipPaintPropertyNode::CreateAlias(*real_clip);
  float opacity = 2.0 / 255.0;
  auto real_effect =
      CreateOpacityEffect(e0(), transform.get(), clip.get(), opacity);
  auto effect = EffectPaintPropertyNode::CreateAlias(*real_effect);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(*transform, *clip, *effect)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // Transform is applied to this PaintChunk.
    rects_with_color.push_back(
        RectWithColor(FloatRect(60, 70, 50, 60),
                      Color(Color::kBlack).CombineWithAlpha(opacity).Rgb()));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, ClipPushedUp) {
  // Tests merging of an element which has a clipapplied to it,
  // but has an ancestor transform of them. This can happen for fixed-
  // or absolute-position elements which escape scroll transforms.
  auto transform =
      CreateTransform(t0(), TransformationMatrix().Translate(20, 25),
                      FloatPoint3D(100, 100, 0));
  auto transform2 =
      CreateTransform(*transform, TransformationMatrix().Translate(20, 25),
                      FloatPoint3D(100, 100, 0));
  auto clip =
      CreateClip(c0(), transform2.get(), FloatRoundedRect(10, 20, 50, 60));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), *clip, e0())
      .RectDrawing(FloatRect(0, 0, 300, 400), Color::kBlack);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // The two transforms (combined translation of (40, 50)) are applied here,
    // before clipping.
    rects_with_color.push_back(
        RectWithColor(FloatRect(50, 70, 50, 60), Color(Color::kBlack)));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

// TODO(crbug.com/696842): The effect refuses to "decomposite" because it's in
// a deeper transform space than its chunk. We should allow decomposite if
// the two transform nodes share the same direct compositing ancestor.
TEST_P(PaintArtifactCompositorTest, DISABLED_EffectPushedUp) {
  // Tests merging of an element which has an effect applied to it,
  // but has an ancestor transform of them. This can happen for fixed-
  // or absolute-position elements which escape scroll transforms.

  auto transform =
      CreateTransform(t0(), TransformationMatrix().Translate(20, 25),
                      FloatPoint3D(100, 100, 0));

  auto transform2 =
      CreateTransform(*transform, TransformationMatrix().Translate(20, 25),
                      FloatPoint3D(100, 100, 0));

  float opacity = 2.0 / 255.0;
  auto effect = CreateOpacityEffect(e0(), transform2.get(), &c0(), opacity);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(FloatRect(0, 0, 300, 400), Color::kBlack);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 300, 400),
                      Color(Color::kBlack).CombineWithAlpha(opacity).Rgb()));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

// TODO(crbug.com/696842): The effect refuses to "decomposite" because it's in
// a deeper transform space than its chunk. We should allow decomposite if
// the two transform nodes share the same direct compositing ancestor.
TEST_P(PaintArtifactCompositorTest, DISABLED_EffectAndClipPushedUp) {
  // Tests merging of an element which has an effect applied to it,
  // but has an ancestor transform of them. This can happen for fixed-
  // or absolute-position elements which escape scroll transforms.
  auto transform =
      CreateTransform(t0(), TransformationMatrix().Translate(20, 25),
                      FloatPoint3D(100, 100, 0));
  auto transform2 =
      CreateTransform(*transform, TransformationMatrix().Translate(20, 25),
                      FloatPoint3D(100, 100, 0));
  auto clip =
      CreateClip(c0(), transform.get(), FloatRoundedRect(10, 20, 50, 60));

  float opacity = 2.0 / 255.0;
  auto effect =
      CreateOpacityEffect(e0(), transform2.get(), clip.get(), opacity);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), *clip, *effect)
      .RectDrawing(FloatRect(0, 0, 300, 400), Color::kBlack);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // The clip is under |transform| but not |transform2|, so only an adjustment
    // of (20, 25) occurs.
    rects_with_color.push_back(
        RectWithColor(FloatRect(30, 45, 50, 60),
                      Color(Color::kBlack).CombineWithAlpha(opacity).Rgb()));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, ClipAndEffectNoTransform) {
  // Tests merging of an element which has a clip and effect in the root
  // transform space.
  auto clip = CreateClip(c0(), &t0(), FloatRoundedRect(10, 20, 50, 60));
  float opacity = 2.0 / 255.0;
  auto effect = CreateOpacityEffect(e0(), &t0(), clip.get(), opacity);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), *clip, *effect)
      .RectDrawing(FloatRect(0, 0, 300, 400), Color::kBlack);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(FloatRect(10, 20, 50, 60),
                      Color(Color::kBlack).CombineWithAlpha(opacity).Rgb()));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, TwoClips) {
  // Tests merging of an element which has two clips in the root
  // transform space.
  auto clip = CreateClip(c0(), &t0(), FloatRoundedRect(20, 30, 10, 20));
  auto clip2 = CreateClip(*clip, &t0(), FloatRoundedRect(10, 20, 50, 60));

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(t0(), *clip2, e0())
      .RectDrawing(FloatRect(0, 0, 300, 400), Color::kBlack);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    // The interesction of the two clips is (20, 30, 10, 20).
    rects_with_color.push_back(
        RectWithColor(FloatRect(20, 30, 10, 20), Color(Color::kBlack)));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));

    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, TwoTransformsClipBetween) {
  auto transform =
      CreateTransform(t0(), TransformationMatrix().Translate(20, 25),
                      FloatPoint3D(100, 100, 0));
  auto clip = CreateClip(c0(), &t0(), FloatRoundedRect(0, 0, 50, 60));
  auto transform2 =
      CreateTransform(*transform, TransformationMatrix().Translate(20, 25),
                      FloatPoint3D(100, 100, 0));
  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(*transform2, *clip, e0())
      .RectDrawing(FloatRect(0, 0, 300, 400), Color::kBlack);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);
  ASSERT_EQ(1u, ContentLayerCount());
  {
    Vector<RectWithColor> rects_with_color;
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 100, 100), Color::kWhite));
    rects_with_color.push_back(
        RectWithColor(FloatRect(40, 50, 10, 10), Color(Color::kBlack)));
    rects_with_color.push_back(
        RectWithColor(FloatRect(0, 0, 200, 300), Color::kGray));
    const cc::Layer* layer = ContentLayerAt(0);
    EXPECT_THAT(layer->GetPicture(),
                Pointee(DrawsRectangles(rects_with_color)));
  }
}

TEST_P(PaintArtifactCompositorTest, OverlapTransform) {
  auto transform = CreateTransform(
      t0(), TransformationMatrix().Translate(50, 50), FloatPoint3D(100, 100, 0),
      CompositingReason::k3DTransform);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 200, 300), Color::kGray);

  auto artifact = test_artifact.Build();
  ASSERT_EQ(3u, artifact->PaintChunks().size());
  Update(artifact);
  // The third paint chunk overlaps the second but can't merge due to
  // incompatible transform. The second paint chunk can't merge into the first
  // due to a direct compositing reason.
  ASSERT_EQ(3u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest, MightOverlap) {
  PaintChunk paint_chunk = DefaultChunk();
  paint_chunk.bounds = FloatRect(0, 0, 100, 100);
  PendingLayer pending_layer(paint_chunk, 0, false);

  PaintChunk paint_chunk2 = DefaultChunk();
  paint_chunk2.bounds = FloatRect(0, 0, 100, 100);

  {
    PendingLayer pending_layer2(paint_chunk2, 1, false);
    EXPECT_TRUE(MightOverlap(pending_layer, pending_layer2));
  }

  auto transform = CreateTransform(
      t0(), TransformationMatrix().Translate(99, 0), FloatPoint3D(100, 100, 0));
  {
    paint_chunk2.properties.SetTransform(transform.get());
    PendingLayer pending_layer2(paint_chunk2, 1, false);
    EXPECT_TRUE(MightOverlap(pending_layer, pending_layer2));
  }

  auto transform2 =
      CreateTransform(t0(), TransformationMatrix().Translate(100, 0),
                      FloatPoint3D(100, 100, 0));
  {
    paint_chunk2.properties.SetTransform(transform2.get());
    PendingLayer pending_layer2(paint_chunk2, 1, false);
    EXPECT_FALSE(MightOverlap(pending_layer, pending_layer2));
  }
}

TEST_P(PaintArtifactCompositorTest, PendingLayer) {
  PaintChunk chunk1 = DefaultChunk();
  chunk1.properties = PropertyTreeState::Root();
  chunk1.known_to_be_opaque = true;
  chunk1.bounds = FloatRect(0, 0, 30, 40);

  PendingLayer pending_layer(chunk1, 0, false);

  EXPECT_EQ(FloatRect(0, 0, 30, 40), pending_layer.bounds);
  EXPECT_EQ((Vector<size_t>{0}), pending_layer.paint_chunk_indices);
  EXPECT_EQ(pending_layer.bounds, pending_layer.rect_known_to_be_opaque);

  PaintChunk chunk2 = DefaultChunk();
  chunk2.properties = chunk1.properties;
  chunk2.known_to_be_opaque = true;
  chunk2.bounds = FloatRect(10, 20, 30, 40);
  pending_layer.Merge(PendingLayer(chunk2, 1, false));

  // Bounds not equal to one PaintChunk.
  EXPECT_EQ(FloatRect(0, 0, 40, 60), pending_layer.bounds);
  EXPECT_EQ((Vector<size_t>{0, 1}), pending_layer.paint_chunk_indices);
  EXPECT_NE(pending_layer.bounds, pending_layer.rect_known_to_be_opaque);

  PaintChunk chunk3 = DefaultChunk();
  chunk3.properties = chunk1.properties;
  chunk3.known_to_be_opaque = true;
  chunk3.bounds = FloatRect(-5, -25, 20, 20);
  pending_layer.Merge(PendingLayer(chunk3, 2, false));

  EXPECT_EQ(FloatRect(-5, -25, 45, 85), pending_layer.bounds);
  EXPECT_EQ((Vector<size_t>{0, 1, 2}), pending_layer.paint_chunk_indices);
  EXPECT_NE(pending_layer.bounds, pending_layer.rect_known_to_be_opaque);
}

TEST_P(PaintArtifactCompositorTest, PendingLayerWithGeometry) {
  auto transform =
      CreateTransform(t0(), TransformationMatrix().Translate(20, 25),
                      FloatPoint3D(100, 100, 0));

  PaintChunk chunk1 = DefaultChunk();
  chunk1.properties = PropertyTreeState::Root();
  chunk1.bounds = FloatRect(0, 0, 30, 40);

  PendingLayer pending_layer(chunk1, 0, false);

  EXPECT_EQ(FloatRect(0, 0, 30, 40), pending_layer.bounds);

  PaintChunk chunk2 = DefaultChunk();
  chunk2.properties = chunk1.properties;
  chunk2.properties.SetTransform(transform.get());
  chunk2.bounds = FloatRect(0, 0, 50, 60);
  pending_layer.Merge(PendingLayer(chunk2, 1, false));

  EXPECT_EQ(FloatRect(0, 0, 70, 85), pending_layer.bounds);
}

// TODO(crbug.com/701991):
// The test is disabled because opaque rect mapping is not implemented yet.
TEST_P(PaintArtifactCompositorTest, DISABLED_PendingLayerKnownOpaque) {
  PaintChunk chunk1 = DefaultChunk();
  chunk1.properties = PropertyTreeState::Root();
  chunk1.bounds = FloatRect(0, 0, 30, 40);
  chunk1.known_to_be_opaque = false;
  PendingLayer pending_layer(chunk1, 0, false);

  EXPECT_TRUE(pending_layer.rect_known_to_be_opaque.IsEmpty());

  PaintChunk chunk2 = DefaultChunk();
  chunk2.properties = chunk1.properties;
  chunk2.bounds = FloatRect(0, 0, 25, 35);
  chunk2.known_to_be_opaque = true;
  pending_layer.Merge(PendingLayer(chunk2, 1, false));

  // Chunk 2 doesn't cover the entire layer, so not opaque.
  EXPECT_EQ(chunk2.bounds, pending_layer.rect_known_to_be_opaque);
  EXPECT_NE(pending_layer.bounds, pending_layer.rect_known_to_be_opaque);

  PaintChunk chunk3 = DefaultChunk();
  chunk3.properties = chunk1.properties;
  chunk3.bounds = FloatRect(0, 0, 50, 60);
  chunk3.known_to_be_opaque = true;
  pending_layer.Merge(PendingLayer(chunk3, 2, false));

  // Chunk 3 covers the entire layer, so now it's opaque.
  EXPECT_EQ(chunk3.bounds, pending_layer.bounds);
  EXPECT_EQ(pending_layer.bounds, pending_layer.rect_known_to_be_opaque);
}

scoped_refptr<EffectPaintPropertyNode> CreateSampleEffectNodeWithElementId() {
  EffectPaintPropertyNode::State state;
  state.local_transform_space = &t0();
  state.output_clip = &c0();
  state.opacity = 2.0 / 255.0;
  state.direct_compositing_reasons = CompositingReason::kActiveOpacityAnimation;
  state.compositor_element_id = CompositorElementId(2);
  return EffectPaintPropertyNode::Create(e0(), std::move(state));
}

scoped_refptr<TransformPaintPropertyNode>
CreateSampleTransformNodeWithElementId() {
  TransformPaintPropertyNode::State state;
  state.matrix.Rotate(90);
  state.origin = FloatPoint3D(100, 100, 0);
  state.direct_compositing_reasons = CompositingReason::k3DTransform;
  state.compositor_element_id = CompositorElementId(3);
  return TransformPaintPropertyNode::Create(t0(), std::move(state));
}

TEST_P(PaintArtifactCompositorTest, TransformWithElementId) {
  auto transform = CreateSampleTransformNodeWithElementId();
  TestPaintArtifact artifact;
  artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  EXPECT_EQ(2,
            ElementIdToTransformNodeIndex(transform->GetCompositorElementId()));
}

TEST_P(PaintArtifactCompositorTest, EffectWithElementId) {
  auto effect = CreateSampleEffectNodeWithElementId();
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  EXPECT_EQ(2, ElementIdToEffectNodeIndex(effect->GetCompositorElementId()));
}

TEST_P(PaintArtifactCompositorTest, EffectWithElementIdWithAlias) {
  auto real_effect = CreateSampleEffectNodeWithElementId();
  auto effect = EffectPaintPropertyNode::CreateAlias(*real_effect);
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  EXPECT_EQ(2,
            ElementIdToEffectNodeIndex(real_effect->GetCompositorElementId()));
}

TEST_P(PaintArtifactCompositorTest, CompositedLuminanceMask) {
  auto masked = CreateOpacityEffect(
      e0(), 1.0, CompositingReason::kIsolateCompositedDescendants);
  EffectPaintPropertyNode::State masking_state;
  masking_state.local_transform_space = &t0();
  masking_state.output_clip = &c0();
  masking_state.color_filter = kColorFilterLuminanceToAlpha;
  masking_state.blend_mode = SkBlendMode::kDstIn;
  masking_state.direct_compositing_reasons =
      CompositingReason::kSquashingDisallowed;
  auto masking =
      EffectPaintPropertyNode::Create(*masked, std::move(masking_state));

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *masked)
      .RectDrawing(FloatRect(100, 100, 200, 200), Color::kGray);
  artifact.Chunk(t0(), c0(), *masking)
      .RectDrawing(FloatRect(150, 150, 100, 100), Color::kWhite);
  Update(artifact.Build());
  ASSERT_EQ(2u, ContentLayerCount());

  const cc::Layer* masked_layer = ContentLayerAt(0);
  EXPECT_THAT(masked_layer->GetPicture(),
              Pointee(DrawsRectangle(FloatRect(0, 0, 200, 200), Color::kGray)));
  EXPECT_EQ(Translation(100, 100), masked_layer->ScreenSpaceTransform());
  EXPECT_EQ(gfx::Size(200, 200), masked_layer->bounds());
  const cc::EffectNode* masked_group =
      GetPropertyTrees().effect_tree.Node(masked_layer->effect_tree_index());
  EXPECT_TRUE(masked_group->has_render_surface);

  const cc::Layer* masking_layer = ContentLayerAt(1);
  EXPECT_THAT(
      masking_layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 100, 100), Color::kWhite)));
  EXPECT_EQ(Translation(150, 150), masking_layer->ScreenSpaceTransform());
  EXPECT_EQ(gfx::Size(100, 100), masking_layer->bounds());
  const cc::EffectNode* masking_group =
      GetPropertyTrees().effect_tree.Node(masking_layer->effect_tree_index());
  EXPECT_TRUE(masking_group->has_render_surface);
  EXPECT_EQ(masked_group->id, masking_group->parent_id);
  ASSERT_EQ(1u, masking_group->filters.size());
  EXPECT_EQ(cc::FilterOperation::REFERENCE,
            masking_group->filters.at(0).type());
}

TEST_P(PaintArtifactCompositorTest, UpdateProducesNewSequenceNumber) {
  // A 90 degree clockwise rotation about (100, 100).
  auto transform = CreateTransform(t0(), TransformationMatrix().Rotate(90),
                                   FloatPoint3D(100, 100, 0),
                                   CompositingReason::k3DTransform);
  auto clip = CreateClip(c0(), &t0(), FloatRoundedRect(100, 100, 300, 200));
  auto effect = CreateOpacityEffect(e0(), 0.5);

  TestPaintArtifact test_artifact;
  test_artifact.Chunk(*transform, *clip, *effect)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kWhite);
  test_artifact.Chunk().RectDrawing(FloatRect(0, 0, 100, 100), Color::kGray);
  auto artifact = test_artifact.Build();

  Update(artifact);

  // Two content layers for the differentiated rect drawings and three dummy
  // layers for each of the transform, clip and effect nodes.
  EXPECT_EQ(2u, RootLayer()->children().size());
  int sequence_number = GetPropertyTrees().sequence_number;
  EXPECT_GT(sequence_number, 0);
  for (auto layer : RootLayer()->children()) {
    EXPECT_EQ(sequence_number, layer->property_tree_sequence_number());
  }

  Update(artifact);

  EXPECT_EQ(2u, RootLayer()->children().size());
  sequence_number++;
  EXPECT_EQ(sequence_number, GetPropertyTrees().sequence_number);
  for (auto layer : RootLayer()->children()) {
    EXPECT_EQ(sequence_number, layer->property_tree_sequence_number());
  }

  Update(artifact);

  EXPECT_EQ(2u, RootLayer()->children().size());
  sequence_number++;
  EXPECT_EQ(sequence_number, GetPropertyTrees().sequence_number);
  for (auto layer : RootLayer()->children()) {
    EXPECT_EQ(sequence_number, layer->property_tree_sequence_number());
  }
}

TEST_P(PaintArtifactCompositorTest, DecompositeClip) {
  // A clipped paint chunk that gets merged into a previous layer should
  // only contribute clipped bounds to the layer bound.
  auto clip = CreateClip(c0(), &t0(), FloatRoundedRect(75, 75, 100, 100));

  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(FloatRect(50, 50, 100, 100), Color::kGray);
  artifact.Chunk(t0(), *clip, e0())
      .RectDrawing(FloatRect(100, 100, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());

  const cc::Layer* layer = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 50.f), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(125, 125), layer->bounds());
}

TEST_P(PaintArtifactCompositorTest, DecompositeEffect) {
  // An effect node without direct compositing reason and does not need to
  // group compositing descendants should not be composited and can merge
  // with other chunks.

  auto effect = CreateOpacityEffect(e0(), 0.5);

  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(FloatRect(50, 25, 100, 100), Color::kGray);
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(FloatRect(25, 75, 100, 100), Color::kGray);
  artifact.Chunk().RectDrawing(FloatRect(75, 75, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());

  const cc::Layer* layer = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(25.f, 25.f), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(150, 150), layer->bounds());
  EXPECT_EQ(1, layer->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, DirectlyCompositedEffect) {
  // An effect node with direct compositing shall be composited.
  auto effect = CreateOpacityEffect(e0(), 0.5f, CompositingReason::kAll);

  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(FloatRect(50, 25, 100, 100), Color::kGray);
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(FloatRect(25, 75, 100, 100), Color::kGray);
  artifact.Chunk().RectDrawing(FloatRect(75, 75, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(3u, ContentLayerCount());

  const cc::Layer* layer1 = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 25.f), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer1->bounds());
  EXPECT_EQ(1, layer1->effect_tree_index());

  const cc::Layer* layer2 = ContentLayerAt(1);
  EXPECT_EQ(gfx::Vector2dF(25.f, 75.f), layer2->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer2->bounds());
  const cc::EffectNode* effect_node =
      GetPropertyTrees().effect_tree.Node(layer2->effect_tree_index());
  EXPECT_EQ(1, effect_node->parent_id);
  EXPECT_EQ(0.5f, effect_node->opacity);

  const cc::Layer* layer3 = ContentLayerAt(2);
  EXPECT_EQ(gfx::Vector2dF(75.f, 75.f), layer3->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer3->bounds());
  EXPECT_EQ(1, layer3->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, DecompositeDeepEffect) {
  // A paint chunk may enter multiple level effects with or without compositing
  // reasons. This test verifies we still decomposite effects without a direct
  // reason, but stop at a directly composited effect.
  auto effect1 = CreateOpacityEffect(e0(), 0.1f);
  auto effect2 = CreateOpacityEffect(*effect1, 0.2f, CompositingReason::kAll);
  auto effect3 = CreateOpacityEffect(*effect2, 0.3f);

  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(FloatRect(50, 25, 100, 100), Color::kGray);
  artifact.Chunk(t0(), c0(), *effect3)
      .RectDrawing(FloatRect(25, 75, 100, 100), Color::kGray);
  artifact.Chunk().RectDrawing(FloatRect(75, 75, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(3u, ContentLayerCount());

  const cc::Layer* layer1 = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 25.f), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer1->bounds());
  EXPECT_EQ(1, layer1->effect_tree_index());

  const cc::Layer* layer2 = ContentLayerAt(1);
  EXPECT_EQ(gfx::Vector2dF(25.f, 75.f), layer2->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer2->bounds());
  const cc::EffectNode* effect_node2 =
      GetPropertyTrees().effect_tree.Node(layer2->effect_tree_index());
  EXPECT_EQ(0.2f, effect_node2->opacity);
  const cc::EffectNode* effect_node1 =
      GetPropertyTrees().effect_tree.Node(effect_node2->parent_id);
  EXPECT_EQ(1, effect_node1->parent_id);
  EXPECT_EQ(0.1f, effect_node1->opacity);

  const cc::Layer* layer3 = ContentLayerAt(2);
  EXPECT_EQ(gfx::Vector2dF(75.f, 75.f), layer3->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer3->bounds());
  EXPECT_EQ(1, layer3->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, IndirectlyCompositedEffect) {
  // An effect node without direct compositing still needs to be composited
  // for grouping, if some chunks need to be composited.
  auto effect = CreateOpacityEffect(e0(), 0.5f);
  auto transform = CreateTransform(t0(), TransformationMatrix(), FloatPoint3D(),
                                   CompositingReason::k3DTransform);

  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(FloatRect(50, 25, 100, 100), Color::kGray);
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(FloatRect(25, 75, 100, 100), Color::kGray);
  artifact.Chunk(*transform, c0(), *effect)
      .RectDrawing(FloatRect(75, 75, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(3u, ContentLayerCount());

  const cc::Layer* layer1 = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 25.f), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer1->bounds());
  EXPECT_EQ(1, layer1->effect_tree_index());

  const cc::Layer* layer2 = ContentLayerAt(1);
  EXPECT_EQ(gfx::Vector2dF(25.f, 75.f), layer2->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer2->bounds());
  const cc::EffectNode* effect_node =
      GetPropertyTrees().effect_tree.Node(layer2->effect_tree_index());
  EXPECT_EQ(1, effect_node->parent_id);
  EXPECT_EQ(0.5f, effect_node->opacity);

  const cc::Layer* layer3 = ContentLayerAt(2);
  EXPECT_EQ(gfx::Vector2dF(75.f, 75.f), layer3->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(100, 100), layer3->bounds());
  EXPECT_EQ(effect_node->id, layer3->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, DecompositedEffectNotMergingDueToOverlap) {
  // This tests an effect that doesn't need to be composited, but needs
  // separate backing due to overlap with a previous composited effect.
  auto effect1 = CreateOpacityEffect(e0(), 0.1f);
  auto effect2 = CreateOpacityEffect(e0(), 0.2f);
  auto transform = CreateTransform(t0(), TransformationMatrix(), FloatPoint3D(),
                                   CompositingReason::k3DTransform);
  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(FloatRect(0, 0, 50, 50), Color::kGray);
  artifact.Chunk(t0(), c0(), *effect1)
      .RectDrawing(FloatRect(100, 0, 50, 50), Color::kGray);
  // This chunk has a transform that must be composited, thus causing effect1
  // to be composited too.
  artifact.Chunk(*transform, c0(), *effect1)
      .RectDrawing(FloatRect(200, 0, 50, 50), Color::kGray);
  artifact.Chunk(t0(), c0(), *effect2)
      .RectDrawing(FloatRect(200, 100, 50, 50), Color::kGray);
  // This chunk overlaps with the 2nd chunk, but is seemingly safe to merge.
  // However because effect1 gets composited due to a composited transform,
  // we can't merge with effect1 nor skip it to merge with the first chunk.
  artifact.Chunk(t0(), c0(), *effect2)
      .RectDrawing(FloatRect(100, 0, 50, 50), Color::kGray);

  Update(artifact.Build());
  ASSERT_EQ(4u, ContentLayerCount());

  const cc::Layer* layer1 = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(0.f, 0.f), layer1->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(50, 50), layer1->bounds());
  EXPECT_EQ(1, layer1->effect_tree_index());

  const cc::Layer* layer2 = ContentLayerAt(1);
  EXPECT_EQ(gfx::Vector2dF(100.f, 0.f), layer2->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(50, 50), layer2->bounds());
  const cc::EffectNode* effect_node =
      GetPropertyTrees().effect_tree.Node(layer2->effect_tree_index());
  EXPECT_EQ(1, effect_node->parent_id);
  EXPECT_EQ(0.1f, effect_node->opacity);

  const cc::Layer* layer3 = ContentLayerAt(2);
  EXPECT_EQ(gfx::Vector2dF(200.f, 0.f), layer3->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(50, 50), layer3->bounds());
  EXPECT_EQ(effect_node->id, layer3->effect_tree_index());

  const cc::Layer* layer4 = ContentLayerAt(3);
  EXPECT_EQ(gfx::Vector2dF(100.f, 0.f), layer4->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(150, 150), layer4->bounds());
  EXPECT_EQ(1, layer4->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, UpdatePopulatesCompositedElementIds) {
  auto transform = CreateSampleTransformNodeWithElementId();
  auto effect = CreateSampleEffectNodeWithElementId();
  TestPaintArtifact artifact;
  artifact.Chunk(*transform, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack)
      .Chunk(t0(), c0(), *effect)
      .RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);

  CompositorElementIdSet composited_element_ids;
  Update(artifact.Build(), composited_element_ids);

  EXPECT_EQ(2u, composited_element_ids.size());
  EXPECT_TRUE(
      composited_element_ids.count(transform->GetCompositorElementId()));
  EXPECT_TRUE(composited_element_ids.count(effect->GetCompositorElementId()));
}

TEST_P(PaintArtifactCompositorTest, SkipChunkWithOpacityZero) {
  UpdateWithArtifactWithOpacity(0, false, false);
  ASSERT_EQ(0u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest,
       SkipChunkWithOpacityZeroWithPrecedingChunk) {
  UpdateWithArtifactWithOpacity(0, true, false);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest, SkipChunkWithOpacityZeroSubsequentChunk) {
  UpdateWithArtifactWithOpacity(0, false, true);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest,
       SkipChunkWithOpacityZeroWithPrecedingAndSubsequentChunk) {
  UpdateWithArtifactWithOpacity(0, true, true);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest, SkipChunkWithTinyOpacity) {
  UpdateWithArtifactWithOpacity(0.0003f, false, false);
  ASSERT_EQ(0u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest,
       SkipChunkWithTinyOpacityWithPrecedingChunk) {
  UpdateWithArtifactWithOpacity(0.0003f, true, false);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest, SkipChunkWithTinyOpacitySubsequentChunk) {
  UpdateWithArtifactWithOpacity(0.0003f, false, true);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest,
       SkipChunkWithTinyOpacityWithPrecedingAndSubsequentChunk) {
  UpdateWithArtifactWithOpacity(0.0003f, true, true);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest, DontSkipChunkWithMinimumOpacity) {
  UpdateWithArtifactWithOpacity(0.0004f, false, false);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest,
       DontSkipChunkWithMinimumOpacityWithPrecedingChunk) {
  UpdateWithArtifactWithOpacity(0.0004f, true, false);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest,
       DontSkipChunkWithMinimumOpacitySubsequentChunk) {
  UpdateWithArtifactWithOpacity(0.0004f, false, true);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest,
       DontSkipChunkWithMinimumOpacityWithPrecedingAndSubsequentChunk) {
  UpdateWithArtifactWithOpacity(0.0004f, true, true);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest, DontSkipChunkWithAboveMinimumOpacity) {
  UpdateWithArtifactWithOpacity(0.3f, false, false);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest,
       DontSkipChunkWithAboveMinimumOpacityWithPrecedingChunk) {
  UpdateWithArtifactWithOpacity(0.3f, true, false);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest,
       DontSkipChunkWithAboveMinimumOpacitySubsequentChunk) {
  UpdateWithArtifactWithOpacity(0.3f, false, true);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest,
       DontSkipChunkWithAboveMinimumOpacityWithPrecedingAndSubsequentChunk) {
  UpdateWithArtifactWithOpacity(0.3f, true, true);
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest,
       DontSkipChunkWithTinyOpacityAndDirectCompositingReason) {
  auto effect = CreateOpacityEffect(e0(), 0.0001f, CompositingReason::kCanvas);
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest,
       SkipChunkWithTinyOpacityAndVisibleChildEffectNode) {
  auto tiny_effect =
      CreateOpacityEffect(e0(), 0.0001f, CompositingReason::kNone);
  auto visible_effect =
      CreateOpacityEffect(*tiny_effect, 0.5f, CompositingReason::kNone);
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *visible_effect)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());
  ASSERT_EQ(0u, ContentLayerCount());
}

TEST_P(
    PaintArtifactCompositorTest,
    DontSkipChunkWithTinyOpacityAndVisibleChildEffectNodeWithCompositingParent) {
  auto tiny_effect =
      CreateOpacityEffect(e0(), 0.0001f, CompositingReason::kCanvas);
  auto visible_effect = CreateOpacityEffect(*tiny_effect, 0.5f);
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *visible_effect)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest,
       SkipChunkWithTinyOpacityAndVisibleChildEffectNodeWithCompositingChild) {
  auto tiny_effect = CreateOpacityEffect(e0(), 0.0001f);
  auto visible_effect =
      CreateOpacityEffect(*tiny_effect, 0.5f, CompositingReason::kCanvas);
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *visible_effect)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());
  ASSERT_EQ(0u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest, UpdateManagesLayerElementIds) {
  auto transform = CreateSampleTransformNodeWithElementId();
  CompositorElementId element_id = transform->GetCompositorElementId();

  {
    TestPaintArtifact artifact;
    artifact.Chunk(*transform, c0(), e0())
        .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);

    Update(artifact.Build());
    ASSERT_EQ(1u, ContentLayerCount());
    ASSERT_TRUE(GetLayerTreeHost().LayerByElementId(element_id));
  }

  {
    TestPaintArtifact artifact;
    ASSERT_TRUE(GetLayerTreeHost().LayerByElementId(element_id));
    Update(artifact.Build());
    ASSERT_EQ(0u, ContentLayerCount());
    ASSERT_FALSE(GetLayerTreeHost().LayerByElementId(element_id));
  }
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipSimple) {
  // This tests the simplist case that a single layer needs to be clipped
  // by a single composited rounded clip.
  FloatSize corner(5, 5);
  FloatRoundedRect rrect(FloatRect(50, 50, 300, 200), corner, corner, corner,
                         corner);
  auto c1 = CreateClip(c0(), &t0(), rrect,
                       CompositingReason::kWillChangeCompositingHint);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //           l1
  // l0 [ mask_effect_0 ]
  // [ mask_isolation_0 ]
  // [        e0        ]
  // One content layer, one clip mask.
  ASSERT_EQ(2u, RootLayer()->children().size());
  ASSERT_EQ(1u, ContentLayerCount());
  ASSERT_EQ(1u, SynthesizedClipLayerCount());

  const cc::Layer* content0 = RootLayer()->children()[0].get();
  const cc::Layer* clip_mask0 = RootLayer()->children()[1].get();

  constexpr int c0_id = 1;
  constexpr int e0_id = 1;

  EXPECT_EQ(ContentLayerAt(0), content0);
  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree.Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree.Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);

  EXPECT_EQ(SynthesizedClipLayerAt(0), clip_mask0);
  EXPECT_EQ(gfx::Size(300, 200), clip_mask0->bounds());
  EXPECT_EQ(c1_id, clip_mask0->clip_tree_index());
  int mask_effect_0_id = clip_mask0->effect_tree_index();
  const cc::EffectNode& mask_effect_0 =
      *GetPropertyTrees().effect_tree.Node(mask_effect_0_id);
  ASSERT_EQ(mask_isolation_0_id, mask_effect_0.parent_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_0.blend_mode);
}

TEST_P(PaintArtifactCompositorTest,
       SynthesizedClipIndirectlyCompositedClipPath) {
  // This tests the case that a clip node needs to be synthesized due to
  // applying clip path to a composited effect.
  auto c1 = CreateClipPathClip(c0(), &t0(), FloatRoundedRect(50, 50, 300, 200));
  auto e1 = CreateOpacityEffect(e0(), &t0(), c1.get(), 1,
                                CompositingReason::kWillChangeCompositingHint);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, *e1)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //   l0         l1
  // [ e1 ][ mask_effect_0 ]
  // [  mask_isolation_0   ]
  // [         e0          ]
  // One content layer, one clip mask.
  ASSERT_EQ(2u, RootLayer()->children().size());
  ASSERT_EQ(1u, ContentLayerCount());
  ASSERT_EQ(1u, SynthesizedClipLayerCount());

  const cc::Layer* content0 = RootLayer()->children()[0].get();
  const cc::Layer* clip_mask0 = RootLayer()->children()[1].get();

  constexpr int c0_id = 1;
  constexpr int e0_id = 1;

  EXPECT_EQ(ContentLayerAt(0), content0);
  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree.Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int e1_id = content0->effect_tree_index();
  const cc::EffectNode& cc_e1 = *GetPropertyTrees().effect_tree.Node(e1_id);
  EXPECT_EQ(c1_id, cc_e1.clip_id);
  int mask_isolation_0_id = cc_e1.parent_id;
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree.Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(c1_id, mask_isolation_0.clip_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);

  EXPECT_EQ(SynthesizedClipLayerAt(0), clip_mask0);
  EXPECT_EQ(gfx::Size(300, 200), clip_mask0->bounds());
  EXPECT_EQ(c1_id, clip_mask0->clip_tree_index());
  int mask_effect_0_id = clip_mask0->effect_tree_index();
  const cc::EffectNode& mask_effect_0 =
      *GetPropertyTrees().effect_tree.Node(mask_effect_0_id);
  ASSERT_EQ(mask_isolation_0_id, mask_effect_0.parent_id);
  EXPECT_EQ(c1_id, mask_effect_0.clip_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_0.blend_mode);
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipContiguous) {
  // This tests the case that a two back-to-back composited layers having
  // the same composited rounded clip can share the synthesized mask.
  auto t1 = CreateTransform(t0(), TransformationMatrix(), FloatPoint3D(),
                            CompositingReason::kWillChangeCompositingHint);

  FloatSize corner(5, 5);
  FloatRoundedRect rrect(FloatRect(50, 50, 300, 200), corner, corner, corner,
                         corner);
  auto c1 = CreateClip(c0(), &t0(), rrect,
                       CompositingReason::kWillChangeCompositingHint);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(*t1, *c1, e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //              l2
  // l0 l1 [ mask_effect_0 ]
  // [  mask_isolation_0   ]
  // [         e0          ]
  // Two content layers, one clip mask.
  ASSERT_EQ(3u, RootLayer()->children().size());
  ASSERT_EQ(2u, ContentLayerCount());
  ASSERT_EQ(1u, SynthesizedClipLayerCount());

  const cc::Layer* content0 = RootLayer()->children()[0].get();
  const cc::Layer* content1 = RootLayer()->children()[1].get();
  const cc::Layer* clip_mask0 = RootLayer()->children()[2].get();

  constexpr int t0_id = 1;
  constexpr int c0_id = 1;
  constexpr int e0_id = 1;

  EXPECT_EQ(ContentLayerAt(0), content0);
  EXPECT_EQ(t0_id, content0->transform_tree_index());
  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree.Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree.Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);

  EXPECT_EQ(ContentLayerAt(1), content1);
  int t1_id = content1->transform_tree_index();
  const cc::TransformNode& cc_t1 =
      *GetPropertyTrees().transform_tree.Node(t1_id);
  ASSERT_EQ(t0_id, cc_t1.parent_id);
  EXPECT_EQ(c1_id, content1->clip_tree_index());
  EXPECT_EQ(mask_isolation_0_id, content1->effect_tree_index());

  EXPECT_EQ(SynthesizedClipLayerAt(0), clip_mask0);
  EXPECT_EQ(gfx::Size(300, 200), clip_mask0->bounds());
  EXPECT_EQ(t0_id, clip_mask0->transform_tree_index());
  EXPECT_EQ(c1_id, clip_mask0->clip_tree_index());
  int mask_effect_0_id = clip_mask0->effect_tree_index();
  const cc::EffectNode& mask_effect_0 =
      *GetPropertyTrees().effect_tree.Node(mask_effect_0_id);
  ASSERT_EQ(mask_isolation_0_id, mask_effect_0.parent_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_0.blend_mode);
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipDiscontiguous) {
  // This tests the case that a two composited layers having the same
  // composited rounded clip cannot share the synthesized mask if there is
  // another layer in the middle.
  auto t1 = CreateTransform(t0(), TransformationMatrix(), FloatPoint3D(),
                            CompositingReason::kWillChangeCompositingHint);

  FloatSize corner(5, 5);
  FloatRoundedRect rrect(FloatRect(50, 50, 300, 200), corner, corner, corner,
                         corner);
  auto c1 = CreateClip(c0(), &t0(), rrect,
                       CompositingReason::kWillChangeCompositingHint);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(*t1, c0(), e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //           l1                      l4
  // l0 [ mask_effect_0 ]    l3 [ mask_effect_1 ]
  // [ mask_isolation_0 ] l2 [ mask_isolation_1 ]
  // [                    e0                    ]
  // Three content layers, two clip mask.
  ASSERT_EQ(5u, RootLayer()->children().size());
  ASSERT_EQ(3u, ContentLayerCount());
  ASSERT_EQ(2u, SynthesizedClipLayerCount());

  const cc::Layer* content0 = RootLayer()->children()[0].get();
  const cc::Layer* clip_mask0 = RootLayer()->children()[1].get();
  const cc::Layer* content1 = RootLayer()->children()[2].get();
  const cc::Layer* content2 = RootLayer()->children()[3].get();
  const cc::Layer* clip_mask1 = RootLayer()->children()[4].get();

  constexpr int t0_id = 1;
  constexpr int c0_id = 1;
  constexpr int e0_id = 1;

  EXPECT_EQ(ContentLayerAt(0), content0);
  EXPECT_EQ(t0_id, content0->transform_tree_index());
  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree.Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree.Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);

  EXPECT_EQ(SynthesizedClipLayerAt(0), clip_mask0);
  EXPECT_EQ(gfx::Size(300, 200), clip_mask0->bounds());
  EXPECT_EQ(t0_id, clip_mask0->transform_tree_index());
  EXPECT_EQ(c1_id, clip_mask0->clip_tree_index());
  int mask_effect_0_id = clip_mask0->effect_tree_index();
  const cc::EffectNode& mask_effect_0 =
      *GetPropertyTrees().effect_tree.Node(mask_effect_0_id);
  ASSERT_EQ(mask_isolation_0_id, mask_effect_0.parent_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_0.blend_mode);

  EXPECT_EQ(ContentLayerAt(1), content1);
  int t1_id = content1->transform_tree_index();
  const cc::TransformNode& cc_t1 =
      *GetPropertyTrees().transform_tree.Node(t1_id);
  ASSERT_EQ(t0_id, cc_t1.parent_id);
  EXPECT_EQ(c0_id, content1->clip_tree_index());
  EXPECT_EQ(e0_id, content1->effect_tree_index());

  EXPECT_EQ(ContentLayerAt(2), content2);
  EXPECT_EQ(t0_id, content2->transform_tree_index());
  EXPECT_EQ(c1_id, content2->clip_tree_index());
  int mask_isolation_1_id = content2->effect_tree_index();
  const cc::EffectNode& mask_isolation_1 =
      *GetPropertyTrees().effect_tree.Node(mask_isolation_1_id);
  EXPECT_NE(mask_isolation_0_id, mask_isolation_1_id);
  ASSERT_EQ(e0_id, mask_isolation_1.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_1.blend_mode);

  EXPECT_EQ(SynthesizedClipLayerAt(1), clip_mask1);
  EXPECT_EQ(gfx::Size(300, 200), clip_mask1->bounds());
  EXPECT_EQ(t0_id, clip_mask1->transform_tree_index());
  EXPECT_EQ(c1_id, clip_mask1->clip_tree_index());
  int mask_effect_1_id = clip_mask1->effect_tree_index();
  const cc::EffectNode& mask_effect_1 =
      *GetPropertyTrees().effect_tree.Node(mask_effect_1_id);
  ASSERT_EQ(mask_isolation_1_id, mask_effect_1.parent_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_1.blend_mode);
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipAcrossChildEffect) {
  // This tests the case that an effect having the same output clip as the
  // layers before and after it can share the synthesized mask.
  FloatSize corner(5, 5);
  FloatRoundedRect rrect(FloatRect(50, 50, 300, 200), corner, corner, corner,
                         corner);
  auto c1 = CreateClip(c0(), &t0(), rrect,
                       CompositingReason::kWillChangeCompositingHint);
  auto e1 = CreateOpacityEffect(e0(), &t0(), c1.get(), 1,
                                CompositingReason::kWillChangeCompositingHint);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, *e1)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //      l1             l3
  // l0 [ e1 ] l2 [ mask_effect_0 ]
  // [      mask_isolation_0      ]
  // [             e0             ]
  // Three content layers, one clip mask.
  ASSERT_EQ(4u, RootLayer()->children().size());
  ASSERT_EQ(3u, ContentLayerCount());
  ASSERT_EQ(1u, SynthesizedClipLayerCount());

  const cc::Layer* content0 = RootLayer()->children()[0].get();
  const cc::Layer* content1 = RootLayer()->children()[1].get();
  const cc::Layer* content2 = RootLayer()->children()[2].get();
  const cc::Layer* clip_mask0 = RootLayer()->children()[3].get();

  constexpr int c0_id = 1;
  constexpr int e0_id = 1;

  EXPECT_EQ(ContentLayerAt(0), content0);
  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree.Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree.Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);

  EXPECT_EQ(ContentLayerAt(1), content1);
  EXPECT_EQ(c1_id, content1->clip_tree_index());
  int e1_id = content1->effect_tree_index();
  const cc::EffectNode& cc_e1 = *GetPropertyTrees().effect_tree.Node(e1_id);
  ASSERT_EQ(mask_isolation_0_id, cc_e1.parent_id);

  EXPECT_EQ(ContentLayerAt(2), content2);
  EXPECT_EQ(c1_id, content2->clip_tree_index());
  EXPECT_EQ(mask_isolation_0_id, content2->effect_tree_index());

  EXPECT_EQ(SynthesizedClipLayerAt(0), clip_mask0);
  EXPECT_EQ(gfx::Size(300, 200), clip_mask0->bounds());
  EXPECT_EQ(c1_id, clip_mask0->clip_tree_index());
  int mask_effect_0_id = clip_mask0->effect_tree_index();
  const cc::EffectNode& mask_effect_0 =
      *GetPropertyTrees().effect_tree.Node(mask_effect_0_id);
  ASSERT_EQ(mask_isolation_0_id, mask_effect_0.parent_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_0.blend_mode);
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipRespectOutputClip) {
  // This tests the case that a layer cannot share the synthesized mask despite
  // having the same composited rounded clip if it's enclosed by an effect not
  // clipped by the common clip.
  FloatSize corner(5, 5);
  FloatRoundedRect rrect(FloatRect(50, 50, 300, 200), corner, corner, corner,
                         corner);
  auto c1 = CreateClip(c0(), &t0(), rrect,
                       CompositingReason::kWillChangeCompositingHint);

  CompositorFilterOperations non_trivial_filter;
  non_trivial_filter.AppendBlurFilter(5);
  auto e1 = CreateFilterEffect(e0(), non_trivial_filter, FloatPoint(),
                               CompositingReason::kWillChangeCompositingHint);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, *e1)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //                               l3
  //           l1        l2 [ mask_effect_1 ]           l5
  // l0 [ mask_effect_0 ][ mask_isolation_1 ] l4 [ mask_effect_2 ]
  // [ mask_isolation_0 ][        e1        ][ mask_isolation_2  ]
  // [                            e0                             ]
  // Three content layers, three clip mask.
  ASSERT_EQ(6u, RootLayer()->children().size());
  ASSERT_EQ(3u, ContentLayerCount());
  ASSERT_EQ(3u, SynthesizedClipLayerCount());

  const cc::Layer* content0 = RootLayer()->children()[0].get();
  const cc::Layer* clip_mask0 = RootLayer()->children()[1].get();
  const cc::Layer* content1 = RootLayer()->children()[2].get();
  const cc::Layer* clip_mask1 = RootLayer()->children()[3].get();
  const cc::Layer* content2 = RootLayer()->children()[4].get();
  const cc::Layer* clip_mask2 = RootLayer()->children()[5].get();

  constexpr int c0_id = 1;
  constexpr int e0_id = 1;

  EXPECT_EQ(ContentLayerAt(0), content0);
  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree.Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree.Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);

  EXPECT_EQ(SynthesizedClipLayerAt(0), clip_mask0);
  EXPECT_EQ(gfx::Size(300, 200), clip_mask0->bounds());
  EXPECT_EQ(c1_id, clip_mask0->clip_tree_index());
  int mask_effect_0_id = clip_mask0->effect_tree_index();
  const cc::EffectNode& mask_effect_0 =
      *GetPropertyTrees().effect_tree.Node(mask_effect_0_id);
  ASSERT_EQ(mask_isolation_0_id, mask_effect_0.parent_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_0.blend_mode);

  EXPECT_EQ(ContentLayerAt(1), content1);
  EXPECT_EQ(c1_id, content1->clip_tree_index());
  int mask_isolation_1_id = content1->effect_tree_index();
  const cc::EffectNode& mask_isolation_1 =
      *GetPropertyTrees().effect_tree.Node(mask_isolation_1_id);
  EXPECT_NE(mask_isolation_0_id, mask_isolation_1_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_1.blend_mode);
  int e1_id = mask_isolation_1.parent_id;
  const cc::EffectNode& cc_e1 = *GetPropertyTrees().effect_tree.Node(e1_id);
  ASSERT_EQ(e0_id, cc_e1.parent_id);

  EXPECT_EQ(SynthesizedClipLayerAt(1), clip_mask1);
  EXPECT_EQ(gfx::Size(300, 200), clip_mask1->bounds());
  EXPECT_EQ(c1_id, clip_mask1->clip_tree_index());
  int mask_effect_1_id = clip_mask1->effect_tree_index();
  const cc::EffectNode& mask_effect_1 =
      *GetPropertyTrees().effect_tree.Node(mask_effect_1_id);
  ASSERT_EQ(mask_isolation_1_id, mask_effect_1.parent_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_1.blend_mode);

  EXPECT_EQ(ContentLayerAt(2), content2);
  EXPECT_EQ(c1_id, content2->clip_tree_index());
  int mask_isolation_2_id = content2->effect_tree_index();
  const cc::EffectNode& mask_isolation_2 =
      *GetPropertyTrees().effect_tree.Node(mask_isolation_2_id);
  EXPECT_NE(mask_isolation_0_id, mask_isolation_2_id);
  EXPECT_NE(mask_isolation_1_id, mask_isolation_2_id);
  ASSERT_EQ(e0_id, mask_isolation_2.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_2.blend_mode);

  EXPECT_EQ(SynthesizedClipLayerAt(2), clip_mask2);
  EXPECT_EQ(gfx::Size(300, 200), clip_mask2->bounds());
  EXPECT_EQ(c1_id, clip_mask2->clip_tree_index());
  int mask_effect_2_id = clip_mask2->effect_tree_index();
  const cc::EffectNode& mask_effect_2 =
      *GetPropertyTrees().effect_tree.Node(mask_effect_2_id);
  ASSERT_EQ(mask_isolation_2_id, mask_effect_2.parent_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_2.blend_mode);
}

TEST_P(PaintArtifactCompositorTest, SynthesizedClipDelegateBlending) {
  // This tests the case that an effect with exotic blending cannot share
  // the synthesized mask with its siblings because its blending has to be
  // applied by the outermost mask.
  FloatSize corner(5, 5);
  FloatRoundedRect rrect(FloatRect(50, 50, 300, 200), corner, corner, corner,
                         corner);
  auto c1 = CreateClip(c0(), &t0(), rrect,
                       CompositingReason::kWillChangeCompositingHint);

  EffectPaintPropertyNode::State e1_state;
  e1_state.local_transform_space = &t0();
  e1_state.output_clip = c1.get();
  e1_state.blend_mode = SkBlendMode::kMultiply;
  e1_state.direct_compositing_reasons =
      CompositingReason::kWillChangeCompositingHint;
  auto e1 = EffectPaintPropertyNode::Create(e0(), std::move(e1_state));

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, *e1)
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  artifact.Chunk(t0(), *c1, e0())
      .RectDrawing(FloatRect(0, 0, 100, 100), Color::kBlack);
  Update(artifact.Build());

  // Expectation in effect stack diagram:
  //           l1          l2         l3                   l5
  // l0 [ mask_effect_0 ][ e1 ][ mask_effect_1 ] l4 [ mask_effect_2 ]
  // [ mask_isolation_0 ][  mask_isolation_1   ][ mask_isolation_2  ]
  // [                              e0                              ]
  // Three content layers, three clip mask.
  ASSERT_EQ(6u, RootLayer()->children().size());
  ASSERT_EQ(3u, ContentLayerCount());
  ASSERT_EQ(3u, SynthesizedClipLayerCount());

  const cc::Layer* content0 = RootLayer()->children()[0].get();
  const cc::Layer* clip_mask0 = RootLayer()->children()[1].get();
  const cc::Layer* content1 = RootLayer()->children()[2].get();
  const cc::Layer* clip_mask1 = RootLayer()->children()[3].get();
  const cc::Layer* content2 = RootLayer()->children()[4].get();
  const cc::Layer* clip_mask2 = RootLayer()->children()[5].get();

  constexpr int c0_id = 1;
  constexpr int e0_id = 1;

  EXPECT_EQ(ContentLayerAt(0), content0);
  int c1_id = content0->clip_tree_index();
  const cc::ClipNode& cc_c1 = *GetPropertyTrees().clip_tree.Node(c1_id);
  EXPECT_EQ(gfx::RectF(50, 50, 300, 200), cc_c1.clip);
  ASSERT_EQ(c0_id, cc_c1.parent_id);
  int mask_isolation_0_id = content0->effect_tree_index();
  const cc::EffectNode& mask_isolation_0 =
      *GetPropertyTrees().effect_tree.Node(mask_isolation_0_id);
  ASSERT_EQ(e0_id, mask_isolation_0.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);

  EXPECT_EQ(SynthesizedClipLayerAt(0), clip_mask0);
  EXPECT_EQ(gfx::Size(300, 200), clip_mask0->bounds());
  EXPECT_EQ(c1_id, clip_mask0->clip_tree_index());
  int mask_effect_0_id = clip_mask0->effect_tree_index();
  const cc::EffectNode& mask_effect_0 =
      *GetPropertyTrees().effect_tree.Node(mask_effect_0_id);
  ASSERT_EQ(mask_isolation_0_id, mask_effect_0.parent_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_0.blend_mode);

  EXPECT_EQ(ContentLayerAt(1), content1);
  EXPECT_EQ(c1_id, content1->clip_tree_index());
  int e1_id = content1->effect_tree_index();
  const cc::EffectNode& cc_e1 = *GetPropertyTrees().effect_tree.Node(e1_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, cc_e1.blend_mode);
  int mask_isolation_1_id = cc_e1.parent_id;
  const cc::EffectNode& mask_isolation_1 =
      *GetPropertyTrees().effect_tree.Node(mask_isolation_1_id);
  EXPECT_NE(mask_isolation_0_id, mask_isolation_1_id);
  ASSERT_EQ(e0_id, mask_isolation_1.parent_id);
  EXPECT_EQ(SkBlendMode::kMultiply, mask_isolation_1.blend_mode);

  EXPECT_EQ(SynthesizedClipLayerAt(1), clip_mask1);
  EXPECT_EQ(gfx::Size(300, 200), clip_mask1->bounds());
  EXPECT_EQ(c1_id, clip_mask1->clip_tree_index());
  int mask_effect_1_id = clip_mask1->effect_tree_index();
  const cc::EffectNode& mask_effect_1 =
      *GetPropertyTrees().effect_tree.Node(mask_effect_1_id);
  ASSERT_EQ(mask_isolation_1_id, mask_effect_1.parent_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_1.blend_mode);

  EXPECT_EQ(ContentLayerAt(2), content2);
  EXPECT_EQ(c1_id, content2->clip_tree_index());
  int mask_isolation_2_id = content2->effect_tree_index();
  const cc::EffectNode& mask_isolation_2 =
      *GetPropertyTrees().effect_tree.Node(mask_isolation_2_id);
  EXPECT_NE(mask_isolation_0_id, mask_isolation_2_id);
  EXPECT_NE(mask_isolation_1_id, mask_isolation_2_id);
  ASSERT_EQ(e0_id, mask_isolation_2.parent_id);
  EXPECT_EQ(SkBlendMode::kSrcOver, mask_isolation_0.blend_mode);

  EXPECT_EQ(SynthesizedClipLayerAt(2), clip_mask2);
  EXPECT_EQ(gfx::Size(300, 200), clip_mask2->bounds());
  EXPECT_EQ(c1_id, clip_mask2->clip_tree_index());
  int mask_effect_2_id = clip_mask2->effect_tree_index();
  const cc::EffectNode& mask_effect_2 =
      *GetPropertyTrees().effect_tree.Node(mask_effect_2_id);
  ASSERT_EQ(mask_isolation_2_id, mask_effect_2.parent_id);
  EXPECT_EQ(SkBlendMode::kDstIn, mask_effect_2.blend_mode);
}

TEST_P(PaintArtifactCompositorTest, WillBeRemovedFromFrame) {
  auto effect = CreateSampleEffectNodeWithElementId();
  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *effect)
      .RectDrawing(FloatRect(100, 100, 200, 100), Color::kBlack);
  Update(artifact.Build());

  ASSERT_EQ(1u, ContentLayerCount());
  WillBeRemovedFromFrame();
  // We would need a fake or mock LayerTreeHost to validate that we
  // unregister all element ids, so just check layer count for now.
  EXPECT_EQ(0u, ContentLayerCount());
}

TEST_P(PaintArtifactCompositorTest, ContentsNonOpaque) {
  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(FloatRect(100, 100, 200, 200), Color::kBlack);
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());
  EXPECT_FALSE(ContentLayerAt(0)->contents_opaque());
}

TEST_P(PaintArtifactCompositorTest, ContentsOpaque) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .RectDrawing(FloatRect(100, 100, 200, 200), Color::kBlack)
      .KnownToBeOpaque();
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());
  EXPECT_TRUE(ContentLayerAt(0)->contents_opaque());
}

TEST_P(PaintArtifactCompositorTest, ContentsOpaqueSubpixel) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .RectDrawing(FloatRect(100.5, 100.5, 200, 200), Color::kBlack)
      .KnownToBeOpaque();
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());
  EXPECT_EQ(gfx::Size(201, 201), ContentLayerAt(0)->bounds());
  EXPECT_FALSE(ContentLayerAt(0)->contents_opaque());
}

TEST_P(PaintArtifactCompositorTest, ContentsOpaqueUnitedNonOpaque) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .RectDrawing(FloatRect(100, 100, 200, 200), Color::kBlack)
      .KnownToBeOpaque()
      .Chunk()
      .RectDrawing(FloatRect(200, 200, 200, 200), Color::kBlack)
      .KnownToBeOpaque();
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());
  EXPECT_EQ(gfx::Size(300, 300), ContentLayerAt(0)->bounds());
  EXPECT_FALSE(ContentLayerAt(0)->contents_opaque());
}

TEST_P(PaintArtifactCompositorTest, ContentsOpaqueUnitedOpaque1) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .RectDrawing(FloatRect(100, 100, 300, 300), Color::kBlack)
      .KnownToBeOpaque()
      .Chunk()
      .RectDrawing(FloatRect(200, 200, 200, 200), Color::kBlack)
      .KnownToBeOpaque();
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());
  EXPECT_EQ(gfx::Size(300, 300), ContentLayerAt(0)->bounds());
  EXPECT_TRUE(ContentLayerAt(0)->contents_opaque());
}

TEST_P(PaintArtifactCompositorTest, ContentsOpaqueUnitedOpaque2) {
  TestPaintArtifact artifact;
  artifact.Chunk()
      .RectDrawing(FloatRect(100, 100, 200, 200), Color::kBlack)
      .KnownToBeOpaque()
      .Chunk()
      .RectDrawing(FloatRect(100, 100, 300, 300), Color::kBlack)
      .KnownToBeOpaque();
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());
  EXPECT_EQ(gfx::Size(300, 300), ContentLayerAt(0)->bounds());
  // TODO(crbug.com/701991): Upgrade GeometryMapper to make this test pass with
  // the following EXPECT_FALSE changed to EXPECT_TRUE.
  EXPECT_FALSE(ContentLayerAt(0)->contents_opaque());
}

TEST_P(PaintArtifactCompositorTest, DecompositeEffectWithNoOutputClip) {
  // This test verifies effect nodes with no output clip correctly decomposites
  // if there is no compositing reasons.
  auto clip1 = CreateClip(c0(), &t0(), FloatRoundedRect(75, 75, 100, 100));
  auto effect1 = CreateOpacityEffect(e0(), &t0(), nullptr, 0.5);

  TestPaintArtifact artifact;
  artifact.Chunk().RectDrawing(FloatRect(50, 50, 100, 100), Color::kGray);
  artifact.Chunk(t0(), *clip1, *effect1)
      .RectDrawing(FloatRect(100, 100, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());

  const cc::Layer* layer = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50.f, 50.f), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(125, 125), layer->bounds());
  EXPECT_EQ(1, layer->effect_tree_index());
}

TEST_P(PaintArtifactCompositorTest, CompositedEffectWithNoOutputClip) {
  // This test verifies effect nodes with no output clip but has compositing
  // reason correctly squash children chunks and assign clip node.
  auto clip1 = CreateClip(c0(), &t0(), FloatRoundedRect(75, 75, 100, 100));

  auto effect1 =
      CreateOpacityEffect(e0(), &t0(), nullptr, 0.5, CompositingReason::kAll);

  TestPaintArtifact artifact;
  artifact.Chunk(t0(), c0(), *effect1)
      .RectDrawing(FloatRect(50, 50, 100, 100), Color::kGray);
  artifact.Chunk(t0(), *clip1, *effect1)
      .RectDrawing(FloatRect(100, 100, 100, 100), Color::kGray);
  Update(artifact.Build());
  ASSERT_EQ(1u, ContentLayerCount());

  const cc::Layer* layer = ContentLayerAt(0);
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

  // The layer's painting is initially not clipped.
  auto clip = CreateClip(c0(), &t0(), FloatRoundedRect(10, 20, 300, 400));
  TestPaintArtifact artifact1;
  artifact1.Chunk(t0(), *clip, e0())
      .RectDrawing(FloatRect(50, 50, 200, 200), Color::kBlack);
  artifact1.Client(0).Validate();
  artifact1.Client(1).Validate();
  Update(artifact1.Build());
  ASSERT_EQ(1u, ContentLayerCount());

  auto* layer = ContentLayerAt(0);
  EXPECT_EQ(gfx::Vector2dF(50, 50), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(200, 200), layer->bounds());
  EXPECT_THAT(
      layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 200, 200), Color::kBlack)));

  // The layer's painting overflows the left, top, right edges of the clip.
  auto artifact2 = TestPaintArtifact()
                       .Chunk(artifact1.Client(0))
                       .Properties(t0(), *clip, e0())
                       .RectDrawing(artifact1.Client(1),
                                    FloatRect(0, 0, 400, 200), Color::kBlack)
                       .Build();
  // Simluate commit to the compositor thread.
  layer->PushPropertiesTo(
      layer->CreateLayerImpl(host_impl.active_tree()).get());
  Update(artifact2);
  ASSERT_EQ(1u, ContentLayerCount());
  ASSERT_EQ(layer, ContentLayerAt(0));

  // Invalidate the first chunk because its transform in layer changed.
  EXPECT_EQ(gfx::Rect(0, 0, 300, 180), layer->update_rect());
  EXPECT_EQ(gfx::Vector2dF(10, 20), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(300, 180), layer->bounds());
  EXPECT_THAT(
      layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 390, 180), Color::kBlack)));

  // The layer's painting overflows all edges of the clip.
  auto artifact3 =
      TestPaintArtifact()
          .Chunk(artifact1.Client(0))
          .Properties(t0(), *clip, e0())
          .RectDrawing(artifact1.Client(1), FloatRect(-100, -200, 500, 800),
                       Color::kBlack)
          .Build();
  // Simluate commit to the compositor thread.
  layer->PushPropertiesTo(
      layer->CreateLayerImpl(host_impl.active_tree()).get());
  Update(artifact3);
  ASSERT_EQ(1u, ContentLayerCount());
  ASSERT_EQ(layer, ContentLayerAt(0));

  // We should not invalidate the layer because the origin didn't change
  // because of the clip.
  EXPECT_EQ(gfx::Rect(), layer->update_rect());
  EXPECT_EQ(gfx::Vector2dF(10, 20), layer->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(300, 400), layer->bounds());
  EXPECT_THAT(
      layer->GetPicture(),
      Pointee(DrawsRectangle(FloatRect(0, 0, 390, 580), Color::kBlack)));
}

// Test that PaintArtifactCompositor creates the correct nodes for the visual
// viewport's page scale and scroll layers to support pinch-zooming.
TEST_P(PaintArtifactCompositorTest, CreatesViewportNodes) {
  TransformationMatrix matrix;
  matrix.Scale(2);
  TransformPaintPropertyNode::State transform_state{matrix};
  transform_state.compositor_element_id =
      CompositorElementIdFromUniqueObjectId(1);

  auto scale_transform_node = TransformPaintPropertyNode::Create(
      TransformPaintPropertyNode::Root(), std::move(transform_state));

  TestPaintArtifact artifact;
  CompositorElementIdSet element_ids;
  Update(artifact.Build(), element_ids, scale_transform_node.get());

  cc::TransformTree& transform_tree = GetPropertyTrees().transform_tree;
  cc::TransformNode* cc_transform_node = transform_tree.FindNodeFromElementId(
      transform_state.compositor_element_id);
  EXPECT_TRUE(cc_transform_node);
  EXPECT_EQ(TransformationMatrix::ToTransform(matrix),
            cc_transform_node->post_local);
  EXPECT_TRUE(cc_transform_node->local.IsIdentity());
  EXPECT_TRUE(cc_transform_node->pre_local.IsIdentity());
}

}  // namespace blink
