// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"

#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/paint_recorder.h"
#include "cc/test/paint_op_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_begin_layer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasfilter_string.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
namespace {

using ::blink_testing::ParseFilter;
using ::cc::ClipPathOp;
using ::cc::ClipRectOp;
using ::cc::PaintOpEq;
using ::cc::RestoreOp;
using ::cc::SaveLayerAlphaOp;
using ::cc::SaveLayerOp;
using ::cc::SaveOp;
using ::cc::SetMatrixOp;
using ::cc::TranslateOp;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointee;

// A view of a `cc::PaintRecord` which drops the leading and trailing
// `SaveOp` and `RestoreOp` that are present in every single recordings.
class RecordedOpsView {
 public:
  explicit RecordedOpsView(cc::PaintRecord record)
      : record_(std::move(record)), begin_(record_.begin()), end_(begin_) {
    CHECK_GE(record_.size(), 2u);

    // The first `PaintOp` must be a `SaveOp`.
    EXPECT_THAT(*begin_, PaintOpEq<SaveOp>());

    // Move `begin_` to the second element, and `end_` to the last, so tthat
    // iterating between `begin_` and `end_` will skip the last element.
    ++begin_;
    for (size_t i = 0; i < record_.size() - 1; ++i) {
      ++end_;
    }

    // The last `PaintOp` must be a `RestoreOp`.
    EXPECT_THAT(*end_, PaintOpEq<RestoreOp>());
  }

  using value_type = cc::PaintOp;
  size_t size() const { return record_.size() - 2; }
  bool empty() const { return size() == 0; }
  cc::PaintOpBuffer::Iterator begin() const { return begin_; }
  cc::PaintOpBuffer::Iterator end() const { return end_; }

 private:
  cc::PaintRecord record_;
  cc::PaintOpBuffer::Iterator begin_;
  cc::PaintOpBuffer::Iterator end_;
};

// Test version of BaseRenderingContext2D. BaseRenderingContext2D can't be
// tested directly because it's an abstract class. This test class essentially
// just gives a definition to all pure virtual method, making it instantiable.
class TestRenderingContext2D final
    : public GarbageCollected<TestRenderingContext2D>,
      public BaseRenderingContext2D {
 public:
  explicit TestRenderingContext2D(V8TestingScope& scope)
      : BaseRenderingContext2D(
            scheduler::GetSingleThreadTaskRunnerForTesting()),
        execution_context_(scope.GetExecutionContext()) {
    recorder_.beginRecording(gfx::Size(1, 1));

    // Production code (see `CanvasResourceHost::InitializeForRecording()`)
    // initializes the matrix stack by calling `RestoreMatrixClipStack`. .
    RestoreMatrixClipStack(GetPaintCanvas());
  }
  ~TestRenderingContext2D() override = default;

  // Returns the content of the paint recorder, leaving it empty.
  cc::PaintRecord FlushRecorder() {
    cc::PaintRecord record = recorder_.finishRecordingAsPicture();
    recorder_.beginRecording(gfx::Size(1, 1));
    return record;
  }

  // Get the PaintOps recorded by the context and re-initialize it to be ready
  // to capture more ops. The top-level `SaveOp` and `RestoreOp` are not
  // included in the result since these are implementation details not relevant
  // to unit tests validating specific canvas APIs.
  RecordedOpsView GetRecordedOps() {
    cc::PaintRecord record = FlushRecorder();
    RestoreMatrixClipStack(GetPaintCanvas());
    return RecordedOpsView(std::move(record));
  }

  int StateStackDepth() {
    // Subtract the extra save that gets added when the context is initialized.
    return state_stack_.size() - 1;
  }

  int OpenedLayerCount() { return layer_count_; }

  bool OriginClean() const override { return true; }
  void SetOriginTainted() override {}

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

  ExecutionContext* GetTopExecutionContext() const override {
    return execution_context_;
  }

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
  void FlushCanvas(CanvasResourceProvider::FlushReason) override {}

  Member<ExecutionContext> execution_context_;
  cc::InspectablePaintRecorder recorder_;
  bool context_lost_ = false;
};

V8UnionCanvasFilterOrString* MakeBlurCanvasFilter(float std_deviation) {
  FilterOperations ops;
  ops.Operations().push_back(
      MakeGarbageCollected<BlurFilterOperation>(Length::Fixed(std_deviation)));

  return MakeGarbageCollected<V8UnionCanvasFilterOrString>(
      MakeGarbageCollected<CanvasFilter>(ops));
}

BeginLayerOptions* FilterOption(blink::V8TestingScope& scope,
                                const std::string& filter) {
  BeginLayerOptions* options = BeginLayerOptions::Create();
  options->setFilter(ParseFilter(scope, filter));
  return options;
}

TEST(BaseRenderingContextLayerTests, ContextLost) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->SetContextLost(true);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  EXPECT_THAT(context->GetRecordedOps(), IsEmpty());
}

TEST(BaseRenderingContextLayerTests, ResetsAndRestoresShadowStates) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setShadowBlur(1.0);
  context->setShadowOffsetX(2.0);
  context->setShadowOffsetY(3.0);
  context->setShadowColor("red");

  EXPECT_EQ(context->shadowBlur(), 1.0);
  EXPECT_EQ(context->shadowOffsetX(), 2.0);
  EXPECT_EQ(context->shadowOffsetY(), 3.0);
  EXPECT_EQ(context->shadowColor(), "#ff0000");

  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);

  EXPECT_EQ(context->shadowBlur(), 0.0);
  EXPECT_EQ(context->shadowOffsetX(), 0.0);
  EXPECT_EQ(context->shadowOffsetY(), 0.0);
  EXPECT_EQ(context->shadowColor(), "rgba(0, 0, 0, 0)");

  context->endLayer(exception_state);

  EXPECT_EQ(context->shadowBlur(), 1.0);
  EXPECT_EQ(context->shadowOffsetX(), 2.0);
  EXPECT_EQ(context->shadowOffsetY(), 3.0);
  EXPECT_EQ(context->shadowColor(), "#ff0000");
}

TEST(BaseRenderingContextLayerTests, ResetsAndRestoresCompositeStates) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalAlpha(0.7);
  context->setGlobalCompositeOperation("xor");

  EXPECT_EQ(context->globalAlpha(), 0.7);
  EXPECT_EQ(context->globalCompositeOperation(), "xor");

  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);

  EXPECT_EQ(context->globalAlpha(), 1.0);
  EXPECT_EQ(context->globalCompositeOperation(), "source-over");

  context->endLayer(exception_state);

  EXPECT_EQ(context->globalAlpha(), 0.7);
  EXPECT_EQ(context->globalCompositeOperation(), "xor");
}

TEST(BaseRenderingContextLayerTests, ResetsAndRestoresFilterStates) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  V8UnionCanvasFilterOrString* filter = MakeBlurCanvasFilter(20.0f);
  context->setFilter(scope.GetScriptState(), filter);

  ASSERT_TRUE(context->filter()->IsCanvasFilter());
  EXPECT_EQ(context->filter()->GetAsCanvasFilter()->Operations(),
            filter->GetAsCanvasFilter()->Operations());
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  ASSERT_TRUE(context->filter()->IsCanvasFilter());
  EXPECT_EQ(context->filter()->GetAsCanvasFilter()->Operations(),
            filter->GetAsCanvasFilter()->Operations());

  context->endLayer(exception_state);

  ASSERT_TRUE(context->filter()->IsCanvasFilter());
  EXPECT_EQ(context->filter()->GetAsCanvasFilter()->Operations(),
            filter->GetAsCanvasFilter()->Operations());
}

TEST(BaseRenderingContextLayerTests, BeginLayerThrowsOnInvalidFilterParam) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'colorMatrix', values: 'invalid'})"),
      scope.GetExceptionState());

  EXPECT_EQ(scope.GetExceptionState().CodeAs<ESErrorType>(),
            ESErrorType::kTypeError);
  // `beginLayer` should be a no-op if an exception is raised.
  EXPECT_EQ(context->StateStackDepth(), 0);
  EXPECT_EQ(context->OpenedLayerCount(), 0);
}

TEST(BaseRenderingContextLayerGlobalStateTests, DefaultRenderingStates) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(PaintOpEq<SaveLayerAlphaOp>(1.0f), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, GlobalAlpha) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalAlpha(0.3);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(PaintOpEq<SaveLayerAlphaOp>(0.3f), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, BlendingOperation) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalCompositeOperation("multiply");
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kMultiply);

  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(PaintOpEq<SaveLayerOp>(flags), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, CompositeOperation) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalCompositeOperation("source-in");
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrcIn);

  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(PaintOpEq<SaveLayerOp>(flags), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, Shadow) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags flags;
  flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));
  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(PaintOpEq<SaveLayerOp>(flags), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, GlobalAlphaAndBlending) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalAlpha(0.3);
  context->setGlobalCompositeOperation("multiply");
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags flags;
  flags.setAlphaf(0.3f);
  flags.setBlendMode(SkBlendMode::kMultiply);

  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(PaintOpEq<SaveLayerOp>(flags), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, GlobalAlphaAndComposite) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalAlpha(0.3);
  context->setGlobalCompositeOperation("source-in");
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(composite_flags),
                          PaintOpEq<SaveLayerAlphaOp>(0.3f),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, GlobalAlphaAndShadow) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setGlobalAlpha(0.5);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<SaveLayerAlphaOp>(0.5f),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, GlobalAlphaBlendingAndShadow) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setGlobalAlpha(0.5);
  context->setGlobalCompositeOperation("multiply");
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));
  shadow_flags.setBlendMode(SkBlendMode::kMultiply);

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<SaveLayerAlphaOp>(0.5f),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, GlobalAlphaCompositeAndShadow) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setGlobalAlpha(0.5);
  context->setGlobalCompositeOperation("source-in");
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));
  shadow_flags.setBlendMode(SkBlendMode::kSrcIn);

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<SaveLayerAlphaOp>(0.5f),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, BlendingAndShadow) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setGlobalCompositeOperation("multiply");
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));
  shadow_flags.setBlendMode(SkBlendMode::kMultiply);

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, CompositeAndShadow) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setGlobalCompositeOperation("source-in");
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));
  shadow_flags.setBlendMode(SkBlendMode::kSrcIn);

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, Filter) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 10})"),
      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags flags;
  flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(10.0f, 10.0f, SkTileMode::kDecal, nullptr));
  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(PaintOpEq<SaveLayerOp>(flags), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterAndGlobalAlpha) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalAlpha(0.3);
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags flags;
  flags.setAlphaf(0.3f);
  flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));
  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(PaintOpEq<SaveLayerOp>(flags), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterAndBlending) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalCompositeOperation("multiply");
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags flags;
  flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));
  flags.setBlendMode(SkBlendMode::kMultiply);
  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(PaintOpEq<SaveLayerOp>(flags), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterAndComposite) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalCompositeOperation("source-in");
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  cc::PaintFlags filter_flags;
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(composite_flags),
                          PaintOpEq<SaveLayerOp>(filter_flags),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterAndShadow) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));

  cc::PaintFlags filter_flags;
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<SaveLayerOp>(filter_flags),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterGlobalAlphaAndBlending) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalAlpha(0.3);
  context->setGlobalCompositeOperation("multiply");
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags flags;
  flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));
  flags.setAlphaf(0.3f);
  flags.setBlendMode(SkBlendMode::kMultiply);
  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(PaintOpEq<SaveLayerOp>(flags), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterGlobalAlphaAndComposite) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalAlpha(0.3);
  context->setGlobalCompositeOperation("source-in");
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  cc::PaintFlags filter_flags;
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));
  filter_flags.setAlphaf(0.3f);

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(composite_flags),
                          PaintOpEq<SaveLayerOp>(filter_flags),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterGlobalAlphaAndShadow) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalAlpha(0.4);
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));

  cc::PaintFlags filter_flags;
  filter_flags.setAlphaf(0.4f);
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<SaveLayerOp>(filter_flags),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests,
     FilterGlobalAlphaBlendingAndShadow) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalAlpha(0.4);
  context->setGlobalCompositeOperation("multiply");
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setBlendMode(SkBlendMode::kMultiply);
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));

  cc::PaintFlags filter_flags;
  filter_flags.setAlphaf(0.4f);
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<SaveLayerOp>(filter_flags),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests,
     FilterGlobalAlphaCompositeAndShadow) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalAlpha(0.4);
  context->setGlobalCompositeOperation("source-in");
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setBlendMode(SkBlendMode::kSrcIn);
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));

  cc::PaintFlags filter_flags;
  filter_flags.setAlphaf(0.4f);
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<SaveLayerOp>(filter_flags),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterBlendingAndShadow) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalCompositeOperation("multiply");
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setBlendMode(SkBlendMode::kMultiply);
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));

  cc::PaintFlags filter_flags;
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<SaveLayerOp>(filter_flags),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterCompositeAndShadow) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalCompositeOperation("source-in");
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setBlendMode(SkBlendMode::kSrcIn);
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));

  cc::PaintFlags filter_flags;
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<SaveLayerOp>(filter_flags),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextLayerGlobalStateTests, BeginLayerIgnoresGlobalFilter) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  V8UnionCanvasFilterOrString* filter = MakeBlurCanvasFilter(20.0f);
  context->setFilter(scope.GetScriptState(), filter);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);

  ASSERT_TRUE(context->filter()->IsCanvasFilter());
  EXPECT_EQ(context->filter()->GetAsCanvasFilter()->Operations(),
            filter->GetAsCanvasFilter()->Operations());

  context->endLayer(exception_state);

  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(PaintOpEq<SaveLayerAlphaOp>(1.0f), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextRestoreStackTests, RestoresSaves) {
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->save();
  context->save();
  context->save();

  EXPECT_THAT(RecordedOpsView(context->FlushRecorder()),
              ElementsAre(PaintOpEq<SaveOp>(), PaintOpEq<SaveOp>(),
                          PaintOpEq<SaveOp>(), PaintOpEq<RestoreOp>(),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));

  // `FlushRecorder()` flushed the recording canvas, leaving it empty.
  ASSERT_THAT(context->FlushRecorder(), IsEmpty());

  context->RestoreMatrixClipStack(context->GetPaintCanvas());
  context->restore(exception_state);
  context->restore(exception_state);
  context->restore(exception_state);

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveOp>(), PaintOpEq<SaveOp>(),
                          PaintOpEq<SaveOp>(), PaintOpEq<RestoreOp>(),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextRestoreStackTests, RestoresTransforms) {
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->translate(10.0, 0.0);
  context->translate(0.0, 20.0);
  context->save();  // No transforms to restore on that level.
  context->save();
  context->translate(15.0, 15.0);

  EXPECT_THAT(
      RecordedOpsView(context->FlushRecorder()),
      ElementsAre(PaintOpEq<TranslateOp>(10.0, 0.0),  // Root transforms.
                  PaintOpEq<TranslateOp>(0.0, 20.0),
                  PaintOpEq<SaveOp>(),  // Nested state without transform.
                  PaintOpEq<SaveOp>(),  // Nested state with transform.
                  PaintOpEq<TranslateOp>(15.0, 15.0), PaintOpEq<RestoreOp>(),
                  PaintOpEq<RestoreOp>()));

  // `FlushRecorder()` flushed the recording canvas, leaving it empty.
  ASSERT_THAT(context->FlushRecorder(), IsEmpty());

  context->RestoreMatrixClipStack(context->GetPaintCanvas());
  context->restore(exception_state);

  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(
          // Root transforms.
          PaintOpEq<SetMatrixOp>(SkM44(1.f, 0.f, 0.f, 10.f, 0.f, 1.f, 0.f, 20.f,
                                       0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f)),
          PaintOpEq<SaveOp>(),  // Nested state without transform.
          PaintOpEq<SaveOp>(),  // Nested state with transform.
          PaintOpEq<SetMatrixOp>(SkM44(1.f, 0.f, 0.f, 25.f, 0.f, 1.f, 0.f, 35.f,
                                       0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f)),
          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextRestoreStackTests, RestoresClip) {
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  // Clipping from an empty matrix stack. Clip can be restored without having
  // to reset the transform.
  context->beginPath();
  context->rect(0, 0, 100, 100);
  context->clip();

  // Clipping from a nested identity transform. Clip can be restored without
  // having to reset the transform.
  context->save();
  context->translate(10.0, 0.0);
  context->beginPath();
  context->moveTo(100, 100);
  context->lineTo(200, 100);
  context->translate(0.0, 20.0);
  context->lineTo(150, 200);
  context->clip();
  context->translate(15.0, 15.0);

  // Clip nested in a parent transform, restoring clip will require resetting
  // the transform to identity.
  context->save();
  context->translate(3.0, 0.0);
  context->beginPath();
  context->moveTo(150, 50);
  context->lineTo(200, 200);
  context->translate(0.0, 3.0);
  context->lineTo(100, 200);
  context->clip();

  EXPECT_THAT(
      RecordedOpsView(context->FlushRecorder()),
      ElementsAre(
          // Root clip, but no transform.
          PaintOpEq<ClipRectOp>(SkRect::MakeLTRB(0, 0, 100, 100),
                                SkClipOp::kIntersect,
                                /*antialias=*/false),
          PaintOpEq<SaveOp>(),  // Nested state with clip and transforms.
          PaintOpEq<TranslateOp>(10.0, 0.0), PaintOpEq<TranslateOp>(0.0, 20.0),
          PaintOpEq<ClipPathOp>(
              SkPath::Polygon({{100, 80}, {200, 80}, {150, 200}},
                              /*isClosed=*/false),
              SkClipOp::kIntersect, /*antialias=*/false,
              /*use_paint_cache=*/UsePaintCache::kDisabled),
          PaintOpEq<TranslateOp>(15.0, 15.0),
          PaintOpEq<SaveOp>(),  // Second nested clip.
          PaintOpEq<TranslateOp>(3.0, 0.0), PaintOpEq<TranslateOp>(0.0, 3.0),
          PaintOpEq<ClipPathOp>(
              SkPath::Polygon({{150, 47}, {200, 197}, {100, 200}},
                              /*isClosed=*/false),
              SkClipOp::kIntersect, /*antialias=*/false,
              /*use_paint_cache=*/UsePaintCache::kDisabled),
          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));

  // `FlushRecorder()` flushed the recording canvas, leaving it empty.
  ASSERT_THAT(context->FlushRecorder(), IsEmpty());

  context->RestoreMatrixClipStack(context->GetPaintCanvas());
  context->restore(exception_state);

  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(
          // Empty matrix stack, no need to reset matrix before setting clip.
          PaintOpEq<ClipRectOp>(SkRect::MakeLTRB(0, 0, 100, 100),
                                SkClipOp::kIntersect,
                                /*antialias=*/false),
          // Current transform is identity, no need to reset matrix either.
          PaintOpEq<SaveOp>(),
          PaintOpEq<ClipPathOp>(
              SkPath::Polygon({{110, 100}, {210, 100}, {160, 220}},
                              /*isClosed=*/false),
              SkClipOp::kIntersect, /*antialias=*/false,
              /*use_paint_cache=*/UsePaintCache::kDisabled),
          PaintOpEq<SetMatrixOp>(SkM44(1.f, 0.f, 0.f, 25.f, 0.f, 1.f, 0.f, 35.f,
                                       0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f)),
          // Current transform is not identity, need to reset matrix.
          PaintOpEq<SaveOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                                       0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f)),
          PaintOpEq<ClipPathOp>(
              SkPath::Polygon({{178, 85}, {228, 235}, {128, 238}},
                              /*isClosed=*/false),
              SkClipOp::kIntersect, /*antialias=*/false,
              /*use_paint_cache=*/UsePaintCache::kDisabled),
          PaintOpEq<SetMatrixOp>(SkM44(1.f, 0.f, 0.f, 28.f, 0.f, 1.f, 0.f, 38.f,
                                       0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f)),
          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextRestoreStackTests, RestoresLayers) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalAlpha(0.4);
  context->setGlobalCompositeOperation("source-in");
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setBlendMode(SkBlendMode::kSrcIn);
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));

  cc::PaintFlags filter_flags;
  filter_flags.setAlphaf(0.4f);
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(RecordedOpsView(context->FlushRecorder()),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<SaveLayerOp>(filter_flags),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));

  // `FlushRecorder()` flushed the recording canvas, leaving it empty.
  ASSERT_THAT(context->FlushRecorder(), IsEmpty());

  context->RestoreMatrixClipStack(context->GetPaintCanvas());
  context->endLayer(exception_state);

  EXPECT_THAT(context->GetRecordedOps(),
              ElementsAre(PaintOpEq<SaveLayerOp>(shadow_flags),
                          PaintOpEq<SaveLayerOp>(filter_flags),
                          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextResetTest, DiscardsRenderStates) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setGlobalAlpha(0.5);
  context->setGlobalCompositeOperation("source-in");
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);

  // Discard the rendering states:
  context->reset();
  // Discard the recording:
  EXPECT_THAT(context->GetRecordedOps(), Not(IsEmpty()));
  // The recording should now be empty:
  ASSERT_THAT(context->GetRecordedOps(), IsEmpty());

  // Do some operation and check that the rendering state was reset:
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  EXPECT_THAT(
      context->GetRecordedOps(),
      ElementsAre(PaintOpEq<SaveLayerAlphaOp>(1.0f), PaintOpEq<RestoreOp>()));
  EXPECT_EQ(context->StateStackDepth(), 1);
  EXPECT_EQ(context->OpenedLayerCount(), 1);
}

TEST(BaseRenderingContextLayersCallOrderTests, LoneBeginLayer) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(context->StateStackDepth(), 1);
  EXPECT_EQ(context->OpenedLayerCount(), 1);
}

TEST(BaseRenderingContextLayersCallOrderTests, LoneRestore) {
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->restore(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(context->StateStackDepth(), 0);
  EXPECT_EQ(context->OpenedLayerCount(), 0);
}

TEST(BaseRenderingContextLayersCallOrderTests, LoneEndLayer) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->endLayer(scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
  EXPECT_EQ(context->StateStackDepth(), 0);
  EXPECT_EQ(context->OpenedLayerCount(), 0);
}

TEST(BaseRenderingContextLayersCallOrderTests, SaveRestore) {
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->save();
  context->restore(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(context->StateStackDepth(), 0);
  EXPECT_EQ(context->OpenedLayerCount(), 0);
}

TEST(BaseRenderingContextLayersCallOrderTests, SaveResetRestore) {
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->save();
  context->reset();
  context->restore(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(context->StateStackDepth(), 0);
  EXPECT_EQ(context->OpenedLayerCount(), 0);
}

TEST(BaseRenderingContextLayersCallOrderTests, BeginLayerEndLayer) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  context->endLayer(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(context->StateStackDepth(), 0);
  EXPECT_EQ(context->OpenedLayerCount(), 0);
}

TEST(BaseRenderingContextLayersCallOrderTests, BeginLayerResetEndLayer) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  context->reset();
  context->endLayer(scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
  EXPECT_EQ(context->StateStackDepth(), 0);
  EXPECT_EQ(context->OpenedLayerCount(), 0);
}

TEST(BaseRenderingContextLayersCallOrderTests, SaveBeginLayer) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->save();
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(context->StateStackDepth(), 2);
  EXPECT_EQ(context->OpenedLayerCount(), 1);
}

TEST(BaseRenderingContextLayersCallOrderTests, SaveEndLayer) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->save();
  context->endLayer(scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
  EXPECT_EQ(context->StateStackDepth(), 1);
  EXPECT_EQ(context->OpenedLayerCount(), 0);
}

TEST(BaseRenderingContextLayersCallOrderTests, BeginLayerSave) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  context->save();
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(context->StateStackDepth(), 2);
  EXPECT_EQ(context->OpenedLayerCount(), 1);
}

TEST(BaseRenderingContextLayersCallOrderTests, BeginLayerRestore) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  context->restore(scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
  EXPECT_EQ(context->StateStackDepth(), 1);
  EXPECT_EQ(context->OpenedLayerCount(), 1);
}

TEST(BaseRenderingContextLayersCallOrderTests, SaveBeginLayerRestore) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->save();
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  context->restore(scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
  EXPECT_EQ(context->StateStackDepth(), 2);
  EXPECT_EQ(context->OpenedLayerCount(), 1);
}

TEST(BaseRenderingContextLayersCallOrderTests, BeginLayerSaveEndLayer) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  context->save();
  context->endLayer(scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
  EXPECT_EQ(context->StateStackDepth(), 2);
  EXPECT_EQ(context->OpenedLayerCount(), 1);
}

}  // namespace
}  // namespace blink
