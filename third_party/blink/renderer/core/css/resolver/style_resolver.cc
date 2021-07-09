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

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_value_factory.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
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
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
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
#include "third_party/blink/renderer/core/css/scoped_css_value.h"
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
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/mathml/mathml_fraction_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_operator_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_padded_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_space_element.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
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
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

void SetAnimationUpdateIfNeeded(StyleResolverState& state, Element& element) {
  // If any changes to CSS Animations were detected, stash the update away for
  // application after the layout object is updated if we're in the appropriate
  // scope.
  if (state.AnimationUpdate().IsEmpty())
    return;

  auto& element_animations = element.EnsureElementAnimations();
  auto& document_animations = state.GetDocument().GetDocumentAnimations();

  element_animations.CssAnimations().SetPendingUpdate(state.AnimationUpdate());

  if (RuntimeEnabledFeatures::CSSIsolatedAnimationUpdatesEnabled()) {
    if (document_animations.AnimationUpdatesAllowed()) {
      state.GetDocument()
          .GetDocumentAnimations()
          .AddElementWithPendingAnimationUpdate(element);
    }
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
  // call ComputeBaseComputedStyleDiff() to check that the optimization was
  // sound.
  return true;
#else
  return !base_computed_style;
#endif  // !DCHECK_IS_ON()
}

// Compare the base computed style with the one we compute to validate that the
// optimization is sound. A return value of g_null_atom means the diff was
// empty (which is what we want).
String ComputeBaseComputedStyleDiff(const ComputedStyle* base_computed_style,
                                    const ComputedStyle& computed_style) {
#if DCHECK_IS_ON()
  if (!base_computed_style)
    return g_null_atom;
  // Under certain conditions ComputedStyle::operator==() may return false for
  // differences that are permitted during an animation.
  // The FontFaceCache version number may be increased without forcing a style
  // recalc (see crbug.com/471079).
  if (!base_computed_style->GetFont().IsFallbackValid())
    return g_null_atom;
  // Images use instance equality rather than value equality (see
  // crbug.com/781461).
  for (CSSPropertyID id :
       {CSSPropertyID::kBackgroundImage, CSSPropertyID::kWebkitMaskImage}) {
    if (!CSSPropertyEquality::PropertiesEqual(
            PropertyHandle(CSSProperty::Get(id)), *base_computed_style,
            computed_style)) {
      return g_null_atom;
    }
  }

  if (*base_computed_style == computed_style)
    return g_null_atom;

  StringBuilder builder;
  builder.Append("Field diff: ");

  Vector<String> diff = base_computed_style->DebugDiffFields(computed_style);

  for (const String& s : diff) {
    builder.Append(s);
    builder.Append(" ");
  }

  return builder.ToString();
#else
  return g_null_atom;
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

StyleResolver::StyleResolver(Document& document)
    : document_(document),
      initial_style_(ComputedStyle::CreateInitialStyleSingleton()) {
  UpdateMediaType();
}

StyleResolver::~StyleResolver() = default;

void StyleResolver::Dispose() {
  initial_style_.reset();
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
  // TODO(futhark): If the resolver is null here, it means we are matching rules
  // for custom element default styles. Since we don't have a
  // ScopedStyleResolver if the custom element does not have a shadow root,
  // there is no way to collect @-rules for @font-face, @keyframes, etc. We
  // currently pass the element's TreeScope, which might not be what we want. It
  // means that if you have:
  //
  //   <style>@keyframes anim { ... }</style>
  //   <custom-element></custom-element>
  //
  // and the custom-element is defined with:
  //
  //   @keyframes anim { ... }
  //   custom-element { animation-name: anim }
  //
  // it means that the custom element will pick up the @keyframes definition
  // from the element's scope.
  collector.FinishAddingAuthorRulesForTreeScope(
      resolver ? resolver->GetTreeScope() : element.GetTreeScope());
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
    collector.FinishAddingAuthorRulesForTreeScope((*it)->GetTreeScope());
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
      StyleEngine& style_engine = element.GetDocument().GetStyleEngine();
      RuleSet* rule_set = style_engine.RuleSetForSheet(*style);
      if (rule_set) {
        collector.CollectMatchingRules(MatchRequest(
            rule_set, nullptr /* scope */, style, style_sheet_index,
            style_engine.EnsureVTTOriginatingElement()));
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
    element_scope_resolver->CollectMatchingElementScopeRules(collector);
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

  collector.FinishAddingAuthorRulesForTreeScope(
      element_scope_resolver ? element_scope_resolver->GetTreeScope()
                             : element.GetTreeScope());
}

void StyleResolver::MatchPseudoPartRulesForUAHost(
    const Element& element,
    ElementRuleCollector& collector) {
  const AtomicString& pseudo_id = element.ShadowPseudoId();
  if (pseudo_id != shadow_element_names::kPseudoInputPlaceholder &&
      pseudo_id != shadow_element_names::kPseudoFileUploadButton) {
    return;
  }

  // We allow ::placeholder pseudo element after ::part(). See
  // MatchSlottedRulesForUAHost for a more detailed explanation.
  DCHECK(element.OwnerShadowHost());
  MatchPseudoPartRules(*element.OwnerShadowHost(), collector,
                       /* for_shadow_pseudo */ true);
}

void StyleResolver::MatchPseudoPartRules(const Element& element,
                                         ElementRuleCollector& collector,
                                         bool for_shadow_pseudo) {
  if (!for_shadow_pseudo)
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
      resolver->CollectMatchingPartPseudoRules(collector, current_names,
                                               for_shadow_pseudo);
      collector.SortAndTransferMatchedRules();
      collector.FinishAddingAuthorRulesForTreeScope(resolver->GetTreeScope());
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

void StyleResolver::MatchAuthorRules(
    const Element& element,
    ScopedStyleResolver* element_scope_resolver,
    ElementRuleCollector& collector) {
  MatchHostAndCustomElementRules(element, collector);
  MatchSlottedRules(element, collector);
  MatchElementScopeRules(element, element_scope_resolver, collector);
  MatchPseudoPartRules(element, collector);
}

void StyleResolver::MatchUserRules(ElementRuleCollector& collector) {
  collector.ClearMatchedRules();
  GetDocument().GetStyleEngine().CollectMatchingUserRules(collector);
  collector.SortAndTransferMatchedRules();
  collector.FinishAddingUserRules();
}

namespace {

bool IsInMediaUAShadow(const Element& element) {
  ShadowRoot* root = element.ContainingShadowRoot();
  if (!root || !root->IsUserAgent())
    return false;
  ShadowRoot* outer_root;
  do {
    outer_root = root;
    root = root->host().ContainingShadowRoot();
  } while (root && root->IsUserAgent());
  return outer_root->host().IsMediaElement();
}

}  // namespace

void StyleResolver::MatchUARules(const Element& element,
                                 ElementRuleCollector& collector) {
  collector.SetMatchingUARules(true);

  CSSDefaultStyleSheets& default_style_sheets =
      CSSDefaultStyleSheets::Instance();
  if (!print_media_type_) {
    if (LIKELY(element.IsHTMLElement() || element.IsVTTElement())) {
      MatchRuleSet(collector, default_style_sheets.DefaultHtmlStyle());
      if (UNLIKELY(IsInMediaUAShadow(element))) {
        MatchRuleSet(collector,
                     default_style_sheets.DefaultMediaControlsStyle());
      }
    } else if (element.IsSVGElement()) {
      MatchRuleSet(collector, default_style_sheets.DefaultSVGStyle());
    } else if (element.namespaceURI() == mathml_names::kNamespaceURI) {
      MatchRuleSet(collector, default_style_sheets.DefaultMathMLStyle());
    }
  } else {
    MatchRuleSet(collector, default_style_sheets.DefaultPrintStyle());
  }

  // In quirks mode, we match rules from the quirks user agent sheet.
  if (GetDocument().InQuirksMode())
    MatchRuleSet(collector, default_style_sheets.DefaultHtmlQuirksStyle());

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
  Element& element = state.GetElement();
  MatchUARules(element, collector);
  MatchUserRules(collector);

  // Now check author rules, beginning first with presentational attributes
  // mapped from HTML.
  if (element.IsStyledElement() && !state.IsForPseudoElement()) {
    collector.AddElementStyleProperties(element.PresentationAttributeStyle());

    // Now we check additional mapped declarations.
    // Tables and table cells share an additional mapped rule that must be
    // applied after all attributes, since their mapped style depends on the
    // values of multiple attributes.
    collector.AddElementStyleProperties(
        element.AdditionalPresentationAttributeStyle());

    if (auto* html_element = DynamicTo<HTMLElement>(element)) {
      if (html_element->HasDirectionAuto()) {
        collector.AddElementStyleProperties(
            html_element->CachedDirectionality() == TextDirection::kLtr
                ? LeftToRightDeclaration()
                : RightToLeftDeclaration());
      }
    }
  }

  ScopedStyleResolver* element_scope_resolver = ScopedResolverFor(element);
  MatchAuthorRules(element, element_scope_resolver, collector);

  if (element.IsStyledElement() && !state.IsForPseudoElement()) {
    // Now check SMIL animation override style.
    auto* svg_element = DynamicTo<SVGElement>(element);
    if (include_smil_properties && svg_element) {
      collector.AddElementStyleProperties(
          svg_element->AnimatedSMILStyleProperties(), false /* isCacheable */);
    }
  }

  collector.FinishAddingAuthorRulesForTreeScope(
      element_scope_resolver ? element_scope_resolver->GetTreeScope()
                             : element.GetTreeScope());
}

scoped_refptr<ComputedStyle> StyleResolver::StyleForViewport() {
  scoped_refptr<ComputedStyle> viewport_style = InitialStyleForElement();

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

static void IncrementResolvedStyleCounters(const StyleRequest& style_request,
                                           Document& document) {
  document.GetStyleEngine().IncStyleForElementCount();

  if (style_request.IsPseudoStyleRequest()) {
    INCREMENT_STYLE_STATS_COUNTER(document.GetStyleEngine(),
                                  pseudo_elements_styled, 1);
  } else {
    INCREMENT_STYLE_STATS_COUNTER(document.GetStyleEngine(), elements_styled,
                                  1);
  }
}

scoped_refptr<ComputedStyle> StyleResolver::ResolveStyle(
    Element* element,
    const StyleRecalcContext& style_recalc_context,
    const StyleRequest& style_request) {
  if (!element) {
    DCHECK(style_request.IsPseudoStyleRequest());
    return nullptr;
  }

  DCHECK(!style_request.IsPseudoStyleRequest() ||
         style_request.parent_override);
  DCHECK(GetDocument().GetFrame());
  DCHECK(GetDocument().GetSettings());

  SelectorFilterParentScope::EnsureParentStackIsPushed();

  StyleResolverState state(GetDocument(), *element, style_recalc_context,
                           style_request);

  STACK_UNINITIALIZED StyleCascade cascade(state);

  ApplyBaseStyle(element, style_recalc_context, style_request, state, cascade);

  if (style_request.IsPseudoStyleRequest() && state.HadNoMatchedProperties())
    return state.TakeStyle();

  if (ApplyAnimatedStyle(state, cascade)) {
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  styles_animated, 1);
    StyleAdjuster::AdjustComputedStyle(
        state, style_request.IsPseudoStyleRequest() ? nullptr : element);
  }

  IncrementResolvedStyleCounters(style_request, GetDocument());

  if (!style_request.IsPseudoStyleRequest()) {
    if (IsA<HTMLBodyElement>(*element)) {
      GetDocument().GetTextLinkColors().SetTextColor(
          state.Style()->GetCurrentColor());
    }

    if (element->IsMathMLElement())
      ApplyMathMLCustomStyleProperties(element, state);
  } else if (IsHighlightPseudoElement(style_request.pseudo_id)) {
    if (element->GetComputedStyle() &&
        element->GetComputedStyle()->TextShadow() !=
            state.Style()->TextShadow()) {
      // This counts the usage of text-shadow in CSS highlight pseudos.
      UseCounter::Count(GetDocument(),
                        WebFeature::kTextShadowInHighlightPseudo);
      if (state.Style()->TextShadow()) {
        // This counts the cases in which text-shadow is not "none" in CSS
        // highlight pseudos, as the most common use case is using it to disable
        // text-shadow, and that won't be need once some painting issues related
        // to highlight pseudos are fixed.
        UseCounter::Count(GetDocument(),
                          WebFeature::kTextShadowNotNoneInHighlightPseudo);
      }
    }
  }

  if (Element* animating_element = state.GetAnimatingElement())
    SetAnimationUpdateIfNeeded(state, *animating_element);

  if (state.Style()->HasViewportUnits())
    GetDocument().SetHasViewportUnits();

  if (state.Style()->HasContainerRelativeUnits()) {
    state.Style()->SetDependsOnContainerQueries(true);
    GetDocument().GetStyleEngine().SetUsesContainerRelativeUnits();
  }

  if (state.Style()->HasRemUnits())
    GetDocument().GetStyleEngine().SetUsesRemUnit(true);

  if (state.Style()->HasGlyphRelativeUnits())
    UseCounter::Count(GetDocument(), WebFeature::kHasGlyphRelativeUnits);

  state.LoadPendingResources();

  // Now return the style.
  return state.TakeStyle();
}

static bool AllowsInheritance(const StyleRequest& style_request,
                              const ComputedStyle* parent_style) {
  // The spec disallows inheritance for ::backdrop.
  return parent_style && style_request.pseudo_id != kPseudoIdBackdrop;
}

void StyleResolver::InitStyleAndApplyInheritance(
    Element& element,
    const StyleRequest& style_request,
    StyleResolverState& state) {
  if (AllowsInheritance(style_request, state.ParentStyle())) {
    scoped_refptr<ComputedStyle> style = CreateComputedStyle();
    style->InheritFrom(
        *state.ParentStyle(),
        (!style_request.IsPseudoStyleRequest() && IsAtShadowBoundary(&element))
            ? ComputedStyle::kAtShadowBoundary
            : ComputedStyle::kNotAtShadowBoundary);
    state.SetStyle(std::move(style));

    // contenteditable attribute (implemented by -webkit-user-modify) should
    // be propagated from shadow host to distributed node.
    if (!style_request.IsPseudoStyleRequest() && element.AssignedSlot()) {
      if (Element* parent = element.parentElement()) {
        if (const ComputedStyle* shadow_host_style = parent->GetComputedStyle())
          state.Style()->SetUserModify(shadow_host_style->UserModify());
      }
    }
  } else {
    state.SetStyle(InitialStyleForElement());
    state.SetParentStyle(ComputedStyle::Clone(*state.Style()));
    state.SetLayoutParentStyle(state.ParentStyle());
    if (!style_request.IsPseudoStyleRequest() &&
        element != GetDocument().documentElement()) {
      // Strictly, we should only allow the root element to inherit from
      // initial styles, but we allow getComputedStyle() for connected
      // elements outside the flat tree rooted at an unassigned shadow host
      // child or a slot fallback element.
      DCHECK((IsShadowHost(element.parentNode()) ||
              IsA<HTMLSlotElement>(element.parentNode())) &&
             !LayoutTreeBuilderTraversal::ParentElement(element));
      state.Style()->SetIsEnsuredOutsideFlatTree();
    }
  }
  state.Style()->SetStyleType(style_request.pseudo_id);
  state.Style()->SetPseudoArgument(style_request.pseudo_argument);

  if (!style_request.IsPseudoStyleRequest() && element.IsLink()) {
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
    const StyleRecalcContext& style_recalc_context,
    const StyleRequest& style_request,
    StyleResolverState& state,
    StyleCascade& cascade) {
  DCHECK(style_request.pseudo_id != kPseudoIdFirstLineInherited);

  bool base_is_usable =
      state.CanCacheBaseStyle() && CanReuseBaseComputedStyle(state);
  const ComputedStyle* animation_base_computed_style =
      base_is_usable ? CachedAnimationBaseComputedStyle(state) : nullptr;
  if (ShouldComputeBaseComputedStyle(animation_base_computed_style)) {
    InitStyleAndApplyInheritance(*element, style_request, state);

    GetDocument().GetStyleEngine().EnsureUAStyleForElement(*element);

    // This adds a CSSInitialColorValue to the cascade for the document
    // element. The CSSInitialColorValue will resolve to a color-scheme
    // sensitive color in Color::ApplyValue. It is added at the start of the
    // MatchResult such that subsequent declarations (even from the UA sheet)
    // get a higher priority.
    //
    // TODO(crbug.com/1046753): Remove this when canvastext is supported.
    if (!style_request.IsPseudoStyleRequest() &&
        element == state.GetDocument().documentElement()) {
      cascade.MutableMatchResult().AddMatchedProperties(
          DocumentElementUserAgentDeclarations());
    }

    ElementRuleCollector collector(state.ElementContext(), style_recalc_context,
                                   selector_filter_,
                                   cascade.MutableMatchResult(), state.Style(),
                                   state.Style()->InsideLink());

    if (style_request.IsPseudoStyleRequest()) {
      collector.SetPseudoElementStyleRequest(style_request);
      GetDocument().GetStyleEngine().EnsureUAStyleForPseudoElement(
          style_request.pseudo_id);
    }

    // TODO(obrufau): support styling nested pseudo-elements
    if (style_request.IsPseudoStyleRequest() && element->IsPseudoElement()) {
      MatchUARules(*element, collector);
    } else {
      MatchAllRules(
          state, collector,
          style_request.matching_behavior != kMatchAllRulesExcludingSMIL);
    }

    if (tracker_)
      AddMatchedRulesToTracker(collector);

    if (style_request.IsPseudoStyleRequest() &&
        !collector.MatchedResult().HasMatchedProperties()) {
      StyleAdjuster::AdjustComputedStyle(state, nullptr /* element */);
      state.SetHadNoMatchedProperties();
      return;
    }

    if (!style_request.IsPseudoStyleRequest() && element->GetComputedStyle() &&
        element->GetComputedStyle()->TextAutosizingMultiplier() !=
            state.Style()->TextAutosizingMultiplier()) {
      // Preserve the text autosizing multiplier on style recalc. Autosizer will
      // update it during layout if needed.
      // NOTE: this must occur before ApplyMatchedProperties for correct
      // computation of font-relative lengths.
      state.Style()->SetTextAutosizingMultiplier(
          element->GetComputedStyle()->TextAutosizingMultiplier());
    }

    CascadeAndApplyMatchedProperties(state, cascade);

    if (collector.MatchedResult().DependsOnContainerQueries())
      state.Style()->SetDependsOnContainerQueries(true);

    ApplyCallbackSelectors(state);

    // Cache our original display.
    state.Style()->SetOriginalDisplay(state.Style()->Display());

    StyleAdjuster::AdjustComputedStyle(
        state, style_request.IsPseudoStyleRequest() ? nullptr : element);

    DCHECK_EQ(g_null_atom, ComputeBaseComputedStyleDiff(
                               animation_base_computed_style, *state.Style()));
  }

  if (base_is_usable) {
    DCHECK(animation_base_computed_style);
    state.SetStyle(ComputedStyle::Clone(*animation_base_computed_style));
    state.Style()->SetStyleType(style_request.pseudo_id);
    if (!state.ParentStyle()) {
      state.SetParentStyle(InitialStyleForElement());
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
    const CSSValue* value,
    double offset) {
  // TODO(alancutter): Avoid creating a StyleResolverState just to apply a
  // single value on a ComputedStyle.
  // TOOD(crbug.com/1223030): Propagate a real StyleRecalcContext to handle
  // container relative units.
  StyleResolverState state(element.GetDocument(), element, StyleRecalcContext(),
                           StyleRequest(parent_style));
  state.SetStyle(ComputedStyle::Clone(base_style));
  if (value) {
    STACK_UNINITIALIZED StyleCascade cascade(state);
    auto* set =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(state.GetParserMode());
    set->SetProperty(property.GetCSSProperty().PropertyID(), *value);
    cascade.MutableMatchResult().FinishAddingUARules();
    cascade.MutableMatchResult().FinishAddingUserRules();
    cascade.MutableMatchResult().AddMatchedProperties(set);
    cascade.MutableMatchResult().FinishAddingAuthorRulesForTreeScope(
        element.GetTreeScope());
    cascade.Apply();
  }
  return CompositorKeyframeValueFactory::Create(property, *state.Style(),
                                                offset);
}

scoped_refptr<const ComputedStyle> StyleResolver::StyleForPage(
    uint32_t page_index,
    const AtomicString& page_name) {
  scoped_refptr<const ComputedStyle> initial_style = InitialStyleForElement();
  if (!GetDocument().documentElement())
    return initial_style;

  StyleResolverState state(GetDocument(), *GetDocument().documentElement(),
                           StyleRecalcContext(),
                           StyleRequest(initial_style.get()));

  scoped_refptr<ComputedStyle> style = CreateComputedStyle();
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

const ComputedStyle& StyleResolver::InitialStyle() const {
  return *initial_style_;
}

scoped_refptr<ComputedStyle> StyleResolver::CreateComputedStyle() const {
  return ComputedStyle::Clone(*initial_style_);
}

scoped_refptr<ComputedStyle> StyleResolver::InitialStyleForElement() const {
  const LocalFrame* frame = GetDocument().GetFrame();

  scoped_refptr<ComputedStyle> initial_style = CreateComputedStyle();

  initial_style->SetRtlOrdering(
      GetDocument().VisuallyOrdered() ? EOrder::kVisual : EOrder::kLogical);
  initial_style->SetZoom(
      frame && !GetDocument().Printing() ? frame->PageZoomFactor() : 1);
  initial_style->SetEffectiveZoom(initial_style->Zoom());
  initial_style->SetInForcedColorsMode(GetDocument().InForcedColorsMode());
  initial_style->SetTapHighlightColor(
      ComputedStyleInitialValues::InitialTapHighlightColor());

  FontDescription document_font_description =
      initial_style->GetFontDescription();
  document_font_description.SetLocale(
      LayoutLocale::Get(GetDocument().ContentLanguage()));

  initial_style->SetFontDescription(document_font_description);
  initial_style->SetUserModify(GetDocument().InDesignMode()
                                   ? EUserModify::kReadWrite
                                   : EUserModify::kReadOnly);
  FontBuilder(&GetDocument()).CreateInitialFont(*initial_style);

  scoped_refptr<StyleInitialData> initial_data =
      GetDocument().GetStyleEngine().MaybeCreateAndGetInitialData();
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
  // TODO(crbug.com/1145970): Use actual StyleRecalcContext.
  StyleRecalcContext style_recalc_context;
  ElementRuleCollector collector(state.ElementContext(), style_recalc_context,
                                 selector_filter_, match_result, state.Style(),
                                 EInsideLink::kNotInsideLink);
  collector.SetMode(SelectorChecker::kCollectingStyleRules);
  CollectPseudoRulesForElement(*element, collector, kPseudoIdNone,
                               rules_to_include);
  return collector.MatchedStyleRuleList();
}

HeapHashMap<CSSPropertyName, Member<const CSSValue>>
StyleResolver::CascadedValuesForElement(Element* element, PseudoId pseudo_id) {
  StyleResolverState state(GetDocument(), *element);
  state.SetStyle(CreateComputedStyle());

  STACK_UNINITIALIZED StyleCascade cascade(state);
  // TODO(crbug.com/1145970): Use actual StyleRecalcContext.
  StyleRecalcContext style_recalc_context;
  ElementRuleCollector collector(state.ElementContext(), style_recalc_context,
                                 selector_filter_, cascade.MutableMatchResult(),
                                 state.Style(), EInsideLink::kNotInsideLink);
  collector.SetPseudoElementStyleRequest(StyleRequest(pseudo_id, nullptr));
  MatchAllRules(state, collector, false /* include_smil_properties */);

  cascade.Apply();
  return cascade.GetCascadedValues();
}

Element* StyleResolver::FindContainerForElement(
    Element* element,
    const AtomicString& container_name) {
  auto context = StyleRecalcContext::FromAncestors(*element);
  return ContainerQueryEvaluator::FindContainer(context, container_name);
}

RuleIndexList* StyleResolver::PseudoCSSRulesForElement(
    Element* element,
    PseudoId pseudo_id,
    unsigned rules_to_include) {
  DCHECK(element);
  StyleResolverState state(GetDocument(), *element);
  MatchResult match_result;
  StyleRecalcContext style_recalc_context =
      StyleRecalcContext::FromAncestors(*element);
  ElementRuleCollector collector(state.ElementContext(), style_recalc_context,
                                 selector_filter_, match_result, state.Style(),
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
  collector.SetPseudoElementStyleRequest(StyleRequest(pseudo_id, nullptr));

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
    MatchAuthorRules(element, ScopedResolverFor(element), collector);
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

  if (!animating_element)
    return false;

  DCHECK(animating_element == &element ||
         animating_element->ParentOrShadowHostElement() == element);

  if (!HasAnimationsOrTransitions(state)) {
    // Ensure that the base computed style is not stale even if not currently
    // running an animation or transition. This ensures that any new transitions
    // use the correct starting point based on the "before change" style.
    UpdateAnimationBaseComputedStyle(state, cascade, false);
    return false;
  }

  CSSAnimations::CalculateAnimationUpdate(
      state.AnimationUpdate(), *animating_element, state.GetElement(),
      *state.Style(), state.ParentStyle(), this);
  CSSAnimations::CalculateCompositorAnimationUpdate(
      state.AnimationUpdate(), *animating_element, element, *state.Style(),
      state.ParentStyle(), WasViewportResized());
  CSSAnimations::CalculateTransitionUpdate(state.AnimationUpdate(),
                                           *animating_element, *state.Style());

  CSSAnimations::SnapshotCompositorKeyframes(
      element, state.AnimationUpdate(), *state.Style(), state.ParentStyle());

  bool has_update = !state.AnimationUpdate().IsEmpty();
  UpdateAnimationBaseComputedStyle(state, cascade, has_update);

  if (!has_update)
    return false;

  const ActiveInterpolationsMap& animations =
      state.AnimationUpdate().ActiveInterpolationsForAnimations();
  const ActiveInterpolationsMap& transitions =
      state.AnimationUpdate().ActiveInterpolationsForTransitions();

  cascade.AddInterpolations(&animations, CascadeOrigin::kAnimation);
  cascade.AddInterpolations(&transitions, CascadeOrigin::kTransition);

  CascadeFilter filter;
  if (state.Style()->StyleType() == kPseudoIdMarker)
    filter = filter.Add(CSSProperty::kValidForMarker, false);
  if (IsHighlightPseudoElement(state.Style()->StyleType()))
    filter = filter.Add(CSSProperty::kValidForHighlight, false);
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
            resolver->KeyframeStylesForAnimation(animation_name))
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
    // reusing the style data structures. Note that we cannot do this if the
    // direct parent is a ShadowRoot.
    if (state.ParentStyle()->InheritedDataShared(
            *cached_matched_properties->parent_computed_style) &&
        !IsAtShadowBoundary(&element)) {
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
                                  *state.ParentStyle());
  }
}

bool StyleResolver::CanReuseBaseComputedStyle(const StyleResolverState& state) {
  // TODO(crbug.com/1180159): @container and transitions properly.
  if (RuntimeEnabledFeatures::CSSContainerQueriesEnabled())
    return false;

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

const CSSValue* StyleResolver::ComputeValue(
    Element* element,
    const CSSPropertyName& property_name,
    const CSSValue& value) {
  const ComputedStyle* base_style = element->GetComputedStyle();
  StyleResolverState state(element->GetDocument(), *element);
  STACK_UNINITIALIZED StyleCascade cascade(state);
  state.SetStyle(ComputedStyle::Clone(*base_style));
  auto* set =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(state.GetParserMode());
  if (property_name.IsCustomProperty()) {
    set->SetProperty(CSSPropertyValue(property_name, value));
  } else {
    set->SetProperty(property_name.Id(), value);
  }
  cascade.MutableMatchResult().FinishAddingUARules();
  cascade.MutableMatchResult().FinishAddingUserRules();
  cascade.MutableMatchResult().AddMatchedProperties(set);
  cascade.MutableMatchResult().FinishAddingAuthorRulesForTreeScope(
      element->GetTreeScope());
  cascade.Apply();

  CSSPropertyRef property_ref(property_name, element->GetDocument());
  return ComputedStyleUtils::ComputedPropertyValue(property_ref.GetProperty(),
                                                   *state.Style());
}

FilterOperations StyleResolver::ComputeFilterOperations(
    Element* element,
    const Font& font,
    const CSSValue& filter_value) {
  scoped_refptr<ComputedStyle> parent = CreateComputedStyle();
  parent->SetFont(font);

  // TODO(crbug.com/1145970): Use actual StyleRecalcContext.
  StyleResolverState state(GetDocument(), *element, StyleRecalcContext(),
                           StyleRequest(parent.get()));

  state.SetStyle(ComputedStyle::Clone(*parent));

  StyleBuilder::ApplyProperty(GetCSSPropertyFilter(), state,
                              ScopedCSSValue(filter_value, &GetDocument()));

  state.LoadPendingResources();

  return state.Style()->Filter();
}

scoped_refptr<ComputedStyle> StyleResolver::StyleForInterpolations(
    Element& element,
    ActiveInterpolationsMap& interpolations) {
  // TODO(crbug.com/1145970): Use actual StyleRecalcContext.
  StyleRecalcContext style_recalc_context;
  StyleRequest style_request;
  StyleResolverState state(GetDocument(), element, style_recalc_context,
                           style_request);
  STACK_UNINITIALIZED StyleCascade cascade(state);

  ApplyBaseStyle(&element, style_recalc_context, style_request, state, cascade);
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
    state.SetParentStyle(InitialStyleForElement());
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

  // TODO(crbug.com/1145970): Use actual StyleRecalcContext.
  StyleRecalcContext style_recalc_context;
  MatchResult match_result;
  ElementRuleCollector collector(state.ElementContext(), style_recalc_context,
                                 selector_filter_, match_result, state.Style(),
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
  StyleResolverState state(GetDocument(), element, StyleRecalcContext(),
                           StyleRequest(style));
  state.SetStyle(style);

  for (const CSSProperty* property : properties) {
    if (property->IDEquals(CSSPropertyID::kLineHeight))
      UpdateFont(state);
    // TODO(futhark): If we start supporting fonts on ShadowRoot.fonts in
    // addition to Document.fonts, we need to pass the correct TreeScope instead
    // of GetDocument() in the ScopedCSSValue below.
    StyleBuilder::ApplyProperty(
        *property, state,
        ScopedCSSValue(
            *property_set.GetPropertyCSSValue(property->PropertyID()),
            &GetDocument()));
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

scoped_refptr<ComputedStyle> StyleResolver::CreateAnonymousStyleWithDisplay(
    const ComputedStyle& parent_style,
    EDisplay display) {
  scoped_refptr<ComputedStyle> new_style = CreateComputedStyle();
  new_style->InheritFrom(parent_style);
  new_style->SetUnicodeBidi(parent_style.GetUnicodeBidi());
  new_style->SetDisplay(display);
  return new_style;
}

scoped_refptr<ComputedStyle>
StyleResolver::CreateInheritedDisplayContentsStyleIfNeeded(
    const ComputedStyle& parent_style,
    const ComputedStyle& layout_parent_style) {
  if (parent_style.InheritedEqual(layout_parent_style))
    return nullptr;
  return CreateAnonymousStyleWithDisplay(parent_style, EDisplay::kInline);
}

#define PROPAGATE_FROM(source, getter, setter, initial) \
  PROPAGATE_VALUE(source ? source->getter() : initial, getter, setter);

#define PROPAGATE_VALUE(value, getter, setter)     \
  if ((new_viewport_style->getter()) != (value)) { \
    new_viewport_style->setter(value);             \
    changed = true;                                \
  }

namespace {

bool PropagateScrollSnapStyleToViewport(
    Document& document,
    const ComputedStyle* document_element_style,
    ComputedStyle* new_viewport_style) {
  bool changed = false;
  // We only propagate the properties related to snap container since viewport
  // defining element cannot be a snap area.
  PROPAGATE_FROM(document_element_style, GetScrollSnapType, SetScrollSnapType,
                 cc::ScrollSnapType());
  PROPAGATE_FROM(document_element_style, ScrollPaddingTop, SetScrollPaddingTop,
                 Length());
  PROPAGATE_FROM(document_element_style, ScrollPaddingRight,
                 SetScrollPaddingRight, Length());
  PROPAGATE_FROM(document_element_style, ScrollPaddingBottom,
                 SetScrollPaddingBottom, Length());
  PROPAGATE_FROM(document_element_style, ScrollPaddingLeft,
                 SetScrollPaddingLeft, Length());

  if (changed) {
    document.GetSnapCoordinator().SnapContainerDidChange(
        *document.GetLayoutView());
  }

  return changed;
}

}  // namespace

bool StyleResolver::ShouldStopBodyPropagation(const Element& body_or_html) {
  DCHECK(!body_or_html.NeedsReattachLayoutTree())
      << "This method relies on LayoutObject to be attached and up-to-date";
  DCHECK(IsA<HTMLBodyElement>(body_or_html) ||
         IsA<HTMLHtmlElement>(body_or_html));
  LayoutObject* layout_object = body_or_html.GetLayoutObject();
  if (!layout_object)
    return true;
  bool contained = layout_object->ShouldApplyAnyContainment();
  if (contained) {
    UseCounter::Count(GetDocument(), IsA<HTMLHtmlElement>(body_or_html)
                                         ? WebFeature::kHTMLRootContained
                                         : WebFeature::kHTMLBodyContained);
  }
  if (!RuntimeEnabledFeatures::CSSContainedBodyPropagationEnabled())
    return false;
  DCHECK_EQ(contained,
            layout_object->StyleRef().ShouldApplyAnyContainment(body_or_html))
      << "Applied containment must give the same result from LayoutObject and "
         "ComputedStyle";
  return contained;
}

void StyleResolver::PropagateStyleToViewport() {
  DCHECK(GetDocument().InStyleRecalc());
  Element* document_element = GetDocument().documentElement();
  const ComputedStyle* document_element_style =
      document_element && document_element->GetLayoutObject()
          ? document_element->GetComputedStyle()
          : nullptr;
  const ComputedStyle* body_style = nullptr;
  if (HTMLBodyElement* body = GetDocument().FirstBodyElement()) {
    if (!ShouldStopBodyPropagation(*document_element) &&
        !ShouldStopBodyPropagation(*body)) {
      body_style = body->GetComputedStyle();
    }
  }

  const ComputedStyle& viewport_style =
      GetDocument().GetLayoutView()->StyleRef();
  scoped_refptr<ComputedStyle> new_viewport_style =
      ComputedStyle::Clone(viewport_style);
  bool changed = false;
  bool update_scrollbar_style = false;

  // Writing mode and direction
  {
    const ComputedStyle* direction_style =
        body_style ? body_style : document_element_style;
    PROPAGATE_FROM(direction_style, GetWritingMode, SetWritingMode,
                   WritingMode::kHorizontalTb);
    PROPAGATE_FROM(direction_style, Direction, SetDirection,
                   TextDirection::kLtr);
  }

  // Background
  {
    const ComputedStyle* background_style = document_element_style;
    // http://www.w3.org/TR/css3-background/#body-background
    // <html> root element with no background steals background from its first
    // <body> child.
    // Also see LayoutBoxModelObject::BackgroundTransfersToView()
    if (body_style && !background_style->HasBackground())
      background_style = body_style;

    Color background_color = Color::kTransparent;
    FillLayer background_layers(EFillLayerType::kBackground, true);
    EImageRendering image_rendering = EImageRendering::kAuto;

    if (background_style) {
      background_color = background_style->VisitedDependentColor(
          GetCSSPropertyBackgroundColor());
      background_layers = background_style->BackgroundLayers();
      for (auto* current_layer = &background_layers; current_layer;
           current_layer = current_layer->Next()) {
        // http://www.w3.org/TR/css3-background/#root-background
        // The root element background always have painting area of the whole
        // canvas.
        current_layer->SetClip(EFillBox::kBorder);

        // The root element doesn't scroll. It always propagates its layout
        // overflow to the viewport. Positioning background against either box
        // is equivalent to positioning against the scrolled box of the
        // viewport.
        if (current_layer->Attachment() == EFillAttachment::kScroll)
          current_layer->SetAttachment(EFillAttachment::kLocal);
      }
      image_rendering = background_style->ImageRendering();
    }

    if (viewport_style.VisitedDependentColor(GetCSSPropertyBackgroundColor()) !=
            background_color ||
        viewport_style.BackgroundLayers() != background_layers ||
        viewport_style.ImageRendering() != image_rendering) {
      changed = true;
      new_viewport_style->SetBackgroundColor(StyleColor(background_color));
      new_viewport_style->AccessBackgroundLayers() = background_layers;
      new_viewport_style->SetImageRendering(image_rendering);
    }
  }

  // Overflow
  {
    const ComputedStyle* overflow_style = document_element_style;
    if (body_style &&
        document_element_style->IsOverflowVisibleAlongBothAxes()) {
      overflow_style = body_style;

      // The body element has its own scrolling box, independent from the
      // viewport.  This is a bit of a weird edge case in the CSS spec that we
      // might want to try to eliminate some day (eg. for ScrollTopLeftInterop
      // - see http://crbug.com/157855).
      if (body_style && body_style->IsScrollContainer()) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kBodyScrollsInAdditionToViewport);
      }
    }

    // TODO(954423): overscroll-behavior (and most likely overflow-anchor)
    // should be propagated from the document element and not the viewport
    // defining element.
    PROPAGATE_FROM(overflow_style, OverscrollBehaviorX, SetOverscrollBehaviorX,
                   EOverscrollBehavior::kAuto);
    PROPAGATE_FROM(overflow_style, OverscrollBehaviorY, SetOverscrollBehaviorY,
                   EOverscrollBehavior::kAuto);

    // Counts any time overscroll behavior break if we change its viewport
    // propagation logic. Overscroll behavior only breaks if the body style
    // (i.e. non-document style) was propagated to the viewport and the
    // body style has a different overscroll behavior from the document one.
    // TODO(954423): Remove once propagation logic change is complete.
    if (document_element_style && overflow_style &&
        overflow_style != document_element_style) {
      EOverscrollBehavior document_x =
          document_element_style->OverscrollBehaviorX();
      EOverscrollBehavior document_y =
          document_element_style->OverscrollBehaviorY();
      EOverscrollBehavior body_x = overflow_style->OverscrollBehaviorX();
      EOverscrollBehavior body_y = overflow_style->OverscrollBehaviorY();
      // Document style is auto but body is not: fixing crbug.com/954423 might
      // break the page.
      if ((document_x == EOverscrollBehavior::kAuto && document_x != body_x) ||
          (document_y == EOverscrollBehavior::kAuto && document_y != body_y)) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kOversrollBehaviorOnViewportBreaks);
      }
      // Body style is auto but document is not: currently we are showing the
      // wrong behavior, and fixing crbug.com/954423 gives the correct behavior.
      if ((body_x == EOverscrollBehavior::kAuto && document_x != body_x) ||
          (body_y == EOverscrollBehavior::kAuto && document_y != body_y)) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kOverscrollBehaviorWillBeFixed);
      }
    }

    EOverflow overflow_x = EOverflow::kAuto;
    EOverflow overflow_y = EOverflow::kAuto;
    EOverflowAnchor overflow_anchor = EOverflowAnchor::kAuto;

    if (overflow_style) {
      overflow_x = overflow_style->OverflowX();
      overflow_y = overflow_style->OverflowY();
      overflow_anchor = overflow_style->OverflowAnchor();
      // Visible overflow on the viewport is meaningless, and the spec says to
      // treat it as 'auto'. The spec also says to treat 'clip' as 'hidden'.
      if (overflow_x == EOverflow::kVisible)
        overflow_x = EOverflow::kAuto;
      else if (overflow_x == EOverflow::kClip)
        overflow_x = EOverflow::kHidden;
      if (overflow_y == EOverflow::kVisible)
        overflow_y = EOverflow::kAuto;
      else if (overflow_y == EOverflow::kClip)
        overflow_y = EOverflow::kHidden;
      if (overflow_anchor == EOverflowAnchor::kVisible)
        overflow_anchor = EOverflowAnchor::kAuto;

      if (GetDocument().IsInMainFrame()) {
        using OverscrollBehaviorType = cc::OverscrollBehavior::Type;
        GetDocument().GetPage()->GetChromeClient().SetOverscrollBehavior(
            *GetDocument().GetFrame(),
            cc::OverscrollBehavior(static_cast<OverscrollBehaviorType>(
                                       overflow_style->OverscrollBehaviorX()),
                                   static_cast<OverscrollBehaviorType>(
                                       overflow_style->OverscrollBehaviorY())));
      }

      if (overflow_style->HasCustomScrollbarStyle())
        update_scrollbar_style = true;
    }

    PROPAGATE_VALUE(overflow_x, OverflowX, SetOverflowX)
    PROPAGATE_VALUE(overflow_y, OverflowY, SetOverflowY)
    PROPAGATE_VALUE(overflow_anchor, OverflowAnchor, SetOverflowAnchor);
  }

  // Misc
  {
    PROPAGATE_FROM(document_element_style, GetEffectiveTouchAction,
                   SetEffectiveTouchAction, TouchAction::kAuto);
    PROPAGATE_FROM(document_element_style, GetScrollBehavior, SetScrollBehavior,
                   mojom::blink::ScrollBehavior::kAuto);
    PROPAGATE_FROM(document_element_style, DarkColorScheme, SetDarkColorScheme,
                   false);
    PROPAGATE_FROM(document_element_style, ScrollbarGutter, SetScrollbarGutter,
                   kScrollbarGutterAuto);
    PROPAGATE_FROM(document_element_style, ForcedColorAdjust,
                   SetForcedColorAdjust, EForcedColorAdjust::kAuto);
  }

  changed |= PropagateScrollSnapStyleToViewport(
      GetDocument(), document_element_style, new_viewport_style.get());

  if (changed) {
    new_viewport_style->UpdateFontOrientation();
    FontBuilder(&GetDocument()).CreateInitialFont(*new_viewport_style);
  }
  if (changed || update_scrollbar_style)
    GetDocument().GetLayoutView()->SetStyle(new_viewport_style);
}
#undef PROPAGATE_VALUE
#undef PROPAGATE_FROM

}  // namespace blink
