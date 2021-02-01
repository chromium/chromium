// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/handwriting/handwriting_drawing.h"

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_prediction.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_recognizer.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_stroke.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

HandwritingDrawing::HandwritingDrawing(ExecutionContext* context,
                                       HandwritingRecognizer* recognizer)
    : recognizer_(recognizer) {}

HandwritingDrawing::~HandwritingDrawing() = default;

void HandwritingDrawing::addStroke(HandwritingStroke* stroke) {
  // It is meaningless to add stroke to an invalidated drawing. However we may
  // need to remove/clear strokes to save resource.
  if (IsValid()) {
    strokes_.push_back(stroke);
  }
}

void HandwritingDrawing::removeStroke(const HandwritingStroke* stroke) {
  wtf_size_t pos = strokes_.ReverseFind(stroke);
  if (pos != kNotFound) {
    strokes_.EraseAt(pos);
  }
}

void HandwritingDrawing::clear() {
  strokes_.clear();
}

const HeapVector<Member<HandwritingStroke>>& HandwritingDrawing::getStrokes() {
  return strokes_;
}

ScriptPromise HandwritingDrawing::getPrediction(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!IsValid()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "The recognizer has been invalidated."));
    return promise;
  }
  // TODO(crbug.com/1166910): This should call out to a mojo service. However,
  // we can't land the mojo service until a browser side implementation is
  // available (for security review). Until then, use this stub which never
  // resolves.
  resolver->Resolve();

  return promise;
}

void HandwritingDrawing::Trace(Visitor* visitor) const {
  visitor->Trace(strokes_);
  visitor->Trace(recognizer_);
  ScriptWrappable::Trace(visitor);
}

bool HandwritingDrawing::IsValid() const {
  return recognizer_ != nullptr && recognizer_->IsValid();
}

}  // namespace blink
