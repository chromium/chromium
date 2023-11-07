// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/rule_set_diff.h"

#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

void RuleSetDiff::AddDiff(StyleRuleBase* rule) {
  DCHECK(!HasNewRuleSet());
  DCHECK(rule);
  if (unrepresentable_) {
    return;
  }

  if (!IsA<StyleRule>(rule)) {
    MarkUnrepresentable();
  } else {
    changed_rules_.insert(To<StyleRule>(rule));
  }
}

RuleSet* RuleSetDiff::CreateDiffRuleset() const {
  if (unrepresentable_) {
    return nullptr;
  }

  if (old_ruleset_->RuleCount() + new_ruleset_->RuleCount() >=
      (1 << RuleData::kPositionBits)) {
    return nullptr;
  }

  RuleSet* ruleset = MakeGarbageCollected<RuleSet>();
  ruleset->AddFilteredRulesFromOtherSet(*old_ruleset_, changed_rules_);
  ruleset->AddFilteredRulesFromOtherSet(*new_ruleset_, changed_rules_);
  return ruleset;
}

}  // namespace blink
