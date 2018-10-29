// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>
#include <memory>

#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kSampleRate = 4800;

static void VerifyBusWithOffset(AudioBus* bus,
                                int offset,
                                int frames,
                                float start,
                                float start_offset,
                                float increment) {
  for (int ch = 0; ch < bus->channels(); ++ch) {
    const float v = start_offset + start + ch * bus->frames() * increment;
    for (int i = offset; i < offset + frames; ++i) {
      ASSERT_FLOAT_EQ(v + i * increment, bus->channel(ch)[i]) << "i=" << i
                                                              << ", ch=" << ch;
    }
  }
}

static void VerifyBus(AudioBus* bus, int frames, float start, float increment) {
  VerifyBusWithOffset(bus, 0, frames, start, 0, increment);
}

static void TrimRangeTest(SampleFormat sample_format) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = kSampleRate / 10;
  const base::TimeDelta timestamp = base::TimeDelta();
  const base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
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
  VerifyBus(bus.get(), frames, 0, 1);

  // Trim 10ms of frames from the middle of the buffer.
  int trim_start = frames / 2;
  const int trim_length = kSampleRate / 100;
  const base::TimeDelta trim_duration = base::TimeDelta::FromMilliseconds(10);
  buffer->TrimRange(trim_start, trim_start + trim_length);
  EXPECT_EQ(frames - trim_length, buffer->frame_count());
  EXPECT_EQ(timestamp, buffer->timestamp());
  EXPECT_EQ(duration - trim_duration, buffer->duration());
  bus->Zero();
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), trim_start, 0, 1);
  VerifyBusWithOffset(bus.get(),
                      trim_start,
                      buffer->frame_count() - trim_start,
                      0,
                      trim_length,
                      1);

  // Trim 10ms of frames from the start, which just adjusts the buffer's
  // internal start offset.
  buffer->TrimStart(trim_length);
  trim_start -= trim_length;
  EXPECT_EQ(frames - 2 * trim_length, buffer->frame_count());
  EXPECT_EQ(timestamp, buffer->timestamp());
  EXPECT_EQ(duration - 2 * trim_duration, buffer->duration());
  bus->Zero();
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), trim_start, trim_length, 1);
  VerifyBusWithOffset(bus.get(),
                      trim_start,
                      buffer->frame_count() - trim_start,
                      trim_length,
                      trim_length,
                      1);

  // Trim 10ms of frames from the end, which just adjusts the buffer's frame
  // count.
  buffer->TrimEnd(trim_length);
  EXPECT_EQ(frames - 3 * trim_length, buffer->frame_count());
  EXPECT_EQ(timestamp, buffer->timestamp());
  EXPECT_EQ(duration - 3 * trim_duration, buffer->duration());
  bus->Zero();
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), trim_start, trim_length, 1);
  VerifyBusWithOffset(bus.get(),
                      trim_start,
                      buffer->frame_count() - trim_start,
                      trim_length,
                      trim_length,
                      1);

  // Trim another 10ms from the inner portion of the buffer.
  buffer->TrimRange(trim_start, trim_start + trim_length);
  EXPECT_EQ(frames - 4 * trim_length, buffer->frame_count());
  EXPECT_EQ(timestamp, buffer->timestamp());
  EXPECT_EQ(duration - 4 * trim_duration, buffer->duration());
  bus->Zero();
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), trim_start, trim_length, 1);
  VerifyBusWithOffset(bus.get(),
                      trim_start,
                      buffer->frame_count() - trim_start,
                      trim_length,
                      trim_length * 2,
                      1);

  // Trim off the end using TrimRange() to ensure end index is exclusive.
  buffer->TrimRange(buffer->frame_count() - trim_length, buffer->frame_count());
  EXPECT_EQ(frames - 5 * trim_length, buffer->frame_count());
  EXPECT_EQ(timestamp, buffer->timestamp());
  EXPECT_EQ(duration - 5 * trim_duration, buffer->duration());
  bus->Zero();
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), trim_start, trim_length, 1);
  VerifyBusWithOffset(bus.get(),
                      trim_start,
                      buffer->frame_count() - trim_start,
                      trim_length,
                      trim_length * 2,
                      1);

  // Trim off the start using TrimRange() to ensure start index is inclusive.
  buffer->TrimRange(0, trim_length);
  trim_start -= trim_length;
  EXPECT_EQ(frames - 6 * trim_length, buffer->frame_count());
  EXPECT_EQ(timestamp, buffer->timestamp());
  EXPECT_EQ(duration - 6 * trim_duration, buffer->duration());
  bus->Zero();
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), trim_start, 2 * trim_length, 1);
  VerifyBusWithOffset(bus.get(),
                      trim_start,
                      buffer->frame_count() - trim_start,
                      trim_length * 2,
                      trim_length * 2,
                      1);
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

TEST(AudioBufferTest, CopyBitstreamFrom) {
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
  const int kChannelCount = ChannelLayoutToChannelCount(kChannelLayout);
  const int kFrameCount = 128;
  const uint8_t kTestData[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                               11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                               22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);
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

TEST(AudioBufferTest, CreateEOSBuffer) {
  scoped_refptr<AudioBuffer> buffer = AudioBuffer::CreateEOSBuffer();
  EXPECT_TRUE(buffer->end_of_stream());
}

TEST(AudioBufferTest, FrameSize) {
  const uint8_t kTestData[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                               11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                               22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

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
  VerifyBus(bus.get(), frames, 1.0f / std::numeric_limits<int32_t>::max(),
            1.0f / std::numeric_limits<int32_t>::max());

  // Read second 10 frames.
  bus->Zero();
  buffer->ReadFrames(10, 10, 0, bus.get());
  VerifyBus(bus.get(), 10, 11.0f / std::numeric_limits<int32_t>::max(),
            1.0f / std::numeric_limits<int32_t>::max());
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
  VerifyBus(bus.get(), 10, 1, 1);

  // Read second 10 frames.
  bus->Zero();
  buffer->ReadFrames(10, 10, 0, bus.get());
  VerifyBus(bus.get(), 10, 11, 1);
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
  VerifyBus(bus.get(), frames, 1, 1);

  // Now read 20 frames from the middle of the buffer.
  bus->Zero();
  buffer->ReadFrames(20, 50, 0, bus.get());
  VerifyBus(bus.get(), 20, 51, 1);
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
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(10), buffer->duration());
  EXPECT_FALSE(buffer->end_of_stream());

  // Read all 100 frames from the buffer. All data should be 0.
  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(frames, 0, 0, bus.get());
  VerifyBus(bus.get(), frames, 0, 0);
}

TEST(AudioBufferTest, TrimEmptyBuffer) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = kSampleRate / 10;
  const base::TimeDelta start_time;
  const base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
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
  const base::TimeDelta trim_duration = base::TimeDelta::FromMilliseconds(10);
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
  const base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
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
  const base::TimeDelta ten_ms = base::TimeDelta::FromMilliseconds(10);

  std::unique_ptr<AudioBus> bus = AudioBus::Create(channels, frames);
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), buffer->frame_count(), 0.0f, 1.0f);

  // Trim off 10ms of frames from the start.
  buffer->TrimStart(ten_ms_of_frames);
  EXPECT_EQ(start_time, buffer->timestamp());
  EXPECT_EQ(frames - ten_ms_of_frames, buffer->frame_count());
  EXPECT_EQ(duration - ten_ms, buffer->duration());
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), buffer->frame_count(), ten_ms_of_frames, 1.0f);

  // Trim off 10ms of frames from the end.
  buffer->TrimEnd(ten_ms_of_frames);
  EXPECT_EQ(start_time, buffer->timestamp());
  EXPECT_EQ(frames - 2 * ten_ms_of_frames, buffer->frame_count());
  EXPECT_EQ(duration - 2 * ten_ms, buffer->duration());
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), buffer->frame_count(), ten_ms_of_frames, 1.0f);

  // Trim off 40ms more from the start.
  buffer->TrimStart(4 * ten_ms_of_frames);
  EXPECT_EQ(start_time, buffer->timestamp());
  EXPECT_EQ(frames - 6 * ten_ms_of_frames, buffer->frame_count());
  EXPECT_EQ(duration - 6 * ten_ms, buffer->duration());
  buffer->ReadFrames(buffer->frame_count(), 0, 0, bus.get());
  VerifyBus(bus.get(), buffer->frame_count(), 5 * ten_ms_of_frames, 1.0f);

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
