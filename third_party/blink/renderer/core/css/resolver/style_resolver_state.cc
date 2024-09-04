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

#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/css_light_dark_value_pair.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

Element* ComputeStyledElement(const StyleRequest& style_request,
                              Element& element) {
  Element* styled_element = style_request.styled_element;
  if (!styled_element) {
    styled_element = &element;
  }
  if (style_request.IsPseudoStyleRequest()) {
    styled_element = styled_element->GetStyledPseudoElement(
        style_request.pseudo_id, style_request.pseudo_argument);
  }
  return styled_element;
}

}  // namespace

StyleResolverState::StyleResolverState(
    Document& document,
    Element& element,
    const StyleRecalcContext* style_recalc_context,
    const StyleRequest& style_request)
    : element_context_(element),
      document_(&document),
      parent_style_(style_request.parent_override),
      layout_parent_style_(style_request.layout_parent_override),
      old_style_(style_recalc_context ? style_recalc_context->old_style
                                      : nullptr),
      pseudo_request_type_(style_request.type),
      font_builder_(&document),
      styled_element_(ComputeStyledElement(style_request, element)),
      element_style_resources_(
          GetStyledElement() ? *GetStyledElement() : GetElement(),
          document.DevicePixelRatio()),
      element_type_(style_request.IsPseudoStyleRequest()
                        ? ElementType::kPseudoElement
                        : ElementType::kElement),
      container_unit_context_(
          style_recalc_context
              ? style_recalc_context->container
              : ContainerQueryEvaluator::ParentContainerCandidateElement(
                    element)),
      anchor_evaluator_(style_recalc_context
                            ? style_recalc_context->anchor_evaluator
                            : nullptr),
      originating_element_style_(style_request.originating_element_style),
      is_for_highlight_(IsHighlightPseudoElement(style_request.pseudo_id)),
      uses_highlight_pseudo_inheritance_(
          ::blink::UsesHighlightPseudoInheritance(style_request.pseudo_id)),
      is_outside_flat_tree_(style_recalc_context
                                ? style_recalc_context->is_outside_flat_tree
                                : false),
      can_trigger_animations_(style_request.can_trigger_animations) {
  DCHECK(!!parent_style_ == !!layout_parent_style_);

  if (UsesHighlightPseudoInheritance()) {
    DCHECK(originating_element_style_);
  } else {
    if (!parent_style_) {
      parent_style_ = element_context_.ParentStyle();
    }
    if (!layout_parent_style_) {
      layout_parent_style_ = element_context_.LayoutParentStyle();
    }
  }

  if (!layout_parent_style_) {
    layout_parent_style_ = parent_style_;
  }

  DCHECK(document.IsActive());
}

StyleResolverState::~StyleResolverState() {
  // For performance reasons, explicitly clear HeapVectors and
  // HeapHashMaps to avoid giving a pressure on Oilpan's GC.
  animation_update_.Clear();
}

bool StyleResolverState::IsInheritedForUnset(
    const CSSProperty& property) const {
  return property.IsInherited() || UsesHighlightPseudoInheritance();
}

EInsideLink StyleResolverState::InsideLink() const {
  if (inside_link_.has_value()) {
    return *inside_link_;
  }
  if (ParentStyle()) {
    inside_link_ = ParentStyle()->InsideLink();
  } else {
    inside_link_ = EInsideLink::kNotInsideLink;
  }
  if (element_type_ != ElementType::kPseudoElement && GetElement().IsLink()) {
    inside_link_ = ElementLinkState();
    if (inside_link_ != EInsideLink::kNotInsideLink) {
      bool force_visited = false;
      probe::ForcePseudoState(&GetElement(), CSSSelector::kPseudoVisited,
                              &force_visited);
      if (force_visited) {
        inside_link_ = EInsideLink::kInsideVisitedLink;
      }
    }
  } else if (uses_highlight_pseudo_inheritance_) {
    // Highlight pseudo-elements acquire the link status of the originating
    // element. Note that highlight pseudo-elements do not *inherit* from
    // the originating element [1], and therefore ParentStyle()->InsideLink()
    // would otherwise always be kNotInsideLink.
    //
    // [1] https://drafts.csswg.org/css-pseudo-4/#highlight-cascade
    inside_link_ = ElementLinkState();
  }
  return *inside_link_;
}

const ComputedStyle* StyleResolverState::TakeStyle() {
  if (had_no_matched_properties_ &&
      pseudo_request_type_ == StyleRequest::kForRenderer) {
    return nullptr;
  }
  return style_builder_->TakeStyle();
}

void StyleResolverState::UpdateLengthConversionData() {
  css_to_length_conversion_data_ = CSSToLengthConversionData(
      *style_builder_, ParentStyle(), RootElementStyle(),
      GetDocument().GetStyleEngine().GetViewportSize(),
      CSSToLengthConversionData::ContainerSizes(container_unit_context_),
      CSSToLengthConversionData::AnchorData(
          anchor_evaluator_, StyleBuilder().PositionAnchor(),
          StyleBuilder().PositionAreaOffsets()),
      StyleBuilder().EffectiveZoom(), length_conversion_flags_);
  element_style_resources_.UpdateLengthConversionData(
      &css_to_length_conversion_data_);
}

CSSToLengthConversionData StyleResolverState::UnzoomedLengthConversionData(
    const FontSizeStyle& font_size_style) {
  const ComputedStyle* root_font_style = RootElementStyle();
  CSSToLengthConversionData::FontSizes font_sizes(font_size_style,
                                                  root_font_style);
  CSSToLengthConversionData::LineHeightSize line_height_size(
      ParentStyle() ? ParentStyle()->GetFontSizeStyle()
                    : style_builder_->GetFontSizeStyle(),
      root_font_style);
  CSSToLengthConversionData::ViewportSize viewport_size(
      GetDocument().GetLayoutView());
  CSSToLengthConversionData::ContainerSizes container_sizes(
      container_unit_context_);
  CSSToLengthConversionData::AnchorData anchor_data(
      anchor_evaluator_, StyleBuilder().PositionAnchor(),
      StyleBuilder().PositionAreaOffsets());
  return CSSToLengthConversionData(
      StyleBuilder().GetWritingMode(), font_sizes, line_height_size,
      viewport_size, container_sizes, anchor_data, 1, length_conversion_flags_);
}

CSSToLengthConversionData StyleResolverState::FontSizeConversionData() {
  return UnzoomedLengthConversionData(ParentStyle()->GetFontSizeStyle());
}

CSSToLengthConversionData StyleResolverState::UnzoomedLengthConversionData() {
  return UnzoomedLengthConversionData(style_builder_->GetFontSizeStyle());
}

void StyleResolverState::SetParentStyle(const ComputedStyle* parent_style) {
  parent_style_ = std::move(parent_style);
  if (style_builder_) {
    // Need to update conversion data for 'lh' units.
    UpdateLengthConversionData();
  }
}

void StyleResolverState::SetLayoutParentStyle(
    const ComputedStyle* parent_style) {
  layout_parent_style_ = parent_style;
}

void StyleResolverState::LoadPendingResources() {
  if (pseudo_request_type_ == StyleRequest::kForComputedStyle ||
      (ParentStyle() && ParentStyle()->IsEnsuredInDisplayNone()) ||
      StyleBuilder().IsEnsuredOutsideFlatTree()) {
    return;
  }
  if (StyleBuilder().Display() == EDisplay::kNone &&
      !GetElement().LayoutObjectIsNeeded(style_builder_->GetDisplayStyle())) {
    // Don't load resources for display:none elements unless we are animating
    // display. If we are animating display, we might otherwise have ended up
    // caching a base style with pending images.
    Element* animating_element = GetAnimatingElement();
    if (!animating_element || !CSSAnimations::IsAnimatingDisplayProperty(
                                  animating_element->GetElementAnimations())) {
      return;
    }
  }

  if (StyleBuilder().StyleType() == kPseudoIdSearchText ||
      StyleBuilder().StyleType() == kPseudoIdTargetText) {
    // Do not load any resources for these pseudos, since that could leak text
    // content to external stylesheets.
    return;
  }

  element_style_resources_.LoadPendingResources(StyleBuilder());
}

const FontDescription& StyleResolverState::ParentFontDescription() const {
  return parent_style_->GetFontDescription();
}

void StyleResolverState::SetZoom(float f) {
  float parent_effective_zoom = ParentStyle()
                                    ? ParentStyle()->EffectiveZoom()
                                    : ComputedStyleInitialValues::InitialZoom();

  StyleBuilder().SetZoom(f);

  if (f != 1.f) {
    GetDocument().CountUse(WebFeature::kCascadedCSSZoomNotEqualToOne);
  }

  if (StyleBuilder().SetEffectiveZoom(parent_effective_zoom * f)) {
    font_builder_.DidChangeEffectiveZoom();
  }
}

void StyleResolverState::SetEffectiveZoom(float f) {
  if (StyleBuilder().SetEffectiveZoom(f)) {
    font_builder_.DidChangeEffectiveZoom();
  }
}

void StyleResolverState::SetWritingMode(WritingMode new_writing_mode) {
  if (StyleBuilder().GetWritingMode() == new_writing_mode) {
    return;
  }
  StyleBuilder().SetWritingMode(new_writing_mode);
  UpdateLengthConversionData();
  font_builder_.DidChangeWritingMode();
}

void StyleResolverState::SetTextSizeAdjust(
    TextSizeAdjust new_text_size_adjust) {
  if (StyleBuilder().GetTextSizeAdjust() == new_text_size_adjust) {
    return;
  }

  if (!new_text_size_adjust.IsAuto()) {
    GetDocument().CountUse(WebFeature::kTextSizeAdjustNotAuto);
    if (new_text_size_adjust.Multiplier() != 1.f) {
      GetDocument().CountUse(WebFeature::kTextSizeAdjustPercentNot100);
    }
  }

  StyleBuilder().SetTextSizeAdjust(new_text_size_adjust);
  // When `TextSizeAdjustImprovements` is enabled, text-size-adjust affects
  // font-size during style building.
  if (RuntimeEnabledFeatures::TextSizeAdjustImprovementsEnabled()) {
    UpdateLengthConversionData();
    font_builder_.DidChangeTextSizeAdjust();
  }
}

void StyleResolverState::SetTextOrientation(ETextOrientation text_orientation) {
  if (StyleBuilder().GetTextOrientation() != text_orientation) {
    StyleBuilder().SetTextOrientation(text_orientation);
    font_builder_.DidChangeTextOrientation();
  }
}

void StyleResolverState::SetPositionAnchor(ScopedCSSName* position_anchor) {
  if (StyleBuilder().PositionAnchor() != position_anchor) {
    StyleBuilder().SetPositionAnchor(position_anchor);
    css_to_length_conversion_data_.SetAnchorData(
        CSSToLengthConversionData::AnchorData(
            anchor_evaluator_, position_anchor,
            StyleBuilder().PositionAreaOffsets()));
  }
}

void StyleResolverState::SetPositionAreaOffsets(
    const std::optional<PositionAreaOffsets>& position_area_offsets) {
  if (StyleBuilder().PositionAreaOffsets() != position_area_offsets) {
    StyleBuilder().SetPositionAreaOffsets(position_area_offsets);
    css_to_length_conversion_data_.SetAnchorData(
        CSSToLengthConversionData::AnchorData(anchor_evaluator_,
                                              StyleBuilder().PositionAnchor(),
                                              position_area_offsets));
  }
}

CSSParserMode StyleResolverState::GetParserMode() const {
  return GetDocument().InQuirksMode() ? kHTMLQuirksMode : kHTMLStandardMode;
}

Element* StyleResolverState::GetAnimatingElement() const {
  // When querying pseudo element styles for an element that does not generate
  // such a pseudo element, the styled_element_ is the originating element. Make
  // sure we only do animations for true pseudo elements.
  return IsForPseudoElement() ? GetPseudoElement() : styled_element_;
}

PseudoElement* StyleResolverState::GetPseudoElement() const {
  return DynamicTo<PseudoElement>(styled_element_);
}

const CSSValue& StyleResolverState::ResolveLightDarkPair(
    const CSSValue& value) {
  if (const auto* pair = DynamicTo<CSSLightDarkValuePair>(value)) {
    if (StyleBuilder().UsedColorScheme() == mojom::blink::ColorScheme::kLight) {
      return pair->First();
    }
    return pair->Second();
  }
  return value;
}

void StyleResolverState::UpdateFont() {
  GetFontBuilder().CreateFont(StyleBuilder(), ParentStyle());
  SetConversionFontSizes(CSSToLengthConversionData::FontSizes(
      style_builder_->GetFontSizeStyle(), RootElementStyle()));
  SetConversionZoom(StyleBuilder().EffectiveZoom());
}

void StyleResolverState::UpdateLineHeight() {
  css_to_length_conversion_data_.SetLineHeightSize(
      CSSToLengthConversionData::LineHeightSize(
          style_builder_->GetFontSizeStyle(),
          GetDocument().documentElement()->GetComputedStyle()));
}

bool StyleResolverState::CanAffectAnimations() const {
  return conditionally_affects_animations_ ||
         StyleBuilder().CanAffectAnimations();
}

}  // namespace blink
