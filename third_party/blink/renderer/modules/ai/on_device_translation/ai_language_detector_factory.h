// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_LANGUAGE_DETECTOR_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_LANGUAGE_DETECTOR_FACTORY_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_detector_create_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_language_detector.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_language_detector_capabilities.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

class AICreateMonitor;

// `ExecutionContextClient` gives us access to the browser interface broker.
class AILanguageDetectorFactory final : public ScriptWrappable,
                                        public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AILanguageDetectorFactory(
      ExecutionContext* context,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  void Trace(Visitor* visitor) const override;

  // Creates an `AILanguageDetector`, with a model ready to use.
  ScriptPromise<AILanguageDetector> create(
      ScriptState* script_state,
      AILanguageDetectorCreateOptions* options,
      ExceptionState& exception_state);

  ScriptPromise<AILanguageDetectorCapabilities> capabilities(
      ScriptState* script_state);

  HeapMojoRemote<
      language_detection::mojom::blink::ContentLanguageDetectionDriver>&
  GetLangaugeDetectionDriverRemote();

 private:
  class AILanguageDetectorCreateTask
      : public GarbageCollected<AILanguageDetectorCreateTask> {
   public:
    AILanguageDetectorCreateTask(
        ExecutionContext* execution_context,
        scoped_refptr<base::SequencedTaskRunner> task_runner,
        ScriptPromiseResolver<AILanguageDetector>* resolver,
        LanguageDetectionModel* model,
        const AILanguageDetectorCreateOptions* options);

    void CreateDetector(base::File model_file);

    void Trace(Visitor* visitor) const;

   private:
    void OnModelLoaded(base::expected<LanguageDetectionModel*,
                                      DetectLanguageError> maybe_model);

    Member<AICreateMonitor> monitor_;
    Member<ScriptPromiseResolver<AILanguageDetector>> resolver_;
    Member<LanguageDetectionModel> language_detection_model_;
  };

  static void OnModelFileReceived(LanguageDetectionModel* model,
                                  AICreateMonitor* monitor,
                                  base::OnceClosure on_created_callback,
                                  base::File model_file);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  Member<LanguageDetectionModel> language_detection_model_;

  HeapMojoRemote<
      language_detection::mojom::blink::ContentLanguageDetectionDriver>
      language_detection_driver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_LANGUAGE_DETECTOR_FACTORY_H_
