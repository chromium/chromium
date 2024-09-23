// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_rule_usage_tracker.h"

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

StyleRuleUsageTracker::RuleListByStyleSheet StyleRuleUsageTracker::TakeDelta() {
  RuleListByStyleSheet result;
  result.swap(used_rules_delta_);
  return result;
}

bool StyleRuleUsageTracker::InsertToUsedRulesMap(
    const CSSStyleSheet* parent_sheet,
    const StyleRule* rule) {
  HeapHashSet<Member<const StyleRule>>* set =
      used_rules_
          .insert(parent_sheet,
                  MakeGarbageCollected<HeapHashSet<Member<const StyleRule>>>())
          .stored_value->value;
  return set->insert(rule).is_new_entry;
}

void StyleRuleUsageTracker::Track(const CSSStyleSheet* parent_sheet,
                                  const StyleRule* rule) {
  if (!parent_sheet) {
    return;
  }
  if (!InsertToUsedRulesMap(parent_sheet, rule)) {
    return;
  }
  auto it = used_rules_delta_.find(parent_sheet);
  if (it != used_rules_delta_.end()) {
    it->value->push_back(rule);
  } else {
    used_rules_delta_
        .insert(parent_sheet,
                MakeGarbageCollected<HeapVector<Member<const StyleRule>>>())
        .stored_value->value->push_back(rule);
  }
}

void StyleRuleUsageTracker::Trace(Visitor* visitor) const {
  visitor->Trace(used_rules_);
  visitor->Trace(used_rules_delta_);
}

}  // namespace blink
