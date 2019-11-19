/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_length_list.h"

#include "third_party/blink/renderer/core/svg/svg_animate_element.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

SVGLengthList::SVGLengthList(SVGLengthMode mode) : mode_(mode) {}

SVGLengthList::~SVGLengthList() = default;

SVGLengthList* SVGLengthList::Clone() {
  auto* ret = MakeGarbageCollected<SVGLengthList>(mode_);
  ret->DeepCopy(this);
  return ret;
}

SVGPropertyBase* SVGLengthList::CloneForAnimation(const String& value) const {
  auto* ret = MakeGarbageCollected<SVGLengthList>(mode_);
  ret->SetValueAsString(value);
  return ret;
}

String SVGLengthList::ValueAsString() const {
  return SVGListPropertyHelper<SVGLengthList, SVGLength>::SerializeList();
}

template <typename CharType>
SVGParsingError SVGLengthList::ParseInternal(const CharType*& ptr,
                                             const CharType* end) {
  const CharType* list_start = ptr;
  while (ptr < end) {
    const CharType* start = ptr;
    // TODO(shanmuga.m): Enable calc for SVGLengthList
    while (ptr < end && *ptr != ',' && !IsHTMLSpace<CharType>(*ptr))
      ptr++;
    if (ptr == start)
      break;
    String value_string(start, static_cast<wtf_size_t>(ptr - start));
    if (value_string.IsEmpty())
      break;

    auto* length = MakeGarbageCollected<SVGLength>(mode_);
    SVGParsingError length_parse_status =
        length->SetValueAsString(value_string);
    if (length_parse_status != SVGParseStatus::kNoError)
      return length_parse_status.OffsetWith(start - list_start);
    Append(length);
    SkipOptionalSVGSpacesOrDelimiter(ptr, end);
  }
  return SVGParseStatus::kNoError;
}

SVGParsingError SVGLengthList::SetValueAsString(const String& value) {
  Clear();

  if (value.IsEmpty())
    return SVGParseStatus::kNoError;

  if (value.Is8Bit()) {
    const LChar* ptr = value.Characters8();
    const LChar* end = ptr + value.length();
    return ParseInternal(ptr, end);
  }
  const UChar* ptr = value.Characters16();
  const UChar* end = ptr + value.length();
  return ParseInternal(ptr, end);
}

void SVGLengthList::Add(SVGPropertyBase* other, SVGElement* context_element) {
  SVGLengthList* other_list = ToSVGLengthList(other);

  if (length() != other_list->length())
    return;

  SVGLengthContext length_context(context_element);
  for (uint32_t i = 0; i < length(); ++i)
    at(i)->SetValue(
        at(i)->Value(length_context) + other_list->at(i)->Value(length_context),
        length_context);
}

SVGLength* SVGLengthList::CreatePaddingItem() const {
  return MakeGarbageCollected<SVGLength>(mode_);
}

void SVGLengthList::CalculateAnimatedValue(
    const SVGAnimateElement& animation_element,
    float percentage,
    unsigned repeat_count,
    SVGPropertyBase* from_value,
    SVGPropertyBase* to_value,
    SVGPropertyBase* to_at_end_of_duration_value,
    SVGElement* context_element) {
  SVGLengthList* from_list = ToSVGLengthList(from_value);
  SVGLengthList* to_list = ToSVGLengthList(to_value);
  SVGLengthList* to_at_end_of_duration_list =
      ToSVGLengthList(to_at_end_of_duration_value);

  SVGLengthContext length_context(context_element);
  DCHECK_EQ(mode_, SVGLength::LengthModeForAnimatedLengthAttribute(
                       animation_element.AttributeName()));

  uint32_t from_length_list_size = from_list->length();
  uint32_t to_length_list_size = to_list->length();
  uint32_t to_at_end_of_duration_list_size =
      to_at_end_of_duration_list->length();

  const bool is_to_animation =
      animation_element.GetAnimationMode() == kToAnimation;
  if (!AdjustFromToListValues(from_list, to_list, percentage, is_to_animation))
    return;

  for (uint32_t i = 0; i < to_length_list_size; ++i) {
    // TODO(shanmuga.m): Support calc for SVGLengthList animation
    float animated_number = at(i)->Value(length_context);
    const SVGLength* length_for_unit_type = to_list->at(i);
    float effective_from = 0;
    if (from_length_list_size) {
      if (percentage < 0.5)
        length_for_unit_type = from_list->at(i);
      effective_from = from_list->at(i)->Value(length_context);
    }
    float effective_to = to_list->at(i)->Value(length_context);
    float effective_to_at_end =
        i < to_at_end_of_duration_list_size
            ? to_at_end_of_duration_list->at(i)->Value(length_context)
            : 0;

    animation_element.AnimateAdditiveNumber(
        percentage, repeat_count, effective_from, effective_to,
        effective_to_at_end, animated_number);
    // |animated_number| is in user units.
    CSSPrimitiveValue::UnitType unit_type =
        length_for_unit_type->IsCalculated()
            ? CSSPrimitiveValue::UnitType::kUserUnits
            : length_for_unit_type->NumericLiteralType();
    at(i)->SetUnitType(unit_type);
    at(i)->SetValue(animated_number, length_context);
  }
}

float SVGLengthList::CalculateDistance(SVGPropertyBase* to, SVGElement*) {
  // FIXME: Distance calculation is not possible for SVGLengthList right now. We
  // need the distance for every single value.
  return -1;
}
}  // namespace blink
