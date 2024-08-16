// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/on_device_translation/language_detector.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_detection_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_detector.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/language_detection/detect.h"

namespace blink {

LanguageDetector::LanguageDetector() = default;

void LanguageDetector::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

HeapVector<Member<LanguageDetectionResult>> ConvertResult(
    WTF::Vector<LanguagePrediction> predictions) {
  HeapVector<Member<LanguageDetectionResult>> result;
  for (const auto& prediction : predictions) {
    auto* one = MakeGarbageCollected<LanguageDetectionResult>();
    result.push_back(one);
    one->setDetectedLanguage(String(prediction.language));
    one->setConfidence(prediction.score);
  }
  return result;
}

void OnDetectComplete(
    ScriptPromiseResolver<IDLSequence<LanguageDetectionResult>>* resolver,
    base::expected<WTF::Vector<LanguagePrediction>, DetectLanguageError>
        result) {
  if (result.has_value()) {
    // Order the result from most to least confident.
    std::sort(result.value().rbegin(), result.value().rend());
    resolver->Resolve(ConvertResult(result.value()));
  } else {
    switch (result.error()) {
      case DetectLanguageError::kUnavailable:
        resolver->Reject("Model not available");
    }
  }
}

ScriptPromise<IDLSequence<LanguageDetectionResult>> LanguageDetector::detect(
    ScriptState* script_state,
    const WTF::String& input,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise<IDLSequence<LanguageDetectionResult>>();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<LanguageDetectionResult>>>(
      script_state);

  DetectLanguage(input,
                 WTF::BindOnce(OnDetectComplete, WrapPersistent(resolver)));
  return resolver->Promise();
}

}  // namespace blink
