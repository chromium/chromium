// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_summarizer_factory.h"

#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/renderer/modules/ai/ai_summarizer.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"

namespace blink {

AISummarizerFactory::AISummarizerFactory(
    ExecutionContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context),
      text_session_factory_(
          MakeGarbageCollected<AITextSessionFactory>(context, task_runner)),
      task_runner_(task_runner) {}

void AISummarizerFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(text_session_factory_);
}

ScriptPromise<AISummarizerCapabilities> AISummarizerFactory::capabilities(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AISummarizerCapabilities>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AISummarizerCapabilities>>(
          script_state);
  auto promise = resolver->Promise();

  text_session_factory_->CanCreateTextSession(WTF::BindOnce(
      [](ScriptPromiseResolver<AISummarizerCapabilities>* resolver,
         AISummarizerFactory* factory, AICapabilityAvailability availability,
         mojom::blink::ModelAvailabilityCheckResult check_result) {
        resolver->Resolve(MakeGarbageCollected<AISummarizerCapabilities>(
            AICapabilityAvailabilityToV8(availability)));
      },
      WrapPersistent(resolver), WrapWeakPersistent(this)));
  return promise;
}

ScriptPromise<AISummarizer> AISummarizerFactory::create(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AISummarizer>();
  }
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AISummarizer>>(script_state);
  auto promise = resolver->Promise();

  text_session_factory_->CreateTextSession(
      /*sampling_params=*/nullptr, /*system_prompt=*/WTF::String(),
      WTF::BindOnce(
          [](ScriptPromiseResolver<AISummarizer>* resolver,
             AISummarizerFactory* factory,
             base::expected<AITextSession*, DOMException*> result) {
            if (result.has_value()) {
              resolver->Resolve(MakeGarbageCollected<AISummarizer>(
                  factory->GetExecutionContext(), result.value(),
                  factory->task_runner_));
            } else {
              resolver->Reject(result.error());
            }
          },
          WrapPersistent(resolver), WrapWeakPersistent(this)));

  return promise;
}

}  // namespace blink
