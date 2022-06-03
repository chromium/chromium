// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/values_test_util.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/api/declarative/declarative_manifest_data.h"
#include "extensions/common/manifest_test.h"

namespace extensions {

using api_test_utils::ParseDictionary;
using DeclarativeManifestTest = ManifestTest;

TEST_F(DeclarativeManifestTest, Valid) {
  scoped_refptr<Extension> extension = LoadAndExpectSuccess("event_rules.json");
  DeclarativeManifestData* manifest_data =
      DeclarativeManifestData::Get(extension.get());
  ASSERT_TRUE(manifest_data);
  std::vector<DeclarativeManifestData::Rule> rules =
      manifest_data->RulesForEvent("foo");
  EXPECT_EQ(1u, rules.size());
  std::unique_ptr<base::DictionaryValue> expected_rule = ParseDictionary(
      "{"
      "  \"actions\": [{"
      "    \"instanceType\": \"action_type\""
      "  }],"
      "  \"conditions\" : [{"
      "    \"instanceType\" : \"condition_type\""
      "  }]"
      "}");
  EXPECT_TRUE(expected_rule->Equals(rules[0].ToValue().get()));
}

TEST_F(DeclarativeManifestTest, ConditionMissingType) {
  // Create extension
  base::Value manifest_data = base::test::ParseJson(
      "{"
      "  \"name\": \"Test\","
      "  \"version\": \"1\","
      "  \"manifest_version\": 2,"
      "  \"event_rules\": ["
      "    {"
      "      \"event\": \"declarativeContent.onPageChanged\","
      "      \"actions\": [{"
      "        \"type\": \"declarativeContent.ShowAction\""
      "      }],"
      "      \"conditions\" : [{"
      "        \"css\": [\"video\"]"
      "      }]"
      "    }"
      "  ]"
      "}");
  ManifestData manifest(std::move(manifest_data), "test");
  LoadAndExpectError(manifest, "'type' is required and must be a string");
}

TEST_F(DeclarativeManifestTest, ConditionNotDictionary) {
  // Create extension
  base::Value manifest_data = base::test::ParseJson(
      "{"
      "  \"name\": \"Test\","
      "  \"version\": \"1\","
      "  \"manifest_version\": 2,"
      "  \"event_rules\": ["
      "    {"
      "      \"event\": \"declarativeContent.onPageChanged\","
      "      \"actions\": [{"
      "        \"type\": \"declarativeContent.ShowAction\""
      "      }],"
      "      \"conditions\" : [true]"
      "    }"
      "  ]"
      "}");
  ManifestData manifest(std::move(manifest_data), "test");
  LoadAndExpectError(manifest, "expected dictionary, got boolean");
}

TEST_F(DeclarativeManifestTest, ActionMissingType) {
  // Create extension
  base::Value manifest_data = base::test::ParseJson(
      "{"
      "  \"name\": \"Test\","
      "  \"version\": \"1\","
      "  \"manifest_version\": 2,"
      "  \"event_rules\": ["
      "    {"
      "      \"event\": \"declarativeContent.onPageChanged\","
      "      \"actions\": [{}],"
      "      \"conditions\" : [{"
      "        \"css\": [\"video\"],"
      "        \"type\" : \"declarativeContent.PageStateMatcher\""
      "      }]"
      "    }"
      "  ]"
      "}");
  ManifestData manifest(std::move(manifest_data), "test");
  LoadAndExpectError(manifest, "'type' is required and must be a string");
}

TEST_F(DeclarativeManifestTest, ActionNotDictionary) {
  // Create extension
  base::Value manifest_data = base::test::ParseJson(
      "{"
      "  \"name\": \"Test\","
      "  \"version\": \"1\","
      "  \"manifest_version\": 2,"
      "  \"event_rules\": ["
      "    {"
      "      \"event\": \"declarativeContent.onPageChanged\","
      "      \"actions\": [[]],"
      "      \"conditions\" : [{"
      "        \"css\": [\"video\"],"
      "        \"type\" : \"declarativeContent.PageStateMatcher\""
      "      }]"
      "    }"
      "  ]"
      "}");
  ManifestData manifest(std::move(manifest_data), "test");
  LoadAndExpectError(manifest, "expected dictionary, got list");
}

TEST_F(DeclarativeManifestTest, EventRulesNotList) {
  // Create extension
  base::Value manifest_data = base::test::ParseJson(
      "{"
      "  \"name\": \"Test\","
      "  \"version\": \"1\","
      "  \"manifest_version\": 2,"
      "  \"event_rules\": {}"
      "}");
  ManifestData manifest(std::move(manifest_data), "test");
  LoadAndExpectError(manifest, "'event_rules' expected list, got dictionary");
}

TEST_F(DeclarativeManifestTest, EventRuleNotDictionary) {
  // Create extension
  base::Value manifest_data = base::test::ParseJson(
      "{"
      "  \"name\": \"Test\","
      "  \"version\": \"1\","
      "  \"manifest_version\": 2,"
      "  \"event_rules\": [0,1,2]"
      "}");
  ManifestData manifest(std::move(manifest_data), "test");
  LoadAndExpectError(manifest, "expected dictionary, got integer");
}

TEST_F(DeclarativeManifestTest, EventMissingFromRule) {
  // Create extension
  base::Value manifest_data = base::test::ParseJson(
      "{"
      "  \"name\": \"Test\","
      "  \"version\": \"1\","
      "  \"manifest_version\": 2,"
      "  \"event_rules\": ["
      "    {"
      "      \"actions\": [{"
      "        \"type\": \"declarativeContent.ShowAction\""
      "      }],"
      "      \"conditions\" : [{"
      "        \"css\": [\"video\"],"
      "        \"type\" : \"declarativeContent.PageStateMatcher\""
      "      }]"
      "    }"
      "  ]"
      "}");
  ManifestData manifest(std::move(manifest_data), "test");
  LoadAndExpectError(manifest, "'event' is required");
}

TEST_F(DeclarativeManifestTest, RuleFailedToPopulate) {
  // Create extension
  base::Value manifest_data = base::test::ParseJson(
      "{"
      "  \"name\": \"Test\","
      "  \"version\": \"1\","
      "  \"manifest_version\": 2,"
      "  \"event_rules\": ["
      "    {"
      "      \"event\": \"declarativeContent.onPageChanged\""
      "    }"
      "  ]"
      "}");
  ManifestData manifest(std::move(manifest_data), "test");
  LoadAndExpectError(manifest, "rule failed to populate");
}

}  // namespace extensions
