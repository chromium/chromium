// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/proofreader.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

String ApplyCorrections(Vector<Correction> corrections, const String& input) {
  String result = "";
  uint32_t index = 0;
  for (const Correction& c : corrections) {
    auto start_index = c.error_start;
    auto end_index = c.error_end;
    if (index < start_index) {
      result = result + input.Substring(index, start_index - index);
    }
    result = result + c.correction;
    index = end_index;
  }
  if (index < input.length()) {
    result = result + input.Substring(index);
  }

  return result;
}

}  // namespace

TEST(GetProofreadingCorrections, SimpleReplacement) {
  const String kInput = "aple";
  const String kCorrectedInput = "apple";
  auto list_of_corrections = GetCorrections(kInput, kCorrectedInput);
  EXPECT_EQ(ApplyCorrections(list_of_corrections, kInput), kCorrectedInput);
}

TEST(GetProofreadingCorrections, SimpleInsertion) {
  const String kInput = "I don't want do this.";
  const String kCorrectedInput = "I don't want to do this.";
  auto list_of_corrections = GetCorrections(kInput, kCorrectedInput);
  EXPECT_EQ(ApplyCorrections(list_of_corrections, kInput), kCorrectedInput);
}

TEST(GetProofreadingCorrections, SimpleDeletion) {
  const String kInput = "Can you help me to do this?";
  const String kCorrectedInput = "Can you help me do this?";
  auto list_of_corrections = GetCorrections(kInput, kCorrectedInput);
  EXPECT_EQ(ApplyCorrections(list_of_corrections, kInput), kCorrectedInput);
}

TEST(GetProofreadingCorrections, SimpleOrderSwapping) {
  const String kInput = "Why I can't use phone?";
  const String kCorrectedInput = "Why can't I use my phone?";
  auto list_of_corrections = GetCorrections(kInput, kCorrectedInput);
  EXPECT_EQ(ApplyCorrections(list_of_corrections, kInput), kCorrectedInput);
}

TEST(GetProofreadingCorrections, MixedOperations) {
  const String kInput = "can you profread this fir me";
  const String kCorrectedInput = "Can you proofread this for me?";
  auto list_of_corrections = GetCorrections(kInput, kCorrectedInput);
  EXPECT_EQ(ApplyCorrections(list_of_corrections, kInput), kCorrectedInput);
}

TEST(GetProofreadingCorrections, MultipleSentencesWithEmoji) {
  const String kInput =
      "Having walked the dog, the leash was still in my hand. Her, who is "
      "usually very calm, suddenly bolted towards the fence, nearly tripping I "
      "😢";
  const String kCorrectedInput =
      "Having walked the dog, I still had the leash in my hand. She, who is "
      "usually very calm, suddenly bolted towards the fence, nearly tripping "
      "me 😢";
  auto list_of_corrections = GetCorrections(kInput, kCorrectedInput);
  EXPECT_EQ(ApplyCorrections(list_of_corrections, kInput), kCorrectedInput);
}

TEST(GetProofreadingCorrections, ComplicateSentenceWithHyphens) {
  const String kInput =
      "The critically-acclaimed – albeit controversial, docu-series explore "
      "the socio-economic ramifications of late-stage capitalism in "
      "post-industrial society's.";
  const String kCorrectedInput =
      "The critically acclaimed—albeit controversial—docuseries explores the "
      "socioeconomic ramifications of late-stage capitalism in post-industrial "
      "societies.";
  auto list_of_corrections = GetCorrections(kInput, kCorrectedInput);
  EXPECT_EQ(ApplyCorrections(list_of_corrections, kInput), kCorrectedInput);
}

TEST(GetProofreadingCorrections, Numbers) {
  const String kInput = "this are my number: 12345678910";
  const String kCorrectedInput = "This is my number: 123-4567-8910.";
  auto list_of_corrections = GetCorrections(kInput, kCorrectedInput);
  EXPECT_EQ(ApplyCorrections(list_of_corrections, kInput), kCorrectedInput);
}

TEST(GetProofreadingCorrections, NonUtf8Input) {
  const char16_t nonutf8_input[] = u"A\xD83D";
  const String kInput = String(nonutf8_input);
  const String kCorrectedInput = " ";
  auto list_of_corrections = GetCorrections(kInput, kCorrectedInput);
  EXPECT_EQ(ApplyCorrections(list_of_corrections, kInput), kCorrectedInput);
}

TEST(GetProofreadingCorrections, NoChange) {
  const String kInput = "Hello world!";
  const String kCorrectedInput = "Hello world!";
  auto list_of_corrections = GetCorrections(kInput, kCorrectedInput);
  EXPECT_EQ(ApplyCorrections(list_of_corrections, kInput), kCorrectedInput);
}

TEST(GetProofreadingCorrections, TrailingSpaces) {
  const String kInput = "Hello world  ";
  const String kCorrectedInput = "Hello world";
  auto list_of_corrections = GetCorrections(kInput, kCorrectedInput);
  EXPECT_EQ(ApplyCorrections(list_of_corrections, kInput), kCorrectedInput);
}

TEST(GetProofreadingCorrections, LeadingSpaces) {
  const String kInput = "  Hello world.";
  const String kCorrectedInput = "Hello world.";
  auto list_of_corrections = GetCorrections(kInput, kCorrectedInput);
  EXPECT_EQ(ApplyCorrections(list_of_corrections, kInput), kCorrectedInput);
}

}  // namespace blink
