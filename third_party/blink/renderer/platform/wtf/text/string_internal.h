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

template <typename SearchCharacterType, typename MatchCharacterType>
inline wtf_size_t FindInternal(base::span<const SearchCharacterType> search,
                               base::span<const MatchCharacterType> match,
                               wtf_size_t index) {
  // Optimization: keep a running hash of the strings,
  // only call equal() if the hashes match.

  wtf_size_t match_length = base::checked_cast<wtf_size_t>(match.size());
  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  wtf_size_t delta =
      base::checked_cast<wtf_size_t>(search.size() - match.size());

  wtf_size_t search_hash = 0;
  wtf_size_t match_hash = 0;

  for (size_t i = 0; i < match_length; ++i) {
    search_hash += search[i];
    match_hash += match[i];
  }

  wtf_size_t i = 0;
  // Keep looping until we match.
  //
  // We don't use base::span methods for better performance.
  const SearchCharacterType* search_data = search.data();
  while (search_hash != match_hash ||
         !std::equal(match.begin(), match.end(), search_data)) {
    if (i == delta) {
      return kNotFound;
    }
    // SAFETY: This function ensures `search_data[match_length]` and
    // `search_data[0]` are safe.
    search_hash += UNSAFE_BUFFERS(search_data[match_length]);
    search_hash -= UNSAFE_BUFFERS(search_data[0]);
    ++i;
    UNSAFE_BUFFERS(++search_data);
  }
  return index + i;
}

// Optimized for the most common case where `search` and `match` are LChar.
template <>
ALWAYS_INLINE wtf_size_t FindInternal(base::span<const LChar> search,
                                      base::span<const LChar> match,
                                      wtf_size_t index) {
  CHECK_LT(1u, match.size());

  base::span<const LChar> current = search;

  while (current.size() >= match.size()) {
    base::span<const LChar> search_span =
        current.first(current.size() - match.size() + 1);

    // SAFETY: Safe because we're staying within the bounds of the span. Did not
    // use other options (such as std::find) because this is empirically faster
    // in a hot method.
    const LChar* p = UNSAFE_BUFFERS(static_cast<const LChar*>(memchr(
        search_span.data(), match[0], search_span.size() * sizeof(LChar))));
    if (!p) {
      return kNotFound;
    }

    current = current.subspan(static_cast<wtf_size_t>(p - current.data()));
    CHECK_LE(match.size(), current.size());

    // SAFETY: Safe because we're reading match.size() chars from current and
    // match and we've just CHECK'd that current is at least as long as match.
    // Did not use other options because this is empirically faster in a hot
    // method.
    if (UNSAFE_BUFFERS(memcmp(current.data(), match.data(),
                              match.size() * sizeof(LChar))) == 0) {
      return index + (p - search.data());
    }

    current = current.subspan(1u);
  }

  return kNotFound;
}

inline wtf_size_t Find(const StringView& string,
                       const StringView& match,
                       wtf_size_t index) {
  const wtf_size_t match_length = match.length();

  // Optimization: fast case for strings of length 1.
  if (match_length == 1) {
    return string.Is8Bit() ? blink::Find(string.Span8(), match[0], index)
                           : blink::Find(string.Span16(), match[0], index);
  }

  // Check index & matchLength are in range.
  if (index > string.length()) {
    return kNotFound;
  }
  const wtf_size_t search_length = string.length() - index;
  if (match_length > search_length) {
    return kNotFound;
  }
  // If the string to match is the empty string, return `index`. At this point
  // we know it is <= `string.length()`.
  if (!match_length) [[unlikely]] {
    return index;
  }

  if (string.Is8Bit()) {
    if (match.Is8Bit()) {
      return FindInternal(string.Span8().subspan(index), match.Span8(), index);
    }
    return FindInternal(string.Span8().subspan(index), match.Span16(), index);
  }
  if (match.Is8Bit()) {
    return FindInternal(string.Span16().subspan(index), match.Span8(), index);
  }
  return FindInternal(string.Span16().subspan(index), match.Span16(), index);
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE wtf_size_t ReverseFind(
    base::span<const SearchCharacterType> search,
    base::span<const MatchCharacterType> match,
    wtf_size_t index) {
  // Optimization: keep a running hash of the strings,
  // only call equal if the hashes match.

  wtf_size_t match_length = base::checked_cast<wtf_size_t>(match.size());
  // delta is the number of additional times to test; delta == 0 means test only
  // once.
  wtf_size_t delta = std::min(
      index, base::checked_cast<wtf_size_t>(search.size() - match_length));

  wtf_size_t search_hash = 0;
  wtf_size_t match_hash = 0;
  for (wtf_size_t i = 0; i < match_length; ++i) {
    search_hash += search[delta + i];
    match_hash += match[i];
  }

  // Keep looping until we match.
  //
  // We don't use base::span methods for better performance.
  // SAFETY: This function ensures `search.data() + delta` and
  // `search.data() + delta + match_length` are safe.
  const SearchCharacterType* search_data =
      UNSAFE_BUFFERS(search.data() + delta);
  while (search_hash != match_hash ||
         !std::equal(match.begin(), match.end(), search_data)) {
    if (!delta) {
      return kNotFound;
    }
    --delta;
    UNSAFE_BUFFERS(--search_data);
    search_hash -= UNSAFE_BUFFERS(search_data[match_length]);
    search_hash += UNSAFE_BUFFERS(search_data[0]);
  }
  return delta;
}

}  // namespace blink::internal

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_INTERNAL_H_
