// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/worker_internals.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/testing/origin_trials_test.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "ui/gfx/image/canvas_image_source.h"

namespace blink {

WorkerInternals::~WorkerInternals() = default;

WorkerInternals::WorkerInternals() = default;

OriginTrialsTest* WorkerInternals::originTrialsTest() const {
  return MakeGarbageCollected<OriginTrialsTest>();
}

void WorkerInternals::countFeature(ScriptState* script_state,
                                   uint32_t feature,
                                   ExceptionState& exception_state) {
  if (feature > static_cast<int32_t>(WebFeature::kMaxValue)) {
    exception_state.ThrowTypeError(
        "The given feature does not exist in WebFeature.");
    return;
  }
  UseCounter::Count(ExecutionContext::From(script_state),
                    static_cast<WebFeature>(feature));
}

void WorkerInternals::countDeprecation(ScriptState* script_state,
                                       uint32_t feature,
                                       ExceptionState& exception_state) {
  if (feature > static_cast<int32_t>(WebFeature::kMaxValue)) {
    exception_state.ThrowTypeError(
        "The given feature does not exist in WebFeature.");
    return;
  }
  Deprecation::CountDeprecation(ExecutionContext::From(script_state),
                                static_cast<WebFeature>(feature));
}

void WorkerInternals::collectGarbage(ScriptState* script_state) {
  script_state->GetIsolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
}

void WorkerInternals::forceLoseCanvasContext(CanvasRenderingContext* ctx) {
  ctx->LoseContext(CanvasRenderingContext::kSyntheticLostContext);
}

bool WorkerInternals::isCanvasImageSourceAccelerated(
    const CanvasImageSource* image_source) const {
  return image_source->IsAccelerated();
}

String WorkerInternals::getCanvasNoiseToken(ScriptState* script_state) {
  std::optional<NoiseToken> token =
      ExecutionContext::From(script_state)->CanvasNoiseToken();
  if (token.has_value()) {
    return String::Number(token->Value());
  }
  return String();
}

}  // namespace blink
