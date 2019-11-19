// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

TEST(FontStyleResolverTest, Simple) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFont, "15px Ahem", true,
                        SecureContextMode::kInsecureContext);

  FontDescription desc = FontStyleResolver::ComputeFont(*style, nullptr);

  EXPECT_EQ(desc.SpecifiedSize(), 15);
  EXPECT_EQ(desc.ComputedSize(), 15);
  EXPECT_EQ(desc.Family().Family(), "Ahem");
}

TEST(FontStyleResolverTest, InvalidSize) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFont, "-1px Ahem", true,
                        SecureContextMode::kInsecureContext);

  FontDescription desc = FontStyleResolver::ComputeFont(*style, nullptr);

  EXPECT_EQ(desc.Family().Family(), nullptr);
  EXPECT_EQ(desc.SpecifiedSize(), 0);
  EXPECT_EQ(desc.ComputedSize(), 0);
}

TEST(FontStyleResolverTest, InvalidWeight) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFont, "wrong 1px Ahem", true,
                        SecureContextMode::kInsecureContext);

  FontDescription desc = FontStyleResolver::ComputeFont(*style, nullptr);

  EXPECT_EQ(desc.Family().Family(), nullptr);
  EXPECT_EQ(desc.SpecifiedSize(), 0);
  EXPECT_EQ(desc.ComputedSize(), 0);
}

TEST(FontStyleResolverTest, InvalidEverything) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFont,
                        "wrong wrong wrong 1px Ahem", true,
                        SecureContextMode::kInsecureContext);

  FontDescription desc = FontStyleResolver::ComputeFont(*style, nullptr);

  EXPECT_EQ(desc.Family().Family(), nullptr);
  EXPECT_EQ(desc.SpecifiedSize(), 0);
  EXPECT_EQ(desc.ComputedSize(), 0);
}

TEST(FontStyleResolverTest, RelativeSize) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(style, CSSPropertyID::kFont, "italic 2ex Ahem", true,
                        SecureContextMode::kInsecureContext);

  FontDescription desc = FontStyleResolver::ComputeFont(*style, nullptr);

  EXPECT_EQ(desc.Family().Family(), "Ahem");
  EXPECT_EQ(desc.SpecifiedSize(), 10);
  EXPECT_EQ(desc.ComputedSize(), 10);
}

}  // namespace blink
