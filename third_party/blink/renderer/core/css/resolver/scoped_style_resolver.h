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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_SCOPED_STYLE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_SCOPED_STYLE_RESOLVER_H_

#include "third_party/blink/renderer/core/css/active_style_sheets.h"
#include "third_party/blink/renderer/core/css/element_rule_collector.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class CounterStyleMap;
class PageRuleCollector;
class PartNames;
class StyleSheetContents;

// ScopedStyleResolver collects the style sheets that occur within a TreeScope
// and provides methods to collect the rules that apply to a given element,
// broken down by what kind of scope they apply to (e.g. shadow host, slotted,
// etc).
class CORE_EXPORT ScopedStyleResolver final
    : public GarbageCollected<ScopedStyleResolver> {
 public:
  explicit ScopedStyleResolver(TreeScope& scope) : scope_(scope) {}
  ScopedStyleResolver(const ScopedStyleResolver&) = delete;
  ScopedStyleResolver& operator=(const ScopedStyleResolver&) = delete;

  const TreeScope& GetTreeScope() const { return *scope_; }
  ScopedStyleResolver* Parent() const;

  StyleRuleKeyframes* KeyframeStylesForAnimation(
      const AtomicString& animation_name);

  CounterStyleMap* GetCounterStyleMap() { return counter_style_map_; }
  static void CounterStyleRulesChanged(TreeScope& scope);

  void AppendActiveStyleSheets(unsigned index, const ActiveStyleSheetVector&);
  void CollectMatchingElementScopeRules(ElementRuleCollector&);
  void CollectMatchingShadowHostRules(ElementRuleCollector&);
  void CollectMatchingSlottedRules(ElementRuleCollector&);
  void CollectMatchingPartPseudoRules(ElementRuleCollector&,
                                      PartNames& part_names,
                                      bool for_shadow_pseudo);
  void MatchPageRules(PageRuleCollector&);
  void CollectFeaturesTo(RuleFeatureSet&,
                         HeapHashSet<Member<const StyleSheetContents>>&
                             visited_shared_style_sheet_contents) const;
  void ResetStyle();
  void SetHasUnresolvedKeyframesRule() {
    has_unresolved_keyframes_rule_ = true;
  }
  bool NeedsAppendAllSheets() const { return needs_append_all_sheets_; }
  void SetNeedsAppendAllSheets() { needs_append_all_sheets_ = true; }
  static void KeyframesRulesAdded(const TreeScope&);
  static Element& InvalidationRootForTreeScope(const TreeScope&);

  void Trace(Visitor*) const;

 private:
  void AddSlottedRules(const RuleSet&, CSSStyleSheet*, unsigned sheet_index);
  void AddFontFaceRules(const RuleSet&);
  void AddCounterStyleRules(const RuleSet&);
  void AddKeyframeRules(const RuleSet&);
  void AddKeyframeStyle(StyleRuleKeyframes*);

  CounterStyleMap& EnsureCounterStyleMap();

  Member<TreeScope> scope_;

  HeapVector<Member<CSSStyleSheet>> style_sheets_;
  MediaQueryResultList viewport_dependent_media_query_results_;
  MediaQueryResultList device_dependent_media_query_results_;

  using KeyframesRuleMap =
      HeapHashMap<AtomicString, Member<StyleRuleKeyframes>>;
  KeyframesRuleMap keyframes_rule_map_;

  Member<CounterStyleMap> counter_style_map_;

  class RuleSubSet final : public GarbageCollected<RuleSubSet> {
   public:
    RuleSubSet(CSSStyleSheet* sheet, unsigned index, RuleSet* rules)
        : parent_style_sheet_(sheet), parent_index_(index), rule_set_(rules) {}

    Member<CSSStyleSheet> parent_style_sheet_;
    unsigned parent_index_;
    Member<RuleSet> rule_set_;

    void Trace(Visitor*) const;
  };
  using CSSStyleSheetRuleSubSet = HeapVector<Member<RuleSubSet>>;

  Member<CSSStyleSheetRuleSubSet> slotted_rule_set_;

  bool has_unresolved_keyframes_rule_ = false;
  bool needs_append_all_sheets_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_SCOPED_STYLE_RESOLVER_H_
