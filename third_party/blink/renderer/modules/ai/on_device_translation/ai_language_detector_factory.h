// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_LANGUAGE_DETECTOR_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_LANGUAGE_DETECTOR_FACTORY_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_language_detector_create_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_availability.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/language_detector.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

// `ExecutionContextClient` gives us access to the browser interface broker.
class AILanguageDetectorFactory final : public ScriptWrappable,
                                        public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AILanguageDetectorFactory(ExecutionContext* context);

  void Trace(Visitor* visitor) const override;

  // Checks the availability of the Language Detection model.
  ScriptPromise<V8AIAvailability> availability(ScriptState* script_state,
                                               ExceptionState& exception_state);

  // Creates an `LanguageDetector`, with a model ready to use.
  ScriptPromise<LanguageDetector> create(ScriptState* script_state,
                                         LanguageDetectorCreateOptions* options,
                                         ExceptionState& exception_state);

 private:
  void OnGotStatus(
      ScriptPromiseResolver<V8AIAvailability>* resolver,
      language_detection::mojom::blink::LanguageDetectionModelStatus result);

  Member<LanguageDetectionModel> language_detection_model_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_LANGUAGE_DETECTOR_FACTORY_H_
