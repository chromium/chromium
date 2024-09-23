// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/client/audio/audio_player.h"

#include <cstdint>
#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kAudioSamplesPerFrame = 25;
const int kAudioSampleBytes = 4;
const int kAudioFrameBytes = kAudioSamplesPerFrame * kAudioSampleBytes;
const int kPaddingBytes = 16;

// TODO(nicholss): Update legacy audio player to use new audio buffer code.
// TODO(garykac): Generate random audio data in the tests rather than having
// a single constant value.
const uint8_t kDefaultBufferData = 0x5A;
const uint8_t kDummyAudioData = 0x8B;

}  // namespace

namespace remoting {

class FakeAudioPlayer : public AudioPlayer {
 public:
  FakeAudioPlayer() = default;

  bool ResetAudioPlayer(AudioPacket::SamplingRate) override { return true; }

  uint32_t GetSamplesPerFrame() override { return kAudioSamplesPerFrame; }
};

class AudioPlayerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    audio_ = std::make_unique<FakeAudioPlayer>();
    buffer_.reset(new char[kAudioFrameBytes + kPaddingBytes]);
  }

  void TearDown() override {}

  void ConsumeAudioFrame() {
    uint8_t* buffer = reinterpret_cast<uint8_t*>(buffer_.get());
    memset(buffer, kDefaultBufferData, kAudioFrameBytes + kPaddingBytes);
    AudioPlayer::AudioPlayerCallback(reinterpret_cast<void*>(buffer_.get()),
                                     kAudioFrameBytes,
                                     reinterpret_cast<void*>(audio_.get()));
    // Verify we haven't written beyond the end of the buffer.
    for (int i = 0; i < kPaddingBytes; i++)
      ASSERT_EQ(kDefaultBufferData, *(buffer + kAudioFrameBytes + i));
  }

  // Check that the first |num_bytes| bytes are filled with audio data and
  // the rest of the buffer is zero-filled.
  void CheckAudioFrameBytes(int num_bytes) {
    uint8_t* buffer = reinterpret_cast<uint8_t*>(buffer_.get());
    int i = 0;
    for (; i < num_bytes; i++) {
      ASSERT_EQ(kDummyAudioData, *(buffer + i));
    }
    // Rest of audio frame must be filled with '0's.
    for (; i < kAudioFrameBytes; i++) {
      ASSERT_EQ(0, *(buffer + i));
    }
  }

  int GetNumQueuedSamples() {
    return audio_->queued_bytes_ / kAudioSampleBytes;
  }

  int GetNumQueuedPackets() {
    return static_cast<int>(audio_->queued_packets_.size());
  }

  int GetBytesConsumed() { return static_cast<int>(audio_->bytes_consumed_); }

  std::unique_ptr<AudioPlayer> audio_;
  std::unique_ptr<char[]> buffer_;
};

std::unique_ptr<AudioPacket> CreatePacketWithSamplingRate(
    AudioPacket::SamplingRate rate,
    int samples) {
  std::unique_ptr<AudioPacket> packet(new AudioPacket());
  packet->set_encoding(AudioPacket::ENCODING_RAW);
  packet->set_sampling_rate(rate);
  packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
  packet->set_channels(AudioPacket::CHANNELS_STEREO);

  // The data must be a multiple of 4 bytes (channels x bytes_per_sample).
  std::string data;
  data.resize(samples * kAudioSampleBytes, kDummyAudioData);
  packet->add_data(data);

  return packet;
}

std::unique_ptr<AudioPacket> CreatePacket44100Hz(int samples) {
  return CreatePacketWithSamplingRate(AudioPacket::SAMPLING_RATE_44100,
                                      samples);
}

std::unique_ptr<AudioPacket> CreatePacket48000Hz(int samples) {
  return CreatePacketWithSamplingRate(AudioPacket::SAMPLING_RATE_48000,
                                      samples);
}

TEST_F(AudioPlayerTest, Init) {
  ASSERT_EQ(0, GetNumQueuedPackets());

  audio_->ProcessAudioPacket(CreatePacket44100Hz(10), {});
  ASSERT_EQ(1, GetNumQueuedPackets());
}

TEST_F(AudioPlayerTest, MultipleSamples) {
  audio_->ProcessAudioPacket(CreatePacket44100Hz(10), {});
  ASSERT_EQ(10, GetNumQueuedSamples());
  ASSERT_EQ(1, GetNumQueuedPackets());

  audio_->ProcessAudioPacket(CreatePacket44100Hz(20), {});
  ASSERT_EQ(30, GetNumQueuedSamples());
  ASSERT_EQ(2, GetNumQueuedPackets());
}

TEST_F(AudioPlayerTest, ChangeSampleRate) {
  audio_->ProcessAudioPacket(CreatePacket44100Hz(10), {});
  ASSERT_EQ(10, GetNumQueuedSamples());
  ASSERT_EQ(1, GetNumQueuedPackets());

  // New packet with different sampling rate causes previous samples to
  // be removed.
  audio_->ProcessAudioPacket(CreatePacket48000Hz(20), {});
  ASSERT_EQ(20, GetNumQueuedSamples());
  ASSERT_EQ(1, GetNumQueuedPackets());
}

TEST_F(AudioPlayerTest, ExceedLatency) {
  // Push about 4 seconds worth of samples.
  for (int i = 0; i < 100; ++i) {
    audio_->ProcessAudioPacket(CreatePacket48000Hz(2000), {});
  }

  // Verify that we don't have more than 0.5s.
  EXPECT_LT(GetNumQueuedSamples(), 24000);
}

// Incoming packets: 100
// Consume: 25 (w/ 75 remaining, offset 25 into packet)
TEST_F(AudioPlayerTest, ConsumePartialPacket) {
  int total_samples = 0;
  int bytes_consumed = 0;

  // Process 100 samples.
  int packet1_samples = 100;
  total_samples += packet1_samples;
  audio_->ProcessAudioPacket(CreatePacket44100Hz(packet1_samples), {});
  ASSERT_EQ(total_samples, GetNumQueuedSamples());
  ASSERT_EQ(1, GetNumQueuedPackets());
  ASSERT_EQ(bytes_consumed, GetBytesConsumed());

  // Consume one frame (=25) of samples.
  ConsumeAudioFrame();
  total_samples -= kAudioSamplesPerFrame;
  bytes_consumed += kAudioFrameBytes;
  ASSERT_EQ(total_samples, GetNumQueuedSamples());
  ASSERT_EQ(1, GetNumQueuedPackets());
  ASSERT_EQ(bytes_consumed, GetBytesConsumed());
  CheckAudioFrameBytes(kAudioFrameBytes);

  // Remaining samples.
  ASSERT_EQ(75, total_samples);
  ASSERT_EQ(25 * kAudioSampleBytes, bytes_consumed);
}

// Incoming packets: 20, 70
// Consume: 25, 25 (w/ 40 remaining, offset 30 into packet)
TEST_F(AudioPlayerTest, ConsumeAcrossPackets) {
  int total_samples = 0;
  int bytes_consumed = 0;

  // Packet 1.
  int packet1_samples = 20;
  total_samples += packet1_samples;
  audio_->ProcessAudioPacket(CreatePacket44100Hz(packet1_samples), {});
  ASSERT_EQ(total_samples, GetNumQueuedSamples());

  // Packet 2.
  int packet2_samples = 70;
  total_samples += packet2_samples;
  audio_->ProcessAudioPacket(CreatePacket44100Hz(packet2_samples), {});
  ASSERT_EQ(total_samples, GetNumQueuedSamples());
  ASSERT_EQ(bytes_consumed, GetBytesConsumed());

  // Consume 1st frame of 25 samples.
  // This will consume the entire 1st packet.
  ConsumeAudioFrame();
  total_samples -= kAudioSamplesPerFrame;
  bytes_consumed += kAudioFrameBytes - (packet1_samples * kAudioSampleBytes);
  ASSERT_EQ(total_samples, GetNumQueuedSamples());
  ASSERT_EQ(1, GetNumQueuedPackets());
  ASSERT_EQ(bytes_consumed, GetBytesConsumed());
  CheckAudioFrameBytes(kAudioFrameBytes);

  // Consume 2nd frame of 25 samples.
  ConsumeAudioFrame();
  total_samples -= kAudioSamplesPerFrame;
  bytes_consumed += kAudioFrameBytes;
  ASSERT_EQ(total_samples, GetNumQueuedSamples());
  ASSERT_EQ(1, GetNumQueuedPackets());
  ASSERT_EQ(bytes_consumed, GetBytesConsumed());
  CheckAudioFrameBytes(kAudioFrameBytes);

  // Remaining samples.
  ASSERT_EQ(40, total_samples);
  ASSERT_EQ(30 * kAudioSampleBytes, bytes_consumed);
}

// Incoming packets: 50, 30
// Consume: 25, 25, 25 (w/ 5 remaining, offset 25 into packet)
TEST_F(AudioPlayerTest, ConsumeEntirePacket) {
  int total_samples = 0;
  int bytes_consumed = 0;

  // Packet 1.
  int packet1_samples = 50;
  total_samples += packet1_samples;
  audio_->ProcessAudioPacket(CreatePacket44100Hz(packet1_samples), {});
  ASSERT_EQ(total_samples, GetNumQueuedSamples());
  ASSERT_EQ(bytes_consumed, GetBytesConsumed());

  // Packet 2.
  int packet2_samples = 30;
  total_samples += packet2_samples;
  audio_->ProcessAudioPacket(CreatePacket44100Hz(packet2_samples), {});
  ASSERT_EQ(total_samples, GetNumQueuedSamples());
  ASSERT_EQ(bytes_consumed, GetBytesConsumed());

  // Consume 1st frame of 25 samples.
  ConsumeAudioFrame();
  total_samples -= kAudioSamplesPerFrame;
  bytes_consumed += kAudioFrameBytes;
  ASSERT_EQ(total_samples, GetNumQueuedSamples());
  ASSERT_EQ(2, GetNumQueuedPackets());
  ASSERT_EQ(bytes_consumed, GetBytesConsumed());
  CheckAudioFrameBytes(kAudioFrameBytes);

  // Consume 2nd frame of 25 samples.
  // This will consume the entire first packet (exactly), but the entry for
  // this packet will stick around (empty) until the next audio chunk is
  // consumed.
  ConsumeAudioFrame();
  total_samples -= kAudioSamplesPerFrame;
  bytes_consumed += kAudioFrameBytes;
  ASSERT_EQ(total_samples, GetNumQueuedSamples());
  ASSERT_EQ(2, GetNumQueuedPackets());
  ASSERT_EQ(bytes_consumed, GetBytesConsumed());
  CheckAudioFrameBytes(kAudioFrameBytes);

  // Consume 3rd frame of 25 samples.
  ConsumeAudioFrame();
  total_samples -= kAudioSamplesPerFrame;
  bytes_consumed += kAudioFrameBytes - (packet1_samples * kAudioSampleBytes);
  ASSERT_EQ(total_samples, GetNumQueuedSamples());
  ASSERT_EQ(1, GetNumQueuedPackets());
  ASSERT_EQ(bytes_consumed, GetBytesConsumed());
  CheckAudioFrameBytes(kAudioFrameBytes);

  // Remaining samples.
  ASSERT_EQ(5, total_samples);
  ASSERT_EQ(25 * kAudioSampleBytes, bytes_consumed);
}

// Incoming packets: <none>
// Consume: 25
TEST_F(AudioPlayerTest, NoDataToConsume) {
  // Attempt to consume a frame of 25 samples.
  ConsumeAudioFrame();
  ASSERT_EQ(0, GetNumQueuedSamples());
  ASSERT_EQ(0, GetNumQueuedPackets());
  ASSERT_EQ(0, GetBytesConsumed());
  CheckAudioFrameBytes(0);
}

// Incoming packets: 10
// Consume: 25
TEST_F(AudioPlayerTest, NotEnoughDataToConsume) {
  int total_samples = 0;
  int bytes_consumed = 0;

  // Packet 1.
  int packet1_samples = 10;
  total_samples += packet1_samples;
  audio_->ProcessAudioPacket(CreatePacket44100Hz(packet1_samples), {});
  ASSERT_EQ(total_samples, GetNumQueuedSamples());
  ASSERT_EQ(bytes_consumed, GetBytesConsumed());

  // Attempt to consume a frame of 25 samples.
  ConsumeAudioFrame();
  ASSERT_EQ(0, GetNumQueuedSamples());
  ASSERT_EQ(0, GetNumQueuedPackets());
  ASSERT_EQ(0, GetBytesConsumed());
  CheckAudioFrameBytes(packet1_samples * kAudioSampleBytes);
}

}  // namespace remoting
