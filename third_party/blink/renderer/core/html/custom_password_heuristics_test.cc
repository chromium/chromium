// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom_password_heuristics.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(CustomPasswordHeuristicsTest, IsLikelyJSCustomPasswordField) {
  // Empty or short values are not classified.
  EXPECT_FALSE(IsLikelyJSCustomPasswordField(""));
  EXPECT_FALSE(IsLikelyJSCustomPasswordField("*"));

  // Classic masking patterns.
  EXPECT_TRUE(IsLikelyJSCustomPasswordField("**"));
  EXPECT_TRUE(IsLikelyJSCustomPasswordField("*****"));

  // Masking with one visible trailing character (often typed last).
  EXPECT_TRUE(IsLikelyJSCustomPasswordField("*a"));
  EXPECT_TRUE(IsLikelyJSCustomPasswordField("****z"));

  // Plain text is not masked.
  EXPECT_FALSE(IsLikelyJSCustomPasswordField("password"));
  EXPECT_FALSE(IsLikelyJSCustomPasswordField("12345"));

  // Whitespace disqualifies masking.
  EXPECT_FALSE(IsLikelyJSCustomPasswordField("**** "));
  EXPECT_FALSE(IsLikelyJSCustomPasswordField(" ****"));

  // Mid-string visible characters disqualify masking.
  EXPECT_FALSE(IsLikelyJSCustomPasswordField("**a*"));
  EXPECT_FALSE(IsLikelyJSCustomPasswordField("a****"));
}

}  // namespace blink
