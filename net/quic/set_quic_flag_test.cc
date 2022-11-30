// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/set_quic_flag.h"

#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_flags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {

TEST(SetQuicFlagTest, Bool) {
  FLAGS_quic_enforce_single_packet_chlo = true;
  SetQuicFlagByName("FLAGS_quic_enforce_single_packet_chlo", "false");
  EXPECT_FALSE(FLAGS_quic_enforce_single_packet_chlo);
  SetQuicFlagByName("FLAGS_quic_enforce_single_packet_chlo", "true");
  EXPECT_TRUE(FLAGS_quic_enforce_single_packet_chlo);
  SetQuicFlagByName("FLAGS_quic_enforce_single_packet_chlo", "False");
  EXPECT_FALSE(FLAGS_quic_enforce_single_packet_chlo);
  SetQuicFlagByName("FLAGS_quic_enforce_single_packet_chlo", "True");
  EXPECT_TRUE(FLAGS_quic_enforce_single_packet_chlo);
}

TEST(SetQuicFlagTest, Double) {
  FLAGS_quic_bbr_cwnd_gain = 3.0;
  SetQuicFlagByName("FLAGS_quic_bbr_cwnd_gain", "1.5");
  EXPECT_EQ(1.5, FLAGS_quic_bbr_cwnd_gain);
}

TEST(SetQuicFlagTest, DoubleInvalid) {
  FLAGS_quic_bbr_cwnd_gain = 3.0;
  SetQuicFlagByName("FLAGS_quic_bbr_cwnd_gain", "true");
  EXPECT_EQ(3.0, FLAGS_quic_bbr_cwnd_gain);
}

TEST(SetQuicFlagTest, Int64) {
  FLAGS_quic_max_tracked_packet_count = 100;
  SetQuicFlagByName("FLAGS_quic_max_tracked_packet_count", "5");
  EXPECT_EQ(5, FLAGS_quic_max_tracked_packet_count);
}

TEST(SetQuicFlagTest, Int64Invalid) {
  FLAGS_quic_max_tracked_packet_count = 100;
  SetQuicFlagByName("FLAGS_quic_max_tracked_packet_count", "false");
  EXPECT_EQ(100, FLAGS_quic_max_tracked_packet_count);
}

TEST(SetQuicFlagTest, Uint64) {
  FLAGS_quic_key_update_confidentiality_limit = 100;
  SetQuicFlagByName("FLAGS_quic_key_update_confidentiality_limit", "5");
  EXPECT_EQ(5u, FLAGS_quic_key_update_confidentiality_limit);
}

TEST(SetQuicFlagTest, Uint64Invalid) {
  FLAGS_quic_key_update_confidentiality_limit = 100;
  SetQuicFlagByName("FLAGS_quic_key_update_confidentiality_limit", "false");
  EXPECT_EQ(100u, FLAGS_quic_key_update_confidentiality_limit);
}

TEST(SetQuicFlagTest, Uint64Negative) {
  FLAGS_quic_key_update_confidentiality_limit = 4096;
  SetQuicFlagByName("FLAGS_quic_key_update_confidentiality_limit", "-1");
  EXPECT_EQ(4096u, FLAGS_quic_key_update_confidentiality_limit);
}

TEST(SetQuicFlagTest, Int32) {
  FLAGS_quic_lumpy_pacing_size = 1;
  SetQuicFlagByName("FLAGS_quic_lumpy_pacing_size", "10");
  EXPECT_EQ(10, FLAGS_quic_lumpy_pacing_size);
}

TEST(SetQuicFlagTest, Int32Invalid) {
  FLAGS_quic_lumpy_pacing_size = 1;
  SetQuicFlagByName("FLAGS_quic_lumpy_pacing_size", "false");
  EXPECT_EQ(1, FLAGS_quic_lumpy_pacing_size);
}

}  // namespace net::test
