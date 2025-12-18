// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_IGNORING_ASCII_CASE_HASH_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_IGNORING_ASCII_CASE_HASH_H_

#include "base/check.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_lower_hash_reader.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct IgnoringAsciiCaseHash {
  STATIC_ONLY(IgnoringAsciiCaseHash);

  static unsigned GetHash(const String& string) {
    if (string.ContainsOnlyASCIIOrEmpty() && string.IsLowerASCII()) {
      return HashTraits<String>::GetHash(string);
    }
    base::span<const char> bytes = base::as_chars(string.RawByteSpan());
    if (string.Is8Bit()) {
      return StringHasher::ComputeHashAndMaskTop8Bits<
          AsciiLowerHashReader<LChar>>(bytes.data(), bytes.size());
    }
    if (string.ContainsOnlyLatin1OrEmpty()) {
      using Reader = AsciiConvertTo8AndLowerHashReader;
      return StringHasher::ComputeHashAndMaskTop8Bits<Reader>(
          bytes.data(), bytes.size() / Reader::kCompressionFactor);
    }
    return StringHasher::ComputeHashAndMaskTop8Bits<
        AsciiLowerHashReader<UChar>>(bytes.data(), bytes.size());
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
    base::span<const char> bytes = base::as_chars(string.RawByteSpan());
    if (string.Is8Bit()) {
      return StringHasher::ComputeHashAndMaskTop8Bits<
          AsciiLowerHashReader<LChar>>(bytes.data(), bytes.size());
    }
    if (string.ContainsOnlyLatin1OrEmpty()) {
      using Reader = AsciiConvertTo8AndLowerHashReader;
      return StringHasher::ComputeHashAndMaskTop8Bits<Reader>(
          bytes.data(), bytes.size() / Reader::kCompressionFactor);
    }
    return StringHasher::ComputeHashAndMaskTop8Bits<
        AsciiLowerHashReader<UChar>>(bytes.data(), bytes.size());
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
