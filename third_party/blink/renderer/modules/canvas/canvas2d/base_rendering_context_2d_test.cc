// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"

#include "base/memory/scoped_refptr.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/paint_recorder.h"
#include "cc/test/paint_op_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasfilter_string.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
namespace {

using ::cc::PaintOpEq;
using ::cc::RestoreOp;
using ::cc::SaveLayerAlphaOp;
using ::cc::SaveLayerOp;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pointee;

// Test version of BaseRenderingContext2D. BaseRenderingContext2D can't be
// tested directly because it's an abstract class. This test class essentially
// just gives a definition to all pure virtual method, making it instantiable.
class TestRenderingContext2D final
    : public GarbageCollected<TestRenderingContext2D>,
      public BaseRenderingContext2D {
  USING_PRE_FINALIZER(TestRenderingContext2D, Dispose);

 public:
  explicit TestRenderingContext2D()
      : BaseRenderingContext2D(
            scheduler::GetSingleThreadTaskRunnerForTesting()),
        execution_context_(MakeGarbageCollected<NullExecutionContext>()) {
    recorder_.beginRecording();
  }
  ~TestRenderingContext2D() override = default;

  void Dispose() { execution_context_->NotifyContextDestroyed(); }

  cc::PaintRecord getRecording() {
    return recorder_.finishRecordingAsPicture();
  }

  bool OriginClean() const override { return true; }
  void SetOriginTainted() override {}
  bool WouldTaintOrigin(CanvasImageSource*) override { return false; }

  int Width() const override { return 300; }
  int Height() const override { return 300; }

  bool CanCreateCanvas2dResourceProvider() const override { return false; }

  RespectImageOrientationEnum RespectImageOrientation() const override {
    return kRespectImageOrientation;
  }

  Color GetCurrentColor() const override { return Color::kBlack; }

  cc::PaintCanvas* GetOrCreatePaintCanvas() override {
    return recorder_.getRecordingCanvas();
  }
  cc::PaintCanvas* GetPaintCanvas() override {
    return recorder_.getRecordingCanvas();
  }
  void WillDraw(const SkIRect& dirty_rect,
                CanvasPerformanceMonitor::DrawType) override {}

  sk_sp<PaintFilter> StateGetFilter() override {
    return GetState().GetFilterForOffscreenCanvas({}, this);
  }
  void SnapshotStateForFilter() override {}
  ExecutionContext* GetTopExecutionContext() const override {
    return execution_context_.Get();
  }
  void ValidateStateStackWithCanvas(const cc::PaintCanvas*) const override {}

  bool HasAlpha() const override { return false; }

  void SetContextLost(bool context_lost) { context_lost_ = context_lost; }
  bool isContextLost() const override { return context_lost_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(execution_context_);
    BaseRenderingContext2D::Trace(visitor);
  }

 protected:
  PredefinedColorSpace GetDefaultImageDataColorSpace() const override {
    return PredefinedColorSpace::kSRGB;
  }

  void WillOverwriteCanvas() override {}

 private:
  void FlushCanvas() override {}

  Member<NullExecutionContext> execution_context_;
  cc::PaintRecorder recorder_;
  bool context_lost_ = false;
};

V8UnionCanvasFilterOrString* MakeBlurCanvasFilter(float std_deviation) {
  FilterOperations ops;
  ops.Operations().push_back(
      MakeGarbageCollected<BlurFilterOperation>(Length::Fixed(std_deviation)));

  return MakeGarbageCollected<V8UnionCanvasFilterOrString>(
      MakeGarbageCollected<CanvasFilter>(ops));
}

TEST(BaseRenderingContextLayerTests, ContextLost) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();
  context->SetContextLost(true);
  context->beginLayer();
  context->endLayer();

  EXPECT_THAT(context->getRecording(), IsEmpty());
}

TEST(BaseRenderingContextLayerTests, ResetsAndRestoresShadowStates) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();
  context->setShadowBlur(1.0);
  context->setShadowOffsetX(2.0);
  context->setShadowOffsetY(3.0);
  context->setShadowColor("red");

  EXPECT_EQ(context->shadowBlur(), 1.0);
  EXPECT_EQ(context->shadowOffsetX(), 2.0);
  EXPECT_EQ(context->shadowOffsetY(), 3.0);
  EXPECT_EQ(context->shadowColor(), "#ff0000");

  context->beginLayer();

  EXPECT_EQ(context->shadowBlur(), 0.0);
  EXPECT_EQ(context->shadowOffsetX(), 0.0);
  EXPECT_EQ(context->shadowOffsetY(), 0.0);
  EXPECT_EQ(context->shadowColor(), "rgba(0, 0, 0, 0)");

  context->endLayer();

  EXPECT_EQ(context->shadowBlur(), 1.0);
  EXPECT_EQ(context->shadowOffsetX(), 2.0);
  EXPECT_EQ(context->shadowOffsetY(), 3.0);
  EXPECT_EQ(context->shadowColor(), "#ff0000");
}

TEST(BaseRenderingContextLayerTests, ResetsAndRestoresCompositeStates) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();
  context->setGlobalAlpha(0.7);
  context->setGlobalCompositeOperation("xor");

  EXPECT_EQ(context->globalAlpha(), 0.7);
  EXPECT_EQ(context->globalCompositeOperation(), "xor");

  context->beginLayer();

  EXPECT_EQ(context->globalAlpha(), 1.0);
  EXPECT_EQ(context->globalCompositeOperation(), "source-over");

  context->endLayer();

  EXPECT_EQ(context->globalAlpha(), 0.7);
  EXPECT_EQ(context->globalCompositeOperation(), "xor");
}

TEST(BaseRenderingContextLayerTests, ResetsAndRestoresFilterStates) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();

  V8UnionCanvasFilterOrString* filter = MakeBlurCanvasFilter(20.0f);
  context->setFilter(context->GetTopExecutionContext(), filter);

  ASSERT_TRUE(context->filter()->IsCanvasFilter());
  EXPECT_EQ(context->filter()->GetAsCanvasFilter()->Operations(),
            filter->GetAsCanvasFilter()->Operations());

  context->beginLayer();

  ASSERT_TRUE(context->filter()->IsString());
  EXPECT_EQ(context->filter()->GetAsString(), "none");

  context->endLayer();

  ASSERT_TRUE(context->filter()->IsCanvasFilter());
  EXPECT_EQ(context->filter()->GetAsCanvasFilter()->Operations(),
            filter->GetAsCanvasFilter()->Operations());
}

TEST(BaseRenderingContextLayerTests, DefaultRenderingStates) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();
  context->beginLayer();
  context->endLayer();

  EXPECT_THAT(
      context->getRecording(),
      ElementsAre(PaintOpEq<SaveLayerAlphaOp>(1.0f), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerTests, GlobalAlpha) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();

  context->setGlobalAlpha(0.3);
  context->beginLayer();
  context->endLayer();

  EXPECT_THAT(
      context->getRecording(),
      ElementsAre(PaintOpEq<SaveLayerAlphaOp>(0.3f), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerTests, GlobalComposite) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();

  context->setGlobalCompositeOperation("xor");
  context->beginLayer();
  context->endLayer();

  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kXor);

  EXPECT_THAT(
      context->getRecording(),
      ElementsAre(PaintOpEq<SaveLayerOp>(flags), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerTests, GlobalAlphaAndComposite) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();

  context->setGlobalAlpha(0.3);
  context->setGlobalCompositeOperation("xor");
  context->beginLayer();
  context->endLayer();

  cc::PaintFlags flags;
  flags.setAlphaf(0.3f);
  flags.setBlendMode(SkBlendMode::kXor);

  EXPECT_THAT(
      context->getRecording(),
      ElementsAre(PaintOpEq<SaveLayerOp>(flags), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerTests, Shadow) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();

  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->beginLayer();
  context->endLayer();

  cc::PaintFlags flags;
  flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));
  EXPECT_THAT(
      context->getRecording(),
      ElementsAre(PaintOpEq<SaveLayerOp>(flags), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerTests, ShadowAndGlobalAlpha) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();

  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setGlobalAlpha(0.5);
  context->beginLayer();
  context->endLayer();

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));

  EXPECT_THAT(context->getRecording(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<SaveLayerAlphaOp>(0.5f),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerTests, ShadowGlobalAlphaAndComposite) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();

  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setGlobalAlpha(0.5);
  context->setGlobalCompositeOperation("xor");
  context->beginLayer();
  context->endLayer();

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));
  shadow_flags.setBlendMode(SkBlendMode::kXor);

  EXPECT_THAT(context->getRecording(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<SaveLayerAlphaOp>(0.5f),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerTests, Filter) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();

  context->setFilter(context->GetTopExecutionContext(),
                     MakeBlurCanvasFilter(10.0f));
  context->beginLayer();
  context->endLayer();

  cc::PaintFlags flags;
  flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(10.0f, 10.0f, SkTileMode::kDecal, nullptr));
  EXPECT_THAT(
      context->getRecording(),
      ElementsAre(PaintOpEq<SaveLayerOp>(flags), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerTests, FilterAndGlobalAlpha) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();

  context->setFilter(context->GetTopExecutionContext(),
                     MakeBlurCanvasFilter(20.0f));
  context->setGlobalAlpha(0.3);
  context->beginLayer();
  context->endLayer();

  cc::PaintFlags flags;
  flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));
  EXPECT_THAT(context->getRecording(),
              ElementsAre(PaintOpEq<SaveLayerOp>(flags),
                          PaintOpEq<SaveLayerAlphaOp>(0.3f),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerTests, FilterGlobalAlphaAndComposite) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();

  context->setFilter(context->GetTopExecutionContext(),
                     MakeBlurCanvasFilter(20.0f));
  context->setGlobalAlpha(0.3);
  context->setGlobalCompositeOperation("xor");
  context->beginLayer();
  context->endLayer();

  cc::PaintFlags flags;
  flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));
  flags.setBlendMode(SkBlendMode::kXor);
  EXPECT_THAT(context->getRecording(),
              ElementsAre(PaintOpEq<SaveLayerOp>(flags),
                          PaintOpEq<SaveLayerAlphaOp>(0.3f),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerTests, FilterAndShadow) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();

  context->setFilter(context->GetTopExecutionContext(),
                     MakeBlurCanvasFilter(20.0f));
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->beginLayer();
  context->endLayer();

  cc::PaintFlags flags;
  flags.setImageFilter(sk_make_sp<ComposePaintFilter>(
      sk_make_sp<DropShadowPaintFilter>(
          0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
          DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr),
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr)));

  EXPECT_THAT(
      context->getRecording(),
      ElementsAre(PaintOpEq<SaveLayerOp>(flags), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerTests, FilterShadowAndGlobalAlpha) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();

  context->setFilter(context->GetTopExecutionContext(),
                     MakeBlurCanvasFilter(20.0f));
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setGlobalAlpha(0.2f);
  context->beginLayer();
  context->endLayer();

  cc::PaintFlags flags;
  flags.setImageFilter(sk_make_sp<ComposePaintFilter>(
      sk_make_sp<DropShadowPaintFilter>(
          0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
          DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr),
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr)));

  EXPECT_THAT(context->getRecording(),
              ElementsAre(PaintOpEq<SaveLayerOp>(flags),
                          PaintOpEq<SaveLayerAlphaOp>(0.2f),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerTests, FilterShadowGlobalAlphaAndComposite) {
  auto* context = MakeGarbageCollected<TestRenderingContext2D>();

  context->setFilter(context->GetTopExecutionContext(),
                     MakeBlurCanvasFilter(20.0f));
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setGlobalAlpha(0.2f);
  context->setGlobalCompositeOperation("xor");
  context->beginLayer();
  context->endLayer();

  cc::PaintFlags flags;
  flags.setImageFilter(sk_make_sp<ComposePaintFilter>(
      sk_make_sp<DropShadowPaintFilter>(
          0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
          DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr),
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr)));
  flags.setBlendMode(SkBlendMode::kXor);

  EXPECT_THAT(context->getRecording(),
              ElementsAre(PaintOpEq<SaveLayerOp>(flags),
                          PaintOpEq<SaveLayerAlphaOp>(0.2f),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

}  // namespace
}  // namespace blink
