/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_point_list.h"

#include "third_party/blink/renderer/core/svg/svg_animate_element.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

SVGPointList::SVGPointList() = default;

SVGPointList::~SVGPointList() = default;

String SVGPointList::ValueAsString() const {
  return SVGListPropertyHelper<SVGPointList, SVGPoint>::SerializeList();
}

template <typename CharType>
SVGParsingError SVGPointList::Parse(const CharType*& ptr, const CharType* end) {
  if (!SkipOptionalSVGSpaces(ptr, end))
    return SVGParseStatus::kNoError;

  const CharType* list_start = ptr;
  for (;;) {
    float x = 0;
    float y = 0;
    if (!ParseNumber(ptr, end, x) ||
        !ParseNumber(ptr, end, y, kDisallowWhitespace))
      return SVGParsingError(SVGParseStatus::kExpectedNumber, ptr - list_start);

    Append(MakeGarbageCollected<SVGPoint>(FloatPoint(x, y)));

    if (!SkipOptionalSVGSpaces(ptr, end))
      break;

    if (*ptr == ',') {
      ++ptr;
      SkipOptionalSVGSpaces(ptr, end);

      // ',' requires the list to be continued
      continue;
    }
  }
  return SVGParseStatus::kNoError;
}

SVGParsingError SVGPointList::SetValueAsString(const String& value) {
  Clear();

  if (value.IsEmpty())
    return SVGParseStatus::kNoError;

  if (value.Is8Bit()) {
    const LChar* ptr = value.Characters8();
    const LChar* end = ptr + value.length();
    return Parse(ptr, end);
  }
  const UChar* ptr = value.Characters16();
  const UChar* end = ptr + value.length();
  return Parse(ptr, end);
}

void SVGPointList::Add(SVGPropertyBase* other, SVGElement* context_element) {
  SVGPointList* other_list = ToSVGPointList(other);

  if (length() != other_list->length())
    return;

  for (uint32_t i = 0; i < length(); ++i)
    at(i)->SetValue(at(i)->Value() + other_list->at(i)->Value());
}

void SVGPointList::CalculateAnimatedValue(
    const SVGAnimateElement& animation_element,
    float percentage,
    unsigned repeat_count,
    SVGPropertyBase* from_value,
    SVGPropertyBase* to_value,
    SVGPropertyBase* to_at_end_of_duration_value,
    SVGElement* context_element) {
  SVGPointList* from_list = ToSVGPointList(from_value);
  SVGPointList* to_list = ToSVGPointList(to_value);
  SVGPointList* to_at_end_of_duration_list =
      ToSVGPointList(to_at_end_of_duration_value);

  uint32_t from_point_list_size = from_list->length();
  uint32_t to_point_list_size = to_list->length();
  uint32_t to_at_end_of_duration_list_size =
      to_at_end_of_duration_list->length();

  const bool is_to_animation =
      animation_element.GetAnimationMode() == kToAnimation;
  if (!AdjustFromToListValues(from_list, to_list, percentage, is_to_animation))
    return;

  for (uint32_t i = 0; i < to_point_list_size; ++i) {
    float animated_x = at(i)->X();
    float animated_y = at(i)->Y();

    FloatPoint effective_from;
    if (from_point_list_size)
      effective_from = from_list->at(i)->Value();
    FloatPoint effective_to = to_list->at(i)->Value();
    FloatPoint effective_to_at_end;
    if (i < to_at_end_of_duration_list_size)
      effective_to_at_end = to_at_end_of_duration_list->at(i)->Value();

    animation_element.AnimateAdditiveNumber(
        percentage, repeat_count, effective_from.X(), effective_to.X(),
        effective_to_at_end.X(), animated_x);
    animation_element.AnimateAdditiveNumber(
        percentage, repeat_count, effective_from.Y(), effective_to.Y(),
        effective_to_at_end.Y(), animated_y);
    at(i)->SetValue(FloatPoint(animated_x, animated_y));
  }
}

float SVGPointList::CalculateDistance(SVGPropertyBase* to, SVGElement*) {
  // FIXME: Distance calculation is not possible for SVGPointList right now. We
  // need the distance for every single value.
  return -1;
}

}  // namespace blink
