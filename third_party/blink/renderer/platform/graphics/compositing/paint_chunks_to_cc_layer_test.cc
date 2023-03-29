// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"

#include <initializer_list>

#include "cc/layers/layer.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/render_surface_filters.h"
#include "cc/test/paint_op_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {
namespace {

using ::cc::PaintOpEq;
using ::cc::PaintOpIs;
using ::testing::ElementsAre;

class PaintChunksToCcLayerTest : public testing::Test,
                                 public PaintTestConfigurations {};

INSTANTIATE_PAINT_TEST_SUITE_P(PaintChunksToCcLayerTest);

const DisplayItemClient& DefaultClient() {
  DEFINE_STATIC_LOCAL(
      Persistent<FakeDisplayItemClient>, fake_client,
      (MakeGarbageCollected<FakeDisplayItemClient>("FakeDisplayItemClient")));
  return *fake_client;
}

PaintChunk::Id DefaultId() {
  return PaintChunk::Id(DefaultClient().Id(), DisplayItem::kDrawingFirst);
}

void UpdateLayerProperties(cc::Layer& layer,
                           const PropertyTreeState& layer_state,
                           const PaintChunkSubset& chunks) {
  cc::LayerSelection layer_selection;
  PaintChunksToCcLayer::UpdateLayerProperties(
      layer, layer_state, chunks, layer_selection, /*selection_only=*/false);
}

class TestChunks {
 public:
  // Add a paint chunk with a non-empty paint record and given property nodes.
  void AddChunk(
      const TransformPaintPropertyNodeOrAlias& t,
      const ClipPaintPropertyNodeOrAlias& c,
      const EffectPaintPropertyNodeOrAlias& e,
      const gfx::Rect& bounds = gfx::Rect(0, 0, 100, 100),
      const absl::optional<gfx::Rect>& drawable_bounds = absl::nullopt) {
    cc::PaintOpBuffer buffer;
    buffer.push<cc::DrawRectOp>(
        gfx::RectToSkRect(drawable_bounds ? *drawable_bounds : bounds),
        cc::PaintFlags());
    AddChunk(buffer.ReleaseAsRecord(), t, c, e, bounds, drawable_bounds);
  }

  // Add a paint chunk with a given paint record and property nodes.
  void AddChunk(
      PaintRecord record,
      const TransformPaintPropertyNodeOrAlias& t,
      const ClipPaintPropertyNodeOrAlias& c,
      const EffectPaintPropertyNodeOrAlias& e,
      const gfx::Rect& bounds = gfx::Rect(0, 0, 100, 100),
      const absl::optional<gfx::Rect>& drawable_bounds = absl::nullopt) {
    auto& items = paint_artifact_->GetDisplayItemList();
    auto i = items.size();
    items.AllocateAndConstruct<DrawingDisplayItem>(
        DefaultId().client_id, DefaultId().type,
        drawable_bounds ? *drawable_bounds : bounds, std::move(record),
        RasterEffectOutset::kNone);

    auto& chunks = paint_artifact_->PaintChunks();
    chunks.emplace_back(i, i + 1, DefaultClient(), DefaultId(),
                        PropertyTreeStateOrAlias(t, c, e));
    chunks.back().bounds = bounds;
    chunks.back().drawable_bounds = drawable_bounds ? *drawable_bounds : bounds;
  }

  void AddEmptyChunk(const TransformPaintPropertyNode& t,
                     const ClipPaintPropertyNode& c,
                     const EffectPaintPropertyNode& e,
                     const gfx::Rect& bounds = gfx::Rect(0, 0, 100, 100)) {
    auto& chunks = paint_artifact_->PaintChunks();
    auto i = paint_artifact_->GetDisplayItemList().size();
    chunks.emplace_back(i, i, DefaultClient(), DefaultId(),
                        PropertyTreeState(t, c, e));
    chunks.back().bounds = bounds;
  }

  Vector<PaintChunk>* GetChunks() { return &paint_artifact_->PaintChunks(); }

  PaintChunkSubset Build() {
    return PaintChunkSubset(std::move(paint_artifact_));
  }

 private:
  scoped_refptr<PaintArtifact> paint_artifact_ =
      base::MakeRefCounted<PaintArtifact>();
};

TEST_P(PaintChunksToCcLayerTest, EffectGroupingSimple) {
  // This test verifies effects are applied as a group.
  auto e1 = CreateOpacityEffect(e0(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), *e1, gfx::Rect(0, 0, 50, 50));
  chunks.AddChunk(t0(), c0(), *e1, gfx::Rect(20, 20, 70, 70));

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());
  EXPECT_THAT(output, ElementsAre(PaintOpEq<cc::SaveLayerAlphaOp>(
                                      SkRect::MakeXYWH(0, 0, 90, 90),
                                      0.5f),                      // <e1>
                                  PaintOpIs<cc::DrawRecordOp>(),  // <p0/>
                                  PaintOpIs<cc::DrawRecordOp>(),  // <p1/>
                                  PaintOpIs<cc::RestoreOp>()));   // </e1>
}

TEST_P(PaintChunksToCcLayerTest, EffectGroupingNested) {
  // This test verifies nested effects are grouped properly.
  auto e1 = CreateOpacityEffect(e0(), 0.5f);
  auto e2 = CreateOpacityEffect(*e1, 0.5f);
  auto e3 = CreateOpacityEffect(*e1, 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), *e2);
  chunks.AddChunk(t0(), c0(), *e3, gfx::Rect(111, 222, 333, 444));

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());
  EXPECT_THAT(
      output,
      ElementsAre(
          PaintOpEq<cc::SaveLayerAlphaOp>(SkRect::MakeXYWH(0, 0, 444, 666),
                                          0.5f),  // <e1>
          PaintOpEq<cc::SaveLayerAlphaOp>(SkRect::MakeXYWH(0, 0, 100, 100),
                                          0.5f),  // <e2>
          PaintOpIs<cc::DrawRecordOp>(),          // <p0/>
          PaintOpIs<cc::RestoreOp>(),             // </e2>
          PaintOpEq<cc::SaveLayerAlphaOp>(SkRect::MakeXYWH(111, 222, 333, 444),
                                          0.5f),  // <e3>
          PaintOpIs<cc::DrawRecordOp>(),          // <p1/>
          PaintOpIs<cc::RestoreOp>(),             // </e3>
          PaintOpIs<cc::RestoreOp>()));           // </e1>
}

TEST_P(PaintChunksToCcLayerTest, EffectFilterGroupingNestedWithTransforms) {
  // This test verifies nested effects with transforms are grouped properly.
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto t2 = Create2DTranslation(*t1, -50, -50);
  auto e1 = CreateOpacityEffect(e0(), *t2, &c0(), 0.5);

  CompositorFilterOperations filter;
  filter.AppendBlurFilter(5);
  auto e2 = CreateFilterEffect(*e1, filter);
  TestChunks chunks;
  chunks.AddChunk(*t2, c0(), *e1, gfx::Rect(0, 0, 50, 50));
  chunks.AddChunk(*t1, c0(), *e2, gfx::Rect(20, 20, 70, 70));

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  cc::PaintFlags expected_flags;
  expected_flags.setImageFilter(cc::RenderSurfaceFilters::BuildImageFilter(
      filter.AsCcFilterOperations(), gfx::SizeF()));
  EXPECT_THAT(
      output,
      ElementsAre(
          PaintOpIs<cc::SaveOp>(),
          PaintOpEq<cc::ConcatOp>(
              gfx::TransformToSkM44(t1->Matrix() * t2->Matrix())),  // <t1*t2>
          // chunk1.bounds + e2(t2^-1(chunk2.bounds))
          PaintOpEq<cc::SaveLayerAlphaOp>(SkRect::MakeXYWH(0, 0, 155, 155),
                                          0.5f),  // <e1>
          PaintOpIs<cc::DrawRecordOp>(),          // <p1/>
          // t2^-1(chunk2.bounds)
          PaintOpEq<cc::SaveLayerOp>(SkRect::MakeXYWH(70, 70, 70, 70),
                                     expected_flags),  // <e2>
          PaintOpIs<cc::SaveOp>(),
          // t2^1
          PaintOpEq<cc::TranslateOp>(-t2->Get2dTranslation().x(),
                                     -t2->Get2dTranslation().y()),  // <t2^-1>
          PaintOpIs<cc::DrawRecordOp>(),                            // <p2/>
          PaintOpIs<cc::RestoreOp>(),                               // </t2^-1>
          PaintOpIs<cc::RestoreOp>(),                               // </e2>
          PaintOpIs<cc::RestoreOp>(),                               // </e1>
          PaintOpIs<cc::RestoreOp>()));                             // </t1*t2>
}

TEST_P(PaintChunksToCcLayerTest, InterleavedClipEffect) {
  // This test verifies effects are enclosed by their output clips.
  // It is the same as the example made in the class comments of
  // ConversionContext.
  // Refer to PaintChunksToCcLayer.cpp for detailed explanation.
  // (Search "State management example".)
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto c2 = CreateClip(*c1, t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto c3 = CreateClip(*c2, t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto c4 = CreateClip(*c3, t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto e1 = CreateOpacityEffect(e0(), t0(), c2.get(), 0.5);
  auto e2 = CreateOpacityEffect(*e1, t0(), c4.get(), 0.5);
  TestChunks chunks;
  chunks.AddChunk(t0(), *c2, e0());
  chunks.AddChunk(t0(), *c3, e0());
  chunks.AddChunk(t0(), *c4, *e2, gfx::Rect(0, 0, 50, 50));
  chunks.AddChunk(t0(), *c3, *e1, gfx::Rect(20, 20, 70, 70));
  chunks.AddChunk(t0(), *c4, e0());

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());
  EXPECT_THAT(
      output,
      ElementsAre(
          PaintOpIs<cc::SaveOp>(),
          PaintOpIs<cc::ClipRectOp>(),    // <c1+c2>
          PaintOpIs<cc::DrawRecordOp>(),  // <p0/>
          PaintOpIs<cc::SaveOp>(),
          PaintOpIs<cc::ClipRectOp>(),    // <c3>
          PaintOpIs<cc::DrawRecordOp>(),  // <p1/>
          PaintOpIs<cc::RestoreOp>(),     // </c3>
          PaintOpEq<cc::SaveLayerAlphaOp>(SkRect::MakeXYWH(0, 0, 90, 90),
                                          0.5f),  // <e1>
          PaintOpIs<cc::SaveOp>(),
          PaintOpIs<cc::ClipRectOp>(),  // <c3+c4>
          PaintOpEq<cc::SaveLayerAlphaOp>(SkRect::MakeXYWH(0, 0, 50, 50),
                                          0.5f),  // <e2>
          PaintOpIs<cc::DrawRecordOp>(),          // <p2/>
          PaintOpIs<cc::RestoreOp>(),             // </e2>
          PaintOpIs<cc::RestoreOp>(),             // </c3+c4>
          PaintOpIs<cc::SaveOp>(),
          PaintOpIs<cc::ClipRectOp>(),    // <c3>
          PaintOpIs<cc::DrawRecordOp>(),  // <p3/>
          PaintOpIs<cc::RestoreOp>(),     // </c3>
          PaintOpIs<cc::RestoreOp>(),     // </e1>
          PaintOpIs<cc::SaveOp>(),
          PaintOpIs<cc::ClipRectOp>(),    // <c3+c4>
          PaintOpIs<cc::DrawRecordOp>(),  // <p4/>
          PaintOpIs<cc::RestoreOp>(),     // </c3+c4>
          PaintOpIs<cc::RestoreOp>()));   // </c1+c2>
}

TEST_P(PaintChunksToCcLayerTest, ClipSpaceInversion) {
  // This test verifies chunks that have a shallower transform state than
  // its clip can still be painted. The infamous CSS corner case:
  // <div style="position:absolute; clip:rect(...)">
  //     <div style="position:fixed;">Clipped but not scroll along.</div>
  // </div>
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto c1 = CreateClip(c0(), *t1, FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  TestChunks chunks;
  chunks.AddChunk(t0(), *c1, e0());

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());
  EXPECT_THAT(output, ElementsAre(PaintOpIs<cc::SaveOp>(),
                                  PaintOpIs<cc::ConcatOp>(),    // <t1
                                  PaintOpIs<cc::ClipRectOp>(),  //  c1>
                                  PaintOpIs<cc::SaveOp>(),
                                  PaintOpIs<cc::ConcatOp>(),      // <t1^-1>
                                  PaintOpIs<cc::DrawRecordOp>(),  // <p0/>
                                  PaintOpIs<cc::RestoreOp>(),     // </t1^-1>
                                  PaintOpIs<cc::RestoreOp>()));   // </c1 t1>
}

TEST_P(PaintChunksToCcLayerTest, OpacityEffectSpaceInversion) {
  // This test verifies chunks that have a shallower transform state than
  // its effect can still be painted. The infamous CSS corner case:
  // <div style="overflow:scroll">
  //   <div style="opacity:0.5">
  //     <div style="position:absolute;">Transparent but not scroll along.</div>
  //   </div>
  // </div>
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto e1 = CreateOpacityEffect(e0(), *t1, &c0(), 0.5);
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), *e1);
  chunks.AddChunk(*t1, c0(), *e1);

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());
  EXPECT_THAT(
      output,
      ElementsAre(
          PaintOpIs<cc::SaveOp>(),
          PaintOpEq<cc::ConcatOp>(gfx::TransformToSkM44(t1->Matrix())),  // <t1>
          PaintOpEq<cc::SaveLayerAlphaOp>(SkRect::MakeXYWH(0, 0, 100, 100),
                                          0.5f),  // <e1>
          PaintOpIs<cc::SaveOp>(),
          PaintOpEq<cc::ConcatOp>(gfx::TransformToSkM44(
              t1->Matrix().GetCheckedInverse())),  // <t1^-1>
          PaintOpIs<cc::DrawRecordOp>(),           // <p0/>
          PaintOpIs<cc::RestoreOp>(),              // </t1^-1>
          PaintOpIs<cc::DrawRecordOp>(),           // <p1/>
          PaintOpIs<cc::RestoreOp>(),              // </e1>
          PaintOpIs<cc::RestoreOp>()));            // </t1>
}

TEST_P(PaintChunksToCcLayerTest, FilterEffectSpaceInversion) {
  // This test verifies chunks that have a shallower transform state than
  // its effect can still be painted. The infamous CSS corner case:
  // <div style="overflow:scroll">
  //   <div style="filter:blur(1px)">
  //     <div style="position:absolute;">Filtered but not scroll along.</div>
  //   </div>
  // </div>
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(5);
  auto e1 = CreateFilterEffect(e0(), *t1, &c0(), filter);
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), *e1);

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  cc::PaintFlags expected_flags;
  expected_flags.setImageFilter(cc::RenderSurfaceFilters::BuildImageFilter(
      filter.AsCcFilterOperations(), gfx::SizeF()));
  EXPECT_THAT(
      output,
      ElementsAre(
          PaintOpIs<cc::SaveOp>(),
          PaintOpEq<cc::ConcatOp>(gfx::TransformToSkM44(t1->Matrix())),  // <t1>
          PaintOpEq<cc::SaveLayerOp>(SkRect::MakeXYWH(0, 0, 50, 50),
                                     expected_flags),  // <e1>
          PaintOpIs<cc::SaveOp>(),
          PaintOpEq<cc::ConcatOp>(gfx::TransformToSkM44(
              t1->Matrix().GetCheckedInverse())),  // <t1^-1>
          PaintOpIs<cc::DrawRecordOp>(),           // <p0/>
          PaintOpIs<cc::RestoreOp>(),              // </t1^-1>
          PaintOpIs<cc::RestoreOp>(),              // </e1>
          PaintOpIs<cc::RestoreOp>()));            // </t1>
}

TEST_P(PaintChunksToCcLayerTest, NonRootLayerSimple) {
  // This test verifies a layer with composited property state does not
  // apply properties again internally.
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto e1 = CreateOpacityEffect(e0(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(*t1, *c1, *e1);

  PaintRecord output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState(*t1, *c1, *e1));
  EXPECT_THAT(output, ElementsAre(PaintOpIs<cc::DrawRecordOp>()));
}

TEST_P(PaintChunksToCcLayerTest, NonRootLayerTransformEscape) {
  // This test verifies chunks that have a shallower transform state than the
  // layer can still be painted.
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto e1 = CreateOpacityEffect(e0(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t0(), *c1, *e1);

  PaintRecord output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState(*t1, *c1, *e1));
  EXPECT_THAT(output, ElementsAre(PaintOpIs<cc::SaveOp>(),
                                  PaintOpIs<cc::ConcatOp>(),      // <t1^-1>
                                  PaintOpIs<cc::DrawRecordOp>(),  // <p0/>
                                  PaintOpIs<cc::RestoreOp>()));   // </t1^-1>
}

TEST_P(PaintChunksToCcLayerTest, EffectWithNoOutputClip) {
  // This test verifies effect with no output clip can be correctly processed.
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto c2 = CreateClip(*c1, t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto e1 = CreateOpacityEffect(e0(), t0(), nullptr, 0.5);

  TestChunks chunks;
  chunks.AddChunk(t0(), *c2, *e1);

  PaintRecord output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState(t0(), *c1, e0()));
  EXPECT_THAT(output, ElementsAre(PaintOpEq<cc::SaveLayerAlphaOp>(
                                      SkRect::MakeXYWH(0, 0, 100, 100),
                                      0.5f),  // <e1>
                                  PaintOpIs<cc::SaveOp>(),
                                  PaintOpIs<cc::ClipRectOp>(),    // <c2>
                                  PaintOpIs<cc::DrawRecordOp>(),  // <p0/>
                                  PaintOpIs<cc::RestoreOp>(),     // </c2>
                                  PaintOpIs<cc::RestoreOp>()));   // </e1>
}

TEST_P(PaintChunksToCcLayerTest,
       EffectWithNoOutputClipNestedInDecompositedEffect) {
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto e1 = CreateOpacityEffect(e0(), 0.5);
  auto e2 = CreateOpacityEffect(*e1, t0(), nullptr, 0.5);

  TestChunks chunks;
  chunks.AddChunk(t0(), *c1, *e2);

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());
  EXPECT_THAT(
      output,
      ElementsAre(
          PaintOpEq<cc::SaveLayerAlphaOp>(SkRect::MakeXYWH(0, 0, 100, 100),
                                          0.5f),  // <e1>
          PaintOpEq<cc::SaveLayerAlphaOp>(SkRect::MakeXYWH(0, 0, 100, 100),
                                          0.5f),  // <e2>
          PaintOpIs<cc::SaveOp>(),
          PaintOpIs<cc::ClipRectOp>(),    // <c1>
          PaintOpIs<cc::DrawRecordOp>(),  // <p0/>
          PaintOpIs<cc::RestoreOp>(),     // </c1>
          PaintOpIs<cc::RestoreOp>(),     // </e2>
          PaintOpIs<cc::RestoreOp>()));   // </e1>
}

TEST_P(PaintChunksToCcLayerTest,
       EffectWithNoOutputClipNestedInCompositedEffect) {
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto e1 = CreateOpacityEffect(e0(), 0.5);
  auto e2 = CreateOpacityEffect(*e1, t0(), nullptr, 0.5);

  TestChunks chunks;
  chunks.AddChunk(t0(), *c1, *e2);

  PaintRecord output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState(t0(), c0(), *e1));
  EXPECT_THAT(output, ElementsAre(PaintOpEq<cc::SaveLayerAlphaOp>(
                                      SkRect::MakeXYWH(0, 0, 100, 100),
                                      0.5f),  // <e2>
                                  PaintOpIs<cc::SaveOp>(),
                                  PaintOpIs<cc::ClipRectOp>(),    // <c1>
                                  PaintOpIs<cc::DrawRecordOp>(),  // <p0/>
                                  PaintOpIs<cc::RestoreOp>(),     // </c1>
                                  PaintOpIs<cc::RestoreOp>()));   // </e2>
}

TEST_P(PaintChunksToCcLayerTest,
       EffectWithNoOutputClipNestedInCompositedEffectAndClip) {
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto e1 = CreateOpacityEffect(e0(), 0.5);
  auto e2 = CreateOpacityEffect(*e1, t0(), nullptr, 0.5);

  TestChunks chunks;
  chunks.AddChunk(t0(), *c1, *e2);

  PaintRecord output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState(t0(), *c1, *e1));
  EXPECT_THAT(output, ElementsAre(PaintOpEq<cc::SaveLayerAlphaOp>(
                                      SkRect::MakeXYWH(0, 0, 100, 100),
                                      0.5f),                      // <e2>
                                  PaintOpIs<cc::DrawRecordOp>(),  // <p0/>
                                  PaintOpIs<cc::RestoreOp>()));   // </e2>
}

TEST_P(PaintChunksToCcLayerTest, VisualRect) {
  auto layer_transform = CreateTransform(t0(), MakeScaleMatrix(20));
  auto chunk_transform = Create2DTranslation(*layer_transform, 50, 100);

  TestChunks chunks;
  chunks.AddChunk(*chunk_transform, c0(), e0());

  auto cc_list = base::MakeRefCounted<cc::DisplayItemList>();
  PaintChunksToCcLayer::ConvertInto(
      chunks.Build(), PropertyTreeState(*layer_transform, c0(), e0()),
      gfx::Vector2dF(100, 200), nullptr, *cc_list);
  EXPECT_EQ(gfx::Rect(-50, -100, 100, 100), cc_list->VisualRectForTesting(4));

  EXPECT_THAT(cc_list->FinalizeAndReleaseAsRecord(),
              ElementsAre(PaintOpIs<cc::SaveOp>(),        //
                          PaintOpIs<cc::TranslateOp>(),   // <layer_offset>
                          PaintOpIs<cc::SaveOp>(),        //
                          PaintOpIs<cc::TranslateOp>(),   // <layer_transform>
                          PaintOpIs<cc::DrawRecordOp>(),  // <p0/>
                          PaintOpIs<cc::RestoreOp>(),     // </layer_transform>
                          PaintOpIs<cc::RestoreOp>()));   // </layer_offset>
}

TEST_P(PaintChunksToCcLayerTest, NoncompositedClipPath) {
  auto c1 = CreateClipPathClip(c0(), t0(), FloatRoundedRect(1, 2, 3, 4));
  TestChunks chunks;
  chunks.AddChunk(t0(), *c1, e0());

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(output,
              ElementsAre(PaintOpIs<cc::SaveOp>(),        //
                          PaintOpIs<cc::ClipRectOp>(),    //
                          PaintOpIs<cc::ClipPathOp>(),    // <clip_path>
                          PaintOpIs<cc::DrawRecordOp>(),  // <p0/>
                          PaintOpIs<cc::RestoreOp>()));   // </clip_path>
}

TEST_P(PaintChunksToCcLayerTest, EmptyClipsAreElided) {
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto c1c2 = CreateClip(*c1, t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto c2 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));

  TestChunks chunks;
  chunks.AddChunk(PaintRecord(), t0(), *c1, e0());
  chunks.AddChunk(PaintRecord(), t0(), *c1c2, e0());
  chunks.AddChunk(PaintRecord(), t0(), *c1c2, e0());
  chunks.AddChunk(PaintRecord(), t0(), *c1c2, e0());
  chunks.AddChunk(PaintRecord(), t0(), *c1, e0());
  // D1
  chunks.AddChunk(t0(), *c2, e0());

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  // Note that c1 and c1c2 are elided.
  EXPECT_THAT(output, ElementsAre(PaintOpIs<cc::SaveOp>(),        //
                                  PaintOpIs<cc::ClipRectOp>(),    // <c2>
                                  PaintOpIs<cc::DrawRecordOp>(),  // D1
                                  PaintOpIs<cc::RestoreOp>()      // </c2>
                                  ));
}

TEST_P(PaintChunksToCcLayerTest, NonEmptyClipsAreStored) {
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto c1c2 = CreateClip(*c1, t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto c2 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));

  TestChunks chunks;
  chunks.AddChunk(PaintRecord(), t0(), *c1, e0());
  chunks.AddChunk(PaintRecord(), t0(), *c1c2, e0());
  chunks.AddChunk(PaintRecord(), t0(), *c1c2, e0());
  // D1
  chunks.AddChunk(t0(), *c1c2, e0());
  chunks.AddChunk(PaintRecord(), t0(), *c1, e0());
  // D2
  chunks.AddChunk(t0(), *c2, e0());

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(output, ElementsAre(PaintOpIs<cc::SaveOp>(),
                                  PaintOpIs<cc::ClipRectOp>(),    // <c1+c2>
                                  PaintOpIs<cc::DrawRecordOp>(),  // D1
                                  PaintOpIs<cc::RestoreOp>(),     // </c1+c2>
                                  PaintOpIs<cc::SaveOp>(),
                                  PaintOpIs<cc::ClipRectOp>(),    // <c2>
                                  PaintOpIs<cc::DrawRecordOp>(),  // D2
                                  PaintOpIs<cc::RestoreOp>()      // </c2>
                                  ));
}

TEST_P(PaintChunksToCcLayerTest, EmptyEffectsAreStored) {
  auto e1 = CreateOpacityEffect(e0(), 0.5);

  TestChunks chunks;
  chunks.AddChunk(PaintRecord(), t0(), c0(), e0());
  chunks.AddChunk(PaintRecord(), t0(), c0(), *e1);

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(output, ElementsAre(PaintOpEq<cc::SaveLayerAlphaOp>(
                                      SkRect::MakeXYWH(0, 0, 100, 100),
                                      0.5f),                  // <e1>
                                  PaintOpIs<cc::RestoreOp>()  // </e1>
                                  ));
}

TEST_P(PaintChunksToCcLayerTest, CombineClips) {
  FloatRoundedRect clip_rect(0, 0, 100, 100);
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto c1 = CreateClip(c0(), t0(), clip_rect);
  auto c2 = CreateClip(*c1, t0(), clip_rect);
  auto c3 = CreateClip(*c2, *t1, clip_rect);
  auto c4 = CreateClip(*c3, *t1, clip_rect);
  auto c5 = CreateClipPathClip(*c4, *t1, clip_rect);
  auto c6 = CreateClip(*c5, *t1, clip_rect);

  TestChunks chunks;
  chunks.AddChunk(*t1, *c6, e0());
  chunks.AddChunk(*t1, *c3, e0());

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(
      output,
      ElementsAre(PaintOpIs<cc::SaveOp>(),
                  PaintOpIs<cc::ClipRectOp>(),  // <c1+c2>
                  PaintOpIs<cc::SaveOp>(), PaintOpIs<cc::ConcatOp>(),  // <t1
                  PaintOpIs<cc::ClipRectOp>(),
                  PaintOpIs<cc::ClipPathOp>(),  //  c3+c4+c5>
                  PaintOpIs<cc::SaveOp>(), PaintOpIs<cc::ClipRectOp>(),  // <c6>
                  PaintOpIs<cc::DrawRecordOp>(),  // <p0/>
                  PaintOpIs<cc::RestoreOp>(),     // </c6>
                  PaintOpIs<cc::RestoreOp>(),     // </c3+c4+c5 t1>
                  PaintOpIs<cc::SaveOp>(), PaintOpIs<cc::ConcatOp>(),  // <t1
                  PaintOpIs<cc::ClipRectOp>(),                         // c3>
                  PaintOpIs<cc::DrawRecordOp>(),                       // <p1/>
                  PaintOpIs<cc::RestoreOp>(),    // </c3 t1>
                  PaintOpIs<cc::RestoreOp>()));  // </c1+c2>
}

TEST_P(PaintChunksToCcLayerTest, CombineClipsAcrossTransform) {
  FloatRoundedRect clip_rect(0, 0, 100, 100);
  auto identity = Create2DTranslation(t0(), 0, 0);
  auto non_identity = CreateTransform(*identity, MakeScaleMatrix(2));
  auto non_invertible = CreateTransform(*non_identity, MakeScaleMatrix(0));
  EXPECT_FALSE(non_invertible->Matrix().IsInvertible());
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0, 0, 100, 100));
  auto c2 = CreateClip(*c1, *identity, FloatRoundedRect(50, 50, 100, 100));
  auto c3 = CreateClip(*c2, *non_identity, FloatRoundedRect(1, 2, 3, 4));
  auto c4 = CreateClip(*c3, *non_invertible, FloatRoundedRect(5, 6, 7, 8));

  TestChunks chunks;
  chunks.AddChunk(*non_invertible, *c4, e0());

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  // We combine c1/c2 across |identity|, but not c2/c3 across |non_identity|
  // and c3/c4 across |non_invertible|.
  EXPECT_THAT(
      output,
      ElementsAre(PaintOpIs<cc::SaveOp>(),
                  PaintOpEq<cc::ClipRectOp>(SkRect::MakeXYWH(50, 50, 50, 50),
                                            SkClipOp::kIntersect,
                                            /*antialias=*/true),  // <c1+c2>
                  PaintOpIs<cc::SaveOp>(),
                  PaintOpEq<cc::ConcatOp>(gfx::TransformToSkM44(
                      non_identity->Matrix())),  // <non_identity
                  PaintOpEq<cc::ClipRectOp>(SkRect::MakeXYWH(1, 2, 3, 4),
                                            SkClipOp::kIntersect,
                                            /*antialias=*/true),  //  c3>
                  PaintOpIs<cc::SaveOp>(),
                  PaintOpEq<cc::ConcatOp>(gfx::TransformToSkM44(
                      non_invertible->Matrix())),  // <non_invertible
                  PaintOpEq<cc::ClipRectOp>(SkRect::MakeXYWH(5, 6, 7, 8),
                                            SkClipOp::kIntersect,
                                            /*antialias=*/true),  //  c4>
                  PaintOpIs<cc::DrawRecordOp>(),                  // <p0/>
                  PaintOpIs<cc::RestoreOp>(),  // </c4 non_invertible>
                  PaintOpIs<cc::RestoreOp>(),  // </c3 non_identity>
                  PaintOpIs<cc::RestoreOp>()   // </c1+c2>
                  ));
}

TEST_P(PaintChunksToCcLayerTest, CombineClipsWithRoundedRects) {
  FloatRoundedRect rect(0, 0, 100, 100);
  FloatRoundedRect big_rounded_rect(gfx::RectF(0, 0, 200, 200), 5);
  FloatRoundedRect small_rounded_rect(gfx::RectF(0, 0, 100, 100), 5);

  auto c1 = CreateClip(c0(), t0(), rect);
  auto c2 = CreateClip(*c1, t0(), small_rounded_rect);
  auto c3 = CreateClip(*c2, t0(), rect);
  auto c4 = CreateClip(*c3, t0(), big_rounded_rect);
  auto c5 = CreateClip(*c4, t0(), rect);
  auto c6 = CreateClip(*c5, t0(), big_rounded_rect);
  auto c7 = CreateClip(*c6, t0(), small_rounded_rect);

  TestChunks chunks;
  chunks.AddChunk(t0(), *c7, e0());

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(
      output,
      ElementsAre(PaintOpIs<cc::SaveOp>(),
                  PaintOpEq<cc::ClipRRectOp>(SkRRect(small_rounded_rect),
                                             SkClipOp::kIntersect,
                                             /*antialias=*/true),  // <c1+c2+c3>
                  PaintOpIs<cc::SaveOp>(),
                  PaintOpEq<cc::ClipRRectOp>(SkRRect(big_rounded_rect),
                                             SkClipOp::kIntersect,
                                             /*antialias=*/true),  // <c4>
                  PaintOpIs<cc::SaveOp>(),
                  PaintOpEq<cc::ClipRectOp>(gfx::RectFToSkRect(rect.Rect()),
                                            SkClipOp::kIntersect,
                                            /*antialias=*/true),  // <c5>
                  PaintOpIs<cc::SaveOp>(),
                  PaintOpEq<cc::ClipRRectOp>(SkRRect(big_rounded_rect),
                                             SkClipOp::kIntersect,
                                             /*antialias=*/true),  // <c6>
                  PaintOpIs<cc::SaveOp>(),
                  PaintOpEq<cc::ClipRRectOp>(SkRRect(small_rounded_rect),
                                             SkClipOp::kIntersect,
                                             /*antialias=*/true),  // <c7>
                  PaintOpIs<cc::DrawRecordOp>(),                   // <p0/>
                  PaintOpIs<cc::RestoreOp>(),                      // </c7>
                  PaintOpIs<cc::RestoreOp>(),                      // </c6>
                  PaintOpIs<cc::RestoreOp>(),                      // </c5>
                  PaintOpIs<cc::RestoreOp>(),                      // </c4>
                  PaintOpIs<cc::RestoreOp>()));  // </c1+c2+c3>
}

TEST_P(PaintChunksToCcLayerTest, ChunksSamePropertyTreeState) {
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto t2 = CreateTransform(*t1, MakeScaleMatrix(3));
  auto c1 = CreateClip(c0(), *t1, FloatRoundedRect(0, 0, 100, 100));

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*t1, c0(), e0());
  chunks.AddChunk(*t1, c0(), e0());
  chunks.AddChunk(*t1, *c1, e0());
  chunks.AddChunk(*t1, *c1, e0());
  chunks.AddChunk(*t2, *c1, e0());
  chunks.AddChunk(*t2, *c1, e0());

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(
      output,
      ElementsAre(PaintOpIs<cc::DrawRecordOp>(),                       // <p0/>
                  PaintOpIs<cc::SaveOp>(), PaintOpIs<cc::ConcatOp>(),  // <t1>
                  PaintOpIs<cc::DrawRecordOp>(),                       // <p1/>
                  PaintOpIs<cc::DrawRecordOp>(),                       // <p2/>
                  PaintOpIs<cc::SaveOp>(), PaintOpIs<cc::ClipRectOp>(),  // <c1>
                  PaintOpIs<cc::DrawRecordOp>(),                       // <p3/>
                  PaintOpIs<cc::DrawRecordOp>(),                       // <p4/>
                  PaintOpIs<cc::SaveOp>(), PaintOpIs<cc::ConcatOp>(),  // <t2>
                  PaintOpIs<cc::DrawRecordOp>(),                       // <p5/>
                  PaintOpIs<cc::DrawRecordOp>(),                       // <p6/>
                  PaintOpIs<cc::RestoreOp>(),                          // </t2>
                  PaintOpIs<cc::RestoreOp>(),                          // </c1>
                  PaintOpIs<cc::RestoreOp>()));                        // </t1>
}

TEST_P(PaintChunksToCcLayerTest, NoOpForIdentityTransforms) {
  auto t1 = Create2DTranslation(t0(), 0, 0);
  auto t2 = Create2DTranslation(*t1, 0, 0);
  auto t3 = Create2DTranslation(*t2, 0, 0);
  auto c1 = CreateClip(c0(), *t2, FloatRoundedRect(0, 0, 100, 100));
  auto c2 = CreateClip(*c1, *t3, FloatRoundedRect(0, 0, 200, 50));

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*t1, c0(), e0());
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*t1, c0(), e0());
  chunks.AddChunk(*t2, c0(), e0());
  chunks.AddChunk(*t1, c0(), e0());
  chunks.AddChunk(*t1, *c2, e0());

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(output,
              ElementsAre(PaintOpIs<cc::DrawRecordOp>(),  // <p0/>
                          PaintOpIs<cc::DrawRecordOp>(),  // <p1/>
                          PaintOpIs<cc::DrawRecordOp>(),  // <p2/>
                          PaintOpIs<cc::DrawRecordOp>(),  // <p3/>
                          PaintOpIs<cc::DrawRecordOp>(),  // <p4/>
                          PaintOpIs<cc::DrawRecordOp>(),  // <p5/>
                          PaintOpIs<cc::SaveOp>(),
                          PaintOpIs<cc::ClipRectOp>(),    // <c1+c2>
                          PaintOpIs<cc::DrawRecordOp>(),  // <p6/>
                          PaintOpIs<cc::RestoreOp>()));   // </c1+c2>
}

TEST_P(PaintChunksToCcLayerTest, EffectsWithSameTransform) {
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto e1 = CreateOpacityEffect(e0(), *t1, &c0(), 0.1f);
  auto e2 = CreateOpacityEffect(e0(), *t1, &c0(), 0.2f);

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*t1, c0(), *e1);
  chunks.AddChunk(*t1, c0(), *e2);

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(
      output,
      ElementsAre(PaintOpIs<cc::DrawRecordOp>(),                       // <p0/>
                  PaintOpIs<cc::SaveOp>(), PaintOpIs<cc::ConcatOp>(),  // <t1>
                  PaintOpIs<cc::SaveLayerAlphaOp>(),                   // <e1>
                  PaintOpIs<cc::DrawRecordOp>(),                       // <p1/>
                  PaintOpIs<cc::RestoreOp>(),                          // </e1>
                  PaintOpIs<cc::SaveLayerAlphaOp>(),                   // <e2>
                  PaintOpIs<cc::DrawRecordOp>(),                       // <p2>
                  PaintOpIs<cc::RestoreOp>(),                          // </e2>
                  PaintOpIs<cc::RestoreOp>()));                        // </t1>
}

TEST_P(PaintChunksToCcLayerTest, NestedEffectsWithSameTransform) {
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto e1 = CreateOpacityEffect(e0(), *t1, &c0(), 0.1f);
  auto e2 = CreateOpacityEffect(*e1, *t1, &c0(), 0.2f);

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*t1, c0(), *e1);
  chunks.AddChunk(*t1, c0(), *e2);

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(
      output,
      ElementsAre(PaintOpIs<cc::DrawRecordOp>(),                       // <p0/>
                  PaintOpIs<cc::SaveOp>(), PaintOpIs<cc::ConcatOp>(),  // <t1>
                  PaintOpIs<cc::SaveLayerAlphaOp>(),                   // <e1>
                  PaintOpIs<cc::DrawRecordOp>(),                       // <p1/>
                  PaintOpIs<cc::SaveLayerAlphaOp>(),                   // <e2>
                  PaintOpIs<cc::DrawRecordOp>(),                       // <p2>
                  PaintOpIs<cc::RestoreOp>(),                          // </e2>
                  PaintOpIs<cc::RestoreOp>(),                          // </e1>
                  PaintOpIs<cc::RestoreOp>()));                        // </t1>
}

TEST_P(PaintChunksToCcLayerTest, NoopTransformIsNotEmitted) {
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto noop_t2 = TransformPaintPropertyNodeAlias::Create(*t1);
  auto noop_t3 = TransformPaintPropertyNodeAlias::Create(*noop_t2);
  auto t4 = CreateTransform(*noop_t3, MakeScaleMatrix(2));
  auto noop_t5 = TransformPaintPropertyNodeAlias::Create(*t4);
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*t1, c0(), e0());
  chunks.AddChunk(*noop_t2, c0(), e0());
  chunks.AddChunk(*noop_t3, c0(), e0());
  chunks.AddChunk(*noop_t2, c0(), e0());
  chunks.AddChunk(*t4, c0(), e0());
  chunks.AddChunk(*noop_t5, c0(), e0());
  chunks.AddChunk(*t4, c0(), e0());

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(
      output,
      ElementsAre(PaintOpIs<cc::DrawRecordOp>(),  // draw with t0
                  PaintOpIs<cc::SaveOp>(), PaintOpIs<cc::ConcatOp>(),  // t1
                  PaintOpIs<cc::DrawRecordOp>(),  // draw with t1
                  PaintOpIs<cc::DrawRecordOp>(),  // draw with noop_t2
                  PaintOpIs<cc::DrawRecordOp>(),  // draw with noop_t3
                  PaintOpIs<cc::DrawRecordOp>(),  // draw with noop_t2
                  PaintOpIs<cc::RestoreOp>(),     // end t1
                  PaintOpIs<cc::SaveOp>(), PaintOpIs<cc::ConcatOp>(),  // t4
                  PaintOpIs<cc::DrawRecordOp>(),  // draw with t4
                  PaintOpIs<cc::DrawRecordOp>(),  // draw with noop_t5
                  PaintOpIs<cc::DrawRecordOp>(),  // draw with t4
                  PaintOpIs<cc::RestoreOp>()      // end t4
                  ));
}

TEST_P(PaintChunksToCcLayerTest, OnlyNoopTransformIsNotEmitted) {
  auto noop_t1 = TransformPaintPropertyNodeAlias::Create(t0());
  auto noop_t2 = TransformPaintPropertyNodeAlias::Create(*noop_t1);

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*noop_t1, c0(), e0());
  chunks.AddChunk(*noop_t2, c0(), e0());

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(output, ElementsAre(PaintOpIs<cc::DrawRecordOp>(),
                                  PaintOpIs<cc::DrawRecordOp>(),
                                  PaintOpIs<cc::DrawRecordOp>()));
}

TEST_P(PaintChunksToCcLayerTest, NoopTransformFirstThenBackToParent) {
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto noop_t2 = TransformPaintPropertyNodeAlias::Create(*t1);

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*noop_t2, c0(), e0());
  chunks.AddChunk(*t1, c0(), e0());

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(output,
              ElementsAre(PaintOpIs<cc::DrawRecordOp>(),  // t0
                          PaintOpIs<cc::SaveOp>(),
                          PaintOpIs<cc::ConcatOp>(),      // t1 + noop_t2
                          PaintOpIs<cc::DrawRecordOp>(),  // draw with above
                          PaintOpIs<cc::DrawRecordOp>(),  // draw with just t1
                          PaintOpIs<cc::RestoreOp>()      // end t1
                          ));
}

TEST_P(PaintChunksToCcLayerTest, ClipUndoesNoopTransform) {
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto noop_t2 = TransformPaintPropertyNodeAlias::Create(*t1);
  auto c1 = CreateClip(c0(), *t1, FloatRoundedRect(0.f, 0.f, 1.f, 1.f));

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*noop_t2, c0(), e0());
  // The clip's local transform is t1, which is the parent of noop_t2.
  chunks.AddChunk(*noop_t2, *c1, e0());

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(
      output,
      ElementsAre(PaintOpIs<cc::DrawRecordOp>(),  // t0
                  PaintOpIs<cc::SaveOp>(),
                  PaintOpIs<cc::ConcatOp>(),  // t1 + noop_t2
                  PaintOpIs<cc::DrawRecordOp>(), PaintOpIs<cc::SaveOp>(),
                  PaintOpIs<cc::ClipRectOp>(),  // c1 (with t1 space)
                  PaintOpIs<cc::DrawRecordOp>(),
                  PaintOpIs<cc::RestoreOp>(),  // end c1
                  PaintOpIs<cc::RestoreOp>()   // end t1
                  ));
}

TEST_P(PaintChunksToCcLayerTest, EffectUndoesNoopTransform) {
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto noop_t2 = TransformPaintPropertyNodeAlias::Create(*t1);
  auto e1 = CreateOpacityEffect(e0(), *t1, &c0(), 0.5);

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*noop_t2, c0(), e0());
  // The effects's local transform is t1, which is the parent of noop_t2.
  chunks.AddChunk(*noop_t2, c0(), *e1);

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(output, ElementsAre(PaintOpIs<cc::DrawRecordOp>(),  // t0
                                  PaintOpIs<cc::SaveOp>(),
                                  PaintOpIs<cc::ConcatOp>(),  // t1 + noop_t2
                                  PaintOpIs<cc::DrawRecordOp>(),
                                  PaintOpIs<cc::SaveLayerAlphaOp>(),  // e1
                                  PaintOpIs<cc::DrawRecordOp>(),
                                  PaintOpIs<cc::RestoreOp>(),  // end e1
                                  PaintOpIs<cc::RestoreOp>()   // end t1
                                  ));
}

TEST_P(PaintChunksToCcLayerTest, NoopClipDoesNotEmitItems) {
  FloatRoundedRect clip_rect(0.f, 0.f, 1.f, 1.f);
  auto c1 = CreateClip(c0(), t0(), clip_rect);
  auto noop_c2 = ClipPaintPropertyNodeAlias::Create(*c1);
  auto noop_c3 = ClipPaintPropertyNodeAlias::Create(*noop_c2);
  auto c4 = CreateClip(*noop_c3, t0(), clip_rect);

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(t0(), *c1, e0());
  chunks.AddChunk(t0(), *noop_c2, e0());
  chunks.AddChunk(t0(), *noop_c3, e0());
  chunks.AddChunk(t0(), *c4, e0());
  chunks.AddChunk(t0(), *noop_c2, e0());
  chunks.AddChunk(t0(), *c1, e0());

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(
      output,
      ElementsAre(PaintOpIs<cc::DrawRecordOp>(),                         // c0
                  PaintOpIs<cc::SaveOp>(), PaintOpIs<cc::ClipRectOp>(),  // c1
                  PaintOpIs<cc::DrawRecordOp>(),  // draw with c1
                  PaintOpIs<cc::DrawRecordOp>(),  // draw with noop_c2
                  PaintOpIs<cc::DrawRecordOp>(),  // draw_with noop_c3
                  PaintOpIs<cc::SaveOp>(), PaintOpIs<cc::ClipRectOp>(),  // c4
                  PaintOpIs<cc::DrawRecordOp>(),  // draw with c4
                  PaintOpIs<cc::RestoreOp>(),     // end c4
                  PaintOpIs<cc::DrawRecordOp>(),  // draw with noop_c2
                  PaintOpIs<cc::DrawRecordOp>(),  // draw with c1
                  PaintOpIs<cc::RestoreOp>()      // end noop_c2 (or c1)
                  ));
}

TEST_P(PaintChunksToCcLayerTest, EffectUndoesNoopClip) {
  FloatRoundedRect clip_rect(0.f, 0.f, 1.f, 1.f);
  auto c1 = CreateClip(c0(), t0(), clip_rect);
  auto noop_c2 = ClipPaintPropertyNodeAlias::Create(*c1);
  auto e1 = CreateOpacityEffect(e0(), t0(), c1.get(), 0.5);

  TestChunks chunks;
  chunks.AddChunk(t0(), *noop_c2, e0());
  chunks.AddChunk(t0(), *noop_c2, *e1);

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(output,
              ElementsAre(PaintOpIs<cc::SaveOp>(),
                          PaintOpIs<cc::ClipRectOp>(),    // noop_c2
                          PaintOpIs<cc::DrawRecordOp>(),  // draw with noop_c2
                          PaintOpIs<cc::SaveLayerAlphaOp>(),  // e1
                          PaintOpIs<cc::DrawRecordOp>(),      // draw with e1
                          PaintOpIs<cc::RestoreOp>(),         // end e1
                          PaintOpIs<cc::RestoreOp>()          // end noop_c2
                          ));
}

TEST_P(PaintChunksToCcLayerTest, NoopEffectDoesNotEmitItems) {
  auto e1 = CreateOpacityEffect(e0(), 0.5f);
  auto noop_e2 = EffectPaintPropertyNodeAlias::Create(*e1);
  auto noop_e3 = EffectPaintPropertyNodeAlias::Create(*noop_e2);
  auto e4 = CreateOpacityEffect(*noop_e3, 0.5f);

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(t0(), c0(), *e1);
  chunks.AddChunk(t0(), c0(), *noop_e2);
  chunks.AddChunk(t0(), c0(), *noop_e3);
  chunks.AddChunk(t0(), c0(), *e4);
  chunks.AddChunk(t0(), c0(), *noop_e2);
  chunks.AddChunk(t0(), c0(), *e1);

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  EXPECT_THAT(output,
              ElementsAre(PaintOpIs<cc::DrawRecordOp>(),      // e0
                          PaintOpIs<cc::SaveLayerAlphaOp>(),  // e1
                          PaintOpIs<cc::DrawRecordOp>(),      // draw with e1
                          PaintOpIs<cc::DrawRecordOp>(),  // draw with noop_e2
                          PaintOpIs<cc::DrawRecordOp>(),  // draw_with noop_e3
                          PaintOpIs<cc::SaveLayerAlphaOp>(),  // e4
                          PaintOpIs<cc::DrawRecordOp>(),      // draw with e4
                          PaintOpIs<cc::RestoreOp>(),         // end e4
                          PaintOpIs<cc::DrawRecordOp>(),  // draw with noop_e2
                          PaintOpIs<cc::DrawRecordOp>(),  // draw with e1
                          PaintOpIs<cc::RestoreOp>()      // end noop_e2 (or e1)
                          ));
}

TEST_P(PaintChunksToCcLayerTest, EmptyChunkRect) {
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(5);
  auto e1 = CreateFilterEffect(e0(), t0(), &c0(), filter);
  TestChunks chunks;
  chunks.AddChunk(PaintRecord(), t0(), c0(), *e1, {0, 0, 0, 0});

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());

  cc::PaintFlags expected_flags;
  expected_flags.setImageFilter(cc::RenderSurfaceFilters::BuildImageFilter(
      filter.AsCcFilterOperations(), gfx::SizeF()));
  EXPECT_THAT(output, ElementsAre(PaintOpEq<cc::SaveLayerOp>(
                                      SkRect::MakeXYWH(0, 0, 0, 0),
                                      expected_flags),           // <e1>
                                  PaintOpIs<cc::RestoreOp>()));  // </e1>
}

static sk_sp<cc::PaintFilter> MakeFilter(gfx::RectF bounds) {
  PaintFilter::CropRect rect(gfx::RectFToSkRect(bounds));
  return sk_make_sp<ColorFilterPaintFilter>(
      SkColorFilters::Blend(SK_ColorBLUE, SkBlendMode::kSrc), nullptr, &rect);
}

TEST_P(PaintChunksToCcLayerTest, ReferenceFilterOnEmptyChunk) {
  CompositorFilterOperations filter;
  filter.AppendReferenceFilter(MakeFilter(gfx::RectF(12, 26, 93, 84)));
  filter.SetReferenceBox(gfx::RectF(11, 22, 33, 44));
  ASSERT_TRUE(filter.HasReferenceFilter());
  auto e1 = CreateFilterEffect(e0(), t0(), &c0(), filter);
  TestChunks chunks;
  chunks.AddEmptyChunk(t0(), c0(), *e1, gfx::Rect(0, 0, 200, 300));

  auto cc_list = base::MakeRefCounted<cc::DisplayItemList>();
  PaintChunksToCcLayer::ConvertInto(chunks.Build(), PropertyTreeState::Root(),
                                    gfx::Vector2dF(5, 10), nullptr, *cc_list);
  ASSERT_EQ(5u, cc_list->TotalOpCount());
  // (7 16) is (12, 26) - layer_offset.
  gfx::Rect expected_visual_rect(7, 16, 93, 84);
  for (size_t i = 0; i < cc_list->TotalOpCount(); i++) {
    SCOPED_TRACE(testing::Message() << "Visual rect of op " << i);
    EXPECT_EQ(expected_visual_rect, cc_list->VisualRectForTesting(i));
  }

  auto output = cc_list->FinalizeAndReleaseAsRecord();

  cc::PaintFlags expected_flags;
  expected_flags.setImageFilter(cc::RenderSurfaceFilters::BuildImageFilter(
      filter.AsCcFilterOperations(), gfx::SizeF()));
  EXPECT_THAT(output, ElementsAre(PaintOpIs<cc::SaveOp>(),
                                  PaintOpIs<cc::TranslateOp>(),  // layer offset
                                  PaintOpEq<cc::SaveLayerOp>(
                                      SkRect::MakeXYWH(12, 26, 93, 84),
                                      expected_flags),         // <e1>
                                  PaintOpIs<cc::RestoreOp>(),  // </e1>
                                  PaintOpIs<cc::RestoreOp>()));
}

TEST_P(PaintChunksToCcLayerTest, ReferenceFilterOnChunkWithDrawingDisplayItem) {
  CompositorFilterOperations filter;
  filter.AppendReferenceFilter(MakeFilter(gfx::RectF(7, 16, 93, 84)));
  filter.SetReferenceBox(gfx::RectF(11, 22, 33, 44));
  ASSERT_TRUE(filter.HasReferenceFilter());
  auto e1 = CreateFilterEffect(e0(), t0(), &c0(), filter);
  auto clip_expander = CreatePixelMovingFilterClipExpander(c0(), *e1);
  TestChunks chunks;
  chunks.AddChunk(t0(), *clip_expander, *e1, gfx::Rect(5, 10, 200, 300),
                  gfx::Rect(10, 15, 20, 30));

  auto cc_list = base::MakeRefCounted<cc::DisplayItemList>();
  PaintChunksToCcLayer::ConvertInto(chunks.Build(), PropertyTreeState::Root(),
                                    gfx::Vector2dF(5, 10), nullptr, *cc_list);
  ASSERT_EQ(7u, cc_list->TotalOpCount());
  // This is the visual rect for all filter related paint operations, which is
  // the union of the draw record and the output bounds of the filter with empty
  // input in the layer's space. This is also the rect that the chunk bounds map
  // to via MapVisualRect since the filter does not actually use the source.
  gfx::Rect expected_filter_visual_rect(2, 6, 93, 84);
  // TotalOpCount() - 1 because the DrawRecord op has a sub operation.
  for (size_t i = 0; i < cc_list->TotalOpCount() - 1; i++) {
    SCOPED_TRACE(testing::Message() << "Visual rect of op " << i);
    EXPECT_EQ(expected_filter_visual_rect, cc_list->VisualRectForTesting(i));
  }

  auto output = cc_list->FinalizeAndReleaseAsRecord();

  cc::PaintFlags expected_flags;
  expected_flags.setImageFilter(cc::RenderSurfaceFilters::BuildImageFilter(
      filter.AsCcFilterOperations(), gfx::SizeF()));
  EXPECT_THAT(
      output,
      ElementsAre(PaintOpIs<cc::SaveOp>(),
                  PaintOpIs<cc::TranslateOp>(),  // layer offset
                  // The effect bounds are the union of the chunk's
                  // drawable_bounds and the output bounds of the filter
                  // with empty input in the filter's space.
                  PaintOpEq<cc::SaveLayerOp>(SkRect::MakeXYWH(7, 15, 93, 85),
                                             expected_flags),  // <e1>
                  PaintOpIs<cc::DrawRecordOp>(),  // the DrawingDisplayItem
                  PaintOpIs<cc::RestoreOp>(),     // </e1>
                  PaintOpIs<cc::RestoreOp>()));
}

TEST_P(PaintChunksToCcLayerTest, FilterClipExpanderUnderClip) {
  // This tests the situation of crbug.com/1350017.
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(10);
  auto e1 = CreateFilterEffect(e0(), t0(), &c0(), filter);
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(10, 20, 30, 40));
  auto clip_expander = CreatePixelMovingFilterClipExpander(*c1, *e1);
  TestChunks chunks;
  chunks.AddChunk(t0(), *clip_expander, *e1, gfx::Rect(5, 10, 200, 300),
                  gfx::Rect(10, 15, 20, 30));

  auto output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());
  ASSERT_EQ(7u, output.total_op_count());
  EXPECT_THAT(
      output,
      ElementsAre(PaintOpIs<cc::SaveLayerOp>(),  // <e1>
                  PaintOpIs<cc::SaveOp>(),
                  PaintOpIs<cc::ClipRectOp>(),    // <c1>
                  PaintOpIs<cc::DrawRecordOp>(),  // the DrawingDisplayItem
                  PaintOpIs<cc::RestoreOp>(),     // </c1>
                  PaintOpIs<cc::RestoreOp>()));   // </e1>
}

TEST_P(PaintChunksToCcLayerTest,
       UpdateLayerPropertiesRegionCaptureDataSetOnLayer) {
  auto layer = cc::Layer::Create();

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0(), gfx::Rect(5, 10, 200, 300),
                  gfx::Rect(10, 15, 20, 30));

  const auto kCropId = RegionCaptureCropId(base::Token::CreateRandom());
  const RegionCaptureData kMap{{kCropId, gfx::Rect{50, 60, 100, 200}}};
  chunks.GetChunks()->back().region_capture_data =
      std::make_unique<RegionCaptureData>(kMap);

  UpdateLayerProperties(*layer, PropertyTreeState::Root(), chunks.Build());

  const gfx::Rect actual_bounds =
      layer->capture_bounds().bounds().find(kCropId.value())->second;
  EXPECT_EQ((gfx::Rect{50, 60, 100, 200}), actual_bounds);
}

TEST_P(PaintChunksToCcLayerTest,
       UpdateLayerPropertiesRegionCaptureDataUsesLayerOffset) {
  auto layer = cc::Layer::Create();
  layer->SetOffsetToTransformParent(gfx::Vector2dF{10, 15});
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0(), gfx::Rect(5, 10, 200, 300),
                  gfx::Rect(10, 15, 20, 30));

  const auto kCropId = RegionCaptureCropId(base::Token::CreateRandom());
  const RegionCaptureData kMap{{kCropId, gfx::Rect{50, 60, 100, 200}}};
  chunks.GetChunks()->back().region_capture_data =
      std::make_unique<RegionCaptureData>(kMap);

  UpdateLayerProperties(*layer, PropertyTreeState::Root(), chunks.Build());

  const gfx::Rect actual_bounds =
      layer->capture_bounds().bounds().find(kCropId.value())->second;
  EXPECT_EQ((gfx::Rect{40, 45, 100, 200}), actual_bounds);
}

TEST_P(PaintChunksToCcLayerTest, UpdateLayerPropertiesRegionCaptureDataEmpty) {
  auto layer = cc::Layer::Create();
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0(), gfx::Rect(5, 10, 200, 300),
                  gfx::Rect(10, 15, 20, 30));
  UpdateLayerProperties(*layer, PropertyTreeState::Root(), chunks.Build());
  EXPECT_TRUE(layer->capture_bounds().bounds().empty());
}

TEST_P(PaintChunksToCcLayerTest,
       UpdateLayerPropertiesRegionCaptureDataEmptyBounds) {
  auto layer = cc::Layer::Create();

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0(), gfx::Rect(5, 10, 200, 300),
                  gfx::Rect(10, 15, 20, 30));

  const auto kCropId = RegionCaptureCropId(base::Token::CreateRandom());
  const RegionCaptureData kMap{{kCropId, gfx::Rect{}}};
  chunks.GetChunks()->back().region_capture_data =
      std::make_unique<RegionCaptureData>(kMap);

  UpdateLayerProperties(*layer, PropertyTreeState::Root(), chunks.Build());

  const gfx::Rect actual_bounds =
      layer->capture_bounds().bounds().find(kCropId.value())->second;
  EXPECT_TRUE(actual_bounds.IsEmpty());
}

TEST_P(PaintChunksToCcLayerTest,
       UpdateLayerPropertiesRegionCaptureDataMultipleChunks) {
  auto layer = cc::Layer::Create();

  TestChunks chunks;

  // Add the first chunk with region capture bounds.
  chunks.AddChunk(t0(), c0(), e0(), gfx::Rect(5, 10, 200, 300),
                  gfx::Rect(10, 15, 20, 30));
  const auto kCropId = RegionCaptureCropId(base::Token::CreateRandom());
  const RegionCaptureData kMap{{kCropId, gfx::Rect{50, 60, 100, 200}}};
  chunks.GetChunks()->back().region_capture_data =
      std::make_unique<RegionCaptureData>(kMap);

  // Add a second chunk with additional region capture bounds.
  chunks.AddChunk(t0(), c0(), e0(), gfx::Rect(6, 12, 244, 366),
                  gfx::Rect(20, 30, 40, 60));
  const auto kSecondCropId = RegionCaptureCropId(base::Token::CreateRandom());
  const auto kThirdCropId = RegionCaptureCropId(base::Token::CreateRandom());
  const RegionCaptureData kSecondMap{
      {kSecondCropId, gfx::Rect{51, 61, 101, 201}},
      {kThirdCropId, gfx::Rect{52, 62, 102, 202}}};
  chunks.GetChunks()->back().region_capture_data =
      std::make_unique<RegionCaptureData>(kSecondMap);

  UpdateLayerProperties(*layer, PropertyTreeState::Root(), chunks.Build());

  EXPECT_EQ((gfx::Rect{50, 60, 100, 200}),
            layer->capture_bounds().bounds().find(kCropId.value())->second);
  EXPECT_EQ(
      (gfx::Rect{51, 61, 101, 201}),
      layer->capture_bounds().bounds().find(kSecondCropId.value())->second);
  EXPECT_EQ(
      (gfx::Rect{52, 62, 102, 202}),
      layer->capture_bounds().bounds().find(kThirdCropId.value())->second);
}

TEST_P(PaintChunksToCcLayerTest, NonCompositedBackdropFilter) {
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(5);
  auto e1 = CreateBackdropFilterEffect(e0(), filter);
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), *e1, gfx::Rect(0, 0, 50, 50));

  PaintRecord output =
      PaintChunksToCcLayer::Convert(chunks.Build(), PropertyTreeState::Root());
  // TODO(crbug.com/1334293): For now non-composited backdrop filters are
  // ignored.
  EXPECT_THAT(output, ElementsAre(PaintOpIs<cc::SaveLayerAlphaOp>(),
                                  PaintOpIs<cc::DrawRecordOp>(),
                                  PaintOpIs<cc::RestoreOp>()));
}

}  // namespace
}  // namespace blink
