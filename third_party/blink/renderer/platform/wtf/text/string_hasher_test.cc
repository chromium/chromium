/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/case_folding_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/convert_to_8bit_hash_reader.h"

namespace blink {

namespace {

const char kNullLChars[1] = {0};
const UChar kNullUChars[1] = {0};

const uint64_t kEmptyStringHash = 0x5A6EF77074EBC84B;
const uint64_t kSingleNullCharacterHash = 0x48DFCE108249B3F8;

const LChar kTestALChars[5] = {0x41, 0x95, 0xFF, 0x50, 0x01};
const UChar kTestAUChars[5] = {0x41, 0x95, 0xFF, 0x50, 0x01};
const UChar kTestBUChars[5] = {0x41, 0x95, 0xFFFF, 0x1080, 0x01};

const uint64_t kTestAHash = 0xE9422771E0A5DDE6;
const uint64_t kTestBHash = 0x4A2DA770EEA75C1E;

bool EqualCaseFoldingHash(StringView a, StringView b) {
  unsigned hash_a = a.Is8Bit() ? CaseFoldingHash::GetHash(a.Span8())
                               : CaseFoldingHash::GetHash(a.Span16());
  unsigned hash_b = b.Is8Bit() ? CaseFoldingHash::GetHash(b.Span8())
                               : CaseFoldingHash::GetHash(b.Span16());
  return hash_a == hash_b;
}

}  // anonymous namespace

TEST(StringHasherTest, StringHasher_ComputeHashAndMaskTop8Bits) {
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits(nullptr, 0));
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits(kNullLChars, 0));
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits<ConvertTo8BitHashReader>(
                nullptr, 0));
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits<ConvertTo8BitHashReader>(
                (const char*)kNullUChars, 0));

  EXPECT_EQ(kSingleNullCharacterHash & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits(kNullLChars, 1));
  EXPECT_EQ(kSingleNullCharacterHash & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits<ConvertTo8BitHashReader>(
                (const char*)kNullUChars, 1));

  EXPECT_EQ(kTestAHash & 0xFFFFFF, StringHasher::ComputeHashAndMaskTop8Bits(
                                       (const char*)kTestALChars, 5));
  EXPECT_EQ(kTestAHash & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits<ConvertTo8BitHashReader>(
                (const char*)kTestAUChars, 5));
  EXPECT_EQ(kTestBHash & 0xFFFFFF, StringHasher::ComputeHashAndMaskTop8Bits(
                                       (const char*)kTestBUChars, 10));

  // Test a slightly longer case (including characters that fit in Latin1
  // but not in ASCII).
  constexpr base::span<const char> kStr =
      base::span_from_cstring("A quick browñ föx jumps over thé lazy dog");
  std::array<UChar, kStr.size()> wide_str;
  std::ranges::copy(base::as_bytes(kStr), wide_str.begin());
  auto wide_bytes = base::as_chars(base::as_byte_span(wide_str));
  unsigned expected_hash =
      StringHasher::ComputeHashAndMaskTop8Bits(kStr.data(), kStr.size());
  using Reader = ConvertTo8BitHashReader;
  EXPECT_EQ(expected_hash, StringHasher::ComputeHashAndMaskTop8Bits<Reader>(
                               wide_bytes.data(),
                               wide_bytes.size() / Reader::kCompressionFactor));
  EXPECT_NE(expected_hash, StringHasher::ComputeHashAndMaskTop8Bits(
                               wide_bytes.data(), wide_bytes.size() / 2));
  EXPECT_NE(expected_hash, StringHasher::ComputeHashAndMaskTop8Bits(
                               wide_bytes.data(), wide_bytes.size()));
}

TEST(StringHasherTest, StringHasher_HashMemory) {
  EXPECT_EQ(kEmptyStringHash,
            StringHasher::HashMemory(base::span<const uint8_t>()));
  EXPECT_EQ(kEmptyStringHash,
            StringHasher::HashMemory(base::span<const uint8_t, 0>()));
  EXPECT_EQ(kEmptyStringHash, StringHasher::HashMemory(
                                  base::as_byte_span(kNullUChars).first(0u)));

  EXPECT_EQ(
      kSingleNullCharacterHash,
      StringHasher::HashMemory(base::as_byte_span(kNullUChars).first(1u)));

  EXPECT_EQ(kTestAHash, StringHasher::HashMemory(kTestALChars));
  EXPECT_EQ(kTestBHash,
            StringHasher::HashMemory(base::as_byte_span(kTestBUChars)));
}

TEST(StringHasherTest, CaseFoldingHash) {
  EXPECT_FALSE(EqualCaseFoldingHash("foo", "bar"));
  EXPECT_TRUE(EqualCaseFoldingHash("foo", "FOO"));
  EXPECT_TRUE(EqualCaseFoldingHash("foo", "Foo"));
  EXPECT_TRUE(EqualCaseFoldingHash("Longer string 123", "longEr String 123"));
  EXPECT_TRUE(EqualCaseFoldingHash(String::FromUTF8("Ünicode"),
                                   String::FromUTF8("ünicode")));
}

TEST(StringHasherTest, ContractionAndExpansion) {
  // CaseFoldingHash is the only current reader using the expansion logic,
  // so we use it to test that the expansion logic is correct for various sizes;
  // we don't really use the case folding itself here. We make a string that's
  // long enough that we will hit most of the paths.
  String str =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_!?'$";
  for (unsigned i = 0; i < str.length(); ++i) {
    String s8 = str.Substring(0, i);
    String s16 = s8;
    s16.Ensure16Bit();
    EXPECT_EQ(CaseFoldingHash::GetHash(s8), CaseFoldingHash::GetHash(s16));
    EXPECT_EQ(GetHash(s8), GetHash(s16));
  }
}

}  // namespace blink
