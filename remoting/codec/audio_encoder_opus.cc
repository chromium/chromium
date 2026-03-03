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

// If not using `kOpusSampleRate`, the only other input sample rate we accept is
// 44.1kHz.
constexpr AudioPacket::SamplingRate kAltSamplingRate =
    AudioPacket::SAMPLING_RATE_44100;

// Opus supports frame sizes of 2.5, 5, 10, 20, 40 and 60 ms. We use 20 ms
// frames to balance latency and efficiency.
constexpr base::TimeDelta kFrameDuration = base::Milliseconds(20);

// Number of audio frames per "opus frame" when using default sampling rate.
constexpr size_t kOpusFrameCount = kOpusSamplingRate *
                                   kFrameDuration.InMilliseconds() /
                                   base::Time::kMillisecondsPerSecond;

constexpr AudioPacket::BytesPerSample kBytesPerSample =
    AudioPacket::BYTES_PER_SAMPLE_2;

// Size in frames of the resampler's fill requests.
constexpr size_t kResamplerRequestSize =
    media::SincResampler::kDefaultRequestSize;

constexpr bool IsSupportedSampleRate(int rate) {
  return rate == 44100 || rate == 48000;
}

}  // namespace

class ResamplerFifoImpl : public AudioEncoderOpus::ResamplerFifo {
 public:
  explicit ResamplerFifoImpl(size_t size_in_frames, size_t channels)
      : chunk_size_(kResamplerRequestSize * channels),
        storage_buffer_(
            base::AlignedUninit<int16_t>(size_in_frames * channels,
                                         media::AudioBus::kChannelAlignment)),
        crossover_buffer_(
            base::AlignedUninit<int16_t>(chunk_size_,
                                         media::AudioBus::kChannelAlignment)) {}
  ~ResamplerFifoImpl() override = default;

  void AddNewSamples(base::span<const int16_t> samples) override {
    CHECK(new_samples_.empty());
    new_samples_ = samples;
  }

  [[nodiscard]] base::span<const int16_t> TakeChunk() override {
    CHECK_GE(remaining_samples(), chunk_size_);

    if (saved_samples_.empty()) {
      return new_samples_.take_first(chunk_size_);
    }

    if (saved_samples_.size() > chunk_size_) {
      return saved_samples_.take_first(chunk_size_);
    }

    // If we're here, we have to combine some of the previous samples with the
    // new ones. To return a continuous span, we have to make a temporary copy
    // into `crossover_buffer_`.
    const size_t new_samples_needed = chunk_size_ - saved_samples_.size();
    auto [saved_dest, new_dest] =
        base::span(crossover_buffer_).split_at(saved_samples_.size());

    saved_dest.copy_from_nonoverlapping(saved_samples_);
    saved_samples_ = {};

    new_dest.copy_from_nonoverlapping(
        new_samples_.take_first(new_samples_needed));

    // Return merged samples.
    return crossover_buffer_;
  }

  void SaveNewSamples() override {
    CHECK_LE(remaining_samples(), storage_buffer_.size());

    // No other saved samples. Copy directly.
    if (saved_samples_.empty()) {
      auto save_dest = storage_buffer_.first(new_samples_.size());
      save_dest.copy_from_nonoverlapping(new_samples_);
      new_samples_ = {};
      saved_samples_ = save_dest;
      return;
    }

    // There are some saved samples remaining. Copy them to the front of
    // `storage_buffer_` first, and then append the new samples.
    const size_t total_size = remaining_samples();
    auto storage = storage_buffer_.first(total_size);
    auto [previous_dest, new_dest] = storage.split_at(saved_samples_.size());

    previous_dest.copy_from(saved_samples_);
    new_dest.copy_from_nonoverlapping(new_samples_);

    new_samples_ = {};
    saved_samples_ = storage;
  }

  size_t remaining_samples() const override {
    return saved_samples_.size() + new_samples_.size();
  }

  size_t GetChunkSizeForTesting() const override { return chunk_size_; }

 private:
  const size_t chunk_size_;

  // Location where `new_samples_` are copied, during `SaveNewSamples()`;
  base::AlignedHeapArray<int16_t> storage_buffer_;

  // Temporary location where `saved_samples_` and `new_samples_` are stored,
  // when there aren't enough saved samples to completely fill one chunk.
  base::AlignedHeapArray<int16_t> crossover_buffer_;

  // Portion of `storage_buffer_` which contains unused samples.
  base::raw_span<const int16_t> saved_samples_;

  // Points towards external, unowned memory.
  base::raw_span<const int16_t> new_samples_;
};

AudioEncoderOpus::AudioEncoderOpus() = default;

AudioEncoderOpus::~AudioEncoderOpus() {
  DestroyEncoder();
}

// static
std::unique_ptr<AudioEncoderOpus::ResamplerFifo>
AudioEncoderOpus::GetEmptyFifoForTesting(size_t size_in_frames,
                                         size_t channels) {
  return std::make_unique<ResamplerFifoImpl>(size_in_frames, channels);
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

  needs_resampling_ = sampling_rate_ != kOpusSamplingRate;

  // Drop any previous samples.
  leftover_encoder_samples_ = {};

  if (needs_resampling_) {
    CHECK_EQ(sampling_rate_, kAltSamplingRate);
    resampler_ = std::make_unique<media::MultiChannelResampler>(
        channels_, sampling_rate_ / double{kOpusSamplingRate},
        kResamplerRequestSize,
        base::BindRepeating(&AudioEncoderOpus::FetchBytesToResample,
                            base::Unretained(this)));

    const size_t min_input_frames_needed =
        resampler_->GetMaxInputFramesRequested(kOpusFrameCount);

    resampling_samples_needed_ = min_input_frames_needed * channels_;

    resampler_fifo_ = std::make_unique<ResamplerFifoImpl>(
        resampling_samples_needed_, channels_);
    resampler_bus_ = media::AudioBus::Create(channels_, kOpusFrameCount);
  }

  encoder_samples_needed_ = kOpusFrameCount * channels_;
  encoder_input_ = base::AlignedUninit<int16_t>(encoder_samples_needed_);
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
  CHECK(needs_resampling_);
  CHECK_GE(resampler_fifo_->remaining_samples(),
           static_cast<size_t>(audio_bus->frames() * channels_));
  static_assert(kBytesPerSample == 2, "FromInterleaved expects 2 bytes.");
  audio_bus->FromInterleaved<media::SignedInt16SampleTypeTraits>(
      resampler_fifo_->TakeChunk());
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

  base::span<const uint8_t> byte_input = base::as_byte_span(packet->data(0));
  CHECK_EQ(byte_input.size() % kBytesPerSample, 0u);
  CHECK(base::IsAligned(byte_input.data(), sizeof(int16_t)));

  // SAFETY: This data is coming from an external source, but we've CHECK'ed
  // that there are the right number of bytes, and that they have the proper
  // alignment.
  base::span<const int16_t> input_samples = UNSAFE_BUFFERS(
      base::span(reinterpret_cast<const int16_t*>(byte_input.data()),
                 byte_input.size() / sizeof(int16_t)));

  if (needs_resampling_) {
    return EncodeInternalWithResampling(input_samples);
  }

  return EncodeInternal(input_samples);
}

bool AudioEncoderOpus::EncodeData(base::span<const int16_t> samples,
                                  AudioPacket* destination) {
  CHECK_EQ(samples.size(), encoder_samples_needed_);

  // Initialize output buffer.
  std::string* data = destination->add_data();
  data->resize(encoder_samples_needed_ * kBytesPerSample);

  // Encode.
  unsigned char* buffer = reinterpret_cast<unsigned char*>(std::data(*data));
  int result = opus_encode(encoder_, samples.data(), kOpusFrameCount, buffer,
                           data->length());
  if (result < 0) {
    LOG(ERROR) << "opus_encode() failed with error code: " << result;
    return false;
  }

  CHECK_LE(result, static_cast<int>(data->length()));
  data->resize(result);
  return true;
}

std::unique_ptr<AudioPacket> AudioEncoderOpus::CreatePacket() {
  // Create a new packet of encoded data.
  auto encoded_packet = std::make_unique<AudioPacket>();
  encoded_packet->set_encoding(AudioPacket::ENCODING_OPUS);
  encoded_packet->set_sampling_rate(kOpusSamplingRate);
  encoded_packet->set_channels(channels_);
  return encoded_packet;
}

std::unique_ptr<AudioPacket> AudioEncoderOpus::EncodeInternal(
    base::span<const int16_t> input_samples) {
  CHECK(!needs_resampling_);

  // Create a new packet of encoded data.
  auto encoded_packet = CreatePacket();

  while (leftover_encoder_samples_.size() + input_samples.size() >=
         encoder_samples_needed_) {
    // If there are no leftover frames, encode directly.
    if (leftover_encoder_samples_.empty()) {
      EncodeData(input_samples.take_first(encoder_samples_needed_),
                 encoded_packet.get());
      continue;
    }

    // Fill `encoder_input_` completely.
    const size_t free_space_size =
        encoder_samples_needed_ - leftover_encoder_samples_.size();

    encoder_input_.subspan(leftover_encoder_samples_.size())
        .copy_from_nonoverlapping(input_samples.take_first(free_space_size));

    // Encode the samples. All clean leftovers, as they already encoded.
    EncodeData(encoder_input_, encoded_packet.get());
    leftover_encoder_samples_ = {};
  }

  // Copy unused samples into `encoder_input_`.
  if (!input_samples.empty()) {
    CHECK_LT(input_samples.size(), encoder_samples_needed_);
    const size_t used_space = leftover_encoder_samples_.size();
    encoder_input_.subspan(used_space, input_samples.size())
        .copy_from_nonoverlapping(input_samples);
    leftover_encoder_samples_ =
        encoder_input_.first(used_space + input_samples.size());
  }

  // Return nullptr if there's nothing in the packet.
  if (encoded_packet->data_size() == 0) {
    return nullptr;
  }

  return encoded_packet;
}

std::unique_ptr<AudioPacket> AudioEncoderOpus::EncodeInternalWithResampling(
    base::span<const int16_t> input_samples) {
  CHECK(needs_resampling_);
  CHECK(resampler_fifo_);

  // We always encode the full `encoder_samples_needed_` samples in
  // `encoder_input_` at once. Leftover samples are stored in `resampling_fifo_`
  // instead, at their original `kAltSamplingRate`.
  CHECK(leftover_encoder_samples_.empty());

  // Create a new packet of encoded data.
  auto encoded_packet = CreatePacket();

  // Add a reference to the incoming samples without copying them.
  resampler_fifo_->AddNewSamples(input_samples);

  while (resampler_fifo_->remaining_samples() >= resampling_samples_needed_) {
    resampler_->Resample(kOpusFrameCount, resampler_bus_.get());
    resampler_bus_->ToInterleaved<media::SignedInt16SampleTypeTraits>(
        encoder_input_);
    EncodeData(encoder_input_, encoded_packet.get());
  }

  // Save unused samples.
  resampler_fifo_->SaveNewSamples();

  // Return nullptr if there's nothing in the packet.
  if (encoded_packet->data_size() == 0) {
    return nullptr;
  }

  return encoded_packet;
}

}  // namespace remoting
