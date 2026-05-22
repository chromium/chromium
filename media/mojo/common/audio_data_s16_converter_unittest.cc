// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/audio_data_s16_converter.h"

#include <array>
#include <memory>

#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/sample_format.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

static constexpr size_t kTestVectorSize = 10;
static constexpr size_t kStereoFrameCount = kTestVectorSize / 2;
static constexpr int kSampleRate = 48000;
static constexpr std::array<int16_t, kTestVectorSize> kTestVectorContents = {
    INT16_MIN,     0, INT16_MAX, INT16_MIN, INT16_MAX / 2,
    INT16_MIN / 2, 0, INT16_MAX, 0,         0};
static constexpr std::array<int16_t, kStereoFrameCount>
    kExpectedMixedVectorContents = {INT16_MIN / 2, 0, 0, INT16_MAX / 2, 0};

std::unique_ptr<AudioBus> CreateStereoAudioBusWithTestData() {
  auto audio_bus = AudioBus::Create(2, kStereoFrameCount);
  audio_bus->FromInterleaved<SignedInt16SampleTypeTraits>(kTestVectorContents);
  return audio_bus;
}

scoped_refptr<AudioBuffer> CreatePopulatedF32StereoBuffer() {
  auto audio_bus = CreateStereoAudioBusWithTestData();
  return AudioBuffer::CopyFrom(CHANNEL_LAYOUT_STEREO, kSampleRate,
                               base::TimeDelta(), audio_bus.get());
}

}  // namespace

class AudioDataS16ConverterTest : public testing::Test {
 public:
  AudioDataS16ConverterTest();
  ~AudioDataS16ConverterTest() override = default;

 protected:
  std::unique_ptr<AudioDataS16Converter> converter_;
};

AudioDataS16ConverterTest::AudioDataS16ConverterTest() {
  converter_ = std::make_unique<AudioDataS16Converter>();
}

TEST_F(AudioDataS16ConverterTest, ConvertToAudioDataS16_AudioBus_MONO) {
  // Set up original audio bus.
  std::unique_ptr<AudioBus> audio_bus = AudioBus::Create(1, kTestVectorSize);
  audio_bus->FromInterleaved<SignedInt16SampleTypeTraits>(kTestVectorContents);

  // Convert.
  mojom::AudioDataS16Ptr result = converter_->ConvertToAudioDataS16(
      *audio_bus, kSampleRate, CHANNEL_LAYOUT_MONO,
      /*is_multichannel_supported=*/false);

  // Compare.
  EXPECT_EQ(base::span(result->data), base::span(kTestVectorContents));
}

TEST_F(AudioDataS16ConverterTest, ConvertToAudioDataS16_AudioBus_STEREO) {
  // Set up original audio bus.
  std::unique_ptr<AudioBus> audio_bus = CreateStereoAudioBusWithTestData();

  // Convert with multichannel supported = true.
  mojom::AudioDataS16Ptr result = converter_->ConvertToAudioDataS16(
      *audio_bus, kSampleRate, CHANNEL_LAYOUT_STEREO,
      /*is_multichannel_supported=*/true);

  // Compare.
  ASSERT_EQ(2, result->channel_count);
  ASSERT_EQ(static_cast<size_t>(result->frame_count), kStereoFrameCount);
  EXPECT_EQ(base::span(result->data), base::span(kTestVectorContents));
}

TEST_F(AudioDataS16ConverterTest,
       ConvertToAudioDataS16_AudioBus_STEREO_DownMix) {
  // Set up original audio bus.
  std::unique_ptr<AudioBus> audio_bus = CreateStereoAudioBusWithTestData();

  // Mix and convert.
  mojom::AudioDataS16Ptr result = converter_->ConvertToAudioDataS16(
      *audio_bus, kSampleRate, CHANNEL_LAYOUT_STEREO,
      /*is_multichannel_supported=*/false);

  // Compare.
  ASSERT_EQ(1, result->channel_count);
  EXPECT_EQ(base::span(result->data), base::span(kExpectedMixedVectorContents));
}

TEST_F(AudioDataS16ConverterTest, ConvertToAudioDataS16_AudioBuffer) {
  // Create S16 Mono AudioBuffer.
  scoped_refptr<AudioBuffer> buffer = AudioBuffer::CreateBuffer(
      SampleFormat::kSampleFormatS16, CHANNEL_LAYOUT_MONO, 1, kSampleRate,
      kTestVectorSize);

  // Populate manually.
  auto raw_channel = buffer->channels()[0];
  auto bus_span = base::subtle::reinterpret_span<int16_t>(raw_channel);
  ASSERT_EQ(bus_span.size(), kTestVectorSize);
  bus_span.copy_from(kTestVectorContents);

  // Convert.
  mojom::AudioDataS16Ptr result = converter_->ConvertToAudioDataS16(
      buffer, /*is_multichannel_supported=*/false);

  // Compare.
  ASSERT_EQ(1, result->channel_count);
  ASSERT_EQ(buffer->frame_count(), result->frame_count);
  ASSERT_EQ(buffer->sample_rate(), result->sample_rate);
  EXPECT_EQ(base::span(result->data), base::span(kTestVectorContents));
}

TEST_F(AudioDataS16ConverterTest,
       ConvertToAudioDataS16_AudioBuffer_FormatConversion) {
  auto buffer = CreatePopulatedF32StereoBuffer();

  // Convert with multichannel supported = true.
  mojom::AudioDataS16Ptr result = converter_->ConvertToAudioDataS16(
      buffer, /*is_multichannel_supported=*/true);

  // Compare.
  ASSERT_EQ(2, result->channel_count);
  ASSERT_EQ(static_cast<size_t>(result->frame_count), kStereoFrameCount);
  EXPECT_EQ(base::span(result->data), base::span(kTestVectorContents));
}

TEST_F(AudioDataS16ConverterTest,
       ConvertToAudioDataS16_AudioBuffer_FormatConversion_DownMix) {
  auto buffer = CreatePopulatedF32StereoBuffer();

  // Convert with multichannel supported = false (triggers downmix).
  mojom::AudioDataS16Ptr result = converter_->ConvertToAudioDataS16(
      buffer, /*is_multichannel_supported=*/false);

  // Compare.
  ASSERT_EQ(1, result->channel_count);
  ASSERT_EQ(static_cast<size_t>(result->frame_count), kStereoFrameCount);
  EXPECT_EQ(base::span(result->data), base::span(kExpectedMixedVectorContents));
}

}  // namespace media
