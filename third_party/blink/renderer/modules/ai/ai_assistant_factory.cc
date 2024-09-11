// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_assistant_factory.h"

#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_assistant_initial_prompt.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_assistant_initial_prompt_role.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_assistant.h"
#include "third_party/blink/renderer/modules/ai/ai_assistant_capabilities.h"
#include "third_party/blink/renderer/modules/ai/ai_capability_availability.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

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
  NOTREACHED_IN_MIGRATION();
}

}  // namespace

AIAssistantFactory::AIAssistantFactory(AI* ai)
    : ExecutionContextClient(ai->GetExecutionContext()),
      ai_(ai),
      text_session_factory_(
          MakeGarbageCollected<AITextSessionFactory>(ai->GetExecutionContext(),
                                                     ai->GetTaskRunner())),
      task_runner_(ai->GetTaskRunner()) {}

void AIAssistantFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_);
  visitor->Trace(text_session_factory_);
}

void AIAssistantFactory::OnGetTextModelInfoComplete(
    ScriptPromiseResolver<AIAssistantCapabilities>* resolver,
    AIAssistantCapabilities* capabilities,
    mojom::blink::AITextModelInfoPtr text_model_info) {
  CHECK(text_model_info);
  capabilities->SetDefaultTopK(text_model_info->default_top_k);
  capabilities->SetMaxTopK(text_model_info->max_top_k);
  capabilities->SetDefaultTemperature(text_model_info->default_temperature);
  resolver->Resolve(capabilities);
}

void AIAssistantFactory::OnCanCreateSessionComplete(
    ScriptPromiseResolver<AIAssistantCapabilities>* resolver,
    AICapabilityAvailability availability,
    ModelAvailabilityCheckResult check_result) {
  auto* capabilities = MakeGarbageCollected<AIAssistantCapabilities>(
      AICapabilityAvailabilityToV8(availability));
  if (availability == AICapabilityAvailability::kNo) {
    resolver->Resolve(capabilities);
    return;
  }

  ai_->GetAIRemote()->GetTextModelInfo(WTF::BindOnce(
      &AIAssistantFactory::OnGetTextModelInfoComplete, WrapPersistent(this),
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
  text_session_factory_->CanCreateTextSession(
      AIMetrics::AISessionType::kAssistant,
      WTF::BindOnce(&AIAssistantFactory::OnCanCreateSessionComplete,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

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
  mojom::blink::AITextSessionSamplingParamsPtr sampling_params;
  WTF::String system_prompt;
  Vector<mojom::blink::AIAssistantInitialPromptPtr> initial_prompts;
  if (options) {
    if (!options->hasTopK() && !options->hasTemperature()) {
      sampling_params = nullptr;
    } else if (options->hasTopK() && options->hasTemperature()) {
      sampling_params = mojom::blink::AITextSessionSamplingParams::New(
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

  text_session_factory_->CreateTextSession(
      AIMetrics::AISessionType::kAssistant, std::move(sampling_params),
      system_prompt, std::move(initial_prompts),
      WTF::BindOnce(
          [](ScriptPromiseResolver<AIAssistant>* resolver,
             AIAssistantFactory* factory,
             base::expected<AITextSession*, DOMException*> result) {
            if (result.has_value()) {
              resolver->Resolve(MakeGarbageCollected<AIAssistant>(
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
