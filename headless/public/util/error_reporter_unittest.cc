// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/error_reporter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {

TEST(ErrorReporterTest, NoErrors) {
  ErrorReporter reporter;
#if DCHECK_IS_ON()
  EXPECT_FALSE(reporter.HasErrors());
  EXPECT_TRUE(reporter.errors().empty());
#endif  // DCHECK_IS_ON()
}

TEST(ErrorReporterTest, TopLevelErrors) {
  ErrorReporter reporter;
  reporter.AddError("instructions unclear");
  reporter.AddError("head stuck in std::unordered_map");
#if DCHECK_IS_ON()
  EXPECT_TRUE(reporter.HasErrors());
  EXPECT_EQ(2u, reporter.errors().size());
  EXPECT_EQ("instructions unclear", reporter.errors()[0]);
  EXPECT_EQ("head stuck in std::unordered_map", reporter.errors()[1]);
#endif  // DCHECK_IS_ON()
}

TEST(ErrorReporterTest, UnnamedContext) {
  ErrorReporter reporter;
  reporter.Push();
  reporter.AddError("lp0 is on fire");
  reporter.Pop();
#if DCHECK_IS_ON()
  EXPECT_TRUE(reporter.HasErrors());
  EXPECT_EQ(1u, reporter.errors().size());
  EXPECT_EQ("lp0 is on fire", reporter.errors()[0]);
#endif  // DCHECK_IS_ON()
}

TEST(ErrorReporterTest, NestedContexts) {
  ErrorReporter reporter;
  reporter.Push();
  reporter.SetName("ship");
  reporter.Push();
  reporter.SetName("front");
  reporter.AddError("fell off");
  reporter.Pop();
  reporter.AddError("uh oh");
  reporter.Pop();
#if DCHECK_IS_ON()
  EXPECT_TRUE(reporter.HasErrors());
  EXPECT_EQ(2u, reporter.errors().size());
  EXPECT_EQ("ship.front: fell off", reporter.errors()[0]);
  EXPECT_EQ("ship: uh oh", reporter.errors()[1]);
#endif  // DCHECK_IS_ON()
}

}  // namespace headless
