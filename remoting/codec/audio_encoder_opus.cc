// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/audio_encoder_opus.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/multi_channel_resampler.h"
#include "third_party/opus/src/include/opus.h"

namespace remoting {

namespace {

// Output 160 kb/s bitrate.
constexpr int kOutputBitrateBps = 160 * 1024;

// Opus doesn't support 44100 sampling rate so we always resample to 48kHz.
constexpr AudioPacket::SamplingRate kOpusSamplingRate =
    AudioPacket::SAMPLING_RATE_48000;

// Opus supports frame sizes of 2.5, 5, 10, 20, 40 and 60 ms. We use 20 ms
// frames to balance latency and efficiency.
constexpr base::TimeDelta kFrameDuration = base::Milliseconds(20);

// Number of audio frames per "opus frame" when using default sampling rate.
constexpr size_t kOpusFrameCount = kOpusSamplingRate *
                                   kFrameDuration.InMilliseconds() /
                                   base::Time::kMillisecondsPerSecond;

constexpr AudioPacket::BytesPerSample kBytesPerSample =
    AudioPacket::BYTES_PER_SAMPLE_2;

constexpr bool IsSupportedSampleRate(int rate) {
  return rate == 44100 || rate == 48000;
}

}  // namespace

AudioEncoderOpus::AudioEncoderOpus() = default;

AudioEncoderOpus::~AudioEncoderOpus() {
  DestroyEncoder();
}

void AudioEncoderOpus::InitEncoder() {
  DCHECK(!encoder_);
  int error;
  encoder_ = opus_encoder_create(kOpusSamplingRate, channels_,
                                 OPUS_APPLICATION_AUDIO, &error);
  if (!encoder_) {
    LOG(ERROR) << "Failed to create OPUS encoder. Error code: " << error;
    return;
  }

  opus_encoder_ctl(encoder_.get(), OPUS_SET_BITRATE(kOutputBitrateBps));

  frame_size_ =
      media::AudioTimestampHelper::TimeToFrames(kFrameDuration, sampling_rate_);

  if (sampling_rate_ != kOpusSamplingRate) {
    size_t total_samples =
        base::CheckMul(kOpusFrameCount, channels_).ValueOrDie<size_t>();
    resample_buffer_ = base::AlignedUninit<int16_t>(
        total_samples, media::AudioBus::kChannelAlignment);
    // TODO(sergeyu): Figure out the right buffer size to use per packet instead
    // of using media::SincResampler::kDefaultRequestSize.
    resampler_ = std::make_unique<media::MultiChannelResampler>(
        channels_, sampling_rate_ / double{kOpusSamplingRate},
        media::SincResampler::kDefaultRequestSize,
        base::BindRepeating(&AudioEncoderOpus::FetchBytesToResample,
                            base::Unretained(this)));
    resampler_bus_ = media::AudioBus::Create(channels_, kOpusFrameCount);
  }

  // Drop leftover data because it's for different sampling rate.
  leftover_frames_ = 0;
  leftover_samples_size_in_frames_ =
      frame_size_ + media::SincResampler::kDefaultRequestSize;
  leftover_samples_.reset(
      new int16_t[leftover_samples_size_in_frames_ * channels_]);
}

void AudioEncoderOpus::DestroyEncoder() {
  if (encoder_) {
    opus_encoder_destroy(encoder_);
    encoder_ = nullptr;
  }

  resampler_.reset();
}

bool AudioEncoderOpus::ResetForPacket(AudioPacket* packet) {
  if (packet->channels() != channels_ ||
      packet->sampling_rate() != sampling_rate_) {
    DestroyEncoder();

    channels_ = packet->channels();
    sampling_rate_ = packet->sampling_rate();

    if (channels_ <= 0 || channels_ > 2 ||
        !IsSupportedSampleRate(sampling_rate_)) {
      LOG(WARNING) << "Unsupported OPUS parameters: " << channels_
                   << " channels with " << sampling_rate_
                   << " samples per second.";
      return false;
    }

    InitEncoder();
  }

  return encoder_ != nullptr;
}

void AudioEncoderOpus::FetchBytesToResample(int resampler_frame_delay,
                                            media::AudioBus* audio_bus) {
  DCHECK(resampling_data_);
  int samples_left = (resampling_data_size_ - resampling_data_pos_) /
                     kBytesPerSample / channels_;
  DCHECK_LE(audio_bus->frames(), samples_left);
  static_assert(kBytesPerSample == 2, "FromInterleaved expects 2 bytes.");
  audio_bus->FromInterleaved<media::SignedInt16SampleTypeTraits>(
      reinterpret_cast<const int16_t*>(
          UNSAFE_TODO(resampling_data_ + resampling_data_pos_)),
      audio_bus->frames());
  resampling_data_pos_ += audio_bus->frames() * kBytesPerSample * channels_;
  DCHECK_LE(resampling_data_pos_, static_cast<int>(resampling_data_size_));
}

int AudioEncoderOpus::GetBitrate() {
  return kOutputBitrateBps;
}

std::unique_ptr<AudioPacket> AudioEncoderOpus::Encode(
    std::unique_ptr<AudioPacket> packet) {
  DCHECK_EQ(AudioPacket::ENCODING_RAW, packet->encoding());
  DCHECK_EQ(1, packet->data_size());
  DCHECK_EQ(kBytesPerSample, packet->bytes_per_sample());

  if (!ResetForPacket(packet.get())) {
    LOG(ERROR) << "Encoder initialization failed";
    return nullptr;
  }

  int frames_in_packet = packet->data(0).size() / kBytesPerSample / channels_;
  const int16_t* next_sample =
      UNSAFE_TODO(reinterpret_cast<const int16_t*>(packet->data(0).data()));

  // Create a new packet of encoded data.
  auto encoded_packet = std::make_unique<AudioPacket>();
  encoded_packet->set_encoding(AudioPacket::ENCODING_OPUS);
  encoded_packet->set_sampling_rate(kOpusSamplingRate);
  encoded_packet->set_channels(channels_);

  const int prefetch_frames =
      resampler_.get() ? media::SincResampler::kDefaultRequestSize : 0;
  int frames_wanted = frame_size_ + prefetch_frames;

  while (leftover_frames_ + frames_in_packet >= frames_wanted) {
    const int16_t* pcm_buffer = nullptr;

    // Combine the packet with the leftover samples, if any.
    if (leftover_frames_ > 0) {
      pcm_buffer = leftover_samples_.get();
      const int frames_to_copy = frames_wanted - leftover_frames_;
      UNSAFE_TODO(memcpy(leftover_samples_.get() + leftover_frames_ * channels_,
                         next_sample,
                         frames_to_copy * kBytesPerSample * channels_));
    } else {
      pcm_buffer = next_sample;
    }

    // Resample data if necessary.
    int frames_consumed = 0;
    if (resampler_.get()) {
      resampling_data_ = reinterpret_cast<const char*>(pcm_buffer);
      resampling_data_pos_ = 0;
      resampling_data_size_ = frames_wanted * channels_ * kBytesPerSample;
      resampler_->Resample(kOpusFrameCount, resampler_bus_.get());
      resampling_data_ = nullptr;
      frames_consumed = resampling_data_pos_ / channels_ / kBytesPerSample;

      static_assert(kBytesPerSample == 2, "ToInterleaved expects 2 bytes.");
      resampler_bus_->ToInterleaved<media::SignedInt16SampleTypeTraits>(
          resample_buffer_);
      pcm_buffer = resample_buffer_.data();
    } else {
      frames_consumed = frame_size_;
    }

    // Initialize output buffer.
    std::string* data = encoded_packet->add_data();
    data->resize(kOpusFrameCount * kBytesPerSample * channels_);

    // Encode.
    unsigned char* buffer = reinterpret_cast<unsigned char*>(std::data(*data));
    int result = opus_encode(encoder_, pcm_buffer, kOpusFrameCount, buffer,
                             data->length());
    if (result < 0) {
      LOG(ERROR) << "opus_encode() failed with error code: " << result;
      return nullptr;
    }

    DCHECK_LE(result, static_cast<int>(data->length()));
    data->resize(result);

    // Cleanup leftover buffer.
    if (frames_consumed >= leftover_frames_) {
      frames_consumed -= leftover_frames_;
      leftover_frames_ = 0;
      UNSAFE_TODO(next_sample += frames_consumed * channels_);
      frames_in_packet -= frames_consumed;
    } else {
      leftover_frames_ -= frames_consumed;
      UNSAFE_TODO(memmove(leftover_samples_.get(),
                          leftover_samples_.get() + frames_consumed * channels_,
                          leftover_frames_ * channels_ * kBytesPerSample));
    }
  }

  // Store the leftover samples.
  if (frames_in_packet > 0) {
    DCHECK_LE(leftover_frames_ + frames_in_packet,
              leftover_samples_size_in_frames_);
    UNSAFE_TODO(memmove(leftover_samples_.get() + leftover_frames_ * channels_,
                        next_sample,
                        frames_in_packet * kBytesPerSample * channels_));
    leftover_frames_ += frames_in_packet;
  }

  // Return nullptr if there's nothing in the packet.
  if (encoded_packet->data_size() == 0) {
    return nullptr;
  }

  return encoded_packet;
}

}  // namespace remoting
