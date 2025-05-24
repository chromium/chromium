// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/openscreen_frame_sender.h"

#include <memory>
#include <numeric>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "media/base/audio_codecs.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/video_codecs.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/test/openscreen_test_helpers.h"
#include "media/cast/test/test_with_cast_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::cast {
namespace {

constexpr uint32_t kFirstSsrc = 35535;
constexpr int kRtpTimebase = 9000;
constexpr int kAudioBitrate = 100 * 1000;

static const FrameSenderConfig kAudioConfig{
    kFirstSsrc,
    kFirstSsrc + 1,
    base::Milliseconds(100),
    kDefaultTargetPlayoutDelay,
    /* use_hardware_encoder= */ false,
    kDefaultAudioSamplingRate,
    /* channels= */ 2,
    kAudioBitrate,
    kAudioBitrate,
    kAudioBitrate,
    kDefaultMaxFrameRate,
    std::nullopt,
    AudioCodecParams{.codec = AudioCodec::kOpus}};

static const FrameSenderConfig kVideoConfig{
    kFirstSsrc + 2,
    kFirstSsrc + 3,
    base::Milliseconds(100),
    kDefaultTargetPlayoutDelay,
    /* use_hardware_encoder= */ false,
    kRtpTimebase,
    /* channels = */ 1,
    kDefaultMaxVideoBitrate,
    kDefaultMinVideoBitrate,
    std::midpoint<int>(kDefaultMinVideoBitrate, kDefaultMaxVideoBitrate),
    kDefaultMaxFrameRate,
    VideoCodecParams(VideoCodec::kVP8),
    std::nullopt};

}  // namespace

class OpenscreenFrameSenderTest : public TestWithCastEnvironment,
                                  public FrameSender::Client {
 public:
  // FrameSender::Client overrides.
  int GetNumberOfFramesInEncoder() const override { return 0; }
  base::TimeDelta GetEncoderBacklogDuration() const override { return {}; }
  void OnFrameCanceled(FrameId frame_id) override {}

 protected:
  OpenscreenFrameSenderTest()
      : openscreen_test_senders_(OpenscreenTestSenders::Config(
            GetMainThreadTaskRunner(),
            GetMockTickClock(),
            openscreen::cast::RtpPayloadType::kAudioOpus,
            openscreen::cast::RtpPayloadType::kVideoVp8)),
        audio_sender_(std::make_unique<OpenscreenFrameSender>(
            cast_environment(),
            kAudioConfig,
            std::move(openscreen_test_senders_.audio_sender),
            *this)),
        video_sender_(std::make_unique<OpenscreenFrameSender>(
            cast_environment(),
            kVideoConfig,
            std::move(openscreen_test_senders_.video_sender),
            *this)) {}

  void set_suggested_bitrate(int bitrate) { suggested_bitrate_ = bitrate; }

  OpenscreenFrameSender& audio_sender() { return *audio_sender_; }

  OpenscreenFrameSender& video_sender() { return *video_sender_; }

 private:
  OpenscreenTestSenders openscreen_test_senders_;
  std::unique_ptr<OpenscreenFrameSender> audio_sender_;
  std::unique_ptr<OpenscreenFrameSender> video_sender_;

  int suggested_bitrate_ = 0;
};

TEST_F(OpenscreenFrameSenderTest, RespectsTargetPlayoutDelay) {
  EXPECT_EQ(kDefaultTargetPlayoutDelay, audio_sender().GetTargetPlayoutDelay());
  EXPECT_EQ(kDefaultTargetPlayoutDelay, video_sender().GetTargetPlayoutDelay());

  const auto new_delay = base::Milliseconds(123);
  audio_sender().SetTargetPlayoutDelay(new_delay);
  video_sender().SetTargetPlayoutDelay(new_delay);
  EXPECT_EQ(new_delay, audio_sender().GetTargetPlayoutDelay());
  EXPECT_EQ(new_delay, video_sender().GetTargetPlayoutDelay());

  // Should never go below the minimum (100 milliseconds, set above).
  const auto too_low_delay = base::Milliseconds(98);
  audio_sender().SetTargetPlayoutDelay(too_low_delay);
  video_sender().SetTargetPlayoutDelay(too_low_delay);
  EXPECT_EQ(base::Milliseconds(100), audio_sender().GetTargetPlayoutDelay());
  EXPECT_EQ(base::Milliseconds(100), video_sender().GetTargetPlayoutDelay());

  // Should never go above the maximum (kDefaultTargetPlayoutDelay, set above).
  const auto too_high_delay = base::Milliseconds(1000);
  audio_sender().SetTargetPlayoutDelay(too_high_delay);
  video_sender().SetTargetPlayoutDelay(too_high_delay);
  EXPECT_EQ(kDefaultTargetPlayoutDelay, audio_sender().GetTargetPlayoutDelay());
  EXPECT_EQ(kDefaultTargetPlayoutDelay, video_sender().GetTargetPlayoutDelay());
}

TEST_F(OpenscreenFrameSenderTest, CanEnqueueFirstFrame) {
  auto audio_frame = std::make_unique<SenderEncodedFrame>();
  audio_frame->frame_id = openscreen::cast::FrameId(1);
  audio_frame->referenced_frame_id = audio_frame->frame_id;
  audio_frame->reference_time = base::TimeTicks::Now();
  audio_frame->data = base::HeapArray<uint8_t>::WithSize(10);
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped,
            audio_sender().EnqueueFrame(std::move(audio_frame)));

  auto video_frame = std::make_unique<SenderEncodedFrame>();
  video_frame->frame_id = openscreen::cast::FrameId(1);
  video_frame->referenced_frame_id = video_frame->frame_id;
  video_frame->reference_time = base::TimeTicks::Now();
  video_frame->data = base::HeapArray<uint8_t>::WithSize(10);
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped,
            video_sender().EnqueueFrame(std::move(video_frame)));
}

TEST_F(OpenscreenFrameSenderTest, RecordsRtpTimestamps) {
  constexpr RtpTimeTicks kAudioRtpTimestamp{456};
  auto audio_frame = std::make_unique<SenderEncodedFrame>();
  audio_frame->frame_id = openscreen::cast::FrameId(1);
  audio_frame->referenced_frame_id = audio_frame->frame_id;
  audio_frame->reference_time = base::TimeTicks::Now();
  audio_frame->rtp_timestamp = kAudioRtpTimestamp;
  audio_frame->data = base::HeapArray<uint8_t>::WithSize(10);

  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped,
            audio_sender().EnqueueFrame(std::move(audio_frame)));
  EXPECT_EQ(kAudioRtpTimestamp, audio_sender().GetRecordedRtpTimestamp(
                                    openscreen::cast::FrameId(1)));

  constexpr RtpTimeTicks kVideoRtpTimestamp{1337};
  auto video_frame = std::make_unique<SenderEncodedFrame>();
  video_frame->frame_id = openscreen::cast::FrameId(1);
  video_frame->referenced_frame_id = video_frame->frame_id;
  video_frame->reference_time = base::TimeTicks::Now();
  video_frame->rtp_timestamp = kVideoRtpTimestamp;
  video_frame->data = base::HeapArray<uint8_t>::WithSize(10);
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped,
            video_sender().EnqueueFrame(std::move(video_frame)));
  EXPECT_EQ(kVideoRtpTimestamp, video_sender().GetRecordedRtpTimestamp(
                                    openscreen::cast::FrameId(1)));

  // Should return empty if the frame ID is not recorded.
  EXPECT_EQ(RtpTimeTicks{}, audio_sender().GetRecordedRtpTimestamp(
                                openscreen::cast::FrameId(2000)));
  EXPECT_EQ(RtpTimeTicks{}, video_sender().GetRecordedRtpTimestamp(
                                openscreen::cast::FrameId(2000)));
}

TEST_F(OpenscreenFrameSenderTest, HandlesReferencingUnknownFrameIds) {
  // First two frames should be fine and set everything up.
  constexpr RtpTimeTicks kAudioRtpTimestamp{456};
  auto audio_frame = std::make_unique<SenderEncodedFrame>();
  audio_frame->frame_id = openscreen::cast::FrameId(1);
  audio_frame->referenced_frame_id = openscreen::cast::FrameId(1);
  audio_frame->reference_time = base::TimeTicks::Now();
  audio_frame->rtp_timestamp = kAudioRtpTimestamp;
  audio_frame->data = base::HeapArray<uint8_t>::WithSize(10);
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped,
            audio_sender().EnqueueFrame(std::move(audio_frame)));
  EXPECT_EQ(kAudioRtpTimestamp, audio_sender().GetRecordedRtpTimestamp(
                                    openscreen::cast::FrameId(1)));

  constexpr RtpTimeTicks kVideoRtpTimestamp{1337};
  auto video_frame = std::make_unique<SenderEncodedFrame>();
  video_frame->frame_id = openscreen::cast::FrameId(1);
  video_frame->referenced_frame_id = openscreen::cast::FrameId(1);
  video_frame->reference_time = base::TimeTicks::Now();
  video_frame->rtp_timestamp = kVideoRtpTimestamp;
  video_frame->data = base::HeapArray<uint8_t>::WithSize(10);
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped,
            video_sender().EnqueueFrame(std::move(video_frame)));
  EXPECT_EQ(kVideoRtpTimestamp, video_sender().GetRecordedRtpTimestamp(
                                    openscreen::cast::FrameId(1)));

  // Then the next two should be dropped for referencing unknown IDs.
  constexpr RtpTimeTicks kAudioRtpTimestampTwo{600};
  auto audio_frame_two = std::make_unique<SenderEncodedFrame>();
  audio_frame_two->frame_id = openscreen::cast::FrameId(10);
  audio_frame_two->referenced_frame_id = openscreen::cast::FrameId(9);
  audio_frame_two->reference_time = base::TimeTicks::Now();
  audio_frame_two->rtp_timestamp = kAudioRtpTimestampTwo;
  audio_frame_two->data = base::HeapArray<uint8_t>::WithSize(10);
  EXPECT_EQ(CastStreamingFrameDropReason::kInvalidReferencedFrameId,
            audio_sender().EnqueueFrame(std::move(audio_frame_two)));

  constexpr RtpTimeTicks kVideoRtpTimestampTwo{1500};
  auto video_frame_two = std::make_unique<SenderEncodedFrame>();
  video_frame_two->frame_id = openscreen::cast::FrameId(3);
  video_frame_two->referenced_frame_id = openscreen::cast::FrameId(2);
  video_frame_two->reference_time = base::TimeTicks::Now();
  video_frame_two->rtp_timestamp = kVideoRtpTimestampTwo;
  video_frame_two->data = base::HeapArray<uint8_t>::WithSize(10);
  EXPECT_EQ(CastStreamingFrameDropReason::kInvalidReferencedFrameId,
            video_sender().EnqueueFrame(std::move(video_frame_two)));
}

}  // namespace media::cast
