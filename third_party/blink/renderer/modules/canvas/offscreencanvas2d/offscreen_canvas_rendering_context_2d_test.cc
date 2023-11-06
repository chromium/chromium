// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/offscreencanvas2d/offscreen_canvas_rendering_context_2d.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_begin_layer_options.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

OffscreenCanvasRenderingContext2D* GetContext(V8TestingScope& scope,
                                              OffscreenCanvas* host) {
  CanvasRenderingContext* context = host->GetCanvasRenderingContext(
      scope.GetExecutionContext(), "2d", CanvasContextCreationAttributesCore());
  CHECK(context->IsRenderingContext2D());
  return static_cast<OffscreenCanvasRenderingContext2D*>(context);
}

TEST(OffscreenCanvasHostTest,
     TransferToOffscreenThrowsErrorsProducedByContext) {
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

}  // namespace
}  // namespace blink
