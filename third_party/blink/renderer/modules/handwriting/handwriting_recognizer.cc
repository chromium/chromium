// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/handwriting/handwriting_recognizer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_hints.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_drawing.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {
const char kInvalidErrorMessage[] = "This recognizer has been invalidated.";
}

HandwritingRecognizer::HandwritingRecognizer(ExecutionContext* context)
    : is_valid_(true) {}

HandwritingRecognizer::~HandwritingRecognizer() = default;

bool HandwritingRecognizer::IsValid() {
  return is_valid_;
}

HandwritingDrawing* HandwritingRecognizer::startDrawing(
    ScriptState* script_state,
    const HandwritingHints* hints,
    ExceptionState& exception_state) {
  if (!is_valid_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidErrorMessage);
    return nullptr;
  }

  auto* handwriting_drawing = MakeGarbageCollected<HandwritingDrawing>(
      ExecutionContext::From(script_state), this);

  return handwriting_drawing;
}

void HandwritingRecognizer::finish(ExceptionState& exception_state) {
  if (!is_valid_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidErrorMessage);
    return;
  }

  Invalidate();
}

void HandwritingRecognizer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

void HandwritingRecognizer::Invalidate() {
  is_valid_ = false;

  // TODO(crbug.com/1166910): This should shut down the mojo connection.
  // However, we can't land the mojo service until a browser side implementation
  // is available (for security review). Until then, use this stub which never
  // resolves.
}

}  // namespace blink
