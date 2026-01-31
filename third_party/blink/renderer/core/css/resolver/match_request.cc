#include "third_party/blink/renderer/core/css/resolver/match_request.h"

#include "third_party/blink/renderer/core/css/element_rule_collector.h"
#include "third_party/blink/renderer/core/css/rule_set.h"

namespace blink {

void RuleSetGroup::AddRuleSet(RuleSet* rule_set) {
  CHECK(!IsFull());
  rule_sets_[num_rule_sets_] = rule_set;

  // Our bit in each of the bitmaps.
  const RuleSetBitmap rule_set_mask = RuleSetBitmap{1} << num_rule_sets_;

  if (rule_set->HasAnyAttrRules()) {
    has_any_attr_rules_ |= rule_set_mask;
    if (rule_set->HasBucketForStyleAttribute()) {
      need_style_synchronized_ = true;
    }
  }
  if (!rule_set->UniversalRules().empty()) {
    has_universal_rules_ |= rule_set_mask;
  }
  if (rule_set->HasAnyInputRules()) {
    has_any_input_rules_ |= rule_set_mask;
  }
  if (!rule_set->LinkPseudoClassRules().empty()) {
    has_link_pseudo_class_rules_ |= rule_set_mask;
  }
  if (!rule_set->OverscrollTargetRules().empty()) {
    has_overscroll_target_pseudo_class_rules_ |= rule_set_mask;
  }
  if (!rule_set->FocusPseudoClassRules().empty()) {
    has_focus_pseudo_class_rules_ |= rule_set_mask;
  }
  if (!rule_set->FocusVisiblePseudoClassRules().empty()) {
    has_focus_visible_pseudo_class_rules_ |= rule_set_mask;
  }

  if (rule_set->SingleScope()) {
    single_scope_ |= rule_set_mask;
  } else {
    not_single_scope_ |= rule_set_mask;
  }

  ++num_rule_sets_;
}

void AddRuleSetToRuleSetGroupList(RuleSet* rule_set,
                                  HeapVector<RuleSetGroup>& rule_set_group) {
  if (rule_set_group.empty() || rule_set_group.back().IsFull()) {
    unsigned rule_set_group_index = rule_set_group.size();
    rule_set_group.emplace_back(RuleSetGroup(rule_set_group_index));
  }
  rule_set_group.back().AddRuleSet(rule_set);
}

}  // namespace blink
