// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_fifo.h"

#include <algorithm>
#include <memory>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioFifoTest : public testing::Test {
 public:
  AudioFifoTest() = default;

  AudioFifoTest(const AudioFifoTest&) = delete;
  AudioFifoTest& operator=(const AudioFifoTest&) = delete;

  ~AudioFifoTest() override = default;

  void VerifyValue(base::span<float> data, float value) {
    for (size_t i = 0; i < data.size(); ++i) {
      ASSERT_FLOAT_EQ(value, data[i]) << "i=" << i;
    }
  }
};

// Verify that construction works as intended.
TEST_F(AudioFifoTest, Construct) {
  static constexpr int kChannels = 6;
  static constexpr size_t kMaxFrameCount = 128;
  AudioFifo fifo(kChannels, kMaxFrameCount);
  EXPECT_EQ(fifo.frames(), 0u);
}

// Pushes audio bus objects to a FIFO and fill it up to different degrees.
TEST_F(AudioFifoTest, Push) {
  static constexpr int kChannels = 2;
  static constexpr size_t kMaxFrameCount = 128;
  AudioFifo fifo(kChannels, kMaxFrameCount);
  {
    SCOPED_TRACE("Push 50%");
    static constexpr size_t kHalfFrameCount = kMaxFrameCount / 2;
    std::unique_ptr<AudioBus> bus =
        AudioBus::Create(kChannels, kHalfFrameCount);
    EXPECT_EQ(fifo.frames(), 0u);
    fifo.Push(bus.get());
    EXPECT_EQ(fifo.frames(), kHalfFrameCount);
    fifo.Clear();
  }
  {
    SCOPED_TRACE("Push 100%");
    std::unique_ptr<AudioBus> bus = AudioBus::Create(kChannels, kMaxFrameCount);
    EXPECT_EQ(fifo.frames(), 0u);
    fifo.Push(bus.get());
    EXPECT_EQ(fifo.frames(), static_cast<size_t>(bus->frames()));
    fifo.Clear();
  }
}

// Consumes audio bus objects from a FIFO and empty it to different degrees.
TEST_F(AudioFifoTest, Consume) {
  static const int kChannels = 2;
  static const size_t kMaxFrameCount = 128;
  AudioFifo fifo(kChannels, kMaxFrameCount);
  {
    std::unique_ptr<AudioBus> bus = AudioBus::Create(kChannels, kMaxFrameCount);
    fifo.Push(bus.get());
    EXPECT_EQ(fifo.frames(), kMaxFrameCount);
  }
  {
    SCOPED_TRACE("Consume 50%");
    static constexpr size_t kHalfFrameCount = kMaxFrameCount / 2;
    std::unique_ptr<AudioBus> bus =
        AudioBus::Create(kChannels, kHalfFrameCount);
    fifo.Consume(bus.get(), 0, bus->frames());
    EXPECT_EQ(fifo.frames(), kHalfFrameCount);
    fifo.Push(bus.get());
    EXPECT_EQ(fifo.frames(), kMaxFrameCount);
  }
  {
    SCOPED_TRACE("Consume 100%");
    std::unique_ptr<AudioBus> bus = AudioBus::Create(kChannels, kMaxFrameCount);
    fifo.Consume(bus.get(), 0, bus->frames());
    EXPECT_EQ(fifo.frames(), 0u);
    fifo.Push(bus.get());
    EXPECT_EQ(fifo.frames(), kMaxFrameCount);
  }
}

TEST_F(AudioFifoTest, ConsumeWithStartFrame) {
  static constexpr int kChannels = 2;
  static constexpr size_t kMaxFrameCount = 128;
  AudioFifo fifo(kChannels, kMaxFrameCount);
  std::unique_ptr<AudioBus> bus = AudioBus::Create(kChannels, kMaxFrameCount);

  // Fill the fifo with `kTestValue`.
  static constexpr float kTestValue = 0.5f;
  for (auto channel : bus->AllChannels()) {
    std::ranges::fill(channel, kTestValue);
  }
  fifo.Push(bus.get());

  static constexpr size_t kOffset = 10;
  static constexpr size_t kCount = 5;

  // Fill `output_bus` with an offset.
  bus->Zero();
  fifo.Consume(bus.get(), kOffset, kCount);

  // `kTestValue` should only be present at `kOffset`, for `kCount` values.
  for (auto channel : bus->AllChannels()) {
    VerifyValue(channel.first(kOffset), 0);
    VerifyValue(channel.subspan(kOffset, kCount), kTestValue);
    VerifyValue(channel.subspan(kOffset + kCount), 0);
  }
}

// Verify that the frames() method of the FIFO works as intended while
// appending and removing audio bus elements to/from the FIFO.
TEST_F(AudioFifoTest, FramesInFifo) {
  static constexpr int kChannels = 2;
  static constexpr size_t kMaxFrameCount = 64;
  AudioFifo fifo(kChannels, kMaxFrameCount);

  // Fill up the FIFO and verify that the size grows as it should while adding
  // one audio frame each time.
  std::unique_ptr<AudioBus> bus = AudioBus::Create(kChannels, 1);
  size_t n = 0;
  while (fifo.frames() < kMaxFrameCount) {
    fifo.Push(bus.get());
    EXPECT_EQ(fifo.frames(), ++n);
  }
  EXPECT_EQ(fifo.frames(), kMaxFrameCount);

  // Empty the FIFO and verify that the size decreases as it should.
  // Reduce the size of the FIFO by one frame each time.
  while (fifo.frames() > 0u) {
    fifo.Consume(bus.get(), 0, bus->frames());
    EXPECT_EQ(fifo.frames(), --n);
  }
  EXPECT_EQ(fifo.frames(), 0u);

  // Verify that a steady-state size of #frames in the FIFO is maintained
  // during a sequence of Push/Consume calls which involves wrapping. We
  // ensure wrapping by selecting a buffer size which does divides the FIFO
  // size with a remainder of one.
  std::unique_ptr<AudioBus> bus2 =
      AudioBus::Create(kChannels, (kMaxFrameCount / 4) - 1);
  const size_t frames_in_fifo = static_cast<size_t>(bus2->frames());
  fifo.Push(bus2.get());
  EXPECT_EQ(fifo.frames(), frames_in_fifo);
  for (n = 0u; n < kMaxFrameCount; ++n) {
    fifo.Push(bus2.get());
    fifo.Consume(bus2.get(), 0, frames_in_fifo);
    EXPECT_EQ(fifo.frames(), frames_in_fifo);
  }
}

// Perform a sequence of Push/Consume calls and verify that the data written
// to the FIFO is correctly retrieved, i.e., that the order is correct and the
// values are correct.
TEST_F(AudioFifoTest, VerifyDataValues) {
  static constexpr int kChannels = 2;
  static constexpr size_t kFrameCount = 2;
  static constexpr size_t kFifoFrameCount = 5 * kFrameCount;

  AudioFifo fifo(kChannels, kFifoFrameCount);
  std::unique_ptr<AudioBus> bus = AudioBus::Create(kChannels, kFrameCount);

  // Start by filling up the FIFO with audio frames. The first audio frame
  // will contain all 1's, the second all 2's etc. All channels contain the
  // same value.
  int value = 1;
  while (fifo.frames() < kFifoFrameCount) {
    for (auto channel : bus->AllChannels()) {
      std::ranges::fill(channel, value);
    }
    fifo.Push(bus.get());
    EXPECT_EQ(fifo.frames(), static_cast<size_t>(bus->frames() * value));
    ++value;
  }

  // FIFO should be full now.
  EXPECT_EQ(fifo.frames(), kFifoFrameCount);

  // Consume all audio frames in the FIFO and verify that the stored values
  // are correct. In this example, we shall read out: 1, 2, 3, 4, 5 in that
  // order. Note that we set |frames_to_consume| to half the size of the bus.
  // It means that we shall read out the same value two times in row.
  value = 1;
  int n = 1;
  const size_t frames_to_consume = bus->frames() / 2;
  while (fifo.frames() > 0) {
    fifo.Consume(bus.get(), 0, frames_to_consume);
    for (auto channel : bus->AllChannels()) {
      VerifyValue(channel.first(frames_to_consume), value);
    }
    if (n++ % 2 == 0) {
      ++value;  // counts 1, 1, 2, 2, 3, 3,...
    }
  }

  // FIFO should be empty now.
  EXPECT_EQ(fifo.frames(), 0u);

  // Push one audio bus to the FIFO and fill it with 1's.
  value = 1;
  for (auto channel : bus->AllChannels()) {
    std::ranges::fill(channel, value);
  }
  fifo.Push(bus.get());
  EXPECT_EQ(fifo.frames(), static_cast<size_t>(bus->frames()));

  // Keep calling Consume/Push a few rounds and verify that we read out the
  // correct values. The number of elements shall be fixed (kFrameCount)
  // during this phase.
  for (size_t i = 0; i < 5 * kFifoFrameCount; i++) {
    fifo.Consume(bus.get(), 0, bus->frames());
    for (auto channel : bus->AllChannels()) {
      VerifyValue(channel, value);
      std::ranges::fill(channel, value + 1);
    }
    fifo.Push(bus.get());
    EXPECT_EQ(fifo.frames(), static_cast<size_t>(bus->frames()));
    ++value;
  }
}

}  // namespace media
