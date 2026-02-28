// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/audio_encoder_opus.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <memory>
#include <numbers>
#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "remoting/codec/audio_decoder_opus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

// Maximum value that can be encoded in a 16-bit signed sample.
const int kMaxSampleValue = 32767;

// Phase shift between left and right channels.
const double kChannelPhaseShift = 2 * std::numbers::pi / 3;

// The sampling rate that OPUS uses internally and that we expect to get
// from the decoder.
const AudioPacket_SamplingRate kDefaultSamplingRate =
    AudioPacket::SAMPLING_RATE_48000;

// Maximum latency expected from the encoder.
const int kMaxLatencyMs = 40;

// When verifying results ignore the first 1k samples. This is necessary because
// it takes some time for the codec to adjust for the input signal.
const int kSkippedFirstSamples = 1000;

// Maximum standard deviation of the difference between original and decoded
// signals as a proportion of kMaxSampleValue. For two unrelated signals this
// difference will be close to 1.0, even for signals that differ only slightly.
// The value is chosen such that all the tests pass normally, but fail with
// small changes (e.g. one sample shift between signals).
const double kMaxSignalDeviation = 0.1;

}  // namespace

class OpusAudioEncoderTest : public testing::Test {
 public:
  // Return test signal value at the specified position |pos|. |frequency_hz|
  // defines frequency of the signal. |channel| is used to calculate phase shift
  // of the signal, so that different signals are generated for left and right
  // channels.
  static int16_t GetSampleValue(AudioPacket::SamplingRate rate,
                                double frequency_hz,
                                double pos,
                                int channel) {
    double angle =
        pos * 2 * std::numbers::pi * frequency_hz / static_cast<double>(rate) +
        kChannelPhaseShift * channel;
    return static_cast<int>(std::sin(angle) * kMaxSampleValue + 0.5);
  }

  // Creates  audio packet filled with a test signal with the specified
  // |frequency_hz|.
  std::unique_ptr<AudioPacket> CreatePacket(int samples,
                                            AudioPacket::SamplingRate rate,
                                            double frequency_hz,
                                            int pos,
                                            int channels) {
    std::vector<int16_t> data(samples * channels);
    for (int i = 0; i < samples; ++i) {
      for (int j = 0; j < channels; ++j) {
        data[i * channels + j] = GetSampleValue(rate, frequency_hz, i + pos, j);
      }
    }

    std::unique_ptr<AudioPacket> packet(new AudioPacket());
    packet->add_data(reinterpret_cast<char*>(data.data()),
                     samples * channels * sizeof(int16_t));
    packet->set_encoding(AudioPacket::ENCODING_RAW);
    packet->set_sampling_rate(rate);
    packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
    packet->set_channels(static_cast<AudioPacket::Channels>(channels));
    return packet;
  }

  // Decoded data is normally shifted in phase relative to the original signal.
  // This function returns the approximate shift in samples by finding the first
  // point when signal goes from negative to positive.
  double EstimateSignalShift(const std::vector<int16_t>& received_data,
                             int channels) {
    for (size_t i = kSkippedFirstSamples;
         i < received_data.size() / channels - 1; i++) {
      int16_t this_sample = received_data[i * channels];
      int16_t next_sample = received_data[(i + 1) * channels];
      if (this_sample < 0 && next_sample > 0) {
        return i +
               static_cast<double>(-this_sample) / (next_sample - this_sample);
      }
    }
    return 0;
  }

  // Compares decoded signal with the test signal that was encoded. It estimates
  // phase shift from the original signal, then calculates standard deviation of
  // the difference between original and decoded signals.
  void ValidateReceivedData(int samples,
                            AudioPacket::SamplingRate rate,
                            double frequency_hz,
                            const std::vector<int16_t>& received_data,
                            int channels) {
    double shift = EstimateSignalShift(received_data, channels);
    double diff_sqare_sum = 0;
    for (size_t i = kSkippedFirstSamples; i < received_data.size() / channels;
         i++) {
      for (int j = 0; j < channels; ++j) {
        double d = received_data[i * channels + j] -
                   GetSampleValue(rate, frequency_hz, i - shift, j);
        diff_sqare_sum += d * d;
      }
    }
    double deviation =
        std::sqrt(diff_sqare_sum / received_data.size()) / kMaxSampleValue;
    LOG(ERROR) << "Decoded signal deviation: " << deviation;
    EXPECT_LE(deviation, kMaxSignalDeviation);
  }

  void TestEncodeDecode(int packet_size,
                        double frequency_hz,
                        AudioPacket::SamplingRate rate,
                        int channels = 2) {
    const int kTotalTestSamples = 24000;

    encoder_ = std::make_unique<AudioEncoderOpus>();
    decoder_ = std::make_unique<AudioDecoderOpus>();

    std::vector<int16_t> received_data;
    int pos = 0;
    for (; pos < kTotalTestSamples; pos += packet_size) {
      std::unique_ptr<AudioPacket> source_packet =
          CreatePacket(packet_size, rate, frequency_hz, pos, channels);
      std::unique_ptr<AudioPacket> encoded =
          encoder_->Encode(std::move(source_packet));
      if (encoded.get()) {
        std::unique_ptr<AudioPacket> decoded =
            decoder_->Decode(std::move(encoded));
        EXPECT_EQ(kDefaultSamplingRate, decoded->sampling_rate());
        for (int i = 0; i < decoded->data_size(); ++i) {
          const int16_t* data = UNSAFE_TODO(
              reinterpret_cast<const int16_t*>(decoded->data(i).data()));
          received_data.insert(
              received_data.end(), data,
              UNSAFE_TODO(data + decoded->data(i).size() / sizeof(int16_t)));
        }
      }
    }

    // Verify that at most kMaxLatencyMs worth of samples is buffered inside
    // |encoder_| and |decoder_|.
    EXPECT_GE(static_cast<int>(received_data.size()) / channels,
              pos - rate * kMaxLatencyMs / 1000);

    ValidateReceivedData(packet_size, kDefaultSamplingRate, frequency_hz,
                         received_data, channels);
  }

 protected:
  std::unique_ptr<AudioEncoderOpus> encoder_;
  std::unique_ptr<AudioDecoderOpus> decoder_;
};

TEST_F(OpusAudioEncoderTest, CreateAndDestroy) {}

TEST_F(OpusAudioEncoderTest, NoResampling) {
  TestEncodeDecode(2000, 50, AudioPacket::SAMPLING_RATE_48000);
  TestEncodeDecode(2000, 3000, AudioPacket::SAMPLING_RATE_48000);
  TestEncodeDecode(2000, 10000, AudioPacket::SAMPLING_RATE_48000);
}

TEST_F(OpusAudioEncoderTest, Resampling) {
  TestEncodeDecode(2000, 50, AudioPacket::SAMPLING_RATE_44100);
  TestEncodeDecode(2000, 3000, AudioPacket::SAMPLING_RATE_44100);
  TestEncodeDecode(2000, 10000, AudioPacket::SAMPLING_RATE_44100);
}

TEST_F(OpusAudioEncoderTest, BufferSizeAndResampling) {
  TestEncodeDecode(500, 3000, AudioPacket::SAMPLING_RATE_44100);
  TestEncodeDecode(1000, 3000, AudioPacket::SAMPLING_RATE_44100);
  TestEncodeDecode(5000, 3000, AudioPacket::SAMPLING_RATE_44100);
}

TEST_F(OpusAudioEncoderTest, Mono) {
  TestEncodeDecode(2000, 3000, AudioPacket::SAMPLING_RATE_48000, 1);
}

TEST_F(OpusAudioEncoderTest, DynamicConfigChange) {
  encoder_ = std::make_unique<AudioEncoderOpus>();
  decoder_ = std::make_unique<AudioDecoderOpus>();

  auto test_config = [&](int samples, AudioPacket::SamplingRate rate,
                         int channels) {
    std::unique_ptr<AudioPacket> source_packet =
        CreatePacket(samples, rate, 3000, 0, channels);
    std::unique_ptr<AudioPacket> encoded =
        encoder_->Encode(std::move(source_packet));
    // It might take multiple packets to get output due to buffering,
    // but here we just want to ensure it doesn't crash and eventually
    // produces something or handles the reset.
    if (encoded) {
      std::unique_ptr<AudioPacket> decoded =
          decoder_->Decode(std::move(encoded));
      EXPECT_EQ(kDefaultSamplingRate, decoded->sampling_rate());
      EXPECT_EQ(channels, decoded->channels());
    }
  };

  // Switch between various configs.
  test_config(2000, AudioPacket::SAMPLING_RATE_48000, 2);
  test_config(2000, AudioPacket::SAMPLING_RATE_44100, 2);
  test_config(2000, AudioPacket::SAMPLING_RATE_48000, 1);
  test_config(2000, AudioPacket::SAMPLING_RATE_44100, 1);
}

TEST_F(OpusAudioEncoderTest, UnsupportedParameters) {
  encoder_ = std::make_unique<AudioEncoderOpus>();

  // 3 channels (Unsupported).
  auto packet =
      CreatePacket(2000, AudioPacket::SAMPLING_RATE_48000, 3000, 0, 3);
  EXPECT_FALSE(encoder_->Encode(std::move(packet)));

  // Unsupported sampling rate.
  packet = CreatePacket(2000, static_cast<AudioPacket::SamplingRate>(8000),
                        3000, 0, 2);
  EXPECT_FALSE(encoder_->Encode(std::move(packet)));
}

TEST_F(OpusAudioEncoderTest, SmallPackets) {
  encoder_ = std::make_unique<AudioEncoderOpus>();

  // Send 10ms of 48kHz audio (480 samples). Opus frame is 20ms.
  auto packet = CreatePacket(480, AudioPacket::SAMPLING_RATE_48000, 3000, 0, 2);
  auto encoded = encoder_->Encode(std::move(packet));
  EXPECT_FALSE(encoded);  // Should be buffered.

  // Send another 10ms.
  packet = CreatePacket(480, AudioPacket::SAMPLING_RATE_48000, 3000, 480, 2);
  encoded = encoder_->Encode(std::move(packet));
  EXPECT_TRUE(encoded);  // Now we should have a full 20ms frame.
}

TEST_F(OpusAudioEncoderTest, SmallPacketsWithResampling) {
  encoder_ = std::make_unique<AudioEncoderOpus>();

  // Send 10ms of 44.1kHz audio (441 samples). Opus frame is 20ms.
  auto packet = CreatePacket(441, AudioPacket::SAMPLING_RATE_44100, 3000, 0, 2);
  auto encoded = encoder_->Encode(std::move(packet));
  EXPECT_FALSE(encoded);  // Should be buffered.

  // Send another 30ms (1323 samples). Total 40ms.
  packet = CreatePacket(1323, AudioPacket::SAMPLING_RATE_44100, 3000, 441, 2);
  encoded = encoder_->Encode(std::move(packet));
  EXPECT_TRUE(encoded);
  // We should have at least one encoded chunk.
  EXPECT_GE(encoded->data_size(), 1);
}

TEST_F(OpusAudioEncoderTest, GetBitrate) {
  encoder_ = std::make_unique<AudioEncoderOpus>();
  EXPECT_EQ(encoder_->GetBitrate(), 160 * 1024);
}

// Makes sure that the FIFO returns spans from the new samples, without copying
// data.
TEST_F(OpusAudioEncoderTest, ResamplerFifo_TakeChunkDirect) {
  auto fifo = AudioEncoderOpus::GetEmptyFifoForTesting(2048, 2);
  ASSERT_TRUE(fifo);
  const size_t chunk_size = fifo->GetChunkSizeForTesting();

  std::vector<int16_t> samples(chunk_size, 42);
  fifo->AddNewSamples(samples);
  EXPECT_EQ(fifo->remaining_samples(), chunk_size);

  auto chunk = fifo->TakeChunk();
  // Make sure we haven't copied any data internally.
  EXPECT_EQ(chunk.data(), samples.data());

  EXPECT_EQ(chunk.size(), chunk_size);
  EXPECT_EQ(chunk[0], 42);
  EXPECT_EQ(fifo->remaining_samples(), 0u);

  fifo.reset();
}

// Make sure that `SaveNewSamples()` copies new samples to internal storage.
TEST_F(OpusAudioEncoderTest, ResamplerFifo_SaveNewSamples) {
  auto fifo = AudioEncoderOpus::GetEmptyFifoForTesting(2048, 2);
  const size_t chunk_size = fifo->GetChunkSizeForTesting();

  {
    std::vector<int16_t> data = {1, 2, 3, 4};
    fifo->AddNewSamples(data);
    EXPECT_EQ(fifo->remaining_samples(), 4u);

    // This copies `data` into the FIFO's internal storage.
    fifo->SaveNewSamples();
  }
  // `data` is now destroyed.

  EXPECT_EQ(fifo->remaining_samples(), 4u);

  // Verify the data is still there by completing a chunk and taking it.
  std::vector<int16_t> part2(chunk_size - 4, 7);
  fifo->AddNewSamples(part2);
  auto chunk = fifo->TakeChunk();
  EXPECT_EQ(chunk.size(), chunk_size);
  EXPECT_EQ(chunk[0], 1);
  EXPECT_EQ(chunk[3], 4);
  EXPECT_EQ(chunk[4], 7);

  fifo.reset();
}

// Make sure that we can receive a mix of saved and new samples.
TEST_F(OpusAudioEncoderTest, ResamplerFifo_TakeChunkCrossover) {
  auto fifo = AudioEncoderOpus::GetEmptyFifoForTesting(2048, 2);
  const size_t chunk_size = fifo->GetChunkSizeForTesting();

  // Add some samples and compact.
  std::vector<int16_t> samples_to_save = {1, 2, 3, 4};
  fifo->AddNewSamples(samples_to_save);
  fifo->SaveNewSamples();

  // Add more samples to complete a chunk.
  std::vector<int16_t> large_chunks(chunk_size * 2, 7);
  fifo->AddNewSamples(large_chunks);

  // TakeChunk should now use the crossover buffer.
  auto chunk = fifo->TakeChunk();
  EXPECT_EQ(chunk.size(), chunk_size);
  EXPECT_EQ(chunk[0], 1);
  EXPECT_EQ(chunk[3], 4);
  EXPECT_EQ(chunk[4], 7);

  // Make sure we can pull remaining samples from the remaining `large_chunks`.
  auto chunk_from_new_samples = fifo->TakeChunk();
  EXPECT_EQ(chunk_from_new_samples.size(), chunk_size);

  fifo.reset();
}

TEST_F(OpusAudioEncoderTest, ResamplerFifo_TakeChunkFromSavedSamples) {
  auto fifo = AudioEncoderOpus::GetEmptyFifoForTesting(2048, 2);
  const size_t chunk_size = fifo->GetChunkSizeForTesting();

  // Add more than a chunk and compact.
  std::vector<int16_t> data(chunk_size + 10, 42);
  fifo->AddNewSamples(data);
  fifo->SaveNewSamples();

  // TakeChunk should pull from the compacted buffer.
  auto chunk = fifo->TakeChunk();
  EXPECT_EQ(chunk.size(), chunk_size);
  EXPECT_EQ(fifo->remaining_samples(), 10u);

  fifo.reset();
}

// Makes sure that
TEST_F(OpusAudioEncoderTest, ResamplerFifo_SaveLargeChunks) {
  const size_t kFifoSize = 2048;
  auto fifo = AudioEncoderOpus::GetEmptyFifoForTesting(kFifoSize, 2);

  // Completely fill the FIFO.
  std::vector<int16_t> max_capacity(kFifoSize, 42);
  fifo->AddNewSamples(max_capacity);
  fifo->SaveNewSamples();

  // Add more samples that can fit
  std::vector<int16_t> huge_chunk(kFifoSize * 2, 42);
  fifo->AddNewSamples(huge_chunk);

  EXPECT_GT(fifo->remaining_samples(), kFifoSize);

  while (fifo->remaining_samples() > kFifoSize) {
    std::ignore = fifo->TakeChunk();
  }

  // Make sure we can save the data after consuming enough of it.
  fifo->SaveNewSamples();

  fifo.reset();
}

}  // namespace remoting
