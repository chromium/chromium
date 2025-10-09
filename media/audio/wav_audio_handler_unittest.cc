// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/wav_audio_handler.h"

#include <stddef.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "media/audio/test_data.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {
// WAV header comes first in the test data.
constexpr size_t kWavHeaderSize = 12;
constexpr size_t kWavDataSizeIndex = 4;

// "fmt " header comes next.
constexpr size_t kFormatHeaderIndex = kWavHeaderSize;
constexpr size_t kFormatHeaderSize = 8;
constexpr size_t kFormatPayloadSize = 16;
constexpr size_t kChannelIndex = kWavHeaderSize + kFormatHeaderSize + 2;
constexpr size_t kBitsPerSampleIndex = kWavHeaderSize + kFormatHeaderSize + 14;
constexpr size_t kSampleRateIndex = kWavHeaderSize + kFormatHeaderSize + 4;

// "data" header comes last.
constexpr size_t kDataHeaderIndex =
    kWavHeaderSize + kFormatHeaderSize + kFormatPayloadSize;

}  // namespace

TEST(WavAudioHandlerTest, SampleDataTest) {
  std::string data(kTestAudioData, kTestAudioDataSize);
  auto handler = WavAudioHandler::Create(base::as_byte_span(data));
  ASSERT_TRUE(handler);
  ASSERT_EQ(2, handler->GetNumChannels());
  ASSERT_EQ(16, handler->bits_per_sample_for_testing());
  ASSERT_EQ(48000, handler->GetSampleRate());
  ASSERT_EQ(1, handler->total_frames_for_testing());
  ASSERT_EQ(20u, handler->GetDuration().InMicroseconds());

  ASSERT_EQ(4U, handler->data().size());
  const char kData[] = "\x01\x00\x01\x00";
  ASSERT_EQ(base::byte_span_from_cstring(kData), handler->data());

  std::unique_ptr<AudioBus> bus =
      AudioBus::Create(handler->GetNumChannels(),
                       handler->data().size() / handler->GetNumChannels());

  size_t frames_written = 0u;
  ASSERT_TRUE(handler->CopyTo(bus.get(), &frames_written));
  const int bytes_per_frame =
      handler->GetNumChannels() * handler->bits_per_sample_for_testing() / 8;
  ASSERT_EQ(static_cast<size_t>(handler->data().size()),
            frames_written * bytes_per_frame);
}

TEST(WavAudioHandlerTest, SampleFloatDataTest) {
  std::string data(kTestFloatAudioData, kTestFloatAudioDataSize);
  auto handler = WavAudioHandler::Create(base::as_byte_span(data));
  ASSERT_TRUE(handler);
  ASSERT_EQ(2, handler->GetNumChannels());
  ASSERT_EQ(32, handler->bits_per_sample_for_testing());
  ASSERT_EQ(48000, handler->GetSampleRate());
  ASSERT_EQ(1, handler->total_frames_for_testing());
  ASSERT_EQ(20u, handler->GetDuration().InMicroseconds());

  ASSERT_EQ(8U, handler->data().size());
  const char kData[] = "\x00\x01\x00\x00\x01\x00\x00\x00";
  ASSERT_EQ(base::byte_span_from_cstring(kData), handler->data());

  std::unique_ptr<AudioBus> bus =
      AudioBus::Create(handler->GetNumChannels(),
                       handler->data().size() / handler->GetNumChannels());

  size_t frames_written = 0u;
  ASSERT_TRUE(handler->CopyTo(bus.get(), &frames_written));
  const int bytes_per_frame =
      handler->GetNumChannels() * handler->bits_per_sample_for_testing() / 8;
  ASSERT_EQ(static_cast<size_t>(handler->data().size()),
            frames_written * bytes_per_frame);
}

TEST(WavAudioHandlerTest, SampleExtensibleDataTest) {
  std::string data(kTestExtensibleAudioData, kTestExtensibleAudioDataSize);
  auto handler = WavAudioHandler::Create(base::as_byte_span(data));
  ASSERT_TRUE(handler);
  ASSERT_EQ(2, handler->GetNumChannels());
  ASSERT_EQ(32, handler->bits_per_sample_for_testing());
  ASSERT_EQ(48000, handler->GetSampleRate());
  ASSERT_EQ(1, handler->total_frames_for_testing());
  ASSERT_EQ(20u, handler->GetDuration().InMicroseconds());

  ASSERT_EQ(8U, handler->data().size());
  const char kData[] = "\x01\x00\x00\x00\x01\x00\x00\x00";
  ASSERT_EQ(base::byte_span_from_cstring(kData), handler->data());

  std::unique_ptr<AudioBus> bus =
      AudioBus::Create(handler->GetNumChannels(),
                       handler->data().size() / handler->GetNumChannels());

  size_t frames_written = 0u;
  ASSERT_TRUE(handler->CopyTo(bus.get(), &frames_written));
  const int bytes_per_frame =
      handler->GetNumChannels() * handler->bits_per_sample_for_testing() / 8;
  ASSERT_EQ(static_cast<size_t>(handler->data().size()),
            frames_written * bytes_per_frame);
}

TEST(WavAudioHandlerTest, TestZeroChannelsIsNotValid) {
  // Read in the sample data and modify the channel field to hold |00|00|.
  std::string data(kTestAudioData, kTestAudioDataSize);
  data[kChannelIndex] = '\x00';
  data[kChannelIndex + 1] = '\x00';
  auto handler = WavAudioHandler::Create(base::as_byte_span(data));
  EXPECT_FALSE(handler);
}

TEST(WavAudioHandlerTest, TestZeroBitsPerSampleIsNotValid) {
  // Read in the sample data and modify the bits_per_sample field to hold
  // |00|00|.
  std::string data(kTestAudioData, kTestAudioDataSize);
  data[kBitsPerSampleIndex] = '\x00';
  data[kBitsPerSampleIndex + 1] = '\x00';
  auto handler = WavAudioHandler::Create(base::as_byte_span(data));
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
  auto handler = WavAudioHandler::Create(base::as_byte_span(data));
  EXPECT_FALSE(handler);
}

TEST(WavAudioHandlerTest, TestTooBigTotalSizeIsOkay) {
  // The size filed in the header should hold a very big number.
  std::string data(kTestAudioData, kTestAudioDataSize);
  data[kWavDataSizeIndex] = '\x00';
  data[kWavDataSizeIndex + 1] = '\xFF';
  data[kWavDataSizeIndex + 2] = '\xFF';
  data[kWavDataSizeIndex + 3] = '\x00';
  auto handler = WavAudioHandler::Create(base::as_byte_span(data));
  EXPECT_TRUE(handler);
  EXPECT_TRUE(handler->Initialize());
  ASSERT_EQ(2, handler->GetNumChannels());
  ASSERT_EQ(16, handler->bits_per_sample_for_testing());
  ASSERT_EQ(48000, handler->GetSampleRate());
  ASSERT_EQ(1, handler->total_frames_for_testing());
  ASSERT_EQ(20u, handler->GetDuration().InMicroseconds());

  ASSERT_EQ(4U, handler->data().size());
  const char kData[] = "\x01\x00\x01\x00";
  ASSERT_EQ(base::byte_span_from_cstring(kData), handler->data());
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
  auto handler = WavAudioHandler::Create(base::as_byte_span(data));
  EXPECT_TRUE(handler);
  EXPECT_TRUE(handler->Initialize());
  ASSERT_EQ(2, handler->GetNumChannels());
  ASSERT_EQ(16, handler->bits_per_sample_for_testing());
  ASSERT_EQ(48000, handler->GetSampleRate());
  ASSERT_EQ(1, handler->total_frames_for_testing());
  ASSERT_EQ(20u, handler->GetDuration().InMicroseconds());

  ASSERT_EQ(4U, handler->data().size());
  const char kData[] = "\x01\x00\x01\x00";
  ASSERT_EQ(base::byte_span_from_cstring(kData), handler->data());
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
  auto handler = WavAudioHandler::Create(base::as_byte_span(data));
  EXPECT_FALSE(handler);
}

TEST(WavAudioHandlerTest, TestNoFmtSectionIsNotValid) {
  // Write over the "fmt " header. No valid handler should be returned.
  std::string data(kTestAudioData, kTestAudioDataSize);
  data[kFormatHeaderIndex] = 'a';
  data[kFormatHeaderIndex + 1] = 'b';
  data[kFormatHeaderIndex + 2] = 'c';
  data[kFormatHeaderIndex + 3] = 'd';
  auto handler = WavAudioHandler::Create(base::as_byte_span(data));
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
  auto handler = WavAudioHandler::Create(base::as_byte_span(data));
  EXPECT_TRUE(handler);
  EXPECT_TRUE(handler->Initialize());
  ASSERT_EQ(2, handler->GetNumChannels());
  ASSERT_EQ(16, handler->bits_per_sample_for_testing());
  ASSERT_EQ(48000, handler->GetSampleRate());
  ASSERT_EQ(0, handler->total_frames_for_testing());
  ASSERT_EQ(0u, handler->GetDuration().InMicroseconds());
  ASSERT_EQ(0u, handler->data().size());
}

// Test extensible format with insufficient data size.
TEST(WavAudioHandlerTest, ExtensibleFormatTooShort) {
  // clang-format off
  constexpr auto kInvalidExtensibleWav = std::to_array<uint8_t>({
      // RIFF header.
      'R', 'I', 'F', 'F',
      0x1C, 0x00, 0x00, 0x00,  // chunk size (28 bytes = 8 + 20 for fmt chunk only).
      'W', 'A', 'V', 'E',
      // fmt chunk with extensible format but insufficient size.
      'f', 'm', 't', ' ',
      0x14, 0x00, 0x00, 0x00,  // chunk size (20 bytes, less than required 40).
      0xFE, 0xFF,              // WAVE_FORMAT_EXTENSIBLE.
      0x02, 0x00,              // 2 channels.
      0x44, 0xAC, 0x00, 0x00,  // 44100 Hz sample rate.
      0x10, 0xB1, 0x02, 0x00,  // byte rate (44100 * 2 * 2 = 176400).
      0x04, 0x00,              // block align (2 channels * 2 bytes).
      0x10, 0x00,              // 16 bits per sample.
      0x10, 0x00,              // valid bits per sample.
      0x00, 0x00, 0x00, 0x00,  // channel mask.
      // Missing subformat GUID (should be 16 more bytes for extensible format).
  });
  // clang-format on

  auto handler = WavAudioHandler::Create(base::span(kInvalidExtensibleWav));
  EXPECT_FALSE(handler);
}

// Test invalid parameters validation.
TEST(WavAudioHandlerTest, InvalidParametersValidation) {
  // Test unsupported PCM bit depth.
  // clang-format off
  constexpr auto kUnsupportedPcmBitsWav = std::to_array<uint8_t>({
      // RIFF header.
      'R', 'I', 'F', 'F',
      0x28, 0x00, 0x00, 0x00,  // chunk size (40 bytes = 8 + 16 + 8 + 8).
      'W', 'A', 'V', 'E',
      // fmt chunk.
      'f', 'm', 't', ' ',
      0x10, 0x00, 0x00, 0x00,  // chunk size (16 bytes).
      0x01, 0x00,              // PCM format.
      0x02, 0x00,              // 2 channels.
      0x44, 0xAC, 0x00, 0x00,  // 44100 Hz sample rate.
      0x38, 0x09, 0x04, 0x00,  // byte rate (44100 * 2 * 3 = 264600).
      0x06, 0x00,              // block align (2 channels * 3 bytes).
      0x18, 0x00,              // 24 bits per sample (not supported for PCM).
      // data chunk.
      'd', 'a', 't', 'a',
      0x08, 0x00, 0x00, 0x00,  // data size (8 bytes).
      0x00, 0x00, 0x00, 0x00,  // sample data (padding to match 24-bit format).
      0x00, 0x00, 0x00, 0x00
  });
  // clang-format on

  auto handler = WavAudioHandler::Create(base::span(kUnsupportedPcmBitsWav));
  EXPECT_FALSE(handler);

  // Test unsupported float bit depth.
  // clang-format off
  constexpr auto kUnsupportedFloatBitsWav = std::to_array<uint8_t>({
      // RIFF header.
      'R', 'I', 'F', 'F',
      0x28, 0x00, 0x00, 0x00,  // chunk size (40 bytes = 8 + 16 + 8 + 8).
      'W', 'A', 'V', 'E',
      // fmt chunk.
      'f', 'm', 't', ' ',
      0x10, 0x00, 0x00, 0x00,  // chunk size (16 bytes).
      0x03, 0x00,              // IEEE float format.
      0x02, 0x00,              // 2 channels.
      0x44, 0xAC, 0x00, 0x00,  // 44100 Hz sample rate.
      0x38, 0x09, 0x04, 0x00,  // byte rate (44100 * 2 * 3 = 264600).
      0x06, 0x00,              // block align (2 channels * 3 bytes).
      0x18, 0x00,              // 24 bits per sample (not supported for float).
      // data chunk.
      'd', 'a', 't', 'a',
      0x08, 0x00, 0x00, 0x00,  // data size (8 bytes).
      0x00, 0x00, 0x00, 0x00,  // sample data (padding to match 24-bit format).
      0x00, 0x00, 0x00, 0x00
  });
  // clang-format on

  auto handler2 = WavAudioHandler::Create(base::span(kUnsupportedFloatBitsWav));
  EXPECT_FALSE(handler2);
}

// Test invalid RIFF chunk ID.
TEST(WavAudioHandlerTest, InvalidRiffChunkId) {
  // clang-format off
  constexpr auto kInvalidRiffWav = std::to_array<uint8_t>({
      // Invalid RIFF header.
      'X', 'I', 'F', 'F',      // Wrong chunk ID (should be "RIFF").
      0x24, 0x00, 0x00, 0x00,  // chunk size (36 bytes = 8 + 16 + 8 + 4).
      'W', 'A', 'V', 'E',      // format.
      // fmt chunk.
      'f', 'm', 't', ' ',
      0x10, 0x00, 0x00, 0x00,  // chunk size (16 bytes).
      0x01, 0x00,              // PCM format.
      0x02, 0x00,              // 2 channels.
      0x44, 0xAC, 0x00, 0x00,  // 44100 Hz sample rate.
      0x10, 0xB1, 0x02, 0x00,  // byte rate (44100 * 2 * 2 = 176400).
      0x04, 0x00,              // block align (2 channels * 2 bytes).
      0x10, 0x00,              // 16 bits per sample.
      // data chunk.
      'd', 'a', 't', 'a',
      0x04, 0x00, 0x00, 0x00,  // data size (4 bytes).
      0x00, 0x00, 0x00, 0x00   // sample data.
  });
  // clang-format on

  auto handler = WavAudioHandler::Create(base::span(kInvalidRiffWav));
  EXPECT_FALSE(handler);
}

// Test missing length field.
TEST(WavAudioHandlerTest, MissingLengthField) {
  // clang-format off
  constexpr auto kTruncatedWav = std::to_array<uint8_t>({
      // RIFF header without length.
      'R', 'I', 'F', 'F'
  });
  // clang-format on

  auto handler = WavAudioHandler::Create(base::span(kTruncatedWav));
  EXPECT_FALSE(handler);
}

// Test invalid format ID.
TEST(WavAudioHandlerTest, InvalidFormatId) {
  // clang-format off
  constexpr auto kInvalidFormatWav = std::to_array<uint8_t>({
      // RIFF header.
      'R', 'I', 'F', 'F',
      0x24, 0x00, 0x00, 0x00,  // chunk size (36 bytes = 8 + 16 + 8 + 4).
      'X', 'A', 'V', 'E',      // Wrong format ID (should be "WAVE").
      // fmt chunk.
      'f', 'm', 't', ' ',
      0x10, 0x00, 0x00, 0x00,  // chunk size (16 bytes).
      0x01, 0x00,              // PCM format.
      0x02, 0x00,              // 2 channels.
      0x44, 0xAC, 0x00, 0x00,  // 44100 Hz sample rate.
      0x10, 0xB1, 0x02, 0x00,  // byte rate (44100 * 2 * 2 = 176400).
      0x04, 0x00,              // block align (2 channels * 2 bytes).
      0x10, 0x00,              // 16 bits per sample.
      // data chunk.
      'd', 'a', 't', 'a',
      0x04, 0x00, 0x00, 0x00,  // data size (4 bytes).
      0x00, 0x00, 0x00, 0x00   // sample data.
  });
  // clang-format on

  auto handler = WavAudioHandler::Create(base::span(kInvalidFormatWav));
  EXPECT_FALSE(handler);
}

// Test missing format chunk.
TEST(WavAudioHandlerTest, MissingFormatChunk) {
  // clang-format off
  constexpr auto kNoFormatWav = std::to_array<uint8_t>({
      // RIFF header.
      'R', 'I', 'F', 'F',
      0x0C, 0x00, 0x00, 0x00,  // chunk size (12 bytes = 8 + 4 for data chunk).
      'W', 'A', 'V', 'E',
      // Only data chunk, no fmt chunk.
      'd', 'a', 't', 'a',
      0x04, 0x00, 0x00, 0x00,  // data size (4 bytes).
      0x00, 0x00, 0x00, 0x00   // sample data.
  });
  // clang-format on

  auto handler = WavAudioHandler::Create(base::span(kNoFormatWav));
  EXPECT_FALSE(handler);
}

// Test accessor methods and basic functionality.
TEST(WavAudioHandlerTest, AccessorMethodsAndBasicFunctionality) {
  // clang-format off
  constexpr auto kValidWav = std::to_array<uint8_t>({
      // RIFF header.
      'R', 'I', 'F', 'F',
      0x24, 0x00, 0x00, 0x00,  // chunk size (36 bytes = 8 + 16 + 8 + 8).
      'W', 'A', 'V', 'E',
      // fmt chunk.
      'f', 'm', 't', ' ',
      0x10, 0x00, 0x00, 0x00,  // chunk size (16 bytes).
      0x01, 0x00,              // PCM format.
      0x02, 0x00,              // 2 channels.
      0x44, 0xAC, 0x00, 0x00,  // 44100 Hz sample rate.
      0x10, 0xB1, 0x02, 0x00,  // byte rate (44100 * 2 * 2 = 176400).
      0x04, 0x00,              // block align (2 channels * 2 bytes).
      0x10, 0x00,              // 16 bits per sample.
      // data chunk.
      'd', 'a', 't', 'a',
      0x08, 0x00, 0x00, 0x00,  // data size (8 bytes).
      0x00, 0x00, 0x01, 0x00,  // sample data (2 frames * 2 channels * 2 bytes).
      0x00, 0x00, 0x01, 0x00
  });
  // clang-format on

  auto handler = WavAudioHandler::Create(base::span(kValidWav));
  ASSERT_TRUE(handler);

  // Test accessor methods.
  EXPECT_EQ(2, handler->GetNumChannels());
  EXPECT_EQ(44100, handler->GetSampleRate());

  // Test GetDuration.
  EXPECT_GT(handler->GetDuration().InMicroseconds(), 0);

  // Test CopyTo functionality.
  auto bus = AudioBus::Create(2, 2);
  size_t frames_written = 0;
  EXPECT_TRUE(handler->CopyTo(bus.get(), &frames_written));
  EXPECT_EQ(2u, frames_written);

  // Test Reset functionality.
  handler->Reset();
  EXPECT_FALSE(handler->AtEnd());
}

// Test 64-bit float format support.
TEST(WavAudioHandlerTest, Float64FormatSupport) {
  // clang-format off
  // Use WAVE_FORMAT_EXTENSIBLE with padding chunk to ensure data is 8-byte aligned.
  // Structure: 12 (RIFF) + 48 (fmt) + 12 (pad with 4 bytes data) + 8 (data header) = 80
  // Data starts at offset 80, which is 8-byte aligned (80 % 8 == 0).
  alignas(8) constexpr auto kFloat64Wav = std::to_array<uint8_t>({
      // RIFF header.
      'R', 'I', 'F', 'F',
      0x68, 0x00, 0x00, 0x00,  // chunk size (104 bytes = 4 + 48 + 12 + 8 + 32).
      'W', 'A', 'V', 'E',
      // fmt chunk with WAVE_FORMAT_EXTENSIBLE (40 bytes data).
      'f', 'm', 't', ' ',
      0x28, 0x00, 0x00, 0x00,  // chunk size (40 bytes for extensible format).
      0xFE, 0xFF,              // WAVE_FORMAT_EXTENSIBLE.
      0x02, 0x00,              // 2 channels.
      0x44, 0xAC, 0x00, 0x00,  // 44100 Hz sample rate.
      0x40, 0xC4, 0x0A, 0x00,  // byte rate (44100 * 2 * 8 = 705600).
      0x10, 0x00,              // block align (2 channels * 8 bytes).
      0x40, 0x00,              // 64 bits per sample.
      0x16, 0x00,              // extension size (22 bytes).
      0x40, 0x00,              // valid bits per sample (64).
      0x03, 0x00, 0x00, 0x00,  // channel mask (3 = front left | front right).
      // Subformat GUID for IEEE float: 00000003-0000-0010-8000-00aa00389b71.
      0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71,
      // Padding chunk to align data section (12 bytes total: 4 ID + 4 size + 4 data).
      'P', 'A', 'D', ' ',
      0x04, 0x00, 0x00, 0x00,  // chunk size (4 bytes).
      0x00, 0x00, 0x00, 0x00,  // padding data (4 bytes).
      // data chunk (now starts at offset 72, and data at 80, which is 8-byte aligned).
      'd', 'a', 't', 'a',
      0x20, 0x00, 0x00, 0x00,  // data size (32 bytes = 2 frames * 2 channels * 8 bytes).
      // Sample data: 2 frames of 64-bit float data (starts at offset 80).
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // frame 1, channel 1 (0.0).
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // frame 1, channel 2 (0.0).
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // frame 2, channel 1 (0.0).
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // frame 2, channel 2 (0.0).
  });
  // clang-format on

  auto handler = WavAudioHandler::Create(base::span(kFloat64Wav));
  ASSERT_TRUE(handler);
  EXPECT_EQ(64, handler->bits_per_sample_for_testing());

  auto bus = AudioBus::Create(2, 2);
  size_t frames_written = 0;
  EXPECT_TRUE(handler->CopyTo(bus.get(), &frames_written));
  EXPECT_EQ(2u, frames_written);
}

// Test 8-bit and 32-bit PCM format support.
TEST(WavAudioHandlerTest, PcmFormatVariations) {
  // Test 8-bit PCM.
  // clang-format off
  alignas(8) constexpr auto k8BitPcmWav = std::to_array<uint8_t>({
      // RIFF header.
      'R', 'I', 'F', 'F',
      0x24, 0x00, 0x00, 0x00,  // chunk size (36 bytes = 8 + 16 + 8 + 4).
      'W', 'A', 'V', 'E',
      // fmt chunk.
      'f', 'm', 't', ' ',
      0x10, 0x00, 0x00, 0x00,  // chunk size (16 bytes).
      0x01, 0x00,              // PCM format.
      0x01, 0x00,              // 1 channel.
      0x40, 0x1F, 0x00, 0x00,  // 8000 Hz sample rate.
      0x40, 0x1F, 0x00, 0x00,  // byte rate (8000 * 1 * 1 = 8000).
      0x01, 0x00,              // block align (1 channel * 1 byte).
      0x08, 0x00,              // 8 bits per sample.
      // data chunk.
      'd', 'a', 't', 'a',
      0x04, 0x00, 0x00, 0x00,  // data size (4 bytes).
      0x80, 0x90, 0xA0, 0xB0   // 8-bit samples (4 samples).
  });
  // clang-format on

  auto handler = WavAudioHandler::Create(base::span(k8BitPcmWav));
  ASSERT_TRUE(handler);
  EXPECT_EQ(8, handler->bits_per_sample_for_testing());

  auto bus = AudioBus::Create(1, 4);
  size_t frames_written = 0;
  EXPECT_TRUE(handler->CopyTo(bus.get(), &frames_written));
  EXPECT_EQ(4u, frames_written);

  // Test 32-bit PCM.
  // clang-format off
  // Use WAVE_FORMAT_EXTENSIBLE with padding chunk to ensure data is 8-byte aligned.
  // Structure: 12 (RIFF) + 48 (fmt) + 12 (pad with 4 bytes data) + 8 (data header) = 80
  // Data starts at offset 80, which is 8-byte aligned (80 % 8 == 0).
  alignas(8) constexpr auto k32BitPcmWav = std::to_array<uint8_t>({
      // RIFF header.
      'R', 'I', 'F', 'F',
      0x50, 0x00, 0x00, 0x00,  // chunk size (80 bytes = 4 + 48 + 12 + 8 + 8).
      'W', 'A', 'V', 'E',
      // fmt chunk with WAVE_FORMAT_EXTENSIBLE (40 bytes data).
      'f', 'm', 't', ' ',
      0x28, 0x00, 0x00, 0x00,  // chunk size (40 bytes for extensible format).
      0xFE, 0xFF,              // WAVE_FORMAT_EXTENSIBLE.
      0x01, 0x00,              // 1 channel.
      0x44, 0xAC, 0x00, 0x00,  // 44100 Hz sample rate.
      0x10, 0xB1, 0x02, 0x00,  // byte rate (44100 * 1 * 4 = 176400).
      0x04, 0x00,              // block align (1 channel * 4 bytes).
      0x20, 0x00,              // 32 bits per sample.
      0x16, 0x00,              // extension size (22 bytes).
      0x20, 0x00,              // valid bits per sample (32).
      0x04, 0x00, 0x00, 0x00,  // channel mask (4 = front center).
      // Subformat GUID for PCM: 00000001-0000-0010-8000-00aa00389b71.
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71,
      // Padding chunk to align data section (12 bytes total: 4 ID + 4 size + 4 data).
      'P', 'A', 'D', ' ',
      0x04, 0x00, 0x00, 0x00,  // chunk size (4 bytes).
      0x00, 0x00, 0x00, 0x00,  // padding data (4 bytes).
      // data chunk (now starts at offset 72, and data at 80, which is 8-byte aligned).
      'd', 'a', 't', 'a',
      0x08, 0x00, 0x00, 0x00,  // data size (8 bytes).
      // Sample data (starts at offset 80, which is 8-byte aligned).
      0x00, 0x00, 0x01, 0x00,  // 32-bit sample 1 (little-endian: 65536).
      0x00, 0x00, 0x02, 0x00   // 32-bit sample 2 (little-endian: 131072).
  });
  // clang-format on

  auto handler2 = WavAudioHandler::Create(base::span(k32BitPcmWav));
  ASSERT_TRUE(handler2);
  EXPECT_EQ(32, handler2->bits_per_sample_for_testing());

  auto bus2 = AudioBus::Create(1, 2);
  size_t frames_written2 = 0;
  EXPECT_TRUE(handler2->CopyTo(bus2.get(), &frames_written2));
  EXPECT_EQ(2u, frames_written2);
}

// Test AtEnd() boundary condition and CopyTo at end of data.
TEST(WavAudioHandlerTest, AtEndBoundaryCondition) {
  // clang-format off
  constexpr auto kSmallWav = std::to_array<uint8_t>({
      // RIFF header.
      'R', 'I', 'F', 'F',
      0x24, 0x00, 0x00, 0x00,  // chunk size (36 bytes = 4 + 24 + 8).
      'W', 'A', 'V', 'E',
      // fmt chunk.
      'f', 'm', 't', ' ',
      0x10, 0x00, 0x00, 0x00,  // chunk size (16 bytes).
      0x01, 0x00,              // PCM format.
      0x01, 0x00,              // 1 channel.
      0x44, 0xAC, 0x00, 0x00,  // 44100 Hz sample rate.
      0xAC, 0x44, 0x00, 0x00,  // byte rate (44100 * 1 * 1 = 44100).
      0x01, 0x00,              // block align (1 channel * 1 byte).
      0x08, 0x00,              // 8 bits per sample.
      // data chunk with zero length.
      'd', 'a', 't', 'a',
      0x00, 0x00, 0x00, 0x00   // data size (0 bytes).
  });
  // clang-format on

  auto handler = WavAudioHandler::Create(base::span(kSmallWav));
  ASSERT_TRUE(handler);

  // Initially should not be at end since we haven't read any data.
  EXPECT_TRUE(handler->AtEnd());

  auto bus = AudioBus::Create(1, 4);
  auto channel_data = bus->channel_span(0);
  for (int i = 0; i < bus->frames(); ++i) {
    channel_data[i] = 1.0f;
  }

  size_t frames_written = 0;
  EXPECT_TRUE(handler->CopyTo(bus.get(), &frames_written));
  EXPECT_EQ(0u, frames_written);

  // Verify bus was zeroed.
  for (int i = 0; i < bus->frames(); ++i) {
    EXPECT_EQ(0.0f, channel_data[i]);
  }
}

// Test skipping unknown chunks to cover DVLOG path.
TEST(WavAudioHandlerTest, SkipUnknownChunks) {
  // clang-format off
  constexpr auto kWavWithUnknownChunk = std::to_array<uint8_t>({
      // RIFF header.
      'R', 'I', 'F', 'F',
      0x34, 0x00, 0x00, 0x00,  // chunk size (52 bytes = 4 + 24 + 12 + 12).
      'W', 'A', 'V', 'E',
      // fmt chunk.
      'f', 'm', 't', ' ',
      0x10, 0x00, 0x00, 0x00,  // chunk size (16 bytes).
      0x01, 0x00,              // PCM format.
      0x01, 0x00,              // 1 channel.
      0x44, 0xAC, 0x00, 0x00,  // 44100 Hz sample rate.
      0xAC, 0x44, 0x00, 0x00,  // byte rate (44100 * 1 * 1 = 44100).
      0x01, 0x00,              // block align (1 channel * 1 byte).
      0x08, 0x00,              // 8 bits per sample.
      // Unknown chunk "LIST".
      'L', 'I', 'S', 'T',
      0x04, 0x00, 0x00, 0x00,  // chunk size (4 bytes).
      0x00, 0x00, 0x00, 0x00,  // dummy data.
      // data chunk.
      'd', 'a', 't', 'a',
      0x04, 0x00, 0x00, 0x00,  // data size (4 bytes).
      0x80, 0x90, 0xA0, 0xB0   // sample data.
  });
  // clang-format on

  auto handler = WavAudioHandler::Create(base::span(kWavWithUnknownChunk));
  ASSERT_TRUE(handler);
  EXPECT_EQ(1, handler->GetNumChannels());
  EXPECT_EQ(8, handler->bits_per_sample_for_testing());
  EXPECT_EQ(4u, handler->data().size());
}

// Test to specifically trigger the DVLOG path that calls
// valid_bits_per_sample() and is_extensible() accessor methods.
TEST(WavAudioHandlerTest, TriggerAccessorMethodsInDVLOG) {
  // clang-format off
  constexpr auto kInvalidExtensibleWav = std::to_array<uint8_t>({
      // RIFF header.
      'R', 'I', 'F', 'F',
      0x40, 0x00, 0x00, 0x00,  // chunk size (64 bytes = 4 + 48 + 12).
      'W', 'A', 'V', 'E',
      // fmt chunk - extensible format.
      'f', 'm', 't', ' ',
      0x28, 0x00, 0x00, 0x00,  // chunk size (40 bytes for extensible).
      0xFE, 0xFF,              // WAVE_FORMAT_EXTENSIBLE.
      0x00, 0x00,              // 0 channels (invalid - will trigger DVLOG).
      0x44, 0xAC, 0x00, 0x00,  // 44100 Hz sample rate.
      0x00, 0x00, 0x00, 0x00,  // byte rate (0 due to invalid channels).
      0x00, 0x00,              // block align (0 due to invalid channels).
      0x10, 0x00,              // 16 bits per sample.
      0x16, 0x00,              // extension size (22 bytes).
      0x10, 0x00,              // valid bits per sample.
      0x04, 0x00, 0x00, 0x00,  // channel mask.
      // Subformat GUID for PCM.
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71,
      // data chunk.
      'd', 'a', 't', 'a',
      0x04, 0x00, 0x00, 0x00,  // data size (4 bytes).
      0x00, 0x00, 0x00, 0x00   // sample data.
  });
  // clang-format on

  auto handler = WavAudioHandler::Create(base::span(kInvalidExtensibleWav));
  EXPECT_FALSE(handler);
}

}  // namespace media
