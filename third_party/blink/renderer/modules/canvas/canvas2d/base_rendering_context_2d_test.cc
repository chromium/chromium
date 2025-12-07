// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_record.h"
#include "cc/test/paint_op_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_begin_layer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/recording_test_utils.h"
#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_test_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/draw_looper_builder.h"
#include "third_party/blink/renderer/platform/graphics/flush_reason.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTileMode.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class MemoryManagedPaintCanvas;

namespace {

using ::blink_testing::ParseFilter;
using ::blink_testing::RecordedOpsAre;
using ::cc::DrawImageOp;
using ::cc::PaintOpEq;
using ::cc::PaintOpIs;
using ::cc::RestoreOp;
using ::cc::SaveLayerOp;

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
            MakeGarbageCollected<HTMLCanvasElement>(scope.GetDocument()),
            CanvasContextCreationAttributesCore(),
            scheduler::GetSingleThreadTaskRunnerForTesting()),
        execution_context_(scope.GetExecutionContext()),
        recorder_(gfx::Size(Width(), Height()), this) {}
  ~TestRenderingContext2D() override = default;

  // Returns the content of the paint recorder, leaving it empty.
  cc::PaintRecord FlushRecorder() { return recorder_.ReleaseMainRecording(); }

  bool OriginClean() const override { return true; }
  void SetOriginTainted() override {}

  int Width() const override { return 300; }
  int Height() const override { return 300; }

  bool CanCreateResourceProvider() override { return false; }

  RespectImageOrientationEnum RespectImageOrientation() const override {
    return kRespectImageOrientation;
  }

  Color GetCurrentColor() const override { return Color::kBlack; }

  MemoryManagedPaintCanvas* GetOrCreatePaintCanvas() override {
    // Context child classes uses `GetOrCreatePaintCanvas` to check for context
    // loss.
    if (isContextLost()) [[unlikely]] {
      return nullptr;
    }

    return &recorder_.getRecordingCanvas();
  }
  using BaseRenderingContext2D::GetPaintCanvas;  // Pull the non-const overload.
  const MemoryManagedPaintCanvas* GetPaintCanvas() const override {
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

  void SetContextLost(LostContextMode context_lost_mode) {
    context_lost_mode_ = context_lost_mode;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(execution_context_);
    BaseRenderingContext2D::Trace(visitor);
  }

  HTMLCanvasElement* HostAsHTMLCanvasElement() const override {
    return static_cast<HTMLCanvasElement*>(Host());
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
    if (Host() == nullptr) {
      return false;
    }
    auto* style = CSSParser::ParseFont(new_font, execution_context_);
    if (style == nullptr) {
      return false;
    }
    auto* selector = Host()->GetFontSelector();
    FontDescription font_description =
        FontStyleResolver::ComputeFont(*style, selector->BaseFontSelector());
    GetState().SetFont(font_description, selector);
    return true;
  }

  CanvasResourceProvider* GetResourceProvider() const override {
    return nullptr;
  }

  CanvasResourceProvider* GetOrCreateResourceProvider() override {
    return nullptr;
  }

  // Implementing pure virtual functions from CanvasRenderingContext.
  scoped_refptr<StaticBitmapImage> GetImage() override { return nullptr; }
  std::unique_ptr<CanvasResourceProvider> ReplaceResourceProvider(
      std::unique_ptr<CanvasResourceProvider>) override {
    return nullptr;
  }

  bool IsComposited() const override { return false; }
  bool IsPaintable() const override { return true; }
  void Stop() override {}

  Member<ExecutionContext> execution_context_;
  bool restore_matrix_enabled_ = true;
  MemoryManagedPaintRecorder recorder_;
};

BeginLayerOptions* FilterOption(blink::V8TestingScope& scope,
                                const std::string& filter) {
  BeginLayerOptions* options = BeginLayerOptions::Create();
  options->setFilter(ParseFilter(scope, filter));
  return options;
}

TEST(BaseRenderingContextLayerTests, ContextLost) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
  NonThrowableExceptionState exception_state;

  context->SetContextLost(CanvasRenderingContext::kRealLostContext);
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      exception_state);
  context->endLayer(exception_state);

  EXPECT_THAT(context->FlushRecorder(), RecordedOpsAre());
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

TEST(BaseRenderingContextLayersCSSTests,
     FilterOperationsWithStyleResolutionHost) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* context = MakeGarbageCollected<TestRenderingContext2D>(scope);
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

}  // namespace
}  // namespace blink
