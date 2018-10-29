// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_received_packet_manager.h"

#include <algorithm>
#include <ostream>
#include <vector>

#include "net/third_party/quic/core/quic_connection_stats.h"
#include "net/third_party/quic/platform/api/quic_test.h"

namespace quic {
namespace test {
namespace {

struct TestParams {
  explicit TestParams(QuicTransportVersion version) : version(version) {}

  friend std::ostream& operator<<(std::ostream& os, const TestParams& p) {
    os << "{ version: " << QuicVersionToString(p.version) << " }";
    return os;
  }

  QuicTransportVersion version;
};

std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  QuicTransportVersionVector all_supported_versions =
      AllSupportedTransportVersions();
  for (size_t i = 0; i < all_supported_versions.size(); ++i) {
    params.push_back(TestParams(all_supported_versions[i]));
  }
  return params;
}

class QuicReceivedPacketManagerTest : public QuicTestWithParam<TestParams> {
 protected:
  QuicReceivedPacketManagerTest() : received_manager_(&stats_) {
    received_manager_.set_save_timestamps(true);
  }

  void RecordPacketReceipt(QuicPacketNumber packet_number) {
    RecordPacketReceipt(packet_number, QuicTime::Zero());
  }

  void RecordPacketReceipt(QuicPacketNumber packet_number,
                           QuicTime receipt_time) {
    QuicPacketHeader header;
    header.packet_number = packet_number;
    received_manager_.RecordPacketReceived(header, receipt_time);
  }

  QuicConnectionStats stats_;
  QuicReceivedPacketManager received_manager_;
};

INSTANTIATE_TEST_CASE_P(QuicReceivedPacketManagerTest,
                        QuicReceivedPacketManagerTest,
                        ::testing::ValuesIn(GetTestParams()));

TEST_P(QuicReceivedPacketManagerTest, DontWaitForPacketsBefore) {
  QuicPacketHeader header;
  header.packet_number = 2u;
  received_manager_.RecordPacketReceived(header, QuicTime::Zero());
  header.packet_number = 7u;
  received_manager_.RecordPacketReceived(header, QuicTime::Zero());
  EXPECT_TRUE(received_manager_.IsAwaitingPacket(3u));
  EXPECT_TRUE(received_manager_.IsAwaitingPacket(6u));
  received_manager_.DontWaitForPacketsBefore(4);
  EXPECT_FALSE(received_manager_.IsAwaitingPacket(3u));
  EXPECT_TRUE(received_manager_.IsAwaitingPacket(6u));
}

TEST_P(QuicReceivedPacketManagerTest, GetUpdatedAckFrame) {
  QuicPacketHeader header;
  header.packet_number = 2u;
  QuicTime two_ms = QuicTime::Zero() + QuicTime::Delta::FromMilliseconds(2);
  EXPECT_FALSE(received_manager_.ack_frame_updated());
  received_manager_.RecordPacketReceived(header, two_ms);
  EXPECT_TRUE(received_manager_.ack_frame_updated());

  QuicFrame ack = received_manager_.GetUpdatedAckFrame(QuicTime::Zero());
  EXPECT_FALSE(received_manager_.ack_frame_updated());
  // When UpdateReceivedPacketInfo with a time earlier than the time of the
  // largest observed packet, make sure that the delta is 0, not negative.
  EXPECT_EQ(QuicTime::Delta::Zero(), ack.ack_frame->ack_delay_time);
  EXPECT_EQ(1u, ack.ack_frame->received_packet_times.size());

  QuicTime four_ms = QuicTime::Zero() + QuicTime::Delta::FromMilliseconds(4);
  ack = received_manager_.GetUpdatedAckFrame(four_ms);
  EXPECT_FALSE(received_manager_.ack_frame_updated());
  // When UpdateReceivedPacketInfo after not having received a new packet,
  // the delta should still be accurate.
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(2),
            ack.ack_frame->ack_delay_time);
  // And received packet times won't have change.
  EXPECT_EQ(1u, ack.ack_frame->received_packet_times.size());

  header.packet_number = 999u;
  received_manager_.RecordPacketReceived(header, two_ms);
  header.packet_number = 4u;
  received_manager_.RecordPacketReceived(header, two_ms);
  header.packet_number = 1000u;
  received_manager_.RecordPacketReceived(header, two_ms);
  EXPECT_TRUE(received_manager_.ack_frame_updated());
  ack = received_manager_.GetUpdatedAckFrame(two_ms);
  EXPECT_FALSE(received_manager_.ack_frame_updated());
  // UpdateReceivedPacketInfo should discard any times which can't be
  // expressed on the wire.
  EXPECT_EQ(2u, ack.ack_frame->received_packet_times.size());
}

TEST_P(QuicReceivedPacketManagerTest, UpdateReceivedConnectionStats) {
  EXPECT_FALSE(received_manager_.ack_frame_updated());
  RecordPacketReceipt(1);
  EXPECT_TRUE(received_manager_.ack_frame_updated());
  RecordPacketReceipt(6);
  RecordPacketReceipt(2,
                      QuicTime::Zero() + QuicTime::Delta::FromMilliseconds(1));

  EXPECT_EQ(4u, stats_.max_sequence_reordering);
  EXPECT_EQ(1000, stats_.max_time_reordering_us);
  EXPECT_EQ(1u, stats_.packets_reordered);
}

TEST_P(QuicReceivedPacketManagerTest, LimitAckRanges) {
  received_manager_.set_max_ack_ranges(10);
  EXPECT_FALSE(received_manager_.ack_frame_updated());
  for (int i = 0; i < 100; ++i) {
    RecordPacketReceipt(1 + 2 * i);
    EXPECT_TRUE(received_manager_.ack_frame_updated());
    received_manager_.GetUpdatedAckFrame(QuicTime::Zero());
    EXPECT_GE(10u, received_manager_.ack_frame().packets.NumIntervals());
    EXPECT_EQ(1u + 2 * i, received_manager_.ack_frame().packets.Max());
    for (int j = 0; j < std::min(10, i + 1); ++j) {
      EXPECT_TRUE(
          received_manager_.ack_frame().packets.Contains(1 + (i - j) * 2));
      EXPECT_FALSE(received_manager_.ack_frame().packets.Contains((i - j) * 2));
    }
  }
}

TEST_P(QuicReceivedPacketManagerTest, IgnoreOutOfOrderTimestamps) {
  EXPECT_FALSE(received_manager_.ack_frame_updated());
  RecordPacketReceipt(1, QuicTime::Zero());
  EXPECT_TRUE(received_manager_.ack_frame_updated());
  EXPECT_EQ(1u, received_manager_.ack_frame().received_packet_times.size());
  RecordPacketReceipt(2,
                      QuicTime::Zero() + QuicTime::Delta::FromMilliseconds(1));
  EXPECT_EQ(2u, received_manager_.ack_frame().received_packet_times.size());
  RecordPacketReceipt(3, QuicTime::Zero());
  EXPECT_EQ(2u, received_manager_.ack_frame().received_packet_times.size());
}

}  // namespace
}  // namespace test
}  // namespace quic
