// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"

#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_util.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/base/waiting.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_config.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_decoder_broker.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_config_eval.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

#include <memory>
#include <vector>

namespace blink {

// static
std::unique_ptr<AudioDecoderTraits::MediaDecoderType>
AudioDecoderTraits::CreateDecoder(ExecutionContext& execution_context,
                                  media::MediaLog* media_log) {
  return std::make_unique<AudioDecoderBroker>(media_log, execution_context);
}

// static
void AudioDecoderTraits::UpdateDecoderLog(const MediaDecoderType& decoder,
                                          const MediaConfigType& media_config,
                                          media::MediaLog* media_log) {
  media_log->SetProperty<media::MediaLogProperty::kFrameTitle>(
      std::string("AudioDecoder(WebCodecs)"));
  media_log->SetProperty<media::MediaLogProperty::kAudioDecoderName>(
      decoder.GetDisplayName());
  media_log->SetProperty<media::MediaLogProperty::kIsPlatformAudioDecoder>(
      decoder.IsPlatformDecoder());
  media_log->SetProperty<media::MediaLogProperty::kAudioTracks>(
      std::vector<MediaConfigType>{media_config});
}

// static
void AudioDecoderTraits::InitializeDecoder(
    MediaDecoderType& decoder,
    const MediaConfigType& media_config,
    MediaDecoderType::InitCB init_cb,
    MediaDecoderType::OutputCB output_cb) {
  decoder.Initialize(media_config, nullptr /* cdm_context */,
                     std::move(init_cb), output_cb, media::WaitingCB());
}

// static
int AudioDecoderTraits::GetMaxDecodeRequests(const MediaDecoderType& decoder) {
  return 1;
}

// static
AudioDecoder* AudioDecoder::Create(ScriptState* script_state,
                                   const AudioDecoderInit* init,
                                   ExceptionState& exception_state) {
  return MakeGarbageCollected<AudioDecoder>(script_state, init,
                                            exception_state);
}

AudioDecoder::AudioDecoder(ScriptState* script_state,
                           const AudioDecoderInit* init,
                           ExceptionState& exception_state)
    : DecoderTemplate<AudioDecoderTraits>(script_state, init, exception_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

CodecConfigEval AudioDecoder::MakeMediaConfig(const ConfigType& config,
                                              MediaConfigType* out_media_config,
                                              String* out_console_message) {
  media::AudioCodec codec = media::kUnknownAudioCodec;
  bool is_codec_ambiguous = true;
  bool parse_succeeded = ParseAudioCodecString("", config.codec().Utf8(),
                                               &is_codec_ambiguous, &codec);

  if (!parse_succeeded) {
    *out_console_message = "Failed to parse codec string.";
    return CodecConfigEval::kInvalid;
  }

  if (is_codec_ambiguous) {
    *out_console_message = "Codec string is ambiguous.";
    return CodecConfigEval::kInvalid;
  }

  if (!media::IsSupportedAudioType({codec})) {
    *out_console_message = "Configuration is not supported.";
    return CodecConfigEval::kUnsupported;
  }

  std::vector<uint8_t> extra_data;
  if (config.hasDescription()) {
    if (config.description().IsArrayBuffer()) {
      DOMArrayBuffer* buffer = config.description().GetAsArrayBuffer();
      uint8_t* start = static_cast<uint8_t*>(buffer->Data());
      size_t size = buffer->ByteLength();
      extra_data.assign(start, start + size);
    } else {
      DCHECK(config.description().IsArrayBufferView());
      DOMArrayBufferView* view =
          config.description().GetAsArrayBufferView().Get();
      uint8_t* start = static_cast<uint8_t*>(view->BaseAddress());
      size_t size = view->byteLength();
      extra_data.assign(start, start + size);
    }
  }

  media::ChannelLayout channel_layout =
      config.numberOfChannels() > 8
          // GuesschannelLayout() doesn't know how to guess above 8 channels.
          ? media::CHANNEL_LAYOUT_DISCRETE
          : media::GuessChannelLayout(config.numberOfChannels());

  // TODO(chcunningham): Add sample format to IDL.
  out_media_config->Initialize(
      codec, media::kSampleFormatPlanarF32, channel_layout, config.sampleRate(),
      extra_data, media::EncryptionScheme::kUnencrypted,
      base::TimeDelta() /* seek preroll */, 0 /* codec delay */);

  return CodecConfigEval::kSupported;
}

media::StatusOr<scoped_refptr<media::DecoderBuffer>>
AudioDecoder::MakeDecoderBuffer(const InputType& chunk) {
  auto decoder_buffer = media::DecoderBuffer::CopyFrom(
      static_cast<uint8_t*>(chunk.data()->Data()), chunk.data()->ByteLength());
  decoder_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(chunk.timestamp()));
  decoder_buffer->set_is_key_frame(chunk.type() == "key");
  return decoder_buffer;
}

}  // namespace blink
