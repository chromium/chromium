// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_sender.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/video_frame_factory.h"
#include "media/cast/constants.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_impl.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/test/fake_video_encode_accelerator_factory.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::cast {

namespace {
static const uint8_t kPixelValue = 123;
static const int kWidth = 320;
static const int kHeight = 240;

using testing::_;
using testing::AtLeast;

void SaveOperationalStatus(OperationalStatus* out_status,
                           OperationalStatus in_status) {
  DVLOG(1) << "OperationalStatus transitioning from " << *out_status << " to "
           << in_status;
  *out_status = in_status;
}

class TestPacketSender : public PacketTransport {
 public:
  TestPacketSender()
      : number_of_rtp_packets_(0), number_of_rtcp_packets_(0), paused_(false) {}

  TestPacketSender(const TestPacketSender&) = delete;
  TestPacketSender& operator=(const TestPacketSender&) = delete;

  // A singular packet implies a RTCP packet.
  bool SendPacket(PacketRef packet, base::OnceClosure cb) final {
    if (paused_) {
      stored_packet_ = packet;
      callback_ = std::move(cb);
      return false;
    }
    if (IsRtcpPacket(&packet->data[0], packet->data.size())) {
      ++number_of_rtcp_packets_;
    } else {
      // Check that at least one RTCP packet was sent before the first RTP
      // packet.  This confirms that the receiver will have the necessary lip
      // sync info before it has to calculate the playout time of the first
      // frame.
      if (number_of_rtp_packets_ == 0)
        EXPECT_LE(1, number_of_rtcp_packets_);
      ++number_of_rtp_packets_;
    }
    return true;
  }

  int64_t GetBytesSent() final { return 0; }

  void StartReceiving(PacketReceiverCallbackWithStatus packet_receiver) final {}

  void StopReceiving() final {}

  int number_of_rtp_packets() const { return number_of_rtp_packets_; }

  int number_of_rtcp_packets() const { return number_of_rtcp_packets_; }

  void SetPause(bool paused) {
    paused_ = paused;
    if (!paused && stored_packet_.get()) {
      SendPacket(stored_packet_, base::OnceClosure());
      std::move(callback_).Run();
    }
  }

 private:
  int number_of_rtp_packets_;
  int number_of_rtcp_packets_;
  bool paused_;
  base::OnceClosure callback_;
  PacketRef stored_packet_;
};

void IgnorePlayoutDelayChanges(base::TimeDelta unused_playout_delay) {}

class PeerVideoSender : public VideoSender {
 public:
  PeerVideoSender(
      scoped_refptr<CastEnvironment> cast_environment,
      const FrameSenderConfig& video_config,
      const StatusChangeCallback& status_change_cb,
      const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
      CastTransport* const transport_sender)
      : VideoSender(cast_environment,
                    video_config,
                    status_change_cb,
                    create_vea_cb,
                    transport_sender,
                    base::BindRepeating(&IgnorePlayoutDelayChanges),
                    base::BindRepeating(&PeerVideoSender::ProcessFeedback,
                                        base::Unretained(this))) {}

  void OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback) {
    frame_sender_for_testing()->OnReceivedCastFeedback(cast_feedback);
  }

  void OnReceivedPli() { frame_sender_for_testing()->OnReceivedPli(); }

  void ProcessFeedback(const media::VideoCaptureFeedback& feedback) {
    feedback_ = feedback;
  }

  VideoCaptureFeedback GetFeedback() { return feedback_; }

 private:
  VideoCaptureFeedback feedback_;
};

class TransportClient : public CastTransport::Client {
 public:
  TransportClient() = default;

  TransportClient(const TransportClient&) = delete;
  TransportClient& operator=(const TransportClient&) = delete;

  void OnStatusChanged(CastTransportStatus status) final {
    EXPECT_EQ(TRANSPORT_STREAM_INITIALIZED, status);
  }
  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<FrameEvent>> frame_events,
      std::unique_ptr<std::vector<PacketEvent>> packet_events) final {}
  void ProcessRtpPacket(std::unique_ptr<Packet> packet) final {}
};

}  // namespace

class VideoSenderTest : public ::testing::Test {
 public:
  VideoSenderTest(const VideoSenderTest&) = delete;
  VideoSenderTest& operator=(const VideoSenderTest&) = delete;

 protected:
  VideoSenderTest()
      : task_runner_(new FakeSingleThreadTaskRunner(&testing_clock_)),
        cast_environment_(new CastEnvironment(&testing_clock_,
                                              task_runner_,
                                              task_runner_,
                                              task_runner_)),
        operational_status_(STATUS_UNINITIALIZED),
        vea_factory_(task_runner_) {
    testing_clock_.Advance(base::TimeTicks::Now() - base::TimeTicks());
    vea_factory_.SetAutoRespond(true);
    last_pixel_value_ = kPixelValue;
    transport_ = new TestPacketSender();
    transport_sender_ = std::make_unique<CastTransportImpl>(
        &testing_clock_, base::TimeDelta(), std::make_unique<TransportClient>(),
        base::WrapUnique(transport_.get()), task_runner_);
  }

  ~VideoSenderTest() override = default;

  void TearDown() final {
    video_sender_.reset();
    task_runner_->RunTasks();
  }

  // If |external| is true then external video encoder (VEA) is used.
  // |expect_init_success| is true if initialization is expected to succeed.
  void InitEncoder(bool external, bool expect_init_success) {
    FrameSenderConfig video_config = GetDefaultVideoSenderConfig();
    video_config.use_hardware_encoder = external;

    ASSERT_EQ(operational_status_, STATUS_UNINITIALIZED);

    if (external) {
      vea_factory_.SetInitializationWillSucceed(expect_init_success);
      video_sender_ = std::make_unique<PeerVideoSender>(
          cast_environment_, video_config,
          base::BindRepeating(&SaveOperationalStatus, &operational_status_),
          base::BindRepeating(
              &FakeVideoEncodeAcceleratorFactory::CreateVideoEncodeAccelerator,
              base::Unretained(&vea_factory_)),
          transport_sender_.get());
    } else {
      video_sender_ = std::make_unique<PeerVideoSender>(
          cast_environment_, video_config,
          base::BindRepeating(&SaveOperationalStatus, &operational_status_),
          base::DoNothing(), transport_sender_.get());
    }
    task_runner_->RunTasks();
  }

  scoped_refptr<media::VideoFrame> GetNewVideoFrame() {
    if (first_frame_timestamp_.is_null())
      first_frame_timestamp_ = testing_clock_.NowTicks();
    gfx::Size size(kWidth, kHeight);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(
            PIXEL_FORMAT_I420, size, gfx::Rect(size), size,
            testing_clock_.NowTicks() - first_frame_timestamp_);
    PopulateVideoFrame(video_frame.get(), last_pixel_value_++);
    return video_frame;
  }

  scoped_refptr<media::VideoFrame> GetLargeNewVideoFrame() {
    if (first_frame_timestamp_.is_null())
      first_frame_timestamp_ = testing_clock_.NowTicks();
    gfx::Size size(kWidth, kHeight);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(
            PIXEL_FORMAT_I420, size, gfx::Rect(size), size,
            testing_clock_.NowTicks() - first_frame_timestamp_);
    PopulateVideoFrameWithNoise(video_frame.get());
    return video_frame;
  }

  void RunTasks(int during_ms) {
    task_runner_->Sleep(base::Milliseconds(during_ms));
  }

  base::SimpleTestTickClock testing_clock_;
  const scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  const scoped_refptr<CastEnvironment> cast_environment_;
  OperationalStatus operational_status_;
  FakeVideoEncodeAcceleratorFactory vea_factory_;
  raw_ptr<TestPacketSender> transport_;  // Owned by CastTransport.
  std::unique_ptr<CastTransportImpl> transport_sender_;
  std::unique_ptr<PeerVideoSender> video_sender_;
  int last_pixel_value_;
  base::TimeTicks first_frame_timestamp_;
};

TEST_F(VideoSenderTest, BuiltInEncoder) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  const base::TimeTicks reference_time = testing_clock_.NowTicks();
  video_sender_->InsertRawVideoFrame(video_frame, reference_time);

  task_runner_->RunTasks();
  EXPECT_LE(1, transport_->number_of_rtp_packets());
  EXPECT_LE(1, transport_->number_of_rtcp_packets());
}

TEST_F(VideoSenderTest, ExternalEncoder) {
  InitEncoder(true, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  // The SizeAdaptableExternalVideoEncoder initally reports STATUS_INITIALIZED
  // so that frames will be sent to it.  Therefore, no encoder activity should
  // have occurred at this point.  Send a frame to spurn creation of the
  // underlying ExternalVideoEncoder instance.
  if (vea_factory_.vea_response_count() == 0) {
    video_sender_->InsertRawVideoFrame(GetNewVideoFrame(),
                                       testing_clock_.NowTicks());
    task_runner_->RunTasks();
  }
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);
  RunTasks(33);

  // VideoSender created an encoder for 1280x720 frames, in order to provide the
  // INITIALIZED status.
  EXPECT_EQ(1, vea_factory_.vea_response_count());

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  for (int i = 0; i < 3; ++i) {
    const base::TimeTicks reference_time = testing_clock_.NowTicks();
    video_sender_->InsertRawVideoFrame(video_frame, reference_time);
    RunTasks(33);
    // VideoSender re-created the encoder for the 320x240 frames we're
    // providing.
    EXPECT_EQ(1, vea_factory_.vea_response_count());
  }

  video_sender_.reset(NULL);
  task_runner_->RunTasks();
  EXPECT_EQ(1, vea_factory_.vea_response_count());
}

TEST_F(VideoSenderTest, ExternalEncoderInitFails) {
  InitEncoder(true, false);

  // The SizeAdaptableExternalVideoEncoder initally reports STATUS_INITIALIZED
  // so that frames will be sent to it.  Send a frame to spurn creation of the
  // underlying ExternalVideoEncoder instance, which should result in failure.
  if (operational_status_ == STATUS_INITIALIZED ||
      operational_status_ == STATUS_CODEC_REINIT_PENDING) {
    video_sender_->InsertRawVideoFrame(GetNewVideoFrame(),
                                       testing_clock_.NowTicks());
    task_runner_->RunTasks();
  }
  EXPECT_EQ(STATUS_CODEC_INIT_FAILED, operational_status_);

  video_sender_.reset(NULL);
  task_runner_->RunTasks();
}

TEST_F(VideoSenderTest, RtcpTimer) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  const base::TimeTicks reference_time = testing_clock_.NowTicks();
  video_sender_->InsertRawVideoFrame(video_frame, reference_time);

  // Make sure that we send at least one RTCP packet.
  base::TimeDelta max_rtcp_timeout =
      base::Milliseconds(1) + kRtcpReportInterval * 3 / 2;

  RunTasks(max_rtcp_timeout.InMilliseconds());
  EXPECT_LE(1, transport_->number_of_rtp_packets());
  EXPECT_LE(1, transport_->number_of_rtcp_packets());
  // Build Cast msg and expect RTCP packet.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.remote_ssrc = 2;
  cast_feedback.ack_frame_id = FrameId::first();
  video_sender_->OnReceivedCastFeedback(cast_feedback);
  RunTasks(max_rtcp_timeout.InMilliseconds());
  EXPECT_LE(1, transport_->number_of_rtcp_packets());
}

TEST_F(VideoSenderTest, ResendTimer) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

  const base::TimeTicks reference_time = testing_clock_.NowTicks();
  video_sender_->InsertRawVideoFrame(video_frame, reference_time);

  // ACK the key frame.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.remote_ssrc = 2;
  cast_feedback.ack_frame_id = FrameId::first();
  video_sender_->OnReceivedCastFeedback(cast_feedback);

  video_frame = GetNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, reference_time);

  base::TimeDelta max_resend_timeout =
      kDefaultTargetPlayoutDelay + base::Milliseconds(1);

  // Make sure that we do a re-send.
  RunTasks(max_resend_timeout.InMilliseconds());
  // Should have sent at least 3 packets.
  EXPECT_LE(3, transport_->number_of_rtp_packets() +
                   transport_->number_of_rtcp_packets());
}

TEST_F(VideoSenderTest, LogAckReceivedEvent) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  SimpleEventSubscriber event_subscriber;
  cast_environment_->logger()->Subscribe(&event_subscriber);

  int num_frames = 10;
  for (int i = 0; i < num_frames; i++) {
    scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

    const base::TimeTicks reference_time = testing_clock_.NowTicks();
    video_sender_->InsertRawVideoFrame(video_frame, reference_time);
    RunTasks(33);
  }

  task_runner_->RunTasks();

  RtcpCastMessage cast_feedback(1);
  cast_feedback.ack_frame_id = FrameId::first() + num_frames - 1;

  video_sender_->OnReceivedCastFeedback(cast_feedback);

  std::vector<FrameEvent> frame_events;
  event_subscriber.GetFrameEventsAndReset(&frame_events);

  ASSERT_TRUE(!frame_events.empty());
  EXPECT_EQ(FRAME_ACK_RECEIVED, frame_events.rbegin()->type);
  EXPECT_EQ(VIDEO_EVENT, frame_events.rbegin()->media_type);
  EXPECT_EQ(FrameId::first() + num_frames - 1, frame_events.rbegin()->frame_id);

  cast_environment_->logger()->Unsubscribe(&event_subscriber);
}

TEST_F(VideoSenderTest, StopSendingInTheAbsenceOfAck) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  // Send a stream of frames and don't ACK; by default we shouldn't have more
  // than 4 frames in flight.
  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, testing_clock_.NowTicks());
  // Give time for the frame to be processed plus handling some of the playout
  // delay.
  RunTasks(300);

  // Send 3 more frames and record the number of packets sent.
  for (int i = 0; i < 3; ++i) {
    video_frame = GetNewVideoFrame();
    video_sender_->InsertRawVideoFrame(video_frame, testing_clock_.NowTicks());
    RunTasks(33);
  }
  const int number_of_packets_sent = transport_->number_of_rtp_packets();

  // Send 3 more frames - they should not be encoded, as we have not received
  // any acks.
  for (int i = 0; i < 3; ++i) {
    video_frame = GetNewVideoFrame();
    video_sender_->InsertRawVideoFrame(video_frame, testing_clock_.NowTicks());
    RunTasks(33);
  }

  // We expect a frame to be retransmitted because of duplicated ACKs.
  // Only one packet of the frame is re-transmitted.
  EXPECT_EQ(number_of_packets_sent + 1, transport_->number_of_rtp_packets());

  // Start acking and make sure we're back to steady-state.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.remote_ssrc = 2;
  cast_feedback.ack_frame_id = FrameId::first();
  video_sender_->OnReceivedCastFeedback(cast_feedback);
  EXPECT_LE(4, transport_->number_of_rtp_packets() +
                   transport_->number_of_rtcp_packets());

  // Empty the pipeline.
  RunTasks(100);
  // Should have sent at least 7 packets.
  EXPECT_LE(7, transport_->number_of_rtp_packets() +
                   transport_->number_of_rtcp_packets());
}

TEST_F(VideoSenderTest, DuplicateAckRetransmit) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, testing_clock_.NowTicks());
  RunTasks(33);
  RtcpCastMessage cast_feedback(1);
  cast_feedback.remote_ssrc = 2;
  cast_feedback.ack_frame_id = FrameId::first();

  // Send 3 more frames but don't ACK.
  for (int i = 0; i < 3; ++i) {
    video_frame = GetNewVideoFrame();
    video_sender_->InsertRawVideoFrame(video_frame, testing_clock_.NowTicks());
    RunTasks(33);
  }
  const int number_of_packets_sent = transport_->number_of_rtp_packets();

  // Send duplicated ACKs and mix some invalid NACKs.
  for (int i = 0; i < 10; ++i) {
    RtcpCastMessage ack_feedback(1);
    ack_feedback.remote_ssrc = 2;
    ack_feedback.ack_frame_id = FrameId::first();
    RtcpCastMessage nack_feedback(1);
    nack_feedback.remote_ssrc = 2;
    nack_feedback.missing_frames_and_packets[FrameId::first() + 255] =
        PacketIdSet();
    video_sender_->OnReceivedCastFeedback(ack_feedback);
    video_sender_->OnReceivedCastFeedback(nack_feedback);
  }
  EXPECT_EQ(number_of_packets_sent, transport_->number_of_rtp_packets());

  // Re-transmit one packet because of duplicated ACKs.
  for (int i = 0; i < 3; ++i) {
    RtcpCastMessage ack_feedback(1);
    ack_feedback.remote_ssrc = 2;
    ack_feedback.ack_frame_id = FrameId::first();
    video_sender_->OnReceivedCastFeedback(ack_feedback);
  }
  EXPECT_EQ(number_of_packets_sent + 1, transport_->number_of_rtp_packets());
}

TEST_F(VideoSenderTest, DuplicateAckRetransmitDoesNotCancelRetransmits) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, testing_clock_.NowTicks());
  RunTasks(33);
  RtcpCastMessage cast_feedback(1);
  cast_feedback.remote_ssrc = 2;
  cast_feedback.ack_frame_id = FrameId::first();

  // Send 2 more frames but don't ACK.
  for (int i = 0; i < 2; ++i) {
    video_frame = GetNewVideoFrame();
    video_sender_->InsertRawVideoFrame(video_frame, testing_clock_.NowTicks());
    RunTasks(33);
  }
  // Pause the transport
  transport_->SetPause(true);

  // Insert one more video frame.
  video_frame = GetLargeNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, testing_clock_.NowTicks());
  RunTasks(33);

  const int number_of_packets_sent = transport_->number_of_rtp_packets();

  // Send duplicated ACKs and mix some invalid NACKs.
  for (int i = 0; i < 10; ++i) {
    RtcpCastMessage ack_feedback(1);
    ack_feedback.remote_ssrc = 2;
    ack_feedback.ack_frame_id = FrameId::first();
    RtcpCastMessage nack_feedback(1);
    nack_feedback.remote_ssrc = 2;
    nack_feedback.missing_frames_and_packets[FrameId::first() + 255] =
        PacketIdSet();
    video_sender_->OnReceivedCastFeedback(ack_feedback);
    video_sender_->OnReceivedCastFeedback(nack_feedback);
  }
  EXPECT_EQ(number_of_packets_sent, transport_->number_of_rtp_packets());

  // Re-transmit one packet because of duplicated ACKs.
  for (int i = 0; i < 3; ++i) {
    RtcpCastMessage ack_feedback(1);
    ack_feedback.remote_ssrc = 2;
    ack_feedback.ack_frame_id = FrameId::first();
    video_sender_->OnReceivedCastFeedback(ack_feedback);
  }

  transport_->SetPause(false);
  RunTasks(100);
  EXPECT_LT(number_of_packets_sent + 1, transport_->number_of_rtp_packets());
}

TEST_F(VideoSenderTest, AcksCancelRetransmits) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  transport_->SetPause(true);
  scoped_refptr<media::VideoFrame> video_frame = GetLargeNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, testing_clock_.NowTicks());
  RunTasks(33);

  // Frame should be in buffer, waiting. Now let's ack it.
  RtcpCastMessage cast_feedback(1);
  cast_feedback.remote_ssrc = 2;
  cast_feedback.ack_frame_id = FrameId::first();
  video_sender_->OnReceivedCastFeedback(cast_feedback);

  transport_->SetPause(false);
  RunTasks(33);
  EXPECT_EQ(0, transport_->number_of_rtp_packets());
}

TEST_F(VideoSenderTest, CheckVideoFrameFactoryIsNull) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  EXPECT_EQ(nullptr, video_sender_->CreateVideoFrameFactory().get());
}

TEST_F(VideoSenderTest, ReportsResourceUtilizationInCallback) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  for (int i = 0; i < 3; ++i) {
    scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();

    const base::TimeTicks reference_time = testing_clock_.NowTicks();
    video_sender_->InsertRawVideoFrame(video_frame, reference_time);

    // Run encode tasks.  VideoSender::OnEncodedVideoFrame() will be called once
    // encoding of the frame is complete, and this is when the
    // resource_utilization metadata is populated.
    RunTasks(33);

    // Check that the resource_utilization value is set and non-negative.  Don't
    // check for specific values because they are dependent on real-world CPU
    // encode time, which can vary across test runs.
    double utilization = video_sender_->GetFeedback().resource_utilization;
    EXPECT_LE(0.0, utilization);
    if (i == 0)
      EXPECT_GE(1.0, utilization);  // Key frames never exceed 1.0.
    DVLOG(1) << "Utilization computed by VideoSender is: " << utilization;
  }
}

TEST_F(VideoSenderTest, CancelSendingOnReceivingPli) {
  InitEncoder(false, true);
  ASSERT_EQ(STATUS_INITIALIZED, operational_status_);

  // Send a frame and ACK it.
  scoped_refptr<media::VideoFrame> video_frame = GetNewVideoFrame();
  video_sender_->InsertRawVideoFrame(video_frame, testing_clock_.NowTicks());
  RunTasks(33);

  RtcpCastMessage cast_feedback(1);
  cast_feedback.remote_ssrc = 2;
  cast_feedback.ack_frame_id = FrameId::first();
  video_sender_->OnReceivedCastFeedback(cast_feedback);

  transport_->SetPause(true);
  // Send three more frames.
  for (int i = 0; i < 3; i++) {
    video_frame = GetNewVideoFrame();
    video_sender_->InsertRawVideoFrame(video_frame, testing_clock_.NowTicks());
    RunTasks(33);
  }
  EXPECT_EQ(1, transport_->number_of_rtp_packets());

  // Frames should be in buffer, waiting.
  // Received PLI from receiver.
  video_sender_->OnReceivedPli();
  video_frame = GetNewVideoFrame();
  video_sender_->InsertRawVideoFrame(
      video_frame, testing_clock_.NowTicks() + base::Milliseconds(1000));
  RunTasks(33);
  transport_->SetPause(false);
  RunTasks(33);
  EXPECT_EQ(2, transport_->number_of_rtp_packets());
}

}  // namespace media::cast
