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

void PrintTo(const Vector<cc::PaintOpType>& ops, std::ostream* os) {
  *os << "[";
  bool first = true;
  for (auto op : ops) {
    if (first)
      first = false;
    else
      *os << ", ";
    *os << op;
  }
  *os << "]";
}

void PrintTo(const cc::PaintRecord& record, std::ostream* os) {
  Vector<cc::PaintOpType> ops;
  for (const cc::PaintOp& op : record)
    ops.push_back(op.GetType());
  PrintTo(ops, os);
}

void PrintTo(const SkRect& rect, std::ostream* os) {
  *os << (cc::PaintOp::IsUnsetRect(rect) ? "(unset)"
                                         : gfx::SkRectToRectF(rect).ToString());
}

namespace blink {
namespace {

class PaintChunksToCcLayerTest : public testing::Test,
                                 public PaintTestConfigurations {};

INSTANTIATE_PAINT_TEST_SUITE_P(PaintChunksToCcLayerTest);

// Matches PaintOpTypes in a PaintRecord.
class PaintRecordMatcher
    : public testing::MatcherInterface<const cc::PaintOpBuffer&> {
 public:
  static testing::Matcher<const cc::PaintOpBuffer&> Make(
      std::initializer_list<cc::PaintOpType> args) {
    return testing::MakeMatcher(new PaintRecordMatcher(args));
  }
  PaintRecordMatcher(std::initializer_list<cc::PaintOpType> args)
      : expected_ops_(args) {}

  bool MatchAndExplain(const cc::PaintOpBuffer& buffer,
                       testing::MatchResultListener* listener) const override {
    size_t index = 0;
    for (cc::PaintOpBuffer::Iterator it(&buffer); it; ++it, ++index) {
      cc::PaintOpType op_type = it->GetType();
      if (index < expected_ops_.size() && expected_ops_[index] == op_type)
        continue;

      if (listener->IsInterested()) {
        *listener << "unexpected op " << op_type << " at index " << index
                  << ",";
        if (index < expected_ops_.size())
          *listener << " expecting " << expected_ops_[index] << ".";
        else
          *listener << " expecting end of list.";
      }
      return false;
    }
    if (index == expected_ops_.size())
      return true;
    if (listener->IsInterested()) {
      *listener << "unexpected end of list, expecting " << expected_ops_[index]
                << " at index " << index << ".";
    }
    return false;
  }

  void DescribeTo(::std::ostream* os) const override {
    PrintTo(expected_ops_, os);
  }

 private:
  Vector<cc::PaintOpType> expected_ops_;
};

#define EXPECT_EFFECT_BOUNDS(x, y, width, height, op_buffer, index)         \
  do {                                                                      \
    SkRect bounds;                                                          \
    if (const auto* save_layer_alpha =                                      \
            (op_buffer).GetOpAtForTesting<cc::SaveLayerAlphaOp>(index)) {   \
      bounds = save_layer_alpha->bounds;                                    \
    } else if (const auto* save_layer =                                     \
                   (op_buffer).GetOpAtForTesting<cc::SaveLayerOp>(index)) { \
      bounds = save_layer->bounds;                                          \
    } else {                                                                \
      FAIL() << "No SaveLayer[Alpha]Op at " << index;                       \
    }                                                                       \
    EXPECT_EQ(SkRect::MakeXYWH(x, y, width, height), bounds);               \
  } while (false)

#define EXPECT_TRANSFORM_MATRIX(transform, op_buffer, index)                 \
  do {                                                                       \
    const auto* concat = (op_buffer).GetOpAtForTesting<cc::ConcatOp>(index); \
    ASSERT_NE(nullptr, concat);                                              \
    EXPECT_EQ(gfx::TransformToSkM44(transform), concat->matrix);             \
  } while (false)

#define EXPECT_TRANSLATE(x, y, op_buffer, index)               \
  do {                                                         \
    const auto* translate =                                    \
        (op_buffer).GetOpAtForTesting<cc::TranslateOp>(index); \
    ASSERT_NE(nullptr, translate);                             \
    EXPECT_EQ(x, translate->dx);                               \
    EXPECT_EQ(y, translate->dy);                               \
  } while (false)

#define EXPECT_CLIP(r, op_buffer, index)                      \
  do {                                                        \
    const auto* clip_op =                                     \
        (op_buffer).GetOpAtForTesting<cc::ClipRectOp>(index); \
    ASSERT_NE(nullptr, clip_op);                              \
    EXPECT_EQ(gfx::RectFToSkRect(r), clip_op->rect);          \
  } while (false)

#define EXPECT_ROUNDED_CLIP(r, op_buffer, index)               \
  do {                                                         \
    const auto* clip_op =                                      \
        (op_buffer).GetOpAtForTesting<cc::ClipRRectOp>(index); \
    ASSERT_NE(nullptr, clip_op);                               \
    EXPECT_EQ(SkRRect(r), clip_op->rrect);                     \
  } while (false)

const DisplayItemClient& DefaultClient() {
  DEFINE_STATIC_LOCAL(
      Persistent<FakeDisplayItemClient>, fake_client,
      (MakeGarbageCollected<FakeDisplayItemClient>("FakeDisplayItemClient")));
  return *fake_client;
}

PaintChunk::Id DefaultId() {
  return PaintChunk::Id(DefaultClient().Id(), DisplayItem::kDrawingFirst);
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
    auto record = sk_make_sp<PaintRecord>();
    record->push<cc::DrawRectOp>(
        gfx::RectToSkRect(drawable_bounds ? *drawable_bounds : bounds),
        cc::PaintFlags());
    AddChunk(std::move(record), t, c, e, bounds, drawable_bounds);
  }

  // Add a paint chunk with a given paint record and property nodes.
  void AddChunk(
      sk_sp<PaintRecord> record,
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

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());
  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make({cc::PaintOpType::SaveLayerAlpha,  // <e1>
                                cc::PaintOpType::DrawRecord,      // <p0/>
                                cc::PaintOpType::DrawRecord,      // <p1/>
                                cc::PaintOpType::Restore}));      // </e1>
  EXPECT_EFFECT_BOUNDS(0, 0, 90, 90, *output, 0);
}

TEST_P(PaintChunksToCcLayerTest, EffectGroupingNested) {
  // This test verifies nested effects are grouped properly.
  auto e1 = CreateOpacityEffect(e0(), 0.5f);
  auto e2 = CreateOpacityEffect(*e1, 0.5f);
  auto e3 = CreateOpacityEffect(*e1, 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), *e2);
  chunks.AddChunk(t0(), c0(), *e3, gfx::Rect(111, 222, 333, 444));

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());
  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make({cc::PaintOpType::SaveLayerAlpha,  // <e1>
                                cc::PaintOpType::SaveLayerAlpha,  // <e2>
                                cc::PaintOpType::DrawRecord,      // <p0/>
                                cc::PaintOpType::Restore,         // </e2>
                                cc::PaintOpType::SaveLayerAlpha,  // <e3>
                                cc::PaintOpType::DrawRecord,      // <p1/>
                                cc::PaintOpType::Restore,         // </e3>
                                cc::PaintOpType::Restore}));      // </e1>
  EXPECT_EFFECT_BOUNDS(0, 0, 444, 666, *output, 0);
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 1);
  EXPECT_EFFECT_BOUNDS(111, 222, 333, 444, *output, 4);
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

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());
  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make(
          {cc::PaintOpType::Save, cc::PaintOpType::Concat,     // <t1*t2>
           cc::PaintOpType::SaveLayerAlpha,                    // <e1>
           cc::PaintOpType::DrawRecord,                        // <p1/>
           cc::PaintOpType::SaveLayer,                         // <e2>
           cc::PaintOpType::Save, cc::PaintOpType::Translate,  // <t2^-1>
           cc::PaintOpType::DrawRecord,                        // <p2/>
           cc::PaintOpType::Restore,                           // </t2^-1>
           cc::PaintOpType::Restore,                           // </e2>
           cc::PaintOpType::Restore,                           // </e1>
           cc::PaintOpType::Restore}));                        // </t1*t2>
  EXPECT_TRANSFORM_MATRIX(t1->Matrix() * t2->Matrix(), *output, 1);
  // chunk1.bounds + e2(t2^-1(chunk2.bounds))
  EXPECT_EFFECT_BOUNDS(0, 0, 155, 155, *output, 2);
  // t2^-1(chunk2.bounds)
  EXPECT_EFFECT_BOUNDS(70, 70, 70, 70, *output, 4);
  // t2^1
  EXPECT_TRANSLATE(-t2->Get2dTranslation().x(), -t2->Get2dTranslation().y(),
                   *output, 6);
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

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());
  EXPECT_THAT(*output, PaintRecordMatcher::Make(
                           {cc::PaintOpType::Save,
                            cc::PaintOpType::ClipRect,    // <c1+c2>
                            cc::PaintOpType::DrawRecord,  // <p0/>
                            cc::PaintOpType::Save,
                            cc::PaintOpType::ClipRect,        // <c3>
                            cc::PaintOpType::DrawRecord,      // <p1/>
                            cc::PaintOpType::Restore,         // </c3>
                            cc::PaintOpType::SaveLayerAlpha,  // <e1>
                            cc::PaintOpType::Save,
                            cc::PaintOpType::ClipRect,        // <c3+c4>
                            cc::PaintOpType::SaveLayerAlpha,  // <e2>
                            cc::PaintOpType::DrawRecord,      // <p2/>
                            cc::PaintOpType::Restore,         // </e2>
                            cc::PaintOpType::Restore,         // </c3+c4>
                            cc::PaintOpType::Save,
                            cc::PaintOpType::ClipRect,    // <c3>
                            cc::PaintOpType::DrawRecord,  // <p3/>
                            cc::PaintOpType::Restore,     // </c3>
                            cc::PaintOpType::Restore,     // </e1>
                            cc::PaintOpType::Save,
                            cc::PaintOpType::ClipRect,    // <c3+c4>
                            cc::PaintOpType::DrawRecord,  // <p4/>
                            cc::PaintOpType::Restore,     // </c3+c4>
                            cc::PaintOpType::Restore}));  // </c1+c2>
  EXPECT_EFFECT_BOUNDS(0, 0, 90, 90, *output, 7);
  EXPECT_EFFECT_BOUNDS(0, 0, 50, 50, *output, 10);
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

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make(
                  {cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1
                   cc::PaintOpType::ClipRect,                       //  c1>
                   cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1^-1>
                   cc::PaintOpType::DrawRecord,                     // <p0/>
                   cc::PaintOpType::Restore,                        // </t1^-1>
                   cc::PaintOpType::Restore}));                     // </c1 t1>
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

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make(
                  {cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1>
                   cc::PaintOpType::SaveLayerAlpha,                 // <e1>
                   cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1^-1>
                   cc::PaintOpType::DrawRecord,                     // <p0/>
                   cc::PaintOpType::Restore,                        // </t1^-1>
                   cc::PaintOpType::DrawRecord,                     // <p1/>
                   cc::PaintOpType::Restore,                        // </e1>
                   cc::PaintOpType::Restore}));                     // </t1>
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 2);
  EXPECT_TRANSFORM_MATRIX(t1->Matrix(), *output, 1);
  EXPECT_TRANSFORM_MATRIX(t1->Matrix().GetCheckedInverse(), *output, 4);
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

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());
  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make(
          {cc::PaintOpType::Save, cc::PaintOpType::Concat,     // <t1>
           cc::PaintOpType::SaveLayer,                         // <e1>
           cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1^-1>
           cc::PaintOpType::DrawRecord,                     // <p0/>
           cc::PaintOpType::Restore,                        // </t1^-1>
           cc::PaintOpType::Restore,                        // </e1>
           cc::PaintOpType::Restore}));                     // </t1>
  EXPECT_TRANSFORM_MATRIX(t1->Matrix(), *output, 1);
  EXPECT_EFFECT_BOUNDS(0, 0, 50, 50, *output, 2);
  EXPECT_TRANSFORM_MATRIX(t1->Matrix().GetCheckedInverse(), *output, 4);
}

TEST_P(PaintChunksToCcLayerTest, NonRootLayerSimple) {
  // This test verifies a layer with composited property state does not
  // apply properties again internally.
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto e1 = CreateOpacityEffect(e0(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(*t1, *c1, *e1);

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState(*t1, *c1, *e1), gfx::Vector2dF());
  EXPECT_THAT(*output, PaintRecordMatcher::Make({cc::PaintOpType::DrawRecord}));
}

TEST_P(PaintChunksToCcLayerTest, NonRootLayerTransformEscape) {
  // This test verifies chunks that have a shallower transform state than the
  // layer can still be painted.
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto e1 = CreateOpacityEffect(e0(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t0(), *c1, *e1);

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState(*t1, *c1, *e1), gfx::Vector2dF());
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make(
                  {cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1^-1>
                   cc::PaintOpType::DrawRecord,                     // <p0/>
                   cc::PaintOpType::Restore}));                     // </t1^-1>
}

TEST_P(PaintChunksToCcLayerTest, EffectWithNoOutputClip) {
  // This test verifies effect with no output clip can be correctly processed.
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto c2 = CreateClip(*c1, t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto e1 = CreateOpacityEffect(e0(), t0(), nullptr, 0.5);

  TestChunks chunks;
  chunks.AddChunk(t0(), *c2, *e1);

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState(t0(), *c1, e0()), gfx::Vector2dF());
  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make({cc::PaintOpType::SaveLayerAlpha,  // <e1>
                                cc::PaintOpType::Save,
                                cc::PaintOpType::ClipRect,    // <c2>
                                cc::PaintOpType::DrawRecord,  // <p0/>
                                cc::PaintOpType::Restore,     // </c2>
                                cc::PaintOpType::Restore}));  // </e1>
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 0);
}

TEST_P(PaintChunksToCcLayerTest,
       EffectWithNoOutputClipNestedInDecompositedEffect) {
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto e1 = CreateOpacityEffect(e0(), 0.5);
  auto e2 = CreateOpacityEffect(*e1, t0(), nullptr, 0.5);

  TestChunks chunks;
  chunks.AddChunk(t0(), *c1, *e2);

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());
  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make({cc::PaintOpType::SaveLayerAlpha,  // <e1>
                                cc::PaintOpType::SaveLayerAlpha,  // <e2>
                                cc::PaintOpType::Save,
                                cc::PaintOpType::ClipRect,    // <c1>
                                cc::PaintOpType::DrawRecord,  // <p0/>
                                cc::PaintOpType::Restore,     // </c1>
                                cc::PaintOpType::Restore,     // </e2>
                                cc::PaintOpType::Restore}));  // </e1>
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 0);
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 1);
}

TEST_P(PaintChunksToCcLayerTest,
       EffectWithNoOutputClipNestedInCompositedEffect) {
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto e1 = CreateOpacityEffect(e0(), 0.5);
  auto e2 = CreateOpacityEffect(*e1, t0(), nullptr, 0.5);

  TestChunks chunks;
  chunks.AddChunk(t0(), *c1, *e2);

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState(t0(), c0(), *e1), gfx::Vector2dF());
  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make({cc::PaintOpType::SaveLayerAlpha,  // <e2>
                                cc::PaintOpType::Save,
                                cc::PaintOpType::ClipRect,    // <c1>
                                cc::PaintOpType::DrawRecord,  // <p0/>
                                cc::PaintOpType::Restore,     // </c1>
                                cc::PaintOpType::Restore}));  // </e2>
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 0);
}

TEST_P(PaintChunksToCcLayerTest,
       EffectWithNoOutputClipNestedInCompositedEffectAndClip) {
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto e1 = CreateOpacityEffect(e0(), 0.5);
  auto e2 = CreateOpacityEffect(*e1, t0(), nullptr, 0.5);

  TestChunks chunks;
  chunks.AddChunk(t0(), *c1, *e2);

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState(t0(), *c1, *e1), gfx::Vector2dF());
  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make({cc::PaintOpType::SaveLayerAlpha,  // <e2>
                                cc::PaintOpType::DrawRecord,      // <p0/>
                                cc::PaintOpType::Restore}));      // </e2>
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 0);
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

  EXPECT_THAT(*cc_list->FinalizeAndReleaseAsRecord(),
              PaintRecordMatcher::Make(
                  {cc::PaintOpType::Save,        //
                   cc::PaintOpType::Translate,   // <layer_offset>
                   cc::PaintOpType::Save,        //
                   cc::PaintOpType::Translate,   // <layer_transform>
                   cc::PaintOpType::DrawRecord,  // <p0/>
                   cc::PaintOpType::Restore,     // </layer_transform>
                   cc::PaintOpType::Restore}));  // </layer_offset>
}

TEST_P(PaintChunksToCcLayerTest, NoncompositedClipPath) {
  auto c1 = CreateClipPathClip(c0(), t0(), FloatRoundedRect(1, 2, 3, 4));
  TestChunks chunks;
  chunks.AddChunk(t0(), *c1, e0());

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make({cc::PaintOpType::Save,        //
                                cc::PaintOpType::ClipRect,    //
                                cc::PaintOpType::ClipPath,    // <clip_path>
                                cc::PaintOpType::DrawRecord,  // <p0/>
                                cc::PaintOpType::Restore}));  // </clip_path>
}

TEST_P(PaintChunksToCcLayerTest, EmptyClipsAreElided) {
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto c1c2 = CreateClip(*c1, t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto c2 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));

  TestChunks chunks;
  chunks.AddChunk(nullptr, t0(), *c1, e0());
  chunks.AddChunk(nullptr, t0(), *c1c2, e0());
  chunks.AddChunk(nullptr, t0(), *c1c2, e0());
  chunks.AddChunk(nullptr, t0(), *c1c2, e0());
  chunks.AddChunk(nullptr, t0(), *c1, e0());
  // D1
  chunks.AddChunk(t0(), *c2, e0());

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  // Note that c1 and c1c2 are elided.
  EXPECT_THAT(*output, PaintRecordMatcher::Make({
                           cc::PaintOpType::Save,        //
                           cc::PaintOpType::ClipRect,    // <c2>
                           cc::PaintOpType::DrawRecord,  // D1
                           cc::PaintOpType::Restore,     // </c2>
                       }));
}

TEST_P(PaintChunksToCcLayerTest, NonEmptyClipsAreStored) {
  auto c1 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto c1c2 = CreateClip(*c1, t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  auto c2 = CreateClip(c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));

  TestChunks chunks;
  chunks.AddChunk(nullptr, t0(), *c1, e0());
  chunks.AddChunk(nullptr, t0(), *c1c2, e0());
  chunks.AddChunk(nullptr, t0(), *c1c2, e0());
  // D1
  chunks.AddChunk(t0(), *c1c2, e0());
  chunks.AddChunk(nullptr, t0(), *c1, e0());
  // D2
  chunks.AddChunk(t0(), *c2, e0());

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({
                  cc::PaintOpType::Save, cc::PaintOpType::ClipRect,  // <c1+c2>
                  cc::PaintOpType::DrawRecord,                       // D1
                  cc::PaintOpType::Restore,                          // </c1+c2>
                  cc::PaintOpType::Save, cc::PaintOpType::ClipRect,  // <c2>
                  cc::PaintOpType::DrawRecord,                       // D2
                  cc::PaintOpType::Restore,                          // </c2>
              }));
}

TEST_P(PaintChunksToCcLayerTest, EmptyEffectsAreStored) {
  auto e1 = CreateOpacityEffect(e0(), 0.5);

  TestChunks chunks;
  chunks.AddChunk(nullptr, t0(), c0(), e0());
  chunks.AddChunk(nullptr, t0(), c0(), *e1);

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output, PaintRecordMatcher::Make({
                           cc::PaintOpType::SaveLayerAlpha,  // <e1>
                           cc::PaintOpType::Restore,         // </e1>
                       }));
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 0);
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

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make(
          {cc::PaintOpType::Save, cc::PaintOpType::ClipRect,      // <c1+c2>
           cc::PaintOpType::Save, cc::PaintOpType::Concat,        // <t1
           cc::PaintOpType::ClipRect, cc::PaintOpType::ClipPath,  //  c3+c4+c5>
           cc::PaintOpType::Save, cc::PaintOpType::ClipRect,      // <c6>
           cc::PaintOpType::DrawRecord,                           // <p0/>
           cc::PaintOpType::Restore,                              // </c6>
           cc::PaintOpType::Restore,                        // </c3+c4+c5 t1>
           cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1
           cc::PaintOpType::ClipRect,                       // c3>
           cc::PaintOpType::DrawRecord,                     // <p1/>
           cc::PaintOpType::Restore,                        // </c3 t1>
           cc::PaintOpType::Restore}));                     // </c1+c2>
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

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  // We combine c1/c2 across |identity|, but not c2/c3 across |non_identity|
  // and c3/c4 across |non_invertible|.
  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make(
          {cc::PaintOpType::Save, cc::PaintOpType::ClipRect,  // <c1+c2>
           cc::PaintOpType::Save, cc::PaintOpType::Concat,    // <non_identity
           cc::PaintOpType::ClipRect,                         //  c3>
           cc::PaintOpType::Save, cc::PaintOpType::Concat,    // <non_invertible
           cc::PaintOpType::ClipRect,                         //  c4>
           cc::PaintOpType::DrawRecord,                       // <p0/>
           cc::PaintOpType::Restore,     // </c4 non_invertible>
           cc::PaintOpType::Restore,     // </c3 non_identity>
           cc::PaintOpType::Restore}));  // </c1+c2>

  EXPECT_CLIP(gfx::RectF(50, 50, 50, 50), *output, 1);
  EXPECT_TRANSFORM_MATRIX(non_identity->Matrix(), *output, 3);
  EXPECT_CLIP(gfx::RectF(1, 2, 3, 4), *output, 4);
  EXPECT_TRANSFORM_MATRIX(non_invertible->Matrix(), *output, 6);
  EXPECT_CLIP(gfx::RectF(5, 6, 7, 8), *output, 7);
}

TEST_P(PaintChunksToCcLayerTest, CombineClipsWithRoundedRects) {
  FloatRoundedRect clip_rect(0, 0, 100, 100);
  FloatRoundedRect big_rounded_clip_rect(gfx::RectF(0, 0, 200, 200), 5);
  FloatRoundedRect small_rounded_clip_rect(gfx::RectF(0, 0, 100, 100), 5);

  auto c1 = CreateClip(c0(), t0(), clip_rect);
  auto c2 = CreateClip(*c1, t0(), small_rounded_clip_rect);
  auto c3 = CreateClip(*c2, t0(), clip_rect);
  auto c4 = CreateClip(*c3, t0(), big_rounded_clip_rect);
  auto c5 = CreateClip(*c4, t0(), clip_rect);
  auto c6 = CreateClip(*c5, t0(), big_rounded_clip_rect);
  auto c7 = CreateClip(*c6, t0(), small_rounded_clip_rect);

  TestChunks chunks;
  chunks.AddChunk(t0(), *c7, e0());

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make(
          {cc::PaintOpType::Save, cc::PaintOpType::ClipRRect,  // <c1+c2+c3>
           cc::PaintOpType::Save, cc::PaintOpType::ClipRRect,  // <c4>
           cc::PaintOpType::Save, cc::PaintOpType::ClipRect,   // <c5>
           cc::PaintOpType::Save, cc::PaintOpType::ClipRRect,  // <c6>
           cc::PaintOpType::Save, cc::PaintOpType::ClipRRect,  // <c7>
           cc::PaintOpType::DrawRecord,                        // <p0/>
           cc::PaintOpType::Restore,                           // </c7>
           cc::PaintOpType::Restore,                           // </c6>
           cc::PaintOpType::Restore,                           // </c5>
           cc::PaintOpType::Restore,                           // </c4>
           cc::PaintOpType::Restore}));                        // </c1+c2+c3>

  EXPECT_ROUNDED_CLIP(small_rounded_clip_rect, *output, 1);
  EXPECT_ROUNDED_CLIP(big_rounded_clip_rect, *output, 3);
  EXPECT_CLIP(clip_rect.Rect(), *output, 5);
  EXPECT_ROUNDED_CLIP(big_rounded_clip_rect, *output, 7);
  EXPECT_ROUNDED_CLIP(small_rounded_clip_rect, *output, 9);
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

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output,
              PaintRecordMatcher::Make(
                  {cc::PaintOpType::DrawRecord,                       // <p0/>
                   cc::PaintOpType::Save, cc::PaintOpType::Concat,    // <t1>
                   cc::PaintOpType::DrawRecord,                       // <p1/>
                   cc::PaintOpType::DrawRecord,                       // <p2/>
                   cc::PaintOpType::Save, cc::PaintOpType::ClipRect,  // <c1>
                   cc::PaintOpType::DrawRecord,                       // <p3/>
                   cc::PaintOpType::DrawRecord,                       // <p4/>
                   cc::PaintOpType::Save, cc::PaintOpType::Concat,    // <t2>
                   cc::PaintOpType::DrawRecord,                       // <p5/>
                   cc::PaintOpType::DrawRecord,                       // <p6/>
                   cc::PaintOpType::Restore,                          // </t2>
                   cc::PaintOpType::Restore,                          // </c1>
                   cc::PaintOpType::Restore}));                       // </t1>
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

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output,
              PaintRecordMatcher::Make(
                  {cc::PaintOpType::DrawRecord,                       // <p0/>
                   cc::PaintOpType::DrawRecord,                       // <p1/>
                   cc::PaintOpType::DrawRecord,                       // <p2/>
                   cc::PaintOpType::DrawRecord,                       // <p3/>
                   cc::PaintOpType::DrawRecord,                       // <p4/>
                   cc::PaintOpType::DrawRecord,                       // <p5/>
                   cc::PaintOpType::Save, cc::PaintOpType::ClipRect,  // <c1+c2>
                   cc::PaintOpType::DrawRecord,                       // <p6/>
                   cc::PaintOpType::Restore}));  // </c1+c2>
}

TEST_P(PaintChunksToCcLayerTest, EffectsWithSameTransform) {
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto e1 = CreateOpacityEffect(e0(), *t1, &c0(), 0.1f);
  auto e2 = CreateOpacityEffect(e0(), *t1, &c0(), 0.2f);

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*t1, c0(), *e1);
  chunks.AddChunk(*t1, c0(), *e2);

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output,
              PaintRecordMatcher::Make(
                  {cc::PaintOpType::DrawRecord,                     // <p0/>
                   cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1>
                   cc::PaintOpType::SaveLayerAlpha,                 // <e1>
                   cc::PaintOpType::DrawRecord,                     // <p1/>
                   cc::PaintOpType::Restore,                        // </e1>
                   cc::PaintOpType::SaveLayerAlpha,                 // <e2>
                   cc::PaintOpType::DrawRecord,                     // <p2>
                   cc::PaintOpType::Restore,                        // </e2>
                   cc::PaintOpType::Restore}));                     // </t1>
}

TEST_P(PaintChunksToCcLayerTest, NestedEffectsWithSameTransform) {
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto e1 = CreateOpacityEffect(e0(), *t1, &c0(), 0.1f);
  auto e2 = CreateOpacityEffect(*e1, *t1, &c0(), 0.2f);

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*t1, c0(), *e1);
  chunks.AddChunk(*t1, c0(), *e2);

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output,
              PaintRecordMatcher::Make(
                  {cc::PaintOpType::DrawRecord,                     // <p0/>
                   cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1>
                   cc::PaintOpType::SaveLayerAlpha,                 // <e1>
                   cc::PaintOpType::DrawRecord,                     // <p1/>
                   cc::PaintOpType::SaveLayerAlpha,                 // <e2>
                   cc::PaintOpType::DrawRecord,                     // <p2>
                   cc::PaintOpType::Restore,                        // </e2>
                   cc::PaintOpType::Restore,                        // </e1>
                   cc::PaintOpType::Restore}));                     // </t1>
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

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({
                  cc::PaintOpType::DrawRecord,  // draw with t0
                  cc::PaintOpType::Save, cc::PaintOpType::Concat,  // t1
                  cc::PaintOpType::DrawRecord,  // draw with t1
                  cc::PaintOpType::DrawRecord,  // draw with noop_t2
                  cc::PaintOpType::DrawRecord,  // draw with noop_t3
                  cc::PaintOpType::DrawRecord,  // draw with noop_t2
                  cc::PaintOpType::Restore,     // end t1
                  cc::PaintOpType::Save, cc::PaintOpType::Concat,  // t4
                  cc::PaintOpType::DrawRecord,  // draw with t4
                  cc::PaintOpType::DrawRecord,  // draw with noop_t5
                  cc::PaintOpType::DrawRecord,  // draw with t4
                  cc::PaintOpType::Restore      // end t4
              }));
}

TEST_P(PaintChunksToCcLayerTest, OnlyNoopTransformIsNotEmitted) {
  auto noop_t1 = TransformPaintPropertyNodeAlias::Create(t0());
  auto noop_t2 = TransformPaintPropertyNodeAlias::Create(*noop_t1);

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*noop_t1, c0(), e0());
  chunks.AddChunk(*noop_t2, c0(), e0());

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output, PaintRecordMatcher::Make({cc::PaintOpType::DrawRecord,
                                                 cc::PaintOpType::DrawRecord,
                                                 cc::PaintOpType::DrawRecord}));
}

TEST_P(PaintChunksToCcLayerTest, NoopTransformFirstThenBackToParent) {
  auto t1 = CreateTransform(t0(), MakeScaleMatrix(2));
  auto noop_t2 = TransformPaintPropertyNodeAlias::Create(*t1);

  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0());
  chunks.AddChunk(*noop_t2, c0(), e0());
  chunks.AddChunk(*t1, c0(), e0());

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output, PaintRecordMatcher::Make({
                           cc::PaintOpType::DrawRecord,  // t0
                           cc::PaintOpType::Save,
                           cc::PaintOpType::Concat,      // t1 + noop_t2
                           cc::PaintOpType::DrawRecord,  // draw with above
                           cc::PaintOpType::DrawRecord,  // draw with just t1
                           cc::PaintOpType::Restore      // end t1
                       }));
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

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output, PaintRecordMatcher::Make({
                           cc::PaintOpType::DrawRecord,  // t0
                           cc::PaintOpType::Save,
                           cc::PaintOpType::Concat,  // t1 + noop_t2
                           cc::PaintOpType::DrawRecord, cc::PaintOpType::Save,
                           cc::PaintOpType::ClipRect,  // c1 (with t1 space)
                           cc::PaintOpType::DrawRecord,
                           cc::PaintOpType::Restore,  // end c1
                           cc::PaintOpType::Restore   // end t1
                       }));
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

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output, PaintRecordMatcher::Make({
                           cc::PaintOpType::DrawRecord,  // t0
                           cc::PaintOpType::Save,
                           cc::PaintOpType::Concat,  // t1 + noop_t2
                           cc::PaintOpType::DrawRecord,
                           cc::PaintOpType::SaveLayerAlpha,  // e1
                           cc::PaintOpType::DrawRecord,
                           cc::PaintOpType::Restore,  // end e1
                           cc::PaintOpType::Restore   // end t1
                       }));
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

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({
                  cc::PaintOpType::DrawRecord,                       // c0
                  cc::PaintOpType::Save, cc::PaintOpType::ClipRect,  // c1
                  cc::PaintOpType::DrawRecord,  // draw with c1
                  cc::PaintOpType::DrawRecord,  // draw with noop_c2
                  cc::PaintOpType::DrawRecord,  // draw_with noop_c3
                  cc::PaintOpType::Save, cc::PaintOpType::ClipRect,  // c4
                  cc::PaintOpType::DrawRecord,  // draw with c4
                  cc::PaintOpType::Restore,     // end c4
                  cc::PaintOpType::DrawRecord,  // draw with noop_c2
                  cc::PaintOpType::DrawRecord,  // draw with c1
                  cc::PaintOpType::Restore      // end noop_c2 (or c1)
              }));
}

TEST_P(PaintChunksToCcLayerTest, EffectUndoesNoopClip) {
  FloatRoundedRect clip_rect(0.f, 0.f, 1.f, 1.f);
  auto c1 = CreateClip(c0(), t0(), clip_rect);
  auto noop_c2 = ClipPaintPropertyNodeAlias::Create(*c1);
  auto e1 = CreateOpacityEffect(e0(), t0(), c1.get(), 0.5);

  TestChunks chunks;
  chunks.AddChunk(t0(), *noop_c2, e0());
  chunks.AddChunk(t0(), *noop_c2, *e1);

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({
                  cc::PaintOpType::Save, cc::PaintOpType::ClipRect,  // noop_c2
                  cc::PaintOpType::DrawRecord,      // draw with noop_c2
                  cc::PaintOpType::SaveLayerAlpha,  // e1
                  cc::PaintOpType::DrawRecord,      // draw with e1
                  cc::PaintOpType::Restore,         // end e1
                  cc::PaintOpType::Restore          // end noop_c2
              }));
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

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());

  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({
                  cc::PaintOpType::DrawRecord,      // e0
                  cc::PaintOpType::SaveLayerAlpha,  // e1
                  cc::PaintOpType::DrawRecord,      // draw with e1
                  cc::PaintOpType::DrawRecord,      // draw with noop_e2
                  cc::PaintOpType::DrawRecord,      // draw_with noop_e3
                  cc::PaintOpType::SaveLayerAlpha,  // e4
                  cc::PaintOpType::DrawRecord,      // draw with e4
                  cc::PaintOpType::Restore,         // end e4
                  cc::PaintOpType::DrawRecord,      // draw with noop_e2
                  cc::PaintOpType::DrawRecord,      // draw with e1
                  cc::PaintOpType::Restore          // end noop_e2 (or e1)
              }));
}

TEST_P(PaintChunksToCcLayerTest, EmptyChunkRect) {
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(5);
  auto e1 = CreateFilterEffect(e0(), t0(), &c0(), filter);
  TestChunks chunks;
  chunks.AddChunk(nullptr, t0(), c0(), *e1, {0, 0, 0, 0});

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({cc::PaintOpType::SaveLayer,   // <e1>
                                        cc::PaintOpType::Restore}));  // </e1>
  EXPECT_EFFECT_BOUNDS(0, 0, 0, 0, *output, 0);
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
  EXPECT_THAT(*output, PaintRecordMatcher::Make(
                           {cc::PaintOpType::Save,
                            cc::PaintOpType::Translate,  // layer offset
                            cc::PaintOpType::SaveLayer,  // <e1>
                            cc::PaintOpType::Restore,    // </e1>
                            cc::PaintOpType::Restore}));
  EXPECT_EFFECT_BOUNDS(12, 26, 93, 84, *output, 2);
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
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make(
                  {cc::PaintOpType::Save,
                   cc::PaintOpType::Translate,   // layer offset
                   cc::PaintOpType::SaveLayer,   // <e1>
                   cc::PaintOpType::DrawRecord,  // the DrawingDisplayItem
                   cc::PaintOpType::Restore,     // </e1>
                   cc::PaintOpType::Restore}));
  // The effect bounds are the union of the chunk's drawable_bounds and the
  // output bounds of the filter with empty input in the filter's space.
  EXPECT_EFFECT_BOUNDS(7, 15, 93, 85, *output, 2);
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

  auto output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());
  ASSERT_EQ(7u, output->total_op_count());
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make(
                  {cc::PaintOpType::SaveLayer,  // <e1>
                   cc::PaintOpType::Save,
                   cc::PaintOpType::ClipRect,    // <c1>
                   cc::PaintOpType::DrawRecord,  // the DrawingDisplayItem
                   cc::PaintOpType::Restore,     // </c1>
                   cc::PaintOpType::Restore}));  // </e1>
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

  PaintChunksToCcLayer::UpdateLayerProperties(*layer, PropertyTreeState::Root(),
                                              chunks.Build());

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

  PaintChunksToCcLayer::UpdateLayerProperties(*layer, PropertyTreeState::Root(),
                                              chunks.Build());

  const gfx::Rect actual_bounds =
      layer->capture_bounds().bounds().find(kCropId.value())->second;
  EXPECT_EQ((gfx::Rect{40, 45, 100, 200}), actual_bounds);
}

TEST_P(PaintChunksToCcLayerTest, UpdateLayerPropertiesRegionCaptureDataEmpty) {
  auto layer = cc::Layer::Create();
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e0(), gfx::Rect(5, 10, 200, 300),
                  gfx::Rect(10, 15, 20, 30));
  PaintChunksToCcLayer::UpdateLayerProperties(*layer, PropertyTreeState::Root(),
                                              chunks.Build());
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

  PaintChunksToCcLayer::UpdateLayerProperties(*layer, PropertyTreeState::Root(),
                                              chunks.Build());

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

  PaintChunksToCcLayer::UpdateLayerProperties(*layer, PropertyTreeState::Root(),
                                              chunks.Build());

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

  sk_sp<PaintRecord> output = PaintChunksToCcLayer::Convert(
      chunks.Build(), PropertyTreeState::Root(), gfx::Vector2dF());
  // TODO(crbug.com/1334293): For now non-composited backdrop filters are
  // ignored.
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({cc::PaintOpType::SaveLayerAlpha,
                                        cc::PaintOpType::DrawRecord,
                                        cc::PaintOpType::Restore}));
}

}  // namespace
}  // namespace blink
