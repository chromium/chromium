/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/css/page_rule_collector.h"

#include <algorithm>
#include "third_party/blink/renderer/core/css/cascade_layer_map.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

bool PageRuleCollector::IsLeftPage(const ComputedStyle* root_element_style,
                                   uint32_t page_index) const {
  bool is_first_page_left = false;
  DCHECK(root_element_style);
  if (!root_element_style->IsLeftToRightDirection()) {
    is_first_page_left = true;
  }

  return (page_index + (is_first_page_left ? 1 : 0)) % 2;
}

bool PageRuleCollector::IsFirstPage(uint32_t page_index) const {
  // FIXME: In case of forced left/right page, page at index 1 (not 0) can be
  // the first page.
  return (!page_index);
}

PageRuleCollector::PageRuleCollector(const ComputedStyle* root_element_style,
                                     CSSAtRuleID at_rule_id,
                                     uint32_t page_index,
                                     const AtomicString& page_name,
                                     MatchResult& match_result)
    : is_left_page_(IsLeftPage(root_element_style, page_index)),
      is_first_page_(IsFirstPage(page_index)),
      at_rule_id_(at_rule_id),
      page_name_(page_name),
      result_(match_result) {
  DCHECK(at_rule_id_ == CSSAtRuleID::kCSSAtRulePage ||
         (at_rule_id_ >= CSSAtRuleID::kCSSAtRuleTopLeftCorner &&
          at_rule_id_ <= CSSAtRuleID::kCSSAtRuleRightBottom));
}

void PageRuleCollector::MatchPageRules(RuleSet* rules,
                                       CascadeOrigin origin,
                                       TreeScope* tree_scope,
                                       const CascadeLayerMap* layer_map) {
  if (!rules) {
    return;
  }

  rules->CompactRulesIfNeeded();
  HeapVector<Member<StyleRulePage>> matched_page_rules;
  MatchPageRulesForList(matched_page_rules, rules->PageRules());
  if (matched_page_rules.empty()) {
    return;
  }

  std::stable_sort(
      matched_page_rules.begin(), matched_page_rules.end(),
      [layer_map](const StyleRulePage* r1, const StyleRulePage* r2) {
        if (r1->GetCascadeLayer() != r2->GetCascadeLayer()) {
          DCHECK(layer_map);
          return layer_map->CompareLayerOrder(r1->GetCascadeLayer(),
                                              r2->GetCascadeLayer()) < 0;
        }
        return r1->Selector()->Specificity() < r2->Selector()->Specificity();
      });

  if (origin == CascadeOrigin::kAuthor) {
    CHECK(tree_scope);
    result_.BeginAddingAuthorRulesForTreeScope(*tree_scope);
  }

  MatchedProperties::Data options;
  if (RuntimeEnabledFeatures::PageMarginBoxesEnabled()) {
    // See https://drafts.csswg.org/css-page-3/#page-property-list
    options.valid_property_filter =
        static_cast<uint8_t>(ValidPropertyFilter::kPageContext);
  } else {
    // When PageMarginBoxes aren't enabled, we'll only allow the properties and
    // descriptors that have an effect without that feature.
    options.valid_property_filter =
        static_cast<uint8_t>(ValidPropertyFilter::kLimitedPageContext);
  }
  options.origin = origin;

  for (const StyleRulePage* rule : matched_page_rules) {
    if (at_rule_id_ == CSSAtRuleID::kCSSAtRulePage) {
      result_.AddMatchedProperties(&rule->Properties(), options);
    } else {
      for (const auto child_rule : rule->ChildRules()) {
        const auto& margin_rule = To<StyleRulePageMargin>(*child_rule.Get());
        if (margin_rule.ID() == at_rule_id_) {
          result_.AddMatchedProperties(&margin_rule.Properties(), options);
        }
      }
    }
  }
}

static bool CheckPageSelectorComponents(const CSSSelector* selector,
                                        bool is_left_page,
                                        bool is_first_page,
                                        const AtomicString& page_name) {
  for (const CSSSelector* component = selector; component;
       component = component->NextSimpleSelector()) {
    if (component->Match() == CSSSelector::kTag) {
      const AtomicString& local_name = component->TagQName().LocalName();
      DCHECK_NE(local_name, CSSSelector::UniversalSelectorAtom());
      if (local_name != page_name) {
        return false;
      }
    }

    CSSSelector::PseudoType pseudo_type = component->GetPseudoType();
    if ((pseudo_type == CSSSelector::kPseudoLeftPage && !is_left_page) ||
        (pseudo_type == CSSSelector::kPseudoRightPage && is_left_page) ||
        (pseudo_type == CSSSelector::kPseudoFirstPage && !is_first_page)) {
      return false;
    }
  }
  return true;
}

void PageRuleCollector::MatchPageRulesForList(
    HeapVector<Member<StyleRulePage>>& matched_rules,
    const HeapVector<Member<StyleRulePage>>& rules) {
  for (unsigned i = 0; i < rules.size(); ++i) {
    StyleRulePage* rule = rules[i];

    if (!CheckPageSelectorComponents(rule->Selector(), is_left_page_,
                                     is_first_page_, page_name_)) {
      continue;
    }

    // If the rule has no properties to apply, and also no margin rules, then
    // ignore it.
    if (rule->Properties().IsEmpty() && rule->ChildRules().empty()) {
      continue;
    }

    // Add this rule to our list of matched rules.
    matched_rules.push_back(rule);
  }
}

}  // namespace blink
