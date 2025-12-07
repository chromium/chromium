// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/test_runner_utils.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TestRunnerTest : public testing::Test {};

TEST_F(TestRunnerTest, ParseWebSettingsString_Basic) {
  auto result = TestRunnerUtils::ParseWebSettingsString("key:value");
  EXPECT_THAT(result, testing::ElementsAre(testing::Pair("key", "value")));
}

TEST_F(TestRunnerTest, ParseWebSettingsString_Multiple) {
  auto result =
      TestRunnerUtils::ParseWebSettingsString("key1:value1,key2:value2");
  EXPECT_THAT(result, testing::ElementsAre(testing::Pair("key1", "value1"),
                                           testing::Pair("key2", "value2")));
}

TEST_F(TestRunnerTest, ParseWebSettingsString_Empty) {
  auto result = TestRunnerUtils::ParseWebSettingsString("");
  EXPECT_THAT(result, testing::ElementsAre());
}

TEST_F(TestRunnerTest, ParseWebSettingsString_BasicWithTrailingComma) {
  auto result = TestRunnerUtils::ParseWebSettingsString("key:value,");
  EXPECT_THAT(result, testing::ElementsAre(testing::Pair("key", "value")));
}

TEST_F(TestRunnerTest, ParseWebSettingsString_InvalidFormat) {
  auto result = TestRunnerUtils::ParseWebSettingsString("key;value");
  EXPECT_THAT(result, testing::ElementsAre());
}

TEST_F(TestRunnerTest, ParseWebSettingsString_TrimmingSpace) {
  auto result = TestRunnerUtils::ParseWebSettingsString(" key: value ");
  EXPECT_THAT(result, testing::ElementsAre(testing::Pair("key", "value")));
}

TEST_F(TestRunnerTest, ParseWebSettingsString_TrimmingLineBreak) {
  auto result =
      TestRunnerUtils::ParseWebSettingsString("key1:value1,\nkey2:value2");
  EXPECT_THAT(result, testing::ElementsAre(testing::Pair("key1", "value1"),
                                           testing::Pair("key2", "value2")));
}

}  // namespace content
