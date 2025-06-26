/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_OPERATORS_ATOMIC_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_OPERATORS_ATOMIC_H_

#include "third_party/blink/renderer/platform/wtf/text/string_operators.h"

namespace blink {

template <>
class StringTypeAdapter<AtomicString> : public StringTypeAdapter<StringView> {
 public:
  explicit StringTypeAdapter(const AtomicString& string)
      : StringTypeAdapter<StringView>(string) {}
};

inline StringAppend<const char*, AtomicString> operator+(
    const char* string1,
    const AtomicString& string2) {
  return StringAppend<const char*, AtomicString>(string1, string2);
}

inline StringAppend<const UChar*, AtomicString> operator+(
    const UChar* string1,
    const AtomicString& string2) {
  return StringAppend<const UChar*, AtomicString>(string1, string2);
}

template <typename T>
StringAppend<AtomicString, T> operator+(const AtomicString& string1,
                                        T string2) {
  return StringAppend<AtomicString, T>(string1, string2);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_OPERATORS_ATOMIC_H_
