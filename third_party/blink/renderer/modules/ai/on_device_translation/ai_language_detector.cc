// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_language_detector.h"

#include "third_party/blink/renderer/platform/language_detection/detect.h"

namespace blink {

AILanguageDetector::AILanguageDetector() = default;

ScriptPromise<IDLSequence<LanguageDetectionResult>> AILanguageDetector::detect(
    ScriptState* script_state,
    const WTF::String& input,
    AILanguageDetectorDetectOptions* options,
    ExceptionState& exception_state) {
  // TODO(crbug.com/349927087): Take `options` into account.
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise<IDLSequence<LanguageDetectionResult>>();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<LanguageDetectionResult>>>(
      script_state);

  DetectLanguage(input, WTF::BindOnce(AILanguageDetector::OnDetectComplete,
                                      WrapPersistent(resolver)));
  return resolver->Promise();
}

void AILanguageDetector::destroy(ScriptState*) {
  // TODO(crbug.com/349927087): Implement the function.
}

HeapVector<Member<LanguageDetectionResult>> AILanguageDetector::ConvertResult(
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

void AILanguageDetector::OnDetectComplete(
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

}  // namespace blink
