// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/reentrancy_checker.h"

#include "base/check_op.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class ReentrancyCheckerTest : public testing::Test {
 public:
  void UseNonReentrantScope() { NON_REENTRANT_SCOPE(reentrancy_checker_); }

 protected:
  REENTRANCY_CHECKER(reentrancy_checker_);
};

TEST_F(ReentrancyCheckerTest, Construct) {}

TEST_F(ReentrancyCheckerTest, NonReentrantUse) {
  NON_REENTRANT_SCOPE(reentrancy_checker_);
}

TEST_F(ReentrancyCheckerTest, ReentrantUse) {
  NON_REENTRANT_SCOPE(reentrancy_checker_);

#if DCHECK_IS_ON()
  EXPECT_DEATH_IF_SUPPORTED(UseNonReentrantScope(), "reentered");
#else
  // Does nothing if DCHECKs are off.
  UseNonReentrantScope();
#endif  // DCHECK_IS_ON()
}

}  // namespace media
