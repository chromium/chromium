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

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

namespace {

const LChar kNullLChars[2] = {0, 0};
const UChar kNullUChars[2] = {0, 0};

const unsigned kEmptyStringHash = 0x4EC889EU;
const unsigned kSingleNullCharacterHash = 0x3D3ABF44U;

const LChar kTestALChars[6] = {0x41, 0x95, 0xFF, 0x50, 0x01, 0};
const UChar kTestAUChars[6] = {0x41, 0x95, 0xFF, 0x50, 0x01, 0};
const UChar kTestBUChars[6] = {0x41, 0x95, 0xFFFF, 0x1080, 0x01, 0};

const unsigned kTestAHash1 = 0xEA32B004;
const unsigned kTestAHash2 = 0x93F0F71E;
const unsigned kTestAHash3 = 0xCB609EB1;
const unsigned kTestAHash4 = 0x7984A706;
const unsigned kTestAHash5 = 0x0427561F;

const unsigned kTestBHash1 = 0xEA32B004;
const unsigned kTestBHash2 = 0x93F0F71E;
const unsigned kTestBHash3 = 0x59EB1B2C;
const unsigned kTestBHash4 = 0xA7BCCC0A;
const unsigned kTestBHash5 = 0x79201649;

}  // anonymous namespace

TEST(StringHasherTest, StringHasher) {
  StringHasher hasher;

  // The initial state of the hasher.
  EXPECT_EQ(kEmptyStringHash, hasher.GetHash());
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
}

TEST(StringHasherTest, StringHasher_addCharacter) {
  StringHasher hasher;

  // Hashing a single character.
  hasher = StringHasher();
  hasher.AddCharacter(0);
  EXPECT_EQ(kSingleNullCharacterHash, hasher.GetHash());
  EXPECT_EQ(kSingleNullCharacterHash & 0xFFFFFF,
            hasher.HashWithTop8BitsMasked());

  // Hashing five characters, checking the intermediate state after each is
  // added.
  hasher = StringHasher();
  hasher.AddCharacter(kTestAUChars[0]);
  EXPECT_EQ(kTestAHash1, hasher.GetHash());
  EXPECT_EQ(kTestAHash1 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacter(kTestAUChars[1]);
  EXPECT_EQ(kTestAHash2, hasher.GetHash());
  EXPECT_EQ(kTestAHash2 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacter(kTestAUChars[2]);
  EXPECT_EQ(kTestAHash3, hasher.GetHash());
  EXPECT_EQ(kTestAHash3 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacter(kTestAUChars[3]);
  EXPECT_EQ(kTestAHash4, hasher.GetHash());
  EXPECT_EQ(kTestAHash4 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacter(kTestAUChars[4]);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());

  // Hashing a second set of five characters, including non-Latin-1 characters.
  hasher = StringHasher();
  hasher.AddCharacter(kTestBUChars[0]);
  EXPECT_EQ(kTestBHash1, hasher.GetHash());
  EXPECT_EQ(kTestBHash1 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacter(kTestBUChars[1]);
  EXPECT_EQ(kTestBHash2, hasher.GetHash());
  EXPECT_EQ(kTestBHash2 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacter(kTestBUChars[2]);
  EXPECT_EQ(kTestBHash3, hasher.GetHash());
  EXPECT_EQ(kTestBHash3 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacter(kTestBUChars[3]);
  EXPECT_EQ(kTestBHash4, hasher.GetHash());
  EXPECT_EQ(kTestBHash4 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacter(kTestBUChars[4]);
  EXPECT_EQ(kTestBHash5, hasher.GetHash());
  EXPECT_EQ(kTestBHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
}

TEST(StringHasherTest, StringHasher_addCharacters) {
  StringHasher hasher;

  // Hashing zero characters.
  hasher = StringHasher();
  hasher.AddCharacters(static_cast<LChar*>(nullptr), 0);
  EXPECT_EQ(kEmptyStringHash, hasher.GetHash());
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kNullLChars, 0);
  EXPECT_EQ(kEmptyStringHash, hasher.GetHash());
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(static_cast<UChar*>(nullptr), 0);
  EXPECT_EQ(kEmptyStringHash, hasher.GetHash());
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kNullUChars, 0);
  EXPECT_EQ(kEmptyStringHash, hasher.GetHash());
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF, hasher.HashWithTop8BitsMasked());

  // Hashing one character.
  hasher = StringHasher();
  hasher.AddCharacters(kNullLChars, 1);
  EXPECT_EQ(kSingleNullCharacterHash, hasher.GetHash());
  EXPECT_EQ(kSingleNullCharacterHash & 0xFFFFFF,
            hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kNullUChars, 1);
  EXPECT_EQ(kSingleNullCharacterHash, hasher.GetHash());
  EXPECT_EQ(kSingleNullCharacterHash & 0xFFFFFF,
            hasher.HashWithTop8BitsMasked());

  // Hashing five characters, all at once.
  hasher = StringHasher();
  hasher.AddCharacters(kTestALChars, 5);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kTestAUChars, 5);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kTestBUChars, 5);
  EXPECT_EQ(kTestBHash5, hasher.GetHash());
  EXPECT_EQ(kTestBHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());

  // Hashing five characters, in groups of two, then the last one.
  hasher = StringHasher();
  hasher.AddCharacters(kTestALChars, 2);
  EXPECT_EQ(kTestAHash2, hasher.GetHash());
  EXPECT_EQ(kTestAHash2 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacters(kTestALChars + 2, 2);
  EXPECT_EQ(kTestAHash4, hasher.GetHash());
  EXPECT_EQ(kTestAHash4 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacters(kTestALChars + 4, 1);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kTestALChars, 2);
  hasher.AddCharacters(kTestALChars + 2, 2);
  hasher.AddCharacters(kTestALChars + 4, 1);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kTestAUChars, 2);
  EXPECT_EQ(kTestAHash2, hasher.GetHash());
  EXPECT_EQ(kTestAHash2 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacters(kTestAUChars + 2, 2);
  EXPECT_EQ(kTestAHash4, hasher.GetHash());
  EXPECT_EQ(kTestAHash4 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacters(kTestAUChars + 4, 1);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kTestAUChars, 2);
  hasher.AddCharacters(kTestAUChars + 2, 2);
  hasher.AddCharacters(kTestAUChars + 4, 1);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kTestBUChars, 2);
  EXPECT_EQ(kTestBHash2, hasher.GetHash());
  EXPECT_EQ(kTestBHash2 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacters(kTestBUChars + 2, 2);
  EXPECT_EQ(kTestBHash4, hasher.GetHash());
  EXPECT_EQ(kTestBHash4 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacters(kTestBUChars + 4, 1);
  EXPECT_EQ(kTestBHash5, hasher.GetHash());
  EXPECT_EQ(kTestBHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kTestBUChars, 2);
  hasher.AddCharacters(kTestBUChars + 2, 2);
  hasher.AddCharacters(kTestBUChars + 4, 1);
  EXPECT_EQ(kTestBHash5, hasher.GetHash());
  EXPECT_EQ(kTestBHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());

  // Hashing five characters, the first three, then the last two.
  hasher = StringHasher();
  hasher.AddCharacters(kTestALChars, 3);
  EXPECT_EQ(kTestAHash3, hasher.GetHash());
  EXPECT_EQ(kTestAHash3 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacters(kTestALChars + 3, 2);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kTestALChars, 3);
  EXPECT_EQ(kTestAHash3, hasher.GetHash());
  EXPECT_EQ(kTestAHash3 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacters(kTestALChars + 3, 2);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kTestAUChars, 3);
  EXPECT_EQ(kTestAHash3, hasher.GetHash());
  EXPECT_EQ(kTestAHash3 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacters(kTestAUChars + 3, 2);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kTestAUChars, 3);
  EXPECT_EQ(kTestAHash3, hasher.GetHash());
  EXPECT_EQ(kTestAHash3 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacters(kTestAUChars + 3, 2);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kTestBUChars, 3);
  EXPECT_EQ(kTestBHash3, hasher.GetHash());
  EXPECT_EQ(kTestBHash3 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacters(kTestBUChars + 3, 2);
  EXPECT_EQ(kTestBHash5, hasher.GetHash());
  EXPECT_EQ(kTestBHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharacters(kTestBUChars, 3);
  hasher.AddCharacters(kTestBUChars + 3, 2);
  EXPECT_EQ(kTestBHash5, hasher.GetHash());
  EXPECT_EQ(kTestBHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
}

TEST(StringHasherTest, StringHasher_addCharactersAssumingAligned) {
  StringHasher hasher;

  // Hashing zero characters.
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(static_cast<LChar*>(nullptr), 0);
  EXPECT_EQ(kEmptyStringHash, hasher.GetHash());
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(kNullLChars, 0);
  EXPECT_EQ(kEmptyStringHash, hasher.GetHash());
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(static_cast<UChar*>(nullptr), 0);
  EXPECT_EQ(kEmptyStringHash, hasher.GetHash());
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(kNullUChars, 0);
  EXPECT_EQ(kEmptyStringHash, hasher.GetHash());
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF, hasher.HashWithTop8BitsMasked());

  // Hashing one character.
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(kNullLChars, 1);
  EXPECT_EQ(kSingleNullCharacterHash, hasher.GetHash());
  EXPECT_EQ(kSingleNullCharacterHash & 0xFFFFFF,
            hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(kNullUChars, 1);
  EXPECT_EQ(kSingleNullCharacterHash, hasher.GetHash());
  EXPECT_EQ(kSingleNullCharacterHash & 0xFFFFFF,
            hasher.HashWithTop8BitsMasked());

  // Hashing five characters, all at once.
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(kTestALChars, 5);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(kTestAUChars, 5);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(kTestBUChars, 5);
  EXPECT_EQ(kTestBHash5, hasher.GetHash());
  EXPECT_EQ(kTestBHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());

  // Hashing five characters, in groups of two, then the last one.
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(kTestALChars, 2);
  EXPECT_EQ(kTestAHash2, hasher.GetHash());
  EXPECT_EQ(kTestAHash2 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharactersAssumingAligned(kTestALChars + 2, 2);
  EXPECT_EQ(kTestAHash4, hasher.GetHash());
  EXPECT_EQ(kTestAHash4 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharactersAssumingAligned(kTestALChars + 4, 1);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(kTestALChars, 2);
  hasher.AddCharactersAssumingAligned(kTestALChars + 2, 2);
  hasher.AddCharactersAssumingAligned(kTestALChars + 4, 1);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(kTestAUChars, 2);
  EXPECT_EQ(kTestAHash2, hasher.GetHash());
  EXPECT_EQ(kTestAHash2 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharactersAssumingAligned(kTestAUChars + 2, 2);
  EXPECT_EQ(kTestAHash4, hasher.GetHash());
  EXPECT_EQ(kTestAHash4 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharactersAssumingAligned(kTestAUChars + 4, 1);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(kTestAUChars, 2);
  hasher.AddCharactersAssumingAligned(kTestAUChars + 2, 2);
  hasher.AddCharactersAssumingAligned(kTestAUChars + 4, 1);
  EXPECT_EQ(kTestAHash5, hasher.GetHash());
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(kTestBUChars, 2);
  EXPECT_EQ(kTestBHash2, hasher.GetHash());
  EXPECT_EQ(kTestBHash2 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharactersAssumingAligned(kTestBUChars + 2, 2);
  EXPECT_EQ(kTestBHash4, hasher.GetHash());
  EXPECT_EQ(kTestBHash4 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharactersAssumingAligned(kTestBUChars + 4, 1);
  EXPECT_EQ(kTestBHash5, hasher.GetHash());
  EXPECT_EQ(kTestBHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher = StringHasher();
  hasher.AddCharactersAssumingAligned(kTestBUChars, 2);
  hasher.AddCharactersAssumingAligned(kTestBUChars + 2, 2);
  hasher.AddCharactersAssumingAligned(kTestBUChars + 4, 1);
  EXPECT_EQ(kTestBHash5, hasher.GetHash());
  EXPECT_EQ(kTestBHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());

  // Hashing five characters, first two characters one at a time,
  // then two more, then the last one.
  hasher = StringHasher();
  hasher.AddCharacter(kTestBUChars[0]);
  EXPECT_EQ(kTestBHash1, hasher.GetHash());
  EXPECT_EQ(kTestBHash1 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharacter(kTestBUChars[1]);
  EXPECT_EQ(kTestBHash2, hasher.GetHash());
  EXPECT_EQ(kTestBHash2 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharactersAssumingAligned(kTestBUChars[2], kTestBUChars[3]);
  EXPECT_EQ(kTestBHash4, hasher.GetHash());
  EXPECT_EQ(kTestBHash4 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
  hasher.AddCharactersAssumingAligned(kTestBUChars + 4, 1);
  EXPECT_EQ(kTestBHash5, hasher.GetHash());
  EXPECT_EQ(kTestBHash5 & 0xFFFFFF, hasher.HashWithTop8BitsMasked());
}

TEST(StringHasherTest, StringHasher_computeHash) {
  EXPECT_EQ(kEmptyStringHash,
            StringHasher::ComputeHash(static_cast<LChar*>(nullptr), 0));
  EXPECT_EQ(kEmptyStringHash, StringHasher::ComputeHash(kNullLChars, 0));
  EXPECT_EQ(kEmptyStringHash,
            StringHasher::ComputeHash(static_cast<UChar*>(nullptr), 0));
  EXPECT_EQ(kEmptyStringHash, StringHasher::ComputeHash(kNullUChars, 0));

  EXPECT_EQ(kSingleNullCharacterHash,
            StringHasher::ComputeHash(kNullLChars, 1));
  EXPECT_EQ(kSingleNullCharacterHash,
            StringHasher::ComputeHash(kNullUChars, 1));

  EXPECT_EQ(kTestAHash5, StringHasher::ComputeHash(kTestALChars, 5));
  EXPECT_EQ(kTestAHash5, StringHasher::ComputeHash(kTestAUChars, 5));
  EXPECT_EQ(kTestBHash5, StringHasher::ComputeHash(kTestBUChars, 5));
}

TEST(StringHasherTest, StringHasher_computeHashAndMaskTop8Bits) {
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits(
                static_cast<LChar*>(nullptr), 0));
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits(kNullLChars, 0));
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits(
                static_cast<UChar*>(nullptr), 0));
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits(kNullUChars, 0));

  EXPECT_EQ(kSingleNullCharacterHash & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits(kNullLChars, 1));
  EXPECT_EQ(kSingleNullCharacterHash & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits(kNullUChars, 1));

  EXPECT_EQ(kTestAHash5 & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits(kTestALChars, 5));
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits(kTestAUChars, 5));
  EXPECT_EQ(kTestBHash5 & 0xFFFFFF,
            StringHasher::ComputeHashAndMaskTop8Bits(kTestBUChars, 5));
}

TEST(StringHasherTest, StringHasher_hashMemory) {
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF, StringHasher::HashMemory(nullptr, 0));
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF,
            StringHasher::HashMemory(kNullUChars, 0));
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF, StringHasher::HashMemory<0>(nullptr));
  EXPECT_EQ(kEmptyStringHash & 0xFFFFFF,
            StringHasher::HashMemory<0>(kNullUChars));

  EXPECT_EQ(kSingleNullCharacterHash & 0xFFFFFF,
            StringHasher::HashMemory(kNullUChars, 2));
  EXPECT_EQ(kSingleNullCharacterHash & 0xFFFFFF,
            StringHasher::HashMemory<2>(kNullUChars));

  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, StringHasher::HashMemory(kTestAUChars, 10));
  EXPECT_EQ(kTestAHash5 & 0xFFFFFF, StringHasher::HashMemory<10>(kTestAUChars));
  EXPECT_EQ(kTestBHash5 & 0xFFFFFF, StringHasher::HashMemory(kTestBUChars, 10));
  EXPECT_EQ(kTestBHash5 & 0xFFFFFF, StringHasher::HashMemory<10>(kTestBUChars));
}

}  // namespace WTF
