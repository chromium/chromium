// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_engine.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/public/common/css/navigation_controls.h"
#include "third_party/blink/public/common/css/preferred_color_scheme.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

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

  // A wrapper to add a reason for UpdateAllLifecyclePhases
  void UpdateAllLifecyclePhases() {
    GetDocument().View()->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  }

  Node* GetStyleRecalcRoot() {
    return GetStyleEngine().style_recalc_root_.GetRootNode();
  }

  const CSSValue* ComputedValue(Element* element, String property_name) {
    CSSPropertyRef ref(property_name, GetDocument());
    DCHECK(ref.IsValid());
    return ref.GetProperty().CSSValueFromComputedStyle(
        element->ComputedStyleRef(),
        /* layout_object */ nullptr,
        /* allow_visited_style */ false);
  }

  void InjectSheet(String key, WebDocument::CSSOrigin origin, String text) {
    auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
    sheet->ParseString(text);
    GetStyleEngine().InjectSheet(StyleSheetKey(key), sheet, origin);
  }

  bool IsUseCounted(mojom::WebFeature feature) {
    return GetDocument().IsUseCounted(feature);
  }

  void ClearUseCounter(mojom::WebFeature feature) {
    GetDocument().ClearUseCounterForTesting(feature);
    DCHECK(!IsUseCounted(feature));
  }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void StyleEngineTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(IntSize(800, 600));
}

StyleEngineTest::RuleSetInvalidation
StyleEngineTest::ScheduleInvalidationsForRules(TreeScope& tree_scope,
                                               const String& css_text) {
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(
          kHTMLStandardMode, SecureContextMode::kInsecureContext));
  sheet->ParseString(css_text);
  HeapHashSet<Member<RuleSet>> rule_sets;
  RuleSet& rule_set =
      sheet->EnsureRuleSet(MediaQueryEvaluator(GetDocument().GetFrame()),
                           kRuleHasDocumentSecurityOrigin);
  rule_set.CompactRulesIfNeeded();
  if (rule_set.NeedsFullRecalcForRuleSetInvalidation())
    return kRuleSetInvalidationFullRecalc;
  rule_sets.insert(&rule_set);
  GetStyleEngine().ScheduleInvalidationsForRuleSets(tree_scope, rule_sets);
  return kRuleSetInvalidationsScheduled;
}

TEST_F(StyleEngineTest, DocumentDirtyAfterInject) {
  auto* parsed_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  parsed_sheet->ParseString("div {}");
  GetStyleEngine().InjectSheet("", parsed_sheet);
  EXPECT_FALSE(IsDocumentStyleSheetCollectionClean());
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsDocumentStyleSheetCollectionClean());
}

TEST_F(StyleEngineTest, AnalyzedInject) {
  GetDocument().body()->setInnerHTML(R"HTML(
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
  UpdateAllLifecyclePhases();

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

  auto* green_parsed_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  green_parsed_sheet->ParseString(
      "#t1 { color: green !important }"
      "#t2 { color: white !important }"
      "#t3 { color: white }");
  StyleSheetKey green_key("green");
  GetStyleEngine().InjectSheet(green_key, green_parsed_sheet,
                               WebDocument::kUserOrigin);
  UpdateAllLifecyclePhases();

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

  auto* blue_parsed_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  blue_parsed_sheet->ParseString(
      "#t1 { color: blue !important }"
      "#t2 { color: silver }"
      "#t3 { color: silver !important }");
  StyleSheetKey blue_key("blue");
  GetStyleEngine().InjectSheet(blue_key, blue_parsed_sheet,
                               WebDocument::kUserOrigin);
  UpdateAllLifecyclePhases();

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
  UpdateAllLifecyclePhases();
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
  UpdateAllLifecyclePhases();
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

  auto* font_face_parsed_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
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
  UpdateAllLifecyclePhases();

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

  auto* style_element = MakeGarbageCollected<HTMLStyleElement>(
      GetDocument(), CreateElementFlags());
  style_element->setInnerHTML(
      "@font-face {"
      " font-family: 'Cool Font';"
      " src: url(dummy);"
      " font-weight: normal;"
      " font-style: italic;"
      "}");
  GetDocument().body()->AppendChild(style_element);
  UpdateAllLifecyclePhases();

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
  UpdateAllLifecyclePhases();

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
  ASSERT_FALSE(GetStyleEngine().GetStyleResolver().FindKeyframesRule(
      t5, AtomicString("dummy-animation")));

  auto* keyframes_parsed_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  keyframes_parsed_sheet->ParseString("@keyframes dummy-animation { from {} }");
  StyleSheetKey keyframes_key("keyframes");
  GetStyleEngine().InjectSheet(keyframes_key, keyframes_parsed_sheet,
                               WebDocument::kUserOrigin);
  UpdateAllLifecyclePhases();

  // After injecting the style sheet, a @keyframes rule named dummy-animation
  // is found with one keyframe.
  StyleRuleKeyframes* keyframes =
      GetStyleEngine().GetStyleResolver().FindKeyframesRule(
          t5, AtomicString("dummy-animation"));
  ASSERT_TRUE(keyframes);
  EXPECT_EQ(1u, keyframes->Keyframes().size());

  style_element = MakeGarbageCollected<HTMLStyleElement>(GetDocument(),
                                                         CreateElementFlags());
  style_element->setInnerHTML("@keyframes dummy-animation { from {} to {} }");
  GetDocument().body()->AppendChild(style_element);
  UpdateAllLifecyclePhases();

  // Author @keyframes rules take precedence; now there are two keyframes (from
  // and to).
  keyframes = GetStyleEngine().GetStyleResolver().FindKeyframesRule(
      t5, AtomicString("dummy-animation"));
  ASSERT_TRUE(keyframes);
  EXPECT_EQ(2u, keyframes->Keyframes().size());

  GetDocument().body()->RemoveChild(style_element);
  UpdateAllLifecyclePhases();

  keyframes = GetStyleEngine().GetStyleResolver().FindKeyframesRule(
      t5, AtomicString("dummy-animation"));
  ASSERT_TRUE(keyframes);
  EXPECT_EQ(1u, keyframes->Keyframes().size());

  GetStyleEngine().RemoveInjectedSheet(keyframes_key, WebDocument::kUserOrigin);
  UpdateAllLifecyclePhases();

  // Injected @keyframes rules are no longer available once removed.
  ASSERT_FALSE(GetStyleEngine().GetStyleResolver().FindKeyframesRule(
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

  auto* custom_properties_parsed_sheet =
      MakeGarbageCollected<StyleSheetContents>(
          MakeGarbageCollected<CSSParserContext>(GetDocument()));
  custom_properties_parsed_sheet->ParseString(
      ":root {"
      " --stop-color: red !important;"
      " --go-color: green;"
      "}");
  StyleSheetKey custom_properties_key("custom_properties");
  GetStyleEngine().InjectSheet(custom_properties_key,
                               custom_properties_parsed_sheet,
                               WebDocument::kUserOrigin);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t6->GetComputedStyle());
  ASSERT_TRUE(t7->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t6->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t7->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(custom_properties_key,
                                       WebDocument::kUserOrigin);
  UpdateAllLifecyclePhases();
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

  auto* media_queries_parsed_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
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
  UpdateAllLifecyclePhases();
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
  UpdateAllLifecyclePhases();
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

  auto* parsed_author_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
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
  UpdateAllLifecyclePhases();
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
  UpdateAllLifecyclePhases();
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

  auto* parsed_removable_red_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  parsed_removable_red_sheet->ParseString("#t11 { color: red !important; }");
  StyleSheetKey removable_red_sheet_key("removable_red_sheet");
  GetStyleEngine().InjectSheet(removable_red_sheet_key,
                               parsed_removable_red_sheet,
                               WebDocument::kUserOrigin);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  EXPECT_EQ(MakeRGB(255, 0, 0), t11->GetComputedStyle()->VisitedDependentColor(
                                     GetCSSPropertyColor()));

  auto* parsed_removable_green_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  parsed_removable_green_sheet->ParseString(
      "#t11 { color: green !important; }");
  StyleSheetKey removable_green_sheet_key("removable_green_sheet");
  GetStyleEngine().InjectSheet(removable_green_sheet_key,
                               parsed_removable_green_sheet,
                               WebDocument::kUserOrigin);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  EXPECT_EQ(MakeRGB(0, 128, 0), t11->GetComputedStyle()->VisitedDependentColor(
                                     GetCSSPropertyColor()));

  auto* parsed_removable_red_sheet2 = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  parsed_removable_red_sheet2->ParseString("#t11 { color: red !important; }");
  GetStyleEngine().InjectSheet(removable_red_sheet_key,
                               parsed_removable_red_sheet2,
                               WebDocument::kUserOrigin);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  EXPECT_EQ(MakeRGB(255, 0, 0), t11->GetComputedStyle()->VisitedDependentColor(
                                     GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(removable_red_sheet_key,
                                       WebDocument::kAuthorOrigin);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  // Removal works only within the same origin.
  EXPECT_EQ(MakeRGB(255, 0, 0), t11->GetComputedStyle()->VisitedDependentColor(
                                     GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(removable_red_sheet_key,
                                       WebDocument::kUserOrigin);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  // The last sheet with the given key is removed.
  EXPECT_EQ(MakeRGB(0, 128, 0), t11->GetComputedStyle()->VisitedDependentColor(
                                     GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(removable_green_sheet_key,
                                       WebDocument::kUserOrigin);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  // Only the last sheet with the given key is removed.
  EXPECT_EQ(MakeRGB(255, 0, 0), t11->GetComputedStyle()->VisitedDependentColor(
                                     GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(removable_red_sheet_key,
                                       WebDocument::kUserOrigin);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  EXPECT_EQ(
      MakeRGB(255, 255, 255),
      t11->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, InjectedFontFace) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
     @font-face {
      font-family: 'Author';
      src: url(user);
     }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();

  FontDescription font_description;
  FontFaceCache* cache = GetStyleEngine().GetFontSelector()->GetFontFaceCache();
  EXPECT_TRUE(cache->Get(font_description, "Author"));
  EXPECT_FALSE(cache->Get(font_description, "User"));

  auto* user_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  user_sheet->ParseString(
      "@font-face {"
      "  font-family: 'User';"
      "  src: url(author);"
      "}");

  StyleSheetKey user_key("user");
  GetStyleEngine().InjectSheet(user_key, user_sheet, WebDocument::kUserOrigin);

  UpdateAllLifecyclePhases();

  EXPECT_TRUE(cache->Get(font_description, "Author"));
  EXPECT_TRUE(cache->Get(font_description, "User"));
}

TEST_F(StyleEngineTest, IgnoreInvalidPropertyValue) {
  GetDocument().body()->setInnerHTML(
      "<section><div id='t1'>Red</div></section>"
      "<style id='s1'>div { color: red; } section div#t1 { color:rgb(0");
  UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, TextToSheetCache) {
  auto* element = MakeGarbageCollected<HTMLStyleElement>(GetDocument(),
                                                         CreateElementFlags());

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
  ThreadState::Current()->CollectAllGarbageForTesting();

  element = MakeGarbageCollected<HTMLStyleElement>(GetDocument(),
                                                   CreateElementFlags());
  sheet1 = GetStyleEngine().CreateSheet(*element, sheet_text, min_pos, context);

  // Check that we did not use a cached StyleSheetContents after the garbage
  // collection.
  EXPECT_FALSE(sheet1->Contents()->IsUsedFromTextCache());
}

TEST_F(StyleEngineTest, RuleSetInvalidationTypeSelectors) {
  GetDocument().body()->setInnerHTML(R"HTML(
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

  UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(kRuleSetInvalidationsScheduled,
            ScheduleInvalidationsForRules(GetDocument(),
                                          "span { background: green}"));
  UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  before_count = after_count;
  EXPECT_EQ(kRuleSetInvalidationsScheduled,
            ScheduleInvalidationsForRules(GetDocument(),
                                          "body div { background: green}"));
  UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(2u, after_count - before_count);

  EXPECT_EQ(kRuleSetInvalidationFullRecalc,
            ScheduleInvalidationsForRules(GetDocument(),
                                          "div * { background: green}"));
  UpdateAllLifecyclePhases();

  before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(kRuleSetInvalidationsScheduled,
            ScheduleInvalidationsForRules(GetDocument(),
                                          "#i b { background: green}"));
  UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);
}

TEST_F(StyleEngineTest, RuleSetInvalidationCustomPseudo) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>progress { -webkit-appearance:none }</style>
    <progress></progress>
    <div></div><div></div><div></div><div></div><div></div><div></div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(ScheduleInvalidationsForRules(
                GetDocument(), "::-webkit-progress-bar { background: green }"),
            kRuleSetInvalidationsScheduled);
  UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(3u, after_count - before_count);
}

TEST_F(StyleEngineTest, RuleSetInvalidationHost) {
  GetDocument().body()->setInnerHTML(
      "<div id=nohost></div><div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);

  shadow_root.setInnerHTML("<div></div><div></div><div></div>");
  UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(ScheduleInvalidationsForRules(
                shadow_root, ":host(#nohost), #nohost { background: green}"),
            kRuleSetInvalidationsScheduled);
  UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(0u, after_count - before_count);

  before_count = after_count;
  EXPECT_EQ(ScheduleInvalidationsForRules(shadow_root,
                                          ":host(#host) { background: green}"),
            kRuleSetInvalidationsScheduled);
  UpdateAllLifecyclePhases();
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
  GetDocument().body()->setInnerHTML(R"HTML(
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

  shadow_root.setInnerHTML("<slot name=other></slot><slot></slot>");
  UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(ScheduleInvalidationsForRules(
                shadow_root, "::slotted(.s1) { background: green}"),
            kRuleSetInvalidationsScheduled);
  UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(4u, after_count - before_count);

  before_count = after_count;
  EXPECT_EQ(ScheduleInvalidationsForRules(shadow_root,
                                          "::slotted(*) { background: green}"),
            kRuleSetInvalidationsScheduled);
  UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(4u, after_count - before_count);
}

TEST_F(StyleEngineTest, RuleSetInvalidationHostContext) {
  GetDocument().body()->setInnerHTML("<div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);

  shadow_root.setInnerHTML("<div></div><div class=a></div><div></div>");
  UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(ScheduleInvalidationsForRules(
                shadow_root, ":host-context(.nomatch) .a { background: green}"),
            kRuleSetInvalidationsScheduled);
  UpdateAllLifecyclePhases();
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
  GetDocument().body()->setInnerHTML("<div id=host></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);

  shadow_root.setInnerHTML("<div></div><div class=a></div><div></div>");
  UpdateAllLifecyclePhases();

  EXPECT_EQ(ScheduleInvalidationsForRules(
                shadow_root, ".a ::content span { background: green}"),
            kRuleSetInvalidationFullRecalc);
}

TEST_F(StyleEngineTest, HasViewportDependentMediaQueries) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>div {}</style>
    <style id='sheet' media='(min-width: 200px)'>
      div {}
    </style>
  )HTML");

  Element* style_element = GetDocument().getElementById("sheet");

  for (unsigned i = 0; i < 10; i++) {
    GetDocument().body()->RemoveChild(style_element);
    UpdateAllLifecyclePhases();
    GetDocument().body()->AppendChild(style_element);
    UpdateAllLifecyclePhases();
  }

  EXPECT_TRUE(GetStyleEngine().HasViewportDependentMediaQueries());

  GetDocument().body()->RemoveChild(style_element);
  UpdateAllLifecyclePhases();

  EXPECT_FALSE(GetStyleEngine().HasViewportDependentMediaQueries());
}

TEST_F(StyleEngineTest, StyleMediaAttributeStyleChange) {
  GetDocument().body()->setInnerHTML(
      "<style id='s1' media='(max-width: 1px)'>#t1 { color: green }</style>"
      "<div id='t1'>Green</div><div></div>");
  UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                  GetCSSPropertyColor()));

  unsigned before_count = GetStyleEngine().StyleForElementCount();

  Element* s1 = GetDocument().getElementById("s1");
  s1->setAttribute(blink::html_names::kMediaAttr, "(max-width: 2000px)");
  UpdateAllLifecyclePhases();

  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, StyleMediaAttributeNoStyleChange) {
  GetDocument().body()->setInnerHTML(
      "<style id='s1' media='(max-width: 1000px)'>#t1 { color: green }</style>"
      "<div id='t1'>Green</div><div></div>");
  UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  unsigned before_count = GetStyleEngine().StyleForElementCount();

  Element* s1 = GetDocument().getElementById("s1");
  s1->setAttribute(blink::html_names::kMediaAttr, "(max-width: 2000px)");
  UpdateAllLifecyclePhases();

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

  GetDocument().body()->setInnerHTML(
      "<style id='s1'>#t1 { color: blue }</style>"
      "<div id='t1'>Green</div>");
  UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 0, 255), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  auto* sheet = To<CSSStyleSheet>(GetDocument().StyleSheets().item(0));
  ASSERT_TRUE(sheet);
  DummyExceptionStateForTesting exception_state;
  ASSERT_TRUE(sheet->cssRules(exception_state));
  CSSStyleRule* style_rule =
      To<CSSStyleRule>(sheet->cssRules(exception_state)->item(0));
  ASSERT_FALSE(exception_state.HadException());
  ASSERT_TRUE(style_rule);
  ASSERT_TRUE(style_rule->style());

  // Modify the CSSPropertyValueSet once to make it a mutable set. Subsequent
  // modifications will not change the CSSPropertyValueSet pointer and cache
  // hash value will be the same.
  style_rule->style()->setProperty(GetDocument().GetExecutionContext(), "color",
                                   "red", "", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhases();

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(255, 0, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  style_rule->style()->setProperty(GetDocument().GetExecutionContext(), "color",
                                   "green", "", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhases();

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0), t1->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, VisitedExplicitInheritanceMatchedPropertiesCache) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      :visited { overflow: inherit }
    </style>
    <span id="span"><a href></a></span>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* span = GetDocument().getElementById("span");
  const ComputedStyle* style = span->GetComputedStyle();
  EXPECT_FALSE(style->ChildHasExplicitInheritance());

  style = span->firstChild()->GetComputedStyle();
  EXPECT_TRUE(MatchedPropertiesCache::IsStyleCacheable(*style));

  span->SetInlineStyleProperty(CSSPropertyID::kColor, "blue");

  // Should not DCHECK on applying overflow:inherit on cached matched properties
  UpdateAllLifecyclePhases();
}

TEST_F(StyleEngineTest, ScheduleInvalidationAfterSubtreeRecalc) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style id='s1'>
      .t1 span { color: green }
      .t2 span { color: green }
    </style>
    <style id='s2'>div { background: lime }</style>
    <div id='t1'></div>
    <div id='t2'></div>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  Element* t2 = GetDocument().getElementById("t2");
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t2);

  // Sanity test.
  t1->setAttribute(blink::html_names::kClassAttr, "t1");
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_TRUE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_TRUE(t1->NeedsStyleInvalidation());

  UpdateAllLifecyclePhases();

  // platformColorsChanged() triggers SubtreeStyleChange on document(). If that
  // for some reason should change, this test will start failing and the
  // SubtreeStyleChange must be set another way.
  // Calling setNeedsStyleRecalc() explicitly with an arbitrary reason instead
  // requires us to CORE_EXPORT the reason strings.
  GetStyleEngine().PlatformColorsChanged();

  // Check that no invalidations sets are scheduled when the document node is
  // already SubtreeStyleChange.
  t2->setAttribute(blink::html_names::kClassAttr, "t2");
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());

  UpdateAllLifecyclePhases();
  auto* s2 = To<HTMLStyleElement>(GetDocument().getElementById("s2"));
  ASSERT_TRUE(s2);
  s2->setDisabled(true);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_TRUE(GetDocument().NeedsStyleInvalidation());

  UpdateAllLifecyclePhases();
  GetStyleEngine().PlatformColorsChanged();
  s2->setDisabled(false);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());

  UpdateAllLifecyclePhases();
  auto* s1 = To<HTMLStyleElement>(GetDocument().getElementById("s1"));
  ASSERT_TRUE(s1);
  s1->setDisabled(true);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_TRUE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_TRUE(t1->NeedsStyleInvalidation());
  EXPECT_TRUE(t2->NeedsStyleInvalidation());

  UpdateAllLifecyclePhases();
  GetStyleEngine().PlatformColorsChanged();
  s1->setDisabled(false);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_FALSE(t1->NeedsStyleInvalidation());
  EXPECT_FALSE(t2->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, ScheduleRuleSetInvalidationsOnNewShadow) {
  GetDocument().body()->setInnerHTML("<div id='host'></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  UpdateAllLifecyclePhases();
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);

  shadow_root.setInnerHTML(R"HTML(
    <style>
      span { color: green }
      t1 { color: green }
    </style>
    <div id='t1'></div>
    <span></span>
  )HTML");

  GetStyleEngine().UpdateActiveStyle();
  EXPECT_TRUE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_TRUE(shadow_root.NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyHttpEquivDefaultStyle) {
  GetDocument().body()->setInnerHTML(
      "<style>div { color:pink }</style><div id=container></div>");
  UpdateAllLifecyclePhases();

  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());

  Element* container = GetDocument().getElementById("container");
  ASSERT_TRUE(container);
  container->setInnerHTML("<meta http-equiv='default-style' content=''>");
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());

  container->setInnerHTML(
      "<meta http-equiv='default-style' content='preferred'>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, StyleSheetsForStyleSheetList_Document) {
  GetDocument().body()->setInnerHTML("<style>span { color: green }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(GetDocument());
  EXPECT_EQ(1u, sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  GetDocument().body()->setInnerHTML(
      "<style>span { color: green }</style><style>div { color: pink }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& second_sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(GetDocument());
  EXPECT_EQ(2u, second_sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, StyleSheetsForStyleSheetList_ShadowRoot) {
  GetDocument().body()->setInnerHTML("<div id='host'></div>");
  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);

  UpdateAllLifecyclePhases();
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);

  shadow_root.setInnerHTML("<style>span { color: green }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(shadow_root);
  EXPECT_EQ(1u, sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  shadow_root.setInnerHTML(
      "<style>span { color: green }</style><style>div { color: pink }</style>");
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());

  const auto& second_sheet_list =
      GetStyleEngine().StyleSheetsForStyleSheetList(shadow_root);
  EXPECT_EQ(2u, second_sheet_list.size());
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

class StyleEngineClient : public frame_test_helpers::TestWebWidgetClient {
 public:
  // WebWidgetClient overrides.
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
  float device_scale_factor_ = 1.f;
};

TEST_F(StyleEngineTest, ViewportDescriptionForZoomDSF) {
  StyleEngineClient client;
  client.set_device_scale_factor(1.f);

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.Initialize(nullptr, nullptr, &client);
  web_view_impl->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();

  auto desc = document->GetViewportData().GetViewportDescription();
  float min_width = desc.min_width.GetFloatValue();
  float max_width = desc.max_width.GetFloatValue();
  float min_height = desc.min_height.GetFloatValue();
  float max_height = desc.max_height.GetFloatValue();

  const float device_scale = 3.5f;
  client.set_device_scale_factor(device_scale);
  web_view_impl->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  desc = document->GetViewportData().GetViewportDescription();
  EXPECT_FLOAT_EQ(device_scale * min_width, desc.min_width.GetFloatValue());
  EXPECT_FLOAT_EQ(device_scale * max_width, desc.max_width.GetFloatValue());
  EXPECT_FLOAT_EQ(device_scale * min_height, desc.min_height.GetFloatValue());
  EXPECT_FLOAT_EQ(device_scale * max_height, desc.max_height.GetFloatValue());
}

TEST_F(StyleEngineTest, MediaQueryAffectingValueChanged_StyleElementNoMedia) {
  GetDocument().body()->setInnerHTML("<style>div{color:pink}</style>");
  UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged(MediaValueChange::kOther);
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_StyleElementMediaNoValue) {
  GetDocument().body()->setInnerHTML("<style media>div{color:pink}</style>");
  UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged(MediaValueChange::kOther);
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_StyleElementMediaEmpty) {
  GetDocument().body()->setInnerHTML("<style media=''>div{color:pink}</style>");
  UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged(MediaValueChange::kOther);
  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());
}

// TODO(futhark@chromium.org): The test cases below where all queries are either
// "all" or "not all", we could have detected those and not trigger an active
// stylesheet update for those cases.

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_StyleElementMediaNoValid) {
  GetDocument().body()->setInnerHTML(
      "<style media=',,'>div{color:pink}</style>");
  UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged(MediaValueChange::kOther);
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, MediaQueryAffectingValueChanged_StyleElementMediaAll) {
  GetDocument().body()->setInnerHTML(
      "<style media='all'>div{color:pink}</style>");
  UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged(MediaValueChange::kOther);
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_StyleElementMediaNotAll) {
  GetDocument().body()->setInnerHTML(
      "<style media='not all'>div{color:pink}</style>");
  UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged(MediaValueChange::kOther);
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, MediaQueryAffectingValueChanged_StyleElementMediaType) {
  GetDocument().body()->setInnerHTML(
      "<style media='print'>div{color:pink}</style>");
  UpdateAllLifecyclePhases();
  GetStyleEngine().MediaQueryAffectingValueChanged(MediaValueChange::kOther);
  EXPECT_TRUE(GetStyleEngine().NeedsActiveStyleUpdate());
}

TEST_F(StyleEngineTest, EmptyPseudo_RemoveLast) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .empty:empty + span { color: purple }
    </style>
    <div id=t1 class=empty>Text</div>
    <span></span>
    <div id=t2 class=empty><span></span></div>
    <span></span>
  )HTML");

  UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  t1->firstChild()->remove();
  EXPECT_TRUE(t1->NeedsStyleInvalidation());

  Element* t2 = GetDocument().getElementById("t2");
  t2->firstChild()->remove();
  EXPECT_TRUE(t2->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyPseudo_RemoveNotLast) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .empty:empty + span { color: purple }
    </style>
    <div id=t1 class=empty>Text<span></span></div>
    <span></span>
    <div id=t2 class=empty><span></span><span></span></div>
    <span></span>
  )HTML");

  UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  t1->firstChild()->remove();
  EXPECT_FALSE(t1->NeedsStyleInvalidation());

  Element* t2 = GetDocument().getElementById("t2");
  t2->firstChild()->remove();
  EXPECT_FALSE(t2->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyPseudo_InsertFirst) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .empty:empty + span { color: purple }
    </style>
    <div id=t1 class=empty></div>
    <span></span>
    <div id=t2 class=empty></div>
    <span></span>
  )HTML");

  UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  t1->appendChild(Text::Create(GetDocument(), "Text"));
  EXPECT_TRUE(t1->NeedsStyleInvalidation());

  Element* t2 = GetDocument().getElementById("t2");
  t2->appendChild(MakeGarbageCollected<HTMLSpanElement>(GetDocument()));
  EXPECT_TRUE(t2->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyPseudo_InsertNotFirst) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .empty:empty + span { color: purple }
    </style>
    <div id=t1 class=empty>Text</div>
    <span></span>
    <div id=t2 class=empty><span></span></div>
    <span></span>
  )HTML");

  UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById("t1");
  t1->appendChild(Text::Create(GetDocument(), "Text"));
  EXPECT_FALSE(t1->NeedsStyleInvalidation());

  Element* t2 = GetDocument().getElementById("t2");
  t2->appendChild(MakeGarbageCollected<HTMLSpanElement>(GetDocument()));
  EXPECT_FALSE(t2->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyPseudo_ModifyTextData_SingleNode) {
  GetDocument().body()->setInnerHTML(R"HTML(
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

  UpdateAllLifecyclePhases();

  To<Text>(t1->firstChild())->setData("");
  EXPECT_TRUE(t1->NeedsStyleInvalidation());

  To<Text>(t2->firstChild())->setData("Text");
  EXPECT_TRUE(t2->NeedsStyleInvalidation());

  // This is not optimal. We do not detect that we change text to/from
  // non-empty string.
  To<Text>(t3->firstChild())->setData("NewText");
  EXPECT_TRUE(t3->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, EmptyPseudo_ModifyTextData_HasSiblings) {
  GetDocument().body()->setInnerHTML(R"HTML(
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

  UpdateAllLifecyclePhases();

  To<Text>(t1->firstChild())->setData("");
  EXPECT_FALSE(t1->NeedsStyleInvalidation());

  To<Text>(t2->lastChild())->setData("Text");
  EXPECT_FALSE(t2->NeedsStyleInvalidation());

  To<Text>(t3->firstChild())->setData("NewText");
  EXPECT_FALSE(t3->NeedsStyleInvalidation());
}

TEST_F(StyleEngineTest, MediaQueriesChangeDefaultFontSize) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { color: red }
      @media (max-width: 40em) {
        body { color: green }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetSettings()->SetDefaultFontSize(40);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangeColorScheme) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kLight);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { color: red }
      @media (prefers-color-scheme: dark) {
        body { color: green }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kDark);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangeColorSchemeForcedDarkMode) {
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kDark);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @media (prefers-color-scheme: light) {
        body { color: green }
      }
      @media (prefers-color-scheme: dark) {
        body { color: red }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangePrefersReducedMotion) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { color: red }
      @media (prefers-reduced-motion: reduce) {
        body { color: green }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetSettings()->SetPrefersReducedMotion(true);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangePrefersReducedDataOn) {
  GetNetworkStateNotifier().SetSaveDataEnabled(true);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { color: red }
      @media (prefers-reduced-data: reduce) {
        body { color: green }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();

  EXPECT_TRUE(GetNetworkStateNotifier().SaveDataEnabled());
  EXPECT_EQ(MakeRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangePrefersReducedDataOff) {
  GetNetworkStateNotifier().SetSaveDataEnabled(false);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { color: red }
      @media (prefers-reduced-data: reduce) {
        body { color: green }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();

  EXPECT_FALSE(GetNetworkStateNotifier().SaveDataEnabled());
  EXPECT_EQ(MakeRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangeForcedColors) {
  ScopedForcedColorsForTest scoped_feature(true);
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body {
        forced-color-adjust: none;
      }
      @media (forced-colors: none) {
        body { color: red }
      }
      @media (forced-colors: active) {
        body { color: green }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetForcedColors(GetDocument(), ForcedColors::kActive);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangeForcedColorsAndPreferredColorScheme) {
  ScopedForcedColorsForTest scoped_feature(true);
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body {
        forced-color-adjust: none;
      }
      @media (forced-colors: none) and (prefers-color-scheme: light) {
        body { color: red }
      }
      @media (forced-colors: none) and (prefers-color-scheme: dark) {
        body { color: green }
      }
      @media (forced-colors: active) and (prefers-color-scheme: dark) {
        body { color: orange }
      }
      @media (forced-colors: active) and (prefers-color-scheme: light) {
        body { color: blue }
      }
    </style>
    <body></body>
  )HTML");

  // ForcedColors = kNone, PreferredColorScheme = kLight
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetForcedColors(GetDocument(), ForcedColors::kNone);
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kLight);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  // ForcedColors = kNone, PreferredColorScheme = kDark
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kDark);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  // ForcedColors = kActive, PreferredColorScheme = kDark
  color_scheme_helper.SetForcedColors(GetDocument(), ForcedColors::kActive);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(255, 165, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  // ForcedColors = kActive, PreferredColorScheme = kLight
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kLight);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(0, 0, 255),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesColorSchemeOverride) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kLight);
  EXPECT_EQ(PreferredColorScheme::kLight,
            GetDocument().GetSettings()->GetPreferredColorScheme());

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { color: red }
      @media (prefers-color-scheme: dark) {
        body { color: green }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetMediaFeatureOverride("prefers-color-scheme",
                                                   "dark");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearMediaFeatureOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, PreferredColorSchemeMetric) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kLight);
  EXPECT_FALSE(IsUseCounted(WebFeature::kPreferredColorSchemeDark));
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kDark);
  EXPECT_TRUE(IsUseCounted(WebFeature::kPreferredColorSchemeDark));
}

// The preferred color scheme setting can differ from the preferred color
// scheme when forced dark mode is enabled. This is so that forced dark mode
// does not invert pages that support dark mode.
TEST_F(StyleEngineTest, PreferredColorSchemeSettingMetric) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kLight);
  GetDocument().GetSettings()->SetForceDarkModeEnabled(false);
  EXPECT_FALSE(IsUseCounted(WebFeature::kPreferredColorSchemeDark));
  EXPECT_FALSE(IsUseCounted(WebFeature::kPreferredColorSchemeDarkSetting));

  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kDark);
  // Clear the UseCounters before they are updated by the
  // |SetForceDarkModeEnabled| call, below.
  ClearUseCounter(WebFeature::kPreferredColorSchemeDark);
  ClearUseCounter(WebFeature::kPreferredColorSchemeDarkSetting);
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);

  EXPECT_FALSE(IsUseCounted(WebFeature::kPreferredColorSchemeDark));
  EXPECT_TRUE(IsUseCounted(WebFeature::kPreferredColorSchemeDarkSetting));
}

TEST_F(StyleEngineTest, ForcedDarkModeMetric) {
  GetDocument().GetSettings()->SetForceDarkModeEnabled(false);
  EXPECT_FALSE(IsUseCounted(WebFeature::kForcedDarkMode));
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
  EXPECT_TRUE(IsUseCounted(WebFeature::kForcedDarkMode));
}

TEST_F(StyleEngineTest, MediaQueriesReducedMotionOverride) {
  EXPECT_FALSE(GetDocument().GetSettings()->GetPrefersReducedMotion());

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { color: red }
      @media (prefers-reduced-motion: reduce) {
        body { color: green }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetMediaFeatureOverride("prefers-reduced-motion",
                                                   "reduce");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearMediaFeatureOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangeNavigationControls) {
  ScopedMediaQueryNavigationControlsForTest scoped_feature(true);
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @media (navigation-controls: none) {
        body { color: red }
      }
      @media (navigation-controls: back-button) {
        body { color: green }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetSettings()->SetNavigationControls(
      NavigationControls::kBackButton);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(MakeRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, ShadowRootStyleRecalcCrash) {
  GetDocument().body()->setInnerHTML("<div id=host></div>");
  auto* host = To<HTMLElement>(GetDocument().getElementById("host"));
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);

  shadow_root.setInnerHTML(R"HTML(
    <span id=span></span>
    <style>
      :nth-child(odd) { color: green }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();

  // This should not cause DCHECK errors on style recalc flags.
  shadow_root.getElementById("span")->remove();
  host->SetInlineStyleProperty(CSSPropertyID::kDisplay, "inline");
  UpdateAllLifecyclePhases();
}

TEST_F(StyleEngineTest, GetComputedStyleOutsideFlatTreeCrash) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body, div { display: contents }
      div::before { display: contents; content: "" }
    </style>
    <div id=inner></div>
  )HTML");

  GetDocument().documentElement()->CreateV0ShadowRootForTesting();
  UpdateAllLifecyclePhases();
  GetDocument().body()->EnsureComputedStyle();
  GetDocument().getElementById("inner")->SetInlineStyleProperty(
      CSSPropertyID::kColor, "blue");
  UpdateAllLifecyclePhases();
}

TEST_F(StyleEngineTest, RejectSelectorForPseudoElement) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      div::before { content: "" }
      .not-in-filter div::before { color: red }
    </style>
    <div class='not-in-filter'></div>
  )HTML");
  UpdateAllLifecyclePhases();

  StyleEngine& engine = GetStyleEngine();
  // If the Stats() were already enabled, we would not start with 0 counts.
  EXPECT_FALSE(engine.Stats());
  engine.SetStatsEnabled(true);

  StyleResolverStats* stats = engine.Stats();
  ASSERT_TRUE(stats);
  EXPECT_EQ(0u, stats->rules_fast_rejected);

  Element* div = GetDocument().QuerySelector("div");
  ASSERT_TRUE(div);
  div->SetInlineStyleProperty(CSSPropertyID::kColor, "green");

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetStyleEngine().RecalcStyle();

  // Should fast reject ".not-in-filter div::before {}" for both the div and its
  // ::before pseudo element.
  EXPECT_EQ(2u, stats->rules_fast_rejected);
}

TEST_F(StyleEngineTest, MarkForWhitespaceReattachment) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=d1><span></span></div>
    <div id=d2><span></span><span></span></div>
    <div id=d3><span></span><span></span></div>
  )HTML");

  Element* d1 = GetDocument().getElementById("d1");
  Element* d2 = GetDocument().getElementById("d2");
  Element* d3 = GetDocument().getElementById("d3");

  UpdateAllLifecyclePhases();

  d1->firstChild()->remove();
  EXPECT_TRUE(GetStyleEngine().NeedsWhitespaceReattachment(d1));
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetStyleEngine().NeedsLayoutTreeRebuild());

  GetStyleEngine().MarkForWhitespaceReattachment();
  EXPECT_FALSE(GetStyleEngine().NeedsLayoutTreeRebuild());

  UpdateAllLifecyclePhases();

  d2->firstChild()->remove();
  d2->firstChild()->remove();
  EXPECT_TRUE(GetStyleEngine().NeedsWhitespaceReattachment(d2));
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetStyleEngine().NeedsLayoutTreeRebuild());

  GetStyleEngine().MarkForWhitespaceReattachment();
  EXPECT_FALSE(GetStyleEngine().NeedsLayoutTreeRebuild());

  UpdateAllLifecyclePhases();

  d3->firstChild()->remove();
  EXPECT_TRUE(GetStyleEngine().NeedsWhitespaceReattachment(d3));
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetStyleEngine().NeedsLayoutTreeRebuild());

  GetStyleEngine().MarkForWhitespaceReattachment();
  EXPECT_TRUE(GetStyleEngine().NeedsLayoutTreeRebuild());
}

TEST_F(StyleEngineTest, FirstLetterRemoved) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>.fl::first-letter { color: pink }</style>
    <div class=fl id=d1><div><span id=f1>A</span></div></div>
    <div class=fl id=d2><div><span id=f2>BB</span></div></div>
    <div class=fl id=d3><div><span id=f3>C<!---->C</span></div></div>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* d1 = GetDocument().getElementById("d1");
  Element* d2 = GetDocument().getElementById("d2");
  Element* d3 = GetDocument().getElementById("d3");

  FirstLetterPseudoElement* fl1 =
      To<FirstLetterPseudoElement>(d1->GetPseudoElement(kPseudoIdFirstLetter));
  EXPECT_TRUE(fl1);

  GetDocument().getElementById("f1")->firstChild()->remove();

  EXPECT_FALSE(d1->firstChild()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(d1->firstChild()->ChildNeedsReattachLayoutTree());
  EXPECT_FALSE(d1->firstChild()->NeedsReattachLayoutTree());
  EXPECT_TRUE(d1->ChildNeedsStyleRecalc());
  EXPECT_TRUE(fl1->NeedsStyleRecalc());

  UpdateAllLifecyclePhases();
  EXPECT_FALSE(
      To<FirstLetterPseudoElement>(d1->GetPseudoElement(kPseudoIdFirstLetter)));

  FirstLetterPseudoElement* fl2 =
      To<FirstLetterPseudoElement>(d2->GetPseudoElement(kPseudoIdFirstLetter));
  EXPECT_TRUE(fl2);

  GetDocument().getElementById("f2")->firstChild()->remove();

  EXPECT_FALSE(d2->firstChild()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(d2->firstChild()->ChildNeedsReattachLayoutTree());
  EXPECT_FALSE(d2->firstChild()->NeedsReattachLayoutTree());
  EXPECT_TRUE(d2->ChildNeedsStyleRecalc());
  EXPECT_TRUE(fl2->NeedsStyleRecalc());

  UpdateAllLifecyclePhases();
  EXPECT_FALSE(
      To<FirstLetterPseudoElement>(d2->GetPseudoElement(kPseudoIdFirstLetter)));

  FirstLetterPseudoElement* fl3 =
      To<FirstLetterPseudoElement>(d3->GetPseudoElement(kPseudoIdFirstLetter));
  EXPECT_TRUE(fl3);

  Element* f3 = GetDocument().getElementById("f3");
  f3->firstChild()->remove();

  EXPECT_FALSE(d3->firstChild()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(d3->firstChild()->ChildNeedsReattachLayoutTree());
  EXPECT_FALSE(d3->firstChild()->NeedsReattachLayoutTree());
  EXPECT_TRUE(d3->ChildNeedsStyleRecalc());
  EXPECT_TRUE(fl3->NeedsStyleRecalc());

  UpdateAllLifecyclePhases();
  fl3 =
      To<FirstLetterPseudoElement>(d3->GetPseudoElement(kPseudoIdFirstLetter));
  EXPECT_TRUE(fl3);
  EXPECT_EQ(f3->lastChild()->GetLayoutObject(),
            fl3->RemainingTextLayoutObject());
}

TEST_F(StyleEngineTest, InitialDataCreation) {
  UpdateAllLifecyclePhases();

  // There should be no initial data if nothing is registered.
  EXPECT_FALSE(GetStyleEngine().MaybeCreateAndGetInitialData());

  // After registering, there should be initial data.
  css_test_helpers::RegisterProperty(GetDocument(), "--x", "<length>", "10px",
                                     false);
  auto data1 = GetStyleEngine().MaybeCreateAndGetInitialData();
  EXPECT_TRUE(data1);

  // After a full recalc, we should have the same initial data.
  GetDocument().body()->setInnerHTML("<style>* { font-size: 1px; } </style>");
  EXPECT_TRUE(GetDocument().documentElement()->NeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().documentElement()->ChildNeedsStyleRecalc());
  UpdateAllLifecyclePhases();
  auto data2 = GetStyleEngine().MaybeCreateAndGetInitialData();
  EXPECT_TRUE(data2);
  EXPECT_EQ(data1, data2);

  // After registering a new property, initial data should be invalidated,
  // such that the new initial data is different.
  css_test_helpers::RegisterProperty(GetDocument(), "--y", "<color>", "black",
                                     false);
  EXPECT_NE(data1, GetStyleEngine().MaybeCreateAndGetInitialData());
}

TEST_F(StyleEngineTest, CSSSelectorEmptyWhitespaceOnlyFail) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>.match:empty { background-color: red }</style>
    <div></div>
    <div> <span></span></div>
    <div> <!-- -->X</div>
    <div></div>
    <div> <!-- --></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);

  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSSelectorEmptyWhitespaceOnlyFail));

  auto* div_elements = GetDocument().getElementsByTagName("div");
  ASSERT_TRUE(div_elements);
  ASSERT_EQ(5u, div_elements->length());

  auto is_counted = [](Element* element) {
    element->setAttribute(blink::html_names::kClassAttr, "match");
    element->GetDocument().View()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
    return element->GetDocument().IsUseCounted(
        WebFeature::kCSSSelectorEmptyWhitespaceOnlyFail);
  };

  EXPECT_FALSE(is_counted(div_elements->item(0)));
  EXPECT_FALSE(is_counted(div_elements->item(1)));
  EXPECT_FALSE(is_counted(div_elements->item(2)));
  EXPECT_FALSE(is_counted(div_elements->item(3)));
  EXPECT_TRUE(is_counted(div_elements->item(4)));
}

TEST_F(StyleEngineTest, EnsuredComputedStyleRecalc) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div style="display:none">
      <div>
        <div id="computed">
          <span id="span"><span>XXX</span></span>
        </div>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* computed = GetDocument().getElementById("computed");
  Element* span_outer = GetDocument().getElementById("span");
  Node* span_inner = span_outer->firstChild();

  // Initially all null in display:none subtree.
  EXPECT_FALSE(computed->GetComputedStyle());
  EXPECT_FALSE(span_outer->GetComputedStyle());
  EXPECT_FALSE(span_inner->GetComputedStyle());

  // Force computed style down to #computed.
  computed->EnsureComputedStyle();
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(computed->GetComputedStyle());
  EXPECT_FALSE(span_outer->GetComputedStyle());
  EXPECT_FALSE(span_inner->GetComputedStyle());

  // Setting span color should not create ComputedStyles during style recalc.
  span_outer->SetInlineStyleProperty(CSSPropertyID::kColor, "blue");
  EXPECT_TRUE(span_outer->NeedsStyleRecalc());
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetStyleEngine().RecalcStyle();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

  EXPECT_FALSE(span_outer->NeedsStyleRecalc());
  EXPECT_FALSE(span_outer->GetComputedStyle());
  EXPECT_FALSE(span_inner->GetComputedStyle());
  // #computed still non-null because #span_outer is the recalc root.
  EXPECT_TRUE(computed->GetComputedStyle());

  // Triggering style recalc which propagates the color down the tree should
  // clear ComputedStyle objects in the display:none subtree.
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kColor, "pink");
  UpdateAllLifecyclePhases();

  EXPECT_FALSE(computed->GetComputedStyle());
  EXPECT_FALSE(span_outer->GetComputedStyle());
  EXPECT_FALSE(span_inner->GetComputedStyle());
}

TEST_F(StyleEngineTest, EnsureCustomComputedStyle) {
  GetDocument().body()->setInnerHTML("");
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=div>
      <progress id=progress>
    </div>
  )HTML");
  UpdateAllLifecyclePhases();

  // Note: <progress> is chosen because it creates ProgressShadowElement
  // instances, which override CustomStyleForLayoutObject with
  // display:none.
  Element* div = GetDocument().getElementById("div");
  Element* progress = GetDocument().getElementById("progress");
  ASSERT_TRUE(div);
  ASSERT_TRUE(progress);

  // This causes ProgressShadowElements to get ComputedStyles with
  // IsEnsuredInDisplayNone==true.
  for (Node* node = progress; node;
       node = FlatTreeTraversal::Next(*node, progress)) {
    node->EnsureComputedStyle();
  }

  // This triggers layout tree building.
  div->SetInlineStyleProperty(CSSPropertyID::kDisplay, "inline");
  UpdateAllLifecyclePhases();

  // We must not create LayoutObjects for Nodes with
  // IsEnsuredInDisplayNone==true
  for (Node* node = progress; node;
       node = FlatTreeTraversal::Next(*node, progress)) {
    ASSERT_TRUE(!node->GetComputedStyle() ||
                !node->ComputedStyleRef().IsEnsuredInDisplayNone() ||
                !node->GetLayoutObject());
  }
}

// Via HTMLFormControlElement, it's possible to enter
// Node::MarkAncestorsWithChildNeedsStyleRecalc for nodes which have
// isConnected==true, but an ancestor with isConnected==false. This is because
// we mark the ancestor chain for style recalc via HTMLFormElement::
// InvalidateDefaultButtonStyle while the subtree disconnection
// is taking place.
TEST_F(StyleEngineTest, NoCrashWhenMarkingPartiallyRemovedSubtree) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #foo:default {} /* Needed to enter Element::PseudoStateChanged */
    </style>
    <form id="form">
      <div id="outer">
        <button>
        <div id="inner"></div>
      </div>
    </form>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* form = GetDocument().getElementById("form");
  Element* outer = GetDocument().getElementById("outer");
  Element* inner = GetDocument().getElementById("inner");
  ASSERT_TRUE(form);
  ASSERT_TRUE(outer);
  ASSERT_TRUE(inner);

  // Add some more buttons, to give InvalidateDefaultButtonStyle
  // something to do when the original <button> is removed.
  inner->setInnerHTML("<button><button>");
  UpdateAllLifecyclePhases();

  form->removeChild(outer);
}

TEST_F(StyleEngineTest, ColorSchemeBaseBackgroundChange) {
  ScopedCSSColorSchemeForTest enable_color_scheme(true);
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kDark);
  UpdateAllLifecyclePhases();

  EXPECT_EQ(Color::kWhite, GetDocument().View()->BaseBackgroundColor());

  GetDocument().documentElement()->SetInlineStyleProperty(
      CSSPropertyID::kColorScheme, "dark");
  UpdateAllLifecyclePhases();

  EXPECT_EQ(Color(0x12, 0x12, 0x12),
            GetDocument().View()->BaseBackgroundColor());

  color_scheme_helper.SetForcedColors(GetDocument(), ForcedColors::kActive);
  UpdateAllLifecyclePhases();
  Color system_background_color = LayoutTheme::GetTheme().SystemColor(
      CSSValueID::kCanvas, ColorScheme::kLight);

  EXPECT_EQ(system_background_color,
            GetDocument().View()->BaseBackgroundColor());
}

TEST_F(StyleEngineTest, ColorSchemeOverride) {
  ScopedCSSColorSchemeForTest enable_color_scheme(true);
  ScopedCSSColorSchemeUARenderingForTest enable_color_scheme_ua(true);

  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kLight);

  GetDocument().documentElement()->SetInlineStyleProperty(
      CSSPropertyID::kColorScheme, "light dark");
  UpdateAllLifecyclePhases();

  EXPECT_EQ(
      ColorScheme::kLight,
      GetDocument().documentElement()->GetComputedStyle()->UsedColorScheme());

  GetDocument().GetPage()->SetMediaFeatureOverride("prefers-color-scheme",
                                                   "dark");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(
      ColorScheme::kDark,
      GetDocument().documentElement()->GetComputedStyle()->UsedColorScheme());
}

TEST_F(StyleEngineTest, PseudoElementBaseComputedStyle) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { background-color: white }
        to { background-color: blue }
      }
      #anim::before {
        content:"";
        animation: anim 1s;
      }
    </style>
    <div id="anim"></div>
  )HTML");

  UpdateAllLifecyclePhases();

  auto* anim_element = GetDocument().getElementById("anim");
  auto* before = anim_element->GetPseudoElement(kPseudoIdBefore);
  auto* animations = before->GetElementAnimations();

  ASSERT_TRUE(animations);

  before->SetNeedsAnimationStyleRecalc();
  UpdateAllLifecyclePhases();

  scoped_refptr<ComputedStyle> base_computed_style =
      animations->base_computed_style_;
  EXPECT_TRUE(base_computed_style);

  before->SetNeedsAnimationStyleRecalc();
  UpdateAllLifecyclePhases();

  EXPECT_TRUE(animations->base_computed_style_);
#if !DCHECK_IS_ON()
  // When DCHECK is enabled, BaseComputedStyle() returns null and we repeatedly
  // create new instances which means the pointers will be different here.
  EXPECT_EQ(base_computed_style, animations->base_computed_style_);
#endif
}

TEST_F(StyleEngineTest, NeedsLayoutTreeRebuild) {
  UpdateAllLifecyclePhases();

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(GetStyleEngine().NeedsLayoutTreeRebuild());

  GetDocument().documentElement()->SetInlineStyleProperty(
      CSSPropertyID::kDisplay, "none");

  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetDocument().GetStyleEngine().RecalcStyle();

  EXPECT_TRUE(GetStyleEngine().NeedsLayoutTreeRebuild());
}

TEST_F(StyleEngineTest, ForceReattachLayoutTreeStyleRecalcRoot) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="outer">
      <div id="inner"></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* outer = GetDocument().getElementById("outer");
  Element* inner = GetDocument().getElementById("inner");

  outer->SetForceReattachLayoutTree();
  inner->SetInlineStyleProperty(CSSPropertyID::kColor, "blue");

  EXPECT_EQ(outer, GetStyleRecalcRoot());
}

TEST_F(StyleEngineTest, RecalcPropagatedWritingMode) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kWritingMode,
                                               "vertical-lr");

  UpdateAllLifecyclePhases();

  // Make sure that recalculating style for the root element does not trigger a
  // visual diff that requires layout. That is, we take the body -> root
  // propagation of writing-mode into account before setting ComputedStyle on
  // the root LayoutObject.
  GetDocument().documentElement()->SetInlineStyleProperty(
      CSSPropertyID::kWritingMode, "horizontal-tb");

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetDocument().GetStyleEngine().RecalcStyle();

  EXPECT_FALSE(GetStyleEngine().NeedsLayoutTreeRebuild());
  EXPECT_FALSE(GetDocument().View()->NeedsLayout());
}

TEST_F(StyleEngineTest, GetComputedStyleOutsideFlatTree) {
  GetDocument().body()->setInnerHTML(
      R"HTML(<div id="host"><div id="outer"><div id="inner"><div id="innermost"></div></div></div></div>)HTML");

  auto* host = GetDocument().getElementById("host");
  auto* outer = GetDocument().getElementById("outer");
  auto* inner = GetDocument().getElementById("inner");
  auto* innermost = GetDocument().getElementById("innermost");

  host->AttachShadowRootInternal(ShadowRootType::kOpen);
  UpdateAllLifecyclePhases();

  EXPECT_TRUE(host->GetComputedStyle());
  // ComputedStyle is not generated outside the flat tree.
  EXPECT_FALSE(outer->GetComputedStyle());
  EXPECT_FALSE(inner->GetComputedStyle());
  EXPECT_FALSE(innermost->GetComputedStyle());

  inner->EnsureComputedStyle();
  scoped_refptr<const ComputedStyle> outer_style = outer->GetComputedStyle();
  scoped_refptr<const ComputedStyle> inner_style = inner->GetComputedStyle();

  ASSERT_TRUE(outer_style);
  ASSERT_TRUE(inner_style);
  EXPECT_FALSE(innermost->GetComputedStyle());
  EXPECT_TRUE(outer_style->IsEnsuredOutsideFlatTree());
  EXPECT_TRUE(inner_style->IsEnsuredOutsideFlatTree());
  EXPECT_EQ(Color::kTransparent, inner_style->VisitedDependentColor(
                                     GetCSSPropertyBackgroundColor()));

  inner->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor, "green");
  UpdateAllLifecyclePhases();

  // Old ensured style is not cleared before we re-ensure it.
  EXPECT_TRUE(inner->NeedsStyleRecalc());
  EXPECT_EQ(inner_style, inner->GetComputedStyle());

  inner->EnsureComputedStyle();

  // Outer style was not dirty - we still have the same ComputedStyle object.
  EXPECT_EQ(outer_style, outer->GetComputedStyle());
  EXPECT_NE(inner_style, inner->GetComputedStyle());

  inner_style = inner->GetComputedStyle();
  EXPECT_EQ(Color(0, 128, 0), inner_style->VisitedDependentColor(
                                  GetCSSPropertyBackgroundColor()));

  // Making outer dirty will require that we clear ComputedStyles all the way up
  // ensuring the style for innermost later because of inheritance.
  outer->SetInlineStyleProperty(CSSPropertyID::kColor, "green");
  UpdateAllLifecyclePhases();

  EXPECT_EQ(outer_style, outer->GetComputedStyle());
  EXPECT_EQ(inner_style, inner->GetComputedStyle());
  EXPECT_FALSE(innermost->GetComputedStyle());

  auto* innermost_style = innermost->EnsureComputedStyle();

  EXPECT_NE(outer_style, outer->GetComputedStyle());
  EXPECT_NE(inner_style, inner->GetComputedStyle());
  ASSERT_TRUE(innermost_style);
  EXPECT_EQ(Color(0, 128, 0),
            innermost_style->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MoveSlottedOutsideFlatTree) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="host1"><span></span></div>
    <div id="host2"></div>
  )HTML");

  auto* host1 = GetDocument().getElementById("host1");
  auto* host2 = GetDocument().getElementById("host2");
  auto* span = host1->firstChild();

  ShadowRoot& shadow_root =
      host1->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML("<slot></slot>");
  host2->AttachShadowRootInternal(ShadowRootType::kOpen);

  UpdateAllLifecyclePhases();

  host2->appendChild(span);
  EXPECT_FALSE(GetStyleRecalcRoot());

  span->remove();
  EXPECT_FALSE(GetStyleRecalcRoot());
}

TEST_F(StyleEngineTest, StyleRecalcRootInShadowTree) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="host"></div>
  )HTML");
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML("<div><span></span></div>");
  UpdateAllLifecyclePhases();

  Element* span = To<Element>(shadow_root.firstChild()->firstChild());
  // Mark style dirty.
  span->SetInlineStyleProperty(CSSPropertyID::kColor, "blue");

  EXPECT_EQ(span, GetStyleRecalcRoot());
}

TEST_F(StyleEngineTest, StyleRecalcRootOutsideFlatTree) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="host"><div id="ensured"><span></span></div></div>
    <div id="dirty"></div>
  )HTML");

  auto* host = GetDocument().getElementById("host");
  auto* dirty = GetDocument().getElementById("dirty");
  auto* ensured = GetDocument().getElementById("ensured");
  auto* span = To<Element>(ensured->firstChild());

  host->AttachShadowRootInternal(ShadowRootType::kOpen);

  UpdateAllLifecyclePhases();

  dirty->SetInlineStyleProperty(CSSPropertyID::kColor, "blue");
  EXPECT_EQ(dirty, GetStyleRecalcRoot());

  // Ensure a computed style for the span parent to try to trick us into
  // incorrectly using the span as a recalc root.
  ensured->EnsureComputedStyle();
  span->SetInlineStyleProperty(CSSPropertyID::kColor, "pink");

  // <span> is outside the flat tree, so it should not affect the style recalc
  // root.
  EXPECT_EQ(dirty, GetStyleRecalcRoot());

  // Should not trigger any DCHECK failures.
  UpdateAllLifecyclePhases();
}

TEST_F(StyleEngineTest, RemoveStyleRecalcRootFromFlatTree) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=host><span></span></div>
  )HTML");

  auto* host = GetDocument().getElementById("host");
  auto* span = To<Element>(host->firstChild());

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML("<div><slot></slot></div>");

  UpdateAllLifecyclePhases();

  // Make the span style dirty.
  span->setAttribute("style", "color:green");

  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_EQ(span, GetStyleRecalcRoot());

  auto* div = shadow_root.firstChild();
  auto* slot = To<Element>(div->firstChild());

  slot->setAttribute("name", "x");
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();

  // Make sure shadow tree div and slot have their ChildNeedsStyleRecalc()
  // cleared.
  EXPECT_FALSE(div->ChildNeedsStyleRecalc());
  EXPECT_FALSE(slot->ChildNeedsStyleRecalc());
  EXPECT_FALSE(span->NeedsStyleRecalc());
  EXPECT_FALSE(GetStyleRecalcRoot());
}

TEST_F(StyleEngineTest, SlottedWithEnsuredStyleOutsideFlatTree) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="host"><span></span></div>
  )HTML");

  auto* host = GetDocument().getElementById("host");
  auto* span = To<Element>(host->firstChild());

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML(R"HTML(
    <div><slot name="default"></slot></div>
  )HTML");

  UpdateAllLifecyclePhases();

  // Ensure style outside the flat tree.
  const ComputedStyle* style = span->EnsureComputedStyle();
  ASSERT_TRUE(style);
  EXPECT_TRUE(style->IsEnsuredOutsideFlatTree());

  span->setAttribute("slot", "default");
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
  EXPECT_EQ(span, GetStyleRecalcRoot());
  EXPECT_FALSE(span->GetComputedStyle());
}

TEST_F(StyleEngineTest, RecalcEnsuredStyleOutsideFlatTreeV0) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="host"><span></span></div>
  )HTML");

  auto* host = GetDocument().getElementById("host");
  auto* span = To<Element>(host->firstChild());

  host->CreateV0ShadowRootForTesting();
  UpdateAllLifecyclePhases();

  EXPECT_FALSE(span->FlatTreeParentForChildDirty());

  // Ensure style outside the flat tree.
  const ComputedStyle* style = span->EnsureComputedStyle();
  ASSERT_TRUE(style);
  EXPECT_TRUE(style->IsEnsuredOutsideFlatTree());
  EXPECT_EQ(EDisplay::kInline, style->Display());

  span->SetInlineStyleProperty(CSSPropertyID::kDisplay, "block");
  EXPECT_FALSE(GetStyleRecalcRoot());
  EXPECT_FALSE(GetDocument().body()->ChildNeedsStyleRecalc());
}

TEST_F(StyleEngineTest, ForceReattachRecalcRootAttachShadow) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="reattach"></div><div id="host"><span></span></div>
  )HTML");

  auto* reattach = GetDocument().getElementById("reattach");
  auto* host = GetDocument().getElementById("host");

  UpdateAllLifecyclePhases();

  reattach->SetForceReattachLayoutTree();
  EXPECT_FALSE(reattach->NeedsStyleRecalc());
  EXPECT_EQ(reattach, GetStyleRecalcRoot());

  // Attaching the shadow root will call RemovedFromFlatTree() on the span child
  // of the host. The style recalc root should still be #reattach.
  host->AttachShadowRootInternal(ShadowRootType::kOpen);
  EXPECT_EQ(reattach, GetStyleRecalcRoot());
}

TEST_F(StyleEngineTest, InitialColorChange) {
  // Set color scheme to light.
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kLight);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      :root { color-scheme: light dark }
      #initial { color: initial }
    </style>
    <div id="initial"></div>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* initial = GetDocument().getElementById("initial");
  ASSERT_TRUE(initial);
  ASSERT_TRUE(GetDocument().documentElement());
  const ComputedStyle* document_element_style =
      GetDocument().documentElement()->GetComputedStyle();
  ASSERT_TRUE(document_element_style);
  EXPECT_EQ(Color::kBlack, document_element_style->VisitedDependentColor(
                               GetCSSPropertyColor()));

  const ComputedStyle* initial_style = initial->GetComputedStyle();
  ASSERT_TRUE(initial_style);
  EXPECT_EQ(Color::kBlack,
            initial_style->VisitedDependentColor(GetCSSPropertyColor()));

  // Change color scheme to dark.
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kDark);
  UpdateAllLifecyclePhases();

  document_element_style = GetDocument().documentElement()->GetComputedStyle();
  ASSERT_TRUE(document_element_style);
  EXPECT_EQ(Color::kWhite, document_element_style->VisitedDependentColor(
                               GetCSSPropertyColor()));

  initial_style = initial->GetComputedStyle();
  ASSERT_TRUE(initial_style);
  EXPECT_EQ(Color::kWhite,
            initial_style->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_InvalidateForChangedSizeQueries) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @media (min-width: 1000px) {
        div { color: green }
      }
    </style>
    <style>
      @media (min-width: 1200px) {
        * { color: red }
      }
    </style>
    <style>
      @media print {
        * { color: blue }
      }
    </style>
    <div id="green"></div>
    <span></span>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* div = GetDocument().getElementById("green");
  EXPECT_EQ(Color::kBlack, div->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  unsigned initial_count = GetStyleEngine().StyleForElementCount();

  GetDocument().View()->SetLayoutSizeFixedToFrameSize(false);
  GetDocument().View()->SetLayoutSize(IntSize(1100, 800));
  UpdateAllLifecyclePhases();

  // Only the single div element should have its style recomputed.
  EXPECT_EQ(1u, GetStyleEngine().StyleForElementCount() - initial_count);
  EXPECT_EQ(MakeRGB(0, 128, 0), div->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_InvalidateForChangedTypeQuery) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @media speech {
        div { color: green }
      }
    </style>
    <style>
      @media (max-width: 100px) {
        * { color: red }
      }
    </style>
    <style>
      @media print {
        * { color: blue }
      }
    </style>
    <div id="green"></div>
    <span></span>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* div = GetDocument().getElementById("green");
  EXPECT_EQ(Color::kBlack, div->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  unsigned initial_count = GetStyleEngine().StyleForElementCount();

  GetDocument().GetSettings()->SetMediaTypeOverride("speech");
  UpdateAllLifecyclePhases();

  // Only the single div element should have its style recomputed.
  EXPECT_EQ(1u, GetStyleEngine().StyleForElementCount() - initial_count);
  EXPECT_EQ(MakeRGB(0, 128, 0), div->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest,
       MediaQueryAffectingValueChanged_InvalidateForChangedReducedMotionQuery) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @media (prefers-reduced-motion: reduce) {
        div { color: green }
      }
    </style>
    <style>
      @media (max-width: 100px) {
        * { color: red }
      }
    </style>
    <style>
      @media print {
        * { color: blue }
      }
    </style>
    <div id="green"></div>
    <span></span>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* div = GetDocument().getElementById("green");
  EXPECT_EQ(Color::kBlack, div->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  unsigned initial_count = GetStyleEngine().StyleForElementCount();

  GetDocument().GetSettings()->SetPrefersReducedMotion(true);
  UpdateAllLifecyclePhases();

  // Only the single div element should have its style recomputed.
  EXPECT_EQ(1u, GetStyleEngine().StyleForElementCount() - initial_count);
  EXPECT_EQ(MakeRGB(0, 128, 0), div->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, SummaryDisplayUseCount) {
  // Should not be use-counted: wrong element type.
  GetDocument().body()->setInnerHTML(
      "<style>div { display: block; }</style><div></div>");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kSummaryElementWithDisplayBlockAuthorRule));

  // Should not be use-counted: wrong display type:
  GetDocument().body()->setInnerHTML(
      "<style>summary { display: inline; }</style><summary></summary>");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kSummaryElementWithDisplayBlockAuthorRule));

  // Should be use-counted:
  GetDocument().body()->setInnerHTML(
      "<style>summary { display: block; }</style><summary></summary>");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kSummaryElementWithDisplayBlockAuthorRule));
}

TEST_F(StyleEngineTest, RevertUseCount) {
  ScopedCSSRevertForTest scoped_feature(true);

  GetDocument().body()->setInnerHTML(
      "<style>div { display: unset; }</style><div></div>");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSKeywordRevert));

  GetDocument().body()->setInnerHTML(
      "<style>div { display: revert; }</style><div></div>");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSKeywordRevert));
}

TEST_F(StyleEngineTest, RevertUseCountForCustomProperties) {
  ScopedCSSRevertForTest scoped_feature(true);

  GetDocument().body()->setInnerHTML(
      "<style>div { --x: unset; }</style><div></div>");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSKeywordRevert));

  GetDocument().body()->setInnerHTML(
      "<style>div { --x: revert; }</style><div></div>");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSKeywordRevert));
}

TEST_F(StyleEngineTest, NoRevertUseCountForForcedColors) {
  ScopedForcedColorsForTest scoped_feature(true);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #elem { color: red; }
    </style>
    <div id=ref></div>
    <div id=elem></div>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* ref = GetDocument().getElementById("ref");
  Element* elem = GetDocument().getElementById("elem");
  ASSERT_TRUE(ref);
  ASSERT_TRUE(elem);

  // This test assumes that the initial color is not 'red'. Verify that
  // assumption.
  ASSERT_NE(ComputedValue(ref, "color")->CssText(),
            ComputedValue(elem, "color")->CssText());

  EXPECT_EQ("rgb(255, 0, 0)", ComputedValue(elem, "color")->CssText());

  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetForcedColors(GetDocument(), ForcedColors::kActive);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(ComputedValue(ref, "color")->CssText(),
            ComputedValue(elem, "color")->CssText());

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSKeywordRevert));
}

TEST_F(StyleEngineTest, PrintNoDarkColorScheme) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(PreferredColorScheme::kDark);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      :root { color-scheme: light dark }
      @media (prefers-color-scheme: light) {
        body { color: green; }
      }
      @media (prefers-color-scheme: dark) {
        body { color: red; }
      }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  Element* body = GetDocument().body();
  Element* root = GetDocument().documentElement();

  EXPECT_EQ(Color::kWhite, root->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));
  EXPECT_EQ(ColorScheme::kDark,
            root->GetComputedStyle()->UsedColorSchemeForInitialColors());
  EXPECT_EQ(MakeRGB(255, 0, 0), body->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  FloatSize page_size(400, 400);
  GetDocument().GetFrame()->StartPrinting(page_size, page_size, 1);
  EXPECT_EQ(Color::kBlack, root->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));
  EXPECT_EQ(ColorScheme::kLight,
            root->GetComputedStyle()->UsedColorSchemeForInitialColors());
  EXPECT_EQ(MakeRGB(0, 128, 0), body->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));

  GetDocument().GetFrame()->EndPrinting();
  EXPECT_EQ(Color::kWhite, root->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));
  EXPECT_EQ(ColorScheme::kDark,
            root->GetComputedStyle()->UsedColorSchemeForInitialColors());
  EXPECT_EQ(MakeRGB(255, 0, 0), body->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, AtPropertyUseCount) {
  ScopedCSSVariables2AtPropertyForTest scoped_feature(true);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { --x: No @property rule here; }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSAtRuleProperty));

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @property --x {
        syntax: "<length>";
        inherits: false;
        initial-value: 0px;
      }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSAtRuleProperty));
}

TEST_F(StyleEngineTest, AtScrollTimelineUseCount) {
  ScopedCSSScrollTimelineForTest scoped_feature(true);

  GetDocument().body()->setInnerHTML("<div>No @scroll-timline</div>");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kCSSAtRuleScrollTimeline));

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @scroll-timeline foo { }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSAtRuleScrollTimeline));
}

TEST_F(StyleEngineTest, MediaQueryAffectedByViewportSanityCheck) {
  GetDocument().body()->setInnerHTML("<audio controls>");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetStyleEngine().MediaQueryAffectedByViewportChange());
}

TEST_F(StyleEngineTest, RemoveDeclaredPropertiesEmptyRegistry) {
  ScopedCSSVariables2AtPropertyForTest scoped_feature(true);

  EXPECT_FALSE(GetDocument().GetPropertyRegistry());
  PropertyRegistration::RemoveDeclaredProperties(GetDocument());
  EXPECT_FALSE(GetDocument().GetPropertyRegistry());
}

TEST_F(StyleEngineTest, AtPropertyInUserOrigin) {
  // @property in the user origin:
  InjectSheet("user1", WebDocument::kUserOrigin, R"CSS(
    @property --x {
      syntax: "<length>";
      inherits: false;
      initial-value: 10px;
    }
  )CSS");
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(ComputedValue(GetDocument().body(), "--x"));
  EXPECT_EQ("10px", ComputedValue(GetDocument().body(), "--x")->CssText());

  // @property in the author origin (should win over user origin)
  InjectSheet("author", WebDocument::kAuthorOrigin, R"CSS(
    @property --x {
      syntax: "<length>";
      inherits: false;
      initial-value: 20px;
    }
  )CSS");
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(ComputedValue(GetDocument().body(), "--x"));
  EXPECT_EQ("20px", ComputedValue(GetDocument().body(), "--x")->CssText());

  // An additional @property in the user origin:
  InjectSheet("user2", WebDocument::kUserOrigin, R"CSS(
    @property --y {
      syntax: "<length>";
      inherits: false;
      initial-value: 30px;
    }
  )CSS");
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(ComputedValue(GetDocument().body(), "--x"));
  ASSERT_TRUE(ComputedValue(GetDocument().body(), "--y"));
  EXPECT_EQ("20px", ComputedValue(GetDocument().body(), "--x")->CssText());
  EXPECT_EQ("30px", ComputedValue(GetDocument().body(), "--y")->CssText());
}

TEST_F(StyleEngineTest, SystemColorComputeToSelfUseCount) {
  // Don't count system color use by itself - only in conjunction with
  // color-scheme.
  GetDocument().body()->setInnerHTML(
      "<style>div { color: MenuText; }</style><div></div>");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kCSSSystemColorComputeToSelf));

  // Count system color use when used on an element with a different
  // color-scheme from its parent.
  GetDocument().body()->setInnerHTML(
      "<style>"
      "div { color: MenuText; color-scheme: dark; }"
      "</style><div></div>");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kCSSSystemColorComputeToSelf));
}

TEST_F(StyleEngineTest, InvalidVariableUnsetUseCount) {
  // Do not count for basic variable usage.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #outer { --x: foo; }
      #inner { --x: bar; }
    </style>
    <div id=outer>
      <div id=inner></div>
    <div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSInvalidVariableUnset));
  ClearUseCounter(WebFeature::kCSSInvalidVariableUnset);

  // Do not count when a fallback handles the unknown variable.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #outer { --x: foo; }
      #inner { --x: var(--unknown,bar); }
    </style>
    <div id=outer>
      <div id=inner></div>
    <div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSInvalidVariableUnset));
  ClearUseCounter(WebFeature::kCSSInvalidVariableUnset);

  // Do not count for explicit 'unset'.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #outer { --x: foo; }
      #inner { --x: unset; }
    </style>
    <div id=outer>
      <div id=inner></div>
    <div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSInvalidVariableUnset));
  ClearUseCounter(WebFeature::kCSSInvalidVariableUnset);

  // Do not count when we anyway end up with the guaranteed-invalid value.
  // (Applies to registered properties as well).
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @property --y {
        syntax: "*";
        inherits: true;
      }
      @property --z {
        syntax: "*";
        inherits: false;
      }
      #inner {
        --x: var(--unknown);
        --y: var(--unknown);
        --z: var(--unknown);
      }
    </style>
    <div id=outer>
      <div id=inner></div>
    <div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSInvalidVariableUnset));
  ClearUseCounter(WebFeature::kCSSInvalidVariableUnset);

  // Count when 'unset' inherits something that not guaranteed-invalid.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #outer { --x: foo; }
      #inner { --x: var(--unknown); }
    </style>
    <div id=outer>
      <div id=inner></div>
    <div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSInvalidVariableUnset));
  ClearUseCounter(WebFeature::kCSSInvalidVariableUnset);

  // Do not count for non-universal registered custom properties.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @property --x {
        syntax: "<length>";
        inherits: true;
        initial-value: 0px;
      }
      #outer { --x: 1px; }
      #inner { --x: var(--unknown); }
    </style>
    <div id=outer>
      <div id=inner></div>
    <div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSInvalidVariableUnset));
  ClearUseCounter(WebFeature::kCSSInvalidVariableUnset);

  // Count for universal registered custom properties.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @property --x {
        syntax: "*";
        inherits: true;
      }
      #outer { --x: bar; }
      #inner { --x: var(--unknown); }
    </style>
    <div id=outer>
      <div id=inner></div>
    <div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSInvalidVariableUnset));
  ClearUseCounter(WebFeature::kCSSInvalidVariableUnset);

  // Do not count for non-inherited universal registered custom properties
  // without initial value.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @property --x {
        syntax: "*";
        inherits: false;
      }
      #outer { --x: bar; }
      #inner { --x: var(--unknown); }
    </style>
    <div id=outer>
      <div id=inner></div>
    <div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSInvalidVariableUnset));
  ClearUseCounter(WebFeature::kCSSInvalidVariableUnset);

  // Count for universal registered custom properties even with an
  // initial-value defined.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @property --x {
        syntax: "*";
        inherits: true;
        initial-value: foo;
      }
      #outer { --x: bar; }
      #inner { --x: var(--unknown); }
    </style>
    <div id=outer>
      <div id=inner></div>
    <div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSInvalidVariableUnset));
  ClearUseCounter(WebFeature::kCSSInvalidVariableUnset);

  // Do not count for cycles.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @property --a {
        syntax: "*";
        inherits: true;
      }
      @property --b {
        syntax: "*";
        inherits: true;
      }
      #outer {
        --a: foo;
        --b: foo;
        --c: foo;
        --d: foo;
      }
      #inner {
        --a: var(--b);
        --b: var(--a);
        --c: var(--d);
        --d: var(--c);
      }
    </style>
    <div id=outer>
      <div id=inner></div>
    <div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSInvalidVariableUnset));
  ClearUseCounter(WebFeature::kCSSInvalidVariableUnset);

  // Count for @keyframes
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { --x: var(--unknown); }
        to { --x: var(--unknown); }
      }
      #outer {
        --x: foo;
      }
      #inner {
        animation: anim 10s;
      }
    </style>
    <div id=outer>
      <div id=inner></div>
    <div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSInvalidVariableUnset));
  ClearUseCounter(WebFeature::kCSSInvalidVariableUnset);

  // Don't count for @keyframes if there's nothing to inherit.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { --x: var(--unknown); }
        to { --x: var(--unknown); }
      }
      #inner {
        animation: anim 10s;
      }
    </style>
    <div id=outer>
      <div id=inner></div>
    <div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSInvalidVariableUnset));
  ClearUseCounter(WebFeature::kCSSInvalidVariableUnset);
}

// https://crbug.com/1050564
TEST_F(StyleEngineTest, MediaAttributeChangeUpdatesFontCacheVersion) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @font-face { font-family: custom-font; src: url(fake-font.woff); }
    </style>
    <style id=target>
      .display-none { display: none; }
    </style>
    <div style="font-family: custom-font">foo</div>
    <div class="display-none">bar</div>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* target = GetDocument().getElementById("target");
  target->setAttribute(html_names::kMediaAttr, "print");

  // Shouldn't crash.
  UpdateAllLifecyclePhases();
}

class StyleEngineSimTest : public SimTest {};

TEST_F(StyleEngineSimTest, OwnerColorScheme) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");

  LoadURL("https://example.com");

  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      iframe { color-scheme: dark }
    </style>
    <iframe id="frame" src="https://example.com/frame.html"></iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
    <!doctype html>
    <p>Frame</p>
  )HTML");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  auto* frame_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("frame"));
  auto* frame_document = frame_element->contentDocument();
  ASSERT_TRUE(frame_document);
  EXPECT_EQ(ColorScheme::kDark,
            frame_document->GetStyleEngine().GetOwnerColorScheme());

  frame_element->SetInlineStyleProperty(CSSPropertyID::kColorScheme, "light");

  test::RunPendingTasks();
  Compositor().BeginFrame();
  EXPECT_EQ(ColorScheme::kLight,
            frame_document->GetStyleEngine().GetOwnerColorScheme());
}

TEST_F(StyleEngineSimTest, OwnerColorSchemeBaseBackground) {
  ScopedCSSColorSchemeForTest enable_color_scheme(true);

  SimRequest main_resource("https://example.com", "text/html");
  SimRequest dark_frame_resource("https://example.com/dark.html", "text/html");
  SimRequest light_frame_resource("https://example.com/light.html",
                                  "text/html");

  LoadURL("https://example.com");

  main_resource.Complete(R"HTML(
    <style>
      .dark { color-scheme: dark }
    </style>
    <iframe id="dark-frame" src="dark.html"></iframe>
    <iframe id="light-frame" src="light.html"></iframe>
  )HTML");

  dark_frame_resource.Complete(R"HTML(
    <!doctype html>
    <meta name=color-scheme content="dark">
    <p>Frame</p>
  )HTML");

  light_frame_resource.Complete(R"HTML(
    <!doctype html>
    <p>Frame</p>
  )HTML");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  auto* dark_document =
      To<HTMLIFrameElement>(GetDocument().getElementById("dark-frame"))
          ->contentDocument();
  auto* light_document =
      To<HTMLIFrameElement>(GetDocument().getElementById("light-frame"))
          ->contentDocument();
  ASSERT_TRUE(dark_document);
  ASSERT_TRUE(light_document);

  EXPECT_TRUE(dark_document->View()->ShouldPaintBaseBackgroundColor());
  EXPECT_EQ(Color(0x12, 0x12, 0x12),
            dark_document->View()->BaseBackgroundColor());
  EXPECT_FALSE(light_document->View()->ShouldPaintBaseBackgroundColor());

  GetDocument().documentElement()->setAttribute(blink::html_names::kClassAttr,
                                                "dark");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_FALSE(dark_document->View()->ShouldPaintBaseBackgroundColor());
  EXPECT_TRUE(light_document->View()->ShouldPaintBaseBackgroundColor());
  EXPECT_EQ(Color::kWhite, light_document->View()->BaseBackgroundColor());
}

}  // namespace blink
