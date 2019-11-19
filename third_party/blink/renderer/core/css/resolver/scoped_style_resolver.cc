/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"

#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/page_rule_collector.h"
#include "third_party/blink/renderer/core/css/part_names.h"
#include "third_party/blink/renderer/core/css/resolver/match_request.h"
#include "third_party/blink/renderer/core/css/rule_feature_set.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg/svg_style_element.h"

namespace blink {

ScopedStyleResolver* ScopedStyleResolver::Parent() const {
  for (TreeScope* scope = GetTreeScope().ParentTreeScope(); scope;
       scope = scope->ParentTreeScope()) {
    if (ScopedStyleResolver* resolver = scope->GetScopedStyleResolver())
      return resolver;
  }
  return nullptr;
}

void ScopedStyleResolver::AddKeyframeRules(const RuleSet& rule_set) {
  const HeapVector<Member<StyleRuleKeyframes>> keyframes_rules =
      rule_set.KeyframesRules();
  for (auto rule : keyframes_rules)
    AddKeyframeStyle(rule);
}

void ScopedStyleResolver::AddFontFaceRules(const RuleSet& rule_set) {
  // FIXME(BUG 72461): We don't add @font-face rules of scoped style sheets for
  // the moment.
  if (!GetTreeScope().RootNode().IsDocumentNode())
    return;

  Document& document = GetTreeScope().GetDocument();
  CSSFontSelector* css_font_selector =
      document.GetStyleEngine().GetFontSelector();
  const HeapVector<Member<StyleRuleFontFace>> font_face_rules =
      rule_set.FontFaceRules();
  for (auto& font_face_rule : font_face_rules) {
    if (FontFace* font_face = FontFace::Create(&document, font_face_rule))
      css_font_selector->GetFontFaceCache()->Add(font_face_rule, font_face);
  }
  if (font_face_rules.size() && document.GetStyleResolver())
    document.GetStyleResolver()->InvalidateMatchedPropertiesCache();
}

void ScopedStyleResolver::AppendActiveStyleSheets(
    unsigned index,
    const ActiveStyleSheetVector& active_sheets) {
  for (auto* active_iterator = active_sheets.begin() + index;
       active_iterator != active_sheets.end(); active_iterator++) {
    CSSStyleSheet* sheet = active_iterator->first;
    viewport_dependent_media_query_results_.AppendVector(
        sheet->ViewportDependentMediaQueryResults());
    device_dependent_media_query_results_.AppendVector(
        sheet->DeviceDependentMediaQueryResults());
    if (!active_iterator->second)
      continue;
    const RuleSet& rule_set = *active_iterator->second;
    author_style_sheets_.push_back(sheet);
    AddKeyframeRules(rule_set);
    AddFontFaceRules(rule_set);
    AddTreeBoundaryCrossingRules(rule_set, sheet, index);
    AddSlottedRules(rule_set, sheet, index++);
  }
}

void ScopedStyleResolver::CollectFeaturesTo(
    RuleFeatureSet& features,
    HeapHashSet<Member<const StyleSheetContents>>&
        visited_shared_style_sheet_contents) const {
  features.ViewportDependentMediaQueryResults().AppendVector(
      viewport_dependent_media_query_results_);
  features.DeviceDependentMediaQueryResults().AppendVector(
      device_dependent_media_query_results_);

  for (auto sheet : author_style_sheets_) {
    DCHECK(sheet->ownerNode() || sheet->IsConstructed());
    StyleSheetContents* contents = sheet->Contents();
    if (contents->HasOneClient() ||
        visited_shared_style_sheet_contents.insert(contents).is_new_entry)
      features.Add(contents->GetRuleSet().Features());
  }

  if (tree_boundary_crossing_rule_set_) {
    for (const auto& rules : *tree_boundary_crossing_rule_set_)
      features.Add(rules->rule_set_->Features());
  }
  if (slotted_rule_set_) {
    for (const auto& rules : *slotted_rule_set_)
      features.Add(rules->rule_set_->Features());
  }
}

void ScopedStyleResolver::ResetAuthorStyle() {
  author_style_sheets_.clear();
  viewport_dependent_media_query_results_.clear();
  device_dependent_media_query_results_.clear();
  keyframes_rule_map_.clear();
  tree_boundary_crossing_rule_set_ = nullptr;
  slotted_rule_set_ = nullptr;
  has_deep_or_shadow_selector_ = false;
  needs_append_all_sheets_ = false;
}

StyleRuleKeyframes* ScopedStyleResolver::KeyframeStylesForAnimation(
    const StringImpl* animation_name) {
  if (keyframes_rule_map_.IsEmpty())
    return nullptr;

  KeyframesRuleMap::iterator it = keyframes_rule_map_.find(animation_name);
  if (it == keyframes_rule_map_.end())
    return nullptr;

  return it->value.Get();
}

void ScopedStyleResolver::AddKeyframeStyle(StyleRuleKeyframes* rule) {
  AtomicString s(rule->GetName());

  if (rule->IsVendorPrefixed()) {
    KeyframesRuleMap::iterator it = keyframes_rule_map_.find(s.Impl());
    if (it == keyframes_rule_map_.end())
      keyframes_rule_map_.Set(s.Impl(), rule);
    else if (it->value->IsVendorPrefixed())
      keyframes_rule_map_.Set(s.Impl(), rule);
  } else {
    keyframes_rule_map_.Set(s.Impl(), rule);
  }
}

Element& ScopedStyleResolver::InvalidationRootForTreeScope(
    const TreeScope& tree_scope) {
  DCHECK(tree_scope.GetDocument().documentElement());
  if (tree_scope.RootNode() == tree_scope.GetDocument())
    return *tree_scope.GetDocument().documentElement();
  return To<ShadowRoot>(tree_scope.RootNode()).host();
}

void ScopedStyleResolver::KeyframesRulesAdded(const TreeScope& tree_scope) {
  // Called when @keyframes rules are about to be added/removed from a
  // TreeScope. @keyframes rules may apply to animations on elements in the
  // same TreeScope as the stylesheet, or the host element in the parent
  // TreeScope if the TreeScope is a shadow tree.
  if (!tree_scope.GetDocument().documentElement())
    return;

  ScopedStyleResolver* resolver = tree_scope.GetScopedStyleResolver();
  ScopedStyleResolver* parent_resolver =
      tree_scope.ParentTreeScope()
          ? tree_scope.ParentTreeScope()->GetScopedStyleResolver()
          : nullptr;

  bool had_unresolved_keyframes = false;
  if (resolver && resolver->has_unresolved_keyframes_rule_) {
    resolver->has_unresolved_keyframes_rule_ = false;
    had_unresolved_keyframes = true;
  }
  if (parent_resolver && parent_resolver->has_unresolved_keyframes_rule_) {
    parent_resolver->has_unresolved_keyframes_rule_ = false;
    had_unresolved_keyframes = true;
  }

  if (had_unresolved_keyframes) {
    // If an animation ended up not being started because no @keyframes
    // rules were found for the animation-name, we need to recalculate style
    // for the elements in the scope, including its shadow host if
    // applicable.
    InvalidationRootForTreeScope(tree_scope)
        .SetNeedsStyleRecalc(kSubtreeStyleChange,
                             StyleChangeReasonForTracing::Create(
                                 style_change_reason::kStyleSheetChange));
    return;
  }

  // If we have animations running, added/removed @keyframes may affect these.
  tree_scope.GetDocument().Timeline().InvalidateKeyframeEffects(tree_scope);
}

void ScopedStyleResolver::CollectMatchingAuthorRules(
    ElementRuleCollector& collector,
    ShadowV0CascadeOrder cascade_order) {
  wtf_size_t sheet_index = 0;
  for (auto sheet : author_style_sheets_) {
    DCHECK(sheet->ownerNode() || sheet->IsConstructed());
    MatchRequest match_request(&sheet->Contents()->GetRuleSet(),
                               &scope_->RootNode(), sheet, sheet_index++);
    collector.CollectMatchingRules(match_request, cascade_order);
  }
}

void ScopedStyleResolver::CollectMatchingShadowHostRules(
    ElementRuleCollector& collector,
    ShadowV0CascadeOrder cascade_order) {
  wtf_size_t sheet_index = 0;
  for (auto sheet : author_style_sheets_) {
    DCHECK(sheet->ownerNode() || sheet->IsConstructed());
    MatchRequest match_request(&sheet->Contents()->GetRuleSet(),
                               &scope_->RootNode(), sheet, sheet_index++);
    collector.CollectMatchingShadowHostRules(match_request, cascade_order);
  }
}

void ScopedStyleResolver::CollectMatchingSlottedRules(
    ElementRuleCollector& collector,
    ShadowV0CascadeOrder cascade_order) {
  if (!slotted_rule_set_)
    return;

  for (const auto& rules : *slotted_rule_set_) {
    MatchRequest request(rules->rule_set_.Get(), &GetTreeScope().RootNode(),
                         rules->parent_style_sheet_, rules->parent_index_);
    collector.CollectMatchingRules(request, cascade_order, true);
  }
}

void ScopedStyleResolver::CollectMatchingTreeBoundaryCrossingRules(
    ElementRuleCollector& collector,
    ShadowV0CascadeOrder cascade_order) {
  if (!tree_boundary_crossing_rule_set_)
    return;

  for (const auto& rules : *tree_boundary_crossing_rule_set_) {
    MatchRequest request(rules->rule_set_.Get(), &GetTreeScope().RootNode(),
                         rules->parent_style_sheet_, rules->parent_index_);
    collector.CollectMatchingRules(request, cascade_order, true);
  }
}

void ScopedStyleResolver::CollectMatchingPartPseudoRules(
    ElementRuleCollector& collector,
    PartNames& part_names,
    ShadowV0CascadeOrder cascade_order) {
  wtf_size_t sheet_index = 0;
  for (auto sheet : author_style_sheets_) {
    DCHECK(sheet->ownerNode() || sheet->IsConstructed());
    MatchRequest match_request(&sheet->Contents()->GetRuleSet(),
                               &scope_->RootNode(), sheet, sheet_index++);
    collector.CollectMatchingPartPseudoRules(match_request, part_names,
                                             cascade_order);
  }
}

void ScopedStyleResolver::MatchPageRules(PageRuleCollector& collector) {
  // Only consider the global author RuleSet for @page rules, as per the HTML5
  // spec.
  DCHECK(scope_->RootNode().IsDocumentNode());
  for (auto sheet : author_style_sheets_)
    collector.MatchPageRules(&sheet->Contents()->GetRuleSet());
}

void ScopedStyleResolver::Trace(blink::Visitor* visitor) {
  visitor->Trace(scope_);
  visitor->Trace(author_style_sheets_);
  visitor->Trace(keyframes_rule_map_);
  visitor->Trace(tree_boundary_crossing_rule_set_);
  visitor->Trace(slotted_rule_set_);
}

static void AddRules(RuleSet* rule_set,
                     const HeapVector<MinimalRuleData>& rules) {
  for (const auto& info : rules)
    rule_set->AddRule(info.rule_, info.selector_index_, info.flags_);
}

void ScopedStyleResolver::AddTreeBoundaryCrossingRules(
    const RuleSet& author_rules,
    CSSStyleSheet* parent_style_sheet,
    unsigned sheet_index) {
  bool is_document_scope = GetTreeScope().RootNode().IsDocumentNode();
  if (author_rules.DeepCombinatorOrShadowPseudoRules().IsEmpty() &&
      (is_document_scope ||
       (author_rules.ContentPseudoElementRules().IsEmpty())))
    return;

  if (!author_rules.DeepCombinatorOrShadowPseudoRules().IsEmpty())
    has_deep_or_shadow_selector_ = true;

  auto* rule_set_for_scope = MakeGarbageCollected<RuleSet>();
  AddRules(rule_set_for_scope,
           author_rules.DeepCombinatorOrShadowPseudoRules());

  if (!is_document_scope)
    AddRules(rule_set_for_scope, author_rules.ContentPseudoElementRules());

  if (!tree_boundary_crossing_rule_set_) {
    tree_boundary_crossing_rule_set_ =
        MakeGarbageCollected<CSSStyleSheetRuleSubSet>();
    GetTreeScope().GetDocument().GetStyleEngine().AddTreeBoundaryCrossingScope(
        GetTreeScope());
  }

  tree_boundary_crossing_rule_set_->push_back(MakeGarbageCollected<RuleSubSet>(
      parent_style_sheet, sheet_index, rule_set_for_scope));
}

void ScopedStyleResolver::V0ShadowAddedOnV1Document() {
  // See the comment in AddSlottedRules().
  if (!slotted_rule_set_)
    return;

  if (!tree_boundary_crossing_rule_set_) {
    tree_boundary_crossing_rule_set_ =
        MakeGarbageCollected<CSSStyleSheetRuleSubSet>();
    GetTreeScope().GetDocument().GetStyleEngine().AddTreeBoundaryCrossingScope(
        GetTreeScope());
  }
  tree_boundary_crossing_rule_set_->AppendVector(*slotted_rule_set_);
  slotted_rule_set_ = nullptr;
}

void ScopedStyleResolver::AddSlottedRules(const RuleSet& author_rules,
                                          CSSStyleSheet* parent_style_sheet,
                                          unsigned sheet_index) {
  bool is_document_scope = GetTreeScope().RootNode().IsDocumentNode();
  if (is_document_scope || author_rules.SlottedPseudoElementRules().IsEmpty())
    return;

  auto* slotted_rule_set = MakeGarbageCollected<RuleSet>();
  AddRules(slotted_rule_set, author_rules.SlottedPseudoElementRules());

  // In case ::slotted rule is used in V0/V1 mixed document, put ::slotted
  // rules in tree boundary crossing rules as the pure v1 fast path in
  // StyleResolver misses them.
  // Adding this tree scope to tree boundary crossing scopes may end up in
  // O(N^2) where N is number of scopes which has ::slotted() rules.
  // Once the document-wide cascade order flag downgrades from V1 to V0,
  // these slotted rules have to be moved back to tree boundary crossing
  // rule sets. See V0ShadowAddedOnV1Document().
  if (GetTreeScope().GetDocument().MayContainV0Shadow()) {
    if (!tree_boundary_crossing_rule_set_) {
      tree_boundary_crossing_rule_set_ =
          MakeGarbageCollected<CSSStyleSheetRuleSubSet>();
      GetTreeScope()
          .GetDocument()
          .GetStyleEngine()
          .AddTreeBoundaryCrossingScope(GetTreeScope());
    }
    tree_boundary_crossing_rule_set_->push_back(
        MakeGarbageCollected<RuleSubSet>(parent_style_sheet, sheet_index,
                                         slotted_rule_set));
    return;
  }
  if (!slotted_rule_set_)
    slotted_rule_set_ = MakeGarbageCollected<CSSStyleSheetRuleSubSet>();
  slotted_rule_set_->push_back(MakeGarbageCollected<RuleSubSet>(
      parent_style_sheet, sheet_index, slotted_rule_set));
}

void ScopedStyleResolver::RuleSubSet::Trace(blink::Visitor* visitor) {
  visitor->Trace(parent_style_sheet_);
  visitor->Trace(rule_set_);
}

}  // namespace blink
