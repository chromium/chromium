// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_engine.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/shadow_root_init.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class StyleEngineTest : public testing::Test {
 protected:
  void SetUp() override;

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }
  StyleEngine& GetStyleEngine() { return GetDocument().GetStyleEngine(); }

  bool IsDocumentStyleSheetCollectionClean() {
    return !GetStyleEngine().ShouldUpdateDocumentStyleSheetCollection();
  }

  enum RuleSetInvalidation {
    kRuleSetInvalidationsScheduled,
    kRuleSetInvalidationFullRecalc
  };
  RuleSetInvalidation ScheduleInvalidationsForRules(TreeScope&,
                                                    const String& css_text);

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void StyleEngineTest::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
}

StyleEngineTest::RuleSetInvalidation
StyleEngineTest::ScheduleInvalidationsForRules(TreeScope& tree_scope,
                                               const String& css_text) {
  StyleSheetContents* sheet =
      StyleSheetContents::Create(CSSParserContext::Create(
          kHTMLStandardMode, SecureContextMode::kInsecureContext));
  sheet->ParseString(css_text);
  HeapHashSet<Member<RuleSet>> rule_sets;
  RuleSet& rule_set = sheet->EnsureRuleSet(MediaQueryEvaluator(),
                                           kRuleHasDocumentSecurityOrigin);
  rule_set.CompactRulesIfNeeded();
  if (rule_set.NeedsFullRecalcForRuleSetInvalidation())
    return kRuleSetInvalidationFullRecalc;
  rule_sets.insert(&rule_set);
  GetStyleEngine().ScheduleInvalidationsForRuleSets(tree_scope, rule_sets);
  return kRuleSetInvalidationsScheduled;
}

TEST_F(StyleEngineTest, DocumentDirtyAfterInject) {
  StyleSheetContents* parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  parsed_sheet->ParseString("div {}");
  GetStyleEngine().InjectSheet("", parsed_sheet);
  EXPECT_FALSE(IsDocumentStyleSheetCollectionClean());
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsDocumentStyleSheetCollectionClean());
}

TEST_F(StyleEngineTest, AnalyzedInject) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
     @font-face {
      font-family: 'Cool Font';
      src: url(dummy);
      font-weight: bold;
     }
     :root {
      --stop-color: black !important;
      --go-color: white;
     }
     #t1 { color: red !important }
     #t2 { color: black }
     #t4 { font-family: 'Cool Font'; font-weight: bold; font-style: italic }
     #t5 { animation-name: dummy-animation }
     #t6 { color: var(--stop-color); }
     #t7 { color: var(--go-color); }
     .red { color: red; }
     #t11 { color: white; }
    </style>
    <div id='t1'>Green</div>
    <div id='t2'>White</div>
    <div id='t3' style='color: black !important'>White</div>
    <div id='t4'>I look cool.</div>
    <div id='t5'>I animate!</div>
    <div id='t6'>Stop!</div>
    <div id='t7'>Go</div>
    <div id='t8' style='color: white !important'>screen: Red; print: Black</div>
    <div id='t9' class='red'>Green</div>
    <div id='t10' style='color: black !important'>Black</div>
    <div id='t11'>White</div>
    <div></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  Element* t2 = GetDocument().getElementById("t2");
  Element* t3 = GetDocument().getElementById("t3");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t2);
  ASSERT_TRUE(t3);
  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t2->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t3->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));

  const unsigned initial_count = GetStyleEngine().StyleForElementCount();

  StyleSheetContents* green_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  green_parsed_sheet->ParseString(
      "#t1 { color: green !important }"
      "#t2 { color: white !important }"
      "#t3 { color: white }");
  StyleSheetKey green_key("green");
  GetStyleEngine().InjectSheet(green_key, green_parsed_sheet,
                               WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(3u, GetStyleEngine().StyleForElementCount() - initial_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());

  // Important user rules override both regular and important author rules.
  EXPECT_EQ(MakeRGB(0, 128, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t2->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t3->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));

  StyleSheetContents* blue_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  blue_parsed_sheet->ParseString(
      "#t1 { color: blue !important }"
      "#t2 { color: silver }"
      "#t3 { color: silver !important }");
  StyleSheetKey blue_key("blue");
  GetStyleEngine().InjectSheet(blue_key, blue_parsed_sheet,
                               WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(6u, GetStyleEngine().StyleForElementCount() - initial_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());

  // Only important user rules override previously set important user rules.
  EXPECT_EQ(MakeRGB(0, 0, 255), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t2->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  // Important user rules override inline author rules.
  EXPECT_EQ(
      MakeRGB(192, 192, 192),
      t3->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(green_key, WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(9u, GetStyleEngine().StyleForElementCount() - initial_count);
  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());

  // Regular user rules do not override author rules.
  EXPECT_EQ(MakeRGB(0, 0, 255), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t2->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));
  EXPECT_EQ(
      MakeRGB(192, 192, 192),
      t3->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(blue_key, WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(12u, GetStyleEngine().StyleForElementCount() - initial_count);
  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t2->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t3->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));

  // @font-face rules

  Element* t4 = GetDocument().getElementById("t4");
  ASSERT_TRUE(t4);
  ASSERT_TRUE(t4->GetComputedStyle());

  // There's only one font and it's bold and normal.
  EXPECT_EQ(1u, GetStyleEngine().GetFontSelector()->GetFontFaceCache()
                ->GetNumSegmentedFacesForTesting());
  CSSSegmentedFontFace* font_face =
      GetStyleEngine().GetFontSelector()->GetFontFaceCache()
      ->Get(t4->GetComputedStyle()->GetFontDescription(),
            AtomicString("Cool Font"));
  EXPECT_TRUE(font_face);
  FontSelectionCapabilities capabilities =
      font_face->GetFontSelectionCapabilities();
  ASSERT_EQ(capabilities.weight,
            FontSelectionRange({BoldWeightValue(), BoldWeightValue()}));
  ASSERT_EQ(capabilities.slope,
            FontSelectionRange({NormalSlopeValue(), NormalSlopeValue()}));

  StyleSheetContents* font_face_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  font_face_parsed_sheet->ParseString(
      "@font-face {"
      " font-family: 'Cool Font';"
      " src: url(dummy);"
      " font-weight: bold;"
      " font-style: italic;"
      "}");
  StyleSheetKey font_face_key("font_face");
  GetStyleEngine().InjectSheet(font_face_key, font_face_parsed_sheet,
                               WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // After injecting a more specific font, now there are two and the
  // bold-italic one is selected.
  EXPECT_EQ(2u, GetStyleEngine().GetFontSelector()->GetFontFaceCache()
                ->GetNumSegmentedFacesForTesting());
  font_face = GetStyleEngine().GetFontSelector()->GetFontFaceCache()
              ->Get(t4->GetComputedStyle()->GetFontDescription(),
                    AtomicString("Cool Font"));
  EXPECT_TRUE(font_face);
  capabilities = font_face->GetFontSelectionCapabilities();
  ASSERT_EQ(capabilities.weight,
            FontSelectionRange({BoldWeightValue(), BoldWeightValue()}));
  ASSERT_EQ(capabilities.slope,
            FontSelectionRange({ItalicSlopeValue(), ItalicSlopeValue()}));

  auto* style_element =
      HTMLStyleElement::Create(GetDocument(), CreateElementFlags());
  style_element->SetInnerHTMLFromString(
      "@font-face {"
      " font-family: 'Cool Font';"
      " src: url(dummy);"
      " font-weight: normal;"
      " font-style: italic;"
      "}");
  GetDocument().body()->AppendChild(style_element);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Now there are three fonts, but the newest one does not override the older,
  // better matching one.
  EXPECT_EQ(3u, GetStyleEngine().GetFontSelector()->GetFontFaceCache()
                ->GetNumSegmentedFacesForTesting());
  font_face = GetStyleEngine().GetFontSelector()->GetFontFaceCache()
              ->Get(t4->GetComputedStyle()->GetFontDescription(),
                    AtomicString("Cool Font"));
  EXPECT_TRUE(font_face);
  capabilities = font_face->GetFontSelectionCapabilities();
  ASSERT_EQ(capabilities.weight,
            FontSelectionRange({BoldWeightValue(), BoldWeightValue()}));
  ASSERT_EQ(capabilities.slope,
            FontSelectionRange({ItalicSlopeValue(), ItalicSlopeValue()}));

  GetStyleEngine().RemoveInjectedSheet(font_face_key, WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // After removing the injected style sheet we're left with a bold-normal and
  // a normal-italic font, and the latter is selected by the matching algorithm
  // as font-style trumps font-weight.
  EXPECT_EQ(2u, GetStyleEngine().GetFontSelector()->GetFontFaceCache()
                ->GetNumSegmentedFacesForTesting());
  font_face = GetStyleEngine().GetFontSelector()->GetFontFaceCache()
              ->Get(t4->GetComputedStyle()->GetFontDescription(),
                    AtomicString("Cool Font"));
  EXPECT_TRUE(font_face);
  capabilities = font_face->GetFontSelectionCapabilities();
  ASSERT_EQ(capabilities.weight,
            FontSelectionRange({NormalWeightValue(), NormalWeightValue()}));
  ASSERT_EQ(capabilities.slope,
            FontSelectionRange({ItalicSlopeValue(), ItalicSlopeValue()}));

  // @keyframes rules

  Element* t5 = GetDocument().getElementById("t5");
  ASSERT_TRUE(t5);

  // There's no @keyframes rule named dummy-animation
  ASSERT_FALSE(GetStyleEngine().Resolver()->FindKeyframesRule(
      t5, AtomicString("dummy-animation")));

  StyleSheetContents* keyframes_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  keyframes_parsed_sheet->ParseString("@keyframes dummy-animation { from {} }");
  StyleSheetKey keyframes_key("keyframes");
  GetStyleEngine().InjectSheet(keyframes_key, keyframes_parsed_sheet,
                               WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // After injecting the style sheet, a @keyframes rule named dummy-animation
  // is found with one keyframe.
  StyleRuleKeyframes* keyframes =
      GetStyleEngine().Resolver()->FindKeyframesRule(
          t5, AtomicString("dummy-animation"));
  ASSERT_TRUE(keyframes);
  EXPECT_EQ(1u, keyframes->Keyframes().size());

  style_element = HTMLStyleElement::Create(GetDocument(), CreateElementFlags());
  style_element->SetInnerHTMLFromString(
      "@keyframes dummy-animation { from {} to {} }");
  GetDocument().body()->AppendChild(style_element);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Author @keyframes rules take precedence; now there are two keyframes (from
  // and to).
  keyframes = GetStyleEngine().Resolver()->FindKeyframesRule(
      t5, AtomicString("dummy-animation"));
  ASSERT_TRUE(keyframes);
  EXPECT_EQ(2u, keyframes->Keyframes().size());

  GetDocument().body()->RemoveChild(style_element);
  GetDocument().View()->UpdateAllLifecyclePhases();

  keyframes = GetStyleEngine().Resolver()->FindKeyframesRule(
      t5, AtomicString("dummy-animation"));
  ASSERT_TRUE(keyframes);
  EXPECT_EQ(1u, keyframes->Keyframes().size());

  GetStyleEngine().RemoveInjectedSheet(keyframes_key, WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Injected @keyframes rules are no longer available once removed.
  ASSERT_FALSE(GetStyleEngine().Resolver()->FindKeyframesRule(
      t5, AtomicString("dummy-animation")));

  // Custom properties

  Element* t6 = GetDocument().getElementById("t6");
  Element* t7 = GetDocument().getElementById("t7");
  ASSERT_TRUE(t6);
  ASSERT_TRUE(t7);
  ASSERT_TRUE(t6->GetComputedStyle());
  ASSERT_TRUE(t7->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 0), t6->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t7->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  StyleSheetContents* custom_properties_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  custom_properties_parsed_sheet->ParseString(
      ":root {"
      " --stop-color: red !important;"
      " --go-color: green;"
      "}");
  StyleSheetKey custom_properties_key("custom_properties");
  GetStyleEngine().InjectSheet(custom_properties_key,
                               custom_properties_parsed_sheet,
                               WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t6->GetComputedStyle());
  ASSERT_TRUE(t7->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t6->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t7->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(custom_properties_key,
                                       WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t6->GetComputedStyle());
  ASSERT_TRUE(t7->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 0), t6->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t7->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  // Media queries

  Element* t8 = GetDocument().getElementById("t8");
  ASSERT_TRUE(t8);
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t8->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  StyleSheetContents* media_queries_parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  media_queries_parsed_sheet->ParseString(
      "@media screen {"
      " #t8 {"
      "  color: red !important;"
      " }"
      "}"
      "@media print {"
      " #t8 {"
      "  color: black !important;"
      " }"
      "}");
  StyleSheetKey media_queries_sheet_key("media_queries_sheet");
  GetStyleEngine().InjectSheet(media_queries_sheet_key,
                               media_queries_parsed_sheet,
                               WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t8->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  FloatSize page_size(400, 400);
  GetDocument().GetFrame()->StartPrinting(page_size, page_size, 1);
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 0), t8->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));

  GetDocument().GetFrame()->EndPrinting();
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t8->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(media_queries_sheet_key,
                                       WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t8->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  // Author style sheets

  Element* t9 = GetDocument().getElementById("t9");
  Element* t10 = GetDocument().getElementById("t10");
  ASSERT_TRUE(t9);
  ASSERT_TRUE(t10);
  ASSERT_TRUE(t9->GetComputedStyle());
  ASSERT_TRUE(t10->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t9->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t10->GetComputedStyle()->VisitedDependentColor(
                                   GetCSSPropertyColor()));

  StyleSheetContents* parsed_author_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  parsed_author_sheet->ParseString(
      "#t9 {"
      " color: green;"
      "}"
      "#t10 {"
      " color: white !important;"
      "}");
  StyleSheetKey author_sheet_key("author_sheet");
  GetStyleEngine().InjectSheet(author_sheet_key, parsed_author_sheet,
                               WebDocument::kAuthorOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t9->GetComputedStyle());
  ASSERT_TRUE(t10->GetComputedStyle());

  // Specificity works within author origin.
  EXPECT_EQ(MakeRGB(0, 128, 0), t9->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  // Important author rules do not override important inline author rules.
  EXPECT_EQ(MakeRGB(0, 0, 0), t10->GetComputedStyle()->VisitedDependentColor(
                                   GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(author_sheet_key,
                                       WebDocument::kAuthorOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t9->GetComputedStyle());
  ASSERT_TRUE(t10->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t9->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(MakeRGB(0, 0, 0), t10->GetComputedStyle()->VisitedDependentColor(
                                   GetCSSPropertyColor()));

  // Style sheet removal

  Element* t11 = GetDocument().getElementById("t11");
  ASSERT_TRUE(t11);
  ASSERT_TRUE(t11->GetComputedStyle());
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t11->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  StyleSheetContents* parsed_removable_red_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  parsed_removable_red_sheet->ParseString("#t11 { color: red !important; }");
  StyleSheetKey removable_red_sheet_key("removable_red_sheet");
  GetStyleEngine().InjectSheet(removable_red_sheet_key,
                               parsed_removable_red_sheet,
                               WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  EXPECT_EQ(MakeRGB(255, 0, 0), t11->GetComputedStyle()->VisitedDependentColor(
                                     GetCSSPropertyColor()));

  StyleSheetContents* parsed_removable_green_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  parsed_removable_green_sheet->ParseString(
      "#t11 { color: green !important; }");
  StyleSheetKey removable_green_sheet_key("removable_green_sheet");
  GetStyleEngine().InjectSheet(removable_green_sheet_key,
                               parsed_removable_green_sheet,
                               WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  EXPECT_EQ(MakeRGB(0, 128, 0), t11->GetComputedStyle()->VisitedDependentColor(
                                     GetCSSPropertyColor()));

  StyleSheetContents* parsed_removable_red_sheet2 =
      StyleSheetContents::Create(CSSParserContext::Create(GetDocument()));
  parsed_removable_red_sheet2->ParseString("#t11 { color: red !important; }");
  GetStyleEngine().InjectSheet(removable_red_sheet_key,
                               parsed_removable_red_sheet2,
                               WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  EXPECT_EQ(MakeRGB(255, 0, 0), t11->GetComputedStyle()->VisitedDependentColor(
                                     GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(removable_red_sheet_key,
                                       WebDocument::kAuthorOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  // Removal works only within the same origin.
  EXPECT_EQ(MakeRGB(255, 0, 0), t11->GetComputedStyle()->VisitedDependentColor(
                                     GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(removable_red_sheet_key,
                                       WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  // The last sheet with the given key is removed.
  EXPECT_EQ(MakeRGB(0, 128, 0), t11->GetComputedStyle()->VisitedDependentColor(
                                     GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(removable_green_sheet_key,
                                       WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  // Only the last sheet with the given key is removed.
  EXPECT_EQ(MakeRGB(255, 0, 0), t11->GetComputedStyle()->VisitedDependentColor(
                                     GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(removable_red_sheet_key,
                                       WebDocument::kUserOrigin);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t11->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, IgnoreInvalidPropertyValue) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<section><div id='t1'>Red</div></section>"
      "<style id='s1'>div { color: red; } section div#t1 { color:rgb(0");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, TextToSheetCache) {
  auto* element = HTMLStyleElement::Create(GetDocument(), CreateElementFlags());

  String sheet_text("div {}");
  TextPosition min_pos = TextPosition::MinimumPosition();
  StyleEngineContext context;

  CSSStyleSheet* sheet1 =
      GetStyleEngine().CreateSheet(*element, sheet_text, min_pos, context);

  // Check that the first sheet is not using a cached StyleSheetContents.
  EXPECT_FALSE(sheet1->Contents()->IsUsedFromTextCache());

  CSSStyleSheet* sheet2 =
      GetStyleEngine().CreateSheet(*element, sheet_text, min_pos, context);

  // Check that the second sheet uses the cached StyleSheetContents for the
  // first.
  EXPECT_EQ(sheet1->Contents(), sheet2->Contents());
  EXPECT_TRUE(sheet2->Contents()->IsUsedFromTextCache());

  sheet1 = nullptr;
  sheet2 = nullptr;
  element = nullptr;

  // Garbage collection should clear the weak reference in the
  // StyleSheetContents cache.
  ThreadState::Current()->CollectAllGarbage();

  element = HTMLStyleElement::Create(GetDocument(), CreateElementFlags());
  sheet1 = GetStyleEngine().CreateSheet(*element, sheet_text, min_pos, context);

  // Check that we did not use a cached StyleSheetContents after the garbage
  // collection.
  EXPECT_FALSE(sheet1->Contents()->IsUsedFromTextCache());
}

TEST_F(StyleEngineTest, RuleSetInvalidationTypeSelectors) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <div>
      <span></span>
      <div></div>
    </div>
    <b></b><b></b><b></b><b></b>
    <i id=i>
      <i>
        <b></b>
      </i>
    </i>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(kRuleSetInvalidationsScheduled,
            ScheduleInvalidationsForRules(GetDocument(),
                                          "span { background: green}"));
  GetDocument().View()->UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  before_count = after_count;
  EXPECT_EQ(kRuleSetInvalidationsScheduled,
            ScheduleInvalidationsForRules(GetDocument(),
                                          "body div { background: green}"));
  GetDocument().View()->UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(2u, after_count - before_count);

  EXPECT_EQ(kRuleSetInvalidationFullRecalc,
            ScheduleInvalidationsForRules(GetDocument(),
                                          "div * { background: green}"));
  GetDocument().View()->UpdateAllLifecyclePhases();

  before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(kRuleSetInvalidationsScheduled,
            ScheduleInvalidationsForRules(GetDocument(),
                                          "#i b { background: green}"));
  GetDocument().View()->UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);
}

TEST_F(StyleEngineTest, RuleSetInvalidationCustomPseudo) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>progress { -webkit-appearance:none }</style>
    <progress></progress>
    <div></div><div></div><div></div><div></div><div></div><div></div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(ScheduleInvalidationsForRules(
                GetDocument(), "::-webkit-progress-bar { background: green }"),
            kRuleSetInvalidationsScheduled);
  GetDocument().View()->UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(3u, after_count - before_count);
}

TEST_F(StyleEngineTest, RuleSetInvalidationHost) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<div id=nohost></div><div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);

  shadow_root.SetInnerHTMLFromString("<div></div><div></div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(ScheduleInvalidationsForRules(
                shadow_root, ":host(#nohost), #nohost { background: green}"),
            kRuleSetInvalidationsScheduled);
  GetDocument().View()->UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(0u, after_count - before_count);

  before_count = after_count;
  EXPECT_EQ(ScheduleInvalidationsForRules(shadow_root,
                                          ":host(#host) { background: green}"),
            kRuleSetInvalidationsScheduled);
  GetDocument().View()->UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);
  EXPECT_EQ(ScheduleInvalidationsForRules(shadow_root,
                                          ":host(div) { background: green}"),
            kRuleSetInvalidationsScheduled);

  EXPECT_EQ(ScheduleInvalidationsForRules(shadow_root,
                                          ":host(*) { background: green}"),
            kRuleSetInvalidationFullRecalc);
  EXPECT_EQ(ScheduleInvalidationsForRules(
                shadow_root, ":host(*) :hover { background: green}"),
            kRuleSetInvalidationFullRecalc);
}

TEST_F(StyleEngineTest, RuleSetInvalidationSlotted) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <div id=host>
      <span slot=other class=s1></span>
      <span class=s2></span>
      <span class=s1></span>
      <span></span>
    </div>
  )HTML");

  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);

  shadow_root.SetInnerHTMLFromString("<slot name=other></slot><slot></slot>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(ScheduleInvalidationsForRules(
                shadow_root, "::slotted(.s1) { background: green}"),
            kRuleSetInvalidationsScheduled);
  GetDocument().View()->UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(4u, after_count - before_count);

  before_count = after_count;
  EXPECT_EQ(ScheduleInvalidationsForRules(shadow_root,
                                          "::slotted(*) { background: green}"),
            kRuleSetInvalidationsScheduled);
  GetDocument().View()->UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(4u, after_count - before_count);
}

TEST_F(StyleEngineTest, RuleSetInvalidationHostContext) {
  GetDocument().body()->SetInnerHTMLFromString("<div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);

  shadow_root.SetInnerHTMLFromString(
      "<div></div><div class=a></div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(ScheduleInvalidationsForRules(
                shadow_root, ":host-context(.nomatch) .a { background: green}"),
            kRuleSetInvalidationsScheduled);
  GetDocument().View()->UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  EXPECT_EQ(ScheduleInvalidationsForRules(
                shadow_root, ":host-context(:hover) { background: green}"),
            kRuleSetInvalidationFullRecalc);
  EXPECT_EQ(ScheduleInvalidationsForRules(
                shadow_root, ":host-context(#host) { background: green}"),
            kRuleSetInvalidationFullRecalc);
}

TEST_F(StyleEngineTest, RuleSetInvalidationV0BoundaryCrossing) {
  GetDocument().body()->SetInnerHTMLFromString("<div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);

  shadow_root.SetInnerHTMLFromString(
      "<div></div><div class=a></div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(ScheduleInvalidationsForRules(
                shadow_root, ".a ::content span { background: green}"),
            kRuleSetInvalidationFullRecalc);
}

TEST_F(StyleEngineTest, HasViewportDependentMediaQueries) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>div {}</style>
    <style id='sheet' media='(min-width: 200px)'>
      div {}
    </style>
  )HTML");

  Element* style_element = GetDocument().getElementById("sheet");

  for (unsigned i = 0; i < 10; i++) {
    GetDocument().body()->RemoveChild(style_element);
    GetDocument().View()->UpdateAllLifecyclePhases();
    GetDocument().body()->AppendChild(style_element);
    GetDocument().View()->UpdateAllLifecyclePhases();
  }

  EXPECT_TRUE(GetStyleEngine().HasViewportDependentMediaQueries());

  GetDocument().body()->RemoveChild(style_element);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FALSE(GetStyleEngine().HasViewportDependentMediaQueries());
}

TEST_F(StyleEngineTest, StyleMediaAttributeStyleChange) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style id='s1' media='(max-width: 1px)'>#t1 { color: green }</style>"
      "<div id='t1'>Green</div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));

  unsigned before_count = GetStyleEngine().StyleForElementCount();

  Element* s1 = GetDocument().getElementById("s1");
  s1->setAttribute(blink::HTMLNames::mediaAttr, "(max-width: 2000px)");
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, StyleMediaAttributeNoStyleChange) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style id='s1' media='(max-width: 1000px)'>#t1 { color: green }</style>"
      "<div id='t1'>Green</div><div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  unsigned before_count = GetStyleEngine().StyleForElementCount();

  Element* s1 = GetDocument().getElementById("s1");
  s1->setAttribute(blink::HTMLNames::mediaAttr, "(max-width: 2000px)");
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(0u, after_count - before_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, ModifyStyleRuleMatchedPropertiesCache) {
  // Test that the MatchedPropertiesCache is cleared when a StyleRule is
  // modified. The MatchedPropertiesCache caches results based on
  // CSSPropertyValueSet pointers. When a mutable CSSPropertyValueSet is
  // modified, the pointer doesn't change, yet the declarations do.

  GetDocument().body()->SetInnerHTMLFromString(
      "<style id='s1'>#t1 { color: blue }</style>"
      "<div id='t1'>Green</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 255), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  CSSStyleSheet* sheet = ToCSSStyleSheet(GetDocument().StyleSheets().item(0));
  ASSERT_TRUE(sheet);
  DummyExceptionStateForTesting exception_state;
  ASSERT_TRUE(sheet->cssRules(exception_state));
  CSSStyleRule* style_rule =
      ToCSSStyleRule(sheet->cssRules(exception_state)->item(0));
  ASSERT_FALSE(exception_state.HadException());
  ASSERT_TRUE(style_rule);
  ASSERT_TRUE(style_rule->style());

  // Modify the CSSPropertyValueSet once to make it a mutable set. Subsequent
  // modifications will not change the CSSPropertyValueSet pointer and cache
  // hash value will be the same.
  style_rule->style()->setProperty(&GetDocument(), "color", "red", "",
                                   ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  style_rule->style()->setProperty(&GetDocument(), "color", "green", "",
                                   ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, VisitedExplicitInheritanceMatchedPropertiesCache) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
      :visited { overflow: inherit }
    </style>
    <span id="span"><a href></a></span>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* span = GetDocument().getElementById("span");
  const ComputedStyle* style = span->GetComputedStyle();
  EXPECT_FALSE(style->HasExplicitlyInheritedProperties());

  style = span->firstChild()->GetComputedStyle();
  EXPECT_TRUE(MatchedPropertiesCache::IsStyleCacheable(*style));

  span->SetInlineStyleProperty(CSSPropertyColor, "blue");

  // Should not DCHECK on applying overflow:inherit on cached matched properties
  GetDocument().View()->UpdateAllLifecyclePhases();
}

TEST_F(StyleEngineTest, ScheduleInvalidationAfterSubtreeRecalc) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style id='s1'>
      .t1 span { color: green }
      .t2 span { color: green }
    </style>
    <style id='s2'>div { background: lime }</style>
    <div id='t1'></div>
    <div id='t2'></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  Element* t2 = GetDocument().getElementById("t2");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t2);

  // Sanity test.
  t1->setAttribute(blink::HTMLNames::classAttr, "t1");
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_TRUE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_TRUE(t1->NeedsStyleInvalidation());

  GetDocument().View()->UpdateAllLifecyclePhases();

  // platformColorsChanged() triggers SubtreeStyleChange on document(). If that
  // for some reason should change, this test will start failing and the
  // SubtreeStyleChange must be set another way.
  // Calling setNeedsStyleRecalc() explicitly with an arbitrary reason instead
  // requires us to CORE_EXPORT the reason strings.
  GetStyleEngine().PlatformColorsChanged();

  // Check that no invalidations sets are scheduled when the document node is
  // already SubtreeStyleChange.
  t2->setAttribute(blink::HTMLNames::classAttr, "t2");
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());

  GetDocument().View()->UpdateAllLifecyclePhases();
  HTMLStyleElement* s2 = ToHTMLStyleElement(GetDocument().getElementById("s2"));
  ASSERT_TRUE(s2);
  s2->setDisabled(true);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_TRUE(GetDocument().NeedsStyleInvalidation());

  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().PlatformColorsChanged();
  s2->setDisabled(false);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());

  GetDocument().View()->UpdateAllLifecyclePhases();
  HTMLStyleElement* s1 = ToHTMLStyleElement(GetDocument().getElementById("s1"));
  ASSERT_TRUE(s1);
  s1->setDisabled(true);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_TRUE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_TRUE(t1->NeedsStyleInvalidation());
  EXPECT_TRUE(t2->NeedsStyleInvalidation());

  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().PlatformColorsChanged();
  s1->setDisabled(false);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_FALSE(t1->NeedsStyleInvalidation());
  EXPECT_FALSE(t2->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, NoScheduledRuleSetInvalidationsOnNewShadow) {
  GetDocument().body()->SetInnerHTMLFromString("<div id='host'></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  GetDocument().View()->UpdateAllLifecyclePhases();
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);

  shadow_root.SetInnerHTMLFromString(R"HTML(
    <style>
      span { color: green }
      t1 { color: green }
    </style>
    <div id='t1'></div>
    <span></span>
  )HTML");

  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyHttpEquivDefaultStyle) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style>div { color:pink }</style><div id=container></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());

  Element* container = GetDocument().getElementById("container");
  ASSERT_TRUE(container);
  container->SetInnerHTMLFromString(
      "<meta http-equiv='default-style' content=''>");
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());

  container->SetInnerHTMLFromString(
      "<meta http-equiv='default-style' content='preferred'>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, StyleSheetsForStyleSheetList_Document) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style>span { color: green }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(GetDocument());
  EXPECT_EQ(1u, sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  GetDocument().body()->SetInnerHTMLFromString(
      "<style>span { color: green }</style><style>div { color: pink }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& second_sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(GetDocument());
  EXPECT_EQ(2u, second_sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  GetStyleEngine().MarkAllTreeScopesDirty();
  GetStyleEngine().StyleSheetsForStyleSheetList(GetDocument());
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, StyleSheetsForStyleSheetList_ShadowRoot) {
  GetDocument().body()->SetInnerHTMLFromString("<div id='host'></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  GetDocument().View()->UpdateAllLifecyclePhases();
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);

  shadow_root.SetInnerHTMLFromString("<style>span { color: green }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(shadow_root);
  EXPECT_EQ(1u, sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  shadow_root.SetInnerHTMLFromString(
      "<style>span { color: green }</style><style>div { color: pink }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& second_sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(shadow_root);
  EXPECT_EQ(2u, second_sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  GetStyleEngine().MarkAllTreeScopesDirty();
  GetStyleEngine().StyleSheetsForStyleSheetList(shadow_root);
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());
}

class StyleEngineClient : public frame_test_helpers::TestWebViewClient {
 public:
  StyleEngineClient() : device_scale_factor_(1.f) {}
  void ConvertWindowToViewport(WebFloatRect* rect) override {
    rect->x *= device_scale_factor_;
    rect->y *= device_scale_factor_;
    rect->width *= device_scale_factor_;
    rect->height *= device_scale_factor_;
  }
  void set_device_scale_factor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }

 private:
  float device_scale_factor_;
};

TEST_F(StyleEngineTest, ViewportDescriptionForZoomDSF) {
  StyleEngineClient client;
  client.set_device_scale_factor(1.f);

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.Initialize(nullptr, &client, nullptr, nullptr);
  web_view_impl->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(web_view_impl->GetPage()->MainFrame())->GetDocument();

  auto desc = document->GetViewportData().GetViewportDescription();
  float min_width = desc.min_width.GetFloatValue();
  float max_width = desc.max_width.GetFloatValue();
  float min_height = desc.min_height.GetFloatValue();
  float max_height = desc.max_height.GetFloatValue();

  const float device_scale = 3.5f;
  client.set_device_scale_factor(device_scale);
  web_view_impl->UpdateAllLifecyclePhases();

  desc = document->GetViewportData().GetViewportDescription();
  EXPECT_FLOAT_EQ(device_scale * min_width, desc.min_width.GetFloatValue());
  EXPECT_FLOAT_EQ(device_scale * max_width, desc.max_width.GetFloatValue());
  EXPECT_FLOAT_EQ(device_scale * min_height, desc.min_height.GetFloatValue());
  EXPECT_FLOAT_EQ(device_scale * max_height, desc.max_height.GetFloatValue());
}

TEST_F(StyleEngineTest, MediaQueryAffectingValueChanged_StyleElementNoMedia) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_StyleElementMediaNoValue) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style media>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_StyleElementMediaEmpty) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style media=''>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());
}

// TODO(futhark@chromium.org): The test cases below where all queries are either
// "all" or "not all", we could have detected those and not trigger an active
// stylesheet update for those cases.

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_StyleElementMediaNoValid) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style media=',,'>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, MediaQueryAffectingValueChanged_StyleElementMediaAll) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style media='all'>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_StyleElementMediaNotAll) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style media='not all'>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, MediaQueryAffectingValueChanged_StyleElementMediaType) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<style media='print'>div{color:pink}</style>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged();
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, EmptyPseudo_RemoveLast) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
      .empty:empty + span { color: purple }
    </style>
    <div id=t1 class=empty>Text</div>
    <span></span>
    <div id=t2 class=empty><span></span></div>
    <span></span>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  t1->firstChild()->remove();
  EXPECT_TRUE(t1->NeedsStyleInvalidation());

  Element* t2 = GetDocument().getElementById("t2");
  t2->firstChild()->remove();
  EXPECT_TRUE(t2->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyPseudo_RemoveNotLast) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
      .empty:empty + span { color: purple }
    </style>
    <div id=t1 class=empty>Text<span></span></div>
    <span></span>
    <div id=t2 class=empty><span></span><span></span></div>
    <span></span>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  t1->firstChild()->remove();
  EXPECT_FALSE(t1->NeedsStyleInvalidation());

  Element* t2 = GetDocument().getElementById("t2");
  t2->firstChild()->remove();
  EXPECT_FALSE(t2->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyPseudo_InsertFirst) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
      .empty:empty + span { color: purple }
    </style>
    <div id=t1 class=empty></div>
    <span></span>
    <div id=t2 class=empty></div>
    <span></span>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  t1->appendChild(Text::Create(GetDocument(), "Text"));
  EXPECT_TRUE(t1->NeedsStyleInvalidation());

  Element* t2 = GetDocument().getElementById("t2");
  t2->appendChild(HTMLSpanElement::Create(GetDocument()));
  EXPECT_TRUE(t2->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyPseudo_InsertNotFirst) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
      .empty:empty + span { color: purple }
    </style>
    <div id=t1 class=empty>Text</div>
    <span></span>
    <div id=t2 class=empty><span></span></div>
    <span></span>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  t1->appendChild(Text::Create(GetDocument(), "Text"));
  EXPECT_FALSE(t1->NeedsStyleInvalidation());

  Element* t2 = GetDocument().getElementById("t2");
  t2->appendChild(HTMLSpanElement::Create(GetDocument()));
  EXPECT_FALSE(t2->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyPseudo_ModifyTextData_SingleNode) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
      .empty:empty + span { color: purple }
    </style>
    <div id=t1 class=empty>Text</div>
    <span></span>
    <div id=t2 class=empty></div>
    <span></span>
    <div id=t3 class=empty>Text</div>
    <span></span>
  )HTML");

  Element* t1 = GetDocument().getElementById("t1");
  Element* t2 = GetDocument().getElementById("t2");
  Element* t3 = GetDocument().getElementById("t3");

  t2->appendChild(Text::Create(GetDocument(), ""));

  GetDocument().View()->UpdateAllLifecyclePhases();

  ToText(t1->firstChild())->setData("");
  EXPECT_TRUE(t1->NeedsStyleInvalidation());

  ToText(t2->firstChild())->setData("Text");
  EXPECT_TRUE(t2->NeedsStyleInvalidation());

  // This is not optimal. We do not detect that we change text to/from
  // non-empty string.
  ToText(t3->firstChild())->setData("NewText");
  EXPECT_TRUE(t3->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyPseudo_ModifyTextData_HasSiblings) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
      .empty:empty + span { color: purple }
    </style>
    <div id=t1 class=empty>Text<span></span></div>
    <span></span>
    <div id=t2 class=empty><span></span></div>
    <span></span>
    <div id=t3 class=empty>Text<span></span></div>
    <span></span>
  )HTML");

  Element* t1 = GetDocument().getElementById("t1");
  Element* t2 = GetDocument().getElementById("t2");
  Element* t3 = GetDocument().getElementById("t3");

  t2->appendChild(Text::Create(GetDocument(), ""));

  GetDocument().View()->UpdateAllLifecyclePhases();

  ToText(t1->firstChild())->setData("");
  EXPECT_FALSE(t1->NeedsStyleInvalidation());

  ToText(t2->lastChild())->setData("Text");
  EXPECT_FALSE(t2->NeedsStyleInvalidation());

  ToText(t3->firstChild())->setData("NewText");
  EXPECT_FALSE(t3->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, MediaQueriesChangeDefaultFontSize) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
      body { color: red }
      @media (max-width: 40em) {
        body { color: green }
      }
    </style>
    <body></body>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetSettings()->SetDefaultFontSize(40);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, ShadowRootStyleRecalcCrash) {
  GetDocument().body()->SetInnerHTMLFromString("<div id=host></div>");
  HTMLElement* host = ToHTMLElement(GetDocument().getElementById("host"));
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);

  shadow_root.SetInnerHTMLFromString(R"HTML(
    <span id=span></span>
    <style>
      :nth-child(odd) { color: green }
    </style>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // This should not cause DCHECK errors on style recalc flags.
  shadow_root.getElementById("span")->remove();
  host->SetInlineStyleProperty(CSSPropertyDisplay, "inline");
  GetDocument().View()->UpdateAllLifecyclePhases();
}

TEST_F(StyleEngineTest, GetComputedStyleOutsideFlatTreeCrash) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
      body, div { display: contents }
      div::before { display: contents; content: "" }
    </style>
    <div id=inner></div>
  )HTML");

  GetDocument().documentElement()->CreateV0ShadowRootForTesting();
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetDocument().body()->EnsureComputedStyle();
  GetDocument().getElementById("inner")->SetInlineStyleProperty(
      CSSPropertyColor, "blue");
  GetDocument().View()->UpdateAllLifecyclePhases();
}

TEST_F(StyleEngineTest, RejectSelectorForPseudoElement) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>
      div::before { content: "" }
      .not-in-filter div::before { color: red }
    </style>
    <div class='not-in-filter'></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  StyleEngine& engine = GetStyleEngine();
  engine.SetStatsEnabled(true);

  StyleResolverStats* stats = engine.Stats();
  ASSERT_TRUE(stats);

  Element* div = GetDocument().QuerySelector("div");
  ASSERT_TRUE(div);
  div->SetInlineStyleProperty(CSSPropertyColor, "green");

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetStyleEngine().RecalcStyle(kNoChange);

  // Should fast reject ".not-in-filter div::before {}" for both the div and its
  // ::before pseudo element.
  EXPECT_EQ(2u, stats->rules_fast_rejected);
}

TEST_F(StyleEngineTest, MarkForWhitespaceReattachment) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <div id=d1><span></span></div>
    <div id=d2><span></span><span></span></div>
    <div id=d3><span></span><span></span></div>
  )HTML");

  Element* d1 = GetDocument().getElementById("d1");
  Element* d2 = GetDocument().getElementById("d2");
  Element* d3 = GetDocument().getElementById("d3");

  GetDocument().View()->UpdateAllLifecyclePhases();

  d1->firstChild()->remove();
  EXPECT_TRUE(GetStyleEngine().NeedsWhitespaceReattachment(d1));
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().ChildNeedsReattachLayoutTree());

  GetStyleEngine().MarkForWhitespaceReattachment();
  EXPECT_FALSE(GetDocument().ChildNeedsReattachLayoutTree());

  GetDocument().View()->UpdateAllLifecyclePhases();

  d2->firstChild()->remove();
  d2->firstChild()->remove();
  EXPECT_TRUE(GetStyleEngine().NeedsWhitespaceReattachment(d2));
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().ChildNeedsReattachLayoutTree());

  GetStyleEngine().MarkForWhitespaceReattachment();
  EXPECT_FALSE(GetDocument().ChildNeedsReattachLayoutTree());

  GetDocument().View()->UpdateAllLifecyclePhases();

  d3->firstChild()->remove();
  EXPECT_TRUE(GetStyleEngine().NeedsWhitespaceReattachment(d3));
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().ChildNeedsReattachLayoutTree());

  GetStyleEngine().MarkForWhitespaceReattachment();
  EXPECT_TRUE(GetDocument().ChildNeedsReattachLayoutTree());
}

TEST_F(StyleEngineTest, FirstLetterRemoved) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style>.fl::first-letter { color: pink }</style>
    <div class=fl id=d1><div><span id=f1>A</span></div></div>
    <div class=fl id=d2><div><span id=f2>BB</span></div></div>
    <div class=fl id=d3><div><span id=f3>C<!---->C</span></div></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* d1 = GetDocument().getElementById("d1");
  Element* d2 = GetDocument().getElementById("d2");
  Element* d3 = GetDocument().getElementById("d3");

  FirstLetterPseudoElement* fl1 =
      ToFirstLetterPseudoElement(d1->GetPseudoElement(kPseudoIdFirstLetter));
  EXPECT_TRUE(fl1);

  GetDocument().getElementById("f1")->firstChild()->remove();

  EXPECT_FALSE(d1->firstChild()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(d1->firstChild()->ChildNeedsReattachLayoutTree());
  EXPECT_FALSE(d1->firstChild()->NeedsReattachLayoutTree());
  EXPECT_TRUE(d1->ChildNeedsStyleRecalc());
  EXPECT_TRUE(fl1->NeedsStyleRecalc());

  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(
      ToFirstLetterPseudoElement(d1->GetPseudoElement(kPseudoIdFirstLetter)));

  FirstLetterPseudoElement* fl2 =
      ToFirstLetterPseudoElement(d2->GetPseudoElement(kPseudoIdFirstLetter));
  EXPECT_TRUE(fl2);

  GetDocument().getElementById("f2")->firstChild()->remove();

  EXPECT_FALSE(d2->firstChild()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(d2->firstChild()->ChildNeedsReattachLayoutTree());
  EXPECT_FALSE(d2->firstChild()->NeedsReattachLayoutTree());
  EXPECT_TRUE(d2->ChildNeedsStyleRecalc());
  EXPECT_TRUE(fl2->NeedsStyleRecalc());

  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(
      ToFirstLetterPseudoElement(d2->GetPseudoElement(kPseudoIdFirstLetter)));

  FirstLetterPseudoElement* fl3 =
      ToFirstLetterPseudoElement(d3->GetPseudoElement(kPseudoIdFirstLetter));
  EXPECT_TRUE(fl3);

  Element* f3 = GetDocument().getElementById("f3");
  f3->firstChild()->remove();

  EXPECT_FALSE(d3->firstChild()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(d3->firstChild()->ChildNeedsReattachLayoutTree());
  EXPECT_FALSE(d3->firstChild()->NeedsReattachLayoutTree());
  EXPECT_TRUE(d3->ChildNeedsStyleRecalc());
  EXPECT_TRUE(fl3->NeedsStyleRecalc());

  GetDocument().View()->UpdateAllLifecyclePhases();
  fl3 = ToFirstLetterPseudoElement(d3->GetPseudoElement(kPseudoIdFirstLetter));
  EXPECT_TRUE(fl3);
  EXPECT_EQ(f3->lastChild()->GetLayoutObject(),
            fl3->RemainingTextLayoutObject());
}

}  // namespace blink
