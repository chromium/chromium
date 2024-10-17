// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_shader.h"
#include "cc/paint/refcounted_buffer.h"
#include "cc/test/paint_op_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_begin_layer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasfilter_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/recording_test_utils.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/style/filter_operation.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_test_utils.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/mesh_2d_index_buffer.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/mesh_2d_uv_buffer.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/mesh_2d_vertex_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/draw_looper_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_canvas.h"  // IWYU pragma: keep (https://github.com/clangd/clangd/issues/2044)
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/graphics/pattern.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTileMode.h"
#include "third_party/skia/include/private/base/SkPoint_impl.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
namespace {

using ::blink_testing::ClearRectFlags;
using ::blink_testing::FillFlags;
using ::blink_testing::ParseFilter;
using ::blink_testing::RecordedOpsAre;
using ::blink_testing::RecordedOpsView;
using ::cc::ClipPathOp;
using ::cc::ClipRectOp;
using ::cc::ConcatOp;
using ::cc::DrawColorOp;
using ::cc::DrawImageRectOp;
using ::cc::DrawRectOp;
using ::cc::DrawVerticesOp;
using ::cc::PaintFlags;
using ::cc::PaintImage;
using ::cc::PaintOpEq;
using ::cc::PaintOpIs;
using ::cc::PaintShader;
using ::cc::RestoreOp;
using ::cc::SaveLayerAlphaOp;
using ::cc::SaveLayerFiltersOp;
using ::cc::SaveLayerOp;
using ::cc::SaveOp;
using ::cc::ScaleOp;
using ::cc::SetMatrixOp;
using ::cc::TranslateOp;
using ::cc::UsePaintCache;
using ::testing::IsEmpty;

// Test version of BaseRenderingContext2D. BaseRenderingContext2D can't be
// tested directly because it's an abstract class. This test class essentially
// just gives a definition to all pure virtual method, making it instantiable.
class TestRenderingContext2D final
    : public GarbageCollected<TestRenderingContext2D>,
      public BaseRenderingContext2D,
      public MemoryManagedPaintRecorder::Client {
 public:
  explicit TestRenderingContext2D(V8TestingScope& scope)
      : BaseRenderingContext2D(
            scheduler::GetSingleThreadTaskRunnerForTesting()),
        execution_context_(scope.GetExecutionContext()),
        recorder_(gfx::Size(Width(), Height()), this),
        host_canvas_element_(nullptr) {}
  ~TestRenderingContext2D() override = default;

  // Returns the content of the paint recorder, leaving it empty.
  cc::PaintRecord FlushRecorder() { return recorder_.ReleaseMainRecording(); }

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
    return &recorder_.getRecordingCanvas();
  }
  using BaseRenderingContext2D::GetPaintCanvas;  // Pull the non-const overload.
  const cc::PaintCanvas* GetPaintCanvas() const override {
    return &recorder_.getRecordingCanvas();
  }
  void WillDraw(const SkIRect& dirty_rect,
                CanvasPerformanceMonitor::DrawType) override {}

  sk_sp<PaintFilter> StateGetFilter() override {
    return GetState().GetFilterForOffscreenCanvas({}, this);
  }

  ExecutionContext* GetTopExecutionContext() const override {
    return execution_context_.Get();
  }

  bool HasAlpha() const override { return true; }

  void SetContextLost(bool context_lost) { context_lost_ = context_lost; }
  bool isContextLost() const override { return context_lost_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(execution_context_);
    visitor->Trace(host_canvas_element_);
    BaseRenderingContext2D::Trace(visitor);
  }

  void SetRestoreMatrixEnabled(bool enabled) {
    restore_matrix_enabled_ = enabled;
  }

  void SetHostHTMLCanvas(HTMLCanvasElement* host_canvas_element) {
    host_canvas_element_ = host_canvas_element;
  }

 protected:
  PredefinedColorSpace GetDefaultImageDataColorSpace() const override {
    return PredefinedColorSpace::kSRGB;
  }

  HTMLCanvasElement* HostAsHTMLCanvasElement() const override {
    return host_canvas_element_;
  }

 private:
  void InitializeForRecording(cc::PaintCanvas* canvas) const override {
    if (restore_matrix_enabled_) {
      RestoreMatrixClipStack(canvas);
    }
  }
  void RecordingCleared() override {}

  std::optional<cc::PaintRecord> FlushCanvas(FlushReason) override {
    return recorder_.ReleaseMainRecording();
  }

  const MemoryManagedPaintRecorder* Recorder() const override {
    return &recorder_;
  }

  bool ResolveFont(const String& new_font) override {
    if (host_canvas_element_ == nullptr) {
      return false;
    }
    auto* style = CSSParser::ParseFont(new_font, execution_context_);
    if (style == nullptr) {
      return false;
    }
    FontDescription font_description = FontStyleResolver::ComputeFont(
        *style, host_canvas_element_->GetFontSelector());
    GetState().SetFont(font_description,
                       host_canvas_element_->GetFontSelector());
    return true;
  }

  Member<ExecutionContext> execution_context_;
  bool restore_matrix_enabled_ = true;
  bool context_lost_ = false;
  MemoryManagedPaintRecorder recorder_;
  Member<HTMLCanvasElement> host_canvas_element_;
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

// Tests a plain fillRect.
TEST(BaseRenderingContextCompositingTests, FillRect) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->translate(4, 5);
  context->fillRect(1, 1, 5, 5);

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<TranslateOp>(4, 5),
                             PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5),
                                                   FillFlags())));
}

// Tests a fillRect with a CanvasPattern.
TEST(BaseRenderingContextCompositingTests, Pattern) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  auto* pattern = MakeGarbageCollected<CanvasPattern>(
      Image::NullImage(), Pattern::kRepeatModeXY, /*origin_clean=*/true);

  context->setFillStyle(scope.GetIsolate(),
                        pattern->ToV8(scope.GetScriptState()),
                        scope.GetExceptionState());
  context->translate(4, 5);
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags flags = FillFlags();
  flags.setShader(PaintShader::MakeColor(SkColors::kTransparent));

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(
                  PaintOpEq<TranslateOp>(4, 5),
                  PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), flags)));
}

// Tests a plain drawImage.
TEST(BaseRenderingContextCompositingTests, DrawImage) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  auto* bitmap = MakeGarbageCollected<HTMLCanvasElement>(scope.GetDocument());
  context->translate(4, 5);
  context->drawImage(bitmap, 0, 0, 10, 10, 0, 0, 10, 10, exception_state);

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<TranslateOp>(4, 5),
                             PaintOpIs<DrawImageRectOp>()));
}

// Tests drawing with context filter.
TEST(BaseRenderingContextCompositingTests, Filter) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setFilter(scope.GetScriptState(), MakeBlurCanvasFilter(20.0f));
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags filter_flags;
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          // TODO: crbug.com/364549423 - No need to reset matrix, it's
          // already identity.
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          // TODO: crbug.com/364549423 - Evaluate whether the filter could be
          // applied on the DrawRectOp directly.
          PaintOpEq<SaveLayerOp>(filter_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), FillFlags()),
          PaintOpEq<RestoreOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1))));
}

// Tests drawing with context filter and a transform.
TEST(BaseRenderingContextCompositingTests, FilterTransform) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setFilter(scope.GetScriptState(), MakeBlurCanvasFilter(20.0f));
  context->translate(4, 5);
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags filter_flags;
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<TranslateOp>(4, 5),
                             PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                                          0, 1, 0, 0,  //
                                                          0, 0, 1, 0,  //
                                                          0, 0, 0, 1)),
                             PaintOpEq<SaveLayerOp>(filter_flags),
                             PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                                          0, 1, 0, 5,  //
                                                          0, 0, 1, 0,  //
                                                          0, 0, 0, 1)),
                             PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5),
                                                   FillFlags()),
                             PaintOpEq<RestoreOp>(),
                             PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                                          0, 1, 0, 5,  //
                                                          0, 0, 1, 0,  //
                                                          0, 0, 0, 1))));
}

// Tests drawing with a shadow.
TEST(BaseRenderingContextCompositingTests, Shadow) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->translate(4, 5);
  context->fillRect(1, 1, 5, 5);

  // TODO: crbug.com/364549423 - Remove draw-looper.
  cc::PaintFlags shadow_flags = FillFlags();
  DrawLooperBuilder draw_looper_builder;
  draw_looper_builder.AddShadow(/*offset=*/{2, 3}, /*blur=*/2,
                                Color::FromRGB(255, 0, 0),
                                DrawLooperBuilder::kShadowIgnoresTransforms,
                                DrawLooperBuilder::kShadowRespectsAlpha);
  draw_looper_builder.AddUnmodifiedContent();
  shadow_flags.setLooper(draw_looper_builder.DetachDrawLooper());

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<TranslateOp>(4, 5),
                             PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5),
                                                   shadow_flags)));
}

// Tests the "copy" composite operation, which is handled as a special case
// clearing the canvas before draw.
TEST(BaseRenderingContextCompositingTests, CopyOp) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setGlobalCompositeOperation("copy");
  context->translate(4, 5);
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags composite_flags = FillFlags();
  composite_flags.setBlendMode(SkBlendMode::kSrc);

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          PaintOpEq<TranslateOp>(4, 5),
          // TODO: crbug.com/364549423 - Evaluate which is faster between
          // clearing the frame buffer manually and using a layer with a `kSrc`
          // blend mode.
          PaintOpEq<DrawColorOp>(SkColors::kTransparent, SkBlendMode::kSrc),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5),
                                composite_flags)));
}

// Tests drawing with a blending operation.
TEST(BaseRenderingContextCompositingTests, Multiply) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setGlobalCompositeOperation("multiply");
  context->translate(4, 5);
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags composite_flags = FillFlags();
  composite_flags.setBlendMode(SkBlendMode::kMultiply);

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<TranslateOp>(4, 5),
                             PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5),
                                                   composite_flags)));
}

// Tests drawing with a composite operation.
TEST(BaseRenderingContextCompositingTests, DstOut) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setGlobalCompositeOperation("destination-out");
  context->translate(4, 5);
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags composite_flags = FillFlags();
  composite_flags.setBlendMode(SkBlendMode::kDstOut);

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<TranslateOp>(4, 5),
                             PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5),
                                                   composite_flags)));
}

// Tests drawing with a composite operation operating on the full surface. These
// ops impact all pixels, even those outside the drawn shape.
TEST(BaseRenderingContextCompositingTests, SrcIn) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setGlobalCompositeOperation("source-in");
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          // TODO: crbug.com/364549423 - No need to reset matrix, it's
          // already identity.
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<SaveLayerOp>(composite_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), FillFlags()),
          PaintOpEq<RestoreOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1))));
}

// Tests composite ops operating on the full surface. These ops impact all
// pixels, even those outside the drawn shape.
TEST(BaseRenderingContextCompositingTests, SrcInTransform) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setGlobalCompositeOperation("source-in");
  context->translate(4, 5);
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          PaintOpEq<TranslateOp>(4, 5),
          // TODO: crbug.com/364549423 - No need to reset matrix, source-in
          // isn't impacted by transforms.
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          // TODO: crbug.com/364549423 - Evaluate whether the composite op could
          // be applied on the DrawRectOp directly.
          PaintOpEq<SaveLayerOp>(composite_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                       0, 1, 0, 5,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), FillFlags()),
          PaintOpEq<RestoreOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                       0, 1, 0, 5,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1))));
}

// Tests drawing with context filter and a "copy" composite operation. The copy
// op should clear previous drawing but the filter should be applied as normal.
TEST(BaseRenderingContextCompositingTests, FilterCopyOp) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setFilter(scope.GetScriptState(), MakeBlurCanvasFilter(20.0f));
  context->setGlobalCompositeOperation("copy");
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags filter_flags;
  filter_flags.setBlendMode(SkBlendMode::kSrc);
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          // TODO: crbug.com/364549423 - No need to reset matrix, it's
          // already identity.
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          // TODO: crbug.com/364549423 - Evaluate which is faster between
          // clearing the frame buffer manually and using a layer with a `kSrc`
          // blend mode.
          PaintOpEq<SaveLayerOp>(filter_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), FillFlags()),
          PaintOpEq<RestoreOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1))));
}

// Tests drawing with context filter, a shadow and a "copy" composite operation.
// The copy op should clear previous drawing and the shadow shouldn't be
// rasterized, but the filter should be applied as normal.
TEST(BaseRenderingContextCompositingTests, FilterShadowCopyOp) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setFilter(scope.GetScriptState(), MakeBlurCanvasFilter(20.0f));
  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->setGlobalCompositeOperation("copy");
  context->fillRect(1, 1, 5, 5);

  sk_sp<cc::PaintFilter> blur_filter =
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr);

  cc::PaintFlags shadow_flags = FillFlags();
  shadow_flags.setBlendMode(SkBlendMode::kSrc);
  // TODO: crbug.com/364549423 - The `ComposePaintFilter`s are useless here.
  shadow_flags.setImageFilter(sk_make_sp<ComposePaintFilter>(
      sk_make_sp<ComposePaintFilter>(nullptr, nullptr), blur_filter));

  // TODO: crbug.com/364549423 - Remove draw-looper.
  DrawLooperBuilder draw_looper_builder;
  draw_looper_builder.AddShadow(/*offset=*/{2, 3}, /*blur=*/2,
                                Color::FromRGB(255, 0, 0),
                                DrawLooperBuilder::kShadowIgnoresTransforms,
                                DrawLooperBuilder::kShadowRespectsAlpha);
  shadow_flags.setLooper(draw_looper_builder.DetachDrawLooper());

  cc::PaintFlags foreground_flags;
  foreground_flags.setBlendMode(SkBlendMode::kSrc);
  foreground_flags.setImageFilter(blur_filter);

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          // TODO: crbug.com/364549423 - No need to reset matrix, it's
          // already identity.
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          // TODO: crbug.com/364549423 - There is no need to draw a shadow, it
          // will be overwritten by the foreground right afterwards.
          PaintOpEq<SaveLayerOp>(shadow_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), FillFlags()),
          PaintOpEq<RestoreOp>(),  //
          PaintOpEq<SaveLayerOp>(foreground_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), FillFlags()),
          PaintOpEq<RestoreOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1))));
}

// Tests a shadow with a "copy" composite operation, which is handled as a
// special case clearing the canvas before draw. Thus, the shadow shouldn't be
// drawn since the foreground overwrites it.
TEST(BaseRenderingContextCompositingTests, ShadowCopyOp) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->setGlobalCompositeOperation("copy");
  context->translate(4, 5);
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags composite_flags = FillFlags();
  composite_flags.setBlendMode(SkBlendMode::kSrc);

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<TranslateOp>(4, 5),
                             PaintOpEq<DrawColorOp>(SkColors::kTransparent,
                                                    SkBlendMode::kSrc),
                             PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5),
                                                   composite_flags)));
}

// Tests fillRect with a shadow and a globalCompositeOperator that can't be
// implemented using a `DropShadowPaintFilter` (it requires separate compositing
// of the shadow and foreground.
TEST(BaseRenderingContextCompositingTests, ShadowMultiply) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->setGlobalCompositeOperation("multiply");
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kMultiply);

  // TODO: crbug.com/364549423 - Remove draw-looper.
  cc::PaintFlags shadow_only_flags = FillFlags();
  DrawLooperBuilder draw_looper_builder;
  draw_looper_builder.AddShadow(/*offset=*/{2, 3}, /*blur=*/2,
                                Color::FromRGB(255, 0, 0),
                                DrawLooperBuilder::kShadowIgnoresTransforms,
                                DrawLooperBuilder::kShadowRespectsAlpha);
  shadow_only_flags.setLooper(draw_looper_builder.DetachDrawLooper());

  cc::PaintFlags foreground_flags = FillFlags();

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          // TODO: crbug.com/364549423 - No need to reset matrix, it's
          // already identity.
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<SaveLayerOp>(composite_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5),
                                shadow_only_flags),
          PaintOpEq<RestoreOp>(),  //
          PaintOpEq<SaveLayerOp>(composite_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), foreground_flags),
          PaintOpEq<RestoreOp>(),                   //
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1))));
}

// Tests fillRect with a shadow and a globalCompositeOperator that can't be
// implemented using a `DropShadowPaintFilter` (it requires separate compositing
// of the shadow and foreground.
TEST(BaseRenderingContextCompositingTests, ShadowMultiplyTransform) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->setGlobalCompositeOperation("multiply");
  context->translate(4, 5);
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kMultiply);

  // TODO: crbug.com/364549423 - Remove draw-looper.
  cc::PaintFlags shadow_only_flags = FillFlags();
  DrawLooperBuilder draw_looper_builder;
  draw_looper_builder.AddShadow(/*offset=*/{2, 3}, /*blur=*/2,
                                Color::FromRGB(255, 0, 0),
                                DrawLooperBuilder::kShadowIgnoresTransforms,
                                DrawLooperBuilder::kShadowRespectsAlpha);
  shadow_only_flags.setLooper(draw_looper_builder.DetachDrawLooper());

  cc::PaintFlags foreground_flags = FillFlags();

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<TranslateOp>(4, 5),
                             PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                                          0, 1, 0, 0,  //
                                                          0, 0, 1, 0,  //
                                                          0, 0, 0, 1)),
                             PaintOpEq<SaveLayerOp>(composite_flags),
                             PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                                          0, 1, 0, 5,  //
                                                          0, 0, 1, 0,  //
                                                          0, 0, 0, 1)),
                             PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5),
                                                   shadow_only_flags),
                             PaintOpEq<RestoreOp>(),  //
                             PaintOpEq<SaveLayerOp>(composite_flags),
                             PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                                          0, 1, 0, 5,  //
                                                          0, 0, 1, 0,  //
                                                          0, 0, 0, 1)),
                             PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5),
                                                   foreground_flags),
                             PaintOpEq<RestoreOp>(),
                             PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                                          0, 1, 0, 5,  //
                                                          0, 0, 1, 0,  //
                                                          0, 0, 0, 1))));
}

// Tests fillRect with a shadow and a composite op that can be implemented using
// a `DropShadowPaintFilter`.
TEST(BaseRenderingContextCompositingTests, ShadowDstOutTransform) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->setGlobalCompositeOperation("destination-out");
  context->translate(4, 5);
  context->fillRect(1, 1, 5, 5);

  // TODO: crbug.com/364549423 - Remove draw-looper.
  cc::PaintFlags flags = FillFlags();
  flags.setBlendMode(SkBlendMode::kDstOut);
  DrawLooperBuilder draw_looper_builder;
  draw_looper_builder.AddShadow(/*offset=*/{2, 3}, /*blur=*/2,
                                Color::FromRGB(255, 0, 0),
                                DrawLooperBuilder::kShadowIgnoresTransforms,
                                DrawLooperBuilder::kShadowRespectsAlpha);
  draw_looper_builder.AddUnmodifiedContent();
  flags.setLooper(draw_looper_builder.DetachDrawLooper());

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(
                  PaintOpEq<TranslateOp>(4, 5),
                  PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), flags)));
}

// Tests a fillRect with a shadow and a composite op operating on the full
// surface. These ops impact all pixels, even those outside the drawn shape.
TEST(BaseRenderingContextCompositingTests, ShadowSrcIn) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->setGlobalCompositeOperation("source-in");
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  // TODO: crbug.com/364549423 - Remove draw-looper.
  cc::PaintFlags shadow_only_flags = FillFlags();
  DrawLooperBuilder draw_looper_builder;
  draw_looper_builder.AddShadow(/*offset=*/{2, 3}, /*blur=*/2,
                                Color::FromRGB(255, 0, 0),
                                DrawLooperBuilder::kShadowIgnoresTransforms,
                                DrawLooperBuilder::kShadowRespectsAlpha);
  shadow_only_flags.setLooper(draw_looper_builder.DetachDrawLooper());

  cc::PaintFlags foreground_flags = FillFlags();

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          // TODO: crbug.com/364549423 - No need to reset matrix, it's
          // already identity.
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<SaveLayerOp>(composite_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5),
                                shadow_only_flags),
          PaintOpEq<RestoreOp>(),  //
          PaintOpEq<SaveLayerOp>(composite_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), foreground_flags),
          PaintOpEq<RestoreOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1))));
}

// Tests a fillRect with a shadow and a composite op operating on the full
// surface. These ops impact all pixels, even those outside the drawn shape.
TEST(BaseRenderingContextCompositingTests, ShadowSrcInTransform) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->setGlobalCompositeOperation("source-in");
  context->translate(4, 5);
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  // TODO: crbug.com/364549423 - Remove draw-looper.
  cc::PaintFlags shadow_only_flags = FillFlags();
  DrawLooperBuilder draw_looper_builder;
  draw_looper_builder.AddShadow(/*offset=*/{2, 3}, /*blur=*/2,
                                Color::FromRGB(255, 0, 0),
                                DrawLooperBuilder::kShadowIgnoresTransforms,
                                DrawLooperBuilder::kShadowRespectsAlpha);
  shadow_only_flags.setLooper(draw_looper_builder.DetachDrawLooper());

  cc::PaintFlags foreground_flags = FillFlags();

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          PaintOpEq<TranslateOp>(4, 5),
          // TODO: crbug.com/364549423 - Undoing the transform has no effect
          // because a draw-looper is used. Without the draw-looper, the shadow
          // would need to be applied on the layer, not the draw op.
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<SaveLayerOp>(composite_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                       0, 1, 0, 5,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5),
                                shadow_only_flags),
          PaintOpEq<RestoreOp>(),  //
          PaintOpEq<SaveLayerOp>(composite_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                       0, 1, 0, 5,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), foreground_flags),
          PaintOpEq<RestoreOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                       0, 1, 0, 5,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1))));
}

// Tests a fillRect with a shadow and a CanvasPattern.
TEST(BaseRenderingContextCompositingTests, ShadowPattern) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  auto* pattern = MakeGarbageCollected<CanvasPattern>(
      Image::NullImage(), Pattern::kRepeatModeXY, /*origin_clean=*/true);

  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->setFillStyle(scope.GetIsolate(),
                        pattern->ToV8(scope.GetScriptState()),
                        scope.GetExceptionState());
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags shadow_flags = FillFlags();
  shadow_flags.setShader(PaintShader::MakeColor(SkColors::kTransparent));

  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      2.0f, 3.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);
  // TODO: crbug.com/364549423 - The `ComposePaintFilter`s are useless here.
  sk_sp<cc::PaintFilter> background_filter = sk_make_sp<ComposePaintFilter>(
      sk_make_sp<ComposePaintFilter>(nullptr, shadow_filter), nullptr);
  shadow_flags.setImageFilter(background_filter);

  cc::PaintFlags pattern_flags = FillFlags();
  pattern_flags.setShader(PaintShader::MakeColor(SkColors::kTransparent));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          // TODO: crbug.com/364549423 - No need to reset matrix, it's already
          // identity.
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<SaveLayerOp>(shadow_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), pattern_flags),
          PaintOpEq<RestoreOp>(),  //
          // TODO: crbug.com/364549423 - The layer shouldn't be needed here.
          PaintOpEq<SaveLayerOp>(PaintFlags()),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), pattern_flags),
          PaintOpEq<RestoreOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1))));
}

// Tests a fillRect with a shadow, a CanvasPattern and a transform.
TEST(BaseRenderingContextCompositingTests, ShadowPatternTransform) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  auto* pattern = MakeGarbageCollected<CanvasPattern>(
      Image::NullImage(), Pattern::kRepeatModeXY, /*origin_clean=*/true);

  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->setFillStyle(scope.GetIsolate(),
                        pattern->ToV8(scope.GetScriptState()),
                        scope.GetExceptionState());
  context->translate(4, 5);
  context->fillRect(1, 1, 5, 5);

  cc::PaintFlags shadow_flags = FillFlags();
  shadow_flags.setShader(PaintShader::MakeColor(SkColors::kTransparent));

  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      2.0f, 3.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);
  // TODO: crbug.com/364549423 - The `ComposePaintFilter`s are useless here.
  sk_sp<cc::PaintFilter> background_filter = sk_make_sp<ComposePaintFilter>(
      sk_make_sp<ComposePaintFilter>(nullptr, shadow_filter), nullptr);
  shadow_flags.setImageFilter(background_filter);

  cc::PaintFlags pattern_flags = FillFlags();
  pattern_flags.setShader(PaintShader::MakeColor(SkColors::kTransparent));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          PaintOpEq<TranslateOp>(4, 5),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<SaveLayerOp>(shadow_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                       0, 1, 0, 5,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), pattern_flags),
          PaintOpEq<RestoreOp>(),  //
          // TODO: crbug.com/364549423 - The layer shouldn't be needed here.
          PaintOpEq<SaveLayerOp>(PaintFlags()),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                       0, 1, 0, 5,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), pattern_flags),
          PaintOpEq<RestoreOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                       0, 1, 0, 5,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1))));
}

// Tests a drawImage with a shadow.
TEST(BaseRenderingContextCompositingTests, ShadowDrawImage) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  auto* bitmap = MakeGarbageCollected<HTMLCanvasElement>(scope.GetDocument());
  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->drawImage(bitmap, 0, 0, 10, 10, 0, 0, 10, 10, exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      2.0f, 3.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          // TODO: crbug.com/364549423 - No need to reset matrix, it's already
          // identity.
          PaintOpEq<SaveOp>(),
          PaintOpEq<ConcatOp>(SkM44(1, 0, 0, 0,  //
                                    0, 1, 0, 0,  //
                                    0, 0, 1, 0,  //
                                    0, 0, 0, 1)),
          PaintOpEq<SaveLayerOp>(SkRect::MakeXYWH(0, 0, 10, 10), shadow_flags),
          PaintOpEq<ConcatOp>(SkM44(1, 0, 0, 0,  //
                                    0, 1, 0, 0,  //
                                    0, 0, 1, 0,  //
                                    0, 0, 0, 1)),
          PaintOpIs<DrawImageRectOp>(),  //
          PaintOpEq<RestoreOp>(),        //
          PaintOpEq<RestoreOp>()));
}

// Tests a drawImage with a shadow and a transform.
TEST(BaseRenderingContextCompositingTests, ShadowDrawImageTransform) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  auto* bitmap = MakeGarbageCollected<HTMLCanvasElement>(scope.GetDocument());
  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->translate(4, 5);
  context->drawImage(bitmap, 0, 0, 10, 10, 0, 0, 10, 10, exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      2.0f, 3.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<TranslateOp>(4, 5),  //
                             PaintOpEq<SaveOp>(),
                             PaintOpEq<ConcatOp>(SkM44(1, 0, 0, -4,  //
                                                       0, 1, 0, -5,  //
                                                       0, 0, 1, 0,   //
                                                       0, 0, 0, 1)),
                             PaintOpEq<SaveLayerOp>(
                                 SkRect::MakeXYWH(4, 5, 10, 10), shadow_flags),
                             PaintOpEq<ConcatOp>(SkM44(1, 0, 0, 4,  //
                                                       0, 1, 0, 5,  //
                                                       0, 0, 1, 0,  //
                                                       0, 0, 0, 1)),
                             PaintOpIs<DrawImageRectOp>(),
                             PaintOpEq<RestoreOp>(),  //
                             PaintOpEq<RestoreOp>()));
}

// Tests a drawImage with a shadow and a composite operation requiring an extra
// layer (requires `CompositedDraw`).
TEST(BaseRenderingContextCompositingTests, DrawImageShadowSrcIn) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  auto* bitmap = MakeGarbageCollected<HTMLCanvasElement>(scope.GetDocument());
  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->setGlobalCompositeOperation("source-in");
  context->drawImage(bitmap, 0, 0, 10, 10, 0, 0, 10, 10, exception_state);

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      2.0f, 3.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          // TODO: crbug.com/364549423 - No need to reset matrix, it's already
          // identity.
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          // TODO: crbug.com/364549423 - Check whether two nested layers are
          // needed here. set and unset right afterwards.
          PaintOpEq<SaveLayerOp>(composite_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<SaveOp>(),
          PaintOpEq<ConcatOp>(SkM44(1, 0, 0, 0,  //
                                    0, 1, 0, 0,  //
                                    0, 0, 1, 0,  //
                                    0, 0, 0, 1)),
          PaintOpEq<SaveLayerOp>(SkRect::MakeXYWH(0, 0, 10, 10), shadow_flags),
          PaintOpEq<ConcatOp>(SkM44(1, 0, 0, 0,  //
                                    0, 1, 0, 0,  //
                                    0, 0, 1, 0,  //
                                    0, 0, 0, 1)),
          PaintOpIs<DrawImageRectOp>(),  //
          PaintOpEq<RestoreOp>(),        //
          PaintOpEq<RestoreOp>(),        //
          PaintOpEq<RestoreOp>(),        //
          PaintOpEq<SaveLayerOp>(composite_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpIs<DrawImageRectOp>(),  //
          PaintOpEq<RestoreOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1))));
}

// Tests a drawImage with a shadow, a transform and a composite operation
// requiring an extra layer (requires `CompositedDraw`).
TEST(BaseRenderingContextCompositingTests, DrawImageShadowSrcInTransform) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  auto* bitmap = MakeGarbageCollected<HTMLCanvasElement>(scope.GetDocument());
  context->setShadowBlur(2);
  context->setShadowOffsetX(2);
  context->setShadowOffsetY(3);
  context->setShadowColor("red");
  context->translate(4, 5);
  context->setGlobalCompositeOperation("source-in");
  context->drawImage(bitmap, 0, 0, 10, 10, 0, 0, 10, 10, exception_state);

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      2.0f, 3.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          PaintOpEq<TranslateOp>(4, 5),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                       0, 1, 0, 0,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          // TODO: crbug.com/364549423 - Check whether two nested layers are
          // needed here. set and unset right afterwards.
          PaintOpEq<SaveLayerOp>(composite_flags),
          // TODO: crbug.com/364549423 - Matrix shouldn't be set and unset right
          // afterwards.
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                       0, 1, 0, 5,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<SaveOp>(),
          PaintOpEq<ConcatOp>(SkM44(1, 0, 0, -4,  //
                                    0, 1, 0, -5,  //
                                    0, 0, 1, 0,   //
                                    0, 0, 0, 1)),
          PaintOpEq<SaveLayerOp>(SkRect::MakeXYWH(4, 5, 10, 10), shadow_flags),
          PaintOpEq<ConcatOp>(SkM44(1, 0, 0, 4,  //
                                    0, 1, 0, 5,  //
                                    0, 0, 1, 0,  //
                                    0, 0, 0, 1)),
          PaintOpIs<DrawImageRectOp>(),  //
          PaintOpEq<RestoreOp>(),        //
          PaintOpEq<RestoreOp>(),        //
          PaintOpEq<RestoreOp>(),        //
          PaintOpEq<SaveLayerOp>(composite_flags),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                       0, 1, 0, 5,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpIs<DrawImageRectOp>(),  //
          PaintOpEq<RestoreOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                       0, 1, 0, 5,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1))));
}

TEST(BaseRenderingContextLayerTests, ContextLost) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->SetContextLost(true);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  EXPECT_THAT(context->FlushRecorder(), RecordedOpsAre());
}

TEST(BaseRenderingContextLayerTests, ResetsAndRestoresShadowStates) {
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  ASSERT_TRUE(context->filter()->IsString());
  EXPECT_EQ(context->filter()->GetAsString(), "none");

  context->endLayer(exception_state);

  ASSERT_TRUE(context->filter()->IsCanvasFilter());
  EXPECT_EQ(context->filter()->GetAsCanvasFilter()->Operations(),
            filter->GetAsCanvasFilter()->Operations());
}

TEST(BaseRenderingContextLayerTests, BeginLayerThrowsOnInvalidFilterParam) {
  test::TaskEnvironment task_environment;
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

TEST(BaseRenderingContextLayerTests, putImageDataThrowsInLayer) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  NonThrowableExceptionState no_exception;
  ImageData* image =
      context->createImageData(/*sw=*/10, /*sh=*/10, no_exception);
  // `putImageData` shouldn't throw on it's own.
  context->putImageData(image, /*dx=*/0, /*dy=*/0, no_exception);
  // Make sure the exception isn't caused by calling the function twice.
  context->putImageData(image, /*dx=*/0, /*dy=*/0, no_exception);
  // Calling again inside a layer should throw.
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      no_exception);
  context->putImageData(image, /*dx=*/0, /*dy=*/0, scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST(BaseRenderingContextLayerGlobalStateTests, DefaultRenderingStates) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerAlphaOp>(1.0f),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, GlobalAlpha) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalAlpha(0.3);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerAlphaOp>(0.3f),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, BlendingOperation) {
  test::TaskEnvironment task_environment;
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

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerOp>(flags),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, CompositeOperation) {
  test::TaskEnvironment task_environment;
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

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerOp>(flags),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, Shadow) {
  test::TaskEnvironment task_environment;
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
  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerOp>(flags),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, GlobalAlphaAndBlending) {
  test::TaskEnvironment task_environment;
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

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerOp>(flags),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, GlobalAlphaAndComposite) {
  test::TaskEnvironment task_environment;
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

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(
                  PaintOpEq<SaveLayerOp>(composite_flags),
                  PaintOpEq<SaveLayerAlphaOp>(0.3f), PaintOpEq<RestoreOp>(),
                  PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, GlobalAlphaAndShadow) {
  test::TaskEnvironment task_environment;
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

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(
                  PaintOpEq<SaveLayerOp>(shadow_flags),
                  PaintOpEq<SaveLayerAlphaOp>(0.5f), PaintOpEq<RestoreOp>(),
                  PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, GlobalAlphaBlendingAndShadow) {
  test::TaskEnvironment task_environment;
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

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kMultiply);

  cc::PaintFlags shadow_flags;
  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);
  sk_sp<cc::PaintFilter> foreground_filter = nullptr;

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(DrawRecordOpEq(
          PaintOpEq<SaveLayerFiltersOp>(
              std::array{shadow_filter, foreground_filter}, composite_flags),
          PaintOpEq<SaveLayerAlphaOp>(0.5f), PaintOpEq<RestoreOp>(),
          PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, GlobalAlphaCompositeAndShadow) {
  test::TaskEnvironment task_environment;
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

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);
  sk_sp<cc::PaintFilter> foreground_filter = nullptr;

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(DrawRecordOpEq(
          PaintOpEq<SaveLayerFiltersOp>(
              std::array{shadow_filter, foreground_filter}, composite_flags),
          PaintOpEq<SaveLayerAlphaOp>(0.5f), PaintOpEq<RestoreOp>(),
          PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, BlendingAndShadow) {
  test::TaskEnvironment task_environment;
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

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kMultiply);

  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);
  sk_sp<cc::PaintFilter> foreground_filter = nullptr;

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(DrawRecordOpEq(
          PaintOpEq<SaveLayerFiltersOp>(
              std::array{shadow_filter, foreground_filter}, composite_flags),
          PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, CompositeAndShadow) {
  test::TaskEnvironment task_environment;
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

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);
  sk_sp<cc::PaintFilter> foreground_filter = nullptr;

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(DrawRecordOpEq(
          PaintOpEq<SaveLayerFiltersOp>(
              std::array{shadow_filter, foreground_filter}, composite_flags),
          PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, Filter) {
  test::TaskEnvironment task_environment;
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
  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerOp>(flags),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterAndGlobalAlpha) {
  test::TaskEnvironment task_environment;
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
  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerOp>(flags),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterAndBlending) {
  test::TaskEnvironment task_environment;
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
  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerOp>(flags),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterAndComposite) {
  test::TaskEnvironment task_environment;
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

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(
                  PaintOpEq<SaveLayerOp>(composite_flags),
                  PaintOpEq<SaveLayerOp>(filter_flags), PaintOpEq<RestoreOp>(),
                  PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterAndShadow) {
  test::TaskEnvironment task_environment;
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

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(
                  PaintOpEq<SaveLayerOp>(shadow_flags),
                  PaintOpEq<SaveLayerOp>(filter_flags), PaintOpEq<RestoreOp>(),
                  PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterGlobalAlphaAndBlending) {
  test::TaskEnvironment task_environment;
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
  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerOp>(flags),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterGlobalAlphaAndComposite) {
  test::TaskEnvironment task_environment;
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

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(
                  PaintOpEq<SaveLayerOp>(composite_flags),
                  PaintOpEq<SaveLayerOp>(filter_flags), PaintOpEq<RestoreOp>(),
                  PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterGlobalAlphaAndShadow) {
  test::TaskEnvironment task_environment;
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

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(
                  PaintOpEq<SaveLayerOp>(shadow_flags),
                  PaintOpEq<SaveLayerOp>(filter_flags), PaintOpEq<RestoreOp>(),
                  PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests,
     FilterGlobalAlphaBlendingAndShadow) {
  test::TaskEnvironment task_environment;
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

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kMultiply);

  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);
  sk_sp<cc::PaintFilter> foreground_filter = nullptr;

  cc::PaintFlags filter_flags;
  filter_flags.setAlphaf(0.4f);
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(DrawRecordOpEq(
          PaintOpEq<SaveLayerFiltersOp>(
              std::array{shadow_filter, foreground_filter}, composite_flags),
          PaintOpEq<SaveLayerOp>(filter_flags), PaintOpEq<RestoreOp>(),
          PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests,
     FilterGlobalAlphaCompositeAndShadow) {
  test::TaskEnvironment task_environment;
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

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);
  sk_sp<cc::PaintFilter> foreground_filter = nullptr;

  cc::PaintFlags filter_flags;
  filter_flags.setAlphaf(0.4f);
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(DrawRecordOpEq(
          PaintOpEq<SaveLayerFiltersOp>(
              std::array{shadow_filter, foreground_filter}, composite_flags),
          PaintOpEq<SaveLayerOp>(filter_flags), PaintOpEq<RestoreOp>(),
          PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterBlendingAndShadow) {
  test::TaskEnvironment task_environment;
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

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kMultiply);

  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);
  sk_sp<cc::PaintFilter> foreground_filter = nullptr;

  cc::PaintFlags filter_flags;
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(DrawRecordOpEq(
          PaintOpEq<SaveLayerFiltersOp>(
              std::array{shadow_filter, foreground_filter}, composite_flags),
          PaintOpEq<SaveLayerOp>(filter_flags), PaintOpEq<RestoreOp>(),
          PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, FilterCompositeAndShadow) {
  test::TaskEnvironment task_environment;
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

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);
  sk_sp<cc::PaintFilter> foreground_filter = nullptr;

  cc::PaintFlags filter_flags;
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(DrawRecordOpEq(
          PaintOpEq<SaveLayerFiltersOp>(
              std::array{shadow_filter, foreground_filter}, composite_flags),
          PaintOpEq<SaveLayerOp>(filter_flags), PaintOpEq<RestoreOp>(),
          PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, ContextFilter) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setFilter(scope.GetScriptState(), MakeBlurCanvasFilter(20.0f));
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags filter_flags;
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerOp>(filter_flags),
                                    PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, ContextFilterLayerFilter) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setFilter(scope.GetScriptState(), MakeBlurCanvasFilter(2.0f));
  context->beginLayer(scope.GetScriptState(),
                      FilterOption(scope, "'blur(5px)'"), exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags global_flags;
  cc::PaintFlags layer_flags;
  global_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(2.0f, 2.0f, SkTileMode::kDecal, nullptr));
  layer_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(5.0f, 5.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(
                  PaintOpEq<SaveLayerOp>(global_flags),
                  PaintOpEq<SaveLayerOp>(layer_flags), PaintOpEq<RestoreOp>(),
                  PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, ContextFilterShadow) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setFilter(scope.GetScriptState(), MakeBlurCanvasFilter(5.0f));
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags layer_flags;
  sk_sp<cc::PaintFilter> foreground_filter =
      sk_make_sp<BlurPaintFilter>(5.0f, 5.0f, SkTileMode::kDecal, nullptr);

  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);

  sk_sp<cc::PaintFilter> background_filter =
      sk_make_sp<ComposePaintFilter>(shadow_filter, foreground_filter);

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(DrawRecordOpEq(
          PaintOpEq<SaveLayerFiltersOp>(
              std::array{background_filter, foreground_filter}, layer_flags),
          PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, TransformsAlone) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->translate(4, 5);
  context->beginLayer(
      scope.GetScriptState(), BeginLayerOptions::Create(), exception_state);
  context->endLayer(exception_state);

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(
                PaintOpEq<TranslateOp>(4, 5),
                DrawRecordOpEq(
                  PaintOpEq<SaveLayerAlphaOp>(1.0f),
                  PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, TransformsWithShadow) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->translate(4, 5);
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->beginLayer(
      scope.GetScriptState(), BeginLayerOptions::Create(), exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags shadow_flags;
  shadow_flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(
                PaintOpEq<TranslateOp>(4, 5),
                DrawRecordOpEq(
                  PaintOpEq<SaveOp>(),
                  PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                               0, 1, 0, 0,  //
                                               0, 0, 1, 0,  //
                                               0, 0, 0, 1)),
                  PaintOpEq<SaveLayerOp>(shadow_flags),
                  PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                               0, 1, 0, 5,  //
                                               0, 0, 1, 0,  //
                                               0, 0, 0, 1)),
                  PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, TransformsWithContextFilter) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->translate(4, 5);
  context->setFilter(scope.GetScriptState(), MakeBlurCanvasFilter(5.0f));
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags filter_flags;
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(5.0f, 5.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          PaintOpEq<TranslateOp>(4, 5),
          DrawRecordOpEq(PaintOpEq<SaveOp>(),
                         PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                                      0, 1, 0, 0,  //
                                                      0, 0, 1, 0,  //
                                                      0, 0, 0, 1)),
                         PaintOpEq<SaveLayerOp>(filter_flags),
                         PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                                      0, 1, 0, 5,  //
                                                      0, 0, 1, 0,  //
                                                      0, 0, 0, 1)),
                         PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests,
     TransformsWithShadowAndCompositedDraw) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->translate(4, 5);
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setGlobalCompositeOperation("source-in");
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);
  sk_sp<cc::PaintFilter> foreground_filter = nullptr;

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          PaintOpEq<TranslateOp>(4, 5),
          DrawRecordOpEq(PaintOpEq<SaveOp>(),
                         PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                                      0, 1, 0, 0,  //
                                                      0, 0, 1, 0,  //
                                                      0, 0, 0, 1)),
                         PaintOpEq<SaveLayerFiltersOp>(
                             std::array{shadow_filter, foreground_filter},
                             composite_flags),
                         PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                                      0, 1, 0, 5,  //
                                                      0, 0, 1, 0,  //
                                                      0, 0, 0, 1)),
                         PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests,
     TransformsWithShadowAndContextFilter) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->translate(4, 5);
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setFilter(scope.GetScriptState(), MakeBlurCanvasFilter(5.0f));
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags layer_flags;
  sk_sp<cc::PaintFilter> foreground_filter =
      sk_make_sp<BlurPaintFilter>(5.0f, 5.0f, SkTileMode::kDecal, nullptr);

  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);

  sk_sp<cc::PaintFilter> background_filter =
      sk_make_sp<ComposePaintFilter>(shadow_filter, foreground_filter);

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          PaintOpEq<TranslateOp>(4, 5),
          DrawRecordOpEq(PaintOpEq<SaveOp>(),
                         PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                                      0, 1, 0, 0,  //
                                                      0, 0, 1, 0,  //
                                                      0, 0, 0, 1)),
                         PaintOpEq<SaveLayerFiltersOp>(
                             std::array{background_filter, foreground_filter},
                             layer_flags),
                         PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                                      0, 1, 0, 5,  //
                                                      0, 0, 1, 0,  //
                                                      0, 0, 0, 1)),
                         PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, NonInvertibleTransform) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->scale(0, 5);
  context->setGlobalAlpha(0.3f);
  context->setGlobalCompositeOperation("source-in");
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->beginLayer(scope.GetScriptState(),
                      FilterOption(scope, "'blur(1px)'"), exception_state);
  context->endLayer(exception_state);

  // Because the layer is not rasterizable, the shadow, global alpha,
  // composite op and filter are optimized away.
  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<ScaleOp>(0, 5),
                             DrawRecordOpEq(PaintOpEq<SaveLayerAlphaOp>(1.0f),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests, CopyCompositeOp) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->setGlobalCompositeOperation("copy");
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(DrawRecordOpEq(
          PaintOpEq<DrawColorOp>(SkColors::kTransparent, SkBlendMode::kSrc),
          PaintOpEq<SaveLayerAlphaOp>(1.0f), PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayerGlobalStateTests,
     CopyCompositeOpWithOtherStates) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->translate(6, 7);
  context->setGlobalAlpha(0.4);
  context->setGlobalCompositeOperation("copy");
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->setFilter(scope.GetScriptState(), MakeBlurCanvasFilter(5.0f));
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags global_flags;
  global_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(5.0f, 5.0f, SkTileMode::kDecal, nullptr));

  cc::PaintFlags layer_flags;
  layer_flags.setAlphaf(0.4f);
  layer_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          PaintOpEq<TranslateOp>(6, 7),
          DrawRecordOpEq(
              PaintOpEq<DrawColorOp>(SkColors::kTransparent, SkBlendMode::kSrc),
              PaintOpEq<SaveOp>(),
              PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                           0, 1, 0, 0,  //
                                           0, 0, 1, 0,  //
                                           0, 0, 0, 1)),
              PaintOpEq<SaveLayerOp>(global_flags),
              PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 6,  //
                                           0, 1, 0, 7,  //
                                           0, 0, 1, 0,  //
                                           0, 0, 0, 1)),
              PaintOpEq<SaveLayerOp>(layer_flags), PaintOpEq<RestoreOp>(),
              PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextRestoreStackTests, RestoresSaves) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->save();
  context->save();
  context->save();

  // Disable automatic matrix restore so this test could manually invoke it.
  context->SetRestoreMatrixEnabled(false);

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<SaveOp>(), PaintOpEq<SaveOp>(),
                             PaintOpEq<SaveOp>(), PaintOpEq<RestoreOp>(),
                             PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));

  // `FlushRecorder()` flushed the recording canvas, leaving it empty.
  ASSERT_THAT(context->FlushRecorder(), IsEmpty());

  context->RestoreMatrixClipStack(context->GetPaintCanvas());
  context->restore(exception_state);
  context->restore(exception_state);
  context->restore(exception_state);

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<SaveOp>(), PaintOpEq<SaveOp>(),
                             PaintOpEq<SaveOp>(), PaintOpEq<RestoreOp>(),
                             PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextRestoreStackTests, RestoresTransforms) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->translate(10.0, 0.0);
  context->translate(0.0, 20.0);
  context->save();  // No transforms to restore on that level.
  context->save();
  context->translate(15.0, 15.0);

  // Disable automatic matrix restore so this test could manually invoke it.
  context->SetRestoreMatrixEnabled(false);

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(PaintOpEq<TranslateOp>(10.0, 0.0),  // Root transforms.
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
      context->FlushRecorder(),
      RecordedOpsAre(
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
  test::TaskEnvironment task_environment;
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

  // Disable automatic matrix restore so this test could manually invoke it.
  context->SetRestoreMatrixEnabled(false);

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
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
      context->FlushRecorder(),
      RecordedOpsAre(
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

TEST(BaseRenderingContextRestoreStackTests, UnclosedLayersAreNotFlushed) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->save();
  context->translate(1, 2);
  context->fillRect(0, 0, 4, 4);
  context->save();
  context->translate(3, 4);
  context->fillRect(1, 1, 5, 5);

  context->setGlobalAlpha(0.4);
  context->setGlobalCompositeOperation("source-in");
  context->setShadowBlur(2.0);
  context->setShadowColor("red");
  context->beginLayer(
      scope.GetScriptState(),
      FilterOption(scope, "({name: 'gaussianBlur', stdDeviation: 20})"),
      exception_state);
  context->translate(5, 6);
  context->fillRect(2, 2, 6, 6);

  // Only draw ops preceding `beginLayer` gets flushed.
  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          PaintOpEq<SaveOp>(), PaintOpEq<TranslateOp>(1, 2),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(0, 0, 4, 4), FillFlags()),
          PaintOpEq<SaveOp>(), PaintOpEq<TranslateOp>(3, 4),
          PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 1, 5, 5), FillFlags()),
          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));

  context->fillRect(3, 3, 7, 7);

  // Matrix stack gets rebuilt, but recording contains no draw calls.
  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<SaveOp>(),
                             PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 1,  //
                                                          0, 1, 0, 2,  //
                                                          0, 0, 1, 0,  //
                                                          0, 0, 0, 1)),
                             PaintOpEq<SaveOp>(),
                             PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                                          0, 1, 0, 6,  //
                                                          0, 0, 1, 0,  //
                                                          0, 0, 0, 1)),
                             PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));

  context->endLayer(exception_state);

  cc::PaintFlags composite_flags;
  composite_flags.setBlendMode(SkBlendMode::kSrcIn);

  sk_sp<cc::PaintFilter> shadow_filter = sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 1.0f, 1.0f, SkColors::kRed,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr);
  sk_sp<cc::PaintFilter> foreground_filter = nullptr;

  cc::PaintFlags filter_flags;
  filter_flags.setAlphaf(0.4f);
  filter_flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(20.0f, 20.0f, SkTileMode::kDecal, nullptr));

  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(
          PaintOpEq<SaveOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 1,  //
                                       0, 1, 0, 2,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          PaintOpEq<SaveOp>(),
          PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                       0, 1, 0, 6,  //
                                       0, 0, 1, 0,  //
                                       0, 0, 0, 1)),
          DrawRecordOpEq(
              PaintOpEq<SaveOp>(),
              PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 0,  //
                                           0, 1, 0, 0,  //
                                           0, 0, 1, 0,  //
                                           0, 0, 0, 1)),
              PaintOpEq<SaveLayerFiltersOp>(
                  std::array{shadow_filter, foreground_filter},
                  composite_flags),
              PaintOpEq<SetMatrixOp>(SkM44(1, 0, 0, 4,  //
                                           0, 1, 0, 6,  //
                                           0, 0, 1, 0,  //
                                           0, 0, 0, 1)),
              PaintOpEq<SaveLayerOp>(filter_flags),
              PaintOpEq<TranslateOp>(5.0f, 6.0f),
              PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(2, 2, 6, 6), FillFlags()),
              PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(3, 3, 7, 7), FillFlags()),
              PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>(),
              PaintOpEq<RestoreOp>()),

          PaintOpEq<RestoreOp>(), PaintOpEq<RestoreOp>()));
}

TEST(BaseRenderingContextResetTest, DiscardsRenderStates) {
  test::TaskEnvironment task_environment;
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

  EXPECT_EQ(context->StateStackDepth(), 1);
  EXPECT_EQ(context->OpenedLayerCount(), 1);

  // Discard the rendering states:
  context->reset();

  EXPECT_EQ(context->StateStackDepth(), 0);
  EXPECT_EQ(context->OpenedLayerCount(), 0);

  // `reset` discards all paint ops and reset the canvas content.
  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<DrawRectOp>(
                  SkRect::MakeXYWH(0, 0, context->Width(), context->Height()),
                  ClearRectFlags())));

  // The recording should now be empty:
  ASSERT_THAT(RecordedOpsView(context->FlushRecorder()), IsEmpty());

  // Do some operation and check that the rendering state was reset:
  context->fillRect(1, 2, 3, 4);

  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(PaintOpEq<DrawRectOp>(SkRect::MakeXYWH(1, 2, 3, 4),
                                                   FillFlags())));
}

TEST(BaseRenderingContextLayersCallOrderTests, LoneBeginLayer) {
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->restore(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(context->StateStackDepth(), 0);
  EXPECT_EQ(context->OpenedLayerCount(), 0);
}

TEST(BaseRenderingContextLayersCallOrderTests, LoneEndLayer) {
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->save();
  context->restore(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(context->StateStackDepth(), 0);
  EXPECT_EQ(context->OpenedLayerCount(), 0);
}

TEST(BaseRenderingContextLayersCallOrderTests, SaveResetRestore) {
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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

TEST(BaseRenderingContextLayersCallOrderTests, NestedLayers) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState no_exception;
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      no_exception);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      no_exception);
  EXPECT_EQ(context->StateStackDepth(), 2);
  EXPECT_EQ(context->OpenedLayerCount(), 2);
  context->endLayer(no_exception);
  context->endLayer(no_exception);
  EXPECT_EQ(context->StateStackDepth(), 0);
  EXPECT_EQ(context->OpenedLayerCount(), 0);

  // Nested layers are all stored in the same side recording and drawn as a
  // whole to the main recording.
  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerAlphaOp>(1.0f),  //
                                    PaintOpEq<SaveLayerAlphaOp>(1.0f),  //
                                    PaintOpEq<RestoreOp>(),             //
                                    PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayersCSSTests,
     FilterOperationsWithStyleResolutionHost) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  context->SetHostHTMLCanvas(
      MakeGarbageCollected<HTMLCanvasElement>(scope.GetDocument()));
  context->setFont("10px sans-serif");
  NonThrowableExceptionState exception_state;
  context->beginLayer(scope.GetScriptState(),
                      FilterOption(scope, "'blur(1em)'"), exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags flags;
  flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(10.0f, 10.0f, SkTileMode::kDecal, nullptr));
  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerOp>(flags),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextLayersCSSTests,
     FilterOperationsWithNoStyleResolutionHost) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;
  context->beginLayer(scope.GetScriptState(),
                      FilterOption(scope, "'blur(1em)'"), exception_state);
  context->endLayer(exception_state);

  cc::PaintFlags flags;
  // Font sized is assumed to be 16px when no style resolution is available.
  flags.setImageFilter(
      sk_make_sp<BlurPaintFilter>(16.0f, 16.0f, SkTileMode::kDecal, nullptr));
  EXPECT_THAT(context->FlushRecorder(),
              RecordedOpsAre(DrawRecordOpEq(PaintOpEq<SaveLayerOp>(flags),
                                            PaintOpEq<RestoreOp>())));
}

TEST(BaseRenderingContextMeshTests, DrawMesh) {
  test::TaskEnvironment task_environment;

  scoped_refptr<cc::RefCountedBuffer<SkPoint>> vbuf =
      base::MakeRefCounted<cc::RefCountedBuffer<SkPoint>>(
          std::vector<SkPoint>{{0, 0}, {100, 0}, {100, 100}, {0, 100}});
  scoped_refptr<cc::RefCountedBuffer<SkPoint>> uvbuf =
      base::MakeRefCounted<cc::RefCountedBuffer<SkPoint>>(
          std::vector<SkPoint>{{0, 0}, {1, 0}, {1, 1}, {0, 1}});
  scoped_refptr<cc::RefCountedBuffer<uint16_t>> ibuf =
      base::MakeRefCounted<cc::RefCountedBuffer<uint16_t>>(
          std::vector<uint16_t>{0, 1, 2, 0, 2, 3});

  V8TestingScope scope;
  NonThrowableExceptionState no_exception;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);

  context->drawMesh(
      MakeGarbageCollected<Mesh2DVertexBuffer>(vbuf),
      MakeGarbageCollected<Mesh2DUVBuffer>(uvbuf),
      MakeGarbageCollected<Mesh2DIndexBuffer>(ibuf),
      MakeGarbageCollected<V8CanvasImageSource>(
          MakeGarbageCollected<ImageBitmap>(
              context->createImageData(/*sw=*/10, /*sh=*/10, no_exception),
              /*crop_rect=*/std::nullopt)),
      no_exception);

  PaintFlags flags = FillFlags();
  SkMatrix local_matrix = SkMatrix::Scale(1.0f / 10, 1.0f / 10);
  flags.setShader(PaintShader::MakeImage(PaintImage(), SkTileMode::kClamp,
                                         SkTileMode::kClamp, &local_matrix));
  EXPECT_THAT(
      context->FlushRecorder(),
      RecordedOpsAre(PaintOpEq<DrawVerticesOp>(vbuf, uvbuf, ibuf, flags)));
}

}  // namespace
}  // namespace blink
