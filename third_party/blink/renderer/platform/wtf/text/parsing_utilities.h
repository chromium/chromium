/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_PARSING_UTILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_PARSING_UTILITIES_H_

#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

template <typename CharType>
bool SkipExactly(base::span<const CharType> chars,
                 CharType delimiter,
                 size_t& position) {
  if (position < chars.size() && chars[position] == delimiter) {
    ++position;
    return true;
  }
  return false;
}

template <typename CharType>
bool SkipExactly(base::span<const CharType>& chars, CharType c) {
  if (!chars.empty() && chars.front() == c) {
    chars = chars.template subspan<1u>();
    return true;
  }
  return false;
}

template <typename CharType, bool predicate(CharType)>
bool SkipExactly(base::span<const CharType> chars, size_t& position) {
  if (position < chars.size() && predicate(chars[position])) {
    ++position;
    return true;
  }
  return false;
}

template <typename CharType>
bool SkipToken(const base::span<const CharType> chars,
               StringView token,
               size_t& position) {
  if (position + token.length() > chars.size()) {
    return false;
  }
  auto subspan = chars.subspan(position, token.length());
  bool matched = VisitCharacters(
      token, [&subspan](auto token_chars) { return subspan == token_chars; });
  if (matched) {
    position += token.length();
  }
  return matched;
}

template <typename CharType>
bool SkipToken(base::span<const CharType>& chars, std::string_view token) {
  if (chars.size() < token.size()) {
    return false;
  }
  if (chars.first(token.size()) != base::span(token)) {
    return false;
  }

  chars = chars.subspan(token.size());
  return true;
}

template <typename CharType>
[[nodiscard]] size_t SkipUntil(base::span<const CharType> chars,
                               size_t position,
                               CharType delimiter) {
  while (position < chars.size() && chars[position] != delimiter) {
    ++position;
  }
  return position;
}

template <typename CharType, bool predicate(CharType)>
[[nodiscard]] size_t SkipUntil(base::span<const CharType> chars,
                               size_t position) {
  while (position < chars.size() && !predicate(chars[position])) {
    ++position;
  }
  return position;
}

template <typename CharType, bool predicate(CharType)>
[[nodiscard]] size_t SkipWhile(base::span<const CharType> chars,
                               size_t position) {
  while (position < chars.size() && predicate(chars[position])) {
    ++position;
  }
  return position;
}

template <typename CharType, bool predicate(CharType)>
[[nodiscard]] size_t ReverseSkipWhile(base::span<const CharType> chars,
                                      size_t position,
                                      size_t start) {
  while (position >= start && predicate(chars[position])) {
    --position;
  }
  return position;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_PARSING_UTILITIES_H_
