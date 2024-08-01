// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>
#include <memory>

#include "base/time/time.h"
#include "media/base/audio_block_fifo.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioBlockFifoTest : public testing::Test {
 public:
  AudioBlockFifoTest() = default;

  AudioBlockFifoTest(const AudioBlockFifoTest&) = delete;
  AudioBlockFifoTest& operator=(const AudioBlockFifoTest&) = delete;

  ~AudioBlockFifoTest() override = default;

  void PushAndVerify(AudioBlockFifo* fifo,
                     int frames_to_push,
                     int channels,
                     int block_frames,
                     int max_frames) {
    for (int filled_frames = max_frames - fifo->GetUnfilledFrames();
         filled_frames + frames_to_push <= max_frames;) {
      Push(fifo, frames_to_push, channels);
      filled_frames += frames_to_push;
      EXPECT_EQ(max_frames - filled_frames, fifo->GetUnfilledFrames());
      EXPECT_EQ(static_cast<int>(filled_frames / block_frames),
                fifo->available_blocks());
    }
  }

  void Push(AudioBlockFifo* fifo, int frames_to_push, int channels) {
    DCHECK_LE(frames_to_push, fifo->GetUnfilledFrames());
    const int bytes_per_sample = 2;
    const int data_byte_size = bytes_per_sample * channels * frames_to_push;
    auto data = std::make_unique<uint8_t[]>(data_byte_size);
    memset(data.get(), 1, data_byte_size);
    fifo->Push(data.get(), frames_to_push, bytes_per_sample);
  }

  void ConsumeAndVerify(AudioBlockFifo* fifo,
                        int expected_unfilled_frames,
                        int expected_available_blocks) {
    const AudioBus* bus = fifo->Consume();
    EXPECT_EQ(fifo->GetUnfilledFrames(), expected_unfilled_frames);
    EXPECT_EQ(fifo->available_blocks(), expected_available_blocks);

    // Verify the audio data is not 0.
    for (int i = 0; i < bus->channels(); ++i) {
      EXPECT_GT(bus->channel(i)[0], 0.0f);
      EXPECT_GT(bus->channel(i)[bus->frames() - 1], 0.0f);
    }
  }
};

// Verify that construction works as intended.
TEST_F(AudioBlockFifoTest, Construct) {
  const int channels = 6;
  const int frames = 128;
  const int blocks = 4;
  AudioBlockFifo fifo(channels, frames, blocks);
  EXPECT_EQ(0, fifo.available_blocks());
  EXPECT_EQ(frames * blocks, fifo.GetUnfilledFrames());
}

// Pushes audio bus objects to/from a FIFO up to different degrees.
TEST_F(AudioBlockFifoTest, Push) {
  const int channels = 2;
  const int frames = 128;
  const int blocks = 2;
  AudioBlockFifo fifo(channels, frames, blocks);

  // Push frames / 2 of data until FIFO is full.
  PushAndVerify(&fifo, frames / 2, channels, frames, frames * blocks);
  fifo.Clear();

  // Push frames of data until FIFO is full.
  PushAndVerify(&fifo, frames, channels, frames, frames * blocks);
  fifo.Clear();

  // Push 1.5 * frames of data.
  PushAndVerify(&fifo, frames * 1.5, channels, frames, frames * blocks);
  fifo.Clear();
}

// Perform a sequence of Push/Consume calls to different degrees, and verify
// things are correct.
TEST_F(AudioBlockFifoTest, PushAndConsume) {
  const int channels = 2;
  const int frames = 441;
  const int blocks = 4;
  AudioBlockFifo fifo(channels, frames, blocks);
  PushAndVerify(&fifo, frames, channels, frames, frames * blocks);
  EXPECT_TRUE(fifo.GetUnfilledFrames() == 0);
  EXPECT_TRUE(fifo.available_blocks() == blocks);

  // Consume 1 block of data.
  const AudioBus* bus = fifo.Consume();
  EXPECT_TRUE(channels == bus->channels());
  EXPECT_TRUE(frames == bus->frames());
  EXPECT_TRUE(fifo.available_blocks() == (blocks - 1));
  EXPECT_TRUE(fifo.GetUnfilledFrames() == frames);

  // Fill it up again.
  PushAndVerify(&fifo, frames, channels, frames, frames * blocks);
  EXPECT_TRUE(fifo.GetUnfilledFrames() == 0);
  EXPECT_TRUE(fifo.available_blocks() == blocks);

  // Consume all blocks of data.
  for (int i = 1; i <= blocks; ++i) {
    bus = fifo.Consume();
    EXPECT_TRUE(channels == bus->channels());
    EXPECT_TRUE(frames == bus->frames());
    EXPECT_TRUE(fifo.GetUnfilledFrames() == frames * i);
    EXPECT_TRUE(fifo.available_blocks() == (blocks - i));
  }
  EXPECT_TRUE(fifo.GetUnfilledFrames() == frames * blocks);
  EXPECT_TRUE(fifo.available_blocks() == 0);

  fifo.Clear();
  int new_push_frames = 128;
  // Change the input frame and try to fill up the FIFO.
  PushAndVerify(&fifo, new_push_frames, channels, frames, frames * blocks);
  EXPECT_TRUE(fifo.GetUnfilledFrames() != 0);
  EXPECT_TRUE(fifo.available_blocks() == blocks - 1);

  // Consume all the existing filled blocks of data.
  while (fifo.available_blocks()) {
    bus = fifo.Consume();
    EXPECT_TRUE(channels == bus->channels());
    EXPECT_TRUE(frames == bus->frames());
  }

  // Since one block of FIFO has not been completely filled up, there should
  // be remaining frames.
  const int number_of_push =
      static_cast<int>(frames * blocks / new_push_frames);
  const int remain_frames = frames * blocks - fifo.GetUnfilledFrames();
  EXPECT_EQ(number_of_push * new_push_frames - frames * (blocks - 1),
            remain_frames);

  // Completely fill up the buffer again.
  new_push_frames = frames * blocks - remain_frames;
  PushAndVerify(&fifo, new_push_frames, channels, frames, frames * blocks);
  EXPECT_TRUE(fifo.GetUnfilledFrames() == 0);
  EXPECT_TRUE(fifo.available_blocks() == blocks);
}

// Perform a sequence of Push/Consume calls to a 1 block FIFO.
TEST_F(AudioBlockFifoTest, PushAndConsumeOneBlockFifo) {
  static const int channels = 2;
  static const int frames = 441;
  static const int blocks = 1;
  AudioBlockFifo fifo(channels, frames, blocks);
  PushAndVerify(&fifo, frames, channels, frames, frames * blocks);
  EXPECT_TRUE(fifo.GetUnfilledFrames() == 0);
  EXPECT_TRUE(fifo.available_blocks() == blocks);

  // Consume 1 block of data.
  const AudioBus* bus = fifo.Consume();
  EXPECT_TRUE(channels == bus->channels());
  EXPECT_TRUE(frames == bus->frames());
  EXPECT_TRUE(fifo.available_blocks() == 0);
  EXPECT_TRUE(fifo.GetUnfilledFrames() == frames);
}

TEST_F(AudioBlockFifoTest, PushAndConsumeSilence) {
  static const int channels = 2;
  static const int frames = 441;
  static const int blocks = 2;
  AudioBlockFifo fifo(channels, frames, blocks);
  // First push non-zero data.
  Push(&fifo, frames, channels);
  // Then push silence.
  fifo.PushSilence(frames);
  EXPECT_TRUE(fifo.GetUnfilledFrames() == 0);
  EXPECT_TRUE(fifo.available_blocks() == blocks);

  // Consume two blocks of data. The first should not be zero, but the second
  // should be.
  EXPECT_FALSE(fifo.Consume()->AreFramesZero());
  EXPECT_TRUE(fifo.Consume()->AreFramesZero());
}

// Dynamically increase the capacity of FIFO and verify buffers are correct.
TEST_F(AudioBlockFifoTest, DynamicallyIncreaseCapacity) {
  // Create a FIFO with default blocks of buffers.
  const int channels = 2;
  const int frames = 441;
  const int default_blocks = 2;
  AudioBlockFifo fifo(channels, frames, default_blocks);
  Push(&fifo, frames, channels);
  int expected_unfilled_frames = frames;
  int expected_available_blocks = 1;
  EXPECT_EQ(expected_unfilled_frames, fifo.GetUnfilledFrames());
  EXPECT_EQ(expected_available_blocks, fifo.available_blocks());

  // Increase the capacity dynamically for the first time.
  const int new_blocks_1 = 3;
  fifo.IncreaseCapacity(new_blocks_1);
  expected_unfilled_frames += new_blocks_1 * frames;
  EXPECT_EQ(fifo.GetUnfilledFrames(), expected_unfilled_frames);
  EXPECT_EQ(fifo.available_blocks(), expected_available_blocks);

  // Verify the previous buffer is not affected by the dynamic capacity
  // increment.
  expected_unfilled_frames += frames;
  expected_available_blocks -= 1;
  ConsumeAndVerify(&fifo, expected_unfilled_frames, expected_available_blocks);

  // Fill another |new_blocks_1 + 0.5| blocks of data to the FIFO.
  const int frames_to_push = static_cast<int>((new_blocks_1 + 0.5) * frames);
  int max_frames = frames * (default_blocks + new_blocks_1);
  Push(&fifo, frames_to_push, channels);
  expected_unfilled_frames = max_frames - frames_to_push;
  expected_available_blocks = new_blocks_1;
  EXPECT_EQ(fifo.GetUnfilledFrames(), expected_unfilled_frames);
  EXPECT_EQ(fifo.available_blocks(), expected_available_blocks);

  // Increase the capacity dynamically for the second time.
  const int new_blocks_2 = 2;
  fifo.IncreaseCapacity(new_blocks_2);
  max_frames += new_blocks_2 * frames;
  expected_unfilled_frames += new_blocks_2 * frames;
  EXPECT_EQ(fifo.GetUnfilledFrames(), expected_unfilled_frames);
  EXPECT_EQ(fifo.available_blocks(), expected_available_blocks);

  // Verify the previous buffers are not affected by the dynamic capacity
  // increment.
  while (fifo.available_blocks()) {
    expected_unfilled_frames += frames;
    expected_available_blocks -= 1;
    ConsumeAndVerify(&fifo, expected_unfilled_frames,
                     expected_available_blocks);
  }

  // Fill up one block of buffer and consume it, FIFO should then be empty.
  const int available_frames = max_frames - expected_unfilled_frames;
  Push(&fifo, frames - available_frames, channels);
  ConsumeAndVerify(&fifo, max_frames, 0);
}

}  // namespace media
