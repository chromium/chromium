// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_assistant_factory.h"

#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_assistant_capabilities.h"
#include "third_party/blink/renderer/modules/ai/ai_capability_availability.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

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
      WTF::BindOnce(&AIAssistantFactory::OnCanCreateSessionComplete,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

ScriptPromise<AITextSession> AIAssistantFactory::create(
    ScriptState* script_state,
    const AITextSessionOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AITextSession>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AITextSession>>(script_state);
  auto promise = resolver->Promise();
  mojom::blink::AITextSessionSamplingParamsPtr sampling_params;
  WTF::String system_prompt;
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
    if (options->hasSystemPrompt()) {
      system_prompt = options->systemPrompt();
    }
  }

  text_session_factory_->CreateTextSession(
      std::move(sampling_params), system_prompt,
      WTF::BindOnce(
          [](ScriptPromiseResolver<AITextSession>* resolver,
             base::expected<AITextSession*, DOMException*> result) {
            if (result.has_value()) {
              resolver->Resolve(result.value());
            } else {
              resolver->Reject(result.error());
            }
          },
          WrapPersistent(resolver)));

  return promise;
}

}  // namespace blink
