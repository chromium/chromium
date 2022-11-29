// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style_test_utils.h"

namespace blink {

void SetFillStyleString(BaseRenderingContext2D* ctx,
                        ScriptState* script_state,
                        const String& string) {
  NonThrowableExceptionState exception_state;
  ctx->setFillStyle(
      script_state->GetIsolate(),
      ToV8Traits<IDLString>::ToV8(script_state, string).ToLocalChecked(),
      exception_state);
}

void SetStrokeStyleString(BaseRenderingContext2D* ctx,
                          ScriptState* script_state,
                          const String& string) {
  NonThrowableExceptionState exception_state;
  ctx->setStrokeStyle(
      script_state->GetIsolate(),
      ToV8Traits<IDLString>::ToV8(script_state, string).ToLocalChecked(),
      exception_state);
}

String GetStrokeStyleAsString(BaseRenderingContext2D* ctx,
                              ScriptState* script_state) {
  NonThrowableExceptionState exception_state;
  auto* isolate = script_state->GetIsolate();
  auto result = ctx->strokeStyle(script_state);
  return NativeValueTraits<IDLString>::NativeValue(isolate, result,
                                                   exception_state);
}

String GetFillStyleAsString(BaseRenderingContext2D* ctx,
                            ScriptState* script_state) {
  NonThrowableExceptionState exception_state;
  auto result = ctx->fillStyle(script_state);
  return NativeValueTraits<IDLString>::NativeValue(script_state->GetIsolate(),
                                                   result, exception_state);
}

}  // namespace blink
