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

// Tests if the input is non-flac audio.
TEST(FlacAudioHandlerTest, BadSampleDataTest) {
  // Set the wav audio data.
  const std::string data(kTestAudioData, kTestAudioDataSize);

  FlacAudioHandler handler(data);
  ASSERT_FALSE(handler.Initialize());
}

}  // namespace media
