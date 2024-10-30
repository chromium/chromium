// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_language_model_factory.h"

#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_create_monitor_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_model_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_model_initial_prompt.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_model_initial_prompt_role.h"
#include "third_party/blink/renderer/core/events/progress_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_capability_availability.h"
#include "third_party/blink/renderer/modules/ai/ai_create_monitor.h"
#include "third_party/blink/renderer/modules/ai/ai_language_model.h"
#include "third_party/blink/renderer/modules/ai/ai_language_model_capabilities.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

namespace {

mojom::blink::AIAssistantInitialPromptRole AILanguageModelInitialPromptRole(
    V8AILanguageModelInitialPromptRole role) {
  switch (role.AsEnum()) {
    case V8AILanguageModelInitialPromptRole::Enum::kSystem:
      return mojom::blink::AIAssistantInitialPromptRole::kSystem;
    case V8AILanguageModelInitialPromptRole::Enum::kUser:
      return mojom::blink::AIAssistantInitialPromptRole::kUser;
    case V8AILanguageModelInitialPromptRole::Enum::kAssistant:
      return mojom::blink::AIAssistantInitialPromptRole::kAssistant;
  }
  NOTREACHED();
}

class CreateLanguageModelClient
    : public GarbageCollected<CreateLanguageModelClient>,
      public mojom::blink::AIManagerCreateAssistantClient,
      public AIMojoClient<AILanguageModel> {
 public:
  CreateLanguageModelClient(
      ScriptState* script_state,
      AI* ai,
      ScriptPromiseResolver<AILanguageModel>* resolver,
      AbortSignal* signal,
      mojom::blink::AIAssistantSamplingParamsPtr sampling_params,
      WTF::String system_prompt,
      Vector<mojom::blink::AIAssistantInitialPromptPtr> initial_prompts,
      AICreateMonitor* monitor)
      : AIMojoClient(script_state, ai, resolver, signal),
        ai_(ai),
        monitor_(monitor),
        receiver_(this, ai->GetExecutionContext()) {
    if (monitor) {
      ai_->GetAIRemote()->AddModelDownloadProgressObserver(
          monitor->BindRemote());
    }

    mojo::PendingRemote<mojom::blink::AIManagerCreateAssistantClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   ai->GetTaskRunner());
    ai_->GetAIRemote()->CreateAssistant(
        std::move(client_remote), mojom::blink::AIAssistantCreateOptions::New(
                                      std::move(sampling_params), system_prompt,
                                      std::move(initial_prompts)));
  }
  ~CreateLanguageModelClient() override = default;

  CreateLanguageModelClient(const CreateLanguageModelClient&) = delete;
  CreateLanguageModelClient& operator=(const CreateLanguageModelClient&) =
      delete;

  void Trace(Visitor* visitor) const override {
    AIMojoClient::Trace(visitor);
    visitor->Trace(ai_);
    visitor->Trace(monitor_);
    visitor->Trace(receiver_);
  }

  void OnResult(
      mojo::PendingRemote<mojom::blink::AIAssistant> language_model_remote,
      mojom::blink::AIAssistantInfoPtr info) override {
    if (!GetResolver()) {
      return;
    }

    if (info) {
      AILanguageModel* language_model = MakeGarbageCollected<AILanguageModel>(
          ai_->GetExecutionContext(), std::move(language_model_remote),
          ai_->GetTaskRunner(), std::move(info), /*current_tokens=*/0);
      GetResolver()->Resolve(language_model);
    } else {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          kExceptionMessageUnableToCreateSession);
    }
    Cleanup();
  }

  void ResetReceiver() override { receiver_.reset(); }

 private:
  Member<AI> ai_;
  // The `CreateLanguageModelClient` owns the `AICreateMonitor`, so the
  // `ai.languageModel.create()` will only receive model download progress
  // update while the creation promise is pending. After the `AILanguageModel`
  // is created, the `AICreateMonitor` will be destroyed so there is no more
  // events even if the model is uninstalled and downloaded again.
  Member<AICreateMonitor> monitor_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateAssistantClient,
                   CreateLanguageModelClient>
      receiver_;
};

}  // namespace

AILanguageModelFactory::AILanguageModelFactory(AI* ai)
    : ExecutionContextClient(ai->GetExecutionContext()),
      ai_(ai),
      task_runner_(ai->GetTaskRunner()) {}

void AILanguageModelFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_);
}

void AILanguageModelFactory::OnGetModelInfoComplete(
    ScriptPromiseResolver<AILanguageModelCapabilities>* resolver,
    AILanguageModelCapabilities* capabilities,
    mojom::blink::AIModelInfoPtr model_info) {
  CHECK(model_info);
  capabilities->SetDefaultTopK(model_info->default_top_k);
  capabilities->SetMaxTopK(model_info->max_top_k);
  capabilities->SetDefaultTemperature(model_info->default_temperature);
  resolver->Resolve(capabilities);
}

void AILanguageModelFactory::OnCanCreateSessionComplete(
    ScriptPromiseResolver<AILanguageModelCapabilities>* resolver,
    mojom::blink::ModelAvailabilityCheckResult check_result) {
  AICapabilityAvailability availability = HandleModelAvailabilityCheckResult(
      GetExecutionContext(), AIMetrics::AISessionType::kLanguageModel,
      check_result);
  auto* capabilities = MakeGarbageCollected<AILanguageModelCapabilities>(
      AICapabilityAvailabilityToV8(availability));
  if (availability == AICapabilityAvailability::kNo) {
    resolver->Resolve(capabilities);
    return;
  }

  ai_->GetAIRemote()->GetModelInfo(WTF::BindOnce(
      &AILanguageModelFactory::OnGetModelInfoComplete, WrapPersistent(this),
      WrapPersistent(resolver), WrapPersistent(capabilities)));
}

ScriptPromise<AILanguageModelCapabilities> AILanguageModelFactory::capabilities(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AILanguageModelCapabilities>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AILanguageModelCapabilities>>(
          script_state);
  auto promise = resolver->Promise();

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kCanCreateSession);

  ai_->GetAIRemote()->CanCreateAssistant(
      WTF::BindOnce(&AILanguageModelFactory::OnCanCreateSessionComplete,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

ScriptPromise<AILanguageModel> AILanguageModelFactory::create(
    ScriptState* script_state,
    const AILanguageModelCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AILanguageModel>();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<AILanguageModel>>(
      script_state);
  auto promise = resolver->Promise();

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kCreateSession);

  if (!ai_->GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  mojom::blink::AIAssistantSamplingParamsPtr sampling_params;
  WTF::String system_prompt;
  WTF::Vector<mojom::blink::AIAssistantInitialPromptPtr> initial_prompts;
  AbortSignal* signal = nullptr;
  AICreateMonitor* monitor = MakeGarbageCollected<AICreateMonitor>(
      GetExecutionContext(), task_runner_);

  if (options) {
    signal = options->getSignalOr(nullptr);
    if (signal && signal->aborted()) {
      resolver->Reject(signal->reason(script_state));
      return promise;
    }

    if (options->hasMonitor()) {
      std::ignore = options->monitor()->Invoke(nullptr, monitor);
    }

    // The temperature and top_k are optional, but they must be provided
    // together.
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

    if (options->hasSystemPrompt()) {
      system_prompt = options->systemPrompt();
    }

    if (options->hasInitialPrompts()) {
      auto& prompts = options->initialPrompts();
      if (prompts.size() > 0) {
        size_t start_index = 0;
        // Only the first prompt might have a `system` role, so it's handled
        // separately.
        auto* first_prompt = prompts.begin()->Get();
        if (first_prompt->role() ==
            V8AILanguageModelInitialPromptRole::Enum::kSystem) {
          if (options->hasSystemPrompt()) {
            // If the system prompt cannot be provided both from system prompt
            // and initial prompts, so reject with a `TypeError`.
            resolver->RejectWithTypeError(
                kExceptionMessageSystemPromptIsDefinedMultipleTimes);
            return promise;
          }
          system_prompt = first_prompt->content();
          start_index++;
        }
        for (size_t index = start_index; index < prompts.size(); ++index) {
          auto prompt = prompts[index];
          if (prompt->role() ==
              V8AILanguageModelInitialPromptRole::Enum::kSystem) {
            // If any prompt except the first one has a `system` role, reject
            // with a `TypeError`.
            resolver->RejectWithTypeError(
                kExceptionMessageSystemPromptIsNotTheFirst);
            return promise;
          }
          initial_prompts.push_back(mojom::blink::AIAssistantInitialPrompt::New(
              AILanguageModelInitialPromptRole(prompt->role()),
              prompt->content()));
        }
      }
    }
  }

  MakeGarbageCollected<CreateLanguageModelClient>(
      script_state, ai_, resolver, signal, std::move(sampling_params),
      system_prompt, std::move(initial_prompts), monitor);

  return promise;
}

}  // namespace blink
