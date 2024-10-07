// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_assistant_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_capability_availability.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ai/ai_assistant_factory.h"
#include "third_party/blink/renderer/modules/ai/ai_rewriter_factory.h"
#include "third_party/blink/renderer/modules/ai/ai_summarizer_factory.h"
#include "third_party/blink/renderer/modules/ai/ai_writer_factory.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

AI::AI(ExecutionContext* context)
    : ExecutionContextClient(context),
      task_runner_(context->GetTaskRunner(TaskType::kInternalDefault)),
      ai_remote_(context) {}

void AI::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_remote_);
  visitor->Trace(ai_assistant_factory_);
  visitor->Trace(ai_summarizer_factory_);
  visitor->Trace(ai_writer_factory_);
  visitor->Trace(ai_rewriter_factory_);
  visitor->Trace(ai_language_detector_factory_);
}

HeapMojoRemote<mojom::blink::AIManager>& AI::GetAIRemote() {
  if (!ai_remote_.is_bound()) {
    if (GetExecutionContext()) {
      GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
          ai_remote_.BindNewPipeAndPassReceiver(task_runner_));
    }
  }
  return ai_remote_;
}

scoped_refptr<base::SequencedTaskRunner> AI::GetTaskRunner() {
  return task_runner_;
}

AIAssistantFactory* AI::assistant() {
  if (!ai_assistant_factory_) {
    ai_assistant_factory_ = MakeGarbageCollected<AIAssistantFactory>(this);
  }
  return ai_assistant_factory_.Get();
}

AISummarizerFactory* AI::summarizer() {
  if (!ai_summarizer_factory_) {
    ai_summarizer_factory_ = MakeGarbageCollected<AISummarizerFactory>(
        this, GetExecutionContext(), task_runner_);
  }
  return ai_summarizer_factory_.Get();
}

AIWriterFactory* AI::writer() {
  if (!ai_writer_factory_) {
    ai_writer_factory_ = MakeGarbageCollected<AIWriterFactory>(this);
  }
  return ai_writer_factory_.Get();
}

AIRewriterFactory* AI::rewriter() {
  if (!ai_rewriter_factory_) {
    ai_rewriter_factory_ = MakeGarbageCollected<AIRewriterFactory>(this);
  }
  return ai_rewriter_factory_.Get();
}

AILanguageDetectorFactory* AI::languageDetector() {
  if (!ai_language_detector_factory_) {
    ai_language_detector_factory_ =
        MakeGarbageCollected<AILanguageDetectorFactory>();
  }
  return ai_language_detector_factory_.Get();
}

}  // namespace blink
