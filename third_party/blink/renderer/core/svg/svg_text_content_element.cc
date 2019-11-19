/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_query.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg/svg_point_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_rect_tear_off.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

template <>
const SVGEnumerationMap& GetEnumerationMap<SVGLengthAdjustType>() {
  static const SVGEnumerationMap::Entry enum_items[] = {
      {kSVGLengthAdjustSpacing, "spacing"},
      {kSVGLengthAdjustSpacingAndGlyphs, "spacingAndGlyphs"},
  };
  static const SVGEnumerationMap entries(enum_items);
  return entries;
}

// SVGTextContentElement's 'textLength' attribute needs special handling.
// It should return getComputedTextLength() when textLength is not specified
// manually.
class SVGAnimatedTextLength final : public SVGAnimatedLength {
 public:
  SVGAnimatedTextLength(SVGTextContentElement* context_element)
      : SVGAnimatedLength(context_element,
                          svg_names::kTextLengthAttr,
                          SVGLengthMode::kWidth,
                          SVGLength::Initial::kUnitlessZero) {}

  SVGLengthTearOff* baseVal() override {
    auto* text_content_element = To<SVGTextContentElement>(ContextElement());
    if (!text_content_element->TextLengthIsSpecifiedByUser())
      BaseValue()->NewValueSpecifiedUnits(
          CSSPrimitiveValue::UnitType::kNumber,
          text_content_element->getComputedTextLength());

    return SVGAnimatedLength::baseVal();
  }
};

SVGTextContentElement::SVGTextContentElement(const QualifiedName& tag_name,
                                             Document& document)
    : SVGGraphicsElement(tag_name, document),
      text_length_(MakeGarbageCollected<SVGAnimatedTextLength>(this)),
      text_length_is_specified_by_user_(false),
      length_adjust_(
          MakeGarbageCollected<SVGAnimatedEnumeration<SVGLengthAdjustType>>(
              this,
              svg_names::kLengthAdjustAttr,
              kSVGLengthAdjustSpacing)) {
  AddToPropertyMap(text_length_);
  AddToPropertyMap(length_adjust_);
}

void SVGTextContentElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(text_length_);
  visitor->Trace(length_adjust_);
  SVGGraphicsElement::Trace(visitor);
}

unsigned SVGTextContentElement::getNumberOfChars() {
  GetDocument().UpdateStyleAndLayoutForNode(this);
  return SVGTextQuery(GetLayoutObject()).NumberOfCharacters();
}

float SVGTextContentElement::getComputedTextLength() {
  GetDocument().UpdateStyleAndLayoutForNode(this);
  return SVGTextQuery(GetLayoutObject()).TextLength();
}

float SVGTextContentElement::getSubStringLength(
    unsigned charnum,
    unsigned nchars,
    ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this);

  unsigned number_of_chars = getNumberOfChars();
  if (charnum >= number_of_chars) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("charnum", charnum,
                                                    getNumberOfChars()));
    return 0.0f;
  }

  if (nchars > number_of_chars - charnum)
    nchars = number_of_chars - charnum;

  return SVGTextQuery(GetLayoutObject()).SubStringLength(charnum, nchars);
}

SVGPointTearOff* SVGTextContentElement::getStartPositionOfChar(
    unsigned charnum,
    ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this);

  if (charnum >= getNumberOfChars()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("charnum", charnum,
                                                    getNumberOfChars()));
    return nullptr;
  }

  FloatPoint point =
      SVGTextQuery(GetLayoutObject()).StartPositionOfCharacter(charnum);
  return SVGPointTearOff::CreateDetached(point);
}

SVGPointTearOff* SVGTextContentElement::getEndPositionOfChar(
    unsigned charnum,
    ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this);

  if (charnum >= getNumberOfChars()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("charnum", charnum,
                                                    getNumberOfChars()));
    return nullptr;
  }

  FloatPoint point =
      SVGTextQuery(GetLayoutObject()).EndPositionOfCharacter(charnum);
  return SVGPointTearOff::CreateDetached(point);
}

SVGRectTearOff* SVGTextContentElement::getExtentOfChar(
    unsigned charnum,
    ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this);

  if (charnum >= getNumberOfChars()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("charnum", charnum,
                                                    getNumberOfChars()));
    return nullptr;
  }

  FloatRect rect = SVGTextQuery(GetLayoutObject()).ExtentOfCharacter(charnum);
  return SVGRectTearOff::CreateDetached(rect);
}

float SVGTextContentElement::getRotationOfChar(
    unsigned charnum,
    ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this);

  if (charnum >= getNumberOfChars()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("charnum", charnum,
                                                    getNumberOfChars()));
    return 0.0f;
  }

  return SVGTextQuery(GetLayoutObject()).RotationOfCharacter(charnum);
}

int SVGTextContentElement::getCharNumAtPosition(
    SVGPointTearOff* point,
    ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this);
  return SVGTextQuery(GetLayoutObject())
      .CharacterNumberAtPosition(point->Target()->Value());
}

void SVGTextContentElement::selectSubString(unsigned charnum,
                                            unsigned nchars,
                                            ExceptionState& exception_state) {
  unsigned number_of_chars = getNumberOfChars();
  if (charnum >= number_of_chars) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("charnum", charnum,
                                                    getNumberOfChars()));
    return;
  }

  if (nchars > number_of_chars - charnum)
    nchars = number_of_chars - charnum;

  DCHECK(GetDocument().GetFrame());
  GetDocument().GetFrame()->Selection().SelectSubString(*this, charnum, nchars);
}

bool SVGTextContentElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name.Matches(xml_names::kSpaceAttr))
    return true;
  return SVGGraphicsElement::IsPresentationAttribute(name);
}

void SVGTextContentElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name.Matches(xml_names::kSpaceAttr)) {
    DEFINE_STATIC_LOCAL(const AtomicString, preserve_string, ("preserve"));

    if (value == preserve_string) {
      UseCounter::Count(GetDocument(), WebFeature::kWhiteSpacePreFromXMLSpace);
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kWhiteSpace,
                                              CSSValueID::kPre);
    } else {
      UseCounter::Count(GetDocument(),
                        WebFeature::kWhiteSpaceNowrapFromXMLSpace);
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kWhiteSpace,
                                              CSSValueID::kNowrap);
    }
  } else {
    SVGGraphicsElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void SVGTextContentElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  if (attr_name == svg_names::kTextLengthAttr)
    text_length_is_specified_by_user_ = true;

  if (attr_name == svg_names::kTextLengthAttr ||
      attr_name == svg_names::kLengthAdjustAttr ||
      attr_name == xml_names::kSpaceAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    if (LayoutObject* layout_object = GetLayoutObject())
      MarkForLayoutAndParentResourceInvalidation(*layout_object);

    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(attr_name);
}

bool SVGTextContentElement::SelfHasRelativeLengths() const {
  // Any element of the <text> subtree is advertized as using relative lengths.
  // On any window size change, we have to relayout the text subtree, as the
  // effective 'on-screen' font size may change.
  return true;
}

SVGTextContentElement* SVGTextContentElement::ElementFromLineLayoutItem(
    const LineLayoutItem& line_layout_item) {
  if (!line_layout_item ||
      (!line_layout_item.IsSVGText() && !line_layout_item.IsSVGInline()))
    return nullptr;

  DCHECK(line_layout_item.GetNode());
  return DynamicTo<SVGTextContentElement>(line_layout_item.GetNode());
}

}  // namespace blink
