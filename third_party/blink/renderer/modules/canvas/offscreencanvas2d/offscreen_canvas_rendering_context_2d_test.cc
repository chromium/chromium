// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_encode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_begin_layer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_blob_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_imagedata_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_factories.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

OffscreenCanvasRenderingContext2D* GetContext(V8TestingScope& scope,
                                              OffscreenCanvas* host) {
  CanvasRenderingContext* context = host->GetCanvasRenderingContext(
      scope.GetExecutionContext(),
      CanvasRenderingContext::CanvasRenderingAPI::k2D,
      CanvasContextCreationAttributesCore());
  CHECK(context->IsRenderingContext2D());
  return static_cast<OffscreenCanvasRenderingContext2D*>(context);
}

TEST(OffscreenCanvasHostTest,
     TransferToOffscreenThrowsErrorsProducedByContext) {
  test::TaskEnvironment task_environment_;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), /*width=*/1,
                                       /*height=*/1);
  OffscreenCanvasRenderingContext2D* context = GetContext(scope, host);

  // Make the context implementation of `transferToImageBitmap` fail by doing
  // an invalid operation (call `transferToImageBitmap` inside a layer).
  NonThrowableExceptionState no_exception;
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      no_exception);
  host->transferToImageBitmap(scope.GetScriptState(),
                              scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST(OffscreenCanvasHostTest, TransferToOffscreenThrowsUnknownErrorAsFallback) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  // Make `transferToImageBitmap` fail by creating the canvas that is too big.
  auto* host =
      OffscreenCanvas::Create(scope.GetScriptState(), /*width=*/100000000,
                              /*height=*/100000000);
  // A context must exist for `transferToImageBitmap` to work.
  GetContext(scope, host);

  host->transferToImageBitmap(scope.GetScriptState(),
                              scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kUnknownError);
}

TEST(OffscreenCanvasRenderingContext2DTest, TransferToOffscreenThrowsInLayers) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), /*width=*/10,
                                       /*height=*/10);
  OffscreenCanvasRenderingContext2D* context = GetContext(scope, host);
  NonThrowableExceptionState no_exception;
  // `TransferToImageBitmap` shouldn't throw on it's own.
  context->TransferToImageBitmap(scope.GetScriptState(), no_exception);
  // Make sure the exception isn't caused by calling the function twice.
  context->TransferToImageBitmap(scope.GetScriptState(), no_exception);
  // Calling again inside a layer should throw.
  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      no_exception);
  context->TransferToImageBitmap(scope.GetScriptState(),
                                 scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

// Checks `convertToBlob` throws an exception if called inside a layer.
TEST(OffscreenCanvasRenderingContext2DTest, UnclosedLayerConvertToBlob) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), /*width=*/10,
                                       /*height=*/10);
  OffscreenCanvasRenderingContext2D* context = GetContext(scope, host);

  NonThrowableExceptionState no_exception;
  auto* options = ImageEncodeOptions::Create();

  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      no_exception);

  // Throws inside layers:
  host->convertToBlob(scope.GetScriptState(), options,
                      scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  context->endLayer(no_exception);

  // Doesn't throw outside layers:
  host->convertToBlob(scope.GetScriptState(), options, no_exception);
}

// Checks `CreateImageBitmap` throws an exception if called inside a layer.
TEST(OffscreenCanvasRenderingContext2DTest, UnclosedLayerCreateImageBitmap) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), /*width=*/10,
                                       /*height=*/10);
  OffscreenCanvasRenderingContext2D* context = GetContext(scope, host);

  NonThrowableExceptionState no_exception;
  auto* image = MakeGarbageCollected<V8ImageBitmapSource>(host);
  auto* options = ImageBitmapOptions::Create();

  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      no_exception);

  // Throws inside layers:
  ImageBitmapFactories::CreateImageBitmap(scope.GetScriptState(), image,
                                          options, scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  context->endLayer(no_exception);

  // Doesn't throw outside layers:
  ImageBitmapFactories::CreateImageBitmap(scope.GetScriptState(), image,
                                          options, no_exception);
}

// Checks `createPattern` throws an exception the source has unclosed layers.
TEST(OffscreenCanvasRenderingContext2DTest, UnclosedLayerCreatePattern) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), /*width=*/10,
                                       /*height=*/10);
  OffscreenCanvasRenderingContext2D* context = GetContext(scope, host);

  NonThrowableExceptionState no_exception;
  auto* image = MakeGarbageCollected<V8CanvasImageSource>(host);

  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      no_exception);

  // Throws inside layers:
  context->createPattern(image, "repeat", scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  context->endLayer(no_exception);

  // Doesn't throw outside layers:
  context->createPattern(image, "repeat", no_exception);
}

// Checks `drawImage` throws an exception the source has unclosed layers.
TEST(OffscreenCanvasRenderingContext2DTest, UnclosedLayerDrawImage) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), /*width=*/10,
                                       /*height=*/10);
  OffscreenCanvasRenderingContext2D* context = GetContext(scope, host);

  NonThrowableExceptionState no_exception;
  auto* image = MakeGarbageCollected<V8CanvasImageSource>(host);

  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      no_exception);

  // Throws inside layers:
  context->drawImage(image, /*x=*/0, /*y=*/0, scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  context->endLayer(no_exception);

  // Doesn't throw outside layers:
  context->drawImage(image, /*x=*/0, /*y=*/0, no_exception);
}

// Checks `getImageData` throws an exception if called inside a layer.
TEST(OffscreenCanvasRenderingContext2DTest, UnclosedLayerGetImageData) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), /*width=*/10,
                                       /*height=*/10);
  OffscreenCanvasRenderingContext2D* context = GetContext(scope, host);
  NonThrowableExceptionState no_exception;

  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      no_exception);

  // Throws inside layers:
  context->getImageData(/*sx=*/0, /*sy=*/0, /*sw=*/1, /*sh=*/1,
                        scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  context->endLayer(no_exception);

  // Doesn't throw outside layers:
  context->getImageData(/*sx=*/0, /*sy=*/0, /*sw=*/1, /*sh=*/1, no_exception);
}

// Checks `putImageData` throws an exception if called inside a layer.
TEST(OffscreenCanvasRenderingContext2DTest, UnclosedLayerPutImageData) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), /*width=*/10,
                                       /*height=*/10);
  OffscreenCanvasRenderingContext2D* context = GetContext(scope, host);

  NonThrowableExceptionState no_exception;
  ImageData* image_data =
      ImageData::Create(context->Width(), context->Height(), no_exception);

  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      no_exception);

  // Throws inside layers:
  context->putImageData(image_data, /*dx=*/0, /*dy=*/0,
                        scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  context->endLayer(no_exception);

  // Doesn't throw outside layers:
  context->putImageData(image_data, /*dx=*/0, /*dy=*/0, no_exception);
}

// Checks `transferToImageBitmap` throws an exception if called inside a layer.
TEST(OffscreenCanvasRenderingContext2DTest,
     UnclosedLayerTransferToImageBitmap) {
  test::TaskEnvironment task_environment;
  ScopedCanvas2dLayersForTest layer_feature(/*enabled=*/true);
  V8TestingScope scope;
  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), /*width=*/10,
                                       /*height=*/10);
  OffscreenCanvasRenderingContext2D* context = GetContext(scope, host);
  NonThrowableExceptionState no_exception;

  context->beginLayer(scope.GetScriptState(), BeginLayerOptions::Create(),
                      no_exception);

  // Throws inside layers:
  host->transferToImageBitmap(scope.GetScriptState(),
                              scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  context->endLayer(no_exception);

  // Doesn't throw outside layers:
  host->transferToImageBitmap(scope.GetScriptState(), no_exception);
}

// Regression test for https://crbug.com/1509382.
TEST(OffscreenCanvasRenderingContext2DTest, NoCrashOnDocumentShutdown) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* host = OffscreenCanvas::Create(scope.GetScriptState(), /*width=*/10,
                                       /*height=*/10);
  OffscreenCanvasRenderingContext2D* context = GetContext(scope, host);
  context->setFont("12px Ahem");
  scope.GetDocument().Shutdown();
  context->measureText("hello world");
}

}  // namespace
}  // namespace blink
