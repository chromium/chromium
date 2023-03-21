// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_ATOMIC_STRING_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_ATOMIC_STRING_CACHE_H_

#include "third_party/blink/renderer/core/html/parser/literal_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// HTMLAtomicStringCache provides a fixed size cache of strings that is used
// during parsing, and specifically for attribute values. The cache lookup is
// cheap (much cheaper than AtomicString). This benefits parsing when the same
// attribute values are repeated.
class HTMLAtomicStringCache {
 public:
  ALWAYS_INLINE static AtomicString MakeAttributeValue(
      const UCharLiteralBuffer<32>& string) {
    return MakeAtomicString(string);
  }

  ALWAYS_INLINE static AtomicString MakeAttributeValue(
      base::span<const UChar> string) {
    return MakeAtomicString(string);
  }

  ALWAYS_INLINE static AtomicString MakeAttributeValue(
      base::span<const LChar> string) {
    return MakeAtomicString(string);
  }

  ALWAYS_INLINE static void Clear() { Cache().fill({}); }

 private:
  // The value of kMaxStringLengthForCache and kCapacity were chosen
  // empirically by WebKit:
  // https://github.com/WebKit/WebKit/blob/main/Source/WebCore/html/parser/HTMLNameCache.h
  static constexpr auto kMaxStringLengthForCache = 36;
  static constexpr auto kCapacity = 512;
  using AtomicStringCache = std::array<AtomicString, kCapacity>;

  template <typename CharacterType>
  ALWAYS_INLINE static AtomicString MakeAtomicString(
      base::span<const CharacterType> string) {
    // If the attribute has no values, the null atom is used to represent
    // absence of attributes, we set the value to an empty atom.
    if (string.empty()) {
      return g_empty_atom;
    }

    auto length = string.size();
    if (length > kMaxStringLengthForCache) {
      return AtomicString(string.data(), static_cast<unsigned>(length));
    }

    auto first_character = string[0];
    auto last_character = string[length - 1];
    auto& slot = AtomicStringCacheSlot(first_character, last_character, length);

    if (!Equal(slot.Impl(), string.data(), static_cast<unsigned>(length))) {
      AtomicString result(string.data(), static_cast<unsigned>(length));
      slot = result;
    }

    return slot;
  }

  ALWAYS_INLINE static AtomicString MakeAtomicString(
      const UCharLiteralBuffer<32>& string) {
    // If the attribute has no values, the null atom is used to represent
    // absence of attributes, we set the value to an empty atom.
    if (string.IsEmpty()) {
      return g_empty_atom;
    }

    auto length = string.size();
    if (length > kMaxStringLengthForCache) {
      return AtomicString(string.data(), length);
    }

    auto first_character = string[0];
    auto last_character = string[length - 1];
    auto& slot = AtomicStringCacheSlot(first_character, last_character, length);

    if (!Equal(slot.Impl(), string.data(), length)) {
      AtomicString result(string.data(), length);
      slot = result;
    }

    return slot;
  }

  ALWAYS_INLINE static AtomicStringCache& Cache() {
    DEFINE_STATIC_LOCAL(AtomicStringCache, attribute_value_cache_, ());

    return attribute_value_cache_;
  }

  // Description from WebCore: the default string hashing algorithm only barely
  // outperforms this simple hash function on Speedometer (i.e., a cache hit
  // rate of 99.24% using the default hash algorithm vs. 99.15% using the
  // "first/last character and length" hash).
  ALWAYS_INLINE static AtomicString& AtomicStringCacheSlot(
      UChar first_character,
      UChar last_character,
      UChar length) {
    unsigned hash =
        (first_character << 6) ^ ((last_character << 14) ^ first_character);
    hash += (hash >> 14) + (length << 14);
    hash ^= hash << 14;
    return Cache()[(hash + (hash >> 6)) % kCapacity];
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_ATOMIC_STRING_CACHE_H_
