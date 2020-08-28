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

#include "third_party/blink/renderer/core/svg/svg_number_list.h"

#include "third_party/blink/renderer/core/svg/animation/smil_animation_effect_parameters.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

SVGNumberList::SVGNumberList() = default;

SVGNumberList::~SVGNumberList() = default;

template <typename CharType>
SVGParsingError SVGNumberList::Parse(const CharType*& ptr,
                                     const CharType* end) {
  const CharType* list_start = ptr;
  while (ptr < end) {
    float number = 0;
    if (!ParseNumber(ptr, end, number))
      return SVGParsingError(SVGParseStatus::kExpectedNumber, ptr - list_start);
    Append(MakeGarbageCollected<SVGNumber>(number));
  }
  return SVGParseStatus::kNoError;
}

SVGParsingError SVGNumberList::SetValueAsString(const String& value) {
  Clear();

  if (value.IsEmpty())
    return SVGParseStatus::kNoError;

  // Don't call |clear()| if an error is encountered. SVG policy is to use
  // valid items before error.
  // Spec: http://www.w3.org/TR/SVG/single-page.html#implnote-ErrorProcessing
  if (value.Is8Bit()) {
    const LChar* ptr = value.Characters8();
    const LChar* end = ptr + value.length();
    return Parse(ptr, end);
  }
  const UChar* ptr = value.Characters16();
  const UChar* end = ptr + value.length();
  return Parse(ptr, end);
}

void SVGNumberList::Add(SVGPropertyBase* other, SVGElement* context_element) {
  auto* other_list = To<SVGNumberList>(other);

  if (length() != other_list->length())
    return;

  for (uint32_t i = 0; i < length(); ++i)
    at(i)->SetValue(at(i)->Value() + other_list->at(i)->Value());
}

void SVGNumberList::CalculateAnimatedValue(
    const SMILAnimationEffectParameters& parameters,
    float percentage,
    unsigned repeat_count,
    SVGPropertyBase* from_value,
    SVGPropertyBase* to_value,
    SVGPropertyBase* to_at_end_of_duration_value,
    SVGElement* context_element) {
  auto* from_list = To<SVGNumberList>(from_value);
  auto* to_list = To<SVGNumberList>(to_value);
  auto* to_at_end_of_duration_list =
      To<SVGNumberList>(to_at_end_of_duration_value);

  uint32_t from_list_size = from_list->length();
  uint32_t to_list_size = to_list->length();
  uint32_t to_at_end_of_duration_list_size =
      to_at_end_of_duration_list->length();

  if (!AdjustFromToListValues(from_list, to_list, percentage,
                              parameters.is_to_animation))
    return;

  for (uint32_t i = 0; i < to_list_size; ++i) {
    float effective_from = from_list_size ? from_list->at(i)->Value() : 0;
    float effective_to = to_list_size ? to_list->at(i)->Value() : 0;
    float effective_to_at_end = i < to_at_end_of_duration_list_size
                                    ? to_at_end_of_duration_list->at(i)->Value()
                                    : 0;

    float animated = at(i)->Value();
    AnimateAdditiveNumber(parameters, percentage, repeat_count, effective_from,
                          effective_to, effective_to_at_end, animated);
    at(i)->SetValue(animated);
  }
}

float SVGNumberList::CalculateDistance(SVGPropertyBase* to, SVGElement*) {
  // FIXME: Distance calculation is not possible for SVGNumberList right now. We
  // need the distance for every single value.
  return -1;
}

Vector<float> SVGNumberList::ToFloatVector() const {
  Vector<float> vec;
  vec.ReserveInitialCapacity(length());
  for (uint32_t i = 0; i < length(); ++i)
    vec.UncheckedAppend(at(i)->Value());
  return vec;
}

}  // namespace blink
