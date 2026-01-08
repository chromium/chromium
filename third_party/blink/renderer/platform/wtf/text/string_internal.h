/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_INTERNAL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_INTERNAL_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

// Functions for WTF string class implementations.

namespace blink::internal {

// A pair of the start offset and the length.
using CharacterRange = std::pair<wtf_size_t, wtf_size_t>;

template <typename CharType, class Predicate>
inline CharacterRange StrippedMatchedCharactersRange(base::span<CharType> chars,
                                                     Predicate predicate) {
  if (chars.empty()) {
    return {0, 0};
  }
  DCHECK_LE(chars.size(), std::numeric_limits<wtf_size_t>::max());

  size_t start = 0;
  size_t end = chars.size() - 1;

  // Skip matching characters from the start.
  while (start <= end && predicate(chars[start])) {
    ++start;
  }

  // String only contains matching characters.
  if (start > end) {
    return {0, 0};
  }

  // Skip matching characters from the end.
  while (end && predicate(chars[end])) {
    --end;
  }
  return {static_cast<wtf_size_t>(start),
          static_cast<wtf_size_t>(end + 1 - start)};
}

}  // namespace blink::internal

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_INTERNAL_H_
