// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/notification/version_range.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(VersionRangeTest, ExactVersionMatch) {
  VersionRange range("1.2.3");
  ASSERT_TRUE(range.IsValid());
  ASSERT_TRUE(range.ContainsVersion("1.2.3"));
  ASSERT_FALSE(range.ContainsVersion("1.2.1"));
  ASSERT_FALSE(range.ContainsVersion("1.2.5"));
}

TEST(VersionRangeTest, UnboundMinimumExclusiveMaximum) {
  VersionRange range("[-1.2.3)");
  ASSERT_TRUE(range.IsValid());
  ASSERT_TRUE(range.ContainsVersion("0.1"));
  ASSERT_TRUE(range.ContainsVersion("1.2"));
  ASSERT_TRUE(range.ContainsVersion("1.2.2"));
  ASSERT_FALSE(range.ContainsVersion("1.2.3"));
  ASSERT_FALSE(range.ContainsVersion("1.2.5"));
  ASSERT_FALSE(range.ContainsVersion("2"));
}

TEST(VersionRangeTest, UnboundMinimumInclusiveMaximum) {
  VersionRange range("[-1.2.3]");
  ASSERT_TRUE(range.IsValid());
  ASSERT_TRUE(range.ContainsVersion("0.1"));
  ASSERT_TRUE(range.ContainsVersion("1.2"));
  ASSERT_TRUE(range.ContainsVersion("1.2.2"));
  ASSERT_TRUE(range.ContainsVersion("1.2.3"));
  ASSERT_FALSE(range.ContainsVersion("1.2.5"));
  ASSERT_FALSE(range.ContainsVersion("2"));
}

TEST(VersionRangeTest, InclusiveMinimumUnboundMaximum) {
  VersionRange range("[1.2.3-)");
  ASSERT_TRUE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0.1"));
  ASSERT_FALSE(range.ContainsVersion("1.2.2"));
  ASSERT_TRUE(range.ContainsVersion("1.2.3"));
  ASSERT_TRUE(range.ContainsVersion("1.2.5"));
  ASSERT_TRUE(range.ContainsVersion("2"));
}

TEST(VersionRangeTest, ExclusiveMinimumUnboundMaximum) {
  VersionRange range("(1.2.3-)");
  ASSERT_TRUE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0.1"));
  ASSERT_FALSE(range.ContainsVersion("1.2.2"));
  ASSERT_FALSE(range.ContainsVersion("1.2.3"));
  ASSERT_TRUE(range.ContainsVersion("1.2.5"));
  ASSERT_TRUE(range.ContainsVersion("2"));
}

TEST(VersionRangeTest, InclusiveMinimumExclusiveMaximum) {
  VersionRange range("[1.2-3)");
  ASSERT_TRUE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0.1"));
  ASSERT_FALSE(range.ContainsVersion("1.1"));
  ASSERT_TRUE(range.ContainsVersion("1.2"));
  ASSERT_TRUE(range.ContainsVersion("1.2.2"));
  ASSERT_TRUE(range.ContainsVersion("1.2.3"));
  ASSERT_TRUE(range.ContainsVersion("2"));
  ASSERT_FALSE(range.ContainsVersion("3"));
  ASSERT_FALSE(range.ContainsVersion("4"));
}

TEST(VersionRangeTest, ExclusiveMinimumInclusiveMaximum) {
  VersionRange range("(1.2-3]");
  ASSERT_TRUE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0.1"));
  ASSERT_FALSE(range.ContainsVersion("1.1"));
  ASSERT_FALSE(range.ContainsVersion("1.2"));
  ASSERT_TRUE(range.ContainsVersion("1.2.2"));
  ASSERT_TRUE(range.ContainsVersion("1.2.3"));
  ASSERT_TRUE(range.ContainsVersion("2"));
  ASSERT_TRUE(range.ContainsVersion("3"));
  ASSERT_FALSE(range.ContainsVersion("3.1"));
  ASSERT_FALSE(range.ContainsVersion("4"));
}

TEST(VersionRangeTest, ExclusiveMinimumExclusiveMaximum) {
  VersionRange range("(1.2-3)");
  ASSERT_TRUE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0.1"));
  ASSERT_FALSE(range.ContainsVersion("1.1"));
  ASSERT_FALSE(range.ContainsVersion("1.2"));
  ASSERT_TRUE(range.ContainsVersion("1.2.2"));
  ASSERT_TRUE(range.ContainsVersion("1.2.3"));
  ASSERT_TRUE(range.ContainsVersion("2"));
  ASSERT_FALSE(range.ContainsVersion("3"));
  ASSERT_FALSE(range.ContainsVersion("3.1"));
  ASSERT_FALSE(range.ContainsVersion("4"));
}

TEST(VersionRangeTest, AnyVersion1) {
  VersionRange range("(-)");
  ASSERT_TRUE(range.IsValid());
  ASSERT_TRUE(range.ContainsVersion("1"));
  ASSERT_TRUE(range.ContainsVersion("1.1"));
  ASSERT_TRUE(range.ContainsVersion("2"));
  ASSERT_TRUE(range.ContainsVersion("3"));
  ASSERT_TRUE(range.ContainsVersion("4"));
}

TEST(VersionRangeTest, AnyVersion2) {
  VersionRange range("[-]");
  ASSERT_TRUE(range.IsValid());
  ASSERT_TRUE(range.ContainsVersion("1"));
  ASSERT_TRUE(range.ContainsVersion("1.1"));
  ASSERT_TRUE(range.ContainsVersion("2"));
  ASSERT_TRUE(range.ContainsVersion("3"));
  ASSERT_TRUE(range.ContainsVersion("4"));
}

TEST(VersionRangeTest, InvalidRangeSpec_SyntaxError1) {
  VersionRange range("");
  ASSERT_FALSE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0"));
  ASSERT_FALSE(range.ContainsVersion("1"));
}

TEST(VersionRangeTest, InvalidRangeSpec_SyntaxError2) {
  VersionRange range("90j3awef");
  ASSERT_FALSE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0"));
  ASSERT_FALSE(range.ContainsVersion("1"));
}

TEST(VersionRangeTest, InvalidRangeSpec_SyntaxError3) {
  VersionRange range("[1.2)");
  ASSERT_FALSE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0"));
  ASSERT_FALSE(range.ContainsVersion("1.2"));
}

TEST(VersionRangeTest, InvalidRangeSpec_SyntaxError4) {
  VersionRange range("[1.2-");
  ASSERT_FALSE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0"));
  ASSERT_FALSE(range.ContainsVersion("1.2"));
}

TEST(VersionRangeTest, InvalidRangeSpec_SyntaxError5) {
  VersionRange range("1.2-)");
  ASSERT_FALSE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0"));
  ASSERT_FALSE(range.ContainsVersion("1.2"));
}

TEST(VersionRangeTest, InvalidRangeSpec_SyntaxError6) {
  VersionRange range("1.2-");
  ASSERT_FALSE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0"));
  ASSERT_FALSE(range.ContainsVersion("1.2"));
}

TEST(VersionRangeTest, InvalidRangeSpec_SyntaxError7) {
  VersionRange range("-1.2");
  ASSERT_FALSE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0"));
  ASSERT_FALSE(range.ContainsVersion("1.2"));
}

TEST(VersionRangeTest, InvalidRangeSpec_SyntaxError8) {
  VersionRange range("[-1.2");
  ASSERT_FALSE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0"));
  ASSERT_FALSE(range.ContainsVersion("1.2"));
}

TEST(VersionRangeTest, InvalidRangeSpec_SyntaxError9) {
  VersionRange range("-1.2)");
  ASSERT_FALSE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("0"));
  ASSERT_FALSE(range.ContainsVersion("1.2"));
}

TEST(VersionRangeTest, InvalidRangeSpec_MinBiggerThanMax) {
  VersionRange range("[3.4-1.2)");
  ASSERT_FALSE(range.IsValid());
  ASSERT_FALSE(range.ContainsVersion("1.2"));
  ASSERT_FALSE(range.ContainsVersion("2.0"));
  ASSERT_FALSE(range.ContainsVersion("3.4"));
}

}  // namespace remoting
