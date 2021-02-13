// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_encoder.h"

#include "media/audio/audio_opus_encoder.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_frame_init.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_metadata.h"

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

  auto output_cb = WTF::BindRepeating(
      &AudioEncoder::CallOutputCallback, WrapCrossThreadWeakPersistent(this),
      // We can't use |active_config_| from |this| because it can change by
      // the time the callback is executed.
      WrapCrossThreadPersistent(active_config_.Get()), reset_count_);

  auto status_callback = [](AudioEncoder* self, uint32_t reset_count,
                            media::Status status) {
    if (!self || self->reset_count_ != reset_count)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      self->HandleError(
          self->logger_->MakeException("Encoding error.", status));
    }
  };

  media::AudioParameters input_params(media::AudioParameters::AUDIO_PCM_LINEAR,
                                      media::CHANNEL_LAYOUT_DISCRETE,
                                      active_config_->sample_rate, 0);
  input_params.set_channels_for_discrete(active_config_->channels);
  media_encoder_ = std::make_unique<media::AudioOpusEncoder>(
      input_params, output_cb,
      WTF::BindRepeating(status_callback, WrapCrossThreadWeakPersistent(this),
                         reset_count_),
      active_config_->bitrate);
  produced_first_output_ = false;
}

void AudioEncoder::ProcessEncode(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(media_encoder_);
  DCHECK_EQ(request->type, Request::Type::kEncode);
  DCHECK_GT(requested_encodes_, 0);

  auto* frame = request->frame.Release();
  auto* buffer = frame->buffer();

  // Converting time at the beginning of the frame (aka timestamp) into
  // time at the end of the frame (aka capture time) that is expected by
  // media::AudioEncoder.
  base::TimeTicks capture_time =
      base::TimeTicks() +
      base::TimeDelta::FromMicroseconds(frame->timestamp()) +
      media::AudioTimestampHelper::FramesToTime(buffer->length(),
                                                active_config_->sample_rate);
  DCHECK(buffer);
  {
    auto audio_bus = media::AudioBus::CreateWrapper(buffer->numberOfChannels());
    for (int channel = 0; channel < audio_bus->channels(); channel++) {
      float* data = buffer->getChannelData(channel)->Data();
      DCHECK(data);
      audio_bus->SetChannelData(channel, data);
    }
    audio_bus->set_frames(buffer->length());
    media_encoder_->EncodeAudio(*audio_bus, capture_time);
  }

  frame->close();
}

void AudioEncoder::ProcessReconfigure(Request* request) {
  // Audio decoders don't currently support any meaningful reconfiguring
}
void AudioEncoder::ProcessFlush(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(media_encoder_);
  DCHECK_EQ(request->type, Request::Type::kFlush);

  media_encoder_->Flush();
  request->resolver->Resolve();
}

AudioEncoder::ParsedConfig* AudioEncoder::ParseConfig(
    const AudioEncoderConfig* opts,
    ExceptionState& exception_state) {
  auto* result = MakeGarbageCollected<ParsedConfig>();
  result->codec = opts->codec().Utf8() == "opus" ? media::kCodecOpus
                                                 : media::kUnknownAudioCodec;
  result->channels = opts->numberOfChannels();
  result->bitrate = opts->bitrate();
  result->sample_rate = opts->sampleRate();
  result->codec_string = opts->codec();

  if (result->channels == 0) {
    exception_state.ThrowTypeError("Invalid channel number.");
    return nullptr;
  }

  if (result->bitrate == 0) {
    exception_state.ThrowTypeError("Invalid bitrate.");
    return nullptr;
  }

  if (result->sample_rate == 0) {
    exception_state.ThrowTypeError("Invalid sample rate.");
    return nullptr;
  }
  return result;
}

bool AudioEncoder::CanReconfigure(ParsedConfig& original_config,
                                  ParsedConfig& new_config) {
  return original_config.codec == new_config.codec &&
         original_config.channels == new_config.channels &&
         original_config.bitrate == new_config.bitrate &&
         original_config.sample_rate == new_config.sample_rate;
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
    media::EncodedAudioBuffer encoded_buffer) {
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
  if (!produced_first_output_) {
    decoder_config = MakeGarbageCollected<AudioDecoderConfig>();
    decoder_config->setCodec(active_config->codec_string);
    decoder_config->setSampleRate(active_config->sample_rate);
    decoder_config->setNumberOfChannels(active_config->channels);
    auto extra_data = media_encoder_->GetExtraData();
    if (!extra_data.empty()) {
      auto* desc_array_buf =
          DOMArrayBuffer::Create(extra_data.data(), extra_data.size());
      decoder_config->setDescription(
          ArrayBufferOrArrayBufferView::FromArrayBuffer(desc_array_buf));
    }
    produced_first_output_ = true;
  }

  ScriptState::Scope scope(script_state_);
  output_callback_->InvokeAndReportException(nullptr, chunk, decoder_config);
}

}  // namespace blink
