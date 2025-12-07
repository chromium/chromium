// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"

#include <memory>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/types/to_address.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_util.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/base/waiting.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_support.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webcodecs/array_buffer_util.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_decoder_broker.h"
#include "third_party/blink/renderer/modules/webcodecs/decrypt_config_util.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

bool VerifyDescription(const AudioDecoderConfig& config,
                       String* js_error_message) {
  // https://www.w3.org/TR/webcodecs-flac-codec-registration
  // https://www.w3.org/TR/webcodecs-vorbis-codec-registration
  bool description_required = false;
  if (config.codec() == "flac" || config.codec() == "vorbis") {
    description_required = true;
  }

  if (description_required && !config.hasDescription()) {
    *js_error_message = "Invalid config; description is required.";
    return false;
  }

  // For Opus with more than 2 channels, we need a description. While we can
  // guess a channel mapping for up to 8 channels, we don't know whether the
  // encoded Opus streams will be mono or stereo streams.
  if (config.codec() == "opus" && config.numberOfChannels() > 2 &&
      !config.hasDescription()) {
    *js_error_message =
        "Invalid config; description is required for multi-channel Opus.";
    return false;
  }

  if (config.hasDescription()) {
    auto desc_wrapper = AsSpan<const uint8_t>(config.description());

    if (!desc_wrapper.data()) {
      *js_error_message = "Invalid config; description is detached.";
      return false;
    }
  }

  return true;
}

AudioDecoderConfig* CopyConfig(const AudioDecoderConfig& config) {
  AudioDecoderConfig* copy = AudioDecoderConfig::Create();
  copy->setCodec(config.codec());
  copy->setSampleRate(config.sampleRate());
  copy->setNumberOfChannels(config.numberOfChannels());
  if (config.hasDescription()) {
    auto desc_wrapper = AsSpan<const uint8_t>(config.description());
    if (!desc_wrapper.empty()) {
      DOMArrayBuffer* buffer_copy = DOMArrayBuffer::Create(desc_wrapper);
      copy->setDescription(
          MakeGarbageCollected<AllowSharedBufferSource>(buffer_copy));
    }
  }
  return copy;
}

std::optional<media::AudioCodec> TryGetPcmCodec(const String& codec) {
  String codecs_str = codec.LowerASCII();
  if (codecs_str == "ulaw") {
    return media::AudioCodec::kPCM_MULAW;
  }

  if (codecs_str == "alaw") {
    return media::AudioCodec::kPCM_ALAW;
  }

  if (codecs_str == "pcm-u8" || codecs_str == "pcm-s16" ||
      codecs_str == "pcm-s24" || codecs_str == "pcm-s32" ||
      codecs_str == "pcm-f32") {
    return media::AudioCodec::kPCM;
  }

  return std::nullopt;
}

media::SampleFormat PcmCodecToSampleFormat(const String& codec) {
  String codecs_str = codec.LowerASCII();

  if (codecs_str == "pcm-u8") {
    return media::SampleFormat::kSampleFormatU8;
  }

  if (codecs_str == "pcm-s16") {
    return media::SampleFormat::kSampleFormatS16;
  }

  if (codecs_str == "pcm-s24") {
    return media::SampleFormat::kSampleFormatS24;
  }

  if (codecs_str == "pcm-s32") {
    return media::SampleFormat::kSampleFormatS32;
  }

  if (codecs_str == "pcm-f32") {
    return media::SampleFormat::kSampleFormatF32;
  }

  return media::SampleFormat::kSampleFormatPlanarF32;
}

// static
std::unique_ptr<AudioDecoderTraits::MediaDecoderType>
AudioDecoderTraits::CreateDecoder(
    ExecutionContext& execution_context,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    media::MediaLog* media_log) {
  return std::make_unique<AudioDecoderBroker>(media_log, execution_context);
}

// static
void AudioDecoderTraits::UpdateDecoderLog(const MediaDecoderType& decoder,
                                          const MediaConfigType& media_config,
                                          media::MediaLog* media_log) {
  media_log->SetProperty<media::MediaLogProperty::kAudioDecoderName>(
      decoder.GetDecoderType());
  media_log->SetProperty<media::MediaLogProperty::kIsPlatformAudioDecoder>(
      decoder.IsPlatformDecoder());
  media_log->SetProperty<media::MediaLogProperty::kAudioTracks>(
      std::vector<MediaConfigType>{media_config});
  MEDIA_LOG(INFO, media_log)
      << "Initialized AudioDecoder: " << media_config.AsHumanReadableString();
  base::UmaHistogramEnumeration("Blink.WebCodecs.AudioDecoder.Codec",
                                media_config.codec());
}

// static
void AudioDecoderTraits::InitializeDecoder(
    MediaDecoderType& decoder,
    bool /*low_delay*/,
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
const char* AudioDecoderTraits::GetName() {
  return "AudioDecoder";
}

// static
AudioDecoder* AudioDecoder::Create(ScriptState* script_state,
                                   const AudioDecoderInit* init,
                                   ExceptionState& exception_state) {
  auto* result =
      MakeGarbageCollected<AudioDecoder>(script_state, init, exception_state);
  return exception_state.HadException() ? nullptr : result;
}

// static
ScriptPromise<AudioDecoderSupport> AudioDecoder::isConfigSupported(
    ScriptState* script_state,
    const AudioDecoderConfig* config,
    ExceptionState& exception_state) {
  String js_error_message;
  std::optional<media::AudioType> audio_type =
      IsValidAudioDecoderConfig(*config, &js_error_message);

  if (!audio_type) {
    exception_state.ThrowTypeError(js_error_message);
    return EmptyPromise();
  }

  AudioDecoderSupport* support = AudioDecoderSupport::Create();
  support->setSupported(media::IsDecoderSupportedAudioType(*audio_type));
  support->setConfig(CopyConfig(*config));
  return ToResolvedPromise<AudioDecoderSupport>(script_state, support);
}

// static
std::optional<media::AudioType> AudioDecoder::IsValidAudioDecoderConfig(
    const AudioDecoderConfig& config,
    String* js_error_message) {
  if (config.numberOfChannels() == 0) {
    *js_error_message = String::Format(
        "Invalid channel count; channel count must be non-zero, received %d.",
        config.numberOfChannels());
    return std::nullopt;
  }

  if (config.sampleRate() == 0) {
    *js_error_message = String::Format(
        "Invalid sample rate; sample rate must be non-zero, received %d.",
        config.sampleRate());
    return std::nullopt;
  }

  if (config.codec().LengthWithStrippedWhiteSpace() == 0) {
    *js_error_message = "Invalid codec; codec is required.";
    return std::nullopt;
  }
  // Match codec strings from the codec registry:
  // https://www.w3.org/TR/webcodecs-codec-registry/#audio-codec-registry
  std::optional<media::AudioCodec> pcm_type = TryGetPcmCodec(config.codec());
  if (pcm_type.has_value()) {
    return media::AudioType{.codec = *pcm_type};
  }

  if (!VerifyDescription(config, js_error_message)) {
    CHECK(!js_error_message->empty());
    return std::nullopt;
  }

  std::optional<media::AudioType> audio_type =
      media::ParseAudioCodecString("", config.codec().Utf8());
  if (!audio_type) {
    *js_error_message = "Unknown or ambiguous codec name.";
    return media::AudioType{.codec = media::AudioCodec::kUnknown};
  }

  return audio_type;
}

// static
std::optional<media::AudioDecoderConfig>
AudioDecoder::MakeMediaAudioDecoderConfig(const ConfigType& config,
                                          String* js_error_message) {
  std::optional<media::AudioType> audio_type =
      IsValidAudioDecoderConfig(config, js_error_message);
  if (!audio_type) {
    // Checked by IsValidConfig().
    NOTREACHED();
  }
  if (audio_type->codec == media::AudioCodec::kUnknown) {
    return std::nullopt;
  }

  std::vector<uint8_t> extra_data;
  if (config.hasDescription()) {
    auto desc_wrapper = AsSpan<const uint8_t>(config.description());

    if (!desc_wrapper.data()) {
      // We should never get here, since this should be caught in
      // IsValidAudioDecoderConfig().
      *js_error_message = "Invalid config; description is detached.";
      return std::nullopt;
    }

    if (!desc_wrapper.empty()) {
      extra_data.assign(base::to_address(desc_wrapper.begin()),
                        base::to_address(desc_wrapper.end()));
    }
  }

  media::ChannelLayout channel_layout =
      config.numberOfChannels() > 8
          // GuesschannelLayout() doesn't know how to guess above 8 channels.
          ? media::CHANNEL_LAYOUT_DISCRETE
          : media::GuessChannelLayout(config.numberOfChannels());

  auto encryption_scheme = media::EncryptionScheme::kUnencrypted;
  if (config.hasEncryptionScheme()) {
    auto scheme = ToMediaEncryptionScheme(config.encryptionScheme());
    if (!scheme) {
      *js_error_message = "Unsupported encryption scheme";
      return std::nullopt;
    }
    encryption_scheme = scheme.value();
  }

  // TODO(chcunningham): Add sample format to IDL.
  media::AudioDecoderConfig media_config;

  media::SampleFormat format = media::kSampleFormatPlanarF32;
  if (audio_type->codec == media::AudioCodec::kPCM) {
    // There is a case of the codec being "1", which is a valid PCM codec for
    // WAV in media/base/mime_util_internal.cc. We should reject this case for
    // webcodecs.
    if (config.codec() == "1") {
      return std::nullopt;
    }
    format = PcmCodecToSampleFormat(config.codec());
  }

  media_config.Initialize(audio_type->codec, format, channel_layout,
                          config.sampleRate(), extra_data, encryption_scheme,
                          base::TimeDelta() /* seek preroll */,
                          0 /* codec delay */);
  if (!media_config.IsValidConfig()) {
    *js_error_message = "Unsupported config.";
    return std::nullopt;
  }

  return media_config;
}

AudioDecoder::AudioDecoder(ScriptState* script_state,
                           const AudioDecoderInit* init,
                           ExceptionState& exception_state)
    : DecoderTemplate<AudioDecoderTraits>(script_state, init, exception_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

bool AudioDecoder::IsValidConfig(const ConfigType& config,
                                 String* js_error_message) {
  return IsValidAudioDecoderConfig(config, js_error_message /* out */)
      .has_value();
}

std::optional<media::AudioDecoderConfig> AudioDecoder::MakeMediaConfig(
    const ConfigType& config,
    String* js_error_message) {
  DCHECK(js_error_message);
  return MakeMediaAudioDecoderConfig(config, js_error_message /* out */);
}

media::DecoderStatus::Or<scoped_refptr<media::DecoderBuffer>>
AudioDecoder::MakeInput(const InputType& chunk, bool verify_key_frame) {
  if (verify_key_frame && !chunk.buffer()->is_key_frame())
    return media::DecoderStatus::Codes::kKeyFrameRequired;
  return chunk.buffer();
}

media::DecoderStatus::Or<AudioDecoder::OutputType*> AudioDecoder::MakeOutput(
    scoped_refptr<MediaOutputType> output,
    ExecutionContext* context) {
  if (!blink::audio_utilities::IsValidAudioBufferSampleRate(
          output->sample_rate())) {
    return media::DecoderStatus(
        media::DecoderStatus::Codes::kInvalidArgument,
        String::Format("Invalid decoded audio output sample rate. Got %u, "
                       "which is outside [%f, %f]",
                       output->sample_rate(),
                       blink::audio_utilities::MinAudioBufferSampleRate(),
                       blink::audio_utilities::MaxAudioBufferSampleRate())
            .Ascii());
  }

  if (static_cast<uint32_t>(output->channel_count()) >
      BaseAudioContext::MaxNumberOfChannels()) {
    return media::DecoderStatus(
        media::DecoderStatus::Codes::kInvalidArgument,
        String::Format("Invalid decoded audio output channel "
                       "count. Got %u, which exceeds %u",
                       output->channel_count(),
                       BaseAudioContext::MaxNumberOfChannels())
            .Ascii());
  }

  return MakeGarbageCollected<AudioDecoderTraits::OutputType>(
      std::move(output));
}

const AtomicString& AudioDecoder::InterfaceName() const {
  return event_target_names::kAudioDecoder;
}

}  // namespace blink
