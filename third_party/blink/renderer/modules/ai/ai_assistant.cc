// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_assistant.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/ai/ai_assistant_factory.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class CloneAssistantClient
    : public GarbageCollected<CloneAssistantClient>,
      public mojom::blink::AIManagerCreateAssistantClient,
      public AIMojoClient<AIAssistant> {
 public:
  CloneAssistantClient(AIAssistant* assistant,
                       ScriptPromiseResolver<AIAssistant>* resolver,
                       AbortSignal* signal,
                       base::PassKey<AIAssistant> pass_key)
      : AIMojoClient(assistant, resolver, signal),
        pass_key_(pass_key),
        assistant_(assistant),
        receiver_(this, assistant->GetExecutionContext()) {
    mojo::PendingRemote<mojom::blink::AIManagerCreateAssistantClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   assistant->GetTaskRunner());
    assistant_->GetAIAssistantRemote()->Fork(std::move(client_remote));
  }
  ~CloneAssistantClient() override = default;

  CloneAssistantClient(const CloneAssistantClient&) = delete;
  CloneAssistantClient& operator=(const CloneAssistantClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIMojoClient::Trace(visitor);
    visitor->Trace(assistant_);
    visitor->Trace(receiver_);
  }

  // mojom::blink::AIManagerCreateAssistantClient implementation.
  void OnResult(mojo::PendingRemote<mojom::blink::AIAssistant> assistant_remote,
                mojom::blink::AIAssistantInfoPtr info) override {
    if (!GetResolver()) {
      return;
    }

    if (info) {
      AIAssistant* cloned_assistant = MakeGarbageCollected<AIAssistant>(
          assistant_->GetExecutionContext(), std::move(assistant_remote),
          assistant_->GetTaskRunner(), std::move(info),
          assistant_->GetCurrentTokens());
      GetResolver()->Resolve(cloned_assistant);
    } else {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          kExceptionMessageUnableToCloneSession);
    }

    Cleanup();
  }

 private:
  base::PassKey<AIAssistant> pass_key_;
  Member<AIAssistant> assistant_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateAssistantClient,
                   CloneAssistantClient>
      receiver_;
};

class CountPromptTokensClient
    : public GarbageCollected<CountPromptTokensClient>,
      public mojom::blink::AIAssistantCountPromptTokensClient,
      public AIMojoClient<IDLUnsignedLongLong> {
 public:
  CountPromptTokensClient(AIAssistant* assistant,
                          ScriptPromiseResolver<IDLUnsignedLongLong>* resolver,
                          AbortSignal* signal,
                          const WTF::String& input)
      : AIMojoClient(assistant, resolver, signal),
        assistant_(assistant),
        receiver_(this, assistant->GetExecutionContext()) {
    mojo::PendingRemote<mojom::blink::AIAssistantCountPromptTokensClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   assistant->GetTaskRunner());
    assistant_->GetAIAssistantRemote()->CountPromptTokens(
        input, std::move(client_remote));
  }
  ~CountPromptTokensClient() override = default;

  CountPromptTokensClient(const CountPromptTokensClient&) = delete;
  CountPromptTokensClient& operator=(const CountPromptTokensClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIMojoClient::Trace(visitor);
    visitor->Trace(assistant_);
    visitor->Trace(receiver_);
  }

  // mojom::blink::AIAssistantCountPromptTokensClient implementation.
  void OnResult(uint32_t number_of_tokens) override {
    if (!GetResolver()) {
      return;
    }

    GetResolver()->Resolve(number_of_tokens);
    Cleanup();
  }

 private:
  Member<AIAssistant> assistant_;
  HeapMojoReceiver<mojom::blink::AIAssistantCountPromptTokensClient,
                   CountPromptTokensClient>
      receiver_;
};

}  // namespace

AIAssistant::AIAssistant(
    ExecutionContext* execution_context,
    mojo::PendingRemote<mojom::blink::AIAssistant> pending_remote,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    blink::mojom::blink::AIAssistantInfoPtr info,
    uint64_t current_tokens)
    : ExecutionContextClient(execution_context),
      current_tokens_(current_tokens),
      task_runner_(task_runner),
      assistant_remote_(execution_context) {
  assistant_remote_.Bind(std::move(pending_remote), task_runner);
  if (info) {
    SetInfo(base::PassKey<AIAssistant>(), std::move(info));
  }
}

void AIAssistant::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(assistant_remote_);
}

ScriptPromise<IDLString> AIAssistant::prompt(
    ScriptState* script_state,
    const WTF::String& input,
    const AIAssistantPromptOptions* options,
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

  if (!assistant_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return ScriptPromise<IDLString>();
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    ThrowAbortedException(exception_state);
    return ScriptPromise<IDLString>();
  }

  auto [promise, pending_remote] = CreateModelExecutionResponder(
      script_state, signal, task_runner_, AIMetrics::AISessionType::kAssistant,
      WTF::BindOnce(&AIAssistant::OnResponseComplete,
                    WrapWeakPersistent(this)));
  assistant_remote_->Prompt(input, std::move(pending_remote));
  return promise;
}

ReadableStream* AIAssistant::promptStreaming(
    ScriptState* script_state,
    const WTF::String& input,
    const AIAssistantPromptOptions* options,
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

  if (!assistant_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return nullptr;
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    ThrowAbortedException(exception_state);
    return nullptr;
  }

  auto [readable_stream, pending_remote] =
      CreateModelExecutionStreamingResponder(
          script_state, signal, task_runner_,
          AIMetrics::AISessionType::kAssistant,
          WTF::BindOnce(&AIAssistant::OnResponseComplete,
                        WrapWeakPersistent(this)));
  assistant_remote_->Prompt(input, std::move(pending_remote));
  return readable_stream;
}

ScriptPromise<AIAssistant> AIAssistant::clone(
    ScriptState* script_state,
    const AIAssistantCloneOptions* options,
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

  if (!assistant_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return resolver->Promise();
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    ThrowAbortedException(exception_state);
    return ScriptPromise<AIAssistant>();
  }

  MakeGarbageCollected<CloneAssistantClient>(this, resolver, signal,
                                             base::PassKey<AIAssistant>());

  return resolver->Promise();
}

ScriptPromise<IDLUnsignedLongLong> AIAssistant::countPromptTokens(
    ScriptState* script_state,
    const WTF::String& input,
    const AIAssistantPromptOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLUnsignedLongLong>();
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kAssistant),
      AIMetrics::AIAPI::kSessionCountPromptTokens);

  ScriptPromiseResolver<IDLUnsignedLongLong>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUnsignedLongLong>>(
          script_state);

  if (!assistant_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return resolver->Promise();
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    ThrowAbortedException(exception_state);
    return ScriptPromise<IDLUnsignedLongLong>();
  }

  MakeGarbageCollected<CountPromptTokensClient>(this, resolver, signal, input);

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

  if (assistant_remote_) {
    assistant_remote_->Destroy();
    assistant_remote_.reset();
  }
}

void AIAssistant::OnResponseComplete(std::optional<uint64_t> current_tokens) {
  if (current_tokens.has_value()) {
    current_tokens_ = current_tokens.value();
  }
}

void AIAssistant::SetInfo(std::variant<base::PassKey<AIAssistantFactory>,
                                       base::PassKey<AIAssistant>> pass_key,
                          const blink::mojom::blink::AIAssistantInfoPtr info) {
  CHECK(info);
  top_k_ = info->sampling_params->top_k;
  temperature_ = info->sampling_params->temperature;
  max_tokens_ = info->max_tokens;
}

HeapMojoRemote<mojom::blink::AIAssistant>& AIAssistant::GetAIAssistantRemote() {
  return assistant_remote_;
}

scoped_refptr<base::SequencedTaskRunner> AIAssistant::GetTaskRunner() {
  return task_runner_;
}

uint64_t AIAssistant::GetCurrentTokens() {
  return current_tokens_;
}

}  // namespace blink
