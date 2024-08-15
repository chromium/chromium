// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/port_range.h"

#include <sstream>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(PortRange, ParseEmpty) {
  PortRange port_range;

  EXPECT_TRUE(PortRange::Parse("", &port_range));
  EXPECT_TRUE(port_range.is_null());
}

TEST(PortRange, ParseValid) {
  PortRange port_range;

  EXPECT_TRUE(PortRange::Parse("1-65535", &port_range));
  EXPECT_FALSE(port_range.is_null());
  EXPECT_EQ(1u, port_range.min_port);
  EXPECT_EQ(65535u, port_range.max_port);

  EXPECT_TRUE(PortRange::Parse(" 1 - 65535 ", &port_range));
  EXPECT_FALSE(port_range.is_null());
  EXPECT_EQ(1u, port_range.min_port);
  EXPECT_EQ(65535u, port_range.max_port);

  EXPECT_TRUE(PortRange::Parse("12400-12400", &port_range));
  EXPECT_FALSE(port_range.is_null());
  EXPECT_EQ(12400u, port_range.min_port);
  EXPECT_EQ(12400u, port_range.max_port);
}

TEST(PortRange, ParseInvalid) {
  PortRange port_range;
  port_range.min_port = 123;
  port_range.max_port = 456;

  EXPECT_FALSE(PortRange::Parse("-65535", &port_range));
  EXPECT_FALSE(PortRange::Parse("1-", &port_range));
  EXPECT_FALSE(PortRange::Parse("-", &port_range));
  EXPECT_FALSE(PortRange::Parse("-1-65535", &port_range));
  EXPECT_FALSE(PortRange::Parse("1--65535", &port_range));
  EXPECT_FALSE(PortRange::Parse("1-65535-", &port_range));
  EXPECT_FALSE(PortRange::Parse("0-65535", &port_range));
  EXPECT_FALSE(PortRange::Parse("1-65536", &port_range));
  EXPECT_FALSE(PortRange::Parse("1-4294967295", &port_range));
  EXPECT_FALSE(PortRange::Parse("10-1", &port_range));
  EXPECT_FALSE(PortRange::Parse("1foo-2bar", &port_range));

  // Unsuccessful parses should NOT modify their output.
  EXPECT_EQ(123, port_range.min_port);
  EXPECT_EQ(456, port_range.max_port);
}

TEST(PortRange, Output) {
  PortRange port_range;
  port_range.min_port = 123;
  port_range.max_port = 456;

  std::ostringstream str;
  str << port_range;

  EXPECT_THAT(str.str(), testing::MatchesRegex(".*123.*456.*"));
}

TEST(PortRange, Equality) {
  PortRange port_range_1;
  port_range_1.min_port = 123;
  port_range_1.max_port = 456;

  PortRange port_range_2;
  port_range_2.min_port = 123;
  port_range_2.max_port = 456;

  PortRange port_range_3;
  port_range_3.min_port = 456;
  port_range_3.max_port = 789;

  EXPECT_EQ(port_range_1, port_range_2);
  EXPECT_NE(port_range_1, port_range_3);
}

}  // namespace remoting
