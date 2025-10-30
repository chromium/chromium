// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"

namespace blink {

TEST(MediaQueryParserTest, CustomQueryOnly) {
  StringView str = "(--custom-media)";
  CSSParserTokenStream stream(str);
  MediaQuerySet* media_query_set =
      MediaQueryParser::ParseMediaQuerySet(stream, nullptr);
  EXPECT_TRUE(stream.AtEnd());
  EXPECT_EQ(media_query_set->QueryVector().size(), 1);
  EXPECT_EQ(media_query_set->MediaText(), str);
}

TEST(MediaQueryParserTest, InvalidCustomQueryWithValue) {
  StringView str = "(--custom-media: 3)";
  CSSParserTokenStream stream(str);
  MediaQuerySet* media_query_set =
      MediaQueryParser::ParseMediaQuerySet(stream, nullptr);
  EXPECT_TRUE(stream.AtEnd());
  EXPECT_EQ(media_query_set->QueryVector().size(), 1);
}

TEST(MediaQueryParserTest, InvalidCustomQueryWithRange) {
  StringView str = "(--custom-media = 3)";
  CSSParserTokenStream stream(str);
  MediaQuerySet* media_query_set =
      MediaQueryParser::ParseMediaQuerySet(stream, nullptr);
  EXPECT_TRUE(stream.AtEnd());
  EXPECT_EQ(media_query_set->QueryVector().size(), 1);
}

TEST(MediaQueryParserTest, SimpleWithCustomQuery) {
  StringView str = "(--custom-media) and (color)";
  CSSParserTokenStream stream(str);
  MediaQuerySet* media_query_set =
      MediaQueryParser::ParseMediaQuerySet(stream, nullptr);
  EXPECT_TRUE(stream.AtEnd());
  EXPECT_EQ(media_query_set->QueryVector().size(), 1);
  EXPECT_EQ(media_query_set->MediaText(), str);
}

TEST(MediaQueryParserTest, ListWithCustomQuery) {
  StringView str = "(color) or (--custom-media), (hover)";
  CSSParserTokenStream stream(str);
  MediaQuerySet* media_query_set =
      MediaQueryParser::ParseMediaQuerySet(stream, nullptr);
  EXPECT_TRUE(stream.AtEnd());
  EXPECT_EQ(media_query_set->QueryVector().size(), 2);
  EXPECT_EQ(media_query_set->MediaText(), str);
}

TEST(MediaQueryParserTest, ListWithCustomQueryAndMediaType) {
  StringView str =
      "screen and (--custom-media), print and (--custom-media), not all and "
      "(--custom-media)";
  CSSParserTokenStream stream(str);
  MediaQuerySet* media_query_set =
      MediaQueryParser::ParseMediaQuerySet(stream, nullptr);
  EXPECT_TRUE(stream.AtEnd());
  EXPECT_EQ(media_query_set->QueryVector().size(), 3);
  EXPECT_EQ(media_query_set->MediaText(), str);
}

}  // namespace blink
