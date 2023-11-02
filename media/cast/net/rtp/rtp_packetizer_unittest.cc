// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/rtp_packetizer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/test/simple_test_tick_clock.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/cast/common/encoded_frame.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtp/packet_storage.h"
#include "media/cast/net/rtp/rtp_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/openscreen/src/cast/streaming/encoded_frame.h"

namespace media {
namespace cast {

namespace {

constexpr int kPayload = 127;
constexpr uint32_t kTimestampMs = 10;
constexpr uint16_t kSeqNum = 33;
constexpr int kSsrc = 0x12345;
constexpr unsigned int kFrameSize = 5000;

// The maximum packet length is the internet MTU (1500) minus the IP and ICMP
// header size.
constexpr int kMaxPacketLength = 1472;

// The maximum payload size is the maximum packet size (set using the internet
// standard MTU of 1500) minus the size of all layers' headers combined.
constexpr int kMaxPayloadSize = 1449;

// Allows for enough time to execute packets for a video frame of size
// |kFrameSize|.
static int kTaskExecutionMilliseconds = 34;

}  // namespace

class TestRtpPacketTransport : public PacketTransport {
 public:
  explicit TestRtpPacketTransport(RtpPacketizerConfig config)
      : config_(config),
        sequence_number_(kSeqNum),
        packets_sent_(0),
        expected_number_of_packets_(0),
        expected_packet_id_(0),
        expected_frame_id_(FrameId::first() + 1) {}

  TestRtpPacketTransport(const TestRtpPacketTransport&) = delete;
  TestRtpPacketTransport& operator=(const TestRtpPacketTransport&) = delete;

  void VerifyRtpHeader(const RtpCastHeader& rtp_header) {
    VerifyCommonRtpHeader(rtp_header);
    VerifyCastRtpHeader(rtp_header);
  }

  void VerifyCommonRtpHeader(const RtpCastHeader& rtp_header) {
    EXPECT_EQ(kPayload, rtp_header.payload_type);
    EXPECT_EQ(sequence_number_, rtp_header.sequence_number);
    EXPECT_EQ(expected_rtp_timestamp_, rtp_header.rtp_timestamp);
    EXPECT_EQ(config_.ssrc, rtp_header.sender_ssrc);
    EXPECT_EQ(0, rtp_header.num_csrcs);
  }

  void VerifyCastRtpHeader(const RtpCastHeader& rtp_header) {
    EXPECT_FALSE(rtp_header.is_key_frame);
    EXPECT_EQ(expected_frame_id_, rtp_header.frame_id);
    EXPECT_EQ(expected_packet_id_, rtp_header.packet_id);
    EXPECT_EQ(expected_number_of_packets_ - 1, rtp_header.max_packet_id);
    EXPECT_TRUE(rtp_header.is_reference);
    EXPECT_EQ(expected_frame_id_ - 1, rtp_header.reference_frame_id);
    if (rtp_header.packet_id != 0) {
      EXPECT_EQ(rtp_header.num_extensions, 0)
          << "Extensions only allowed on first packet of a frame";
    }
  }

  bool SendPacket(PacketRef packet, base::OnceClosure cb) final {
    ++packets_sent_;
    RtpParser parser(kSsrc, kPayload);
    RtpCastHeader rtp_header;
    const uint8_t* payload_data;
    size_t payload_size;
    parser.ParsePacket(&packet->data[0], packet->data.size(), &rtp_header,
                       &payload_data, &payload_size);
    VerifyRtpHeader(rtp_header);
    ++sequence_number_;
    ++expected_packet_id_;
    return true;
  }

  int64_t GetBytesSent() final { return 0; }

  void StartReceiving(PacketReceiverCallbackWithStatus packet_receiver) final {}

  void StopReceiving() final {}

  size_t number_of_packets_received() const { return packets_sent_; }

  void set_expected_number_of_packets(size_t expected_number_of_packets) {
    expected_number_of_packets_ = expected_number_of_packets;
  }

  void set_rtp_timestamp(RtpTimeTicks rtp_timestamp) {
    expected_rtp_timestamp_ = rtp_timestamp;
  }

  RtpPacketizerConfig config_;
  uint32_t sequence_number_;
  size_t packets_sent_;
  size_t number_of_packets_;
  size_t expected_number_of_packets_;
  // Assuming packets arrive in sequence.
  int expected_packet_id_;
  FrameId expected_frame_id_;
  RtpTimeTicks expected_rtp_timestamp_;
};

class RtpPacketizerTest : public ::testing::Test {
 public:
  RtpPacketizerTest(const RtpPacketizerTest&) = delete;
  RtpPacketizerTest& operator=(const RtpPacketizerTest&) = delete;

 protected:
  RtpPacketizerTest()
      : task_runner_(new FakeSingleThreadTaskRunner(&testing_clock_)) {
    config_.sequence_number = kSeqNum;
    config_.ssrc = kSsrc;
    config_.payload_type = kPayload;
    config_.max_payload_length = kMaxPacketLength;
    transport_ = std::make_unique<TestRtpPacketTransport>(config_);
    pacer_ = std::make_unique<PacedSender>(kTargetBurstSize, kMaxBurstSize,
                                           &testing_clock_, nullptr,
                                           transport_.get(), task_runner_);
    pacer_->RegisterSsrc(config_.ssrc, false);
    rtp_packetizer_ = std::make_unique<RtpPacketizer>(
        pacer_.get(), &packet_storage_, config_);
    video_frame_.dependency =
        openscreen::cast::EncodedFrame::Dependency::kDependent;
    video_frame_.frame_id = FrameId::first() + 1;
    video_frame_.referenced_frame_id = video_frame_.frame_id - 1;
    video_frame_.data.assign(kFrameSize, 123);
    video_frame_.rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(0x0055aa11));
  }

  void RunTasks() {
    for (int i = 0; i < kTaskExecutionMilliseconds; ++i) {
      // Call process the timers every 1 ms.
      testing_clock_.Advance(base::Milliseconds(1));
      task_runner_->RunTasks();
    }
  }

  // Sends |video_frame_| to the |rtp_packetizer_|.
  void SendFrame() {
    transport_->set_rtp_timestamp(video_frame_.rtp_timestamp);
    testing_clock_.Advance(base::Milliseconds(kTimestampMs));
    video_frame_.reference_time = testing_clock_.NowTicks();
    rtp_packetizer_->SendFrameAsPackets(video_frame_);
    RunTasks();
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  EncodedFrame video_frame_;
  PacketStorage packet_storage_;
  RtpPacketizerConfig config_;
  std::unique_ptr<TestRtpPacketTransport> transport_;
  std::unique_ptr<PacedSender> pacer_;
  std::unique_ptr<RtpPacketizer> rtp_packetizer_;
};

TEST_F(RtpPacketizerTest, SendStandardPackets) {
  size_t expected_num_of_packets = kFrameSize / kMaxPacketLength + 1;
  transport_->set_expected_number_of_packets(expected_num_of_packets);
  SendFrame();
  EXPECT_EQ(expected_num_of_packets, transport_->number_of_packets_received());
}

TEST_F(RtpPacketizerTest, SendEmptyPacket) {
  video_frame_.data.clear();
  transport_->set_expected_number_of_packets(0);
  SendFrame();
  EXPECT_EQ(0u, transport_->number_of_packets_received());
}

TEST_F(RtpPacketizerTest, SendPacketOfMaximumSize) {
  video_frame_.data.assign(kMaxPayloadSize, 88);
  transport_->set_expected_number_of_packets(1);
  SendFrame();
  EXPECT_EQ(1u, transport_->number_of_packets_received());
}

TEST_F(RtpPacketizerTest, SendPacketJustAboveMaximumSize) {
  video_frame_.data.assign(kMaxPayloadSize + 1, 88);
  transport_->set_expected_number_of_packets(2);
  SendFrame();
  EXPECT_EQ(2u, transport_->number_of_packets_received());
}

TEST_F(RtpPacketizerTest, SendPacketExactMultipleOfMaximumSize) {
  video_frame_.data.assign(kMaxPayloadSize * 3, 88);
  transport_->set_expected_number_of_packets(3);
  SendFrame();
  EXPECT_EQ(3u, transport_->number_of_packets_received());
}

TEST_F(RtpPacketizerTest, SendPacketsWithAdaptivePlayoutExtension) {
  size_t expected_num_of_packets = kFrameSize / kMaxPacketLength + 1;
  transport_->set_expected_number_of_packets(expected_num_of_packets);
  video_frame_.new_playout_delay_ms = 500;
  SendFrame();
  EXPECT_EQ(expected_num_of_packets, transport_->number_of_packets_received());
}

TEST_F(RtpPacketizerTest, Stats) {
  EXPECT_FALSE(rtp_packetizer_->send_packet_count());
  EXPECT_FALSE(rtp_packetizer_->send_octet_count());
  constexpr size_t kExpectedNumOfPackets = kFrameSize / kMaxPacketLength + 1;
  transport_->set_expected_number_of_packets(kExpectedNumOfPackets);
  SendFrame();

  EXPECT_EQ(kExpectedNumOfPackets, rtp_packetizer_->send_packet_count());
  EXPECT_EQ(kFrameSize, rtp_packetizer_->send_octet_count());
  EXPECT_EQ(kExpectedNumOfPackets, transport_->number_of_packets_received());
}

}  // namespace cast
}  // namespace media
