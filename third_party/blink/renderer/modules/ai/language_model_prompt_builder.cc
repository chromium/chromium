// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model_prompt_builder.h"

#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_prompt_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt_input.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_languagemodelpromptdict_string.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/language_model.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_source_util.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {
namespace {

using ResolveCallback = base::OnceCallback<void(
    WTF::Vector<mojom::blink::AILanguageModelPromptPtr>)>;
using RejectCallback = base::OnceCallback<void(const ScriptValue& error)>;

// Helper class for converting types and managing async processing.
class LanguageModelPromptBuilder
    : public GarbageCollected<LanguageModelPromptBuilder> {
 public:
  explicit LanguageModelPromptBuilder(
      ScriptState* script_state,
      AbortSignal* abort_signal,
      WTF::HashSet<mojom::blink::AILanguageModelPromptType> allowed_types,
      const V8LanguageModelPromptInput* input,
      ResolveCallback resolve_callback,
      RejectCallback reject_callback);
  void Trace(Visitor*) const;

 private:
  void Build(const V8LanguageModelPromptInput* input);
  // Called to reject the promise and return an error to the callback.
  void Reject(DOMException* value);
  void Reject(ScriptValue value);
  // Resolve the promise and invoke the callback with `processed_prompts_`.
  void Resolve();
  // Process a single dictionary entry in the prompt input array.
  void ProcessEntry(const LanguageModelPromptDict* entry);
  // Process a single content entry and convert it to a mojo struct; returns
  // nullptr on error.
  mojom::blink::AILanguageModelPromptContentPtr ProcessContent(
      const V8LanguageModelPromptType& type,
      V8LanguageModelPromptContent* content);

  // ToMojo converts various V8 types to AILanguageModelPromptContent.
  mojom::blink::AILanguageModelPromptContentPtr ToMojo(String prompt);
  mojom::blink::AILanguageModelPromptContentPtr ToMojo(
      AudioBuffer* audio_buffer);
  mojom::blink::AILanguageModelPromptContentPtr ToMojo(
      base::span<uint8_t> audio_bytes);
  mojom::blink::AILanguageModelPromptContentPtr ToMojo(Blob* blob);
  mojom::blink::AILanguageModelPromptContentPtr ToMojo(
      V8ImageBitmapSource* bitmap);

  SelfKeepAlive<LanguageModelPromptBuilder> keep_alive_{this};
  WTF::Vector<mojom::blink::AILanguageModelPromptPtr> processed_prompts_;
  Member<ScriptState> script_state_;
  Member<AbortSignal> abort_signal_;
  WTF::HashSet<mojom::blink::AILanguageModelPromptType> allowed_types_;

  ResolveCallback resolve_callback_;
  RejectCallback reject_callback_;
};

LanguageModelPromptBuilder::LanguageModelPromptBuilder(
    ScriptState* script_state,
    AbortSignal* abort_signal,
    WTF::HashSet<mojom::blink::AILanguageModelPromptType> allowed_types,
    const V8LanguageModelPromptInput* input,
    ResolveCallback resolve_callback,
    RejectCallback reject_callback)
    : script_state_(script_state),
      abort_signal_(abort_signal),
      allowed_types_(allowed_types),
      resolve_callback_(std::move(resolve_callback)),
      reject_callback_(std::move(reject_callback)) {
  Build(input);
}

void LanguageModelPromptBuilder::Reject(DOMException* value) {
  Reject(ScriptValue::From(script_state_, value));
}

void LanguageModelPromptBuilder::Reject(ScriptValue value) {
  if (resolve_callback_.is_null() || reject_callback_.is_null()) {
    return;  // Already rejected or resolved.
  }
  std::move(reject_callback_).Run(value);
  keep_alive_.Clear();
}

void LanguageModelPromptBuilder::Resolve() {
  if (resolve_callback_.is_null() || reject_callback_.is_null()) {
    return;  // Already rejected or resolved.
  }
  std::move(resolve_callback_).Run(std::move(processed_prompts_));
  keep_alive_.Clear();
}

void LanguageModelPromptBuilder::Build(
    const V8LanguageModelPromptInput* input) {
  HeapVector<Member<V8UnionLanguageModelPromptDictOrString>> sequence;
  if (input->IsLanguageModelPromptDictOrStringSequence()) {
    sequence = std::move(input->GetAsLanguageModelPromptDictOrStringSequence());
  }
  if (input->IsV8LanguageModelPrompt()) {
    auto* entry = input->GetAsV8LanguageModelPrompt();
    sequence.push_back(entry);
  }
  for (const auto& entry : sequence) {
    if (reject_callback_.is_null()) {
      return;
    }
    if (abort_signal_ && abort_signal_->aborted()) {
      Reject(abort_signal_->reason(script_state_));
      return;
    }

    if (entry->GetContentType() ==
        V8LanguageModelPrompt::ContentType::kString) {
      LanguageModelPromptDict* dict =
          MakeGarbageCollected<LanguageModelPromptDict>();
      V8LanguageModelPromptContent* content =
          MakeGarbageCollected<V8LanguageModelPromptContent>(
              entry->GetAsString());
      dict->setRole(V8LanguageModelPromptRole::Enum::kUser);
      dict->setType(V8LanguageModelPromptType::Enum::kText);
      dict->setContent(std::move(content));
      ProcessEntry(dict);
      continue;
    }
    CHECK(entry->GetContentType() ==
          V8LanguageModelPrompt::ContentType::kLanguageModelPromptDict);
    ProcessEntry(entry->GetAsLanguageModelPromptDict());
  }
  // TODO(crbug.com/414906618): This just synchronously resolves for now, but
  // will asynchronously resolve in the future.
  Resolve();
}

void LanguageModelPromptBuilder::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(abort_signal_);
}

void LanguageModelPromptBuilder::ProcessEntry(
    const LanguageModelPromptDict* entry) {
  CHECK(!resolve_callback_.is_null() && !reject_callback_.is_null());
  mojom::blink::AILanguageModelPromptContentPtr content =
      ProcessContent(entry->type(), entry->content());
  auto mojo_prompt = mojom::blink::AILanguageModelPrompt::New();
  mojo_prompt->role = LanguageModel::ConvertRoleToMojo(entry->role());
  mojo_prompt->content = std::move(content);
  processed_prompts_.push_back(std::move(mojo_prompt));
}

mojom::blink::AILanguageModelPromptContentPtr
LanguageModelPromptBuilder::ProcessContent(
    const V8LanguageModelPromptType& type,
    V8LanguageModelPromptContent* content) {
  switch (type.AsEnum()) {
    case V8LanguageModelPromptType::Enum::kText:
      return ToMojo(content->GetAsString());
    case V8LanguageModelPromptType::Enum::kImage: {
      if (!allowed_types_.Contains(
              mojom::blink::AILanguageModelPromptType::kImage)) {
        Reject(DOMException::Create(
            "Image not supported. Session is not initialized with image "
            "support.",
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return nullptr;
      }

      UseCounter::Count(ExecutionContext::From(script_state_),
                        WebFeature::kLanguageModel_Prompt_Input_Image);
      if (content->IsV8ImageBitmapSource()) {
        return ToMojo(content->GetAsV8ImageBitmapSource());
      }
      Reject(DOMException::Create(
          "Unsupported image type",
          DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
      return nullptr;
    }
    case V8LanguageModelPromptType::Enum::kAudio: {
      if (!allowed_types_.Contains(
              mojom::blink::AILanguageModelPromptType::kAudio)) {
        Reject(DOMException::Create(
            "Audio not supported. Session is not initialized with audio "
            "support.",
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return nullptr;
      }
      UseCounter::Count(ExecutionContext::From(script_state_),
                        WebFeature::kLanguageModel_Prompt_Input_Audio);
      switch (content->GetContentType()) {
        case V8LanguageModelPromptContent::ContentType::kAudioBuffer:
          return ToMojo(content->GetAsAudioBuffer());
        case V8LanguageModelPromptContent::ContentType::kBlob:
          return ToMojo(content->GetAsBlob());
        case V8LanguageModelPromptContent::ContentType::kArrayBuffer:
          return ToMojo(content->GetAsArrayBuffer()->Content()->ByteSpan());
        case V8LanguageModelPromptContent::ContentType::kArrayBufferView:
          return ToMojo(content->GetAsArrayBufferView()->ByteSpan());
        default:
          Reject(DOMException::Create(
              "Unsupported audio content type",
              DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
          return nullptr;
      }
    }
  }
}

mojom::blink::AILanguageModelPromptContentPtr
LanguageModelPromptBuilder::ToMojo(String prompt) {
  return mojom::blink::AILanguageModelPromptContent::NewText(prompt);
}

mojom::blink::AILanguageModelPromptContentPtr
LanguageModelPromptBuilder::ToMojo(AudioBuffer* audio_buffer) {
  if (audio_buffer->numberOfChannels() > 2) {
    // TODO(crbug.com/382180351): Support more than 2 channels.
    Reject(DOMException::Create(
        "Audio with more than 2 channels is not supported.",
        DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
    return nullptr;
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

mojom::blink::AILanguageModelPromptContentPtr
LanguageModelPromptBuilder::ToMojo(base::span<uint8_t> audio_bytes) {
  // TODO(crbug.com/401010825): Use the file sample rate.
  scoped_refptr<AudioBus> bus = AudioBus::CreateBusFromInMemoryAudioFile(
      audio_bytes, /*mix_to_mono=*/true, /*sample_rate=*/48000);
  if (!bus) {
    // TODO(crbug.com/409615288): This should throw a TypeError according to the
    // spec.
    Reject(DOMException::Create(
        "Missing or invalid audio data.",
        DOMException::GetErrorName(DOMExceptionCode::kDataError)));
    return nullptr;
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

mojom::blink::AILanguageModelPromptContentPtr
LanguageModelPromptBuilder::ToMojo(Blob* blob) {
  // TODO(crbug.com/382180351): Make blob reading async or alternatively
  // use FileReaderSync instead (fix linker and exception issues).
  SyncedFileReaderAccumulator* blobReader =
      MakeGarbageCollected<SyncedFileReaderAccumulator>();

  auto [error_code, reader_data] = blobReader->Load(
      blob->GetBlobDataHandle(), ExecutionContext::From(script_state_)
                                     ->GetTaskRunner(TaskType::kFileReading));
  if (error_code != FileErrorCode::kOK) {
    Reject(DOMException::Create(
        "Failed to read blob.",
        DOMException::GetErrorName(DOMExceptionCode::kDataError)));
    return nullptr;
  }
  ArrayBufferContents audio_contents =
      std::move(reader_data).AsArrayBufferContents();
  if (!audio_contents.IsValid()) {
    Reject(DOMException::Create(
        "Failed to read contents of blob.",
        DOMException::GetErrorName(DOMExceptionCode::kDataError)));
    return nullptr;
  }
  return ToMojo(audio_contents.ByteSpan());
}

mojom::blink::AILanguageModelPromptContentPtr
LanguageModelPromptBuilder::ToMojo(V8ImageBitmapSource* bitmap) {
  ExceptionState exception_state(nullptr);
  std::optional<SkBitmap> skia_bitmap =
      GetBitmapFromV8ImageBitmapSource(script_state_, bitmap, exception_state);
  if (!skia_bitmap) {
    CHECK(exception_state.HadException());
    Reject(DOMException::Create(
        "Unable to get bitmap from image content",
        DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
    return nullptr;
  }
  return mojom::blink::AILanguageModelPromptContent::NewBitmap(
      skia_bitmap.value());
}
}  // namespace

void ConvertPromptInputsToMojo(
    ScriptState* script_state,
    AbortSignal* abort_signal,
    const V8LanguageModelPromptInput* input,
    WTF::HashSet<mojom::blink::AILanguageModelPromptType> allowed_types,
    ResolveCallback resolve_callback,
    RejectCallback reject_callback) {
  MakeGarbageCollected<LanguageModelPromptBuilder>(
      script_state, abort_signal, allowed_types, input,
      std::move(resolve_callback), std::move(reject_callback));
}

}  // namespace blink
