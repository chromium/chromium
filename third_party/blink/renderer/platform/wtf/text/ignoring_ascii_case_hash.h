// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_IGNORING_ASCII_CASE_HASH_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_IGNORING_ASCII_CASE_HASH_H_

#include "base/check.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace internal {

// This class is a LChar reader that converts ASCII upper-case characters to
// lower-case. This is to be used as a character reader for StringHasher.
class IgnoringAsciiCaseHashReader8 {
  STATIC_ONLY(IgnoringAsciiCaseHashReader8);

 public:
  // We never contract 16 to 8 bits, so this must always be 1.
  static constexpr unsigned kCompressionFactor = 1;

  // We always produce Latin1 output.
  static constexpr unsigned kExpansionFactor = 1;

  // SAFETY: rapidhash callback.
  UNSAFE_BUFFER_USAGE static inline uint64_t Read64(const uint8_t* ptr) {
    auto* p = reinterpret_cast<const LChar*>(ptr);
    return ToLower(p[0]) | (ToLower(p[1]) << 8) | (ToLower(p[2]) << 16) |
           (ToLower(p[3]) << 24) | (ToLower(p[4]) << 32) |
           (ToLower(p[5]) << 40) | (ToLower(p[6]) << 48) |
           (ToLower(p[7]) << 56);
  }

  // SAFETY: rapidhash callback.
  UNSAFE_BUFFER_USAGE static inline uint64_t Read32(const uint8_t* ptr) {
    auto* p = reinterpret_cast<const LChar*>(ptr);
    return ToLower(p[0]) | (ToLower(p[1]) << 8) | (ToLower(p[2]) << 16) |
           (ToLower(p[3]) << 24);
  }

  // SAFETY: rapidhash callback.
  UNSAFE_BUFFER_USAGE static inline uint64_t ReadSmall(const uint8_t* ptr,
                                                       size_t return_size) {
    auto* p = reinterpret_cast<const LChar*>(ptr);
    DCHECK_LE(return_size, 3u);
    // The structure of the return value must match to PlainHashReader of
    // rapidhash.
    return (ToLower(p[0]) << 56) | (ToLower(p[return_size >> 1]) << 32) |
           ToLower(p[return_size - 1]);
  }

 private:
  static uint64_t ToLower(LChar ch) { return ToASCIILower(ch); }
};

// This class is a UChar reader that converts ASCII upper-case characters to
// lower-case. This is to be used as a character reader for StringHasher.
class IgnoringAsciiCaseHashReader16 {
  STATIC_ONLY(IgnoringAsciiCaseHashReader16);

 public:
  // We contract 16 to 8 bits.
  static constexpr unsigned kCompressionFactor = 2;

  // We always produce Latin1 output, though we take in UTF-16.
  static constexpr unsigned kExpansionFactor = 1;

  // SAFETY: rapidhash callback.
  UNSAFE_BUFFER_USAGE static inline uint64_t Read64(const uint8_t* ptr) {
    auto* p = reinterpret_cast<const UChar*>(ptr);
    return ToLower(p[0]) | (ToLower(p[1]) << 8) | (ToLower(p[2]) << 16) |
           (ToLower(p[3]) << 24) | (ToLower(p[4]) << 32) |
           (ToLower(p[5]) << 40) | (ToLower(p[6]) << 48) |
           (ToLower(p[7]) << 56);
  }

  // SAFETY: rapidhash callback.
  UNSAFE_BUFFER_USAGE static inline uint64_t Read32(const uint8_t* ptr) {
    auto* p = reinterpret_cast<const UChar*>(ptr);
    return ToLower(p[0]) | (ToLower(p[1]) << 8) | (ToLower(p[2]) << 16) |
           (ToLower(p[3]) << 24);
  }

  // SAFETY: rapidhash callback.
  UNSAFE_BUFFER_USAGE static inline uint64_t ReadSmall(const uint8_t* ptr,
                                                       size_t return_size) {
    auto* p = reinterpret_cast<const UChar*>(ptr);
    DCHECK_LE(return_size, 3u);
    // The structure of the return value must match to PlainHashReader of
    // rapidhash.
    return (ToLower(p[0]) << 56) | (ToLower(p[return_size >> 1]) << 32) |
           ToLower(p[return_size - 1]);
  }

 private:
  static uint64_t ToLower(UChar ch) {
    DCHECK_LT(ch, 0x0100);
    return ToASCIILower(ch);
  }
};

}  // namespace internal

struct IgnoringAsciiCaseHash {
  STATIC_ONLY(IgnoringAsciiCaseHash);

  static unsigned GetHash(const String& string) {
    if (string.ContainsOnlyASCIIOrEmpty() && string.IsLowerASCII()) {
      return HashTraits<String>::GetHash(string);
    }
    if (string.Is8Bit()) {
      auto span = string.Span8();
      return StringHasher::ComputeHashAndMaskTop8Bits<
          internal::IgnoringAsciiCaseHashReader8>(base::as_chars(span).data(),
                                                  span.size());
    }
    if (string.ContainsOnlyLatin1OrEmpty()) {
      auto span = string.Span16();
      return StringHasher::ComputeHashAndMaskTop8Bits<
          internal::IgnoringAsciiCaseHashReader16>(base::as_chars(span).data(),
                                                   span.size());
    }
    // This code path would be rarely used.
    return HashTraits<String>::GetHash(string.LowerASCII());
  }

  static unsigned GetHash(const AtomicString& string) {
    return GetHash(string.GetString());
  }

  static bool Equal(const String& a, const String& b) {
    return EqualIgnoringASCIICase(a, b);
  }

  static bool Equal(const AtomicString& a, const AtomicString& b) {
    return EqualIgnoringASCIICase(a, b);
  }

  static const bool kSafeToCompareToEmptyOrDeleted = false;
};

// HashTraits for ASCII case-insensitive strings.
template <typename T>
  requires std::is_same_v<T, String> || std::is_same_v<T, AtomicString>
struct IgnoringAsciiCaseHashTraits : HashTraits<T>, IgnoringAsciiCaseHash {
  using IgnoringAsciiCaseHash::Equal;
  using IgnoringAsciiCaseHash::GetHash;
  using IgnoringAsciiCaseHash::kSafeToCompareToEmptyOrDeleted;
};

// HashTranslator for a hash with String or AtomicString keys.
// We can find an entry for a StringView without creating a new String or a new
// AtomicString.
//
// HashMap<String, ...> map;
// auto it = map.Find<IgnoringAsciiCaseHashTranslator, StringView>(string_view);
struct IgnoringAsciiCaseHashTranslator {
  STATIC_ONLY(IgnoringAsciiCaseHashTranslator);

  static unsigned GetHash(StringView string) {
    if (string.SharedImpl()) {
      return IgnoringAsciiCaseHash::GetHash(string.ToString());
    }
    if (string.Is8Bit()) {
      auto span = string.Span8();
      return StringHasher::ComputeHashAndMaskTop8Bits<
          internal::IgnoringAsciiCaseHashReader8>(base::as_chars(span).data(),
                                                  span.size());
    }
    if (string.ContainsOnlyLatin1OrEmpty()) {
      auto span = string.Span16();
      return StringHasher::ComputeHashAndMaskTop8Bits<
          internal::IgnoringAsciiCaseHashReader16>(base::as_chars(span).data(),
                                                   span.size());
    }
    // This code path would be rarely used.
    return HashTraits<String>::GetHash(string.ToString().LowerASCII());
  }

  static bool Equal(const String& a, StringView b) {
    return EqualIgnoringASCIICase(a, b);
  }

  static bool Equal(const AtomicString& a, StringView b) {
    return EqualIgnoringASCIICase(a, b);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_IGNORING_ASCII_CASE_HASH_H_
