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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/svg/svg_point_list.h"

#include "third_party/blink/renderer/core/svg/animation/smil_animation_effect_parameters.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

SVGPointList::SVGPointList() = default;

SVGPointList::~SVGPointList() = default;

template <typename CharType>
SVGParsingError SVGPointList::Parse(const CharType* ptr, const CharType* end) {
  if (!SkipOptionalSVGSpaces(ptr, end))
    return SVGParseStatus::kNoError;

  const CharType* list_start = ptr;
  for (;;) {
    float x = 0;
    float y = 0;
    if (!ParseNumber(ptr, end, x) ||
        !ParseNumber(ptr, end, y, kDisallowWhitespace))
      return SVGParsingError(SVGParseStatus::kExpectedNumber, ptr - list_start);

    Append(MakeGarbageCollected<SVGPoint>(gfx::PointF(x, y)));

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

  if (value.empty())
    return SVGParseStatus::kNoError;

  return WTF::VisitCharacters(value, [&](auto chars) {
    return Parse(chars.data(), chars.data() + chars.size());
  });
}

void SVGPointList::Add(const SVGPropertyBase* other,
                       const SVGElement* context_element) {
  auto* other_list = To<SVGPointList>(other);

  if (length() != other_list->length())
    return;

  for (uint32_t i = 0; i < length(); ++i) {
    at(i)->SetValue(at(i)->Value() +
                    other_list->at(i)->Value().OffsetFromOrigin());
  }
}

void SVGPointList::CalculateAnimatedValue(
    const SMILAnimationEffectParameters& parameters,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from_value,
    const SVGPropertyBase* to_value,
    const SVGPropertyBase* to_at_end_of_duration_value,
    const SVGElement* context_element) {
  auto* from_list = To<SVGPointList>(from_value);
  auto* to_list = To<SVGPointList>(to_value);

  if (!AdjustFromToListValues(from_list, to_list, percentage))
    return;

  auto* to_at_end_of_duration_list =
      To<SVGPointList>(to_at_end_of_duration_value);

  uint32_t from_point_list_size = from_list->length();
  uint32_t to_point_list_size = to_list->length();
  uint32_t to_at_end_of_duration_list_size =
      to_at_end_of_duration_list->length();

  for (uint32_t i = 0; i < to_point_list_size; ++i) {
    gfx::PointF effective_from;
    if (from_point_list_size)
      effective_from = from_list->at(i)->Value();
    gfx::PointF effective_to = to_list->at(i)->Value();
    gfx::PointF effective_to_at_end;
    if (i < to_at_end_of_duration_list_size)
      effective_to_at_end = to_at_end_of_duration_list->at(i)->Value();

    gfx::PointF result(
        ComputeAnimatedNumber(parameters, percentage, repeat_count,
                              effective_from.x(), effective_to.x(),
                              effective_to_at_end.x()),
        ComputeAnimatedNumber(parameters, percentage, repeat_count,
                              effective_from.y(), effective_to.y(),
                              effective_to_at_end.y()));
    if (parameters.is_additive)
      result += at(i)->Value().OffsetFromOrigin();

    at(i)->SetValue(result);
  }
}

float SVGPointList::CalculateDistance(const SVGPropertyBase* to,
                                      const SVGElement*) const {
  // FIXME: Distance calculation is not possible for SVGPointList right now. We
  // need the distance for every single value.
  return -1;
}

}  // namespace blink
