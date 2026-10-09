// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"

#import <set>
#import <string>
#import <vector>

#import "base/values.h"
#import "components/ttc/app/public/tool_types.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using TtcToolDefinitionsTest = PlatformTest;

// Tests that GetDefaultToolDefinitions returns the expected set of navigation
// tools with valid JSON schemas.
TEST_F(TtcToolDefinitionsTest, TestGetDefaultToolDefinitions) {
  std::vector<ttc::ToolDefinition> definitions =
      ttc::GetDefaultToolDefinitions();
  ASSERT_EQ(definitions.size(), 3u);

  std::set<std::string> tool_names;
  for (const ttc::ToolDefinition& def : definitions) {
    SCOPED_TRACE(testing::Message() << "def.name=" << def.name);
    ASSERT_FALSE(def.name.empty());
    tool_names.insert(def.name);
    EXPECT_FALSE(def.description.empty());

    if (!def.parameters_json_schema.empty()) {
      const std::string* schema_type =
          def.parameters_json_schema.FindString("type");
      ASSERT_NE(schema_type, nullptr);
      EXPECT_EQ(*schema_type, "object");
      ASSERT_NE(def.parameters_json_schema.FindDict("properties"), nullptr);
    }
  }

  std::set<std::string> expected_tool_names = {
      ttc::kToolGoBack,
      ttc::kToolGoForward,
      ttc::kToolOpenUrl,
  };
  EXPECT_EQ(tool_names, expected_tool_names);
}

}  // namespace
