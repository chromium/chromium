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

mojom::blink::AILanguageModelPromptContentPtr ToMojo(String prompt) {
  return mojom::blink::AILanguageModelPromptContent::NewText(prompt);
}

mojom::blink::AILanguageModelPromptContentPtr ToMojo(
    AudioBuffer* audio_buffer,
    ExceptionState& exception_state) {
  if (audio_buffer->numberOfChannels() > 2) {
    // TODO(crbug.com/382180351): Support more than 2 channels.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Audio with more than 2 channels is not supported.");
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

mojom::blink::AILanguageModelPromptContentPtr ToMojo(
    base::span<uint8_t> audio_bytes,
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  // TODO(crbug.com/401010825): Use the file sample rate.
  scoped_refptr<AudioBus> bus = AudioBus::CreateBusFromInMemoryAudioFile(
      audio_bytes, /*mix_to_mono=*/true, /*sample_rate=*/48000);
  if (!bus) {
    // TODO(crbug.com/409615288): This should throw a TypeError according to the
    // spec.
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "Missing or invalid audio data.");
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

mojom::blink::AILanguageModelPromptContentPtr ToMojo(
    Blob* blob,
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  // TODO(crbug.com/382180351): Make blob reading async or alternatively
  // use FileReaderSync instead (fix linker and exception issues).
  SyncedFileReaderAccumulator* blobReader =
      MakeGarbageCollected<SyncedFileReaderAccumulator>();

  auto [error_code, reader_data] = blobReader->Load(
      blob->GetBlobDataHandle(),
      execution_context->GetTaskRunner(TaskType::kFileReading));
  if (error_code != FileErrorCode::kOK) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "Failed to read blob.");
    return nullptr;
  }
  ArrayBufferContents audio_contents =
      std::move(reader_data).AsArrayBufferContents();
  if (!audio_contents.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "Failed to read blob.");
    return nullptr;
  }
  return ToMojo(audio_contents.ByteSpan(), execution_context, exception_state);
}

mojom::blink::AILanguageModelPromptContentPtr ToMojo(
    V8ImageBitmapSource* bitmap,
    ScriptState* script_state,
    ExceptionState& exception_state) {
  std::optional<SkBitmap> skia_bitmap =
      GetBitmapFromV8ImageBitmapSource(script_state, bitmap, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return mojom::blink::AILanguageModelPromptContent::NewBitmap(
      skia_bitmap.value());
}

mojom::blink::AILanguageModelPromptContentPtr ConvertPromptToMojoContent(
    V8LanguageModelPromptType content_type,
    const V8LanguageModelPromptContent* content,
    ScriptState* script_state,
    ExceptionState& exception_state,
    ExecutionContext* execution_context,
    WTF::HashSet<mojom::blink::AILanguageModelPromptType>& allowed_types) {
  switch (content_type.AsEnum()) {
    case V8LanguageModelPromptType::Enum::kText:
      return ToMojo(content->GetAsString());
    case V8LanguageModelPromptType::Enum::kImage:
      if (!allowed_types.Contains(
              mojom::blink::AILanguageModelPromptType::kImage)) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kNotSupportedError,
            "Image not supported. Session is not initialized with image "
            "support.");
        return nullptr;
      }
      UseCounter::Count(execution_context,
                        WebFeature::kLanguageModel_Prompt_Input_Image);
      if (content->IsV8ImageBitmapSource()) {
        return ToMojo(content->GetAsV8ImageBitmapSource(), script_state,
                      exception_state);
      }
      exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                        "Unsupported image content type");
      return nullptr;
    case V8LanguageModelPromptType::Enum::kAudio:
      if (!allowed_types.Contains(
              mojom::blink::AILanguageModelPromptType::kAudio)) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kNotSupportedError,
            "Audio not supported. Session is not initialized with audio "
            "support.");
        return nullptr;
      }
      UseCounter::Count(execution_context,
                        WebFeature::kLanguageModel_Prompt_Input_Audio);
      switch (content->GetContentType()) {
        case V8LanguageModelPromptContent::ContentType::kAudioBuffer:
          return ToMojo(content->GetAsAudioBuffer(), exception_state);
        case V8LanguageModelPromptContent::ContentType::kBlob:
          return ToMojo(content->GetAsBlob(), execution_context,
                        exception_state);
        case V8LanguageModelPromptContent::ContentType::kArrayBuffer:
          return ToMojo(content->GetAsArrayBuffer()->Content()->ByteSpan(),
                        execution_context, exception_state);
        case V8LanguageModelPromptContent::ContentType::kArrayBufferView:
          return ToMojo(content->GetAsArrayBufferView()->ByteSpan(),
                        execution_context, exception_state);
        default:
          exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                            "Unsupported audio content type");
          return nullptr;
      }
  }
}

// Return `prompt`'s content as a mojo struct or nullptr if there was an error.
mojom::blink::AILanguageModelPromptPtr ConvertPromptToMojo(
    const V8LanguageModelPrompt* prompt,
    ScriptState* script_state,
    ExceptionState& exception_state,
    ExecutionContext* execution_context,
    WTF::HashSet<mojom::blink::AILanguageModelPromptType>& allowed_types) {
  switch (prompt->GetContentType()) {
    // Handle basic string prompt.
    case V8LanguageModelPrompt::ContentType::kString: {
      auto result = mojom::blink::AILanguageModelPrompt::New();
      result->content = ToMojo(prompt->GetAsString());
      if (result->content.is_null()) {
        return nullptr;
      }
      result->role = mojom::blink::AILanguageModelPromptRole::kUser;
      return result;
    }
    // Handle dictionary for multimodal input.
    case V8LanguageModelPrompt::ContentType::kLanguageModelPromptDict:
      LanguageModelPromptDict* dict = prompt->GetAsLanguageModelPromptDict();
      auto result = mojom::blink::AILanguageModelPrompt::New();
      result->content = ConvertPromptToMojoContent(
          dict->type(), dict->content(), script_state, exception_state,
          execution_context, allowed_types);
      if (result->content.is_null()) {
        return nullptr;
      }
      result->role = LanguageModel::ConvertRoleToMojo(dict->role());
      return result;
  }
}

}  // namespace

// Populates the `prompts` mojo struct vector from `input`. Returns an exception
// if some input was specified incorrectly or inaccessible, nullptr otherwise.
std::optional<WTF::Vector<mojom::blink::AILanguageModelPromptPtr>> BuildPrompts(
    const V8LanguageModelPromptInput* input,
    ScriptState* script_state,
    ExceptionState& exception_state,
    ExecutionContext* execution_context,
    WTF::HashSet<mojom::blink::AILanguageModelPromptType>& allowed_types) {
  WTF::Vector<mojom::blink::AILanguageModelPromptPtr> prompts;
  if (input->IsLanguageModelPromptDictOrStringSequence()) {
    const auto& sequence =
        input->GetAsLanguageModelPromptDictOrStringSequence();
    for (const auto& entry : sequence) {
      mojom::blink::AILanguageModelPromptPtr prompt =
          ConvertPromptToMojo(entry, script_state, exception_state,
                              execution_context, allowed_types);
      if (prompt.is_null()) {
        return std::nullopt;
      }
      prompts.push_back(std::move(prompt));
    }
  } else {
    CHECK(input->IsV8LanguageModelPrompt());
    auto* entry = input->GetAsV8LanguageModelPrompt();
    mojom::blink::AILanguageModelPromptPtr prompt = ConvertPromptToMojo(
        entry, script_state, exception_state, execution_context, allowed_types);
    if (prompt.is_null()) {
      return std::nullopt;
    }
    prompts.push_back(std::move(prompt));
  }

  return prompts;
}

}  // namespace blink
