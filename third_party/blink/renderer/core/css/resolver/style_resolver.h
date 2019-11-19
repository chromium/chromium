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

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/element_rule_collector.h"
#include "third_party/blink/renderer/core/css/pseudo_style_request.h"
#include "third_party/blink/renderer/core/css/resolver/css_property_priority.h"
#include "third_party/blink/renderer/core/css/resolver/matched_properties_cache.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/css/selector_filter.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class CSSValue;
class CompositorKeyframeValue;
class Document;
class Element;
class Interpolation;
class MatchResult;
class RuleSet;
class CSSPropertyValueSet;
class StyleRuleUsageTracker;
class PropertyHandle;

enum RuleMatchingBehavior { kMatchAllRules, kMatchAllRulesExcludingSMIL };
enum ApplyMask { kApplyMaskRegular = 1 << 0, kApplyMaskVisited = 1 << 1 };

// This class selects a ComputedStyle for a given element in a document based on
// the document's collection of stylesheets (user styles, author styles, UA
// style). There is a 1-1 relationship of StyleResolver and Document.
class CORE_EXPORT StyleResolver final : public GarbageCollected<StyleResolver> {
 public:
  explicit StyleResolver(Document&);
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

  scoped_refptr<const ComputedStyle> StyleForPage(int page_index);
  scoped_refptr<const ComputedStyle> StyleForText(Text*);

  static scoped_refptr<ComputedStyle> StyleForViewport(Document&);

  // TODO(esprehn): StyleResolver should probably not contain tree walking
  // state, instead we should pass a context object during recalcStyle.
  SelectorFilter& GetSelectorFilter() { return selector_filter_; }

  StyleRuleKeyframes* FindKeyframesRule(const Element*,
                                        const AtomicString& animation_name);

  // These methods will give back the set of rules that matched for a given
  // element (or a pseudo-element).
  enum CSSRuleFilter {
    kUAAndUserCSSRules = 1 << 1,
    kAuthorCSSRules = 1 << 2,
    kEmptyCSSRules = 1 << 3,
    kCrossOriginCSSRules = 1 << 4,
    kAllButEmptyCSSRules =
        kUAAndUserCSSRules | kAuthorCSSRules | kCrossOriginCSSRules,
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

  void ComputeFont(Element&, ComputedStyle*, const CSSPropertyValueSet&);

  // FIXME: Rename to reflect the purpose, like didChangeFontSize or something.
  void InvalidateMatchedPropertiesCache();

  void SetResizedForViewportUnits();
  void ClearResizedForViewportUnits();

  void SetRuleUsageTracker(StyleRuleUsageTracker*);
  void UpdateMediaType();

  static bool HasAuthorBackground(const StyleResolverState&);

  void Trace(blink::Visitor*);

 private:
  // FIXME: This should probably go away, folded into FontBuilder.
  void UpdateFont(StyleResolverState&);

  void AddMatchedRulesToTracker(const ElementRuleCollector&);

  void LoadPendingResources(StyleResolverState&);

  void CollectPseudoRulesForElement(const Element&,
                                    ElementRuleCollector&,
                                    PseudoId,
                                    unsigned rules_to_include);
  void MatchRuleSet(ElementRuleCollector&, RuleSet*);
  void MatchUARules(ElementRuleCollector&);
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

  struct CacheSuccess {
    STACK_ALLOCATED();

   public:
    bool is_inherited_cache_hit;
    bool is_non_inherited_cache_hit;
    unsigned cache_hash;
    Member<const CachedMatchedProperties> cached_matched_properties;

    CacheSuccess(bool is_inherited_cache_hit,
                 bool is_non_inherited_cache_hit,
                 unsigned cache_hash,
                 const CachedMatchedProperties* cached_matched_properties)
        : is_inherited_cache_hit(is_inherited_cache_hit),
          is_non_inherited_cache_hit(is_non_inherited_cache_hit),
          cache_hash(cache_hash),
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
  };

  // These flags indicate whether an apply pass for a given CSSPropertyPriority
  // and isImportant is required.
  class NeedsApplyPass {
   public:
    bool Get(CSSPropertyPriority priority, bool is_important) const {
      return flags_[GetIndex(priority, is_important)];
    }
    void Set(CSSPropertyPriority priority, bool is_important) {
      flags_[GetIndex(priority, is_important)] = true;
    }

   private:
    static size_t GetIndex(CSSPropertyPriority priority, bool is_important) {
      DCHECK(priority >= 0 && priority < kPropertyPriorityCount);
      return priority * 2 + is_important;
    }
    bool flags_[kPropertyPriorityCount * 2] = {0};
  };

  enum class ForcedColorFilter { kEnabled, kDisabled };

  enum ShouldUpdateNeedsApplyPass {
    kCheckNeedsApplyPass = false,
    kUpdateNeedsApplyPass = true,
  };

  CacheSuccess ApplyMatchedCache(StyleResolverState&, const MatchResult&);
  void ApplyCustomProperties(StyleResolverState&,
                             const MatchResult&,
                             const CacheSuccess&,
                             NeedsApplyPass&);
  void ApplyMatchedAnimationProperties(StyleResolverState&,
                                       const MatchResult&,
                                       const CacheSuccess&,
                                       NeedsApplyPass&);
  void ApplyMatchedHighPriorityProperties(StyleResolverState&,
                                          const MatchResult&,
                                          const CacheSuccess&,
                                          bool& apply_inherited_only,
                                          NeedsApplyPass&);
  void ApplyMatchedLowPriorityProperties(StyleResolverState&,
                                         const MatchResult&,
                                         const CacheSuccess&,
                                         bool& apply_inherited_only,
                                         NeedsApplyPass&);
  void ApplyMatchedProperties(StyleResolverState&, const MatchResult&);
  template <CSSPropertyPriority priority>
  void ApplyForcedColors(StyleResolverState& state,
                         const MatchResult& match_result,
                         bool apply_inherited_only,
                         NeedsApplyPass& needs_apply_pass);
  template <CSSPropertyPriority priority>
  void ApplyUaForcedColors(StyleResolverState& state,
                           const MatchResult& match_result,
                           bool apply_inherited_only,
                           NeedsApplyPass& needs_apply_pass);

  void CascadeAndApplyMatchedProperties(StyleResolverState&,
                                        const MatchResult&);
  void CascadeMatchResult(StyleResolverState&,
                          StyleCascade&,
                          const MatchResult&);
  void CascadeRange(StyleResolverState&,
                    StyleCascade&,
                    const MatchedPropertiesRange&,
                    StyleCascade::Origin);
  void CascadeTransitions(StyleResolverState&, StyleCascade&);
  void CascadeAnimations(StyleResolverState&, StyleCascade&);
  void CascadeInterpolations(StyleCascade&,
                             const ActiveInterpolationsMap&,
                             StyleCascade::Origin);

  void CalculateAnimationUpdate(StyleResolverState&);

  bool ApplyAnimatedStandardProperties(StyleResolverState&);

  void ApplyCallbackSelectors(StyleResolverState&);

  template <CSSPropertyPriority priority, ShouldUpdateNeedsApplyPass>
  void ApplyMatchedProperties(
      StyleResolverState&,
      const MatchedPropertiesRange&,
      bool important,
      bool inherited_only,
      NeedsApplyPass&,
      ForcedColorFilter forced_colors = ForcedColorFilter::kDisabled);
  template <CSSPropertyPriority priority, ShouldUpdateNeedsApplyPass>
  void ApplyProperties(
      StyleResolverState&,
      const CSSPropertyValueSet* properties,
      bool is_important,
      bool inherited_only,
      NeedsApplyPass&,
      ValidPropertyFilter,
      unsigned apply_mask,
      ForcedColorFilter forced_colors = ForcedColorFilter::kDisabled);
  template <CSSPropertyPriority priority>
  void ApplyAnimatedStandardProperties(StyleResolverState&,
                                       const ActiveInterpolationsMap&);
  template <CSSPropertyPriority priority>
  void ApplyAllProperty(StyleResolverState&,
                        const CSSValue&,
                        bool inherited_only,
                        ValidPropertyFilter,
                        unsigned apply_mask);

  void ApplyCascadedColorValue(StyleResolverState&);

  bool PseudoStyleForElementInternal(Element&,
                                     const PseudoElementStyleRequest&,
                                     StyleResolverState&);

  bool HasAuthorBorder(const StyleResolverState&);
  Document& GetDocument() const { return *document_; }
  bool WasViewportResized() const { return was_viewport_resized_; }

  bool IsForcedColorsModeEnabled() const;

  MatchedPropertiesCache matched_properties_cache_;
  Member<Document> document_;
  SelectorFilter selector_filter_;

  Member<StyleRuleUsageTracker> tracker_;

  bool print_media_type_ = false;
  bool was_viewport_resized_ = false;
  DISALLOW_COPY_AND_ASSIGN(StyleResolver);

  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest, ApplyInternalLightDarkColor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_RESOLVER_H_
