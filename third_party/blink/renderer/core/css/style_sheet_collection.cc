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

#include "third_party/blink/renderer/core/css/active_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/rule_set_diff.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_sheet_candidate.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"

namespace blink {

static void CreateRuleSets(const StyleEngine& engine,
                           const MediaQueryEvaluator& medium,
                           ActiveStyleSheetVector& active_style_sheets,
                           HeapVector<Member<RuleSetDiff>>& rule_set_diffs);

void StyleSheetCollection::ReplaceActiveStyleSheets(
    const MediaQueryEvaluator& medium,
    ActiveStyleSheetVector new_active_style_sheets,
    HeapVector<Member<StyleSheet>> new_style_sheets_for_style_sheet_list) {
  HeapVector<Member<RuleSetDiff>> rule_set_diffs;
  CreateRuleSets(GetDocument().GetStyleEngine(), medium,
                 new_active_style_sheets, rule_set_diffs);

  GetDocument().GetStyleEngine().ApplyRuleSetChanges(
      GetTreeScope(), active_style_sheets_, new_active_style_sheets,
      rule_set_diffs);

  active_style_sheets_ = std::move(new_active_style_sheets);
  style_sheets_for_style_sheet_list_ =
      std::move(new_style_sheets_for_style_sheet_list);
  sheet_list_dirty_ = false;
}

// FIXME(sesse): Store this somewhere (including the two-level Eval() form),
// so that we know when we need to invalidate.
static bool MatchMediaForMixins(const MediaQueryEvaluator& evaluator,
                                const MediaQuerySet* media_queries) {
  if (!media_queries) {
    return true;
  }
  return evaluator.Eval(*media_queries);
}

static void ExtractMixinsFromRules(
    base::span<const Member<StyleRuleBase>> rules,
    const MediaQueryEvaluator& medium,
    MixinMap& mixins) {
  for (StyleRuleBase* rule : rules) {
    // TODO(sesse): @container, @layer, @scope, @starting-style are waiting for
    // a resolution in https://github.com/w3c/csswg-drafts/issues/12417.
    if (auto* media_rule = DynamicTo<StyleRuleMedia>(rule)) {
      if (MatchMediaForMixins(medium, media_rule->MediaQueries())) {
        ExtractMixinsFromRules(media_rule->ChildRules(), medium, mixins);
      }
    } else if (auto* supports_rule = DynamicTo<StyleRuleSupports>(rule)) {
      if (supports_rule->ConditionIsSupported()) {
        ExtractMixinsFromRules(supports_rule->ChildRules(), medium, mixins);
      }
    } else if (auto* mixin_rule = DynamicTo<StyleRuleMixin>(rule)) {
      mixins.insert(mixin_rule->GetName(), mixin_rule);
    }
  }
}

// Creates RuleSets for everything in active_style_sheets.
// This is done as a separate pass, because we do not know what mixins
// we have (which is required to create RuleSets) before we've seen
// all stylesheets.
//
// Can only be called once.
static void CreateRuleSets(const StyleEngine& engine,
                           const MediaQueryEvaluator& medium,
                           ActiveStyleSheetVector& active_style_sheets,
                           HeapVector<Member<RuleSetDiff>>& rule_set_diffs) {
  MixinMap mixins;
  for (auto& [css_sheet, rule_set] : active_style_sheets) {
    ExtractMixinsFromRules(css_sheet->Contents()->ChildRules(), medium, mixins);
  }

  // Keep track of ensured RuleSets with @layer rules to detect
  // StyleSheetContents sharing; RuleSets should not be shared
  // between two equal sheets with @layer rules, since anonymous
  // layers need to be unique.
  HeapHashSet<Member<const RuleSet>> layer_rule_sets;

  for (auto& [css_sheet, rule_set] : active_style_sheets) {
    CHECK_EQ(rule_set, nullptr);
    rule_set = engine.RuleSetForSheet(*css_sheet, mixins);

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
        !layer_rule_sets.insert(rule_set).is_new_entry) {
      // The condition above is met for a stylesheet with cascade layers which
      // shares StyleSheetContents with another stylesheet in this TreeScope.
      // WillMutateRules() creates a unique StyleSheetContents for this sheet to
      // avoid incorrectly identifying two separate anonymous layers as the same
      // layer.
      //
      // TODO(sesse): Can we detect this before creating the RuleSet?
      css_sheet->WillMutateRules();
      rule_set = engine.RuleSetForSheet(*css_sheet, mixins);
    }

    if (css_sheet->Contents()->GetRuleSetDiff()) {
      rule_set_diffs.push_back(css_sheet->Contents()->GetRuleSetDiff());
      css_sheet->Contents()->ClearRuleSetDiff();
    }
  }
}

void StyleSheetCollection::Trace(Visitor* visitor) const {
  visitor->Trace(active_style_sheets_);
  visitor->Trace(style_sheets_for_style_sheet_list_);
  visitor->Trace(tree_scope_);
  visitor->Trace(style_sheet_candidate_nodes_);
}

StyleSheetCollection::StyleSheetCollection(TreeScope& tree_scope)
    : tree_scope_(tree_scope) {}

void StyleSheetCollection::AddStyleSheetCandidateNode(Node& node) {
  if (node.isConnected()) {
    style_sheet_candidate_nodes_.Add(&node);
  }
}

// FIXME(sesse): This overwrites the list from UpdateActiveStyleSheets()
// with a different one (it doesn't e.g. include adopted style sheets,
// but DocumentStyleSheetCollection::UpdateActiveStyleSheets() does);
// do we really want this? Why do we have a separate function for this
// at all; cannot one call the other?
void StyleSheetCollection::UpdateStyleSheetList() {
  if (!sheet_list_dirty_) {
    return;
  }

  HeapVector<Member<StyleSheet>> new_list;
  for (Node* node : style_sheet_candidate_nodes_) {
    StyleSheetCandidate candidate(*node);
    DCHECK(!candidate.IsXSL());
    if (candidate.IsEnabledAndLoading()) {
      continue;
    }
    if (StyleSheet* sheet = candidate.Sheet()) {
      new_list.push_back(sheet);
    }
  }

  style_sheets_for_style_sheet_list_ = std::move(new_list);
  sheet_list_dirty_ = false;
}

}  // namespace blink
