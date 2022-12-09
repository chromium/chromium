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

#include "base/containers/adapters.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_value_factory.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_color_value.h"
#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_selector_watch.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_try_rule.h"
#include "third_party/blink/renderer/core/css/element_rule_collector.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/page_rule_collector.h"
#include "third_party/blink/renderer/core/css/part_names.h"
#include "third_party/blink/renderer/core/css/post_style_update_scope.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
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
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
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
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/style_initial_data.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

scoped_refptr<const ComputedStyle> BuildInitialStyleForImg(
    const scoped_refptr<const ComputedStyle>& initial_style) {
  if (!RuntimeEnabledFeatures::CSSOverflowForReplacedElementsEnabled())
    return initial_style;

  // This matches the img {} declarations in html.css to avoid copy-on-write
  // when only UA styles apply for these properties. See crbug.com/1369454
  // for details.
  ComputedStyleBuilder builder(*initial_style);
  builder.SetOverflowX(EOverflow::kClip);
  builder.SetOverflowY(EOverflow::kClip);
  builder.SetOverflowClipMargin(StyleOverflowClipMargin::CreateContent());
  return builder.TakeStyle();
}

bool ShouldStoreOldStyle(const StyleRecalcContext& style_recalc_context,
                         StyleResolverState& state) {
  // Storing the old style is only relevant if we risk computing the style
  // more than once for the same element. This is only possible if we are
  // currently inside a container.
  //
  // If we are not inside a container, we can fall back to the default
  // behavior (in CSSAnimations) of using the current style on Element
  // as the old style.
  return style_recalc_context.container && state.CanAffectAnimations();
}

bool ShouldSetPendingUpdate(StyleResolverState& state, Element& element) {
  if (!state.AnimationUpdate().IsEmpty())
    return true;
  // Even when the animation update is empty, we must still set the pending
  // update in order to clear PreviousActiveInterpolationsForAnimations.
  //
  // See CSSAnimations::MaybeApplyPendingUpdate
  if (const ElementAnimations* element_animations =
          element.GetElementAnimations()) {
    return element_animations->CssAnimations()
        .HasPreviousActiveInterpolationsForAnimations();
  }
  return false;
}

void SetAnimationUpdateIfNeeded(const StyleRecalcContext& style_recalc_context,
                                StyleResolverState& state,
                                Element& element) {
  if (auto* data = PostStyleUpdateScope::CurrentAnimationData()) {
    if (ShouldStoreOldStyle(style_recalc_context, state))
      data->StoreOldStyleIfNeeded(element);
  }

  // If any changes to CSS Animations were detected, stash the update away for
  // application after the layout object is updated if we're in the appropriate
  // scope.
  if (!ShouldSetPendingUpdate(state, element))
    return;

  if (auto* data = PostStyleUpdateScope::CurrentAnimationData())
    data->SetPendingUpdate(element, state.AnimationUpdate());
}

ElementAnimations* GetElementAnimations(const StyleResolverState& state) {
  if (!state.GetAnimatingElement())
    return nullptr;
  return state.GetAnimatingElement()->GetElementAnimations();
}

bool HasAnimationsOrTransitions(const StyleResolverState& state) {
  return state.StyleBuilder().Animations() ||
         state.StyleBuilder().Transitions() ||
         (state.GetAnimatingElement() &&
          state.GetAnimatingElement()->HasAnimations());
}

bool HasTimelines(const StyleResolverState& state) {
  if (state.StyleBuilder().ScrollTimelineName())
    return true;
  if (state.StyleBuilder().ViewTimelineName())
    return true;
  if (ElementAnimations* element_animations = GetElementAnimations(state))
    return element_animations->CssAnimations().HasTimelines();
  return false;
}

bool IsAnimationStyleChange(Element& element) {
  if (auto* element_animations = element.GetElementAnimations())
    return element_animations->IsAnimationStyleChange();
  return false;
}

#if DCHECK_IS_ON()
// Compare the base computed style with the one we compute to validate that the
// optimization is sound. A return value of g_null_atom means the diff was
// empty (which is what we want).
String ComputeBaseComputedStyleDiff(const ComputedStyle* base_computed_style,
                                    const ComputedStyle& computed_style) {
  using DebugField = ComputedStyleBase::DebugField;

  if (!base_computed_style)
    return g_null_atom;
  if (*base_computed_style == computed_style)
    return g_null_atom;

  HashSet<DebugField> exclusions;

  // Under certain conditions ComputedStyle::operator==() may return false for
  // differences that are permitted during an animation.
  // The FontFaceCache version number may be increased without forcing a style
  // recalc (see crbug.com/471079).
  if (!base_computed_style->GetFont().IsFallbackValid())
    exclusions.insert(DebugField::font_);

  // Images use instance equality rather than value equality (see
  // crbug.com/781461).
  if (!CSSPropertyEquality::PropertiesEqual(
          PropertyHandle(CSSProperty::Get(CSSPropertyID::kBackgroundImage)),
          *base_computed_style, computed_style)) {
    exclusions.insert(DebugField::background_);
  }
  if (!CSSPropertyEquality::PropertiesEqual(
          PropertyHandle(CSSProperty::Get(CSSPropertyID::kWebkitMaskImage)),
          *base_computed_style, computed_style)) {
    exclusions.insert(DebugField::mask_);
  }

  // Changes to this flag caused by history.pushState do not always mark
  // for recalc in time, yet VisitedLinkState::DetermineLinkState will provide
  // the up-to-date answer when polled.
  //
  // See crbug.com/1158076.
  exclusions.insert(DebugField::inside_link_);

  Vector<DebugField> diff =
      base_computed_style->DebugDiffFields(computed_style);

  StringBuilder builder;

  for (DebugField field : diff) {
    if (exclusions.Contains(field))
      continue;
    builder.Append(ComputedStyleBase::DebugFieldToString(field));
    builder.Append(" ");
  }

  if (builder.empty())
    return g_null_atom;

  return String("Field diff: ") + builder.ReleaseString();
}
#endif  // DCHECK_IS_ON()

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

void PreserveTextAutosizingMultiplierIfNeeded(
    StyleResolverState& state,
    const StyleRequest& style_request) {
  const ComputedStyle* old_style = state.GetElement().GetComputedStyle();
  if (!style_request.IsPseudoStyleRequest() && old_style) {
    state.StyleBuilder().SetTextAutosizingMultiplier(
        old_style->TextAutosizingMultiplier());
  }
}

bool TextAutosizingMultiplierChanged(const StyleResolverState& state,
                                     const ComputedStyle& base_computed_style) {
  // Note that |old_style| can be a style replaced by
  // TextAutosizer::ApplyMultiplier.
  const ComputedStyle* old_style = state.GetElement().GetComputedStyle();
  return old_style && (old_style->TextAutosizingMultiplier() !=
                       base_computed_style.TextAutosizingMultiplier());
}

PseudoId GetPseudoId(const Element& element, ElementRuleCollector* collector) {
  if (element.IsPseudoElement())
    return element.GetPseudoId();

  return collector ? collector->GetPseudoId() : kPseudoIdNone;
}

void UseCountLegacyOverlapping(Document& document,
                               const ComputedStyle& a,
                               const ComputedStyle& b) {
  if (a.PerspectiveOrigin() != b.PerspectiveOrigin())
    document.CountUse(WebFeature::kCSSLegacyPerspectiveOrigin);
  if (a.GetTransformOrigin() != b.GetTransformOrigin())
    document.CountUse(WebFeature::kCSSLegacyTransformOrigin);
  if (a.BorderImage() != b.BorderImage())
    document.CountUse(WebFeature::kCSSLegacyBorderImage);
  if ((a.BorderTopWidth() != b.BorderTopWidth()) ||
      (a.BorderRightWidth() != b.BorderRightWidth()) ||
      (a.BorderBottomWidth() != b.BorderBottomWidth()) ||
      (a.BorderLeftWidth() != b.BorderLeftWidth())) {
    document.CountUse(WebFeature::kCSSLegacyBorderImageWidth);
  }
}

}  // namespace

static CSSPropertyValueSet* LeftToRightDeclaration() {
  DEFINE_STATIC_LOCAL(
      Persistent<MutableCSSPropertyValueSet>, left_to_right_decl,
      (MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode)));
  if (left_to_right_decl->IsEmpty()) {
    left_to_right_decl->SetLonghandProperty(CSSPropertyID::kDirection,
                                            CSSValueID::kLtr);
  }
  return left_to_right_decl;
}

static CSSPropertyValueSet* RightToLeftDeclaration() {
  DEFINE_STATIC_LOCAL(
      Persistent<MutableCSSPropertyValueSet>, right_to_left_decl,
      (MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode)));
  if (right_to_left_decl->IsEmpty()) {
    right_to_left_decl->SetLonghandProperty(CSSPropertyID::kDirection,
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

// The 'color' property conditionally inherits from the *used* value of its
// parent, and we rely on an explicit value in the cascade to implement this.
// https://drafts.csswg.org/css-color-adjust-1/#propdef-forced-color-adjust
static CSSPropertyValueSet* ForcedColorsUserAgentDeclarations() {
  DEFINE_STATIC_LOCAL(
      Persistent<MutableCSSPropertyValueSet>, decl,
      (MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode)));
  if (decl->IsEmpty())
    decl->SetProperty(CSSPropertyID::kColor, *CSSInheritedValue::Create());
  return decl;
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
    : initial_style_(ComputedStyle::CreateInitialStyleSingleton()),
      initial_style_for_img_(BuildInitialStyleForImg(initial_style_)),
      document_(document) {
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
      DCHECK(element.ShadowPseudoId().empty());
#endif
    DCHECK(!element.IsVTTElement());
    return resolver;
  }

  tree_scope = tree_scope->ParentTreeScope();
  if (!tree_scope)
    return nullptr;
  if (element.ShadowPseudoId().empty() && !element.IsVTTElement())
    return nullptr;
  return tree_scope->GetScopedStyleResolver();
}

// Matches :host and :host-context rules if the element is a shadow host.
// It matches rules from the ShadowHostRules of the ScopedStyleResolver
// of the attached shadow root.
static void MatchHostRules(const Element& element,
                           ElementRuleCollector& collector) {
  ShadowRoot* shadow_root = element.GetShadowRoot();
  ScopedStyleResolver* resolver =
      shadow_root ? shadow_root->GetScopedStyleResolver() : nullptr;
  if (!resolver)
    return;
  collector.ClearMatchedRules();
  resolver->CollectMatchingShadowHostRules(collector);
  collector.SortAndTransferMatchedRules();
  collector.FinishAddingAuthorRulesForTreeScope(resolver->GetTreeScope());
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
  HeapVector<std::pair<Member<HTMLSlotElement>, Member<ScopedStyleResolver>>>
      resolvers;
  {
    HTMLSlotElement* slot = element.AssignedSlot();
    if (!slot)
      return;

    for (; slot; slot = slot->AssignedSlot()) {
      if (ScopedStyleResolver* resolver =
              slot->GetTreeScope().GetScopedStyleResolver()) {
        resolvers.push_back(std::make_pair(slot, resolver));
      }
    }
  }

  for (const auto& [slot, resolver] : base::Reversed(resolvers)) {
    ElementRuleCollector::SlottedRulesScope scope(collector, *slot);
    collector.ClearMatchedRules();
    resolver->CollectMatchingSlottedRules(collector);
    collector.SortAndTransferMatchedRules();
    collector.FinishAddingAuthorRulesForTreeScope(slot->GetTreeScope());
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
  if (!styles.empty()) {
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
    collector.SortAndTransferMatchedRules(true /* is_vtt_embedded_style */);
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
      collector.GetPseudoId() == kPseudoIdNone) {
    // Do not add styles depending on style attributes to the
    // MatchedPropertiesCache (MPC) if they have been modified after parsing.
    // The reason is that such declarations are not shared across elements and
    // the caching would effectively only be useful for multiple resolutions for
    // the same element with the exact same styles.
    //
    // For cases where animations are done by modifying the style attribute
    // every frame, making the style cacheable would effectively just fill up
    // the MPC with unnecessary ComputedStyles.
    bool is_inline_style_cacheable = !element.InlineStyle()->IsMutable();
    collector.AddElementStyleProperties(element.InlineStyle(),
                                        is_inline_style_cacheable,
                                        true /* is_inline_style */);
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

void StyleResolver::MatchPseudoPartRules(const Element& part_matching_element,
                                         ElementRuleCollector& collector,
                                         bool for_shadow_pseudo) {
  if (!for_shadow_pseudo)
    MatchPseudoPartRulesForUAHost(part_matching_element, collector);

  DOMTokenList* part = part_matching_element.GetPart();
  if (!part || !part->length() || !part_matching_element.IsInShadowTree())
    return;

  PartNames current_names(part->TokenSet());

  // Consider ::part rules in this element’s tree scope or above. Rules in this
  // element’s tree scope will only match if preceded by a :host or :host() that
  // matches one of its containing shadow hosts (see MatchForRelation).
  for (const Element* element = &part_matching_element; element;
       element = element->OwnerShadowHost()) {
    // Consider the ::part rules for the given scope.
    TreeScope& tree_scope = element->GetTreeScope();
    if (ScopedStyleResolver* resolver = tree_scope.GetScopedStyleResolver()) {
      // PartRulesScope must be provided with the host where we want to start
      // the search for container query containers. For the first iteration of
      // this loop, `element` is the `part_matching_element`, but we want to
      // start the search at `part_matching_element`'s host. For subsequent
      // iterations, `element` is the correct starting element/host.
      const Element* host = (element == &part_matching_element)
                                ? element->OwnerShadowHost()
                                : element;
      DCHECK(IsShadowHost(host));
      ElementRuleCollector::PartRulesScope scope(collector,
                                                 const_cast<Element&>(*host));
      collector.ClearMatchedRules();
      resolver->CollectMatchingPartPseudoRules(collector, current_names,
                                               for_shadow_pseudo);
      collector.SortAndTransferMatchedRules();
      collector.FinishAddingAuthorRulesForTreeScope(resolver->GetTreeScope());
    }

    // If we have now considered the :host/:host() ::part rules in our own tree
    // scope and the ::part rules in the scope directly above...
    if (element != &part_matching_element) {
      // ...then subsequent containing tree scopes require mapping part names
      // through @exportparts before considering ::part rules. If no parts are
      // forwarded, the element is now unreachable and we can stop.
      if (element->HasPartNamesMap())
        current_names.PushMap(*element->PartNamesMap());
      else
        return;
    }
  }
}

void StyleResolver::MatchAuthorRules(
    const Element& element,
    ScopedStyleResolver* element_scope_resolver,
    ElementRuleCollector& collector) {
  MatchHostRules(element, collector);
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

template <typename Functor>
void StyleResolver::ForEachUARulesForElement(const Element& element,
                                             ElementRuleCollector* collector,
                                             Functor& func) const {
  CSSDefaultStyleSheets& default_style_sheets =
      CSSDefaultStyleSheets::Instance();
  if (!print_media_type_) {
    if (LIKELY(element.IsHTMLElement() || element.IsVTTElement())) {
      func(default_style_sheets.DefaultHtmlStyle());
      if (UNLIKELY(IsInMediaUAShadow(element))) {
        func(default_style_sheets.DefaultMediaControlsStyle());
      }
    } else if (element.IsSVGElement()) {
      func(default_style_sheets.DefaultSVGStyle());
    } else if (element.namespaceURI() == mathml_names::kNamespaceURI) {
      func(default_style_sheets.DefaultMathMLStyle());
    }
  } else {
    func(default_style_sheets.DefaultPrintStyle());
  }

  // In quirks mode, we match rules from the quirks user agent sheet.
  if (GetDocument().InQuirksMode())
    func(default_style_sheets.DefaultHtmlQuirksStyle());

  // If document uses view source styles (in view source mode or in xml
  // viewer mode), then we match rules from the view source style sheet.
  if (GetDocument().IsViewSource())
    func(default_style_sheets.DefaultViewSourceStyle());

  // If the system is in forced colors mode, match rules from the forced colors
  // style sheet.
  if (IsForcedColorsModeEnabled())
    func(default_style_sheets.DefaultForcedColorStyle());

  const auto pseudo_id = GetPseudoId(element, collector);
  if (pseudo_id != kPseudoIdNone) {
    if (IsTransitionPseudoElement(pseudo_id)) {
      func(GetDocument().GetStyleEngine().DefaultViewTransitionStyle());
    } else if (auto* rule_set =
                   default_style_sheets.DefaultPseudoElementStyleOrNull()) {
      func(rule_set);
    }
  }
}

void StyleResolver::MatchUARules(const Element& element,
                                 ElementRuleCollector& collector) {
  collector.SetMatchingUARules(true);

  MatchRequest match_request;
  auto func = [&match_request](RuleSet* rules) {
    match_request.AddRuleset(rules, /*style_sheet=*/nullptr);
  };
  ForEachUARulesForElement(element, &collector, func);

  if (!match_request.IsEmpty())
    MatchRuleSets(collector, match_request);

  collector.FinishAddingUARules();
  collector.SetMatchingUARules(false);
}

void StyleResolver::MatchRuleSets(ElementRuleCollector& collector,
                                  const MatchRequest& match_request) {
  collector.ClearMatchedRules();
  collector.CollectMatchingRules(match_request);
  collector.SortAndTransferMatchedRules();
}

void StyleResolver::MatchPresentationalHints(StyleResolverState& state,
                                             ElementRuleCollector& collector) {
  Element& element = state.GetElement();
  if (element.IsStyledElement() && !state.IsForPseudoElement()) {
    // Do not add styles depending on presentation attributes to the
    // MatchedPropertiesCache (MPC) for SVG elements. The reason is that such
    // declarations are not shared across elements and the caching would
    // effectively only be useful for multiple resolutions for the same element
    // with the exact same styles. We do this for SVG elements specifically
    // since we have cases where SVG elements are animated by changing an
    // attribute every frame, filling up the MPC.
    const bool is_cacheable = !element.IsSVGElement();

    collector.AddElementStyleProperties(element.PresentationAttributeStyle(),
                                        is_cacheable);

    // Now we check additional mapped declarations.
    // Tables and table cells share an additional mapped rule that must be
    // applied after all attributes, since their mapped style depends on the
    // values of multiple attributes.
    collector.AddElementStyleProperties(
        element.AdditionalPresentationAttributeStyle(), is_cacheable);

    if (auto* html_element = DynamicTo<HTMLElement>(element)) {
      if (html_element->HasDirectionAuto()) {
        collector.AddElementStyleProperties(
            html_element->CachedDirectionality() == TextDirection::kLtr
                ? LeftToRightDeclaration()
                : RightToLeftDeclaration());
      }
    }
  }
  collector.FinishAddingPresentationalHints();
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
  MatchPresentationalHints(state, collector);

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
  ComputedStyleBuilder builder = InitialStyleBuilderForElement();

  builder.SetZIndex(0);
  builder.SetIsStackingContextWithoutContainment(true);
  builder.SetDisplay(EDisplay::kBlock);
  builder.SetPosition(EPosition::kAbsolute);

  // Document::InheritHtmlAndBodyElementStyles will set the final overflow
  // style values, but they should initially be auto to avoid premature
  // scrollbar removal in PaintLayerScrollableArea::UpdateAfterStyleChange.
  builder.SetOverflowX(EOverflow::kAuto);
  builder.SetOverflowY(EOverflow::kAuto);

  GetDocument().GetStyleEngine().ApplyVisionDeficiencyStyle(builder);

  return builder.TakeStyle();
}

static StyleBaseData* GetBaseData(const StyleResolverState& state) {
  Element* animating_element = state.GetAnimatingElement();
  if (!animating_element)
    return nullptr;
  auto* old_style = animating_element->GetComputedStyle();
  return old_style ? old_style->BaseData().get() : nullptr;
}

static const ComputedStyle* CachedAnimationBaseComputedStyle(
    StyleResolverState& state) {
  if (auto* base_data = GetBaseData(state))
    return base_data->GetBaseComputedStyle();
  return nullptr;
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

// This is the core of computing style for a given element, ie., first compute
// base style and then apply animation style. (Not all elements needing style
// recalc ever hit ResolveStyle(); e.g., the “independent inherited properties
// optimization” can cause it to be skipped.)
//
// Generally, when an element is marked for style recalc, we do not reuse any
// style from previous computations, but re-compute from scratch every time.
// However: If possible, we compute base style only once and cache it, and then
// just apply animation style on top of the cached base style. This is because
// it's a common situation that elements have an unchanging base and then some
// independent animation properties that change every frame and don't affect
// any other properties or elements. (The exceptions can be found in
// CanReuseBaseComputedStyle().) This is known as the “base computed style
// optimization”.
scoped_refptr<ComputedStyle> StyleResolver::ResolveStyle(
    Element* element,
    const StyleRecalcContext& style_recalc_context,
    const StyleRequest& style_request) {
  if (!element) {
    DCHECK(style_request.IsPseudoStyleRequest());
    return nullptr;
  }

  DCHECK(GetDocument().GetFrame());
  DCHECK(GetDocument().GetSettings());

  SelectorFilterParentScope::EnsureParentStackIsPushed();

  // The StyleResolverState is where we actually end up accumulating the
  // computed style. It's just a convenient way of not having to send
  // a lot of input/output variables around between the different functions.
  StyleResolverState state(GetDocument(), *element, &style_recalc_context,
                           style_request);

  STACK_UNINITIALIZED StyleCascade cascade(state);

  // Compute the base style, or reuse an existing cached base style if
  // applicable (ie., only animation has changed). This is the bulk of the
  // style computation itself, also also where the caching for the base
  // computed style optimization happens.
  ApplyBaseStyle(element, style_recalc_context, style_request, state, cascade);

  if (style_request.IsPseudoStyleRequest() && state.HadNoMatchedProperties()) {
    DCHECK(!cascade.InlineStyleLost());
    return state.TakeStyle();
  }

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
          state.StyleBuilder().GetCurrentColor());
    }

    if (RuntimeEnabledFeatures::MathMLCoreEnabled() &&
        IsA<MathMLElement>(element)) {
      ApplyMathMLCustomStyleProperties(element, state);
    }
  } else if (IsHighlightPseudoElement(style_request.pseudo_id)) {
    if (element->GetComputedStyle() &&
        element->GetComputedStyle()->TextShadow() !=
            state.StyleBuilder().TextShadow()) {
      // This counts the usage of text-shadow in CSS highlight pseudos.
      UseCounter::Count(GetDocument(),
                        WebFeature::kTextShadowInHighlightPseudo);
      if (state.StyleBuilder().TextShadow()) {
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
    SetAnimationUpdateIfNeeded(style_recalc_context, state, *animating_element);

  GetDocument().AddViewportUnitFlags(state.Style()->ViewportUnitFlags());

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

void StyleResolver::ApplyInheritance(Element& element,
                                     const StyleRequest& style_request,
                                     StyleResolverState& state) {
  if (UsesHighlightPseudoInheritance(style_request.pseudo_id)) {
    // When resolving highlight styles for children, we need to default all
    // properties (whether or not defined as inherited) to parent values.

    // Sadly, ComputedStyle creation is unavoidable until ElementRuleCollector
    // and friends stop relying on ComputedStyle mutation. The good news is that
    // if the element has no rules for this highlight pseudo, we skip resolution
    // entirely (leaving the scoped_refptr untouched). The bad news is that if
    // the element has rules but no matched properties, we currently clone.

    state.SetStyle(ComputedStyle::Clone(*state.ParentStyle()));
  } else {
    // We use a different initial_style for img elements to match the overrides
    // in html.css. This avoids allocation overhead from copy-on-write when
    // these properties are set only via UA styles. The overhead shows up on
    // motionmark which stress tests this code. See crbub.com/1369454 for
    // details.
    ComputedStyleBuilder builder(IsA<HTMLImageElement>(element)
                                     ? *initial_style_for_img_
                                     : *initial_style_);

    builder.InheritFrom(
        *state.ParentStyle(),
        (!style_request.IsPseudoStyleRequest() && IsAtShadowBoundary(&element))
            ? ComputedStyleBuilder::kAtShadowBoundary
            : ComputedStyleBuilder::kNotAtShadowBoundary);
    state.SetStyle(builder.TakeStyle());

    // contenteditable attribute (implemented by -webkit-user-modify) should
    // be propagated from shadow host to distributed node.
    if (!style_request.IsPseudoStyleRequest() && element.AssignedSlot()) {
      if (Element* parent = element.parentElement()) {
        if (const ComputedStyle* shadow_host_style = parent->GetComputedStyle())
          state.StyleBuilder().SetUserModify(shadow_host_style->UserModify());
      }
    }
  }
}

void StyleResolver::InitStyleAndApplyInheritance(
    Element& element,
    const StyleRequest& style_request,
    StyleResolverState& state) {
  if (AllowsInheritance(style_request, state.ParentStyle())) {
    ApplyInheritance(element, style_request, state);
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
      state.StyleBuilder().SetIsEnsuredOutsideFlatTree();
    }
  }
  state.StyleBuilder().SetStyleType(style_request.pseudo_id);
  state.StyleBuilder().SetPseudoArgument(style_request.pseudo_argument);

  // For highlight inheritance, propagate link visitedness, forced-colors
  // status, and font properties (for font- or glyph-relative offsets in
  // ‘text-shadow’) from the originating element, even if we have no parent
  // highlight ComputedStyle we can inherit from.
  if (UsesHighlightPseudoInheritance(style_request.pseudo_id)) {
    state.StyleBuilder().SetInsideLink(state.ElementLinkState());
    state.StyleBuilder().SetInForcedColorsMode(
        style_request.originating_element_style->InForcedColorsMode());
    state.StyleBuilder().SetForcedColorAdjust(
        style_request.originating_element_style->ForcedColorAdjust());
    state.StyleBuilder().SetFont(
        style_request.originating_element_style->GetFont());
    state.StyleBuilder().SetLineHeight(
        style_request.originating_element_style->LineHeight());
  }

  if (!style_request.IsPseudoStyleRequest() && element.IsLink()) {
    state.StyleBuilder().SetIsLink();
    EInsideLink link_state = state.ElementLinkState();
    if (link_state != EInsideLink::kNotInsideLink) {
      bool force_visited = false;
      probe::ForcePseudoState(&element, CSSSelector::kPseudoVisited,
                              &force_visited);
      if (force_visited)
        link_state = EInsideLink::kInsideVisitedLink;
    }
    state.StyleBuilder().SetInsideLink(link_state);
  }
}

void StyleResolver::ApplyMathMLCustomStyleProperties(
    Element* element,
    StyleResolverState& state) {
  DCHECK(RuntimeEnabledFeatures::MathMLCoreEnabled() &&
         IsA<MathMLElement>(element));
  ComputedStyleBuilder& builder = state.StyleBuilder();
  if (auto* space = DynamicTo<MathMLSpaceElement>(*element)) {
    space->AddMathBaselineIfNeeded(builder, state.CssToLengthConversionData());
  } else if (auto* padded = DynamicTo<MathMLPaddedElement>(*element)) {
    padded->AddMathBaselineIfNeeded(builder, state.CssToLengthConversionData());
    padded->AddMathPaddedDepthIfNeeded(builder,
                                       state.CssToLengthConversionData());
    padded->AddMathPaddedLSpaceIfNeeded(builder,
                                        state.CssToLengthConversionData());
    padded->AddMathPaddedVOffsetIfNeeded(builder,
                                         state.CssToLengthConversionData());
  } else if (auto* fraction = DynamicTo<MathMLFractionElement>(*element)) {
    fraction->AddMathFractionBarThicknessIfNeeded(
        builder, state.CssToLengthConversionData());
  } else if (auto* operator_element =
                 DynamicTo<MathMLOperatorElement>(*element)) {
    operator_element->AddMathLSpaceIfNeeded(builder,
                                            state.CssToLengthConversionData());
    operator_element->AddMathRSpaceIfNeeded(builder,
                                            state.CssToLengthConversionData());
    operator_element->AddMathMinSizeIfNeeded(builder,
                                             state.CssToLengthConversionData());
    operator_element->AddMathMaxSizeIfNeeded(builder,
                                             state.CssToLengthConversionData());
  }
}

bool CanApplyInlineStyleIncrementally(Element* element,
                                      const StyleResolverState& state,
                                      const StyleRequest& style_request) {
  // If non-independent properties are modified, we need to do a full
  // recomputation; otherwise, the properties we're setting could affect
  // the interpretation of other properties (e.g. if a script is setting
  // el.style.fontSize = "24px", that could affect the interpretation
  // of "border-width: 0.2em", but our incremental style recalculation
  // won't update border width).
  //
  // This also covers the case where the inline style got new or removed
  // existing property declarations. We cannot say easily how that would
  // affect the cascade, so we do a full recalculation in that case.
  if (element->GetStyleChangeType() != kInlineIndependentStyleChange) {
    return false;
  }

  // We must, obviously, have an existing style to do incremental calculation.
  if (!element->GetComputedStyle()) {
    return false;
  }

  // Pseudo-elements can't have inline styles. We also don't have the old
  // style in this situation (|element| is the originating element in in
  // this case, so using that style would be wrong).
  if (style_request.IsPseudoStyleRequest()) {
    return false;
  }

  // Links have special handling of visited/not-visited colors (they are
  // represented using special -internal-* properties), which happens
  // during expansion of the CSS cascade. Since incremental style doesn't
  // replicate this behavior, we don't try to compute incremental style
  // for anything that is a link or inside a link.
  if (element->GetComputedStyle()->InsideLink() !=
      EInsideLink::kNotInsideLink) {
    return false;
  }

  // If in the existing style, any inline property _lost_ the cascade
  // (e.g. to an !important class declaration), modifying the ComputedStyle
  // directly may be wrong. This is rare, so we can just skip those cases.
  if (element->GetComputedStyle()->InlineStyleLostCascade()) {
    return false;
  }

  // Custom style callbacks can do style adjustment after style resolution.
  if (element->HasCustomStyleCallbacks()) {
    return false;
  }

  // We don't bother with the root element; it's a special case.
  if (!state.ParentStyle()) {
    return false;
  }

  // We don't currently support combining incremental style and the
  // base computed style animation; we'd have to apply the incremental
  // style onto the base as opposed to the computed style itself,
  // and we don't support that. It should be rare to animate elements
  // _both_ with animations and mutating inline style anyway.
  if (GetElementAnimations(state) || element->GetComputedStyle()->BaseData()) {
    return false;
  }

  const CSSPropertyValueSet* inline_style = element->InlineStyle();
  if (inline_style) {
    int num_properties = inline_style->PropertyCount();
    for (int property_idx = 0; property_idx < num_properties; ++property_idx) {
      CSSPropertyValueSet::PropertyReference property =
          inline_style->PropertyAt(property_idx);

      // If a script mutated inline style properties that are not idempotent,
      // we would not normally even reach this path (we wouldn't get a changed
      // signal saying “inline incremental style modified”, just “style
      // modified”). However, we could have such properties set on inline style
      // _before_ this calculation, and their continued existence blocks us from
      // reusing the style (because e.g. the StyleAdjuster is not necessarily
      // idempotent in such cases).
      if (!CSSProperty::Get(property.Id()).IsIdempotent()) {
        return false;
      }

      // Variables and reverts are resolved in StyleCascade, which we don't run
      // in this path; thus, we cannot support them.
      if (property.Value().IsVariableReferenceValue() ||
          property.Value().IsPendingSubstitutionValue() ||
          property.Value().IsRevertValue() ||
          property.Value().IsRevertLayerValue()) {
        return false;
      }
    }
  }

  return true;
}

// This is the core of computing base style for a given element, ie., the style
// that does not depend on animations.
//
// The typical flow (barring special rules for pseudo-elements and similar) is:
//
//   1. Initialize the style object, by cloning the initial style.
//      (InitStyleAndApplyInheritance() -> ApplyInheritance() ->
//      CreateComputedStyle()).
//   2. Copy any inherited properties from the parent element.
//      (InitStyleAndApplyInheritance() -> ApplyInheritance() ->
//      ComputedStyleBase::InheritFrom()).
//   3. Collect all CSS rules that apply to this element
//      (MatchAllRules(), into ElementRuleCollector).
//   4. Apply all the found rules in the correct order
//      (CascadeAndApplyMatchedProperties(), using StyleCascade).
//
// The base style is cached by the caller if possible (see ResolveStyle() on
// the “base computed style optimization”).
void StyleResolver::ApplyBaseStyleNoCache(
    Element* element,
    const StyleRecalcContext& style_recalc_context,
    const StyleRequest& style_request,
    StyleResolverState& state,
    StyleCascade& cascade) {
  InitStyleAndApplyInheritance(*element, style_request, state);

  // For some very special elements (e.g. <video>): Ensure internal UA style
  // rules that are relevant for the element exist in the stylesheet.
  GetDocument().GetStyleEngine().EnsureUAStyleForElement(*element);

  if (!style_request.IsPseudoStyleRequest() && IsForcedColorsModeEnabled()) {
    cascade.MutableMatchResult().AddMatchedProperties(
        ForcedColorsUserAgentDeclarations());
  }

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
                                 selector_filter_, cascade.MutableMatchResult(),
                                 state.StyleBuilder().InsideLink());

  if (style_request.IsPseudoStyleRequest()) {
    collector.SetPseudoElementStyleRequest(style_request);
    GetDocument().GetStyleEngine().EnsureUAStyleForPseudoElement(
        style_request.pseudo_id);
  }

  // TODO(obrufau): support styling nested pseudo-elements
  if (style_request.rules_to_include == StyleRequest::kUAOnly ||
      (style_request.IsPseudoStyleRequest() && element->IsPseudoElement())) {
    MatchUARules(*element, collector);
  } else {
    MatchAllRules(
        state, collector,
        style_request.matching_behavior != kMatchAllRulesExcludingSMIL);
  }

  if (tracker_)
    AddMatchedRulesToTracker(collector);

  const MatchResult& match_result = collector.MatchedResult();

  if (style_request.IsPseudoStyleRequest()) {
    if (!match_result.HasMatchedProperties()) {
      StyleAdjuster::AdjustComputedStyle(state, nullptr /* element */);
      state.SetHadNoMatchedProperties();
      return;
    }
  }

  // Preserve the text autosizing multiplier on style recalc. Autosizer will
  // update it during layout if needed.
  // NOTE: This must occur before CascadeAndApplyMatchedProperties for correct
  // computation of font-relative lengths.
  PreserveTextAutosizingMultiplierIfNeeded(state, style_request);

  if (match_result.HasNonUniversalHighlightPseudoStyles())
    state.StyleBuilder().SetHasNonUniversalHighlightPseudoStyles(true);
  if (match_result.HasNonUaHighlightPseudoStyles())
    state.StyleBuilder().SetHasNonUaHighlightPseudoStyles(true);

  CascadeAndApplyMatchedProperties(state, cascade);

  if (match_result.HasFlag(MatchFlag::kAffectedByDrag))
    state.StyleBuilder().SetAffectedByDrag();
  if (match_result.HasFlag(MatchFlag::kAffectedByFocusWithin))
    state.StyleBuilder().SetAffectedByFocusWithin();
  if (match_result.HasFlag(MatchFlag::kAffectedByHover))
    state.StyleBuilder().SetAffectedByHover();
  if (match_result.HasFlag(MatchFlag::kAffectedByActive))
    state.StyleBuilder().SetAffectedByActive();
  if (match_result.DependsOnSizeContainerQueries())
    state.Style()->SetDependsOnSizeContainerQueries(true);
  if (match_result.DependsOnStyleContainerQueries())
    state.Style()->SetDependsOnStyleContainerQueries(true);
  if (match_result.FirstLineDependsOnSizeContainerQueries())
    state.StyleBuilder().SetFirstLineDependsOnSizeContainerQueries(true);
  if (match_result.DependsOnStaticViewportUnits())
    state.Style()->SetHasStaticViewportUnits();
  if (match_result.DependsOnDynamicViewportUnits())
    state.Style()->SetHasDynamicViewportUnits();
  if (match_result.DependsOnRemContainerQueries())
    state.Style()->SetHasRemUnits();
  if (match_result.ConditionallyAffectsAnimations())
    state.SetCanAffectAnimations();
  if (!match_result.CustomHighlightNames().empty()) {
    state.StyleBuilder().SetCustomHighlightNames(
        match_result.CustomHighlightNames());
  }
  state.StyleBuilder().SetPseudoElementStyles(
      match_result.PseudoElementStyles());

  ApplyCallbackSelectors(state);

  // Cache our original display.
  state.StyleBuilder().SetOriginalDisplay(state.StyleBuilder().Display());

  StyleAdjuster::AdjustComputedStyle(
      state, style_request.IsPseudoStyleRequest() ? nullptr : element);
}

// In the normal case, just a forwarder to ApplyBaseStyleNoCache(); see that
// function for the meat of the computation. However, this is where the
// “computed base style optimization” is applied if possible, and also
// incremental inline style updates:
//
// If we have an existing computed style, and the only changes have been
// mutations of independent properties on the element's inline style
// (see CanApplyInlineStyleIncrementally() for the precise conditions),
// we may reuse the old computed style and just reapply the element's
// inline style on top of it. This allows us to skip collecting elements
// and computing the full cascade, which can be a significant win when
// animating elements via inline style from JavaScript.
void StyleResolver::ApplyBaseStyle(
    Element* element,
    const StyleRecalcContext& style_recalc_context,
    const StyleRequest& style_request,
    StyleResolverState& state,
    StyleCascade& cascade) {
  DCHECK(style_request.pseudo_id != kPseudoIdFirstLineInherited);

  if (state.CanCacheBaseStyle() && CanReuseBaseComputedStyle(state)) {
    const ComputedStyle* animation_base_computed_style =
        CachedAnimationBaseComputedStyle(state);
    DCHECK(animation_base_computed_style);
#if DCHECK_IS_ON()
    // The invariant in the base computed style optimization is that as long as
    // |IsAnimationStyleChange| is true, the computed style that would be
    // generated by the style resolver is equivalent to the one we hold
    // internally. To ensure this, we always compute a new style here
    // disregarding the fact that we have a base computed style when DCHECKs are
    // enabled, and call ComputeBaseComputedStyleDiff() to check that the
    // optimization was sound.
    ApplyBaseStyleNoCache(element, style_recalc_context, style_request, state,
                          cascade);
    scoped_refptr<const ComputedStyle> style_snapshot =
        state.StyleBuilder().CloneStyle();
    DCHECK_EQ(g_null_atom, ComputeBaseComputedStyleDiff(
                               animation_base_computed_style, *style_snapshot));
#endif

    state.SetStyle(ComputedStyle::Clone(*animation_base_computed_style));
    state.StyleBuilder().SetBaseData(
        scoped_refptr<StyleBaseData>(GetBaseData(state)));
    state.StyleBuilder().SetStyleType(style_request.pseudo_id);
    if (!state.ParentStyle()) {
      state.SetParentStyle(InitialStyleForElement());
      state.SetLayoutParentStyle(state.ParentStyle());
    }
    MaybeResetCascade(cascade);
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  base_styles_used, 1);
    return;
  }

  if (!style_recalc_context.parent_forces_recalc &&
      CanApplyInlineStyleIncrementally(element, state, style_request)) {
    // We are in a situation where we can reuse the old style
    // and just apply the element's inline style on top of it
    // (see the function comment).
    state.SetStyle(ComputedStyle::Clone(*element->GetComputedStyle()));

    const CSSPropertyValueSet* inline_style = element->InlineStyle();
    if (inline_style) {
      int num_properties = inline_style->PropertyCount();
      for (int property_idx = 0; property_idx < num_properties;
           ++property_idx) {
        CSSPropertyValueSet::PropertyReference property =
            inline_style->PropertyAt(property_idx);
        StyleBuilder::ApplyProperty(
            property.Name(), state,
            ScopedCSSValue(property.Value(), &GetDocument()));
      }
    }

    // AdjustComputedStyle() will set these flags if needed,
    // but will (generally) not unset them, so reset them before
    // computation.
    state.Style()->SetIsStackingContextWithoutContainment(false);
    state.StyleBuilder()
        .SetInsideFragmentationContextWithNondeterministicEngine(
            state.ParentStyle()
                ->InsideFragmentationContextWithNondeterministicEngine());

    StyleAdjuster::AdjustComputedStyle(
        state, style_request.IsPseudoStyleRequest() ? nullptr : element);

    // Normally done by StyleResolver::MaybeAddToMatchedPropertiesCache(),
    // when applying the cascade. Note that this is probably redundant
    // (we'll be loading pending resources later), but not doing so would
    // currently create diffs below.
    state.LoadPendingResources();

#if DCHECK_IS_ON()
    // Verify that we got the right answer.
    scoped_refptr<ComputedStyle> incremental_style = state.TakeStyle();
    ApplyBaseStyleNoCache(element, style_recalc_context, style_request, state,
                          cascade);

    // Having false positives here is OK (and can happen if an inline style
    // element used to be “inherit” but no longer is); it is only used to see
    // whether parent elements need to propagate inherited properties down to
    // children or not. We'd be doing too much work in such cases, but still
    // maintain correctness.
    if (incremental_style->HasExplicitInheritance()) {
      state.StyleBuilder().SetHasExplicitInheritance();
    }

    // Similarly, if a style went from using viewport units to not,
    // the flags can stick around in the incremental version. This can cause
    // invalidations when none are needed, but is otherwise harmless.
    state.Style()->SetViewportUnitFlags(state.Style()->ViewportUnitFlags() |
                                        incremental_style->ViewportUnitFlags());

    DCHECK_EQ(g_null_atom, ComputeBaseComputedStyleDiff(incremental_style.get(),
                                                        *state.Style()));
    // The incremental style must not contain BaseData, otherwise we'd risk
    // creating an infinite chain of BaseData/ComputedStyle in
    // ApplyAnimatedStyle.
    DCHECK(!incremental_style->BaseData());
#endif
    return;
  }

  // None of the caches applied, so we need a full recalculation.
  ApplyBaseStyleNoCache(element, style_recalc_context, style_request, state,
                        cascade);
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
  StyleResolverState state(element.GetDocument(), element,
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(parent_style));
  state.SetStyle(ComputedStyle::Clone(base_style));
  if (value) {
    STACK_UNINITIALIZED StyleCascade cascade(state);
    auto* set =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(state.GetParserMode());
    set->SetProperty(property.GetCSSPropertyName(), *value);
    cascade.MutableMatchResult().FinishAddingUARules();
    cascade.MutableMatchResult().FinishAddingUserRules();
    cascade.MutableMatchResult().FinishAddingPresentationalHints();
    cascade.MutableMatchResult().AddMatchedProperties(set);
    cascade.MutableMatchResult().FinishAddingAuthorRulesForTreeScope(
        element.GetTreeScope());
    cascade.Apply();
  }
  scoped_refptr<const ComputedStyle> style = state.TakeStyle();
  return CompositorKeyframeValueFactory::Create(property, *style, offset);
}

scoped_refptr<const ComputedStyle> StyleResolver::StyleForPage(
    uint32_t page_index,
    const AtomicString& page_name) {
  scoped_refptr<const ComputedStyle> initial_style = InitialStyleForElement();
  if (!GetDocument().documentElement())
    return initial_style;

  StyleResolverState state(GetDocument(), *GetDocument().documentElement(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(initial_style.get()));

  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  const ComputedStyle* root_element_style =
      state.RootElementStyle() ? state.RootElementStyle()
                               : GetDocument().GetComputedStyle();
  DCHECK(root_element_style);
  builder.InheritFrom(*root_element_style);
  state.SetStyle(builder.TakeStyle());

  STACK_UNINITIALIZED StyleCascade cascade(state);

  PageRuleCollector collector(root_element_style, page_index, page_name,
                              cascade.MutableMatchResult());

  collector.MatchPageRules(
      CSSDefaultStyleSheets::Instance().DefaultPrintStyle(),
      nullptr /* layer_map */);

  if (ScopedStyleResolver* scoped_resolver =
          GetDocument().GetScopedStyleResolver())
    scoped_resolver->MatchPageRules(collector);

  cascade.Apply();

  // Now return the style.
  return state.TakeStyle();
}

const ComputedStyle& StyleResolver::InitialStyle() const {
  DCHECK(initial_style_);
  return *initial_style_;
}

scoped_refptr<ComputedStyle> StyleResolver::CreateComputedStyle() const {
  DCHECK(initial_style_);
  return ComputedStyle::Clone(*initial_style_);
}

ComputedStyleBuilder StyleResolver::CreateComputedStyleBuilder() const {
  DCHECK(initial_style_);
  return ComputedStyleBuilder(*initial_style_);
}

float StyleResolver::InitialZoom() const {
  const Document& document = GetDocument();
  if (const LocalFrame* frame = document.GetFrame())
    return !document.Printing() ? frame->PageZoomFactor() : 1;
  return 1;
}

ComputedStyleBuilder StyleResolver::InitialStyleBuilderForElement() const {
  StyleEngine& engine = GetDocument().GetStyleEngine();

  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.SetRtlOrdering(GetDocument().VisuallyOrdered() ? EOrder::kVisual
                                                         : EOrder::kLogical);
  builder.SetZoom(InitialZoom());
  builder.SetEffectiveZoom(InitialZoom());
  builder.SetInForcedColorsMode(GetDocument().InForcedColorsMode());
  builder.SetTapHighlightColor(
      ComputedStyleInitialValues::InitialTapHighlightColor());

  builder.SetUsedColorScheme(engine.GetPageColorSchemes(),
                             engine.GetPreferredColorScheme(),
                             engine.GetForceDarkModeEnabled());

  FontDescription document_font_description = builder.GetFontDescription();
  document_font_description.SetLocale(
      LayoutLocale::Get(GetDocument().ContentLanguage()));

  builder.SetFontDescription(document_font_description);
  builder.SetUserModify(GetDocument().InDesignMode() ? EUserModify::kReadWrite
                                                     : EUserModify::kReadOnly);
  FontBuilder(&GetDocument()).CreateInitialFont(builder);

  scoped_refptr<StyleInitialData> initial_data =
      engine.MaybeCreateAndGetInitialData();
  if (initial_data)
    builder.SetInitialData(std::move(initial_data));

  return builder;
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

void StyleResolver::AddMatchedRulesToTracker(
    const ElementRuleCollector& collector) {
  collector.AddMatchedRulesToTracker(tracker_);
}

StyleRuleList* StyleResolver::StyleRulesForElement(Element* element,
                                                   unsigned rules_to_include) {
  DCHECK(element);
  StyleResolverState state(GetDocument(), *element);
  MatchResult match_result;
  ElementRuleCollector collector(
      state.ElementContext(), StyleRecalcContext::FromAncestors(*element),
      selector_filter_, match_result, EInsideLink::kNotInsideLink);
  collector.SetMode(SelectorChecker::kCollectingStyleRules);
  CollectPseudoRulesForElement(*element, collector, kPseudoIdNone, g_null_atom,
                               rules_to_include);
  return collector.MatchedStyleRuleList();
}

HeapHashMap<CSSPropertyName, Member<const CSSValue>>
StyleResolver::CascadedValuesForElement(Element* element, PseudoId pseudo_id) {
  StyleResolverState state(GetDocument(), *element);
  state.SetStyle(CreateComputedStyle());

  STACK_UNINITIALIZED StyleCascade cascade(state);
  ElementRuleCollector collector(state.ElementContext(),
                                 StyleRecalcContext::FromAncestors(*element),
                                 selector_filter_, cascade.MutableMatchResult(),
                                 EInsideLink::kNotInsideLink);
  collector.SetPseudoElementStyleRequest(StyleRequest(pseudo_id, nullptr));
  MatchAllRules(state, collector, false /* include_smil_properties */);

  cascade.Apply();
  return cascade.GetCascadedValues();
}

Element* StyleResolver::FindContainerForElement(
    Element* element,
    const ContainerSelector& container_selector) {
  DCHECK(element);
  return ContainerQueryEvaluator::FindContainer(
      element->ParentOrShadowHostElement(), container_selector);
}

RuleIndexList* StyleResolver::PseudoCSSRulesForElement(
    Element* element,
    PseudoId pseudo_id,
    const AtomicString& view_transition_name,
    unsigned rules_to_include) {
  DCHECK(element);
  StyleResolverState state(GetDocument(), *element);
  MatchResult match_result;
  StyleRecalcContext style_recalc_context =
      StyleRecalcContext::FromAncestors(*element);
  ElementRuleCollector collector(state.ElementContext(), style_recalc_context,
                                 selector_filter_, match_result,
                                 state.ElementLinkState());
  collector.SetMode(SelectorChecker::kCollectingCSSRules);
  // TODO(obrufau): support collecting rules for nested ::marker
  if (!element->IsPseudoElement()) {
    CollectPseudoRulesForElement(*element, collector, pseudo_id,
                                 view_transition_name, rules_to_include);
  }

  if (tracker_)
    AddMatchedRulesToTracker(collector);
  return collector.MatchedCSSRuleList();
}

RuleIndexList* StyleResolver::CssRulesForElement(Element* element,
                                                 unsigned rules_to_include) {
  return PseudoCSSRulesForElement(element, kPseudoIdNone, g_null_atom,
                                  rules_to_include);
}

void StyleResolver::CollectPseudoRulesForElement(
    const Element& element,
    ElementRuleCollector& collector,
    PseudoId pseudo_id,
    const AtomicString& view_transition_name,
    unsigned rules_to_include) {
  collector.SetPseudoElementStyleRequest(
      StyleRequest(pseudo_id, nullptr, view_transition_name));

  if (rules_to_include & kUACSSRules)
    MatchUARules(element, collector);
  else
    collector.FinishAddingUARules();

  if (rules_to_include & kUserCSSRules)
    MatchUserRules(collector);
  else
    collector.FinishAddingUserRules();

  collector.FinishAddingPresentationalHints();

  if (rules_to_include & kAuthorCSSRules) {
    collector.SetSameOriginOnly(!(rules_to_include & kCrossOriginCSSRules));
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

  if (HasTimelines(state)) {
    CSSAnimations::CalculateTimelineUpdate(
        state.AnimationUpdate(), *animating_element, state.StyleBuilder());
  }

  if (!HasAnimationsOrTransitions(state))
    return false;

  // TODO(crbug.com/1276575) : This assert is currently hit for nested ::marker
  // pseudo elements.
  DCHECK(animating_element == &element ||
         DynamicTo<PseudoElement>(animating_element)->OriginatingElement() ==
             &element);

  if (!IsAnimationStyleChange(*animating_element) ||
      !state.StyleBuilder().BaseData()) {
    state.StyleBuilder().SetBaseData(StyleBaseData::Create(
        state.StyleBuilder().CloneStyle(), cascade.GetImportantSet()));
  }

  CSSAnimations::CalculateAnimationUpdate(
      state.AnimationUpdate(), *animating_element, state.GetElement(),
      state.StyleBuilder(), state.ParentStyle(), this);
  CSSAnimations::CalculateTransitionUpdate(
      state.AnimationUpdate(), *animating_element, state.StyleBuilder());

  bool apply = !state.AnimationUpdate().IsEmpty();
  if (apply) {
    const ActiveInterpolationsMap& animations =
        state.AnimationUpdate().ActiveInterpolationsForAnimations();
    const ActiveInterpolationsMap& transitions =
        state.AnimationUpdate().ActiveInterpolationsForTransitions();

    cascade.AddInterpolations(&animations, CascadeOrigin::kAnimation);
    cascade.AddInterpolations(&transitions, CascadeOrigin::kTransition);

    CascadeFilter filter;
    if (state.StyleBuilder().StyleType() == kPseudoIdMarker)
      filter = filter.Add(CSSProperty::kValidForMarker, false);
    if (IsHighlightPseudoElement(state.StyleBuilder().StyleType())) {
      if (UsesHighlightPseudoInheritance(state.StyleBuilder().StyleType())) {
        filter = filter.Add(CSSProperty::kValidForHighlight, false);
      } else {
        filter = filter.Add(CSSProperty::kValidForHighlightLegacy, false);
      }
    }
    filter = filter.Add(CSSProperty::kAnimation, true);

    cascade.Apply(filter);

    // Start loading resources used by animations.
    state.LoadPendingResources();

    DCHECK(!state.GetFontBuilder().FontDirty());
  }

  CSSAnimations::CalculateCompositorAnimationUpdate(
      state.AnimationUpdate(), *animating_element, element,
      *state.StyleBuilder().GetBaseComputedStyle(), state.ParentStyle(),
      WasViewportResized(), state.AffectsCompositorSnapshots());
  CSSAnimations::SnapshotCompositorKeyframes(
      *animating_element, state.AnimationUpdate(),
      *state.StyleBuilder().GetBaseComputedStyle(), state.ParentStyle());
  CSSAnimations::UpdateAnimationFlags(
      *animating_element, state.AnimationUpdate(), state.StyleBuilder());

  return apply;
}

StyleRuleKeyframes* StyleResolver::FindKeyframesRule(
    const Element* element,
    const Element* animating_element,
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

  // Match UA keyframe rules after user and author rules.
  StyleRuleKeyframes* matched_keyframes_rule = nullptr;
  auto func = [&matched_keyframes_rule, &animation_name](RuleSet* rules) {
    auto keyframes_rules = rules->KeyframesRules();
    for (auto& keyframes_rule : keyframes_rules) {
      if (keyframes_rule->GetName() == animation_name)
        matched_keyframes_rule = keyframes_rule;
    }
  };
  ForEachUARulesForElement(*animating_element, nullptr, func);
  if (matched_keyframes_rule)
    return matched_keyframes_rule;

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
    const ComputedStyleBuilder& builder) const {
  if (!cached_matched_properties)
    return false;
  return cached_matched_properties->computed_style->EffectiveZoom() !=
         builder.EffectiveZoom();
}

bool StyleResolver::CacheSuccess::FontChanged(
    const ComputedStyleBuilder& builder) const {
  if (!cached_matched_properties)
    return false;
  return cached_matched_properties->computed_style->GetFontDescription() !=
         builder.GetFontDescription();
}

bool StyleResolver::CacheSuccess::InheritedVariablesChanged(
    const ComputedStyleBuilder& builder) const {
  if (!cached_matched_properties)
    return false;
  return cached_matched_properties->computed_style->InheritedVariables() !=
         builder.InheritedVariables();
}

bool StyleResolver::CacheSuccess::IsUsableAfterApplyInheritedOnly(
    const ComputedStyleBuilder& builder) const {
  return !EffectiveZoomChanged(builder) && !FontChanged(builder) &&
         !InheritedVariablesChanged(builder);
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

  AtomicString pseudo_argument = state.StyleBuilder().PseudoArgument();
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

      EInsideLink link_status = state.StyleBuilder().InsideLink();
      // If the cache item parent style has identical inherited properties to
      // the current parent style then the resulting style will be identical
      // too. We copy the inherited properties over from the cache and are done.
      state.StyleBuilder().InheritFrom(
          *cached_matched_properties->computed_style);

      // Unfortunately the 'link status' is treated like an inherited property.
      // We need to explicitly restore it.
      state.StyleBuilder().SetInsideLink(link_status);

      is_inherited_cache_hit = true;
    }
    if (!IsForcedColorsModeEnabled() || is_inherited_cache_hit) {
      bool non_universal_highlights =
          state.StyleBuilder().HasNonUniversalHighlightPseudoStyles();
      bool non_ua_highlights =
          state.StyleBuilder().HasNonUaHighlightPseudoStyles();

      state.StyleBuilder().CopyNonInheritedFromCached(
          *cached_matched_properties->computed_style);

      // Restore the non-universal highlight pseudo flag that was set while
      // collecting matching rules. These fields are in a raredata field group,
      // so CopyNonInheritedFromCached will clobber them despite custom_copy.
      // TODO(crbug.com/1024156): do this for CustomHighlightNames too, so we
      // can remove the cache-busting for ::highlight() in IsStyleCacheable
      state.StyleBuilder().SetHasNonUniversalHighlightPseudoStyles(
          non_universal_highlights);
      state.StyleBuilder().SetHasNonUaHighlightPseudoStyles(non_ua_highlights);

      // If the child style is a cache hit, we'll never reach StyleBuilder::
      // ApplyProperty, hence we'll never set the flag on the parent.
      // (We do the same thing for independently inherited properties in
      // Element::RecalcOwnStyle().)
      if (state.StyleBuilder().HasExplicitInheritance())
        state.ParentStyle()->SetChildHasExplicitInheritance();
      is_non_inherited_cache_hit = true;
    }
    state.UpdateFont();
  }
  // This is needed because pseudo_argument is copied to the state.Style() as
  // part of a raredata field when copying non-inherited values from the cached
  // result. The argument isn't a style property per se, it represents the
  // argument to the matching element which should remain unchanged.
  state.StyleBuilder().SetPseudoArgument(pseudo_argument);

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
    matched_properties_cache_.Add(cache_success.key,
                                  state.StyleBuilder().CloneStyle(),
                                  ComputedStyle::Clone(*state.ParentStyle()));
  }
}

bool StyleResolver::CanReuseBaseComputedStyle(const StyleResolverState& state) {
  ElementAnimations* element_animations = GetElementAnimations(state);
  if (!element_animations || !element_animations->IsAnimationStyleChange())
    return false;

  StyleBaseData* base_data = GetBaseData(state);
  if (!base_data || !base_data->GetBaseComputedStyle())
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
    if (base_data->GetBaseComputedStyle()->HasFontRelativeUnits())
      return false;
  }

  // Likewise, When applying an animation or transition for line-height, lh unit
  // lengths in the base style must respond to the animation.
  if (CSSAnimations::IsAnimatingLineHeightProperty(element_animations)) {
    if (base_data->GetBaseComputedStyle()->HasLineHeightRelativeUnits())
      return false;
  }

  // Normally, we apply all active animation effects on top of the style created
  // by regular CSS declarations. However, !important declarations have a
  // higher priority than animation effects [1]. If we're currently animating
  // (not transitioning) a property which was declared !important in the base
  // style, we disable the base computed style optimization.
  // [1] https://drafts.csswg.org/css-cascade-4/#cascade-origin
  if (CSSAnimations::IsAnimatingStandardProperties(
          element_animations, base_data->GetBaseImportantSet(),
          KeyframeEffect::kDefaultPriority)) {
    return false;
  }

  if (TextAutosizingMultiplierChanged(state,
                                      *base_data->GetBaseComputedStyle())) {
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
  set->SetProperty(property_name, value);
  cascade.MutableMatchResult().FinishAddingUARules();
  cascade.MutableMatchResult().FinishAddingUserRules();
  cascade.MutableMatchResult().FinishAddingPresentationalHints();
  cascade.MutableMatchResult().AddMatchedProperties(set);
  cascade.MutableMatchResult().FinishAddingAuthorRulesForTreeScope(
      element->GetTreeScope());
  cascade.Apply();

  CSSPropertyRef property_ref(property_name, element->GetDocument());
  scoped_refptr<const ComputedStyle> style = state.TakeStyle();
  return ComputedStyleUtils::ComputedPropertyValue(property_ref.GetProperty(),
                                                   *style);
}

FilterOperations StyleResolver::ComputeFilterOperations(
    Element* element,
    const Font& font,
    const CSSValue& filter_value) {
  ComputedStyleBuilder parent_builder = CreateComputedStyleBuilder();
  parent_builder.SetFont(font);
  scoped_refptr<const ComputedStyle> parent = parent_builder.TakeStyle();

  StyleResolverState state(GetDocument(), *element,
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(parent.get()));

  state.SetStyle(ComputedStyle::Clone(*parent));

  StyleBuilder::ApplyProperty(GetCSSPropertyFilter(), state,
                              ScopedCSSValue(filter_value, &GetDocument()));

  state.LoadPendingResources();

  scoped_refptr<const ComputedStyle> style = state.TakeStyle();
  return style->Filter();
}

scoped_refptr<ComputedStyle> StyleResolver::StyleForInterpolations(
    Element& element,
    ActiveInterpolationsMap& interpolations) {
  StyleRecalcContext style_recalc_context =
      StyleRecalcContext::FromAncestors(element);
  StyleRequest style_request;
  StyleResolverState state(GetDocument(), element, &style_recalc_context,
                           style_request);
  STACK_UNINITIALIZED StyleCascade cascade(state);

  ApplyBaseStyle(&element, style_recalc_context, style_request, state, cascade);
  state.StyleBuilder().SetBaseData(StyleBaseData::Create(
      state.StyleBuilder().CloneStyle(), cascade.GetImportantSet()));

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

  state.StyleBuilder().SetBaseData(StyleBaseData::Create(&base_style, nullptr));

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

  auto apply = [&state, &cascade, &cache_success](CascadeFilter filter) {
    if (cache_success.ShouldApplyInheritedOnly()) {
      cascade.Apply(filter.Add(CSSProperty::kInherited, false));
      if (!cache_success.IsUsableAfterApplyInheritedOnly(state.StyleBuilder()))
        cascade.Apply(filter.Add(CSSProperty::kInherited, true));
    } else {
      cascade.Apply(filter);
    }
  };

  // In order to use-count whether or not legacy overlapping properties
  // made a real difference to the ComputedStyle, we first apply the cascade
  // while filtering out such properties. If the filter did reject
  // any legacy overlapping properties, we apply all overlapping properties
  // again to get the correct result.
  apply(CascadeFilter(CSSProperty::kLegacyOverlapping, true));

  if (state.RejectedLegacyOverlapping()) {
    scoped_refptr<const ComputedStyle> non_legacy_style =
        state.StyleBuilder().CloneStyle();
    // Re-apply all overlapping properties (both legacy and non-legacy).
    apply(CascadeFilter(CSSProperty::kOverlapping, false));
    UseCountLegacyOverlapping(GetDocument(), *non_legacy_style, *state.Style());
  }

  // NOTE: This flag needs to be set before the entry is added to the
  // matched properties cache, or it will be wrong on cache hits.
  state.StyleBuilder().SetInlineStyleLostCascade(cascade.InlineStyleLost());

  MaybeAddToMatchedPropertiesCache(state, cache_success, result);

  DCHECK(!state.GetFontBuilder().FontDirty());
}

void StyleResolver::ApplyCallbackSelectors(StyleResolverState& state) {
  RuleSet* watched_selectors_rule_set =
      GetDocument().GetStyleEngine().WatchedSelectorsRuleSet();
  if (!watched_selectors_rule_set)
    return;

  MatchResult match_result;
  ElementRuleCollector collector(state.ElementContext(), StyleRecalcContext(),
                                 selector_filter_, match_result,
                                 state.StyleBuilder().InsideLink());
  collector.SetMode(SelectorChecker::kCollectingStyleRules);

  MatchRequest match_request(watched_selectors_rule_set);
  collector.CollectMatchingRules(match_request);
  collector.SortAndTransferMatchedRules();

  if (tracker_)
    AddMatchedRulesToTracker(collector);

  StyleRuleList* rules = collector.MatchedStyleRuleList();
  if (!rules)
    return;
  for (auto rule : *rules)
    state.StyleBuilder().AddCallbackSelector(rule->SelectorsText());
}

// Font properties are also handled by FontStyleResolver outside the main
// thread. If you add/remove properties here, make sure they are also properly
// handled by FontStyleResolver.
void StyleResolver::ComputeFont(Element& element,
                                ComputedStyle* style,
                                const CSSPropertyValueSet& property_set) {
  static const CSSProperty* properties[6] = {
      &GetCSSPropertyFontSize(),        &GetCSSPropertyFontFamily(),
      &GetCSSPropertyFontStretch(),     &GetCSSPropertyFontStyle(),
      &GetCSSPropertyFontVariantCaps(), &GetCSSPropertyFontWeight(),
  };

  // TODO(timloh): This is weird, the style is being used as its own parent
  StyleResolverState state(GetDocument(), element,
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(style));
  state.SetStyle(style);
  if (const ComputedStyle* parent_style = element.GetComputedStyle()) {
    state.SetParentStyle(parent_style);
  }

  for (const CSSProperty* property : properties) {
    // TODO(futhark): If we start supporting fonts on ShadowRoot.fonts in
    // addition to Document.fonts, we need to pass the correct TreeScope instead
    // of GetDocument() in the ScopedCSSValue below.
    StyleBuilder::ApplyProperty(
        *property, state,
        ScopedCSSValue(
            *property_set.GetPropertyCSSValue(property->PropertyID()),
            &GetDocument()));
  }
  state.UpdateFont();
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
  visitor->Trace(formatted_text_element_);
}

bool StyleResolver::IsForcedColorsModeEnabled() const {
  return GetDocument().InForcedColorsMode();
}

ComputedStyleBuilder StyleResolver::CreateAnonymousStyleBuilderWithDisplay(
    const ComputedStyle& parent_style,
    EDisplay display) {
  ComputedStyleBuilder builder(*initial_style_);
  builder.InheritFrom(parent_style);
  builder.SetUnicodeBidi(parent_style.GetUnicodeBidi());
  builder.SetDisplay(display);
  return builder;
}

scoped_refptr<const ComputedStyle>
StyleResolver::CreateInheritedDisplayContentsStyleIfNeeded(
    const ComputedStyle& parent_style,
    const ComputedStyle& layout_parent_style) {
  if (parent_style.InheritedEqual(layout_parent_style))
    return nullptr;
  return CreateAnonymousStyleWithDisplay(parent_style, EDisplay::kInline);
}

#define PROPAGATE_FROM(source, getter, setter, initial) \
  PROPAGATE_VALUE(source ? source->getter() : initial, getter, setter);

#define PROPAGATE_VALUE(value, getter, setter)            \
  if ((new_viewport_style_builder.getter()) != (value)) { \
    new_viewport_style_builder.setter(value);             \
    changed = true;                                       \
  }

namespace {

bool PropagateScrollSnapStyleToViewport(
    Document& document,
    const ComputedStyle* document_element_style,
    ComputedStyleBuilder& new_viewport_style_builder) {
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
  ComputedStyleBuilder new_viewport_style_builder(viewport_style);
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
      new_viewport_style_builder.SetBackgroundColor(
          StyleColor(background_color));
      new_viewport_style_builder.AccessBackgroundLayers() = background_layers;
      new_viewport_style_builder.SetImageRendering(image_rendering);
    }
  }

  // Overflow
  {
    const ComputedStyle* overflow_style = document_element_style;
    if (body_style) {
      if (document_element_style->IsOverflowVisibleAlongBothAxes()) {
        overflow_style = body_style;
      } else if (body_style->IsScrollContainer()) {
        // The body element has its own scrolling box, independent from the
        // viewport.  This is a bit of a weird edge case in the CSS spec that
        // we might want to try to eliminate some day (e.g. for
        // ScrollTopLeftInterop - see http://crbug.com/157855).
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

      if (GetDocument().IsInOutermostMainFrame()) {
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
    PROPAGATE_FROM(document_element_style, EffectiveTouchAction,
                   SetEffectiveTouchAction, TouchAction::kAuto);
    PROPAGATE_FROM(document_element_style, GetScrollBehavior, SetScrollBehavior,
                   mojom::blink::ScrollBehavior::kAuto);
    PROPAGATE_FROM(document_element_style, DarkColorScheme, SetDarkColorScheme,
                   false);
    PROPAGATE_FROM(document_element_style, ColorSchemeForced,
                   SetColorSchemeForced, false);
    PROPAGATE_FROM(document_element_style, ScrollbarGutter, SetScrollbarGutter,
                   kScrollbarGutterAuto);
    PROPAGATE_FROM(document_element_style, ForcedColorAdjust,
                   SetForcedColorAdjust, EForcedColorAdjust::kAuto);
  }

  changed |= PropagateScrollSnapStyleToViewport(
      GetDocument(), document_element_style, new_viewport_style_builder);

  if (changed) {
    new_viewport_style_builder.UpdateFontOrientation();
    FontBuilder(&GetDocument()).CreateInitialFont(new_viewport_style_builder);
  }
  if (changed || update_scrollbar_style) {
    GetDocument().GetLayoutView()->SetStyle(
        new_viewport_style_builder.TakeStyle());
  }
}
#undef PROPAGATE_VALUE
#undef PROPAGATE_FROM

scoped_refptr<const ComputedStyle> StyleResolver::StyleForFormattedText(
    bool is_text_run,
    const FontDescription& default_font,
    const CSSPropertyValueSet* css_property_value_set) {
  return StyleForFormattedText(is_text_run, &default_font,
                               /*parent_style*/ nullptr,
                               css_property_value_set);
}

scoped_refptr<const ComputedStyle> StyleResolver::StyleForFormattedText(
    bool is_text_run,
    const ComputedStyle& parent_style,
    const CSSPropertyValueSet* css_property_value_set) {
  return StyleForFormattedText(is_text_run, /*default_font*/ nullptr,
                               &parent_style, css_property_value_set);
}

scoped_refptr<const ComputedStyle> StyleResolver::StyleForFormattedText(
    bool is_text_run,
    const FontDescription* default_font,
    const ComputedStyle* parent_style,
    const CSSPropertyValueSet* css_property_value_set) {
  DCHECK_NE(!!parent_style, !!default_font)
      << "only one of `default_font` or `parent_style` should be specified";

  // Set up our initial style properties based on either the `default_font` or
  // `parent_style`.
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  ComputedStyle* style = builder.MutableInternalStyle();
  if (default_font)
    builder.SetFontDescription(*default_font);
  else  // parent_style
    builder.InheritFrom(*parent_style);
  builder.SetDisplay(is_text_run ? EDisplay::kInline : EDisplay::kBlock);

  // Apply any properties in the `css_property_value_set`.
  if (css_property_value_set) {
    // Use a dummy/disconnected element when resolving the styles so that we
    // don't inherit anything from existing elements.
    StyleResolverState state(
        GetDocument(), EnsureElementForFormattedText(),
        nullptr /* StyleRecalcContext */,
        StyleRequest{parent_style ? parent_style : &InitialStyle()});
    state.SetStyle(style);

    // Use StyleCascade to apply inheritance in the correct order.
    STACK_UNINITIALIZED StyleCascade cascade(state);
    cascade.MutableMatchResult().AddMatchedProperties(
        css_property_value_set,
        AddMatchedPropertiesOptions::Builder().SetIsInlineStyle(true).Build());
    cascade.Apply();

    StyleAdjuster::AdjustComputedStyle(state, nullptr);
  }

  return builder.TakeStyle();
}

static Font ComputeInitialLetterFont(const ComputedStyle& style,
                                     const ComputedStyle& paragraph_style) {
  const StyleInitialLetter& initial_letter = style.InitialLetter();
  DCHECK(!initial_letter.IsNormal());
  const Font& font = style.GetFont();

  const FontMetrics& metrics = font.PrimaryFont()->GetFontMetrics();
  const float cap_height = metrics.CapHeight();
  const float line_height = paragraph_style.ComputedLineHeight();
  const float cap_height_of_para =
      paragraph_style.GetFont().PrimaryFont()->GetFontMetrics().CapHeight();

  // See https://drafts.csswg.org/css-inline/#sizing-initial-letter
  const float desired_cap_height =
      line_height * (initial_letter.Size() - 1) + cap_height_of_para;
  float adjusted_font_size =
      desired_cap_height * style.ComputedFontSize() / cap_height;

  FontDescription adjusted_font_description = style.GetFontDescription();
  adjusted_font_description.SetComputedSize(adjusted_font_size);
  adjusted_font_description.SetSpecifiedSize(adjusted_font_size);
  while (adjusted_font_size > 1) {
    Font actual_font(adjusted_font_description, font.GetFontSelector());
    const float actual_cap_height =
        actual_font.PrimaryFont()->GetFontMetrics().CapHeight();
    if (actual_cap_height <= desired_cap_height)
      return actual_font;
    --adjusted_font_size;
    adjusted_font_description.SetComputedSize(adjusted_font_size);
    adjusted_font_description.SetSpecifiedSize(adjusted_font_size);
  }
  return font;
}

// https://drafts.csswg.org/css-inline/#initial-letter-layout
// 7.5.1. Properties Applying to Initial Letters
// All properties that apply to an inline box also apply to an inline initial
// letter except for
//  * vertical-align and its sub-properties
//  * font-size,
//  * line-height,
//  * text-edge
//  * inline-sizing.
// Additionally, all of the sizing properties and box-sizing also apply to
// initial letters (see [css-sizing-3]).
scoped_refptr<const ComputedStyle> StyleResolver::StyleForInitialLetterText(
    const ComputedStyle& initial_letter_box_style,
    const ComputedStyle& paragraph_style) {
  DCHECK(paragraph_style.InitialLetter().IsNormal());
  DCHECK(!initial_letter_box_style.InitialLetter().IsNormal());
  ComputedStyleBuilder builder = CreateComputedStyleBuilder();
  builder.InheritFrom(initial_letter_box_style);
  builder.SetFont(
      ComputeInitialLetterFont(initial_letter_box_style, paragraph_style));
  builder.SetLineHeight(Length::Fixed(builder.FontHeight()));
  builder.SetVerticalAlign(EVerticalAlign::kBaseline);
  return builder.TakeStyle();
}

Element& StyleResolver::EnsureElementForFormattedText() {
  if (!formatted_text_element_)
    formatted_text_element_ =
        MakeGarbageCollected<Element>(html_names::kSpanTag, &GetDocument());
  return *formatted_text_element_;
}

scoped_refptr<const ComputedStyle> StyleResolver::ResolvePositionFallbackStyle(
    Element& element,
    unsigned index) {
  const ComputedStyle& base_style = element.ComputedStyleRef();
  const ScopedCSSName* position_fallback = base_style.PositionFallback();
  DCHECK(position_fallback);

  const TreeScope* tree_scope = position_fallback->GetTreeScope();
  if (!tree_scope)
    tree_scope = &GetDocument();

  StyleRulePositionFallback* position_fallback_rule = nullptr;
  for (; tree_scope; tree_scope = tree_scope->ParentTreeScope()) {
    if (ScopedStyleResolver* resolver = tree_scope->GetScopedStyleResolver()) {
      position_fallback_rule =
          resolver->PositionFallbackForName(position_fallback->GetName());
      if (position_fallback_rule)
        break;
    }
  }

  // Try UA rules if no author rule matches
  if (!position_fallback_rule) {
    for (const auto& rule : CSSDefaultStyleSheets::Instance()
                                .DefaultHtmlStyle()
                                ->PositionFallbackRules()) {
      if (position_fallback->GetName() == rule->Name()) {
        position_fallback_rule = rule;
        break;
      }
    }
  }

  if (!position_fallback_rule ||
      index >= position_fallback_rule->TryRules().size())
    return nullptr;

  StyleRuleTry* try_rule = position_fallback_rule->TryRules()[index];
  StyleResolverState state(GetDocument(), element);
  state.SetStyle(ComputedStyle::Clone(base_style));
  const CSSPropertyValueSet& properties = try_rule->Properties();

  STACK_UNINITIALIZED StyleCascade cascade(state);
  cascade.MutableMatchResult().FinishAddingUARules();
  cascade.MutableMatchResult().FinishAddingUserRules();
  cascade.MutableMatchResult().FinishAddingPresentationalHints();
  cascade.MutableMatchResult().AddMatchedProperties(&properties);
  cascade.MutableMatchResult().FinishAddingAuthorRulesForTreeScope(*tree_scope);
  cascade.Apply();

  return state.TakeStyle();
}

}  // namespace blink
