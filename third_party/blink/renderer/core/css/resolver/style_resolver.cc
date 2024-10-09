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

#include <optional>

#include "base/containers/adapters.h"
#include "base/types/optional_util.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/web/web_print_page_description.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_value_factory.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/css/anchor_evaluator.h"
#include "third_party/blink/renderer/core/css/cascade_layer_map.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_color_value.h"
#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_position_try_rule.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_selector_watch.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/element_rule_collector.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/out_of_flow_data.h"
#include "third_party/blink/renderer/core/css/page_margins_style.h"
#include "third_party/blink/renderer/core/css/page_rule_collector.h"
#include "third_party/blink/renderer/core/css/part_names.h"
#include "third_party/blink/renderer/core/css/post_style_update_scope.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_filter.h"
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
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_cue.h"
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

bool ShouldStoreOldStyle(const StyleRecalcContext& style_recalc_context,
                         StyleResolverState& state) {
  // Storing the old style is only relevant if we risk computing the style
  // more than once for the same element. This can happen if we are currently
  // inside a size query container, or doing multiple style resolutions for
  // position-try-fallbacks.
  //
  // For anchored elements that generate pseudo elements, we also need to store
  // the old style for animating pseudo elements because style recalc for the
  // originating anchored elements will always update its pseudo elements,
  // causing the pseudo element styling to also have multiple passes.
  //
  // If we are not inside a size query container or an element with
  // position-try-fallbacks, we can fall back to the default behavior (in
  // CSSAnimations) of using the current style on Element as the old style.
  //
  // TODO(crbug.com/40943044): We also need to check whether we are a descendant
  // of an element with position-try-fallbacks to cover the case where the
  // descendant explicitly inherits insets or other valid @position-try
  // properties from the element with position-try-fallbacks. This applies to
  // descendants of elements with anchor queries as well.
  return (style_recalc_context.container ||
          state.StyleBuilder().HasAnchorFunctions() ||
          state.StyleBuilder().PositionAnchor() ||
          (state.IsForPseudoElement() &&
           (state.ParentStyle()->HasAnchorFunctions() ||
            state.ParentStyle()->PositionAnchor())) ||
          state.StyleBuilder().GetPositionTryFallbacks() != nullptr) &&
         state.CanAffectAnimations();
}

bool ShouldSetPendingUpdate(StyleResolverState& state, Element& element) {
  if (!state.AnimationUpdate().IsEmpty()) {
    return true;
  }
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
    if (ShouldStoreOldStyle(style_recalc_context, state)) {
      data->StoreOldStyleIfNeeded(element);
    }
  }

  // If any changes to CSS Animations were detected, stash the update away for
  // application after the layout object is updated if we're in the appropriate
  // scope.
  if (!ShouldSetPendingUpdate(state, element)) {
    return;
  }

  if (auto* data = PostStyleUpdateScope::CurrentAnimationData()) {
    data->SetPendingUpdate(element, state.AnimationUpdate());
  }
}

ElementAnimations* GetElementAnimations(const StyleResolverState& state) {
  if (!state.GetAnimatingElement()) {
    return nullptr;
  }
  return state.GetAnimatingElement()->GetElementAnimations();
}

bool HasAnimationsOrTransitions(const StyleResolverState& state) {
  return state.StyleBuilder().Animations() ||
         state.StyleBuilder().Transitions() ||
         (state.GetAnimatingElement() &&
          state.GetAnimatingElement()->HasAnimations());
}

bool HasTimelines(const StyleResolverState& state) {
  if (state.StyleBuilder().ScrollTimelineName()) {
    return true;
  }
  if (state.StyleBuilder().ViewTimelineName()) {
    return true;
  }
  if (state.StyleBuilder().TimelineScope()) {
    return true;
  }
  if (ElementAnimations* element_animations = GetElementAnimations(state)) {
    return element_animations->CssAnimations().HasTimelines();
  }
  return false;
}

bool IsAnimationStyleChange(Element& element) {
  if (auto* element_animations = element.GetElementAnimations()) {
    return element_animations->IsAnimationStyleChange();
  }
  return false;
}

#if DCHECK_IS_ON()
// Compare the base computed style with the one we compute to validate that the
// optimization is sound. A return value of g_null_atom means the diff was
// empty (which is what we want).
String ComputeBaseComputedStyleDiff(const ComputedStyle* base_computed_style,
                                    const ComputedStyle& computed_style) {
  using DebugDiff = ComputedStyleBase::DebugDiff;
  using DebugField = ComputedStyleBase::DebugField;

  if (!base_computed_style) {
    return g_null_atom;
  }
  if (*base_computed_style == computed_style) {
    return g_null_atom;
  }

  HashSet<DebugField> exclusions;

  // Under certain conditions ComputedStyle::operator==() may return false for
  // differences that are permitted during an animation.
  // The FontFaceCache version number may be increased without forcing a style
  // recalc (see crbug.com/471079).
  if (!base_computed_style->GetFont().IsFallbackValid()) {
    exclusions.insert(DebugField::font_);
  }

  // Images use instance equality rather than value equality (see
  // crbug.com/781461).
  if (!CSSPropertyEquality::PropertiesEqual(
          PropertyHandle(CSSProperty::Get(CSSPropertyID::kBackgroundImage)),
          *base_computed_style, computed_style)) {
    exclusions.insert(DebugField::background_);
  }
  if (!CSSPropertyEquality::PropertiesEqual(
          PropertyHandle(CSSProperty::Get(CSSPropertyID::kMaskImage)),
          *base_computed_style, computed_style)) {
    exclusions.insert(DebugField::mask_);
  }
  if (!CSSPropertyEquality::PropertiesEqual(
          PropertyHandle(CSSProperty::Get(CSSPropertyID::kBorderImageSource)),
          *base_computed_style, computed_style)) {
    exclusions.insert(DebugField::border_image_);
  }

  // Changes to this flag caused by history.pushState do not always mark
  // for recalc in time, yet VisitedLinkState::DetermineLinkState will provide
  // the up-to-date answer when polled.
  //
  // See crbug.com/1158076.
  exclusions.insert(DebugField::inside_link_);

  // HighlightData is calculated after StyleResolver::ResolveStyle, hence any
  // freshly resolved style for diffing purposes will not contain the updated
  // HighlightData. We can safely ignore this because animations and inline
  // styles do not affect the presence or absence of the various highlight
  // styles, and we will invariably update those styles when we return to
  // RecalcOwnStyle, regardless of how ResolveStyle produces its result.
  exclusions.insert(DebugField::highlight_data_);

  Vector<DebugDiff> diff = base_computed_style->DebugDiffFields(computed_style);

  StringBuilder builder;

  for (const DebugDiff& d : diff) {
    if (exclusions.Contains(d.field)) {
      continue;
    }
    builder.Append(ComputedStyleBase::DebugFieldToString(d.field));
    builder.Append("(was ");
    builder.Append(d.actual.c_str());
    builder.Append(", should be ");
    builder.Append(d.correct.c_str());
    builder.Append(") ");
  }

  if (builder.empty()) {
    return g_null_atom;
  }

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

bool TextAutosizingMultiplierChanged(const StyleResolverState& state,
                                     const ComputedStyle& base_computed_style) {
  // Note that |old_style| can be a style replaced by
  // TextAutosizer::ApplyMultiplier.
  const ComputedStyle* old_style = state.GetElement().GetComputedStyle();
  return old_style && (old_style->TextAutosizingMultiplier() !=
                       base_computed_style.TextAutosizingMultiplier());
}

PseudoId GetPseudoId(const Element& element, ElementRuleCollector* collector) {
  if (element.IsPseudoElement()) {
    return element.GetPseudoIdForStyling();
  }

  return collector ? collector->GetPseudoId() : kPseudoIdNone;
}

void UseCountLegacyOverlapping(Document& document,
                               const ComputedStyle& a,
                               const ComputedStyleBuilder& b) {
  if (a.PerspectiveOrigin() != b.PerspectiveOrigin()) {
    document.CountUse(WebFeature::kCSSLegacyPerspectiveOrigin);
  }
  if (a.GetTransformOrigin() != b.GetTransformOrigin()) {
    document.CountUse(WebFeature::kCSSLegacyTransformOrigin);
  }
  if (a.BorderImage() != b.BorderImage()) {
    document.CountUse(WebFeature::kCSSLegacyBorderImage);
  }
  if ((a.BorderTopWidth() != b.BorderTopWidth()) ||
      (a.BorderRightWidth() != b.BorderRightWidth()) ||
      (a.BorderBottomWidth() != b.BorderBottomWidth()) ||
      (a.BorderLeftWidth() != b.BorderLeftWidth())) {
    document.CountUse(WebFeature::kCSSLegacyBorderImageWidth);
  }
}

void ApplyLengthConversionFlags(StyleResolverState& state) {
  using Flags = CSSToLengthConversionData::Flags;
  using Flag = CSSToLengthConversionData::Flag;

  Flags flags = state.TakeLengthConversionFlags();
  if (!flags) {
    return;
  }

  ComputedStyleBuilder& builder = state.StyleBuilder();

  if (flags & static_cast<Flags>(Flag::kEm)) {
    builder.SetHasEmUnits();
  }
  if (flags & static_cast<Flags>(Flag::kRootFontRelative)) {
    builder.SetHasRootFontRelativeUnits();
  }
  if (flags & static_cast<Flags>(Flag::kGlyphRelative)) {
    builder.SetHasGlyphRelativeUnits();
  }
  if (flags & static_cast<Flags>(Flag::kLineHeightRelative)) {
    builder.SetHasLineHeightRelativeUnits();
  }
  if (flags & static_cast<Flags>(Flag::kStaticViewport)) {
    builder.SetHasStaticViewportUnits();
  }
  if (flags & static_cast<Flags>(Flag::kDynamicViewport)) {
    builder.SetHasDynamicViewportUnits();
  }
  if (flags & static_cast<Flags>(Flag::kContainerRelative)) {
    builder.SetDependsOnSizeContainerQueries(true);
    builder.SetHasContainerRelativeUnits();
  }
  if (flags & static_cast<Flags>(Flag::kTreeScopedReference)) {
    state.SetHasTreeScopedReference();
  }
  if (flags & static_cast<Flags>(Flag::kAnchorRelative)) {
    builder.SetHasAnchorFunctions();
  }
  if (flags & static_cast<Flags>(Flag::kLogicalDirectionRelative)) {
    builder.SetHasLogicalDirectionRelativeUnits();
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
  if (decl->IsEmpty()) {
    decl->SetProperty(CSSPropertyID::kColor, *CSSInheritedValue::Create());
  }
  return decl;
}

// UA rule: * { overlay: none !important }
static CSSPropertyValueSet* UniversalOverlayUserAgentDeclaration() {
  DEFINE_STATIC_LOCAL(
      Persistent<MutableCSSPropertyValueSet>, decl,
      (MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode)));

  if (decl->IsEmpty()) {
    decl->SetProperty(CSSPropertyID::kOverlay,
                      *CSSIdentifierValue::Create(CSSValueID::kNone),
                      true /* important */);
  }
  return decl;
}

// UA rule: ::scroll-marker-group { contain: layout size !important; }
// The generation of ::scroll-marker pseudo-elements
// cannot invalidate layout outside of this pseudo-element.
static CSSPropertyValueSet* ScrollMarkerGroupUserAgentDeclaration() {
  DEFINE_STATIC_LOCAL(
      Persistent<MutableCSSPropertyValueSet>, decl,
      (MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode)));

  if (decl->IsEmpty()) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kLayout));
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kSize));
    decl->SetProperty(CSSPropertyID::kContain, *list, /*important=*/true);
  }
  return decl;
}

static void CollectScopedResolversForHostedShadowTrees(
    const Element& element,
    HeapVector<Member<ScopedStyleResolver>, 8>& resolvers) {
  ShadowRoot* root = element.GetShadowRoot();
  if (!root) {
    return;
  }

  // Adding scoped resolver for active shadow roots for shadow host styling.
  if (ScopedStyleResolver* resolver = root->GetScopedStyleResolver()) {
    resolvers.push_back(resolver);
  }
}

StyleResolver::StyleResolver(Document& document)
    : initial_style_(ComputedStyle::GetInitialStyleSingleton()),
      initial_style_for_img_(ComputedStyle::GetInitialStyleForImgSingleton()),
      document_(document) {
  UpdateMediaType();
}

StyleResolver::~StyleResolver() = default;

void StyleResolver::Dispose() {
  matched_properties_cache_.Clear();
}

void StyleResolver::SetRuleUsageTracker(StyleRuleUsageTracker* tracker) {
  tracker_ = tracker;
}

namespace {

inline ScopedStyleResolver* ScopedResolverFor(const Element& element) {
  TreeScope* tree_scope = &element.GetTreeScope();
  if (ScopedStyleResolver* resolver = tree_scope->GetScopedStyleResolver()) {
    DCHECK(!element.IsVTTElement());
    return resolver;
  }

  return nullptr;
}

inline bool UseParentResolverForUAShadowPseudo(
    const Element& element,
    bool* style_attribute_cascaded_in_parent_scope) {
  // Rules for ::cue and custom pseudo elements like
  // ::-webkit-meter-bar pierce through a single shadow dom boundary and apply
  // to elements in sub-scopes.
  TreeScope* tree_scope = element.GetTreeScope().ParentTreeScope();
  if (!tree_scope) {
    return false;
  }
  const AtomicString& shadow_pseudo_id = element.ShadowPseudoId();
  bool is_vtt = element.IsVTTElement();
  if (shadow_pseudo_id.empty() && !is_vtt) {
    return false;
  }
  ScopedStyleResolver* parent_resolver = tree_scope->GetScopedStyleResolver();
  if (!parent_resolver) {
    return true;
  }
  // Going forward, for shadow pseudo IDs that we standardize as
  // pseudo-elements, we expect styles specified by the author using the
  // pseudo-element to override styles specified in style attributes in
  // the user agent shadow DOM.  However, since we have a substantial
  // number of existing uses with :-webkit-* and :-internal-* pseudo
  // elements that do not override the style attribute, we do not apply
  // this (developer-expected) behavior to those existing
  // pseudo-elements.  (It's possible that we could, but it would
  // require a good bit of compatibility analysis.)
  DCHECK(shadow_pseudo_id.empty() || !shadow_pseudo_id.StartsWith("-") ||
         shadow_pseudo_id.StartsWith("-webkit-") ||
         shadow_pseudo_id.StartsWith("-internal-"))
      << "shadow pseudo IDs should either begin with -webkit- or -internal- "
         "or not begin with a -";
  *style_attribute_cascaded_in_parent_scope = shadow_pseudo_id.StartsWith("-");
  return true;
}

// Matches :host and :host-context rules if the element is a shadow host.
// It matches rules from the ShadowHostRules of the ScopedStyleResolver
// of the attached shadow root.
void MatchHostRules(const Element& element,
                    ElementRuleCollector& collector,
                    StyleRuleUsageTracker* tracker) {
  ShadowRoot* shadow_root = element.GetShadowRoot();
  ScopedStyleResolver* resolver =
      shadow_root ? shadow_root->GetScopedStyleResolver() : nullptr;
  if (!resolver) {
    return;
  }
  collector.ClearMatchedRules();
  collector.BeginAddingAuthorRulesForTreeScope(resolver->GetTreeScope());
  resolver->CollectMatchingShadowHostRules(collector);
  collector.SortAndTransferMatchedRules(
      CascadeOrigin::kAuthor, /*is_vtt_embedded_style=*/false, tracker);
}

void MatchSlottedRules(const Element&,
                       ElementRuleCollector&,
                       StyleRuleUsageTracker* tracker);
void MatchSlottedRulesForUAHost(const Element& element,
                                ElementRuleCollector& collector,
                                StyleRuleUsageTracker* tracker) {
  if (element.ShadowPseudoId() !=
      shadow_element_names::kPseudoInputPlaceholder) {
    return;
  }

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
  MatchSlottedRules(*element.OwnerShadowHost(), collector, tracker);
}

// Matches `::slotted` selectors. It matches rules in the element's slot's
// scope. If that slot is itself slotted it will match rules in the slot's
// slot's scope and so on. The result is that it considers a chain of scopes
// descending from the element's own scope.
void MatchSlottedRules(const Element& element,
                       ElementRuleCollector& collector,
                       StyleRuleUsageTracker* tracker) {
  MatchSlottedRulesForUAHost(element, collector, tracker);
  HeapVector<std::pair<Member<HTMLSlotElement>, Member<ScopedStyleResolver>>>
      resolvers;
  {
    HTMLSlotElement* slot = element.AssignedSlot();
    if (!slot) {
      return;
    }

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
    collector.BeginAddingAuthorRulesForTreeScope(slot->GetTreeScope());
    resolver->CollectMatchingSlottedRules(collector);
    collector.SortAndTransferMatchedRules(
        CascadeOrigin::kAuthor, /*is_vtt_embedded_style=*/false, tracker);
  }
}

const TextTrack* GetTextTrackFromElement(const Element& element) {
  if (auto* vtt_element = DynamicTo<VTTElement>(element)) {
    return vtt_element->GetTrack();
  }
  if (auto* vtt_cue_background_box = DynamicTo<VTTCueBackgroundBox>(element)) {
    return vtt_cue_background_box->GetTrack();
  }
  return nullptr;
}

void MatchVTTRules(const Element& element,
                   ElementRuleCollector& collector,
                   StyleRuleUsageTracker* tracker) {
  const TextTrack* text_track = GetTextTrackFromElement(element);
  if (!text_track) {
    return;
  }
  const HeapVector<Member<CSSStyleSheet>>& styles =
      text_track->GetCSSStyleSheets();
  if (!styles.empty()) {
    int style_sheet_index = 0;
    collector.ClearMatchedRules();
    for (CSSStyleSheet* style : styles) {
      StyleEngine& style_engine = element.GetDocument().GetStyleEngine();
      RuleSet* rule_set = style_engine.RuleSetForSheet(*style);
      if (rule_set) {
        collector.CollectMatchingRules(
            MatchRequest(rule_set, nullptr /* scope */, style,
                         style_sheet_index,
                         style_engine.EnsureVTTOriginatingElement()),
            /*part_names*/ nullptr);
        style_sheet_index++;
      }
    }
    collector.SortAndTransferMatchedRules(
        CascadeOrigin::kAuthor, true /* is_vtt_embedded_style */, tracker);
  }
}

void MatchHostPartRules(const Element& element,
                        ElementRuleCollector& collector,
                        StyleRuleUsageTracker* tracker) {
  DOMTokenList* part = element.GetPart();
  if (!part || !part->length() || !element.IsInShadowTree()) {
    return;
  }

  PartNames current_names(part->TokenSet());

  // Consider ::part rules in this element’s tree scope, which only match if
  // preceded by a :host or :host() that matches one of its containing shadow
  // hosts (see MatchForRelation).
  TreeScope& tree_scope = element.GetTreeScope();
  if (ScopedStyleResolver* resolver = tree_scope.GetScopedStyleResolver()) {
    // PartRulesScope must be provided with the host where we want to start
    // the search for container query containers.  For matching :host::part(),
    // we want to start the search at `element`'s host.
    const Element* host = element.OwnerShadowHost();
    ElementRuleCollector::PartRulesScope scope(collector,
                                               const_cast<Element&>(*host));
    resolver->CollectMatchingPartPseudoRules(collector, &current_names, false);
  }
}

void MatchStyleAttribute(const Element& element,
                         ElementRuleCollector& collector,
                         StyleRuleUsageTracker* tracker) {
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
    collector.AddElementStyleProperties(
        element.InlineStyle(), CascadeOrigin::kAuthor,
        is_inline_style_cacheable, true /* is_inline_style */);
  }
}

// Matches rules from the element's scope. The selectors may cross shadow
// boundaries during matching, like for :host-context.
void MatchElementScopeRules(const Element& element,
                            ElementRuleCollector& collector,
                            StyleRuleUsageTracker* tracker) {
  ScopedStyleResolver* element_scope_resolver = ScopedResolverFor(element);
  bool style_attribute_cascaded_in_parent_scope = false;
  bool use_parent_resolver = UseParentResolverForUAShadowPseudo(
      element, &style_attribute_cascaded_in_parent_scope);
  collector.BeginAddingAuthorRulesForTreeScope(element.GetTreeScope());
  if (element_scope_resolver) {
    collector.ClearMatchedRules();
    DCHECK_EQ(&element_scope_resolver->GetTreeScope(), &element.GetTreeScope());
    element_scope_resolver->CollectMatchingElementScopeRules(
        collector, /*part_shadow_host*/ nullptr);
    if (RuntimeEnabledFeatures::CSSCascadeCorrectScopeEnabled()) {
      MatchHostPartRules(element, collector, tracker);
    }
    collector.SortAndTransferMatchedRules(
        CascadeOrigin::kAuthor, /*is_vtt_embedded_style=*/false, tracker);
  }

  if (!style_attribute_cascaded_in_parent_scope) {
    MatchStyleAttribute(element, collector, tracker);
  }

  if (use_parent_resolver &&
      !RuntimeEnabledFeatures::CSSCascadeCorrectScopeEnabled()) {
    ScopedStyleResolver* parent_scope_resolver =
        element.GetTreeScope().ParentTreeScope()->GetScopedStyleResolver();
    if (parent_scope_resolver) {
      // TODO(crbug.com/40280846): Pseudo elements matching elements inside
      // UA shadow trees (::-internal-*, ::-webkit-*, ::placeholder, etc.,
      // although not ::cue) should end up in the same cascade context as
      // other rules from an outer tree (like ::part() rules), and
      // collected separately from the element's tree scope. That should
      // remove the need for the ParentScopedResolver() here.
      collector.ClearMatchedRules();
      collector.BeginAddingAuthorRulesForTreeScope(
          parent_scope_resolver->GetTreeScope());
      parent_scope_resolver->CollectMatchingElementScopeRules(
          collector, /*part_shadow_host*/ nullptr);
      collector.SortAndTransferMatchedRules(
          CascadeOrigin::kAuthor, /*is_vtt_embedded_style=*/false, tracker);
      if (style_attribute_cascaded_in_parent_scope) {
        MatchStyleAttribute(element, collector, tracker);
      }
    } else {
      CHECK(!style_attribute_cascaded_in_parent_scope);
    }
  }
}

void MatchOuterScopeRules(const Element& matching_element,
                          ElementRuleCollector& collector,
                          StyleRuleUsageTracker* tracker) {
  CHECK(RuntimeEnabledFeatures::CSSCascadeCorrectScopeEnabled());

  // Because ::part() is never allowed after ::part(), or after another
  // pseudo-element, and because elements (generally those in UA shadow trees,
  // but this is also used for VTT) that are exposed as pseudos ("shadow
  // pseudos") are never exposed as parts, the rules from a particular scope
  // can only be used for one of the states below.
  enum class MatchingState {
    kDone,
    kShadowPseudo,
    kPart,
    kPartAboveShadowPseudo,
  };

  MatchingState state = MatchingState::kDone;

  // Given an element that we're trying to match, and a scope containing
  // style rules, there is only a single set of part names that can
  // match the element in that scope.  (It doesn't depend on the
  // selector.  It only depends on what parts are exported from each
  // scope to the scope outside it, via either part= or exportparts=.)
  //
  // This does depend on the idea (see above) that the same element can't be
  // exposed as both a UA shadow pseudo and as a part.
  //
  // Present when state is kMatchingPart or kMatchingPartAboveShadowPseudo.
  std::optional<PartNames> current_part_names;

  auto set_part_names = [&current_part_names](const Element* element) -> bool {
    if (DOMTokenList* part = element->GetPart()) {
      if (part->length() && element->IsInShadowTree()) {
        current_part_names.emplace(part->TokenSet());
        return true;
      }
    }
    current_part_names.reset();
    return false;
  };

  bool style_attribute_cascaded_in_parent_scope = false;
  if (set_part_names(&matching_element)) {
    state = MatchingState::kPart;
  } else if (UseParentResolverForUAShadowPseudo(
                 matching_element, &style_attribute_cascaded_in_parent_scope)) {
    state = MatchingState::kShadowPseudo;
  }

  // Consider rules for ::part() and for UA shadow pseudo-elements from scopes
  // outside this tree scope.  Note that :host::part() rules in the element's
  // own scope are considered in MatchElementScopeRules.
  for (const Element* element = matching_element.OwnerShadowHost();
       element && state != MatchingState::kDone;
       element = element->OwnerShadowHost()) {
    // Consider the ::part rules and pseudo-element rules for the given scope.
    TreeScope& tree_scope = element->GetTreeScope();
    if (ScopedStyleResolver* resolver = tree_scope.GetScopedStyleResolver()) {
      // PartRulesScope must be provided with the host where we want to start
      // the search for container query containers.  Since we're not handling
      // :host::part() here, `element` is the correct starting element/host.
      ElementRuleCollector::PartRulesScope scope(
          collector, const_cast<Element&>(*element));
      collector.ClearMatchedRules();
      collector.BeginAddingAuthorRulesForTreeScope(resolver->GetTreeScope());
      if (state == MatchingState::kPart) {
        resolver->CollectMatchingPartPseudoRules(collector,
                                                 &*current_part_names, false);
      } else {
        resolver->CollectMatchingElementScopeRules(
            collector, base::OptionalToPtr(current_part_names));
      }

      collector.SortAndTransferMatchedRules(
          CascadeOrigin::kAuthor, /*is_vtt_embedded_style=*/false, tracker);

      if (style_attribute_cascaded_in_parent_scope) {
        MatchStyleAttribute(matching_element, collector, tracker);
      }
    }

    if (state == MatchingState::kShadowPseudo) {
      CHECK(!current_part_names);
      // The style attribute only goes in the parent scope (in some legacy
      // cases), never higher.
      style_attribute_cascaded_in_parent_scope = false;

      if (set_part_names(element)) {
        state = MatchingState::kPartAboveShadowPseudo;
      } else {
        // For now we only handle shadow pseudos in the parent scope.
        //
        // TODO(https://crbug.com/356158098): In theory this should be an
        // "else if (element->ShadowPseudoId().empty())", since there could be
        // a chain of pseudo-elements in the next scope outside, and we should
        // continue looping when there are more shadow pseudos to match.
        // However, we don't currently parse any such selectors as valid
        // right now, so it seems wasteful to gather rules from the second
        // outer scope (for example, on an element that's conceptually
        // ::-webkit-media-controls-timeline::-webkit-slider-container) when
        // we know none of them will match.
        state = MatchingState::kDone;
      }
    } else {
      CHECK(current_part_names);
      // Subsequent containing tree scopes require mapping part names through
      // @exportparts before considering ::part rules. If no parts are
      // forwarded, the element is now unreachable and we can stop handling
      // ::part() rules.
      if (element->HasPartNamesMap()) {
        current_part_names->PushMap(*element->PartNamesMap());
      } else {
        state = MatchingState::kDone;
      }
    }
  }
}

}  // namespace

void StyleResolver::MatchPseudoPartRulesForUAHost(
    const Element& element,
    ElementRuleCollector& collector) {
  CHECK(!RuntimeEnabledFeatures::CSSCascadeCorrectScopeEnabled());

  const AtomicString& pseudo_id = element.ShadowPseudoId();
  if (pseudo_id == g_null_atom) {
    return;
  }

  // We allow any pseudo element after ::part(). See
  // MatchSlottedRulesForUAHost for a more detailed explanation.
  Element* shadow_host = element.OwnerShadowHost();
  CHECK(shadow_host);
  MatchPseudoPartRules(*shadow_host, collector, /* for_shadow_pseudo */ true);
}

void StyleResolver::MatchPseudoPartRules(const Element& part_matching_element,
                                         ElementRuleCollector& collector,
                                         bool for_shadow_pseudo) {
  CHECK(!RuntimeEnabledFeatures::CSSCascadeCorrectScopeEnabled());

  if (!for_shadow_pseudo) {
    MatchPseudoPartRulesForUAHost(part_matching_element, collector);
  }

  DOMTokenList* part = part_matching_element.GetPart();
  if (!part || !part->length() || !part_matching_element.IsInShadowTree()) {
    return;
  }

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
      collector.BeginAddingAuthorRulesForTreeScope(resolver->GetTreeScope());
      resolver->CollectMatchingPartPseudoRules(collector, &current_names,
                                               for_shadow_pseudo);
      collector.SortAndTransferMatchedRules(
          CascadeOrigin::kAuthor, /*is_vtt_embedded_style=*/false, tracker_);
    }

    // If we have now considered the :host/:host() ::part rules in our own tree
    // scope and the ::part rules in the scope directly above...
    if (element != &part_matching_element) {
      // ...then subsequent containing tree scopes require mapping part names
      // through @exportparts before considering ::part rules. If no parts are
      // forwarded, the element is now unreachable and we can stop.
      if (element->HasPartNamesMap()) {
        current_names.PushMap(*element->PartNamesMap());
      } else {
        return;
      }
    }
  }
}

void StyleResolver::MatchPositionTryRules(ElementRuleCollector& collector) {
  collector.AddTryStyleProperties();
  collector.AddTryTacticsStyleProperties();
}

void StyleResolver::MatchAuthorRules(const Element& element,
                                     ElementRuleCollector& collector) {
  MatchHostRules(element, collector, tracker_);
  MatchSlottedRules(element, collector, tracker_);
  MatchElementScopeRules(element, collector, tracker_);
  if (RuntimeEnabledFeatures::CSSCascadeCorrectScopeEnabled()) {
    MatchOuterScopeRules(element, collector, tracker_);
  } else {
    MatchPseudoPartRules(element, collector);
  }
  MatchVTTRules(element, collector, tracker_);
  MatchPositionTryRules(collector);
}

void StyleResolver::MatchUserRules(ElementRuleCollector& collector) {
  collector.ClearMatchedRules();
  GetDocument().GetStyleEngine().CollectMatchingUserRules(collector);
  collector.SortAndTransferMatchedRules(
      CascadeOrigin::kUser, /*is_vtt_embedded_style=*/false, tracker_);
}

namespace {

bool IsInMediaUAShadow(const Element& element) {
  ShadowRoot* root = element.ContainingShadowRoot();
  if (!root || !root->IsUserAgent()) {
    return false;
  }
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
    if (element.IsHTMLElement() || element.IsVTTElement()) [[likely]] {
      func(default_style_sheets.DefaultHtmlStyle());
    } else if (element.IsSVGElement()) {
      func(default_style_sheets.DefaultSVGStyle());
    } else if (element.namespaceURI() == mathml_names::kNamespaceURI) {
      func(default_style_sheets.DefaultMathMLStyle());
    }
    if (Fullscreen::HasFullscreenElements()) {
      func(default_style_sheets.DefaultFullscreenStyle());
    }
  } else {
    func(default_style_sheets.DefaultPrintStyle());
  }

  // In quirks mode, we match rules from the quirks user agent sheet.
  if (GetDocument().InQuirksMode()) {
    func(default_style_sheets.DefaultHtmlQuirksStyle());
  }

  // If document uses view source styles (in view source mode or in xml
  // viewer mode), then we match rules from the view source style sheet.
  if (GetDocument().IsViewSource()) {
    func(default_style_sheets.DefaultViewSourceStyle());
  }

  // If the system is in forced colors mode, match rules from the forced colors
  // style sheet.
  if (IsForcedColorsModeEnabled()) {
    func(default_style_sheets.DefaultForcedColorStyle());
  }

  if (GetDocument().IsJSONDocument()) {
    func(default_style_sheets.DefaultJSONDocumentStyle());
  }

  const auto pseudo_id = GetPseudoId(element, collector);
  if (pseudo_id == kPseudoIdNone) {
    return;
  }

  auto* rule_set =
      IsTransitionPseudoElement(pseudo_id)
          ? GetDocument().GetStyleEngine().DefaultViewTransitionStyle()
          : default_style_sheets.DefaultPseudoElementStyleOrNull();
  if (rule_set) {
    func(rule_set);
  }
}

void StyleResolver::MatchUARules(const Element& element,
                                 ElementRuleCollector& collector) {
  collector.SetMatchingUARules(true);

  MatchRequest match_request;
  auto func = [&match_request](RuleSet* rules) {
    match_request.AddRuleset(rules);
  };
  ForEachUARulesForElement(element, &collector, func);

  if (!match_request.IsEmpty()) {
    collector.ClearMatchedRules();
    collector.CollectMatchingRules(match_request, /*part_names*/ nullptr);
    collector.SortAndTransferMatchedRules(
        CascadeOrigin::kUserAgent, /*is_vtt_embedded_style=*/false, tracker_);
  }

  if (IsInMediaUAShadow(element)) {
    RuleSet* rule_set =
        IsForcedColorsModeEnabled()
            ? CSSDefaultStyleSheets::Instance()
                  .DefaultForcedColorsMediaControlsStyle()
            : CSSDefaultStyleSheets::Instance().DefaultMediaControlsStyle();
    // Match media controls UA shadow rules in separate UA origin, as they
    // should override UA styles regardless of specificity.
    MatchRequest media_controls_request(rule_set);
    collector.ClearMatchedRules();
    collector.CollectMatchingRules(media_controls_request,
                                   /*part_names*/ nullptr);
    collector.SortAndTransferMatchedRules(
        CascadeOrigin::kUserAgent, /*is_vtt_embedded_style=*/false, tracker_);
  }

  collector.SetMatchingUARules(false);
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

    collector.AddElementStyleProperties(
        element.PresentationAttributeStyle(),
        CascadeOrigin::kAuthorPresentationalHint, is_cacheable);

    // Now we check additional mapped declarations.
    // Tables and table cells share an additional mapped rule that must be
    // applied after all attributes, since their mapped style depends on the
    // values of multiple attributes.
    collector.AddElementStyleProperties(
        element.AdditionalPresentationAttributeStyle(),
        CascadeOrigin::kAuthorPresentationalHint, is_cacheable);

    if (auto* html_element = DynamicTo<HTMLElement>(element)) {
      if (html_element->HasDirectionAuto()) {
        collector.AddElementStyleProperties(
            html_element->CachedDirectionality() == TextDirection::kLtr
                ? LeftToRightDeclaration()
                : RightToLeftDeclaration(),
            CascadeOrigin::kAuthorPresentationalHint);
      }
    }
  }
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

  MatchAuthorRules(element, collector);

  if (element.IsStyledElement() && !state.IsForPseudoElement()) {
    collector.BeginAddingAuthorRulesForTreeScope(element.GetTreeScope());
    // Now check SMIL animation override style.
    auto* svg_element = DynamicTo<SVGElement>(element);
    if (include_smil_properties && svg_element) {
      collector.AddElementStyleProperties(
          svg_element->AnimatedSMILStyleProperties(), CascadeOrigin::kAuthor,
          false /* isCacheable */);
    }
  }
}

const ComputedStyle* StyleResolver::StyleForViewport() {
  ComputedStyleBuilder builder = InitialStyleBuilderForElement();

  builder.SetZIndex(0);
  builder.SetForcesStackingContext(true);
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
  if (!animating_element) {
    return nullptr;
  }
  auto* old_style = animating_element->GetComputedStyle();
  return old_style ? old_style->BaseData() : nullptr;
}

static const ComputedStyle* CachedAnimationBaseComputedStyle(
    StyleResolverState& state) {
  if (auto* base_data = GetBaseData(state)) {
    return base_data->GetBaseComputedStyle();
  }
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
const ComputedStyle* StyleResolver::ResolveStyle(
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
  // style computation itself, also where the caching for the base
  // computed style optimization happens.
  ApplyBaseStyle(element, style_recalc_context, style_request, state, cascade);

  if (style_recalc_context.is_ensuring_style) {
    state.StyleBuilder().SetIsEnsuredInDisplayNone();
  }

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

  ApplyAnchorData(state);

  IncrementResolvedStyleCounters(style_request, GetDocument());

  if (!style_request.IsPseudoStyleRequest()) {
    if (IsA<HTMLBodyElement>(*element)) {
      GetDocument().GetTextLinkColors().SetTextColor(
          state.StyleBuilder().GetCurrentColor());
    }

    if (IsA<MathMLElement>(element)) {
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

  if (Element* animating_element = state.GetAnimatingElement()) {
    SetAnimationUpdateIfNeeded(style_recalc_context, state, *animating_element);
  }

  GetDocument().AddViewportUnitFlags(state.StyleBuilder().ViewportUnitFlags());

  if (state.StyleBuilder().HasRootFontRelativeUnits()) {
    GetDocument().GetStyleEngine().SetUsesRootFontRelativeUnits(true);
  }

  if (state.StyleBuilder().HasGlyphRelativeUnits()) {
    GetDocument().GetStyleEngine().SetUsesGlyphRelativeUnits(true);
    UseCounter::Count(GetDocument(), WebFeature::kHasGlyphRelativeUnits);
  }

  if (state.StyleBuilder().HasLineHeightRelativeUnits()) {
    GetDocument().GetStyleEngine().SetUsesLineHeightUnits(true);
  }

  state.LoadPendingResources();

  // Now return the style.
  return state.TakeStyle();
}

void StyleResolver::InitStyle(Element& element,
                              const StyleRequest& style_request,
                              const ComputedStyle& source_for_noninherited,
                              const ComputedStyle* parent_style,
                              StyleResolverState& state) {
  if (state.UsesHighlightPseudoInheritance()) {
    // When resolving highlight styles for children, we need to default all
    // properties (whether or not defined as inherited) to parent values.

    // Sadly, ComputedStyle creation is unavoidable until ElementRuleCollector
    // and friends stop relying on ComputedStyle mutation. The good news is that
    // if the element has no rules for this highlight pseudo, we skip resolution
    // entirely (leaving the scoped_refptr untouched). The bad news is that if
    // the element has rules but no matched properties, we currently clone.
    state.SetStyle(*parent_style);

    // Highlight Pseudos do not support custom properties defined on the
    // pseudo itself. They may use var() references but those must be resolved
    // against the originating element. Share the variables from the originating
    // style.
    state.StyleBuilder().CopyInheritedVariablesFrom(
        state.OriginatingElementStyle());
    state.StyleBuilder().CopyNonInheritedVariablesFrom(
        state.OriginatingElementStyle());
  } else {
    state.CreateNewStyle(
        source_for_noninherited, *parent_style,
        (!style_request.IsPseudoStyleRequest() && IsAtShadowBoundary(&element))
            ? ComputedStyleBuilder::kAtShadowBoundary
            : ComputedStyleBuilder::kNotAtShadowBoundary);

    // contenteditable attribute (implemented by -webkit-user-modify) should
    // be propagated from shadow host to distributed node.
    if (!style_request.IsPseudoStyleRequest() && element.AssignedSlot()) {
      if (Element* parent = element.parentElement()) {
        if (!RuntimeEnabledFeatures::
                InheritUserModifyWithoutContenteditableEnabled() ||
            !element.FastHasAttribute(html_names::kContenteditableAttr)) {
          if (const ComputedStyle* shadow_host_style =
                  parent->GetComputedStyle()) {
            state.StyleBuilder().SetUserModify(shadow_host_style->UserModify());
          }
        }
      }
    }
  }
  state.StyleBuilder().SetStyleType(style_request.pseudo_id);
  state.StyleBuilder().SetPseudoArgument(style_request.pseudo_argument);

  // For highlight inheritance, propagate link visitedness, forced-colors
  // status, the font and the line height from the originating element. The
  // font and line height are necessary to correctly resolve font relative
  // units.
  if (state.UsesHighlightPseudoInheritance()) {
    state.StyleBuilder().SetInForcedColorsMode(
        style_request.originating_element_style->InForcedColorsMode());
    state.StyleBuilder().SetForcedColorAdjust(
        style_request.originating_element_style->ForcedColorAdjust());
    state.StyleBuilder().SetFont(
        style_request.originating_element_style->GetFont());
    state.StyleBuilder().SetLineHeight(
        style_request.originating_element_style->LineHeight());
    state.StyleBuilder().SetWritingMode(
        style_request.originating_element_style->GetWritingMode());
  }

  if (!style_request.IsPseudoStyleRequest() && element.IsLink()) {
    state.StyleBuilder().SetIsLink();
  }

  if (!style_request.IsPseudoStyleRequest()) {
    // Preserve the text autosizing multiplier on style recalc. Autosizer will
    // update it during layout if needed.
    // NOTE: This must occur before CascadeAndApplyMatchedProperties for correct
    // computation of font-relative lengths.
    // NOTE: This can never be overwritten by a MPC hit, since we don't use the
    // MPC if TextAutosizingMultiplier() is different from 1.
    state.StyleBuilder().SetTextAutosizingMultiplier(
        state.TextAutosizingMultiplier());
  }
}

void StyleResolver::ApplyMathMLCustomStyleProperties(
    Element* element,
    StyleResolverState& state) {
  DCHECK(IsA<MathMLElement>(element));
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

  // ComputedStyles produced by OOF-interleaving (StyleEngine::
  // UpdateStyleForOutOfFlow) have this flag set. We can not apply the style
  // incrementally on top of this, because ComputedStyles produced by normal
  // style recalcs should not have this flag.
  if (element->GetComputedStyle()->HasAnchorEvaluator()) {
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
      if (property.Value().IsUnparsedDeclaration() ||
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
// that does not depend on animations. For our purposes, style consists of three
// parts:
//
//  A. Properties inherited from the parent (parent style).
//  B. Properties that come from the defaults (initial style).
//  C. Properties from CSS rules that apply from this element
//     (matched properties).
//
// The typical flow (barring special rules for pseudo-elements and similar) is:
//
//   1. Collect all CSS rules that apply to this element
//      (MatchAllRules(), into ElementRuleCollector).
//   2. Figure out where we should get parent style (A) from, and where we
//      should get initial style (B) from; typically the parent element and
//      the global initial style, respectively.
//   3. Construct a new ComputedStyle, merging the two sources (InitStyle()).
//   4. Apply all the found properties (C) in the correct order
//      (ApplyPropertiesFromCascade(), using StyleCascade).
//
// However, the MatchedPropertiesCache can often give us A with the correct
// parts of C pre-applied, or similar for B+C, or simply A+B+C (a full MPC hit).
// Thus, after step 1, we look up the set of properties we've collected in the
// MPC, and if we have a full MPC hit, we stop after step 1. (This is the reason
// why step 1 needs to be first.) If we have a partial hit (we can use A+C
// but not B+C, or the other way around), we use that as one of our sources
// in step 3, and can skip the relevant properties in step 4.
//
// The base style is cached by the caller if possible (see ResolveStyle() on
// the “base computed style optimization”).
void StyleResolver::ApplyBaseStyleNoCache(
    Element* element,
    const StyleRecalcContext& style_recalc_context,
    const StyleRequest& style_request,
    StyleResolverState& state,
    StyleCascade& cascade) {
  // For some very special elements (e.g. <video>): Ensure internal UA style
  // rules that are relevant for the element exist in the stylesheet.
  GetDocument().GetStyleEngine().EnsureUAStyleForElement(*element);

  if (!style_request.IsPseudoStyleRequest()) {
    if (IsForcedColorsModeEnabled()) {
      cascade.MutableMatchResult().AddMatchedProperties(
          ForcedColorsUserAgentDeclarations(),
          {.origin = CascadeOrigin::kUserAgent});
    }

    // UA rule: * { overlay: none !important }
    // and
    // UA rule: ::scroll-marker-group { contain: size !important; }
    // Implemented here because DCHECKs ensures we don't add universal rules to
    // the UA sheets. Note that this is a universal rule in any namespace.
    // Adding this to the html.css would only do the override in the HTML
    // namespace since the sheet has a default namespace.
    cascade.MutableMatchResult().AddMatchedProperties(
        UniversalOverlayUserAgentDeclaration(),
        {.origin = CascadeOrigin::kUserAgent});

    // This adds a CSSInitialColorValue to the cascade for the document
    // element. The CSSInitialColorValue will resolve to a color-scheme
    // sensitive color in Color::ApplyValue. It is added at the start of the
    // MatchResult such that subsequent declarations (even from the UA sheet)
    // get a higher priority.
    //
    // TODO(crbug.com/1046753): Remove this when canvastext is supported.
    if (element == state.GetDocument().documentElement()) {
      cascade.MutableMatchResult().AddMatchedProperties(
          DocumentElementUserAgentDeclarations(),
          {.origin = CascadeOrigin::kUserAgent});
    }
  }

  ElementRuleCollector collector(state.ElementContext(), style_recalc_context,
                                 selector_filter_, cascade.MutableMatchResult(),
                                 state.InsideLink());

  if (style_request.IsPseudoStyleRequest()) {
    if (style_request.pseudo_id == kPseudoIdScrollMarkerGroup) {
      cascade.MutableMatchResult().AddMatchedProperties(
          ScrollMarkerGroupUserAgentDeclaration(),
          {.origin = CascadeOrigin::kUserAgent});
    }

    collector.SetPseudoElementStyleRequest(style_request);
    GetDocument().GetStyleEngine().EnsureUAStyleForPseudoElement(
        style_request.pseudo_id);
  }

  if (!state.ParentStyle()) {
    // We have no parent so use the initial style as the parent. Note that we
    // need to do this before MPC lookup, so that the parent comparison (to
    // determine if we have a hit on inherited properties) is correctly
    // determined.
    state.SetParentStyle(InitialStyleForElement());
    state.SetLayoutParentStyle(state.ParentStyle());

    if (!style_request.IsPseudoStyleRequest() &&
        *element != GetDocument().documentElement()) {
      // Strictly, we should only allow the root element to inherit from
      // initial styles, but we allow getComputedStyle() for connected
      // elements outside the flat tree rooted at an unassigned shadow host
      // child or a slot fallback element.
      DCHECK((IsShadowHost(element->parentNode()) ||
              IsA<HTMLSlotElement>(element->parentNode())) &&
             !LayoutTreeBuilderTraversal::ParentElement(*element));
    }
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

  const MatchResult& match_result = collector.MatchedResult();

  if (style_request.IsPseudoStyleRequest()) {
    if (!match_result.HasMatchedProperties()) {
      InitStyle(*element, style_request, *initial_style_, state.ParentStyle(),
                state);
      StyleAdjuster::AdjustComputedStyle(state, nullptr /* element */);
      state.SetHadNoMatchedProperties();
      return;
    }
  }

  const MatchResult& result = cascade.GetMatchResult();
  CacheSuccess cache_success = ApplyMatchedCache(state, style_request, result);

  if (style_recalc_context.is_ensuring_style &&
      style_recalc_context.is_outside_flat_tree) {
    state.StyleBuilder().SetIsEnsuredOutsideFlatTree();
  }

  if (!cache_success.IsFullCacheHit()) {
    ApplyPropertiesFromCascade(state, cascade, cache_success);
    MaybeAddToMatchedPropertiesCache(state, cache_success);
  }

  // TODO(crbug.com/1024156): do this for CustomHighlightNames too, so we
  // can remove the cache-busting for ::highlight() in IsStyleCacheable
  state.StyleBuilder().SetHasNonUniversalHighlightPseudoStyles(
      match_result.HasNonUniversalHighlightPseudoStyles());
  state.StyleBuilder().SetHasNonUaHighlightPseudoStyles(
      match_result.HasNonUaHighlightPseudoStyles());
  state.StyleBuilder().SetHighlightsDependOnSizeContainerQueries(
      match_result.HighlightsDependOnSizeContainerQueries());

  if (match_result.HasFlag(MatchFlag::kAffectedByDrag)) {
    state.StyleBuilder().SetAffectedByDrag();
  }
  if (match_result.HasFlag(MatchFlag::kAffectedByFocusWithin)) {
    state.StyleBuilder().SetAffectedByFocusWithin();
  }
  if (match_result.HasFlag(MatchFlag::kAffectedByHover)) {
    state.StyleBuilder().SetAffectedByHover();
  }
  if (match_result.HasFlag(MatchFlag::kAffectedByActive)) {
    state.StyleBuilder().SetAffectedByActive();
  }
  if (match_result.HasFlag(MatchFlag::kAffectedByStartingStyle)) {
    state.StyleBuilder().SetIsStartingStyle();
  }
  if (match_result.DependsOnSizeContainerQueries()) {
    state.StyleBuilder().SetDependsOnSizeContainerQueries(true);
  }
  if (match_result.DependsOnStyleContainerQueries()) {
    state.StyleBuilder().SetDependsOnStyleContainerQueries(true);
  }
  if (match_result.DependsOnStateContainerQueries()) {
    state.StyleBuilder().SetDependsOnStateContainerQueries(true);
  }
  if (match_result.FirstLineDependsOnSizeContainerQueries()) {
    state.StyleBuilder().SetFirstLineDependsOnSizeContainerQueries(true);
  }
  if (match_result.DependsOnStaticViewportUnits()) {
    state.StyleBuilder().SetHasStaticViewportUnits();
  }
  if (match_result.DependsOnDynamicViewportUnits()) {
    state.StyleBuilder().SetHasDynamicViewportUnits();
  }
  if (match_result.DependsOnRootFontContainerQueries()) {
    state.StyleBuilder().SetHasRootFontRelativeUnits();
  }
  if (match_result.ConditionallyAffectsAnimations()) {
    state.SetConditionallyAffectsAnimations();
  }
  if (!match_result.CustomHighlightNames().empty()) {
    state.StyleBuilder().SetCustomHighlightNames(
        match_result.CustomHighlightNames());
  }
  state.StyleBuilder().SetPseudoElementStyles(
      match_result.PseudoElementStyles());

  if (RuntimeEnabledFeatures::CSSAdvancedAttrFunctionEnabled() &&
      state.HasAttrFunction()) {
    state.StyleBuilder().SetHasAttrFunction();
  }

  // Now we're done with all operations that may overwrite InsideLink,
  // so we can set it once and for all.
  state.StyleBuilder().SetInsideLink(state.InsideLink());

  ApplyCallbackSelectors(state);
  if (element->IsLink() && (element->HasTagName(html_names::kATag) ||
                            element->HasTagName(html_names::kAreaTag))) {
    ApplyDocumentRulesSelectors(state, To<ContainerNode>(&element->TreeRoot()));
  }

  // Cache our if our original display is inline.
  state.StyleBuilder().SetIsOriginalDisplayInlineType(
      ComputedStyle::IsDisplayInlineType(state.StyleBuilder().Display()));

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

  if (state.CanTriggerAnimations() && CanReuseBaseComputedStyle(state)) {
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
    const ComputedStyle* style_snapshot = state.StyleBuilder().CloneStyle();
    DCHECK_EQ(g_null_atom, ComputeBaseComputedStyleDiff(
                               animation_base_computed_style, *style_snapshot));
#endif

    state.SetStyle(*animation_base_computed_style);
    state.StyleBuilder().SetBaseData(GetBaseData(state));
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

  if (style_recalc_context.can_use_incremental_style &&
      CanApplyInlineStyleIncrementally(element, state, style_request)) {
    // We are in a situation where we can reuse the old style
    // and just apply the element's inline style on top of it
    // (see the function comment).
    state.SetStyle(*element->GetComputedStyle());

    // This is always false when creating a new style, but is not reset
    // when copying the style, so it needs to happen here. After us,
    // Element::StyleForLayoutObject() will call AdjustElementStyle(),
    // which sets it to true if applicable.
    state.StyleBuilder().ResetSkipsContents();

    const CSSPropertyValueSet* inline_style = element->InlineStyle();
    if (inline_style) {
      int num_properties = inline_style->PropertyCount();
      for (int property_idx = 0; property_idx < num_properties;
           ++property_idx) {
        CSSPropertyValueSet::PropertyReference property =
            inline_style->PropertyAt(property_idx);
        StyleBuilder::ApplyProperty(
            property.Name(), state,
            property.Value().EnsureScopedValue(&GetDocument()));
      }
    }

    // Sets flags related to length unit conversions which may have taken
    // place during StyleBuilder::ApplyProperty.
    ApplyLengthConversionFlags(state);

    StyleAdjuster::AdjustComputedStyle(
        state, style_request.IsPseudoStyleRequest() ? nullptr : element);

    // Normally done by StyleResolver::MaybeAddToMatchedPropertiesCache(),
    // when applying the cascade. Note that this is probably redundant
    // (we'll be loading pending resources later), but not doing so would
    // currently create diffs below.
    state.LoadPendingResources();

#if DCHECK_IS_ON()
    // Verify that we got the right answer.
    const ComputedStyle* incremental_style = state.TakeStyle();
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
    state.StyleBuilder().SetViewportUnitFlags(
        state.StyleBuilder().ViewportUnitFlags() |
        incremental_style->ViewportUnitFlags());

    const ComputedStyle* style_snapshot = state.StyleBuilder().CloneStyle();
    DCHECK_EQ(g_null_atom,
              ComputeBaseComputedStyleDiff(incremental_style, *style_snapshot));
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
  state.SetStyle(base_style);
  if (value) {
    STACK_UNINITIALIZED StyleCascade cascade(state);
    auto* set =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(state.GetParserMode());
    set->SetProperty(property.GetCSSPropertyName(), *value);
    cascade.MutableMatchResult().BeginAddingAuthorRulesForTreeScope(
        element.GetTreeScope());
    cascade.MutableMatchResult().AddMatchedProperties(
        set, {.origin = CascadeOrigin::kAuthor});
    cascade.Apply();
  }
  const ComputedStyle* style = state.TakeStyle();
  return CompositorKeyframeValueFactory::Create(property, *style, offset);
}

const ComputedStyle* StyleResolver::StyleForPage(uint32_t page_index,
                                                 const AtomicString& page_name,
                                                 float page_fitting_scale,
                                                 bool ignore_author_style) {
  // The page context inherits from the root element.
  Element* root_element = GetDocument().documentElement();
  if (!root_element) {
    return InitialStyleForElement();
  }
  DCHECK(!GetDocument().NeedsLayoutTreeUpdateForNode(*root_element));
  const ComputedStyle* parent_style = root_element->EnsureComputedStyle();
  StyleResolverState state(GetDocument(), *root_element,
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(parent_style));
  state.CreateNewStyle(*InitialStyleForElement(), *parent_style);

  if (parent_style->Display() == EDisplay::kNone) {
    // The root is display:none. One page box will still be created, but no
    // properties should apply.
    return InitialStyleForElement();
  }

  auto& builder = state.StyleBuilder();
  // Page boxes are blocks.
  builder.SetDisplay(EDisplay::kBlock);

  STACK_UNINITIALIZED StyleCascade cascade(state);

  PageRuleCollector collector(parent_style, CSSAtRuleID::kCSSAtRulePage,
                              page_index, page_name,
                              cascade.MutableMatchResult());

  collector.MatchPageRules(
      CSSDefaultStyleSheets::Instance().DefaultPrintStyle(),
      CascadeOrigin::kUserAgent, nullptr /* tree_scope */,
      nullptr /* layer_map */);

  // Calling this function without being in print mode is unusual and special,
  // but it happens from unit tests, if nothing else.
  if (GetDocument().Printing()) {
    auto* value = CSSNumericLiteralValue::Create(
        page_fitting_scale, CSSPrimitiveValue::UnitType::kNumber);
    StyleBuilder::ApplyProperty(GetCSSPropertyZoom(), state, *value);

    const WebPrintParams& params = GetDocument().GetFrame()->GetPrintParams();
    const WebPrintPageDescription& description =
        params.default_page_description;
    // Set margins from print settings. They may be overridden by author styles,
    // unless params.ignore_css_margins is set.
    auto* set =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
    value = CSSNumericLiteralValue::Create(
        description.margin_top, CSSPrimitiveValue::UnitType::kPixels);
    set->SetProperty(CSSPropertyID::kMarginTop, *value,
                     /*important=*/params.ignore_css_margins);
    value = CSSNumericLiteralValue::Create(
        description.margin_right, CSSPrimitiveValue::UnitType::kPixels);
    set->SetProperty(CSSPropertyID::kMarginRight, *value,
                     /*important=*/params.ignore_css_margins);
    value = CSSNumericLiteralValue::Create(
        description.margin_bottom, CSSPrimitiveValue::UnitType::kPixels);
    set->SetProperty(CSSPropertyID::kMarginBottom, *value,
                     /*important=*/params.ignore_css_margins);
    value = CSSNumericLiteralValue::Create(
        description.margin_left, CSSPrimitiveValue::UnitType::kPixels);
    set->SetProperty(CSSPropertyID::kMarginLeft, *value,
                     /*important=*/params.ignore_css_margins);
    cascade.MutableMatchResult().AddMatchedProperties(
        set, {.origin = CascadeOrigin::kUserAgent});
  }

  if (!ignore_author_style) {
    if (ScopedStyleResolver* scoped_resolver =
            GetDocument().GetScopedStyleResolver()) {
      scoped_resolver->MatchPageRules(collector);
    }
  }

  cascade.Apply();

  state.LoadPendingResources();

  // Now return the style.
  return state.TakeStyle();
}

void StyleResolver::StyleForPageMargins(const ComputedStyle& page_style,
                                        uint32_t page_index,
                                        const AtomicString& page_name,
                                        PageMarginsStyle* margins_style) {
  Element* root_element = GetDocument().documentElement();
  if (!root_element) {
    return;
  }

  struct Entry {
    PageMarginsStyle::MarginSlot slot;
    CSSAtRuleID at_rule_id;
  };
  const Entry table[] = {
      {PageMarginsStyle::TopLeft, CSSAtRuleID::kCSSAtRuleTopLeft},
      {PageMarginsStyle::TopCenter, CSSAtRuleID::kCSSAtRuleTopCenter},
      {PageMarginsStyle::TopRight, CSSAtRuleID::kCSSAtRuleTopRight},
      {PageMarginsStyle::RightTop, CSSAtRuleID::kCSSAtRuleRightTop},
      {PageMarginsStyle::RightMiddle, CSSAtRuleID::kCSSAtRuleRightMiddle},
      {PageMarginsStyle::RightBottom, CSSAtRuleID::kCSSAtRuleRightBottom},
      {PageMarginsStyle::BottomLeft, CSSAtRuleID::kCSSAtRuleBottomLeft},
      {PageMarginsStyle::BottomCenter, CSSAtRuleID::kCSSAtRuleBottomCenter},
      {PageMarginsStyle::BottomRight, CSSAtRuleID::kCSSAtRuleBottomRight},
      {PageMarginsStyle::LeftTop, CSSAtRuleID::kCSSAtRuleLeftTop},
      {PageMarginsStyle::LeftMiddle, CSSAtRuleID::kCSSAtRuleLeftMiddle},
      {PageMarginsStyle::LeftBottom, CSSAtRuleID::kCSSAtRuleLeftBottom},
      {PageMarginsStyle::TopLeftCorner, CSSAtRuleID::kCSSAtRuleTopLeftCorner},
      {PageMarginsStyle::TopRightCorner, CSSAtRuleID::kCSSAtRuleTopRightCorner},
      {PageMarginsStyle::BottomRightCorner,
       CSSAtRuleID::kCSSAtRuleBottomRightCorner},
      {PageMarginsStyle::BottomLeftCorner,
       CSSAtRuleID::kCSSAtRuleBottomLeftCorner}};

  for (const Entry& entry : table) {
    StyleResolverState margin_state(GetDocument(), *root_element,
                                    /*StyleRecalcContext=*/nullptr,
                                    StyleRequest(&page_style));
    margin_state.CreateNewStyle(*InitialStyleForElement(), page_style);
    margin_state.StyleBuilder().SetDisplay(EDisplay::kBlock);
    margin_state.StyleBuilder().SetIsPageMarginBox(true);

    STACK_UNINITIALIZED StyleCascade margin_cascade(margin_state);
    PageRuleCollector margin_rule_collector(
        &page_style, entry.at_rule_id, page_index, page_name,
        margin_cascade.MutableMatchResult());
    margin_rule_collector.MatchPageRules(
        CSSDefaultStyleSheets::Instance().DefaultPrintStyle(),
        CascadeOrigin::kUserAgent, /*tree_scope=*/nullptr,
        /*layer_map=*/nullptr);

    if (ScopedStyleResolver* scoped_resolver =
            GetDocument().GetScopedStyleResolver()) {
      scoped_resolver->MatchPageRules(margin_rule_collector);
    }

    margin_cascade.Apply();

    margin_state.LoadPendingResources();

    (*margins_style)[entry.slot] = margin_state.TakeStyle();
  }
}

void StyleResolver::LoadPaginationResources() {
  // Compute style for pages and page margins (LoadPendingResources()), to
  // initiate loading of resources only needed by printing.
  //
  // TODO(crbug.com/346799729): Make sure that all resources needed are
  // loaded. As it is now, only resources needed on the first page (with no page
  // name) will be loaded. Any resource inside a non-empty @page selector
  // (unless it happens to match the first page) will be missing.
  const ComputedStyle* page_style = StyleForPage(0, /*page_name=*/g_null_atom);
  PageMarginsStyle ignored;
  StyleForPageMargins(*page_style, 0, /*page_name=*/g_null_atom, &ignored);
}

const ComputedStyle& StyleResolver::InitialStyle() const {
  DCHECK(initial_style_);
  return *initial_style_;
}

ComputedStyleBuilder StyleResolver::CreateComputedStyleBuilder() const {
  DCHECK(initial_style_);
  return ComputedStyleBuilder(*initial_style_);
}

ComputedStyleBuilder StyleResolver::CreateComputedStyleBuilderInheritingFrom(
    const ComputedStyle& parent_style) const {
  DCHECK(initial_style_);
  return ComputedStyleBuilder(*initial_style_, parent_style);
}

float StyleResolver::InitialZoom() const {
  const Document& document = GetDocument();
  if (const LocalFrame* frame = document.GetFrame()) {
    return !document.Printing() ? frame->LayoutZoomFactor() : 1;
  }
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

  if (StyleInitialData* initial_data = engine.MaybeCreateAndGetInitialData()) {
    builder.SetInitialData(initial_data);
  }

  if (RuntimeEnabledFeatures::PreferDefaultScrollbarStylesEnabled()) {
    Settings* settings = GetDocument().GetSettings();
    if (settings && settings->GetPrefersDefaultScrollbarStyles()) {
      builder.SetPrefersDefaultScrollbarStyles(true);
    }
  }

  return builder;
}

const ComputedStyle* StyleResolver::InitialStyleForElement() const {
  return InitialStyleBuilderForElement().TakeStyle();
}

const ComputedStyle* StyleResolver::StyleForText(Text* text_node) {
  DCHECK(text_node);
  if (Element* parent = LayoutTreeBuilderTraversal::ParentElement(*text_node)) {
    const ComputedStyle* style = parent->GetComputedStyle();
    if (style && !style->IsEnsuredInDisplayNone()) {
      return style;
    }
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
  collector.SetSuppressVisited(true);
  CollectPseudoRulesForElement(*element, collector, kPseudoIdNone, g_null_atom,
                               rules_to_include);
  return collector.MatchedStyleRuleList();
}

HeapHashMap<CSSPropertyName, Member<const CSSValue>>
StyleResolver::CascadedValuesForElement(Element* element, PseudoId pseudo_id) {
  StyleResolverState state(GetDocument(), *element);
  state.SetStyle(InitialStyle());

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
    const ContainerSelector& container_selector,
    const TreeScope* selector_tree_scope) {
  DCHECK(element);
  return ContainerQueryEvaluator::FindContainer(
      ContainerQueryEvaluator::ParentContainerCandidateElement(*element),
      container_selector, selector_tree_scope);
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

  if (tracker_) {
    AddMatchedRulesToTracker(collector);
  }
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
  StyleRequest style_request{pseudo_id,
                             /* parent_style */ nullptr,
                             /* originating_element_style */ nullptr,
                             view_transition_name};
  if (pseudo_id == kPseudoIdSearchText) {
    // TODO(crbug.com/339298411): handle :current?
    style_request.search_text_request = StyleRequest::kNotCurrent;
  }
  collector.SetPseudoElementStyleRequest(style_request);

  if (rules_to_include & kUACSSRules) {
    MatchUARules(element, collector);
  }

  if (rules_to_include & kUserCSSRules) {
    MatchUserRules(collector);
  }

  if (rules_to_include & kAuthorCSSRules) {
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

  if (!animating_element) {
    return false;
  }

  if (HasTimelines(state)) {
    CSSAnimations::CalculateTimelineUpdate(
        state.AnimationUpdate(), *animating_element, state.StyleBuilder());
  }

  if (!HasAnimationsOrTransitions(state)) {
    return false;
  }

  // TODO(crbug.com/1276575) : This assert is currently hit for nested ::marker
  // pseudo elements.
  DCHECK(
      animating_element == &element ||
      (animating_element->IsSVGElement() &&
       To<SVGElement>(animating_element)->CorrespondingElement() == &element) ||
      DynamicTo<PseudoElement>(animating_element)->OriginatingElement() ==
          &element);

  if (!IsAnimationStyleChange(*animating_element) ||
      !state.StyleBuilder().BaseData()) {
    state.StyleBuilder().SetBaseData(StyleBaseData::Create(
        state.StyleBuilder().CloneStyle(), cascade.GetImportantSet()));
  }

  CSSAnimations::CalculateAnimationUpdate(
      state.AnimationUpdate(), *animating_element, state.GetElement(),
      state.StyleBuilder(), state.ParentStyle(), this,
      state.CanTriggerAnimations());
  CSSAnimations::CalculateTransitionUpdate(
      state.AnimationUpdate(), *animating_element, state.StyleBuilder(),
      state.OldStyle(), state.CanTriggerAnimations());

  bool apply = !state.AnimationUpdate().IsEmpty();
  if (apply) {
    const ActiveInterpolationsMap& animations =
        state.AnimationUpdate().ActiveInterpolationsForAnimations();
    const ActiveInterpolationsMap& transitions =
        state.AnimationUpdate().ActiveInterpolationsForTransitions();

    cascade.AddInterpolations(&animations, CascadeOrigin::kAnimation);
    cascade.AddInterpolations(&transitions, CascadeOrigin::kTransition);

    // Note: this applies the same filter to pseudo elements as its originating
    // element since state.GetElement() returns the originating element when
    // resolving style for pseudo elements.
    CascadeFilter filter = state.GetElement().GetCascadeFilter();
    if (state.StyleBuilder().StyleType() == kPseudoIdMarker) {
      filter = filter.Add(CSSProperty::kValidForMarker, false);
    }
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

    // Apply any length conversion flags produced by CSS/Web animations (e.g.
    // animations involving viewport units would set such flags).
    ApplyLengthConversionFlags(state);

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

void StyleResolver::ApplyAnchorData(StyleResolverState& state) {
  if (AnchorEvaluator* evaluator =
          state.CssToLengthConversionData().GetAnchorEvaluator()) {
    // Pre-compute anchor-center offset so that the OOF layout code does not
    // need to set up an AnchorEvaluator but simply retrieve the offsets from
    // the ComputedStyle.
    if (std::optional<PhysicalOffset> offset =
            evaluator->ComputeAnchorCenterOffsets(state.StyleBuilder());
        offset.has_value()) {
      state.StyleBuilder().SetAnchorCenterOffset(offset);
    }

    // See ComputedStyle::HasAnchorFunctionsWithoutEvaluator.
    state.StyleBuilder().SetHasAnchorEvaluator();
  }
}

StyleResolver::FindKeyframesRuleResult StyleResolver::FindKeyframesRule(
    const Element* element,
    const Element* animating_element,
    const AtomicString& animation_name) {
  HeapVector<Member<ScopedStyleResolver>, 8> resolvers;
  CollectScopedResolversForHostedShadowTrees(*element, resolvers);
  if (ScopedStyleResolver* scoped_resolver =
          element->GetTreeScope().GetScopedStyleResolver()) {
    resolvers.push_back(scoped_resolver);
  }

  for (auto& resolver : resolvers) {
    if (StyleRuleKeyframes* keyframes_rule =
            resolver->KeyframeStylesForAnimation(animation_name)) {
      return FindKeyframesRuleResult{keyframes_rule, &resolver->GetTreeScope()};
    }
  }

  if (StyleRuleKeyframes* keyframes_rule =
          GetDocument().GetStyleEngine().KeyframeStylesForAnimation(
              animation_name)) {
    return FindKeyframesRuleResult{keyframes_rule, nullptr};
  }

  // Match UA keyframe rules after user and author rules.
  StyleRuleKeyframes* matched_keyframes_rule = nullptr;
  auto func = [&matched_keyframes_rule, &animation_name](RuleSet* rules) {
    auto keyframes_rules = rules->KeyframesRules();
    for (auto& keyframes_rule : keyframes_rules) {
      if (keyframes_rule->GetName() == animation_name) {
        matched_keyframes_rule = keyframes_rule;
      }
    }
  };
  ForEachUARulesForElement(*animating_element, nullptr, func);
  if (matched_keyframes_rule) {
    return FindKeyframesRuleResult{matched_keyframes_rule, nullptr};
  }

  for (auto& resolver : resolvers) {
    resolver->SetHasUnresolvedKeyframesRule();
  }
  return FindKeyframesRuleResult();
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
  if (!cached_matched_properties) {
    return false;
  }
  return cached_matched_properties->computed_style->EffectiveZoom() !=
         builder.EffectiveZoom();
}

bool StyleResolver::CacheSuccess::FontChanged(
    const ComputedStyleBuilder& builder) const {
  if (!cached_matched_properties) {
    return false;
  }
  return cached_matched_properties->computed_style->GetFontDescription() !=
         builder.GetFontDescription();
}

bool StyleResolver::CacheSuccess::InheritedVariablesChanged(
    const ComputedStyleBuilder& builder) const {
  if (!cached_matched_properties) {
    return false;
  }
  return !base::ValuesEquivalent(
      cached_matched_properties->computed_style->InheritedVariables(),
      builder.InheritedVariables());
}

bool StyleResolver::CacheSuccess::LineHeightChanged(
    const ComputedStyleBuilder& builder) const {
  if (!cached_matched_properties) {
    return false;
  }
  return cached_matched_properties->computed_style->LineHeight() !=
         builder.LineHeight();
}

bool StyleResolver::CacheSuccess::IsUsableAfterApplyInheritedOnly(
    const ComputedStyleBuilder& builder) const {
  return !EffectiveZoomChanged(builder) && !FontChanged(builder) &&
         !InheritedVariablesChanged(builder) && !LineHeightChanged(builder);
}

StyleResolver::CacheSuccess StyleResolver::ApplyMatchedCache(
    StyleResolverState& state,
    const StyleRequest& style_request,
    const MatchResult& match_result) {
  Element& element = state.GetElement();

  MatchedPropertiesCache::Key key(match_result);

  bool can_use_cache = key.IsValid();
  // NOTE: Do not add anything here without also adding it to
  // MatchedPropertiesCache::IsCacheable(); you would be inserting
  // elements that can never be fetched.
  if (state.UsesHighlightPseudoInheritance()) {
    // Some pseudo-elements, like ::highlight, are special in that
    // they inherit _non-inherited_ properties from their parent.
    // This is different from what the MPC expects; it checks that
    // the parents are the same before declaring that we have a
    // valid hit (the check for InheritedDataShared() below),
    // but it does not do so for non-inherited properties; it assumes
    // that the base for non-inherited style (before applying the
    // matched properties) is always the initial style.
    // Thus, for simplicity, we simply disable the MPC in these cases.
    //
    // TODO(sesse): Why don't we have this problem when we use
    // a different initial style for <img>?
    can_use_cache = false;
  }
  if (!state.GetElement().GetCascadeFilter().IsEmpty()) {
    // The result of applying properties with the same matching declarations can
    // be different if the cascade filter is different.
    can_use_cache = false;
  }

  bool is_inherited_cache_hit = false;
  bool is_non_inherited_cache_hit = false;
  const CachedMatchedProperties* cached_matched_properties =
      can_use_cache ? matched_properties_cache_.Find(key, state) : nullptr;
  // We use a different initial_style for <img> elements to match the overrides
  // in html.css. This avoids allocation overhead from copy-on-write when
  // these properties are set only via UA styles. The overhead shows up on
  // MotionMark, which stress-tests this code. See crbug.com/1369454 for
  // details.
  const ComputedStyle& initial_style = IsA<HTMLImageElement>(element)
                                           ? *initial_style_for_img_
                                           : *initial_style_;

  if (cached_matched_properties) {
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  matched_property_cache_hit, 1);

    is_inherited_cache_hit =
        state.ParentStyle()->InheritedDataShared(
            *cached_matched_properties->parent_computed_style) &&
        !IsAtShadowBoundary(&element);
    is_non_inherited_cache_hit =
        !IsForcedColorsModeEnabled() || is_inherited_cache_hit;

    const ComputedStyle* parent_style =
        is_inherited_cache_hit ? cached_matched_properties->computed_style.Get()
                               : state.ParentStyle();
    const ComputedStyle& source_for_noninherited =
        is_non_inherited_cache_hit
            ? *cached_matched_properties->computed_style.Get()
            : initial_style;

    InitStyle(element, style_request, source_for_noninherited, parent_style,
              state);

    if (cached_matched_properties->computed_style->CanAffectAnimations()) {
      // Need to set this flag from the cached ComputedStyle to make
      // ShouldStoreOldStyle() correctly return true. We do not collect matching
      // rules when the cache is hit, and the flag is set as part of that
      // process for the full style resolution.
      state.StyleBuilder().SetCanAffectAnimations();
    }

    // We can build up the style by copying non-inherited properties from an
    // earlier style object built using the same exact style declarations. We
    // then only need to apply the inherited properties, if any, as their values
    // can depend on the element context. This is fast and saves memory by
    // reusing the style data structures. Note that we cannot do this if the
    // direct parent is a ShadowRoot.
    if (is_inherited_cache_hit) {
      INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                    matched_property_cache_inherited_hit, 1);

      // If the cache item parent style has identical inherited properties to
      // the current parent style then the resulting style will be identical
      // too. We copied the inherited properties over from the cache, so we
      // are done.
    }
    if (is_non_inherited_cache_hit) {
      // If the child style is a cache hit, we'll never reach StyleBuilder::
      // ApplyProperty, hence we'll never set the flag on the parent.
      // (We do the same thing for independently inherited properties in
      // Element::RecalcOwnStyle().)
      if (state.StyleBuilder().HasExplicitInheritance()) {
        state.ParentStyle()->SetChildHasExplicitInheritance();
      }
    }
    state.UpdateFont();
  } else {
    // Initialize a new, plain ComputedStyle with only initial
    // style and inheritance accounted for. We'll return a cache
    // miss, which will cause the caller to apply all the matched
    // properties on top of it.
    InitStyle(element, style_request, initial_style, state.ParentStyle(),
              state);
  }

  // This is needed because pseudo_argument is copied to the
  // state.StyleBuilder() as part of a raredata field when copying
  // non-inherited values from the cached result. The argument isn't a style
  // property per se, it represents the argument to the matching element which
  // should remain unchanged.
  state.StyleBuilder().SetPseudoArgument(style_request.pseudo_argument);

  return CacheSuccess(is_inherited_cache_hit, is_non_inherited_cache_hit, key,
                      cached_matched_properties);
}

void StyleResolver::MaybeAddToMatchedPropertiesCache(
    StyleResolverState& state,
    const CacheSuccess& cache_success) {
  state.LoadPendingResources();

  // NOTE: We replace everything that isn't a full cache hit. There are cases
  // where this would be bad (e.g., every other element we style with the same
  // key has a different parent computed style), but it seems a much more common
  // case, if we don't replace elements giving partial hits, is that a
  // bad entry gets stuck into the MPC and we _never_ get full hits again
  // from there because it's never replaced. (Or, similarly, a partial
  // hit where we have to reapply the inherited properties, or where we trash
  // the “partner cache” in StyleInheritedVariables.)
  if (cache_success.key.IsValid() &&
      MatchedPropertiesCache::IsCacheable(state)) {
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  matched_property_cache_added, 1);
    matched_properties_cache_.Add(cache_success.key,
                                  state.StyleBuilder().CloneStyle(),
                                  state.ParentStyle());
  }
}

bool StyleResolver::CanReuseBaseComputedStyle(const StyleResolverState& state) {
  ElementAnimations* element_animations = GetElementAnimations(state);
  if (!element_animations || !element_animations->IsAnimationStyleChange()) {
    return false;
  }

  StyleBaseData* base_data = GetBaseData(state);
  const ComputedStyle* base_style =
      base_data ? base_data->GetBaseComputedStyle() : nullptr;
  if (!base_style) {
    return false;
  }

  // Animating a custom property can have side effects on other properties
  // via variable references. Disallow base computed style optimization in such
  // cases.
  if (CSSAnimations::IsAnimatingCustomProperties(element_animations)) {
    return false;
  }

  // We need to build the cascade to know what to revert to.
  if (CSSAnimations::IsAnimatingRevert(element_animations)) {
    return false;
  }

  // When applying an animation or transition for a font affecting property,
  // font-relative units (e.g. em, ex) in the base style must respond to the
  // animation. We cannot use the base computed style optimization in such
  // cases.
  if (CSSAnimations::IsAnimatingFontAffectingProperties(element_animations)) {
    if (base_style->HasFontRelativeUnits()) {
      return false;
    }
  }

  // Likewise, When applying an animation or transition for line-height, lh unit
  // lengths in the base style must respond to the animation.
  if (CSSAnimations::IsAnimatingLineHeightProperty(element_animations)) {
    if (base_style->HasLineHeightRelativeUnits()) {
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
          element_animations, base_data->GetBaseImportantSet(),
          KeyframeEffect::kDefaultPriority)) {
    return false;
  }

  if (TextAutosizingMultiplierChanged(state, *base_style)) {
    return false;
  }

  // TODO(crbug.com/40943044): If we need to disable the optimization for
  // elements with position-fallback/anchor(), we probably need to disable
  // for descendants of such elements as well.
  if (base_style->GetPositionTryFallbacks() != nullptr) {
    return false;
  }

  if (base_style->HasAnchorFunctions() || base_style->HasAnchorEvaluator()) {
    // TODO(crbug.com/41483417): Enable this optimization for styles with
    // anchor queries.
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
  state.SetStyle(*base_style);
  auto* set =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(state.GetParserMode());
  set->SetProperty(property_name, value);
  cascade.MutableMatchResult().BeginAddingAuthorRulesForTreeScope(
      element->GetTreeScope());
  cascade.MutableMatchResult().AddMatchedProperties(
      set, {.origin = CascadeOrigin::kAuthor});
  cascade.Apply();

  CSSPropertyRef property_ref(property_name, element->GetDocument());
  const ComputedStyle* style = state.TakeStyle();
  return ComputedStyleUtils::ComputedPropertyValue(property_ref.GetProperty(),
                                                   *style);
}

const CSSValue* StyleResolver::ResolveValue(
    Element& element,
    const ComputedStyle& style,
    const CSSPropertyName& property_name,
    const CSSValue& value) {
  StyleResolverState state(element.GetDocument(), element);
  state.SetStyle(style);
  return StyleCascade::Resolve(state, property_name, value);
}

FilterOperations StyleResolver::ComputeFilterOperations(
    Element* element,
    const Font& font,
    const CSSValue& filter_value) {
  ComputedStyleBuilder parent_builder = CreateComputedStyleBuilder();
  parent_builder.SetFont(font);
  const ComputedStyle* parent = parent_builder.TakeStyle();

  StyleResolverState state(GetDocument(), *element,
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(parent));

  GetDocument().GetStyleEngine().UpdateViewportSize();
  state.SetStyle(*parent);

  StyleBuilder::ApplyProperty(GetCSSPropertyFilter(), state,
                              filter_value.EnsureScopedValue(&GetDocument()));

  state.LoadPendingResources();

  const ComputedStyle* style = state.TakeStyle();
  return style->Filter();
}

const ComputedStyle* StyleResolver::StyleForInterpolations(
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

const ComputedStyle* StyleResolver::BeforeChangeStyleForTransitionUpdate(
    Element& element,
    const ComputedStyle& base_style,
    ActiveInterpolationsMap& transition_interpolations) {
  StyleResolverState state(GetDocument(), element);
  STACK_UNINITIALIZED StyleCascade cascade(state);
  state.SetStyle(base_style);

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

void StyleResolver::ApplyPropertiesFromCascade(StyleResolverState& state,
                                               StyleCascade& cascade,
                                               CacheSuccess cache_success) {
  auto apply = [&state, &cascade, &cache_success](CascadeFilter filter) {
    if (cache_success.ShouldApplyInheritedOnly()) {
      cascade.Apply(filter.Add(CSSProperty::kInherited, false));
      if (!cache_success.IsUsableAfterApplyInheritedOnly(
              state.StyleBuilder())) {
        cascade.Apply(filter.Add(CSSProperty::kInherited, true));
      }
#if DCHECK_IS_ON()
      // Verify that our application went as planned.
      const ComputedStyle* applied_style = state.StyleBuilder().CloneStyle();
      cascade.Apply(filter);
      const ComputedStyle* correct_style = state.StyleBuilder().CloneStyle();
      DCHECK_EQ(g_null_atom,
                ComputeBaseComputedStyleDiff(applied_style, *correct_style));
#endif
    } else {
      cascade.Apply(filter);
    }
  };

  const ComputedStyle* old_style = nullptr;
  if (count_computed_style_bytes_) {
    old_style = state.StyleBuilder().CloneStyle();
  }

  // Note: this applies the same filter to pseudo elements as its originating
  // element since state.GetElement() returns the originating element when
  // resolving style for pseudo elements.
  CascadeFilter filter = state.GetElement().GetCascadeFilter();

  // In order to use-count whether or not legacy overlapping properties
  // made a real difference to the ComputedStyle, we first apply the cascade
  // while filtering out such properties. If the filter did reject
  // any legacy overlapping properties, we apply all overlapping properties
  // again to get the correct result.
  apply(filter.Add(CSSProperty::kLegacyOverlapping, true));

  if (state.RejectedLegacyOverlapping()) {
    const ComputedStyle* non_legacy_style = state.StyleBuilder().CloneStyle();
    // Re-apply all overlapping properties (both legacy and non-legacy).
    apply(filter.Add(CSSProperty::kOverlapping, false));
    UseCountLegacyOverlapping(GetDocument(), *non_legacy_style,
                              state.StyleBuilder());
  }

  if (count_computed_style_bytes_) {
    constexpr size_t kOilpanOverheadBytes =
        sizeof(void*);  // See cppgc::internal::HeapObjectHeader.
    const ComputedStyle* new_style = state.StyleBuilder().CloneStyle();
    for (const auto& [group_name, size] :
         old_style->FindChangedGroups(*new_style)) {
      computed_style_bytes_used_ += size + kOilpanOverheadBytes;
    }
    computed_style_bytes_used_ += sizeof(*new_style) + kOilpanOverheadBytes;
  }

  // NOTE: This flag (and the length conversion flags) need to be set before the
  // entry is added to the matched properties cache, or it will be wrong on
  // cache hits.
  state.StyleBuilder().SetInlineStyleLostCascade(cascade.InlineStyleLost());
  ApplyLengthConversionFlags(state);

  DCHECK(!state.GetFontBuilder().FontDirty());
}

void StyleResolver::ApplyCallbackSelectors(StyleResolverState& state) {
  StyleRuleList* rules = CollectMatchingRulesFromUnconnectedRuleSet(
      state, GetDocument().GetStyleEngine().WatchedSelectorsRuleSet(),
      /*scope=*/nullptr);
  if (!rules) {
    return;
  }
  for (auto rule : *rules) {
    state.StyleBuilder().AddCallbackSelector(rule->SelectorsText());
  }
}

void StyleResolver::ApplyDocumentRulesSelectors(StyleResolverState& state,
                                                ContainerNode* scope) {
  StyleRuleList* rules = CollectMatchingRulesFromUnconnectedRuleSet(
      state, GetDocument().GetStyleEngine().DocumentRulesSelectorsRuleSet(),
      scope);
  if (!rules) {
    return;
  }
  for (auto rule : *rules) {
    state.StyleBuilder().AddDocumentRulesSelector(rule);
  }
}

StyleRuleList* StyleResolver::CollectMatchingRulesFromUnconnectedRuleSet(
    StyleResolverState& state,
    RuleSet* rule_set,
    ContainerNode* scope) {
  if (!rule_set) {
    return nullptr;
  }

  MatchResult match_result;
  ElementRuleCollector collector(state.ElementContext(), StyleRecalcContext(),
                                 selector_filter_, match_result,
                                 state.InsideLink());
  collector.SetMatchingRulesFromNoStyleSheet(true);
  collector.SetMode(SelectorChecker::kCollectingStyleRules);
  MatchRequest match_request(rule_set, scope);
  collector.CollectMatchingRules(match_request, /*part_names*/ nullptr);
  collector.SortAndTransferMatchedRules(
      CascadeOrigin::kAuthor, /*is_vtt_embedded_style=*/false, tracker_);
  collector.SetMatchingRulesFromNoStyleSheet(false);

  return collector.MatchedStyleRuleList();
}

// Font properties are also handled by FontStyleResolver outside the main
// thread. If you add/remove properties here, make sure they are also properly
// handled by FontStyleResolver.
Font StyleResolver::ComputeFont(Element& element,
                                const ComputedStyle& style,
                                const CSSPropertyValueSet& property_set) {
  static const CSSProperty* properties[6] = {
      &GetCSSPropertyFontSize(),        &GetCSSPropertyFontFamily(),
      &GetCSSPropertyFontStretch(),     &GetCSSPropertyFontStyle(),
      &GetCSSPropertyFontVariantCaps(), &GetCSSPropertyFontWeight(),
  };

  // TODO(timloh): This is weird, the style is being used as its own parent
  StyleResolverState state(GetDocument(), element,
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(&style));
  GetDocument().GetStyleEngine().UpdateViewportSize();
  state.SetStyle(style);
  if (const ComputedStyle* parent_style = element.GetComputedStyle()) {
    state.SetParentStyle(parent_style);
  }

  for (const CSSProperty* property : properties) {
    // TODO(futhark): If we start supporting fonts on ShadowRoot.fonts in
    // addition to Document.fonts, we need to pass the correct TreeScope instead
    // of GetDocument() in the EnsureScopedValue below.
    StyleBuilder::ApplyProperty(
        *property, state,
        property_set.GetPropertyCSSValue(property->PropertyID())
            ->EnsureScopedValue(&GetDocument()));
  }
  state.UpdateFont();
  const ComputedStyle* font_style = state.TakeStyle();
  return font_style->GetFont();
}

void StyleResolver::UpdateMediaType() {
  if (LocalFrameView* view = GetDocument().View()) {
    bool was_print = print_media_type_;
    print_media_type_ =
        EqualIgnoringASCIICase(view->MediaType(), media_type_names::kPrint);
    if (was_print != print_media_type_) {
      matched_properties_cache_.ClearViewportDependent();
    }
  }
}

void StyleResolver::Trace(Visitor* visitor) const {
  visitor->Trace(matched_properties_cache_);
  visitor->Trace(initial_style_);
  visitor->Trace(initial_style_for_img_);
  visitor->Trace(selector_filter_);
  visitor->Trace(document_);
  visitor->Trace(tracker_);
}

bool StyleResolver::IsForcedColorsModeEnabled() const {
  return GetDocument().InForcedColorsMode();
}

ComputedStyleBuilder StyleResolver::CreateAnonymousStyleBuilderWithDisplay(
    const ComputedStyle& parent_style,
    EDisplay display) {
  ComputedStyleBuilder builder(*initial_style_, parent_style);
  builder.SetUnicodeBidi(parent_style.GetUnicodeBidi());
  builder.SetDisplay(display);
  return builder;
}

const ComputedStyle* StyleResolver::CreateAnonymousStyleWithDisplay(
    const ComputedStyle& parent_style,
    EDisplay display) {
  return CreateAnonymousStyleBuilderWithDisplay(parent_style, display)
      .TakeStyle();
}

const ComputedStyle* StyleResolver::CreateInheritedDisplayContentsStyleIfNeeded(
    const ComputedStyle& parent_style,
    const ComputedStyle& layout_parent_style) {
  if (parent_style.InheritedEqual(layout_parent_style)) {
    return nullptr;
  }
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

  return changed;
}

}  // namespace

bool StyleResolver::ShouldStopBodyPropagation(const Element& body_or_html) {
  DCHECK(!body_or_html.NeedsReattachLayoutTree())
      << "This method relies on LayoutObject to be attached and up-to-date";
  DCHECK(IsA<HTMLBodyElement>(body_or_html) ||
         IsA<HTMLHtmlElement>(body_or_html));
  LayoutObject* layout_object = body_or_html.GetLayoutObject();
  if (!layout_object) {
    return true;
  }
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
    if (body_style && !background_style->HasBackground()) {
      background_style = body_style;
    }

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
        if (current_layer->Attachment() == EFillAttachment::kScroll) {
          current_layer->SetAttachment(EFillAttachment::kLocal);
        }
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

    // https://github.com/w3c/csswg-drafts/issues/6307
    // In forced colors mode, the internal forced background color is
    // propagated from the root element to the viewport.
    if (IsForcedColorsModeEnabled()) {
      Color internal_forced_background_color =
          document_element_style
              ? document_element_style->VisitedDependentColor(
                    GetCSSPropertyInternalForcedBackgroundColor())
              : Color::kTransparent;
      if (viewport_style.VisitedDependentColor(
              GetCSSPropertyInternalForcedBackgroundColor()) !=
          internal_forced_background_color) {
        changed = true;
        new_viewport_style_builder.SetInternalForcedBackgroundColor(
            StyleColor(internal_forced_background_color));
      }
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
      if (overflow_x == EOverflow::kVisible) {
        overflow_x = EOverflow::kAuto;
      } else if (overflow_x == EOverflow::kClip) {
        overflow_x = EOverflow::kHidden;
      }
      if (overflow_y == EOverflow::kVisible) {
        overflow_y = EOverflow::kAuto;
      } else if (overflow_y == EOverflow::kClip) {
        overflow_y = EOverflow::kHidden;
      }
      if (overflow_anchor == EOverflowAnchor::kVisible) {
        overflow_anchor = EOverflowAnchor::kAuto;
      }

      if (GetDocument().IsInOutermostMainFrame()) {
        using OverscrollBehaviorType = cc::OverscrollBehavior::Type;
        GetDocument().GetPage()->GetChromeClient().SetOverscrollBehavior(
            *GetDocument().GetFrame(),
            cc::OverscrollBehavior(static_cast<OverscrollBehaviorType>(
                                       overflow_style->OverscrollBehaviorX()),
                                   static_cast<OverscrollBehaviorType>(
                                       overflow_style->OverscrollBehaviorY())));
      }

      if (overflow_style->HasCustomScrollbarStyle(document_element)) {
        update_scrollbar_style = true;
      }
    }

    PROPAGATE_VALUE(overflow_x, OverflowX, SetOverflowX)
    PROPAGATE_VALUE(overflow_y, OverflowY, SetOverflowY)
    PROPAGATE_VALUE(overflow_anchor, OverflowAnchor, SetOverflowAnchor);
  }

  // Color
  {
    Color color = StyleColor(CSSValueID::kCanvastext).GetColor();
    if (document_element_style) {
      color =
          document_element_style->VisitedDependentColor(GetCSSPropertyColor());
    }
    if (viewport_style.VisitedDependentColor(GetCSSPropertyColor()) != color) {
      changed = true;
      new_viewport_style_builder.SetColor(StyleColor(color));
    }
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
    PROPAGATE_FROM(document_element_style, ScrollbarWidth, SetScrollbarWidth,
                   EScrollbarWidth::kAuto);
    PROPAGATE_FROM(document_element_style, ScrollbarColor, SetScrollbarColor,
                   nullptr);
    PROPAGATE_FROM(document_element_style, ForcedColorAdjust,
                   SetForcedColorAdjust, EForcedColorAdjust::kAuto);
    PROPAGATE_FROM(document_element_style, ColorSchemeFlagsIsNormal,
                   SetColorSchemeFlagsIsNormal, false);
  }

  // scroll-start
  {
    PROPAGATE_FROM(document_element_style, ScrollStartX, SetScrollStartX,
                   ScrollStartData());
    PROPAGATE_FROM(document_element_style, ScrollStartY, SetScrollStartY,
                   ScrollStartData());
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
    if (actual_cap_height <= desired_cap_height) {
      return actual_font;
    }
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
const ComputedStyle* StyleResolver::StyleForInitialLetterText(
    const ComputedStyle& initial_letter_box_style,
    const ComputedStyle& paragraph_style) {
  DCHECK(paragraph_style.InitialLetter().IsNormal());
  DCHECK(!initial_letter_box_style.InitialLetter().IsNormal());
  ComputedStyleBuilder builder =
      CreateComputedStyleBuilderInheritingFrom(initial_letter_box_style);
  builder.SetFont(
      ComputeInitialLetterFont(initial_letter_box_style, paragraph_style));
  builder.SetLineHeight(Length::Fixed(builder.FontHeight()));
  builder.SetVerticalAlign(EVerticalAlign::kBaseline);
  return builder.TakeStyle();
}

StyleRulePositionTry* StyleResolver::ResolvePositionTryRule(
    const TreeScope* tree_scope,
    AtomicString position_try_name) {
  if (!tree_scope) {
    tree_scope = &GetDocument();
  }

  StyleRulePositionTry* position_try_rule = nullptr;
  for (; tree_scope; tree_scope = tree_scope->ParentTreeScope()) {
    if (ScopedStyleResolver* resolver = tree_scope->GetScopedStyleResolver()) {
      position_try_rule = resolver->PositionTryForName(position_try_name);
      if (position_try_rule) {
        break;
      }
    }
  }

  // Try UA rules if no author rule matches
  if (!position_try_rule) {
    for (const auto& rule : CSSDefaultStyleSheets::Instance()
                                .DefaultHtmlStyle()
                                ->PositionTryRules()) {
      if (position_try_name == rule->Name()) {
        position_try_rule = rule;
        break;
      }
    }
  }

  return position_try_rule;
}

}  // namespace blink
