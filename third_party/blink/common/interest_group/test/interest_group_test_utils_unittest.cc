// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/test/interest_group_test_utils.h"

#include "build/build_config.h"
#include "build/buildflag.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/test_interest_group_builder.h"
#include "url/gurl.h"
#include "url/origin.h"

// NOTE: There doesn't appear to be a way to easily retrieve SCOPED_TRACE()
// elements -- the SCOPED_TRACE() values could be tested if
// https://github.com/google/googletest/issues/4589 is resolved.

namespace {

constexpr char kTestOriginName[] = "https://origin.test";
constexpr char kTestGroupName[] = "shoes";

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/41496717): Fix flaky tests on Windows.
#define MAYBE_InterestGroupTestUtilsTest DISABLED_InterestGroupTestUtilsTest
#else
#define MAYBE_InterestGroupTestUtilsTest InterestGroupTestUtilsTest
#endif
class MAYBE_InterestGroupTestUtilsTest : public testing::Test {
 protected:
  const GURL kTestUrl = GURL(kTestOriginName);
  const url::Origin kTestOrigin = url::Origin::Create(kTestUrl);
};

TEST_F(MAYBE_InterestGroupTestUtilsTest, CompareSimple) {
  blink::InterestGroup actual =
      blink::TestInterestGroupBuilder(/*owner=*/kTestOrigin, kTestGroupName)
          .Build();
  blink::InterestGroup expected = actual;
  IgExpectEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectNotEqualsForTesting(actual, expected),
                          R"(Value of: found_unequal
  Actual: false
Expected: true)");

  expected.name = "cars";
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual.name
    Which is: "shoes"
  expected.name
    Which is: "cars")");
}

TEST_F(MAYBE_InterestGroupTestUtilsTest, CompareOptional) {
  blink::InterestGroup actual =
      blink::TestInterestGroupBuilder(/*owner=*/kTestOrigin, kTestGroupName)
          .Build();
  blink::InterestGroup expected = actual;

  expected.bidding_url = kTestUrl;
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual.bidding_url
    Which is: (nullopt)
  expected.bidding_url
    Which is: (https://origin.test/))");

  expected = actual;
  actual.bidding_url = kTestUrl;
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual.bidding_url
    Which is: (https://origin.test/)
  expected.bidding_url
    Which is: (nullopt))");

  expected.bidding_url = kTestUrl;
  IgExpectEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectNotEqualsForTesting(actual, expected),
                          R"(Value of: found_unequal
  Actual: false
Expected: true)");
}

TEST_F(MAYBE_InterestGroupTestUtilsTest, CompareOptionalVector) {
  blink::InterestGroup actual =
      blink::TestInterestGroupBuilder(/*owner=*/kTestOrigin, kTestGroupName)
          .Build();
  blink::InterestGroup expected = actual;

  expected.trusted_bidding_signals_keys = {"foo"};
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual.has_value()
    Which is: false
  expected.has_value()
    Which is: true)");

  expected = actual;
  actual.trusted_bidding_signals_keys = {"foo"};
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual.has_value()
    Which is: true
  expected.has_value()
    Which is: false)");

  expected.trusted_bidding_signals_keys = {"foo"};
  IgExpectEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectNotEqualsForTesting(actual, expected),
                          R"(Value of: found_unequal
  Actual: false
Expected: true)");
}

TEST_F(MAYBE_InterestGroupTestUtilsTest, CompareOptionalMap) {
  blink::InterestGroup actual =
      blink::TestInterestGroupBuilder(/*owner=*/kTestOrigin, kTestGroupName)
          .Build();
  blink::InterestGroup expected = actual;

  expected.priority_vector = {{"foo", 0.0}};
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual.has_value()
    Which is: false
  expected.has_value()
    Which is: true)");

  expected = actual;
  actual.priority_vector = {{"foo", 0.0}};
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual.has_value()
    Which is: true
  expected.has_value()
    Which is: false)");

  expected.priority_vector = {{"foo", 0.0}};
  IgExpectEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectNotEqualsForTesting(actual, expected),
                          R"(Value of: found_unequal
  Actual: false
Expected: true)");
}

TEST_F(MAYBE_InterestGroupTestUtilsTest, CompareSizeMismatchVector) {
  blink::InterestGroup actual =
      blink::TestInterestGroupBuilder(/*owner=*/kTestOrigin, kTestGroupName)
          .Build();
  blink::InterestGroup expected = actual;

  actual.trusted_bidding_signals_keys = {"foo"};
  expected.trusted_bidding_signals_keys = {"foo", "bar"};
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual.size()
    Which is: 1
  expected.size()
    Which is: 2)");

  actual.trusted_bidding_signals_keys = {"foo", "bar"};
  expected.trusted_bidding_signals_keys = {"foo"};
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual.size()
    Which is: 2
  expected.size()
    Which is: 1)");

  actual.trusted_bidding_signals_keys = {"foo", "bar"};
  expected.trusted_bidding_signals_keys = {"foo", "bar"};
  IgExpectEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectNotEqualsForTesting(actual, expected),
                          R"(Value of: found_unequal
  Actual: false
Expected: true)");
}

TEST_F(MAYBE_InterestGroupTestUtilsTest, CompareSizeMismatchMap) {
  blink::InterestGroup actual =
      blink::TestInterestGroupBuilder(/*owner=*/kTestOrigin, kTestGroupName)
          .Build();
  blink::InterestGroup expected = actual;

  actual.priority_vector = {{"foo", 0.0}};
  expected.priority_vector = {{"foo", 0.0}, {"bar", 1.0}};
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual.size()
    Which is: 1
  expected.size()
    Which is: 2)");

  actual.priority_vector = {{"foo", 0.0}, {"bar", 1.0}};
  expected.priority_vector = {{"foo", 0.0}};
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual.size()
    Which is: 2
  expected.size()
    Which is: 1)");

  actual.priority_vector = {{"foo", 0.0}, {"bar", 1.0}};
  expected.priority_vector = {{"foo", 0.0}, {"bar", 1.0}};
  IgExpectEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectNotEqualsForTesting(actual, expected),
                          R"(Value of: found_unequal
  Actual: false
Expected: true)");
}

TEST_F(MAYBE_InterestGroupTestUtilsTest, CompareElementVector) {
  blink::InterestGroup actual =
      blink::TestInterestGroupBuilder(/*owner=*/kTestOrigin, kTestGroupName)
          .Build();
  blink::InterestGroup expected = actual;

  actual.trusted_bidding_signals_keys = {"foo"};
  expected.trusted_bidding_signals_keys = {"bar"};
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual
    Which is: "foo"
  expected
    Which is: "bar")");

  actual.trusted_bidding_signals_keys = {"foo", "bar"};
  expected.trusted_bidding_signals_keys = {"foo", "baz"};
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual
    Which is: "bar"
  expected
    Which is: "baz")");
}

TEST_F(MAYBE_InterestGroupTestUtilsTest, CompareElementMap) {
  blink::InterestGroup actual =
      blink::TestInterestGroupBuilder(/*owner=*/kTestOrigin, kTestGroupName)
          .Build();
  blink::InterestGroup expected = actual;

  actual.priority_vector = {{"foo", 0.0}};
  expected.priority_vector = {{"bar", 0.0}};
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  a_it->first
    Which is: "foo"
  b_it->first
    Which is: "bar")");

  actual.priority_vector = {{"foo", 0.0}};
  expected.priority_vector = {{"foo", 1.0}};
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual
    Which is: 0
  expected
    Which is: 1)");

  actual.priority_vector = {{"foo", 0.0}, {"bar", 1.0}};
  expected.priority_vector = {{"foo", 1.0}, {"bar", 0.0}};
  IgExpectNotEqualsForTesting(actual, expected);
  // NOTE: IgExpectEqualsForTesting() would yield 2 failures , but
  // EXPECT_NONFATAL_FAILURE() itself will fail if there's more than one
  // failure.
}

TEST_F(MAYBE_InterestGroupTestUtilsTest, CompareDeep) {
  blink::InterestGroup actual =
      blink::TestInterestGroupBuilder(/*owner=*/kTestOrigin, kTestGroupName)
          .Build();
  blink::InterestGroup expected = actual;

  actual.size_groups = {{"foo", {"bar", "baz"}}};
  expected.size_groups = {{"foo", {"bar", "bazz"}}};
  IgExpectNotEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectEqualsForTesting(actual, expected),
                          R"(Expected equality of these values:
  actual
    Which is: "baz"
  expected
    Which is: "bazz")");

  expected.size_groups = {{"foo", {"bar", "baz"}}};
  IgExpectEqualsForTesting(actual, expected);
  EXPECT_NONFATAL_FAILURE(IgExpectNotEqualsForTesting(actual, expected),
                          R"(Value of: found_unequal
  Actual: false
Expected: true)");
}

}  // namespace
