// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_text_session.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

AITextSession::AITextSession(
    ExecutionContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context),
      task_runner_(task_runner),
      text_session_remote_(context) {}

void AITextSession::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(text_session_remote_);
}

mojo::PendingReceiver<blink::mojom::blink::AITextSession>
AITextSession::GetModelSessionReceiver() {
  return text_session_remote_.BindNewPipeAndPassReceiver(task_runner_);
}

void AITextSession::SetInfo(base::PassKey<AITextSessionFactory>,
                            blink::mojom::blink::AITextSessionInfoPtr info) {
  CHECK(!info_) << "The session info should only be set once after creation";
  info_ = std::move(info);
}

bool AITextSession::ThrowExceptionIfIsDestroyed(
    ExceptionState& exception_state) {
  if (is_destroyed_) {
    ThrowSessionDestroyedException(exception_state);
  }
  return is_destroyed_;
}

ScriptPromise<IDLString> AITextSession::prompt(
    ScriptState* script_state,
    const WTF::String& input,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLString>();
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kSessionPrompt);

  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kText),
                             int(input.CharactersSizeInBytes()));

  if (ThrowExceptionIfIsDestroyed(exception_state)) {
    return ScriptPromise<IDLString>();
  }

  auto [promise, pending_remote] = CreateModelExecutionResponder(
      script_state, /*signal=*/nullptr, task_runner_,
      AIMetrics::AISessionType::kText,
      WTF::BindOnce(&AITextSession::OnResponseComplete,
                    WrapWeakPersistent(this)));
  text_session_remote_->Prompt(input, std::move(pending_remote));
  return promise;
}

ReadableStream* AITextSession::promptStreaming(
    ScriptState* script_state,
    const WTF::String& input,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return nullptr;
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kSessionPromptStreaming);

  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kText),
                             int(input.CharactersSizeInBytes()));

  if (ThrowExceptionIfIsDestroyed(exception_state)) {
    return nullptr;
  }

  auto [readable_stream, pending_remote] =
      CreateModelExecutionStreamingResponder(
          script_state, /*signal=*/nullptr, task_runner_,
          AIMetrics::AISessionType::kText,
          WTF::BindOnce(&AITextSession::OnResponseComplete,
                        WrapWeakPersistent(this)));
  text_session_remote_->Prompt(input, std::move(pending_remote));
  return readable_stream;
}

uint64_t AITextSession::maxTokens() const {
  CHECK(info_);
  return info_->max_tokens;
}

uint64_t AITextSession::tokensSoFar() const {
  return current_tokens_;
}

uint64_t AITextSession::tokensLeft() const {
  return maxTokens() - tokensSoFar();
}

uint32_t AITextSession::topK() const {
  CHECK(info_);
  return info_->sampling_params->top_k;
}

float AITextSession::temperature() const {
  CHECK(info_);
  return info_->sampling_params->temperature;
}

ScriptPromise<AITextSession> AITextSession::clone(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AITextSession>();
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kSessionClone);

  ScriptPromiseResolver<AITextSession>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AITextSession>>(script_state);

  if (!ThrowExceptionIfIsDestroyed(exception_state)) {
    AITextSession* cloned_session = MakeGarbageCollected<AITextSession>(
        GetExecutionContext(), task_runner_);
    cloned_session->current_tokens_ = current_tokens_;
    text_session_remote_->Fork(
        cloned_session->GetModelSessionReceiver(),
        WTF::BindOnce(
            [](ScriptPromiseResolver<AITextSession>* resolver,
               AITextSession* cloned_session,
               blink::mojom::blink::AITextSessionInfoPtr info) {
              if (info) {
                cloned_session->info_ = std::move(info);
                resolver->Resolve(cloned_session);
              } else {
                resolver->Reject(DOMException::Create(
                    kExceptionMessageUnableToCloneSession,
                    DOMException::GetErrorName(
                        DOMExceptionCode::kInvalidStateError)));
              }
            },
            WrapPersistent(resolver), WrapPersistent(cloned_session)));
  }

  return resolver->Promise();
}

void AITextSession::destroy(ScriptState* script_state,
                            ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return;
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kSessionDestroy);

  if (!is_destroyed_) {
    is_destroyed_ = true;
    text_session_remote_->Destroy();
  }
}

void AITextSession::OnResponseComplete(std::optional<uint64_t> current_tokens) {
  if (current_tokens.has_value()) {
    current_tokens_ = current_tokens.value();
  }
}

}  // namespace blink
