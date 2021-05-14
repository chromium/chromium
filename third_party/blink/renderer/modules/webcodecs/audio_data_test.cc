// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"

#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {
// Default test values
constexpr int64_t kTimestampInMicroSeconds = 1234;
constexpr int kChannels = 2;
constexpr int kFrames = 20;
constexpr int kSampleRate = 8000;
}  // namespace

class AudioDataTest : public testing::Test {
 protected:
  AudioBuffer* CreateDefaultAudioBuffer() {
    auto* audio_buffer =
        AudioBuffer::CreateUninitialized(kChannels, kFrames, kSampleRate);
    for (int ch = 0; ch < kChannels; ++ch) {
      float* buffer_data = audio_buffer->getChannelData(ch)->Data();
      for (int i = 0; i < kFrames; ++i) {
        buffer_data[i] = static_cast<float>((i + ch * kFrames) / 1000.0f);
      }
    }
    return audio_buffer;
  }

  AudioDataInit* CreateDefaultAudioDataInit(AudioBuffer* buffer) {
    auto* audio_data_init = AudioDataInit::Create();
    audio_data_init->setBuffer(buffer);
    audio_data_init->setTimestamp(kTimestampInMicroSeconds);
    return audio_data_init;
  }
};

TEST_F(AudioDataTest, ConstructFromMediaBuffer) {
  const media::ChannelLayout channel_layout =
      media::ChannelLayout::CHANNEL_LAYOUT_STEREO;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  constexpr base::TimeDelta timestamp =
      base::TimeDelta::FromMicroseconds(kTimestampInMicroSeconds);
  constexpr int kStart = 1;
  constexpr int kIncrement = 1;
  scoped_refptr<media::AudioBuffer> media_buffer =
      media::MakeAudioBuffer<int16_t>(media::SampleFormat::kSampleFormatS16,
                                      channel_layout, channels, kSampleRate,
                                      kStart, kIncrement, kFrames, timestamp);

  auto* frame = MakeGarbageCollected<AudioData>(media_buffer);

  EXPECT_EQ(frame->timestamp(), kTimestampInMicroSeconds);

  EXPECT_TRUE(frame->buffer());
  EXPECT_EQ(frame->buffer()->numberOfChannels(),
            static_cast<unsigned>(channels));
  EXPECT_EQ(frame->buffer()->length(), static_cast<uint32_t>(kFrames));

  // The buffer's internal int16_t value should have been converted to float32.
  constexpr float kFloatIncrement =
      static_cast<float>(kIncrement) / std::numeric_limits<int16_t>::max();
  constexpr float kFloatStart =
      static_cast<float>(kStart) / std::numeric_limits<int16_t>::max();

  // Verify the data was properly converted.
  for (int ch = 0; ch < channels; ++ch) {
    float* internal_channel = frame->buffer()->getChannelData(ch)->Data();
    float start_value = kFloatStart + ch * kFloatIncrement * kFrames;
    for (int i = 0; i < kFrames; ++i) {
      float expected_value = start_value + i * kFloatIncrement;
      ASSERT_FLOAT_EQ(expected_value, internal_channel[i])
          << "i=" << i << ", ch=" << ch;
    }
  }

  // The media::AudioBuffer we receive should match the original |media_buffer|.
  EXPECT_EQ(frame->data(), media_buffer);
}

TEST_F(AudioDataTest, ConstructFromAudioDataInit) {
  auto* audio_buffer = CreateDefaultAudioBuffer();

  auto* audio_data_init = CreateDefaultAudioDataInit(audio_buffer);

  auto* frame = MakeGarbageCollected<AudioData>(audio_data_init);

  EXPECT_EQ(frame->timestamp(), kTimestampInMicroSeconds);
  EXPECT_EQ(frame->buffer(), audio_buffer);
}

}  // namespace blink
