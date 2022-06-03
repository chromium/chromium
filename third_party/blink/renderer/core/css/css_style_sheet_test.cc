// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/bindings/core/v8/v8_union_medialist_string.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CSSStyleSheetTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
  }

  class FunctionForTest : public ScriptFunction {
   public:
    static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                  ScriptValue* output) {
      FunctionForTest* self =
          MakeGarbageCollected<FunctionForTest>(script_state, output);
      return self->BindToV8Function();
    }

    FunctionForTest(ScriptState* script_state, ScriptValue* output)
        : ScriptFunction(script_state), output_(output) {}

   private:
    ScriptValue Call(ScriptValue value) override {
      DCHECK(!value.IsEmpty());
      *output_ = value;
      return value;
    }

    ScriptValue* output_;
  };
};

TEST_F(CSSStyleSheetTest,
       CSSStyleSheetConstructionWithNonEmptyCSSStyleSheetInit) {
  DummyExceptionStateForTesting exception_state;
  CSSStyleSheetInit* init = CSSStyleSheetInit::Create();
  init->setMedia(
      MakeGarbageCollected<V8UnionMediaListOrString>("screen, print"));
  init->setTitle("test");
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
  EXPECT_EQ(sheet->title(), init->title());
  EXPECT_TRUE(sheet->AlternateFromConstructor());
  EXPECT_TRUE(sheet->disabled());
  EXPECT_EQ(sheet->cssRules(exception_state)->length(), 0U);
  ASSERT_FALSE(exception_state.HadException());
}

TEST_F(CSSStyleSheetTest,
       GarbageCollectedShadowRootsRemovedFromAdoptedTreeScopes) {
  SetBodyInnerHTML("<div id='host_a'></div><div id='host_b'></div>");
  auto* host_a = GetElementById("host_a");
  auto& shadow_a = host_a->AttachShadowRootInternal(ShadowRootType::kOpen);
  auto* host_b = GetElementById("host_b");
  auto& shadow_b = host_b->AttachShadowRootInternal(ShadowRootType::kOpen);
  DummyExceptionStateForTesting exception_state;
  CSSStyleSheetInit* init = CSSStyleSheetInit::Create();
  CSSStyleSheet* sheet =
      CSSStyleSheet::Create(GetDocument(), init, exception_state);

  HeapVector<Member<CSSStyleSheet>> adopted_sheets;
  adopted_sheets.push_back(sheet);
  shadow_a.SetAdoptedStyleSheets(adopted_sheets);
  shadow_b.SetAdoptedStyleSheets(adopted_sheets);

  EXPECT_EQ(sheet->adopted_tree_scopes_.size(), 2u);
  EXPECT_EQ(shadow_a.AdoptedStyleSheets().size(), 1u);
  EXPECT_EQ(shadow_b.AdoptedStyleSheets().size(), 1u);

  host_a->remove();
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_EQ(sheet->adopted_tree_scopes_.size(), 1u);
  EXPECT_EQ(shadow_b.AdoptedStyleSheets().size(), 1u);
}

TEST_F(CSSStyleSheetTest, AdoptedStyleSheetMediaQueryEvalChange) {
  SetBodyInnerHTML("<div id=green></div><div id=blue></div>");

  Element* green = GetDocument().getElementById("green");
  Element* blue = GetDocument().getElementById("blue");

  CSSStyleSheetInit* init = CSSStyleSheetInit::Create();
  CSSStyleSheet* sheet =
      CSSStyleSheet::Create(GetDocument(), init, ASSERT_NO_EXCEPTION);
  sheet->replaceSync(
      "@media (max-width: 300px) {#green{color:green}} @media "
      "(prefers-reduced-motion: reduce) {#blue{color:blue}}",
      ASSERT_NO_EXCEPTION);

  HeapVector<Member<CSSStyleSheet>> empty_adopted_sheets;
  HeapVector<Member<CSSStyleSheet>> adopted_sheets;
  adopted_sheets.push_back(sheet);

  GetDocument().SetAdoptedStyleSheets(adopted_sheets);
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(sheet->Contents());
  ASSERT_TRUE(sheet->Contents()->HasRuleSet());
  RuleSet* rule_set = &sheet->Contents()->GetRuleSet();

  EXPECT_EQ(Color::kBlack, green->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  GetDocument().SetAdoptedStyleSheets(empty_adopted_sheets);
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(sheet->Contents()->HasRuleSet());
  EXPECT_EQ(rule_set, &sheet->Contents()->GetRuleSet());
  EXPECT_EQ(Color::kBlack, green->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  GetDocument().View()->SetLayoutSizeFixedToFrameSize(false);
  GetDocument().View()->SetLayoutSize(IntSize(200, 500));
  UpdateAllLifecyclePhasesForTest();

  GetDocument().SetAdoptedStyleSheets(adopted_sheets);
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(sheet->Contents()->HasRuleSet());
  EXPECT_NE(rule_set, &sheet->Contents()->GetRuleSet());
  EXPECT_EQ(
      MakeRGB(0, 128, 0),
      green->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(Color::kBlack, blue->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  GetDocument().SetAdoptedStyleSheets(empty_adopted_sheets);
  GetDocument().GetSettings()->SetPrefersReducedMotion(true);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(Color::kBlack, green->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));
  EXPECT_EQ(Color::kBlack, blue->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  GetDocument().SetAdoptedStyleSheets(adopted_sheets);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(
      MakeRGB(0, 128, 0),
      green->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 255), blue->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

}  // namespace blink
