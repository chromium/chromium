// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/audio/delay_buffer.h"

#include <algorithm>

#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {
namespace {

constexpr int kChannels = 1;
constexpr int kMaxFrames = 32;

#define EXPECT_BUS_VALUES_EQ(bus, begin, end, value)                        \
  {                                                                         \
    const auto IsValue = [](float x) { return x == (value); };              \
    EXPECT_TRUE(std::all_of((bus)->channel(0) + (begin),                    \
                            (bus)->channel(0) + (end) - (begin), IsValue)); \
  }

TEST(DelayBufferTest, RecordsAMaximumNumberOfFrames) {
  DelayBuffer buffer(kMaxFrames);
  ASSERT_EQ(buffer.GetBeginPosition(), buffer.GetEndPosition());

  constexpr int frames_per_bus = kMaxFrames / 4;
  const auto bus = media::AudioBus::Create(kChannels, frames_per_bus);
  std::fill(bus->channel(0), bus->channel(0) + frames_per_bus, 1.0);

  // Fill the buffer.
  DelayBuffer::FrameTicks position = 0;
  for (int i = 0; i < 4; ++i) {
    buffer.Write(position, *bus, 1.0);
    position += frames_per_bus;
    EXPECT_EQ(0, buffer.GetBeginPosition());
    EXPECT_EQ(position, buffer.GetEndPosition());
  }

  // Writing just one more bus should cause the leading frames to be dropped.
  buffer.Write(position, *bus, 1.0);
  position += frames_per_bus;
  EXPECT_EQ(position - kMaxFrames, buffer.GetBeginPosition());
  EXPECT_EQ(position, buffer.GetEndPosition());

  // Now, simulate a gap in the recording by recording the next bus late.
  position += frames_per_bus * 2;
  buffer.Write(position, *bus, 1.0);
  position += frames_per_bus;
  EXPECT_EQ(position - kMaxFrames, buffer.GetBeginPosition());
  EXPECT_EQ(position, buffer.GetEndPosition());
}

TEST(DelayBufferTest, ReadsSilenceIfNothingWasRecorded) {
  DelayBuffer buffer(kMaxFrames);
  ASSERT_EQ(buffer.GetBeginPosition(), buffer.GetEndPosition());

  DelayBuffer::FrameTicks position = 0;
  constexpr int frames_per_bus = kMaxFrames / 4;
  const auto bus = media::AudioBus::Create(kChannels, frames_per_bus);

  for (int i = 0; i < 10; ++i) {
    // Set data in the bus to confirm it is all going to be overwritten.
    std::fill(bus->channel(0), bus->channel(0) + frames_per_bus, 1.0);

    buffer.Read(position, frames_per_bus, bus.get());
    EXPECT_EQ(buffer.GetBeginPosition(), buffer.GetEndPosition());
    EXPECT_BUS_VALUES_EQ(bus, 0, frames_per_bus, 0.0);

    position += frames_per_bus;
  }
}

TEST(DelayBufferTest, ReadsSilenceIfOutsideRecordedRange) {
  DelayBuffer buffer(kMaxFrames);
  ASSERT_EQ(buffer.GetBeginPosition(), buffer.GetEndPosition());

  constexpr int frames_per_bus = kMaxFrames / 4;
  const auto bus = media::AudioBus::Create(kChannels, frames_per_bus);
  std::fill(bus->channel(0), bus->channel(0) + frames_per_bus, 1.0);

  // Fill the buffer.
  DelayBuffer::FrameTicks position = 0;
  for (int i = 0; i < 4; ++i) {
    buffer.Write(position, *bus, 1.0);
    position += frames_per_bus;
  }
  EXPECT_EQ(0, buffer.GetBeginPosition());
  EXPECT_EQ(position, buffer.GetEndPosition());

  // Read before the begin position and expect to get silence.
  std::fill(bus->channel(0), bus->channel(0) + frames_per_bus, 0.5);
  buffer.Read(-kMaxFrames, frames_per_bus, bus.get());
  EXPECT_BUS_VALUES_EQ(bus, 0, frames_per_bus, 0.0);

  // Read at a position one before the begin position. Expect the first sample
  // to be 0.0, and the rest 1.0.
  std::fill(bus->channel(0), bus->channel(0) + frames_per_bus, 0.5);
  buffer.Read(buffer.GetBeginPosition() - 1, frames_per_bus, bus.get());
  EXPECT_EQ(0.0, bus->channel(0)[0]);
  EXPECT_BUS_VALUES_EQ(bus, 1, frames_per_bus - 1, 1.0);

  // Read at a position where the last sample should be 0.0 and the rest 1.0.
  std::fill(bus->channel(0), bus->channel(0) + frames_per_bus, 0.5);
  buffer.Read(buffer.GetEndPosition() - frames_per_bus + 1, frames_per_bus,
              bus.get());
  EXPECT_BUS_VALUES_EQ(bus, 0, frames_per_bus - 1, 1.0);
  EXPECT_EQ(0.0, bus->channel(0)[frames_per_bus - 1]);

  // Read after the end position and expect to get silence.
  std::fill(bus->channel(0), bus->channel(0) + frames_per_bus, 0.5);
  buffer.Read(kMaxFrames, frames_per_bus, bus.get());
  EXPECT_BUS_VALUES_EQ(bus, 0, frames_per_bus, 0.0);
}

TEST(DelayBufferTest, ReadsGapsInRecording) {
  DelayBuffer buffer(kMaxFrames);
  ASSERT_EQ(buffer.GetBeginPosition(), buffer.GetEndPosition());

  constexpr int frames_per_bus = kMaxFrames / 4;
  const auto bus = media::AudioBus::Create(kChannels, frames_per_bus);
  std::fill(bus->channel(0), bus->channel(0) + frames_per_bus, 1.0);

  // Fill the buffer, but with a gap in the third quarter of it.
  DelayBuffer::FrameTicks record_position = 0;
  for (int i = 0; i < 4; ++i) {
    if (i != 2) {
      buffer.Write(record_position, *bus, 1.0);
    }
    record_position += frames_per_bus;
  }
  EXPECT_EQ(0, buffer.GetBeginPosition());
  EXPECT_EQ(record_position, buffer.GetEndPosition());

  // Read through the whole range, but offset by one frame early. Confirm the
  // silence gap appears in the right place.
  DelayBuffer::FrameTicks read_position = -1;
  std::fill(bus->channel(0), bus->channel(0) + frames_per_bus, 0.5);
  buffer.Read(read_position, frames_per_bus, bus.get());
  read_position += frames_per_bus;
  EXPECT_EQ(0.0, bus->channel(0)[0]);
  EXPECT_BUS_VALUES_EQ(bus, 1, frames_per_bus - 1, 1.0);

  std::fill(bus->channel(0), bus->channel(0) + frames_per_bus, 0.5);
  buffer.Read(read_position, frames_per_bus, bus.get());
  read_position += frames_per_bus;
  EXPECT_BUS_VALUES_EQ(bus, 0, frames_per_bus, 1.0);

  std::fill(bus->channel(0), bus->channel(0) + frames_per_bus, 0.5);
  buffer.Read(read_position, frames_per_bus, bus.get());
  read_position += frames_per_bus;
  EXPECT_EQ(1.0, bus->channel(0)[0]);
  // The gap begins.
  EXPECT_BUS_VALUES_EQ(bus, 1, frames_per_bus - 1, 0.0);

  std::fill(bus->channel(0), bus->channel(0) + frames_per_bus, 0.5);
  buffer.Read(read_position, frames_per_bus, bus.get());
  read_position += frames_per_bus;
  EXPECT_EQ(0.0, bus->channel(0)[0]);
  // The gap ends.
  EXPECT_BUS_VALUES_EQ(bus, 1, frames_per_bus - 1, 1.0);
}

}  // namespace
}  // namespace audio
