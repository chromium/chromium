// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_style_sheet.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_style_sheet.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_style_sheet_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observable_array_css_style_sheet.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_medialist_string.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

using CSSStyleSheetTest = PageTestBase;

TEST_F(CSSStyleSheetTest,
       CSSStyleSheetConstructionWithNonEmptyCSSStyleSheetInit) {
  DummyExceptionStateForTesting exception_state;
  CSSStyleSheetInit* init = CSSStyleSheetInit::Create();
  init->setMedia(
      MakeGarbageCollected<V8UnionMediaListOrString>("screen, print"));
  init->setAlternate(true);
  init->setDisabled(true);
  CSSStyleSheet* sheet =
      CSSStyleSheet::Create(GetDocument(), init, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  EXPECT_TRUE(sheet->href().IsNull());
  EXPECT_EQ(sheet->parentStyleSheet(), nullptr);
  EXPECT_EQ(sheet->ownerNode(), nullptr);
  EXPECT_EQ(sheet->ownerRule(), nullptr);
  EXPECT_EQ(sheet->media()->length(), 2U);
  EXPECT_EQ(sheet->media()->mediaText(nullptr), init->media()->GetAsString());
  EXPECT_TRUE(sheet->AlternateFromConstructor());
  EXPECT_TRUE(sheet->disabled());
  EXPECT_EQ(sheet->cssRules(exception_state)->length(), 0U);
  ASSERT_FALSE(exception_state.HadException());
}

TEST_F(CSSStyleSheetTest,
       GarbageCollectedShadowRootsRemovedFromAdoptedTreeScopes) {
  SetBodyInnerHTML("<div id='host_a'></div><div id='host_b'></div>");
  auto* host_a = GetElementById("host_a");
  auto& shadow_a = host_a->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  auto* host_b = GetElementById("host_b");
  auto& shadow_b = host_b->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  DummyExceptionStateForTesting exception_state;
  CSSStyleSheetInit* init = CSSStyleSheetInit::Create();
  CSSStyleSheet* sheet =
      CSSStyleSheet::Create(GetDocument(), init, exception_state);

  HeapVector<Member<CSSStyleSheet>> adopted_sheets;
  adopted_sheets.push_back(sheet);
  shadow_a.SetAdoptedStyleSheetsForTesting(adopted_sheets);
  shadow_b.SetAdoptedStyleSheetsForTesting(adopted_sheets);

  EXPECT_EQ(sheet->adopted_tree_scopes_.size(), 2u);
  EXPECT_EQ(shadow_a.AdoptedStyleSheets()->size(), 1u);
  EXPECT_EQ(shadow_b.AdoptedStyleSheets()->size(), 1u);

  host_a->remove();
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_EQ(sheet->adopted_tree_scopes_.size(), 1u);
  EXPECT_EQ(shadow_b.AdoptedStyleSheets()->size(), 1u);
}

TEST_F(CSSStyleSheetTest, AdoptedStyleSheetMediaQueryEvalChange) {
  SetBodyInnerHTML("<div id=green></div><div id=blue></div>");

  Element* green = GetDocument().getElementById(AtomicString("green"));
  Element* blue = GetDocument().getElementById(AtomicString("blue"));

  CSSStyleSheetInit* init = CSSStyleSheetInit::Create();
  CSSStyleSheet* sheet =
      CSSStyleSheet::Create(GetDocument(), init, ASSERT_NO_EXCEPTION);
  sheet->replaceSync(
      "@media (max-width: 300px) {#green{color:green}} @media "
      "(prefers-reduced-motion: reduce) {#blue{color:blue}}",
      ASSERT_NO_EXCEPTION);

  HeapVector<Member<CSSStyleSheet>> adopted_sheets;
  adopted_sheets.push_back(sheet);

  GetDocument().SetAdoptedStyleSheetsForTesting(adopted_sheets);
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(sheet->Contents());
  ASSERT_TRUE(sheet->Contents()->HasRuleSet());
  RuleSet* rule_set = &sheet->Contents()->GetRuleSet();

  EXPECT_EQ(Color::kBlack, green->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  GetDocument().ClearAdoptedStyleSheets();
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(sheet->Contents()->HasRuleSet());
  EXPECT_EQ(rule_set, &sheet->Contents()->GetRuleSet());
  EXPECT_EQ(Color::kBlack, green->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  GetDocument().View()->SetLayoutSizeFixedToFrameSize(false);
  GetDocument().View()->SetLayoutSize(gfx::Size(200, 500));
  UpdateAllLifecyclePhasesForTest();

  GetDocument().SetAdoptedStyleSheetsForTesting(adopted_sheets);
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(sheet->Contents()->HasRuleSet());
  EXPECT_NE(rule_set, &sheet->Contents()->GetRuleSet());
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      green->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(Color::kBlack, blue->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  GetDocument().ClearAdoptedStyleSheets();
  GetDocument().GetSettings()->SetPrefersReducedMotion(true);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(Color::kBlack, green->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));
  EXPECT_EQ(Color::kBlack, blue->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  GetDocument().SetAdoptedStyleSheetsForTesting(adopted_sheets);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      green->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(0, 0, 255),
      blue->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

}  // namespace blink
