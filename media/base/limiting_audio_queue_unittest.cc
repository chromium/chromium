// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/limiting_audio_queue.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/41494069): Update these tests once AudioBus is spanified..
#pragma allow_unsafe_buffers
#endif

namespace media {

namespace {
constexpr int kSampleRate = 48000;
constexpr int kChannels = 2;
constexpr int kBufferSize = 960;  // 20ms at 48khz
constexpr int kFrequency = 20;
const ChannelLayout kChannelLayout = ChannelLayout::CHANNEL_LAYOUT_STEREO;

void VerifyAudioBuffer(scoped_refptr<AudioBuffer> buffer,
                       int number_frames,
                       AudioBus* expected_data) {
  EXPECT_EQ(buffer->sample_rate(), kSampleRate);
  EXPECT_EQ(buffer->channel_layout(), kChannelLayout);
  EXPECT_EQ(buffer->channel_count(), kChannels);
  EXPECT_EQ(buffer->sample_format(), kSampleFormatPlanarF32);
  EXPECT_EQ(buffer->frame_count(), number_frames);
  EXPECT_EQ(buffer->duration(),
            AudioTimestampHelper::FramesToTime(number_frames, kSampleRate));

  for (int ch = 0; ch < kChannels; ++ch) {
    const size_t kSpanSize = sizeof(float) * static_cast<size_t>(number_frames);
    base::span<uint8_t> input_span = base::make_span(
        reinterpret_cast<uint8_t*>(expected_data->channel(ch)), kSpanSize);
    base::span<uint8_t> output_span =
        base::make_span(buffer->channel_data()[ch], kSpanSize);
    EXPECT_EQ(input_span, output_span);
  }
}

std::unique_ptr<AudioBus> CopyFirstFrames(AudioBus* bus, int num_frames) {
  auto result = AudioBus::Create(bus->channels(), num_frames);
  bus->CopyPartialFramesTo(0, num_frames, 0, result.get());
  return result;
}
}  // namespace

class LimitingAudioQueueTest : public testing::Test {
 public:
  using AudioBusVector = std::vector<std::unique_ptr<AudioBus>>;

  LimitingAudioQueueTest()
      : audio_source_(kChannels, kFrequency, kSampleRate),
        limiting_queue_(std::make_unique<LimitingAudioQueue>(kChannelLayout,
                                                             kSampleRate,
                                                             kChannels,
                                                             kBufferSize)),
        input_bus_(AudioBus::Create(kChannels, kBufferSize)) {}

  LimitingAudioQueueTest(const LimitingAudioQueueTest&) = delete;
  LimitingAudioQueueTest& operator=(const LimitingAudioQueueTest&) = delete;

  ~LimitingAudioQueueTest() override = default;

  void FillWithSine(AudioBus* bus, float scale = 1.0f) {
    audio_source_.OnMoreData(base::TimeDelta(), current_timestamp_, {}, bus);
    current_timestamp_ +=
        AudioTimestampHelper::FramesToTime(kBufferSize, kSampleRate);

    if (scale != 1.0f) {
      for (int ch = 0; ch < kChannels; ++ch) {
        float* channel_data = bus->channel(ch);
        for (int i = 0; i < bus->frames(); ++i) {
          channel_data[i] *= scale;
        }
      }
    }
  }

 protected:
  base::TimeTicks current_timestamp_;
  SineWaveAudioSource audio_source_;
  std::unique_ptr<LimitingAudioQueue> limiting_queue_;
  std::unique_ptr<AudioBus> input_bus_;
};

// Makes sure we can flush a queue that has never had any input.
TEST_F(LimitingAudioQueueTest, EmptyFlush) {
  limiting_queue_->Flush();
}

// Makes sure we can flush a queue that has been cleared.
TEST_F(LimitingAudioQueueTest, FlushClearFlush) {
  limiting_queue_->Flush();
  limiting_queue_->Clear();
  limiting_queue_->Flush();
}

// Makes sure inputs and outputs are bit-wise identical when the limiter isn't
// adjusting gain.
TEST_F(LimitingAudioQueueTest, NoLimiting_IsPassthrough) {
  FillWithSine(input_bus_.get());

  scoped_refptr<AudioBuffer> result;

  auto verify_buffer = [&](scoped_refptr<AudioBuffer> buffer) {
    VerifyAudioBuffer(buffer, kBufferSize, input_bus_.get());
    result = std::move(buffer);
  };

  limiting_queue_->Push(*input_bus_, kBufferSize, base::TimeDelta(),
                        base::BindLambdaForTesting(std::move(verify_buffer)));
  limiting_queue_->Flush();
  EXPECT_TRUE(result);
}

// Makes sure that calling Clear() drops both pending output callbacks, and does
// not include past data in the following buffers.
TEST_F(LimitingAudioQueueTest, Clear_DropsPendingInputs) {
  constexpr float kGuardValue = 0.5f;

  // Fill the first channel with kGuardValue.
  input_bus_->Zero();
  float* first_channel_data = input_bus_->channel(0);
  for (int i = 0; i < input_bus_->frames(); ++i) {
    first_channel_data[i] = kGuardValue;
  }

  // Feed in data into the queue and clear it.
  bool first_buffer_emitted = false;

  limiting_queue_->Push(
      *input_bus_, kBufferSize, base::TimeDelta(),
      base::BindLambdaForTesting(
          [&](scoped_refptr<AudioBuffer>) { first_buffer_emitted = true; }));

  limiting_queue_->Clear();

  // Feed zeros into the limiter queue. There shouldn't be any `kGuardValues`
  // from the first buffer in the output.
  input_bus_->Zero();

  bool second_bufer_emitted = false;
  bool has_values_from_first_buffer = false;
  limiting_queue_->Push(
      *input_bus_, kBufferSize, base::TimeDelta(),
      base::BindLambdaForTesting([&](scoped_refptr<AudioBuffer> buffer) {
        const float* channel_data =
            reinterpret_cast<const float*>(buffer->channel_data()[0]);
        for (int i = 0; i < buffer->frame_count(); ++i) {
          has_values_from_first_buffer |= channel_data[i] == kGuardValue;
        }
        second_bufer_emitted = true;
      }));

  limiting_queue_->Flush();

  EXPECT_FALSE(first_buffer_emitted);
  EXPECT_TRUE(second_bufer_emitted);
  EXPECT_FALSE(has_values_from_first_buffer);
}

// Makes sure inputs and outputs are bit-wise identical when the limiter isn't
// adjusting gain.
TEST_F(LimitingAudioQueueTest, NoLimiting_PartialBuffer_IsPassthrough) {
  FillWithSine(input_bus_.get());

  constexpr int kPartialBuffer = kBufferSize / 4;

  auto partial_buffer = CopyFirstFrames(input_bus_.get(), kPartialBuffer);

  scoped_refptr<AudioBuffer> result;

  auto verify_buffer = [&](scoped_refptr<AudioBuffer> buffer) {
    VerifyAudioBuffer(buffer, kPartialBuffer, partial_buffer.get());
    result = std::move(buffer);
  };

  limiting_queue_->Push(*input_bus_, kPartialBuffer, base::TimeDelta(),
                        base::BindLambdaForTesting(std::move(verify_buffer)));
  limiting_queue_->Flush();

  EXPECT_TRUE(result);
}

TEST_F(LimitingAudioQueueTest, Limiting_CompressesGain) {
  FillWithSine(input_bus_.get(), 2.0f);

  bool has_out_of_range_value = false;
  scoped_refptr<AudioBuffer> result;

  auto verify_buffer = [&](scoped_refptr<AudioBuffer> buffer) {
    for (int ch = 0; ch < kChannels; ++ch) {
      const float* channel_data =
          reinterpret_cast<const float*>(buffer->channel_data()[ch]);
      for (int i = 0; i < kBufferSize; ++i) {
        has_out_of_range_value |= std::abs(channel_data[i]) > 1.0f;
      }
    }

    result = std::move(buffer);
  };

  limiting_queue_->Push(*input_bus_, kBufferSize, base::TimeDelta(),
                        base::BindLambdaForTesting(std::move(verify_buffer)));
  limiting_queue_->Flush();

  EXPECT_FALSE(has_out_of_range_value);
  EXPECT_TRUE(result);
}

TEST_F(LimitingAudioQueueTest, MultipleBuffers) {
  FillWithSine(input_bus_.get(), 2.0f);

  // Use arbitrary buffer sizes.
  const std::vector<int> kBufferSizeSequence = {kBufferSize, kBufferSize / 2,
                                                kBufferSize / 4, kBufferSize,
                                                kBufferSize - 16};

  int buffer_count = 0;
  base::TimeDelta current_timestamp = base::TimeDelta();

  for (int size : kBufferSizeSequence) {
    const base::TimeDelta duration =
        AudioTimestampHelper::FramesToTime(size, kSampleRate);
    limiting_queue_->Push(
        *input_bus_, size, current_timestamp,
        base::BindOnce(
            [](int expected_size, base::TimeDelta expected_duration,
               base::TimeDelta expected_timestamp, int* buffer_count,
               scoped_refptr<AudioBuffer> buffer) {
              EXPECT_EQ(buffer->frame_count(), expected_size);
              EXPECT_EQ(buffer->duration(), expected_duration);
              EXPECT_EQ(buffer->timestamp(), expected_timestamp);
              ++(*buffer_count);
            },
            size, duration, current_timestamp, &buffer_count));

    current_timestamp += duration;
  }

  limiting_queue_->Flush();

  EXPECT_EQ(static_cast<size_t>(buffer_count), kBufferSizeSequence.size());
}

}  // namespace media
