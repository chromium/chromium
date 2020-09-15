// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"

#include <memory>
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hit_region_options.h"
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
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"

using testing::Mock;

namespace blink {

class CanvasRenderingContext2DAPITest : public PageTestBase {
 protected:
  CanvasRenderingContext2DAPITest();
  void SetUp() override;

  HTMLCanvasElement& CanvasElement() const { return *canvas_element_; }
  CanvasRenderingContext2D* Context2D() const;

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

String TrySettingStrokeStyle(CanvasRenderingContext2D* ctx,
                             const String& value) {
  StringOrCanvasGradientOrCanvasPattern arg1, arg2, arg3;
  arg1.SetString("#666");
  ctx->setStrokeStyle(arg1);
  arg2.SetString(value);
  ctx->setStrokeStyle(arg2);
  ctx->strokeStyle(arg3);
  EXPECT_TRUE(arg3.IsString());
  return arg3.GetAsString();
}

String TrySettingFillStyle(CanvasRenderingContext2D* ctx, const String& value) {
  StringOrCanvasGradientOrCanvasPattern arg1, arg2, arg3;
  arg1.SetString("#666");
  ctx->setFillStyle(arg1);
  arg2.SetString(value);
  ctx->setFillStyle(arg2);
  ctx->fillStyle(arg3);
  EXPECT_TRUE(arg3.IsString());
  return arg3.GetAsString();
}

String TrySettingShadowColor(CanvasRenderingContext2D* ctx,
                             const String& value) {
  ctx->setShadowColor("#666");
  ctx->setShadowColor(value);
  return ctx->shadowColor();
}

void TrySettingColor(CanvasRenderingContext2D* ctx,
                     const String& value,
                     const String& expected) {
  EXPECT_EQ(expected, TrySettingStrokeStyle(ctx, value));
  EXPECT_EQ(expected, TrySettingFillStyle(ctx, value));
  EXPECT_EQ(expected, TrySettingShadowColor(ctx, value));
}

TEST_F(CanvasRenderingContext2DAPITest, ColorSerialization) {
  CreateContext(kNonOpaque);
  // Check round trips
  TrySettingColor(Context2D(), "transparent", "rgba(0, 0, 0, 0)");
  TrySettingColor(Context2D(), "red", "#ff0000");
  TrySettingColor(Context2D(), "white", "#ffffff");
  TrySettingColor(Context2D(), "", "#666666");
  TrySettingColor(Context2D(), "RGBA(0, 0, 0, 0)", "rgba(0, 0, 0, 0)");
  TrySettingColor(Context2D(), "rgba(0,255,0,1.0)", "#00ff00");
  TrySettingColor(Context2D(), "rgba(1,2,3,0.4)", "rgba(1, 2, 3, 0.4)");
  TrySettingColor(Context2D(), "RgB(1,2,3)", "#010203");
  TrySettingColor(Context2D(), "rGbA(1,2,3,0)", "rgba(1, 2, 3, 0)");
}

TEST_F(CanvasRenderingContext2DAPITest, DefaultAttributeValues) {
  CreateContext(kNonOpaque);

  {
    StringOrCanvasGradientOrCanvasPattern value;
    Context2D()->strokeStyle(value);
    EXPECT_TRUE(value.IsString());
    EXPECT_EQ(String("#000000"), value.GetAsString());
  }

  {
    StringOrCanvasGradientOrCanvasPattern value;
    Context2D()->fillStyle(value);
    EXPECT_TRUE(value.IsString());
    EXPECT_EQ(String("#000000"), value.GetAsString());
  }

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
  ImageData* image_data =
      Context2D()->createImageData(100, 50, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(100, image_data->width());
  EXPECT_EQ(50, image_data->height());

  for (size_t i = 0;
       i < image_data->data().GetAsUint8ClampedArray()->lengthAsSizeT(); ++i) {
    image_data->data().GetAsUint8ClampedArray()->Data()[i] = 255;
  }

  EXPECT_EQ(255, image_data->data().GetAsUint8ClampedArray()->Data()[32]);

  // createImageData(imageData) should create a new ImageData of the same size
  // as 'imageData' but filled with transparent black

  ImageData* same_size_image_data =
      Context2D()->createImageData(image_data, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(100, same_size_image_data->width());
  EXPECT_EQ(50, same_size_image_data->height());
  EXPECT_EQ(0,
            same_size_image_data->data().GetAsUint8ClampedArray()->Data()[32]);

  // createImageData(width, height) takes the absolute magnitude of the size
  // arguments

  ImageData* imgdata1 = Context2D()->createImageData(10, 20, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ImageData* imgdata2 = Context2D()->createImageData(-10, 20, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ImageData* imgdata3 = Context2D()->createImageData(10, -20, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ImageData* imgdata4 = Context2D()->createImageData(-10, -20, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(800u, imgdata1->data().GetAsUint8ClampedArray()->lengthAsSizeT());
  EXPECT_EQ(800u, imgdata2->data().GetAsUint8ClampedArray()->lengthAsSizeT());
  EXPECT_EQ(800u, imgdata3->data().GetAsUint8ClampedArray()->lengthAsSizeT());
  EXPECT_EQ(800u, imgdata4->data().GetAsUint8ClampedArray()->lengthAsSizeT());
}

TEST_F(CanvasRenderingContext2DAPITest, CreateImageDataTooBig) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  ImageData* too_big_image_data =
      Context2D()->createImageData(1000000, 1000000, exception_state);
  EXPECT_EQ(nullptr, too_big_image_data);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kRangeError, exception_state.CodeAs<ESErrorType>());
}

TEST_F(CanvasRenderingContext2DAPITest, GetImageDataTooBig) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  ImageData* image_data =
      Context2D()->getImageData(0, 0, 1000000, 1000000, exception_state);
  EXPECT_EQ(nullptr, image_data);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kRangeError, exception_state.CodeAs<ESErrorType>());
}

TEST_F(CanvasRenderingContext2DAPITest,
       GetImageDataIntegerOverflowNegativeParams) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  ImageData* image_data = Context2D()->getImageData(
      1, -2147483647, 1, -2147483647, exception_state);
  EXPECT_EQ(nullptr, image_data);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kRangeError, exception_state.CodeAs<ESErrorType>());

  exception_state.ClearException();
  image_data = Context2D()->getImageData(-2147483647, 1, -2147483647, 1,
                                         exception_state);
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

TEST_F(CanvasRenderingContext2DAPITest, AccessibilityRectTestForAddHitRegion) {
  ResetCanvasForAccessibilityRectTest(GetDocument());
  AXContext ax_context(GetDocument());

  Element* button_element = GetDocument().getElementById("button");
  auto* canvas = To<HTMLCanvasElement>(GetDocument().getElementById("canvas"));
  CanvasRenderingContext2D* context =
      static_cast<CanvasRenderingContext2D*>(canvas->RenderingContext());

  NonThrowableExceptionState exception_state;
  HitRegionOptions* options = HitRegionOptions::Create();
  options->setControl(button_element);

  context->beginPath();
  context->rect(10, 10, 40, 40);
  context->addHitRegion(options, exception_state);

  auto* ax_object_cache =
      To<AXObjectCacheImpl>(GetDocument().ExistingAXObjectCache());
  AXObject* ax_object = ax_object_cache->GetOrCreate(button_element);

  LayoutRect ax_bounds = ax_object->GetBoundsInFrameCoordinates();
  EXPECT_EQ(25, ax_bounds.X().ToInt());
  EXPECT_EQ(25, ax_bounds.Y().ToInt());
  EXPECT_EQ(40, ax_bounds.Width().ToInt());
  EXPECT_EQ(40, ax_bounds.Height().ToInt());
}

TEST_F(CanvasRenderingContext2DAPITest,
       AccessibilityRectTestForDrawFocusIfNeeded) {
  ResetCanvasForAccessibilityRectTest(GetDocument());
  AXContext ax_context(GetDocument());

  Element* button_element = GetDocument().getElementById("button");
  auto* canvas = To<HTMLCanvasElement>(GetDocument().getElementById("canvas"));
  CanvasRenderingContext2D* context =
      static_cast<CanvasRenderingContext2D*>(canvas->RenderingContext());

  GetDocument().UpdateStyleAndLayoutTreeForNode(canvas);

  context->beginPath();
  context->rect(10, 10, 40, 40);
  context->drawFocusIfNeeded(button_element);

  auto* ax_object_cache =
      To<AXObjectCacheImpl>(GetDocument().ExistingAXObjectCache());
  AXObject* ax_object = ax_object_cache->GetOrCreate(button_element);

  LayoutRect ax_bounds = ax_object->GetBoundsInFrameCoordinates();
  EXPECT_EQ(25, ax_bounds.X().ToInt());
  EXPECT_EQ(25, ax_bounds.Y().ToInt());
  EXPECT_EQ(40, ax_bounds.Width().ToInt());
  EXPECT_EQ(40, ax_bounds.Height().ToInt());
}

// A IdentifiabilityStudySettingsProvider implementation that opts-into study
// participation.
class ActiveSettingsProvider : public IdentifiabilityStudySettingsProvider {
 public:
  bool IsActive() const override { return true; }

  // The following return values don't matter.
  bool IsAnyTypeOrSurfaceBlocked() const override { return true; }
  bool IsSurfaceAllowed(IdentifiableSurface surface) const override {
    return false;
  }
  bool IsTypeAllowed(IdentifiableSurface::Type type) const override {
    return false;
  }
};

// An RAII class that opts into study participation using
// ActiveSettingsProvider.
class StudyParticipationRaii {
 public:
  StudyParticipationRaii() {
    IdentifiabilityStudySettings::SetGlobalProvider(
        std::make_unique<ActiveSettingsProvider>());
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
}

TEST_F(CanvasRenderingContext2DAPITest, IdentifiabilityStudyDigest_Font) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  Context2D()->setFont("Arial");
  EXPECT_EQ(INT64_C(4260982106376580867),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
}

TEST_F(CanvasRenderingContext2DAPITest, IdentifiabilityStudyDigest_StrokeText) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  Context2D()->strokeText("Sensitive message", 1.0, 1.0);
  EXPECT_EQ(INT64_C(-2943272460643878232),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_TRUE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
}

TEST_F(CanvasRenderingContext2DAPITest, IdentifiabilityStudyDigest_FillText) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  Context2D()->fillText("Sensitive message", 1.0, 1.0);
  EXPECT_EQ(INT64_C(8733208206881150098),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_TRUE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
}

TEST_F(CanvasRenderingContext2DAPITest, IdentifiabilityStudyDigest_TextAlign) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  Context2D()->setTextAlign("center");
  EXPECT_EQ(INT64_C(-4778938416456134710),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
}

TEST_F(CanvasRenderingContext2DAPITest,
       IdentifiabilityStudyDigest_TextBaseline) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  Context2D()->setTextBaseline("top");
  EXPECT_EQ(INT64_C(-3065573128425485855),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
}

TEST_F(CanvasRenderingContext2DAPITest,
       IdentifiabilityStudyDigest_StrokeStyle) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  StringOrCanvasGradientOrCanvasPattern style;
  style.SetString("blue");
  Context2D()->setStrokeStyle(style);
  EXPECT_EQ(INT64_C(2059186787917525779),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
}

TEST_F(CanvasRenderingContext2DAPITest, IdentifiabilityStudyDigest_FillStyle) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  StringOrCanvasGradientOrCanvasPattern style;
  style.SetString("blue");
  Context2D()->setFillStyle(style);
  EXPECT_EQ(INT64_C(-6322980727372024031),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
}

TEST_F(CanvasRenderingContext2DAPITest, IdentifiabilityStudyDigest_Combo) {
  StudyParticipationRaii study_participation_raii;
  CreateContext(kNonOpaque);

  Context2D()->fillText("Sensitive message", 1.0, 1.0);
  EXPECT_EQ(INT64_C(8733208206881150098),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());
  Context2D()->setFont("Helvetica");
  Context2D()->setTextBaseline("bottom");
  Context2D()->setTextAlign("right");
  StringOrCanvasGradientOrCanvasPattern style;
  style.SetString("red");
  Context2D()->setFillStyle(style);
  Context2D()->fillText("Bye", 4.0, 3.0);
  EXPECT_EQ(INT64_C(2368400155273386771),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());

  EXPECT_FALSE(Context2D()->IdentifiabilityEncounteredSkippedOps());
  EXPECT_TRUE(Context2D()->IdentifiabilityEncounteredSensitiveOps());
}

}  // namespace blink
