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
#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_pending_interpolation_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_reflect_value.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_selector_watch.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/element_rule_collector.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/page_rule_collector.h"
#include "third_party/blink/renderer/core/css/part_names.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/resolver/css_variable_animator.h"
#include "third_party/blink/renderer/core/css/resolver/css_variable_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/selector_filter_parent_scope.h"
#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"
#include "third_party/blink/renderer/core/css/resolver/style_animator.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
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
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/style/style_inherited_variables.h"
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
  if (!state.AnimationUpdate().IsEmpty())
    element.EnsureElementAnimations().CssAnimations().SetPendingUpdate(
        state.AnimationUpdate());
}

bool HasAnimationsOrTransitions(const StyleResolverState& state) {
  return state.Style()->Animations() || state.Style()->Transitions() ||
         (state.GetAnimatingElement() &&
          state.GetAnimatingElement()->HasAnimations());
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
  if (element.ShadowPseudoId() != "-webkit-input-placeholder")
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
  if (element.ShadowPseudoId() != "-webkit-input-placeholder")
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

void StyleResolver::MatchUARules(ElementRuleCollector& collector) {
  collector.SetMatchingUARules(true);

  CSSDefaultStyleSheets& default_style_sheets =
      CSSDefaultStyleSheets::Instance();
  RuleSet* user_agent_style_sheet =
      print_media_type_ ? default_style_sheets.DefaultPrintStyle()
                        : default_style_sheets.DefaultStyle();
  MatchRuleSet(collector, user_agent_style_sheet);

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
  MatchUARules(collector);
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

scoped_refptr<ComputedStyle> StyleResolver::StyleForViewport(
    Document& document) {
  scoped_refptr<ComputedStyle> viewport_style =
      InitialStyleForElement(document);

  viewport_style->SetZIndex(0);
  viewport_style->SetIsStackingContext(true);
  viewport_style->SetDisplay(EDisplay::kBlock);
  viewport_style->SetPosition(EPosition::kAbsolute);

  // Document::InheritHtmlAndBodyElementStyles will set the final overflow
  // style values, but they should initially be auto to avoid premature
  // scrollbar removal in PaintLayerScrollableArea::UpdateAfterStyleChange.
  viewport_style->SetOverflowX(EOverflow::kAuto);
  viewport_style->SetOverflowY(EOverflow::kAuto);

  return viewport_style;
}

// Start loading resources referenced by this style.
void StyleResolver::LoadPendingResources(StyleResolverState& state) {
  state.GetElementStyleResources().LoadPendingResources(state.Style());
}

static const ComputedStyle* CachedAnimationBaseComputedStyle(
    StyleResolverState& state) {
  if (!state.GetAnimatingElement())
    return nullptr;

  ElementAnimations* element_animations =
      state.GetAnimatingElement()->GetElementAnimations();
  if (!element_animations)
    return nullptr;

  if (CSSAnimations::IsAnimatingCustomProperties(element_animations)) {
    state.SetIsAnimatingCustomProperties(true);
    // TODO(alancutter): Use the base computed style optimisation in the
    // presence of custom property animations that don't affect pre-animated
    // computed values.
    return nullptr;
  }

  return element_animations->BaseComputedStyle();
}

static void UpdateAnimationBaseComputedStyle(StyleResolverState& state) {
  if (!state.GetAnimatingElement())
    return;

  ElementAnimations* element_animations =
      state.GetAnimatingElement()->GetElementAnimations();
  if (element_animations) {
    if (state.IsAnimatingCustomProperties()) {
      element_animations->ClearBaseComputedStyle();
    } else {
      element_animations->UpdateBaseComputedStyle(state.Style());
    }
  }
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
  const ComputedStyle* animation_base_computed_style =
      can_cache_animation_base_computed_style
          ? CachedAnimationBaseComputedStyle(state)
          : nullptr;

  if (animation_base_computed_style) {
    state.SetStyle(ComputedStyle::Clone(*animation_base_computed_style));
    if (!state.ParentStyle()) {
      state.SetParentStyle(InitialStyleForElement(GetDocument()));
      state.SetLayoutParentStyle(state.ParentStyle());
    }
  } else {
    if (state.ParentStyle()) {
      scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
      style->InheritFrom(*state.ParentStyle(),
                         IsAtShadowBoundary(element)
                             ? ComputedStyle::kAtShadowBoundary
                             : ComputedStyle::kNotAtShadowBoundary);
      state.SetStyle(std::move(style));
    } else {
      state.SetStyle(InitialStyleForElement(GetDocument()));
      state.SetParentStyle(ComputedStyle::Clone(*state.Style()));
      state.SetLayoutParentStyle(state.ParentStyle());
      if (element != GetDocument().documentElement()) {
        // Strictly, we should only allow the root element to inherit from
        // initial styles, but we allow getComputedStyle() for connected
        // elements outside the flat tree rooted at an unassigned shadow host
        // child, or Shadow DOM V0 insertion points.
        DCHECK(element->IsV0InsertionPoint() ||
               (IsShadowHost(element->parentNode()) &&
                !LayoutTreeBuilderTraversal::ParentElement(*element)));
        state.Style()->SetIsEnsuredOutsideFlatTree();
      }
    }
  }

  // contenteditable attribute (implemented by -webkit-user-modify) should
  // be propagated from shadow host to distributed node.
  if (state.DistributedToV0InsertionPoint() || element->AssignedSlot()) {
    if (Element* parent = element->parentElement()) {
      if (const ComputedStyle* style_of_shadow_host =
              parent->GetComputedStyle()) {
        state.Style()->SetUserModify(style_of_shadow_host->UserModify());
      }
    }
  }

  if (element->IsLink()) {
    state.Style()->SetIsLink();
    EInsideLink link_state = state.ElementLinkState();
    if (link_state != EInsideLink::kNotInsideLink) {
      bool force_visited = false;
      probe::ForcePseudoState(element, CSSSelector::kPseudoVisited,
                              &force_visited);
      if (force_visited)
        link_state = EInsideLink::kInsideVisitedLink;
    }
    state.Style()->SetInsideLink(link_state);
  }

  if (!animation_base_computed_style) {
    GetDocument().GetStyleEngine().EnsureUAStyleForElement(*element);

    ElementRuleCollector collector(state.ElementContext(), selector_filter_,
                                   state.Style());

    MatchAllRules(state, collector,
                  matching_behavior != kMatchAllRulesExcludingSMIL);

    // TODO(dominicc): Remove this counter when Issue 590014 is fixed.
    if (element->HasTagName(html_names::kSummaryTag)) {
      MatchedPropertiesRange matched_range =
          collector.MatchedResult().AuthorRules();
      for (const auto& matched : matched_range) {
        const CSSValue* value =
            matched.properties->GetPropertyCSSValue(CSSPropertyID::kDisplay);
        auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
        if (identifier_value &&
            identifier_value->GetValueID() == CSSValueID::kBlock) {
          UseCounter::Count(
              element->GetDocument(),
              WebFeature::kSummaryElementWithDisplayBlockAuthorRule);
        }
      }
    }

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

    ApplyMatchedProperties(state, collector.MatchedResult());
    ApplyCallbackSelectors(state);

    // Cache our original display.
    state.Style()->SetOriginalDisplay(state.Style()->Display());

    StyleAdjuster::AdjustComputedStyle(state, element);

    if (can_cache_animation_base_computed_style)
      UpdateAnimationBaseComputedStyle(state);
  } else {
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  base_styles_used, 1);
  }

  // FIXME: The CSSWG wants to specify that the effects of animations are
  // applied before important rules, but this currently happens here as we
  // require adjustment to have happened before deciding which properties to
  // transition.
  if (ApplyAnimatedStandardProperties(state)) {
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  styles_animated, 1);
    StyleAdjuster::AdjustComputedStyle(state, element);
  }

  if (IsA<HTMLBodyElement>(*element))
    GetDocument().GetTextLinkColors().SetTextColor(state.Style()->GetColor());

  SetAnimationUpdateIfNeeded(state, *element);

  if (state.Style()->HasViewportUnits())
    GetDocument().SetHasViewportUnits();

  if (state.Style()->HasRemUnits())
    GetDocument().GetStyleEngine().SetUsesRemUnit(true);

  if (state.Style()->HasGlyphRelativeUnits())
    UseCounter::Count(GetDocument(), WebFeature::kHasGlyphRelativeUnits);

  // Now return the style.
  return state.TakeStyle();
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
    if (RuntimeEnabledFeatures::CSSCascadeEnabled()) {
      StyleCascade cascade(state);
      auto name = property.GetCSSPropertyName();
      cascade.Add(name, value, StyleCascade::Origin::kAuthor);
      cascade.Apply();
    } else {
      StyleBuilder::ApplyProperty(property.GetCSSPropertyName(), state, *value);
      state.GetFontBuilder().CreateFont(
          state.GetDocument().GetStyleEngine().GetFontSelector(),
          state.StyleRef());
      CSSVariableResolver(state).ResolveVariableDefinitions();
    }
  }
  return CompositorKeyframeValueFactory::Create(property, *state.Style());
}

bool StyleResolver::PseudoStyleForElementInternal(
    Element& element,
    const PseudoElementStyleRequest& pseudo_style_request,
    StyleResolverState& state) {
  DCHECK(GetDocument().GetFrame());
  DCHECK(GetDocument().GetSettings());
  DCHECK(pseudo_style_request.pseudo_id != kPseudoIdFirstLineInherited);
  DCHECK(state.ParentStyle());

  SelectorFilterParentScope::EnsureParentStackIsPushed();

  const ComputedStyle* animation_base_computed_style =
      CachedAnimationBaseComputedStyle(state);

  if (animation_base_computed_style) {
    state.SetStyle(ComputedStyle::Clone(*animation_base_computed_style));
  } else if (pseudo_style_request.AllowsInheritance(state.ParentStyle())) {
    scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
    style->InheritFrom(*state.ParentStyle());
    state.SetStyle(std::move(style));
  } else {
    // ::backdrop inherits from initial styles. All other pseudo elements
    // inherit from their originating element (::before/::after), or originating
    // element descendants (::first-line/::first-letter).
    DCHECK(pseudo_style_request.pseudo_id == kPseudoIdBackdrop);
    state.SetStyle(InitialStyleForElement(GetDocument()));
    state.SetParentStyle(ComputedStyle::Clone(*state.Style()));
  }

  state.Style()->SetStyleType(pseudo_style_request.pseudo_id);

  // Since we don't use pseudo-elements in any of our quirk/print
  // user agent rules, don't waste time walking those rules.

  if (!animation_base_computed_style) {
    // Check UA, user and author rules.
    ElementRuleCollector collector(state.ElementContext(), selector_filter_,
                                   state.Style());
    collector.SetPseudoElementStyleRequest(pseudo_style_request);

    MatchUARules(collector);
    MatchUserRules(collector);
    MatchAuthorRules(state.GetElement(), collector);
    collector.FinishAddingAuthorRulesForTreeScope();

    if (tracker_)
      AddMatchedRulesToTracker(collector);

    if (!collector.MatchedResult().HasMatchedProperties())
      return false;

    ApplyMatchedProperties(state, collector.MatchedResult());
    ApplyCallbackSelectors(state);

    // Cache our original display.
    state.Style()->SetOriginalDisplay(state.Style()->Display());

    // FIXME: Passing 0 as the Element* introduces a lot of complexity
    // in the StyleAdjuster::AdjustComputedStyle code.
    StyleAdjuster::AdjustComputedStyle(state, nullptr);

    UpdateAnimationBaseComputedStyle(state);
  }

  // FIXME: The CSSWG wants to specify that the effects of animations are
  // applied before important rules, but this currently happens here as we
  // require adjustment to have happened before deciding which properties to
  // transition.
  if (ApplyAnimatedStandardProperties(state))
    StyleAdjuster::AdjustComputedStyle(state, nullptr);

  GetDocument().GetStyleEngine().IncStyleForElementCount();
  INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                pseudo_elements_styled, 1);

  if (state.Style()->HasViewportUnits())
    GetDocument().SetHasViewportUnits();

  if (state.Style()->HasGlyphRelativeUnits())
    UseCounter::Count(GetDocument(), WebFeature::kHasGlyphRelativeUnits);

  return true;
}

scoped_refptr<ComputedStyle> StyleResolver::PseudoStyleForElement(
    Element* element,
    const PseudoElementStyleRequest& pseudo_style_request,
    const ComputedStyle* parent_style,
    const ComputedStyle* parent_layout_object_style) {
  DCHECK(parent_style);
  if (!element)
    return nullptr;

  StyleResolverState state(GetDocument(), *element,
                           pseudo_style_request.pseudo_id, parent_style,
                           parent_layout_object_style);
  if (!PseudoStyleForElementInternal(*element, pseudo_style_request, state)) {
    if (pseudo_style_request.type == PseudoElementStyleRequest::kForRenderer)
      return nullptr;
    return state.TakeStyle();
  }

  if (PseudoElement* pseudo_element =
          element->GetPseudoElement(pseudo_style_request.pseudo_id))
    SetAnimationUpdateIfNeeded(state, *pseudo_element);

  // Now return the style.
  return state.TakeStyle();
}

scoped_refptr<const ComputedStyle> StyleResolver::StyleForPage(int page_index) {
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

  PageRuleCollector collector(root_element_style, page_index);

  collector.MatchPageRules(
      CSSDefaultStyleSheets::Instance().DefaultPrintStyle());

  if (ScopedStyleResolver* scoped_resolver =
          GetDocument().GetScopedStyleResolver())
    scoped_resolver->MatchPageRules(collector);

  bool inherited_only = false;

  NeedsApplyPass needs_apply_pass;
  const MatchResult& result = collector.MatchedResult();
  ApplyMatchedProperties<kAnimationPropertyPriority, kUpdateNeedsApplyPass>(
      state, result.AllRules(), false, inherited_only, needs_apply_pass);
  ApplyMatchedProperties<kHighPropertyPriority, kCheckNeedsApplyPass>(
      state, result.AllRules(), false, inherited_only, needs_apply_pass);

  // If our font got dirtied, go ahead and update it now.
  UpdateFont(state);

  ApplyMatchedProperties<kLowPropertyPriority, kCheckNeedsApplyPass>(
      state, result.AllRules(), false, inherited_only, needs_apply_pass);

  LoadPendingResources(state);

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
  state.GetFontBuilder().CreateFont(
      GetDocument().GetStyleEngine().GetFontSelector(), state.StyleRef());
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
  ElementRuleCollector collector(state.ElementContext(), selector_filter_,
                                 state.Style());
  collector.SetMode(SelectorChecker::kCollectingStyleRules);
  CollectPseudoRulesForElement(*element, collector, kPseudoIdNone,
                               rules_to_include);
  return collector.MatchedStyleRuleList();
}

RuleIndexList* StyleResolver::PseudoCSSRulesForElement(
    Element* element,
    PseudoId pseudo_id,
    unsigned rules_to_include) {
  DCHECK(element);
  StyleResolverState state(GetDocument(), *element);
  ElementRuleCollector collector(state.ElementContext(), selector_filter_,
                                 state.Style());
  collector.SetMode(SelectorChecker::kCollectingCSSRules);
  CollectPseudoRulesForElement(*element, collector, pseudo_id,
                               rules_to_include);

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

  if (rules_to_include & kUAAndUserCSSRules) {
    MatchUARules(collector);
    MatchUserRules(collector);
  }

  if (rules_to_include & kAuthorCSSRules) {
    collector.SetSameOriginOnly(!(rules_to_include & kCrossOriginCSSRules));
    collector.SetIncludeEmptyRules(rules_to_include & kEmptyCSSRules);
    MatchAuthorRules(element, collector);
  }
}

bool StyleResolver::ApplyAnimatedStandardProperties(StyleResolverState& state) {
  Element& element = state.GetElement();

  // The animating element may be this element, the pseudo element we are
  // resolving style for, or null if we are resolving style for a pseudo
  // element which is not represented by a PseudoElement like scrollbar pseudo
  // elements.
  const Element* animating_element = state.GetAnimatingElement();
  DCHECK(animating_element == &element || !animating_element ||
         animating_element->ParentOrShadowHostElement() == element);

  if (state.Style()->Animations() ||
      (animating_element && animating_element->HasAnimations())) {
    if (!state.IsAnimationInterpolationMapReady())
      CalculateAnimationUpdate(state);
  } else if (!state.Style()->Transitions()) {
    return false;
  }

  CSSAnimations::CalculateCompositorAnimationUpdate(
      state.AnimationUpdate(), animating_element, element, *state.Style(),
      state.ParentStyle(), WasViewportResized());
  CSSAnimations::CalculateTransitionUpdate(
      state.AnimationUpdate(), CSSAnimations::PropertyPass::kStandard,
      animating_element, *state.Style());

  CSSAnimations::SnapshotCompositorKeyframes(
      element, state.AnimationUpdate(), *state.Style(), state.ParentStyle());

  if (state.AnimationUpdate().IsEmpty())
    return false;

  const ActiveInterpolationsMap& animations_map =
      state.AnimationUpdate().ActiveInterpolationsForStandardAnimations();
  const ActiveInterpolationsMap& transitions_map =
      state.AnimationUpdate().ActiveInterpolationsForStandardTransitions();

  if (RuntimeEnabledFeatures::CSSCascadeEnabled()) {
    // TODO(crbug.com/985049): Use main cascade.
    //
    // For now, we use a dedicated cascade for animation of standard properties.
    // A StyleCascade is required, because otherwise we can't resolve any var()
    // references that may appear in keyframes. Ultimately, we should use ONE
    // cascade for everything, but this is not yet possible.
    using Origin = StyleCascade::Origin;
    StyleCascade cascade(state);
    StyleAnimator animator(state, cascade);
    CascadeInterpolations(cascade, animations_map, Origin::kAnimation);
    CascadeInterpolations(cascade, transitions_map, Origin::kTransition);
    cascade.Apply(animator);
  } else {
    ApplyAnimatedStandardProperties<kHighPropertyPriority>(state,
                                                           animations_map);
    ApplyAnimatedStandardProperties<kHighPropertyPriority>(state,
                                                           transitions_map);

    UpdateFont(state);

    ApplyAnimatedStandardProperties<kLowPropertyPriority>(state,
                                                          animations_map);
    ApplyAnimatedStandardProperties<kLowPropertyPriority>(state,
                                                          transitions_map);
  }

  // Start loading resources used by animations.
  LoadPendingResources(state);

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

template <CSSPropertyPriority priority>
void StyleResolver::ApplyAnimatedStandardProperties(
    StyleResolverState& state,
    const ActiveInterpolationsMap& active_interpolations_map) {
  static_assert(priority != kResolveVariables,
                "Use CSSVariableAnimator for custom property animations");
  // TODO(alancutter): Don't apply presentation attribute animations here,
  // they should instead apply in
  // SVGElement::CollectStyleForPresentationAttribute().
  for (const auto& entry : active_interpolations_map) {
    CSSPropertyID property =
        entry.key.IsCSSProperty()
            ? entry.key.GetCSSProperty().PropertyID()
            : entry.key.PresentationAttribute().PropertyID();
    if (!CSSPropertyPriorityData<priority>::PropertyHasPriority(property))
      continue;
    if (IsForcedColorsModeEnabled() && entry.key.IsCSSProperty() &&
        entry.key.GetCSSProperty().IsAffectedByForcedColors() &&
        state.Style()->ForcedColorAdjust() != EForcedColorAdjust::kNone)
      continue;
    const Interpolation& interpolation = *entry.value.front();
    if (interpolation.IsInvalidatableInterpolation()) {
      CSSInterpolationTypesMap map(state.GetDocument().GetPropertyRegistry(),
                                   state.GetDocument());
      CSSInterpolationEnvironment environment(map, state, nullptr);
      InvalidatableInterpolation::ApplyStack(entry.value, environment);
    } else {
      ToTransitionInterpolation(interpolation).Apply(state);
    }
  }
}

static inline bool IsValidCueStyleProperty(CSSPropertyID id) {
  switch (id) {
    case CSSPropertyID::kBackground:
    case CSSPropertyID::kBackgroundAttachment:
    case CSSPropertyID::kBackgroundClip:
    case CSSPropertyID::kBackgroundColor:
    case CSSPropertyID::kBackgroundImage:
    case CSSPropertyID::kBackgroundOrigin:
    case CSSPropertyID::kBackgroundPosition:
    case CSSPropertyID::kBackgroundPositionX:
    case CSSPropertyID::kBackgroundPositionY:
    case CSSPropertyID::kBackgroundRepeat:
    case CSSPropertyID::kBackgroundRepeatX:
    case CSSPropertyID::kBackgroundRepeatY:
    case CSSPropertyID::kBackgroundSize:
    case CSSPropertyID::kColor:
    case CSSPropertyID::kFont:
    case CSSPropertyID::kFontFamily:
    case CSSPropertyID::kFontSize:
    case CSSPropertyID::kFontStretch:
    case CSSPropertyID::kFontStyle:
    case CSSPropertyID::kFontVariant:
    case CSSPropertyID::kFontWeight:
    case CSSPropertyID::kLineHeight:
    case CSSPropertyID::kOpacity:
    case CSSPropertyID::kOutline:
    case CSSPropertyID::kOutlineColor:
    case CSSPropertyID::kOutlineOffset:
    case CSSPropertyID::kOutlineStyle:
    case CSSPropertyID::kOutlineWidth:
    case CSSPropertyID::kVisibility:
    case CSSPropertyID::kWhiteSpace:
    // FIXME: 'text-decoration' shorthand to be handled when available.
    // See https://chromiumcodereview.appspot.com/19516002 for details.
    case CSSPropertyID::kTextDecoration:
    case CSSPropertyID::kTextShadow:
    case CSSPropertyID::kBorderStyle:
      return true;
    case CSSPropertyID::kTextDecorationLine:
    case CSSPropertyID::kTextDecorationStyle:
    case CSSPropertyID::kTextDecorationColor:
    case CSSPropertyID::kTextDecorationSkipInk:
      return true;
    case CSSPropertyID::kFontVariationSettings:
      return true;
    default:
      break;
  }
  return false;
}

static inline bool IsValidFirstLetterStyleProperty(CSSPropertyID id) {
  switch (id) {
    // Valid ::first-letter properties listed in spec:
    // https://drafts.csswg.org/css-pseudo-4/#first-letter-styling
    case CSSPropertyID::kBackgroundAttachment:
    case CSSPropertyID::kBackgroundBlendMode:
    case CSSPropertyID::kBackgroundClip:
    case CSSPropertyID::kBackgroundColor:
    case CSSPropertyID::kBackgroundImage:
    case CSSPropertyID::kBackgroundOrigin:
    case CSSPropertyID::kBackgroundPosition:
    case CSSPropertyID::kBackgroundPositionX:
    case CSSPropertyID::kBackgroundPositionY:
    case CSSPropertyID::kBackgroundRepeat:
    case CSSPropertyID::kBackgroundRepeatX:
    case CSSPropertyID::kBackgroundRepeatY:
    case CSSPropertyID::kBackgroundSize:
    case CSSPropertyID::kBorderBlockEnd:
    case CSSPropertyID::kBorderBlockEndColor:
    case CSSPropertyID::kBorderBlockEndStyle:
    case CSSPropertyID::kBorderBlockEndWidth:
    case CSSPropertyID::kBorderBlockStart:
    case CSSPropertyID::kBorderBlockStartColor:
    case CSSPropertyID::kBorderBlockStartStyle:
    case CSSPropertyID::kBorderBlockStartWidth:
    case CSSPropertyID::kBorderBottomColor:
    case CSSPropertyID::kBorderBottomLeftRadius:
    case CSSPropertyID::kBorderBottomRightRadius:
    case CSSPropertyID::kBorderBottomStyle:
    case CSSPropertyID::kBorderBottomWidth:
    case CSSPropertyID::kBorderImageOutset:
    case CSSPropertyID::kBorderImageRepeat:
    case CSSPropertyID::kBorderImageSlice:
    case CSSPropertyID::kBorderImageSource:
    case CSSPropertyID::kBorderImageWidth:
    case CSSPropertyID::kBorderInlineEnd:
    case CSSPropertyID::kBorderInlineEndColor:
    case CSSPropertyID::kBorderInlineEndStyle:
    case CSSPropertyID::kBorderInlineEndWidth:
    case CSSPropertyID::kBorderInlineStart:
    case CSSPropertyID::kBorderInlineStartColor:
    case CSSPropertyID::kBorderInlineStartStyle:
    case CSSPropertyID::kBorderInlineStartWidth:
    case CSSPropertyID::kBorderLeftColor:
    case CSSPropertyID::kBorderLeftStyle:
    case CSSPropertyID::kBorderLeftWidth:
    case CSSPropertyID::kBorderRightColor:
    case CSSPropertyID::kBorderRightStyle:
    case CSSPropertyID::kBorderRightWidth:
    case CSSPropertyID::kBorderTopColor:
    case CSSPropertyID::kBorderTopLeftRadius:
    case CSSPropertyID::kBorderTopRightRadius:
    case CSSPropertyID::kBorderTopStyle:
    case CSSPropertyID::kBorderTopWidth:
    case CSSPropertyID::kBoxShadow:
    case CSSPropertyID::kColor:
    case CSSPropertyID::kFloat:
    case CSSPropertyID::kFontFamily:
    case CSSPropertyID::kFontFeatureSettings:
    case CSSPropertyID::kFontKerning:
    case CSSPropertyID::kFontOpticalSizing:
    case CSSPropertyID::kFontSize:
    case CSSPropertyID::kFontSizeAdjust:
    case CSSPropertyID::kFontStretch:
    case CSSPropertyID::kFontStyle:
    case CSSPropertyID::kFontVariant:
    case CSSPropertyID::kFontVariantCaps:
    case CSSPropertyID::kFontVariantLigatures:
    case CSSPropertyID::kFontVariantNumeric:
    case CSSPropertyID::kFontVariantEastAsian:
    case CSSPropertyID::kFontVariationSettings:
    case CSSPropertyID::kFontWeight:
    case CSSPropertyID::kLetterSpacing:
    case CSSPropertyID::kLineHeight:
    case CSSPropertyID::kMarginBlockEnd:
    case CSSPropertyID::kMarginBlockStart:
    case CSSPropertyID::kMarginBottom:
    case CSSPropertyID::kMarginInlineEnd:
    case CSSPropertyID::kMarginInlineStart:
    case CSSPropertyID::kMarginLeft:
    case CSSPropertyID::kMarginRight:
    case CSSPropertyID::kMarginTop:
    case CSSPropertyID::kOpacity:
    case CSSPropertyID::kPaddingBottom:
    case CSSPropertyID::kPaddingLeft:
    case CSSPropertyID::kPaddingRight:
    case CSSPropertyID::kPaddingTop:
    case CSSPropertyID::kTextDecorationColor:
    case CSSPropertyID::kTextDecorationLine:
    case CSSPropertyID::kTextDecorationStyle:
    case CSSPropertyID::kTextDecorationSkipInk:
    case CSSPropertyID::kTextJustify:
    case CSSPropertyID::kTextShadow:
    case CSSPropertyID::kTextTransform:
    case CSSPropertyID::kTextUnderlinePosition:
    case CSSPropertyID::kVerticalAlign:
    case CSSPropertyID::kWebkitBorderHorizontalSpacing:
    case CSSPropertyID::kWebkitBorderImage:
    case CSSPropertyID::kWebkitBorderVerticalSpacing:
    case CSSPropertyID::kWebkitFontSmoothing:
    case CSSPropertyID::kWebkitMarginAfterCollapse:
    case CSSPropertyID::kWebkitMarginBeforeCollapse:
    case CSSPropertyID::kWebkitMarginBottomCollapse:
    case CSSPropertyID::kWebkitMarginCollapse:
    case CSSPropertyID::kWebkitMarginTopCollapse:
    case CSSPropertyID::kWordSpacing:
      return true;

    // Not directly specified in spec, but variables should be supported nearly
    // anywhere.
    case CSSPropertyID::kVariable:
    // Properties that we currently support outside of spec.
    case CSSPropertyID::kVisibility:
      return true;

    default:
      return false;
  }
}

static inline bool IsValidMarkerStyleProperty(CSSPropertyID id) {
  switch (id) {
    // Valid ::marker properties listed in spec:
    // https://drafts.csswg.org/css-pseudo-4/#marker-pseudo
    case CSSPropertyID::kColor:
    case CSSPropertyID::kContent:
    case CSSPropertyID::kDirection:
    case CSSPropertyID::kFont:
    case CSSPropertyID::kFontFamily:
    case CSSPropertyID::kFontFeatureSettings:
    case CSSPropertyID::kFontKerning:
    case CSSPropertyID::kFontOpticalSizing:
    case CSSPropertyID::kFontSize:
    case CSSPropertyID::kFontSizeAdjust:
    case CSSPropertyID::kFontStretch:
    case CSSPropertyID::kFontStyle:
    case CSSPropertyID::kFontVariant:
    case CSSPropertyID::kFontVariantCaps:
    case CSSPropertyID::kFontVariantEastAsian:
    case CSSPropertyID::kFontVariantLigatures:
    case CSSPropertyID::kFontVariantNumeric:
    case CSSPropertyID::kFontVariationSettings:
    case CSSPropertyID::kFontWeight:
    case CSSPropertyID::kTextCombineUpright:
    case CSSPropertyID::kUnicodeBidi:
      return true;

    // Not directly specified in spec, but variables should be supported nearly
    // anywhere.
    case CSSPropertyID::kVariable:
      return true;

    default:
      return false;
  }
}

static bool PassesPropertyFilter(ValidPropertyFilter valid_property_filter,
                                 CSSPropertyID property,
                                 const Document& document) {
  switch (valid_property_filter) {
    case ValidPropertyFilter::kNoFilter:
      return true;
    case ValidPropertyFilter::kFirstLetter:
      return IsValidFirstLetterStyleProperty(property);
    case ValidPropertyFilter::kCue:
      return IsValidCueStyleProperty(property);
    case ValidPropertyFilter::kMarker:
      return IsValidMarkerStyleProperty(property);
  }
  NOTREACHED();
  return true;
}

static inline void ApplyProperty(const CSSProperty& property,
                                 StyleResolverState& state,
                                 const CSSValue& value,
                                 unsigned apply_mask) {
  if (apply_mask & kApplyMaskRegular)
    StyleBuilder::ApplyProperty(property, state, value);
  if (apply_mask & kApplyMaskVisited) {
    if (const CSSProperty* visited = property.GetVisitedProperty())
      StyleBuilder::ApplyProperty(*visited, state, value);
  }
}

// This method expands the 'all' shorthand property to longhand properties
// and applies the expanded longhand properties.
template <CSSPropertyPriority priority>
void StyleResolver::ApplyAllProperty(StyleResolverState& state,
                                     const CSSValue& all_value,
                                     bool inherited_only,
                                     ValidPropertyFilter valid_property_filter,
                                     unsigned apply_mask) {
  // The 'all' property doesn't apply to variables:
  // https://drafts.csswg.org/css-variables/#defining-variables
  if (priority == kResolveVariables)
    return;

  unsigned start_css_property =
      static_cast<unsigned>(CSSPropertyPriorityData<priority>::First());
  unsigned end_css_property =
      static_cast<unsigned>(CSSPropertyPriorityData<priority>::Last());

  for (unsigned i = start_css_property; i <= end_css_property; ++i) {
    CSSPropertyID property_id = static_cast<CSSPropertyID>(i);
    const CSSProperty& property_class =
        CSSProperty::Get(resolveCSSPropertyID(property_id));

    // StyleBuilder does not allow any expanded shorthands.
    if (property_class.IsShorthand())
      continue;

    // all shorthand spec says:
    // The all property is a shorthand that resets all CSS properties
    // except direction and unicode-bidi.
    // c.f. https://drafts.csswg.org/css-cascade/#all-shorthand
    // We skip applyProperty when a given property is unicode-bidi or
    // direction.
    if (!property_class.IsAffectedByAll())
      continue;

    if (!PassesPropertyFilter(valid_property_filter, property_id,
                              GetDocument()))
      continue;

    // When hitting matched properties' cache, only inherited properties will be
    // applied.
    if (inherited_only && !property_class.IsInherited())
      continue;

    ApplyProperty(property_class, state, all_value, apply_mask);
  }
}

template <CSSPropertyPriority priority>
static inline void ApplyProperty(
    const CSSPropertyValueSet::PropertyReference& reference,
    StyleResolverState& state,
    unsigned apply_mask) {
  static_assert(
      priority != kResolveVariables,
      "Application of custom properties must use specialized template");
  DCHECK_NE(reference.Id(), CSSPropertyID::kVariable);
  ApplyProperty(reference.Property(), state, reference.Value(), apply_mask);
}

template <>
inline void ApplyProperty<kResolveVariables>(
    const CSSPropertyValueSet::PropertyReference& reference,
    StyleResolverState& state,
    unsigned apply_mask) {
  CSSPropertyRef ref(reference.Name(), state.GetDocument());
  ApplyProperty(ref.GetProperty(), state, reference.Value(), apply_mask);
}

template <CSSPropertyPriority priority,
          StyleResolver::ShouldUpdateNeedsApplyPass shouldUpdateNeedsApplyPass>
void StyleResolver::ApplyProperties(StyleResolverState& state,
                                    const CSSPropertyValueSet* properties,
                                    bool is_important,
                                    bool inherited_only,
                                    NeedsApplyPass& needs_apply_pass,
                                    ValidPropertyFilter valid_property_filter,
                                    unsigned apply_mask,
                                    ForcedColorFilter forced_colors) {
  unsigned property_count = properties->PropertyCount();
  for (unsigned i = 0; i < property_count; ++i) {
    CSSPropertyValueSet::PropertyReference current = properties->PropertyAt(i);
    CSSPropertyID property_id = current.Id();

    if (property_id == CSSPropertyID::kAll &&
        is_important == current.IsImportant()) {
      if (shouldUpdateNeedsApplyPass) {
        needs_apply_pass.Set(kAnimationPropertyPriority, is_important);
        needs_apply_pass.Set(kHighPropertyPriority, is_important);
        needs_apply_pass.Set(kLowPropertyPriority, is_important);
      }
      ApplyAllProperty<priority>(state, current.Value(), inherited_only,
                                 valid_property_filter, apply_mask);
      continue;
    }

    if (shouldUpdateNeedsApplyPass)
      needs_apply_pass.Set(PriorityForProperty(property_id),
                           current.IsImportant());

    if (is_important != current.IsImportant())
      continue;

    if (!PassesPropertyFilter(valid_property_filter, property_id,
                              GetDocument()))
      continue;

    if (!CSSPropertyPriorityData<priority>::PropertyHasPriority(property_id))
      continue;

    if (inherited_only && !current.IsInherited()) {
      // If the property value is explicitly inherited, we need to apply further
      // non-inherited properties as they might override the value inherited
      // here. For this reason we don't allow declarations with explicitly
      // inherited properties to be cached.
      DCHECK(!current.Value().IsInheritedValue() ||
             (!(apply_mask & kApplyMaskRegular) &&
              (!(apply_mask & kApplyMaskVisited) ||
               !current.Property().GetVisitedProperty())));
      continue;
    }

    if (IsForcedColorsModeEnabled() &&
        forced_colors == ForcedColorFilter::kEnabled &&
        !current.Property().IsAffectedByForcedColors()) {
      continue;
    }

    ApplyProperty<priority>(current, state, apply_mask);
  }
}

static inline unsigned ComputeApplyMask(
    StyleResolverState& state,
    const MatchedProperties& matched_properties) {
  if (state.Style()->InsideLink() == EInsideLink::kNotInsideLink)
    return kApplyMaskRegular;
  static_assert(static_cast<int>(kApplyMaskRegular) ==
                    static_cast<int>(CSSSelector::kMatchLink),
                "kApplyMaskRegular and kMatchLink must match");
  static_assert(static_cast<int>(kApplyMaskVisited) ==
                    static_cast<int>(CSSSelector::kMatchVisited),
                "kApplyMaskVisited and kMatchVisited must match");
  return matched_properties.types_.link_match_type;
}

template <CSSPropertyPriority priority,
          StyleResolver::ShouldUpdateNeedsApplyPass shouldUpdateNeedsApplyPass>
void StyleResolver::ApplyMatchedProperties(StyleResolverState& state,
                                           const MatchedPropertiesRange& range,
                                           bool is_important,
                                           bool inherited_only,
                                           NeedsApplyPass& needs_apply_pass,
                                           ForcedColorFilter forced_colors) {
  if (range.IsEmpty())
    return;

  if (!shouldUpdateNeedsApplyPass &&
      !needs_apply_pass.Get(priority, is_important))
    return;

  for (const auto& matched_properties : range) {
    const unsigned apply_mask = ComputeApplyMask(state, matched_properties);
    ApplyProperties<priority, shouldUpdateNeedsApplyPass>(
        state, matched_properties.properties.Get(), is_important,
        inherited_only, needs_apply_pass,
        static_cast<ValidPropertyFilter>(
            matched_properties.types_.valid_property_filter),
        apply_mask, forced_colors);
  }
}

static unsigned ComputeMatchedPropertiesHash(
    const MatchedProperties* properties,
    unsigned size) {
  return StringHasher::HashMemory(properties, sizeof(MatchedProperties) * size);
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

template <CSSPropertyPriority priority>
void StyleResolver::ApplyForcedColors(StyleResolverState& state,
                                      const MatchResult& match_result,
                                      bool apply_inherited_only,
                                      NeedsApplyPass& needs_apply_pass) {
  if (!IsForcedColorsModeEnabled())
    return;
  if (state.Style()->ForcedColorAdjust() == EForcedColorAdjust::kNone)
    return;

  const CSSValue* unset = cssvalue::CSSUnsetValue::Create();
  unsigned apply_mask = kApplyMaskRegular | kApplyMaskVisited;

  // This simulates 'revert !important' in the user origin.
  // https://drafts.csswg.org/css-color-adjust-1/#forced-colors-properties
  if (priority == kHighPropertyPriority) {
    ApplyProperty(GetCSSPropertyColor(), state, *unset, apply_mask);
    ApplyUaForcedColors<priority>(state, match_result, apply_inherited_only,
                                  needs_apply_pass);
  } else {
    DCHECK(priority == kLowPropertyPriority);
    ApplyProperty(GetCSSPropertyBorderBottomColor(), state, *unset, apply_mask);
    ApplyProperty(GetCSSPropertyBorderLeftColor(), state, *unset, apply_mask);
    ApplyProperty(GetCSSPropertyBorderRightColor(), state, *unset, apply_mask);
    ApplyProperty(GetCSSPropertyBorderTopColor(), state, *unset, apply_mask);
    ApplyProperty(GetCSSPropertyBoxShadow(), state, *unset, apply_mask);
    ApplyProperty(GetCSSPropertyColumnRuleColor(), state, *unset, apply_mask);
    ApplyProperty(GetCSSPropertyFill(), state, *unset, apply_mask);
    ApplyProperty(GetCSSPropertyOutlineColor(), state, *unset, apply_mask);
    ApplyProperty(GetCSSPropertyStroke(), state, *unset, apply_mask);
    ApplyProperty(GetCSSPropertyTextDecorationColor(), state, *unset,
                  apply_mask);
    ApplyProperty(GetCSSPropertyTextShadow(), state, *unset, apply_mask);
    ApplyProperty(GetCSSPropertyWebkitTapHighlightColor(), state, *unset,
                  apply_mask);
    ApplyProperty(GetCSSPropertyWebkitTextEmphasisColor(), state, *unset,
                  apply_mask);

    // Background colors compute to the Window system color for all values
    // except for the alpha channel.
    RGBA32 prev_bg_color = state.Style()->BackgroundColor().GetColor().Rgb();
    RGBA32 sys_bg_color =
        LayoutTheme::GetTheme()
            .SystemColor(CSSValueID::kWindow, WebColorScheme::kLight)
            .Rgb();
    ApplyProperty(GetCSSPropertyBackgroundColor(), state,
                  *cssvalue::CSSColorValue::Create(sys_bg_color), apply_mask);

    ApplyUaForcedColors<priority>(state, match_result, apply_inherited_only,
                                  needs_apply_pass);

    RGBA32 current_bg_color = state.Style()->BackgroundColor().GetColor().Rgb();
    RGBA32 bg_color =
        MakeRGBA(RedChannel(current_bg_color), GreenChannel(current_bg_color),
                 BlueChannel(current_bg_color), AlphaChannel(prev_bg_color));
    ApplyProperty(GetCSSPropertyBackgroundColor(), state,
                  *cssvalue::CSSColorValue::Create(bg_color), apply_mask);
  }
}

template <CSSPropertyPriority priority>
void StyleResolver::ApplyUaForcedColors(StyleResolverState& state,
                                        const MatchResult& match_result,
                                        bool apply_inherited_only,
                                        NeedsApplyPass& needs_apply_pass) {
  auto force_colors = ForcedColorFilter::kEnabled;
  ApplyMatchedProperties<priority, kCheckNeedsApplyPass>(
      state, match_result.UaRules(), false, apply_inherited_only,
      needs_apply_pass, force_colors);
  ApplyMatchedProperties<priority, kCheckNeedsApplyPass>(
      state, match_result.UaRules(), true, apply_inherited_only,
      needs_apply_pass, force_colors);
}

StyleResolver::CacheSuccess StyleResolver::ApplyMatchedCache(
    StyleResolverState& state,
    const MatchResult& match_result) {
  const Element& element = state.GetElement();

  unsigned cache_hash = match_result.IsCacheable()
                            ? ComputeMatchedPropertiesHash(
                                  match_result.GetMatchedProperties().data(),
                                  match_result.GetMatchedProperties().size())
                            : 0;
  bool is_inherited_cache_hit = false;
  bool is_non_inherited_cache_hit = false;
  const CachedMatchedProperties* cached_matched_properties =
      cache_hash ? matched_properties_cache_.Find(
                       cache_hash, state, match_result.GetMatchedProperties())
                 : nullptr;

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
      is_non_inherited_cache_hit = true;
    }
    UpdateFont(state);
  }

  return CacheSuccess(is_inherited_cache_hit, is_non_inherited_cache_hit,
                      cache_hash, cached_matched_properties);
}

void StyleResolver::ApplyCustomProperties(StyleResolverState& state,
                                          const MatchResult& match_result,
                                          const CacheSuccess& cache_success,
                                          NeedsApplyPass& needs_apply_pass) {
  DCHECK(!cache_success.IsFullCacheHit());
  bool apply_inherited_only = cache_success.ShouldApplyInheritedOnly();

  // TODO(leviw): We need the proper bit for tracking whether we need to do
  // this work.
  ApplyMatchedProperties<kResolveVariables, kUpdateNeedsApplyPass>(
      state, match_result.UserRules(), false, apply_inherited_only,
      needs_apply_pass);
  ApplyMatchedProperties<kResolveVariables, kUpdateNeedsApplyPass>(
      state, match_result.AuthorRules(), false, apply_inherited_only,
      needs_apply_pass);
  ApplyMatchedProperties<kResolveVariables, kCheckNeedsApplyPass>(
      state, match_result.AuthorRules(), true, apply_inherited_only,
      needs_apply_pass);
  ApplyMatchedProperties<kResolveVariables, kCheckNeedsApplyPass>(
      state, match_result.UserRules(), true, apply_inherited_only,
      needs_apply_pass);
}

void StyleResolver::ApplyMatchedAnimationProperties(
    StyleResolverState& state,
    const MatchResult& match_result,
    const CacheSuccess& cache_success,
    NeedsApplyPass& needs_apply_pass) {
  DCHECK(!cache_success.IsFullCacheHit());
  bool apply_inherited_only = cache_success.ShouldApplyInheritedOnly();

  ApplyMatchedProperties<kAnimationPropertyPriority, kUpdateNeedsApplyPass>(
      state, match_result.AllRules(), false, apply_inherited_only,
      needs_apply_pass);
  ApplyMatchedProperties<kAnimationPropertyPriority, kCheckNeedsApplyPass>(
      state, match_result.AllRules(), true, apply_inherited_only,
      needs_apply_pass);
}

void StyleResolver::CalculateAnimationUpdate(StyleResolverState& state) {
  const Element* animating_element = state.GetAnimatingElement();

  DCHECK(state.Style()->Animations() || state.Style()->Transitions() ||
         (animating_element && animating_element->HasAnimations()));
  DCHECK(!state.IsAnimationInterpolationMapReady());

  CSSAnimations::CalculateAnimationUpdate(
      state.AnimationUpdate(), animating_element, state.GetElement(),
      *state.Style(), state.ParentStyle(), this);
  CSSAnimations::CalculateTransitionUpdate(state.AnimationUpdate(),
                                           CSSAnimations::PropertyPass::kCustom,
                                           animating_element, *state.Style());

  state.SetIsAnimationInterpolationMapReady();

  if (state.IsAnimatingCustomProperties()) {
    return;
  }
  if (!state.AnimationUpdate()
           .ActiveInterpolationsForCustomAnimations()
           .IsEmpty() ||
      !state.AnimationUpdate()
           .ActiveInterpolationsForCustomTransitions()
           .IsEmpty()) {
    state.SetIsAnimatingCustomProperties(true);
  }
}

void StyleResolver::ApplyMatchedHighPriorityProperties(
    StyleResolverState& state,
    const MatchResult& match_result,
    const CacheSuccess& cache_success,
    bool& apply_inherited_only,
    NeedsApplyPass& needs_apply_pass) {
  // Now we have all of the matched rules in the appropriate order. Walk the
  // rules and apply high-priority properties first, i.e., those properties that
  // other properties depend on.  The order is (1) high-priority not important,
  // (2) high-priority important, (3) normal not important and (4) normal
  // important.
  ApplyMatchedProperties<kHighPropertyPriority, kCheckNeedsApplyPass>(
      state, match_result.AllRules(), false, apply_inherited_only,
      needs_apply_pass);
  for (auto range : ImportantAuthorRanges(match_result)) {
    ApplyMatchedProperties<kHighPropertyPriority, kCheckNeedsApplyPass>(
        state, range, true, apply_inherited_only, needs_apply_pass);
  }
  for (auto range : ImportantUserRanges(match_result)) {
    ApplyMatchedProperties<kHighPropertyPriority, kCheckNeedsApplyPass>(
        state, range, true, apply_inherited_only, needs_apply_pass);
  }
  ApplyMatchedProperties<kHighPropertyPriority, kCheckNeedsApplyPass>(
      state, match_result.UaRules(), true, apply_inherited_only,
      needs_apply_pass);

  if (IsForcedColorsModeEnabled() &&
      state.Style()->ForcedColorAdjust() != EForcedColorAdjust::kNone) {
    ApplyForcedColors<kHighPropertyPriority>(
        state, match_result, apply_inherited_only, needs_apply_pass);
  }

  if (cache_success.cached_matched_properties &&
      cache_success.cached_matched_properties->computed_style
              ->EffectiveZoom() != state.Style()->EffectiveZoom()) {
    state.GetFontBuilder().DidChangeEffectiveZoom();
    apply_inherited_only = false;
  }

  ApplyCascadedColorValue(state);

  // If our font got dirtied, go ahead and update it now.
  UpdateFont(state);

  // Many properties depend on the font. If it changes we just apply all
  // properties.
  if (cache_success.cached_matched_properties &&
      cache_success.cached_matched_properties->computed_style
              ->GetFontDescription() != state.Style()->GetFontDescription())
    apply_inherited_only = false;
}

void StyleResolver::ApplyMatchedLowPriorityProperties(
    StyleResolverState& state,
    const MatchResult& match_result,
    const CacheSuccess& cache_success,
    bool& apply_inherited_only,
    NeedsApplyPass& needs_apply_pass) {
  // Now do the normal priority UA properties.
  ApplyMatchedProperties<kLowPropertyPriority, kCheckNeedsApplyPass>(
      state, match_result.UaRules(), false, apply_inherited_only,
      needs_apply_pass);

  // Cache the UA properties to pass them to LayoutTheme in
  // StyleAdjuster::AdjustComputedStyle.
  state.CacheUserAgentBorderAndBackground();

  // Now do the author and user normal priority properties and all the
  // !important properties.
  ApplyMatchedProperties<kLowPropertyPriority, kCheckNeedsApplyPass>(
      state, match_result.UserRules(), false, apply_inherited_only,
      needs_apply_pass);
  ApplyMatchedProperties<kLowPropertyPriority, kCheckNeedsApplyPass>(
      state, match_result.AuthorRules(), false, apply_inherited_only,
      needs_apply_pass);
  for (auto range : ImportantAuthorRanges(match_result)) {
    ApplyMatchedProperties<kLowPropertyPriority, kCheckNeedsApplyPass>(
        state, range, true, apply_inherited_only, needs_apply_pass);
  }
  for (auto range : ImportantUserRanges(match_result)) {
    ApplyMatchedProperties<kLowPropertyPriority, kCheckNeedsApplyPass>(
        state, range, true, apply_inherited_only, needs_apply_pass);
  }
  ApplyMatchedProperties<kLowPropertyPriority, kCheckNeedsApplyPass>(
      state, match_result.UaRules(), true, apply_inherited_only,
      needs_apply_pass);

  if (IsForcedColorsModeEnabled() &&
      state.Style()->ForcedColorAdjust() != EForcedColorAdjust::kNone) {
    ApplyForcedColors<kLowPropertyPriority>(
        state, match_result, apply_inherited_only, needs_apply_pass);
  }

  if (state.Style()->HasAppearance() && !apply_inherited_only) {
    // Check whether the final border and background differs from the cached UA
    // ones.  When there is a partial match in the MatchedPropertiesCache, these
    // flags will already be set correctly and the value stored in
    // cacheUserAgentBorderAndBackground is incorrect, so doing this check again
    // would give the wrong answer.
    state.Style()->SetHasAuthorBackground(HasAuthorBackground(state));
    state.Style()->SetHasAuthorBorder(HasAuthorBorder(state));
  }

  LoadPendingResources(state);

  if (!state.IsAnimatingCustomProperties() &&
      !cache_success.cached_matched_properties && cache_success.cache_hash &&
      MatchedPropertiesCache::IsCacheable(state)) {
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  matched_property_cache_added, 1);
    matched_properties_cache_.Add(*state.Style(), *state.ParentStyle(),
                                  cache_success.cache_hash,
                                  match_result.GetMatchedProperties());
  }

  DCHECK(!state.GetFontBuilder().FontDirty());
}

void StyleResolver::ApplyMatchedProperties(StyleResolverState& state,
                                           const MatchResult& match_result) {
  INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                matched_property_apply, 1);

  if (RuntimeEnabledFeatures::CSSCascadeEnabled()) {
    CascadeAndApplyMatchedProperties(state, match_result);
    return;
  }

  CacheSuccess cache_success = ApplyMatchedCache(state, match_result);
  bool apply_inherited_only = cache_success.ShouldApplyInheritedOnly();
  NeedsApplyPass needs_apply_pass;

  if (!cache_success.IsFullCacheHit()) {
    ApplyCustomProperties(state, match_result, cache_success, needs_apply_pass);
    ApplyMatchedAnimationProperties(state, match_result, cache_success,
                                    needs_apply_pass);
    ApplyMatchedHighPriorityProperties(state, match_result, cache_success,
                                       apply_inherited_only, needs_apply_pass);
  }

  if (HasAnimationsOrTransitions(state)) {
    // Calculate pre-animated computed values for all registered properties.
    // This is needed to calculate the animation update.
    CSSVariableResolver(state).ComputeRegisteredVariables();

    // Animation update calculation must happen after application of high
    // priority properties, otherwise we can't resolve em' units, making it
    // impossible to know if we should transition in some cases.
    CalculateAnimationUpdate(state);

    if (state.IsAnimatingCustomProperties()) {
      cache_success.SetFailed();

      CSSVariableAnimator(state).ApplyAll();

      // Apply high priority properties again to re-resolve var() references
      // to (now-)animated custom properties.
      // TODO(andruud): Avoid this with https://crbug.com/947004
      ApplyMatchedHighPriorityProperties(state, match_result, cache_success,
                                         apply_inherited_only,
                                         needs_apply_pass);
    }
  }

  if (cache_success.IsFullCacheHit())
    return;

  CSSVariableResolver(state).ResolveVariableDefinitions();

  ApplyMatchedLowPriorityProperties(state, match_result, cache_success,
                                    apply_inherited_only, needs_apply_pass);
}

void StyleResolver::CascadeAndApplyMatchedProperties(
    StyleResolverState& state,
    const MatchResult& match_result) {
  DCHECK(RuntimeEnabledFeatures::CSSCascadeEnabled());

  CacheSuccess cache_success = ApplyMatchedCache(state, match_result);

  StyleCascade cascade(state);
  CascadeMatchResult(state, cascade, match_result);

  // We need to copy the entire cascade before applying, in case there are
  // animations.
  //
  // TODO(crbug.com/985010): Avoid this copy with non-destructive Apply.
  StyleCascade cascade_copy(cascade);
  cascade_copy.RemoveAnimationPriority();

  if (!cache_success.IsFullCacheHit())
    cascade.Apply();

  if (HasAnimationsOrTransitions(state)) {
    CalculateAnimationUpdate(state);

    // Add animation effects for custom properties to the cascade.
    if (state.IsAnimatingCustomProperties()) {
      cache_success.SetFailed();
      CascadeAnimations(state, cascade_copy);
      CascadeTransitions(state, cascade_copy);
      StyleAnimator animator(state, cascade_copy);
      cascade_copy.Apply(animator);
    }
  }

  if (cache_success.IsFullCacheHit())
    return;

  // TODO(crbug.com/985025): We only support full cache hits for now.
  bool apply_inherited_only = false;

  // TODO(crbug.com/985027): Cascade kLowPropertyPriority.
  //
  // Ultimately NeedsApplyPass will be removed, so we don't bother fixing
  // that for this codepath. For now, just always go through the low-priority
  // properties.
  const bool important = true;
  NeedsApplyPass needs_apply_pass;
  needs_apply_pass.Set(kLowPropertyPriority, important);
  needs_apply_pass.Set(kLowPropertyPriority, !important);
  ApplyMatchedLowPriorityProperties(state, match_result, cache_success,
                                    apply_inherited_only, needs_apply_pass);
}

static void CascadeDeclaration(StyleCascade& cascade,
                               const CSSPropertyName& name,
                               const CSSValue& value,
                               StyleCascade::Priority priority,
                               unsigned apply_mask) {
  if (apply_mask & kApplyMaskRegular)
    cascade.Add(name, &value, priority);
  if (apply_mask & kApplyMaskVisited) {
    const CSSProperty* visited =
        CSSProperty::Get(name.Id()).GetVisitedProperty();
    if (visited)
      cascade.Add(visited->GetCSSPropertyName(), &value, priority);
  }
}

// https://drafts.csswg.org/css-cascade/#all-shorthand
static void CascadeAll(StyleResolverState& state,
                       StyleCascade& cascade,
                       StyleCascade::Priority priority,
                       unsigned apply_mask,
                       ValidPropertyFilter filter,
                       const CSSValue& value) {
  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    using LowPrioData = CSSPropertyPriorityData<kLowPropertyPriority>;
    if (LowPrioData::PropertyHasPriority(property_id))
      continue;

    const CSSProperty& property = CSSProperty::Get(property_id);

    if (property.IsShorthand())
      continue;
    if (!property.IsAffectedByAll())
      continue;
    if (!PassesPropertyFilter(filter, property_id, state.GetDocument()))
      continue;

    CascadeDeclaration(cascade, CSSPropertyName(property_id), value, priority,
                       apply_mask);
  }
}

void StyleResolver::CascadeMatchResult(StyleResolverState& state,
                                       StyleCascade& cascade,
                                       const MatchResult& result) {
  DCHECK(RuntimeEnabledFeatures::CSSCascadeEnabled());

  using Origin = StyleCascade::Origin;
  CascadeRange(state, cascade, result.UaRules(), Origin::kUserAgent);
  CascadeRange(state, cascade, result.AuthorRules(), Origin::kAuthor);
  CascadeRange(state, cascade, result.UserRules(), Origin::kUser);
}

void StyleResolver::CascadeRange(StyleResolverState& state,
                                 StyleCascade& cascade,
                                 const MatchedPropertiesRange& range,
                                 StyleCascade::Origin origin) {
  DCHECK(RuntimeEnabledFeatures::CSSCascadeEnabled());

  if (range.IsEmpty())
    return;

  for (const auto& matched_properties : range) {
    auto filter = static_cast<ValidPropertyFilter>(
        matched_properties.types_.valid_property_filter);
    uint16_t tree_order = matched_properties.types_.tree_order;
    unsigned apply_mask = ComputeApplyMask(state, matched_properties);
    const CSSPropertyValueSet* properties = matched_properties.properties.Get();
    unsigned property_count = properties->PropertyCount();

    for (unsigned i = 0; i < property_count; ++i) {
      CSSPropertyValueSet::PropertyReference current =
          properties->PropertyAt(i);
      CSSPropertyID property_id = current.Id();

      StyleCascade::Priority priority(origin, tree_order);

      if (current.IsImportant())
        priority = priority.AddImportance();

      if (property_id == CSSPropertyID::kAll) {
        CascadeAll(state, cascade, priority, apply_mask, filter,
                   current.Value());
        continue;
      }

      using LowPrioData = CSSPropertyPriorityData<kLowPropertyPriority>;
      if (LowPrioData::PropertyHasPriority(property_id))
        continue;

      if (!PassesPropertyFilter(filter, property_id, state.GetDocument()))
        continue;

      CascadeDeclaration(cascade, current.Name(), current.Value(), priority,
                         apply_mask);
    }
  }
}

void StyleResolver::CascadeTransitions(StyleResolverState& state,
                                       StyleCascade& cascade) {
  const auto& update = state.AnimationUpdate();
  const auto& map = update.ActiveInterpolationsForCustomTransitions();
  const auto origin = StyleCascade::Origin::kTransition;
  CascadeInterpolations(cascade, map, origin);
}

void StyleResolver::CascadeAnimations(StyleResolverState& state,
                                      StyleCascade& cascade) {
  const auto& update = state.AnimationUpdate();
  const auto& map = update.ActiveInterpolationsForCustomAnimations();
  const auto origin = StyleCascade::Origin::kAnimation;
  CascadeInterpolations(cascade, map, origin);
}

void StyleResolver::CascadeInterpolations(StyleCascade& cascade,
                                          const ActiveInterpolationsMap& map,
                                          StyleCascade::Origin origin) {
  using Type = cssvalue::CSSPendingInterpolationValue::Type;
  for (const auto& entry : map) {
    auto name = entry.key.GetCSSPropertyName();
    Type type = entry.key.IsPresentationAttribute()
                    ? Type::kPresentationAttribute
                    : Type::kCSSProperty;
    auto* v = cssvalue::CSSPendingInterpolationValue::Create(type);
    cascade.Add(name, v, origin);
  }
}

bool StyleResolver::HasAuthorBackground(const StyleResolverState& state) {
  const CachedUAStyle* cached_ua_style = state.GetCachedUAStyle();
  if (!cached_ua_style)
    return false;

  FillLayer old_fill = cached_ua_style->background_layers;
  FillLayer new_fill = state.Style()->BackgroundLayers();
  // Exclude background-repeat from comparison by resetting it.
  old_fill.SetRepeatX(EFillRepeat::kNoRepeatFill);
  old_fill.SetRepeatY(EFillRepeat::kNoRepeatFill);
  new_fill.SetRepeatX(EFillRepeat::kNoRepeatFill);
  new_fill.SetRepeatY(EFillRepeat::kNoRepeatFill);

  return (old_fill != new_fill || cached_ua_style->background_color !=
                                      state.Style()->BackgroundColor());
}

bool StyleResolver::HasAuthorBorder(const StyleResolverState& state) {
  const CachedUAStyle* cached_ua_style = state.GetCachedUAStyle();
  return cached_ua_style &&
         (cached_ua_style->border_image != state.Style()->BorderImage() ||
          !cached_ua_style->BorderColorEquals(*state.Style()) ||
          !cached_ua_style->BorderWidthEquals(*state.Style()) ||
          !cached_ua_style->BorderRadiiEquals(*state.Style()) ||
          !cached_ua_style->BorderStyleEquals(*state.Style()));
}

void StyleResolver::ApplyCallbackSelectors(StyleResolverState& state) {
  RuleSet* watched_selectors_rule_set =
      GetDocument().GetStyleEngine().WatchedSelectorsRuleSet();
  if (!watched_selectors_rule_set)
    return;

  ElementRuleCollector collector(state.ElementContext(), selector_filter_,
                                 state.Style());
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
    print_media_type_ = DeprecatedEqualIgnoringCase(view->MediaType(),
                                                    media_type_names::kPrint);
    if (was_print != print_media_type_)
      matched_properties_cache_.ClearViewportDependent();
  }
}

void StyleResolver::Trace(blink::Visitor* visitor) {
  visitor->Trace(matched_properties_cache_);
  visitor->Trace(selector_filter_);
  visitor->Trace(document_);
  visitor->Trace(tracker_);
}

bool StyleResolver::IsForcedColorsModeEnabled() const {
  return GetDocument().InForcedColorsMode();
}

void StyleResolver::ApplyCascadedColorValue(StyleResolverState& state) {
  if (RuntimeEnabledFeatures::CSSCascadeEnabled())
    return;

  if (const CSSValue* color_value = state.GetCascadedColorValue()) {
    state.SetCascadedColorValue(nullptr);
    const auto* identifier_value = DynamicTo<CSSIdentifierValue>(color_value);
    if (identifier_value) {
      switch (identifier_value->GetValueID()) {
        case CSSValueID::kCurrentcolor:
          // As per the spec, 'color: currentColor' is treated as 'color:
          // inherit'
        case CSSValueID::kInherit:
          if (state.ParentStyle()->IsColorInternalText())
            state.Style()->SetIsColorInternalText(true);
          else
            state.Style()->SetColor(state.ParentStyle()->GetColor());
          break;
        case CSSValueID::kInitial:
          state.Style()->SetColor(ComputedStyleInitialValues::InitialColor());
          break;
        case CSSValueID::kInternalRootColor:
          state.Style()->SetIsColorInternalText(true);
          break;
        default:
          identifier_value = nullptr;
          break;
      }
    }
    if (!identifier_value) {
      state.Style()->SetColor(
          StyleBuilderConverter::ConvertColor(state, *color_value));
    }
  }

  if (const CSSValue* visited_color_value =
          state.GetCascadedVisitedColorValue()) {
    state.SetCascadedVisitedColorValue(nullptr);
    const auto* identifier_value =
        DynamicTo<CSSIdentifierValue>(visited_color_value);
    if (identifier_value) {
      switch (identifier_value->GetValueID()) {
        case CSSValueID::kCurrentcolor:
          // As per the spec, 'color: currentColor' is treated as 'color:
          // inherit'
        case CSSValueID::kInherit:
          state.Style()->SetInternalVisitedColor(
              state.ParentStyle()->GetColor());
          break;
        case CSSValueID::kInitial:
          state.Style()->SetInternalVisitedColor(
              ComputedStyleInitialValues::InitialColor());
          break;
        default:
          identifier_value = nullptr;
          break;
      }
    }
    if (!identifier_value) {
      state.Style()->SetInternalVisitedColor(
          StyleBuilderConverter::ConvertColor(state, *visited_color_value,
                                              true));
    }
  }
}

}  // namespace blink
