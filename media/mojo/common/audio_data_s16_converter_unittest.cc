// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/common/audio_data_s16_converter.h"

#include <memory>

#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/sample_format.h"
#include "media/base/test_helpers.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

static const int kTestVectorSize = 10;
static const int kSampleRate = 48000;
static const int16_t kTestVectorContents[kTestVectorSize] = {
    INT16_MIN,     0, INT16_MAX, INT16_MIN, INT16_MAX / 2,
    INT16_MIN / 2, 0, INT16_MAX, 0,         0};
static const int16_t kExpectedMixedVectorContents[kTestVectorSize / 2] = {
    INT16_MIN / 2, 0, 0, INT16_MAX / 2, 0};

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

TEST_F(AudioDataS16ConverterTest, ConvertToAudioDataS16_MONO) {
  // Set up original audio bus.
  std::unique_ptr<AudioBus> audio_bus = AudioBus::Create(1, kTestVectorSize);
  audio_bus->FromInterleaved<SignedInt16SampleTypeTraits>(kTestVectorContents,
                                                          kTestVectorSize);

  // Convert.
  mojom::AudioDataS16Ptr result = converter_->ConvertToAudioDataS16(
      *audio_bus, kSampleRate, CHANNEL_LAYOUT_MONO, false);

  // Compare.
  for (int i = 0; i < result->frame_count; i++) {
    ASSERT_EQ(kTestVectorContents[i], result->data[i]);
  }
}

TEST_F(AudioDataS16ConverterTest, ConvertToAudioDataS16_STEREO) {
  // Set up original audio bus.
  std::unique_ptr<AudioBus> audio_bus = AudioBus::Create(2, kTestVectorSize);
  audio_bus->FromInterleaved<SignedInt16SampleTypeTraits>(kTestVectorContents,
                                                          kTestVectorSize / 2);

  // Mix and convert.
  mojom::AudioDataS16Ptr result = converter_->ConvertToAudioDataS16(
      *audio_bus, kSampleRate, CHANNEL_LAYOUT_STEREO, false);

  // Compare.
  ASSERT_EQ(1, result->channel_count);
  for (int i = 0; i < kTestVectorSize / 2; i++) {
    ASSERT_EQ(kExpectedMixedVectorContents[i], result->data[i]);
  }
}

}  // namespace media
