// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_frame.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_frame_init.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {
// Default test values
constexpr uint64_t kTimestampInMicroSeconds = 1234;
constexpr int kChannels = 2;
constexpr int kFrames = 20;
constexpr int kSampleRate = 8000;
}  // namespace

class AudioFrameTest : public testing::Test {
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

  AudioFrameInit* CreateDefaultAudioFrameInit(AudioBuffer* buffer) {
    auto* audio_frame_init = AudioFrameInit::Create();
    audio_frame_init->setBuffer(buffer);
    audio_frame_init->setTimestamp(kTimestampInMicroSeconds);
    return audio_frame_init;
  }
};

TEST_F(AudioFrameTest, ConstructFromMediaBuffer) {
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

  auto* frame = MakeGarbageCollected<AudioFrame>(media_buffer);

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
}

TEST_F(AudioFrameTest, ConstructFromAudioFrameInit) {
  auto* audio_buffer = CreateDefaultAudioBuffer();

  auto* audio_frame_init = CreateDefaultAudioFrameInit(audio_buffer);

  auto* frame = MakeGarbageCollected<AudioFrame>(audio_frame_init);

  EXPECT_EQ(frame->timestamp(), kTimestampInMicroSeconds);
  EXPECT_EQ(frame->buffer(), audio_buffer);
}

TEST_F(AudioFrameTest, VerifySerializationData) {
  auto* audio_buffer = CreateDefaultAudioBuffer();

  // Create a frame from the audio buffer.
  auto* audio_frame_init = CreateDefaultAudioFrameInit(audio_buffer);
  auto* frame = MakeGarbageCollected<AudioFrame>(audio_frame_init);

  // Serialize the data from the frame.
  std::unique_ptr<AudioFrameSerializationData> data =
      frame->GetSerializationData();

  // Make sure attributes match.
  EXPECT_EQ(data->timestamp(),
            base::TimeDelta::FromMicroseconds(kTimestampInMicroSeconds));
  EXPECT_EQ(data->sample_rate(), kSampleRate);
  EXPECT_EQ(data->data()->channels(), kChannels);
  EXPECT_EQ(data->data()->frames(), kFrames);

  // Make sure the data matches.
  for (int ch = 0; ch < kChannels; ++ch) {
    float* buffer_data = audio_buffer->getChannelData(ch)->Data();
    float* serialized_data = data->data()->channel(ch);
    for (int i = 0; i < kFrames; ++i) {
      ASSERT_FLOAT_EQ(buffer_data[i], serialized_data[i])
          << "i=" << i << ", ch=" << ch;
    }
  }
}

TEST_F(AudioFrameTest, ConstructFromSerializationData) {
  // Create a default frame.
  auto* audio_buffer = CreateDefaultAudioBuffer();
  auto* audio_frame_init = CreateDefaultAudioFrameInit(audio_buffer);
  auto* original_frame = MakeGarbageCollected<AudioFrame>(audio_frame_init);

  // Get a copy of the serialization data, and create a new frame from it.
  std::unique_ptr<AudioFrameSerializationData> data =
      original_frame->GetSerializationData();

  auto* new_frame = MakeGarbageCollected<AudioFrame>(std::move(data));

  // Make sure attributes match.
  EXPECT_EQ(original_frame->timestamp(), new_frame->timestamp());
  EXPECT_EQ(original_frame->buffer()->sampleRate(),
            new_frame->buffer()->sampleRate());
  EXPECT_EQ(original_frame->buffer()->numberOfChannels(),
            new_frame->buffer()->numberOfChannels());
  EXPECT_EQ(original_frame->buffer()->length(), new_frame->buffer()->length());

  // Make sure the data matches.
  for (int ch = 0; ch < kChannels; ++ch) {
    float* orig_data = original_frame->buffer()->getChannelData(ch)->Data();
    float* new_data = new_frame->buffer()->getChannelData(ch)->Data();
    for (int i = 0; i < kFrames; ++i) {
      ASSERT_FLOAT_EQ(orig_data[i], new_data[i]) << "i=" << i << ", ch=" << ch;
    }
  }
}

}  // namespace blink
