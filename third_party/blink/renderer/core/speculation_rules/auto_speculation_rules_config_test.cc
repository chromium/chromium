// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/auto_speculation_rules_config.h"

#include "base/types/cxx23_to_underlying.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

using testing::ElementsAre;

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

TEST_F(AutoSpeculationRulesConfigTest, NonJSONConfig) {
  AutoSpeculationRulesConfig config("{]");
  ExpectNoFrameworkSpeculationRules(config);
}

TEST_F(AutoSpeculationRulesConfigTest, NonObjectConfig) {
  AutoSpeculationRulesConfig config("true");
  ExpectNoFrameworkSpeculationRules(config);
}

TEST_F(AutoSpeculationRulesConfigTest, ValidFrameworkToSpeculationRules) {
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

TEST_F(AutoSpeculationRulesConfigTest, NonStringFrameworkSpeculationRules) {
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

TEST_F(AutoSpeculationRulesConfigTest, ValidUrlMatchPattern) {
  AutoSpeculationRulesConfig config(R"(
  {
    "url_match_pattern_to_speculation_rules": {
      "https://example.com/": "speculation_rules_1",
      "https://other.example.com/*": "speculation_rules_2",
      "https://*.example.org/*": "speculation_rules_3",
      "https://*.example.*/*": "speculation_rules_4",
      "https://example.co?/": "speculation_rules_5"
    }
  }
  )");

  EXPECT_THAT(
      config.ForUrl(KURL("https://example.com/")),
      ElementsAre(
          std::make_pair("speculation_rules_1",
                         BrowserInjectedSpeculationRuleOptOut::kRespect),
          std::make_pair("speculation_rules_5",
                         BrowserInjectedSpeculationRuleOptOut::kRespect)));

  EXPECT_THAT(config.ForUrl(KURL("https://example.com/path")), ElementsAre());

  EXPECT_THAT(
      config.ForUrl(KURL("https://other.example.com/path")),
      ElementsAre(
          std::make_pair("speculation_rules_2",
                         BrowserInjectedSpeculationRuleOptOut::kRespect),
          std::make_pair("speculation_rules_4",
                         BrowserInjectedSpeculationRuleOptOut::kRespect)));

  EXPECT_THAT(config.ForUrl(KURL("https://example.org/")), ElementsAre());

  EXPECT_THAT(
      config.ForUrl(KURL("https://www.example.org/path")),
      ElementsAre(
          std::make_pair("speculation_rules_3",
                         BrowserInjectedSpeculationRuleOptOut::kRespect),
          std::make_pair("speculation_rules_4",
                         BrowserInjectedSpeculationRuleOptOut::kRespect)));

  EXPECT_THAT(config.ForUrl(KURL("https://example.co/")),
              ElementsAre(std::make_pair(
                  "speculation_rules_5",
                  BrowserInjectedSpeculationRuleOptOut::kRespect)));

  EXPECT_THAT(config.ForUrl(KURL("https://www.example.xyz/")),
              ElementsAre(std::make_pair(
                  "speculation_rules_4",
                  BrowserInjectedSpeculationRuleOptOut::kRespect)));
}

TEST_F(AutoSpeculationRulesConfigTest, NonObjectUrlMatchPatterns) {
  AutoSpeculationRulesConfig config(R"(
  {
    "url_match_pattern_to_speculation_rules": true
  }
  )");

  // Basically testing that ForUrl() doesn't crash or something.
  EXPECT_TRUE(config.ForUrl(KURL("https://example.com/")).empty());
}

TEST_F(AutoSpeculationRulesConfigTest,
       NonStringUrlMatchPatternSpeculationRules) {
  AutoSpeculationRulesConfig config(R"(
  {
    "url_match_pattern_to_speculation_rules": {
      "https://example.com/": 0
    }
  }
  )");

  EXPECT_TRUE(config.ForUrl(KURL("https://example.com/")).empty());
}

TEST_F(AutoSpeculationRulesConfigTest,
       NonObjectFrameworkValidUrlMatchPatterns) {
  AutoSpeculationRulesConfig config(R"(
  {
    "framework_to_speculation_rules": true,
    "url_match_pattern_to_speculation_rules": {
      "https://example.com/": "speculation_rules_1"
    }
  }
  )");

  ExpectNoFrameworkSpeculationRules(config);
  EXPECT_THAT(config.ForUrl(KURL("https://example.com/")),
              ElementsAre(std::make_pair(
                  "speculation_rules_1",
                  BrowserInjectedSpeculationRuleOptOut::kRespect)));
}

TEST_F(AutoSpeculationRulesConfigTest,
       ValidFrameworkNonObjectUrlMatchPatterns) {
  AutoSpeculationRulesConfig config(R"(
  {
    "framework_to_speculation_rules": {
      "1": "speculation_rules_1"
    },
    "url_match_pattern_to_speculation_rules": true
  }
  )");

  EXPECT_EQ(
      config.ForFramework(mojom::JavaScriptFramework::kVuePress /* = 1 */),
      "speculation_rules_1");
  EXPECT_THAT(config.ForUrl(KURL("https://example.com/")), ElementsAre());
}

TEST_F(AutoSpeculationRulesConfigTest, ValidFrameworkValidUrlMatchPatterns) {
  AutoSpeculationRulesConfig config(R"(
  {
    "framework_to_speculation_rules": {
      "1": "speculation_rules_1"
    },
    "url_match_pattern_to_speculation_rules": {
      "https://example.com/": "speculation_rules_2"
    }
  }
  )");

  EXPECT_EQ(
      config.ForFramework(mojom::JavaScriptFramework::kVuePress /* = 1 */),
      "speculation_rules_1");
  EXPECT_THAT(config.ForUrl(KURL("https://example.com/")),
              ElementsAre(std::make_pair(
                  "speculation_rules_2",
                  BrowserInjectedSpeculationRuleOptOut::kRespect)));
}

TEST_F(AutoSpeculationRulesConfigTest, ValidUrlMatchPatternsIgnoreOptOut) {
  AutoSpeculationRulesConfig config(R"(
  {
    "url_match_pattern_to_speculation_rules_ignore_opt_out": {
      "https://example.com/": "speculation_rules_2"
    }
  }
  )");

  EXPECT_THAT(config.ForUrl(KURL("https://example.com/")),
              ElementsAre(std::make_pair(
                  "speculation_rules_2",
                  BrowserInjectedSpeculationRuleOptOut::kIgnore)));
}

}  // namespace
}  // namespace blink
