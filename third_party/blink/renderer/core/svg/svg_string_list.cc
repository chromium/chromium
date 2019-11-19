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

#include "third_party/blink/renderer/core/svg/svg_string_list.h"

#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

SVGStringListBase::~SVGStringListBase() = default;

void SVGStringListBase::Initialize(const String& item) {
  values_.clear();
  values_.push_back(item);
}

String SVGStringListBase::GetItem(uint32_t index,
                                  ExceptionState& exception_state) {
  if (!CheckIndexBound(index, exception_state))
    return String();

  return values_.at(index);
}

void SVGStringListBase::InsertItemBefore(const String& new_item,
                                         uint32_t index) {
  // Spec: If the index is greater than or equal to numberOfItems, then the new
  // item is appended to the end of the list.
  if (index > values_.size())
    index = values_.size();

  // Spec: Inserts a new item into the list at the specified position. The index
  // of the item before which the new item is to be inserted. The first item is
  // number 0. If the index is equal to 0, then the new item is inserted at the
  // front of the list.
  values_.insert(index, new_item);
}

String SVGStringListBase::RemoveItem(uint32_t index,
                                     ExceptionState& exception_state) {
  if (!CheckIndexBound(index, exception_state))
    return String();

  String old_item = values_.at(index);
  values_.EraseAt(index);
  return old_item;
}

void SVGStringListBase::AppendItem(const String& new_item) {
  values_.push_back(new_item);
}

void SVGStringListBase::ReplaceItem(const String& new_item,
                                    uint32_t index,
                                    ExceptionState& exception_state) {
  if (!CheckIndexBound(index, exception_state))
    return;

  // Update the value at the desired position 'index'.
  values_[index] = new_item;
}

template <typename CharType>
void SVGStringListBase::ParseInternal(const CharType*& ptr,
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

  if (data.IsEmpty())
    return SVGParseStatus::kNoError;

  if (data.Is8Bit()) {
    const LChar* ptr = data.Characters8();
    const LChar* end = ptr + data.length();
    ParseInternal(ptr, end, list_delimiter);
  } else {
    const UChar* ptr = data.Characters16();
    const UChar* end = ptr + data.length();
    ParseInternal(ptr, end, list_delimiter);
  }
  return SVGParseStatus::kNoError;
}

String SVGStringListBase::ValueAsStringWithDelimiter(
    char list_delimiter) const {
  if (values_.IsEmpty())
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

bool SVGStringListBase::CheckIndexBound(uint32_t index,
                                        ExceptionState& exception_state) {
  if (index >= values_.size()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("index", index,
                                                    values_.size()));
    return false;
  }

  return true;
}

void SVGStringListBase::Add(SVGPropertyBase* other,
                            SVGElement* context_element) {
  // SVGStringList is never animated.
  NOTREACHED();
}

void SVGStringListBase::CalculateAnimatedValue(const SVGAnimateElement&,
                                               float,
                                               unsigned,
                                               SVGPropertyBase*,
                                               SVGPropertyBase*,
                                               SVGPropertyBase*,
                                               SVGElement*) {
  // SVGStringList is never animated.
  NOTREACHED();
}

float SVGStringListBase::CalculateDistance(SVGPropertyBase*, SVGElement*) {
  // SVGStringList is never animated.
  NOTREACHED();
  return -1.0f;
}

}  // namespace blink
