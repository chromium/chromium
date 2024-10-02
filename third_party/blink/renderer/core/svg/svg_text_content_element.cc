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
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_query.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg/svg_point_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_rect_tear_off.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

bool IsNGTextOrInline(const LayoutObject* object) {
  return object &&
         (object->IsSVGText() || object->IsInLayoutNGInlineFormattingContext());
}

}  // namespace

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
              kSVGLengthAdjustSpacing)) {}

void SVGTextContentElement::Trace(Visitor* visitor) const {
  visitor->Trace(text_length_);
  visitor->Trace(length_adjust_);
  SVGGraphicsElement::Trace(visitor);
}

unsigned SVGTextContentElement::getNumberOfChars() {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);
  auto* layout_object = GetLayoutObject();
  if (IsNGTextOrInline(layout_object))
    return SvgTextQuery(*layout_object).NumberOfCharacters();
  return 0;
}

float SVGTextContentElement::getComputedTextLength() {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);
  auto* layout_object = GetLayoutObject();
  if (IsNGTextOrInline(layout_object)) {
    SvgTextQuery query(*layout_object);
    return query.SubStringLength(0, query.NumberOfCharacters());
  }
  return 0;
}

float SVGTextContentElement::getSubStringLength(
    unsigned charnum,
    unsigned nchars,
    ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

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

  auto* layout_object = GetLayoutObject();
  if (IsNGTextOrInline(layout_object))
    return SvgTextQuery(*layout_object).SubStringLength(charnum, nchars);
  return 0;
}

SVGPointTearOff* SVGTextContentElement::getStartPositionOfChar(
    unsigned charnum,
    ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (charnum >= getNumberOfChars()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("charnum", charnum,
                                                    getNumberOfChars()));
    return nullptr;
  }

  gfx::PointF point;
  auto* layout_object = GetLayoutObject();
  if (IsNGTextOrInline(layout_object)) {
    point = SvgTextQuery(*layout_object).StartPositionOfCharacter(charnum);
  }
  return SVGPointTearOff::CreateDetached(point);
}

SVGPointTearOff* SVGTextContentElement::getEndPositionOfChar(
    unsigned charnum,
    ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (charnum >= getNumberOfChars()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("charnum", charnum,
                                                    getNumberOfChars()));
    return nullptr;
  }

  gfx::PointF point;
  auto* layout_object = GetLayoutObject();
  if (IsNGTextOrInline(layout_object)) {
    point = SvgTextQuery(*layout_object).EndPositionOfCharacter(charnum);
  }
  return SVGPointTearOff::CreateDetached(point);
}

SVGRectTearOff* SVGTextContentElement::getExtentOfChar(
    unsigned charnum,
    ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (charnum >= getNumberOfChars()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("charnum", charnum,
                                                    getNumberOfChars()));
    return nullptr;
  }

  gfx::RectF rect;
  auto* layout_object = GetLayoutObject();
  if (IsNGTextOrInline(layout_object)) {
    rect = SvgTextQuery(*layout_object).ExtentOfCharacter(charnum);
  }
  return SVGRectTearOff::CreateDetached(rect);
}

float SVGTextContentElement::getRotationOfChar(
    unsigned charnum,
    ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (charnum >= getNumberOfChars()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("charnum", charnum,
                                                    getNumberOfChars()));
    return 0.0f;
  }

  auto* layout_object = GetLayoutObject();
  if (IsNGTextOrInline(layout_object))
    return SvgTextQuery(*layout_object).RotationOfCharacter(charnum);
  return 0.0f;
}

int SVGTextContentElement::getCharNumAtPosition(
    SVGPointTearOff* point,
    ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);
  auto* layout_object = GetLayoutObject();
  if (IsNGTextOrInline(layout_object)) {
    return SvgTextQuery(*layout_object)
        .CharacterNumberAtPosition(point->Target()->Value());
  }
  return -1;
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
      // Longhands of `white-space: pre`.
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWhiteSpaceCollapse, CSSValueID::kPreserve);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kTextWrapMode, CSSValueID::kNowrap);
    } else {
      UseCounter::Count(GetDocument(),
                        WebFeature::kWhiteSpaceNowrapFromXMLSpace);
      // Longhands of `white-space: nowrap`.
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWhiteSpaceCollapse, CSSValueID::kCollapse);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kTextWrapMode, CSSValueID::kNowrap);
    }
  } else {
    SVGGraphicsElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void SVGTextContentElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kTextLengthAttr)
    text_length_is_specified_by_user_ = true;

  if (attr_name == svg_names::kTextLengthAttr ||
      attr_name == svg_names::kLengthAdjustAttr ||
      attr_name == xml_names::kSpaceAttr) {
    if (LayoutObject* layout_object = GetLayoutObject()) {
      if (auto* ng_text =
              LayoutSVGText::LocateLayoutSVGTextAncestor(layout_object)) {
        ng_text->SetNeedsPositioningValuesUpdate();
      }
      MarkForLayoutAndParentResourceInvalidation(*layout_object);
    }

    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(params);
}

bool SVGTextContentElement::SelfHasRelativeLengths() const {
  // Any element of the <text> subtree is advertized as using relative lengths.
  // On any window size change, we have to relayout the text subtree, as the
  // effective 'on-screen' font size may change.
  return true;
}

SVGTextContentElement* SVGTextContentElement::ElementFromLineLayoutItem(
    const LineLayoutItem& line_layout_item) {
  return nullptr;
}

SVGAnimatedPropertyBase* SVGTextContentElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == text_length_->AttributeName()) {
    return text_length_.Get();
  } else if (attribute_name == svg_names::kLengthAdjustAttr) {
    return length_adjust_.Get();
  } else {
    return SVGGraphicsElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGTextContentElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{text_length_.Get(), length_adjust_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGGraphicsElement::SynchronizeAllSVGAttributes();
}

}  // namespace blink
