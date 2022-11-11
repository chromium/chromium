// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_options.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"

#include <memory>

namespace blink {

class HTMLTokenizerTest : public testing::Test {
 public:
  static void SetAppropriateEndTagName(HTMLTokenizer& tokenizer,
                                       const String& string) {
    tokenizer.appropriate_end_tag_name_.clear();
    tokenizer.appropriate_end_tag_name_.Append(string);
  }

  static String GetAppropriateEndTagName(HTMLTokenizer& tokenizer) {
    return tokenizer.appropriate_end_tag_name_.AsString();
  }

  static void SetBufferedEndTagName(HTMLTokenizer& tokenizer,
                                    const String& string) {
    tokenizer.buffered_end_tag_name_.clear();
    tokenizer.buffered_end_tag_name_.Append(string.Span8());
  }

  static String GetBufferedEndTagName(HTMLTokenizer& tokenizer) {
    return tokenizer.buffered_end_tag_name_.AsString();
  }
};

// This is a regression test for crbug.com/619141
TEST_F(HTMLTokenizerTest, ZeroOffsetAttributeNameRange) {
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);
  SegmentedString input("<script ");
  EXPECT_EQ(nullptr, tokenizer->NextToken(input));

  SegmentedString input2("type='javascript'");
  // Below should not fail ASSERT
  EXPECT_EQ(nullptr, tokenizer->NextToken(input2));
}

TEST_F(HTMLTokenizerTest, SaveAndRestoreSnapshot) {
  HTMLParserOptions options;
  HTMLTokenizer tokenizer(options);
  String end_tag_name("end-tag");
  String buffered_end_tag_name("buffered-end-tag");
  SetAppropriateEndTagName(tokenizer, end_tag_name);
  SetBufferedEndTagName(tokenizer, buffered_end_tag_name);
  HTMLTokenizerSnapshot snapshot;
  tokenizer.GetSnapshot(snapshot);

  HTMLTokenizer tokenizer2(options);
  tokenizer2.RestoreSnapshot(snapshot);
  EXPECT_EQ(end_tag_name, GetAppropriateEndTagName(tokenizer2));
  EXPECT_EQ(buffered_end_tag_name, GetBufferedEndTagName(tokenizer2));

  // Append an empty snapshot, which should clear the data.
  tokenizer2.RestoreSnapshot(HTMLTokenizerSnapshot());
  EXPECT_TRUE(GetAppropriateEndTagName(tokenizer2).empty());
  EXPECT_TRUE(GetBufferedEndTagName(tokenizer2).empty());
}

}  // namespace blink
