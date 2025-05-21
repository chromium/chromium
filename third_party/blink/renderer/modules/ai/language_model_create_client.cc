// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model_create_client.h"

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_create_monitor_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_prompt_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_languagemodelpromptdict_string.h"
#include "third_party/blink/renderer/core/dom/quota_exceeded_error.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"

namespace blink {

LanguageModelCreateClient::LanguageModelCreateClient(
    ScriptPromiseResolver<LanguageModel>* resolver,
    LanguageModelCreateOptions* options)
    : ExecutionContextClient(
          ExecutionContext::From(resolver->GetScriptState())),
      AIContextObserver(resolver->GetScriptState(),
                        this,
                        resolver,
                        options->getSignalOr(nullptr)),
      receiver_(this, GetExecutionContext()),
      options_(options),
      task_runner_(
          GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault)) {
  if (options->hasMonitor()) {
    monitor_ = MakeGarbageCollected<CreateMonitor>(GetExecutionContext(),
                                                   task_runner_);
    std::ignore = options->monitor()->Invoke(nullptr, monitor_);
    HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
        AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
    ai_manager_remote->AddModelDownloadProgressObserver(monitor_->BindRemote());
  }
}

LanguageModelCreateClient::~LanguageModelCreateClient() = default;

void LanguageModelCreateClient::Trace(Visitor* visitor) const {
  ExecutionContextClient::Trace(visitor);
  AIContextObserver::Trace(visitor);
  visitor->Trace(receiver_);
  visitor->Trace(options_);
  visitor->Trace(monitor_);
}

void LanguageModelCreateClient::Create() {
  mojom::blink::AILanguageModelSamplingParamsPtr sampling_params;
  Vector<mojom::blink::AILanguageModelExpectedInputPtr> expected_inputs;
  Vector<mojom::blink::AILanguageModelPromptPtr> initial_prompts;

  auto sampling_params_or_exception = ResolveSamplingParamsOption(options_);
  if (!sampling_params_or_exception.has_value()) {
    switch (sampling_params_or_exception.error()) {
      case SamplingParamsOptionError::kOnlyOneOfTopKAndTemperatureIsProvided:
        GetResolver()->Reject(DOMException::Create(
            kExceptionMessageInvalidTemperatureAndTopKFormat,
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        break;
      case SamplingParamsOptionError::kInvalidTopK:
        GetResolver()->RejectWithRangeError(kExceptionMessageInvalidTopK);
        break;
      case SamplingParamsOptionError::kInvalidTemperature:
        GetResolver()->RejectWithRangeError(
            kExceptionMessageInvalidTemperature);
        break;
    }
    return;
  }
  sampling_params = std::move(sampling_params_or_exception.value());

  if (options_->hasExpectedInputs()) {
    expected_inputs = ToMojoExpectedInputs(options_->expectedInputs());
  }

  // TODO(crbug.com/381974893): Remove this warning after a couple milestones.
  if (options_->hasSystemPrompt()) {
    GetExecutionContext()->AddConsoleMessage(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "`systemPrompt` is no longer supported. Use "
        "`initialPrompts: [{role: 'system', content: ... }, ...]` instead.");
  }

  if (options_->hasInitialPrompts()) {
    for (const auto& prompt : options_->initialPrompts()) {
      // The API impl only accepts a prompt dict for now, more to come soon!
      if (!prompt->IsLanguageModelPromptDict()) {
        GetResolver()->RejectWithTypeError("Input type not supported");
        return;
      }
      auto* dict = prompt->GetAsLanguageModelPromptDict();
      if (dict->role() == V8LanguageModelPromptRole::Enum::kSystem &&
          !initial_prompts.empty()) {
        // Only the first prompt supports the `system` role.
        GetResolver()->RejectWithTypeError(
            kExceptionMessageSystemPromptIsNotTheFirst);
        return;
      }
      // The API impl only accepts string for now, more to come soon!
      if (!dict->content()->IsString()) {
        GetResolver()->RejectWithTypeError("Input type not supported");
        return;
      }
      initial_prompts.push_back(mojom::blink::AILanguageModelPrompt::New(
          LanguageModel::ConvertRoleToMojo(dict->role()),
          mojom::blink::AILanguageModelPromptContent::NewText(
              dict->content()->GetAsString())));
    }
  }

  mojo::PendingRemote<mojom::blink::AIManagerCreateLanguageModelClient>
      client_remote;
  receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(), task_runner_);
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());
  ai_manager_remote->CreateLanguageModel(
      std::move(client_remote),
      mojom::blink::AILanguageModelCreateOptions::New(
          std::move(sampling_params), std::move(initial_prompts),
          std::move(expected_inputs)));
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
    mojom::blink::AIManagerCreateClientError error) {
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
      QuotaExceededError::Reject(GetResolver(), kExceptionMessageInputTooLarge);
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

}  // namespace blink
