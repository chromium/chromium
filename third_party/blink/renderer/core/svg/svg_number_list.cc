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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/svg/svg_number_list.h"

#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
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

  if (value.empty())
    return SVGParseStatus::kNoError;

  // Don't call |clear()| if an error is encountered. SVG policy is to use
  // valid items before error.
  // Spec: http://www.w3.org/TR/SVG/single-page.html#implnote-ErrorProcessing
  return WTF::VisitCharacters(value, [&](auto chars) {
    const auto* start = chars.data();
    return Parse(start, start + chars.size());
  });
}

void SVGNumberList::Add(const SVGPropertyBase* other,
                        const SVGElement* context_element) {
  auto* other_list = To<SVGNumberList>(other);
  if (length() != other_list->length())
    return;
  for (uint32_t i = 0; i < length(); ++i)
    at(i)->Add(other_list->at(i), context_element);
}

void SVGNumberList::CalculateAnimatedValue(
    const SMILAnimationEffectParameters& parameters,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from_value,
    const SVGPropertyBase* to_value,
    const SVGPropertyBase* to_at_end_of_duration_value,
    const SVGElement* context_element) {
  auto* from_list = To<SVGNumberList>(from_value);
  auto* to_list = To<SVGNumberList>(to_value);

  if (!AdjustFromToListValues(from_list, to_list, percentage))
    return;

  auto* to_at_end_of_duration_list =
      To<SVGNumberList>(to_at_end_of_duration_value);

  uint32_t from_list_size = from_list->length();
  uint32_t to_list_size = to_list->length();
  uint32_t to_at_end_of_duration_list_size =
      to_at_end_of_duration_list->length();

  const bool needs_neutral_element =
      !from_list_size || to_list_size != to_at_end_of_duration_list_size;
  const SVGNumber* neutral =
      needs_neutral_element ? CreatePaddingItem() : nullptr;
  for (uint32_t i = 0; i < to_list_size; ++i) {
    const SVGNumber* from = from_list_size ? from_list->at(i) : neutral;
    const SVGNumber* to_at_end = i < to_at_end_of_duration_list_size
                                     ? to_at_end_of_duration_list->at(i)
                                     : neutral;
    at(i)->CalculateAnimatedValue(parameters, percentage, repeat_count, from,
                                  to_list->at(i), to_at_end, context_element);
  }
}

float SVGNumberList::CalculateDistance(const SVGPropertyBase* to,
                                       const SVGElement*) const {
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
