// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/rule_set_diff.h"

#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "v8/include/cppgc/garbage-collected.h"

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
    AddRules(rule);
  }
}

// When some style rule has changed, we need to add any child rules as well,
// since the '&' selector can refer to a parent rule.
void RuleSetDiff::AddRules(StyleRuleBase* rule) {
  if (auto* style_rule = DynamicTo<StyleRule>(rule)) {
    changed_rules_.insert(style_rule);
    if (GCedHeapVector<Member<StyleRuleBase>>* child_rules =
            style_rule->ChildRules()) {
      for (StyleRuleBase* child_rule : *child_rules) {
        AddRules(child_rule);
      }
    }
  } else if (auto* group_rule = DynamicTo<StyleRuleGroup>(rule)) {
    for (StyleRuleBase* child_rule : group_rule->ChildRules()) {
      AddRules(child_rule);
    }
  }
  // Note that StyleRuleNestedDeclarations holds an inner StyleRule
  // which is not reachable by the two branches above. However, adding
  // that rule here would be redundant since its selector list is always
  // a copy of the parent rule's selector list.
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
  ruleset->CompactRulesIfNeeded();
  return ruleset;
}

void RuleSetDiff::Trace(Visitor* visitor) const {
  visitor->Trace(old_ruleset_);
  visitor->Trace(new_ruleset_);
  visitor->Trace(changed_rules_);
}

}  // namespace blink
