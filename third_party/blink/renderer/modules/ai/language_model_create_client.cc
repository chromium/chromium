// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model_create_client.h"

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_create_monitor_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_message_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_languagemodelmessagecontentsequence_string.h"
#include "third_party/blink/renderer/core/dom/quota_exceeded_error.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai_features.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/language_model_prompt_builder.h"

namespace blink {

LanguageModelCreateClient::LanguageModelCreateClient(
    ScriptPromiseResolver<LanguageModel>* resolver,
    LanguageModelCreateOptions* options,
    mojom::blink::AILanguageModelSamplingParamsPtr resolved_sampling_params)
    : ExecutionContextClient(
          ExecutionContext::From(resolver->GetScriptState())),
      AIContextObserver(resolver->GetScriptState(),
                        this,
                        resolver,
                        options->getSignalOr(nullptr)),
      receiver_(this, GetExecutionContext()),
      options_(options),
      resolved_sampling_params_(std::move(resolved_sampling_params)),
      task_runner_(
          GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault)) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());

  if (options->hasMonitor()) {
    monitor_ = MakeGarbageCollected<CreateMonitor>(
        GetExecutionContext(), options->getSignalOr(nullptr), task_runner_);
    // If an exception is thrown, don't initiate the model download.
    // `AICreateMonitorCallback`'s `Invoke` will automatically reject the
    // promise with the thrown exception.
    if (options->monitor()->Invoke(nullptr, monitor_).IsNothing()) {
      return;
    }
    ai_manager_remote->AddModelDownloadProgressObserver(monitor_->BindRemote());
  }

  LanguageModel::ExecuteAvailability(
      ai_manager_remote, options, resolved_sampling_params_.Clone(),
      BindOnce(&LanguageModelCreateClient::Create, WrapPersistent(this)));
}

LanguageModelCreateClient::~LanguageModelCreateClient() = default;

void LanguageModelCreateClient::Trace(Visitor* visitor) const {
  ExecutionContextClient::Trace(visitor);
  AIContextObserver::Trace(visitor);
  visitor->Trace(receiver_);
  visitor->Trace(options_);
  visitor->Trace(monitor_);
}

void LanguageModelCreateClient::Create(
    mojom::blink::ModelAvailabilityCheckResult result) {
  // Abort may have been triggered by `OnDownloadProgressUpdate`.
  if (!GetResolver()) {
    return;
  }

  Availability availability = ConvertModelAvailabilityCheckResult(result);
  if (availability == Availability::kUnavailable) {
    GetResolver()->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError,
        ConvertModelAvailabilityCheckResultToDebugString(result));
    return;
  }

  ScriptState* script_state = GetScriptState();
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);

  // Prompt APIs are only available within window and extension worker
  // contexts by default. User activation is not consumed by workers,
  // as they lack the ability to do so.
  if (window && RequiresUserActivation(availability) &&
      !MeetsUserActivationRequirements(window)) {
    GetResolver()->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError,
        kExceptionMessageUserActivationRequired);
    return;
  }

  HashSet<mojom::blink::AILanguageModelPromptType> maybe_allowed_types;
  maybe_allowed_types.insert(mojom::blink::AILanguageModelPromptType::kText);
  Vector<mojom::blink::AILanguageModelExpectedPtr> expected_in, expected_out;
  if (options_->hasExpectedInputs()) {
    expected_in = ToMojoExpectations(options_->expectedInputs());
    for (const auto& expected : expected_in) {
      // TODO(crbug.com/422803232): reject when `expected` has tool types
      // without AIPromptAPIToolUse runtime feature enabled.
      if (expected->type != mojom::blink::AILanguageModelPromptType::kText &&
          !RuntimeEnabledFeatures::AIPromptAPIMultimodalInputEnabled(
              GetExecutionContext())) {
        GetResolver()->Reject(DOMException::Create(
            kExceptionMessageUnableToCreateSession,
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
      }
      // TODO(crbug.com/417817645): Check model capabilities before conversion.
      maybe_allowed_types.insert(expected->type);
    }
  }
  if (options_->hasExpectedOutputs()) {
    expected_out = ToMojoExpectations(options_->expectedOutputs());
    for (const auto& expected : expected_out) {
      // TODO(crbug.com/422803232): reject when `expected` has tool types
      // without AIPromptAPIToolUse runtime feature enabled.
      if (expected->type != mojom::blink::AILanguageModelPromptType::kText) {
        GetResolver()->Reject(DOMException::Create(
            kExceptionMessageUnableToCreateSession,
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
      }
    }
  }

  if (!options_->hasInitialPrompts() || options_->initialPrompts().empty()) {
    OnInitialPromptsResolved(std::move(expected_in), std::move(expected_out),
                             /*initial_prompts=*/{});
    return;
  }

  // TODO(crbug.com/419583879): Add better test coverage for initialPrompts.
  for (const auto& message : options_->initialPrompts()) {
    if (message->role() == V8LanguageModelMessageRole::Enum::kSystem &&
        message != options_->initialPrompts().front()) {
      // Only the first prompt supports the `system` role.
      GetResolver()->RejectWithTypeError(
          kExceptionMessagePromptWithSystemRoleIsNotTheFirst);
      return;
    }
    if (message->prefix()) {
      GetResolver()->Reject(DOMException::Create(
          "initialPrompts cannot specify an assistant response prefix.",
          DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
      return;
    }
  }

  if (options_->hasTools()) {
    // Check if the AIPromptAPIToolUse feature is enabled.
    if (!RuntimeEnabledFeatures::AIPromptAPIToolUseEnabled(
            GetExecutionContext())) {
      GetResolver()->Reject(DOMException::Create(
          "Tool use feature is not enabled",
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
      return;
    }
  }

  ConvertPromptInputsToMojo(
      GetScriptState(), options_->getSignalOr(nullptr),
      MakeGarbageCollected<V8LanguageModelPrompt>(options_->initialPrompts()),
      maybe_allowed_types, /*json_schema=*/"",
      BindOnce(&LanguageModelCreateClient::OnInitialPromptsResolved,
               WrapPersistent(this), std::move(expected_in),
               std::move(expected_out)),
      BindOnce(&LanguageModelCreateClient::OnInitialPromptsRejected,
               WrapPersistent(this)));
}

void LanguageModelCreateClient::OnResult(
    mojo::PendingRemote<mojom::blink::AILanguageModel> pending_remote,
    mojom::blink::AILanguageModelInstanceInfoPtr info) {
  if (!GetResolver()) {
    return;
  }
  if (pending_remote && monitor_) {
    // Ensure that a download completion event is sent.
    monitor_->OnDownloadProgressUpdate(0, kNormalizedDownloadProgressMax);

    // Abort may have been triggered by `OnDownloadProgressUpdate`.
    if (!this->GetResolver()) {
      return;
    }

    // Ensure that a download completion event is sent.
    monitor_->OnDownloadProgressUpdate(kNormalizedDownloadProgressMax,
                                       kNormalizedDownloadProgressMax);

    // Abort may have been triggered by `OnDownloadProgressUpdate`.
    if (!this->GetResolver()) {
      return;
    }
  }

  CHECK(info);
  if (GetExecutionContext() && pending_remote) {
    GetResolver()->Resolve(MakeGarbageCollected<LanguageModel>(
        GetExecutionContext(), std::move(pending_remote), task_runner_,
        std::move(info)));
  } else {
    GetResolver()->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        kExceptionMessageUnableToCreateSession);
  }
  Cleanup();
}

void LanguageModelCreateClient::OnError(
    mojom::blink::AIManagerCreateClientError error,
    mojom::blink::QuotaErrorInfoPtr quota_error_info) {
  if (!GetResolver()) {
    return;
  }

  using mojom::blink::AIManagerCreateClientError;

  switch (error) {
    case AIManagerCreateClientError::kUnableToCreateSession:
    case AIManagerCreateClientError::kUnableToCalculateTokenSize: {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          kExceptionMessageUnableToCreateSession);
      break;
    }
    case AIManagerCreateClientError::kInitialInputTooLarge: {
      CHECK(quota_error_info);
      QuotaExceededError::Reject(
          GetResolver(), kExceptionMessageInputTooLarge,
          static_cast<double>(quota_error_info->quota),
          static_cast<double>(quota_error_info->requested));
      break;
    }
    case AIManagerCreateClientError::kUnsupportedLanguage: {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kNotSupportedError,
          kExceptionMessageUnsupportedLanguages);
      break;
    }
  }
  Cleanup();
}

void LanguageModelCreateClient::ResetReceiver() {
  receiver_.reset();
}

void LanguageModelCreateClient::OnInitialPromptsResolved(
    Vector<mojom::blink::AILanguageModelExpectedPtr> expected_inputs,
    Vector<mojom::blink::AILanguageModelExpectedPtr> expected_outputs,
    Vector<mojom::blink::AILanguageModelPromptPtr> initial_prompts) {
  if (!GetResolver()) {
    return;
  }
  mojo::PendingRemote<mojom::blink::AIManagerCreateLanguageModelClient>
      client_remote;
  receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(), task_runner_);
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CreateLanguageModel(
      std::move(client_remote),
      mojom::blink::AILanguageModelCreateOptions::New(
          resolved_sampling_params_.Clone(), std::move(initial_prompts),
          std::move(expected_inputs), std::move(expected_outputs)));
}

void LanguageModelCreateClient::OnInitialPromptsRejected(
    const ScriptValue& error) {
  if (GetResolver()) {
    GetResolver()->Reject(error);
  }
}

}  // namespace blink
