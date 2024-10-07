// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_LANGUAGE_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_LANGUAGE_DETECTOR_H_

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_detector_detect_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_detection_result.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/language_detection/detect.h"

namespace blink {

class AILanguageDetector final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AILanguageDetector();
  ~AILanguageDetector() override = default;

  ScriptPromise<IDLSequence<LanguageDetectionResult>> detect(
      ScriptState* script_state,
      const WTF::String& input,
      AILanguageDetectorDetectOptions* options,
      ExceptionState& exception_state);
  void destroy(ScriptState*);

  // TODO(crbug.com/349927087): Make the functions below free functions.
  static HeapVector<Member<LanguageDetectionResult>> ConvertResult(
      WTF::Vector<LanguagePrediction> predictions);
  static void OnDetectComplete(
      ScriptPromiseResolver<IDLSequence<LanguageDetectionResult>>* resolver,
      base::expected<WTF::Vector<LanguagePrediction>, DetectLanguageError>
          result);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_LANGUAGE_DETECTOR_H_
