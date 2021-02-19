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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_RESOLVER_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_RESOLVER_STATE_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/css/css_animation_update.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/pseudo_style_request.h"
#include "third_party/blink/renderer/core/css/resolver/css_to_style_map.h"
#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"
#include "third_party/blink/renderer/core/css/resolver/element_style_resources.h"
#include "third_party/blink/renderer/core/css/resolver/font_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

class ComputedStyle;
class FontDescription;
class PseudoElement;

// A per-element object which wraps an ElementResolveContext. It collects state
// throughout the process of computing the style. It also gives convenient
// access to other element-related information.
class CORE_EXPORT StyleResolverState {
  STACK_ALLOCATED();

  enum class ElementType { kElement, kPseudoElement };

 public:
  StyleResolverState(Document&,
                     Element&,
                     const ComputedStyle* parent_style = nullptr,
                     const ComputedStyle* layout_parent_style = nullptr);
  StyleResolverState(Document&,
                     Element&,
                     PseudoId,
                     PseudoElementStyleRequest::RequestType,
                     const ComputedStyle* parent_style,
                     const ComputedStyle* layout_parent_style);
  StyleResolverState(const StyleResolverState&) = delete;
  StyleResolverState& operator=(const StyleResolverState&) = delete;
  ~StyleResolverState();

  bool IsForPseudoElement() const {
    return element_type_ == ElementType::kPseudoElement;
  }

  // In FontFaceSet and CanvasRenderingContext2D, we don't have an element to
  // grab the document from.  This is why we have to store the document
  // separately.
  Document& GetDocument() const { return *document_; }
  // These are all just pass-through methods to ElementResolveContext.
  Element& GetElement() const { return element_context_.GetElement(); }
  const ContainerNode* ParentNode() const {
    return element_context_.ParentNode();
  }
  const ComputedStyle* RootElementStyle() const {
    if (const auto* root_element_style = element_context_.RootElementStyle())
      return root_element_style;
    return Style();
  }
  EInsideLink ElementLinkState() const {
    return element_context_.ElementLinkState();
  }

  const ElementResolveContext& ElementContext() const {
    return element_context_;
  }

  void SetStyle(scoped_refptr<ComputedStyle>);
  const ComputedStyle* Style() const { return style_.get(); }
  ComputedStyle* Style() { return style_.get(); }
  ComputedStyle& StyleRef() {
    DCHECK(style_);
    return *style_;
  }
  scoped_refptr<ComputedStyle> TakeStyle();

  const CSSToLengthConversionData& CssToLengthConversionData() const {
    return css_to_length_conversion_data_;
  }
  CSSToLengthConversionData FontSizeConversionData() const;
  CSSToLengthConversionData UnzoomedLengthConversionData() const;

  void SetConversionFontSizes(
      const CSSToLengthConversionData::FontSizes& font_sizes) {
    css_to_length_conversion_data_.SetFontSizes(font_sizes);
  }
  void SetConversionZoom(float zoom) {
    css_to_length_conversion_data_.SetZoom(zoom);
  }

  CSSAnimationUpdate& AnimationUpdate() { return animation_update_; }
  const CSSAnimationUpdate& AnimationUpdate() const {
    return animation_update_;
  }

  bool IsAnimationInterpolationMapReady() const {
    return is_animation_interpolation_map_ready_;
  }
  void SetIsAnimationInterpolationMapReady() {
    is_animation_interpolation_map_ready_ = true;
  }

  Element* GetAnimatingElement() const;

  void SetParentStyle(scoped_refptr<const ComputedStyle>);
  const ComputedStyle* ParentStyle() const { return parent_style_.get(); }

  void SetLayoutParentStyle(scoped_refptr<const ComputedStyle>);
  const ComputedStyle* LayoutParentStyle() const {
    return layout_parent_style_.get();
  }

  ElementStyleResources& GetElementStyleResources() {
    return element_style_resources_;
  }

  void LoadPendingResources();

  // FIXME: Once styleImage can be made to not take a StyleResolverState
  // this convenience function should be removed. As-is, without this, call
  // sites are extremely verbose.
  StyleImage* GetStyleImage(CSSPropertyID property_id, const CSSValue& value) {
    return element_style_resources_.GetStyleImage(property_id, value);
  }

  FontBuilder& GetFontBuilder() { return font_builder_; }
  const FontBuilder& GetFontBuilder() const { return font_builder_; }
  // FIXME: These exist as a primitive way to track mutations to font-related
  // properties on a ComputedStyle. As designed, these are very error-prone, as
  // some callers set these directly on the ComputedStyle w/o telling us.
  // Presumably we'll want to design a better wrapper around ComputedStyle for
  // tracking these mutations and separate it from StyleResolverState.
  const FontDescription& ParentFontDescription() const;

  void SetZoom(float);
  void SetEffectiveZoom(float);
  void SetWritingMode(WritingMode);
  void SetTextOrientation(ETextOrientation);

  CSSParserMode GetParserMode() const;

  // If the input CSSValue is a CSSLightDarkValuePair, return the light or dark
  // CSSValue based on the UsedColorScheme. For all other values, just return a
  // reference to the passed value. If the property is a non-inherited one, mark
  // the ComputedStyle as having such a pair since that will make sure its not
  // stored in the MatchedPropertiesCache.
  const CSSValue& ResolveLightDarkPair(const CSSProperty&, const CSSValue&);

  void SetCanCacheBaseStyle(bool state) { can_cache_base_style_ = state; }
  bool CanCacheBaseStyle() const { return can_cache_base_style_; }

 private:
  StyleResolverState(Document&,
                     Element&,
                     PseudoElement*,
                     PseudoElementStyleRequest::RequestType,
                     ElementType,
                     const ComputedStyle* parent_style,
                     const ComputedStyle* layout_parent_style);

  CSSToLengthConversionData UnzoomedLengthConversionData(
      const ComputedStyle* font_style) const;

  ElementResolveContext element_context_;
  Document* document_;

  // style_ is the primary output for each element's style resolve.
  scoped_refptr<ComputedStyle> style_;

  CSSToLengthConversionData css_to_length_conversion_data_;

  // parent_style_ is not always just ElementResolveContext::ParentStyle(),
  // so we keep it separate.
  scoped_refptr<const ComputedStyle> parent_style_;
  // This will almost-always be the same that parent_style_, except in the
  // presence of display: contents. This is the style against which we have to
  // do adjustment.
  scoped_refptr<const ComputedStyle> layout_parent_style_;

  CSSAnimationUpdate animation_update_;
  bool is_animation_interpolation_map_ready_ = false;
  PseudoElementStyleRequest::RequestType pseudo_request_type_;

  FontBuilder font_builder_;

  ElementStyleResources element_style_resources_;
  Element* pseudo_element_;
  ElementType element_type_;

  // True if the base style can be cached to optimize style recalculations for
  // animation updates or transition retargeting.
  bool can_cache_base_style_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_RESOLVER_STATE_H_
