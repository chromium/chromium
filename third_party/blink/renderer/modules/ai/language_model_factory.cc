
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model_factory.h"

#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_availability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_create_monitor_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_expected_input.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_prompt_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_prompt_role.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt_input.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_languagemodelpromptdict_string.h"
#include "third_party/blink/renderer/core/events/progress_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/availability.h"
#include "third_party/blink/renderer/modules/ai/create_monitor.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/language_model.h"
#include "third_party/blink/renderer/modules/ai/language_model_params.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

namespace {

mojom::blink::AILanguageModelPromptType ToMojoInputType(
    V8LanguageModelPromptType type) {
  switch (type.AsEnum()) {
    case V8LanguageModelPromptType::Enum::kText:
      return mojom::blink::AILanguageModelPromptType::kText;
    case V8LanguageModelPromptType::Enum::kAudio:
      return mojom::blink::AILanguageModelPromptType::kAudio;
    case V8LanguageModelPromptType::Enum::kImage:
      return mojom::blink::AILanguageModelPromptType::kImage;
  }
}

Vector<mojom::blink::AILanguageModelExpectedInputPtr> ToMojoExpectedInputs(
    const HeapVector<Member<LanguageModelExpectedInput>>& expected_inputs) {
  Vector<mojom::blink::AILanguageModelExpectedInputPtr> result;
  result.reserve(expected_inputs.size());
  std::ranges::transform(
      expected_inputs, std::back_inserter(result),
      [](const Member<LanguageModelExpectedInput>& expected_input) {
        auto value = mojom::blink::AILanguageModelExpectedInput::New();
        value->type = ToMojoInputType(expected_input->type());
        if (expected_input->hasLanguages()) {
          value->languages = ToMojoLanguageCodes(expected_input->languages());
        }
        return value;
      });
  return result;
}

class CreateLanguageModelClient
    : public GarbageCollected<CreateLanguageModelClient>,
      public mojom::blink::AIManagerCreateLanguageModelClient,
      public AIContextObserver<LanguageModel> {
 public:
  CreateLanguageModelClient(
      ScriptState* script_state,
      AI* ai,
      ScriptPromiseResolver<LanguageModel>* resolver,
      AbortSignal* signal,
      mojom::blink::AILanguageModelSamplingParamsPtr sampling_params,
      Vector<mojom::blink::AILanguageModelPromptPtr> initial_prompts,
      CreateMonitor* monitor,
      Vector<mojom::blink::AILanguageModelExpectedInputPtr> expected_inputs)
      : AIContextObserver(script_state, ai, resolver, signal),
        ai_(ai),
        monitor_(monitor),
        receiver_(this, ai->GetExecutionContext()) {
    if (monitor) {
      ai_->GetAIRemote()->AddModelDownloadProgressObserver(
          monitor->BindRemote());
    }

    mojo::PendingRemote<mojom::blink::AIManagerCreateLanguageModelClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   ai->GetTaskRunner());
    ai_->GetAIRemote()->CreateLanguageModel(
        std::move(client_remote),
        mojom::blink::AILanguageModelCreateOptions::New(
            std::move(sampling_params), std::move(initial_prompts),
            std::move(expected_inputs)));
  }
  ~CreateLanguageModelClient() override = default;

  CreateLanguageModelClient(const CreateLanguageModelClient&) = delete;
  CreateLanguageModelClient& operator=(const CreateLanguageModelClient&) =
      delete;

  void Trace(Visitor* visitor) const override {
    AIContextObserver::Trace(visitor);
    visitor->Trace(ai_);
    visitor->Trace(monitor_);
    visitor->Trace(receiver_);
  }

  // mojom::blink::AIManagerCreateLanguageModelClient implementation.
  void OnResult(
      mojo::PendingRemote<mojom::blink::AILanguageModel> language_model_remote,
      mojom::blink::AILanguageModelInstanceInfoPtr info) override {
    if (!GetResolver()) {
      return;
    }

    if (monitor_) {
      // Ensure that a download completion event is sent.
      monitor_->OnDownloadProgressUpdate(kNormalizedDownloadProgressMax,
                                         kNormalizedDownloadProgressMax);
    }

    CHECK(info);
    LanguageModel* language_model = MakeGarbageCollected<LanguageModel>(
        ai_->GetExecutionContext(), std::move(language_model_remote),
        ai_->GetTaskRunner(), std::move(info));
    GetResolver()->Resolve(language_model);

    Cleanup();
  }

  void OnError(mojom::blink::AIManagerCreateClientError error) override {
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
        GetResolver()->RejectWithDOMException(
            DOMExceptionCode::kQuotaExceededError,
            kExceptionMessageInputTooLarge);
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

  void ResetReceiver() override { receiver_.reset(); }

 private:
  Member<AI> ai_;
  // The `CreateLanguageModelClient` owns the `CreateMonitor`, so the
  // `LanguageModel.create()` will only receive model download progress
  // update while the creation promise is pending. After the `LanguageModel`
  // is created, the `CreateMonitor` will be destroyed so there is no more
  // events even if the model is uninstalled and downloaded again.
  Member<CreateMonitor> monitor_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateLanguageModelClient,
                   CreateLanguageModelClient>
      receiver_;
};

}  // namespace

LanguageModelFactory::LanguageModelFactory(AI* ai)
    : ExecutionContextClient(ai->GetExecutionContext()),
      ai_(ai),
      task_runner_(ai->GetTaskRunner()) {}

void LanguageModelFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_);
}

void LanguageModelFactory::OnCanCreateLanguageModelComplete(
    ScriptPromiseResolver<V8Availability>* resolver,
    mojom::blink::ModelAvailabilityCheckResult check_result) {
  Availability availability = HandleModelAvailabilityCheckResult(
      GetExecutionContext(), AIMetrics::AISessionType::kLanguageModel,
      check_result);
  resolver->Resolve(AvailabilityToV8(availability));
}

ScriptPromise<V8Availability> LanguageModelFactory::availability(
    ScriptState* script_state,
    const LanguageModelCreateCoreOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<V8Availability>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8Availability>>(script_state);
  auto promise = resolver->Promise();

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kCanCreateSession);
  mojom::blink::AILanguageModelSamplingParamsPtr sampling_params;
  Vector<mojom::blink::AILanguageModelExpectedInputPtr> expected_inputs;
  Vector<mojom::blink::AILanguageModelPromptPtr> initial_prompts;
  if (options && options->hasExpectedInputs()) {
    expected_inputs = ToMojoExpectedInputs(options->expectedInputs());
  }

  auto sampling_params_or_exception = ResolveSamplingParamsOption(options);
  if (!sampling_params_or_exception.has_value()) {
    resolver->Resolve(AvailabilityToV8(Availability::kUnavailable));
    return promise;
  }
  sampling_params = std::move(sampling_params_or_exception.value());

  ai_->GetAIRemote()->CanCreateLanguageModel(
      mojom::blink::AILanguageModelCreateOptions::New(
          std::move(sampling_params), std::move(initial_prompts),
          std::move(expected_inputs)),
      WTF::BindOnce(&LanguageModelFactory::OnCanCreateLanguageModelComplete,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void LanguageModelFactory::OnGetLanguageModelParamsComplete(
    ScriptPromiseResolver<IDLNullable<LanguageModelParams>>* resolver,
    mojom::blink::AILanguageModelParamsPtr language_model_params) {
  if (!language_model_params) {
    resolver->Resolve(nullptr);
    return;
  }

  auto* params = MakeGarbageCollected<LanguageModelParams>(
      language_model_params->default_sampling_params->top_k,
      language_model_params->max_sampling_params->top_k,
      language_model_params->default_sampling_params->temperature,
      language_model_params->max_sampling_params->temperature);

  resolver->Resolve(params);
}

ScriptPromise<IDLNullable<LanguageModelParams>> LanguageModelFactory::params(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLNullable<LanguageModelParams>>();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLNullable<LanguageModelParams>>>(script_state);
  auto promise = resolver->Promise();

  ai_->GetAIRemote()->GetLanguageModelParams(
      WTF::BindOnce(&LanguageModelFactory::OnGetLanguageModelParamsComplete,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

ScriptPromise<LanguageModel> LanguageModelFactory::create(
    ScriptState* script_state,
    const LanguageModelCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<LanguageModel>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<LanguageModel>>(script_state);
  auto promise = resolver->Promise();

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kCreateSession);

  if (!ai_->GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  mojom::blink::AILanguageModelSamplingParamsPtr sampling_params;
  Vector<mojom::blink::AILanguageModelExpectedInputPtr> expected_inputs;
  Vector<mojom::blink::AILanguageModelPromptPtr> initial_prompts;
  AbortSignal* signal = nullptr;
  CreateMonitor* monitor =
      MakeGarbageCollected<CreateMonitor>(GetExecutionContext(), task_runner_);

  if (options) {
    signal = options->getSignalOr(nullptr);
    if (signal && signal->aborted()) {
      resolver->Reject(signal->reason(script_state));
      return promise;
    }

    if (options->hasMonitor()) {
      std::ignore = options->monitor()->Invoke(nullptr, monitor);
    }

    auto sampling_params_or_exception = ResolveSamplingParamsOption(options);
    if (!sampling_params_or_exception.has_value()) {
      switch (sampling_params_or_exception.error()) {
        case SamplingParamsOptionError::kOnlyOneOfTopKAndTemperatureIsProvided:
          resolver->Reject(DOMException::Create(
              kExceptionMessageInvalidTemperatureAndTopKFormat,
              DOMException::GetErrorName(
                  DOMExceptionCode::kNotSupportedError)));
          break;
        case SamplingParamsOptionError::kInvalidTopK:
          resolver->RejectWithRangeError(kExceptionMessageInvalidTopK);
          break;
        case SamplingParamsOptionError::kInvalidTemperature:
          resolver->RejectWithRangeError(kExceptionMessageInvalidTemperature);
          break;
      }
      return promise;
    }
    sampling_params = std::move(sampling_params_or_exception.value());

    if (options->hasExpectedInputs()) {
      expected_inputs = ToMojoExpectedInputs(options->expectedInputs());
    }

    // TODO(crbug.com/381974893): Remove this warning after a couple milestones.
    if (options->hasSystemPrompt()) {
      GetExecutionContext()->AddConsoleMessage(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "`systemPrompt` is no longer supported. Use "
          "`initialPrompts: [{role: 'system', content: ... }, ...]` instead.");
    }

    if (options->hasInitialPrompts()) {
      for (const auto& prompt : options->initialPrompts()) {
        // The API impl only accepts a prompt dict for now, more to come soon!
        if (!prompt->IsLanguageModelPromptDict()) {
          resolver->RejectWithTypeError("Input type not supported");
          return promise;
        }
        auto* dict = prompt->GetAsLanguageModelPromptDict();
        if (dict->role() == V8LanguageModelPromptRole::Enum::kSystem &&
            !initial_prompts.empty()) {
          // Only the first prompt supports the `system` role.
          resolver->RejectWithTypeError(
              kExceptionMessageSystemPromptIsNotTheFirst);
          return promise;
        }
        // The API impl only accepts string for now, more to come soon!
        if (!dict->content()->IsString()) {
          resolver->RejectWithTypeError("Input type not supported");
          return promise;
        }
        initial_prompts.push_back(mojom::blink::AILanguageModelPrompt::New(
            LanguageModel::ConvertRoleToMojo(dict->role()),
            mojom::blink::AILanguageModelPromptContent::NewText(
                dict->content()->GetAsString())));
      }
    }
  }

  MakeGarbageCollected<CreateLanguageModelClient>(
      script_state, ai_, resolver, signal, std::move(sampling_params),
      std::move(initial_prompts), monitor, std::move(expected_inputs));

  return promise;
}

}  // namespace blink
