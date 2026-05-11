// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/flac_audio_handler.h"

#include <algorithm>
#include <cstddef>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/types/zip.h"
#include "media/audio/test_data.h"
#include "media/base/audio_bus.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
constexpr int kDefaultFrameCount = 1024;
}

// Tests if the decoder can decode flac audio to the same content for the first
// bus after calling `Reset`.
TEST(FlacAudioHandlerTest, SampleDataTest) {
  std::string bitstream;
  const base::FilePath file_path = GetTestDataFilePath("bear.flac");
  EXPECT_TRUE(base::ReadFileToString(file_path, &bitstream));

  FlacAudioHandler handler(bitstream);
  ASSERT_TRUE(handler.Initialize());

  auto bus1 = AudioBus::Create(handler.GetNumChannels(), kDefaultFrameCount);
  size_t frames_written1 = 0u;
  ASSERT_TRUE(handler.CopyTo(bus1.get(), &frames_written1));

  // Reset the decoder and re-decode the file from its head again.
  handler.Reset();

  auto bus2 = AudioBus::Create(handler.GetNumChannels(), kDefaultFrameCount);
  size_t frames_written2 = 0u;
  ASSERT_TRUE(handler.CopyTo(bus2.get(), &frames_written2));

  ASSERT_EQ(frames_written1, frames_written2);

  // Compare the content in two buses.
  for (auto [channel_1, channel_2] :
       base::zip(bus1->AllChannels(), bus2->AllChannels())) {
    for (int s = 0; s < bus1->frames(); ++s) {
      ASSERT_FLOAT_EQ(channel_1[s], channel_2[s]);
    }
  }
}

// Tests that multiple sequential partial copies perfectly match a single full
// copy.
TEST(FlacAudioHandlerTest, CopyPartialFramesTo) {
  std::string bitstream;
  const base::FilePath file_path = GetTestDataFilePath("bear.flac");
  ASSERT_TRUE(base::ReadFileToString(file_path, &bitstream));

  FlacAudioHandler handler(bitstream);
  ASSERT_TRUE(handler.Initialize());

  // Get the baseline from a single full CopyTo().
  auto bus_expected =
      AudioBus::Create(handler.GetNumChannels(), kDefaultFrameCount);
  size_t expected_frames_written = 0u;
  ASSERT_TRUE(handler.CopyTo(bus_expected.get(), &expected_frames_written));

  // Reset to decode the same section again.
  handler.Reset();

  // Fill a second bus using multiple partial copies at varying offsets.
  auto bus_actual =
      AudioBus::Create(handler.GetNumChannels(), kDefaultFrameCount);
  size_t total_frames_written = 0u;

  constexpr int kChunkSize = 256;
  int frames_remaining = kDefaultFrameCount;
  int current_offset = 0;

  while (frames_remaining > 0) {
    int frames_to_request = std::min(kChunkSize, frames_remaining);
    size_t frames_written = 0;

    ASSERT_TRUE(handler.CopyPartialFramesTo(bus_actual.get(), frames_to_request,
                                            current_offset, &frames_written));

    ASSERT_EQ(frames_written, static_cast<size_t>(frames_to_request));

    total_frames_written += frames_written;
    current_offset += frames_written;
    frames_remaining -= frames_written;
  }

  // Compare the contents of the two buses.
  ASSERT_EQ(expected_frames_written, total_frames_written);
  for (auto [channel_actual, channel_expected] :
       base::zip(bus_actual->AllChannels(), bus_expected->AllChannels())) {
    EXPECT_EQ(channel_actual, channel_expected);
  }
}

// Tests that when reaching the end of the stream during a copy, only the
// remainder of the requested frame block is zeroed out, leaving the rest
// of the bus untouched.
TEST(FlacAudioHandlerTest, CopyPartialFramesToZeroesOnlyLeftoverFrames) {
  std::string bitstream;
  const base::FilePath file_path = GetTestDataFilePath("bear.flac");
  ASSERT_TRUE(base::ReadFileToString(file_path, &bitstream));

  FlacAudioHandler handler(bitstream);
  ASSERT_TRUE(handler.Initialize());

  const int total_frames = handler.total_frames_for_testing();
  ASSERT_GT(total_frames, 2);

  // Consume all but the last 2 frames to position the handler right before EOF.
  const int frames_to_consume = total_frames - 2;
  auto dump_bus = AudioBus::Create(handler.GetNumChannels(), frames_to_consume);
  size_t frames_written = 0;
  ASSERT_TRUE(handler.CopyPartialFramesTo(dump_bus.get(), frames_to_consume,
                                          /*bus_start_frame=*/0,
                                          &frames_written));
  ASSERT_EQ(frames_written, static_cast<size_t>(frames_to_consume));
  ASSERT_FALSE(handler.AtEnd());

  // Create a bus with 6 frames. Fill it with a dummy signal (1.0f).
  auto bus = AudioBus::Create(handler.GetNumChannels(), /*frames=*/6);
  for (auto channel : bus->AllChannels()) {
    std::ranges::fill(channel, 1.0f);
  }

  // Request 4 frames at offset 1.
  // There are only 2 frames left in the stream, so it should write 2 frames,
  // zero out the next 2 frames, and leave the rest of the bus untouched.
  ASSERT_TRUE(handler.CopyPartialFramesTo(
      bus.get(), /*frame_count=*/4, /*bus_start_frame=*/1, &frames_written));

  // The handler should report 2 valid frames read.
  ASSERT_EQ(frames_written, 2u);
  ASSERT_TRUE(handler.AtEnd());

  for (auto channel : bus->AllChannels()) {
    // Frame 0: Untouched (`bus_start_frame=1`).
    EXPECT_FLOAT_EQ(channel[0], 1.0f);

    // Frame 1, 2: Valid data from FLAC (assume it's not exactly 1.0f).
    EXPECT_NE(channel[1], 1.0f);
    EXPECT_NE(channel[2], 1.0f);

    // Frame 3, 4: Zeroed because we asked for 4 frames but only had 2.
    EXPECT_FLOAT_EQ(channel[3], 0.0f);
    EXPECT_FLOAT_EQ(channel[4], 0.0f);

    // Frame 5: Untouched (past the requested block).
    EXPECT_FLOAT_EQ(channel[5], 1.0f);
  }
}

// Tests if the input is non-flac audio.
TEST(FlacAudioHandlerTest, BadSampleDataTest) {
  // Set the wav audio data.
  const std::string data(kTestAudioData, kTestAudioDataSize);

  FlacAudioHandler handler(data);
  ASSERT_FALSE(handler.Initialize());
}

}  // namespace media
