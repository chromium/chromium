// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/handwriting/handwriting_recognizer.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_hints.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_drawing.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {
const char kInvalidErrorMessage[] = "This recognizer has been invalidated.";
}

HandwritingRecognizer::HandwritingRecognizer(
    ExecutionContext* context,
    mojo::PendingRemote<handwriting::mojom::blink::HandwritingRecognizer>
        pending_remote)
    : remote_service_(context) {
  remote_service_.Bind(std::move(pending_remote),
                       context->GetTaskRunner(TaskType::kInternalDefault));
}

HandwritingRecognizer::~HandwritingRecognizer() = default;

bool HandwritingRecognizer::IsValid() {
  return remote_service_.is_bound();
}

void HandwritingRecognizer::GetPrediction(
    Vector<handwriting::mojom::blink::HandwritingStrokePtr> strokes,
    handwriting::mojom::blink::HandwritingHintsPtr hints,
    handwriting::mojom::blink::HandwritingRecognizer::GetPredictionCallback
        callback) {
  remote_service_->GetPrediction(std::move(strokes), std::move(hints),
                                 std::move(callback));
}

HandwritingDrawing* HandwritingRecognizer::startDrawing(
    ScriptState* script_state,
    const HandwritingHints* hints,
    ExceptionState& exception_state) {
  if (!IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidErrorMessage);
    return nullptr;
  }

  auto* handwriting_drawing = MakeGarbageCollected<HandwritingDrawing>(
      ExecutionContext::From(script_state), this, hints);

  return handwriting_drawing;
}

void HandwritingRecognizer::finish(ExceptionState& exception_state) {
  if (!IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidErrorMessage);
    return;
  }

  Invalidate();
}

void HandwritingRecognizer::Trace(Visitor* visitor) const {
  visitor->Trace(remote_service_);
  ScriptWrappable::Trace(visitor);
}

void HandwritingRecognizer::Invalidate() {
  remote_service_.reset();
}

}  // namespace blink
