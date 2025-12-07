// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/rule_set_diff.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

#if DCHECK_IS_ON()

TEST(RuleSetDiffTest, AllRules) {
  test::TaskEnvironment task_environment;

  css_test_helpers::TestStyleSheet old_sheet;
  old_sheet.AddCSSRules(".a {}");
  RuleSet& old_rule_set = old_sheet.GetRuleSet();

  css_test_helpers::TestStyleSheet new_sheet;
  new_sheet.AddCSSRules(".b {}");
  RuleSet& new_rule_set = new_sheet.GetRuleSet();

  auto* rule_set_diff = MakeGarbageCollected<RuleSetDiff>(&old_rule_set);
  base::span<const RuleData> class_rules =
      old_rule_set.ClassRules(AtomicString("a"));
  ASSERT_EQ(1u, class_rules.size());
  rule_set_diff->AddDiff(class_rules.front().Rule());
  rule_set_diff->NewRuleSetCreated(&new_rule_set);

  RuleSet* diff_ruleset = rule_set_diff->CreateDiffRuleset();
  ASSERT_TRUE(diff_ruleset);
  EXPECT_EQ(1u, diff_ruleset->AllRulesForTest().size());
}

#endif  // DCHECK_IS_ON()

}  // namespace blink
