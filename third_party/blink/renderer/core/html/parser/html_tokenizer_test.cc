// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_options.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

// This is a regression test for crbug.com/619141
TEST(HTMLTokenizerTest, ZeroOffsetAttributeNameRange) {
  test::TaskEnvironment task_environment;
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);
  SegmentedString input("<script ");
  EXPECT_EQ(nullptr, tokenizer->NextToken(input));

  SegmentedString input2("type='javascript'");
  // Below should not fail ASSERT
  EXPECT_EQ(nullptr, tokenizer->NextToken(input2));
}

}  // namespace blink
