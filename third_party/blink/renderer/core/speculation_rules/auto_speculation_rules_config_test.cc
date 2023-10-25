// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/auto_speculation_rules_config.h"
#include "base/types/cxx23_to_underlying.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class AutoSpeculationRulesConfigTest : public ::testing::Test {
 protected:
  void ExpectNoFrameworkSpeculationRules(
      const AutoSpeculationRulesConfig& config) {
    for (auto i = base::to_underlying(mojom::JavaScriptFramework::kMinValue);
         i <= base::to_underlying(mojom::JavaScriptFramework::kMaxValue); ++i) {
      auto framework = static_cast<mojom::JavaScriptFramework>(i);
      EXPECT_TRUE(config.ForFramework(framework).IsNull());
    }
  }
};

TEST_F(AutoSpeculationRulesConfigTest, EmptyConfig) {
  AutoSpeculationRulesConfig config("{}");
  ExpectNoFrameworkSpeculationRules(config);
}

TEST_F(AutoSpeculationRulesConfigTest, ValidConfig) {
  AutoSpeculationRulesConfig config(R"(
  {
    "framework_to_speculation_rules": {
      "1": "speculation_rules_1",
      "3": "speculation_rules_3"
    }
  }
  )");

  EXPECT_TRUE(config.ForFramework(mojom::JavaScriptFramework::kNuxt /* = 0 */)
                  .IsNull());
  EXPECT_EQ(
      config.ForFramework(mojom::JavaScriptFramework::kVuePress /* = 1 */),
      "speculation_rules_1");
  EXPECT_TRUE(config.ForFramework(mojom::JavaScriptFramework::kSapper /* = 2 */)
                  .IsNull());
  EXPECT_EQ(config.ForFramework(mojom::JavaScriptFramework::kGatsby /* = 1 */),
            "speculation_rules_3");
}

TEST_F(AutoSpeculationRulesConfigTest, NonJSONConfig) {
  AutoSpeculationRulesConfig config("{]");
  ExpectNoFrameworkSpeculationRules(config);
}

TEST_F(AutoSpeculationRulesConfigTest, NonObjectConfig) {
  AutoSpeculationRulesConfig config("true");
  ExpectNoFrameworkSpeculationRules(config);
}

TEST_F(AutoSpeculationRulesConfigTest, NonObjectFrameworkToSpeculationRules) {
  AutoSpeculationRulesConfig config(R"(
  {
    "framework_to_speculation_rules": true
  }
  )");
  ExpectNoFrameworkSpeculationRules(config);
}

TEST_F(AutoSpeculationRulesConfigTest, OutOfRangeFramework) {
  static_assert(base::to_underlying(mojom::JavaScriptFramework::kMaxValue) <
                999);

  AutoSpeculationRulesConfig config(R"(
  {
    "framework_to_speculation_rules": {
      "999": "speculation_rules_999",
      "1": "speculation_rules_1"
    }
  }
  )");
  EXPECT_EQ(
      config.ForFramework(mojom::JavaScriptFramework::kVuePress /* = 1 */),
      "speculation_rules_1");
  EXPECT_TRUE(config.ForFramework(static_cast<mojom::JavaScriptFramework>(999))
                  .IsNull());
}

TEST_F(AutoSpeculationRulesConfigTest, NonIntegerFramework) {
  static_assert(base::to_underlying(mojom::JavaScriptFramework::kMaxValue) <
                999);

  AutoSpeculationRulesConfig config(R"(
  {
    "framework_to_speculation_rules": {
      "999.1": "speculation_rules_999.1",
      "1": "speculation_rules_1"
    }
  }
  )");
  EXPECT_EQ(
      config.ForFramework(mojom::JavaScriptFramework::kVuePress /* = 1 */),
      "speculation_rules_1");
  EXPECT_TRUE(config.ForFramework(static_cast<mojom::JavaScriptFramework>(999))
                  .IsNull());
}

TEST_F(AutoSpeculationRulesConfigTest, NonStringSpeculationRules) {
  static_assert(base::to_underlying(mojom::JavaScriptFramework::kMaxValue) <
                999);

  AutoSpeculationRulesConfig config(R"(
  {
    "framework_to_speculation_rules": {
      "0": 0,
      "1": "speculation_rules_1"
    }
  }
  )");
  EXPECT_TRUE(config.ForFramework(mojom::JavaScriptFramework::kNuxt /* = 0 */)
                  .IsNull());
  EXPECT_EQ(
      config.ForFramework(mojom::JavaScriptFramework::kVuePress /* = 1 */),
      "speculation_rules_1");
}

}  // namespace
}  // namespace blink
