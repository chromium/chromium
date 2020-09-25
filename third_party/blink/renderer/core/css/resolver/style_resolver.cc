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

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"

#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_value_factory.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_initial_color_value.h"
#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_selector_watch.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/element_rule_collector.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/page_rule_collector.h"
#include "third_party/blink/renderer/core/css/part_names.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/selector_filter_parent_scope.h"
#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_stats.h"
#include "third_party/blink/renderer/core/css/resolver/style_rule_usage_tracker.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/mathml/mathml_fraction_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_operator_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_padded_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_space_element.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/style/style_initial_data.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

namespace {

void SetAnimationUpdateIfNeeded(StyleResolverState& state, Element& element) {
  // If any changes to CSS Animations were detected, stash the update away for
  // application after the layout object is updated if we're in the appropriate
  // scope.
  if (!state.AnimationUpdate().IsEmpty()) {
    auto& element_animations = element.EnsureElementAnimations();
    element_animations.CssAnimations().SetPendingUpdate(
        state.AnimationUpdate());
  }
}

bool HasAnimationsOrTransitions(const StyleResolverState& state) {
  return state.Style()->Animations() || state.Style()->Transitions() ||
         (state.GetAnimatingElement() &&
          state.GetAnimatingElement()->HasAnimations());
}

bool ShouldComputeBaseComputedStyle(const ComputedStyle* base_computed_style) {
#if DCHECK_IS_ON()
  // The invariant in the base computed style optimization is that as long as
  // |IsAnimationStyleChange| is true, the computed style that would be
  // generated by the style resolver is equivalent to the one we hold
  // internally. To ensure this, we always compute a new style here disregarding
  // the fact that we have a base computed style when DCHECKs are enabled, and
  // call ValidateBaseComputedStyle() to check that the optimization was sound.
  return true;
#else
  return !base_computed_style;
#endif  // !DCHECK_IS_ON()
}

// Compare the base computed style with the one we compute to validate that the
// optimization is sound.
bool ValidateBaseComputedStyle(const ComputedStyle* base_computed_style,
                               const ComputedStyle& computed_style) {
#if DCHECK_IS_ON()
  if (!base_computed_style)
    return true;
  // Under certain conditions ComputedStyle::operator==() may return false for
  // differences that are permitted during an animation.
  // The FontFaceCache version number may be increased without forcing a style
  // recalc (see crbug.com/471079).
  if (!base_computed_style->GetFont().IsFallbackValid())
    return true;
  // Images use instance equality rather than value equality (see
  // crbug.com/781461).
  for (CSSPropertyID id :
       {CSSPropertyID::kBackgroundImage, CSSPropertyID::kWebkitMaskImage}) {
    if (!CSSPropertyEquality::PropertiesEqual(
            PropertyHandle(CSSProperty::Get(id)), *base_computed_style,
            computed_style)) {
      return true;
    }
  }
  return *base_computed_style == computed_style;
#else
  return true;
#endif  // DCHECK_IS_ON()
}

// When force-computing the base computed style for validation purposes,
// we need to reset the StyleCascade when the base computed style optimization
// is used. This is because we don't want the computation of the base to
// populate the cascade, as they are supposed to be empty when the optimization
// is in use. This is to match the behavior of non-DCHECK builds.
void MaybeResetCascade(StyleCascade& cascade) {
#if DCHECK_IS_ON()
  cascade.Reset();
#endif  // DCHECK_IS_ON()
}

}  // namespace

static CSSPropertyValueSet* LeftToRightDeclaration() {
  DEFINE_STATIC_LOCAL(
      Persistent<MutableCSSPropertyValueSet>, left_to_right_decl,
      (MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode)));
  if (left_to_right_decl->IsEmpty()) {
    left_to_right_decl->SetProperty(CSSPropertyID::kDirection,
                                    CSSValueID::kLtr);
  }
  return left_to_right_decl;
}

static CSSPropertyValueSet* RightToLeftDeclaration() {
  DEFINE_STATIC_LOCAL(
      Persistent<MutableCSSPropertyValueSet>, right_to_left_decl,
      (MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode)));
  if (right_to_left_decl->IsEmpty()) {
    right_to_left_decl->SetProperty(CSSPropertyID::kDirection,
                                    CSSValueID::kRtl);
  }
  return right_to_left_decl;
}

static CSSPropertyValueSet* DocumentElementUserAgentDeclarations() {
  DEFINE_STATIC_LOCAL(
      Persistent<MutableCSSPropertyValueSet>, document_element_ua_decl,
      (MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode)));
  if (document_element_ua_decl->IsEmpty()) {
    document_element_ua_decl->SetProperty(CSSPropertyID::kColor,
                                          *CSSInitialColorValue::Create());
  }
  return document_element_ua_decl;
}

static void CollectScopedResolversForHostedShadowTrees(
    const Element& element,
    HeapVector<Member<ScopedStyleResolver>, 8>& resolvers) {
  ShadowRoot* root = element.GetShadowRoot();
  if (!root)
    return;

  // Adding scoped resolver for active shadow roots for shadow host styling.
  if (ScopedStyleResolver* resolver = root->GetScopedStyleResolver())
    resolvers.push_back(resolver);
}

StyleResolver::StyleResolver(Document& document) : document_(document) {
  UpdateMediaType();
}

StyleResolver::~StyleResolver() = default;

void StyleResolver::Dispose() {
  matched_properties_cache_.Clear();
}

void StyleResolver::SetRuleUsageTracker(StyleRuleUsageTracker* tracker) {
  tracker_ = tracker;
}

static inline ScopedStyleResolver* ScopedResolverFor(const Element& element) {
  // For normal elements, returning element->treeScope().scopedStyleResolver()
  // is enough. Rules for ::cue and custom pseudo elements like
  // ::-webkit-meter-bar pierce through a single shadow dom boundary and apply
  // to elements in sub-scopes.
  //
  // An assumption here is that these elements belong to scopes without a
  // ScopedStyleResolver due to the fact that VTT scopes and UA shadow trees
  // don't have <style> or <link> elements. This is backed up by the DCHECKs
  // below. The one exception to this assumption are the media controls which
  // use a <style> element for CSS animations in the shadow DOM. If a <style>
  // element is present in the shadow DOM then this will also block any
  // author styling.

  TreeScope* tree_scope = &element.GetTreeScope();
  if (ScopedStyleResolver* resolver = tree_scope->GetScopedStyleResolver()) {
#if DCHECK_IS_ON()
    if (!element.HasMediaControlAncestor())
      DCHECK(element.ShadowPseudoId().IsEmpty());
#endif
    DCHECK(!element.IsVTTElement());
    return resolver;
  }

  tree_scope = tree_scope->ParentTreeScope();
  if (!tree_scope)
    return nullptr;
  if (element.ShadowPseudoId().IsEmpty() && !element.IsVTTElement())
    return nullptr;
  return tree_scope->GetScopedStyleResolver();
}

// Matches :host and :host-context rules if the element is a shadow host.
// It matches rules from the ShadowHostRules of the ScopedStyleResolver
// of the attached shadow root.
static void MatchHostRules(const Element& element,
                           ElementRuleCollector& collector) {
  ShadowRoot* shadow_root = element.GetShadowRoot();
  if (!shadow_root)
    return;
  if (ScopedStyleResolver* resolver = shadow_root->GetScopedStyleResolver())
    resolver->CollectMatchingShadowHostRules(collector);
}

// Matches custom element rules from Custom Element Default Style.
static void MatchCustomElementRules(const Element& element,
                                    ElementRuleCollector& collector) {
  if (!RuntimeEnabledFeatures::CustomElementDefaultStyleEnabled())
    return;
  if (CustomElementDefinition* definition =
          element.GetCustomElementDefinition()) {
    if (definition->HasDefaultStyleSheets()) {
      for (CSSStyleSheet* style : definition->DefaultStyleSheets()) {
        if (!style)
          continue;
        RuleSet* rule_set =
            element.GetDocument().GetStyleEngine().RuleSetForSheet(*style);
        if (rule_set)
          collector.CollectMatchingRules(MatchRequest(rule_set));
      }
    }
  }
}

// Matches :host and :host-context rules
// and custom element rules from Custom Element Default Style.
static void MatchHostAndCustomElementRules(const Element& element,
                                           ElementRuleCollector& collector) {
  ShadowRoot* shadow_root = element.GetShadowRoot();
  ScopedStyleResolver* resolver =
      shadow_root ? shadow_root->GetScopedStyleResolver() : nullptr;
  if (!resolver && !RuntimeEnabledFeatures::CustomElementDefaultStyleEnabled())
    return;
  collector.ClearMatchedRules();
  MatchCustomElementRules(element, collector);
  MatchHostRules(element, collector);
  collector.SortAndTransferMatchedRules();
  collector.FinishAddingAuthorRulesForTreeScope();
}

static void MatchSlottedRules(const Element&, ElementRuleCollector&);
static void MatchSlottedRulesForUAHost(const Element& element,
                                       ElementRuleCollector& collector) {
  if (element.ShadowPseudoId() != shadow_element_names::kPseudoInputPlaceholder)
    return;

  // We allow ::placeholder pseudo element after ::slotted(). Since we are
  // matching such pseudo elements starting from inside the UA shadow DOM of
  // the element having the placeholder, we need to match ::slotted rules from
  // the scopes to which the placeholder's host element may be slotted.
  //
  // Example:
  //
  // <div id=host>
  //   <:shadow-root>
  //     <style>::slotted(input)::placeholder { color: green }</style>
  //     <slot />
  //   </:shadow-root>
  //   <input placeholder="PLACEHOLDER-TEXT">
  //     <:ua-shadow-root>
  //       ... <placeholder>PLACEHOLDER-TEXT</placeholder> ...
  //     </:ua-shadow-root>
  //   </input>
  // </div>
  //
  // Here we need to match the ::slotted rule from the #host shadow tree where
  // the input is slotted on the placeholder element.
  DCHECK(element.OwnerShadowHost());
  MatchSlottedRules(*element.OwnerShadowHost(), collector);
}

// Matches `::slotted` selectors. It matches rules in the element's slot's
// scope. If that slot is itself slotted it will match rules in the slot's
// slot's scope and so on. The result is that it considers a chain of scopes
// descending from the element's own scope.
static void MatchSlottedRules(const Element& element,
                              ElementRuleCollector& collector) {
  MatchSlottedRulesForUAHost(element, collector);
  HTMLSlotElement* slot = element.AssignedSlot();
  if (!slot)
    return;

  HeapVector<Member<ScopedStyleResolver>> resolvers;
  for (; slot; slot = slot->AssignedSlot()) {
    if (ScopedStyleResolver* resolver =
            slot->GetTreeScope().GetScopedStyleResolver())
      resolvers.push_back(resolver);
  }
  for (auto it = resolvers.rbegin(); it != resolvers.rend(); ++it) {
    collector.ClearMatchedRules();
    (*it)->CollectMatchingSlottedRules(collector);
    collector.SortAndTransferMatchedRules();
    collector.FinishAddingAuthorRulesForTreeScope();
  }
}

const static TextTrack* GetTextTrackFromElement(const Element& element) {
  if (auto* vtt_element = DynamicTo<VTTElement>(element))
    return vtt_element->GetTrack();
  if (auto* vtt_cue_background_box = DynamicTo<VTTCueBackgroundBox>(element))
    return vtt_cue_background_box->GetTrack();
  return nullptr;
}

static void MatchVTTRules(const Element& element,
                                  ElementRuleCollector& collector) {
  const TextTrack* text_track = GetTextTrackFromElement(element);
  if (!text_track)
    return;
  const HeapVector<Member<CSSStyleSheet>>& styles =
      text_track->GetCSSStyleSheets();
  if (!styles.IsEmpty()) {
    int style_sheet_index = 0;
    collector.ClearMatchedRules();
    for (CSSStyleSheet* style : styles) {
      RuleSet* rule_set =
          element.GetDocument().GetStyleEngine().RuleSetForSheet(*style);
      if (rule_set) {
        collector.CollectMatchingRules(
            MatchRequest(rule_set, nullptr /* scope */, style,
                         style_sheet_index, true /* is_from_webvtt */));
        style_sheet_index++;
      }
    }
    collector.SortAndTransferMatchedRules();
  }
}

// Matches rules from the element's scope. The selectors may cross shadow
// boundaries during matching, like for :host-context.
static void MatchElementScopeRules(const Element& element,
                                   ScopedStyleResolver* element_scope_resolver,
                                   ElementRuleCollector& collector) {
  if (element_scope_resolver) {
    collector.ClearMatchedRules();
    element_scope_resolver->CollectMatchingAuthorRules(collector);
    element_scope_resolver->CollectMatchingTreeBoundaryCrossingRules(collector);
    collector.SortAndTransferMatchedRules();
  }

  MatchVTTRules(element, collector);
  if (element.IsStyledElement() && element.InlineStyle() &&
      !collector.IsCollectingForPseudoElement()) {
    // Inline style is immutable as long as there is no CSSOM wrapper.
    bool is_inline_style_cacheable = !element.InlineStyle()->IsMutable();
    collector.AddElementStyleProperties(element.InlineStyle(),
                                        is_inline_style_cacheable);
  }

  collector.FinishAddingAuthorRulesForTreeScope();
}

void StyleResolver::MatchPseudoPartRulesForUAHost(
    const Element& element,
    ElementRuleCollector& collector) {
  if (element.ShadowPseudoId() != shadow_element_names::kPseudoInputPlaceholder)
    return;

  // We allow ::placeholder pseudo element after ::part(). See
  // MatchSlottedRulesForUAHost for a more detailed explanation.
  DCHECK(element.OwnerShadowHost());
  MatchPseudoPartRules(*element.OwnerShadowHost(), collector);
}

void StyleResolver::MatchPseudoPartRules(const Element& element,
                                         ElementRuleCollector& collector) {
  MatchPseudoPartRulesForUAHost(element, collector);
  DOMTokenList* part = element.GetPart();
  if (!part)
    return;

  PartNames current_names(part->TokenSet());

  // ::part selectors in the shadow host's scope and above can match this
  // element.
  Element* host = element.OwnerShadowHost();
  if (!host)
    return;

  while (current_names.size()) {
    TreeScope& tree_scope = host->GetTreeScope();
    if (ScopedStyleResolver* resolver = tree_scope.GetScopedStyleResolver()) {
      collector.ClearMatchedRules();
      resolver->CollectMatchingPartPseudoRules(collector, current_names);
      collector.SortAndTransferMatchedRules();
      collector.FinishAddingAuthorRulesForTreeScope();
    }

    // If the host doesn't forward any parts using partmap= then the element is
    // unreachable from any scope further above and we can stop.
    const NamesMap* part_map = host->PartNamesMap();
    if (!part_map)
      return;

    // We have reached the top-level document.
    if (!(host = host->OwnerShadowHost()))
      return;

    current_names.PushMap(*part_map);
  }
}

static bool ShouldCheckScope(const Element& element,
                             const Node& scoping_node,
                             bool is_inner_tree_scope) {
  if (is_inner_tree_scope &&
      element.GetTreeScope() != scoping_node.GetTreeScope()) {
    // Check if |element| may be affected by a ::content rule in |scopingNode|'s
    // style.  If |element| is a descendant of a shadow host which is ancestral
    // to |scopingNode|, the |element| should be included for rule collection.
    // Skip otherwise.
    const TreeScope* scope = &scoping_node.GetTreeScope();
    while (scope && scope->ParentTreeScope() != &element.GetTreeScope())
      scope = scope->ParentTreeScope();
    Element* shadow_host =
        scope ? scope->RootNode().OwnerShadowHost() : nullptr;
    return shadow_host && element.IsDescendantOf(shadow_host);
  }

  // When |element| can be distributed to |scopingNode| via <shadow>, ::content
  // rule can match, thus the case should be included.
  if (!is_inner_tree_scope &&
      scoping_node.ParentOrShadowHostNode() ==
          element.GetTreeScope().RootNode().ParentOrShadowHostNode())
    return true;

  // Obviously cases when ancestor scope has /deep/ or ::shadow rule should be
  // included.  Skip otherwise.
  return scoping_node.GetTreeScope()
      .GetScopedStyleResolver()
      ->HasDeepOrShadowSelector();
}

void StyleResolver::MatchScopedRulesV0(
    const Element& element,
    ElementRuleCollector& collector,
    ScopedStyleResolver* element_scope_resolver) {
  // Match rules from treeScopes in the reverse tree-of-trees order, since the
  // cascading order for normal rules is such that when comparing rules from
  // different shadow trees, the rule from the tree which comes first in the
  // tree-of-trees order wins. From other treeScopes than the element's own
  // scope, only tree-boundary-crossing rules may match.

  bool match_element_scope_done =
      !element_scope_resolver && !element.InlineStyle();

  const auto& tree_boundary_crossing_scopes =
      GetDocument().GetStyleEngine().TreeBoundaryCrossingScopes();
  for (auto it = tree_boundary_crossing_scopes.rbegin();
       it != tree_boundary_crossing_scopes.rend(); ++it) {
    const TreeScope& scope = (*it)->ContainingTreeScope();
    ScopedStyleResolver* resolver = scope.GetScopedStyleResolver();
    DCHECK(resolver);

    bool is_inner_tree_scope =
        element.ContainingTreeScope().IsInclusiveAncestorOf(scope);
    if (!ShouldCheckScope(element, **it, is_inner_tree_scope))
      continue;

    if (!match_element_scope_done &&
        scope.IsInclusiveAncestorOf(element.ContainingTreeScope())) {
      match_element_scope_done = true;

      // At this point, the iterator has either encountered the scope for the
      // element itself (if that scope has boundary-crossing rules), or the
      // iterator has moved to a scope which appears before the element's scope
      // in the tree-of-trees order.  Try to match all rules from the element's
      // scope.

      MatchElementScopeRules(element, element_scope_resolver, collector);
      if (resolver == element_scope_resolver) {
        // Boundary-crossing rules already collected in matchElementScopeRules.
        continue;
      }
    }

    collector.ClearMatchedRules();
    resolver->CollectMatchingTreeBoundaryCrossingRules(collector);
    collector.SortAndTransferMatchedRules();
    collector.FinishAddingAuthorRulesForTreeScope();
  }

  if (!match_element_scope_done)
    MatchElementScopeRules(element, element_scope_resolver, collector);
}

void StyleResolver::MatchAuthorRules(const Element& element,
                                     ElementRuleCollector& collector) {
  if (GetDocument().GetShadowCascadeOrder() ==
      ShadowCascadeOrder::kShadowCascadeV0) {
    MatchAuthorRulesV0(element, collector);
    return;
  }
  MatchHostAndCustomElementRules(element, collector);

  ScopedStyleResolver* element_scope_resolver = ScopedResolverFor(element);
  if (GetDocument().MayContainV0Shadow()) {
    MatchScopedRulesV0(element, collector, element_scope_resolver);
    return;
  }

  MatchSlottedRules(element, collector);
  MatchElementScopeRules(element, element_scope_resolver, collector);
  MatchPseudoPartRules(element, collector);
}

void StyleResolver::MatchAuthorRulesV0(const Element& element,
                                       ElementRuleCollector& collector) {
  collector.ClearMatchedRules();

  ShadowV0CascadeOrder cascade_order = 0;
  HeapVector<Member<ScopedStyleResolver>, 8> resolvers_in_shadow_tree;
  CollectScopedResolversForHostedShadowTrees(element, resolvers_in_shadow_tree);

  // Apply :host and :host-context rules from inner scopes.
  for (int j = resolvers_in_shadow_tree.size() - 1; j >= 0; --j)
    resolvers_in_shadow_tree.at(j)->CollectMatchingShadowHostRules(
        collector, ++cascade_order);

  // Apply normal rules from element scope.
  if (ScopedStyleResolver* resolver = ScopedResolverFor(element))
    resolver->CollectMatchingAuthorRules(collector, ++cascade_order);

  // Apply /deep/ and ::shadow rules from outer scopes, and ::content from
  // inner.
  CollectTreeBoundaryCrossingRulesV0CascadeOrder(element, collector);
  collector.SortAndTransferMatchedRules();
}

void StyleResolver::MatchUserRules(ElementRuleCollector& collector) {
  collector.ClearMatchedRules();
  GetDocument().GetStyleEngine().CollectMatchingUserRules(collector);
  collector.SortAndTransferMatchedRules();
  collector.FinishAddingUserRules();
}

void StyleResolver::MatchUARules(const Element& element,
                                 ElementRuleCollector& collector) {
  collector.SetMatchingUARules(true);

  CSSDefaultStyleSheets& default_style_sheets =
      CSSDefaultStyleSheets::Instance();
  if (!print_media_type_) {
    if (LIKELY(element.IsHTMLElement() || element.IsVTTElement()))
      MatchRuleSet(collector, default_style_sheets.DefaultStyle());
    else if (element.IsSVGElement())
      MatchRuleSet(collector, default_style_sheets.DefaultSVGStyle());
    else if (element.namespaceURI() == mathml_names::kNamespaceURI)
      MatchRuleSet(collector, default_style_sheets.DefaultMathMLStyle());
  } else {
    MatchRuleSet(collector, default_style_sheets.DefaultPrintStyle());
  }

  // In quirks mode, we match rules from the quirks user agent sheet.
  if (GetDocument().InQuirksMode())
    MatchRuleSet(collector, default_style_sheets.DefaultQuirksStyle());

  // If document uses view source styles (in view source mode or in xml viewer
  // mode), then we match rules from the view source style sheet.
  if (GetDocument().IsViewSource())
    MatchRuleSet(collector, default_style_sheets.DefaultViewSourceStyle());

  // If the system is in forced colors mode, match rules from the forced colors
  // style sheet.
  if (IsForcedColorsModeEnabled())
    MatchRuleSet(collector, default_style_sheets.DefaultForcedColorStyle());

  if (collector.IsCollectingForPseudoElement()) {
    if (RuleSet* default_pseudo_style =
            default_style_sheets.DefaultPseudoElementStyleOrNull())
      MatchRuleSet(collector, default_pseudo_style);
  }

  collector.FinishAddingUARules();
  collector.SetMatchingUARules(false);
}

void StyleResolver::MatchRuleSet(ElementRuleCollector& collector,
                                 RuleSet* rules) {
  collector.ClearMatchedRules();
  collector.CollectMatchingRules(MatchRequest(rules));
  collector.SortAndTransferMatchedRules();
}

DISABLE_CFI_PERF
void StyleResolver::MatchAllRules(StyleResolverState& state,
                                  ElementRuleCollector& collector,
                                  bool include_smil_properties) {
  MatchUARules(state.GetElement(), collector);
  MatchUserRules(collector);

  // Now check author rules, beginning first with presentational attributes
  // mapped from HTML.
  if (state.GetElement().IsStyledElement()) {
    collector.AddElementStyleProperties(
        state.GetElement().PresentationAttributeStyle());

    // Now we check additional mapped declarations.
    // Tables and table cells share an additional mapped rule that must be
    // applied after all attributes, since their mapped style depends on the
    // values of multiple attributes.
    collector.AddElementStyleProperties(
        state.GetElement().AdditionalPresentationAttributeStyle());

    if (auto* html_element = DynamicTo<HTMLElement>(state.GetElement())) {
      bool is_auto;
      TextDirection text_direction =
          html_element->DirectionalityIfhasDirAutoAttribute(is_auto);
      if (is_auto) {
        state.SetHasDirAutoAttribute(true);
        collector.AddElementStyleProperties(
            text_direction == TextDirection::kLtr ? LeftToRightDeclaration()
                                                  : RightToLeftDeclaration());
      }
    }
  }

  MatchAuthorRules(state.GetElement(), collector);

  if (state.GetElement().IsStyledElement()) {
    // For Shadow DOM V1, inline style is already collected in
    // matchScopedRules().
    if (GetDocument().GetShadowCascadeOrder() ==
            ShadowCascadeOrder::kShadowCascadeV0 &&
        state.GetElement().InlineStyle()) {
      // Inline style is immutable as long as there is no CSSOM wrapper.
      bool is_inline_style_cacheable =
          !state.GetElement().InlineStyle()->IsMutable();
      collector.AddElementStyleProperties(state.GetElement().InlineStyle(),
                                          is_inline_style_cacheable);
    }

    // Now check SMIL animation override style.
    auto* svg_element = DynamicTo<SVGElement>(state.GetElement());
    if (include_smil_properties && svg_element) {
      collector.AddElementStyleProperties(
          svg_element->AnimatedSMILStyleProperties(), false /* isCacheable */);
    }
  }

  collector.FinishAddingAuthorRulesForTreeScope();
}

void StyleResolver::CollectTreeBoundaryCrossingRulesV0CascadeOrder(
    const Element& element,
    ElementRuleCollector& collector) {
  const auto& tree_boundary_crossing_scopes =
      GetDocument().GetStyleEngine().TreeBoundaryCrossingScopes();
  if (tree_boundary_crossing_scopes.IsEmpty())
    return;

  // When comparing rules declared in outer treescopes, outer's rules win.
  ShadowV0CascadeOrder outer_cascade_order =
      tree_boundary_crossing_scopes.size() * 2;
  // When comparing rules declared in inner treescopes, inner's rules win.
  ShadowV0CascadeOrder inner_cascade_order =
      tree_boundary_crossing_scopes.size();

  for (const auto& scoping_node : tree_boundary_crossing_scopes) {
    // Skip rule collection for element when tree boundary crossing rules of
    // scopingNode's scope can never apply to it.
    bool is_inner_tree_scope =
        element.ContainingTreeScope().IsInclusiveAncestorOf(
            scoping_node->ContainingTreeScope());
    if (!ShouldCheckScope(element, *scoping_node, is_inner_tree_scope))
      continue;

    ShadowV0CascadeOrder cascade_order =
        is_inner_tree_scope ? inner_cascade_order : outer_cascade_order;
    scoping_node->GetTreeScope()
        .GetScopedStyleResolver()
        ->CollectMatchingTreeBoundaryCrossingRules(collector, cascade_order);

    ++inner_cascade_order;
    --outer_cascade_order;
  }
}

scoped_refptr<ComputedStyle> StyleResolver::StyleForViewport() {
  scoped_refptr<ComputedStyle> viewport_style =
      InitialStyleForElement(GetDocument());

  viewport_style->SetZIndex(0);
  viewport_style->SetIsStackingContextWithoutContainment(true);
  viewport_style->SetDisplay(EDisplay::kBlock);
  viewport_style->SetPosition(EPosition::kAbsolute);

  // Document::InheritHtmlAndBodyElementStyles will set the final overflow
  // style values, but they should initially be auto to avoid premature
  // scrollbar removal in PaintLayerScrollableArea::UpdateAfterStyleChange.
  viewport_style->SetOverflowX(EOverflow::kAuto);
  viewport_style->SetOverflowY(EOverflow::kAuto);

  GetDocument().GetStyleEngine().ApplyVisionDeficiencyStyle(viewport_style);

  return viewport_style;
}

static ElementAnimations* GetElementAnimations(
    const StyleResolverState& state) {
  if (!state.GetAnimatingElement())
    return nullptr;
  return state.GetAnimatingElement()->GetElementAnimations();
}

static const ComputedStyle* CachedAnimationBaseComputedStyle(
    StyleResolverState& state) {
  ElementAnimations* element_animations = GetElementAnimations(state);
  if (!element_animations)
    return nullptr;

  return element_animations->BaseComputedStyle();
}

static void UpdateAnimationBaseComputedStyle(StyleResolverState& state,
                                             StyleCascade& cascade,
                                             bool forced_update) {
  if (!state.GetAnimatingElement())
    return;

  if (!state.CanCacheBaseStyle())
    return;

  if (forced_update)
    state.GetAnimatingElement()->EnsureElementAnimations();

  ElementAnimations* element_animations =
      state.GetAnimatingElement()->GetElementAnimations();
  if (!element_animations)
    return;

  if (element_animations->IsAnimationStyleChange() &&
      element_animations->BaseComputedStyle()) {
    return;
  }

  std::unique_ptr<CSSBitset> important_set = cascade.GetImportantSet();
  element_animations->UpdateBaseComputedStyle(state.Style(),
                                              std::move(important_set));
}

scoped_refptr<ComputedStyle> StyleResolver::StyleForElement(
    Element* element,
    const ComputedStyle* default_parent,
    const ComputedStyle* default_layout_parent,
    RuleMatchingBehavior matching_behavior) {
  DCHECK(GetDocument().GetFrame());
  DCHECK(GetDocument().GetSettings());

  GetDocument().GetStyleEngine().IncStyleForElementCount();
  INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(), elements_styled,
                                1);

  SelectorFilterParentScope::EnsureParentStackIsPushed();

  StyleResolverState state(GetDocument(), *element, default_parent,
                           default_layout_parent);

  // This function can return different results with different last 3 params,
  // but we can only cache one base computed style for animations, thus we cache
  // only when this function is called with default last 3 params.
  bool can_cache_animation_base_computed_style =
      !default_parent && !default_layout_parent &&
      matching_behavior == kMatchAllRules;
  state.SetCanCacheBaseStyle(can_cache_animation_base_computed_style);

  STACK_UNINITIALIZED StyleCascade cascade(state);

  ApplyBaseStyle(element, state, cascade, cascade.MutableMatchResult(),
                 matching_behavior, can_cache_animation_base_computed_style);

  if (ApplyAnimatedStyle(state, cascade)) {
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  styles_animated, 1);
    StyleAdjuster::AdjustComputedStyle(state, element);
  }

  if (IsA<HTMLBodyElement>(*element)) {
    GetDocument().GetTextLinkColors().SetTextColor(
        state.Style()->GetCurrentColor());
  }

  if (element->IsMathMLElement())
    ApplyMathMLCustomStyleProperties(element, state);

  SetAnimationUpdateIfNeeded(state, *element);

  if (state.Style()->HasViewportUnits())
    GetDocument().SetHasViewportUnits();

  if (state.Style()->HasRemUnits())
    GetDocument().GetStyleEngine().SetUsesRemUnit(true);

  if (state.Style()->HasGlyphRelativeUnits())
    UseCounter::Count(GetDocument(), WebFeature::kHasGlyphRelativeUnits);

  state.LoadPendingResources();

  // Now return the style.
  return state.TakeStyle();
}

void StyleResolver::InitStyleAndApplyInheritance(Element& element,
                                                 StyleResolverState& state) {
  if (state.ParentStyle()) {
    scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
    style->InheritFrom(*state.ParentStyle(),
                       IsAtShadowBoundary(&element)
                           ? ComputedStyle::kAtShadowBoundary
                           : ComputedStyle::kNotAtShadowBoundary);
    state.SetStyle(std::move(style));

    // contenteditable attribute (implemented by -webkit-user-modify) should
    // be propagated from shadow host to distributed node.
    if (state.DistributedToV0InsertionPoint() || element.AssignedSlot()) {
      if (Element* parent = element.parentElement()) {
        if (const ComputedStyle* shadow_host_style = parent->GetComputedStyle())
          state.Style()->SetUserModify(shadow_host_style->UserModify());
      }
    }
  } else {
    state.SetStyle(InitialStyleForElement(GetDocument()));
    state.SetParentStyle(ComputedStyle::Clone(*state.Style()));
    state.SetLayoutParentStyle(state.ParentStyle());
    if (element != GetDocument().documentElement()) {
      // Strictly, we should only allow the root element to inherit from
      // initial styles, but we allow getComputedStyle() for connected
      // elements outside the flat tree rooted at an unassigned shadow host
      // child, a slot fallback element, or Shadow DOM V0 insertion points.
      DCHECK(element.IsV0InsertionPoint() ||
             ((IsShadowHost(element.parentNode()) ||
               IsA<HTMLSlotElement>(element.parentNode())) &&
              !LayoutTreeBuilderTraversal::ParentElement(element)));
      state.Style()->SetIsEnsuredOutsideFlatTree();
    }
  }

  if (element.IsLink()) {
    state.Style()->SetIsLink();
    EInsideLink link_state = state.ElementLinkState();
    if (link_state != EInsideLink::kNotInsideLink) {
      bool force_visited = false;
      probe::ForcePseudoState(&element, CSSSelector::kPseudoVisited,
                              &force_visited);
      if (force_visited)
        link_state = EInsideLink::kInsideVisitedLink;
    }
    state.Style()->SetInsideLink(link_state);
  }
}

void StyleResolver::ApplyMathMLCustomStyleProperties(
    Element* element,
    StyleResolverState& state) {
  DCHECK(element && element->IsMathMLElement());
  ComputedStyle& style = state.StyleRef();
  if (auto* space = DynamicTo<MathMLSpaceElement>(*element)) {
    space->AddMathBaselineIfNeeded(style, state.CssToLengthConversionData());
  } else if (auto* padded = DynamicTo<MathMLPaddedElement>(*element)) {
    padded->AddMathBaselineIfNeeded(style, state.CssToLengthConversionData());
    padded->AddMathPaddedDepthIfNeeded(style,
                                       state.CssToLengthConversionData());
    padded->AddMathPaddedLSpaceIfNeeded(style,
                                        state.CssToLengthConversionData());
    padded->AddMathPaddedVOffsetIfNeeded(style,
                                         state.CssToLengthConversionData());
  } else if (auto* fraction = DynamicTo<MathMLFractionElement>(*element)) {
    fraction->AddMathFractionBarThicknessIfNeeded(
        style, state.CssToLengthConversionData());
  } else if (auto* operator_element =
                 DynamicTo<MathMLOperatorElement>(*element)) {
    operator_element->AddMathLSpaceIfNeeded(style,
                                            state.CssToLengthConversionData());
    operator_element->AddMathRSpaceIfNeeded(style,
                                            state.CssToLengthConversionData());
    operator_element->AddMathMinSizeIfNeeded(style,
                                             state.CssToLengthConversionData());
    operator_element->AddMathMaxSizeIfNeeded(style,
                                             state.CssToLengthConversionData());
  }
}

void StyleResolver::ApplyBaseStyle(
    Element* element,
    StyleResolverState& state,
    StyleCascade& cascade,
    MatchResult& match_result,
    RuleMatchingBehavior matching_behavior,
    bool can_cache_animation_base_computed_style) {
  bool base_is_usable = can_cache_animation_base_computed_style &&
                        CanReuseBaseComputedStyle(state);
  const ComputedStyle* animation_base_computed_style =
      base_is_usable ? CachedAnimationBaseComputedStyle(state) : nullptr;
  if (ShouldComputeBaseComputedStyle(animation_base_computed_style)) {
    InitStyleAndApplyInheritance(*element, state);

    GetDocument().GetStyleEngine().EnsureUAStyleForElement(*element);

    // This adds a CSSInitialColorValue to the cascade for the document
    // element. The CSSInitialColorValue will resolve to a color-scheme
    // sensitive color in Color::ApplyValue. It is added at the start of the
    // MatchResult such that subsequent declarations (even from the UA sheet)
    // get a higher priority.
    //
    // TODO(crbug.com/1046753): Remove this when canvastext is supported.
    if (element == state.GetDocument().documentElement()) {
      cascade.MutableMatchResult().AddMatchedProperties(
          DocumentElementUserAgentDeclarations());
    }

    ElementRuleCollector collector(state.ElementContext(), selector_filter_,
                                   match_result, state.Style(),
                                   state.Style()->InsideLink());

    MatchAllRules(state, collector,
                  matching_behavior != kMatchAllRulesExcludingSMIL);

    if (tracker_)
      AddMatchedRulesToTracker(collector);

    if (element->GetComputedStyle() &&
        element->GetComputedStyle()->TextAutosizingMultiplier() !=
            state.Style()->TextAutosizingMultiplier()) {
      // Preserve the text autosizing multiplier on style recalc. Autosizer will
      // update it during layout if needed.
      // NOTE: this must occur before ApplyMatchedProperties for correct
      // computation of font-relative lengths.
      state.Style()->SetTextAutosizingMultiplier(
          element->GetComputedStyle()->TextAutosizingMultiplier());
    }

    if (state.HasDirAutoAttribute())
      state.Style()->SetSelfOrAncestorHasDirAutoAttribute(true);

    CascadeAndApplyMatchedProperties(state, cascade);

    ApplyCallbackSelectors(state);

    // Cache our original display.
    state.Style()->SetOriginalDisplay(state.Style()->Display());

    StyleAdjuster::AdjustComputedStyle(state, element);

    DCHECK(ValidateBaseComputedStyle(animation_base_computed_style,
                                     *state.Style()));
  }

  if (base_is_usable) {
    DCHECK(animation_base_computed_style);
    state.SetStyle(ComputedStyle::Clone(*animation_base_computed_style));
    if (!state.ParentStyle()) {
      state.SetParentStyle(InitialStyleForElement(GetDocument()));
      state.SetLayoutParentStyle(state.ParentStyle());
    }
    MaybeResetCascade(cascade);
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  base_styles_used, 1);
  }
}

CompositorKeyframeValue* StyleResolver::CreateCompositorKeyframeValueSnapshot(
    Element& element,
    const ComputedStyle& base_style,
    const ComputedStyle* parent_style,
    const PropertyHandle& property,
    const CSSValue* value) {
  // TODO(alancutter): Avoid creating a StyleResolverState just to apply a
  // single value on a ComputedStyle.
  StyleResolverState state(element.GetDocument(), element, parent_style,
                           parent_style);
  state.SetStyle(ComputedStyle::Clone(base_style));
  if (value) {
    STACK_UNINITIALIZED StyleCascade cascade(state);
    auto* set =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(state.GetParserMode());
    set->SetProperty(property.GetCSSProperty().PropertyID(), *value);
    cascade.MutableMatchResult().FinishAddingUARules();
    cascade.MutableMatchResult().FinishAddingUserRules();
    cascade.MutableMatchResult().AddMatchedProperties(set);
    cascade.Apply();
  }
  return CompositorKeyframeValueFactory::Create(property, *state.Style());
}

scoped_refptr<ComputedStyle> StyleResolver::PseudoStyleForElement(
    Element* element,
    const PseudoElementStyleRequest& pseudo_style_request,
    const ComputedStyle* parent_style,
    const ComputedStyle* parent_layout_object_style) {
  DCHECK(parent_style);
  if (!element)
    return nullptr;

  StyleResolverState state(
      GetDocument(), *element, pseudo_style_request.pseudo_id,
      pseudo_style_request.type, parent_style, parent_layout_object_style);

  DCHECK(GetDocument().GetFrame());
  DCHECK(GetDocument().GetSettings());
  DCHECK(pseudo_style_request.pseudo_id != kPseudoIdFirstLineInherited);

  SelectorFilterParentScope::EnsureParentStackIsPushed();

  bool base_is_usable = CanReuseBaseComputedStyle(state);
  const ComputedStyle* animation_base_computed_style =
      base_is_usable ? CachedAnimationBaseComputedStyle(state) : nullptr;

  // Since we don't use pseudo-elements in any of our quirk/print
  // user agent rules, don't waste time walking those rules.

  STACK_UNINITIALIZED StyleCascade cascade(state);

  if (ShouldComputeBaseComputedStyle(animation_base_computed_style)) {
    if (pseudo_style_request.AllowsInheritance(parent_style)) {
      scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
      style->InheritFrom(*parent_style);
      state.SetStyle(std::move(style));
    } else {
      // ::backdrop inherits from initial styles. All other pseudo elements
      // inherit from their originating element (::before/::after), or
      // originating element descendants (::first-line/::first-letter).
      DCHECK(pseudo_style_request.pseudo_id == kPseudoIdBackdrop);
      state.SetStyle(InitialStyleForElement(GetDocument()));
      state.SetParentStyle(ComputedStyle::Clone(*state.Style()));
    }
    state.Style()->SetStyleType(pseudo_style_request.pseudo_id);

    // Check UA, user and author rules.
    ElementRuleCollector collector(state.ElementContext(), selector_filter_,
                                   cascade.MutableMatchResult(), state.Style(),
                                   state.Style()->InsideLink());
    collector.SetPseudoElementStyleRequest(pseudo_style_request);

    GetDocument().GetStyleEngine().EnsureUAStyleForPseudoElement(
        pseudo_style_request.pseudo_id);

    MatchUARules(*element, collector);
    // TODO(obrufau): support styling nested pseudo-elements
    if (!element->IsPseudoElement()) {
      MatchUserRules(collector);
      MatchAuthorRules(*element, collector);
    }

    if (tracker_)
      AddMatchedRulesToTracker(collector);

    if (!collector.MatchedResult().HasMatchedProperties()) {
      StyleAdjuster::AdjustComputedStyle(state, nullptr);
      if (pseudo_style_request.type == PseudoElementStyleRequest::kForRenderer)
        return nullptr;
      return state.TakeStyle();
    }

    CascadeAndApplyMatchedProperties(state, cascade);

    ApplyCallbackSelectors(state);

    // Cache our original display.
    state.Style()->SetOriginalDisplay(state.Style()->Display());

    // FIXME: Passing 0 as the Element* introduces a lot of complexity
    // in the StyleAdjuster::AdjustComputedStyle code.
    StyleAdjuster::AdjustComputedStyle(state, nullptr);

    DCHECK(ValidateBaseComputedStyle(animation_base_computed_style,
                                     *state.Style()));
  }

  if (animation_base_computed_style) {
    state.SetStyle(ComputedStyle::Clone(*animation_base_computed_style));
    state.Style()->SetStyleType(pseudo_style_request.pseudo_id);
    MaybeResetCascade(cascade);
  }

  state.SetCanCacheBaseStyle(true);
  if (ApplyAnimatedStyle(state, cascade))
    StyleAdjuster::AdjustComputedStyle(state, nullptr);

  GetDocument().GetStyleEngine().IncStyleForElementCount();
  INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                pseudo_elements_styled, 1);

  if (state.Style()->HasViewportUnits())
    GetDocument().SetHasViewportUnits();

  if (state.Style()->HasGlyphRelativeUnits())
    UseCounter::Count(GetDocument(), WebFeature::kHasGlyphRelativeUnits);

  if (PseudoElement* pseudo_element =
          element->GetPseudoElement(pseudo_style_request.pseudo_id)) {
    SetAnimationUpdateIfNeeded(state, *pseudo_element);
    state.LoadPendingResources();
  }

  return state.TakeStyle();
}

scoped_refptr<const ComputedStyle> StyleResolver::StyleForPage(
    uint32_t page_index,
    const AtomicString& page_name) {
  scoped_refptr<const ComputedStyle> initial_style =
      InitialStyleForElement(GetDocument());
  if (!GetDocument().documentElement())
    return initial_style;

  StyleResolverState state(GetDocument(), *GetDocument().documentElement(),
                           initial_style.get(), initial_style.get());

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  const ComputedStyle* root_element_style =
      state.RootElementStyle() ? state.RootElementStyle()
                               : GetDocument().GetComputedStyle();
  DCHECK(root_element_style);
  style->InheritFrom(*root_element_style);
  state.SetStyle(std::move(style));

  STACK_UNINITIALIZED StyleCascade cascade(state);

  PageRuleCollector collector(root_element_style, page_index, page_name,
                              cascade.MutableMatchResult());

  collector.MatchPageRules(
      CSSDefaultStyleSheets::Instance().DefaultPrintStyle());

  if (ScopedStyleResolver* scoped_resolver =
          GetDocument().GetScopedStyleResolver())
    scoped_resolver->MatchPageRules(collector);

  cascade.Apply();

  // Now return the style.
  return state.TakeStyle();
}

scoped_refptr<ComputedStyle> StyleResolver::InitialStyleForElement(
    Document& document) {
  const LocalFrame* frame = document.GetFrame();

  scoped_refptr<ComputedStyle> initial_style = ComputedStyle::Create();

  initial_style->SetRtlOrdering(document.VisuallyOrdered() ? EOrder::kVisual
                                                           : EOrder::kLogical);
  initial_style->SetZoom(frame && !document.Printing() ? frame->PageZoomFactor()
                                                       : 1);
  initial_style->SetEffectiveZoom(initial_style->Zoom());

  FontDescription document_font_description =
      initial_style->GetFontDescription();
  document_font_description.SetLocale(
      LayoutLocale::Get(document.ContentLanguage()));

  initial_style->SetFontDescription(document_font_description);
  initial_style->SetUserModify(document.InDesignMode()
                                   ? EUserModify::kReadWrite
                                   : EUserModify::kReadOnly);
  document.SetupFontBuilder(*initial_style);

  scoped_refptr<StyleInitialData> initial_data =
      document.GetStyleEngine().MaybeCreateAndGetInitialData();
  if (initial_data)
    initial_style->SetInitialData(std::move(initial_data));

  return initial_style;
}

scoped_refptr<const ComputedStyle> StyleResolver::StyleForText(
    Text* text_node) {
  DCHECK(text_node);
  if (Node* parent_node = LayoutTreeBuilderTraversal::Parent(*text_node)) {
    const ComputedStyle* style = parent_node->GetComputedStyle();
    if (style && !style->IsEnsuredInDisplayNone())
      return style;
  }
  return nullptr;
}

void StyleResolver::UpdateFont(StyleResolverState& state) {
  state.GetFontBuilder().CreateFont(state.StyleRef(), state.ParentStyle());
  state.SetConversionFontSizes(CSSToLengthConversionData::FontSizes(
      state.Style(), state.RootElementStyle()));
  state.SetConversionZoom(state.Style()->EffectiveZoom());
}

void StyleResolver::AddMatchedRulesToTracker(
    const ElementRuleCollector& collector) {
  collector.AddMatchedRulesToTracker(tracker_);
}

StyleRuleList* StyleResolver::StyleRulesForElement(Element* element,
                                                   unsigned rules_to_include) {
  DCHECK(element);
  StyleResolverState state(GetDocument(), *element);
  MatchResult match_result;
  ElementRuleCollector collector(state.ElementContext(), selector_filter_,
                                 match_result, state.Style(),
                                 EInsideLink::kNotInsideLink);
  collector.SetMode(SelectorChecker::kCollectingStyleRules);
  CollectPseudoRulesForElement(*element, collector, kPseudoIdNone,
                               rules_to_include);
  return collector.MatchedStyleRuleList();
}

HeapHashMap<CSSPropertyName, Member<const CSSValue>>
StyleResolver::CascadedValuesForElement(Element* element, PseudoId pseudo_id) {
  StyleResolverState state(GetDocument(), *element);
  state.SetStyle(ComputedStyle::Create());

  STACK_UNINITIALIZED StyleCascade cascade(state);
  ElementRuleCollector collector(state.ElementContext(), selector_filter_,
                                 cascade.MutableMatchResult(), state.Style(),
                                 EInsideLink::kNotInsideLink);
  collector.SetPseudoElementStyleRequest(PseudoElementStyleRequest(pseudo_id));
  MatchAllRules(state, collector, false /* include_smil_properties */);

  cascade.Apply();
  return cascade.GetCascadedValues();
}

RuleIndexList* StyleResolver::PseudoCSSRulesForElement(
    Element* element,
    PseudoId pseudo_id,
    unsigned rules_to_include) {
  DCHECK(element);
  StyleResolverState state(GetDocument(), *element);
  MatchResult match_result;
  ElementRuleCollector collector(state.ElementContext(), selector_filter_,
                                 match_result, state.Style(),
                                 EInsideLink::kNotInsideLink);
  collector.SetMode(SelectorChecker::kCollectingCSSRules);
  // TODO(obrufau): support collecting rules for nested ::marker
  if (!element->IsPseudoElement()) {
    CollectPseudoRulesForElement(*element, collector, pseudo_id,
                                 rules_to_include);
  }

  if (tracker_)
    AddMatchedRulesToTracker(collector);
  return collector.MatchedCSSRuleList();
}

RuleIndexList* StyleResolver::CssRulesForElement(Element* element,
                                                 unsigned rules_to_include) {
  return PseudoCSSRulesForElement(element, kPseudoIdNone, rules_to_include);
}

void StyleResolver::CollectPseudoRulesForElement(
    const Element& element,
    ElementRuleCollector& collector,
    PseudoId pseudo_id,
    unsigned rules_to_include) {
  collector.SetPseudoElementStyleRequest(PseudoElementStyleRequest(pseudo_id));

  if (rules_to_include & kUACSSRules)
    MatchUARules(element, collector);
  else
    collector.FinishAddingUARules();

  if (rules_to_include & kUserCSSRules)
    MatchUserRules(collector);
  else
    collector.FinishAddingUserRules();

  if (rules_to_include & kAuthorCSSRules) {
    collector.SetSameOriginOnly(!(rules_to_include & kCrossOriginCSSRules));
    collector.SetIncludeEmptyRules(rules_to_include & kEmptyCSSRules);
    MatchAuthorRules(element, collector);
  }
}

bool StyleResolver::ApplyAnimatedStyle(StyleResolverState& state,
                                       StyleCascade& cascade) {
  Element& element = state.GetElement();

  // The animating element may be this element, the pseudo element we are
  // resolving style for, or null if we are resolving style for a pseudo
  // element which is not represented by a PseudoElement like scrollbar pseudo
  // elements.
  Element* animating_element = state.GetAnimatingElement();
  DCHECK(animating_element == &element || !animating_element ||
         animating_element->ParentOrShadowHostElement() == element);

  if (!HasAnimationsOrTransitions(state)) {
    // Ensure that the base computed style is not stale even if not currently
    // running an animation or transition. This ensures that any new transitions
    // use the correct starting point based on the "before change" style.
    UpdateAnimationBaseComputedStyle(state, cascade, false);
    return false;
  }

  CalculateAnimationUpdate(state);

  CSSAnimations::CalculateCompositorAnimationUpdate(
      state.AnimationUpdate(), animating_element, element, *state.Style(),
      state.ParentStyle(), WasViewportResized());
  CSSAnimations::CalculateTransitionUpdate(
      state.AnimationUpdate(), CSSAnimations::PropertyPass::kStandard,
      animating_element, *state.Style());
  CSSAnimations::CalculateTransitionUpdate(state.AnimationUpdate(),
                                           CSSAnimations::PropertyPass::kCustom,
                                           animating_element, *state.Style());

  CSSAnimations::SnapshotCompositorKeyframes(
      element, state.AnimationUpdate(), *state.Style(), state.ParentStyle());

  bool has_update = !state.AnimationUpdate().IsEmpty();
  UpdateAnimationBaseComputedStyle(state, cascade, has_update);

  if (!has_update)
    return false;

  const ActiveInterpolationsMap& standard_animations =
      state.AnimationUpdate().ActiveInterpolationsForStandardAnimations();
  const ActiveInterpolationsMap& standard_transitions =
      state.AnimationUpdate().ActiveInterpolationsForStandardTransitions();
  const ActiveInterpolationsMap& custom_animations =
      state.AnimationUpdate().ActiveInterpolationsForCustomAnimations();
  const ActiveInterpolationsMap& custom_transitions =
      state.AnimationUpdate().ActiveInterpolationsForCustomTransitions();

  cascade.AddInterpolations(&standard_animations, CascadeOrigin::kAnimation);
  cascade.AddInterpolations(&standard_transitions, CascadeOrigin::kTransition);
  cascade.AddInterpolations(&custom_animations, CascadeOrigin::kAnimation);
  cascade.AddInterpolations(&custom_transitions, CascadeOrigin::kTransition);

  CascadeFilter filter;
  if (IsForcedColorsModeEnabled(state))
    filter = filter.Add(CSSProperty::kIsAffectedByForcedColors, true);
  if (state.Style()->StyleType() == kPseudoIdMarker)
    filter = filter.Add(CSSProperty::kValidForMarker, false);
  filter = filter.Add(CSSProperty::kAnimation, true);

  cascade.Apply(filter);

  // Start loading resources used by animations.
  state.LoadPendingResources();

  DCHECK(!state.GetFontBuilder().FontDirty());

  return true;
}

StyleRuleKeyframes* StyleResolver::FindKeyframesRule(
    const Element* element,
    const AtomicString& animation_name) {
  HeapVector<Member<ScopedStyleResolver>, 8> resolvers;
  CollectScopedResolversForHostedShadowTrees(*element, resolvers);
  if (ScopedStyleResolver* scoped_resolver =
          element->GetTreeScope().GetScopedStyleResolver())
    resolvers.push_back(scoped_resolver);

  for (auto& resolver : resolvers) {
    if (StyleRuleKeyframes* keyframes_rule =
            resolver->KeyframeStylesForAnimation(animation_name.Impl()))
      return keyframes_rule;
  }

  if (StyleRuleKeyframes* keyframes_rule =
          GetDocument().GetStyleEngine().KeyframeStylesForAnimation(
              animation_name))
    return keyframes_rule;

  for (auto& resolver : resolvers)
    resolver->SetHasUnresolvedKeyframesRule();
  return nullptr;
}

void StyleResolver::InvalidateMatchedPropertiesCache() {
  matched_properties_cache_.Clear();
}

void StyleResolver::SetResizedForViewportUnits() {
  DCHECK(!was_viewport_resized_);
  was_viewport_resized_ = true;
  GetDocument().GetStyleEngine().UpdateActiveStyle();
  matched_properties_cache_.ClearViewportDependent();
}

void StyleResolver::ClearResizedForViewportUnits() {
  was_viewport_resized_ = false;
}

bool StyleResolver::CacheSuccess::EffectiveZoomChanged(
    const ComputedStyle& style) const {
  if (!cached_matched_properties)
    return false;
  return cached_matched_properties->computed_style->EffectiveZoom() !=
         style.EffectiveZoom();
}

bool StyleResolver::CacheSuccess::FontChanged(
    const ComputedStyle& style) const {
  if (!cached_matched_properties)
    return false;
  return cached_matched_properties->computed_style->GetFontDescription() !=
         style.GetFontDescription();
}

bool StyleResolver::CacheSuccess::InheritedVariablesChanged(
    const ComputedStyle& style) const {
  if (!cached_matched_properties)
    return false;
  return cached_matched_properties->computed_style->InheritedVariables() !=
         style.InheritedVariables();
}

bool StyleResolver::CacheSuccess::IsUsableAfterApplyInheritedOnly(
    const ComputedStyle& style) const {
  return !EffectiveZoomChanged(style) && !FontChanged(style) &&
         !InheritedVariablesChanged(style);
}

StyleResolver::CacheSuccess StyleResolver::ApplyMatchedCache(
    StyleResolverState& state,
    const MatchResult& match_result) {
  const Element& element = state.GetElement();

  MatchedPropertiesCache::Key key(match_result);

  bool is_inherited_cache_hit = false;
  bool is_non_inherited_cache_hit = false;
  const CachedMatchedProperties* cached_matched_properties =
      key.IsValid() ? matched_properties_cache_.Find(key, state) : nullptr;

  if (cached_matched_properties && MatchedPropertiesCache::IsCacheable(state)) {
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  matched_property_cache_hit, 1);
    // We can build up the style by copying non-inherited properties from an
    // earlier style object built using the same exact style declarations. We
    // then only need to apply the inherited properties, if any, as their values
    // can depend on the element context. This is fast and saves memory by
    // reusing the style data structures.
    if (state.ParentStyle()->InheritedDataShared(
            *cached_matched_properties->parent_computed_style) &&
        !IsAtShadowBoundary(&element) &&
        (!state.DistributedToV0InsertionPoint() || element.AssignedSlot() ||
         state.Style()->UserModify() == EUserModify::kReadOnly)) {
      INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                    matched_property_cache_inherited_hit, 1);

      EInsideLink link_status = state.Style()->InsideLink();
      // If the cache item parent style has identical inherited properties to
      // the current parent style then the resulting style will be identical
      // too. We copy the inherited properties over from the cache and are done.
      state.Style()->InheritFrom(*cached_matched_properties->computed_style);

      // Unfortunately the link status is treated like an inherited property. We
      // need to explicitly restore it.
      state.Style()->SetInsideLink(link_status);

      is_inherited_cache_hit = true;
    }
    if (!IsForcedColorsModeEnabled() || is_inherited_cache_hit) {
      state.Style()->CopyNonInheritedFromCached(
          *cached_matched_properties->computed_style);
      // If the child style is a cache hit, we'll never reach StyleBuilder::
      // ApplyProperty, hence we'll never set the flag on the parent.
      if (state.Style()->HasExplicitInheritance())
        state.ParentStyle()->SetChildHasExplicitInheritance();
      is_non_inherited_cache_hit = true;
    }
    UpdateFont(state);
  }

  return CacheSuccess(is_inherited_cache_hit, is_non_inherited_cache_hit, key,
                      cached_matched_properties);
}

void StyleResolver::MaybeAddToMatchedPropertiesCache(
    StyleResolverState& state,
    const CacheSuccess& cache_success,
    const MatchResult& match_result) {
  state.LoadPendingResources();
  if (!cache_success.cached_matched_properties && cache_success.key.IsValid() &&
      MatchedPropertiesCache::IsCacheable(state)) {
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  matched_property_cache_added, 1);
    matched_properties_cache_.Add(cache_success.key, *state.Style(),
                                  *state.ParentStyle(), state.Dependencies());
  }
}

void StyleResolver::CalculateAnimationUpdate(StyleResolverState& state) {
  Element* animating_element = state.GetAnimatingElement();

  DCHECK(state.Style()->Animations() || state.Style()->Transitions() ||
         (animating_element && animating_element->HasAnimations()));
  DCHECK(!state.IsAnimationInterpolationMapReady());

  CSSAnimations::CalculateAnimationUpdate(
      state.AnimationUpdate(), animating_element, state.GetElement(),
      *state.Style(), state.ParentStyle(), this);

  state.SetIsAnimationInterpolationMapReady();
}

bool StyleResolver::CanReuseBaseComputedStyle(const StyleResolverState& state) {
  ElementAnimations* element_animations = GetElementAnimations(state);
  if (!element_animations || !element_animations->BaseComputedStyle())
    return false;

  if (!element_animations->IsAnimationStyleChange())
    return false;

  // Animating a custom property can have side effects on other properties
  // via variable references. Disallow base computed style optimization in such
  // cases.
  if (CSSAnimations::IsAnimatingCustomProperties(element_animations))
    return false;

  // We need to build the cascade to know what to revert to.
  if (CSSAnimations::IsAnimatingRevert(element_animations))
    return false;

  // When applying an animation or transition for a font affecting property,
  // font-relative units (e.g. em, ex) in the base style must respond to the
  // animation. We cannot use the base computed style optimization in such
  // cases.
  if (CSSAnimations::IsAnimatingFontAffectingProperties(element_animations)) {
    if (element_animations->BaseComputedStyle() &&
        element_animations->BaseComputedStyle()->HasFontRelativeUnits()) {
      return false;
    }
  }

  // Normally, we apply all active animation effects on top of the style created
  // by regular CSS declarations. However, !important declarations have a
  // higher priority than animation effects [1]. If we're currently animating
  // (not transitioning) a property which was declared !important in the base
  // style, we disable the base computed style optimization.
  // [1] https://drafts.csswg.org/css-cascade-4/#cascade-origin
  if (CSSAnimations::IsAnimatingStandardProperties(
          element_animations, element_animations->BaseImportantSet(),
          KeyframeEffect::kDefaultPriority)) {
    return false;
  }

  return true;
}

scoped_refptr<ComputedStyle> StyleResolver::StyleForInterpolations(
    Element& element,
    ActiveInterpolationsMap& interpolations) {
  StyleResolverState state(GetDocument(), element);
  STACK_UNINITIALIZED StyleCascade cascade(state);

  ApplyBaseStyle(&element, state, cascade, cascade.MutableMatchResult(),
                 kMatchAllRules, true);
  ApplyInterpolations(state, cascade, interpolations);

  return state.TakeStyle();
}

void StyleResolver::ApplyInterpolations(
    StyleResolverState& state,
    StyleCascade& cascade,
    ActiveInterpolationsMap& interpolations) {
  cascade.AddInterpolations(&interpolations, CascadeOrigin::kAnimation);
  cascade.Apply();
}

scoped_refptr<ComputedStyle>
StyleResolver::BeforeChangeStyleForTransitionUpdate(
    Element& element,
    const ComputedStyle& base_style,
    ActiveInterpolationsMap& transition_interpolations) {
  StyleResolverState state(GetDocument(), element);
  STACK_UNINITIALIZED StyleCascade cascade(state);
  state.SetStyle(ComputedStyle::Clone(base_style));

  // Various property values may depend on the parent style. A valid parent
  // style is required, even if animating the root element, in order to
  // handle these dependencies. The root element inherits from initial
  // styles.
  if (!state.ParentStyle()) {
    if (element != GetDocument().documentElement()) {
      // Do not apply interpolations to a detached element.
      return state.TakeStyle();
    }
    state.SetParentStyle(InitialStyleForElement(GetDocument()));
    state.SetLayoutParentStyle(state.ParentStyle());
  }

  // TODO(crbug.com/1098937): Include active CSS animations in a separate
  // interpolations map and add each map at the appropriate CascadeOrigin.
  ApplyInterpolations(state, cascade, transition_interpolations);
  return state.TakeStyle();
}

void StyleResolver::CascadeAndApplyMatchedProperties(StyleResolverState& state,
                                                     StyleCascade& cascade) {
  const MatchResult& result = cascade.GetMatchResult();

  CacheSuccess cache_success = ApplyMatchedCache(state, result);

  if (cache_success.IsFullCacheHit())
    return;

  if (cache_success.ShouldApplyInheritedOnly()) {
    cascade.Apply(CascadeFilter(CSSProperty::kInherited, false));
    if (!cache_success.IsUsableAfterApplyInheritedOnly(state.StyleRef()))
      cascade.Apply(CascadeFilter(CSSProperty::kInherited, true));
  } else {
    cascade.Apply();
  }

  MaybeAddToMatchedPropertiesCache(state, cache_success, result);

  DCHECK(!state.GetFontBuilder().FontDirty());
}

void StyleResolver::ApplyCallbackSelectors(StyleResolverState& state) {
  RuleSet* watched_selectors_rule_set =
      GetDocument().GetStyleEngine().WatchedSelectorsRuleSet();
  if (!watched_selectors_rule_set)
    return;

  MatchResult match_result;
  ElementRuleCollector collector(state.ElementContext(), selector_filter_,
                                 match_result, state.Style(),
                                 state.Style()->InsideLink());
  collector.SetMode(SelectorChecker::kCollectingStyleRules);
  collector.SetIncludeEmptyRules(true);

  MatchRequest match_request(watched_selectors_rule_set);
  collector.CollectMatchingRules(match_request);
  collector.SortAndTransferMatchedRules();

  if (tracker_)
    AddMatchedRulesToTracker(collector);

  StyleRuleList* rules = collector.MatchedStyleRuleList();
  if (!rules)
    return;
  for (auto rule : *rules)
    state.Style()->AddCallbackSelector(rule->SelectorList().SelectorsText());
}

// Font properties are also handled by FontStyleResolver outside the main
// thread. If you add/remove properties here, make sure they are also properly
// handled by FontStyleResolver.
void StyleResolver::ComputeFont(Element& element,
                                ComputedStyle* style,
                                const CSSPropertyValueSet& property_set) {
  static const CSSProperty* properties[7] = {
      &GetCSSPropertyFontSize(),        &GetCSSPropertyFontFamily(),
      &GetCSSPropertyFontStretch(),     &GetCSSPropertyFontStyle(),
      &GetCSSPropertyFontVariantCaps(), &GetCSSPropertyFontWeight(),
      &GetCSSPropertyLineHeight(),
  };

  // TODO(timloh): This is weird, the style is being used as its own parent
  StyleResolverState state(GetDocument(), element, style, style);
  state.SetStyle(style);

  for (const CSSProperty* property : properties) {
    if (property->IDEquals(CSSPropertyID::kLineHeight))
      UpdateFont(state);
    StyleBuilder::ApplyProperty(
        *property, state,
        *property_set.GetPropertyCSSValue(property->PropertyID()));
  }
}

void StyleResolver::UpdateMediaType() {
  if (LocalFrameView* view = GetDocument().View()) {
    bool was_print = print_media_type_;
    print_media_type_ =
        EqualIgnoringASCIICase(view->MediaType(), media_type_names::kPrint);
    if (was_print != print_media_type_)
      matched_properties_cache_.ClearViewportDependent();
  }
}

void StyleResolver::Trace(Visitor* visitor) const {
  visitor->Trace(matched_properties_cache_);
  visitor->Trace(selector_filter_);
  visitor->Trace(document_);
  visitor->Trace(tracker_);
}

bool StyleResolver::IsForcedColorsModeEnabled() const {
  return GetDocument().InForcedColorsMode();
}

bool StyleResolver::IsForcedColorsModeEnabled(
    const StyleResolverState& state) const {
  return IsForcedColorsModeEnabled() &&
         state.Style()->ForcedColorAdjust() != EForcedColorAdjust::kNone;
}

}  // namespace blink
