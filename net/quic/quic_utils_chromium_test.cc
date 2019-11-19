// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_utils_chromium.h"

#include <map>

#include "net/third_party/quiche/src/quic/core/crypto/crypto_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

TEST(QuicUtilsChromiumTest, ParseQuicConnectionOptions) {
  quic::QuicTagVector empty_options = ParseQuicConnectionOptions("");
  EXPECT_TRUE(empty_options.empty());

  quic::QuicTagVector parsed_options =
      ParseQuicConnectionOptions("TIMER,TBBR,REJ");
  quic::QuicTagVector expected_options;
  expected_options.push_back(quic::kTIME);
  expected_options.push_back(quic::kTBBR);
  expected_options.push_back(quic::kREJ);
  EXPECT_EQ(expected_options, parsed_options);
}

TEST(QuicUtilsChromiumTest, FindOrNullTest) {
  std::map<int, int> m;
  m[0] = 2;

  // Check FindOrNull
  int* p1 = FindOrNull(m, 0);
  CHECK_EQ(*p1, 2);
  ++(*p1);
  const std::map<int, int>& const_m = m;
  const int* p2 = FindOrNull(const_m, 0);
  CHECK_EQ(*p2, 3);
  CHECK(FindOrNull(m, 1) == nullptr);
}

TEST(QuicUtilsChromiumTest, FindOrDieTest) {
  std::map<int, int> m;
  m[10] = 15;
  EXPECT_EQ(15, FindOrDie(m, 10));
  // TODO(rtenneti): Use the latest DEATH macros after merging with latest rch's
  // changes.
  // ASSERT_DEATH(FindOrDie(m, 8), "Map key not found: 8");

  // Make sure the non-const reference returning version works.
  FindOrDie(m, 10) = 20;
  EXPECT_EQ(20, FindOrDie(m, 10));

  // Make sure we can lookup values in a const map.
  const std::map<int, int>& const_m = m;
  EXPECT_EQ(20, FindOrDie(const_m, 10));
}

}  // namespace
}  // namespace test
}  // namespace net
