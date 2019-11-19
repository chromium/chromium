/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2012. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_transform_list.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/svg/svg_animate_element.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/core/svg/svg_transform_distance.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// These should be kept in sync with enum SVGTransformType
const unsigned kRequiredValuesForType[] = {0, 6, 1, 1, 1, 1, 1};
const unsigned kOptionalValuesForType[] = {0, 0, 1, 1, 2, 0, 0};
static_assert(static_cast<int>(SVGTransformType::kUnknown) == 0,
              "index of SVGTransformType::kUnknown has changed");
static_assert(static_cast<int>(SVGTransformType::kMatrix) == 1,
              "index of SVGTransformType::kMatrix has changed");
static_assert(static_cast<int>(SVGTransformType::kTranslate) == 2,
              "index of SVGTransformType::kTranslate has changed");
static_assert(static_cast<int>(SVGTransformType::kScale) == 3,
              "index of SVGTransformType::kScale has changed");
static_assert(static_cast<int>(SVGTransformType::kRotate) == 4,
              "index of SVGTransformType::kRotate has changed");
static_assert(static_cast<int>(SVGTransformType::kSkewx) == 5,
              "index of SVGTransformType::kSkewx has changed");
static_assert(static_cast<int>(SVGTransformType::kSkewy) == 6,
              "index of SVGTransformType::kSkewy has changed");
static_assert(base::size(kRequiredValuesForType) - 1 ==
                  static_cast<int>(SVGTransformType::kSkewy),
              "the number of transform types have changed");
static_assert(base::size(kRequiredValuesForType) ==
                  base::size(kOptionalValuesForType),
              "the arrays should have the same number of elements");

const unsigned kMaxTransformArguments = 6;

using TransformArguments = Vector<float, kMaxTransformArguments>;

template <typename CharType>
SVGParseStatus ParseTransformArgumentsForType(SVGTransformType type,
                                              const CharType*& ptr,
                                              const CharType* end,
                                              TransformArguments& arguments) {
  const size_t required = kRequiredValuesForType[static_cast<int>(type)];
  const size_t optional = kOptionalValuesForType[static_cast<int>(type)];
  const size_t required_with_optional = required + optional;
  DCHECK_LE(required_with_optional, kMaxTransformArguments);
  DCHECK(arguments.IsEmpty());

  bool trailing_delimiter = false;

  while (arguments.size() < required_with_optional) {
    float argument_value = 0;
    if (!ParseNumber(ptr, end, argument_value, kAllowLeadingWhitespace))
      break;

    arguments.push_back(argument_value);
    trailing_delimiter = false;

    if (arguments.size() == required_with_optional)
      break;

    if (SkipOptionalSVGSpaces(ptr, end) && *ptr == ',') {
      ++ptr;
      trailing_delimiter = true;
    }
  }

  if (arguments.size() != required &&
      arguments.size() != required_with_optional)
    return SVGParseStatus::kExpectedNumber;
  if (trailing_delimiter)
    return SVGParseStatus::kTrailingGarbage;

  return SVGParseStatus::kNoError;
}

SVGTransform* CreateTransformFromValues(SVGTransformType type,
                                        const TransformArguments& arguments) {
  auto* transform = MakeGarbageCollected<SVGTransform>();
  switch (type) {
    case SVGTransformType::kSkewx:
      transform->SetSkewX(arguments[0]);
      break;
    case SVGTransformType::kSkewy:
      transform->SetSkewY(arguments[0]);
      break;
    case SVGTransformType::kScale:
      // Spec: if only one param given, assume uniform scaling.
      if (arguments.size() == 1)
        transform->SetScale(arguments[0], arguments[0]);
      else
        transform->SetScale(arguments[0], arguments[1]);
      break;
    case SVGTransformType::kTranslate:
      // Spec: if only one param given, assume 2nd param to be 0.
      if (arguments.size() == 1)
        transform->SetTranslate(arguments[0], 0);
      else
        transform->SetTranslate(arguments[0], arguments[1]);
      break;
    case SVGTransformType::kRotate:
      if (arguments.size() == 1)
        transform->SetRotate(arguments[0], 0, 0);
      else
        transform->SetRotate(arguments[0], arguments[1], arguments[2]);
      break;
    case SVGTransformType::kMatrix:
      transform->SetMatrix(AffineTransform(arguments[0], arguments[1],
                                           arguments[2], arguments[3],
                                           arguments[4], arguments[5]));
      break;
    case SVGTransformType::kUnknown:
      NOTREACHED();
      break;
  }
  return transform;
}

}  // namespace

SVGTransformList::SVGTransformList() = default;

SVGTransformList::SVGTransformList(SVGTransformType transform_type,
                                   const String& value) {
  TransformArguments arguments;
  bool at_end_of_value = false;
  SVGParseStatus status = SVGParseStatus::kParsingFailed;
  if (value.IsEmpty()) {
  } else if (value.Is8Bit()) {
    const LChar* ptr = value.Characters8();
    const LChar* end = ptr + value.length();
    status =
        ParseTransformArgumentsForType(transform_type, ptr, end, arguments);
    at_end_of_value = !SkipOptionalSVGSpaces(ptr, end);
  } else {
    const UChar* ptr = value.Characters16();
    const UChar* end = ptr + value.length();
    status =
        ParseTransformArgumentsForType(transform_type, ptr, end, arguments);
    at_end_of_value = !SkipOptionalSVGSpaces(ptr, end);
  }

  if (at_end_of_value && status == SVGParseStatus::kNoError)
    Append(CreateTransformFromValues(transform_type, arguments));
}

SVGTransformList::~SVGTransformList() = default;

SVGTransform* SVGTransformList::Consolidate() {
  AffineTransform matrix;
  if (!Concatenate(matrix))
    return nullptr;

  return Initialize(MakeGarbageCollected<SVGTransform>(matrix));
}

bool SVGTransformList::Concatenate(AffineTransform& result) const {
  if (IsEmpty())
    return false;

  ConstIterator it = begin();
  ConstIterator it_end = end();
  for (; it != it_end; ++it)
    result *= it->Matrix();

  return true;
}

namespace {

CSSValueID MapTransformFunction(const SVGTransform& transform) {
  switch (transform.TransformType()) {
    case SVGTransformType::kMatrix:
      return CSSValueID::kMatrix;
    case SVGTransformType::kTranslate:
      return CSSValueID::kTranslate;
    case SVGTransformType::kScale:
      return CSSValueID::kScale;
    case SVGTransformType::kRotate:
      return CSSValueID::kRotate;
    case SVGTransformType::kSkewx:
      return CSSValueID::kSkewX;
    case SVGTransformType::kSkewy:
      return CSSValueID::kSkewY;
    case SVGTransformType::kUnknown:
    default:
      NOTREACHED();
  }
  return CSSValueID::kInvalid;
}

CSSValue* CreateTransformCSSValue(const SVGTransform& transform) {
  CSSValueID function_id = MapTransformFunction(transform);
  CSSFunctionValue* transform_value =
      MakeGarbageCollected<CSSFunctionValue>(function_id);
  switch (function_id) {
    case CSSValueID::kRotate: {
      transform_value->Append(*CSSNumericLiteralValue::Create(
          transform.Angle(), CSSPrimitiveValue::UnitType::kDegrees));
      FloatPoint rotation_origin = transform.RotationCenter();
      if (!ToFloatSize(rotation_origin).IsZero()) {
        transform_value->Append(*CSSNumericLiteralValue::Create(
            rotation_origin.X(), CSSPrimitiveValue::UnitType::kUserUnits));
        transform_value->Append(*CSSNumericLiteralValue::Create(
            rotation_origin.Y(), CSSPrimitiveValue::UnitType::kUserUnits));
      }
      break;
    }
    case CSSValueID::kSkewX:
    case CSSValueID::kSkewY:
      transform_value->Append(*CSSNumericLiteralValue::Create(
          transform.Angle(), CSSPrimitiveValue::UnitType::kDegrees));
      break;
    case CSSValueID::kMatrix:
      transform_value->Append(*CSSNumericLiteralValue::Create(
          transform.Matrix().A(), CSSPrimitiveValue::UnitType::kUserUnits));
      transform_value->Append(*CSSNumericLiteralValue::Create(
          transform.Matrix().B(), CSSPrimitiveValue::UnitType::kUserUnits));
      transform_value->Append(*CSSNumericLiteralValue::Create(
          transform.Matrix().C(), CSSPrimitiveValue::UnitType::kUserUnits));
      transform_value->Append(*CSSNumericLiteralValue::Create(
          transform.Matrix().D(), CSSPrimitiveValue::UnitType::kUserUnits));
      transform_value->Append(*CSSNumericLiteralValue::Create(
          transform.Matrix().E(), CSSPrimitiveValue::UnitType::kUserUnits));
      transform_value->Append(*CSSNumericLiteralValue::Create(
          transform.Matrix().F(), CSSPrimitiveValue::UnitType::kUserUnits));
      break;
    case CSSValueID::kScale:
      transform_value->Append(*CSSNumericLiteralValue::Create(
          transform.Matrix().A(), CSSPrimitiveValue::UnitType::kUserUnits));
      transform_value->Append(*CSSNumericLiteralValue::Create(
          transform.Matrix().D(), CSSPrimitiveValue::UnitType::kUserUnits));
      break;
    case CSSValueID::kTranslate:
      transform_value->Append(*CSSNumericLiteralValue::Create(
          transform.Matrix().E(), CSSPrimitiveValue::UnitType::kUserUnits));
      transform_value->Append(*CSSNumericLiteralValue::Create(
          transform.Matrix().F(), CSSPrimitiveValue::UnitType::kUserUnits));
      break;
    default:
      NOTREACHED();
  }
  return transform_value;
}

}  // namespace

const CSSValue* SVGTransformList::CssValue() const {
  // Build a structure of CSSValues from the list we have, mapping functions as
  // appropriate.
  // TODO(fs): Eventually we'd want to support the exact same syntax here as in
  // the property, but there are some issues (crbug.com/577219 for instance)
  // that complicates things.
  size_t length = this->length();
  if (!length)
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (length == 1) {
    list->Append(*CreateTransformCSSValue(*at(0)));
    return list;
  }
  ConstIterator it = begin();
  ConstIterator it_end = end();
  for (; it != it_end; ++it)
    list->Append(*CreateTransformCSSValue(**it));
  return list;
}

namespace {

template <typename CharType>
SVGTransformType ParseAndSkipTransformType(const CharType*& ptr,
                                           const CharType* end) {
  if (ptr >= end)
    return SVGTransformType::kUnknown;

  if (*ptr == 's') {
    if (SkipToken(ptr, end, "skewX"))
      return SVGTransformType::kSkewx;
    if (SkipToken(ptr, end, "skewY"))
      return SVGTransformType::kSkewy;
    if (SkipToken(ptr, end, "scale"))
      return SVGTransformType::kScale;

    return SVGTransformType::kUnknown;
  }
  if (SkipToken(ptr, end, "translate"))
    return SVGTransformType::kTranslate;
  if (SkipToken(ptr, end, "rotate"))
    return SVGTransformType::kRotate;
  if (SkipToken(ptr, end, "matrix"))
    return SVGTransformType::kMatrix;

  return SVGTransformType::kUnknown;
}

}  // namespace

template <typename CharType>
SVGParsingError SVGTransformList::ParseInternal(const CharType*& ptr,
                                                const CharType* end) {
  Clear();

  const CharType* start = ptr;
  bool delim_parsed = false;
  while (SkipOptionalSVGSpaces(ptr, end)) {
    delim_parsed = false;

    SVGTransformType transform_type = ParseAndSkipTransformType(ptr, end);
    if (transform_type == SVGTransformType::kUnknown)
      return SVGParsingError(SVGParseStatus::kExpectedTransformFunction,
                             ptr - start);

    if (!SkipOptionalSVGSpaces(ptr, end) || *ptr != '(')
      return SVGParsingError(SVGParseStatus::kExpectedStartOfArguments,
                             ptr - start);
    ptr++;

    TransformArguments arguments;
    SVGParseStatus status =
        ParseTransformArgumentsForType(transform_type, ptr, end, arguments);
    if (status != SVGParseStatus::kNoError)
      return SVGParsingError(status, ptr - start);
    DCHECK_GE(arguments.size(),
              kRequiredValuesForType[static_cast<int>(transform_type)]);

    if (!SkipOptionalSVGSpaces(ptr, end) || *ptr != ')')
      return SVGParsingError(SVGParseStatus::kExpectedEndOfArguments,
                             ptr - start);
    ptr++;

    Append(CreateTransformFromValues(transform_type, arguments));

    if (SkipOptionalSVGSpaces(ptr, end) && *ptr == ',') {
      ++ptr;
      delim_parsed = true;
    }
  }
  if (delim_parsed)
    return SVGParsingError(SVGParseStatus::kTrailingGarbage, ptr - start);
  return SVGParseStatus::kNoError;
}

bool SVGTransformList::Parse(const UChar*& ptr, const UChar* end) {
  return ParseInternal(ptr, end) == SVGParseStatus::kNoError;
}

bool SVGTransformList::Parse(const LChar*& ptr, const LChar* end) {
  return ParseInternal(ptr, end) == SVGParseStatus::kNoError;
}

SVGTransformType ParseTransformType(const String& string) {
  if (string.IsEmpty())
    return SVGTransformType::kUnknown;
  if (string.Is8Bit()) {
    const LChar* ptr = string.Characters8();
    const LChar* end = ptr + string.length();
    return ParseAndSkipTransformType(ptr, end);
  }
  const UChar* ptr = string.Characters16();
  const UChar* end = ptr + string.length();
  return ParseAndSkipTransformType(ptr, end);
}

String SVGTransformList::ValueAsString() const {
  return SVGListPropertyHelper<SVGTransformList, SVGTransform>::SerializeList();
}

SVGParsingError SVGTransformList::SetValueAsString(const String& value) {
  if (value.IsEmpty()) {
    Clear();
    return SVGParseStatus::kNoError;
  }

  SVGParsingError parse_error;
  if (value.Is8Bit()) {
    const LChar* ptr = value.Characters8();
    const LChar* end = ptr + value.length();
    parse_error = ParseInternal(ptr, end);
  } else {
    const UChar* ptr = value.Characters16();
    const UChar* end = ptr + value.length();
    parse_error = ParseInternal(ptr, end);
  }

  if (parse_error != SVGParseStatus::kNoError)
    Clear();

  return parse_error;
}

SVGPropertyBase* SVGTransformList::CloneForAnimation(
    const String& value) const {
  DCHECK(RuntimeEnabledFeatures::WebAnimationsSVGEnabled());
  return SVGListPropertyHelper::CloneForAnimation(value);
}

void SVGTransformList::Add(SVGPropertyBase* other,
                           SVGElement* context_element) {
  if (IsEmpty())
    return;

  SVGTransformList* other_list = ToSVGTransformList(other);
  if (length() != other_list->length())
    return;

  DCHECK_EQ(length(), 1u);
  SVGTransform* from_transform = at(0);
  SVGTransform* to_transform = other_list->at(0);

  DCHECK_EQ(from_transform->TransformType(), to_transform->TransformType());
  Initialize(
      SVGTransformDistance::AddSVGTransforms(from_transform, to_transform));
}

void SVGTransformList::CalculateAnimatedValue(
    const SVGAnimateElement& animation_element,
    float percentage,
    unsigned repeat_count,
    SVGPropertyBase* from_value,
    SVGPropertyBase* to_value,
    SVGPropertyBase* to_at_end_of_duration_value,
    SVGElement* context_element) {
  // Spec: To animations provide specific functionality to get a smooth change
  // from the underlying value to the 'to' attribute value, which conflicts
  // mathematically with the requirement for additive transform animations to be
  // post-multiplied. As a consequence, in SVG 1.1 the behavior of to animations
  // for 'animateTransform' is undefined.
  // FIXME: This is not taken into account yet.
  SVGTransformList* from_list = ToSVGTransformList(from_value);
  SVGTransformList* to_list = ToSVGTransformList(to_value);
  SVGTransformList* to_at_end_of_duration_list =
      ToSVGTransformList(to_at_end_of_duration_value);

  size_t to_list_size = to_list->length();
  if (!to_list_size)
    return;

  // Get a reference to the from value before potentially cleaning it out (in
  // the case of a To animation.)
  SVGTransform* to_transform = to_list->at(0);
  SVGTransform* effective_from = nullptr;
  // If there's an existing 'from'/underlying value of the same type use that,
  // else use a "zero transform".
  if (from_list->length() &&
      from_list->at(0)->TransformType() == to_transform->TransformType())
    effective_from = from_list->at(0);
  else
    effective_from = MakeGarbageCollected<SVGTransform>(
        to_transform->TransformType(), SVGTransform::kConstructZeroTransform);

  SVGTransform* current_transform =
      SVGTransformDistance(effective_from, to_transform)
          .ScaledDistance(percentage)
          .AddToSVGTransform(effective_from);
  if (animation_element.GetAnimationMode() == kToAnimation) {
    Initialize(current_transform);
    return;
  }
  // Never resize the animatedTransformList to the toList size, instead either
  // clear the list or append to it.
  if (!IsEmpty() && !animation_element.IsAdditive())
    Clear();

  if (repeat_count && animation_element.IsAccumulated()) {
    SVGTransform* effective_to_at_end =
        !to_at_end_of_duration_list->IsEmpty()
            ? to_at_end_of_duration_list->at(0)
            : MakeGarbageCollected<SVGTransform>(
                  to_transform->TransformType(),
                  SVGTransform::kConstructZeroTransform);
    Append(SVGTransformDistance::AddSVGTransforms(
        current_transform, effective_to_at_end, repeat_count));
  } else {
    Append(current_transform);
  }
}

float SVGTransformList::CalculateDistance(SVGPropertyBase* to_value,
                                          SVGElement*) {
  // FIXME: This is not correct in all cases. The spec demands that each
  // component (translate x and y for example) is paced separately. To implement
  // this we need to treat each component as individual animation everywhere.

  SVGTransformList* to_list = ToSVGTransformList(to_value);
  if (IsEmpty() || length() != to_list->length())
    return -1;

  DCHECK_EQ(length(), 1u);
  if (at(0)->TransformType() == to_list->at(0)->TransformType())
    return -1;

  // Spec: http://www.w3.org/TR/SVG/animate.html#complexDistances
  // Paced animations assume a notion of distance between the various animation
  // values defined by the 'to', 'from', 'by' and 'values' attributes.  Distance
  // is defined only for scalar types (such as <length>), colors and the subset
  // of transformation types that are supported by 'animateTransform'.
  return SVGTransformDistance(at(0), to_list->at(0)).Distance();
}

}  // namespace blink
