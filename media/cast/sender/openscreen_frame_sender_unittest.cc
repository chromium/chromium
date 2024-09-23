// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/openscreen_frame_sender.h"

#include <memory>
#include <numeric>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "components/openscreen_platform/task_runner.h"
#include "media/base/audio_codecs.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/video_codecs.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/public/environment.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"
#include "third_party/openscreen/src/cast/streaming/sender_packet_router.h"
#include "third_party/openscreen/src/platform/api/time.h"
#include "third_party/openscreen/src/platform/base/trivial_clock_traits.h"

namespace media::cast {
namespace {

constexpr uint32_t kFirstSsrc = 35535;
constexpr int kRtpTimebase = 9000;
constexpr char kAesSecretKey[] = "65386FD9BCC30BC7FB6A4DD1D3B0FA5E";
constexpr char kAesIvMask[] = "64A6AAC2821880145271BB15B0188821";
constexpr int kAudioBitrate = 100 * 1000;

static const FrameSenderConfig kAudioConfig{
    kFirstSsrc,
    kFirstSsrc + 1,
    base::Milliseconds(100),
    kDefaultTargetPlayoutDelay,
    RtpPayloadType::AUDIO_OPUS,
    /* use_hardware_encoder= */ false,
    kDefaultAudioSamplingRate,
    /* channels= */ 2,
    kAudioBitrate,
    kAudioBitrate,
    kAudioBitrate,
    kDefaultMaxFrameRate,
    kAesSecretKey,
    kAesIvMask,
    std::nullopt,
    AudioCodecParams{.codec = AudioCodec::kOpus}};
static const openscreen::cast::SessionConfig kOpenscreenAudioConfig =
    ToOpenscreenSessionConfig(kAudioConfig, /* is_pli_enabled= */ true);

static const FrameSenderConfig kVideoConfig{
    kFirstSsrc + 2,
    kFirstSsrc + 3,
    base::Milliseconds(100),
    kDefaultTargetPlayoutDelay,
    RtpPayloadType::VIDEO_VP8,
    /* use_hardware_encoder= */ false,
    kRtpTimebase,
    /* channels = */ 1,
    kDefaultMaxVideoBitrate,
    kDefaultMinVideoBitrate,
    std::midpoint<int>(kDefaultMinVideoBitrate, kDefaultMaxVideoBitrate),
    kDefaultMaxFrameRate,
    kAesSecretKey,
    kAesIvMask,
    VideoCodecParams(VideoCodec::kVP8),
    std::nullopt};
static const openscreen::cast::SessionConfig kOpenscreenVideoConfig =
    ToOpenscreenSessionConfig(kVideoConfig, /* is_pli_enabled= */ true);

}  // namespace

class OpenscreenFrameSenderTest : public ::testing::Test,
                                  public FrameSender::Client {
 public:
  // FrameSender::Client overrides.
  int GetNumberOfFramesInEncoder() const override { return 0; }
  base::TimeDelta GetEncoderBacklogDuration() const override { return {}; }
  void OnFrameCanceled(FrameId frame_id) override {}

  int get_suggested_bitrate() { return suggested_bitrate_; }

 protected:
  OpenscreenFrameSenderTest()
      : task_runner_(
            base::MakeRefCounted<FakeSingleThreadTaskRunner>(&testing_clock_)),
        cast_environment_(base::MakeRefCounted<CastEnvironment>(&testing_clock_,
                                                                task_runner_,
                                                                task_runner_,
                                                                task_runner_)),
        openscreen_task_runner_(task_runner_),
        openscreen_environment_(openscreen::Clock::now,
                                openscreen_task_runner_,
                                openscreen::IPEndpoint::kAnyV4()),
        openscreen_packet_router_(openscreen_environment_,
                                  20,
                                  std::chrono::milliseconds(10)) {
    auto openscreen_audio_sender = std::make_unique<openscreen::cast::Sender>(
        openscreen_environment_, openscreen_packet_router_,
        kOpenscreenAudioConfig, openscreen::cast::RtpPayloadType::kAudioOpus);
    auto openscreen_video_sender = std::make_unique<openscreen::cast::Sender>(
        openscreen_environment_, openscreen_packet_router_,
        kOpenscreenVideoConfig, openscreen::cast::RtpPayloadType::kVideoVp8);

    audio_sender_ = std::make_unique<OpenscreenFrameSender>(
        cast_environment_, kAudioConfig, std::move(openscreen_audio_sender),
        *this,
        base::BindRepeating(&OpenscreenFrameSenderTest::get_suggested_bitrate,
                            // Safe because we destroy the audio sender before
                            // destroying `this`.
                            base::Unretained(this)));

    video_sender_ = std::make_unique<OpenscreenFrameSender>(
        cast_environment_, kVideoConfig, std::move(openscreen_video_sender),
        *this,
        base::BindRepeating(&OpenscreenFrameSenderTest::get_suggested_bitrate,
                            // Safe because we destroy the audio sender before
                            // destroying `this`.
                            base::Unretained(this)));
  }

  void RecordShouldDropNextFrame(bool should_drop) {
    video_sender_->RecordShouldDropNextFrame(should_drop);
  }

  void set_suggested_bitrate(int bitrate) { suggested_bitrate_ = bitrate; }

  OpenscreenFrameSender& audio_sender() { return *audio_sender_; }

  OpenscreenFrameSender& video_sender() { return *video_sender_; }

 private:
  base::SimpleTestTickClock testing_clock_;
  const scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  const scoped_refptr<CastEnvironment> cast_environment_;

  // openscreen::Sender related classes.
  openscreen_platform::TaskRunner openscreen_task_runner_;
  openscreen::cast::Environment openscreen_environment_;
  openscreen::cast::SenderPacketRouter openscreen_packet_router_;
  std::unique_ptr<openscreen::cast::Sender> openscreen_video_sender_;
  std::unique_ptr<openscreen::cast::Sender> openscreen_audio_sender_;

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
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped,
            audio_sender().EnqueueFrame(std::move(audio_frame)));

  auto video_frame = std::make_unique<SenderEncodedFrame>();
  video_frame->frame_id = openscreen::cast::FrameId(1);
  video_frame->referenced_frame_id = video_frame->frame_id;
  video_frame->reference_time = base::TimeTicks::Now();
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
  EXPECT_EQ(CastStreamingFrameDropReason::kInvalidReferencedFrameId,
            audio_sender().EnqueueFrame(std::move(audio_frame_two)));

  constexpr RtpTimeTicks kVideoRtpTimestampTwo{1500};
  auto video_frame_two = std::make_unique<SenderEncodedFrame>();
  video_frame_two->frame_id = openscreen::cast::FrameId(3);
  video_frame_two->referenced_frame_id = openscreen::cast::FrameId(2);
  video_frame_two->reference_time = base::TimeTicks::Now();
  video_frame_two->rtp_timestamp = kVideoRtpTimestampTwo;
  EXPECT_EQ(CastStreamingFrameDropReason::kInvalidReferencedFrameId,
            video_sender().EnqueueFrame(std::move(video_frame_two)));
}

TEST_F(OpenscreenFrameSenderTest, HandlesSuggestedBitratesCorrectly) {
  // NOTE: the VideoBitrateSuggester tests this workflow more thoroughly.

  // We should start with the maximum video bitrate.
  set_suggested_bitrate(5000001);
  EXPECT_EQ(5000000, video_sender().GetSuggestedBitrate(base::TimeTicks{},
                                                        base::TimeDelta{}));

  // It should cap at the bitrate suggested by Open Screen.
  set_suggested_bitrate(4998374);
  EXPECT_EQ(4998374, video_sender().GetSuggestedBitrate(base::TimeTicks{},
                                                        base::TimeDelta{}));
}

}  // namespace media::cast
