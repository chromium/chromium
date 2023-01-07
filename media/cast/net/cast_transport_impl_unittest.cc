// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/cast_transport_impl.h"

#include <gtest/gtest.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/values.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/cast/common/encoded_frame.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/encoded_frame.h"

using Dependency = openscreen::cast::EncodedFrame::Dependency;

namespace media {
namespace cast {

namespace {

const int64_t kStartMillisecond = INT64_C(12345678900000);
const uint32_t kVideoSsrc = 1;
const uint32_t kAudioSsrc = 2;

class StubRtcpObserver : public RtcpObserver {
 public:
  StubRtcpObserver() = default;

  StubRtcpObserver(const StubRtcpObserver&) = delete;
  StubRtcpObserver& operator=(const StubRtcpObserver&) = delete;

  void OnReceivedCastMessage(const RtcpCastMessage& cast_message) final {}
  void OnReceivedRtt(base::TimeDelta round_trip_time) final {}
  void OnReceivedPli() final {}
};

}  // namespace

class FakePacketSender : public PacketTransport {
 public:
  FakePacketSender() : paused_(false), packets_sent_(0), bytes_sent_(0) {}

  FakePacketSender(const FakePacketSender&) = delete;
  FakePacketSender& operator=(const FakePacketSender&) = delete;

  bool SendPacket(PacketRef packet, base::OnceClosure cb) final {
    if (paused_) {
      stored_packet_ = packet;
      callback_ = std::move(cb);
      return false;
    }
    ++packets_sent_;
    bytes_sent_ += packet->data.size();
    return true;
  }

  int64_t GetBytesSent() final { return bytes_sent_; }

  void StartReceiving(PacketReceiverCallbackWithStatus packet_receiver) final {}

  void StopReceiving() final {}

  void SetPaused(bool paused) {
    paused_ = paused;
    if (!paused && stored_packet_.get()) {
      SendPacket(stored_packet_, base::OnceClosure());
      std::move(callback_).Run();
    }
  }

  int packets_sent() const { return packets_sent_; }

 private:
  bool paused_;
  base::OnceClosure callback_;
  PacketRef stored_packet_;
  int packets_sent_;
  int64_t bytes_sent_;
};

class CastTransportImplTest : public ::testing::Test {
 public:
  void ReceivedLoggingEvents() { num_times_logging_callback_called_++; }

 protected:
  CastTransportImplTest() : num_times_logging_callback_called_(0) {
    testing_clock_.Advance(base::Milliseconds(kStartMillisecond));
    task_runner_ = new FakeSingleThreadTaskRunner(&testing_clock_);
  }

  ~CastTransportImplTest() override = default;

  void InitWithoutLogging();
  void InitWithOptions();
  void InitWithLogging();

  void InitializeVideo() {
    CastTransportRtpConfig rtp_config;
    rtp_config.ssrc = kVideoSsrc;
    rtp_config.feedback_ssrc = 2;
    rtp_config.rtp_payload_type = RtpPayloadType::VIDEO_VP8;
    transport_sender_->InitializeStream(rtp_config,
                                        std::make_unique<StubRtcpObserver>());
  }

  void InitializeAudio() {
    CastTransportRtpConfig rtp_config;
    rtp_config.ssrc = kAudioSsrc;
    rtp_config.feedback_ssrc = 3;
    rtp_config.rtp_payload_type = RtpPayloadType::AUDIO_OPUS;
    transport_sender_->InitializeStream(rtp_config,
                                        std::make_unique<StubRtcpObserver>());
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  std::unique_ptr<CastTransportImpl> transport_sender_;
  raw_ptr<FakePacketSender> transport_;  // Owned by CastTransport.
  int num_times_logging_callback_called_;
};

namespace {

class TransportClient : public CastTransport::Client {
 public:
  explicit TransportClient(
      CastTransportImplTest* cast_transport_sender_impl_test)
      : cast_transport_sender_impl_test_(cast_transport_sender_impl_test) {}

  TransportClient(const TransportClient&) = delete;
  TransportClient& operator=(const TransportClient&) = delete;

  void OnStatusChanged(CastTransportStatus status) final {}
  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<FrameEvent>> frame_events,
      std::unique_ptr<std::vector<PacketEvent>> packet_events) final {
    CHECK(cast_transport_sender_impl_test_);
    cast_transport_sender_impl_test_->ReceivedLoggingEvents();
  }
  void ProcessRtpPacket(std::unique_ptr<Packet> packet) final {}

 private:
  const raw_ptr<CastTransportImplTest> cast_transport_sender_impl_test_;
};

}  // namespace

void CastTransportImplTest::InitWithoutLogging() {
  transport_ = new FakePacketSender();
  transport_sender_ = std::make_unique<CastTransportImpl>(
      &testing_clock_, base::TimeDelta(),
      std::make_unique<TransportClient>(nullptr),
      base::WrapUnique(transport_.get()), task_runner_);
  task_runner_->RunTasks();
}

void CastTransportImplTest::InitWithOptions() {
  base::Value::Dict options;
  options.Set("disable_wifi_scan", true);
  options.Set("media_streaming_mode", true);
  options.Set("pacer_target_burst_size", 20);
  options.Set("pacer_max_burst_size", 100);
  transport_ = new FakePacketSender();
  transport_sender_ = std::make_unique<CastTransportImpl>(
      &testing_clock_, base::TimeDelta(),
      std::make_unique<TransportClient>(nullptr),
      base::WrapUnique(transport_.get()), task_runner_);
  transport_sender_->SetOptions(options);
  task_runner_->RunTasks();
}

void CastTransportImplTest::InitWithLogging() {
  transport_ = new FakePacketSender();
  transport_sender_ = std::make_unique<CastTransportImpl>(
      &testing_clock_, base::Milliseconds(10),
      std::make_unique<TransportClient>(this),
      base::WrapUnique(transport_.get()), task_runner_);
  task_runner_->RunTasks();
}

TEST_F(CastTransportImplTest, InitWithoutLogging) {
  InitWithoutLogging();
  task_runner_->Sleep(base::Milliseconds(50));
  EXPECT_EQ(0, num_times_logging_callback_called_);
}

TEST_F(CastTransportImplTest, InitWithOptions) {
  InitWithOptions();
  task_runner_->Sleep(base::Milliseconds(50));
  EXPECT_EQ(0, num_times_logging_callback_called_);
}

TEST_F(CastTransportImplTest, NacksCancelRetransmits) {
  InitWithLogging();
  InitializeVideo();
  task_runner_->Sleep(base::Milliseconds(50));
  EXPECT_EQ(0, num_times_logging_callback_called_);

  // A fake frame that will be decomposed into 4 packets.
  EncodedFrame fake_frame;
  fake_frame.frame_id = FrameId::first() + 1;
  fake_frame.referenced_frame_id = FrameId::first() + 1;
  fake_frame.rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(1));
  fake_frame.dependency = Dependency::kKeyFrame;
  fake_frame.data.resize(5000, ' ');

  transport_sender_->InsertFrame(kVideoSsrc, fake_frame);
  task_runner_->Sleep(base::Milliseconds(10));
  EXPECT_EQ(4, transport_->packets_sent());
  EXPECT_EQ(1, num_times_logging_callback_called_);

  // Resend packet 0.
  MissingFramesAndPacketsMap missing_packets;
  missing_packets[fake_frame.frame_id].insert(0);
  missing_packets[fake_frame.frame_id].insert(1);
  missing_packets[fake_frame.frame_id].insert(2);

  transport_->SetPaused(true);
  DedupInfo dedup_info;
  dedup_info.resend_interval = base::Milliseconds(10);
  transport_sender_->ResendPackets(kVideoSsrc, missing_packets, true,
                                   dedup_info);

  task_runner_->Sleep(base::Milliseconds(10));
  EXPECT_EQ(2, num_times_logging_callback_called_);

  RtcpCastMessage cast_message;
  cast_message.remote_ssrc = kVideoSsrc;
  cast_message.ack_frame_id = FrameId::first() + 1;
  cast_message.missing_frames_and_packets[fake_frame.frame_id].insert(3);
  transport_sender_->OnReceivedCastMessage(kVideoSsrc, cast_message);
  transport_->SetPaused(false);
  task_runner_->Sleep(base::Milliseconds(10));
  EXPECT_EQ(3, num_times_logging_callback_called_);

  // Resend one packet in the socket when unpaused.
  // Resend one more packet from NACK.
  EXPECT_EQ(6, transport_->packets_sent());
}

TEST_F(CastTransportImplTest, CancelRetransmits) {
  InitWithLogging();
  InitializeVideo();
  task_runner_->Sleep(base::Milliseconds(50));
  EXPECT_EQ(0, num_times_logging_callback_called_);

  // A fake frame that will be decomposed into 4 packets.
  EncodedFrame fake_frame;
  fake_frame.frame_id = FrameId::first() + 1;
  fake_frame.referenced_frame_id = FrameId::first() + 1;
  fake_frame.rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(1));
  fake_frame.dependency = Dependency::kKeyFrame;
  fake_frame.data.resize(5000, ' ');

  transport_sender_->InsertFrame(kVideoSsrc, fake_frame);
  task_runner_->Sleep(base::Milliseconds(10));
  EXPECT_EQ(4, transport_->packets_sent());
  EXPECT_EQ(1, num_times_logging_callback_called_);

  // Resend all packets for frame 1.
  MissingFramesAndPacketsMap missing_packets;
  missing_packets[fake_frame.frame_id].insert(kRtcpCastAllPacketsLost);

  transport_->SetPaused(true);
  DedupInfo dedup_info;
  dedup_info.resend_interval = base::Milliseconds(10);
  transport_sender_->ResendPackets(kVideoSsrc, missing_packets, true,
                                   dedup_info);

  task_runner_->Sleep(base::Milliseconds(10));
  EXPECT_EQ(2, num_times_logging_callback_called_);

  std::vector<FrameId> cancel_sending_frames;
  cancel_sending_frames.push_back(fake_frame.frame_id);
  transport_sender_->CancelSendingFrames(kVideoSsrc, cancel_sending_frames);
  transport_->SetPaused(false);
  task_runner_->Sleep(base::Milliseconds(10));
  EXPECT_EQ(2, num_times_logging_callback_called_);

  // Resend one packet in the socket when unpaused.
  EXPECT_EQ(5, transport_->packets_sent());
}

TEST_F(CastTransportImplTest, Kickstart) {
  InitWithLogging();
  InitializeVideo();
  task_runner_->Sleep(base::Milliseconds(50));
  EXPECT_EQ(0, num_times_logging_callback_called_);

  // A fake frame that will be decomposed into 4 packets.
  EncodedFrame fake_frame;
  fake_frame.frame_id = FrameId::first() + 1;
  fake_frame.referenced_frame_id = FrameId::first() + 1;
  fake_frame.rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(1));
  fake_frame.dependency = Dependency::kKeyFrame;
  fake_frame.data.resize(5000, ' ');

  transport_->SetPaused(true);
  transport_sender_->InsertFrame(kVideoSsrc, fake_frame);
  transport_sender_->ResendFrameForKickstart(kVideoSsrc, fake_frame.frame_id);
  transport_->SetPaused(false);
  task_runner_->Sleep(base::Milliseconds(10));
  EXPECT_EQ(4, transport_->packets_sent());
  EXPECT_EQ(1, num_times_logging_callback_called_);

  // Resend 2 packets for frame 1.
  MissingFramesAndPacketsMap missing_packets;
  missing_packets[fake_frame.frame_id].insert(0);
  missing_packets[fake_frame.frame_id].insert(1);

  transport_->SetPaused(true);
  DedupInfo dedup_info;
  dedup_info.resend_interval = base::Milliseconds(10);
  transport_sender_->ResendPackets(kVideoSsrc, missing_packets, true,
                                   dedup_info);
  transport_sender_->ResendFrameForKickstart(kVideoSsrc, fake_frame.frame_id);
  transport_->SetPaused(false);
  task_runner_->Sleep(base::Milliseconds(10));
  EXPECT_EQ(2, num_times_logging_callback_called_);

  // Resend one packet in the socket when unpaused.
  // Two more retransmission packets sent.
  EXPECT_EQ(7, transport_->packets_sent());
}

TEST_F(CastTransportImplTest, DedupRetransmissionWithAudio) {
  InitWithLogging();
  InitializeAudio();
  InitializeVideo();
  task_runner_->Sleep(base::Milliseconds(50));
  EXPECT_EQ(0, num_times_logging_callback_called_);

  // Send two audio frames.
  EncodedFrame fake_audio;
  fake_audio.frame_id = FrameId::first() + 1;
  fake_audio.referenced_frame_id = FrameId::first() + 1;
  fake_audio.reference_time = testing_clock_.NowTicks();
  fake_audio.dependency = Dependency::kKeyFrame;
  fake_audio.data.resize(100, ' ');
  transport_sender_->InsertFrame(kAudioSsrc, fake_audio);
  task_runner_->Sleep(base::Milliseconds(2));
  fake_audio.frame_id = FrameId::first() + 2;
  fake_audio.reference_time = testing_clock_.NowTicks();
  transport_sender_->InsertFrame(kAudioSsrc, fake_audio);
  task_runner_->Sleep(base::Milliseconds(2));
  EXPECT_EQ(2, transport_->packets_sent());

  // Ack the first audio frame.
  RtcpCastMessage cast_message;
  cast_message.remote_ssrc = kAudioSsrc;
  cast_message.ack_frame_id = FrameId::first() + 1;
  transport_sender_->OnReceivedCastMessage(kAudioSsrc, cast_message);
  task_runner_->RunTasks();
  EXPECT_EQ(2, transport_->packets_sent());
  EXPECT_EQ(0, num_times_logging_callback_called_);  // Only 4 ms since last.

  // Send a fake video frame that will be decomposed into 4 packets.
  EncodedFrame fake_video;
  fake_video.frame_id = FrameId::first() + 1;
  fake_video.referenced_frame_id = FrameId::first() + 1;
  fake_video.dependency = Dependency::kKeyFrame;
  fake_video.data.resize(5000, ' ');
  transport_sender_->InsertFrame(kVideoSsrc, fake_video);
  task_runner_->RunTasks();
  EXPECT_EQ(6, transport_->packets_sent());
  EXPECT_EQ(0, num_times_logging_callback_called_);  // Only 4 ms since last.

  // Retransmission is reject because audio is not acked yet.
  cast_message.remote_ssrc = kVideoSsrc;
  cast_message.ack_frame_id = FrameId::first();
  cast_message.missing_frames_and_packets[fake_video.frame_id].insert(3);
  task_runner_->Sleep(base::Milliseconds(10));
  transport_sender_->OnReceivedCastMessage(kVideoSsrc, cast_message);
  task_runner_->RunTasks();
  EXPECT_EQ(6, transport_->packets_sent());
  EXPECT_EQ(1, num_times_logging_callback_called_);

  // Ack the second audio frame.
  cast_message.remote_ssrc = kAudioSsrc;
  cast_message.ack_frame_id = FrameId::first() + 2;
  cast_message.missing_frames_and_packets.clear();
  task_runner_->Sleep(base::Milliseconds(2));
  transport_sender_->OnReceivedCastMessage(kAudioSsrc, cast_message);
  task_runner_->RunTasks();
  EXPECT_EQ(6, transport_->packets_sent());
  EXPECT_EQ(1, num_times_logging_callback_called_);  // Only 6 ms since last.

  // Retransmission of video packet now accepted.
  cast_message.remote_ssrc = kVideoSsrc;
  cast_message.ack_frame_id = FrameId::first() + 1;
  cast_message.missing_frames_and_packets[fake_video.frame_id].insert(3);
  task_runner_->Sleep(base::Milliseconds(2));
  transport_sender_->OnReceivedCastMessage(kVideoSsrc, cast_message);
  task_runner_->RunTasks();
  EXPECT_EQ(7, transport_->packets_sent());
  EXPECT_EQ(1, num_times_logging_callback_called_);  // Only 8 ms since last.

  task_runner_->Sleep(base::Milliseconds(2));
  EXPECT_EQ(2, num_times_logging_callback_called_);
}

}  // namespace cast
}  // namespace media
