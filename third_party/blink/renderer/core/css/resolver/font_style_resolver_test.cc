// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/font_builder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

class FontStyleResolverTest : public testing::Test {
 public:
  FontStyleResolverTest() = default;

 private:
  // To destroy garbage-collected objects, we need TaskEnvironment. It prepares
  // `v8::isolate` and runs garbege collector at its destructor.
  test::TaskEnvironment task_environment_;
};

}  // namespace

TEST_F(FontStyleResolverTest, Simple) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFont, "15px Ahem", true);

  auto maybe = FontStyleResolver::ComputeFont(*style, nullptr);
  ASSERT_TRUE(maybe.has_value());
  FontDescription desc = maybe.value();

  EXPECT_EQ(desc.SpecifiedSize(), 15);
  EXPECT_EQ(desc.ComputedSize(), 15);
  EXPECT_EQ(desc.Family().FamilyName(), "Ahem");
}

TEST_F(FontStyleResolverTest, InvalidSize) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFont, "-1px Ahem", true);

  auto maybe = FontStyleResolver::ComputeFont(*style, nullptr);
  ASSERT_TRUE(maybe.has_value());
  FontDescription desc = maybe.value();

  EXPECT_EQ(desc.Family().FamilyName(), nullptr);
  EXPECT_EQ(desc.SpecifiedSize(), 0);
  EXPECT_EQ(desc.ComputedSize(), 0);
}

TEST_F(FontStyleResolverTest, InvalidWeight) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFont, "wrong 1px Ahem", true);

  auto maybe = FontStyleResolver::ComputeFont(*style, nullptr);
  ASSERT_TRUE(maybe.has_value());
  FontDescription desc = maybe.value();

  EXPECT_EQ(desc.Family().FamilyName(), nullptr);
  EXPECT_EQ(desc.SpecifiedSize(), 0);
  EXPECT_EQ(desc.ComputedSize(), 0);
}

TEST_F(FontStyleResolverTest, InvalidEverything) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFont,
                        "wrong wrong wrong 1px Ahem", true);

  auto maybe = FontStyleResolver::ComputeFont(*style, nullptr);
  ASSERT_TRUE(maybe.has_value());
  FontDescription desc = maybe.value();

  EXPECT_EQ(desc.Family().FamilyName(), nullptr);
  EXPECT_EQ(desc.SpecifiedSize(), 0);
  EXPECT_EQ(desc.ComputedSize(), 0);
}

TEST_F(FontStyleResolverTest, RelativeSize) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFont, "italic 2ex Ahem", true);

  auto maybe = FontStyleResolver::ComputeFont(*style, nullptr);
  ASSERT_TRUE(maybe.has_value());
  FontDescription desc = maybe.value();

  EXPECT_EQ(desc.Family().FamilyName(), "Ahem");
  EXPECT_EQ(desc.SpecifiedSize(), 10);
  EXPECT_EQ(desc.ComputedSize(), 10);
}

TEST_F(FontStyleResolverTest, ElementDependentCalcWeightSkipped) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFontFamily, "Ahem", true);
  CSSParser::ParseValue(style, CSSPropertyID::kFontWeight,
                        "calc(sibling-index())", true);

  auto maybe = FontStyleResolver::ComputeFont(*style, nullptr);
  EXPECT_FALSE(maybe.has_value());
}

TEST_F(FontStyleResolverTest, ElementDependentCalcStretchSkipped) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFontFamily, "Ahem", true);
  CSSParser::ParseValue(style, CSSPropertyID::kFontStretch,
                        "calc(sibling-index() * 1%)", true);

  auto maybe = FontStyleResolver::ComputeFont(*style, nullptr);
  EXPECT_FALSE(maybe.has_value());
}

TEST_F(FontStyleResolverTest, ElementDependentCalcStyleObliqueZeroDegrees) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFontFamily, "Ahem", true);
  CSSParser::ParseValue(style, CSSPropertyID::kFontStyle,
                        "oblique calc(sibling-index() * 1deg)", true);

  auto maybe = FontStyleResolver::ComputeFont(*style, nullptr);
  EXPECT_FALSE(maybe.has_value());
}

TEST_F(FontStyleResolverTest, ElementDependentCalcFontSizeSkipped) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFontFamily, "Ahem", true);
  CSSParser::ParseValue(style, CSSPropertyID::kFontSize,
                        "calc(sibling-index() * 1px)", true);

  auto maybe = FontStyleResolver::ComputeFont(*style, nullptr);
  EXPECT_FALSE(maybe.has_value());
}

}  // namespace blink
