// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_options.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"

#include <memory>

namespace blink {

// This is a regression test for crbug.com/619141
TEST(HTMLTokenizerTest, ZeroOffsetAttributeNameRange) {
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);
  HTMLToken token;

  SegmentedString input("<script ");
  EXPECT_FALSE(tokenizer->NextToken(input, token));

  EXPECT_EQ(HTMLToken::kStartTag, token.GetType());

  SegmentedString input2("type='javascript'");
  // Below should not fail ASSERT
  EXPECT_FALSE(tokenizer->NextToken(input2, token));
}

}  // namespace blink
