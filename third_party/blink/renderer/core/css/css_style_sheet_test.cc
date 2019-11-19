// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/media_list_or_string.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_sheet_init.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
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
  init->setMedia(MediaListOrString::FromString("screen, print"));
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
  EXPECT_EQ(sheet->media()->mediaText(), init->media().GetAsString());
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

}  // namespace blink
