// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model_prompt_builder.h"

#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_prompt_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_blob_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_imagedata_offscreencanvas_svgimageelement_videoframe.h"
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
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/language_model.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_factories.h"
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
  // Contains some metadata about a input being processed.
  struct PendingEntry : public GarbageCollected<PendingEntry> {
    PendingEntry(size_t result_index, LanguageModelPromptDict* original_dict)
        : result_index(result_index), original_dict(original_dict) {}
    void Trace(Visitor* visitor) const { visitor->Trace(original_dict); }
    // Index of processed_prompts_ to write the processed result to.
    size_t result_index;
    Member<LanguageModelPromptDict> original_dict;
  };
  void Build(const V8LanguageModelPromptInput* input);
  // Called to reject the promise and return an error to the callback.
  void Reject(DOMException* value);
  void Reject(ScriptValue value);
  // Resolve the promise and invoke the callback with `processed_prompts_`.
  void Resolve();
  // Callback when an entry is finished converting.
  void OnPromptContentProcessed(
      mojom::blink::AILanguageModelPromptContentPtr content,
      PendingEntry* entry);
  // Process an entry asynchronously. OnPromptContentProcessed() will be
  // called when finished, or Reject() on failure.
  void ProcessEntry(PendingEntry* pending_entry);

  // ToMojo converts various V8 types to AILanguageModelPromptContent and calls
  // OnPromptContentProcessed() when finished, or Reject() on failure.
  void ToMojo(String prompt, PendingEntry* entry);
  void ToMojo(AudioBuffer* audio_buffer, PendingEntry* entry);
  void ToMojo(Blob* blob, PendingEntry* entry);
  void ToMojo(V8ImageBitmapSource* bitmap, PendingEntry* entry);
  void AudioToMojo(base::span<uint8_t> bytes, PendingEntry* entry);
  void BitmapToMojo(base::span<uint8_t> bytes, PendingEntry* entry);

  SelfKeepAlive<LanguageModelPromptBuilder> keep_alive_{this};
  WTF::Vector<mojom::blink::AILanguageModelPromptPtr> processed_prompts_;
  int processed_remaining_ = 0;
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
  if (resolve_callback_.is_null() || reject_callback_.is_null() ||
      !script_state_->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state_);
  std::move(reject_callback_).Run(value);
  keep_alive_.Clear();
}

void LanguageModelPromptBuilder::Resolve() {
  if (resolve_callback_.is_null() || reject_callback_.is_null() ||
      !script_state_->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state_);
  std::move(resolve_callback_).Run(std::move(processed_prompts_));
  keep_alive_.Clear();
}

void LanguageModelPromptBuilder::OnPromptContentProcessed(
    mojom::blink::AILanguageModelPromptContentPtr content,
    PendingEntry* entry) {
  auto mojo_prompt = mojom::blink::AILanguageModelPrompt::New();
  mojo_prompt->role =
      LanguageModel::ConvertRoleToMojo(entry->original_dict->role());
  mojo_prompt->content = std::move(content);
  processed_prompts_[entry->result_index] = std::move(mojo_prompt);
  if (!reject_callback_.is_null() && --processed_remaining_ == 0) {
    Resolve();
  }
}

void LanguageModelPromptBuilder::Build(
    const V8LanguageModelPromptInput* input) {
  HeapVector<Member<V8UnionLanguageModelPromptDictOrString>> entries;
  if (input->IsLanguageModelPromptDictOrStringSequence()) {
    entries = std::move(input->GetAsLanguageModelPromptDictOrStringSequence());
  } else if (input->IsV8LanguageModelPrompt()) {
    entries.push_back(input->GetAsV8LanguageModelPrompt());
  }
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      ExecutionContext::From(script_state_)
          ->GetTaskRunner(TaskType::kInternalDefault);
  processed_remaining_ = 0;
  processed_prompts_.resize(entries.size());

  for (const auto& entry : entries) {
    LanguageModelPromptDict* dict = nullptr;
    if (entry->IsString()) {
      dict = MakeGarbageCollected<LanguageModelPromptDict>();
      V8LanguageModelPromptContent* content =
          MakeGarbageCollected<V8LanguageModelPromptContent>(
              entry->GetAsString());
      dict->setRole(V8LanguageModelPromptRole::Enum::kUser);
      dict->setType(V8LanguageModelPromptType::Enum::kText);
      dict->setContent(std::move(content));
    } else {
      CHECK(entry->IsLanguageModelPromptDict());
      dict = entry->GetAsLanguageModelPromptDict();
    }
    PendingEntry* pending_entry =
        MakeGarbageCollected<PendingEntry>(processed_remaining_++, dict);
    // TODO(crbug.com/417530133): Restore sync processing for some types (text).
    task_runner->PostTask(
        FROM_HERE,
        WTF::BindOnce(&LanguageModelPromptBuilder::ProcessEntry,
                      WrapPersistent(this), WrapPersistent(pending_entry)));
  }
}

void LanguageModelPromptBuilder::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(abort_signal_);
}

void LanguageModelPromptBuilder::ProcessEntry(PendingEntry* pending_entry) {
  if (!script_state_->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state_);
  V8LanguageModelPromptContent* content =
      pending_entry->original_dict->content();
  switch (pending_entry->original_dict->type().AsEnum()) {
    case V8LanguageModelPromptType::Enum::kText:
      if (content->IsString()) {
        ToMojo(content->GetAsString(), pending_entry);
        return;
      }
      Reject(DOMException::Create(
          "Unsupported string type.",
          DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
      return;
    case V8LanguageModelPromptType::Enum::kImage: {
      if (!allowed_types_.Contains(
              mojom::blink::AILanguageModelPromptType::kImage)) {
        Reject(DOMException::Create(
            "Image not supported. Session is not initialized with image "
            "support.",
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return;
      }

      UseCounter::Count(ExecutionContext::From(script_state_),
                        WebFeature::kLanguageModel_Prompt_Input_Image);
      if (content->IsV8ImageBitmapSource()) {
        ToMojo(content->GetAsV8ImageBitmapSource(), pending_entry);
        return;
      }
      if (content->IsArrayBuffer()) {
        BitmapToMojo(content->GetAsArrayBuffer()->Content()->ByteSpan(),
                     pending_entry);
        return;
      }
      if (content->IsArrayBufferView()) {
        BitmapToMojo(content->GetAsArrayBufferView()->ByteSpan(),
                     pending_entry);
        return;
      }
      Reject(DOMException::Create(
          "Unsupported image type",
          DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
      return;
    }
    case V8LanguageModelPromptType::Enum::kAudio: {
      if (!allowed_types_.Contains(
              mojom::blink::AILanguageModelPromptType::kAudio)) {
        Reject(DOMException::Create(
            "Audio not supported. Session is not initialized with audio "
            "support.",
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return;
      }
      UseCounter::Count(ExecutionContext::From(script_state_),
                        WebFeature::kLanguageModel_Prompt_Input_Audio);
      switch (content->GetContentType()) {
        case V8LanguageModelPromptContent::ContentType::kAudioBuffer:
          ToMojo(content->GetAsAudioBuffer(), pending_entry);
          return;
        case V8LanguageModelPromptContent::ContentType::kBlob:
          ToMojo(content->GetAsBlob(), pending_entry);
          return;
        case V8LanguageModelPromptContent::ContentType::kArrayBuffer:
          AudioToMojo(content->GetAsArrayBuffer()->Content()->ByteSpan(),
                      pending_entry);
          return;
        case V8LanguageModelPromptContent::ContentType::kArrayBufferView:
          AudioToMojo(content->GetAsArrayBufferView()->ByteSpan(),
                      pending_entry);
          return;
        default:
          Reject(DOMException::Create(
              "Unsupported audio content type",
              DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
          return;
      }
    }
  }
}

void LanguageModelPromptBuilder::ToMojo(String prompt, PendingEntry* entry) {
  OnPromptContentProcessed(
      mojom::blink::AILanguageModelPromptContent::NewText(prompt), entry);
}

void LanguageModelPromptBuilder::ToMojo(AudioBuffer* audio_buffer,
                                        PendingEntry* entry) {
  if (audio_buffer->numberOfChannels() > 2) {
    // TODO(crbug.com/382180351): Support more than 2 channels.
    Reject(DOMException::Create(
        "Audio with more than 2 channels is not supported.",
        DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
    return;
  }
  auto audio_data = on_device_model::mojom::blink::AudioData::New();
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
  OnPromptContentProcessed(mojom::blink::AILanguageModelPromptContent::NewAudio(
                               std::move(audio_data)),
                           entry);
}

void LanguageModelPromptBuilder::AudioToMojo(base::span<uint8_t> bytes,
                                             PendingEntry* entry) {
  // TODO(crbug.com/401010825): Use the file sample rate.
  scoped_refptr<AudioBus> bus = AudioBus::CreateBusFromInMemoryAudioFile(
      bytes, /*mix_to_mono=*/true, /*sample_rate=*/48000);
  if (!bus) {
    // TODO(crbug.com/409615288): This should throw a TypeError according to the
    // spec.
    Reject(DOMException::Create(
        "Missing or invalid audio data.",
        DOMException::GetErrorName(DOMExceptionCode::kDataError)));
    return;
  }

  auto audio_data = on_device_model::mojom::blink::AudioData::New();
  audio_data->sample_rate = bus->SampleRate();
  audio_data->frame_count = bus->length();
  audio_data->channel_count = bus->NumberOfChannels();
  CHECK_EQ(audio_data->channel_count, 1);
  // TODO(crbug.com/382180351): Avoid a copy.
  audio_data->data = WTF::Vector<float>(bus->length());
  std::copy_n(bus->Channel(0)->Data(), bus->Channel(0)->length(),
              audio_data->data.begin());
  OnPromptContentProcessed(mojom::blink::AILanguageModelPromptContent::NewAudio(
                               std::move(audio_data)),
                           entry);
}

void LanguageModelPromptBuilder::BitmapToMojo(base::span<uint8_t> bytes,
                                              PendingEntry* entry) {
  scoped_refptr<SharedBuffer> buffer = SharedBuffer::Create(bytes);
  if (!ImageDecoder::HasSufficientDataToSniffMimeType(*buffer.get())) {
    Reject(DOMException::Create(
        "Image bytes does not contain a recognized image format.",
        DOMException::GetErrorName(DOMExceptionCode::kDataError)));
    return;
  }

  // TODO(crbug.com/416797732): Using a blob is likely inefficient here. Avoid
  // sending to the browser and back.
  ToMojo(MakeGarbageCollected<V8ImageBitmapSource>(
             Blob::Create(bytes, ImageDecoder::SniffMimeType(buffer))),
         entry);
}

void LanguageModelPromptBuilder::ToMojo(Blob* blob, PendingEntry* entry) {
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
    return;
  }
  ArrayBufferContents audio_contents =
      std::move(reader_data).AsArrayBufferContents();
  if (!audio_contents.IsValid()) {
    Reject(DOMException::Create(
        "Failed to read contents of blob.",
        DOMException::GetErrorName(DOMExceptionCode::kDataError)));
    return;
  }
  AudioToMojo(audio_contents.ByteSpan(), entry);
}

void LanguageModelPromptBuilder::ToMojo(V8ImageBitmapSource* bitmap,
                                        PendingEntry* entry) {
  class Resolve : public ThenCallable<ImageBitmap, Resolve> {
   public:
    explicit Resolve(LanguageModelPromptBuilder* builder, PendingEntry* entry)
        : builder_(builder), entry_(entry) {}
    void Trace(Visitor* visitor) const override {
      visitor->Trace(builder_);
      visitor->Trace(entry_);
      ThenCallable<ImageBitmap, Resolve>::Trace(visitor);
    }
    void React(ScriptState* script_state, ImageBitmap* value) {
      v8::Isolate* isolate = script_state->GetIsolate();
      v8::TryCatch try_catch(isolate);
      ExceptionState exception_state(isolate);
      if(!value) {
        builder_->Reject(DOMException::Create(
          "Invalid image bitmap.",
          DOMException::GetErrorName(DOMExceptionCode::kDataError)));
        return;
      }
      std::optional<SkBitmap> skia_bitmap =
          GetBitmapFromCanvasImageSource(*value, exception_state);
      if (!skia_bitmap) {
        CHECK(exception_state.HadException() && try_catch.HasCaught());
        builder_->Reject(ScriptValue(isolate, try_catch.Exception()));
        return;
      }
      builder_->OnPromptContentProcessed(
          mojom::blink::AILanguageModelPromptContent::NewBitmap(
              skia_bitmap.value()),
          entry_);
    }

   private:
    Member<LanguageModelPromptBuilder> builder_;
    Member<PendingEntry> entry_;
  };
  class Reject : public ThenCallable<IDLAny, Reject> {
   public:
    explicit Reject(LanguageModelPromptBuilder* builder) : builder_(builder) {}
    void Trace(Visitor* visitor) const override {
      visitor->Trace(builder_);
      ThenCallable<IDLAny, Reject>::Trace(visitor);
    }
    void React(ScriptState* script_state, ScriptValue value) {
      builder_->Reject(value);
    }
    Member<LanguageModelPromptBuilder> builder_;
  };
  v8::Isolate* isolate = script_state_->GetIsolate();
      v8::TryCatch try_catch(isolate);
      ExceptionState exception_state(isolate);
  // Note: GetBitmapFromV8ImageBitmapSource doesn't support async which is
  // required for blobs so async ImageBitmapFactories::CreateImageBitmap is
  // preferred.
  ImageBitmapFactories::CreateImageBitmap(
      script_state_, bitmap, MakeGarbageCollected<ImageBitmapOptions>(),
      exception_state)
      .Then(script_state_, MakeGarbageCollected<Resolve>(this, entry),
            MakeGarbageCollected<Reject>(this));

  if (exception_state.HadException()) {
    CHECK(try_catch.HasCaught());
    this->Reject(ScriptValue(isolate, try_catch.Exception()));
  }
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
