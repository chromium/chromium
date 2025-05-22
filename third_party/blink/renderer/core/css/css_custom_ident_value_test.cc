// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"

#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSCustomIdentValueTest : public PageTestBase {
 public:
  const CSSCustomIdentValue* ParseCustomIdent(String value) {
    return DynamicTo<CSSCustomIdentValue>(
        css_test_helpers::ParseValue(GetDocument(), "<custom-ident>", value));
  }
  MediaValues* CreateMediaValues() {
    MediaValues* values = MediaValues::CreateDynamicIfFrameExists(&GetFrame());
    CHECK(values);
    return values;
  }
};

namespace {}  // namespace

TEST_F(CSSCustomIdentValueTest, ResolveSelf) {
  const CSSCustomIdentValue* custom_ident = ParseCustomIdent("something");
  ASSERT_TRUE(custom_ident);
  EXPECT_EQ(custom_ident, custom_ident->Resolve(*CreateMediaValues()));
}

TEST_F(CSSCustomIdentValueTest, ResolveIdentFunction) {
  const CSSCustomIdentValue* custom_ident = ParseCustomIdent("ident(foo 3)");
  ASSERT_TRUE(custom_ident);
  const CSSCustomIdentValue* resolved_ident =
      custom_ident->Resolve(*CreateMediaValues());
  ASSERT_TRUE(resolved_ident);
  EXPECT_EQ("foo3", resolved_ident->CustomCSSText());
}

TEST_F(CSSCustomIdentValueTest, ResolveIdentFunctionUnscoped) {
  const CSSCustomIdentValue* custom_ident = ParseCustomIdent("ident(foo 3)");
  ASSERT_TRUE(custom_ident);
  EXPECT_FALSE(custom_ident->IsScopedValue());
  EXPECT_FALSE(custom_ident->GetTreeScope());
  // An unscoped value should remain unscoped:
  const CSSCustomIdentValue* resolved_ident =
      custom_ident->Resolve(*CreateMediaValues());
  ASSERT_TRUE(resolved_ident);
  EXPECT_EQ("foo3", resolved_ident->CustomCSSText());
  EXPECT_FALSE(resolved_ident->IsScopedValue());
  EXPECT_FALSE(resolved_ident->GetTreeScope());
}

TEST_F(CSSCustomIdentValueTest, ResolveIdentFunctionScoped) {
  const CSSCustomIdentValue* custom_ident = ParseCustomIdent("ident(foo 3)");
  ASSERT_TRUE(custom_ident);
  custom_ident = DynamicTo<CSSCustomIdentValue>(
      custom_ident->EnsureScopedValue(/*tree_scope=*/&GetDocument()));
  EXPECT_TRUE(custom_ident->IsScopedValue());
  EXPECT_EQ(static_cast<TreeScope*>(&GetDocument()),
            custom_ident->GetTreeScope());
  // A scoped value should remain scoped:
  const CSSCustomIdentValue* resolved_ident =
      custom_ident->Resolve(*CreateMediaValues());
  ASSERT_TRUE(resolved_ident);
  EXPECT_EQ("foo3", resolved_ident->CustomCSSText());
  EXPECT_TRUE(resolved_ident->IsScopedValue());
  EXPECT_EQ(static_cast<TreeScope*>(&GetDocument()),
            resolved_ident->GetTreeScope());
}

}  // namespace blink
