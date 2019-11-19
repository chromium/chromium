// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_tree_builder_simulator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/parser/compact_html_token.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"

namespace blink {

TEST(HTMLTreeBuilderSimulatorTest, SelfClosingSVGFollowedByScript) {
  HTMLParserOptions options;
  HTMLTreeBuilderSimulator simulator(options);
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);
  SegmentedString input("<svg/><script></script>");
  HTMLToken token;
  EXPECT_TRUE(tokenizer->NextToken(input, token));
  EXPECT_EQ(HTMLTreeBuilderSimulator::kOtherToken,
            simulator.Simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));

  token.Clear();
  EXPECT_TRUE(tokenizer->NextToken(input, token));
  EXPECT_EQ(HTMLTreeBuilderSimulator::kValidScriptStart,
            simulator.Simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));

  EXPECT_EQ(HTMLTokenizer::kScriptDataState, tokenizer->GetState());

  token.Clear();
  EXPECT_TRUE(tokenizer->NextToken(input, token));
  EXPECT_EQ(HTMLTreeBuilderSimulator::kScriptEnd,
            simulator.Simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));
}

TEST(HTMLTreeBuilderSimulatorTest, SelfClosingMathFollowedByScript) {
  HTMLParserOptions options;
  HTMLTreeBuilderSimulator simulator(options);
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);
  SegmentedString input("<math/><script></script>");
  HTMLToken token;
  EXPECT_TRUE(tokenizer->NextToken(input, token));
  EXPECT_EQ(HTMLTreeBuilderSimulator::kOtherToken,
            simulator.Simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));

  token.Clear();
  EXPECT_TRUE(tokenizer->NextToken(input, token));
  EXPECT_EQ(HTMLTreeBuilderSimulator::kValidScriptStart,
            simulator.Simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));

  EXPECT_EQ(HTMLTokenizer::kScriptDataState, tokenizer->GetState());

  token.Clear();
  EXPECT_TRUE(tokenizer->NextToken(input, token));
  EXPECT_EQ(HTMLTreeBuilderSimulator::kScriptEnd,
            simulator.Simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));
}

TEST(HTMLTreeBuilderSimulatorTest, DetectInvalidScriptType) {
  HTMLParserOptions options;
  HTMLTreeBuilderSimulator simulator(options);
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);
  SegmentedString input("<script type=\"text/html\"></script>");
  HTMLToken token;
  EXPECT_TRUE(tokenizer->NextToken(input, token));
  EXPECT_NE(HTMLTreeBuilderSimulator::kValidScriptStart,
            simulator.Simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));

  token.Clear();
  EXPECT_TRUE(tokenizer->NextToken(input, token));
  EXPECT_EQ(HTMLTreeBuilderSimulator::kScriptEnd,
            simulator.Simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));
}

}  // namespace blink
