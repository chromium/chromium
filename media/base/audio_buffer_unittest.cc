// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <limits>
#include <memory>

#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

constexpr int kSampleRate = 4800;

enum class ValueType { kNormal, kFloat };
static void VerifyBusWithOffset(AudioBus* bus,
                                int offset,
                                int frames,
                                float start,
                                float start_offset,
                                float increment,
                                ValueType type = ValueType::kNormal) {
  for (int ch = 0; ch < bus->channels(); ++ch) {
    const float v = start_offset + start + ch * bus->frames() * increment;
    for (int i = offset; i < offset + frames; ++i) {
      float expected_value = v + i * increment;
      if (type == ValueType::kFloat)
        expected_value /= std::numeric_limits<uint16_t>::max();
      ASSERT_FLOAT_EQ(expected_value, bus->channel(ch)[i])
          << "i=" << i << ", ch=" << ch;
    }
  }
}

class TestExternalMemory : public media::AudioBuffer::ExternalMemory {
 public:
  explicit TestExternalMemory(std::vector<uint8_t> contents)
      : contents_(std::move(contents)) {
    span_ = base::span<uint8_t>(contents_.data(), contents_.size());
  }

 private:
  std::vector<uint8_t> contents_;
};

static std::vector<float*> WrapChannelsAsVector(AudioBus* bus) {
  std::vector<float*> channels(bus->channels());
  for (size_t ch = 0; ch < channels.size(); ++ch)
    channels[ch] = bus->channel(ch);

  return channels;
}

static void VerifyBus(AudioBus* bus,
                      int frames,
                      float start,
                      float increment,
                      ValueType type = ValueType::kNormal) {
  VerifyBusWithOffset(bus, 0, frames, start, 0, increment, type);
}

static void TrimRangeTest(SampleFormat sample_format) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = kSampleRate / 10;
  const base::TimeDelta timestamp = base::TimeDelta();
  const base::TimeDelta duration = base::Milliseconds(100);
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<float>(sample_format,
                                                             channel_layout,
                                                             channels,
                                                             kSampleRate,
                                                             0,
                                                             1,
                                                             frames,
                                                             timestamp);
  EXPECT_EQ(frames, buffer->frame_count());
  EXPECT_EQ(timestamp, buffer->timestamp());
  EXPECT_EQ(duration, buffer->duration());

  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);

  // Verify all frames before trimming.
  buffer->ReadFrames(frames, 0, 0, bus.get());
  VerifyBus(bus.get(), frames, 0, 1, ValueType::kFloat);

  // Trim 10ms of frames from the middle of the buffer.
  int trim_start = frames / 2;
  const int trim_length = kSampleRate / 100;
  const base::TimeDelta trim_duration = base::Milliseconds(10);
  buffer->TrimRange(trim_start, trim_start + trim_length);
  EXPECT_EQ(frames - trim_length, buffer->frame_count());
  EXPECT_EQ(timestamp, buffer->timestamp());
  EXPECT_EQ(duration - trim_duration, buffer->duration());
  bus->Zero();
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), trim_start, 0, 1, ValueType::kFloat);
  VerifyBusWithOffset(bus.get(), trim_start, buffer->frame_count() - trim_start,
                      0, trim_length, 1, ValueType::kFloat);

  // Trim 10ms of frames from the start, which just adjusts the buffer's
  // internal start offset.
  buffer->TrimStart(trim_length);
  trim_start -= trim_length;
  EXPECT_EQ(frames - 2 * trim_length, buffer->frame_count());
  EXPECT_EQ(timestamp, buffer->timestamp());
  EXPECT_EQ(duration - 2 * trim_duration, buffer->duration());
  bus->Zero();
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), trim_start, trim_length, 1, ValueType::kFloat);
  VerifyBusWithOffset(bus.get(), trim_start, buffer->frame_count() - trim_start,
                      trim_length, trim_length, 1, ValueType::kFloat);

  // Trim 10ms of frames from the end, which just adjusts the buffer's frame
  // count.
  buffer->TrimEnd(trim_length);
  EXPECT_EQ(frames - 3 * trim_length, buffer->frame_count());
  EXPECT_EQ(timestamp, buffer->timestamp());
  EXPECT_EQ(duration - 3 * trim_duration, buffer->duration());
  bus->Zero();
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), trim_start, trim_length, 1, ValueType::kFloat);
  VerifyBusWithOffset(bus.get(), trim_start, buffer->frame_count() - trim_start,
                      trim_length, trim_length, 1, ValueType::kFloat);

  // Trim another 10ms from the inner portion of the buffer.
  buffer->TrimRange(trim_start, trim_start + trim_length);
  EXPECT_EQ(frames - 4 * trim_length, buffer->frame_count());
  EXPECT_EQ(timestamp, buffer->timestamp());
  EXPECT_EQ(duration - 4 * trim_duration, buffer->duration());
  bus->Zero();
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), trim_start, trim_length, 1, ValueType::kFloat);
  VerifyBusWithOffset(bus.get(), trim_start, buffer->frame_count() - trim_start,
                      trim_length, trim_length * 2, 1, ValueType::kFloat);

  // Trim off the end using TrimRange() to ensure end index is exclusive.
  buffer->TrimRange(buffer->frame_count() - trim_length, buffer->frame_count());
  EXPECT_EQ(frames - 5 * trim_length, buffer->frame_count());
  EXPECT_EQ(timestamp, buffer->timestamp());
  EXPECT_EQ(duration - 5 * trim_duration, buffer->duration());
  bus->Zero();
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), trim_start, trim_length, 1, ValueType::kFloat);
  VerifyBusWithOffset(bus.get(), trim_start, buffer->frame_count() - trim_start,
                      trim_length, trim_length * 2, 1, ValueType::kFloat);

  // Trim off the start using TrimRange() to ensure start index is inclusive.
  buffer->TrimRange(0, trim_length);
  trim_start -= trim_length;
  EXPECT_EQ(frames - 6 * trim_length, buffer->frame_count());
  EXPECT_EQ(timestamp, buffer->timestamp());
  EXPECT_EQ(duration - 6 * trim_duration, buffer->duration());
  bus->Zero();
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), trim_start, 2 * trim_length, 1, ValueType::kFloat);
  VerifyBusWithOffset(bus.get(), trim_start, buffer->frame_count() - trim_start,
                      trim_length * 2, trim_length * 2, 1, ValueType::kFloat);
}

TEST(AudioBufferTest, CopyFrom) {
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_MONO;
  scoped_refptr<AudioBuffer> original_buffer = MakeAudioBuffer<uint8_t>(
      kSampleFormatU8, kChannelLayout,
      ChannelLayoutToChannelCount(kChannelLayout), kSampleRate, 1, 1,
      kSampleRate / 100, base::TimeDelta());
  scoped_refptr<AudioBuffer> new_buffer =
      AudioBuffer::CopyFrom(kSampleFormatU8,
                            original_buffer->channel_layout(),
                            original_buffer->channel_count(),
                            original_buffer->sample_rate(),
                            original_buffer->frame_count(),
                            &original_buffer->channel_data()[0],
                            original_buffer->timestamp());
  EXPECT_EQ(original_buffer->frame_count(), new_buffer->frame_count());
  EXPECT_EQ(original_buffer->timestamp(), new_buffer->timestamp());
  EXPECT_EQ(original_buffer->duration(), new_buffer->duration());
  EXPECT_EQ(original_buffer->sample_rate(), new_buffer->sample_rate());
  EXPECT_EQ(original_buffer->channel_count(), new_buffer->channel_count());
  EXPECT_EQ(original_buffer->channel_layout(), new_buffer->channel_layout());
  EXPECT_FALSE(original_buffer->end_of_stream());
}

TEST(AudioBufferTest, CopyFromAudioBus) {
  const int kChannelCount = 2;
  const int kFrameCount = kSampleRate / 100;

  // For convenience's sake, create an arbitrary |temp_buffer| and copy it to
  // |audio_bus|, instead of generating data in |audio_bus| ourselves.
  scoped_refptr<AudioBuffer> temp_buffer = MakeAudioBuffer<uint8_t>(
      kSampleFormatU8, CHANNEL_LAYOUT_STEREO, kChannelCount, kSampleRate, 1, 1,
      kFrameCount, base::TimeDelta());

  auto audio_bus = media::AudioBus::Create(kChannelCount, kFrameCount);
  temp_buffer->ReadFrames(kFrameCount, 0, 0, audio_bus.get());

  const base::TimeDelta kTimestamp = base::Milliseconds(123);

  auto audio_buffer_from_bus =
      media::AudioBuffer::CopyFrom(kSampleRate, kTimestamp, audio_bus.get());

  EXPECT_EQ(audio_buffer_from_bus->channel_count(), audio_bus->channels());
  EXPECT_EQ(audio_buffer_from_bus->channel_layout(),
            GuessChannelLayout(kChannelCount));
  EXPECT_EQ(audio_buffer_from_bus->frame_count(), audio_bus->frames());
  EXPECT_EQ(audio_buffer_from_bus->timestamp(), kTimestamp);
  EXPECT_EQ(audio_buffer_from_bus->sample_rate(), kSampleRate);
  EXPECT_EQ(audio_buffer_from_bus->sample_format(),
            SampleFormat::kSampleFormatPlanarF32);
  EXPECT_FALSE(audio_buffer_from_bus->end_of_stream());

  for (int ch = 0; ch < kChannelCount; ++ch) {
    const float* bus_data = audio_bus->channel(ch);
    const float* buffer_data = reinterpret_cast<const float*>(
        audio_buffer_from_bus->channel_data()[ch]);

    for (int i = 0; i < kFrameCount; ++i)
      EXPECT_EQ(buffer_data[i], bus_data[i]);
  }
}

TEST(AudioBufferTest, CopyBitstreamFrom) {
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
  const int kChannelCount = ChannelLayoutToChannelCount(kChannelLayout);
  const int kFrameCount = 128;
  const uint8_t kTestData[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                               11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                               22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
  const base::TimeDelta kTimestamp = base::Microseconds(1337);
  const uint8_t* const data[] = {kTestData};

  scoped_refptr<AudioBuffer> buffer = AudioBuffer::CopyBitstreamFrom(
      kSampleFormatAc3, kChannelLayout, kChannelCount, kSampleRate, kFrameCount,
      data, sizeof(kTestData), kTimestamp);

  EXPECT_EQ(kChannelLayout, buffer->channel_layout());
  EXPECT_EQ(kFrameCount, buffer->frame_count());
  EXPECT_EQ(kSampleRate, buffer->sample_rate());
  EXPECT_EQ(kFrameCount, buffer->frame_count());
  EXPECT_EQ(kTimestamp, buffer->timestamp());
  EXPECT_TRUE(buffer->IsBitstreamFormat());
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST(AudioBufferTest, CreateBitstreamBuffer) {
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
  const int kChannelCount = ChannelLayoutToChannelCount(kChannelLayout);
  const int kFrameCount = 128;
  const int kDataSize = 32;

  scoped_refptr<AudioBuffer> buffer = AudioBuffer::CreateBitstreamBuffer(
      kSampleFormatAc3, kChannelLayout, kChannelCount, kSampleRate, kFrameCount,
      kDataSize);

  EXPECT_EQ(kChannelLayout, buffer->channel_layout());
  EXPECT_EQ(kFrameCount, buffer->frame_count());
  EXPECT_EQ(kSampleRate, buffer->sample_rate());
  EXPECT_EQ(kFrameCount, buffer->frame_count());
  EXPECT_EQ(kNoTimestamp, buffer->timestamp());
  EXPECT_TRUE(buffer->IsBitstreamFormat());
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST(AudioBufferTest, CopyBitstreamFromIECDts) {
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
  const int kChannelCount = ChannelLayoutToChannelCount(kChannelLayout);
  constexpr int kFrameCount = 512;
  constexpr uint8_t kTestData[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                   11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                                   22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
  const base::TimeDelta kTimestamp = base::Microseconds(1337);
  const uint8_t* const data[] = {kTestData};

  scoped_refptr<AudioBuffer> buffer = AudioBuffer::CopyBitstreamFrom(
      kSampleFormatIECDts, kChannelLayout, kChannelCount, kSampleRate,
      kFrameCount, data, sizeof(kTestData), kTimestamp);

  EXPECT_EQ(kChannelLayout, buffer->channel_layout());
  EXPECT_EQ(kFrameCount, buffer->frame_count());
  EXPECT_EQ(kSampleRate, buffer->sample_rate());
  EXPECT_EQ(kFrameCount, buffer->frame_count());
  EXPECT_EQ(kTimestamp, buffer->timestamp());
  EXPECT_TRUE(buffer->IsBitstreamFormat());
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST(AudioBufferTest, WrapExternalMemory) {
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
  const int kChannelCount = 2;
  const int kFrameCount = 10;
  const base::TimeDelta kTimestamp = base::Microseconds(1337);

  std::vector<uint8_t> test_data;
  test_data.insert(test_data.end(), kFrameCount, 1);
  test_data.insert(test_data.end(), kFrameCount, 2);
  uint8_t* first_channel_ptr = test_data.data();
  uint8_t* second_channel_ptr = test_data.data() + kFrameCount;

  auto external_memory =
      std::make_unique<TestExternalMemory>(std::move(test_data));
  auto buffer = AudioBuffer::CreateFromExternalMemory(
      kSampleFormatPlanarU8, kChannelLayout, kChannelCount, kSampleRate,
      kFrameCount, kTimestamp, std::move(external_memory));

  EXPECT_EQ(kChannelLayout, buffer->channel_layout());
  EXPECT_EQ(kSampleRate, buffer->sample_rate());
  EXPECT_EQ(kFrameCount, buffer->frame_count());
  EXPECT_EQ(kChannelCount, buffer->channel_count());
  EXPECT_EQ(static_cast<size_t>(kChannelCount), buffer->channel_data().size());
  EXPECT_EQ(kTimestamp, buffer->timestamp());
  EXPECT_FALSE(buffer->end_of_stream());

  EXPECT_EQ(buffer->channel_data()[0], first_channel_ptr);
  EXPECT_EQ(buffer->channel_data()[1], second_channel_ptr);
}

TEST(AudioBufferTest, CreateBitstreamBufferIECDts) {
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_MONO;
  const int kChannelCount = ChannelLayoutToChannelCount(kChannelLayout);
  const int kFrameCount = 512;
  const int kDataSize = 2048;

  scoped_refptr<AudioBuffer> buffer = AudioBuffer::CreateBitstreamBuffer(
      kSampleFormatIECDts, kChannelLayout, kChannelCount, kSampleRate,
      kFrameCount, kDataSize);

  EXPECT_EQ(kChannelLayout, buffer->channel_layout());
  EXPECT_EQ(kFrameCount, buffer->frame_count());
  EXPECT_EQ(kSampleRate, buffer->sample_rate());
  EXPECT_EQ(kFrameCount, buffer->frame_count());
  EXPECT_EQ(kNoTimestamp, buffer->timestamp());
  EXPECT_TRUE(buffer->IsBitstreamFormat());
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST(AudioBufferTest, CreateEOSBuffer) {
  scoped_refptr<AudioBuffer> buffer = AudioBuffer::CreateEOSBuffer();
  EXPECT_TRUE(buffer->end_of_stream());
}

TEST(AudioBufferTest, FrameSize) {
  const uint8_t kTestData[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                               11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                               22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
  const base::TimeDelta kTimestamp = base::Microseconds(1337);

  const uint8_t* const data[] = {kTestData};
  scoped_refptr<AudioBuffer> buffer =
      AudioBuffer::CopyFrom(kSampleFormatU8,
                            CHANNEL_LAYOUT_STEREO,
                            2,
                            kSampleRate,
                            16,
                            data,
                            kTimestamp);
  EXPECT_EQ(16, buffer->frame_count());  // 2 channels of 8-bit data

  buffer = AudioBuffer::CopyFrom(kSampleFormatF32, CHANNEL_LAYOUT_4_0, 4,
                                 kSampleRate, 2, data, kTimestamp);
  EXPECT_EQ(2, buffer->frame_count());  // now 4 channels of 32-bit data
}

TEST(AudioBufferTest, ReadBitstream) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 1024;
  const size_t data_size = frames / 2;
  const base::TimeDelta start_time;

  scoped_refptr<AudioBuffer> buffer = MakeBitstreamAudioBuffer(
      kSampleFormatEac3, channel_layout, channels, kSampleRate, 1, 1, frames,
      data_size, start_time);
  EXPECT_TRUE(buffer->IsBitstreamFormat());

  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(frames, 0, 0, bus.get());

  EXPECT_TRUE(bus->is_bitstream_format());
  EXPECT_EQ(frames, bus->GetBitstreamFrames());
  EXPECT_EQ(data_size, bus->GetBitstreamDataSize());
  VerifyBitstreamAudioBus(bus.get(), data_size, 1, 1);
}

TEST(AudioBufferTest, ReadBitstreamIECDts) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_MONO;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 512;
  const size_t data_size = frames * 2 * 2;
  const base::TimeDelta start_time;

  scoped_refptr<AudioBuffer> buffer = MakeBitstreamAudioBuffer(
      kSampleFormatIECDts, channel_layout, channels, kSampleRate, 1, 1, frames,
      data_size, start_time);
  EXPECT_TRUE(buffer->IsBitstreamFormat());

  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(frames, 0, 0, bus.get());

  EXPECT_TRUE(bus->is_bitstream_format());
  EXPECT_EQ(frames, bus->GetBitstreamFrames());
  EXPECT_EQ(data_size, bus->GetBitstreamDataSize());
  VerifyBitstreamAudioBus(bus.get(), data_size, 1, 1);
}

TEST(AudioBufferTest, ReadU8) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 10;
  const base::TimeDelta start_time;
  scoped_refptr<AudioBuffer> buffer =
      MakeAudioBuffer<uint8_t>(kSampleFormatU8, channel_layout, channels,
                               kSampleRate, 128, 1, frames, start_time);
  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(frames, 0, 0, bus.get());
  VerifyBus(bus.get(), frames, 0, 1.0f / 127.0f);

  // Now read the same data one frame at a time.
  bus->Zero();
  for (int i = 0; i < frames; ++i)
    buffer->ReadFrames(1, i, i, bus.get());
  VerifyBus(bus.get(), frames, 0, 1.0f / 127.0f);
}

TEST(AudioBufferTest, ReadS16) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 10;
  const base::TimeDelta start_time;
  scoped_refptr<AudioBuffer> buffer =
      MakeAudioBuffer<int16_t>(kSampleFormatS16, channel_layout, channels,
                               kSampleRate, 1, 1, frames, start_time);
  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(frames, 0, 0, bus.get());
  VerifyBus(bus.get(), frames, 1.0f / std::numeric_limits<int16_t>::max(),
            1.0f / std::numeric_limits<int16_t>::max());

  // Now read the same data one frame at a time.
  bus->Zero();
  for (int i = 0; i < frames; ++i)
    buffer->ReadFrames(1, i, i, bus.get());
  VerifyBus(bus.get(), frames, 1.0f / std::numeric_limits<int16_t>::max(),
            1.0f / std::numeric_limits<int16_t>::max());
}

TEST(AudioBufferTest, ReadS32) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 20;
  const base::TimeDelta start_time;
  scoped_refptr<AudioBuffer> buffer =
      MakeAudioBuffer<int32_t>(kSampleFormatS32, channel_layout, channels,
                               kSampleRate, 1, 1, frames, start_time);
  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(frames, 0, 0, bus.get());
  constexpr float kIncrement =
      1.0f / static_cast<float>(std::numeric_limits<int32_t>::max());
  VerifyBus(bus.get(), frames, kIncrement, kIncrement);

  // Read second 10 frames.
  bus->Zero();
  buffer->ReadFrames(10, 10, 0, bus.get());
  VerifyBus(bus.get(), 10, 11.0f * kIncrement, kIncrement);
}

TEST(AudioBufferTest, ReadF32) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 20;
  const base::TimeDelta start_time;
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<float>(kSampleFormatF32,
                                                             channel_layout,
                                                             channels,
                                                             kSampleRate,
                                                             1.0f,
                                                             1.0f,
                                                             frames,
                                                             start_time);
  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(10, 0, 0, bus.get());
  VerifyBus(bus.get(), 10, 1, 1, ValueType::kFloat);

  // Read second 10 frames.
  bus->Zero();
  buffer->ReadFrames(10, 10, 0, bus.get());
  VerifyBus(bus.get(), 10, 11, 1, ValueType::kFloat);
}

TEST(AudioBufferTest, ReadU8Planar) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 20;
  constexpr float kIncrement = 1.0f / 127.0f;
  constexpr float kStart = 0;
  const base::TimeDelta start_time;
  scoped_refptr<AudioBuffer> buffer =
      MakeAudioBuffer<uint8_t>(kSampleFormatPlanarU8, channel_layout, channels,
                               kSampleRate, 128, 1, frames, start_time);
  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(10, 0, 0, bus.get());
  VerifyBus(bus.get(), 10, kStart, kIncrement);

  // Read all the frames backwards, one by one. ch[0] should be 20, 19, ...
  bus->Zero();
  for (int i = frames - 1; i >= 0; --i)
    buffer->ReadFrames(1, i, i, bus.get());
  VerifyBus(bus.get(), frames, kStart, kIncrement);

  // Read 0 frames with different offsets. Existing data in AudioBus should be
  // unchanged.
  buffer->ReadFrames(0, 0, 0, bus.get());
  VerifyBus(bus.get(), frames, kStart, kIncrement);
  buffer->ReadFrames(0, 0, 10, bus.get());
  VerifyBus(bus.get(), frames, kStart, kIncrement);
  buffer->ReadFrames(0, 10, 0, bus.get());
  VerifyBus(bus.get(), frames, kStart, kIncrement);
}

TEST(AudioBufferTest, ReadS16Planar) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 20;
  const base::TimeDelta start_time;
  scoped_refptr<AudioBuffer> buffer =
      MakeAudioBuffer<int16_t>(kSampleFormatPlanarS16, channel_layout, channels,
                               kSampleRate, 1, 1, frames, start_time);
  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(10, 0, 0, bus.get());
  VerifyBus(bus.get(), 10, 1.0f / std::numeric_limits<int16_t>::max(),
            1.0f / std::numeric_limits<int16_t>::max());

  // Read all the frames backwards, one by one. ch[0] should be 20, 19, ...
  bus->Zero();
  for (int i = frames - 1; i >= 0; --i)
    buffer->ReadFrames(1, i, i, bus.get());
  VerifyBus(bus.get(), frames, 1.0f / std::numeric_limits<int16_t>::max(),
            1.0f / std::numeric_limits<int16_t>::max());

  // Read 0 frames with different offsets. Existing data in AudioBus should be
  // unchanged.
  buffer->ReadFrames(0, 0, 0, bus.get());
  VerifyBus(bus.get(), frames, 1.0f / std::numeric_limits<int16_t>::max(),
            1.0f / std::numeric_limits<int16_t>::max());
  buffer->ReadFrames(0, 0, 10, bus.get());
  VerifyBus(bus.get(), frames, 1.0f / std::numeric_limits<int16_t>::max(),
            1.0f / std::numeric_limits<int16_t>::max());
  buffer->ReadFrames(0, 10, 0, bus.get());
  VerifyBus(bus.get(), frames, 1.0f / std::numeric_limits<int16_t>::max(),
            1.0f / std::numeric_limits<int16_t>::max());
}

TEST(AudioBufferTest, ReadS32Planar) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 20;
  constexpr float kIncrement =
      1.0f / static_cast<float>(std::numeric_limits<int32_t>::max());
  constexpr float kStart = kIncrement;
  const base::TimeDelta start_time;
  scoped_refptr<AudioBuffer> buffer =
      MakeAudioBuffer<int32_t>(kSampleFormatPlanarS32, channel_layout, channels,
                               kSampleRate, 1, 1, frames, start_time);
  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(10, 0, 0, bus.get());
  VerifyBus(bus.get(), 10, kStart, kIncrement);

  // Read all the frames backwards, one by one. ch[0] should be 20, 19, ...
  bus->Zero();
  for (int i = frames - 1; i >= 0; --i)
    buffer->ReadFrames(1, i, i, bus.get());
  VerifyBus(bus.get(), frames, kStart, kIncrement);

  // Read 0 frames with different offsets. Existing data in AudioBus should be
  // unchanged.
  buffer->ReadFrames(0, 0, 0, bus.get());
  VerifyBus(bus.get(), frames, kStart, kIncrement);
  buffer->ReadFrames(0, 0, 10, bus.get());
  VerifyBus(bus.get(), frames, kStart, kIncrement);
  buffer->ReadFrames(0, 10, 0, bus.get());
  VerifyBus(bus.get(), frames, kStart, kIncrement);
}

TEST(AudioBufferTest, ReadF32Planar) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 100;
  const base::TimeDelta start_time;
  scoped_refptr<AudioBuffer> buffer =
      MakeAudioBuffer<float>(kSampleFormatPlanarF32,
                             channel_layout,
                             channels,
                             kSampleRate,
                             1.0f,
                             1.0f,
                             frames,
                             start_time);

  // Read all 100 frames from the buffer. F32 is planar, so ch[0] should be 1,
  // 2, 3, 4, ..., ch[1] should be 101, 102, 103, ..., and so on for all 4
  // channels.
  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  buffer->ReadFrames(frames, 0, 0, bus.get());
  VerifyBus(bus.get(), frames, 1, 1, ValueType::kFloat);

  // Now read 20 frames from the middle of the buffer.
  bus->Zero();
  buffer->ReadFrames(20, 50, 0, bus.get());
  VerifyBus(bus.get(), 20, 51, 1, ValueType::kFloat);
}

TEST(AudioBufferTest, WrapOrCopyToAudioBus) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 100;
  const base::TimeDelta start_time;
  scoped_refptr<AudioBuffer> buffer =
      MakeAudioBuffer<float>(kSampleFormatPlanarF32, channel_layout, channels,
                             kSampleRate, 1.0f, 1.0f, frames, start_time);

  // With kSampleFormatPlanarF32, the memory layout should allow |bus| to
  // directly wrap |buffer|'s data.
  std::unique_ptr<AudioBus> bus = AudioBuffer::WrapOrCopyToAudioBus(buffer);
  for (int ch = 0; ch < channels; ++ch) {
    EXPECT_EQ(bus->channel(ch),
              reinterpret_cast<float*>(buffer->channel_data()[ch]));
  }

  // |bus| should have its own reference on |buffer|, so clearing it here should
  // not free the underlying data.
  buffer.reset();
  VerifyBus(bus.get(), frames, 1, 1, ValueType::kFloat);

  // Interleaved samples cannot be wrapped, and samples will be copied out.
  buffer = MakeAudioBuffer<float>(kSampleFormatF32, channel_layout, channels,
                                  kSampleRate, 1.0f, 1.0f, frames, start_time);

  bus = AudioBuffer::WrapOrCopyToAudioBus(buffer);
  buffer.reset();

  VerifyBus(bus.get(), frames, 1, 1, ValueType::kFloat);
}

TEST(AudioBufferTest, EmptyBuffer) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = kSampleRate / 100;
  const base::TimeDelta start_time;
  scoped_refptr<AudioBuffer> buffer = AudioBuffer::CreateEmptyBuffer(
      channel_layout, channels, kSampleRate, frames, start_time);
  EXPECT_EQ(frames, buffer->frame_count());
  EXPECT_EQ(start_time, buffer->timestamp());
  EXPECT_EQ(base::Milliseconds(10), buffer->duration());
  EXPECT_FALSE(buffer->end_of_stream());

  // Read all frames from the buffer. All data should be 0.
  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(frames, 0, 0, bus.get());
  VerifyBus(bus.get(), frames, 0, 0);

  // Set some data to confirm the overwrite.
  std::vector<float*> wrapped_channels = WrapChannelsAsVector(bus.get());
  for (float* wrapped_channel : wrapped_channels)
    memset(wrapped_channel, 123, frames * sizeof(float));
}

TEST(AudioBufferTest, TrimEmptyBuffer) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = kSampleRate / 10;
  const base::TimeDelta start_time;
  const base::TimeDelta duration = base::Milliseconds(100);
  scoped_refptr<AudioBuffer> buffer = AudioBuffer::CreateEmptyBuffer(
      channel_layout, channels, kSampleRate, frames, start_time);
  EXPECT_EQ(frames, buffer->frame_count());
  EXPECT_EQ(start_time, buffer->timestamp());
  EXPECT_EQ(duration, buffer->duration());
  EXPECT_FALSE(buffer->end_of_stream());

  // Read all frames from the buffer. All data should be 0.
  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(frames, 0, 0, bus.get());
  VerifyBus(bus.get(), frames, 0, 0);

  // Trim 10ms of frames from the middle of the buffer.
  int trim_start = frames / 2;
  const int trim_length = kSampleRate / 100;
  const base::TimeDelta trim_duration = base::Milliseconds(10);
  buffer->TrimRange(trim_start, trim_start + trim_length);
  EXPECT_EQ(frames - trim_length, buffer->frame_count());
  EXPECT_EQ(start_time, buffer->timestamp());
  EXPECT_EQ(duration - trim_duration, buffer->duration());
  bus->Zero();
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), trim_start, 0, 0);
}

TEST(AudioBufferTest, Trim) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = kSampleRate / 10;
  const base::TimeDelta start_time;
  const base::TimeDelta duration = base::Milliseconds(100);
  scoped_refptr<AudioBuffer> buffer =
      MakeAudioBuffer<float>(kSampleFormatPlanarF32,
                             channel_layout,
                             channels,
                             kSampleRate,
                             0.0f,
                             1.0f,
                             frames,
                             start_time);
  EXPECT_EQ(frames, buffer->frame_count());
  EXPECT_EQ(start_time, buffer->timestamp());
  EXPECT_EQ(duration, buffer->duration());

  const int ten_ms_of_frames = kSampleRate / 100;
  const base::TimeDelta ten_ms = base::Milliseconds(10);

  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), buffer->frame_count(), 0.0f, 1.0f, ValueType::kFloat);

  // Trim off 10ms of frames from the start.
  buffer->TrimStart(ten_ms_of_frames);
  EXPECT_EQ(start_time, buffer->timestamp());
  EXPECT_EQ(frames - ten_ms_of_frames, buffer->frame_count());
  EXPECT_EQ(duration - ten_ms, buffer->duration());
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), buffer->frame_count(), ten_ms_of_frames, 1.0f,
            ValueType::kFloat);

  // Trim off 10ms of frames from the end.
  buffer->TrimEnd(ten_ms_of_frames);
  EXPECT_EQ(start_time, buffer->timestamp());
  EXPECT_EQ(frames - 2 * ten_ms_of_frames, buffer->frame_count());
  EXPECT_EQ(duration - 2 * ten_ms, buffer->duration());
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), buffer->frame_count(), ten_ms_of_frames, 1.0f,
            ValueType::kFloat);

  // Trim off 40ms more from the start.
  buffer->TrimStart(4 * ten_ms_of_frames);
  EXPECT_EQ(start_time, buffer->timestamp());
  EXPECT_EQ(frames - 6 * ten_ms_of_frames, buffer->frame_count());
  EXPECT_EQ(duration - 6 * ten_ms, buffer->duration());
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), buffer->frame_count(), 5 * ten_ms_of_frames, 1.0f,
            ValueType::kFloat);

  // Trim off the final 40ms from the end.
  buffer->TrimEnd(4 * ten_ms_of_frames);
  EXPECT_EQ(0, buffer->frame_count());
  EXPECT_EQ(start_time, buffer->timestamp());
  EXPECT_EQ(base::TimeDelta(), buffer->duration());
}

TEST(AudioBufferTest, TrimRangePlanar) {
  TrimRangeTest(kSampleFormatPlanarF32);
}

TEST(AudioBufferTest, TrimRangeInterleaved) {
  TrimRangeTest(kSampleFormatF32);
}

TEST(AudioBufferTest, AudioBufferMemoryPool) {
  scoped_refptr<AudioBufferMemoryPool> pool(new AudioBufferMemoryPool());
  EXPECT_EQ(0u, pool->GetPoolSizeForTesting());

  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_MONO;
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<uint8_t>(
      kSampleFormatU8, kChannelLayout,
      ChannelLayoutToChannelCount(kChannelLayout), kSampleRate, 1, 1,
      kSampleRate / 100, base::TimeDelta());

  // Creating and returning a buffer should increase pool size.
  scoped_refptr<AudioBuffer> b1 = AudioBuffer::CopyFrom(
      kSampleFormatU8, buffer->channel_layout(), buffer->channel_count(),
      buffer->sample_rate(), buffer->frame_count(), &buffer->channel_data()[0],
      buffer->timestamp(), pool);
  EXPECT_EQ(0u, pool->GetPoolSizeForTesting());
  b1 = nullptr;
  EXPECT_EQ(1u, pool->GetPoolSizeForTesting());

  // Even (especially) when used with CreateBuffer.
  b1 = AudioBuffer::CreateBuffer(kSampleFormatU8, buffer->channel_layout(),
                                 buffer->channel_count(), buffer->sample_rate(),
                                 buffer->frame_count(), pool);
  EXPECT_EQ(0u, pool->GetPoolSizeForTesting());
  scoped_refptr<AudioBuffer> b2 = AudioBuffer::CreateBuffer(
      kSampleFormatU8, buffer->channel_layout(), buffer->channel_count(),
      buffer->sample_rate(), buffer->frame_count(), pool);
  EXPECT_EQ(0u, pool->GetPoolSizeForTesting());
  b2 = nullptr;
  EXPECT_EQ(1u, pool->GetPoolSizeForTesting());
  b1 = nullptr;
  EXPECT_EQ(2u, pool->GetPoolSizeForTesting());

  // A buffer of a different size should not reuse the buffer and drain pool.
  b2 = AudioBuffer::CreateBuffer(kSampleFormatU8, buffer->channel_layout(),
                                 buffer->channel_count(), buffer->sample_rate(),
                                 buffer->frame_count() / 2, pool);
  EXPECT_EQ(0u, pool->GetPoolSizeForTesting());

  // Mark pool for destruction and ensure buffer is still valid.
  pool = nullptr;
  memset(b2->channel_data()[0], 0, b2->frame_count());

  // Destruct final frame after pool; hope nothing explodes.
  b2 = nullptr;
}

// Test that the channels are aligned according to the pool parameter.
TEST(AudioBufferTest, AudioBufferMemoryPoolAlignment) {
  const int kAlignment = 512;
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_6_1;
  const size_t kChannelCount = ChannelLayoutToChannelCount(kChannelLayout);

  scoped_refptr<AudioBufferMemoryPool> pool(
      new AudioBufferMemoryPool(kAlignment));
  scoped_refptr<AudioBuffer> buffer =
      AudioBuffer::CreateBuffer(kSampleFormatPlanarU8, kChannelLayout,
                                kChannelCount, kSampleRate, kSampleRate, pool);

  ASSERT_EQ(kChannelCount, buffer->channel_data().size());
  for (size_t i = 0; i < kChannelCount; i++) {
    EXPECT_EQ(
        0u, reinterpret_cast<uintptr_t>(buffer->channel_data()[i]) % kAlignment)
        << " channel: " << i;
  }

  buffer.reset();
  EXPECT_EQ(1u, pool->GetPoolSizeForTesting());
}

// Test that the channels are aligned when buffers are not pooled.
TEST(AudioBufferTest, AudioBufferAlignmentUnpooled) {
  constexpr ChannelLayout kChannelLayout = CHANNEL_LAYOUT_6_1;
  const size_t kChannelCount = ChannelLayoutToChannelCount(kChannelLayout);

  scoped_refptr<AudioBuffer> buffer =
      AudioBuffer::CreateBuffer(kSampleFormatPlanarU8, kChannelLayout,
                                kChannelCount, kSampleRate, kSampleRate);

  ASSERT_EQ(kChannelCount, buffer->channel_data().size());
  for (size_t i = 0; i < kChannelCount; i++) {
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(buffer->channel_data()[i]) %
                      AudioBus::kChannelAlignment)
        << " channel: " << i;
  }
}

// Planar allocations use a different path, so make sure pool is used.
TEST(AudioBufferTest, AudioBufferMemoryPoolPlanar) {
  scoped_refptr<AudioBufferMemoryPool> pool(new AudioBufferMemoryPool());
  EXPECT_EQ(0u, pool->GetPoolSizeForTesting());

  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_MONO;
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<uint8_t>(
      kSampleFormatPlanarF32, kChannelLayout,
      ChannelLayoutToChannelCount(kChannelLayout), kSampleRate, 1, 1,
      kSampleRate / 100, base::TimeDelta());

  // Creating and returning a buffer should increase pool size.
  scoped_refptr<AudioBuffer> b1 = AudioBuffer::CopyFrom(
      kSampleFormatPlanarF32, buffer->channel_layout(), buffer->channel_count(),
      buffer->sample_rate(), buffer->frame_count(), &buffer->channel_data()[0],
      buffer->timestamp(), pool);
  EXPECT_EQ(0u, pool->GetPoolSizeForTesting());
  b1 = nullptr;
  EXPECT_EQ(1u, pool->GetPoolSizeForTesting());

  // Even (especially) when used with CreateBuffer.
  b1 = AudioBuffer::CreateBuffer(kSampleFormatU8, buffer->channel_layout(),
                                 buffer->channel_count(), buffer->sample_rate(),
                                 buffer->frame_count(), pool);
  EXPECT_EQ(0u, pool->GetPoolSizeForTesting());

  // Mark pool for destruction and ensure buffer is still valid.
  pool = nullptr;
  memset(b1->channel_data()[0], 0, b1->frame_count());

  // Destruct final frame after pool; hope nothing explodes.
  b1 = nullptr;
}

}  // namespace media
