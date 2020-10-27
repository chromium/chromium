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

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/css/css_light_dark_value_pair.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

StyleResolverState::StyleResolverState(
    Document& document,
    Element& element,
    PseudoElement* pseudo_element,
    PseudoElementStyleRequest::RequestType pseudo_request_type,
    ElementType element_type,
    const ComputedStyle* parent_style,
    const ComputedStyle* layout_parent_style)
    : element_context_(element),
      document_(&document),
      parent_style_(parent_style),
      layout_parent_style_(layout_parent_style),
      pseudo_request_type_(pseudo_request_type),
      font_builder_(&document),
      element_style_resources_(GetElement(),
                               document.DevicePixelRatio(),
                               pseudo_element),
      pseudo_element_(pseudo_element),
      element_type_(element_type) {
  DCHECK(!!parent_style_ == !!layout_parent_style_);

  if (!parent_style_) {
    parent_style_ = element_context_.ParentStyle();
  }

  if (!layout_parent_style_)
    layout_parent_style_ = element_context_.LayoutParentStyle();

  if (!layout_parent_style_)
    layout_parent_style_ = parent_style_;

  DCHECK(document.IsActive());
}

StyleResolverState::StyleResolverState(Document& document,
                                       Element& element,
                                       const ComputedStyle* parent_style,
                                       const ComputedStyle* layout_parent_style)
    : StyleResolverState(document,
                         element,
                         nullptr /* pseudo_element */,
                         PseudoElementStyleRequest::kForRenderer,
                         ElementType::kElement,
                         parent_style,
                         layout_parent_style) {}

StyleResolverState::StyleResolverState(
    Document& document,
    Element& element,
    PseudoId pseudo_id,
    PseudoElementStyleRequest::RequestType pseudo_request_type,
    const ComputedStyle* parent_style,
    const ComputedStyle* layout_parent_style)
    : StyleResolverState(document,
                         element,
                         element.GetPseudoElement(pseudo_id),
                         pseudo_request_type,
                         ElementType::kPseudoElement,
                         parent_style,
                         layout_parent_style) {}

StyleResolverState::~StyleResolverState() {
  // For performance reasons, explicitly clear HeapVectors and
  // HeapHashMaps to avoid giving a pressure on Oilpan's GC.
  animation_update_.Clear();
}

TreeScope& StyleResolverState::GetTreeScope() const {
  return GetElement().GetTreeScope();
}

void StyleResolverState::SetStyle(scoped_refptr<ComputedStyle> style) {
  // FIXME: Improve RAII of StyleResolverState to remove this function.
  style_ = std::move(style);
  css_to_length_conversion_data_ = CSSToLengthConversionData(
      style_.get(), RootElementStyle(), GetDocument().GetLayoutView(),
      style_->EffectiveZoom());
}

scoped_refptr<ComputedStyle> StyleResolverState::TakeStyle() {
  return std::move(style_);
}

CSSToLengthConversionData StyleResolverState::UnzoomedLengthConversionData(
    const ComputedStyle* font_style) const {
  float em = font_style->SpecifiedFontSize();
  float rem = RootElementStyle() ? RootElementStyle()->SpecifiedFontSize() : 1;
  CSSToLengthConversionData::FontSizes font_sizes(
      em, rem, &font_style->GetFont(), font_style->EffectiveZoom());
  CSSToLengthConversionData::ViewportSize viewport_size(
      GetDocument().GetLayoutView());

  return CSSToLengthConversionData(Style(), font_sizes, viewport_size, 1);
}

CSSToLengthConversionData StyleResolverState::FontSizeConversionData() const {
  return UnzoomedLengthConversionData(ParentStyle());
}

CSSToLengthConversionData StyleResolverState::UnzoomedLengthConversionData()
    const {
  return UnzoomedLengthConversionData(Style());
}

void StyleResolverState::SetParentStyle(
    scoped_refptr<const ComputedStyle> parent_style) {
  parent_style_ = std::move(parent_style);
}

void StyleResolverState::SetLayoutParentStyle(
    scoped_refptr<const ComputedStyle> parent_style) {
  layout_parent_style_ = std::move(parent_style);
}

void StyleResolverState::LoadPendingResources() {
  if (pseudo_request_type_ == PseudoElementStyleRequest::kForComputedStyle ||
      (ParentStyle() && ParentStyle()->IsEnsuredInDisplayNone()) ||
      StyleRef().Display() == EDisplay::kNone ||
      StyleRef().IsEnsuredOutsideFlatTree())
    return;

  element_style_resources_.LoadPendingResources(Style());
}

const FontDescription& StyleResolverState::ParentFontDescription() const {
  return parent_style_->GetFontDescription();
}

void StyleResolverState::SetZoom(float f) {
  float parent_effective_zoom = ParentStyle()
                                    ? ParentStyle()->EffectiveZoom()
                                    : ComputedStyleInitialValues::InitialZoom();

  style_->SetZoom(f);

  if (f != 1.f)
    GetDocument().CountUse(WebFeature::kCascadedCSSZoomNotEqualToOne);

  if (style_->SetEffectiveZoom(parent_effective_zoom * f))
    font_builder_.DidChangeEffectiveZoom();
}

void StyleResolverState::SetEffectiveZoom(float f) {
  if (style_->SetEffectiveZoom(f))
    font_builder_.DidChangeEffectiveZoom();
}

void StyleResolverState::SetWritingMode(WritingMode new_writing_mode) {
  if (style_->GetWritingMode() == new_writing_mode) {
    return;
  }
  style_->SetWritingMode(new_writing_mode);
  font_builder_.DidChangeWritingMode();
}

void StyleResolverState::SetTextOrientation(ETextOrientation text_orientation) {
  if (style_->GetTextOrientation() != text_orientation) {
    style_->SetTextOrientation(text_orientation);
    font_builder_.DidChangeTextOrientation();
  }
}

CSSParserMode StyleResolverState::GetParserMode() const {
  return GetDocument().InQuirksMode() ? kHTMLQuirksMode : kHTMLStandardMode;
}

Element* StyleResolverState::GetAnimatingElement() const {
  if (element_type_ == ElementType::kElement)
    return &GetElement();
  DCHECK_EQ(ElementType::kPseudoElement, element_type_);
  return pseudo_element_;
}

const CSSValue& StyleResolverState::ResolveLightDarkPair(
    const CSSProperty& property,
    const CSSValue& value) {
  if (const auto* pair = DynamicTo<CSSLightDarkValuePair>(value)) {
    if (!property.IsInherited())
      Style()->SetHasNonInheritedLightDarkValue();
    if (Style()->UsedColorScheme() == mojom::blink::ColorScheme::kLight)
      return pair->First();
    return pair->Second();
  }
  return value;
}

void StyleResolverState::MarkDependency(const CSSProperty& property) {
  if (!RuntimeEnabledFeatures::CSSMatchedPropertiesCacheDependenciesEnabled())
    return;
  if (!HasValidDependencies())
    return;

  has_incomparable_dependency_ |= !property.IsComputedValueComparable();
  dependencies_.insert(property.GetCSSPropertyName());
}

}  // namespace blink
