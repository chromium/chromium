// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/port_range.h"

#include <sstream>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(PortRange, ParseEmpty) {
  auto port_range = PortRange::Parse("");
  ASSERT_TRUE(port_range.has_value());
  EXPECT_TRUE(port_range->is_null());
}

TEST(PortRange, ParseValid) {
  auto port_range = PortRange::Parse("1-65535");
  ASSERT_TRUE(port_range.has_value());
  EXPECT_FALSE(port_range->is_null());
  EXPECT_EQ(port_range->min_port(), 1u);
  EXPECT_EQ(port_range->max_port(), 65535u);

  port_range = PortRange::Parse(" 1 - 65535 ");
  ASSERT_TRUE(port_range.has_value());
  EXPECT_FALSE(port_range->is_null());
  EXPECT_EQ(port_range->min_port(), 1u);
  EXPECT_EQ(port_range->max_port(), 65535u);

  port_range = PortRange::Parse("12400-12400");
  ASSERT_TRUE(port_range.has_value());
  EXPECT_FALSE(port_range->is_null());
  EXPECT_EQ(port_range->min_port(), 12400u);
  EXPECT_EQ(port_range->max_port(), 12400u);
}

TEST(PortRange, ParseInvalid) {
  EXPECT_FALSE(PortRange::Parse("-65535").has_value());
  EXPECT_FALSE(PortRange::Parse("1-").has_value());
  EXPECT_FALSE(PortRange::Parse("-").has_value());
  EXPECT_FALSE(PortRange::Parse("-1-65535").has_value());
  EXPECT_FALSE(PortRange::Parse("1--65535").has_value());
  EXPECT_FALSE(PortRange::Parse("1-65535-").has_value());
  EXPECT_FALSE(PortRange::Parse("0-65535").has_value());
  EXPECT_FALSE(PortRange::Parse("0-0").has_value());
  EXPECT_FALSE(PortRange::Parse("1-65536").has_value());
  EXPECT_FALSE(PortRange::Parse("1-4294967295").has_value());
  EXPECT_FALSE(PortRange::Parse("1foo-2bar").has_value());
  EXPECT_FALSE(PortRange::Parse("10-1").has_value());
}

TEST(PortRange, Create) {
  EXPECT_TRUE(PortRange::Create(0, 0).has_value());
  EXPECT_TRUE(PortRange::Create(0, 0)->is_null());
  EXPECT_TRUE(PortRange::Create(12400, 12409).has_value());
  EXPECT_EQ(PortRange::Create(12400, 12409)->min_port(), 12400u);
  EXPECT_EQ(PortRange::Create(12400, 12409)->max_port(), 12409u);
  EXPECT_TRUE(PortRange::Create(12400, 12400).has_value());

  EXPECT_FALSE(PortRange::Create(100, 50).has_value());
  EXPECT_FALSE(PortRange::Create(0, 80).has_value());
  EXPECT_FALSE(PortRange::Create(80, 0).has_value());
  EXPECT_FALSE(PortRange::Create(1, 0).has_value());
}

TEST(PortRange, Output) {
  auto port_range = PortRange::Create(123, 456);
  ASSERT_TRUE(port_range.has_value());

  std::ostringstream str;
  str << *port_range;

  EXPECT_THAT(str.str(), testing::MatchesRegex(".*123.*456.*"));
}

TEST(PortRange, Equality) {
  auto port_range_1 = PortRange::Create(123, 456);
  auto port_range_2 = PortRange::Create(123, 456);
  auto port_range_3 = PortRange::Create(456, 789);

  ASSERT_TRUE(port_range_1.has_value());
  ASSERT_TRUE(port_range_2.has_value());
  ASSERT_TRUE(port_range_3.has_value());

  EXPECT_EQ(*port_range_1, *port_range_2);
  EXPECT_NE(*port_range_1, *port_range_3);
}

}  // namespace remoting
