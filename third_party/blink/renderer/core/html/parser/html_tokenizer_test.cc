// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"

#include <memory>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_options.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/core/html/parser/input_stream_preprocessor.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

// A SegmentedString at end-of-file, like HTMLInputStream::MarkEndOfFile().
SegmentedString ClosedInput(const String& data) {
  SegmentedString input(data);
  input.Append(SegmentedString(String(base::span_from_ref(kEndOfFileMarker))));
  input.Close();
  return input;
}

}  // namespace

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

// Regression test for crbug.com/40727112: an incomplete markup declaration at
// end of input becomes a bogus comment instead of being dropped.
TEST(HTMLTokenizerTest, IncompleteDoctypeAtEndOfInput) {
  ScopedHTMLParserTruncatedMarkupDeclarationForTest enabled(true);

  test::TaskEnvironment task_environment;
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);

  SegmentedString input = ClosedInput("<!DOC>");
  HTMLToken* token = tokenizer->NextToken(input);
  ASSERT_TRUE(token);
  EXPECT_EQ(HTMLToken::kComment, token->GetType());
  EXPECT_EQ("DOC", String(token->Data().AsString()));
}

// Text after the bogus comment must not be dropped.
TEST(HTMLTokenizerTest, IncompleteDoctypeDoesNotDropTrailingText) {
  ScopedHTMLParserTruncatedMarkupDeclarationForTest enabled(true);

  test::TaskEnvironment task_environment;
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);

  SegmentedString input = ClosedInput("Abc<!d>Hi");

  HTMLToken* token = tokenizer->NextToken(input);
  ASSERT_TRUE(token);
  EXPECT_EQ(HTMLToken::kCharacter, token->GetType());
  EXPECT_EQ("Abc", String(token->Data().AsString()));
  tokenizer->ClearToken();

  token = tokenizer->NextToken(input);
  ASSERT_TRUE(token);
  EXPECT_EQ(HTMLToken::kComment, token->GetType());
  EXPECT_EQ("d", String(token->Data().AsString()));
  tokenizer->ClearToken();

  token = tokenizer->NextToken(input);
  ASSERT_TRUE(token);
  EXPECT_EQ(HTMLToken::kCharacter, token->GetType());
  EXPECT_EQ("Hi", String(token->Data().AsString()));
}

// Killswitch off: the tokenizer stalls and emits no token, as before.
TEST(HTMLTokenizerTest, IncompleteDoctypeKillswitchPreservesOldBehavior) {
  ScopedHTMLParserTruncatedMarkupDeclarationForTest disabled(false);

  test::TaskEnvironment task_environment;
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);

  SegmentedString input = ClosedInput("<!DOC>");
  EXPECT_EQ(nullptr, tokenizer->NextToken(input));
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
