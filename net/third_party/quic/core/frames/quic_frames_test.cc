// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_ack_frame.h"
#include "net/third_party/quic/core/frames/quic_blocked_frame.h"
#include "net/third_party/quic/core/frames/quic_connection_close_frame.h"
#include "net/third_party/quic/core/frames/quic_frame.h"
#include "net/third_party/quic/core/frames/quic_goaway_frame.h"
#include "net/third_party/quic/core/frames/quic_mtu_discovery_frame.h"
#include "net/third_party/quic/core/frames/quic_padding_frame.h"
#include "net/third_party/quic/core/frames/quic_ping_frame.h"
#include "net/third_party/quic/core/frames/quic_rst_stream_frame.h"
#include "net/third_party/quic/core/frames/quic_stop_waiting_frame.h"
#include "net/third_party/quic/core/frames/quic_stream_frame.h"
#include "net/third_party/quic/core/frames/quic_window_update_frame.h"
#include "net/third_party/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quic/platform/api/quic_interval.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"

namespace quic {
namespace test {
namespace {

class QuicFramesTest : public QuicTest {};

TEST_F(QuicFramesTest, AckFrameToString) {
  QuicAckFrame frame;
  frame.largest_acked = 5;
  frame.ack_delay_time = QuicTime::Delta::FromMicroseconds(3);
  frame.packets.Add(4);
  frame.packets.Add(5);
  frame.received_packet_times = {
      {6, QuicTime::Zero() + QuicTime::Delta::FromMicroseconds(7)}};
  std::ostringstream stream;
  stream << frame;
  EXPECT_EQ(
      "{ largest_acked: 5, ack_delay_time: 3, packets: [ 4 5  ], "
      "received_packets: [ 6 at 7  ], ecn_counters_populated: 0 }\n",
      stream.str());
}

TEST_F(QuicFramesTest, BigAckFrameToString) {
  QuicAckFrame frame;
  frame.largest_acked = 500;
  frame.ack_delay_time = QuicTime::Delta::FromMicroseconds(3);
  frame.packets.AddRange(4, 501);
  frame.received_packet_times = {
      {500, QuicTime::Zero() + QuicTime::Delta::FromMicroseconds(7)}};
  std::ostringstream stream;
  stream << frame;
  EXPECT_EQ(
      "{ largest_acked: 500, ack_delay_time: 3, packets: [ 4...500  ], "
      "received_packets: [ 500 at 7  ], ecn_counters_populated: 0 }\n",
      stream.str());
}

TEST_F(QuicFramesTest, PaddingFrameToString) {
  QuicPaddingFrame frame;
  frame.num_padding_bytes = 1;
  std::ostringstream stream;
  stream << frame;
  EXPECT_EQ("{ num_padding_bytes: 1 }\n", stream.str());
}

TEST_F(QuicFramesTest, RstStreamFrameToString) {
  QuicRstStreamFrame rst_stream;
  QuicFrame frame(&rst_stream);
  SetControlFrameId(1, &frame);
  EXPECT_EQ(1u, GetControlFrameId(frame));
  rst_stream.stream_id = 1;
  rst_stream.error_code = QUIC_STREAM_CANCELLED;
  std::ostringstream stream;
  stream << rst_stream;
  EXPECT_EQ("{ control_frame_id: 1, stream_id: 1, error_code: 6 }\n",
            stream.str());
}

TEST_F(QuicFramesTest, ConnectionCloseFrameToString) {
  QuicConnectionCloseFrame frame;
  frame.error_code = QUIC_NETWORK_IDLE_TIMEOUT;
  frame.error_details = "No recent network activity.";
  std::ostringstream stream;
  stream << frame;
  EXPECT_EQ(
      "{ error_code: 25, error_details: 'No recent network activity.', "
      "frame_type: 0"
      "}\n",
      stream.str());
}

TEST_F(QuicFramesTest, GoAwayFrameToString) {
  QuicGoAwayFrame goaway_frame;
  QuicFrame frame(&goaway_frame);
  SetControlFrameId(2, &frame);
  EXPECT_EQ(2u, GetControlFrameId(frame));
  goaway_frame.error_code = QUIC_NETWORK_IDLE_TIMEOUT;
  goaway_frame.last_good_stream_id = 2;
  goaway_frame.reason_phrase = "Reason";
  std::ostringstream stream;
  stream << goaway_frame;
  EXPECT_EQ(
      "{ control_frame_id: 2, error_code: 25, last_good_stream_id: 2, "
      "reason_phrase: "
      "'Reason' }\n",
      stream.str());
}

TEST_F(QuicFramesTest, WindowUpdateFrameToString) {
  QuicWindowUpdateFrame window_update;
  QuicFrame frame(&window_update);
  SetControlFrameId(3, &frame);
  EXPECT_EQ(3u, GetControlFrameId(frame));
  std::ostringstream stream;
  window_update.stream_id = 1;
  window_update.byte_offset = 2;
  stream << window_update;
  EXPECT_EQ("{ control_frame_id: 3, stream_id: 1, byte_offset: 2 }\n",
            stream.str());
}

TEST_F(QuicFramesTest, BlockedFrameToString) {
  QuicBlockedFrame blocked;
  QuicFrame frame(&blocked);
  SetControlFrameId(4, &frame);
  EXPECT_EQ(4u, GetControlFrameId(frame));
  blocked.stream_id = 1;
  std::ostringstream stream;
  stream << blocked;
  EXPECT_EQ("{ control_frame_id: 4, stream_id: 1 }\n", stream.str());
}

TEST_F(QuicFramesTest, PingFrameToString) {
  QuicPingFrame ping;
  QuicFrame frame(ping);
  SetControlFrameId(5, &frame);
  EXPECT_EQ(5u, GetControlFrameId(frame));
  std::ostringstream stream;
  stream << frame.ping_frame;
  EXPECT_EQ("{ control_frame_id: 5 }\n", stream.str());
}

TEST_F(QuicFramesTest, StreamFrameToString) {
  QuicStreamFrame frame;
  frame.stream_id = 1;
  frame.fin = false;
  frame.offset = 2;
  frame.data_length = 3;
  std::ostringstream stream;
  stream << frame;
  EXPECT_EQ("{ stream_id: 1, fin: 0, offset: 2, length: 3 }\n", stream.str());
}

TEST_F(QuicFramesTest, StopWaitingFrameToString) {
  QuicStopWaitingFrame frame;
  frame.least_unacked = 2;
  std::ostringstream stream;
  stream << frame;
  EXPECT_EQ("{ least_unacked: 2 }\n", stream.str());
}

TEST_F(QuicFramesTest, IsAwaitingPacket) {
  QuicAckFrame ack_frame1;
  ack_frame1.largest_acked = 10u;
  ack_frame1.packets.AddRange(1, 11);
  EXPECT_TRUE(IsAwaitingPacket(ack_frame1, 11u, 0u));
  EXPECT_FALSE(IsAwaitingPacket(ack_frame1, 1u, 0u));

  ack_frame1.packets.Add(12);
  EXPECT_TRUE(IsAwaitingPacket(ack_frame1, 11u, 0u));

  QuicAckFrame ack_frame2;
  ack_frame2.largest_acked = 100u;
  ack_frame2.packets.AddRange(21, 100);
  EXPECT_FALSE(IsAwaitingPacket(ack_frame2, 11u, 20u));
  EXPECT_FALSE(IsAwaitingPacket(ack_frame2, 80u, 20u));
  EXPECT_TRUE(IsAwaitingPacket(ack_frame2, 101u, 20u));

  ack_frame2.packets.AddRange(102, 200);
  EXPECT_TRUE(IsAwaitingPacket(ack_frame2, 101u, 20u));
}

TEST_F(QuicFramesTest, AddPacket) {
  QuicAckFrame ack_frame1;
  ack_frame1.packets.Add(1);
  ack_frame1.packets.Add(99);

  EXPECT_EQ(2u, ack_frame1.packets.NumIntervals());
  EXPECT_EQ(1u, ack_frame1.packets.Min());
  EXPECT_EQ(99u, ack_frame1.packets.Max());

  std::vector<QuicInterval<QuicPacketNumber>> expected_intervals;
  expected_intervals.emplace_back(QuicInterval<QuicPacketNumber>(1, 2));
  expected_intervals.emplace_back(QuicInterval<QuicPacketNumber>(99, 100));

  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals(
      ack_frame1.packets.begin(), ack_frame1.packets.end());

  EXPECT_EQ(expected_intervals, actual_intervals);

  ack_frame1.packets.Add(20);
  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals2(
      ack_frame1.packets.begin(), ack_frame1.packets.end());

  std::vector<QuicInterval<QuicPacketNumber>> expected_intervals2;
  expected_intervals2.emplace_back(QuicInterval<QuicPacketNumber>(1, 2));
  expected_intervals2.emplace_back(QuicInterval<QuicPacketNumber>(20, 21));
  expected_intervals2.emplace_back(QuicInterval<QuicPacketNumber>(99, 100));

  EXPECT_EQ(3u, ack_frame1.packets.NumIntervals());
  EXPECT_EQ(expected_intervals2, actual_intervals2);

  ack_frame1.packets.Add(19);
  ack_frame1.packets.Add(21);

  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals3(
      ack_frame1.packets.begin(), ack_frame1.packets.end());

  std::vector<QuicInterval<QuicPacketNumber>> expected_intervals3;
  expected_intervals3.emplace_back(QuicInterval<QuicPacketNumber>(1, 2));
  expected_intervals3.emplace_back(QuicInterval<QuicPacketNumber>(19, 22));
  expected_intervals3.emplace_back(QuicInterval<QuicPacketNumber>(99, 100));

  EXPECT_EQ(expected_intervals3, actual_intervals3);

  ack_frame1.packets.Add(20);

  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals4(
      ack_frame1.packets.begin(), ack_frame1.packets.end());

  EXPECT_EQ(expected_intervals3, actual_intervals4);

  QuicAckFrame ack_frame2;
  ack_frame2.packets.Add(20);
  ack_frame2.packets.Add(40);
  ack_frame2.packets.Add(60);
  ack_frame2.packets.Add(10);
  ack_frame2.packets.Add(80);

  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals5(
      ack_frame2.packets.begin(), ack_frame2.packets.end());

  std::vector<QuicInterval<QuicPacketNumber>> expected_intervals5;
  expected_intervals5.emplace_back(QuicInterval<QuicPacketNumber>(10, 11));
  expected_intervals5.emplace_back(QuicInterval<QuicPacketNumber>(20, 21));
  expected_intervals5.emplace_back(QuicInterval<QuicPacketNumber>(40, 41));
  expected_intervals5.emplace_back(QuicInterval<QuicPacketNumber>(60, 61));
  expected_intervals5.emplace_back(QuicInterval<QuicPacketNumber>(80, 81));

  EXPECT_EQ(expected_intervals5, actual_intervals5);
}

TEST_F(QuicFramesTest, AddInterval) {
  QuicAckFrame ack_frame1;
  ack_frame1.packets.AddRange(1, 10);
  ack_frame1.packets.AddRange(50, 100);

  EXPECT_EQ(2u, ack_frame1.packets.NumIntervals());
  EXPECT_EQ(1u, ack_frame1.packets.Min());
  EXPECT_EQ(99u, ack_frame1.packets.Max());

  std::vector<QuicInterval<QuicPacketNumber>> expected_intervals;
  expected_intervals.emplace_back(QuicInterval<QuicPacketNumber>(1, 10));
  expected_intervals.emplace_back(QuicInterval<QuicPacketNumber>(50, 100));

  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals(
      ack_frame1.packets.begin(), ack_frame1.packets.end());

  EXPECT_EQ(expected_intervals, actual_intervals);

  // Ensure adding a range within the existing ranges fails.
  EXPECT_QUIC_BUG(ack_frame1.packets.AddRange(20, 30), "");

  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals2(
      ack_frame1.packets.begin(), ack_frame1.packets.end());

  std::vector<QuicInterval<QuicPacketNumber>> expected_intervals2;
  expected_intervals2.emplace_back(QuicInterval<QuicPacketNumber>(1, 10));
  expected_intervals2.emplace_back(QuicInterval<QuicPacketNumber>(50, 100));

  EXPECT_EQ(expected_intervals2.size(), ack_frame1.packets.NumIntervals());
  EXPECT_EQ(expected_intervals2, actual_intervals2);

  // Add ranges at both ends.
  QuicAckFrame ack_frame2;
  ack_frame2.packets.AddRange(20, 25);
  ack_frame2.packets.AddRange(40, 45);
  ack_frame2.packets.AddRange(60, 65);
  ack_frame2.packets.AddRange(10, 15);
  ack_frame2.packets.AddRange(80, 85);

  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals8(
      ack_frame2.packets.begin(), ack_frame2.packets.end());

  std::vector<QuicInterval<QuicPacketNumber>> expected_intervals8;
  expected_intervals8.emplace_back(QuicInterval<QuicPacketNumber>(10, 15));
  expected_intervals8.emplace_back(QuicInterval<QuicPacketNumber>(20, 25));
  expected_intervals8.emplace_back(QuicInterval<QuicPacketNumber>(40, 45));
  expected_intervals8.emplace_back(QuicInterval<QuicPacketNumber>(60, 65));
  expected_intervals8.emplace_back(QuicInterval<QuicPacketNumber>(80, 85));

  EXPECT_EQ(expected_intervals8, actual_intervals8);
}

TEST_F(QuicFramesTest, AddAdjacentForward) {
  QuicAckFrame ack_frame1;
  ack_frame1.packets.Add(49);
  ack_frame1.packets.AddRange(50, 60);
  ack_frame1.packets.AddRange(60, 70);
  ack_frame1.packets.AddRange(70, 100);

  std::vector<QuicInterval<QuicPacketNumber>> expected_intervals;
  expected_intervals.emplace_back(QuicInterval<QuicPacketNumber>(49, 100));

  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals(
      ack_frame1.packets.begin(), ack_frame1.packets.end());

  EXPECT_EQ(expected_intervals, actual_intervals);
}

TEST_F(QuicFramesTest, AddAdjacentReverse) {
  QuicAckFrame ack_frame1;
  ack_frame1.packets.AddRange(70, 100);
  ack_frame1.packets.AddRange(60, 70);
  ack_frame1.packets.AddRange(50, 60);
  ack_frame1.packets.Add(49);

  std::vector<QuicInterval<QuicPacketNumber>> expected_intervals;
  expected_intervals.emplace_back(QuicInterval<QuicPacketNumber>(49, 100));

  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals(
      ack_frame1.packets.begin(), ack_frame1.packets.end());

  EXPECT_EQ(expected_intervals, actual_intervals);
}

TEST_F(QuicFramesTest, RemoveSmallestInterval) {
  QuicAckFrame ack_frame1;
  ack_frame1.largest_acked = 100u;
  ack_frame1.packets.AddRange(51, 60);
  ack_frame1.packets.AddRange(71, 80);
  ack_frame1.packets.AddRange(91, 100);
  ack_frame1.packets.RemoveSmallestInterval();
  EXPECT_EQ(2u, ack_frame1.packets.NumIntervals());
  EXPECT_EQ(71u, ack_frame1.packets.Min());
  EXPECT_EQ(99u, ack_frame1.packets.Max());

  ack_frame1.packets.RemoveSmallestInterval();
  EXPECT_EQ(1u, ack_frame1.packets.NumIntervals());
  EXPECT_EQ(91u, ack_frame1.packets.Min());
  EXPECT_EQ(99u, ack_frame1.packets.Max());
}

class PacketNumberQueueTest : public QuicTest {};

// Tests that a queue contains the expected data after calls to Add().
TEST_F(PacketNumberQueueTest, AddRange) {
  PacketNumberQueue queue;
  queue.AddRange(1, 51);
  queue.Add(53);

  EXPECT_FALSE(queue.Contains(0));
  for (int i = 1; i < 51; ++i) {
    EXPECT_TRUE(queue.Contains(i));
  }
  EXPECT_FALSE(queue.Contains(51));
  EXPECT_FALSE(queue.Contains(52));
  EXPECT_TRUE(queue.Contains(53));
  EXPECT_FALSE(queue.Contains(54));
  EXPECT_EQ(51u, queue.NumPacketsSlow());
  EXPECT_EQ(1u, queue.Min());
  EXPECT_EQ(53u, queue.Max());

  queue.Add(70);
  EXPECT_EQ(70u, queue.Max());
}

// Tests Contains function
TEST_F(PacketNumberQueueTest, Contains) {
  PacketNumberQueue queue;
  EXPECT_FALSE(queue.Contains(0));
  queue.AddRange(5, 10);
  queue.Add(20);

  for (int i = 1; i < 5; ++i) {
    EXPECT_FALSE(queue.Contains(i));
  }

  for (int i = 5; i < 10; ++i) {
    EXPECT_TRUE(queue.Contains(i));
  }
  for (int i = 10; i < 20; ++i) {
    EXPECT_FALSE(queue.Contains(i));
  }
  EXPECT_TRUE(queue.Contains(20));
  EXPECT_FALSE(queue.Contains(21));

  PacketNumberQueue queue2;
  EXPECT_FALSE(queue2.Contains(1));
  for (int i = 1; i < 51; ++i) {
    queue2.Add(2 * i);
  }
  EXPECT_FALSE(queue2.Contains(0));
  for (int i = 1; i < 51; ++i) {
    if (i % 2 == 0) {
      EXPECT_TRUE(queue2.Contains(i));
    } else {
      EXPECT_FALSE(queue2.Contains(i));
    }
  }
  EXPECT_FALSE(queue2.Contains(101));
}

// Tests that a queue contains the expected data after calls to RemoveUpTo().
TEST_F(PacketNumberQueueTest, Removal) {
  PacketNumberQueue queue;
  EXPECT_FALSE(queue.Contains(51));
  queue.AddRange(0, 100);

  EXPECT_TRUE(queue.RemoveUpTo(51));
  EXPECT_FALSE(queue.RemoveUpTo(51));

  EXPECT_FALSE(queue.Contains(0));
  for (int i = 1; i < 51; ++i) {
    EXPECT_FALSE(queue.Contains(i));
  }
  for (int i = 51; i < 100; ++i) {
    EXPECT_TRUE(queue.Contains(i));
  }
  EXPECT_EQ(49u, queue.NumPacketsSlow());
  EXPECT_EQ(51u, queue.Min());
  EXPECT_EQ(99u, queue.Max());

  PacketNumberQueue queue2;
  queue2.AddRange(0, 5);
  EXPECT_TRUE(queue2.RemoveUpTo(3));
  EXPECT_TRUE(queue2.RemoveUpTo(50));
  EXPECT_TRUE(queue2.Empty());
}

// Tests that a queue is empty when all of its elements are removed.
TEST_F(PacketNumberQueueTest, Empty) {
  PacketNumberQueue queue;
  EXPECT_TRUE(queue.Empty());
  EXPECT_EQ(0u, queue.NumPacketsSlow());

  queue.AddRange(1, 100);
  EXPECT_TRUE(queue.RemoveUpTo(100));
  EXPECT_TRUE(queue.Empty());
  EXPECT_EQ(0u, queue.NumPacketsSlow());
}

// Tests that logging the state of a PacketNumberQueue does not crash.
TEST_F(PacketNumberQueueTest, LogDoesNotCrash) {
  std::ostringstream oss;
  PacketNumberQueue queue;
  oss << queue;

  queue.Add(1);
  queue.AddRange(50, 100);
  oss << queue;
}

// Tests that the iterators returned from a packet queue iterate over the queue.
TEST_F(PacketNumberQueueTest, Iterators) {
  PacketNumberQueue queue;
  queue.AddRange(1, 100);

  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals(
      queue.begin(), queue.end());

  PacketNumberQueue queue2;
  for (int i = 1; i < 100; i++) {
    queue2.AddRange(i, i + 1);
  }

  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals2(
      queue2.begin(), queue2.end());

  std::vector<QuicInterval<QuicPacketNumber>> expected_intervals;
  expected_intervals.emplace_back(QuicInterval<QuicPacketNumber>(1, 100));
  EXPECT_EQ(expected_intervals, actual_intervals);
  EXPECT_EQ(expected_intervals, actual_intervals2);
  EXPECT_EQ(actual_intervals, actual_intervals2);
}

TEST_F(PacketNumberQueueTest, ReversedIterators) {
  PacketNumberQueue queue;
  queue.AddRange(1, 100);
  PacketNumberQueue queue2;
  for (int i = 1; i < 100; i++) {
    queue2.AddRange(i, i + 1);
  }
  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals(
      queue.rbegin(), queue.rend());
  const std::vector<QuicInterval<QuicPacketNumber>> actual_intervals2(
      queue2.rbegin(), queue2.rend());

  std::vector<QuicInterval<QuicPacketNumber>> expected_intervals;
  expected_intervals.emplace_back(QuicInterval<QuicPacketNumber>(1, 100));

  EXPECT_EQ(expected_intervals, actual_intervals);
  EXPECT_EQ(expected_intervals, actual_intervals2);
  EXPECT_EQ(actual_intervals, actual_intervals2);

  PacketNumberQueue queue3;
  for (int i = 1; i < 20; i++) {
    queue3.Add(2 * i);
  }

  auto begin = queue3.begin();
  auto end = queue3.end();
  --end;
  auto rbegin = queue3.rbegin();
  auto rend = queue3.rend();
  --rend;

  EXPECT_EQ(*begin, *rend);
  EXPECT_EQ(*rbegin, *end);
}

TEST_F(PacketNumberQueueTest, IntervalLengthAndRemoveInterval) {
  PacketNumberQueue queue;
  queue.AddRange(1, 10);
  queue.AddRange(20, 30);
  queue.AddRange(40, 50);
  EXPECT_EQ(3u, queue.NumIntervals());
  EXPECT_EQ(10u, queue.LastIntervalLength());

  EXPECT_TRUE(queue.RemoveUpTo(25));
  EXPECT_EQ(2u, queue.NumIntervals());
  EXPECT_EQ(10u, queue.LastIntervalLength());
  EXPECT_EQ(25u, queue.Min());
  EXPECT_EQ(49u, queue.Max());
}

}  // namespace
}  // namespace test
}  // namespace quic
