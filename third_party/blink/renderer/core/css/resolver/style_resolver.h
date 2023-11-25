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

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/color_scheme_flags.h"
#include "third_party/blink/renderer/core/css/css_position_fallback_rule.h"
#include "third_party/blink/renderer/core/css/element_rule_collector.h"
#include "third_party/blink/renderer/core/css/resolver/matched_properties_cache.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/css/selector_filter.h"
#include "third_party/blink/renderer/core/css/style_request.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

class CompositorKeyframeValue;
class ContainerSelector;
class CSSPropertyValueSet;
class CSSValue;
class Document;
class Element;
class Interpolation;
class MatchResult;
class PropertyHandle;
class StyleCascade;
class StyleRecalcContext;
class StyleRuleUsageTracker;

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

  const ComputedStyle* ResolveStyle(Element*,
                                    const StyleRecalcContext&,
                                    const StyleRequest& = StyleRequest());

  // Return a reference to the initial style singleton.
  const ComputedStyle& InitialStyle() const;

  // Create a new ComputedStyleBuilder based on the initial style singleton.
  ComputedStyleBuilder CreateComputedStyleBuilder() const;

  // Create a new ComputedStyleBuilder inheriting from the given style.
  ComputedStyleBuilder CreateComputedStyleBuilderInheritingFrom(
      const ComputedStyle& parent_style) const;

  // Create a ComputedStyle for initial styles to be used as the basis for the
  // root element style. In addition to initial values things like zoom, font,
  // forced color mode etc. is set.
  ComputedStyleBuilder InitialStyleBuilderForElement() const;
  const ComputedStyle* InitialStyleForElement() const {
    return InitialStyleBuilderForElement().TakeStyle();
  }

  float InitialZoom() const;

  static CompositorKeyframeValue* CreateCompositorKeyframeValueSnapshot(
      Element&,
      const ComputedStyle& base_style,
      const ComputedStyle* parent_style,
      const PropertyHandle&,
      const CSSValue*,
      double offset);

  const ComputedStyle* StyleForPage(uint32_t page_index,
                                    const AtomicString& page_name);
  const ComputedStyle* StyleForText(Text*);
  const ComputedStyle* StyleForViewport();
  const ComputedStyle* StyleForFormattedText(
      bool is_text_run,
      const ComputedStyle& parent_style,
      const CSSPropertyValueSet* css_property_value_set);
  const ComputedStyle* StyleForFormattedText(
      bool is_text_run,
      const FontDescription& default_font,
      const CSSPropertyValueSet* css_property_value_set);
  // Returns `ComputedStyle` for rendering initial letter text.
  // `initial_letter_box_style` should have non-normal `initial-letter`
  // property.
  const ComputedStyle* StyleForInitialLetterText(
      const ComputedStyle& initial_letter_box_style,
      const ComputedStyle& paragraph_style);

  // Propagate computed values from the root or body element to the viewport
  // when specified to do so.
  void PropagateStyleToViewport();

  // Create ComputedStyle for anonymous boxes.
  ComputedStyleBuilder CreateAnonymousStyleBuilderWithDisplay(
      const ComputedStyle& parent_style,
      EDisplay);
  const ComputedStyle* CreateAnonymousStyleWithDisplay(
      const ComputedStyle& parent_style,
      EDisplay display) {
    return CreateAnonymousStyleBuilderWithDisplay(parent_style, display)
        .TakeStyle();
  }

  // Create ComputedStyle for anonymous wrappers between text boxes and
  // display:contents elements.
  const ComputedStyle* CreateInheritedDisplayContentsStyleIfNeeded(
      const ComputedStyle& parent_style,
      const ComputedStyle& layout_parent_style);

  // TODO(esprehn): StyleResolver should probably not contain tree walking
  // state, instead we should pass a context object during recalcStyle.
  SelectorFilter& GetSelectorFilter() { return selector_filter_; }

  struct FindKeyframesRuleResult {
    StyleRuleKeyframes* rule = nullptr;
    const TreeScope* tree_scope = nullptr;
    STACK_ALLOCATED();
  };
  FindKeyframesRuleResult FindKeyframesRule(const Element*,
                                            const Element* animating_element,
                                            const AtomicString& animation_name);

  // These methods will give back the set of rules that matched for a given
  // element (or a pseudo-element).
  enum CSSRuleFilter {
    kUACSSRules = 1 << 1,
    kUserCSSRules = 1 << 2,
    kAuthorCSSRules = 1 << 3,
    kUAAndUserCSSRules = kUACSSRules | kUserCSSRules,
    kAllButUACSSRules = kUserCSSRules | kAuthorCSSRules,
    kAllCSSRules = kUAAndUserCSSRules | kAuthorCSSRules,
  };
  RuleIndexList* CssRulesForElement(Element*,
                                    unsigned rules_to_include = kAllCSSRules);
  RuleIndexList* PseudoCSSRulesForElement(
      Element*,
      PseudoId,
      const AtomicString& view_transition_name,
      unsigned rules_to_include = kAllCSSRules);
  // Note that StyleRulesForElement will behave as if all links are
  // unvisited; the :visited pseudo class will never match.
  StyleRuleList* StyleRulesForElement(Element*, unsigned rules_to_include);
  HeapHashMap<CSSPropertyName, Member<const CSSValue>> CascadedValuesForElement(
      Element*,
      PseudoId);

  Element* FindContainerForElement(Element*,
                                   const ContainerSelector&,
                                   const TreeScope* selector_tree_scope);

  Font ComputeFont(Element&, const ComputedStyle&, const CSSPropertyValueSet&);

  // FIXME: Rename to reflect the purpose, like didChangeFontSize or something.
  void InvalidateMatchedPropertiesCache();

  void SetResizedForViewportUnits();
  void ClearResizedForViewportUnits();
  bool WasViewportResized() const { return was_viewport_resized_; }

  void SetRuleUsageTracker(StyleRuleUsageTracker*);
  void UpdateMediaType();

  static bool CanReuseBaseComputedStyle(const StyleResolverState& state);

  static const CSSValue* ComputeValue(Element* element,
                                      const CSSPropertyName&,
                                      const CSSValue&);
  // Resolves a single CSSValue in the context of some element's computed style.
  //
  // This is intended for use by the Inspector Agent.
  static const CSSValue* ResolveValue(Element& element,
                                      const CSSPropertyName&,
                                      const CSSValue&);

  // Compute FilterOperations from the specified CSSValue, using the provided
  // Font to resolve any font-relative units.
  //
  // Triggers loading of any external references held by |CSSValue|.
  FilterOperations ComputeFilterOperations(Element*,
                                           const Font&,
                                           const CSSValue&);

  const ComputedStyle* StyleForInterpolations(
      Element& element,
      ActiveInterpolationsMap& animations);

  // When updating transitions, the "before change style" is the style from
  // the previous style change with the addition of all declarative animations
  // ticked to the current time. Ticking the animations is required to ensure
  // smooth retargeting of transitions.
  // https://drafts.csswg.org/css-transitions-1/#before-change-style
  const ComputedStyle* BeforeChangeStyleForTransitionUpdate(
      Element& element,
      const ComputedStyle& base_style,
      ActiveInterpolationsMap& transition_interpolations);
  StyleRulePositionFallback* ResolvePositionFallbackRule(
      const TreeScope* tree_scope,
      AtomicString position_fallback_name);
  const ComputedStyle* ResolvePositionFallbackStyle(Element&, unsigned index);

  // Check if the BODY or HTML element's display or containment stops
  // propagation of BODY style to HTML and viewport.
  bool ShouldStopBodyPropagation(const Element& body_or_html);

  void Trace(Visitor*) const;

 private:
  // Creates a new ComputedStyle, either cloning an existing one
  // or combining two different ones (see the comment on
  // ApplyBaseStyleNoCache() for more details).
  void InitStyle(Element& element,
                 const StyleRequest&,
                 const ComputedStyle& source_for_noninherited,
                 const ComputedStyle* parent_style,
                 StyleResolverState& state);

  void ApplyBaseStyle(Element* element,
                      const StyleRecalcContext&,
                      const StyleRequest&,
                      StyleResolverState& state,
                      StyleCascade& cascade);
  void ApplyBaseStyleNoCache(Element* element,
                             const StyleRecalcContext&,
                             const StyleRequest&,
                             StyleResolverState& state,
                             StyleCascade& cascade);
  void ApplyInterpolations(StyleResolverState& state,
                           StyleCascade& cascade,
                           ActiveInterpolationsMap& interpolations);

  void AddMatchedRulesToTracker(const ElementRuleCollector&);

  void CollectPseudoRulesForElement(const Element&,
                                    ElementRuleCollector&,
                                    PseudoId,
                                    const AtomicString& view_transition_name,
                                    unsigned rules_to_include);
  void MatchUARules(const Element&, ElementRuleCollector&);
  void MatchUserRules(ElementRuleCollector&);
  void MatchPresentationalHints(StyleResolverState&, ElementRuleCollector&);
  // This matches `::part` selectors. It looks in ancestor scopes as far as
  // part mapping requires.
  void MatchPseudoPartRules(const Element&,
                            ElementRuleCollector&,
                            bool for_shadow_pseudo = false);
  void MatchPseudoPartRulesForUAHost(const Element&, ElementRuleCollector&);
  void MatchTryRules(const Element&, ElementRuleCollector&);
  void MatchAuthorRules(const Element&,
                        ElementRuleCollector&);
  void MatchAllRules(StyleResolverState&,
                     ElementRuleCollector&,
                     bool include_smil_properties);
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
    bool EffectiveZoomChanged(const ComputedStyleBuilder&) const;
    bool FontChanged(const ComputedStyleBuilder&) const;
    bool InheritedVariablesChanged(const ComputedStyleBuilder&) const;
    bool LineHeightChanged(const ComputedStyleBuilder&) const;
    bool IsUsableAfterApplyInheritedOnly(const ComputedStyleBuilder&) const;
  };

  CacheSuccess ApplyMatchedCache(StyleResolverState&,
                                 const StyleRequest&,
                                 const MatchResult&);
  void MaybeAddToMatchedPropertiesCache(StyleResolverState&,
                                        const CacheSuccess&,
                                        const MatchResult&);

  void ApplyPropertiesFromCascade(StyleResolverState&,
                                  StyleCascade& cascade,
                                  CacheSuccess cache_success);

  bool ApplyAnimatedStyle(StyleResolverState&, StyleCascade&);

  void ApplyCallbackSelectors(StyleResolverState&);
  void ApplyDocumentRulesSelectors(StyleResolverState&, ContainerNode* scope);
  StyleRuleList* CollectMatchingRulesFromUnconnectedRuleSet(
      StyleResolverState&,
      RuleSet*,
      ContainerNode* scope);

  Document& GetDocument() const { return *document_; }

  bool IsForcedColorsModeEnabled() const;

  template <typename Functor>
  void ForEachUARulesForElement(const Element& element,
                                ElementRuleCollector* collector,
                                Functor& func) const;

  MatchedPropertiesCache matched_properties_cache_;

  // Both these members are on a hot-path for creating ComputedStyle objects.
  subtle::UncompressedMember<const ComputedStyle> initial_style_;
  subtle::UncompressedMember<const ComputedStyle> initial_style_for_img_;
  SelectorFilter selector_filter_;

  Member<Document> document_;
  Member<StyleRuleUsageTracker> tracker_;

  // This is a dummy/disconnected element that we use for FormattedText
  // style computations; see `EnsureElementForFormattedText`.
  Member<Element> formatted_text_element_;

  bool print_media_type_ = false;
  bool was_viewport_resized_ = false;

  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest, ApplyInternalLightDarkColor);
  friend class StyleResolverTest;
  FRIEND_TEST_ALL_PREFIXES(StyleResolverTest, TreeScopedReferences);

  Element& EnsureElementForFormattedText();
  const ComputedStyle* StyleForFormattedText(
      bool is_text_run,
      const FontDescription* default_font,
      const ComputedStyle* parent_style,
      const CSSPropertyValueSet* css_property_value_set);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_RESOLVER_H_
