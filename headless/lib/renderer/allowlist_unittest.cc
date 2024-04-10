// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/renderer/allowlist.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {
namespace {

TEST(AllowlistTest, Empty) {
  EXPECT_THAT(Allowlist("", false).IsAllowed("foo"), testing::IsFalse());
  EXPECT_THAT(Allowlist("", true).IsAllowed("foo"), testing::IsTrue());
}

TEST(AllowlistTest, Trivial) {
  EXPECT_THAT(Allowlist("foo", false).IsAllowed("foo"), testing::IsTrue());
  EXPECT_THAT(Allowlist("foo,bar", false).IsAllowed("foo"), testing::IsTrue());
  EXPECT_THAT(Allowlist("foo,bar", false).IsAllowed("bar"), testing::IsTrue());
  EXPECT_THAT(Allowlist("foo,bar", false).IsAllowed("bazz"),
              testing::IsFalse());
}

TEST(AllowlistTest, Wildcard) {
  EXPECT_THAT(Allowlist("*", false).IsAllowed("foo"), testing::IsTrue());
  EXPECT_THAT(Allowlist("foo", false).IsAllowed("*"), testing::IsFalse());
}

TEST(AllowlistTest, Negative) {
  EXPECT_THAT(Allowlist("-foo,*", false).IsAllowed("foo"), testing::IsFalse());
  EXPECT_THAT(Allowlist("-foo,*", false).IsAllowed("bar"), testing::IsTrue());
}

TEST(AllowlistTest, Order) {
  EXPECT_THAT(Allowlist("-foo,foo", false).IsAllowed("foo"),
              testing::IsFalse());
  EXPECT_THAT(Allowlist("foo,-foo", false).IsAllowed("foo"), testing::IsTrue());
}

}  // namespace
}  // namespace headless
