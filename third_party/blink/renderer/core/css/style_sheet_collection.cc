/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/style_sheet_collection.h"

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/rule_set_diff.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"

namespace blink {

StyleSheetCollection::StyleSheetCollection() = default;

void StyleSheetCollection::Dispose() {
  style_sheets_for_style_sheet_list_.clear();
  active_style_sheets_.clear();
}

void StyleSheetCollection::Swap(StyleSheetCollection& other) {
  swap(style_sheets_for_style_sheet_list_,
       other.style_sheets_for_style_sheet_list_);
  active_style_sheets_.swap(other.active_style_sheets_);
  sheet_list_dirty_ = false;
}

void StyleSheetCollection::SwapSheetsForSheetList(
    HeapVector<Member<StyleSheet>>& sheets) {
  swap(style_sheets_for_style_sheet_list_, sheets);
  sheet_list_dirty_ = false;
}

void StyleSheetCollection::AppendActiveStyleSheet(CSSStyleSheet* sheet) {
  active_style_sheets_.push_back(std::pair(sheet, nullptr));
}

void StyleSheetCollection::CreateRuleSets(StyleEngine& engine) {
  // Keep track of ensured RuleSets with @layer rules to detect
  // StyleSheetContents sharing; RuleSets should not be shared
  // between two equal sheets with @layer rules, since anonymous
  // layers need to be unique.
  HeapHashSet<Member<const RuleSet>> layer_rule_sets_;

  for (auto& [css_sheet, rule_set] : active_style_sheets_) {
    CHECK_EQ(rule_set, nullptr);
    rule_set = engine.RuleSetForSheet(*css_sheet);

    // NOTE: If the user has specified the same CSSStyleSheet object multiple
    // times (which is only possible for constructible stylesheets, in
    // adoptedStyleSheets), then we will not deduplicate them here
    // (HasSingleOwnerNode() returns false, because the StyleSheetContents is
    // indeed owned by only one CSSStyleSheet; we just send in that
    // CSSStyleSheet twice). This means we could get confusing layer ordering if
    // there were other stylesheets with anonymous layers between the
    // duplicates.
    //
    // It is possible that we should change this; our current behavior differs
    // from both Gecko and WebKit. It does not appear to be clear from the
    // standard, though.
    if (rule_set && rule_set->HasCascadeLayers() &&
        !css_sheet->Contents()->HasSingleOwnerNode() &&
        !layer_rule_sets_.insert(rule_set).is_new_entry) {
      // The condition above is met for a stylesheet with cascade layers which
      // shares StyleSheetContents with another stylesheet in this TreeScope.
      // WillMutateRules() creates a unique StyleSheetContents for this sheet to
      // avoid incorrectly identifying two separate anonymous layers as the same
      // layer.
      //
      // TODO(sesse): Can we detect this before creating the RuleSet?
      css_sheet->WillMutateRules();
      rule_set = engine.RuleSetForSheet(*css_sheet);
    }

    if (css_sheet->Contents()->GetRuleSetDiff()) {
      AppendRuleSetDiff(css_sheet->Contents()->GetRuleSetDiff());
      css_sheet->Contents()->ClearRuleSetDiff();
    }
  }
}

void StyleSheetCollection::AppendSheetForList(StyleSheet* sheet) {
  style_sheets_for_style_sheet_list_.push_back(sheet);
}

void StyleSheetCollection::AppendRuleSetDiff(Member<RuleSetDiff> diff) {
  rule_set_diffs_.push_back(diff);
}

void StyleSheetCollection::Trace(Visitor* visitor) const {
  visitor->Trace(active_style_sheets_);
  visitor->Trace(style_sheets_for_style_sheet_list_);
  visitor->Trace(rule_set_diffs_);
}

}  // namespace blink
