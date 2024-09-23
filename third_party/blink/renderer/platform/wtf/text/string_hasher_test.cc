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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/case_folding_hash.h"

namespace WTF {

namespace {

const char kNullLChars[2] = {0, 0};
const UChar kNullUChars[2] = {0, 0};

const unsigned kEmptyStringHash = 0x74EBC84B;
const unsigned kSingleNullCharacterHash = 0x8249B3F8;

const LChar kTestALChars[6] = {0x41, 0x95, 0xFF, 0x50, 0x01, 0};
const UChar kTestAUChars[6] = {0x41, 0x95, 0xFF, 0x50, 0x01, 0};
const UChar kTestBUChars[6] = {0x41, 0x95, 0xFFFF, 0x1080, 0x01, 0};

const unsigned kTestAHash = 0xE0A5DDE6;
const unsigned kTestBHash = 0xEEA75C1E;

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
  const char kStr[] = "A quick browñ föx jumps over thé lazy dog";
  UChar kWideStr[sizeof(kStr)];
  for (unsigned i = 0; i < sizeof(kStr); ++i) {
    kWideStr[i] = static_cast<uint8_t>(kStr[i]);
  }
  EXPECT_EQ(StringHasher::ComputeHashAndMaskTop8Bits(kStr, strlen(kStr)),
            StringHasher::ComputeHashAndMaskTop8Bits<ConvertTo8BitHashReader>(
                (const char*)kWideStr, strlen(kStr)));
  EXPECT_NE(StringHasher::ComputeHashAndMaskTop8Bits(kStr, strlen(kStr)),
            StringHasher::ComputeHashAndMaskTop8Bits((const char*)kWideStr,
                                                     strlen(kStr)));
  EXPECT_NE(StringHasher::ComputeHashAndMaskTop8Bits(kStr, strlen(kStr)),
            StringHasher::ComputeHashAndMaskTop8Bits((const char*)kWideStr,
                                                     strlen(kStr) * 2));
}

TEST(StringHasherTest, StringHasher_HashMemory) {
  EXPECT_EQ(kEmptyStringHash, StringHasher::HashMemory(nullptr, 0));
  EXPECT_EQ(kEmptyStringHash, StringHasher::HashMemory(kNullUChars, 0));
  EXPECT_EQ(kEmptyStringHash, StringHasher::HashMemory<0>(nullptr));
  EXPECT_EQ(kEmptyStringHash, StringHasher::HashMemory<0>(kNullUChars));

  EXPECT_EQ(kSingleNullCharacterHash, StringHasher::HashMemory(kNullLChars, 1));
  EXPECT_EQ(kSingleNullCharacterHash, StringHasher::HashMemory<1>(kNullUChars));

  EXPECT_EQ(kTestAHash, StringHasher::HashMemory(kTestALChars, 5));
  EXPECT_EQ(kTestAHash, StringHasher::HashMemory<5>(kTestALChars));
  EXPECT_EQ(kTestBHash, StringHasher::HashMemory(kTestBUChars, 10));
  EXPECT_EQ(kTestBHash, StringHasher::HashMemory<10>(kTestBUChars));
}

TEST(StringHasherTest, CaseFoldingHash) {
  EXPECT_NE(CaseFoldingHash::GetHash("foo"), CaseFoldingHash::GetHash("bar"));
  EXPECT_EQ(CaseFoldingHash::GetHash("foo"), CaseFoldingHash::GetHash("FOO"));
  EXPECT_EQ(CaseFoldingHash::GetHash("foo"), CaseFoldingHash::GetHash("Foo"));
  EXPECT_EQ(CaseFoldingHash::GetHash("Longer string 123"),
            CaseFoldingHash::GetHash("longEr String 123"));
  EXPECT_EQ(CaseFoldingHash::GetHash(String::FromUTF8("Ünicode")),
            CaseFoldingHash::GetHash(String::FromUTF8("ünicode")));
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
    EXPECT_EQ(WTF::GetHash(s8), WTF::GetHash(s16));
  }
}

}  // namespace WTF
