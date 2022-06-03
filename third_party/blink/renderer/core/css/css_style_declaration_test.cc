// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_style_declaration.h"

#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/document.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSStyleDeclarationTest, getPropertyShorthand) {
  css_test_helpers::TestStyleSheet sheet;

  sheet.AddCSSRules("div { padding: var(--p); }");
  ASSERT_TRUE(sheet.CssRules());
  ASSERT_EQ(1u, sheet.CssRules()->length());
  ASSERT_EQ(CSSRule::kStyleRule, sheet.CssRules()->item(0)->GetType());
  CSSStyleRule* style_rule = To<CSSStyleRule>(sheet.CssRules()->item(0));
  CSSStyleDeclaration* style = style_rule->style();
  ASSERT_TRUE(style);
  EXPECT_EQ(AtomicString(), style->GetPropertyShorthand("padding"));
}

TEST(CSSStyleDeclarationTest, ParsingRevertWithFeatureEnabled) {
  css_test_helpers::TestStyleSheet sheet;
  sheet.AddCSSRules("div { top: revert; --x: revert; }");
  ASSERT_TRUE(sheet.CssRules());
  ASSERT_EQ(1u, sheet.CssRules()->length());
  CSSStyleRule* style_rule = To<CSSStyleRule>(sheet.CssRules()->item(0));
  CSSStyleDeclaration* style = style_rule->style();
  ASSERT_TRUE(style);
  EXPECT_EQ("revert", style->getPropertyValue("top"));
  EXPECT_EQ("revert", style->getPropertyValue("--x"));

  // Test setProperty/getPropertyValue:

  DummyExceptionStateForTesting exception_state;

  style->SetPropertyInternal(CSSPropertyID::kLeft, "left", "revert", false,
                             SecureContextMode::kSecureContext,
                             exception_state);
  style->SetPropertyInternal(CSSPropertyID::kVariable, "--y", " revert", false,
                             SecureContextMode::kSecureContext,
                             exception_state);

  EXPECT_EQ("revert", style->getPropertyValue("left"));
  EXPECT_EQ("revert", style->getPropertyValue("--y"));
  EXPECT_FALSE(exception_state.HadException());
}

}  // namespace blink
