// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_language_model_factory.h"

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
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_availability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_create_monitor_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_model_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_model_prompt_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_model_prompt_role.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_ai_language_model_prompt_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_ai_language_model_prompt_input.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_ailanguagemodelpromptdict_string.h"
#include "third_party/blink/renderer/core/events/progress_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_availability.h"
#include "third_party/blink/renderer/modules/ai/ai_create_monitor.h"
#include "third_party/blink/renderer/modules/ai/ai_language_model.h"
#include "third_party/blink/renderer/modules/ai/ai_language_model_params.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

namespace {

class CreateLanguageModelClient
    : public GarbageCollected<CreateLanguageModelClient>,
      public mojom::blink::AIManagerCreateLanguageModelClient,
      public AIMojoClient<AILanguageModel> {
 public:
  CreateLanguageModelClient(
      ScriptState* script_state,
      AI* ai,
      ScriptPromiseResolver<AILanguageModel>* resolver,
      AbortSignal* signal,
      mojom::blink::AILanguageModelSamplingParamsPtr sampling_params,
      WTF::String system_prompt,
      WTF::Vector<mojom::blink::AILanguageModelPromptPtr> initial_prompts,
      AICreateMonitor* monitor,
      std::optional<WTF::Vector<WTF::String>> expected_input_languages)
      : AIMojoClient(script_state, ai, resolver, signal),
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
    std::optional<Vector<mojom::blink::AILanguageCodePtr>>
        expected_input_language_codes;
    if (expected_input_languages.has_value()) {
      expected_input_language_codes =
          ToMojoLanguageCodes(expected_input_languages.value());
    }
    ai_->GetAIRemote()->CreateLanguageModel(
        std::move(client_remote),
        mojom::blink::AILanguageModelCreateOptions::New(
            std::move(sampling_params), system_prompt,
            std::move(initial_prompts),
            std::move(expected_input_language_codes)));
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

  // mojom::blink::AIManagerCreateLanguageModelClient implementation.
  void OnResult(
      mojo::PendingRemote<mojom::blink::AILanguageModel> language_model_remote,
      mojom::blink::AILanguageModelInstanceInfoPtr info) override {
    if (!GetResolver()) {
      return;
    }

    CHECK(info);
    AILanguageModel* language_model = MakeGarbageCollected<AILanguageModel>(
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
      case AIManagerCreateClientError::kInitialPromptsTooLarge: {
        GetResolver()->RejectWithDOMException(
            DOMExceptionCode::kQuotaExceededError,
            kExceptionMessageInitialPromptTooLarge);
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
  // The `CreateLanguageModelClient` owns the `AICreateMonitor`, so the
  // `ai.languageModel.create()` will only receive model download progress
  // update while the creation promise is pending. After the `AILanguageModel`
  // is created, the `AICreateMonitor` will be destroyed so there is no more
  // events even if the model is uninstalled and downloaded again.
  Member<AICreateMonitor> monitor_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateLanguageModelClient,
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

void AILanguageModelFactory::OnCanCreateLanguageModelComplete(
    ScriptPromiseResolver<V8AIAvailability>* resolver,
    mojom::blink::ModelAvailabilityCheckResult check_result) {
  AIAvailability availability = HandleModelAvailabilityCheckResult(
      GetExecutionContext(), AIMetrics::AISessionType::kLanguageModel,
      check_result);
  resolver->Resolve(AIAvailabilityToV8(availability));
}

ScriptPromise<V8AIAvailability> AILanguageModelFactory::availability(
    ScriptState* script_state,
    const AILanguageModelCreateCoreOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<V8AIAvailability>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8AIAvailability>>(
          script_state);
  auto promise = resolver->Promise();

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kCanCreateSession);
  std::optional<WTF::Vector<mojom::blink::AILanguageCodePtr>>
      expected_language_codes;
  if (options && options->hasExpectedInputLanguages()) {
    expected_language_codes =
        ToMojoLanguageCodes(options->expectedInputLanguages());
  }

  auto result = ResolveSamplingParamsOption(options);
  if (!result.has_value()) {
    resolver->Resolve(AIAvailabilityToV8(AIAvailability::kUnavailable));
    return promise;
  }

  ai_->GetAIRemote()->CanCreateLanguageModel(
      std::move(expected_language_codes),
      WTF::BindOnce(&AILanguageModelFactory::OnCanCreateLanguageModelComplete,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void AILanguageModelFactory::OnGetLanguageModelParamsComplete(
    ScriptPromiseResolver<IDLNullable<AILanguageModelParams>>* resolver,
    mojom::blink::AILanguageModelParamsPtr language_model_params) {
  if (!language_model_params) {
    resolver->Resolve(nullptr);
    return;
  }

  auto* params = MakeGarbageCollected<AILanguageModelParams>(
      language_model_params->default_sampling_params->top_k,
      language_model_params->max_sampling_params->top_k,
      language_model_params->default_sampling_params->temperature,
      language_model_params->max_sampling_params->temperature);

  resolver->Resolve(params);
}

ScriptPromise<IDLNullable<AILanguageModelParams>>
AILanguageModelFactory::params(ScriptState* script_state,
                               ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLNullable<AILanguageModelParams>>();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLNullable<AILanguageModelParams>>>(script_state);
  auto promise = resolver->Promise();

  ai_->GetAIRemote()->GetLanguageModelParams(
      WTF::BindOnce(&AILanguageModelFactory::OnGetLanguageModelParamsComplete,
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

  mojom::blink::AILanguageModelSamplingParamsPtr sampling_params;
  WTF::String system_prompt;
  WTF::Vector<mojom::blink::AILanguageModelPromptPtr> initial_prompts;
  AbortSignal* signal = nullptr;
  AICreateMonitor* monitor = MakeGarbageCollected<AICreateMonitor>(
      GetExecutionContext(), task_runner_);
  std::optional<WTF::Vector<WTF::String>> expected_input_languages;

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

    // The API impl does not yet support expectedInputTypes, more to come soon!
    if (options->hasExpectedInputTypes()) {
      resolver->RejectWithTypeError("expectedInputTypes not supported");
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
        // The API impl only accepts a prompt dict for now, more to come soon!
        if (!first_prompt->IsAILanguageModelPromptDict()) {
          resolver->RejectWithTypeError("Input type not supported");
          return promise;
        }
        auto* first_prompt_dict =
            first_prompt->GetAsAILanguageModelPromptDict();
        if (first_prompt_dict->role() ==
            V8AILanguageModelPromptRole::Enum::kSystem) {
          if (options->hasSystemPrompt()) {
            // If the system prompt cannot be provided both from system prompt
            // and initial prompts, so reject with a `TypeError`.
            resolver->RejectWithTypeError(
                kExceptionMessageSystemPromptIsDefinedMultipleTimes);
            return promise;
          }
          // The API impl only accepts a string for now, more to come soon!
          if (!first_prompt_dict->content()->IsString()) {
            resolver->RejectWithTypeError("Input type not supported");
            return promise;
          }
          system_prompt = first_prompt_dict->content()->GetAsString();
          start_index++;
        }
        for (size_t index = start_index; index < prompts.size(); ++index) {
          auto prompt = prompts[index];
          // The API impl only accepts a prompt dict for now, more to come soon!
          if (!prompt->IsAILanguageModelPromptDict()) {
            resolver->RejectWithTypeError("Input type not supported");
            return promise;
          }
          auto* dict = prompt->GetAsAILanguageModelPromptDict();
          if (dict->role() == V8AILanguageModelPromptRole::Enum::kSystem) {
            // If any prompt except the first one has a `system` role, reject
            // with a `TypeError`.
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
              AILanguageModel::ConvertRoleToMojo(dict->role()),
              mojom::blink::AILanguageModelPromptContent::NewText(
                  dict->content()->GetAsString())));
        }
      }
    }
  }

  if (options->hasExpectedInputLanguages()) {
    expected_input_languages = options->expectedInputLanguages();
  }

  MakeGarbageCollected<CreateLanguageModelClient>(
      script_state, ai_, resolver, signal, std::move(sampling_params),
      system_prompt, std::move(initial_prompts), monitor,
      expected_input_languages);

  return promise;
}

}  // namespace blink
