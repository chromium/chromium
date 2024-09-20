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

#include "third_party/blink/renderer/core/svg/svg_string_list.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

SVGStringListBase::~SVGStringListBase() = default;

void SVGStringListBase::Clear() {
  values_.clear();
}

void SVGStringListBase::Insert(uint32_t index, const String& new_item) {
  values_.insert(index, new_item);
}

void SVGStringListBase::Remove(uint32_t index) {
  values_.EraseAt(index);
}

void SVGStringListBase::Append(const String& new_item) {
  values_.push_back(new_item);
}

void SVGStringListBase::Replace(uint32_t index, const String& new_item) {
  values_[index] = new_item;
}

template <typename CharType>
void SVGStringListBase::ParseInternal(const CharType* ptr,
                                      const CharType* end,
                                      char list_delimiter) {
  while (ptr < end) {
    const CharType* start = ptr;
    while (ptr < end && *ptr != list_delimiter && !IsHTMLSpace<CharType>(*ptr))
      ptr++;
    if (ptr == start)
      break;
    values_.push_back(String(start, static_cast<wtf_size_t>(ptr - start)));
    SkipOptionalSVGSpacesOrDelimiter(ptr, end, list_delimiter);
  }
}

SVGParsingError SVGStringListBase::SetValueAsStringWithDelimiter(
    const String& data,
    char list_delimiter) {
  // FIXME: Add more error checking and reporting.
  values_.clear();

  if (data.empty())
    return SVGParseStatus::kNoError;

  WTF::VisitCharacters(data, [&](auto chars) {
    ParseInternal(chars.data(), chars.data() + chars.size(), list_delimiter);
  });
  return SVGParseStatus::kNoError;
}

String SVGStringListBase::ValueAsStringWithDelimiter(
    char list_delimiter) const {
  if (values_.empty())
    return String();

  StringBuilder builder;

  Vector<String>::const_iterator it = values_.begin();
  Vector<String>::const_iterator it_end = values_.end();
  if (it != it_end) {
    builder.Append(*it);
    ++it;

    for (; it != it_end; ++it) {
      builder.Append(list_delimiter);
      builder.Append(*it);
    }
  }

  return builder.ToString();
}

void SVGStringListBase::Add(const SVGPropertyBase* other,
                            const SVGElement* context_element) {
  // SVGStringList is never animated.
  NOTREACHED_IN_MIGRATION();
}

void SVGStringListBase::CalculateAnimatedValue(
    const SMILAnimationEffectParameters&,
    float,
    unsigned,
    const SVGPropertyBase*,
    const SVGPropertyBase*,
    const SVGPropertyBase*,
    const SVGElement*) {
  // SVGStringList is never animated.
  NOTREACHED_IN_MIGRATION();
}

float SVGStringListBase::CalculateDistance(const SVGPropertyBase*,
                                           const SVGElement*) const {
  // SVGStringList is never animated.
  NOTREACHED_IN_MIGRATION();
  return -1.0f;
}

}  // namespace blink
