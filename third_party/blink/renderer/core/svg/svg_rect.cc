/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_rect.h"

#include "third_party/blink/renderer/core/svg/animation/smil_animation_effect_parameters.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

SVGRect::SVGRect() : is_valid_(true) {}

SVGRect::SVGRect(const FloatRect& rect) : is_valid_(true), value_(rect) {}

SVGRect* SVGRect::Clone() const {
  return MakeGarbageCollected<SVGRect>(value_);
}

template <typename CharType>
SVGParsingError SVGRect::Parse(const CharType*& ptr, const CharType* end) {
  const CharType* start = ptr;
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
  if (!ParseNumber(ptr, end, x) || !ParseNumber(ptr, end, y) ||
      !ParseNumber(ptr, end, width) ||
      !ParseNumber(ptr, end, height, kDisallowWhitespace))
    return SVGParsingError(SVGParseStatus::kExpectedNumber, ptr - start);

  if (SkipOptionalSVGSpaces(ptr, end)) {
    // Nothing should come after the last, fourth number.
    return SVGParsingError(SVGParseStatus::kTrailingGarbage, ptr - start);
  }

  value_ = FloatRect(x, y, width, height);
  is_valid_ = true;
  return SVGParseStatus::kNoError;
}

SVGParsingError SVGRect::SetValueAsString(const String& string) {
  SetInvalid();

  if (string.IsNull())
    return SVGParseStatus::kNoError;

  if (string.IsEmpty())
    return SVGParsingError(SVGParseStatus::kExpectedNumber, 0);

  return WTF::VisitCharacters(string, [&](const auto* chars, unsigned length) {
    return Parse(chars, chars + length);
  });
}

String SVGRect::ValueAsString() const {
  StringBuilder builder;
  builder.AppendNumber(X());
  builder.Append(' ');
  builder.AppendNumber(Y());
  builder.Append(' ');
  builder.AppendNumber(Width());
  builder.Append(' ');
  builder.AppendNumber(Height());
  return builder.ToString();
}

void SVGRect::Add(const SVGPropertyBase* other, const SVGElement*) {
  value_ += To<SVGRect>(other)->Value();
}

void SVGRect::CalculateAnimatedValue(
    const SMILAnimationEffectParameters& parameters,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from_value,
    const SVGPropertyBase* to_value,
    const SVGPropertyBase* to_at_end_of_duration_value,
    const SVGElement*) {
  auto* from_rect = To<SVGRect>(from_value);
  auto* to_rect = To<SVGRect>(to_value);
  auto* to_at_end_of_duration_rect = To<SVGRect>(to_at_end_of_duration_value);

  float animated_x = X();
  float animated_y = Y();
  float animated_width = Width();
  float animated_height = Height();
  AnimateAdditiveNumber(parameters, percentage, repeat_count, from_rect->X(),
                        to_rect->X(), to_at_end_of_duration_rect->X(),
                        animated_x);
  AnimateAdditiveNumber(parameters, percentage, repeat_count, from_rect->Y(),
                        to_rect->Y(), to_at_end_of_duration_rect->Y(),
                        animated_y);
  AnimateAdditiveNumber(parameters, percentage, repeat_count,
                        from_rect->Width(), to_rect->Width(),
                        to_at_end_of_duration_rect->Width(), animated_width);
  AnimateAdditiveNumber(parameters, percentage, repeat_count,
                        from_rect->Height(), to_rect->Height(),
                        to_at_end_of_duration_rect->Height(), animated_height);

  value_ = FloatRect(animated_x, animated_y, animated_width, animated_height);
}

float SVGRect::CalculateDistance(const SVGPropertyBase* to,
                                 const SVGElement* context_element) const {
  // FIXME: Distance calculation is not possible for SVGRect right now. We need
  // the distance for every single value.
  return -1;
}

void SVGRect::SetInvalid() {
  value_ = FloatRect(0.0f, 0.0f, 0.0f, 0.0f);
  is_valid_ = false;
}

}  // namespace blink
