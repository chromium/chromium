// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/aaudio_output.h"

#include <aaudio/AAudio.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/audio/android/audio_device.h"
#include "media/audio/audio_features.h"
#include "media/audio/mock_audio_source_callback.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NotNull;

namespace media {

namespace {

constexpr int kBurstFrames = 192;  // 4ms AAudio burst at 48kHz.

auto FillAudioBusWithConstant(float value) {
  return [value](base::TimeDelta /* delay */,
                 base::TimeTicks /* delay_timestamp */,
                 const AudioGlitchInfo& /* glitch_info */, AudioBus* dest) {
    for (int ch = 0; ch < dest->channels(); ++ch) {
      std::ranges::fill(dest->channel(ch), value);
    }
    return dest->frames();
  };
}

}  // namespace

class AAudioOutputStreamTest : public testing::Test {
 protected:
  void SetUp() override {
    static constexpr int kSampleRate = 48000;
    static constexpr int kFramesPerBuffer = 960;  // 20ms per buffer at 48kHz.

    feature_list_.InitAndEnableFeature(features::kAAudioVariableSizedCallbacks);

    AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                           ChannelLayoutConfig::Stereo(), kSampleRate,
                           kFramesPerBuffer);
    stream_ = std::make_unique<AAudioOutputStream>(
        /*manager=*/nullptr, params, android::AudioDevice::Default(),
        AAUDIO_USAGE_MEDIA, base::DoNothing());
    stream_->CreateAudioBusAndFifo();
  }

  void TearDown() override {
    if (stream_) {
      stream_->Stop();
      stream_.reset();
    }
  }

  void StartWithCallback(AudioOutputStream::AudioSourceCallback* callback) {
    base::AutoLock al(stream_->lock_);
    stream_->callback_ = callback;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<AAudioOutputStream> stream_;
};

// Regression test for crbug.com/576472301: Verify that stopping an
// `AAudioOutputStream` clears any unconsumed frames remaining in `pull_fifo_`
// so they are not played out as a burst of stale audio when the stream is
// reused.
TEST_F(AAudioOutputStreamTest, StopClearsPullFifo) {
  MockAudioSourceCallback first_source;
  MockAudioSourceCallback second_source;

  // Start the stream with `first_source` (producing 0.75f) and request a single
  // 192-frame burst. This pulls a full 960-frame buffer into `pull_fifo_`,
  // consumes 192 frames, and leaves 768 frames buffered in `pull_fifo_`.
  StartWithCallback(&first_source);
  EXPECT_CALL(first_source, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
      .WillOnce(FillAudioBusWithConstant(0.75f));

  std::vector<float> burst(kBurstFrames * 2, 0.0f);
  EXPECT_TRUE(stream_->OnAudioDataRequested(burst));
  EXPECT_THAT(burst, testing::Each(0.75f));

  // Stop the stream (as `AudioOutputDispatcherImpl::StopPhysicalStream()` does
  // when returning an idle physical stream to the pool).
  stream_->Stop();

  // Reuse the stream with `second_source` (producing 0.0f silence) and request
  // another 192-frame burst. `pull_fifo_` must have been cleared on `Stop()` so
  // that `second_source` is queried immediately and no stale 0.75f samples from
  // `first_source` leak out.
  StartWithCallback(&second_source);
  EXPECT_CALL(second_source, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
      .WillOnce(FillAudioBusWithConstant(0.0f));

  std::ranges::fill(burst, -1.0f);
  EXPECT_TRUE(stream_->OnAudioDataRequested(burst));
  EXPECT_THAT(burst, testing::Each(0.0f));

  stream_->Stop();
}

TEST_F(AAudioOutputStreamTest, FlushClearsPullFifo) {
  MockAudioSourceCallback source;

  StartWithCallback(&source);
  EXPECT_CALL(source, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
      .WillOnce(FillAudioBusWithConstant(0.5f));

  std::vector<float> burst(kBurstFrames * 2, 0.0f);
  EXPECT_TRUE(stream_->OnAudioDataRequested(burst));
  EXPECT_THAT(burst, testing::Each(0.5f));

  // Flushing the stream must discard the remaining 768 frames in `pull_fifo_`.
  stream_->Flush();

  EXPECT_CALL(source, OnMoreData(_, _, AudioGlitchInfo(), NotNull()))
      .WillOnce(FillAudioBusWithConstant(0.25f));

  std::ranges::fill(burst, -1.0f);
  EXPECT_TRUE(stream_->OnAudioDataRequested(burst));
  EXPECT_THAT(burst, testing::Each(0.25f));

  stream_->Stop();
}

}  // namespace media
