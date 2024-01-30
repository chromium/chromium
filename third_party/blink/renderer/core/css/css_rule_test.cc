// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_style_sheet_init.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_supports_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CSSRuleTest : public PageTestBase {
 public:
  bool IsUseCounted(WebFeature feature) {
    return GetDocument().IsUseCounted(feature);
  }

  void ClearUseCounter(WebFeature feature) {
    GetDocument().ClearUseCounterForTesting(feature);
    DCHECK(!IsUseCounted(feature));
  }

  StyleRule* MakeNonSignalingRule() {
    return DynamicTo<StyleRule>(
        css_test_helpers::ParseRule(GetDocument(), "body { color: green; }"));
  }

  StyleRule* MakeSignalingRule() {
    StyleRule* non_signaling_rule = MakeNonSignalingRule();
    return css_test_helpers::MakeSignalingRule(
        std::move(*non_signaling_rule),
        CSSSelector::Signal::kBareDeclarationShift);
  }
};

TEST_F(CSSRuleTest, NoUseCountSignalingChildModified_StyleRule) {
  DummyExceptionStateForTesting exception_state;
  auto* stylesheet = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), exception_state);
  StyleRule* style_rule = MakeNonSignalingRule();
  style_rule->AddChildRule(MakeNonSignalingRule());
  style_rule->AddChildRule(MakeNonSignalingRule());
  auto* css_style_rule = MakeGarbageCollected<CSSStyleRule>(
      style_rule, stylesheet, /* position_hint */ 0);

  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));

  ClearUseCounter(WebFeature::kCSSRuleWithSignalingChildModified);
  css_style_rule->insertRule(GetDocument().GetExecutionContext(),
                             "div { left: 10px; }", 0, exception_state);
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));

  ClearUseCounter(WebFeature::kCSSRuleWithSignalingChildModified);
  css_style_rule->deleteRule(0, exception_state);
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));
}

TEST_F(CSSRuleTest, UseCountSignalingChildModified_StyleRule) {
  DummyExceptionStateForTesting exception_state;
  auto* stylesheet = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), exception_state);
  StyleRule* style_rule = MakeNonSignalingRule();
  style_rule->AddChildRule(MakeNonSignalingRule());
  style_rule->AddChildRule(MakeSignalingRule());
  auto* css_style_rule = MakeGarbageCollected<CSSStyleRule>(
      style_rule, stylesheet, /* position_hint */ 0);

  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));

  ClearUseCounter(WebFeature::kCSSRuleWithSignalingChildModified);
  css_style_rule->insertRule(GetDocument().GetExecutionContext(),
                             "div { left: 10px; }", 0, exception_state);
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));

  // This removes the signaling rule. This counts as a mutation that triggers
  // the use-counter.
  ClearUseCounter(WebFeature::kCSSRuleWithSignalingChildModified);
  css_style_rule->deleteRule(css_style_rule->length() - 1, exception_state);
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));

  // The use-counter should trigger if a signaling rule has ever been seen,
  // even if it doesn't exist anymore.
  ClearUseCounter(WebFeature::kCSSRuleWithSignalingChildModified);
  css_style_rule->insertRule(GetDocument().GetExecutionContext(),
                             "div { left: 10px; }", 0, exception_state);
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));

  // No count for mutations that don't affect child rules.
  ClearUseCounter(WebFeature::kCSSRuleWithSignalingChildModified);
  css_style_rule->setSelectorText(GetDocument().GetExecutionContext(), "p");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));
}

TEST_F(CSSRuleTest, NoUseCountSignalingChildModified_GroupingRule) {
  DummyExceptionStateForTesting exception_state;
  auto* stylesheet = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), exception_state);
  HeapVector<Member<StyleRuleBase>> child_rules;
  child_rules.push_back(MakeNonSignalingRule());
  child_rules.push_back(MakeNonSignalingRule());
  auto* supports_rule = MakeGarbageCollected<StyleRuleSupports>(
      "width:100px",
      /* condition_is_supported */ true, std::move(child_rules));
  auto* css_supports_rule =
      MakeGarbageCollected<CSSSupportsRule>(supports_rule, stylesheet);

  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));

  ClearUseCounter(WebFeature::kCSSRuleWithSignalingChildModified);
  css_supports_rule->insertRule(GetDocument().GetExecutionContext(),
                                "div { left: 10px; }", 0, exception_state);
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));

  ClearUseCounter(WebFeature::kCSSRuleWithSignalingChildModified);
  css_supports_rule->deleteRule(0, exception_state);
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));
}

TEST_F(CSSRuleTest, UseCountSignalingChildModified_GroupingRule) {
  DummyExceptionStateForTesting exception_state;
  auto* stylesheet = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), exception_state);
  HeapVector<Member<StyleRuleBase>> child_rules;
  child_rules.push_back(MakeNonSignalingRule());
  child_rules.push_back(MakeSignalingRule());
  auto* supports_rule = MakeGarbageCollected<StyleRuleSupports>(
      "width:100px",
      /* condition_is_supported */ true, std::move(child_rules));
  auto* css_supports_rule =
      MakeGarbageCollected<CSSSupportsRule>(supports_rule, stylesheet);

  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));

  ClearUseCounter(WebFeature::kCSSRuleWithSignalingChildModified);
  css_supports_rule->insertRule(GetDocument().GetExecutionContext(),
                                "div { left: 10px; }", 0, exception_state);
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));

  // This removes the signaling rule. This counts as a mutation that triggers
  // the use-counter.
  ClearUseCounter(WebFeature::kCSSRuleWithSignalingChildModified);
  css_supports_rule->deleteRule(css_supports_rule->length() - 1,
                                exception_state);
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));

  // The use-counter should trigger if a signaling rule has ever been seen,
  // even if it doesn't exist anymore.
  ClearUseCounter(WebFeature::kCSSRuleWithSignalingChildModified);
  css_supports_rule->insertRule(GetDocument().GetExecutionContext(),
                                "div { left: 10px; }", 0, exception_state);
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));

  // No count for mutations that don't affect child rules.
  ClearUseCounter(WebFeature::kCSSRuleWithSignalingChildModified);
  css_supports_rule->SetConditionText(GetDocument().GetExecutionContext(),
                                      "width:200px");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSRuleWithSignalingChildModified));
}

}  // namespace blink
