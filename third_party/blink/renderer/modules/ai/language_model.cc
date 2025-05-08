// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected_macros.h"
#include "base/types/pass_key.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_prompt_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt_input.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_languagemodelpromptdict_string.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/dom_ai.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/language_model_factory.h"
#include "third_party/blink/renderer/modules/ai/language_model_prompt_builder.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_source_util.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_blink_string.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

using AILanguageModelPromptContentOrError =
    std::variant<mojom::blink::AILanguageModelPromptContentPtr, DOMException*>;

class CloneLanguageModelClient
    : public GarbageCollected<CloneLanguageModelClient>,
      public mojom::blink::AIManagerCreateLanguageModelClient,
      public AIContextObserver<LanguageModel> {
 public:
  CloneLanguageModelClient(ScriptState* script_state,
                           LanguageModel* language_model,
                           ScriptPromiseResolver<LanguageModel>* resolver,
                           AbortSignal* signal,
                           base::PassKey<LanguageModel>)
      : AIContextObserver(script_state, language_model, resolver, signal),
        language_model_(language_model),
        receiver_(this, language_model->GetExecutionContext()) {
    mojo::PendingRemote<mojom::blink::AIManagerCreateLanguageModelClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   language_model->GetTaskRunner());
    language_model_->GetAILanguageModelRemote()->Fork(std::move(client_remote));
  }
  ~CloneLanguageModelClient() override = default;

  CloneLanguageModelClient(const CloneLanguageModelClient&) = delete;
  CloneLanguageModelClient& operator=(const CloneLanguageModelClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIContextObserver::Trace(visitor);
    visitor->Trace(language_model_);
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
    LanguageModel* cloned_language_model = MakeGarbageCollected<LanguageModel>(
        language_model_->GetExecutionContext(),
        std::move(language_model_remote), language_model_->GetTaskRunner(),
        std::move(info));
    GetResolver()->Resolve(cloned_language_model);
    Cleanup();
  }

  void OnError(mojom::blink::AIManagerCreateClientError error) override {
    if (!GetResolver()) {
      return;
    }

    GetResolver()->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        kExceptionMessageUnableToCloneSession);
    Cleanup();
  }

  void ResetReceiver() override { receiver_.reset(); }

 private:
  Member<LanguageModel> language_model_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateLanguageModelClient,
                   CloneLanguageModelClient>
      receiver_;
};

class MeasureInputUsageClient
    : public GarbageCollected<MeasureInputUsageClient>,
      public mojom::blink::AILanguageModelMeasureInputUsageClient,
      public AIContextObserver<IDLDouble> {
 public:
  MeasureInputUsageClient(
      ScriptState* script_state,
      LanguageModel* language_model,
      ScriptPromiseResolver<IDLDouble>* resolver,
      AbortSignal* signal,
      WTF::Vector<mojom::blink::AILanguageModelPromptPtr> input)
      : AIContextObserver(script_state, language_model, resolver, signal),
        language_model_(language_model),
        receiver_(this, language_model->GetExecutionContext()) {
    mojo::PendingRemote<mojom::blink::AILanguageModelMeasureInputUsageClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   language_model->GetTaskRunner());
    language_model_->GetAILanguageModelRemote()->MeasureInputUsage(
        std::move(input), std::move(client_remote));
  }
  ~MeasureInputUsageClient() override = default;

  MeasureInputUsageClient(const MeasureInputUsageClient&) = delete;
  MeasureInputUsageClient& operator=(const MeasureInputUsageClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIContextObserver::Trace(visitor);
    visitor->Trace(language_model_);
    visitor->Trace(receiver_);
  }

  // mojom::blink::AILanguageModelMeasureInputUsageClient implementation.
  void OnResult(uint32_t number_of_tokens) override {
    if (!GetResolver()) {
      return;
    }

    GetResolver()->Resolve(number_of_tokens);
    Cleanup();
  }

 protected:
  void ResetReceiver() override { receiver_.reset(); }

 private:
  Member<LanguageModel> language_model_;
  HeapMojoReceiver<mojom::blink::AILanguageModelMeasureInputUsageClient,
                   MeasureInputUsageClient>
      receiver_;
};

// Parses `constraint` from `options` if available. On success or if no
// constraint is present returns true, returns false and throws an exception on
// failure.
bool ParseConstraint(
    ScriptState* script_state,
    const LanguageModelPromptOptions* options,
    ExceptionState& exception_state,
    on_device_model::mojom::blink::ResponseConstraintPtr& constraint) {
  if (!options->hasResponseConstraint()) {
    return true;
  }

  if (!RuntimeEnabledFeatures::AIPromptAPIStructuredOutputEnabled(
          ExecutionContext::From(script_state))) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "responseConstraint is not supported");
    return false;
  }

  v8::Local<v8::Object> value = options->responseConstraint().V8Object();
  if (value->IsRegExp()) {
    constraint = on_device_model::mojom::blink::ResponseConstraint::NewRegex(
        ToBlinkString<String>(script_state->GetIsolate(),
                              value.As<v8::RegExp>()->GetSource(),
                              ExternalMode::kDoNotExternalize));
  } else if (value->IsObject()) {
    String stringified_schema = ValidateAndStringifyObject(
        options->responseConstraint(), script_state, exception_state);
    if (!stringified_schema) {
      // ValidateAndStringifyObject throws an exception when it returns
      // a null string.
      return false;
    }
    constraint =
        on_device_model::mojom::blink::ResponseConstraint::NewJsonSchema(
            stringified_schema);
  } else {
    exception_state.ThrowTypeError("Constraint type is not supported");
    return false;
  }
  return true;
}

}  // namespace

// static
mojom::blink::AILanguageModelPromptRole LanguageModel::ConvertRoleToMojo(
    V8LanguageModelPromptRole role) {
  switch (role.AsEnum()) {
    case V8LanguageModelPromptRole::Enum::kSystem:
      return mojom::blink::AILanguageModelPromptRole::kSystem;
    case V8LanguageModelPromptRole::Enum::kUser:
      return mojom::blink::AILanguageModelPromptRole::kUser;
    case V8LanguageModelPromptRole::Enum::kAssistant:
      return mojom::blink::AILanguageModelPromptRole::kAssistant;
  }
  NOTREACHED();
}

LanguageModel::LanguageModel(
    ExecutionContext* execution_context,
    mojo::PendingRemote<mojom::blink::AILanguageModel> pending_remote,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    blink::mojom::blink::AILanguageModelInstanceInfoPtr info)
    : ExecutionContextClient(execution_context),
      task_runner_(task_runner),
      language_model_remote_(execution_context) {
  language_model_remote_.Bind(std::move(pending_remote), task_runner);
  if (info) {
    input_quota_ = info->input_quota;
    input_usage_ = info->input_usage;
    top_k_ = info->sampling_params->top_k;
    temperature_ = info->sampling_params->temperature;
    if (info->input_types.has_value()) {
      for (const auto& input_type : *(info->input_types)) {
        input_types_.insert(input_type);
      }
    }
  }
}

void LanguageModel::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(language_model_remote_);
}

const AtomicString& LanguageModel::InterfaceName() const {
  return event_target_names::kAILanguageModel;
}

ExecutionContext* LanguageModel::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

// static
ScriptPromise<LanguageModel> LanguageModel::create(
    ScriptState* script_state,
    const LanguageModelCreateOptions* options,
    ExceptionState& exception_state) {
  return DOMAI::ai(*ExecutionContext::From(script_state))
      ->languageModel()
      ->create(script_state, options, exception_state);
}

// static
ScriptPromise<V8Availability> LanguageModel::availability(
    ScriptState* script_state,
    const LanguageModelCreateCoreOptions* options,
    ExceptionState& exception_state) {
  return DOMAI::ai(*ExecutionContext::From(script_state))
      ->languageModel()
      ->availability(script_state, options, exception_state);
}

// static
ScriptPromise<IDLNullable<LanguageModelParams>> LanguageModel::params(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return DOMAI::ai(*ExecutionContext::From(script_state))
      ->languageModel()
      ->params(script_state, exception_state);
}

ScriptPromise<IDLString> LanguageModel::prompt(
    ScriptState* script_state,
    const V8LanguageModelPromptInput* input,
    const LanguageModelPromptOptions* options,
    ExceptionState& exception_state) {
  std::optional<ValidateAndProcessPromptInputResult> processed_input =
      ValidateAndProcessPromptInput(script_state, input, options,
                                    exception_state);
  if (!processed_input.has_value()) {
    return EmptyPromise();
  }
  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionPrompt);

  ScriptPromiseResolver<IDLString>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();

  auto pending_remote = CreateModelExecutionResponder(
      script_state, options->getSignalOr(nullptr), resolver, task_runner_,
      AIMetrics::AISessionType::kLanguageModel,
      WTF::BindOnce(&LanguageModel::OnResponseComplete,
                    WrapWeakPersistent(this)),
      WTF::BindRepeating(&LanguageModel::OnQuotaOverflow,
                         WrapWeakPersistent(this)));

  language_model_remote_->Prompt(
      std::move(processed_input->processed_prompts),
      std::move(processed_input->processed_constraint),
      std::move(pending_remote));
  return promise;
}

ReadableStream* LanguageModel::promptStreaming(
    ScriptState* script_state,
    const V8LanguageModelPromptInput* input,
    const LanguageModelPromptOptions* options,
    ExceptionState& exception_state) {
  std::optional<ValidateAndProcessPromptInputResult> processed_input =
      ValidateAndProcessPromptInput(script_state, input, options,
                                    exception_state);
  if (!processed_input.has_value()) {
    return nullptr;
  }
  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionPromptStreaming);

  auto [stream, remote] = CreateModelExecutionStreamingResponder(
      script_state, options->getSignalOr(nullptr), task_runner_,
      AIMetrics::AISessionType::kLanguageModel,
      WTF::BindOnce(&LanguageModel::OnResponseComplete,
                    WrapWeakPersistent(this)),
      WTF::BindRepeating(&LanguageModel::OnQuotaOverflow,
                         WrapWeakPersistent(this)));

  language_model_remote_->Prompt(
      std::move(processed_input->processed_prompts),
      std::move(processed_input->processed_constraint), std::move(remote));

  return stream;
}

std::optional<LanguageModel::ValidateAndProcessPromptInputResult>
LanguageModel::ValidateAndProcessPromptInput(
    ScriptState* script_state,
    const V8LanguageModelPromptInput* input,
    const LanguageModelPromptOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return std::nullopt;
  }

  if (!input->IsString() &&
      !RuntimeEnabledFeatures::AIPromptAPIMultimodalInputEnabled()) {
    exception_state.ThrowTypeError("Input type not supported");
    return std::nullopt;
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return std::nullopt;
  }

  on_device_model::mojom::blink::ResponseConstraintPtr constraint;
  if (!ParseConstraint(script_state, options, exception_state, constraint)) {
    // ParseConstraint will throw an exception when false is returned.
    return std::nullopt;
  }

  auto prompts = BuildPrompts(input, script_state, exception_state,
                              GetExecutionContext(), input_types_);
  if (!prompts.has_value()) {
    return std::nullopt;
  }

  // TODO(crbug.com/411470034): Aggregate other input type sizes for UMA.
  if (input->IsString()) {
    base::UmaHistogramCounts1M(
        AIMetrics::GetAISessionRequestSizeMetricName(
            AIMetrics::AISessionType::kLanguageModel),
        static_cast<int>(input->GetAsString().CharactersSizeInBytes()));
  }

  if (!language_model_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return std::nullopt;
  }

  return ValidateAndProcessPromptInputResult{
      .processed_constraint = std::move(constraint),
      .processed_prompts = std::move(prompts).value(),
  };
}

ScriptPromise<LanguageModel> LanguageModel::clone(
    ScriptState* script_state,
    const LanguageModelCloneOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<LanguageModel>();
  }

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionClone);

  ScriptPromiseResolver<LanguageModel>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<LanguageModel>>(script_state);
  auto promise = resolver->Promise();

  if (!language_model_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return promise;
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return promise;
  }

  MakeGarbageCollected<CloneLanguageModelClient>(
      script_state, this, resolver, signal, base::PassKey<LanguageModel>());

  return promise;
}

ScriptPromise<IDLDouble> LanguageModel::measureInputUsage(
    ScriptState* script_state,
    const V8LanguageModelPromptInput* input,
    const LanguageModelPromptOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLDouble>();
  }

  // The API impl only accepts a string by default for now, more to come soon!
  if (!input->IsString() &&
      !RuntimeEnabledFeatures::AIPromptAPIMultimodalInputEnabled()) {
    exception_state.ThrowTypeError("Input type not supported");
    return ScriptPromise<IDLDouble>();
  }

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionCountPromptTokens);

  ScriptPromiseResolver<IDLDouble>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLDouble>>(script_state);
  auto promise = resolver->Promise();

  auto prompts = BuildPrompts(input, script_state, exception_state,
                              GetExecutionContext(), input_types_);
  if (!prompts.has_value()) {
    return promise;
  }

  if (!language_model_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return promise;
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return promise;
  }

  MakeGarbageCollected<MeasureInputUsageClient>(
      script_state, this, resolver, signal, std::move(prompts).value());

  return promise;
}

// TODO(crbug.com/355967885): reset the remote to destroy the session.
void LanguageModel::destroy(ScriptState* script_state,
                            ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return;
  }

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionDestroy);

  if (language_model_remote_) {
    language_model_remote_->Destroy();
    language_model_remote_.reset();
  }
}

void LanguageModel::OnResponseComplete(
    mojom::blink::ModelExecutionContextInfoPtr context_info) {
  if (context_info) {
    input_usage_ = context_info->current_tokens;
  }
}

HeapMojoRemote<mojom::blink::AILanguageModel>&
LanguageModel::GetAILanguageModelRemote() {
  return language_model_remote_;
}

scoped_refptr<base::SequencedTaskRunner> LanguageModel::GetTaskRunner() {
  return task_runner_;
}

void LanguageModel::OnQuotaOverflow() {
  DispatchEvent(*Event::Create(event_type_names::kQuotaoverflow));
}

}  // namespace blink
