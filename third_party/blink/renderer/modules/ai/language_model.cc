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
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/dom_ai.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/language_model_factory.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_source_util.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
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
  MeasureInputUsageClient(ScriptState* script_state,
                          LanguageModel* language_model,
                          ScriptPromiseResolver<IDLDouble>* resolver,
                          AbortSignal* signal,
                          const WTF::String& input)
      : AIContextObserver(script_state, language_model, resolver, signal),
        language_model_(language_model),
        receiver_(this, language_model->GetExecutionContext()) {
    mojo::PendingRemote<mojom::blink::AILanguageModelMeasureInputUsageClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   language_model->GetTaskRunner());
    language_model_->GetAILanguageModelRemote()->MeasureInputUsage(
        input, std::move(client_remote));
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

base::expected<mojom::blink::AILanguageModelPromptContentPtr, DOMException*>
ToMojo(String prompt) {
  return mojom::blink::AILanguageModelPromptContent::NewText(prompt);
}

base::expected<mojom::blink::AILanguageModelPromptContentPtr, DOMException*>
ToMojo(AudioBuffer* audio_buffer) {
  if (audio_buffer->numberOfChannels() > 2) {
    // TODO(crbug.com/382180351): Support more than 2 channels.
    return base::unexpected(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSyntaxError,
        "Audio with more than 2 channels is not supported."));
  }
  on_device_model::mojom::blink::AudioDataPtr audio_data =
      on_device_model::mojom::blink::AudioData::New();
  audio_data->sample_rate = audio_buffer->sampleRate();
  audio_data->frame_count = audio_buffer->length();
  // TODO(crbug.com/382180351): Use other mono mixing utils like
  // AudioBus::CreateByMixingToMono.
  audio_data->channel_count = 1;
  base::span<const float> channel0 = audio_buffer->getChannelData(0)->AsSpan();
  audio_data->data = WTF::Vector<float>(channel0.size());
  for (size_t i = 0; i < channel0.size(); ++i) {
    audio_data->data[i] = channel0[i];
    // If second channel exists, average the two channels to produce mono.
    if (audio_buffer->numberOfChannels() > 1) {
      audio_data->data[i] =
          (audio_data->data[i] + audio_buffer->getChannelData(1)->AsSpan()[i]) /
          2.0f;
    }
  }
  return mojom::blink::AILanguageModelPromptContent::NewAudio(
      std::move(audio_data));
}

base::expected<mojom::blink::AILanguageModelPromptContentPtr, DOMException*>
ToMojo(Blob* blob, ExecutionContext* execution_context) {
  // TODO(crbug.com/382180351): Make blob reading async or alternatively
  // use FileReaderSync instead (fix linker and exception issues).
  SyncedFileReaderAccumulator* blobReader =
      MakeGarbageCollected<SyncedFileReaderAccumulator>();

  auto [error_code, reader_data] = blobReader->Load(
      blob->GetBlobDataHandle(),
      execution_context->GetTaskRunner(TaskType::kFileReading));
  if (error_code != FileErrorCode::kOK) {
    return base::unexpected(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError, "Failed to read blob."));
  }
  ArrayBufferContents audio_contents =
      std::move(reader_data).AsArrayBufferContents();
  if (!audio_contents.IsValid()) {
    return base::unexpected(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError, "Failed to read blob."));
  }
  // TODO(crbug.com/401010825): Use the file sample rate.
  scoped_refptr<AudioBus> bus = AudioBus::CreateBusFromInMemoryAudioFile(
      audio_contents.Data(), audio_contents.DataLength(),
      /*mix_to_mono=*/true, /*sample_rate=*/48000);
  if (!bus) {
    return base::unexpected(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError,
        "Blob contains missing or invalid audio data."));
  }

  on_device_model::mojom::blink::AudioDataPtr audio_data =
      on_device_model::mojom::blink::AudioData::New();
  audio_data->sample_rate = bus->SampleRate();
  audio_data->frame_count = bus->length();
  audio_data->channel_count = bus->NumberOfChannels();
  CHECK_EQ(audio_data->channel_count, 1);
  // TODO(crbug.com/382180351): Avoid a copy.
  audio_data->data = WTF::Vector<float>(bus->length());
  std::copy_n(bus->Channel(0)->Data(), bus->Channel(0)->length(),
              audio_data->data.begin());
  return mojom::blink::AILanguageModelPromptContent::NewAudio(
      std::move(audio_data));
}

base::expected<mojom::blink::AILanguageModelPromptContentPtr, DOMException*>
ToMojo(V8ImageBitmapSource* bitmap,
       ScriptState* script_state,
       ExceptionState& exception_state) {
  std::optional<SkBitmap> skia_bitmap =
      GetBitmapFromV8ImageBitmapSource(script_state, bitmap, exception_state);
  if (!skia_bitmap) {
    return base::unexpected(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSyntaxError,
        "Unable to get bitmap from image content"));
  }
  return mojom::blink::AILanguageModelPromptContent::NewBitmap(
      skia_bitmap.value());
}

base::expected<mojom::blink::AILanguageModelPromptContentPtr, DOMException*>
ConvertPromptToMojoContent(V8LanguageModelPromptType content_type,
                           const V8LanguageModelPromptContent* content,
                           ScriptState* script_state,
                           ExceptionState& exception_state,
                           ExecutionContext* execution_context) {
  switch (content_type.AsEnum()) {
    case V8LanguageModelPromptType::Enum::kText:
      return ToMojo(content->GetAsString());
    case V8LanguageModelPromptType::Enum::kImage:
      if (content->IsV8ImageBitmapSource()) {
        return ToMojo(content->GetAsV8ImageBitmapSource(), script_state,
                      exception_state);
      }
      return base::unexpected(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSyntaxError, "Unsupported image content type"));
    case V8LanguageModelPromptType::Enum::kAudio:
      switch (content->GetContentType()) {
        case V8LanguageModelPromptContent::ContentType::kAudioBuffer:
          return ToMojo(content->GetAsAudioBuffer());
        case V8LanguageModelPromptContent::ContentType::kBlob:
          return ToMojo(content->GetAsBlob(), execution_context);
        default:
          return base::unexpected(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kSyntaxError,
              "Unsupported audio content type"));
      }
  }
}

// Return `prompt`'s content as a mojo struct or nullptr if there was an error.
base::expected<mojom::blink::AILanguageModelPromptPtr, DOMException*>
ConvertPromptToMojo(const V8LanguageModelPrompt* prompt,
                    ScriptState* script_state,
                    ExceptionState& exception_state,
                    ExecutionContext* execution_context) {
  switch (prompt->GetContentType()) {
    // Handle basic string prompt.
    case V8LanguageModelPrompt::ContentType::kString: {
      auto result = mojom::blink::AILanguageModelPrompt::New();
      ASSIGN_OR_RETURN(result->content, ToMojo(prompt->GetAsString()));
      result->role = mojom::blink::AILanguageModelPromptRole::kUser;
      return result;
    }
    // Handle dictionary for multimodal input.
    case V8LanguageModelPrompt::ContentType::kLanguageModelPromptDict:
      LanguageModelPromptDict* dict = prompt->GetAsLanguageModelPromptDict();
      auto result = mojom::blink::AILanguageModelPrompt::New();
      ASSIGN_OR_RETURN(result->content,
                       ConvertPromptToMojoContent(dict->type(), dict->content(),
                                                  script_state, exception_state,
                                                  execution_context));
      result->role = LanguageModel::ConvertRoleToMojo(dict->role());
      return result;
  }
}

// Populates the `prompts` mojo struct vector from `input`. Returns an exception
// if some input was specified incorrectly or inaccessible, nullptr otherwise.
base::expected<WTF::Vector<mojom::blink::AILanguageModelPromptPtr>,
               DOMException*>
BuildPrompts(const V8LanguageModelPromptInput* input,
             ScriptState* script_state,
             ExceptionState& exception_state,
             ExecutionContext* execution_context) {
  WTF::Vector<mojom::blink::AILanguageModelPromptPtr> prompts;
  if (input->IsLanguageModelPromptDictOrStringSequence()) {
    const auto& sequence =
        input->GetAsLanguageModelPromptDictOrStringSequence();
    for (const auto& entry : sequence) {
      ASSIGN_OR_RETURN(auto prompt,
                       ConvertPromptToMojo(entry, script_state, exception_state,
                                           execution_context));
      prompts.push_back(std::move(prompt));
    }
  } else {
    CHECK(input->IsV8LanguageModelPrompt());
    auto* entry = input->GetAsV8LanguageModelPrompt();
    ASSIGN_OR_RETURN(auto prompt,
                     ConvertPromptToMojo(entry, script_state, exception_state,
                                         execution_context));
    prompts.push_back(std::move(prompt));
  }

  return prompts;
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
ScriptPromise<V8AIAvailability> LanguageModel::availability(
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
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLString>();
  }

  ScriptPromiseResolver<IDLString>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();

  // The API impl only accepts a string by default for now, more to come soon!
  if (!input->IsString() &&
      !RuntimeEnabledFeatures::AIPromptAPIMultimodalInputEnabled()) {
    resolver->RejectWithTypeError("Input type not supported");
    return promise;
  }

  auto prompts =
      BuildPrompts(input, script_state, exception_state, GetExecutionContext());
  if (!prompts.has_value()) {
    resolver->Reject(prompts.error());
    return promise;
  }

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionPrompt);

  // TODO(crbug.com/385173789): Aggregate other input type sizes for UMA.
  if (input->IsString()) {
    base::UmaHistogramCounts1M(
        AIMetrics::GetAISessionRequestSizeMetricName(
            AIMetrics::AISessionType::kLanguageModel),
        int(input->GetAsString().CharactersSizeInBytes()));
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

  auto pending_remote = CreateModelExecutionResponder(
      script_state, signal, resolver, task_runner_,
      AIMetrics::AISessionType::kLanguageModel,
      WTF::BindOnce(&LanguageModel::OnResponseComplete,
                    WrapWeakPersistent(this)),
      WTF::BindRepeating(&LanguageModel::OnQuotaOverflow,
                         WrapWeakPersistent(this)));
  language_model_remote_->Prompt(std::move(prompts).value(),
                                 std::move(pending_remote));
  return promise;
}

ReadableStream* LanguageModel::promptStreaming(
    ScriptState* script_state,
    const V8LanguageModelPromptInput* input,
    const LanguageModelPromptOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return nullptr;
  }

  // The API impl only accepts a string by default for now, more to come soon!
  if (!input->IsString() &&
      !RuntimeEnabledFeatures::AIPromptAPIMultimodalInputEnabled()) {
    exception_state.ThrowTypeError("Input type not supported");
    return nullptr;
  }

  auto prompts =
      BuildPrompts(input, script_state, exception_state, GetExecutionContext());
  if (!prompts.has_value()) {
    auto* exception = prompts.error();
    CHECK(IsDOMExceptionCode(exception->code()));
    exception_state.ThrowDOMException(
        static_cast<DOMExceptionCode>(exception->code()), exception->message());
    return nullptr;
  }

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionPromptStreaming);

  // TODO(crbug.com/385173789): Aggregate other input type sizes for UMA.
  if (input->IsString()) {
    base::UmaHistogramCounts1M(
        AIMetrics::GetAISessionRequestSizeMetricName(
            AIMetrics::AISessionType::kLanguageModel),
        int(input->GetAsString().CharactersSizeInBytes()));
  }

  if (!language_model_remote_) {
    ThrowSessionDestroyedException(exception_state);
    return nullptr;
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return nullptr;
  }

  auto [readable_stream, pending_remote] =
      CreateModelExecutionStreamingResponder(
          script_state, signal, task_runner_,
          AIMetrics::AISessionType::kLanguageModel,
          WTF::BindOnce(&LanguageModel::OnResponseComplete,
                        WrapWeakPersistent(this)),
          WTF::BindRepeating(&LanguageModel::OnQuotaOverflow,
                             WrapWeakPersistent(this)));

  language_model_remote_->Prompt(std::move(prompts).value(),
                                 std::move(pending_remote));
  return readable_stream;
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
  if (!input->IsString()) {
    exception_state.ThrowTypeError("Input type not supported");
    return ScriptPromise<IDLDouble>();
  }

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionCountPromptTokens);

  ScriptPromiseResolver<IDLDouble>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLDouble>>(script_state);
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

  MakeGarbageCollected<MeasureInputUsageClient>(script_state, this, resolver,
                                                signal, input->GetAsString());

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
