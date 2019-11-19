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
#include "base/macros.h"
#include "third_party/blink/renderer/core/animation/css/css_animation_update.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/resolver/css_to_style_map.h"
#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"
#include "third_party/blink/renderer/core/css/resolver/element_style_resources.h"
#include "third_party/blink/renderer/core/css/resolver/font_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/style/cached_ua_style.h"

namespace blink {

class ComputedStyle;
class FontDescription;
class PseudoElement;

// A per-element object which wraps an ElementResolveContext. It collects state
// throughout the process of computing the style. It also gives convenient
// access to other element-related information.
class CORE_EXPORT StyleResolverState {
  STACK_ALLOCATED();

 public:
  StyleResolverState(Document&,
                     Element&,
                     const ComputedStyle* parent_style = nullptr,
                     const ComputedStyle* layout_parent_style = nullptr);
  StyleResolverState(Document&,
                     Element&,
                     PseudoId,
                     const ComputedStyle* parent_style,
                     const ComputedStyle* layout_parent_style);
  ~StyleResolverState();

  // In FontFaceSet and CanvasRenderingContext2D, we don't have an element to
  // grab the document from.  This is why we have to store the document
  // separately.
  Document& GetDocument() const { return *document_; }
  // These are all just pass-through methods to ElementResolveContext.
  Element& GetElement() const { return element_context_.GetElement(); }
  TreeScope& GetTreeScope() const;
  const ContainerNode* ParentNode() const {
    return element_context_.ParentNode();
  }
  const ComputedStyle* RootElementStyle() const {
    return element_context_.RootElementStyle();
  }
  EInsideLink ElementLinkState() const {
    return element_context_.ElementLinkState();
  }
  bool DistributedToV0InsertionPoint() const {
    return element_context_.DistributedToV0InsertionPoint();
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

  bool IsAnimatingCustomProperties() const {
    return is_animating_custom_properties_;
  }
  void SetIsAnimatingCustomProperties(bool value) {
    is_animating_custom_properties_ = value;
  }

  const Element* GetAnimatingElement() const;

  void SetParentStyle(scoped_refptr<const ComputedStyle>);
  const ComputedStyle* ParentStyle() const { return parent_style_.get(); }

  void SetLayoutParentStyle(scoped_refptr<const ComputedStyle>);
  const ComputedStyle* LayoutParentStyle() const {
    return layout_parent_style_.get();
  }

  void CacheUserAgentBorderAndBackground();

  const CachedUAStyle* GetCachedUAStyle() const {
    return cached_ua_style_.get();
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

  void SetHasDirAutoAttribute(bool value) { has_dir_auto_attribute_ = value; }
  bool HasDirAutoAttribute() const { return has_dir_auto_attribute_; }

  const CSSValue* GetCascadedColorValue() const {
    return cascaded_color_value_;
  }
  const CSSValue* GetCascadedVisitedColorValue() const {
    return cascaded_visited_color_value_;
  }

  void SetCascadedColorValue(const CSSValue* color) {
    cascaded_color_value_ = color;
  }
  void SetCascadedVisitedColorValue(const CSSValue* color) {
    cascaded_visited_color_value_ = color;
  }

  HeapHashMap<CSSPropertyID, Member<const CSSValue>>&
  ParsedPropertiesForPendingSubstitutionCache(
      const cssvalue::CSSPendingSubstitutionValue&) const;

 private:
  enum class AnimatingElementType { kElement, kPseudoElement };

  StyleResolverState(Document&,
                     Element&,
                     PseudoElement*,
                     AnimatingElementType,
                     const ComputedStyle* parent_style,
                     const ComputedStyle* layout_parent_style);

  CSSToLengthConversionData UnzoomedLengthConversionData(
      const ComputedStyle* font_style) const;

  ElementResolveContext element_context_;
  Member<Document> document_;

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
  bool is_animation_interpolation_map_ready_;
  bool is_animating_custom_properties_;

  bool has_dir_auto_attribute_;

  Member<const CSSValue> cascaded_color_value_;
  Member<const CSSValue> cascaded_visited_color_value_;

  FontBuilder font_builder_;

  std::unique_ptr<CachedUAStyle> cached_ua_style_;

  ElementStyleResources element_style_resources_;
  Member<Element> pseudo_element_;
  AnimatingElementType animating_element_type_;

  mutable HeapHashMap<
      Member<const cssvalue::CSSPendingSubstitutionValue>,
      Member<HeapHashMap<CSSPropertyID, Member<const CSSValue>>>>
      parsed_properties_for_pending_substitution_cache_;
  DISALLOW_COPY_AND_ASSIGN(StyleResolverState);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_RESOLVER_STATE_H_
