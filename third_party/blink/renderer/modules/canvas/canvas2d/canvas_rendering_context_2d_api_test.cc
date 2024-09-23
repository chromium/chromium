// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "cc/paint/refcounted_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings_provider.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_data_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_float32array_uint16array_uint8clampedarray.h"  // IWYU pragma: keep
#include "third_party/blink/renderer/bindings/modules/v8/v8_begin_layer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_blob_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_imagedata_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style_test_utils.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/identifiability_study_helper.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/mesh_2d_index_buffer.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/mesh_2d_uv_buffer.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/mesh_2d_vertex_buffer.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_factories.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/private/base/SkPoint_impl.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-local-handle.h"

// GoogleTest expectation macros trigger a bug in IWYU:
// https://github.com/include-what-you-use/include-what-you-use/issues/1546
// IWYU pragma: no_include <string>
// IWYU pragma: no_include <tuple>

namespace v8 {
class Isolate;
}  // namespace v8

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
  canvas_element_ =
      To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));
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
  NonThrowableExceptionState exception_state;
  Context2D()->restore(exception_state);
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
  ImageDataSettings* settings = ImageDataSettings::Create();
  {
    DummyExceptionStateForTesting exception_state;
    ImageData* image_data = Context2D()->getImageData(
        1, -2147483647, 1, -2147483647, settings, exception_state);
    EXPECT_EQ(nullptr, image_data);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(ESErrorType::kRangeError, exception_state.CodeAs<ESErrorType>());
  }

  {
    DummyExceptionStateForTesting exception_state;
    ImageData* image_data = Context2D()->getImageData(
        -2147483647, 1, -2147483647, 1, settings, exception_state);
    EXPECT_EQ(nullptr, image_data);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(ESErrorType::kRangeError, exception_state.CodeAs<ESErrorType>());
  }
}

// Checks `CreateImageBitmap` throws an exception if called inside a layer.
TEST_F(CanvasRenderingContext2DAPITest, UnclosedLayerCreateImageBitmap) {
  V8TestingScope scope;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  CreateContext(kNonOpaque);

  NonThrowableExceptionState no_exception;
  auto* image = MakeGarbageCollected<V8ImageBitmapSource>(&CanvasElement());
  auto* options = ImageBitmapOptions::Create();

  Context2D()->beginLayer(GetScriptState(), BeginLayerOptions::Create(),
                          no_exception);

  // Throws inside layers:
  DummyExceptionStateForTesting exception_state;
  ImageBitmapFactories::CreateImageBitmap(GetScriptState(), image, options,
                                          exception_state);
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  Context2D()->endLayer(no_exception);

  // Doesn't throw outside layers:
  ImageBitmapFactories::CreateImageBitmap(GetScriptState(), image, options,
                                          no_exception);
}

// Checks `createPattern` throws an exception the source has unclosed layers.
TEST_F(CanvasRenderingContext2DAPITest, UnclosedLayerCreatePattern) {
  V8TestingScope scope;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  CreateContext(kNonOpaque);

  NonThrowableExceptionState no_exception;
  auto* image = MakeGarbageCollected<V8CanvasImageSource>(&CanvasElement());

  Context2D()->beginLayer(GetScriptState(), BeginLayerOptions::Create(),
                          no_exception);

  // Throws inside layers:
  DummyExceptionStateForTesting exception_state;
  Context2D()->createPattern(image, "repeat", exception_state);
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  Context2D()->endLayer(no_exception);

  // Doesn't throw outside layers:
  Context2D()->createPattern(image, "repeat", no_exception);
}

// Checks `drawImage` throws an exception the source has unclosed layers.
TEST_F(CanvasRenderingContext2DAPITest, UnclosedLayerDrawImage) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  CreateContext(kNonOpaque);

  NonThrowableExceptionState no_exception;
  auto* image = MakeGarbageCollected<V8CanvasImageSource>(&CanvasElement());

  Context2D()->beginLayer(GetScriptState(), BeginLayerOptions::Create(),
                          no_exception);

  // Throws inside layers:
  DummyExceptionStateForTesting exception_state;
  Context2D()->drawImage(image, /*x=*/0, /*y=*/0, exception_state);
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  Context2D()->endLayer(no_exception);

  // Doesn't throw outside layers:
  Context2D()->drawImage(image, /*x=*/0, /*y=*/0, no_exception);
}

// Checks `getImageData` throws an exception if called inside a layer.
TEST_F(CanvasRenderingContext2DAPITest, UnclosedLayerGetImageData) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  CreateContext(kNonOpaque);
  NonThrowableExceptionState no_exception;

  Context2D()->beginLayer(GetScriptState(), BeginLayerOptions::Create(),
                          no_exception);

  // Throws inside layers:
  DummyExceptionStateForTesting exception_state;
  Context2D()->getImageData(/*sx=*/0, /*sy=*/0, /*sw=*/1, /*sh=*/1,
                            exception_state);
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  Context2D()->endLayer(no_exception);

  // Doesn't throw outside layers:
  Context2D()->getImageData(/*sx=*/0, /*sy=*/0, /*sw=*/1, /*sh=*/1,
                            no_exception);
}

// Checks `putImageData` throws an exception if called inside a layer.
TEST_F(CanvasRenderingContext2DAPITest, UnclosedLayerPutImageData) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  CreateContext(kNonOpaque);

  NonThrowableExceptionState no_exception;
  ImageData* image_data = ImageData::Create(
      Context2D()->Width(), Context2D()->Height(), no_exception);

  Context2D()->beginLayer(GetScriptState(), BeginLayerOptions::Create(),
                          no_exception);

  // Throws inside layers:
  DummyExceptionStateForTesting exception_state;
  Context2D()->putImageData(image_data, /*dx=*/0, /*dy=*/0, exception_state);
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  Context2D()->endLayer(no_exception);

  // Doesn't throw outside layers:
  Context2D()->putImageData(image_data, /*dx=*/0, /*dy=*/0, no_exception);
}

// Checks `toBlob` throws an exception if called inside a layer.
TEST_F(CanvasRenderingContext2DAPITest, UnclosedLayerToBlob) {
  V8TestingScope scope;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  CreateContext(kNonOpaque);

  NonThrowableExceptionState no_exception;
  auto* callback = V8BlobCallback::Create(scope.GetContext()->Global());

  Context2D()->beginLayer(GetScriptState(), BeginLayerOptions::Create(),
                          no_exception);

  // Throws inside layers:
  DummyExceptionStateForTesting exception_state;
  CanvasElement().toBlob(callback, /*mime_type=*/"image/png", exception_state);
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  Context2D()->endLayer(no_exception);

  // Doesn't throw outside layers:
  CanvasElement().toBlob(callback, /*mime_type=*/"image/png", no_exception);
}

// Checks `toDataURL` throws an exception if called inside a layer.
TEST_F(CanvasRenderingContext2DAPITest, UnclosedLayerToDataUrl) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  CreateContext(kNonOpaque);
  NonThrowableExceptionState no_exception;

  Context2D()->beginLayer(GetScriptState(), BeginLayerOptions::Create(),
                          no_exception);

  // Throws inside layers:
  DummyExceptionStateForTesting exception_state;
  CanvasElement().toDataURL(/*mime_type=*/"image/png", exception_state);
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  Context2D()->endLayer(no_exception);

  // Doesn't throw outside layers:
  CanvasElement().toDataURL(/*mime_type=*/"image/png", no_exception);
}

// Checks `drawMesh` throws an exception if called inside a layer.
TEST_F(CanvasRenderingContext2DAPITest, UnclosedLayerDrawMesh) {
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  CreateContext(kNonOpaque);

  NonThrowableExceptionState no_exception;
  auto* image = MakeGarbageCollected<V8CanvasImageSource>(&CanvasElement());

  const Mesh2DVertexBuffer* vbuf = MakeGarbageCollected<Mesh2DVertexBuffer>(
      base::MakeRefCounted<cc::RefCountedBuffer<SkPoint>>(
          std::vector<SkPoint>{{0, 0}, {100, 0}, {100, 100}}));
  const Mesh2DUVBuffer* uvbuf = MakeGarbageCollected<Mesh2DUVBuffer>(
      base::MakeRefCounted<cc::RefCountedBuffer<SkPoint>>(
          std::vector<SkPoint>{{0, 0}, {1, 0}, {1, 1}}));
  const Mesh2DIndexBuffer* ibuf = MakeGarbageCollected<Mesh2DIndexBuffer>(
      base::MakeRefCounted<cc::RefCountedBuffer<uint16_t>>(
          std::vector<uint16_t>{0, 1, 2}));

  Context2D()->beginLayer(GetScriptState(), BeginLayerOptions::Create(),
                          no_exception);

  // Throws inside layers:
  DummyExceptionStateForTesting exception_state;
  Context2D()->drawMesh(vbuf, uvbuf, ibuf, image, exception_state);
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  Context2D()->endLayer(no_exception);

  // Doesn't throw outside layers:
  Context2D()->drawMesh(vbuf, uvbuf, ibuf, image, no_exception);
}

void ResetCanvasForAccessibilityRectTest(Document& document) {
  document.documentElement()->setInnerHTML(R"HTML(
    <canvas id='canvas' style='position:absolute; top:0px; left:0px;
    padding:10px; margin:5px;'>
    <button id='button'></button></canvas>
  )HTML");
  auto* canvas =
      To<HTMLCanvasElement>(document.getElementById(AtomicString("canvas")));

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
  bool IsMetaExperimentActive() const override { return false; }
  bool IsActive() const override { return enabled_; }
  bool IsAnyTypeOrSurfaceBlocked() const override { return false; }
  bool IsSurfaceAllowed(IdentifiableSurface surface) const override {
    return true;
  }
  bool IsTypeAllowed(IdentifiableSurface::Type type) const override {
    return true;
  }

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
  EXPECT_EQ(INT64_C(7339381412423806682),
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
  EXPECT_EQ(INT64_C(8218678546639211996),
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
  EXPECT_EQ(INT64_C(-7525055925911674050),
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
  EXPECT_EQ(INT64_C(-5618040280239325003),
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
  EXPECT_EQ(INT64_C(-6814889525293785691),
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
  EXPECT_EQ(INT64_C(3577524355478740727),
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
  EXPECT_EQ(INT64_C(7953663110297373742),
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
  EXPECT_EQ(INT64_C(-7525055925911674050),
            Context2D()->IdentifiableTextToken().ToUkmMetricValue());
  Context2D()->setFont("Helvetica");
  Context2D()->setTextBaseline("bottom");
  Context2D()->setTextAlign("right");
  SetFillStyleString(Context2D(), GetScriptState(), "red");
  Context2D()->fillText("Bye", 4.0, 3.0);
  EXPECT_EQ(INT64_C(-7631959002534825456),
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

using testing::ElementsAre;
using testing::IsNull;
using testing::Pointee;

MATCHER_P(Mesh2dBufferIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.GetBuffer()->data(), result_listener);
}

NotShared<DOMFloat32Array> CreateFloat32Array(std::vector<float> array) {
  return NotShared<DOMFloat32Array>(DOMFloat32Array::Create(array));
}

NotShared<DOMUint16Array> CreateUint16Array(std::vector<uint16_t> array) {
  return NotShared<DOMUint16Array>(DOMUint16Array::Create(array));
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DVertexBuffer0Floats) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  EXPECT_THAT(Context2D()->createMesh2DVertexBuffer(CreateFloat32Array({}),
                                                    exception_state),
              IsNull());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DVertexBuffer1Float) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  EXPECT_THAT(Context2D()->createMesh2DVertexBuffer(CreateFloat32Array({101}),
                                                    exception_state),
              IsNull());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DVertexBuffer2Floats) {
  CreateContext(kNonOpaque);
  NonThrowableExceptionState exception_state;
  EXPECT_THAT(Context2D()->createMesh2DVertexBuffer(
                  CreateFloat32Array({101, 102}), exception_state),
              Pointee(Mesh2dBufferIs(ElementsAre(SkPoint(101, 102)))));
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DVertexBuffer3Floats) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  EXPECT_THAT(Context2D()->createMesh2DVertexBuffer(
                  CreateFloat32Array({101, 102, 103}), exception_state),
              IsNull());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DVertexBuffer4Floats) {
  CreateContext(kNonOpaque);
  NonThrowableExceptionState exception_state;
  EXPECT_THAT(Context2D()->createMesh2DVertexBuffer(
                  CreateFloat32Array({101, 102, 103, 104}), exception_state),
              Pointee(Mesh2dBufferIs(
                  ElementsAre(SkPoint(101, 102), SkPoint(103, 104)))));
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DUVBuffer0Floats) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  EXPECT_THAT(Context2D()->createMesh2DUVBuffer(CreateFloat32Array({}),
                                                exception_state),
              IsNull());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DUVBuffer1Float) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  EXPECT_THAT(Context2D()->createMesh2DUVBuffer(CreateFloat32Array({101}),
                                                exception_state),
              IsNull());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DUVBuffer2Floats) {
  CreateContext(kNonOpaque);
  NonThrowableExceptionState exception_state;
  EXPECT_THAT(Context2D()->createMesh2DUVBuffer(CreateFloat32Array({101, 102}),
                                                exception_state),
              Pointee(Mesh2dBufferIs(ElementsAre(SkPoint(101, 102)))));
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DUVBuffer3Floats) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  EXPECT_THAT(Context2D()->createMesh2DUVBuffer(
                  CreateFloat32Array({101, 102, 103}), exception_state),
              IsNull());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DUVBuffer4Floats) {
  CreateContext(kNonOpaque);
  NonThrowableExceptionState exception_state;
  EXPECT_THAT(Context2D()->createMesh2DUVBuffer(
                  CreateFloat32Array({101, 102, 103, 104}), exception_state),
              Pointee(Mesh2dBufferIs(
                  ElementsAre(SkPoint(101, 102), SkPoint(103, 104)))));
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DIndexBuffer0Uints) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  EXPECT_THAT(Context2D()->createMesh2DIndexBuffer(CreateUint16Array({}),
                                                   exception_state),
              IsNull());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DIndexBuffer1Uint) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  EXPECT_THAT(Context2D()->createMesh2DIndexBuffer(CreateUint16Array({1}),
                                                   exception_state),
              IsNull());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DIndexBuffer2Uints) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  EXPECT_THAT(Context2D()->createMesh2DIndexBuffer(CreateUint16Array({1, 2}),
                                                   exception_state),
              IsNull());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DUVBuffer3Uints) {
  CreateContext(kNonOpaque);
  NonThrowableExceptionState exception_state;
  EXPECT_THAT(Context2D()->createMesh2DIndexBuffer(CreateUint16Array({1, 2, 3}),
                                                   exception_state),
              Pointee(Mesh2dBufferIs(ElementsAre(1, 2, 3))));
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DIndexBuffer4Uints) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  EXPECT_THAT(Context2D()->createMesh2DIndexBuffer(
                  CreateUint16Array({1, 2, 3, 4}), exception_state),
              IsNull());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DIndexBuffer5Uints) {
  CreateContext(kNonOpaque);
  DummyExceptionStateForTesting exception_state;
  EXPECT_THAT(Context2D()->createMesh2DIndexBuffer(
                  CreateUint16Array({1, 2, 3, 4, 5}), exception_state),
              IsNull());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(CanvasRenderingContext2DAPITest, Mesh2DUVBuffer6Uints) {
  CreateContext(kNonOpaque);
  NonThrowableExceptionState exception_state;
  EXPECT_THAT(Context2D()->createMesh2DIndexBuffer(
                  CreateUint16Array({1, 2, 3, 4, 5, 6}), exception_state),
              Pointee(Mesh2dBufferIs(ElementsAre(1, 2, 3, 4, 5, 6))));
}

TEST_F(CanvasRenderingContext2DAPITest, DrawMesh) {
  CreateContext(kNonOpaque);
  CanvasRenderingContext2D* ctx = Context2D();
  V8CanvasImageSource* image_source =
      MakeGarbageCollected<V8CanvasImageSource>(&CanvasElement());

  DummyExceptionStateForTesting exception_state;
  const auto* vert_buffer = ctx->createMesh2DVertexBuffer(
      CreateFloat32Array({0, 0, 100, 0, 100, 100}), exception_state);
  ASSERT_NE(vert_buffer, nullptr);

  const auto* uv_buffer = ctx->createMesh2DUVBuffer(
      CreateFloat32Array({0, 0, 1, 0, 1, 1}), exception_state);
  ASSERT_NE(uv_buffer, nullptr);

  const auto* index_buffer = ctx->createMesh2DIndexBuffer(
      CreateUint16Array({0, 1, 2}), exception_state);
  ASSERT_NE(index_buffer, nullptr);

  ASSERT_FALSE(exception_state.HadException());

  // valid call
  ctx->drawMesh(vert_buffer, uv_buffer, index_buffer, image_source,
                exception_state);
  EXPECT_FALSE(exception_state.HadException());
}

}  // namespace blink
