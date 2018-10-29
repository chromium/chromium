// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/tools/quic_tcp_like_trace_converter.h"

#include "net/third_party/quic/platform/api/quic_test.h"

namespace quic {
namespace test {
namespace {

TEST(QuicTcpLikeTraceConverterTest, BasicTest) {
  QuicTcpLikeTraceConverter converter;

  EXPECT_EQ(QuicIntervalSet<uint64_t>(0, 100),
            converter.OnStreamFrameSent(1, 0, 100, false));
  EXPECT_EQ(QuicIntervalSet<uint64_t>(100, 200),
            converter.OnStreamFrameSent(3, 0, 100, false));
  EXPECT_EQ(QuicIntervalSet<uint64_t>(200, 300),
            converter.OnStreamFrameSent(3, 100, 100, false));
  EXPECT_EQ(QuicInterval<uint64_t>(300, 450),
            converter.OnControlFrameSent(2, 150));
  EXPECT_EQ(QuicIntervalSet<uint64_t>(450, 550),
            converter.OnStreamFrameSent(1, 100, 100, false));
  EXPECT_EQ(QuicInterval<uint64_t>(550, 650),
            converter.OnControlFrameSent(3, 100));
  EXPECT_EQ(QuicIntervalSet<uint64_t>(650, 850),
            converter.OnStreamFrameSent(3, 200, 200, false));
  EXPECT_EQ(QuicInterval<uint64_t>(850, 1050),
            converter.OnControlFrameSent(4, 200));
  EXPECT_EQ(QuicIntervalSet<uint64_t>(1050, 1100),
            converter.OnStreamFrameSent(1, 200, 50, false));
  EXPECT_EQ(QuicIntervalSet<uint64_t>(1100, 1150),
            converter.OnStreamFrameSent(1, 250, 50, false));
  EXPECT_EQ(QuicIntervalSet<uint64_t>(1150, 1350),
            converter.OnStreamFrameSent(3, 400, 200, false));

  // Stream 1 retransmits [50, 300) and sends new data [300, 350) in the same
  // frame.
  QuicIntervalSet<uint64_t> expected;
  expected.Add(50, 100);
  expected.Add(450, 550);
  expected.Add(1050, 1150);
  expected.Add(1350, 1401);
  EXPECT_EQ(expected, converter.OnStreamFrameSent(1, 50, 300, true));

  expected.Clear();
  // Stream 3 retransmits [150, 500).
  expected.Add(250, 300);
  expected.Add(650, 850);
  expected.Add(1150, 1250);
  EXPECT_EQ(expected, converter.OnStreamFrameSent(3, 150, 350, false));

  // Stream 3 retransmits [300, 600) and sends new data [600, 800) in the same
  // frame.
  expected.Clear();
  expected.Add(750, 850);
  expected.Add(1150, 1350);
  expected.Add(1401, 1602);
  EXPECT_EQ(expected, converter.OnStreamFrameSent(3, 300, 500, true));

  // Stream 3 retransmits fin only frame.
  expected.Clear();
  expected.Add(1601, 1602);
  EXPECT_EQ(expected, converter.OnStreamFrameSent(3, 800, 0, true));

  QuicInterval<uint64_t> expected2;
  // Ignore out of order control frames.
  EXPECT_EQ(expected2, converter.OnControlFrameSent(1, 100));

  // Ignore passed in length for retransmitted frame.
  expected2 = {450, 600};
  EXPECT_EQ(expected2, converter.OnControlFrameSent(2, 200));

  expected2 = {1602, 1702};
  EXPECT_EQ(expected2, converter.OnControlFrameSent(10, 100));
}

TEST(QuicTcpLikeTraceConverterTest, FuzzerTest) {
  QuicTcpLikeTraceConverter converter;
  // Stream does not start from offset 0.
  EXPECT_EQ(QuicIntervalSet<uint64_t>(0, 100),
            converter.OnStreamFrameSent(1, 100, 100, false));
  EXPECT_EQ(QuicIntervalSet<uint64_t>(100, 300),
            converter.OnStreamFrameSent(3, 200, 200, false));
  // Stream does not send data contiguously.
  EXPECT_EQ(QuicIntervalSet<uint64_t>(300, 400),
            converter.OnStreamFrameSent(1, 300, 100, false));

  // Stream fills existing holes.
  QuicIntervalSet<uint64_t> expected;
  expected.Add(0, 100);
  expected.Add(300, 501);
  EXPECT_EQ(expected, converter.OnStreamFrameSent(1, 0, 500, true));

  // Stream sends frame after fin.
  EXPECT_EQ(expected, converter.OnStreamFrameSent(1, 50, 600, false));
}

}  // namespace
}  // namespace test
}  // namespace quic
