// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/handwriting/handwriting_drawing.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_drawing_segment.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_prediction.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_segment.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_recognizer.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_stroke.h"
#include "third_party/blink/renderer/modules/handwriting/handwriting_type_converters.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

namespace {
// The callback to get the recognition result.
void OnRecognitionResult(
    ScriptPromiseResolver<IDLSequence<HandwritingPrediction>>* resolver,
    ScriptState* script_state,
    std::optional<Vector<handwriting::mojom::blink::HandwritingPredictionPtr>>
        predictions) {
  // If `predictions` does not have value, it means the some error happened in
  // recognition. Otherwise, if it has value but the vector is empty, it means
  // the recognition works fine but it can not recognize anything from the
  // input.
  if (!predictions.has_value()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "Internal error."));
    return;
  }
  HeapVector<Member<HandwritingPrediction>> result;
  for (const auto& pred_mojo : predictions.value()) {
    result.push_back(pred_mojo.To<blink::HandwritingPrediction*>());
  }
  resolver->Resolve(std::move(result));
}
}  // namespace

HandwritingDrawing::HandwritingDrawing(ExecutionContext* context,
                                       HandwritingRecognizer* recognizer,
                                       const HandwritingHints* hints)
    : hints_(hints), recognizer_(recognizer) {}

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

ScriptPromise<IDLSequence<HandwritingPrediction>>
HandwritingDrawing::getPrediction(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<HandwritingPrediction>>>(script_state);
  auto promise = resolver->Promise();

  if (!IsValid()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "The recognizer has been invalidated."));
    return promise;
  }

  Vector<handwriting::mojom::blink::HandwritingStrokePtr> strokes;
  for (const auto& stroke : strokes_) {
    strokes.push_back(
        mojo::ConvertTo<handwriting::mojom::blink::HandwritingStrokePtr>(
            stroke.Get()));
  }

  recognizer_->GetPrediction(
      std::move(strokes),
      mojo::ConvertTo<handwriting::mojom::blink::HandwritingHintsPtr>(
          hints_.Get()),
      WTF::BindOnce(&OnRecognitionResult, WrapPersistent(resolver),
                    WrapPersistent(script_state)));

  return promise;
}

void HandwritingDrawing::Trace(Visitor* visitor) const {
  visitor->Trace(hints_);
  visitor->Trace(strokes_);
  visitor->Trace(recognizer_);
  ScriptWrappable::Trace(visitor);
}

bool HandwritingDrawing::IsValid() const {
  return recognizer_ != nullptr && recognizer_->IsValid();
}

}  // namespace blink
