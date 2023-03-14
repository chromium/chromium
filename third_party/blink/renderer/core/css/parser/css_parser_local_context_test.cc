// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSParserLocalContextTest, Constructor) {
  EXPECT_FALSE(CSSParserLocalContext().UseAliasParsing());
  EXPECT_FALSE(CSSParserLocalContext().IsAnimationTainted());
  EXPECT_EQ(CSSPropertyID::kInvalid,
            CSSParserLocalContext().CurrentShorthand());
}

TEST(CSSParserLocalContextTest, WithAliasParsing) {
  const CSSParserLocalContext context;
  EXPECT_FALSE(context.WithAliasParsing(false).UseAliasParsing());
  EXPECT_TRUE(context.WithAliasParsing(true).UseAliasParsing());
}

TEST(CSSParserLocalContextTest, WithAnimationTainted) {
  const CSSParserLocalContext context;
  EXPECT_FALSE(context.WithAnimationTainted(false).IsAnimationTainted());
  EXPECT_TRUE(context.WithAnimationTainted(true).IsAnimationTainted());
}

TEST(CSSParserLocalContextTest, WithCurrentShorthand) {
  const CSSParserLocalContext context;
  const CSSPropertyID shorthand = CSSPropertyID::kBackground;
  EXPECT_EQ(shorthand,
            context.WithCurrentShorthand(shorthand).CurrentShorthand());
}

TEST(CSSParserLocalContextTest, LocalMutation) {
  CSSParserLocalContext context;
  context = context.WithAliasParsing(true);
  context = context.WithAnimationTainted(true);
  context = context.WithCurrentShorthand(CSSPropertyID::kBackground);

  // WithAliasParsing only changes that member.
  {
    auto local_context = context.WithAliasParsing(false);
    EXPECT_FALSE(local_context.UseAliasParsing());
    EXPECT_EQ(CSSPropertyID::kBackground, local_context.CurrentShorthand());
    EXPECT_TRUE(local_context.IsAnimationTainted());
  }

  // WithAnimationTainted only changes that member.
  {
    auto local_context = context.WithAnimationTainted(false);
    EXPECT_TRUE(local_context.UseAliasParsing());
    EXPECT_EQ(CSSPropertyID::kBackground, local_context.CurrentShorthand());
    EXPECT_FALSE(local_context.IsAnimationTainted());
  }

  // WithCurrentShorthand only changes that member.
  {
    auto local_context = context.WithCurrentShorthand(CSSPropertyID::kPadding);
    EXPECT_TRUE(local_context.UseAliasParsing());
    EXPECT_EQ(CSSPropertyID::kPadding, local_context.CurrentShorthand());
    EXPECT_TRUE(local_context.IsAnimationTainted());
  }
}

}  // namespace blink
