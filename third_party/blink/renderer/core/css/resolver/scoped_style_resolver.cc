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
#include "third_party/blink/renderer/core/css/cascade_layer_map.h"
#include "third_party/blink/renderer/core/css/counter_style_map.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/page_rule_collector.h"
#include "third_party/blink/renderer/core/css/part_names.h"
#include "third_party/blink/renderer/core/css/resolver/match_request.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/rule_feature_set.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_scope_data.h"
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
    if (ScopedStyleResolver* resolver = scope->GetScopedStyleResolver()) {
      return resolver;
    }
  }
  return nullptr;
}

void ScopedStyleResolver::AddKeyframeRules(const RuleSet& rule_set) {
  const HeapVector<Member<StyleRuleKeyframes>> keyframes_rules =
      rule_set.KeyframesRules();
  for (auto rule : keyframes_rules) {
    AddKeyframeStyle(rule);
  }
}

CounterStyleMap& ScopedStyleResolver::EnsureCounterStyleMap() {
  if (!counter_style_map_) {
    counter_style_map_ = CounterStyleMap::CreateAuthorCounterStyleMap(*scope_);
  }
  return *counter_style_map_;
}

void ScopedStyleResolver::AddFontFaceRules(const RuleSet& rule_set) {
  // TODO(crbug.com/336876): We don't add @font-face rules of scoped style
  // sheets for the moment.
  if (!GetTreeScope().RootNode().IsDocumentNode()) {
    return;
  }

  Document& document = GetTreeScope().GetDocument();
  CSSFontSelector* css_font_selector =
      document.GetStyleEngine().GetFontSelector();
  const HeapVector<Member<StyleRuleFontFace>> font_face_rules =
      rule_set.FontFaceRules();
  for (auto& font_face_rule : font_face_rules) {
    if (FontFace* font_face = FontFace::Create(&document, font_face_rule,
                                               false /* is_user_style */)) {
      css_font_selector->GetFontFaceCache()->Add(font_face_rule, font_face);
    }
  }
  if (font_face_rules.size()) {
    document.GetStyleResolver().InvalidateMatchedPropertiesCache();
  }
}

void ScopedStyleResolver::AddCounterStyleRules(const RuleSet& rule_set) {
  if (rule_set.CounterStyleRules().empty()) {
    return;
  }
  EnsureCounterStyleMap().AddCounterStyles(rule_set);
}

void ScopedStyleResolver::AppendActiveStyleSheets(
    unsigned index,
    const ActiveStyleSheetVector& active_sheets) {
  for (const ActiveStyleSheet& active_sheet :
       base::span(active_sheets).subspan(index)) {
    CSSStyleSheet* sheet = active_sheet.first;
    media_query_result_flags_.Add(sheet->GetMediaQueryResultFlags());
    if (!active_sheet.second) {
      continue;
    }

    const RuleSet& rule_set = *active_sheet.second;
    if (!active_style_sheets_.empty() &&
        active_style_sheets_.back().second == active_sheet.second) {
      // Some frameworks generate a ton of identical <style> tags;
      // we have already deduplicated them earlier to have the same
      // pointer, so we can just discard them here. Of course,
      // this assumes they come immediately after each other,
      // but this is a cheap win for something that is rather pathological.
      //
      // TODO(sesse): Consider deduplicating noncontiguous stylesheets;
      // however, we'd need to make sure this doesn't change @layer ordering.
    } else {
      active_style_sheets_.push_back(active_sheet);
      AddKeyframeRules(rule_set);
      AddFontFaceRules(rule_set);
      AddCounterStyleRules(rule_set);
      AddPositionTryRules(rule_set);
      AddFunctionRules(rule_set);
      AddFontFeatureValuesRules(rule_set);
    }
    AddImplicitScopeTriggers(*sheet, rule_set);
  }
}

void ScopedStyleResolver::CollectFeaturesTo(
    RuleFeatureSet& features,
    HeapHashSet<Member<const StyleSheetContents>>&
        visited_shared_style_sheet_contents) const {
  features.MutableMediaQueryResultFlags().Add(media_query_result_flags_);

  for (auto [sheet, rule_set] : active_style_sheets_) {
    DCHECK(sheet->ownerNode() || sheet->IsConstructed());
    StyleSheetContents* contents = sheet->Contents();
    if (contents->HasOneClient() ||
        visited_shared_style_sheet_contents.insert(contents).is_new_entry) {
      features.Merge(rule_set->Features());
    }
  }
}

void ScopedStyleResolver::ResetStyle() {
  RemoveImplicitScopeTriggers();
  active_style_sheets_.clear();
  media_query_result_flags_.Clear();
  keyframes_rule_map_.clear();
  position_try_rule_map_.clear();
  font_feature_values_storage_map_.clear();
  function_rule_map_.clear();
  if (counter_style_map_) {
    counter_style_map_->Dispose();
  }
  cascade_layer_map_ = nullptr;
  needs_append_all_sheets_ = false;
}

StyleRuleKeyframes* ScopedStyleResolver::KeyframeStylesForAnimation(
    const AtomicString& animation_name) {
  if (keyframes_rule_map_.empty()) {
    return nullptr;
  }

  KeyframesRuleMap::iterator it = keyframes_rule_map_.find(animation_name);
  if (it == keyframes_rule_map_.end()) {
    return nullptr;
  }

  return it->value.Get();
}

void ScopedStyleResolver::AddKeyframeStyle(StyleRuleKeyframes* rule) {
  AtomicString name = rule->GetName();

  KeyframesRuleMap::iterator it = keyframes_rule_map_.find(name);
  if (it == keyframes_rule_map_.end() ||
      KeyframeStyleShouldOverride(rule, it->value)) {
    keyframes_rule_map_.Set(name, rule);
  }
}

bool ScopedStyleResolver::KeyframeStyleShouldOverride(
    const StyleRuleKeyframes* new_rule,
    const StyleRuleKeyframes* existing_rule) const {
  if (new_rule->IsVendorPrefixed() != existing_rule->IsVendorPrefixed()) {
    return existing_rule->IsVendorPrefixed();
  }
  return !cascade_layer_map_ || cascade_layer_map_->CompareLayerOrder(
                                    existing_rule->GetCascadeLayer(),
                                    new_rule->GetCascadeLayer()) <= 0;
}

Element& ScopedStyleResolver::InvalidationRootForTreeScope(
    const TreeScope& tree_scope) {
  DCHECK(tree_scope.GetDocument().documentElement());
  if (tree_scope.RootNode() == tree_scope.GetDocument()) {
    return *tree_scope.GetDocument().documentElement();
  }
  return To<ShadowRoot>(tree_scope.RootNode()).host();
}

void ScopedStyleResolver::KeyframesRulesAdded(const TreeScope& tree_scope) {
  // Called when @keyframes rules are about to be added/removed from a
  // TreeScope. @keyframes rules may apply to animations on elements in the
  // same TreeScope as the stylesheet, or the host element in the parent
  // TreeScope if the TreeScope is a shadow tree.
  if (!tree_scope.GetDocument().documentElement()) {
    return;
  }

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

  StyleChangeReasonForTracing reason = StyleChangeReasonForTracing::Create(
      style_change_reason::kKeyframesRuleChange);
  if (had_unresolved_keyframes) {
    // If an animation ended up not being started because no @keyframes
    // rules were found for the animation-name, we need to recalculate style
    // for the elements in the scope, including its shadow host if
    // applicable.
    InvalidationRootForTreeScope(tree_scope)
        .SetNeedsStyleRecalc(kSubtreeStyleChange, reason);
    return;
  }

  // If we have animations running, added/removed @keyframes may affect these.
  tree_scope.GetDocument().Timeline().InvalidateKeyframeEffects(tree_scope,
                                                                reason);
}

namespace {

bool CanRejectRuleSet(ElementRuleCollector& collector,
                      const RuleSet& rule_set) {
  const StyleScope* scope = rule_set.SingleScope();
  return scope && collector.CanRejectScope(*scope);
}

}  // namespace

template <class Func>
void ScopedStyleResolver::ForAllStylesheets(ElementRuleCollector& collector,
                                            const Func& func) {
  if (active_style_sheets_.empty()) {
    return;
  }

  MatchRequest match_request{&scope_->RootNode()};
  for (auto [sheet, rule_set] : active_style_sheets_) {
    if (CanRejectRuleSet(collector, *rule_set)) {
      continue;
    }
    match_request.AddRuleset(rule_set.Get());
    if (match_request.IsFull()) {
      func(match_request);
      match_request.ClearAfterMatching();
    }
  }
  if (!match_request.IsEmpty()) {
    func(match_request);
  }
}

void ScopedStyleResolver::CollectMatchingElementScopeRules(
    ElementRuleCollector& collector,
    PartNames* part_names) {
  ForAllStylesheets(
      collector, [&collector, part_names](const MatchRequest& match_request) {
        collector.CollectMatchingRules(match_request, part_names);
      });
}

void ScopedStyleResolver::CollectMatchingShadowHostRules(
    ElementRuleCollector& collector) {
  ForAllStylesheets(collector, [&collector](const MatchRequest& match_request) {
    collector.CollectMatchingShadowHostRules(match_request);
  });
}

void ScopedStyleResolver::CollectMatchingSlottedRules(
    ElementRuleCollector& collector) {
  ForAllStylesheets(collector, [&collector](const MatchRequest& match_request) {
    collector.CollectMatchingSlottedRules(match_request);
  });
}

void ScopedStyleResolver::CollectMatchingPartPseudoRules(
    ElementRuleCollector& collector,
    PartNames* part_names,
    bool for_shadow_pseudo) {
  ForAllStylesheets(collector, [&](const MatchRequest& match_request) {
    collector.CollectMatchingPartPseudoRules(match_request, part_names,
                                             for_shadow_pseudo);
  });
}

void ScopedStyleResolver::MatchPageRules(PageRuleCollector& collector) {
  // Currently, only @page rules in the document scope apply.
  DCHECK(scope_->RootNode().IsDocumentNode());
  for (auto [sheet, rule_set] : active_style_sheets_) {
    collector.MatchPageRules(rule_set.Get(), CascadeOrigin::kAuthor, scope_,
                             GetCascadeLayerMap());
  }
}

void ScopedStyleResolver::RebuildCascadeLayerMap(
    const ActiveStyleSheetVector& sheets) {
  cascade_layer_map_ = MakeGarbageCollected<CascadeLayerMap>(sheets);
}

void ScopedStyleResolver::AddPositionTryRules(const RuleSet& rule_set) {
  for (StyleRulePositionTry* rule : rule_set.PositionTryRules()) {
    auto result = position_try_rule_map_.insert(rule->Name(), rule);
    if (result.is_new_entry) {
      continue;
    }
    Member<StyleRulePositionTry>& stored_rule = result.stored_value->value;
    const bool should_override =
        !cascade_layer_map_ ||
        cascade_layer_map_->CompareLayerOrder(stored_rule->GetCascadeLayer(),
                                              rule->GetCascadeLayer()) <= 0;
    if (should_override) {
      stored_rule = rule;
    }
  }
}

void ScopedStyleResolver::AddFunctionRules(const RuleSet& rule_set) {
  const HeapVector<Member<StyleRuleFunction>> function_rules =
      rule_set.FunctionRules();
  for (StyleRuleFunction* rule : function_rules) {
    // TODO(crbug.com/324780202): Handle @layer.
    function_rule_map_.Set(rule->GetName(), rule);
  }
}

void ScopedStyleResolver::AddFontFeatureValuesRules(const RuleSet& rule_set) {
  // TODO(https://crbug.com/1382722): Support @font-feature-values in shadow
  // trees and support scoping correctly. See CSSFontSelector::GetFontData: In
  // that function we would need to look for parent TreeScopes, but currently,
  // we only check the Document-level TreeScope.
  if (!GetTreeScope().RootNode().IsDocumentNode()) {
    return;
  }

  const HeapVector<Member<StyleRuleFontFeatureValues>>
      font_feature_values_rules = rule_set.FontFeatureValuesRules();
  for (auto& rule : font_feature_values_rules) {
    for (auto& font_family : rule->GetFamilies()) {
      unsigned layer_order = CascadeLayerMap::kImplicitOuterLayerOrder;
      if (cascade_layer_map_ && rule->GetCascadeLayer() != nullptr) {
        layer_order =
            cascade_layer_map_->GetLayerOrder(*rule->GetCascadeLayer());
      }
      auto add_result = font_feature_values_storage_map_.insert(
          String(font_family).FoldCase(), rule->Storage());
      if (add_result.is_new_entry) {
        add_result.stored_value->value.SetLayerOrder(layer_order);
      } else {
        add_result.stored_value->value.FuseUpdate(rule->Storage(), layer_order);
      }
    }
  }
}

StyleRulePositionTry* ScopedStyleResolver::PositionTryForName(
    const AtomicString& try_name) {
  DCHECK(try_name);
  auto iter = position_try_rule_map_.find(try_name);
  if (iter != position_try_rule_map_.end()) {
    return iter->value.Get();
  }
  return nullptr;
}

StyleRuleFunction* ScopedStyleResolver::FunctionForName(StringView name) {
  auto iter = function_rule_map_.find(name.ToString());
  if (iter != function_rule_map_.end()) {
    return iter->value.Get();
  }
  return nullptr;
}

const FontFeatureValuesStorage* ScopedStyleResolver::FontFeatureValuesForFamily(
    AtomicString font_family) {
  if (font_feature_values_storage_map_.empty() || font_family.empty()) {
    return nullptr;
  }

  auto it =
      font_feature_values_storage_map_.find(String(font_family).FoldCase());
  if (it == font_feature_values_storage_map_.end()) {
    return nullptr;
  }

  return &(it->value);
}

// When appending/removing stylesheets, we go through all implicit
// StyleScope instances in each stylesheet and store those instances
// in the StyleScopeData (ElementRareData) of the triggering element.
//
// See StyleScopeData for more information.

namespace {

Element* ImplicitScopeTrigger(TreeScope& scope, CSSStyleSheet& sheet) {
  if (Element* owner_parent = sheet.OwnerParentOrShadowHostElement()) {
    return owner_parent;
  }
  if (sheet.IsAdoptedByTreeScope(scope)) {
    if (ShadowRoot* shadow_root = DynamicTo<ShadowRoot>(scope)) {
      return &shadow_root->host();
    }
  }
  return nullptr;
}

template <typename Func>
void ForEachImplicitScopeTrigger(TreeScope& scope,
                                 CSSStyleSheet& sheet,
                                 const RuleSet& rule_set,
                                 Func func) {
  for (const RuleSet::Interval<StyleScope>& interval :
       rule_set.ScopeIntervals()) {
    const StyleScope* style_scope = interval.value.Get();
    if (!style_scope || !style_scope->IsImplicit()) {
      continue;
    }
    if (Element* scoping_root = ImplicitScopeTrigger(scope, sheet)) {
      func(*scoping_root, *style_scope);
    }
  }
}

}  // namespace

void ScopedStyleResolver::AddImplicitScopeTriggers(CSSStyleSheet& sheet,
                                                   const RuleSet& rule_set) {
  ForEachImplicitScopeTrigger(
      *scope_, sheet, rule_set,
      [&](Element& element, const StyleScope& style_scope) {
        AddImplicitScopeTrigger(element, style_scope);
      });
}

void ScopedStyleResolver::AddImplicitScopeTrigger(
    Element& element,
    const StyleScope& style_scope) {
  DCHECK(style_scope.IsImplicit());
  element.EnsureStyleScopeData().AddTriggeredImplicitScope(style_scope);
}

void ScopedStyleResolver::RemoveImplicitScopeTriggers() {
  for (auto [sheet, rule_set] : active_style_sheets_) {
    RemoveImplicitScopeTriggers(*sheet, *rule_set);
  }
}

void ScopedStyleResolver::RemoveImplicitScopeTriggers(CSSStyleSheet& sheet,
                                                      const RuleSet& rule_set) {
  ForEachImplicitScopeTrigger(
      *scope_, sheet, rule_set,
      [&](Element& element, const StyleScope& style_scope) {
        RemoveImplicitScopeTrigger(element, style_scope);
      });
}

void ScopedStyleResolver::RemoveImplicitScopeTrigger(
    Element& element,
    const StyleScope& style_scope) {
  if (StyleScopeData* style_scope_data = element.GetStyleScopeData()) {
    style_scope_data->RemoveTriggeredImplicitScope(style_scope);
  }
}

void ScopedStyleResolver::Trace(Visitor* visitor) const {
  visitor->Trace(scope_);
  visitor->Trace(active_style_sheets_);
  visitor->Trace(keyframes_rule_map_);
  visitor->Trace(position_try_rule_map_);
  visitor->Trace(function_rule_map_);
  visitor->Trace(counter_style_map_);
  visitor->Trace(cascade_layer_map_);
}

}  // namespace blink
