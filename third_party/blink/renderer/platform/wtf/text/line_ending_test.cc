// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/line_ending.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

TEST(LineEndingTest, NormalizeLineEndingsToCRLF) {
  EXPECT_EQ(String(""), NormalizeLineEndingsToCRLF(""));
  EXPECT_EQ(String("\r\n"), NormalizeLineEndingsToCRLF("\n"));
  EXPECT_EQ(String("\r\n"), NormalizeLineEndingsToCRLF("\r\n"));
  EXPECT_EQ(String("\r\n"), NormalizeLineEndingsToCRLF("\r"));

  EXPECT_EQ(String("abc\r\ndef\r\n"), NormalizeLineEndingsToCRLF("abc\rdef\n"));
}

}  // namespace WTF
