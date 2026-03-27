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

TEST(HTMLTokenizerTest, ProcessingInstruction) {
  test::TaskEnvironment task_environment;
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);
  SegmentedString input("<?target data?>");
  HTMLToken* token = tokenizer->NextToken(input);
  ASSERT_TRUE(token);
  EXPECT_EQ(HTMLToken::kProcessingInstruction, token->GetType());
  EXPECT_EQ("target", token->GetProcessingInstructionTarget().AsString());
  EXPECT_EQ("data", String(token->Data().AsString()));
}

TEST(HTMLTokenizerTest, HasEntity) {
  test::TaskEnvironment task_environment;
  HTMLParserOptions options;

  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);
  SegmentedString input1("a<");
  HTMLToken* token1 = tokenizer->NextToken(input1);
  EXPECT_TRUE(token1);
  if (token1) {
    EXPECT_EQ(HTMLToken::kCharacter, token1->GetType());
    EXPECT_FALSE(token1->HasEntity());
  }

  tokenizer = std::make_unique<HTMLTokenizer>(options);
  SegmentedString input2("&amp;<");
  HTMLToken* token2 = tokenizer->NextToken(input2);
  EXPECT_TRUE(token2);
  if (token2) {
    EXPECT_EQ(HTMLToken::kCharacter, token2->GetType());
    EXPECT_TRUE(token2->HasEntity());
  }

  tokenizer = std::make_unique<HTMLTokenizer>(options);
  SegmentedString input3("&auml;<");
  HTMLToken* token3 = tokenizer->NextToken(input3);
  EXPECT_TRUE(token3);
  if (token3) {
    EXPECT_EQ(HTMLToken::kCharacter, token3->GetType());
    EXPECT_TRUE(token3->HasEntity());
  }
}

}  // namespace blink
