// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class AtRuleDescriptorParserTest : public PageTestBase {};

TEST_F(AtRuleDescriptorParserTest, NoUseCountUACounterStyle) {
  SetBodyInnerHTML(R"HTML(
    <ol>
      <!-- Basic counter styles -->
      <li style="list-style-type: decimal">decimal</li>
      <li style="list-style-type: disc">disc</li>
      <!-- Counter style with additive-symbols -->
      <li style="list-style-type: upper-roman">upper-roman</li>
      <!-- Counter style with fallback ->
      <li style="list-style-type: simp-chinese-informal">chinese</li>
    </ol>
  )HTML");

  EXPECT_FALSE(
      GetDocument().IsUseCounted(mojom::WebFeature::kCSSAtRuleCounterStyle));
  EXPECT_FALSE(GetDocument().IsPropertyCounted(CSSPropertyID::kSystem));
  EXPECT_FALSE(GetDocument().IsPropertyCounted(CSSPropertyID::kSymbols));
  EXPECT_FALSE(
      GetDocument().IsPropertyCounted(CSSPropertyID::kAdditiveSymbols));
  EXPECT_FALSE(GetDocument().IsPropertyCounted(CSSPropertyID::kPrefix));
  EXPECT_FALSE(GetDocument().IsPropertyCounted(CSSPropertyID::kSuffix));
  EXPECT_FALSE(GetDocument().IsPropertyCounted(CSSPropertyID::kNegative));
  EXPECT_FALSE(GetDocument().IsPropertyCounted(CSSPropertyID::kRange));
  EXPECT_FALSE(GetDocument().IsPropertyCounted(CSSPropertyID::kPad));
  EXPECT_FALSE(GetDocument().IsPropertyCounted(CSSPropertyID::kFallback));
  EXPECT_FALSE(GetDocument().IsPropertyCounted(CSSPropertyID::kSpeakAs));
}

TEST_F(AtRuleDescriptorParserTest, UseCountCounterStyleDescriptors) {
  InsertStyleElement(R"CSS(
    @counter-style foo {
      system: symbolic;
      symbols: 'X' 'Y' 'Z';
      prefix: '<';
      suffix: '>';
      negative: '~';
      range: 0 infinite;
      pad: 3 'O';
      fallback: upper-alpha;
      speak-as: numbers;
    }
  )CSS");

  InsertStyleElement(R"CSS(
    @counter-style bar {
      system: additive;
      additive-symbols: 1 'I', 0 'O';
    }
  )CSS");

  EXPECT_TRUE(
      GetDocument().IsUseCounted(mojom::WebFeature::kCSSAtRuleCounterStyle));
  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kSystem));
  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kSymbols));
  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kAdditiveSymbols));
  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kPrefix));
  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kSuffix));
  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kNegative));
  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kRange));
  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kPad));
  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kFallback));
  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kSpeakAs));
}

TEST_F(AtRuleDescriptorParserTest, UseCountFontMetricOverrideDescriptors) {
  InsertStyleElement(R"CSS(
    @font-face {
      font-family: foo;
      src: url(foo.woff);
      ascent-override: 80%;
      descent-override: 20%;
      line-gap-override: 0%;
      size-adjust: 110%;
    }
  )CSS");

  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kAscentOverride));
  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kDescentOverride));
  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kLineGapOverride));
  EXPECT_TRUE(GetDocument().IsPropertyCounted(CSSPropertyID::kSizeAdjust));
}

}  // namespace blink
