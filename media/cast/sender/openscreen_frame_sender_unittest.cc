// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/openscreen_frame_sender.h"

#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/tracing/test_trace_processor.h"
#include "base/test/tracing/trace_test_utils.h"
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
    std::midpoint<uint32_t>(kDefaultMinVideoBitrate, kDefaultMaxVideoBitrate),
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

  void set_suggested_bitrate(uint32_t bitrate) { suggested_bitrate_ = bitrate; }

  OpenscreenFrameSender& audio_sender() { return *audio_sender_; }

  OpenscreenFrameSender& video_sender() { return *video_sender_; }

  MockSender& mock_video_sender() {
    return *static_cast<MockSender*>(video_sender_->sender_.get());
  }

 private:
  OpenscreenTestSenders openscreen_test_senders_;
  std::unique_ptr<OpenscreenFrameSender> audio_sender_;
  std::unique_ptr<OpenscreenFrameSender> video_sender_;

  uint32_t suggested_bitrate_ = 0;
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

// Frames that reference a FrameId that was never enqueued (for example, because
// the frame was dropped after encoding) cannot be decoded by the receiver and
// must be dropped until the next key frame.
TEST_F(OpenscreenFrameSenderTest, DropsFramesReferencingNeverEnqueuedFrames) {
  RtpTimeTicks rtp_timestamp{0};
  const auto enqueue = [&](int frame_id, int referenced_frame_id) {
    rtp_timestamp += RtpTimeDelta::FromTicks(300);
    auto frame = std::make_unique<SenderEncodedFrame>();
    frame->is_key_frame = frame_id == referenced_frame_id;
    frame->frame_id = FrameId::first() + frame_id;
    frame->referenced_frame_id = FrameId::first() + referenced_frame_id;
    frame->reference_time = base::TimeTicks::Now();
    frame->rtp_timestamp = rtp_timestamp;
    frame->data = base::HeapArray<uint8_t>::WithSize(10);
    return video_sender().EnqueueFrame(std::move(frame));
  };

  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped, enqueue(0, 0));
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped, enqueue(1, 0));

  // Frame 2 is never enqueued, so frames depending on it are dropped until the
  // next key frame.
  EXPECT_EQ(CastStreamingFrameDropReason::kInvalidReferencedFrameId,
            enqueue(3, 2));
  EXPECT_EQ(CastStreamingFrameDropReason::kInvalidReferencedFrameId,
            enqueue(4, 3));
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped, enqueue(5, 5));
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped, enqueue(6, 5));

  // Same again, now that the frame IDs have already diverged.
  EXPECT_EQ(CastStreamingFrameDropReason::kInvalidReferencedFrameId,
            enqueue(8, 7));
  EXPECT_EQ(CastStreamingFrameDropReason::kInvalidReferencedFrameId,
            enqueue(9, 8));
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped, enqueue(10, 10));
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped, enqueue(11, 10));
}

// Verifies the frame IDs handed to the Open Screen sender when it rejects a
// frame after encoding.
TEST_F(OpenscreenFrameSenderTest, MapsFrameIdsAfterOpenscreenRejectsFrame) {
  auto mock_sender = std::make_unique<testing::NiceMock<MockSender>>();
  MockSender& mock = *mock_sender;
  FrameId next_openscreen_frame_id = FrameId::first();
  ON_CALL(mock, GetNextFrameId()).WillByDefault([&] {
    return next_openscreen_frame_id;
  });

  OpenscreenFrameSender sender(cast_environment(), kVideoConfig,
                               std::move(mock_sender), *this);

  struct EnqueuedIds {
    FrameId frame_id;
    FrameId referenced_frame_id;
    bool operator==(const EnqueuedIds&) const = default;
  };
  std::vector<EnqueuedIds> enqueued;
  const auto enqueue =
      [&](int frame_id, int referenced_frame_id,
          openscreen::cast::Sender::EnqueueFrameResult result) {
        EXPECT_CALL(mock, EnqueueFrame(testing::_))
            .WillOnce([&, result](const openscreen::cast::EncodedFrame& frame) {
              enqueued.push_back({frame.frame_id, frame.referenced_frame_id});
              if (result == openscreen::cast::Sender::OK) {
                ++next_openscreen_frame_id;
              }
              return result;
            });
        auto frame = std::make_unique<SenderEncodedFrame>();
        frame->is_key_frame = frame_id == referenced_frame_id;
        frame->frame_id = FrameId::first() + frame_id;
        frame->referenced_frame_id = FrameId::first() + referenced_frame_id;
        frame->reference_time = base::TimeTicks::Now();
        frame->data = base::HeapArray<uint8_t>::WithSize(10);
        const CastStreamingFrameDropReason reason =
            sender.EnqueueFrame(std::move(frame));
        testing::Mock::VerifyAndClearExpectations(&mock);
        return reason;
      };

  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped,
            enqueue(0, 0, openscreen::cast::Sender::OK));
  EXPECT_EQ(CastStreamingFrameDropReason::kInFlightDurationTooHighAfterEncoding,
            enqueue(1, 0, openscreen::cast::Sender::MAX_DURATION_IN_FLIGHT));

  // Frame 2 depends on rejected frame 1, so it must never reach Open Screen.
  EXPECT_CALL(mock, EnqueueFrame(testing::_)).Times(0);
  auto frame_2 = std::make_unique<SenderEncodedFrame>();
  frame_2->frame_id = FrameId::first() + 2;
  frame_2->referenced_frame_id = FrameId::first() + 1;
  frame_2->reference_time = base::TimeTicks::Now();
  frame_2->data = base::HeapArray<uint8_t>::WithSize(10);
  EXPECT_EQ(CastStreamingFrameDropReason::kInvalidReferencedFrameId,
            sender.EnqueueFrame(std::move(frame_2)));
  testing::Mock::VerifyAndClearExpectations(&mock);

  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped,
            enqueue(3, 3, openscreen::cast::Sender::OK));
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped,
            enqueue(4, 3, openscreen::cast::Sender::OK));
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped,
            enqueue(5, 4, openscreen::cast::Sender::OK));

  const FrameId first = FrameId::first();
  EXPECT_THAT(enqueued,
              testing::ElementsAre(EnqueuedIds{first, first},
                                   // Rejected by Open Screen.
                                   EnqueuedIds{first + 1, first},
                                   // Key frame 3 takes over Open Screen ID 1.
                                   EnqueuedIds{first + 1, first + 1},
                                   EnqueuedIds{first + 2, first + 1},
                                   EnqueuedIds{first + 3, first + 2}));
}

TEST_F(OpenscreenFrameSenderTest, ReportsFramesDroppedByEncoder) {
  auto mock_sender = std::make_unique<testing::NiceMock<MockSender>>();
  MockSender& mock = *mock_sender;
  constexpr FrameId kNextFrameId = FrameId::first() + 42;
  ON_CALL(mock, GetNextFrameId()).WillByDefault(testing::Return(kNextFrameId));

  OpenscreenFrameSender sender(cast_environment(), kVideoConfig,
                               std::move(mock_sender), *this);

  constexpr RtpTimeTicks kRtpTimestamp{1234};
  EXPECT_CALL(mock, ReportFrameDropEvent(kNextFrameId, kRtpTimestamp,
                                         ToOpenscreenTimePoint(NowTicks())));
  sender.ReportFrameDroppedByEncoder(kRtpTimestamp);
}

TEST_F(OpenscreenFrameSenderTest, ClampsLargePlayoutDelay) {
  FrameSenderConfig large_config = kVideoConfig;
  large_config.min_playout_delay = base::Milliseconds(100);
  large_config.max_playout_delay = base::Milliseconds(70000);  // > 65535ms

  auto mock_sender = std::make_unique<MockSender>();

  OpenscreenFrameSender sender(cast_environment(), large_config,
                               std::move(mock_sender), *this);

  // Set a very large target playout delay.
  sender.SetTargetPlayoutDelay(base::Milliseconds(80000));

  // It should be clamped to std::numeric_limits<uint16_t>::max() (65535ms).
  EXPECT_EQ(base::Milliseconds(std::numeric_limits<uint16_t>::max()),
            sender.GetTargetPlayoutDelay());
}

TEST_F(OpenscreenFrameSenderTest, RejectedFrameDoesNotCorruptState) {
  constexpr RtpTimeTicks kFrame0Rtp{900};
  auto frame0 = std::make_unique<SenderEncodedFrame>();
  frame0->is_key_frame = true;
  frame0->frame_id = openscreen::cast::FrameId::first();
  frame0->referenced_frame_id = frame0->frame_id;
  frame0->reference_time = base::TimeTicks::Now();
  frame0->rtp_timestamp = kFrame0Rtp;
  frame0->data = base::HeapArray<uint8_t>::WithSize(10);
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped,
            video_sender().EnqueueFrame(std::move(frame0)));

  // Simulate Open Screen rejecting Frame 1 due to MAX_DURATION_IN_FLIGHT.
  EXPECT_CALL(mock_video_sender(), EnqueueFrame(testing::_))
      .WillOnce(testing::Return(openscreen::cast::Sender::EnqueueFrameResult::
                                    MAX_DURATION_IN_FLIGHT));

  constexpr RtpTimeTicks kRejectedFrame1Rtp{9900};
  auto frame1 = std::make_unique<SenderEncodedFrame>();
  frame1->is_key_frame = false;
  frame1->frame_id = openscreen::cast::FrameId::first() + 1;
  frame1->referenced_frame_id = openscreen::cast::FrameId::first();
  frame1->reference_time = base::TimeTicks::Now() + base::Milliseconds(100);
  frame1->rtp_timestamp = kRejectedFrame1Rtp;
  frame1->data = base::HeapArray<uint8_t>::WithSize(10);
  EXPECT_EQ(CastStreamingFrameDropReason::kInFlightDurationTooHighAfterEncoding,
            video_sender().EnqueueFrame(std::move(frame1)));

  // ShouldDropNextFrame must query GetInFlightMediaDuration using the last
  // successfully enqueued frame's RTP timestamp (kFrame0Rtp), not the rejected
  // frame's RTP timestamp (kRejectedFrame1Rtp).
  EXPECT_CALL(mock_video_sender(), GetInFlightMediaDuration(kFrame0Rtp))
      .WillOnce(testing::Return(std::chrono::milliseconds(0)));
  EXPECT_EQ(CastStreamingFrameDropReason::kNotDropped,
            video_sender().ShouldDropNextFrame(base::Milliseconds(33)));

  // A subsequent dependent frame referencing the rejected Frame 1 must be
  // dropped with kInvalidReferencedFrameId.
  auto frame2 = std::make_unique<SenderEncodedFrame>();
  frame2->is_key_frame = false;
  frame2->frame_id = openscreen::cast::FrameId::first() + 2;
  frame2->referenced_frame_id = openscreen::cast::FrameId::first() + 1;
  frame2->reference_time = base::TimeTicks::Now() + base::Milliseconds(133);
  frame2->rtp_timestamp = RtpTimeTicks{12900};
  frame2->data = base::HeapArray<uint8_t>::WithSize(10);
  EXPECT_EQ(CastStreamingFrameDropReason::kInvalidReferencedFrameId,
            video_sender().EnqueueFrame(std::move(frame2)));
}

// Each accepted frame gets a transport slice named for its media type. The
// slice ends when the frame is canceled, usually because the Receiver ACKed it.
TEST_F(OpenscreenFrameSenderTest, TracesEachFrameUntilItIsCanceled) {
  base::test::TracingEnvironment tracing_environment;
  base::test::TestTraceProcessor trace_processor;
  trace_processor.StartTrace("cast.stream");

  const auto make_key_frame = [](size_t payload_bytes) {
    auto frame = std::make_unique<SenderEncodedFrame>();
    frame->is_key_frame = true;
    frame->frame_id = FrameId::first();
    frame->referenced_frame_id = frame->frame_id;
    frame->reference_time = base::TimeTicks::Now();
    frame->rtp_timestamp = RtpTimeTicks{100};
    frame->data = base::HeapArray<uint8_t>::WithSize(payload_bytes);
    return frame;
  };

  // Audio goes first. The video slice must still be named for video.
  ASSERT_EQ(CastStreamingFrameDropReason::kNotDropped,
            audio_sender().EnqueueFrame(make_key_frame(20)));
  ASSERT_EQ(CastStreamingFrameDropReason::kNotDropped,
            video_sender().EnqueueFrame(make_key_frame(1000)));

  // The Receiver ACKs the video frame. The audio frame stays in flight.
  mock_video_sender().observer()->OnFrameCanceled(FrameId::first());

  const absl::Status status = trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  const auto result = trace_processor.RunQuery(R"(
    SELECT
      name,
      dur >= 0 AS ended,
      extract_arg(arg_set_id, 'debug.payload_bytes') AS payload_bytes,
      extract_arg(arg_set_id, 'debug.is_key_frame') AS is_key_frame
    FROM slice
    WHERE category = 'cast.stream'
    ORDER BY name
  )");
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(
      result.value(),
      testing::ElementsAre(
          std::vector<std::string>{"name", "ended", "payload_bytes",
                                   "is_key_frame"},
          std::vector<std::string>{"Audio Transport", "0", "20", "1"},
          std::vector<std::string>{"Video Transport", "1", "1000", "1"}));
}

}  // namespace media::cast
