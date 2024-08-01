// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/audio_timestamp_validator.h"

#include <tuple>

#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_util.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;

namespace media {

// Constants to specify the type of audio data used.
static const AudioCodec kCodec = AudioCodec::kVorbis;
static const SampleFormat kSampleFormat = kSampleFormatPlanarF32;
static const base::TimeDelta kSeekPreroll;
static const int kSamplesPerSecond = 10000;
static const base::TimeDelta kBufferDuration = base::Milliseconds(20);
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
static const int kChannelCount = 2;
static const int kChannels = ChannelLayoutToChannelCount(kChannelLayout);
static const int kFramesPerBuffer = kBufferDuration.InMicroseconds() *
                                    kSamplesPerSecond /
                                    base::Time::kMicrosecondsPerSecond;

// Params are:
// 1. Output delay: number of encoded buffers before first decoded output
// 2. Codec delay: number of frames of codec delay in decoder config
// 3. Front discard: front discard for the first buffer
using ValidatorTestParams = testing::tuple<int, int, base::TimeDelta>;

class AudioTimestampValidatorTest
    : public testing::Test,
      public ::testing::WithParamInterface<ValidatorTestParams> {
 public:
  AudioTimestampValidatorTest() = default;

 protected:
  void SetUp() override {
    output_delay_ = testing::get<0>(GetParam());
    codec_delay_ = testing::get<1>(GetParam());
    front_discard_ = testing::get<2>(GetParam());
  }

  int output_delay_;

  int codec_delay_;

  base::TimeDelta front_discard_;

  testing::StrictMock<MockMediaLog> media_log_;
};

TEST_P(AudioTimestampValidatorTest, WarnForEraticTimes) {
  AudioDecoderConfig decoder_config;
  decoder_config.Initialize(kCodec, kSampleFormat, kChannelLayout,
                            kSamplesPerSecond, EmptyExtraData(),
                            EncryptionScheme::kUnencrypted, kSeekPreroll,
                            codec_delay_);

  // Validator should fail to stabilize pattern for timestamp expectations.
  EXPECT_MEDIA_LOG(
      HasSubstr("Failed to reconcile encoded audio times "
                "with decoded output."));

  // No gap warnings should be emitted because the timestamps expectations never
  // stabilized.
  EXPECT_MEDIA_LOG(HasSubstr("timestamp gap detected")).Times(0);

  AudioTimestampValidator validator(decoder_config, &media_log_);

  const base::TimeDelta kRandomOffsets[] = {base::Milliseconds(100),
                                            base::Milliseconds(350)};

  for (int i = 0; i < 100; ++i) {
    // Each buffer's timestamp is kBufferDuration from the previous buffer.
    auto encoded_buffer = base::MakeRefCounted<DecoderBuffer>(0);

    // Ping-pong between two random offsets to prevent validator from
    // stabilizing timestamp pattern.
    base::TimeDelta randomOffset =
        kRandomOffsets[i % std::size(kRandomOffsets)];
    encoded_buffer->set_timestamp(i * kBufferDuration + randomOffset);

    if (i == 0) {
      encoded_buffer->set_discard_padding(
          std::make_pair(front_discard_, base::TimeDelta()));
    }

    validator.CheckForTimestampGap(*encoded_buffer);

    if (i >= output_delay_) {
      // kFramesPerBuffer is derived to perfectly match kBufferDuration, so
      // no gaps exists as long as timestamps are exactly kBufferDuration apart.
      scoped_refptr<AudioBuffer> decoded_buffer = MakeAudioBuffer<float>(
          kSampleFormat, kChannelLayout, kChannelCount, kSamplesPerSecond, 1.0f,
          0.0f, kFramesPerBuffer, i * kBufferDuration);
      validator.RecordOutputDuration(*decoded_buffer);
    }
  }
}

TEST_P(AudioTimestampValidatorTest, NoWarningForValidTimes) {
  AudioDecoderConfig decoder_config;
  decoder_config.Initialize(kCodec, kSampleFormat, kChannelLayout,
                            kSamplesPerSecond, EmptyExtraData(),
                            EncryptionScheme::kUnencrypted, kSeekPreroll,
                            codec_delay_);

  // Validator should quickly stabilize pattern for timestamp expectations.
  EXPECT_MEDIA_LOG(HasSubstr("Failed to reconcile encoded audio times "
                             "with decoded output."))
      .Times(0);

  // Expect no gap warnings for series of buffers with valid timestamps.
  EXPECT_MEDIA_LOG(HasSubstr("timestamp gap detected")).Times(0);

  AudioTimestampValidator validator(decoder_config, &media_log_);

  for (int i = 0; i < 100; ++i) {
    // Each buffer's timestamp is kBufferDuration from the previous buffer.
    auto encoded_buffer = base::MakeRefCounted<DecoderBuffer>(0);
    encoded_buffer->set_timestamp(i * kBufferDuration);

    if (i == 0) {
      encoded_buffer->set_discard_padding(
          std::make_pair(front_discard_, base::TimeDelta()));
    }

    validator.CheckForTimestampGap(*encoded_buffer);

    if (i >= output_delay_) {
      // kFramesPerBuffer is derived to perfectly match kBufferDuration, so
      // no gaps exists as long as timestamps are exactly kBufferDuration apart.
      scoped_refptr<AudioBuffer> decoded_buffer = MakeAudioBuffer<float>(
          kSampleFormat, kChannelLayout, kChannelCount, kSamplesPerSecond, 1.0f,
          0.0f, kFramesPerBuffer, i * kBufferDuration);
      validator.RecordOutputDuration(*decoded_buffer);
    }
  }
}

TEST_P(AudioTimestampValidatorTest, SingleWarnForSingleLargeGap) {
  AudioDecoderConfig decoder_config;
  decoder_config.Initialize(kCodec, kSampleFormat, kChannelLayout,
                            kSamplesPerSecond, EmptyExtraData(),
                            EncryptionScheme::kUnencrypted, kSeekPreroll,
                            codec_delay_);

  AudioTimestampValidator validator(decoder_config, &media_log_);

  // Validator should quickly stabilize pattern for timestamp expectations.
  EXPECT_MEDIA_LOG(HasSubstr("Failed to reconcile encoded audio times "
                             "with decoded output."))
      .Times(0);

  for (int i = 0; i < 100; ++i) {
    // Halfway through the stream, introduce sudden gap of 50 milliseconds.
    base::TimeDelta offset;
    if (i >= 50)
      offset = base::Milliseconds(100);

    // This gap never widens, so expect only a single warning when its first
    // introduced.
    if (i == 50)
      EXPECT_MEDIA_LOG(HasSubstr("timestamp gap detected"));

    auto encoded_buffer = base::MakeRefCounted<DecoderBuffer>(0);
    encoded_buffer->set_timestamp(i * kBufferDuration + offset);

    if (i == 0) {
      encoded_buffer->set_discard_padding(
          std::make_pair(front_discard_, base::TimeDelta()));
    }

    validator.CheckForTimestampGap(*encoded_buffer);

    if (i >= output_delay_) {
      // kFramesPerBuffer is derived to perfectly match kBufferDuration, so
      // no gaps exists as long as timestamps are exactly kBufferDuration apart.
      scoped_refptr<AudioBuffer> decoded_buffer = MakeAudioBuffer<float>(
          kSampleFormat, kChannelLayout, kChannelCount, kSamplesPerSecond, 1.0f,
          0.0f, kFramesPerBuffer, i * kBufferDuration);
      validator.RecordOutputDuration(*decoded_buffer);
    }
  }
}

TEST_P(AudioTimestampValidatorTest, RepeatedWarnForSlowAccumulatingDrift) {
  AudioDecoderConfig decoder_config;
  decoder_config.Initialize(kCodec, kSampleFormat, kChannelLayout,
                            kSamplesPerSecond, EmptyExtraData(),
                            EncryptionScheme::kUnencrypted, kSeekPreroll,
                            codec_delay_);

  AudioTimestampValidator validator(decoder_config, &media_log_);

  EXPECT_MEDIA_LOG(HasSubstr("Failed to reconcile encoded audio times "
                             "with decoded output."))
      .Times(0);

  int num_timestamp_gap_warnings = 0;
  const int kMaxTimestampGapWarnings = 10;  // Must be the same as in .cc

  for (int i = 0; i < 100; ++i) {
    // Wait for delayed output to begin plus an additional two iterations to
    // start using drift offset. The the two iterations without offset will
    // allow the validator to stabilize the pattern of timestamps and begin
    // checking for gaps. Once stable, increase offset by 1 millisecond for each
    // iteration.
    base::TimeDelta offset;
    if (i >= output_delay_ + 2)
      offset = i * base::Milliseconds(1);

    auto encoded_buffer = base::MakeRefCounted<DecoderBuffer>(0);
    encoded_buffer->set_timestamp((i * kBufferDuration) + offset);

    // Expect gap warnings to start when drift hits 50 milliseconds. Warnings
    // should continue as the gap widens until log limit is hit.

    if (offset > base::Milliseconds(50)) {
      EXPECT_LIMITED_MEDIA_LOG(HasSubstr("timestamp gap detected"),
                               num_timestamp_gap_warnings,
                               kMaxTimestampGapWarnings);
    }

    validator.CheckForTimestampGap(*encoded_buffer);

    if (i >= output_delay_) {
      // kFramesPerBuffer is derived to perfectly match kBufferDuration, so
      // no gaps exists as long as timestamps are exactly kBufferDuration apart.
      scoped_refptr<AudioBuffer> decoded_buffer = MakeAudioBuffer<float>(
          kSampleFormat, kChannelLayout, kChannelCount, kSamplesPerSecond, 1.0f,
          0.0f, kFramesPerBuffer, i * kBufferDuration);
      validator.RecordOutputDuration(*decoded_buffer);
    }
  }
}

// Test with cartesian product of various output delay, codec delay, and front
// discard values. These simulate configurations for different containers/codecs
// which present different challenges when building timestamp expectations.
INSTANTIATE_TEST_SUITE_P(
    All,
    AudioTimestampValidatorTest,
    ::testing::Combine(::testing::Values(0, 10),             // output delay
                       ::testing::Values(0, 512),            // codec delay
                       ::testing::Values(base::TimeDelta(),  // front discard
                                         base::Milliseconds(65))));

}  // namespace media
