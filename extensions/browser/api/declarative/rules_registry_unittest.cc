// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/rules_registry.h"

#include <algorithm>
#include <utility>

#include "base/run_loop.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative/test_rules_registry.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kExtensionId[] = "foobar";
const char kRuleId[] = "foo";
const int key = extensions::RulesRegistryService::kDefaultRulesRegistryID;
}  // namespace

namespace extensions {

using api_test_utils::ParseDictionary;

TEST(RulesRegistryTest, FillOptionalIdentifiers) {
  content::BrowserTaskEnvironment task_environment;

  std::string error;
  scoped_refptr<RulesRegistry> registry =
      new TestRulesRegistry(content::BrowserThread::UI, "" /*event_name*/, key);

  // Add rules and check that their identifiers are filled and unique.

  {
    std::vector<api::events::Rule> add_rules;
    add_rules.emplace_back();
    add_rules.emplace_back();
    error = registry->AddRules(kExtensionId, std::move(add_rules));
    EXPECT_TRUE(error.empty()) << error;
  }

  std::vector<const api::events::Rule*> get_rules;
  registry->GetAllRules(kExtensionId, &get_rules);

  ASSERT_EQ(2u, get_rules.size());

  ASSERT_TRUE(get_rules[0]->id.get());
  // Make a copy of the id that this rule was assigned so that we can try to
  // reuse it later when the rule is gone.
  std::string id0 = *get_rules[0]->id;
  EXPECT_NE("", id0);

  ASSERT_TRUE(get_rules[1]->id.get());
  EXPECT_NE("", *get_rules[1]->id);

  EXPECT_NE(id0, *get_rules[1]->id);

  EXPECT_EQ(1u /*extensions*/ + 2u /*rules*/,
            registry->GetNumberOfUsedRuleIdentifiersForTesting());

  // Check that we cannot add a new rule with the same ID.

  {
    std::vector<api::events::Rule> add_rules;
    add_rules.emplace_back();
    add_rules[0].id.reset(new std::string(id0));
    error = registry->AddRules(kExtensionId, std::move(add_rules));
    EXPECT_FALSE(error.empty());
  }

  std::vector<const api::events::Rule*> get_rules_2;
  registry->GetAllRules(kExtensionId, &get_rules_2);
  ASSERT_EQ(2u, get_rules_2.size());
  EXPECT_EQ(1u /*extensions*/ + 2u /*rules*/,
            registry->GetNumberOfUsedRuleIdentifiersForTesting());

  // Check that we can register the old rule IDs once they were unregistered.

  std::vector<std::string> remove_rules_3;
  remove_rules_3.push_back(id0);
  error = registry->RemoveRules(kExtensionId, remove_rules_3);
  EXPECT_TRUE(error.empty()) << error;

  EXPECT_EQ(1u /*extensions*/ + 1u /*rules*/,
            registry->GetNumberOfUsedRuleIdentifiersForTesting());

  std::vector<const api::events::Rule*> get_rules_3a;
  registry->GetAllRules(kExtensionId, &get_rules_3a);
  ASSERT_EQ(1u, get_rules_3a.size());

  {
    std::vector<api::events::Rule> add_rules;
    add_rules.emplace_back();
    add_rules[0].id.reset(new std::string(id0));
    error = registry->AddRules(kExtensionId, std::move(add_rules));
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_EQ(1u /*extensions*/ + 2u /*rules*/,
              registry->GetNumberOfUsedRuleIdentifiersForTesting());
  }

  std::vector<const api::events::Rule*> get_rules_3b;
  registry->GetAllRules(kExtensionId, &get_rules_3b);
  ASSERT_EQ(2u, get_rules_3b.size());

  // Check that we can register a rule with an ID that is not modified.

  error = registry->RemoveAllRules(kExtensionId);
  EXPECT_TRUE(error.empty()) << error;
  EXPECT_EQ(0u /*extensions*/ + 0u /*rules*/,
            registry->GetNumberOfUsedRuleIdentifiersForTesting());

  std::vector<const api::events::Rule*> get_rules_4a;
  registry->GetAllRules(kExtensionId, &get_rules_4a);
  ASSERT_TRUE(get_rules_4a.empty());

  {
    std::vector<api::events::Rule> add_rules;
    add_rules.emplace_back();
    add_rules[0].id.reset(new std::string(kRuleId));
    error = registry->AddRules(kExtensionId, std::move(add_rules));
    EXPECT_TRUE(error.empty()) << error;
  }

  EXPECT_EQ(1u /*extensions*/ + 1u /*rules*/,
            registry->GetNumberOfUsedRuleIdentifiersForTesting());

  std::vector<const api::events::Rule*> get_rules_4b;
  registry->GetAllRules(kExtensionId, &get_rules_4b);

  ASSERT_EQ(1u, get_rules_4b.size());

  ASSERT_TRUE(get_rules_4b[0]->id.get());
  EXPECT_EQ(kRuleId, *get_rules_4b[0]->id);

  // Create extension
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test").SetID(kExtensionId).Build();
  registry->OnExtensionUninstalled(extension.get());
  EXPECT_EQ(0u /*extensions*/ + 0u /*rules*/,
            registry->GetNumberOfUsedRuleIdentifiersForTesting());

  // Make sure that deletion traits of registry are executed.
  registry.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(RulesRegistryTest, FillOptionalPriority) {
  content::BrowserTaskEnvironment task_environment;

  std::string error;
  scoped_refptr<RulesRegistry> registry =
      new TestRulesRegistry(content::BrowserThread::UI, "" /*event_name*/, key);

  // Add rules and check that their priorities are filled if they are empty.

  {
    std::vector<api::events::Rule> add_rules;
    add_rules.emplace_back();
    add_rules[0].priority.reset(new int(2));
    add_rules.emplace_back();
    error = registry->AddRules(kExtensionId, std::move(add_rules));
    EXPECT_TRUE(error.empty()) << error;
  }

  std::vector<const api::events::Rule*> get_rules;
  registry->GetAllRules(kExtensionId, &get_rules);

  ASSERT_EQ(2u, get_rules.size());

  ASSERT_TRUE(get_rules[0]->priority.get());
  ASSERT_TRUE(get_rules[1]->priority.get());

  // Verify the precondition so that the following EXPECT_EQ statements work.
  EXPECT_GT(RulesRegistry::DEFAULT_PRIORITY, 2);
  EXPECT_EQ(2, std::min(*get_rules[0]->priority, *get_rules[1]->priority));
  EXPECT_EQ(RulesRegistry::DEFAULT_PRIORITY,
            std::max(*get_rules[0]->priority, *get_rules[1]->priority));

  // Make sure that deletion traits of registry are executed.
  registry.reset();
  base::RunLoop().RunUntilIdle();
}

// Test verifies 2 rules defined in the manifest appear in the registry.
TEST(RulesRegistryTest, TwoRulesInManifest) {
  content::BrowserTaskEnvironment task_environment;

  // Create extension
  std::unique_ptr<base::DictionaryValue> manifest = ParseDictionary(
      "{"
      "  \"name\": \"Test\","
      "  \"version\": \"1\","
      "  \"manifest_version\": 2,"
      "  \"event_rules\": ["
      "    {"
      "      \"id\": \"000\","
      "      \"priority\": 200,"
      "      \"tags\": [\"tagged\"],"
      "      \"event\": \"declarativeContent.onPageChanged\","
      "      \"actions\": [{"
      "        \"type\": \"declarativeContent.ShowAction\""
      "      }],"
      "      \"conditions\" : [{"
      "        \"css\": [\"video\"],"
      "        \"type\" : \"declarativeContent.PageStateMatcher\""
      "      }]"
      "    },"
      "    {"
      "      \"event\": \"declarativeContent.onPageChanged\","
      "      \"actions\": [{"
      "        \"type\": \"declarativeContent.ShowAction\""
      "      }],"
      "      \"conditions\" : [{"
      "        \"css\": [\"input[type='password']\"],"
      "        \"type\" : \"declarativeContent.PageStateMatcher\""
      "      }]"
      "    }"
      "  ]"
      "}");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetID(kExtensionId)
          .Build();

  scoped_refptr<RulesRegistry> registry = new TestRulesRegistry(
      content::BrowserThread::UI, "declarativeContent.onPageChanged", key);
  // Simulate what RulesRegistryService would do on extension load.
  registry->OnExtensionLoaded(extension.get());

  std::vector<const api::events::Rule*> get_rules;
  registry->GetAllRules(kExtensionId, &get_rules);

  ASSERT_EQ(2u, get_rules.size());
  std::unique_ptr<base::DictionaryValue> expected_rule_0 = ParseDictionary(
      "{"
      "  \"id\": \"000\","
      "  \"priority\": 200,"
      "  \"tags\": [\"tagged\"],"
      "  \"actions\": [{"
      "    \"instanceType\": \"declarativeContent.ShowAction\""
      "  }],"
      "  \"conditions\" : [{"
      "    \"css\": [\"video\"],"
      "    \"instanceType\" : \"declarativeContent.PageStateMatcher\""
      "  }]"
      "}");
  EXPECT_TRUE(expected_rule_0->Equals(get_rules[0]->ToValue().get()));

  std::unique_ptr<base::DictionaryValue> expected_rule_1 = ParseDictionary(
      "{"
      "  \"id\": \"_0_\","
      "  \"priority\": 100,"
      "  \"actions\": [{"
      "    \"instanceType\": \"declarativeContent.ShowAction\""
      "  }],"
      "  \"conditions\" : [{"
      "    \"css\": [\"input[type='password']\"],"
      "    \"instanceType\" : \"declarativeContent.PageStateMatcher\""
      "  }]"
      "}");
  EXPECT_TRUE(expected_rule_1->Equals(get_rules[1]->ToValue().get()));
}

// Tests verifies that rules defined in the manifest cannot be deleted but
// programmatically added rules still can be deleted.
TEST(RulesRegistryTest, DeleteRuleInManifest) {
  content::BrowserTaskEnvironment task_environment;

  // Create extension
  std::unique_ptr<base::DictionaryValue> manifest = ParseDictionary(
      "{"
      "  \"name\": \"Test\","
      "  \"version\": \"1\","
      "  \"manifest_version\": 2,"
      "  \"event_rules\": [{"
      "    \"id\":  \"manifest_rule_0\","
      "    \"event\": \"declarativeContent.onPageChanged\","
      "    \"actions\": [{"
      "      \"type\": \"declarativeContent.ShowAction\""
      "    }],"
      "    \"conditions\" : [{"
      "      \"css\": [\"video\"],"
      "      \"type\" : \"declarativeContent.PageStateMatcher\""
      "    }]"
      "  }]"
      "}");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetID(kExtensionId)
          .Build();

  scoped_refptr<RulesRegistry> registry = new TestRulesRegistry(
      content::BrowserThread::UI, "declarativeContent.onPageChanged", key);
  // Simulate what RulesRegistryService would do on extension load.
  registry->OnExtensionLoaded(extension.get());

  {
    // Add some extra rules outside of the manifest.
    std::vector<api::events::Rule> add_rules;
    api::events::Rule rule_1;
    rule_1.id.reset(new std::string("rule_1"));
    api::events::Rule rule_2;
    rule_2.id.reset(new std::string("rule_2"));
    add_rules.push_back(std::move(rule_1));
    add_rules.push_back(std::move(rule_2));
    registry->AddRules(kExtensionId, std::move(add_rules));
  }

  std::vector<const api::events::Rule*> get_rules;
  registry->GetAllRules(kExtensionId, &get_rules);
  ASSERT_EQ(3u, get_rules.size());
  EXPECT_EQ("manifest_rule_0", *(get_rules[0]->id));
  EXPECT_EQ("rule_1", *(get_rules[1]->id));
  EXPECT_EQ("rule_2", *(get_rules[2]->id));

  // Remove a rule from outside the manifest.
  std::vector<std::string> remove_ids;
  remove_ids.push_back("rule_1");
  EXPECT_TRUE(registry->RemoveRules(kExtensionId, remove_ids).empty());
  get_rules.clear();
  registry->GetAllRules(kExtensionId, &get_rules);
  EXPECT_EQ(2u, get_rules.size());
  EXPECT_EQ("manifest_rule_0", *(get_rules[0]->id));
  EXPECT_EQ("rule_2", *(get_rules[1]->id));

  // Attempt to remove rule in manifest.
  remove_ids.clear();
  remove_ids.push_back("manifest_rule_0");
  EXPECT_FALSE(registry->RemoveRules(kExtensionId, remove_ids).empty());
  get_rules.clear();
  registry->GetAllRules(kExtensionId, &get_rules);
  ASSERT_EQ(2u, get_rules.size());
  EXPECT_EQ("manifest_rule_0", *(get_rules[0]->id));
  EXPECT_EQ("rule_2", *(get_rules[1]->id));

  // Remove all rules.
  registry->RemoveAllRules(kExtensionId);
  get_rules.clear();
  registry->GetAllRules(kExtensionId, &get_rules);
  ASSERT_EQ(1u, get_rules.size());
  EXPECT_EQ("manifest_rule_0", *(get_rules[0]->id));
}

}  // namespace extensions
