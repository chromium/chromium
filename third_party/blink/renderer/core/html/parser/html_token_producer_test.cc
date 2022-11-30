// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_token_producer.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/html/parser/atomic_html_token.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_options.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

class HTMLTokenProducerTest : public testing::Test {
  void SetUp() override {
    testing::Test::SetUp();
    scoped_feature_list_.InitAndEnableFeature(features::kThreadedHtmlTokenizer);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HTMLTokenProducerTest, Basic) {
  HTMLInputStream input_stream;
  HTMLTokenProducer producer(input_stream, HTMLParserOptions(), true,
                             HTMLTokenizer::kDataState);
  String string("<body>a body");
  input_stream.AppendToEnd(SegmentedString(string));
  producer.AppendToEnd(string);
  EXPECT_TRUE(producer.IsUsingBackgroundProducer());
  HTMLToken* token = producer.ParseNextToken();
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kStartTag, token->GetType());
  EXPECT_EQ(html_names::kBodyTag, token->GetName().AsAtomicString());

  token = producer.ParseNextToken();
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kCharacter, token->GetType());
  EXPECT_EQ(String("a body"), token->Characters().AsString());

  // No more tokens, because need end of file.
  token = producer.ParseNextToken();
  EXPECT_FALSE(token);

  input_stream.MarkEndOfFile();
  producer.MarkEndOfFile();
  token = producer.ParseNextToken();
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kEndOfFile, token->GetType());
  EXPECT_TRUE(producer.IsUsingBackgroundProducer());
}

TEST_F(HTMLTokenProducerTest, TagSplitAcrossSegmentReachesEnd) {
  HTMLInputStream input_stream;
  HTMLTokenProducer producer(input_stream, HTMLParserOptions(), true,
                             HTMLTokenizer::kDataState);
  String string("<bo");
  input_stream.AppendToEnd(SegmentedString(string));
  producer.AppendToEnd(string);
  ASSERT_FALSE(producer.ParseNextToken());

  input_stream.AppendToEnd(SegmentedString("dy>"));
  producer.AppendToEnd(String("dy>"));
  input_stream.MarkEndOfFile();
  producer.MarkEndOfFile();
  // Read all the remaining tokens.
  while (producer.ParseNextToken())
    ;
  // Should be at the end of input.
  EXPECT_EQ(0u, input_stream.length());
}

TEST_F(HTMLTokenProducerTest, TagSplitAcrossSegments) {
  HTMLInputStream input_stream;
  HTMLTokenProducer producer(input_stream, HTMLParserOptions(), true,
                             HTMLTokenizer::kDataState);
  String string("<bo");
  input_stream.AppendToEnd(SegmentedString(string));
  producer.AppendToEnd(string);
  ASSERT_FALSE(producer.ParseNextToken());

  input_stream.AppendToEnd(SegmentedString("dy>"));
  producer.AppendToEnd(String("dy>"));
  HTMLToken* token = producer.ParseNextToken();
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kStartTag, token->GetType());
  EXPECT_EQ(html_names::kBodyTag, token->GetName().AsAtomicString());
  EXPECT_TRUE(producer.IsUsingBackgroundProducer());
}

TEST_F(HTMLTokenProducerTest, AbortWithTagSplit) {
  HTMLInputStream input_stream;
  HTMLTokenProducer producer(input_stream, HTMLParserOptions(), true,
                             HTMLTokenizer::kDataState);
  String string("<bo");
  input_stream.AppendToEnd(SegmentedString(string));
  producer.AppendToEnd(string);
  ASSERT_FALSE(producer.ParseNextToken());

  producer.AbortBackgroundParsingForDocumentWrite();
  EXPECT_FALSE(producer.IsUsingBackgroundProducer());

  input_stream.AppendToEnd(SegmentedString("dy>"));
  producer.AppendToEnd(String("dy>"));
  HTMLToken* token = producer.ParseNextToken();
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kStartTag, token->GetType());
  EXPECT_EQ(html_names::kBodyTag, token->GetName().AsAtomicString());
}

TEST_F(HTMLTokenProducerTest, AbortOnBoundary) {
  HTMLInputStream input_stream;
  HTMLTokenProducer producer(input_stream, HTMLParserOptions(), true,
                             HTMLTokenizer::kDataState);
  String string("<body>text");
  input_stream.AppendToEnd(SegmentedString(string));
  producer.AppendToEnd(string);
  HTMLToken* token = producer.ParseNextToken();
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kStartTag, token->GetType());
  EXPECT_EQ(html_names::kBodyTag, token->GetName().AsAtomicString());

  producer.AbortBackgroundParsingForDocumentWrite();
  EXPECT_FALSE(producer.IsUsingBackgroundProducer());
  token = producer.ParseNextToken();
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kCharacter, token->GetType());
  EXPECT_EQ(String("text"), token->Characters().AsString());
}

TEST_F(HTMLTokenProducerTest, AbortOnNullChar) {
  HTMLInputStream input_stream;
  HTMLTokenProducer producer(input_stream, HTMLParserOptions(), true,
                             HTMLTokenizer::kDataState);
  String string("<body>t\0ext</body>", 18u);
  input_stream.AppendToEnd(SegmentedString(string));
  producer.AppendToEnd(string);
  HTMLToken* token = producer.ParseNextToken();
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kStartTag, token->GetType());
  EXPECT_EQ(html_names::kBodyTag, token->GetName().AsAtomicString());

  EXPECT_TRUE(producer.IsUsingBackgroundProducer());
  producer.ClearToken();
  token = producer.ParseNextToken();
  EXPECT_FALSE(producer.IsUsingBackgroundProducer());
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kCharacter, token->GetType());
  EXPECT_EQ(String("text"), token->Characters().AsString());

  producer.ClearToken();
  token = producer.ParseNextToken();
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kEndTag, token->GetType());
  EXPECT_EQ(html_names::kBodyTag, token->GetName().AsAtomicString());
}

TEST_F(HTMLTokenProducerTest, StateMismatch) {
  HTMLInputStream input_stream;
  HTMLTokenProducer producer(input_stream, HTMLParserOptions(), true,
                             HTMLTokenizer::kDataState);
  String string("<svg><script>x");
  input_stream.AppendToEnd(SegmentedString(string));
  producer.AppendToEnd(string);
  HTMLToken* token = producer.ParseNextToken();
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kStartTag, token->GetType());

  EXPECT_TRUE(producer.IsUsingBackgroundProducer());
  producer.ClearToken();
  token = producer.ParseNextToken();
  EXPECT_TRUE(producer.IsUsingBackgroundProducer());
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kStartTag, token->GetType());
  EXPECT_EQ(String("script"), token->GetName().AsString());

  EXPECT_TRUE(producer.IsUsingBackgroundProducer());
  producer.ClearToken();
  token = producer.ParseNextToken();
  EXPECT_FALSE(producer.IsUsingBackgroundProducer());
  ASSERT_TRUE(token);
  EXPECT_EQ(HTMLToken::kCharacter, token->GetType());
}

TEST_F(HTMLTokenProducerTest, CurrentColumn) {
  HTMLInputStream input_stream;
  HTMLTokenProducer producer(input_stream, HTMLParserOptions(), true,
                             HTMLTokenizer::kDataState);
  String string("<body>\nx<div>");
  input_stream.AppendToEnd(SegmentedString(string));
  producer.AppendToEnd(string);
  HTMLToken* token = producer.ParseNextToken();
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kStartTag, token->GetType());

  EXPECT_TRUE(producer.IsUsingBackgroundProducer());
  producer.ClearToken();
  token = producer.ParseNextToken();
  EXPECT_TRUE(producer.IsUsingBackgroundProducer());
  ASSERT_TRUE(token);
  ASSERT_EQ(HTMLToken::kCharacter, token->GetType());
  EXPECT_EQ(1, input_stream.Current().CurrentColumn().ZeroBasedInt());
  EXPECT_EQ(1, input_stream.Current().CurrentLine().ZeroBasedInt());
}

}  // namespace blink
