// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

using css_parsing_utils::ConsumeIdSelector;

TEST(CSSParsingUtilsTest, BasicShapeUseCount) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSBasicShape;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<style>span { shape-outside: circle(); }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST(CSSParsingUtilsTest, Revert) {
  EXPECT_TRUE(css_parsing_utils::IsCSSWideKeyword(CSSValueID::kRevert));
  EXPECT_TRUE(css_parsing_utils::IsCSSWideKeyword("revert"));
}

TEST(CSSParsingUtilsTest, ConsumeIdSelector) {
  {
    String text = "#foo";
    auto tokens = CSSTokenizer(text).TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    EXPECT_EQ("#foo", ConsumeIdSelector(range)->CssText());
  }
  {
    String text = "#bar  ";
    auto tokens = CSSTokenizer(text).TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    EXPECT_EQ("#bar", ConsumeIdSelector(range)->CssText());
    EXPECT_TRUE(range.AtEnd())
        << "ConsumeIdSelector cleans up trailing whitespace";
  }

  {
    String text = "#123";
    auto tokens = CSSTokenizer(text).TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    ASSERT_TRUE(range.Peek().GetType() == kHashToken &&
                range.Peek().GetHashTokenType() == kHashTokenUnrestricted);
    EXPECT_FALSE(ConsumeIdSelector(range))
        << "kHashTokenUnrestricted is not a valid <id-selector>";
  }
  {
    String text = "#";
    auto tokens = CSSTokenizer(text).TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    EXPECT_FALSE(ConsumeIdSelector(range));
  }
  {
    String text = " #foo";
    auto tokens = CSSTokenizer(text).TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    EXPECT_FALSE(ConsumeIdSelector(range))
        << "ConsumeIdSelector does not accept preceding whitespace";
    EXPECT_EQ(kWhitespaceToken, range.Peek().GetType());
  }
  {
    String text = "foo";
    auto tokens = CSSTokenizer(text).TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    EXPECT_FALSE(ConsumeIdSelector(range));
  }
  {
    String text = "##";
    auto tokens = CSSTokenizer(text).TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    EXPECT_FALSE(ConsumeIdSelector(range));
  }
  {
    String text = "10px";
    auto tokens = CSSTokenizer(text).TokenizeToEOF();
    CSSParserTokenRange range(tokens);
    EXPECT_FALSE(ConsumeIdSelector(range));
  }
}

}  // namespace blink
