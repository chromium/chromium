// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule.h"

#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class StyleRuleTest : public PageTestBase {};

// Verifies that a StyleRuleScrollTimeline can be accessed even if
// the runtime flag CSSScrollTimeline is disabled.
//
// Note that this test can be removed when the CSSScrollTimeline flag is
// removed.
TEST_F(StyleRuleTest, StyleRuleScrollTimelineGettersWithoutFeature) {
  ScopedCSSScrollTimelineForTest scoped_feature(false);

  StyleRuleBase* base_rule = nullptr;

  {
    ScopedCSSScrollTimelineForTest scoped_feature(true);
    base_rule = css_test_helpers::ParseRule(GetDocument(), R"CSS(
        @scroll-timeline timeline {
          source: selector(#foo);
          start: 1px;
          end: 2px;
          time-range: 10s;
        }
      )CSS");
  }

  ASSERT_TRUE(base_rule);
  const auto* rule = To<StyleRuleScrollTimeline>(base_rule);

  // Don't crash:
  EXPECT_FALSE(rule->GetName().IsEmpty());
  EXPECT_TRUE(rule->GetSource());
  EXPECT_TRUE(rule->GetStart());
  EXPECT_TRUE(rule->GetEnd());
  EXPECT_TRUE(rule->GetTimeRange());
}

TEST_F(StyleRuleTest, StyleRuleScrollTimelineCopy) {
  ScopedCSSScrollTimelineForTest scoped_feature(true);

  auto* base_rule = css_test_helpers::ParseRule(GetDocument(), R"CSS(
      @scroll-timeline timeline {
        source: selector(#foo);
        start: 1px;
        end: 2px;
        time-range: 10s;
      }
    )CSS");

  ASSERT_TRUE(base_rule);
  auto* base_copy = base_rule->Copy();

  EXPECT_NE(base_rule, base_copy);
  EXPECT_EQ(base_rule->GetType(), base_copy->GetType());

  auto* rule = DynamicTo<StyleRuleScrollTimeline>(base_rule);
  auto* copy = DynamicTo<StyleRuleScrollTimeline>(base_copy);

  ASSERT_TRUE(rule);
  ASSERT_TRUE(copy);

  EXPECT_EQ(rule->GetName(), copy->GetName());
  EXPECT_EQ(rule->GetSource(), copy->GetSource());
  EXPECT_EQ(rule->GetOrientation(), copy->GetOrientation());
  EXPECT_EQ(rule->GetStart(), copy->GetStart());
  EXPECT_EQ(rule->GetEnd(), copy->GetEnd());
  EXPECT_EQ(rule->GetTimeRange(), copy->GetTimeRange());
}

TEST_F(StyleRuleTest, StyleRulePropertyCopy) {
  ScopedCSSVariables2AtPropertyForTest scoped_feature(true);

  auto* base_rule = css_test_helpers::ParseRule(GetDocument(), R"CSS(
      @property --foo {
        syntax: "<length>";
        initial-value: 0px;
        inherits: false;
      }
    )CSS");

  ASSERT_TRUE(base_rule);
  auto* base_copy = base_rule->Copy();

  EXPECT_NE(base_rule, base_copy);
  EXPECT_EQ(base_rule->GetType(), base_copy->GetType());

  auto* rule = DynamicTo<StyleRuleProperty>(base_rule);
  auto* copy = DynamicTo<StyleRuleProperty>(base_copy);

  ASSERT_TRUE(rule);
  ASSERT_TRUE(copy);

  EXPECT_EQ(rule->GetName(), copy->GetName());
  EXPECT_EQ(rule->GetSyntax(), copy->GetSyntax());
  EXPECT_EQ(rule->Inherits(), copy->Inherits());
  EXPECT_EQ(rule->GetInitialValue(), copy->GetInitialValue());
}

}  // namespace blink
