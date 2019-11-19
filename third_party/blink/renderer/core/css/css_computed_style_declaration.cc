/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/css/computed_style_css_value_mapping.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_property_id_templates.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/zoom_adjusted_pixel_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// List of all properties we know how to compute, omitting shorthands.
// NOTE: Do not use this list, use computableProperties() instead
// to respect runtime enabling of CSS properties.
const CSSPropertyID kComputedPropertyArray[] = {
    CSSPropertyID::kAnimationDelay, CSSPropertyID::kAnimationDirection,
    CSSPropertyID::kAnimationDuration, CSSPropertyID::kAnimationFillMode,
    CSSPropertyID::kAnimationIterationCount, CSSPropertyID::kAnimationName,
    CSSPropertyID::kAnimationPlayState, CSSPropertyID::kAnimationTimingFunction,
    CSSPropertyID::kBackgroundAttachment, CSSPropertyID::kBackgroundBlendMode,
    CSSPropertyID::kBackgroundClip, CSSPropertyID::kBackgroundColor,
    CSSPropertyID::kBackgroundImage, CSSPropertyID::kBackgroundOrigin,
    // more-specific background-position-x/y are non-standard
    CSSPropertyID::kBackgroundPosition, CSSPropertyID::kBackgroundRepeat,
    CSSPropertyID::kBackgroundSize, CSSPropertyID::kBorderBottomColor,
    CSSPropertyID::kBorderBottomLeftRadius,
    CSSPropertyID::kBorderBottomRightRadius, CSSPropertyID::kBorderBottomStyle,
    CSSPropertyID::kBorderBottomWidth, CSSPropertyID::kBorderCollapse,
    CSSPropertyID::kBorderImageOutset, CSSPropertyID::kBorderImageRepeat,
    CSSPropertyID::kBorderImageSlice, CSSPropertyID::kBorderImageSource,
    CSSPropertyID::kBorderImageWidth, CSSPropertyID::kBorderLeftColor,
    CSSPropertyID::kBorderLeftStyle, CSSPropertyID::kBorderLeftWidth,
    CSSPropertyID::kBorderRightColor, CSSPropertyID::kBorderRightStyle,
    CSSPropertyID::kBorderRightWidth, CSSPropertyID::kBorderTopColor,
    CSSPropertyID::kBorderTopLeftRadius, CSSPropertyID::kBorderTopRightRadius,
    CSSPropertyID::kBorderTopStyle, CSSPropertyID::kBorderTopWidth,
    CSSPropertyID::kBottom, CSSPropertyID::kBoxShadow,
    CSSPropertyID::kBoxSizing, CSSPropertyID::kBreakAfter,
    CSSPropertyID::kBreakBefore, CSSPropertyID::kBreakInside,
    CSSPropertyID::kCaptionSide, CSSPropertyID::kClear, CSSPropertyID::kClip,
    CSSPropertyID::kColor, CSSPropertyID::kContent, CSSPropertyID::kCursor,
    CSSPropertyID::kDirection, CSSPropertyID::kDisplay,
    CSSPropertyID::kEmptyCells, CSSPropertyID::kFloat,
    CSSPropertyID::kFontFamily, CSSPropertyID::kFontKerning,
    CSSPropertyID::kFontOpticalSizing, CSSPropertyID::kFontSize,
    CSSPropertyID::kFontSizeAdjust, CSSPropertyID::kFontStretch,
    CSSPropertyID::kFontStyle, CSSPropertyID::kFontVariant,
    CSSPropertyID::kFontVariantLigatures, CSSPropertyID::kFontVariantCaps,
    CSSPropertyID::kFontVariantNumeric, CSSPropertyID::kFontVariantEastAsian,
    CSSPropertyID::kFontWeight, CSSPropertyID::kHeight,
    CSSPropertyID::kImageOrientation, CSSPropertyID::kImageRendering,
    CSSPropertyID::kIsolation, CSSPropertyID::kJustifyItems,
    CSSPropertyID::kJustifySelf, CSSPropertyID::kLeft,
    CSSPropertyID::kLetterSpacing, CSSPropertyID::kLineHeight,
    CSSPropertyID::kLineHeightStep, CSSPropertyID::kListStyleImage,
    CSSPropertyID::kListStylePosition, CSSPropertyID::kListStyleType,
    CSSPropertyID::kMarginBottom, CSSPropertyID::kMarginLeft,
    CSSPropertyID::kMarginRight, CSSPropertyID::kMarginTop,
    CSSPropertyID::kMaxHeight, CSSPropertyID::kMaxWidth,
    CSSPropertyID::kMinHeight, CSSPropertyID::kMinWidth,
    CSSPropertyID::kMixBlendMode, CSSPropertyID::kObjectFit,
    CSSPropertyID::kObjectPosition, CSSPropertyID::kOffsetAnchor,
    CSSPropertyID::kOffsetDistance, CSSPropertyID::kOffsetPath,
    CSSPropertyID::kOffsetPosition, CSSPropertyID::kOffsetRotate,
    CSSPropertyID::kOpacity, CSSPropertyID::kOrphans,
    CSSPropertyID::kOutlineColor, CSSPropertyID::kOutlineOffset,
    CSSPropertyID::kOutlineStyle, CSSPropertyID::kOutlineWidth,
    CSSPropertyID::kOverflowAnchor, CSSPropertyID::kOverflowWrap,
    CSSPropertyID::kOverflowX, CSSPropertyID::kOverflowY,
    CSSPropertyID::kPaddingBottom, CSSPropertyID::kPaddingLeft,
    CSSPropertyID::kPaddingRight, CSSPropertyID::kPaddingTop,
    CSSPropertyID::kPointerEvents, CSSPropertyID::kPosition,
    CSSPropertyID::kResize, CSSPropertyID::kRight,
    CSSPropertyID::kScrollBehavior, CSSPropertyID::kScrollCustomization,
    CSSPropertyID::kSpeak, CSSPropertyID::kTableLayout, CSSPropertyID::kTabSize,
    CSSPropertyID::kTextAlign, CSSPropertyID::kTextAlignLast,
    CSSPropertyID::kTextDecoration, CSSPropertyID::kTextDecorationLine,
    CSSPropertyID::kTextDecorationStyle, CSSPropertyID::kTextDecorationColor,
    CSSPropertyID::kTextDecorationSkipInk, CSSPropertyID::kTextJustify,
    CSSPropertyID::kTextUnderlinePosition, CSSPropertyID::kTextIndent,
    CSSPropertyID::kTextRendering, CSSPropertyID::kTextShadow,
    CSSPropertyID::kTextSizeAdjust, CSSPropertyID::kTextOverflow,
    CSSPropertyID::kTextTransform, CSSPropertyID::kTop,
    CSSPropertyID::kTouchAction, CSSPropertyID::kTransitionDelay,
    CSSPropertyID::kTransitionDuration, CSSPropertyID::kTransitionProperty,
    CSSPropertyID::kTransitionTimingFunction, CSSPropertyID::kUnicodeBidi,
    CSSPropertyID::kVerticalAlign, CSSPropertyID::kVisibility,
    CSSPropertyID::kWhiteSpace, CSSPropertyID::kWidows, CSSPropertyID::kWidth,
    CSSPropertyID::kWillChange, CSSPropertyID::kWordBreak,
    CSSPropertyID::kWordSpacing, CSSPropertyID::kZIndex, CSSPropertyID::kZoom,

    CSSPropertyID::kWebkitAppearance, CSSPropertyID::kBackfaceVisibility,
    CSSPropertyID::kWebkitBorderHorizontalSpacing,
    CSSPropertyID::kWebkitBorderImage,
    CSSPropertyID::kWebkitBorderVerticalSpacing, CSSPropertyID::kWebkitBoxAlign,
    CSSPropertyID::kWebkitBoxDecorationBreak,
    CSSPropertyID::kWebkitBoxDirection, CSSPropertyID::kWebkitBoxFlex,
    CSSPropertyID::kWebkitBoxOrdinalGroup, CSSPropertyID::kWebkitBoxOrient,
    CSSPropertyID::kWebkitBoxPack, CSSPropertyID::kWebkitBoxReflect,
    CSSPropertyID::kColumnCount, CSSPropertyID::kColumnGap,
    CSSPropertyID::kColumnRuleColor, CSSPropertyID::kColumnRuleStyle,
    CSSPropertyID::kColumnRuleWidth, CSSPropertyID::kColumnSpan,
    CSSPropertyID::kColumnWidth, CSSPropertyID::kBackdropFilter,
    CSSPropertyID::kAlignContent, CSSPropertyID::kAlignItems,
    CSSPropertyID::kAlignSelf, CSSPropertyID::kFlexBasis,
    CSSPropertyID::kFlexGrow, CSSPropertyID::kFlexShrink,
    CSSPropertyID::kFlexDirection, CSSPropertyID::kFlexWrap,
    CSSPropertyID::kJustifyContent, CSSPropertyID::kWebkitFontSmoothing,
    CSSPropertyID::kGridAutoColumns, CSSPropertyID::kGridAutoFlow,
    CSSPropertyID::kGridAutoRows, CSSPropertyID::kGridColumnEnd,
    CSSPropertyID::kGridColumnStart, CSSPropertyID::kGridTemplateAreas,
    CSSPropertyID::kGridTemplateColumns, CSSPropertyID::kGridTemplateRows,
    CSSPropertyID::kGridRowEnd, CSSPropertyID::kGridRowStart,
    CSSPropertyID::kRowGap, CSSPropertyID::kWebkitHighlight,
    CSSPropertyID::kHyphens, CSSPropertyID::kWebkitHyphenateCharacter,
    CSSPropertyID::kWebkitLineBreak, CSSPropertyID::kWebkitLineClamp,
    CSSPropertyID::kWebkitLocale, CSSPropertyID::kWebkitMarginBeforeCollapse,
    CSSPropertyID::kWebkitMarginAfterCollapse,
    CSSPropertyID::kWebkitMaskBoxImage,
    CSSPropertyID::kWebkitMaskBoxImageOutset,
    CSSPropertyID::kWebkitMaskBoxImageRepeat,
    CSSPropertyID::kWebkitMaskBoxImageSlice,
    CSSPropertyID::kWebkitMaskBoxImageSource,
    CSSPropertyID::kWebkitMaskBoxImageWidth, CSSPropertyID::kWebkitMaskClip,
    CSSPropertyID::kWebkitMaskComposite, CSSPropertyID::kWebkitMaskImage,
    CSSPropertyID::kWebkitMaskOrigin, CSSPropertyID::kWebkitMaskPosition,
    CSSPropertyID::kWebkitMaskRepeat, CSSPropertyID::kWebkitMaskSize,
    CSSPropertyID::kOrder, CSSPropertyID::kPerspective,
    CSSPropertyID::kPerspectiveOrigin, CSSPropertyID::kWebkitPrintColorAdjust,
    CSSPropertyID::kWebkitRtlOrdering, CSSPropertyID::kShapeOutside,
    CSSPropertyID::kShapeImageThreshold, CSSPropertyID::kShapeMargin,
    CSSPropertyID::kWebkitTapHighlightColor, CSSPropertyID::kWebkitTextCombine,
    CSSPropertyID::kWebkitTextDecorationsInEffect,
    CSSPropertyID::kWebkitTextEmphasisColor,
    CSSPropertyID::kWebkitTextEmphasisPosition,
    CSSPropertyID::kWebkitTextEmphasisStyle,
    CSSPropertyID::kWebkitTextFillColor, CSSPropertyID::kWebkitTextOrientation,
    CSSPropertyID::kWebkitTextSecurity, CSSPropertyID::kWebkitTextStrokeColor,
    CSSPropertyID::kWebkitTextStrokeWidth, CSSPropertyID::kTransform,
    CSSPropertyID::kTransformOrigin, CSSPropertyID::kTransformStyle,
    CSSPropertyID::kWebkitUserDrag, CSSPropertyID::kWebkitUserModify,
    CSSPropertyID::kUserSelect, CSSPropertyID::kWebkitWritingMode,
    CSSPropertyID::kWebkitAppRegion, CSSPropertyID::kBufferedRendering,
    CSSPropertyID::kClipPath, CSSPropertyID::kClipRule, CSSPropertyID::kMask,
    CSSPropertyID::kFilter, CSSPropertyID::kFloodColor,
    CSSPropertyID::kFloodOpacity, CSSPropertyID::kLightingColor,
    CSSPropertyID::kStopColor, CSSPropertyID::kStopOpacity,
    CSSPropertyID::kColorInterpolation,
    CSSPropertyID::kColorInterpolationFilters, CSSPropertyID::kColorRendering,
    CSSPropertyID::kFill, CSSPropertyID::kFillOpacity, CSSPropertyID::kFillRule,
    CSSPropertyID::kMarkerEnd, CSSPropertyID::kMarkerMid,
    CSSPropertyID::kMarkerStart, CSSPropertyID::kMaskType,
    CSSPropertyID::kMaskSourceType, CSSPropertyID::kShapeRendering,
    CSSPropertyID::kStroke, CSSPropertyID::kStrokeDasharray,
    CSSPropertyID::kStrokeDashoffset, CSSPropertyID::kStrokeLinecap,
    CSSPropertyID::kStrokeLinejoin, CSSPropertyID::kStrokeMiterlimit,
    CSSPropertyID::kStrokeOpacity, CSSPropertyID::kStrokeWidth,
    CSSPropertyID::kAlignmentBaseline, CSSPropertyID::kBaselineShift,
    CSSPropertyID::kDominantBaseline, CSSPropertyID::kTextAnchor,
    CSSPropertyID::kWritingMode, CSSPropertyID::kVectorEffect,
    CSSPropertyID::kPaintOrder, CSSPropertyID::kD, CSSPropertyID::kCx,
    CSSPropertyID::kCy, CSSPropertyID::kX, CSSPropertyID::kY, CSSPropertyID::kR,
    CSSPropertyID::kRx, CSSPropertyID::kRy, CSSPropertyID::kTranslate,
    CSSPropertyID::kRotate, CSSPropertyID::kScale, CSSPropertyID::kCaretColor,
    CSSPropertyID::kLineBreak};

CSSValueID CssIdentifierForFontSizeKeyword(int keyword_size) {
  DCHECK_NE(keyword_size, 0);
  DCHECK_LE(keyword_size, 8);
  return static_cast<CSSValueID>(static_cast<int>(CSSValueID::kXxSmall) +
                                 keyword_size - 1);
}

void LogUnimplementedPropertyID(const CSSProperty& property) {
  DEFINE_STATIC_LOCAL(HashSet<CSSPropertyID>, property_id_set, ());
  if (property.PropertyID() == CSSPropertyID::kVariable)
    return;
  if (!property_id_set.insert(property.PropertyID()).is_new_entry)
    return;

  DLOG(ERROR) << "Blink does not yet implement getComputedStyle for '"
              << property.GetPropertyName() << "'.";
}

}  // namespace

const Vector<const CSSProperty*>&
CSSComputedStyleDeclaration::ComputableProperties() {
  DEFINE_STATIC_LOCAL(Vector<const CSSProperty*>, properties, ());
  if (properties.IsEmpty()) {
    CSSProperty::FilterWebExposedCSSPropertiesIntoVector(
        kComputedPropertyArray, base::size(kComputedPropertyArray), properties);
  }
  return properties;
}

CSSComputedStyleDeclaration::CSSComputedStyleDeclaration(
    Node* n,
    bool allow_visited_style,
    const String& pseudo_element_name)
    : node_(n),
      pseudo_element_specifier_(
          CSSSelector::ParsePseudoId(pseudo_element_name)),
      allow_visited_style_(allow_visited_style) {}

CSSComputedStyleDeclaration::~CSSComputedStyleDeclaration() = default;

String CSSComputedStyleDeclaration::cssText() const {
  StringBuilder result;
  static const Vector<const CSSProperty*>& properties = ComputableProperties();

  for (unsigned i = 0; i < properties.size(); i++) {
    if (i)
      result.Append(' ');
    result.Append(properties[i]->GetPropertyName());
    result.Append(": ");
    result.Append(GetPropertyValue(properties[i]->PropertyID()));
    result.Append(';');
  }

  return result.ToString();
}

void CSSComputedStyleDeclaration::setCSSText(const ExecutionContext*,
                                             const String&,
                                             ExceptionState& exception_state) {
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNoModificationAllowedError,
      "These styles are computed, and therefore read-only.");
}

const CSSValue*
CSSComputedStyleDeclaration::GetFontSizeCSSValuePreferringKeyword() const {
  if (!node_)
    return nullptr;

  node_->GetDocument().UpdateStyleAndLayout();

  const ComputedStyle* style =
      node_->EnsureComputedStyle(pseudo_element_specifier_);
  if (!style)
    return nullptr;

  if (int keyword_size = style->GetFontDescription().KeywordSize()) {
    return CSSIdentifierValue::Create(
        CssIdentifierForFontSizeKeyword(keyword_size));
  }

  return ZoomAdjustedPixelValue(style->GetFontDescription().ComputedPixelSize(),
                                *style);
}

bool CSSComputedStyleDeclaration::IsMonospaceFont() const {
  if (!node_)
    return false;

  const ComputedStyle* style =
      node_->EnsureComputedStyle(pseudo_element_specifier_);
  if (!style)
    return false;

  return style->GetFontDescription().IsMonospace();
}
const ComputedStyle* CSSComputedStyleDeclaration::ComputeComputedStyle() const {
  Node* styled_node = this->StyledNode();
  DCHECK(styled_node);
  const ComputedStyle* style = styled_node->EnsureComputedStyle(
      styled_node->IsPseudoElement() ? kPseudoIdNone
                                     : pseudo_element_specifier_);
  if (style && style->IsEnsuredOutsideFlatTree()) {
    UseCounter::Count(node_->GetDocument(),
                      WebFeature::kGetComputedStyleOutsideFlatTree);
  }
  return style;
}

Node* CSSComputedStyleDeclaration::StyledNode() const {
  if (!node_)
    return nullptr;

  if (auto* node_element = DynamicTo<Element>(node_.Get())) {
    if (PseudoElement* element =
            node_element->GetPseudoElement(pseudo_element_specifier_))
      return element;
  }
  return node_.Get();
}

LayoutObject* CSSComputedStyleDeclaration::StyledLayoutObject() const {
  auto* node = StyledNode();
  if (!node)
    return nullptr;

  if (pseudo_element_specifier_ != kPseudoIdNone && node == node_.Get())
    return nullptr;

  return node->GetLayoutObject();
}

const CSSValue* CSSComputedStyleDeclaration::GetPropertyCSSValue(
    CSSPropertyID property_id) const {
  if (property_id == CSSPropertyID::kVariable) {
    // TODO(https://crbug.com/980160): Disallow calling this function with
    // kVariable.
    return nullptr;
  }
  return GetPropertyCSSValue(CSSPropertyName(property_id));
}

const CSSValue* CSSComputedStyleDeclaration::GetPropertyCSSValue(
    AtomicString custom_property_name) const {
  return GetPropertyCSSValue(CSSPropertyName(custom_property_name));
}

HeapHashMap<AtomicString, Member<const CSSValue>>
CSSComputedStyleDeclaration::GetVariables() const {
  const ComputedStyle* style = ComputeComputedStyle();
  if (!style)
    return {};
  DCHECK(StyledNode());
  return ComputedStyleCSSValueMapping::GetVariables(
      *style, StyledNode()->GetDocument().GetPropertyRegistry());
}

const CSSValue* CSSComputedStyleDeclaration::GetPropertyCSSValue(
    const CSSPropertyName& property_name) const {
  Node* styled_node = StyledNode();
  if (!styled_node)
    return nullptr;

  Document& document = styled_node->GetDocument();

  if (HTMLFrameOwnerElement* owner = document.LocalOwner()) {
    // We are inside an iframe. If any of our ancestor iframes needs a style
    // and/or layout update, we need to make that up-to-date to resolve viewport
    // media queries and generate boxes as we might be moving to/from
    // display:none in some element in the chain of ancestors.
    //
    // TODO(futhark@chromium.org): There is an open question what the computed
    // style should be in a display:none iframe. If the property we are querying
    // is not layout dependent, we will not update the iframe layout box here.
    bool is_layout_dependent_property =
        !property_name.IsCustomProperty() &&
        CSSProperty::Get(property_name.Id()).IsLayoutDependentProperty();
    if (is_layout_dependent_property ||
        document.GetStyleEngine().HasViewportDependentMediaQueries()) {
      owner->GetDocument().UpdateStyleAndLayout();
      // The style recalc could have caused the styled node to be discarded or
      // replaced if it was a PseudoElement so we need to update it.
      styled_node = StyledNode();
    }
  }

  document.UpdateStyleAndLayoutTreeForNode(styled_node);

  CSSPropertyRef ref(property_name, document);
  if (!ref.IsValid())
    return nullptr;
  const CSSProperty& property_class = ref.GetProperty();

  // The style recalc could have caused the styled node to be discarded or
  // replaced if it was a PseudoElement so we need to update it.
  styled_node = StyledNode();
  LayoutObject* layout_object = StyledLayoutObject();
  const ComputedStyle* style = ComputeComputedStyle();

  if (property_class.IsLayoutDependent(style, layout_object)) {
    document.UpdateStyleAndLayoutForNode(styled_node);
    styled_node = StyledNode();
    style = ComputeComputedStyle();
    layout_object = StyledLayoutObject();
  }

  if (!style)
    return nullptr;

  const CSSValue* value = property_class.CSSValueFromComputedStyle(
      *style, layout_object, allow_visited_style_);
  if (value)
    return value;

  LogUnimplementedPropertyID(property_class);
  return nullptr;
}

String CSSComputedStyleDeclaration::GetPropertyValue(
    CSSPropertyID property_id) const {
  // allow_visited_style_ is true only for access from DevTools.
  if (!allow_visited_style_ &&
      property_id == CSSPropertyID::kWebkitAppearance) {
    UseCounter::Count(
        node_->GetDocument(),
        WebFeature::kGetComputedStyleForWebkitAppearanceExcludeDevTools);
  }
  const CSSValue* value = GetPropertyCSSValue(property_id);
  if (value)
    return value->CssText();
  return "";
}

unsigned CSSComputedStyleDeclaration::length() const {
  if (!node_ || !node_->InActiveDocument())
    return 0;
  return ComputableProperties().size();
}

String CSSComputedStyleDeclaration::item(unsigned i) const {
  if (i >= length())
    return "";

  return ComputableProperties()[i]->GetPropertyNameString();
}

bool CSSComputedStyleDeclaration::CssPropertyMatches(
    CSSPropertyID property_id,
    const CSSValue& property_value) const {
  if (property_id == CSSPropertyID::kFontSize &&
      (property_value.IsPrimitiveValue() ||
       property_value.IsIdentifierValue()) &&
      node_) {
    node_->GetDocument().UpdateStyleAndLayout();
    const ComputedStyle* style =
        node_->EnsureComputedStyle(pseudo_element_specifier_);
    if (style && style->GetFontDescription().KeywordSize()) {
      CSSValueID size_value = CssIdentifierForFontSizeKeyword(
          style->GetFontDescription().KeywordSize());
      auto* identifier_value = DynamicTo<CSSIdentifierValue>(property_value);
      if (identifier_value && identifier_value->GetValueID() == size_value)
        return true;
    }
  }
  const CSSValue* value = GetPropertyCSSValue(property_id);
  return DataEquivalent(value, &property_value);
}

MutableCSSPropertyValueSet* CSSComputedStyleDeclaration::CopyProperties()
    const {
  return CopyPropertiesInSet(ComputableProperties());
}

MutableCSSPropertyValueSet* CSSComputedStyleDeclaration::CopyPropertiesInSet(
    const Vector<const CSSProperty*>& properties) const {
  HeapVector<CSSPropertyValue, 256> list;
  list.ReserveInitialCapacity(properties.size());
  for (unsigned i = 0; i < properties.size(); ++i) {
    const CSSProperty& property = *properties[i];
    const CSSValue* value = GetPropertyCSSValue(property.GetCSSPropertyName());
    if (value)
      list.push_back(CSSPropertyValue(property, *value, false));
  }
  return MakeGarbageCollected<MutableCSSPropertyValueSet>(list.data(),
                                                          list.size());
}

CSSRule* CSSComputedStyleDeclaration::parentRule() const {
  return nullptr;
}

String CSSComputedStyleDeclaration::getPropertyValue(
    const String& property_name) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (!isValidCSSPropertyID(property_id))
    return String();
  if (property_id == CSSPropertyID::kVariable) {
    const CSSValue* value = GetPropertyCSSValue(AtomicString(property_name));
    if (value)
      return value->CssText();
    return String();
  }
#if DCHECK_IS_ON
  DCHECK(CSSProperty::Get(property_id).IsEnabled());
#endif
  return GetPropertyValue(property_id);
}

String CSSComputedStyleDeclaration::getPropertyPriority(const String&) {
  // All computed styles have a priority of not "important".
  return "";
}

String CSSComputedStyleDeclaration::GetPropertyShorthand(const String&) {
  return "";
}

bool CSSComputedStyleDeclaration::IsPropertyImplicit(const String&) {
  return false;
}

void CSSComputedStyleDeclaration::setProperty(const ExecutionContext*,
                                              const String& name,
                                              const String&,
                                              const String&,
                                              ExceptionState& exception_state) {
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNoModificationAllowedError,
      "These styles are computed, and therefore the '" + name +
          "' property is read-only.");
}

String CSSComputedStyleDeclaration::removeProperty(
    const String& name,
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNoModificationAllowedError,
      "These styles are computed, and therefore the '" + name +
          "' property is read-only.");
  return String();
}

const CSSValue* CSSComputedStyleDeclaration::GetPropertyCSSValueInternal(
    CSSPropertyID property_id) {
  if (property_id == CSSPropertyID::kWebkitAppearance && node_) {
    UseCounter::Count(node_->GetDocument(),
                      WebFeature::kGetComputedStyleWebkitAppearance);
  }
  return GetPropertyCSSValue(property_id);
}

const CSSValue* CSSComputedStyleDeclaration::GetPropertyCSSValueInternal(
    AtomicString custom_property_name) {
  DCHECK_EQ(CSSPropertyID::kVariable, cssPropertyID(custom_property_name));
  return GetPropertyCSSValue(custom_property_name);
}

String CSSComputedStyleDeclaration::GetPropertyValueInternal(
    CSSPropertyID property_id) {
  return GetPropertyValue(property_id);
}

void CSSComputedStyleDeclaration::SetPropertyInternal(
    CSSPropertyID id,
    const String&,
    const String&,
    bool,
    SecureContextMode,
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNoModificationAllowedError,
      "These styles are computed, and therefore the '" +
          CSSUnresolvedProperty::Get(id).GetPropertyNameString() +
          "' property is read-only.");
}

void CSSComputedStyleDeclaration::Trace(blink::Visitor* visitor) {
  visitor->Trace(node_);
  CSSStyleDeclaration::Trace(visitor);
}

}  // namespace blink
