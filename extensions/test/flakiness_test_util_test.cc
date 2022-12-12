// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/flakiness_test_util.h"

// Create a "real" subclass just to more accurately mimic what most test
// suites would do.
class FlakinessTestUtilTest : public testing::Test {
 public:
  FlakinessTestUtilTest() = default;
  ~FlakinessTestUtilTest() override = default;
};

// Since we're not really exercising flakiness (just compilation), we only
// instantiate a single test run.
INSTANTIATE_FLAKINESS_TEST(FlakinessTestUtilTest, 1);

// This test exercises the output from the INSTANTIATE_FLAKINESS_TEST macro.
TEST_P(FLAKINESS_TEST_NAME(FlakinessTestUtilTest), FlakinessTest) {
  const ::testing::TestInfo* test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  // Test the generated test suite name.
  EXPECT_STREQ("AAAA_Flaky/AAAA_Deflake_FlakinessTestUtilTest",
               test_info->test_suite_name());
  // Test the test case name. Since we only instantiate with a range of 1, there
  // should only be a single test (/0).
  EXPECT_STREQ("FlakinessTest/0", test_info->name());
}
