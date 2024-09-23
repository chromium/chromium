// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_discard_helper.h"

#include <stddef.h>

#include <memory>

#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const float kDataStep = 0.01f;
static const size_t kSampleRate = 48000;

static DecoderBuffer::TimeInfo CreateTimeInfo(base::TimeDelta timestamp,
                                              base::TimeDelta duration) {
  DecoderBuffer::TimeInfo time_info;
  time_info.timestamp = timestamp;
  time_info.duration = duration;
  return time_info;
}

static scoped_refptr<AudioBuffer> CreateDecodedBuffer(int frames) {
  return MakeAudioBuffer(kSampleFormatPlanarF32, CHANNEL_LAYOUT_MONO, 1,
                         kSampleRate, 0.0f, kDataStep, frames, kNoTimestamp);
}

static float ExtractDecodedData(const AudioBuffer& buffer, int index) {
  // This is really inefficient, but we can't access the raw AudioBuffer if any
  // start trimming has been applied.
  std::unique_ptr<AudioBus> temp_bus =
      AudioBus::Create(buffer.channel_count(), 1);
  buffer.ReadFrames(1, index, 0, temp_bus.get());
  return temp_bus->channel(0)[0] * std::numeric_limits<uint16_t>::max();
}

TEST(AudioDiscardHelperTest, TimeDeltaToFrames) {
  AudioDiscardHelper discard_helper(kSampleRate, 0, false);

  EXPECT_EQ(0u, discard_helper.TimeDeltaToFrames(base::TimeDelta()));
  EXPECT_EQ(kSampleRate / 100,
            discard_helper.TimeDeltaToFrames(base::Milliseconds(10)));

  // Ensure partial frames are rounded down correctly.  The equation below
  // calculates a frame count with a fractional part < 0.5.
  const int small_remainder =
      base::Time::kMicrosecondsPerSecond * (kSampleRate - 0.9) / kSampleRate;
  EXPECT_EQ(kSampleRate - 1, discard_helper.TimeDeltaToFrames(
                                 base::Microseconds(small_remainder)));

  // Ditto, but rounded up using a fractional part > 0.5.
  const int large_remainder =
      base::Time::kMicrosecondsPerSecond * (kSampleRate - 0.4) / kSampleRate;
  EXPECT_EQ(kSampleRate, discard_helper.TimeDeltaToFrames(
                             base::Microseconds(large_remainder)));
}

TEST(AudioDiscardHelperTest, BasicProcessBuffers) {
  AudioDiscardHelper discard_helper(kSampleRate, 0, false);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();

  // Use an estimated duration which doesn't match the number of decoded frames
  // to ensure the helper is correctly setting durations based on output frames.
  const base::TimeDelta kEstimatedDuration = base::Milliseconds(9);
  const base::TimeDelta kActualDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kActualDuration);

  DecoderBuffer::TimeInfo time_info =
      CreateTimeInfo(kTimestamp, kEstimatedDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Verify the basic case where nothing is discarded.
  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  ASSERT_TRUE(discard_helper.initialized());
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kActualDuration, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames, decoded_buffer->frame_count());

  // Verify a Reset() takes us back to an uninitialized state.
  discard_helper.Reset(0);
  ASSERT_FALSE(discard_helper.initialized());

  // Verify a NULL output buffer returns false.
  ASSERT_FALSE(discard_helper.ProcessBuffers(time_info, NULL));
}

TEST(AudioDiscardHelperTest, NegativeTimestampClampsToZero) {
  AudioDiscardHelper discard_helper(kSampleRate, 0, false);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = -base::Seconds(1);
  const base::TimeDelta kDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  DecoderBuffer::TimeInfo time_info = CreateTimeInfo(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Verify the basic case where nothing is discarded.
  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  ASSERT_TRUE(discard_helper.initialized());
  EXPECT_EQ(base::TimeDelta(), decoded_buffer->timestamp());
  EXPECT_EQ(kDuration, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames, decoded_buffer->frame_count());
}

TEST(AudioDiscardHelperTest, ProcessBuffersWithInitialDiscard) {
  AudioDiscardHelper discard_helper(kSampleRate, 0, false);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  // Tell the helper we want to discard half of the initial frames.
  const int kDiscardFrames = kTestFrames / 2;
  discard_helper.Reset(kDiscardFrames);

  DecoderBuffer::TimeInfo time_info = CreateTimeInfo(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Verify half the frames end up discarded.
  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  ASSERT_TRUE(discard_helper.initialized());
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, decoded_buffer->duration());
  EXPECT_EQ(kDiscardFrames, decoded_buffer->frame_count());
  ASSERT_FLOAT_EQ(kDiscardFrames * kDataStep,
                  ExtractDecodedData(*decoded_buffer, 0));
}

TEST(AudioDiscardHelperTest, ProcessBuffersWithLargeInitialDiscard) {
  AudioDiscardHelper discard_helper(kSampleRate, 0, false);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  // Tell the helper we want to discard 1.5 buffers worth of frames.
  discard_helper.Reset(kTestFrames * 1.5);

  DecoderBuffer::TimeInfo time_info = CreateTimeInfo(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // The first call should fail since no output buffer remains.
  ASSERT_FALSE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  ASSERT_TRUE(discard_helper.initialized());

  // Generate another set of buffers and expect half the output frames.
  time_info = CreateTimeInfo(kTimestamp + kDuration, kDuration);
  decoded_buffer = CreateDecodedBuffer(kTestFrames);
  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));

  // The timestamp should match that of the initial buffer.
  const int kDiscardFrames = kTestFrames / 2;
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, decoded_buffer->duration());
  EXPECT_EQ(kDiscardFrames, decoded_buffer->frame_count());
  ASSERT_FLOAT_EQ(kDiscardFrames * kDataStep,
                  ExtractDecodedData(*decoded_buffer, 0));
}

TEST(AudioDiscardHelperTest, AllowNonMonotonicTimestamps) {
  AudioDiscardHelper discard_helper(kSampleRate, 0, false);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  DecoderBuffer::TimeInfo time_info = CreateTimeInfo(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  ASSERT_TRUE(discard_helper.initialized());
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames, decoded_buffer->frame_count());

  // Process the same input buffer again to ensure input timestamps which go
  // backwards in time are not errors.
  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  EXPECT_EQ(kTimestamp + kDuration, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames, decoded_buffer->frame_count());
}

TEST(AudioDiscardHelperTest, DiscardEndPadding) {
  AudioDiscardHelper discard_helper(kSampleRate, 0, false);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  DecoderBuffer::TimeInfo time_info = CreateTimeInfo(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Set a discard padding equivalent to half the buffer.
  time_info.discard_padding = std::make_pair(base::TimeDelta(), kDuration / 2);

  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  ASSERT_TRUE(discard_helper.initialized());
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames / 2, decoded_buffer->frame_count());
}

TEST(AudioDiscardHelperTest, BadDiscardEndPadding) {
  AudioDiscardHelper discard_helper(kSampleRate, 0, false);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  DecoderBuffer::TimeInfo time_info = CreateTimeInfo(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Set a discard padding equivalent to double the buffer size.
  time_info.discard_padding = std::make_pair(base::TimeDelta(), kDuration * 2);

  // Verify the end discard padding is rejected.
  ASSERT_FALSE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  ASSERT_TRUE(discard_helper.initialized());
}

TEST(AudioDiscardHelperTest, InitialDiscardAndDiscardEndPadding) {
  AudioDiscardHelper discard_helper(kSampleRate, 0, false);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  DecoderBuffer::TimeInfo time_info = CreateTimeInfo(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Set a discard padding equivalent to a quarter of the buffer.
  time_info.discard_padding = std::make_pair(base::TimeDelta(), kDuration / 4);

  // Set an initial discard of a quarter of the buffer.
  const int kDiscardFrames = kTestFrames / 4;
  discard_helper.Reset(kDiscardFrames);

  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  ASSERT_TRUE(discard_helper.initialized());
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames / 2, decoded_buffer->frame_count());
  ASSERT_FLOAT_EQ(kDiscardFrames * kDataStep,
                  ExtractDecodedData(*decoded_buffer, 0));
}

TEST(AudioDiscardHelperTest, InitialDiscardAndDiscardPadding) {
  AudioDiscardHelper discard_helper(kSampleRate, 0, false);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  DecoderBuffer::TimeInfo time_info = CreateTimeInfo(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Set all the discard values to be different to ensure each is properly used.
  const int kDiscardFrames = kTestFrames / 4;
  time_info.discard_padding = std::make_pair(kDuration / 8, kDuration / 16);
  discard_helper.Reset(kDiscardFrames);

  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  ASSERT_TRUE(discard_helper.initialized());
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration - kDuration / 4 - kDuration / 8 - kDuration / 16,
            decoded_buffer->duration());
  EXPECT_EQ(kTestFrames - kTestFrames / 4 - kTestFrames / 8 - kTestFrames / 16,
            decoded_buffer->frame_count());
}

TEST(AudioDiscardHelperTest, InitialDiscardAndDiscardPaddingAndDecoderDelay) {
  // Use a decoder delay of 5ms.
  const int kDecoderDelay = kSampleRate / 100 / 2;
  AudioDiscardHelper discard_helper(kSampleRate, kDecoderDelay, false);
  ASSERT_FALSE(discard_helper.initialized());
  discard_helper.Reset(kDecoderDelay);

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  DecoderBuffer::TimeInfo time_info = CreateTimeInfo(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Set a discard padding equivalent to half of the buffer.
  time_info.discard_padding = std::make_pair(kDuration / 2, base::TimeDelta());

  // All of the first buffer should be discarded, half from the initial delay
  // and another half from the front discard padding.
  //
  //    Encoded                   Discard Delay
  //   |--------|     |---------|     |----|
  //   |AAAAAAAA| --> |....|AAAA| --> |AAAA| -------> NULL
  //   |--------|     |---------|     |----|
  //                    Decoded               Discard Front Padding
  //
  ASSERT_FALSE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  ASSERT_TRUE(discard_helper.initialized());

  // Processing another buffer that has front discard set to half the buffer's
  // duration should discard the back half of the buffer since kDecoderDelay is
  // half a buffer.  The end padding should not be discarded until another
  // buffer is processed.  kDuration / 4 is chosen for the end discard since it
  // will force the end discard to start after position zero within the next
  // decoded buffer.
  //
  //    Encoded                    Discard Front Padding (from B)
  //   |--------|     |---------|             |----|
  //   |BBBBBBBB| --> |AAAA|BBBB| ----------> |AAAA|
  //   |--------|     |---------|             |----|
  //                    Decoded
  //           (includes carryover from A)
  //
  time_info.timestamp += kDuration;
  time_info.discard_padding = std::make_pair(kDuration / 2, kDuration / 4);
  decoded_buffer = CreateDecodedBuffer(kTestFrames);
  ASSERT_FLOAT_EQ(0.0f, ExtractDecodedData(*decoded_buffer, 0));
  ASSERT_NEAR(kDecoderDelay * kDataStep,
              ExtractDecodedData(*decoded_buffer, kDecoderDelay),
              kDataStep / 1000);
  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames / 2, decoded_buffer->frame_count());

  // Verify it was actually the latter half of the buffer that was removed.
  ASSERT_FLOAT_EQ(0.0f, ExtractDecodedData(*decoded_buffer, 0));

  // Verify the end discard padding is carried over to the next buffer.  Use
  // kDuration / 2 for the end discard padding so that the next buffer has its
  // start entirely discarded.
  //
  //    Encoded                      Discard End Padding (from B)
  //   |--------|     |---------|             |-------|
  //   |CCCCCCCC| --> |BBBB|CCCC| ----------> |BB|CCCC|
  //   |--------|     |---------|             |-------|
  //                    Decoded
  //           (includes carryover from B)
  //
  time_info.timestamp += kDuration;
  time_info.discard_padding = std::make_pair(base::TimeDelta(), kDuration / 2);
  decoded_buffer = CreateDecodedBuffer(kTestFrames);
  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  EXPECT_EQ(kTimestamp + kDuration / 2, decoded_buffer->timestamp());
  EXPECT_EQ(3 * kDuration / 4, decoded_buffer->duration());
  EXPECT_EQ(3 * kTestFrames / 4, decoded_buffer->frame_count());

  // Verify it was actually the second quarter of the buffer that was removed.
  const int kDiscardFrames = kTestFrames / 4;
  ASSERT_FLOAT_EQ(0.0f, ExtractDecodedData(*decoded_buffer, 0));
  ASSERT_FLOAT_EQ(
      kDiscardFrames * 2 * kDataStep,
      ExtractDecodedData(*decoded_buffer, kDecoderDelay - kDiscardFrames));

  // One last test to ensure carryover discard from the start works.
  //
  //    Encoded                      Discard End Padding (from C)
  //   |--------|     |---------|             |----|
  //   |DDDDDDDD| --> |CCCC|DDDD| ----------> |DDDD|
  //   |--------|     |---------|             |----|
  //                    Decoded
  //           (includes carryover from C)
  //
  time_info.timestamp += kDuration;
  time_info.discard_padding = DecoderBuffer::DiscardPadding();
  decoded_buffer = CreateDecodedBuffer(kTestFrames);
  ASSERT_FLOAT_EQ(0.0f, ExtractDecodedData(*decoded_buffer, 0));
  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  EXPECT_EQ(kTimestamp + kDuration / 2 + 3 * kDuration / 4,
            decoded_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames / 2, decoded_buffer->frame_count());
  ASSERT_FLOAT_EQ(kTestFrames / 2 * kDataStep,
                  ExtractDecodedData(*decoded_buffer, 0));
}

TEST(AudioDiscardHelperTest, DelayedDiscardInitialDiscardAndDiscardPadding) {
  AudioDiscardHelper discard_helper(kSampleRate, 0, true);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  DecoderBuffer::TimeInfo time_info = CreateTimeInfo(kTimestamp, kDuration);

  // Set all the discard values to be different to ensure each is properly used.
  const int kDiscardFrames = kTestFrames / 4;
  time_info.discard_padding = std::make_pair(kDuration / 8, kDuration / 16);
  discard_helper.Reset(kDiscardFrames);

  // Verify nothing is output for the first buffer, yet initialized is true.
  ASSERT_FALSE(discard_helper.ProcessBuffers(time_info, NULL));
  ASSERT_TRUE(discard_helper.initialized());

  // Create an encoded buffer with no discard padding.
  time_info = CreateTimeInfo(kTimestamp + kDuration, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Verify that when the decoded buffer is consumed, the discards from the
  // previous encoded buffer are applied.
  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration - kDuration / 4 - kDuration / 8 - kDuration / 16,
            decoded_buffer->duration());
  EXPECT_EQ(kTestFrames - kTestFrames / 4 - kTestFrames / 8 - kTestFrames / 16,
            decoded_buffer->frame_count());
}

TEST(AudioDiscardHelperTest, CompleteDiscard) {
  AudioDiscardHelper discard_helper(kSampleRate, 0, false);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);
  discard_helper.Reset(0);

  DecoderBuffer::TimeInfo time_info = CreateTimeInfo(kTimestamp, kDuration);
  time_info.discard_padding =
      std::make_pair(kInfiniteDuration, base::TimeDelta());
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Verify all of the first buffer is discarded.
  ASSERT_FALSE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  ASSERT_TRUE(discard_helper.initialized());
  time_info.timestamp = kTimestamp + kDuration;
  time_info.discard_padding = DecoderBuffer::DiscardPadding();

  // Verify a second buffer goes through untouched.
  decoded_buffer = CreateDecodedBuffer(kTestFrames / 2);
  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames / 2, decoded_buffer->frame_count());
  ASSERT_FLOAT_EQ(0.0f, ExtractDecodedData(*decoded_buffer, 0));
}

TEST(AudioDiscardHelperTest, CompleteDiscardWithDelayedDiscard) {
  AudioDiscardHelper discard_helper(kSampleRate, 0, true);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);
  discard_helper.Reset(0);

  DecoderBuffer::TimeInfo time_info = CreateTimeInfo(kTimestamp, kDuration);
  time_info.discard_padding =
      std::make_pair(kInfiniteDuration, base::TimeDelta());
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Setup a delayed discard.
  ASSERT_FALSE(discard_helper.ProcessBuffers(time_info, NULL));
  ASSERT_TRUE(discard_helper.initialized());

  // Verify the first output buffer is dropped.
  time_info.timestamp = kTimestamp + kDuration;
  time_info.discard_padding = DecoderBuffer::DiscardPadding();
  ASSERT_FALSE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));

  // Verify the second buffer goes through untouched.
  time_info.timestamp = kTimestamp + 2 * kDuration;
  decoded_buffer = CreateDecodedBuffer(kTestFrames / 2);
  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames / 2, decoded_buffer->frame_count());
  ASSERT_FLOAT_EQ(0.0f, ExtractDecodedData(*decoded_buffer, 0));
}

TEST(AudioDiscardHelperTest, CompleteDiscardWithInitialDiscardDecoderDelay) {
  // Use a decoder delay of 5ms.
  const int kDecoderDelay = kSampleRate / 100 / 2;
  AudioDiscardHelper discard_helper(kSampleRate, kDecoderDelay, false);
  ASSERT_FALSE(discard_helper.initialized());
  discard_helper.Reset(kDecoderDelay);

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::Milliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  DecoderBuffer::TimeInfo time_info = CreateTimeInfo(kTimestamp, kDuration);
  time_info.discard_padding =
      std::make_pair(kInfiniteDuration, base::TimeDelta());
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Verify all of the first buffer is discarded.
  ASSERT_FALSE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  ASSERT_TRUE(discard_helper.initialized());
  time_info.timestamp = kTimestamp + kDuration;
  time_info.discard_padding = DecoderBuffer::DiscardPadding();

  // Verify 5ms off the front of the second buffer is discarded.
  decoded_buffer = CreateDecodedBuffer(kTestFrames * 2);
  ASSERT_TRUE(discard_helper.ProcessBuffers(time_info, decoded_buffer.get()));
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration * 2 - kDuration / 2, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames * 2 - kDecoderDelay, decoded_buffer->frame_count());
  ASSERT_FLOAT_EQ(kDecoderDelay * kDataStep,
                  ExtractDecodedData(*decoded_buffer, 0));
}

}  // namespace media
