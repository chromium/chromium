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

#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"

namespace blink {

SVGLengthList::SVGLengthList(SVGLengthMode mode) : mode_(mode) {}

SVGLengthList::~SVGLengthList() = default;

SVGLengthList* SVGLengthList::Clone() const {
  auto* ret = MakeGarbageCollected<SVGLengthList>(mode_);
  ret->DeepCopy(this);
  return ret;
}

template <typename CharType>
SVGParsingError SVGLengthList::ParseInternal(
    const base::span<const CharType> chars) {
  for (size_t position = 0; position < chars.size();
       position = SkipOptionalSVGSpacesOrDelimiter(chars, position)) {
    // TODO(shanmuga.m): Enable calc for SVGLengthList
    auto token = TokenUntilSvgSpaceOrDelimiter(chars, position, ',');
    if (token.empty()) {
      break;
    }

    auto* length = MakeGarbageCollected<SVGLength>(mode_);
    SVGParsingError length_parse_status =
        length->SetValueAsString(String(token));
    if (length_parse_status != SVGParseStatus::kNoError)
      return length_parse_status.OffsetWith(position);
    position += token.size();
    Append(length);
  }
  return SVGParseStatus::kNoError;
}

SVGParsingError SVGLengthList::SetValueAsString(const String& value) {
  Clear();

  if (value.empty())
    return SVGParseStatus::kNoError;

  return VisitCharacters(value,
                         [&](auto chars) { return ParseInternal(chars); });
}

void SVGLengthList::Add(const SVGPropertyBase* other,
                        const SVGElement* context_element) {
  auto* other_list = To<SVGLengthList>(other);
  if (length() != other_list->length())
    return;
  for (uint32_t i = 0; i < length(); ++i)
    at(i)->Add(other_list->at(i), context_element);
}

SVGLength* SVGLengthList::CreatePaddingItem() const {
  return MakeGarbageCollected<SVGLength>(mode_);
}

void SVGLengthList::CalculateAnimatedValue(
    const SMILAnimationEffectParameters& parameters,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from_value,
    const SVGPropertyBase* to_value,
    const SVGPropertyBase* to_at_end_of_duration_value,
    const SVGElement* context_element) {
  auto* from_list = To<SVGLengthList>(from_value);
  auto* to_list = To<SVGLengthList>(to_value);

  if (!AdjustFromToListValues(from_list, to_list, percentage))
    return;

  auto* to_at_end_of_duration_list =
      To<SVGLengthList>(to_at_end_of_duration_value);

  uint32_t from_list_size = from_list->length();
  uint32_t to_list_size = to_list->length();
  uint32_t to_at_end_of_duration_list_size =
      to_at_end_of_duration_list->length();

  const bool needs_neutral_element =
      !from_list_size || to_list_size != to_at_end_of_duration_list_size;
  const SVGLength* neutral =
      needs_neutral_element ? CreatePaddingItem() : nullptr;
  for (uint32_t i = 0; i < to_list_size; ++i) {
    const SVGLength* from = from_list_size ? from_list->at(i) : neutral;
    const SVGLength* to_at_end = i < to_at_end_of_duration_list_size
                                     ? to_at_end_of_duration_list->at(i)
                                     : neutral;
    at(i)->CalculateAnimatedValue(parameters, percentage, repeat_count, from,
                                  to_list->at(i), to_at_end, context_element);
  }
}

float SVGLengthList::CalculateDistance(const SVGPropertyBase* to,
                                       const SVGElement*) const {
  // FIXME: Distance calculation is not possible for SVGLengthList right now. We
  // need the distance for every single value.
  return -1;
}
}  // namespace blink
