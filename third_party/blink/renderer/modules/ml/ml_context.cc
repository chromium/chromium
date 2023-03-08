// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_context.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

MLContext::MLContext(const V8MLDevicePreference device_preference,
                     const V8MLPowerPreference power_preference,
                     const V8MLModelFormat model_format,
                     const unsigned int num_threads,
                     ML* ml)
    : device_preference_(device_preference),
      power_preference_(power_preference),
      model_format_(model_format),
      num_threads_(num_threads),
      ml_(ml) {}

MLContext::~MLContext() = default;

V8MLDevicePreference MLContext::GetDevicePreference() const {
  return device_preference_;
}

V8MLPowerPreference MLContext::GetPowerPreference() const {
  return power_preference_;
}

V8MLModelFormat MLContext::GetModelFormat() const {
  return model_format_;
}

unsigned int MLContext::GetNumThreads() const {
  return num_threads_;
}

ML* MLContext::GetML() {
  return ml_.Get();
}

void MLContext::Trace(Visitor* visitor) const {
  visitor->Trace(ml_);

  ScriptWrappable::Trace(visitor);
}

ScriptPromise MLContext::compute(ScriptState* script_state,
                                 MLGraph* graph,
                                 const MLNamedArrayBufferViews& inputs,
                                 const MLNamedArrayBufferViews& outputs,
                                 ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  if (graph->Context() != this) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError,
        "The graph isn't built within this context."));
  } else {
    graph->ComputeAsync(inputs, outputs, resolver, exception_state);
  }

  return promise;
}

void MLContext::computeSync(MLGraph* graph,
                            const MLNamedArrayBufferViews& inputs,
                            const MLNamedArrayBufferViews& outputs,
                            ExceptionState& exception_state) {
  if (graph->Context() != this) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The graph isn't built within this context.");
    return;
  }
  graph->ComputeSync(inputs, outputs, exception_state);
}

}  // namespace blink
