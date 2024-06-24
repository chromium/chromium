// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/prefs_helper.h"

#include "components/crx_file/id_util.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions::declarative_net_request {
namespace {

using ::testing::UnorderedElementsAre;

class PrefsHelperTest : public ExtensionsTest {
 public:
  PrefsHelperTest() = default;

  // ExtensionsTest override.
  void SetUp() override {
    ExtensionsTest::SetUp();

    prefs_ = ExtensionPrefs::Get(browser_context());
    ASSERT_TRUE(prefs_);
    extension_id_ = crx_file::id_util::GenerateId("dummy_extension");
  }

  void TearDown() override {
    // Drop unowned ref before destroying owning object in superclass.
    prefs_ = nullptr;
    ExtensionsTest::TearDown();
  }

 protected:
  using RuleIdsToUpdate = PrefsHelper::RuleIdsToUpdate;
  using UpdateDisabledStaticRulesResult =
      PrefsHelper::UpdateDisabledStaticRulesResult;
  using ExtensionId = extensions::ExtensionId;

  UpdateDisabledStaticRulesResult UpdateDisabledStaticRules(
      RulesetID ruleset_id,
      const RuleIdsToUpdate& ids_to_update) {
    return PrefsHelper(*prefs_).UpdateDisabledStaticRules(
        extension_id_, ruleset_id, ids_to_update);
  }

  base::flat_set<int> GetDisabledRuleIds(RulesetID ruleset_id) const {
    return PrefsHelper(*prefs_).GetDisabledStaticRuleIds(
        extension_id_, ruleset_id);
  }

  size_t GetDisabledRuleCount() const {
    return PrefsHelper(*prefs_).GetDisabledStaticRuleCount(extension_id_);
  }

 private:
  raw_ptr<ExtensionPrefs> prefs_;
  ExtensionId extension_id_;
};

TEST_F(PrefsHelperTest, UpdateStaticRulesTest) {
  const RulesetID ruleset1(1);
  const RulesetID ruleset2(2);

  // Set the disabled static rule limit as 7.
  ScopedRuleLimitOverride scoped_disabled_static_rule_limit_override =
      CreateScopedDisabledStaticRuleLimitOverrideForTesting(7);

  // The initial disabled rule ids set is empty.
  EXPECT_THAT(GetDisabledRuleIds(ruleset1), testing::IsEmpty());
  EXPECT_THAT(GetDisabledRuleIds(ruleset2), testing::IsEmpty());
  EXPECT_EQ(0u, GetDisabledRuleCount());

  // Updating disabled rule ids with empty set doesn't make any change.
  {
    RuleIdsToUpdate ids_to_update(std::vector<int>() /* ids_to_disable */,
                                  std::nullopt /* ids_to_enable */);
    EXPECT_TRUE(ids_to_update.Empty());

    auto result = UpdateDisabledStaticRules(ruleset1, ids_to_update);
    EXPECT_FALSE(result.changed);
    EXPECT_THAT(result.disabled_rule_ids_after_update, testing::IsEmpty());
    EXPECT_EQ(std::nullopt, result.error);
  }
  EXPECT_THAT(GetDisabledRuleIds(ruleset1), testing::IsEmpty());
  EXPECT_THAT(GetDisabledRuleIds(ruleset2), testing::IsEmpty());
  EXPECT_EQ(0u, GetDisabledRuleCount());

  // Disable rule 1, rule 2 and rule 3 of ruleset1.
  {
    RuleIdsToUpdate ids_to_update(
        std::vector<int>({1, 2, 3}) /* ids_to_disable */,
        std::nullopt /* ids_to_enable */);
    EXPECT_FALSE(ids_to_update.Empty());

    auto result = UpdateDisabledStaticRules(ruleset1, ids_to_update);
    EXPECT_TRUE(result.changed);
    EXPECT_THAT(result.disabled_rule_ids_after_update,
                UnorderedElementsAre(1, 2, 3));
    EXPECT_EQ(std::nullopt, result.error);
  }
  EXPECT_THAT(GetDisabledRuleIds(ruleset1), UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIds(ruleset2), testing::IsEmpty());
  EXPECT_EQ(3u, GetDisabledRuleCount());

  // Disable rule 3, rule 4 and rule 5 of ruleset2.
  {
    RuleIdsToUpdate ids_to_update(
        std::vector<int>({3, 4, 5}) /* ids_to_disable */,
        std::nullopt /* ids_to_enable */);
    EXPECT_FALSE(ids_to_update.Empty());

    auto result = UpdateDisabledStaticRules(ruleset2, ids_to_update);
    EXPECT_TRUE(result.changed);
    EXPECT_THAT(result.disabled_rule_ids_after_update,
                UnorderedElementsAre(3, 4, 5));
    EXPECT_EQ(std::nullopt, result.error);
  }

  EXPECT_THAT(GetDisabledRuleIds(ruleset1), UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIds(ruleset2), UnorderedElementsAre(3, 4, 5));
  EXPECT_EQ(6u, GetDisabledRuleCount());

  // Updating disabled rule ids with null set doesn't make any change.
  {
    RuleIdsToUpdate ids_to_update(std::nullopt /* ids_to_disable */,
                                  std::nullopt /* ids_to_enable */);
    EXPECT_TRUE(ids_to_update.Empty());

    auto result = UpdateDisabledStaticRules(ruleset2, ids_to_update);
    EXPECT_FALSE(result.changed);
    EXPECT_THAT(result.disabled_rule_ids_after_update, testing::IsEmpty());
    EXPECT_EQ(std::nullopt, result.error);
  }
  EXPECT_THAT(GetDisabledRuleIds(ruleset1), UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIds(ruleset2), UnorderedElementsAre(3, 4, 5));
  EXPECT_EQ(6u, GetDisabledRuleCount());

  // Disable rule 4 and rule 5 of ruleset2 but it doesn't make any change since
  // they are already disabled. Ignore enabling rule 5 since |ids_to_disable|
  // takes priority over |ids_to_enable|.
  {
    RuleIdsToUpdate ids_to_update(std::vector<int>({4, 5}) /* ids_to_disable */,
                                  std::vector<int>({5}) /* ids_to_enable */);
    EXPECT_FALSE(ids_to_update.Empty());

    auto result = UpdateDisabledStaticRules(ruleset2, ids_to_update);
    EXPECT_FALSE(result.changed);
    EXPECT_THAT(result.disabled_rule_ids_after_update, testing::IsEmpty());
    EXPECT_EQ(std::nullopt, result.error);
  }
  EXPECT_THAT(GetDisabledRuleIds(ruleset1), UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIds(ruleset2), UnorderedElementsAre(3, 4, 5));
  EXPECT_EQ(6u, GetDisabledRuleCount());

  // Enable rule 4 and disable rule 5, rule 6 and rule 7 of ruleset2. Ignore
  // enabling rule 5 since |ids_to_disable| takes priority over |ids_to_enable|.
  // Disabling rule 5 doesn't make any change since rule 5 is already disabled.
  {
    RuleIdsToUpdate ids_to_update(
        std::vector<int>({5, 6, 7}) /* ids_to_disable */,
        std::vector<int>({4, 5}) /* ids_to_enable */);
    EXPECT_FALSE(ids_to_update.Empty());

    auto result = UpdateDisabledStaticRules(ruleset2, ids_to_update);
    EXPECT_TRUE(result.changed);
    EXPECT_THAT(result.disabled_rule_ids_after_update,
                UnorderedElementsAre(3, 5, 6, 7));
    EXPECT_EQ(std::nullopt, result.error);
  }
  EXPECT_THAT(GetDisabledRuleIds(ruleset1), UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIds(ruleset2), UnorderedElementsAre(3, 5, 6, 7));
  EXPECT_EQ(7u, GetDisabledRuleCount());

  // Enable rule 1 and disable rule 3, rule 4 and rule 5 of ruleset2. Ignore
  // enabling rule 3 since |ids_to_disable| takes priority over |ids_to_enable|.
  // This operation fails since it exceeds the disabled static rule count limit.
  {
    RuleIdsToUpdate ids_to_update(
        std::vector<int>({3, 4, 5}) /* ids_to_disable */,
        std::vector<int>({1, 3}) /* ids_to_enable */);
    EXPECT_FALSE(ids_to_update.Empty());

    auto result = UpdateDisabledStaticRules(ruleset1, ids_to_update);
    EXPECT_FALSE(result.changed);
    EXPECT_THAT(result.disabled_rule_ids_after_update, testing::IsEmpty());
    EXPECT_EQ(kDisabledStaticRuleCountExceeded, result.error);
  }
  EXPECT_THAT(GetDisabledRuleIds(ruleset1), UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIds(ruleset2), UnorderedElementsAre(3, 5, 6, 7));
  EXPECT_EQ(7u, GetDisabledRuleCount());

  // Enable rule 1, rule 2 rule 3 and rule 4 of ruleset1. Enabling rule 4
  // doesn't make any change since rule 4 is not disabled.
  {
    RuleIdsToUpdate ids_to_update(
        std::nullopt /* ids_to_disable */,
        std::vector<int>({1, 2, 3, 4}) /* ids_to_enable */);
    EXPECT_FALSE(ids_to_update.Empty());

    auto result = UpdateDisabledStaticRules(ruleset1, ids_to_update);
    EXPECT_TRUE(result.changed);
    EXPECT_THAT(result.disabled_rule_ids_after_update, testing::IsEmpty());
    EXPECT_EQ(std::nullopt, result.error);
  }
  EXPECT_THAT(GetDisabledRuleIds(ruleset1), testing::IsEmpty());
  EXPECT_THAT(GetDisabledRuleIds(ruleset2), UnorderedElementsAre(3, 5, 6, 7));
  EXPECT_EQ(4u, GetDisabledRuleCount());

  // Enable rule 3, rule 4, rule 5, rule 6 and rule 7 of ruleset2. Enabling
  // rule 4 doesn't make any change since rule 4 is not disabled.
  {
    RuleIdsToUpdate ids_to_update(
        std::nullopt /* ids_to_disable */,
        std::vector<int>({3, 4, 5, 6, 7}) /* ids_to_enable */);
    EXPECT_FALSE(ids_to_update.Empty());

    auto result = UpdateDisabledStaticRules(ruleset2, ids_to_update);
    EXPECT_TRUE(result.changed);
    EXPECT_THAT(result.disabled_rule_ids_after_update, testing::IsEmpty());
    EXPECT_EQ(std::nullopt, result.error);
  }
  EXPECT_THAT(GetDisabledRuleIds(ruleset1), testing::IsEmpty());
  EXPECT_THAT(GetDisabledRuleIds(ruleset2), testing::IsEmpty());
  EXPECT_EQ(0u, GetDisabledRuleCount());

  // Enable rule 3, rule 4 and rule 5 of ruleset2. It doesn't make any change
  // since there is no disabled rules.
  {
    RuleIdsToUpdate ids_to_update(
        std::nullopt /* ids_to_disable */,
        std::vector<int>({3, 4, 5}) /* ids_to_enable */);
    EXPECT_FALSE(ids_to_update.Empty());

    auto result = UpdateDisabledStaticRules(ruleset2, ids_to_update);
    EXPECT_FALSE(result.changed);
    EXPECT_THAT(result.disabled_rule_ids_after_update, testing::IsEmpty());
    EXPECT_EQ(std::nullopt, result.error);
  }
  EXPECT_THAT(GetDisabledRuleIds(ruleset1), testing::IsEmpty());
  EXPECT_THAT(GetDisabledRuleIds(ruleset2), testing::IsEmpty());
  EXPECT_EQ(0u, GetDisabledRuleCount());
}

}  // namespace
}  // namespace extensions::declarative_net_request
