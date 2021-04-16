// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_encoder.h"

#include <limits>

#include "base/numerics/safe_conversions.h"
#include "media/audio/audio_opus_encoder.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "media/base/mime_util.h"
#include "media/base/offloading_audio_encoder.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_support.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_frame_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk_metadata.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

AudioEncoderTraits::ParsedConfig* ParseConfigStatic(
    const AudioEncoderConfig* config,
    ExceptionState& exception_state) {
  if (!config) {
    exception_state.ThrowTypeError("No config provided");
    return nullptr;
  }
  auto* result = MakeGarbageCollected<AudioEncoderTraits::ParsedConfig>();

  result->codec = media::kUnknownAudioCodec;
  bool is_codec_ambiguous = true;
  bool parse_succeeded = ParseAudioCodecString(
      "", config->codec().Utf8(), &is_codec_ambiguous, &result->codec);

  if (!parse_succeeded || is_codec_ambiguous) {
    exception_state.ThrowTypeError("Unknown codec.");
    return nullptr;
  }

  result->options.channels = config->numberOfChannels();
  if (result->options.channels < 1 ||
      result->options.channels > media::limits::kMaxChannels) {
    exception_state.ThrowTypeError(String::Format(
        "Invalid channel number; expected range from %d to %d, received %d.", 1,
        media::limits::kMaxChannels, result->options.channels));
    return nullptr;
  }

  result->options.sample_rate = config->sampleRate();
  if (result->options.sample_rate < media::limits::kMinSampleRate ||
      result->options.sample_rate > media::limits::kMaxSampleRate) {
    exception_state.ThrowTypeError(String::Format(
        "Invalid sample rate; expected range from %d to %d, received %d.",
        media::limits::kMinSampleRate, media::limits::kMaxSampleRate,
        result->options.sample_rate));
    return nullptr;
  }

  result->codec_string = config->codec();
  if (config->hasBitrate()) {
    if (config->bitrate() > std::numeric_limits<int>::max()) {
      exception_state.ThrowTypeError(String::Format(
          "Bitrate is too large; expected at most %d, received %" PRIu64,
          std::numeric_limits<int>::max(), config->bitrate()));
      return nullptr;
    }
    result->options.bitrate = static_cast<int>(config->bitrate());
  }

  return result;
}

bool VerifyCodecSupportStatic(AudioEncoderTraits::ParsedConfig* config,
                              ExceptionState* exception_state) {
  switch (config->codec) {
    case media::kCodecOpus: {
      if (config->options.channels > 2) {
        // Our Opus implementation only supports up to 2 channels
        if (exception_state) {
          exception_state->ThrowDOMException(
              DOMExceptionCode::kNotSupportedError,
              String::Format("Too many channels for Opus encoder; "
                             "expected at most 2, received %d.",
                             config->options.channels));
        }
        return false;
      }
      if (config->options.bitrate.has_value() &&
          config->options.bitrate.value() <
              media::AudioOpusEncoder::kMinBitrate) {
        if (exception_state) {
          exception_state->ThrowDOMException(
              DOMExceptionCode::kNotSupportedError,
              String::Format(
                  "Opus bitrate is too low; expected at least %d, received %d.",
                  media::AudioOpusEncoder::kMinBitrate,
                  config->options.bitrate.value()));
        }
        return false;
      }
      return true;
    }
    default:
      if (exception_state) {
        exception_state->ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                           "Unsupported codec type.");
      }
      return false;
  }
}

AudioEncoderConfig* CopyConfig(const AudioEncoderConfig& config) {
  auto* result = AudioEncoderConfig::Create();
  result->setCodec(config.codec());
  result->setSampleRate(config.sampleRate());
  result->setNumberOfChannels(config.numberOfChannels());
  if (config.hasBitrate())
    result->setBitrate(config.bitrate());
  return result;
}

}  // namespace

// static
const char* AudioEncoderTraits::GetNameForDevTools() {
  return "AudioEncoder(WebCodecs)";
}

AudioEncoder* AudioEncoder::Create(ScriptState* script_state,
                                   const AudioEncoderInit* init,
                                   ExceptionState& exception_state) {
  auto* result =
      MakeGarbageCollected<AudioEncoder>(script_state, init, exception_state);
  return exception_state.HadException() ? nullptr : result;
}

AudioEncoder::AudioEncoder(ScriptState* script_state,
                           const AudioEncoderInit* init,
                           ExceptionState& exception_state)
    : Base(script_state, init, exception_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

AudioEncoder::~AudioEncoder() = default;

void AudioEncoder::ProcessConfigure(Request* request) {
  DCHECK_NE(state_.AsEnum(), V8CodecState::Enum::kClosed);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK(active_config_);
  DCHECK_EQ(active_config_->codec, media::kCodecOpus);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto software_encoder = std::make_unique<media::AudioOpusEncoder>();
  media_encoder_ = std::make_unique<media::OffloadingAudioEncoder>(
      std::move(software_encoder));

  auto output_cb = ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
      &AudioEncoder::CallOutputCallback, WrapCrossThreadWeakPersistent(this),
      // We can't use |active_config_| from |this| because it can change by
      // the time the callback is executed.
      WrapCrossThreadPersistent(active_config_.Get()), reset_count_));

  auto done_callback = [](AudioEncoder* self, uint32_t reset_count,
                          media::Status status) {
    if (!self || self->reset_count_ != reset_count)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      self->HandleError(
          self->logger_->MakeException("Encoding error.", status));
    }
    self->stall_request_processing_ = false;
    self->ProcessRequests();
  };

  stall_request_processing_ = true;
  first_output_after_configure_ = true;
  media_encoder_->Initialize(
      active_config_->options, std::move(output_cb),
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          done_callback, WrapCrossThreadWeakPersistent(this), reset_count_)));
}

void AudioEncoder::ProcessEncode(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(media_encoder_);
  DCHECK_EQ(request->type, Request::Type::kEncode);
  DCHECK_GT(requested_encodes_, 0);

  auto* frame = request->frame.Release();
  auto* buffer = frame->buffer();

  auto done_callback = [](AudioEncoder* self, uint32_t reset_count,
                          media::Status status) {
    if (!self || self->reset_count_ != reset_count)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      self->HandleError(
          self->logger_->MakeException("Encoding error.", status));
    }
    self->ProcessRequests();
  };

  if (buffer->numberOfChannels() != uint8_t{active_config_->options.channels} ||
      buffer->sampleRate() != active_config_->options.sample_rate) {
    media::Status error(media::StatusCode::kEncoderFailedEncode);
    error.WithData("channels", int{buffer->numberOfChannels()});
    error.WithData("sampleRate", buffer->sampleRate());

    HandleError(logger_->MakeException(
        "Input audio buffer is incompatible with codec parameters", error));
    frame->close();
    return;
  }

  DCHECK(buffer);

  // TODO(crbug.com/1168418): There are two reasons we need to copy |buffer|
  // data here:
  // 1. AudioBus data needs to be 16 bytes aligned and |buffer| data might not
  // be aligned like that.
  // 2. The encoder might need to access this data on a different thread, which
  // is not allowed from blink point of view.
  //
  // If we could transfer AudioBuffer's data to another thread, we wouldn't need
  // to copy it, if alignment happens to be right.
  auto audio_bus =
      media::AudioBus::Create(buffer->numberOfChannels(), buffer->length());
  for (int channel = 0; channel < audio_bus->channels(); channel++) {
    auto array = buffer->getChannelData(channel);
    size_t byte_length = array->byteLength();
    DCHECK_EQ(byte_length, audio_bus->frames() * sizeof(float));
    memcpy(audio_bus->channel(channel), array->Data(), byte_length);
  }

  base::TimeTicks timestamp =
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(frame->timestamp());
  media_encoder_->Encode(
      std::move(audio_bus), timestamp,
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          done_callback, WrapCrossThreadWeakPersistent(this), reset_count_)));

  frame->close();
}

void AudioEncoder::ProcessReconfigure(Request* request) {
  // Audio decoders don't currently support any meaningful reconfiguring
}

AudioEncoder::ParsedConfig* AudioEncoder::ParseConfig(
    const AudioEncoderConfig* opts,
    ExceptionState& exception_state) {
  return ParseConfigStatic(opts, exception_state);
}

bool AudioEncoder::CanReconfigure(ParsedConfig& original_config,
                                  ParsedConfig& new_config) {
  return original_config.codec == new_config.codec &&
         original_config.options.channels == new_config.options.channels &&
         original_config.options.bitrate == new_config.options.bitrate &&
         original_config.options.sample_rate == new_config.options.sample_rate;
}

AudioFrame* AudioEncoder::CloneFrame(AudioFrame* frame,
                                     ExecutionContext* context) {
  auto* init = AudioFrameInit::Create();
  init->setTimestamp(frame->timestamp());

  auto* buffer = frame->buffer();
  if (!buffer)
    return nullptr;

  // Validata that buffer's data is consistent
  for (auto channel = 0u; channel < buffer->numberOfChannels(); channel++) {
    auto array = buffer->getChannelData(channel);
    float* data = array->Data();
    if (!data)
      return nullptr;
    if (array->length() != buffer->length())
      return nullptr;
  }

  init->setBuffer(buffer);
  return MakeGarbageCollected<AudioFrame>(init);
}

bool AudioEncoder::VerifyCodecSupport(ParsedConfig* config,
                                      ExceptionState& exception_state) {
  return VerifyCodecSupportStatic(config, &exception_state);
}

void AudioEncoder::CallOutputCallback(
    ParsedConfig* active_config,
    uint32_t reset_count,
    media::EncodedAudioBuffer encoded_buffer,
    base::Optional<media::AudioEncoder::CodecDescription> codec_desc) {
  DCHECK(active_config);
  if (!script_state_->ContextIsValid() || !output_callback_ ||
      state_.AsEnum() != V8CodecState::Enum::kConfigured ||
      reset_count != reset_count_)
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto timestamp = encoded_buffer.timestamp - base::TimeTicks();
  auto deleter = [](void* data, size_t length, void*) {
    delete[] static_cast<uint8_t*>(data);
  };
  ArrayBufferContents data(encoded_buffer.encoded_data.release(),
                           encoded_buffer.encoded_data_size, deleter);
  auto* dom_array = MakeGarbageCollected<DOMArrayBuffer>(std::move(data));
  auto* chunk =
      MakeGarbageCollected<EncodedAudioChunk>(timestamp, false, dom_array);

  auto* metadata = MakeGarbageCollected<EncodedAudioChunkMetadata>();
  if (first_output_after_configure_ || codec_desc.has_value()) {
    first_output_after_configure_ = false;
    auto* decoder_config = MakeGarbageCollected<AudioDecoderConfig>();
    decoder_config->setCodec(active_config->codec_string);
    decoder_config->setSampleRate(active_config->options.sample_rate);
    decoder_config->setNumberOfChannels(active_config->options.channels);
    if (codec_desc.has_value()) {
      auto* desc_array_buf = DOMArrayBuffer::Create(codec_desc.value().data(),
                                                    codec_desc.value().size());
      decoder_config->setDescription(
          ArrayBufferOrArrayBufferView::FromArrayBuffer(desc_array_buf));
    }
    metadata->setDecoderConfig(decoder_config);
  }

  ScriptState::Scope scope(script_state_);
  output_callback_->InvokeAndReportException(nullptr, chunk, metadata);
}

// static
ScriptPromise AudioEncoder::isConfigSupported(ScriptState* script_state,
                                              const AudioEncoderConfig* config,
                                              ExceptionState& exception_state) {
  auto* parsed_config = ParseConfigStatic(config, exception_state);
  if (!parsed_config) {
    DCHECK(exception_state.HadException());
    return ScriptPromise();
  }

  auto* support = AudioEncoderSupport::Create();
  support->setSupported(VerifyCodecSupportStatic(parsed_config, nullptr));
  support->setConfig(CopyConfig(*config));
  return ScriptPromise::Cast(script_state, ToV8(support, script_state));
}

}  // namespace blink
