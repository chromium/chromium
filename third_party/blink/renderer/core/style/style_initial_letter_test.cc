// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_initial_letter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// crbug.com/1395673
TEST(StyleInitialLetterTest, LargeSize) {
  EXPECT_GE(StyleInitialLetter(2147483648.0f).Sink(), 1);
  EXPECT_GE(StyleInitialLetter::Drop(2147483648.0f).Sink(), 1);
}

}  // namespace blink
