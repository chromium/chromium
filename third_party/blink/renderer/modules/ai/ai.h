// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_language_detector_factory.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {
class AIAssistantFactory;
class AIRewriterFactory;
class AISummarizerFactory;
class AIWriterFactory;

// The class is the entry point of all the built-in AI APIs. It provides the
// getters for the factories of different functionalities.
class AI final : public ScriptWrappable, public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AI(ExecutionContext* context);
  ~AI() override = default;

  void Trace(Visitor* visitor) const override;

  // model_manager.idl implementation.
  AIAssistantFactory* assistant();
  AISummarizerFactory* summarizer();
  AIRewriterFactory* rewriter();
  AIWriterFactory* writer();
  AILanguageDetectorFactory* languageDetector();

  HeapMojoRemote<mojom::blink::AIManager>& GetAIRemote();

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<mojom::blink::AIManager> ai_remote_;
  Member<AIAssistantFactory> ai_assistant_factory_;
  Member<AISummarizerFactory> ai_summarizer_factory_;
  Member<AIWriterFactory> ai_writer_factory_;
  Member<AIRewriterFactory> ai_rewriter_factory_;
  Member<AILanguageDetectorFactory> ai_language_detector_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_H_
