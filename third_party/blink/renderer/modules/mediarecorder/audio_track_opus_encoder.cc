// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/audio_track_opus_encoder.h"

#include <memory>
#include <optional>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"

namespace {

enum : size_t {
  // Recommended value for opus_encode_float(), according to documentation in
  // third_party/opus/src/include/opus.h, so that the Opus encoder does not
  // degrade the audio due to memory constraints, and is independent of the
  // duration of the encoded buffer.
  kOpusMaxDataBytes = 4000,

  // Opus preferred sampling rate for encoding. This is also the one WebM likes
  // to have: https://wiki.xiph.org/MatroskaOpus.
  kOpusPreferredSamplingRate = 48000,

  // For Opus, we try to encode 60ms, the maximum Opus buffer, for quality
  // reasons.
  kOpusPreferredBufferDurationMs = 60,

  // Maximum buffer multiplier for the AudioEncoders' AudioFifo. Recording is
  // not real time, hence a certain buffering is allowed.
  kMaxNumberOfFifoBuffers = 3,
};

// The amount of Frames in a 60 ms buffer @ 48000 samples/second.
const int kOpusPreferredFramesPerBuffer = kOpusPreferredSamplingRate *
                                          kOpusPreferredBufferDurationMs /
                                          base::Time::kMillisecondsPerSecond;

// Tries to encode |data_in|'s |num_samples| into |data_out|.
bool DoEncode(OpusEncoder* opus_encoder,
              float* data_in,
              int num_samples,
              base::span<uint8_t> data_out,
              size_t* actual_size) {
  DCHECK_EQ(kOpusPreferredFramesPerBuffer, num_samples);
  CHECK_EQ(data_out.size(), kOpusMaxDataBytes);

  const opus_int32 result =
      opus_encode_float(opus_encoder, data_in, num_samples, data_out.data(),
                        static_cast<int>(data_out.size()));

  if (result > 1) {
    *actual_size = result;
    return true;
  }
  // If |result| in {0,1}, do nothing; the documentation says that a return
  // value of zero or one means the packet does not need to be transmitted.
  // Otherwise, we have an error.
  DLOG_IF(ERROR, result < 0) << " encode failed: " << opus_strerror(result);
  return false;
}

}  // anonymous namespace

namespace blink {

AudioTrackOpusEncoder::AudioTrackOpusEncoder(
    OnEncodedAudioCB on_encoded_audio_cb,
    OnEncodedAudioErrorCB on_encoded_audio_error_cb,
    uint32_t bits_per_second,
    bool vbr_enabled)
    : AudioTrackEncoder(std::move(on_encoded_audio_cb),
                        std::move(on_encoded_audio_error_cb)),
      bits_per_second_(bits_per_second),
      vbr_enabled_(vbr_enabled),
      opus_encoder_(nullptr) {}

AudioTrackOpusEncoder::~AudioTrackOpusEncoder() {
  DestroyExistingOpusEncoder();
}

double AudioTrackOpusEncoder::ProvideInput(
    media::AudioBus* audio_bus,
    uint32_t frames_delayed,
    const media::AudioGlitchInfo& glitch_info) {
  fifo_->Consume(audio_bus, 0, audio_bus->frames());
  return 1.0;
}

void AudioTrackOpusEncoder::OnSetFormat(
    const media::AudioParameters& input_params) {
  DVLOG(1) << __func__;
  if (input_params_.Equals(input_params))
    return;

  DestroyExistingOpusEncoder();

  if (!input_params.IsValid()) {
    DLOG(ERROR) << "Invalid params: " << input_params.AsHumanReadableString();
    NotifyError(media::EncoderStatus::Codes::kEncoderUnsupportedConfig);
    return;
  }
  input_params_ = input_params;
  input_params_.set_frames_per_buffer(input_params_.sample_rate() *
                                      kOpusPreferredBufferDurationMs /
                                      base::Time::kMillisecondsPerSecond);

  // third_party/libopus supports up to 2 channels (see implementation of
  // opus_encoder_create()): force |converted_params_| to at most those.
  converted_params_ = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Guess(std::min(input_params_.channels(), 2)),
      kOpusPreferredSamplingRate, kOpusPreferredFramesPerBuffer);
  DVLOG(1) << "|input_params_|:" << input_params_.AsHumanReadableString()
           << " -->|converted_params_|:"
           << converted_params_.AsHumanReadableString();

  converter_ = std::make_unique<media::AudioConverter>(
      input_params_, converted_params_, false /* disable_fifo */);
  converter_->AddInput(this);
  converter_->PrimeWithSilence();

  fifo_ = std::make_unique<media::AudioFifo>(
      input_params_.channels(),
      kMaxNumberOfFifoBuffers * input_params_.frames_per_buffer());

  buffer_.reset(new float[converted_params_.channels() *
                          converted_params_.frames_per_buffer()]);

  // Initialize OpusEncoder.
  int opus_result;
  opus_encoder_ = opus_encoder_create(converted_params_.sample_rate(),
                                      converted_params_.channels(),
                                      OPUS_APPLICATION_AUDIO, &opus_result);
  if (opus_result < 0) {
    DLOG(ERROR) << "Couldn't init Opus encoder: " << opus_strerror(opus_result)
                << ", sample rate: " << converted_params_.sample_rate()
                << ", channels: " << converted_params_.channels();
    NotifyError(media::EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  // Note: As of 2013-10-31, the encoder in "auto bitrate" mode would use a
  // variable bitrate up to 102kbps for 2-channel, 48 kHz audio and a 10 ms
  // buffer duration. The Opus library authors may, of course, adjust this in
  // later versions.
  const opus_int32 bitrate =
      (bits_per_second_ > 0)
          ? base::saturated_cast<opus_int32>(bits_per_second_)
          : OPUS_AUTO;
  if (opus_encoder_ctl(opus_encoder_.get(), OPUS_SET_BITRATE(bitrate)) !=
      OPUS_OK) {
    DLOG(ERROR) << "Failed to set Opus bitrate: " << bitrate;
    NotifyError(media::EncoderStatus::Codes::kEncoderUnsupportedConfig);
    return;
  }

  const opus_int32 vbr_enabled = static_cast<opus_int32>(vbr_enabled_);
  if (opus_encoder_ctl(opus_encoder_.get(), OPUS_SET_VBR(vbr_enabled)) !=
      OPUS_OK) {
    DLOG(ERROR) << "Failed to set Opus VBR mode: " << vbr_enabled;
    NotifyError(media::EncoderStatus::Codes::kEncoderUnsupportedConfig);
    return;
  }
}

void AudioTrackOpusEncoder::EncodeAudio(
    std::unique_ptr<media::AudioBus> input_bus,
    base::TimeTicks capture_time) {
  DVLOG(3) << __func__ << ", #frames " << input_bus->frames();
  DCHECK_EQ(input_bus->channels(), input_params_.channels());
  DCHECK(!capture_time.is_null());
  DCHECK(converter_);

  if (!is_initialized() || paused_)
    return;

  // TODO(mcasas): Consider using a
  // base::circular_deque<std::unique_ptr<AudioBus>> instead of an AudioFifo,
  // to avoid copying data needlessly since we know the sizes of both input and
  // output and they are multiples.
  fifo_->Push(input_bus.get());

  // Wait to have enough |input_bus|s to guarantee a satisfactory conversion,
  // accounting for multiple calls to ProvideInput().
  while (fifo_->frames() >= converter_->GetMaxInputFramesRequested(
                                kOpusPreferredFramesPerBuffer)) {
    std::unique_ptr<media::AudioBus> audio_bus = media::AudioBus::Create(
        converted_params_.channels(), kOpusPreferredFramesPerBuffer);
    converter_->Convert(audio_bus.get());
    audio_bus->ToInterleaved<media::Float32SampleTypeTraits>(
        audio_bus->frames(), buffer_.get());

    if (packet_buffer_.empty()) {
      packet_buffer_ = base::HeapArray<uint8_t>::Uninit(kOpusMaxDataBytes);
    }
    size_t actual_size;
    if (DoEncode(opus_encoder_, buffer_.get(), kOpusPreferredFramesPerBuffer,
                 packet_buffer_, &actual_size)) {
      const base::TimeTicks capture_time_of_first_sample =
          capture_time - media::AudioTimestampHelper::FramesToTime(
                             input_bus->frames(), input_params_.sample_rate());

      auto buffer =
          media::DecoderBuffer::CopyFrom(packet_buffer_.first(actual_size));
      on_encoded_audio_cb_.Run(converted_params_, std::move(buffer),
                               std::nullopt, capture_time_of_first_sample);
    } else {
      // Opus encoder keeps running even if it fails to encode a frame, which
      // is different behavior from the AAC encoder.
      NotifyError(media::EncoderStatus::Codes::kEncoderFailedEncode);
    }
  }
}

void AudioTrackOpusEncoder::DestroyExistingOpusEncoder() {
  // We don't DCHECK that we're on the encoder thread here, as this could be
  // called from the dtor (main thread) or from OnSetFormat() (encoder thread).
  if (opus_encoder_) {
    opus_encoder_destroy(opus_encoder_);
    opus_encoder_ = nullptr;
  }
}

void AudioTrackOpusEncoder::NotifyError(media::EncoderStatus error) {
  if (on_encoded_audio_error_cb_.is_null()) {
    return;
  }

  std::move(on_encoded_audio_error_cb_).Run(std::move(error));
}
}  // namespace blink
