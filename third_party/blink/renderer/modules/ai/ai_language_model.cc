// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_language_model.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_model_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_model_prompt_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_ai_language_model_prompt_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_ai_language_model_prompt_input.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_ailanguagemodelpromptdict_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_blob_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_imagedata_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/modules/ai/ai_language_model_factory.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/shapedetection/shape_detector.h"
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

class CloneLanguageModelClient
    : public GarbageCollected<CloneLanguageModelClient>,
      public mojom::blink::AIManagerCreateLanguageModelClient,
      public AIMojoClient<AILanguageModel> {
 public:
  CloneLanguageModelClient(ScriptState* script_state,
                           AILanguageModel* language_model,
                           ScriptPromiseResolver<AILanguageModel>* resolver,
                           AbortSignal* signal,
                           base::PassKey<AILanguageModel>)
      : AIMojoClient(script_state, language_model, resolver, signal),
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
    AIMojoClient::Trace(visitor);
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
    AILanguageModel* cloned_language_model =
        MakeGarbageCollected<AILanguageModel>(
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
  Member<AILanguageModel> language_model_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateLanguageModelClient,
                   CloneLanguageModelClient>
      receiver_;
};

class CountPromptTokensClient
    : public GarbageCollected<CountPromptTokensClient>,
      public mojom::blink::AILanguageModelCountPromptTokensClient,
      public AIMojoClient<IDLUnsignedLongLong> {
 public:
  CountPromptTokensClient(ScriptState* script_state,
                          AILanguageModel* language_model,
                          ScriptPromiseResolver<IDLUnsignedLongLong>* resolver,
                          AbortSignal* signal,
                          const WTF::String& input)
      : AIMojoClient(script_state, language_model, resolver, signal),
        language_model_(language_model),
        receiver_(this, language_model->GetExecutionContext()) {
    mojo::PendingRemote<mojom::blink::AILanguageModelCountPromptTokensClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   language_model->GetTaskRunner());
    language_model_->GetAILanguageModelRemote()->CountPromptTokens(
        input, std::move(client_remote));
  }
  ~CountPromptTokensClient() override = default;

  CountPromptTokensClient(const CountPromptTokensClient&) = delete;
  CountPromptTokensClient& operator=(const CountPromptTokensClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIMojoClient::Trace(visitor);
    visitor->Trace(language_model_);
    visitor->Trace(receiver_);
  }

  // mojom::blink::AILanguageModelCountPromptTokensClient implementation.
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
  Member<AILanguageModel> language_model_;
  HeapMojoReceiver<mojom::blink::AILanguageModelCountPromptTokensClient,
                   CountPromptTokensClient>
      receiver_;
};

// Return `prompt`'s content as a mojo struct or yield an error.
std::variant<mojom::blink::AILanguageModelPromptPtr, DOMException*>
ConvertPromptToMojo(const V8AILanguageModelPrompt* prompt,
                    ScriptState* script_state,
                    ExceptionState& exception_state,
                    ExecutionContext* execution_context) {
  auto result = mojom::blink::AILanguageModelPrompt::New();
  if (prompt->IsString()) {
    result->role = mojom::blink::AILanguageModelPromptRole::kUser;
    result->content = mojom::blink::AILanguageModelPromptContent::NewText(
        prompt->GetAsString());
  } else if (prompt->IsAILanguageModelPromptDict()) {
    const AILanguageModelPromptDict* dict =
        prompt->GetAsAILanguageModelPromptDict();
    result->role = AILanguageModel::ConvertRoleToMojo(dict->role());
    const V8AILanguageModelPromptContent* content = dict->content();
    if (dict->type() == V8AILanguageModelPromptType::Enum::kText) {
      if (!content->IsString()) {
        return MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kSyntaxError,
            "Content is not text, or subtype is not supported");
      }
      result->content = mojom::blink::AILanguageModelPromptContent::NewText(
          content->GetAsString());
    } else if (dict->type() == V8AILanguageModelPromptType::Enum::kImage) {
      if (!content->IsV8ImageBitmapSource()) {
        return MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kSyntaxError,
            "Content is not image, or subtype is not supported");
      }
      std::optional<SkBitmap> bitmap = ShapeDetector::GetBitmapFromSource(
          script_state, content->GetAsV8ImageBitmapSource(), exception_state);
      if (!bitmap) {
        return MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kSyntaxError,
            "Unable to get bitmap from image content");
      }
      result->content =
          mojom::blink::AILanguageModelPromptContent::NewBitmap(bitmap.value());
    } else if (dict->type() == V8AILanguageModelPromptType::Enum::kAudio) {
      if (dict->content()->IsBlob()) {
        // TODO(crbug.com/382180351): Make blob reading async or alternatively
        // use FileReaderSync instead (fix linker and exception issues).
        SyncedFileReaderAccumulator* blobReader =
            MakeGarbageCollected<SyncedFileReaderAccumulator>();

        auto [error_code, reader_data] = blobReader->Load(
            dict->content()->GetAsBlob()->GetBlobDataHandle(),
            execution_context->GetTaskRunner(TaskType::kFileReading));
        if (error_code != FileErrorCode::kOK) {
          return MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kDataError, "Failed to read blob.");
        }
        ArrayBufferContents audio_contents =
            std::move(reader_data).AsArrayBufferContents();
        if (!audio_contents.IsValid()) {
          return MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kDataError, "Failed to read blob.");
        }
        // TODO(crbug.com/401010825): Use the file sample rate.
        scoped_refptr<AudioBus> bus = AudioBus::CreateBusFromInMemoryAudioFile(
            audio_contents.Data(), audio_contents.DataLength(),
            /*mix_to_mono=*/true, /*sample_rate=*/48000);

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
        result->content = mojom::blink::AILanguageModelPromptContent::NewAudio(
            std::move(audio_data));
      } else if (dict->content()->IsAudioBuffer()) {
        AudioBuffer* audio_buffer = dict->content()->GetAsAudioBuffer();
        if (audio_buffer->numberOfChannels() > 2) {
          // TODO(crbug.com/382180351): Support more than 2 channels.
          return MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kSyntaxError,
              "Audio with more than 2 channels is not supported.");
        }
        on_device_model::mojom::blink::AudioDataPtr audio_data =
            on_device_model::mojom::blink::AudioData::New();
        audio_data->sample_rate = audio_buffer->sampleRate();
        audio_data->frame_count = audio_buffer->length();
        // TODO(crbug.com/382180351): Use other mono mixing utils like
        // AudioBus::CreateByMixingToMono.
        audio_data->channel_count = 1;
        base::span<const float> channel0 =
            audio_buffer->getChannelData(0)->AsSpan();
        audio_data->data = WTF::Vector<float>(channel0.size());
        for (size_t i = 0; i < channel0.size(); ++i) {
          audio_data->data[i] = channel0[i];
          // If second channel exists, average the two channels to produce mono.
          if (audio_buffer->numberOfChannels() > 1) {
            audio_data->data[i] =
                (audio_data->data[i] +
                 audio_buffer->getChannelData(1)->AsSpan()[i]) /
                2.0f;
          }
        }
        result->content = mojom::blink::AILanguageModelPromptContent::NewAudio(
            std::move(audio_data));
      } else {
        return MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kSyntaxError, "Unsupported audio type.");
      }
    }
  } else {
    return MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                              "Input type not recognized");
  }
  return result;
}

// Populates the `prompts` mojo struct vector from `input`. Returns an exception
// if some input was specified incorrectly or inaccessible, nullptr otherwise.
DOMException* BuildPrompts(
    const V8AILanguageModelPromptInput* input,
    ScriptState* script_state,
    ExceptionState& exception_state,
    ExecutionContext* execution_context,
    WTF::Vector<mojom::blink::AILanguageModelPromptPtr>& prompts) {
  if (input->IsAILanguageModelPromptDictOrStringSequence()) {
    const auto& sequence =
        input->GetAsAILanguageModelPromptDictOrStringSequence();
    for (const auto& entry : sequence) {
      auto prompt = ConvertPromptToMojo(entry, script_state, exception_state,
                                        execution_context);
      if (std::holds_alternative<DOMException*>(prompt)) {
        return std::get<DOMException*>(prompt);
      }
      prompts.push_back(
          std::move(std::get<mojom::blink::AILanguageModelPromptPtr>(prompt)));
    }
  } else {
    CHECK(input->IsV8AILanguageModelPrompt());
    auto* entry = input->GetAsV8AILanguageModelPrompt();
    auto prompt = ConvertPromptToMojo(entry, script_state, exception_state,
                                      execution_context);
    if (std::holds_alternative<DOMException*>(prompt)) {
      return std::get<DOMException*>(prompt);
    }
    prompts.push_back(
        std::move(std::get<mojom::blink::AILanguageModelPromptPtr>(prompt)));
  }

  return nullptr;
}

}  // namespace

// static
mojom::blink::AILanguageModelPromptRole AILanguageModel::ConvertRoleToMojo(
    V8AILanguageModelPromptRole role) {
  switch (role.AsEnum()) {
    case V8AILanguageModelPromptRole::Enum::kSystem:
      return mojom::blink::AILanguageModelPromptRole::kSystem;
    case V8AILanguageModelPromptRole::Enum::kUser:
      return mojom::blink::AILanguageModelPromptRole::kUser;
    case V8AILanguageModelPromptRole::Enum::kAssistant:
      return mojom::blink::AILanguageModelPromptRole::kAssistant;
  }
  NOTREACHED();
}

AILanguageModel::AILanguageModel(
    ExecutionContext* execution_context,
    mojo::PendingRemote<mojom::blink::AILanguageModel> pending_remote,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    blink::mojom::blink::AILanguageModelInstanceInfoPtr info)
    : ExecutionContextClient(execution_context),
      task_runner_(task_runner),
      language_model_remote_(execution_context) {
  language_model_remote_.Bind(std::move(pending_remote), task_runner);
  if (info) {
    max_tokens_ = info->max_tokens;
    current_tokens_ = info->current_tokens;
    top_k_ = info->sampling_params->top_k;
    temperature_ = info->sampling_params->temperature;
    if (info->expected_input_languages.has_value()) {
      expected_input_languages_ =
          ToStringLanguageCodes(info->expected_input_languages.value());
    }
  }
}

void AILanguageModel::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(language_model_remote_);
}

const AtomicString& AILanguageModel::InterfaceName() const {
  return event_target_names::kAILanguageModel;
}

ExecutionContext* AILanguageModel::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

ScriptPromise<IDLString> AILanguageModel::prompt(
    ScriptState* script_state,
    const V8AILanguageModelPromptInput* input,
    const AILanguageModelPromptOptions* options,
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

  WTF::Vector<mojom::blink::AILanguageModelPromptPtr> prompts;
  auto* exception = BuildPrompts(input, script_state, exception_state,
                                 GetExecutionContext(), prompts);
  if (exception) {
    resolver->Reject(exception);
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
      WTF::BindOnce(&AILanguageModel::OnResponseComplete,
                    WrapWeakPersistent(this)),
      WTF::BindRepeating(&AILanguageModel::OnContextOverflow,
                         WrapWeakPersistent(this)));
  language_model_remote_->Prompt(std::move(prompts), std::move(pending_remote));
  return promise;
}

ReadableStream* AILanguageModel::promptStreaming(
    ScriptState* script_state,
    const V8AILanguageModelPromptInput* input,
    const AILanguageModelPromptOptions* options,
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

  WTF::Vector<mojom::blink::AILanguageModelPromptPtr> prompts;
  auto* exception = BuildPrompts(input, script_state, exception_state,
                                 GetExecutionContext(), prompts);
  if (exception) {
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
          WTF::BindOnce(&AILanguageModel::OnResponseComplete,
                        WrapWeakPersistent(this)),
          WTF::BindRepeating(&AILanguageModel::OnContextOverflow,
                             WrapWeakPersistent(this)));

  language_model_remote_->Prompt(std::move(prompts), std::move(pending_remote));
  return readable_stream;
}

ScriptPromise<AILanguageModel> AILanguageModel::clone(
    ScriptState* script_state,
    const AILanguageModelCloneOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AILanguageModel>();
  }

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionClone);

  ScriptPromiseResolver<AILanguageModel>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AILanguageModel>>(
          script_state);
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
      script_state, this, resolver, signal, base::PassKey<AILanguageModel>());

  return promise;
}

ScriptPromise<IDLUnsignedLongLong> AILanguageModel::countPromptTokens(
    ScriptState* script_state,
    const WTF::String& input,
    const AILanguageModelPromptOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLUnsignedLongLong>();
  }

  base::UmaHistogramEnumeration(AIMetrics::GetAIAPIUsageMetricName(
                                    AIMetrics::AISessionType::kLanguageModel),
                                AIMetrics::AIAPI::kSessionCountPromptTokens);

  ScriptPromiseResolver<IDLUnsignedLongLong>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUnsignedLongLong>>(
          script_state);
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

  MakeGarbageCollected<CountPromptTokensClient>(script_state, this, resolver,
                                                signal, input);

  return promise;
}

// TODO(crbug.com/355967885): reset the remote to destroy the session.
void AILanguageModel::destroy(ScriptState* script_state,
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

void AILanguageModel::OnResponseComplete(
    mojom::blink::ModelExecutionContextInfoPtr context_info) {
  if (context_info) {
    current_tokens_ = context_info->current_tokens;
  }
}

HeapMojoRemote<mojom::blink::AILanguageModel>&
AILanguageModel::GetAILanguageModelRemote() {
  return language_model_remote_;
}

scoped_refptr<base::SequencedTaskRunner> AILanguageModel::GetTaskRunner() {
  return task_runner_;
}

uint64_t AILanguageModel::GetCurrentTokens() {
  return current_tokens_;
}

void AILanguageModel::OnContextOverflow() {
  DispatchEvent(*Event::Create(event_type_names::kContextoverflow));
}

}  // namespace blink
