// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/speculation_rules/speculation_rules_tags.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using Tags = std::vector<std::optional<std::string>>;

void TestTags(const Tags& tags, const std::string& expected) {
  EXPECT_EQ(expected, SpeculationRulesTags(tags).ConvertStringToHeaderString());
}

// Each string tag should be wrapped with double quotes during parsing.
// https://www.rfc-editor.org/rfc/rfc8941.html#name-serializing-a-string
TEST(SpeculationRulesTagsTest, Basic) {
  TestTags({R"(my-rules)"}, R"("my-rules")");
  TestTags({R"(my-rules1)", R"(my-rules2)"}, R"("my-rules1", "my-rules2")");
}

// No tags should be parsed as null token.
// https://www.rfc-editor.org/rfc/rfc8941.html#section-4.1.7
TEST(SpeculationRulesTagsTest, NoTags) {
  TestTags({std::nullopt}, R"(null)");
  TestTags({std::nullopt, R"(my-rules)"}, R"(null, "my-rules")");
  TestTags({std::nullopt, R"(my-rules)", R"(null)"},
           R"(null, "my-rules", "null")");
}

// A double quote (DQUOTE) should be converted to \" during parsing:
// https://www.rfc-editor.org/rfc/rfc8941.html#name-serializing-a-string
TEST(SpeculationRulesTagsTest, DoubleQuote) {
  TestTags({R"(")"}, R"("\"")");
  TestTags({R"("")"}, R"("\"\"")");
  TestTags({R"(my"rules)"}, R"("my\"rules")");
  TestTags({R"(my-rules)", R"(")"}, R"("\"", "my-rules")");
}

// A backslash should be converted to \\ during parsing:
// https://www.rfc-editor.org/rfc/rfc8941.html#name-serializing-a-string
TEST(SpeculationRulesTagsTest, BackSlash) {
  TestTags({R"(\)"}, R"("\\")");
  TestTags({R"(\\)"}, R"("\\\\")");
  TestTags({R"(my\rules)"}, R"("my\\rules")");
  TestTags({R"(my-rules)", R"(\)"}, R"("\\", "my-rules")");
}

TEST(SpeculationRulesTagsTest, Duplicate) {
  TestTags({std::nullopt, R"(my-rules)", R"(my-rules)"}, R"(null, "my-rules")");
  TestTags({std::nullopt, std::nullopt, R"(my-rules)"}, R"(null, "my-rules")");
}

TEST(SpeculationRulesTagsTest, Sort) {
  TestTags({R"(def)", R"(jkl)", R"(def)", R"(null)", std::nullopt, R"(abc)",
            std::nullopt, R"(ghi)"},
           R"(null, "abc", "def", "ghi", "jkl", "null")");
}

}  // namespace
}  // namespace content
