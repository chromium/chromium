// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/big_endian.h"
#include "base/containers/circular_deque.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace media {
namespace cast {
namespace {

const uint8_t kValue = 123;
const size_t kSize1 = 101;
const size_t kSize2 = 102;
const size_t kSize3 = 103;
const size_t kSize4 = 104;
const size_t kNackSize = 105;
const int64_t kStartMillisecond = INT64_C(12345678900000);
const uint32_t kVideoSsrc = 0x1234;
const uint32_t kAudioSsrc = 0x5678;
const uint32_t kVideoFrameRtpTimestamp = 12345;
const uint32_t kAudioFrameRtpTimestamp = 23456;

// RTCP packets don't really have a packet ID.  However, the bytes where
// TestPacketSender checks for the ID should be set to 31611, so we'll just
// check that.
const uint16_t kRtcpPacketIdMagic = UINT16_C(31611);

class TestPacketSender : public PacketTransport {
 public:
  TestPacketSender() : bytes_sent_(0) {}

  TestPacketSender(const TestPacketSender&) = delete;
  TestPacketSender& operator=(const TestPacketSender&) = delete;

  bool SendPacket(PacketRef packet, base::OnceClosure cb) final {
    EXPECT_FALSE(expected_packet_sizes_.empty());
    size_t expected_packet_size = expected_packet_sizes_.front();
    expected_packet_sizes_.pop_front();
    EXPECT_EQ(expected_packet_size, packet->data.size());
    bytes_sent_ += packet->data.size();

    // Parse for the packet ID and confirm it is the next one we expect.
    EXPECT_LE(kSize1, packet->data.size());
    base::BigEndianReader reader(packet->data);
    bool success = reader.Skip(14);
    uint16_t packet_id = 0xffff;
    success &= reader.ReadU16(&packet_id);
    EXPECT_TRUE(success);
    const uint16_t expected_packet_id = expected_packet_ids_.front();
    expected_packet_ids_.pop_front();
    EXPECT_EQ(expected_packet_id, packet_id);

    return true;
  }

  int64_t GetBytesSent() final { return bytes_sent_; }

  void StartReceiving(PacketReceiverCallbackWithStatus packet_receiver) final {}

  void StopReceiving() final {}

  void AddExpectedSizesAndPacketIds(int packet_size,
                                    uint16_t first_packet_id,
                                    int sequence_length) {
    for (int i = 0; i < sequence_length; ++i) {
      expected_packet_sizes_.push_back(packet_size);
      expected_packet_ids_.push_back(first_packet_id++);
    }
  }

  bool expecting_nothing_else() const { return expected_packet_sizes_.empty(); }

 private:
  base::circular_deque<int> expected_packet_sizes_;
  base::circular_deque<uint16_t> expected_packet_ids_;
  int64_t bytes_sent_;
};

class PacedSenderTest : public ::testing::Test {
 public:
  PacedSenderTest(const PacedSenderTest&) = delete;
  PacedSenderTest& operator=(const PacedSenderTest&) = delete;

 protected:
  PacedSenderTest() {
    testing_clock_.Advance(base::Milliseconds(kStartMillisecond));
    task_runner_ = new FakeSingleThreadTaskRunner(&testing_clock_);
    paced_sender_ = std::make_unique<PacedSender>(
        kTargetBurstSize, kMaxBurstSize, &testing_clock_, &packet_events_,
        &mock_transport_, task_runner_);
    paced_sender_->RegisterSsrc(kAudioSsrc, true);
    paced_sender_->RegisterSsrc(kVideoSsrc, false);
  }

  static void UpdateCastTransportStatus(CastTransportStatus status) {
    NOTREACHED();
  }

  SendPacketVector CreateSendPacketVector(size_t packet_size,
                                          int num_of_packets_in_frame,
                                          bool audio) {
    DCHECK_GE(packet_size, 12u);
    SendPacketVector packets;
    base::TimeTicks frame_tick = testing_clock_.NowTicks();
    // Advance the clock so that we don't get the same |frame_tick|
    // next time this function is called.
    testing_clock_.Advance(base::Milliseconds(1));
    for (int i = 0; i < num_of_packets_in_frame; ++i) {
      PacketKey key(frame_tick, audio ? kAudioSsrc : kVideoSsrc,
                    FrameId::first(), i);

      PacketRef packet(new base::RefCountedData<Packet>);
      packet->data.resize(packet_size, kValue);
      // Fill-in packet header fields to test the header parsing (for populating
      // the logging events).
      base::BigEndianWriter writer(reinterpret_cast<char*>(&packet->data[0]),
                                   packet_size);
      bool success = writer.Skip(4);
      success &= writer.WriteU32(audio ? kAudioFrameRtpTimestamp
                                       : kVideoFrameRtpTimestamp);
      success &= writer.WriteU32(audio ? kAudioSsrc : kVideoSsrc);
      success &= writer.Skip(2);
      success &= writer.WriteU16(i);
      success &= writer.WriteU16(num_of_packets_in_frame - 1);
      CHECK(success);
      packets.push_back(std::make_pair(key, packet));
    }
    return packets;
  }

  void SendWithoutBursting(const SendPacketVector& packets) {
    const size_t kBatchSize = 10;
    for (size_t i = 0; i < packets.size(); i += kBatchSize) {
      const SendPacketVector next_batch(
          packets.begin() + i,
          packets.begin() + i + std::min(packets.size() - i, kBatchSize));
      ASSERT_TRUE(paced_sender_->SendPackets(next_batch));
      testing_clock_.Advance(base::Milliseconds(10));
      task_runner_->RunTasks();
    }
  }

  // Use this function to drain the packet list in PacedSender without having
  // to test the pacing implementation details.
  bool RunUntilEmpty(int max_tries) {
    for (int i = 0; i < max_tries; i++) {
      testing_clock_.Advance(base::Milliseconds(10));
      task_runner_->RunTasks();
      if (mock_transport_.expecting_nothing_else())
        return true;
    }

    return mock_transport_.expecting_nothing_else();
  }

  std::vector<PacketEvent> packet_events_;
  base::SimpleTestTickClock testing_clock_;
  TestPacketSender mock_transport_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  std::unique_ptr<PacedSender> paced_sender_;
};

}  // namespace

TEST_F(PacedSenderTest, PassThroughRtcp) {
  SendPacketVector packets = CreateSendPacketVector(kSize1, 1, true);

  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0), 1);
  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0), 1);
  EXPECT_TRUE(paced_sender_->SendPackets(packets));
  EXPECT_TRUE(paced_sender_->ResendPackets(packets, DedupInfo()));

  mock_transport_.AddExpectedSizesAndPacketIds(kSize2, kRtcpPacketIdMagic, 1);
  Packet tmp(kSize2, kValue);
  EXPECT_TRUE(
      paced_sender_->SendRtcpPacket(1, new base::RefCountedData<Packet>(tmp)));
}

TEST_F(PacedSenderTest, BasicPace) {
  int num_of_packets = 27;
  SendPacketVector packets =
      CreateSendPacketVector(kSize1, num_of_packets, false);
  const base::TimeTicks earliest_event_timestamp = testing_clock_.NowTicks();

  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0), 10);
  EXPECT_TRUE(paced_sender_->SendPackets(packets));

  // Check that we get the next burst.
  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(10), 10);

  base::TimeDelta timeout = base::Milliseconds(10);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // If we call process too early make sure we don't send any packets.
  timeout = base::Milliseconds(5);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Check that we get the next burst.
  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(20), 7);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Check that we don't get any more packets.
  EXPECT_TRUE(RunUntilEmpty(3));
  const base::TimeTicks latest_event_timestamp = testing_clock_.NowTicks();

  // Check that packet logging events match expected values.
  EXPECT_EQ(num_of_packets, static_cast<int>(packet_events_.size()));
  uint16_t expected_packet_id = 0;
  for (const PacketEvent& e : packet_events_) {
    ASSERT_LE(earliest_event_timestamp, e.timestamp);
    ASSERT_GE(latest_event_timestamp, e.timestamp);
    ASSERT_EQ(PACKET_SENT_TO_NETWORK, e.type);
    ASSERT_EQ(VIDEO_EVENT, e.media_type);
    ASSERT_EQ(kVideoFrameRtpTimestamp, e.rtp_timestamp.lower_32_bits());
    ASSERT_EQ(num_of_packets - 1, e.max_packet_id);
    ASSERT_EQ(expected_packet_id++, e.packet_id);
    ASSERT_EQ(kSize1, e.size);
  }
}

TEST_F(PacedSenderTest, PaceWithNack) {
  // Testing what happen when we get multiple NACK requests for a fully lost
  // frames just as we sent the first packets in a frame.
  int num_of_packets_in_frame = 12;
  int num_of_packets_in_nack = 12;

  SendPacketVector nack_packets =
      CreateSendPacketVector(kNackSize, num_of_packets_in_nack, false);

  SendPacketVector first_frame_packets =
      CreateSendPacketVector(kSize1, num_of_packets_in_frame, false);

  SendPacketVector second_frame_packets =
      CreateSendPacketVector(kSize2, num_of_packets_in_frame, true);

  // Check that the first burst of the frame go out on the wire.
  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0), 10);
  EXPECT_TRUE(paced_sender_->SendPackets(first_frame_packets));

  // Add first NACK request.
  EXPECT_TRUE(paced_sender_->ResendPackets(nack_packets, DedupInfo()));

  // Check that we get the first NACK burst.
  mock_transport_.AddExpectedSizesAndPacketIds(kNackSize, UINT16_C(0), 10);
  base::TimeDelta timeout = base::Milliseconds(10);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Add second NACK request.
  EXPECT_TRUE(paced_sender_->ResendPackets(nack_packets, DedupInfo()));

  // Check that we get the next NACK burst.
  mock_transport_.AddExpectedSizesAndPacketIds(kNackSize, UINT16_C(10), 2);
  mock_transport_.AddExpectedSizesAndPacketIds(kNackSize, UINT16_C(0), 8);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // End of NACK plus two packets from the oldest frame.
  // Note that two of the NACKs have been de-duped.
  mock_transport_.AddExpectedSizesAndPacketIds(kNackSize, UINT16_C(8), 2);
  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(10), 2);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // Add second frame.
  // Make sure we don't delay the second frame due to the previous packets.
  mock_transport_.AddExpectedSizesAndPacketIds(kSize2, UINT16_C(0), 10);
  EXPECT_TRUE(paced_sender_->SendPackets(second_frame_packets));

  // Last packets of frame 2.
  mock_transport_.AddExpectedSizesAndPacketIds(kSize2, UINT16_C(10), 2);
  testing_clock_.Advance(timeout);
  task_runner_->RunTasks();

  // No more packets.
  EXPECT_TRUE(RunUntilEmpty(5));

  int expected_video_network_event_count = num_of_packets_in_frame;
  int expected_video_retransmitted_event_count = 2 * num_of_packets_in_nack;
  expected_video_retransmitted_event_count -= 2;  // 2 packets deduped
  int expected_audio_network_event_count = num_of_packets_in_frame;
  EXPECT_EQ(expected_video_network_event_count +
                expected_video_retransmitted_event_count +
                expected_audio_network_event_count,
            static_cast<int>(packet_events_.size()));
  int audio_network_event_count = 0;
  int video_network_event_count = 0;
  int video_retransmitted_event_count = 0;
  for (const PacketEvent& e : packet_events_) {
    if (e.type == PACKET_SENT_TO_NETWORK) {
      if (e.media_type == VIDEO_EVENT)
        video_network_event_count++;
      else
        audio_network_event_count++;
    } else if (e.type == PACKET_RETRANSMITTED) {
      if (e.media_type == VIDEO_EVENT)
        video_retransmitted_event_count++;
    } else {
      FAIL() << "Got unexpected event type " << CastLoggingToString(e.type);
    }
  }
  EXPECT_EQ(expected_audio_network_event_count, audio_network_event_count);
  EXPECT_EQ(expected_video_network_event_count, video_network_event_count);
  EXPECT_EQ(expected_video_retransmitted_event_count,
            video_retransmitted_event_count);
}

TEST_F(PacedSenderTest, PaceWith60fps) {
  // Testing what happen when we get multiple NACK requests for a fully lost
  // frames just as we sent the first packets in a frame.
  int num_of_packets_in_frame = 17;

  SendPacketVector first_frame_packets =
      CreateSendPacketVector(kSize1, num_of_packets_in_frame, false);

  SendPacketVector second_frame_packets =
      CreateSendPacketVector(kSize2, num_of_packets_in_frame, false);

  SendPacketVector third_frame_packets =
      CreateSendPacketVector(kSize3, num_of_packets_in_frame, false);

  SendPacketVector fourth_frame_packets =
      CreateSendPacketVector(kSize4, num_of_packets_in_frame, false);

  base::TimeDelta timeout_10ms = base::Milliseconds(10);

  // Check that the first burst of the frame go out on the wire.
  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0), 10);
  EXPECT_TRUE(paced_sender_->SendPackets(first_frame_packets));

  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(10), 7);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  testing_clock_.Advance(base::Milliseconds(6));

  // Add second frame, after 16 ms.
  mock_transport_.AddExpectedSizesAndPacketIds(kSize2, UINT16_C(0), 3);
  EXPECT_TRUE(paced_sender_->SendPackets(second_frame_packets));
  testing_clock_.Advance(base::Milliseconds(4));

  mock_transport_.AddExpectedSizesAndPacketIds(kSize2, UINT16_C(3), 10);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  mock_transport_.AddExpectedSizesAndPacketIds(kSize2, UINT16_C(13), 4);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  testing_clock_.Advance(base::Milliseconds(3));

  // Add third frame, after 33 ms.
  mock_transport_.AddExpectedSizesAndPacketIds(kSize3, UINT16_C(0), 6);
  EXPECT_TRUE(paced_sender_->SendPackets(third_frame_packets));

  mock_transport_.AddExpectedSizesAndPacketIds(kSize3, UINT16_C(6), 10);
  testing_clock_.Advance(base::Milliseconds(7));
  task_runner_->RunTasks();

  // Add fourth frame, after 50 ms.
  EXPECT_TRUE(paced_sender_->SendPackets(fourth_frame_packets));

  mock_transport_.AddExpectedSizesAndPacketIds(kSize3, UINT16_C(16), 1);
  mock_transport_.AddExpectedSizesAndPacketIds(kSize4, UINT16_C(0), 9);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  mock_transport_.AddExpectedSizesAndPacketIds(kSize4, UINT16_C(9), 8);
  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  testing_clock_.Advance(timeout_10ms);
  task_runner_->RunTasks();

  // No more packets.
  EXPECT_TRUE(RunUntilEmpty(5));
}

TEST_F(PacedSenderTest, SendPriority) {
  // Actual order to the network is:
  // 1. Video packets x 10.
  // 2. RTCP packet x 1.
  // 3. Audio packet x 1.
  // 4. Video retransmission packet x 10.
  // 5. Video packet x 10.
  mock_transport_.AddExpectedSizesAndPacketIds(kSize2, UINT16_C(0), 10);
  mock_transport_.AddExpectedSizesAndPacketIds(kSize3, kRtcpPacketIdMagic, 1);
  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0), 1);
  mock_transport_.AddExpectedSizesAndPacketIds(kSize4, UINT16_C(0), 10);
  mock_transport_.AddExpectedSizesAndPacketIds(kSize2, UINT16_C(10), 10);

  paced_sender_->RegisterPrioritySsrc(kAudioSsrc);

  // Retransmission packets with the earlier timestamp.
  SendPacketVector resend_packets = CreateSendPacketVector(kSize4, 10, false);
  testing_clock_.Advance(base::Milliseconds(10));

  // Send 20 normal video packets. Only 10 will be sent in this
  // call, the rest will be sitting in the queue waiting for pacing.
  EXPECT_TRUE(
      paced_sender_->SendPackets(CreateSendPacketVector(kSize2, 20, false)));

  testing_clock_.Advance(base::Milliseconds(10));

  // Send normal audio packet. This is queued and will be sent
  // earlier than video packets.
  EXPECT_TRUE(
      paced_sender_->SendPackets(CreateSendPacketVector(kSize1, 1, true)));

  // Send RTCP packet. This is queued and will be sent first.
  EXPECT_TRUE(paced_sender_->SendRtcpPacket(
      kVideoSsrc, new base::RefCountedData<Packet>(Packet(kSize3, kValue))));

  // Resend video packets. This is queued and will be sent
  // earlier than normal video packets.
  EXPECT_TRUE(paced_sender_->ResendPackets(resend_packets, DedupInfo()));

  // Roll the clock. Queued packets will be sent in this order:
  // 1. RTCP packet x 1.
  // 2. Audio packet x 1.
  // 3. Video retransmission packet x 10.
  // 4. Video packet x 10.
  task_runner_->RunTasks();
  EXPECT_TRUE(RunUntilEmpty(4));
}

TEST_F(PacedSenderTest, GetLastByteSent) {
  SendPacketVector packets1 = CreateSendPacketVector(kSize1, 1, true);
  SendPacketVector packets2 = CreateSendPacketVector(kSize1, 1, false);

  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0), 1);
  EXPECT_TRUE(paced_sender_->SendPackets(packets1));
  EXPECT_EQ(static_cast<int64_t>(kSize1),
            paced_sender_->GetLastByteSentForPacket(packets1[0].first));
  EXPECT_EQ(static_cast<int64_t>(kSize1),
            paced_sender_->GetLastByteSentForSsrc(kAudioSsrc));
  EXPECT_EQ(0, paced_sender_->GetLastByteSentForSsrc(kVideoSsrc));

  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0), 1);
  EXPECT_TRUE(paced_sender_->SendPackets(packets2));
  EXPECT_EQ(static_cast<int64_t>(2 * kSize1),
            paced_sender_->GetLastByteSentForPacket(packets2[0].first));
  EXPECT_EQ(static_cast<int64_t>(kSize1),
            paced_sender_->GetLastByteSentForSsrc(kAudioSsrc));
  EXPECT_EQ(static_cast<int64_t>(2 * kSize1),
            paced_sender_->GetLastByteSentForSsrc(kVideoSsrc));

  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0), 1);
  EXPECT_TRUE(paced_sender_->ResendPackets(packets1, DedupInfo()));
  EXPECT_EQ(static_cast<int64_t>(3 * kSize1),
            paced_sender_->GetLastByteSentForPacket(packets1[0].first));
  EXPECT_EQ(static_cast<int64_t>(3 * kSize1),
            paced_sender_->GetLastByteSentForSsrc(kAudioSsrc));
  EXPECT_EQ(static_cast<int64_t>(2 * kSize1),
            paced_sender_->GetLastByteSentForSsrc(kVideoSsrc));

  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0), 1);
  EXPECT_TRUE(paced_sender_->ResendPackets(packets2, DedupInfo()));
  EXPECT_EQ(static_cast<int64_t>(4 * kSize1),
            paced_sender_->GetLastByteSentForPacket(packets2[0].first));
  EXPECT_EQ(static_cast<int64_t>(3 * kSize1),
            paced_sender_->GetLastByteSentForSsrc(kAudioSsrc));
  EXPECT_EQ(static_cast<int64_t>(4 * kSize1),
            paced_sender_->GetLastByteSentForSsrc(kVideoSsrc));
}

TEST_F(PacedSenderTest, DedupWithResendInterval) {
  SendPacketVector packets = CreateSendPacketVector(kSize1, 1, true);
  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0), 1);
  EXPECT_TRUE(paced_sender_->SendPackets(packets));
  testing_clock_.Advance(base::Milliseconds(10));

  DedupInfo dedup_info;
  dedup_info.resend_interval = base::Milliseconds(20);

  // This packet will not be sent.
  EXPECT_TRUE(paced_sender_->ResendPackets(packets, dedup_info));
  EXPECT_EQ(static_cast<int64_t>(kSize1), mock_transport_.GetBytesSent());

  dedup_info.resend_interval = base::Milliseconds(5);
  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0), 1);
  EXPECT_TRUE(paced_sender_->ResendPackets(packets, dedup_info));
  EXPECT_EQ(static_cast<int64_t>(2 * kSize1), mock_transport_.GetBytesSent());
}

TEST_F(PacedSenderTest, AllPacketsInSameFrameAreResentFairly) {
  const int kNumPackets = 400;
  SendPacketVector packets = CreateSendPacketVector(kSize1, kNumPackets, false);

  // Send a large frame (400 packets, yeah!).  Confirm that the paced sender
  // sends each packet in the frame exactly once.
  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0),
                                               kNumPackets);
  SendWithoutBursting(packets);
  ASSERT_TRUE(mock_transport_.expecting_nothing_else());

  // Resend packets 2 and 3.  Confirm that the paced sender sends them.  Then,
  // resend all of the first 10 packets.  The paced sender should send packets
  // 0, 1, and 4 through 9 first, and then 2 and 3.
  SendPacketVector couple_of_packets;
  couple_of_packets.push_back(packets[2]);
  couple_of_packets.push_back(packets[3]);

  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(2), 2);
  SendWithoutBursting(couple_of_packets);
  ASSERT_TRUE(mock_transport_.expecting_nothing_else());

  SendPacketVector first_ten_packets;
  for (size_t i = 0; i < 10; ++i)
    first_ten_packets.push_back(packets[i]);
  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(0), 2);
  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(4), 6);
  mock_transport_.AddExpectedSizesAndPacketIds(kSize1, UINT16_C(2), 2);
  SendWithoutBursting(first_ten_packets);
  ASSERT_TRUE(mock_transport_.expecting_nothing_else());
}

}  // namespace cast
}  // namespace media
