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
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
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
  init->setBaseURL("https://example.com/custom/");
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
  EXPECT_EQ(sheet->BaseURL().GetString(), "https://example.com/custom/");
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

TEST_F(CSSStyleSheetTest, DetachCSSOMWrappersWithNullEntries) {
  auto* sheet = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet->replaceSync(".a { color: red; } .b { color: green; }",
                     ASSERT_NO_EXCEPTION);
  EXPECT_EQ(sheet->length(), 2u);

  sheet->item(0);
  // child_rule_cssom_wrappers_[0] is non-nullptr
  // child_rule_cssom_wrappers_[1] is nullptr

  // Call DetachCSSOMWrappers via replaceSync.
  sheet->replaceSync(".c { color: blue; }", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(sheet->length(), 1u);
}

TEST_F(CSSStyleSheetTest, ConstructableStyleSheetCacheSharing) {
  auto* sheet1 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet1->replaceSync("div { color: red; }", ASSERT_NO_EXCEPTION);

  auto* sheet2 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet2->replaceSync("div { color: red; }", ASSERT_NO_EXCEPTION);

  // Both sheets should share the exact same StyleSheetContents.
  EXPECT_EQ(sheet1->Contents(), sheet2->Contents());
  EXPECT_TRUE(sheet1->Contents()->IsUsedFromTextCache());
}

TEST_F(CSSStyleSheetTest, ConstructableStyleSheetCopyOnWriteMutation) {
  auto* sheet1 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet1->replaceSync("div { color: red; }", ASSERT_NO_EXCEPTION);

  auto* sheet2 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet2->replaceSync("div { color: red; }", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(sheet1->Contents(), sheet2->Contents());

  // Mutating sheet2 must trigger copy-on-write and isolate from sheet1.
  sheet2->insertRule("p { color: blue; }", 0, ASSERT_NO_EXCEPTION);
  EXPECT_NE(sheet1->Contents(), sheet2->Contents());
  EXPECT_EQ(sheet1->length(), 1u);
  EXPECT_EQ(sheet2->length(), 2u);
}

TEST_F(CSSStyleSheetTest,
       ConstructableStyleSheetAndStyleElementIsolateDueToParserContext) {
  SetBodyInnerHTML("<style id='style_tag'>div { color: red; }</style>");
  auto* style_tag = To<HTMLStyleElement>(GetElementById("style_tag"));
  ASSERT_TRUE(style_tag);
  ASSERT_TRUE(style_tag->sheet());

  auto* constructed_sheet = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  constructed_sheet->replaceSync("div { color: red; }", ASSERT_NO_EXCEPTION);

  // Constructable stylesheets and <style> elements have different
  // CSSParserContexts (e.g. Referrer configuration), so the unified text cache
  // ensures they do not mistakenly share contents.
  EXPECT_NE(constructed_sheet->Contents(), style_tag->sheet()->Contents());
}

TEST_F(CSSStyleSheetTest,
       StyleElementAndConstructableStyleSheetIsolateDueToParserContext) {
  auto* constructed_sheet = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  constructed_sheet->replaceSync("div { color: red; }", ASSERT_NO_EXCEPTION);

  SetBodyInnerHTML("<style id='style_tag'>div { color: red; }</style>");
  auto* style_tag = To<HTMLStyleElement>(GetElementById("style_tag"));
  ASSERT_TRUE(style_tag);
  ASSERT_TRUE(style_tag->sheet());

  // Reverse ordering: constructable sheet warms cache, subsequent <style>
  // element must not reuse contents with mismatched parser context.
  EXPECT_NE(constructed_sheet->Contents(), style_tag->sheet()->Contents());
}

TEST_F(CSSStyleSheetTest, ConstructableStyleSheetCacheRespectsCustomBaseURL) {
  String text = "div { background-image: url('image.png'); }";

  auto* sheet1 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet1->replaceSync(text, ASSERT_NO_EXCEPTION);

  auto* sheet2 =
      CSSStyleSheet::Create(GetDocument(), KURL("https://cdn.example/assets/"),
                            CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet2->replaceSync(text, ASSERT_NO_EXCEPTION);

  // Sheets with different base URLs must NOT share the cached contents.
  EXPECT_NE(sheet1->Contents(), sheet2->Contents());
}

TEST_F(CSSStyleSheetTest,
       ConstructableStyleSheetDistinctReplacementsDoNotCopyOldContents) {
  auto* sheet1 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet1->replaceSync("div { color: red; }", ASSERT_NO_EXCEPTION);

  auto* sheet2 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet2->replaceSync("div { color: red; }", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(sheet1->Contents(), sheet2->Contents());

  // Replacing sheet2 with distinct text allocates fresh contents without
  // mutating or corrupting sheet1's contents.
  sheet2->replaceSync("p { color: blue; }", ASSERT_NO_EXCEPTION);
  EXPECT_NE(sheet1->Contents(), sheet2->Contents());
  EXPECT_EQ(sheet1->length(), 1u);
  EXPECT_EQ(sheet2->length(), 1u);
}

TEST_F(CSSStyleSheetTest,
       ConstructableStyleSheetCachesAfterHistoricalMutation) {
  auto* sheet1 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet1->replaceSync("a { color: red; }", ASSERT_NO_EXCEPTION);
  sheet1->insertRule("b { color: blue; }", 0, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(sheet1->Contents()->IsMutable());

  // Replacing a historically mutated sheet must allocate fresh contents and be
  // eligible for caching.
  sheet1->replaceSync("c { color: green; }", ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(sheet1->Contents()->IsMutable());

  auto* sheet2 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet2->replaceSync("c { color: green; }", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(sheet1->Contents(), sheet2->Contents());
  EXPECT_TRUE(sheet2->Contents()->IsUsedFromTextCache());
}

TEST_F(CSSStyleSheetTest,
       ConstructableStyleSheetCacheHitMutatesRulesAndUpdatesActiveStyle) {
  SetBodyInnerHTML("<div id='target'>Hello</div>");
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);

  // Warm cache with red style.
  auto* sheet1 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet1->replaceSync("div { color: rgb(255, 0, 0); }", ASSERT_NO_EXCEPTION);

  // sheet2 starts with green style and is adopted.
  auto* sheet2 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet2->replaceSync("div { color: rgb(0, 255, 0); }", ASSERT_NO_EXCEPTION);

  HeapVector<Member<CSSStyleSheet>> adopted;
  adopted.push_back(sheet2);
  GetDocument().SetAdoptedStyleSheetsForTesting(adopted);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      Color::FromRGB(0, 255, 0),
      target->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  // Cache hit replacement to red style must schedule active style update and
  // invalidate matched properties cache.
  sheet2->replaceSync("div { color: rgb(255, 0, 0); }", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(sheet1->Contents(), sheet2->Contents());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      target->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(CSSStyleSheetTest,
       ConstructableStyleSheetDevToolsAllowImportsBypassesCache) {
  String text_with_import = "@import url('test.css'); div { color: red; }";

  auto* sheet1 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet1->replaceSync(text_with_import, ASSERT_NO_EXCEPTION);
  // replaceSync ignores @import rules.
  EXPECT_TRUE(sheet1->Contents()->ImportRules().empty());

  // DevTools SetText with kAllow must not reuse import-stripped contents.
  auto* sheet2 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet2->SetText(text_with_import, CSSImportRules::kAllow);
  EXPECT_NE(sheet1->Contents(), sheet2->Contents());
  EXPECT_EQ(sheet2->Contents()->ImportRules().size(), 1u);
}

TEST_F(CSSStyleSheetTest, NonConstructedSetTextWithImportParsesRules) {
  SetBodyInnerHTML("<style id='style_tag'>div { color: red; }</style>");
  auto* style_tag = To<HTMLStyleElement>(GetElementById("style_tag"));
  ASSERT_TRUE(style_tag);
  CSSStyleSheet* sheet = style_tag->sheet();
  ASSERT_TRUE(sheet);
  EXPECT_FALSE(sheet->IsConstructed());

  // DevTools edit with kAllow and an @import rule on a non-constructed sheet.
  sheet->SetText("@import url('test.css'); p { color: green; }",
                 CSSImportRules::kAllow);
  EXPECT_EQ(sheet->Contents()->ImportRules().size(), 1u);
}

TEST_F(CSSStyleSheetTest, ExternalStyleSheetPreservesHrefAfterSetText) {
  const KURL sheet_url("https://example.com/style.css");
  auto* context =
      MakeGarbageCollected<CSSParserContext>(GetDocument(), sheet_url);
  auto* contents =
      MakeGarbageCollected<StyleSheetContents>(context, sheet_url.GetString());
  auto* sheet = MakeGarbageCollected<CSSStyleSheet>(
      contents, GetDocument(), CSSStyleSheetInit::Create());
  EXPECT_EQ(sheet_url.GetString(), sheet->href());

  // Editing via SetText must preserve original_url_ and sheet->href().
  sheet->SetText("div { color: blue; }", CSSImportRules::kAllow);
  EXPECT_EQ(sheet_url.GetString(), sheet->href());
}

TEST_F(CSSStyleSheetTest,
       ConstructableStyleSheetRepeatedIgnoredImportEmitsConsoleWarning) {
  String text_with_import = "@import url('test.css'); div { color: red; }";

  auto* sheet1 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet1->replaceSync(text_with_import, ASSERT_NO_EXCEPTION);

  auto* sheet2 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet2->replaceSync(text_with_import, ASSERT_NO_EXCEPTION);

  // Sheets with disallowed @import rules must not be cached or shared.
  EXPECT_NE(sheet1->Contents(), sheet2->Contents());
  EXPECT_FALSE(sheet1->Contents()->IsUsedFromTextCache());
  EXPECT_FALSE(sheet2->Contents()->IsUsedFromTextCache());
}

TEST_F(CSSStyleSheetTest,
       ConstructableStyleSheetInitialAddDoesNotMarkReferencedFromCache) {
  String text = "div { color: purple; }";

  auto* sheet1 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet1->replaceSync(text, ASSERT_NO_EXCEPTION);

  // Initial insertion does not mark contents as referenced from text cache.
  EXPECT_FALSE(sheet1->Contents()->IsUsedFromTextCache());
  EXPECT_EQ(sheet1->Contents()->ClientSize(), 1u);

  // Second sheet reusing same text hits the cache and marks it.
  auto* sheet2 = CSSStyleSheet::Create(
      GetDocument(), CSSStyleSheetInit::Create(), ASSERT_NO_EXCEPTION);
  sheet2->replaceSync(text, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(sheet1->Contents(), sheet2->Contents());
  EXPECT_TRUE(sheet2->Contents()->IsUsedFromTextCache());
}

}  // namespace blink
