// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_assistant_factory.h"

#include "base/metrics/histogram_functions.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_assistant_initial_prompt.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_assistant_initial_prompt_role.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_assistant.h"
#include "third_party/blink/renderer/modules/ai/ai_assistant_capabilities.h"
#include "third_party/blink/renderer/modules/ai/ai_capability_availability.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

namespace {

mojom::blink::AIAssistantInitialPromptRole AIAssistantInitialPromptRole(
    V8AIAssistantInitialPromptRole role) {
  switch (role.AsEnum()) {
    case V8AIAssistantInitialPromptRole::Enum::kSystem:
      return mojom::blink::AIAssistantInitialPromptRole::kSystem;
    case V8AIAssistantInitialPromptRole::Enum::kUser:
      return mojom::blink::AIAssistantInitialPromptRole::kUser;
    case V8AIAssistantInitialPromptRole::Enum::kAssistant:
      return mojom::blink::AIAssistantInitialPromptRole::kAssistant;
  }
  NOTREACHED();
}

class CreateAssistantClient
    : public GarbageCollected<CreateAssistantClient>,
      public mojom::blink::AIManagerCreateAssistantClient,
      public AIMojoClient<AIAssistant> {
 public:
  CreateAssistantClient(
      AI* ai,
      ScriptPromiseResolver<AIAssistant>* resolver,
      AbortSignal* signal,
      mojom::blink::AIAssistantSamplingParamsPtr sampling_params,
      WTF::String system_prompt,
      Vector<mojom::blink::AIAssistantInitialPromptPtr> initial_prompts)
      : AIMojoClient(ai, resolver, signal),
        ai_(ai),
        receiver_(this, ai->GetExecutionContext()) {
    mojo::PendingRemote<mojom::blink::AIManagerCreateAssistantClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   ai->GetTaskRunner());
    ai_->GetAIRemote()->CreateAssistant(
        std::move(client_remote), mojom::blink::AIAssistantCreateOptions::New(
                                      std::move(sampling_params), system_prompt,
                                      std::move(initial_prompts)));
  }
  ~CreateAssistantClient() override = default;

  CreateAssistantClient(const CreateAssistantClient&) = delete;
  CreateAssistantClient& operator=(const CreateAssistantClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIMojoClient::Trace(visitor);
    visitor->Trace(ai_);
    visitor->Trace(receiver_);
  }

  void OnResult(mojo::PendingRemote<mojom::blink::AIAssistant> assistant_remote,
                mojom::blink::AIAssistantInfoPtr info) override {
    if (!GetResolver()) {
      return;
    }

    if (info) {
      AIAssistant* assistant = MakeGarbageCollected<AIAssistant>(
          ai_->GetExecutionContext(), std::move(assistant_remote),
          ai_->GetTaskRunner(), std::move(info), /*current_tokens=*/0);
      GetResolver()->Resolve(assistant);
    } else {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          kExceptionMessageUnableToCreateSession);
    }
    Cleanup();
  }

 private:
  Member<AI> ai_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateAssistantClient,
                   CreateAssistantClient>
      receiver_;
};

}  // namespace

AIAssistantFactory::AIAssistantFactory(AI* ai)
    : ExecutionContextClient(ai->GetExecutionContext()),
      ai_(ai),
      task_runner_(ai->GetTaskRunner()) {}

void AIAssistantFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_);
}

void AIAssistantFactory::OnGetModelInfoComplete(
    ScriptPromiseResolver<AIAssistantCapabilities>* resolver,
    AIAssistantCapabilities* capabilities,
    mojom::blink::AIModelInfoPtr model_info) {
  CHECK(model_info);
  capabilities->SetDefaultTopK(model_info->default_top_k);
  capabilities->SetMaxTopK(model_info->max_top_k);
  capabilities->SetDefaultTemperature(model_info->default_temperature);
  resolver->Resolve(capabilities);
}

void AIAssistantFactory::OnCanCreateSessionComplete(
    ScriptPromiseResolver<AIAssistantCapabilities>* resolver,
    mojom::blink::ModelAvailabilityCheckResult check_result) {
  AICapabilityAvailability availability = HandleModelAvailabilityCheckResult(
      GetExecutionContext(), AIMetrics::AISessionType::kAssistant,
      check_result);
  auto* capabilities = MakeGarbageCollected<AIAssistantCapabilities>(
      AICapabilityAvailabilityToV8(availability));
  if (availability == AICapabilityAvailability::kNo) {
    resolver->Resolve(capabilities);
    return;
  }

  ai_->GetAIRemote()->GetModelInfo(WTF::BindOnce(
      &AIAssistantFactory::OnGetModelInfoComplete, WrapPersistent(this),
      WrapPersistent(resolver), WrapPersistent(capabilities)));
}

ScriptPromise<AIAssistantCapabilities> AIAssistantFactory::capabilities(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AIAssistantCapabilities>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AIAssistantCapabilities>>(
          script_state);
  auto promise = resolver->Promise();

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kAssistant),
      AIMetrics::AIAPI::kCanCreateSession);

  ai_->GetAIRemote()->CanCreateAssistant(
      WTF::BindOnce(&AIAssistantFactory::OnCanCreateSessionComplete,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

// TODO(crbug.com/348108460): block the promise until the model is downloaded,
// and add the download progress monitor support.
ScriptPromise<AIAssistant> AIAssistantFactory::create(
    ScriptState* script_state,
    const AIAssistantCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AIAssistant>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AIAssistant>>(script_state);
  auto promise = resolver->Promise();
  mojom::blink::AIAssistantSamplingParamsPtr sampling_params;
  WTF::String system_prompt;
  Vector<mojom::blink::AIAssistantInitialPromptPtr> initial_prompts;

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kAssistant),
      AIMetrics::AIAPI::kCreateSession);

  if (!ai_->GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  AbortSignal* signal = nullptr;

  if (options) {
    signal = options->getSignalOr(nullptr);
    if (signal && signal->aborted()) {
      ThrowAbortedException(exception_state);
      return ScriptPromise<AIAssistant>();
    }

    if (!options->hasTopK() && !options->hasTemperature()) {
      sampling_params = nullptr;
    } else if (options->hasTopK() && options->hasTemperature()) {
      sampling_params = mojom::blink::AIAssistantSamplingParams::New(
          options->topK(), options->temperature());
    } else {
      resolver->Reject(DOMException::Create(
          kExceptionMessageInvalidTemperatureAndTopKFormat,
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
      return promise;
    }

    if (options->hasSystemPrompt() && options->hasInitialPrompts()) {
      // If the `systemPrompt` and `initialPrompts` are both set, reject with a
      // `TypeError`.
      resolver->RejectWithTypeError(
          kExceptionMessageSystemPromptAndInitialPromptsExist);
      return promise;
    }
    if (options->hasSystemPrompt()) {
      system_prompt = options->systemPrompt();
    } else if (options->hasInitialPrompts()) {
      auto& prompts = options->initialPrompts();
      if (prompts.size() > 0) {
        size_t start_index = 0;
        // Only the first prompt can have a `system` role, so it's handled
        // separately.
        auto* first_prompt = prompts.begin()->Get();
        if (first_prompt->role() ==
            V8AIAssistantInitialPromptRole::Enum::kSystem) {
          system_prompt = first_prompt->content();
          start_index++;
        }
        for (size_t index = start_index; index < prompts.size(); ++index) {
          auto prompt = prompts[index];
          if (prompt->role() == V8AIAssistantInitialPromptRole::Enum::kSystem) {
            // If any prompt except the first one has a `system` role, reject
            // with a `TypeError`.
            resolver->RejectWithTypeError(
                kExceptionMessageSystemPromptIsNotTheFirst);
            return promise;
          }
          initial_prompts.push_back(mojom::blink::AIAssistantInitialPrompt::New(
              AIAssistantInitialPromptRole(prompt->role()), prompt->content()));
        }
      }
    }
  }

  MakeGarbageCollected<CreateAssistantClient>(
      ai_, resolver, signal, std::move(sampling_params), system_prompt,
      std::move(initial_prompts));

  return promise;
}

}  // namespace blink
