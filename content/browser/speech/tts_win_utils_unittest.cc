// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_win_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(TtsWinTest, RemoveXml) {
  std::wstring utterance = L"";
  RemoveXml(utterance);
  EXPECT_EQ(L"", utterance);

  utterance = L"What if I told you that 5 < 4?";
  RemoveXml(utterance);
  EXPECT_EQ(L"What if I told you that 5   4?", utterance);

  utterance = L"What does <spell>PBS</spell> stand for?";
  RemoveXml(utterance);
  EXPECT_EQ(L"What does  spell PBS /spell  stand for?", utterance);

  utterance =
      L"What <PARTOFSP PART=\"modifier\" PART=\"modifier\">to</PARTOFSP> say?";
  RemoveXml(utterance);
  EXPECT_EQ(
      L"What  PARTOFSP PART=\"modifier\" PART=\"modifier\" to /PARTOFSP  say?",
      utterance);
}

}  // namespace content
