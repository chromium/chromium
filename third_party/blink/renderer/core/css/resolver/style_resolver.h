/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_RESOLVER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/element_rule_collector.h"
#include "third_party/blink/renderer/core/css/pseudo_style_request.h"
#include "third_party/blink/renderer/core/css/resolver/matched_properties_cache.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/css/selector_filter.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class CompositorKeyframeValue;
class CSSPropertyValueSet;
class CSSValue;
class Document;
class Element;
class Interpolation;
class MatchResult;
class PropertyHandle;
class RuleSet;
class StyleCascade;
class StyleRuleUsageTracker;

enum RuleMatchingBehavior { kMatchAllRules, kMatchAllRulesExcludingSMIL };

// This class selects a ComputedStyle for a given element in a document based on
// the document's collection of stylesheets (user styles, author styles, UA
// style). There is a 1-1 relationship of StyleResolver and Document.
class CORE_EXPORT StyleResolver final : public GarbageCollected<StyleResolver> {
 public:
  explicit StyleResolver(Document&);
  StyleResolver(const StyleResolver&) = delete;
  StyleResolver& operator=(const StyleResolver&) = delete;
  ~StyleResolver();
  void Dispose();

  scoped_refptr<ComputedStyle> StyleForElement(
      Element*,
      const ComputedStyle* parent_style = nullptr,
      const ComputedStyle* layout_parent_style = nullptr,
      RuleMatchingBehavior = kMatchAllRules);

  static scoped_refptr<ComputedStyle> InitialStyleForElement(Document&);

  static CompositorKeyframeValue* CreateCompositorKeyframeValueSnapshot(
      Element&,
      const ComputedStyle& base_style,
      const ComputedStyle* parent_style,
      const PropertyHandle&,
      const CSSValue*);

  scoped_refptr<ComputedStyle> PseudoStyleForElement(
      Element*,
      const PseudoElementStyleRequest&,
      const ComputedStyle* parent_style,
      const ComputedStyle* layout_parent_style);

  scoped_refptr<const ComputedStyle> StyleForPage(
      uint32_t page_index,
      const AtomicString& page_name);
  scoped_refptr<const ComputedStyle> StyleForText(Text*);
  scoped_refptr<ComputedStyle> StyleForViewport();

  // TODO(esprehn): StyleResolver should probably not contain tree walking
  // state, instead we should pass a context object during recalcStyle.
  SelectorFilter& GetSelectorFilter() { return selector_filter_; }

  StyleRuleKeyframes* FindKeyframesRule(const Element*,
                                        const AtomicString& animation_name);

  // These methods will give back the set of rules that matched for a given
  // element (or a pseudo-element).
  enum CSSRuleFilter {
    kUACSSRules = 1 << 1,
    kUserCSSRules = 1 << 2,
    kAuthorCSSRules = 1 << 3,
    kEmptyCSSRules = 1 << 4,
    kCrossOriginCSSRules = 1 << 5,
    kUAAndUserCSSRules = kUACSSRules | kUserCSSRules,
    kAllButEmptyCSSRules =
        kUAAndUserCSSRules | kAuthorCSSRules | kCrossOriginCSSRules,
    kAllButUACSSRules =
        kUserCSSRules | kAuthorCSSRules | kEmptyCSSRules | kCrossOriginCSSRules,
    kAllCSSRules = kAllButEmptyCSSRules | kEmptyCSSRules,
  };
  RuleIndexList* CssRulesForElement(
      Element*,
      unsigned rules_to_include = kAllButEmptyCSSRules);
  RuleIndexList* PseudoCSSRulesForElement(
      Element*,
      PseudoId,
      unsigned rules_to_include = kAllButEmptyCSSRules);
  StyleRuleList* StyleRulesForElement(Element*, unsigned rules_to_include);
  HeapHashMap<CSSPropertyName, Member<const CSSValue>> CascadedValuesForElement(
      Element*,
      PseudoId);

  void ComputeFont(Element&, ComputedStyle*, const CSSPropertyValueSet&);

  // FIXME: Rename to reflect the purpose, like didChangeFontSize or something.
  void InvalidateMatchedPropertiesCache();

  void SetResizedForViewportUnits();
  void ClearResizedForViewportUnits();

  void SetRuleUsageTracker(StyleRuleUsageTracker*);
  void UpdateMediaType();

  static bool CanReuseBaseComputedStyle(const StyleResolverState& state);

  scoped_refptr<ComputedStyle> StyleForInterpolations(
      Element& element,
      ActiveInterpolationsMap& animations);

  // When updating transitions, the "before change style" is the style from
  // the previous style change with the addition of all declarative animations
  // ticked to the current time. Ticking the animations is required to ensure
  // smooth retargeting of transitions.
  // https://drafts.csswg.org/css-transitions-1/#before-change-style
  scoped_refptr<ComputedStyle> BeforeChangeStyleForTransitionUpdate(
      Element& element,
      const ComputedStyle& base_style,
      ActiveInterpolationsMap& transition_interpolations);

  void Trace(Visitor*) const;

 private:
  void InitStyleAndApplyInheritance(Element& element,
                                    StyleResolverState& state);
  void ApplyBaseStyle(Element* element,
                      StyleResolverState& state,
                      StyleCascade& cascade,
                      MatchResult& match_result,
                      RuleMatchingBehavior matching_behavior,
                      bool can_cache_animation_base_computed_style);
  void ApplyInterpolations(StyleResolverState& state,
                           StyleCascade& cascade,
                           ActiveInterpolationsMap& interpolations);

  // FIXME: This should probably go away, folded into FontBuilder.
  void UpdateFont(StyleResolverState&);

  void AddMatchedRulesToTracker(const ElementRuleCollector&);

  void CollectPseudoRulesForElement(const Element&,
                                    ElementRuleCollector&,
                                    PseudoId,
                                    unsigned rules_to_include);
  void MatchRuleSet(ElementRuleCollector&, RuleSet*);
  void MatchUARules(const Element&, ElementRuleCollector&);
  void MatchUserRules(ElementRuleCollector&);
  // This matches `::part` selectors. It looks in ancestor scopes as far as
  // part mapping requires.
  void MatchPseudoPartRules(const Element&, ElementRuleCollector&);
  void MatchPseudoPartRulesForUAHost(const Element&, ElementRuleCollector&);
  void MatchScopedRulesV0(const Element&,
                          ElementRuleCollector&,
                          ScopedStyleResolver*);
  void MatchAuthorRules(const Element&, ElementRuleCollector&);
  void MatchAuthorRulesV0(const Element&, ElementRuleCollector&);
  void MatchAllRules(StyleResolverState&,
                     ElementRuleCollector&,
                     bool include_smil_properties);
  void CollectTreeBoundaryCrossingRulesV0CascadeOrder(const Element&,
                                                      ElementRuleCollector&);
  void ApplyMathMLCustomStyleProperties(Element*, StyleResolverState&);

  struct CacheSuccess {
    STACK_ALLOCATED();

   public:
    bool is_inherited_cache_hit;
    bool is_non_inherited_cache_hit;
    MatchedPropertiesCache::Key key;
    const CachedMatchedProperties* cached_matched_properties;

    CacheSuccess(bool is_inherited_cache_hit,
                 bool is_non_inherited_cache_hit,
                 MatchedPropertiesCache::Key key,
                 const CachedMatchedProperties* cached_matched_properties)
        : is_inherited_cache_hit(is_inherited_cache_hit),
          is_non_inherited_cache_hit(is_non_inherited_cache_hit),
          key(key),
          cached_matched_properties(cached_matched_properties) {}

    bool IsFullCacheHit() const {
      return is_inherited_cache_hit && is_non_inherited_cache_hit;
    }
    bool ShouldApplyInheritedOnly() const {
      return is_non_inherited_cache_hit && !is_inherited_cache_hit;
    }
    void SetFailed() {
      is_inherited_cache_hit = false;
      is_non_inherited_cache_hit = false;
    }
    bool EffectiveZoomChanged(const ComputedStyle&) const;
    bool FontChanged(const ComputedStyle&) const;
    bool InheritedVariablesChanged(const ComputedStyle&) const;
    bool IsUsableAfterApplyInheritedOnly(const ComputedStyle&) const;
  };

  CacheSuccess ApplyMatchedCache(StyleResolverState&, const MatchResult&);
  void MaybeAddToMatchedPropertiesCache(StyleResolverState&,
                                        const CacheSuccess&,
                                        const MatchResult&);

  void CascadeAndApplyMatchedProperties(StyleResolverState&,
                                        StyleCascade& cascade);

  void CalculateAnimationUpdate(StyleResolverState&);

  bool ApplyAnimatedStyle(StyleResolverState&, StyleCascade&);

  void ApplyCallbackSelectors(StyleResolverState&);

  Document& GetDocument() const { return *document_; }
  bool WasViewportResized() const { return was_viewport_resized_; }

  bool IsForcedColorsModeEnabled() const;
  bool IsForcedColorsModeEnabled(const StyleResolverState&) const;

  MatchedPropertiesCache matched_properties_cache_;
  Member<Document> document_;
  SelectorFilter selector_filter_;

  Member<StyleRuleUsageTracker> tracker_;

  bool print_media_type_ = false;
  bool was_viewport_resized_ = false;

  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest, ApplyInternalLightDarkColor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_RESOLVER_H_
