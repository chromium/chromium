// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_encoder.h"

#include "base/numerics/safe_conversions.h"
#include "media/audio/audio_opus_encoder.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/offloading_audio_encoder.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_frame_init.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_metadata.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

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
  produced_first_output_ = false;
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

  // Converting time at the beginning of the frame (aka timestamp) into
  // time at the end of the frame (aka capture time) that is expected by
  // media::AudioEncoder.
  base::TimeTicks capture_time =
      base::TimeTicks() +
      base::TimeDelta::FromMicroseconds(frame->timestamp()) +
      media::AudioTimestampHelper::FramesToTime(
          buffer->length(), active_config_->options.sample_rate);
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

  media_encoder_->Encode(
      std::move(audio_bus), capture_time,
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
  auto* result = MakeGarbageCollected<ParsedConfig>();
  result->codec = opts->codec().Utf8() == "opus" ? media::kCodecOpus
                                                 : media::kUnknownAudioCodec;
  result->options.channels = opts->numberOfChannels();

  result->options.sample_rate = opts->sampleRate();
  result->codec_string = opts->codec();
  if (opts->hasBitrate()) {
    if (!base::IsValueInRangeForNumericType<int>(opts->bitrate())) {
      exception_state.ThrowTypeError("Invalid bitrate.");
      return nullptr;
    }
    result->options.bitrate = static_cast<int>(opts->bitrate());
  }

  if (result->options.channels == 0) {
    exception_state.ThrowTypeError("Invalid channel number.");
    return nullptr;
  }

  if (result->options.sample_rate == 0) {
    exception_state.ThrowTypeError("Invalid sample rate.");
    return nullptr;
  }
  return result;
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
  if (config->codec != media::kCodecOpus) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Unsupported codec type.");
    return false;
  }
  return true;
}

void AudioEncoder::CallOutputCallback(
    ParsedConfig* active_config,
    uint32_t reset_count,
    media::EncodedAudioBuffer encoded_buffer,
    base::Optional<media::AudioEncoder::CodecDescription> codec_desc) {
  if (!script_state_->ContextIsValid() || !output_callback_ ||
      state_.AsEnum() != V8CodecState::Enum::kConfigured ||
      reset_count != reset_count_)
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EncodedAudioMetadata metadata;
  metadata.timestamp = encoded_buffer.timestamp - base::TimeTicks();
  auto deleter = [](void* data, size_t length, void*) {
    delete[] static_cast<uint8_t*>(data);
  };
  ArrayBufferContents data(encoded_buffer.encoded_data.release(),
                           encoded_buffer.encoded_data_size, deleter);
  auto* dom_array = MakeGarbageCollected<DOMArrayBuffer>(std::move(data));
  auto* chunk = MakeGarbageCollected<EncodedAudioChunk>(metadata, dom_array);

  AudioDecoderConfig* decoder_config = nullptr;
  if (!produced_first_output_ || codec_desc.has_value()) {
    decoder_config = MakeGarbageCollected<AudioDecoderConfig>();
    decoder_config->setCodec(active_config->codec_string);
    decoder_config->setSampleRate(active_config->options.sample_rate);
    decoder_config->setNumberOfChannels(active_config->options.channels);
    if (codec_desc.has_value()) {
      auto* desc_array_buf = DOMArrayBuffer::Create(codec_desc.value().data(),
                                                    codec_desc.value().size());
      decoder_config->setDescription(
          ArrayBufferOrArrayBufferView::FromArrayBuffer(desc_array_buf));
    }
    produced_first_output_ = true;
  }

  ScriptState::Scope scope(script_state_);
  output_callback_->InvokeAndReportException(nullptr, chunk, decoder_config);
}

}  // namespace blink
