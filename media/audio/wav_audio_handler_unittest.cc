// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/wav_audio_handler.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "media/audio/test_data.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {
// WAV header comes first in the test data.
const size_t kWavHeaderSize = 12;
const size_t kWavDataSizeIndex = 4;

// "fmt " header comes next.
const size_t kFormatHeaderIndex = kWavHeaderSize;
const size_t kFormatHeaderSize = 8;
const size_t kFormatPayloadSize = 16;
const size_t kChannelIndex = kWavHeaderSize + kFormatHeaderSize + 2;
const size_t kBitsPerSampleIndex = kWavHeaderSize + kFormatHeaderSize + 14;
const size_t kSampleRateIndex = kWavHeaderSize + kFormatHeaderSize + 4;

// "data" header comes last.
const size_t kDataHeaderIndex =
    kWavHeaderSize + kFormatHeaderSize + kFormatPayloadSize;

}  // namespace

TEST(WavAudioHandlerTest, SampleDataTest) {
  std::string data(kTestAudioData, kTestAudioDataSize);
  auto handler = WavAudioHandler::Create(data);
  ASSERT_TRUE(handler);
  ASSERT_EQ(2u, handler->num_channels());
  ASSERT_EQ(16u, handler->bits_per_sample());
  ASSERT_EQ(48000u, handler->sample_rate());
  ASSERT_EQ(1u, handler->total_frames());
  ASSERT_EQ(20u, handler->GetDuration().InMicroseconds());

  ASSERT_EQ(4U, handler->data().size());
  const char kData[] = "\x01\x00\x01\x00";
  ASSERT_EQ(base::StringPiece(kData, base::size(kData) - 1), handler->data());

  std::unique_ptr<AudioBus> bus =
      AudioBus::Create(handler->num_channels(),
                       handler->data().size() / handler->num_channels());

  size_t bytes_written = 0u;
  ASSERT_TRUE(handler->CopyTo(bus.get(), 0, &bytes_written));
  ASSERT_EQ(static_cast<size_t>(handler->data().size()), bytes_written);
}

TEST(WavAudioHandlerTest, SampleExtensibleDataTest) {
  std::string data(kTestExtensibleAudioData, kTestExtensibleAudioDataSize);
  auto handler = WavAudioHandler::Create(data);
  ASSERT_TRUE(handler);
  ASSERT_EQ(2u, handler->num_channels());
  ASSERT_EQ(32u, handler->bits_per_sample());
  ASSERT_EQ(48000u, handler->sample_rate());
  ASSERT_EQ(1u, handler->total_frames());
  ASSERT_EQ(20u, handler->GetDuration().InMicroseconds());

  ASSERT_EQ(8U, handler->data().size());
  const char kData[] = "\x01\x00\x00\x00\x01\x00\x00\x00";
  ASSERT_EQ(base::StringPiece(kData, base::size(kData) - 1), handler->data());

  std::unique_ptr<AudioBus> bus =
      AudioBus::Create(handler->num_channels(),
                       handler->data().size() / handler->num_channels());

  size_t bytes_written = 0u;
  ASSERT_TRUE(handler->CopyTo(bus.get(), 0, &bytes_written));
  ASSERT_EQ(static_cast<size_t>(handler->data().size()), bytes_written);
}

TEST(WavAudioHandlerTest, TestZeroChannelsIsNotValid) {
  // Read in the sample data and modify the channel field to hold |00|00|.
  std::string data(kTestAudioData, kTestAudioDataSize);
  data[kChannelIndex] = '\x00';
  data[kChannelIndex + 1] = '\x00';
  auto handler = WavAudioHandler::Create(data);
  EXPECT_FALSE(handler);
}

TEST(WavAudioHandlerTest, TestZeroBitsPerSampleIsNotValid) {
  // Read in the sample data and modify the bits_per_sample field to hold
  // |00|00|.
  std::string data(kTestAudioData, kTestAudioDataSize);
  data[kBitsPerSampleIndex] = '\x00';
  data[kBitsPerSampleIndex + 1] = '\x00';
  auto handler = WavAudioHandler::Create(data);
  EXPECT_FALSE(handler);
}

TEST(WavAudioHandlerTest, TestZeroSamplesPerSecondIsNotValid) {
  // Read in the sample data and modify the bits_per_sample field to hold
  // |00|00|.
  std::string data(kTestAudioData, kTestAudioDataSize);
  data[kSampleRateIndex] = '\x00';
  data[kSampleRateIndex + 1] = '\x00';
  data[kSampleRateIndex + 2] = '\x00';
  data[kSampleRateIndex + 3] = '\x00';
  auto handler = WavAudioHandler::Create(data);
  EXPECT_FALSE(handler);
}

TEST(WavAudioHandlerTest, TestTooBigTotalSizeIsOkay) {
  // The size filed in the header should hold a very big number.
  std::string data(kTestAudioData, kTestAudioDataSize);
  data[kWavDataSizeIndex] = '\x00';
  data[kWavDataSizeIndex + 1] = '\xFF';
  data[kWavDataSizeIndex + 2] = '\xFF';
  data[kWavDataSizeIndex + 3] = '\x00';
  auto handler = WavAudioHandler::Create(data);
  EXPECT_TRUE(handler);
  ASSERT_EQ(2u, handler->num_channels());
  ASSERT_EQ(16u, handler->bits_per_sample());
  ASSERT_EQ(48000u, handler->sample_rate());
  ASSERT_EQ(1u, handler->total_frames());
  ASSERT_EQ(20u, handler->GetDuration().InMicroseconds());

  ASSERT_EQ(4U, handler->data().size());
  const char kData[] = "\x01\x00\x01\x00";
  ASSERT_EQ(base::StringPiece(kData, base::size(kData) - 1), handler->data());
}

TEST(WavAudioHandlerTest, TestTooBigDataChunkSizeIsOkay) {
  // If the |data| chunk size is last and it indicates it has more than it
  // actually does, that's okay. Just consume the rest of the string. If it
  // is not the last subsection, this file will parse badly.
  std::string data(kTestAudioData, kTestAudioDataSize);
  data[kDataHeaderIndex + 4] = '\x00';
  data[kDataHeaderIndex + 5] = '\xFF';
  data[kDataHeaderIndex + 6] = '\xFF';
  data[kDataHeaderIndex + 7] = '\x00';
  auto handler = WavAudioHandler::Create(data);
  EXPECT_TRUE(handler);
  ASSERT_EQ(2u, handler->num_channels());
  ASSERT_EQ(16u, handler->bits_per_sample());
  ASSERT_EQ(48000u, handler->sample_rate());
  ASSERT_EQ(1u, handler->total_frames());
  ASSERT_EQ(20u, handler->GetDuration().InMicroseconds());

  ASSERT_EQ(4U, handler->data().size());
  const char kData[] = "\x01\x00\x01\x00";
  ASSERT_EQ(base::StringPiece(kData, base::size(kData) - 1), handler->data());
}

TEST(WavAudioHandlerTest, TestTooSmallFormatSizeIsNotValid) {
  // If the |data| chunk size is last and it indicates it has more than it
  // actually does, that's okay. Just consume the rest of the string. If it
  // is not the last subsection, this file will parse badly.
  std::string data(kTestAudioData, kTestAudioDataSize);
  data[kFormatHeaderIndex + 4] = '\x04';
  data[kFormatHeaderIndex + 5] = '\x00';
  data[kFormatHeaderIndex + 6] = '\x00';
  data[kFormatHeaderIndex + 7] = '\x00';
  auto handler = WavAudioHandler::Create(data);
  EXPECT_FALSE(handler);
}

TEST(WavAudioHandlerTest, TestOtherSectionTypesIsOkay) {
  // Append some other subsection header "abcd", the class should just consume
  // and keep going.
  std::string data(kTestAudioData, kTestAudioDataSize);
  data.append("abcd\x04\x00\x00\x00\x01\x02\x03\x04");
  data[kWavDataSizeIndex] += 12;  // This should not overflow.

  auto handler = WavAudioHandler::Create(data);
  EXPECT_TRUE(handler);
  ASSERT_EQ(2u, handler->num_channels());
  ASSERT_EQ(16u, handler->bits_per_sample());
  ASSERT_EQ(48000u, handler->sample_rate());
  ASSERT_EQ(1u, handler->total_frames());
  ASSERT_EQ(20u, handler->GetDuration().InMicroseconds());
  ASSERT_EQ(4u, handler->data().size());
}

TEST(WavAudioHandlerTest, TestNoFmtSectionIsNotValid) {
  // Write over the "fmt " header. No valid handler should be returned.
  std::string data(kTestAudioData, kTestAudioDataSize);
  data[kFormatHeaderIndex] = 'a';
  data[kFormatHeaderIndex + 1] = 'b';
  data[kFormatHeaderIndex + 2] = 'c';
  data[kFormatHeaderIndex + 3] = 'd';
  auto handler = WavAudioHandler::Create(data);
  EXPECT_FALSE(handler);
}

TEST(WavAudioHandlerTest, TestNoDataSectionIsOkay) {
  // This one could go both ways. But for now, let's say that it's okay not
  // to have a "data" section - just make sure everything is zeroed out as it
  // should be.
  std::string data(kTestAudioData, kTestAudioDataSize);
  data[kDataHeaderIndex] = 'a';
  data[kDataHeaderIndex + 1] = 'b';
  data[kDataHeaderIndex + 2] = 'c';
  data[kDataHeaderIndex + 3] = 'd';
  auto handler = WavAudioHandler::Create(data);
  EXPECT_TRUE(handler);
  ASSERT_EQ(2u, handler->num_channels());
  ASSERT_EQ(16u, handler->bits_per_sample());
  ASSERT_EQ(48000u, handler->sample_rate());
  ASSERT_EQ(0u, handler->total_frames());
  ASSERT_EQ(0u, handler->GetDuration().InMicroseconds());
  ASSERT_EQ(0u, handler->data().size());
}

}  // namespace media
