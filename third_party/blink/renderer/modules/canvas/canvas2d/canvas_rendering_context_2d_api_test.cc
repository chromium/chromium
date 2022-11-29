// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"

#include <memory>

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_float32array_uint16array_uint8clampedarray.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style_test_utils.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "ui/accessibility/ax_mode.h"

using testing::Mock;

namespace blink {

class CanvasRenderingContext2DAPITest : public PageTestBase {
 public:
  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(GetDocument().GetFrame());
  }
  v8::Isolate* GetIsolate() { return GetScriptState()->GetIsolate(); }
  CanvasRenderingContext2D* Context2D() const;

 protected:
  CanvasRenderingContext2DAPITest();
  void SetUp() override;
  void TearDown() override;

  HTMLCanvasElement& CanvasElement() const { return *canvas_element_; }

  void CreateContext(OpacityMode);

 private:
  Persistent<HTMLCanvasElement> canvas_element_;
};

CanvasRenderingContext2DAPITest::CanvasRenderingContext2DAPITest() = default;

CanvasRenderingContext2D* CanvasRenderingContext2DAPITest::Context2D() const {
  // If the following check fails, perhaps you forgot to call createContext
  // in your test?
  EXPECT_NE(nullptr, CanvasElement().RenderingContext());
  EXPECT_TRUE(CanvasElement().RenderingContext()->IsRenderingContext2D());
  return static_cast<CanvasRenderingContext2D*>(
      CanvasElement().RenderingContext());
}

void CanvasRenderingContext2DAPITest::CreateContext(OpacityMode opacity_mode) {
  String canvas_type("2d");
  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = opacity_mode == kNonOpaque;
  canvas_element_->GetCanvasRenderingContext(canvas_type, attributes);
  Context2D();  // Calling this for the checks
}

void CanvasRenderingContext2DAPITest::SetUp() {
  PageTestBase::SetUp();
  GetDocument().documentElement()->setInnerHTML(
      "<body><canvas id='c'></canvas></body>");
  UpdateAllLifecyclePhasesForTest();
  canvas_element_ = To<HTMLCanvasElement>(GetDocument().getElementById("c"));
}

void CanvasRenderingContext2DAPITest::TearDown() {
  PageTestBase::TearDown();
  CanvasRenderingContext::GetCanvasPerformanceMonitor().ResetForTesting();
}

TEST_F(CanvasRenderingContext2DAPITest, SetShadowColor_Clamping) {
  CreateContext(kNonOpaque);

  Context2D()->setShadowColor("rgba(0,0,0,0)");
  EXPECT_EQ(String("rgba(0, 0, 0, 0)"), Context2D()->shadowColor());
  Context2D()->setShadowColor("rgb(0,0,0)");
  EXPECT_EQ(String("#000000"), Context2D()->shadowColor());
  Context2D()->setShadowColor("rgb(0,999,0)");
  EXPECT_EQ(String("#00ff00"), Context2D()->shadowColor());
  Context2D()->setShadowColor(
      "rgb(0,"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      ",0)");
  EXPECT_EQ(String("#00ff00"), Context2D()->shadowColor());
  Context2D()->setShadowColor("rgb(0,0,256)");
  EXPECT_EQ(String("#0000ff"), Context2D()->shadowColor());
  Context2D()->setShadowColor(
      "rgb(999999999999999999999999,0,-9999999999999999999999999999)");
  EXPECT_EQ(String("#ff0000"), Context2D()->shadowColor());
  Context2D()->setShadowColor(
      "rgba("
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "9999999999,9,0,1)");
  EXPECT_EQ(String("#ff0900"), Context2D()->shadowColor());
  Context2D()->setShadowColor(
      "rgba("
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "9999999999,9,0,-99999999999999999999999999999999999999)");
  EXPECT_EQ(String("rgba(255, 9, 0, 0)"), Context2D()->shadowColor());
  Context2D()->setShadowColor(
      "rgba(7,"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "9999999999,0,"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "99999999999999999)");
  EXPECT_EQ(String("#07ff00"), Context2D()->shadowColor());
  Context2D()->setShadowColor(
      "rgba(-7,"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "9999999999,0,"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "99999999999999999)");
  EXPECT_EQ(String("#00ff00"), Context2D()->shadowColor());
  Context2D()->setShadowColor("rgba(0%,100%,0%,0.4)");
  EXPECT_EQ(String("rgba(0, 255, 0, 0.4)"), Context2D()->shadowColor());
}

String TrySettingStrokeStyle(CanvasRenderingContext2DAPITest* test,
                             const String& value) {
  SetStrokeStyleString(test->Context2D(), test->GetScriptState(), "#666");
  SetStrokeStyleString(test->Context2D(), test->GetScriptState(), value);
  return GetStrokeStyleAsString(test->Context2D(), test->GetScriptState());
}

String TrySettingFillStyle(CanvasRenderingContext2DAPITest* test,
                           const String& value) {
  SetFillStyleString(test->Context2D(), test->GetScriptState(), "#666");
  SetFillStyleString(test->Context2D(), test->GetScriptState(), value);
  return GetFillStyleAsString(test->Context2D(), test->GetScriptState());
}

String TrySettingShadowColor(CanvasRenderingContext2D* ctx,
                             const String& value) {
  ctx->setShadowColor("#666");
  ctx->setShadowColor(value);
  return ctx->shadowColor();
}

void TrySettingColor(CanvasRenderingContext2DAPITest* test,
                     const String& value,
                     const String& expected) {
  EXPECT_EQ(expected, TrySettingStrokeStyle(test, value));
  EXPECT_EQ(expected, TrySettingFillStyle(test, value));
  EXPECT_EQ(expected, TrySettingShadowColor(test->Context2D(), value));
}

TEST_F(CanvasRenderingContext2DAPITest, ColorSerialization) {
  v8::HandleScope handle_scope(GetIsolate());

  CreateContext(kNonOpaque);
  // Check round trips
  TrySettingColor(this, "transparent", "rgba(0, 0, 0, 0)");
  TrySettingColor(this, "red", "#ff0000");
  TrySettingColor(this, "white", "#ffffff");
  TrySettingColor(this, "", "#666666");
  TrySettingColor(this, "RGBA(0, 0, 0, 0)", "rgba(0, 0, 0, 0)");
  TrySettingColor(this, "rgba(0,255,0,1.0)", "#00ff00");
  TrySettingColor(this, "rgba(1,2,3,0.4)", "rgba(1, 2, 3, 0.4)");
  TrySettingColor(this, "RgB(1,2,3)", "#010203");
  TrySettingColor(this, "rGbA(1,2,3,0)", "rgba(1, 2, 3, 0)");
}

TEST_F(CanvasRenderingContext2DAPITest, DefaultAttributeValues) {
  v8::HandleScope handle_scope(GetIsolate());

  CreateContext(kNonOpaque);

  EXPECT_EQ(String("#000000"),
            GetStrokeStyleAsString(Context2D(), GetScriptState()));

  EXPECT_EQ(String("#000000"),
            GetFillStyleAsString(Context2D(), GetScriptState()));

  EXPECT_EQ(String("rgba(0, 0, 0, 0)"), Context2D()->shadowColor());
}

TEST_F(CanvasRenderingContext2DAPITest, LineDashStateSave) {
  CreateContext(kNonOpaque);

  Vector<double> simple_dash;
  simple_dash.push_back(4);
  simple_dash.push_back(2);

  Context2D()->setLineDash(simple_dash);
  EXPECT_EQ(simple_dash, Context2D()->getLineDash());
  Context2D()->save();
  // Realize the save.
  Context2D()->scale(2, 2);
  EXPECT_EQ(simple_dash, Context2D()->getLineDash());
  Context2D()->restore();
  EXPECT_EQ(simple_dash, Context2D()->getLineDash());
}

TEST_F(CanvasRenderingContext2DAPITest, CreateImageData) {
  CreateContext(kNonOpaque);

  NonThrowableExceptionState exception_state;

  // create a 100x50 imagedata and fill it with white pixels
  ImageDataSettings* settings = ImageDataSettings::Create();
  ImageData* image_data =
      Context2D()->createImageData(100, 50, settings, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(100, image_data->width());
  EXPECT_EQ(50, image_data->height());

  for (size_t i = 0; i < image_data->data()->GetAsUint8ClampedArray()->length();
       ++i) {
    image_data->data()->GetAsUint8ClampedArray()->Data()[i] = 255;
  }

  EXPECT_EQ(255, image_data->data()->GetAsUint8ClampedArray()->Data()[32]);

  // createImageData(imageData) should create a new ImageData of the same size
  // as 'imageData' but filled with transparent black

  ImageData* same_size_image_data =
      Context2D()->createImageData(image_data, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(100, same_size_image_data->width());
  EXPECT_EQ(50, same_size_image_data->height());
  EXPECT_EQ(0,
            same_size_image_data->data()->GetAsUint8ClampedArray()->Data()[32]);

  // createImageData(width, height) takes the absolute magnitude of the size
  // arguments

  ImageData* imgdata1 =
      Context2D()->createImageData(10, 20, settings, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ImageData* imgdata2 =
      Context2D()->createImageData(-10, 20, settings, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ImageData* imgdata3 =
      Context2D()->createImageData(10, -20, settings, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ImageData* imgdata4 =
      Context2D()->createImageData(-10, -20, settings, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(800u, imgdata1->data()->GetAsUint8ClampedArray()->length());
  EXPECT_EQ(800u, imgdata2->data()->GetAsUint8ClampedArray()->length());
  EXPECT_EQ(800u, imgdata3->data()->GetAsUint8ClampedArray()->length());
  EXPECT_EQ(800u, imgdata4->data()->GetAsUint8ClampedArray()->length());
}

TEST_F(CanvasRenderingContext2DAPITest, CreateImageDataTooBig) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  ImageDataSettings* settings = ImageDataSettings::Create();
  ImageData* too_big_image_data =
      Context2D()->createImageData(1000000, 1000000, settings, exception_state);
  EXPECT_EQ(nullptr, too_big_image_data);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kRangeError, exception_state.CodeAs<ESErrorType>());
}

TEST_F(CanvasRenderingContext2DAPITest, GetImageDataTooBig) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  ImageDataSettings* settings = ImageDataSettings::Create();
  ImageData* image_data = Context2D()->getImageData(0, 0, 1000000, 1000000,
                                                    settings, exception_state);
  EXPECT_EQ(nullptr, image_data);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kRangeError, exception_state.CodeAs<ESErrorType>());
}

TEST_F(CanvasRenderingContext2DAPITest,
       GetImageDataIntegerOverflowNegativeParams) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  ImageDataSettings* settings = ImageDataSettings::Create();
  ImageData* image_data = Context2D()->getImageData(
      1, -2147483647, 1, -2147483647, settings, exception_state);
  EXPECT_EQ(nullptr, image_data);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kRangeError, exception_state.CodeAs<ESErrorType>());

  exception_state.ClearException();
  image_data = Context2D()->getImageData(-2147483647, 1, -2147483647, 1,
                                         settings, exception_state);
  EXPECT_EQ(nullptr, image_data);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kRangeError, exception_state.CodeAs<ESErrorType>());
}

void ResetCanvasForAccessibilityRectTest(Document& document) {
  document.documentElement()->setInnerHTML(R"HTML(
    <canvas id='canvas' style='position:absolute; top:0px; left:0px;
    padding:10px; margin:5px;'>
    <button id='button'></button></canvas>
  )HTML");
  auto* canvas = To<HTMLCanvasElement>(document.getElementById("canvas"));

  String canvas_type("2d");
  CanvasContextCreationAttributesCore attributes;
  attributes.alpha = true;
  canvas->GetCanvasRenderingContext(canvas_type, attributes);

  EXPECT_NE(nullptr, canvas->RenderingContext());
  EXPECT_TRUE(canvas->RenderingContext()->IsRenderingContext2D());
}


// A IdentifiabilityStudySettingsProvider implementation that opts-into study
// participation.
class ActiveSettingsProvider : public IdentifiabilityStudySettingsProvider {
 public:
  explicit ActiveSettingsProvider(bool enabled) : enabled_(enabled) {}
  bool IsActive() const override { return enabled_; }
  bool IsAnyTypeOrSurfaceBlocked() const override { return false; }
  bool IsSurfaceAllowed(IdentifiableSurface surface) const override {
    return true;
  }
  bool IsTypeAllowed(IdentifiableSurface::Type type) const override {
    return true;
  }
  bool ShouldActivelySample() const override { return false; }

 private:
  const bool enabled_ = true;
};

// An RAII class that opts into study participation using
// ActiveSettingsProvider.
class StudyParticipationRaii {
 public:
  explicit StudyParticipationRaii(bool enabled = true) {
    IdentifiabilityStudySettings::SetGlobalProvider(
        std::make_unique<ActiveSettingsProvider>(enabled));
  }
  ~StudyParticipationRaii() {
    IdentifiabilityStudySettings::ResetStateForTesting();
  }
};

TEST_F(CanvasRenderingContext2DAPITest, IdentifiabilityStudyMaxOperations) {
  StudyParticipationRaii study_participation_raii;
  constexpr int kMaxOperations = 5;
  IdentifiabilityStudyHelper::ScopedMaxOperationsSetter max_operations_setter(
      kMaxOperations);
  CreateContext(kNonOpaque);

  int64_t last_digest = INT64_C(0);
  for (int i = 0; i < kMaxOperations; i++) {
    Context2D()->setFont("Arial");
    EXPECT_NE(last_digest,
              Context2D()->IdentifiableTextToken().ToUkmMetricValue())
        << i;
    last_digest = Context2D()->IdentifiableTextToken().ToUkmMetricValue();
  }

  Context2D()->setFont("Arial");
  EXPECT_EQ(last_digest,
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_TRUE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredPartiallyDigestedImage());
}

// TODO(crbug.com/1239374): Fix test on Android L and re-enable.
// TODO(crbug.com/1258605): Fix test on Windows and re-enable.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_IdentifiabilityStudyDigest_Font \
  DISABLED_IdentifiabilityStudyDigest_Font
#else
#define MAYBE_IdentifiabilityStudyDigest_Font IdentifiabilityStudyDigest_Font
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(CanvasRenderingContext2DAPITest, MAYBE_IdentifiabilityStudyDigest_Font) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  Context2D()->setFont("Arial");
  EXPECT_EQ(INT64_C(-7111871220951205888),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredPartiallyDigestedImage());
}

TEST_F(CanvasRenderingContext2DAPITest, IdentifiabilityStudyDisabled) {
  StudyParticipationRaii study_participation_raii(/*enabled=*/false);
  constexpr int64_t kTokenBuilderInitialDigest = INT64_C(6544625333304541877);

  CreateContext(kNonOpaque);

  Context2D()->setFont("Arial");
  EXPECT_EQ(kTokenBuilderInitialDigest,
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredPartiallyDigestedImage());
}

// TODO(crbug.com/1239374): Fix test on Android and re-enable.
// TODO(crbug.com/1258605): Fix test on Windows and re-enable.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_IdentifiabilityStudyDigest_StrokeText \
  DISABLED_IdentifiabilityStudyDigest_StrokeText
#else
#define MAYBE_IdentifiabilityStudyDigest_StrokeText \
  IdentifiabilityStudyDigest_StrokeText
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(CanvasRenderingContext2DAPITest,
       MAYBE_IdentifiabilityStudyDigest_StrokeText) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  Context2D()->strokeText("Sensitive message", 1.0, 1.0);
  EXPECT_EQ(INT64_C(2232415440872807707),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_TRUE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredPartiallyDigestedImage());
}

// TODO(crbug.com/1239374): Fix test on Android and re-enable.
// TODO(crbug.com/1258605): Fix test on Windows and re-enable.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_IdentifiabilityStudyDigest_FillText \
  DISABLED_IdentifiabilityStudyDigest_FillText
#else
#define MAYBE_IdentifiabilityStudyDigest_FillText \
  IdentifiabilityStudyDigest_FillText
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(CanvasRenderingContext2DAPITest,
       MAYBE_IdentifiabilityStudyDigest_FillText) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  Context2D()->fillText("Sensitive message", 1.0, 1.0);
  EXPECT_EQ(INT64_C(6317349156921019980),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_TRUE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredPartiallyDigestedImage());
}

// TODO(crbug.com/1239374): Fix test on Android and re-enable.
// TODO(crbug.com/1258605): Fix test on Windows and re-enable.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_IdentifiabilityStudyDigest_TextAlign \
  DISABLED_IdentifiabilityStudyDigest_TextAlign
#else
#define MAYBE_IdentifiabilityStudyDigest_TextAlign \
  IdentifiabilityStudyDigest_TextAlign
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(CanvasRenderingContext2DAPITest,
       MAYBE_IdentifiabilityStudyDigest_TextAlign) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  Context2D()->setTextAlign("center");
  EXPECT_EQ(INT64_C(-1799394612814265049),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredPartiallyDigestedImage());
}

// TODO(crbug.com/1239374): Fix test on Android and re-enable.
// TODO(crbug.com/1258605): Fix test on Windows and re-enable.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_IdentifiabilityStudyDigest_TextBaseline \
  DISABLED_IdentifiabilityStudyDigest_TextBaseline
#else
#define MAYBE_IdentifiabilityStudyDigest_TextBaseline \
  IdentifiabilityStudyDigest_TextBaseline
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(CanvasRenderingContext2DAPITest,
       MAYBE_IdentifiabilityStudyDigest_TextBaseline) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  Context2D()->setTextBaseline("top");
  EXPECT_EQ(INT64_C(-7620161594820691651),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredPartiallyDigestedImage());
}

// TODO(crbug.com/1239374): Fix test on Android and re-enable.
// TODO(crbug.com/1258605): Fix test on Windows and re-enable.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_IdentifiabilityStudyDigest_StrokeStyle \
  DISABLED_IdentifiabilityStudyDigest_StrokeStyle
#else
#define MAYBE_IdentifiabilityStudyDigest_StrokeStyle \
  IdentifiabilityStudyDigest_StrokeStyle
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(CanvasRenderingContext2DAPITest,
       MAYBE_IdentifiabilityStudyDigest_StrokeStyle) {
  v8::HandleScope handle_scope(GetIsolate());
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  SetStrokeStyleString(Context2D(), GetScriptState(), "blue");
  EXPECT_EQ(INT64_C(-1964835352532316734),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredPartiallyDigestedImage());
}

// TODO(crbug.com/1239374): Fix test on Android and re-enable.
// TODO(crbug.com/1258605): Fix test on Windows and re-enable.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_IdentifiabilityStudyDigest_FillStyle \
  DISABLED_IdentifiabilityStudyDigest_FillStyle
#else
#define MAYBE_IdentifiabilityStudyDigest_FillStyle \
  IdentifiabilityStudyDigest_FillStyle
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(CanvasRenderingContext2DAPITest,
       MAYBE_IdentifiabilityStudyDigest_FillStyle) {
  v8::HandleScope handle_scope(GetIsolate());
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  SetFillStyleString(Context2D(), GetScriptState(), "blue");
  EXPECT_EQ(INT64_C(-4860826471555317536),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredPartiallyDigestedImage());
}

// TODO(crbug.com/1239374): Fix test on Android and re-enable.
// TODO(crbug.com/1258605): Fix test on Windows and re-enable.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_IdentifiabilityStudyDigest_Combo \
  DISABLED_IdentifiabilityStudyDigest_Combo
#else
#define MAYBE_IdentifiabilityStudyDigest_Combo IdentifiabilityStudyDigest_Combo
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(CanvasRenderingContext2DAPITest,
       MAYBE_IdentifiabilityStudyDigest_Combo) {
  v8::HandleScope handle_scope(GetIsolate());
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  Context2D()->fillText("Sensitive message", 1.0, 1.0);
  EXPECT_EQ(INT64_C(6317349156921019980),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());
  Context2D()->setFont("Helvetica");
  Context2D()->setTextBaseline("bottom");
  Context2D()->setTextAlign("right");
  SetFillStyleString(Context2D(), GetScriptState(), "red");
  Context2D()->fillText("Bye", 4.0, 3.0);
  EXPECT_EQ(INT64_C(5574475585707445774),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_TRUE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredPartiallyDigestedImage());
}

// TODO(crbug.com/1239374): Fix test on Android L and re-enable.
// TODO(crbug.com/1258605): Fix test on Windows and re-enable.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_IdentifiabilityStudyDigest_putImageData \
  DISABLED_IdentifiabilityStudyDigest_putImageData
#else
#define MAYBE_IdentifiabilityStudyDigest_putImageData \
  IdentifiabilityStudyDigest_putImageData
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(CanvasRenderingContext2DAPITest,
       MAYBE_IdentifiabilityStudyDigest_putImageData) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);
  NonThrowableExceptionState exception_state;

  ImageData* image_data =
      Context2D()->createImageData(/*sw=*/1, /*sh=*/1, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  Context2D()->putImageData(image_data, /*dx=*/1, /*dy=*/1, exception_state);
  EXPECT_EQ(INT64_C(2821795876044191773),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
  EXPECT_TRUE(Context2D()->IdentifiabilityEncounteredPartiallyDigestedImage());
}

// TODO(crbug.com/1239374): Fix test on Android L and re-enable.
// TODO(crbug.com/1258605): Fix test on Windows and re-enable.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_IdentifiabilityStudyDigest_drawImage \
  DISABLED_IdentifiabilityStudyDigest_drawImage
#else
#define MAYBE_IdentifiabilityStudyDigest_drawImage \
  IdentifiabilityStudyDigest_drawImage
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(CanvasRenderingContext2DAPITest,
       MAYBE_IdentifiabilityStudyDigest_drawImage) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);
  NonThrowableExceptionState exception_state;

  // We can use our own canvas as the image source!
  auto* image_source =
      MakeGarbageCollected<V8CanvasImageSource>(&CanvasElement());
  Context2D()->drawImage(image_source, /*x=*/1,
                         /*y=*/1, exception_state);
  EXPECT_EQ(INT64_C(-4851825694092845811),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
  EXPECT_TRUE(Context2D()->IdentifiabilityEncounteredPartiallyDigestedImage());
}

}  // namespace blink
