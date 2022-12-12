// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_FLAKINESS_TEST_UTIL_H_
#define EXTENSIONS_TEST_FLAKINESS_TEST_UTIL_H_

#include "testing/gtest/include/gtest/gtest.h"

// NO *LANDED* CODE SHOULD EVER USE THESE. They are purely for development
// aid.
//
// A set of helper macros to quickly instantiate a test suite for a test to
// run hundreds of times in order to help flush out flakes.
//
// Given a test suite, MyTestSuite, these macros work together to generate a
// parameterized test that will run a given number of times, as follows:
//
//   // Note: Prepend "AAAA_" so that the test always comes first.
//   class AAAA_Deflake_MyTestSuite : public MyTestSuite,
//                                    public testing::WithParamInterface<int> {
//   };
//
//   INSTANTIATE_TEST_SUITE_P(
//       AAAA_Flaky, AAAA_Deflake_MyTestSuite,
//       testing::Range(0, 100));  // Or another provided amount.
//
// Assume there is the following flaky test that you want to exercise:
//
//   class MyTestSuite { ... };
//
//   // Disabled for flakiness.
//   IN_PROC_BROWSER_TEST_F(MyTestSuite, DISABLED_MyTestCase) {
//     ...
//   }
//
// These macros can be used as follows:
//
//   class MyTestSuite { ... };
//
//   INSTANTIATE_FLAKINESS_TEST_100(MyTestSuite);
//   IN_PROC_BROWSER_TEST_P(FLAKINESS_TEST_NAME(MyTestSuite), MyTestCase) {
//     ...
//   }
//
// The generated test will run 100 times.

namespace extensions {

// Produces the flakiness test name, e.g. "AAAA_Deflake_MyTestSuite".
#define FLAKINESS_TEST_NAME(TestName) AAAA_Deflake_##TestName

namespace internal {

// Defines the subclass for the parameterized test suite.
#define DEFINE_FLAKINESS_TEST(TestName) \
  class FLAKINESS_TEST_NAME(TestName)   \
      : public TestName, public testing::WithParamInterface<int> {}

// Instantiates the test suite. This is broken into multiple helpers in order
// to work around C's, uh, interesting macro evaluation patterns.
#define INSTANTIATE_FLAKINESS_TEST_IMPL(IterationCount, FlakinessTestName) \
  INSTANTIATE_TEST_SUITE_P(AAAA_Flaky, FlakinessTestName,                  \
                           testing::Range(0, IterationCount))

#define INSTANTIATE_FLAKINESS_TEST_HELPER(IterationCount, ...) \
  INSTANTIATE_FLAKINESS_TEST_IMPL(IterationCount, ##__VA_ARGS__)

}  // namespace internal

// Defines and instantiates a test suite that will run `IterationCount` times.
#define INSTANTIATE_FLAKINESS_TEST(TestName, IterationCount) \
  DEFINE_FLAKINESS_TEST(TestName);                           \
  INSTANTIATE_FLAKINESS_TEST_HELPER(IterationCount,          \
                                    FLAKINESS_TEST_NAME(TestName))

#define INSTANTIATE_FLAKINESS_TEST_100(TestName) \
  INSTANTIATE_FLAKINESS_TEST(TestName, 100)

#define INSTANTIATE_FLAKINESS_TEST_1000(TestName) \
  INSTANTIATE_FLAKINESS_TEST(TestName, 1000)

}  // namespace extensions

#endif  // EXTENSIONS_TEST_FLAKINESS_TEST_UTIL_H_
