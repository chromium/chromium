// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_assistant.h"

#include "base/metrics/histogram_functions.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom-blink.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"

namespace blink {

AIAssistant::AIAssistant(ExecutionContext* context,
                         AITextSession* text_session,
                         scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context),
      text_session_(text_session),
      task_runner_(task_runner) {}

void AIAssistant::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(text_session_);
}

ScriptPromise<IDLString> AIAssistant::prompt(ScriptState* script_state,
                                             const WTF::String& input,
                                             ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLString>();
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kAssistant),
      AIMetrics::AIAPI::kSessionPrompt);

  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kAssistant),
                             int(input.CharactersSizeInBytes()));

  if (!text_session_) {
    ThrowSessionDestroyedException(exception_state);
    return ScriptPromise<IDLString>();
  }

  auto [promise, pending_remote] = CreateModelExecutionResponder(
      script_state, /*signal=*/nullptr, task_runner_,
      AIMetrics::AISessionType::kAssistant,
      WTF::BindOnce(&AIAssistant::OnResponseComplete,
                    WrapWeakPersistent(this)));
  text_session_->GetRemoteTextSession()->Prompt(input,
                                                std::move(pending_remote));
  return promise;
}

ReadableStream* AIAssistant::promptStreaming(ScriptState* script_state,
                                             const WTF::String& input,
                                             ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return nullptr;
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kAssistant),
      AIMetrics::AIAPI::kSessionPromptStreaming);

  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kAssistant),
                             int(input.CharactersSizeInBytes()));

  if (!text_session_) {
    ThrowSessionDestroyedException(exception_state);
    return nullptr;
  }

  auto [readable_stream, pending_remote] =
      CreateModelExecutionStreamingResponder(
          script_state, /*signal=*/nullptr, task_runner_,
          AIMetrics::AISessionType::kAssistant,
          WTF::BindOnce(&AIAssistant::OnResponseComplete,
                        WrapWeakPersistent(this)));
  text_session_->GetRemoteTextSession()->Prompt(input,
                                                std::move(pending_remote));
  return readable_stream;
}

uint64_t AIAssistant::maxTokens() const {
  blink::mojom::blink::AITextSessionInfoPtr info = text_session_->GetInfo();
  CHECK(info);
  return info->max_tokens;
}

uint64_t AIAssistant::tokensSoFar() const {
  return current_tokens_;
}

uint64_t AIAssistant::tokensLeft() const {
  return maxTokens() - tokensSoFar();
}

uint32_t AIAssistant::topK() const {
  blink::mojom::blink::AITextSessionInfoPtr info = text_session_->GetInfo();
  CHECK(info);
  return info->sampling_params->top_k;
}

float AIAssistant::temperature() const {
  blink::mojom::blink::AITextSessionInfoPtr info = text_session_->GetInfo();
  CHECK(info);
  return info->sampling_params->temperature;
}

ScriptPromise<AIAssistant> AIAssistant::clone(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AIAssistant>();
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kAssistant),
      AIMetrics::AIAPI::kSessionClone);

  ScriptPromiseResolver<AIAssistant>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AIAssistant>>(script_state);

  if (!text_session_) {
    ThrowSessionDestroyedException(exception_state);
    return resolver->Promise();
  }

  AITextSession* cloned_session =
      MakeGarbageCollected<AITextSession>(GetExecutionContext(), task_runner_);
  AIAssistant* cloned_assistant = MakeGarbageCollected<AIAssistant>(
      GetExecutionContext(), cloned_session, task_runner_);
  cloned_assistant->current_tokens_ = current_tokens_;
  text_session_->GetRemoteTextSession()->Fork(
      cloned_assistant->text_session_->GetModelSessionReceiver(),
      WTF::BindOnce(
          [](ScriptPromiseResolver<AIAssistant>* resolver,
             AIAssistant* cloned_assistant,
             blink::mojom::blink::AITextSessionInfoPtr info) {
            if (info) {
              cloned_assistant->text_session_->SetInfo(
                  base::PassKey<AIAssistant>(), std::move(info));
              resolver->Resolve(cloned_assistant);
            } else {
              resolver->Reject(DOMException::Create(
                  kExceptionMessageUnableToCloneSession,
                  DOMException::GetErrorName(
                      DOMExceptionCode::kInvalidStateError)));
            }
          },
          WrapPersistent(resolver), WrapPersistent(cloned_assistant)));

  return resolver->Promise();
}

// TODO(crbug.com/355967885): reset the remote to destroy the session.
void AIAssistant::destroy(ScriptState* script_state,
                          ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return;
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kAssistant),
      AIMetrics::AIAPI::kSessionDestroy);

  if (text_session_) {
    text_session_->GetRemoteTextSession()->Destroy();
    text_session_ = nullptr;
  }
}

void AIAssistant::OnResponseComplete(std::optional<uint64_t> current_tokens) {
  if (current_tokens.has_value()) {
    current_tokens_ = current_tokens.value();
  }
}

}  // namespace blink
