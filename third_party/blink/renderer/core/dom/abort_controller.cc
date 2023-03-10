// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/abort_controller.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

AbortController* AbortController::Create(ExecutionContext* context) {
  return MakeGarbageCollected<AbortController>(
      MakeGarbageCollected<AbortSignal>(context,
                                        AbortSignal::SignalType::kController));
}

AbortController::AbortController(AbortSignal* signal) : signal_(signal) {}

AbortController::~AbortController() = default;

void AbortController::Dispose() {
  if (RuntimeEnabledFeatures::AbortSignalCompositionEnabled()) {
    signal_->DetachFromController();
  }
}

void AbortController::abort(ScriptState* script_state) {
  v8::Local<v8::Value> dom_exception = V8ThrowDOMException::CreateOrEmpty(
      script_state->GetIsolate(), DOMExceptionCode::kAbortError,
      "signal is aborted without reason");
  CHECK(!dom_exception.IsEmpty());
  ScriptValue reason(script_state->GetIsolate(), dom_exception);
  abort(script_state, reason);
}

void AbortController::abort(ScriptState* script_state, ScriptValue reason) {
  signal_->SignalAbort(script_state, reason);
}

void AbortController::Trace(Visitor* visitor) const {
  visitor->Trace(signal_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
