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

#include "third_party/blink/renderer/bindings/core/v8/v8_observable_array_css_style_sheet.h"
#include "third_party/blink/renderer/core/css/active_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/rule_set_diff.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_sheet_candidate.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"

namespace blink {

static void CreateRuleSets(const StyleEngine& engine,
                           const MixinMap& effective_mixins,
                           ActiveStyleSheetVector& active_style_sheets,
                           HeapVector<Member<RuleSetDiff>>& rule_set_diffs);

void StyleSheetCollection::FinishUpdateActiveStyleSheets(
    const MixinMap& effective_mixins) {
  HeapVector<Member<RuleSetDiff>> rule_set_diffs;
  CreateRuleSets(GetDocument().GetStyleEngine(), effective_mixins,
                 pending_active_style_sheets_, rule_set_diffs);

  // We need to clear this before ApplyRuleSetChanges(),
  // as the inspector may call PrepareUpdateActiveStyleSheets()
  // synchronously.

  ActiveStyleSheetVector old_active_style_sheets =
      std::move(active_style_sheets_);
  active_style_sheets_ = std::move(pending_active_style_sheets_);
  pending_active_style_sheets_.clear();

  GetDocument().GetStyleEngine().ApplyRuleSetChanges(
      *tree_scope_, old_active_style_sheets, active_style_sheets_,
      rule_set_diffs);
}

// Creates RuleSets for everything in active_style_sheets.
// This is done as a separate pass, because we do not know what mixins
// we have (which is required to create RuleSets) before we've seen
// all stylesheets.
//
// Can only be called once.
static void CreateRuleSets(const StyleEngine& engine,
                           const MixinMap& effective_mixins,
                           ActiveStyleSheetVector& active_style_sheets,
                           HeapVector<Member<RuleSetDiff>>& rule_set_diffs) {
  // It's possible to add the same StyleSheetContents more than once,
  // either due to StyleSheetContents being shared between multiple
  // CSSStyleSheets (see IsContentsShared()), or due to the same CSSStyleSheet
  // being adopted more than once. When this happens, we may need to create
  // multiple RuleSet objects for the same contents, because anonymous
  // @layers must be unique.
  HeapHashSet<Member<StyleSheetContents>> seen_contents;

  for (auto& [css_sheet, rule_set] : active_style_sheets) {
    CHECK_EQ(rule_set, nullptr);

    StyleSheetContents* contents = css_sheet->Contents();

    if (!seen_contents.insert(contents).is_new_entry &&
        contents->HasRuleSet() && contents->GetRuleSet().HasCascadeLayers()) {
      // We've already seen this StyleSheetContents, but we cannot simply
      // add its cached RuleSet again; it would cause distinct anonymous
      // layers to be misidentified as the same layer.
      rule_set = engine.CreateUnconnectedRuleSet(*css_sheet, effective_mixins);
    } else {
      rule_set = engine.RuleSetForSheet(*css_sheet, effective_mixins);
    }

    if (contents->GetRuleSetDiff()) {
      rule_set_diffs.push_back(contents->GetRuleSetDiff());
      contents->ClearRuleSetDiff();
    }
  }
}

void StyleSheetCollection::Trace(Visitor* visitor) const {
  visitor->Trace(active_style_sheets_);
  visitor->Trace(pending_active_style_sheets_);
  visitor->Trace(style_sheets_for_style_sheet_list_);
  visitor->Trace(tree_scope_);
  visitor->Trace(style_sheet_candidate_nodes_);
  visitor->Trace(mixins_);
}

StyleSheetCollection::StyleSheetCollection(TreeScope& tree_scope)
    : tree_scope_(tree_scope), is_shadow_tree_(IsA<ShadowRoot>(tree_scope)) {
  if (is_shadow_tree_) {
    DCHECK_NE(tree_scope.RootNode(), tree_scope.RootNode().GetDocument());
  } else {
    DCHECK_EQ(tree_scope.RootNode(), tree_scope.RootNode().GetDocument());
  }
}

void StyleSheetCollection::AddStyleSheetCandidateNode(Node& node) {
  if (node.isConnected()) {
    style_sheet_candidate_nodes_.Add(&node);
  }
}

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

void StyleSheetCollection::PrepareUpdateActiveStyleSheets(
    const MediaQueryEvaluator& medium) {
  ActiveStyleSheetVector new_active_style_sheets;
  const String& preferred_name =
      is_shadow_tree_
          ? g_null_atom
          : GetDocument().GetStyleEngine().PreferredStylesheetSetName();

  if (!is_shadow_tree_) {
    for (auto& sheet :
         GetDocument().GetStyleEngine().InjectedAuthorStyleSheets()) {
      new_active_style_sheets.push_back(std::pair(sheet.second, nullptr));
    }
  }

  for (Node* n : style_sheet_candidate_nodes_) {
    StyleSheetCandidate candidate(*n);

    DCHECK(!candidate.IsXSL());
    if (candidate.IsEnabledAndLoading()) {
      continue;
    }

    StyleSheet* sheet = candidate.Sheet();
    if (sheet && candidate.CanBeActivated(preferred_name)) {
      CSSStyleSheet* css_sheet = To<CSSStyleSheet>(sheet);
      new_active_style_sheets.push_back(std::pair(css_sheet, nullptr));
    }
  }

  if (tree_scope_->HasAdoptedStyleSheets()) {
    for (CSSStyleSheet* sheet : *tree_scope_->AdoptedStyleSheets()) {
      if (sheet && sheet->CanBeActivated(preferred_name)) {
        DCHECK_EQ(GetDocument(), sheet->ConstructorDocument());
        new_active_style_sheets.push_back(std::pair(sheet, nullptr));
      }
    }
  }

  if (!is_shadow_tree_) {
    for (CSSStyleSheet* inspector_sheet :
         GetDocument().GetStyleEngine().InspectorStyleSheets()) {
      new_active_style_sheets.push_back(std::pair(inspector_sheet, nullptr));
    }
  }

  mixins_ = MixinMap();
  for (auto& [css_sheet, rule_set] : new_active_style_sheets) {
    mixins_.Merge(
        css_sheet->Contents()->ExtractMixins(medium, mixin_generation_));
  }
  mixins_.generation = mixin_generation_;

  DCHECK(pending_active_style_sheets_.empty());
  pending_active_style_sheets_ = std::move(new_active_style_sheets);
}

}  // namespace blink
