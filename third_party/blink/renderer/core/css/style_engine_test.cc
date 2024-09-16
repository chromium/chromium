// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_engine.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/public/common/css/navigation_controls.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/cascade_layer.h"
#include "third_party/blink/renderer/core/css/cascade_layer_map.h"
#include "third_party/blink/renderer/core/css/counter_style.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_media_rule.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/media_query_list.h"
#include "third_party/blink/renderer/core/css/media_query_list_listener.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_stats.h"
#include "third_party/blink/renderer/core/css/style_scope_frame.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/layout/custom_scrollbar.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_custom_scrollbar_part.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/list/list_marker.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "ui/base/mojom/window_show_state.mojom-blink.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

class StyleEngineTest : public PageTestBase {
 protected:
  bool IsDocumentStyleSheetCollectionClean() {
    return !GetStyleEngine().ShouldUpdateDocumentStyleSheetCollection();
  }

  void ApplyRuleSetInvalidation(TreeScope&, const String& css_text);

  // A wrapper to add a reason for UpdateAllLifecyclePhases
  void UpdateAllLifecyclePhases() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
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
        /* allow_visited_style */ false, CSSValuePhase::kResolvedValue);
  }

  void InjectSheet(String key, WebCssOrigin origin, String text) {
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

  String GetListMarkerText(LayoutObject* list_item) {
    LayoutObject* marker = ListMarker::MarkerFromListItem(list_item);
    return ListMarker::Get(marker)->GetTextChild(*marker).TransformedText();
  }

  size_t FillOrClipPathCacheSize() {
    return GetStyleEngine().fill_or_clip_path_uri_value_cache_.size();
  }

  void SimulateFrame() {
    auto new_time = GetAnimationClock().CurrentTime() + base::Milliseconds(100);
    GetPage().Animator().ServiceScriptedAnimations(new_time);
  }

  std::unique_ptr<DummyPageHolder> DummyPageHolderWithHTML(String html) {
    auto holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
    holder->GetDocument().documentElement()->setInnerHTML(html);
    holder->GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    return holder;
  }
};

class StyleEngineContainerQueryTest : public StyleEngineTest {};

void StyleEngineTest::ApplyRuleSetInvalidation(TreeScope& tree_scope,
                                               const String& css_text) {
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(
          kHTMLStandardMode, SecureContextMode::kInsecureContext));
  sheet->ParseString(css_text);
  HeapHashSet<Member<RuleSet>> rule_sets;
  RuleSet& rule_set =
      sheet->EnsureRuleSet(MediaQueryEvaluator(GetDocument().GetFrame()));
  rule_set.CompactRulesIfNeeded();
  rule_sets.insert(&rule_set);
  SelectorFilter selector_filter;
  selector_filter.PushAllParentsOf(tree_scope);
  StyleScopeFrame style_scope_frame(
      IsA<ShadowRoot>(tree_scope)
          ? To<ShadowRoot>(tree_scope).host()
          : *tree_scope.GetDocument().documentElement());
  GetStyleEngine().ApplyRuleSetInvalidationForTreeScope(
      tree_scope, tree_scope.RootNode(), selector_filter, style_scope_frame,
      rule_sets, /*changed_rule_flags=*/0);
}

TEST_F(StyleEngineTest, DocumentDirtyAfterInject) {
  auto* parsed_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  parsed_sheet->ParseString("div {}");
  GetStyleEngine().InjectSheet(g_empty_atom, parsed_sheet);
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

  Element* t1 = GetDocument().getElementById(AtomicString("t1"));
  Element* t2 = GetDocument().getElementById(AtomicString("t2"));
  Element* t3 = GetDocument().getElementById(AtomicString("t3"));
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t2);
  ASSERT_TRUE(t3);
  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      t1->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      t2->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      t3->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  const unsigned initial_count = GetStyleEngine().StyleForElementCount();

  auto* green_parsed_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  green_parsed_sheet->ParseString(
      "#t1 { color: green !important }"
      "#t2 { color: white !important }"
      "#t3 { color: white }");
  StyleSheetKey green_key("green");
  GetStyleEngine().InjectSheet(green_key, green_parsed_sheet,
                               WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();

  EXPECT_EQ(3u, GetStyleEngine().StyleForElementCount() - initial_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());

  // Important user rules override both regular and important author rules.
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      t1->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(255, 255, 255),
      t2->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      t3->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  auto* blue_parsed_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  blue_parsed_sheet->ParseString(
      "#t1 { color: blue !important }"
      "#t2 { color: silver }"
      "#t3 { color: silver !important }");
  StyleSheetKey blue_key("blue");
  GetStyleEngine().InjectSheet(blue_key, blue_parsed_sheet,
                               WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();

  EXPECT_EQ(6u, GetStyleEngine().StyleForElementCount() - initial_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());

  // Only important user rules override previously set important user rules.
  EXPECT_EQ(
      Color::FromRGB(0, 0, 255),
      t1->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(255, 255, 255),
      t2->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  // Important user rules override inline author rules.
  EXPECT_EQ(
      Color::FromRGB(192, 192, 192),
      t3->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(green_key, WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(9u, GetStyleEngine().StyleForElementCount() - initial_count);
  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());

  // Regular user rules do not override author rules.
  EXPECT_EQ(
      Color::FromRGB(0, 0, 255),
      t1->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      t2->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(192, 192, 192),
      t3->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(blue_key, WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(12u, GetStyleEngine().StyleForElementCount() - initial_count);
  ASSERT_TRUE(t1->GetComputedStyle());
  ASSERT_TRUE(t2->GetComputedStyle());
  ASSERT_TRUE(t3->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      t1->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      t2->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      t3->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  // @font-face rules

  Element* t4 = GetDocument().getElementById(AtomicString("t4"));
  ASSERT_TRUE(t4);
  ASSERT_TRUE(t4->GetComputedStyle());

  // There's only one font and it's bold and normal.
  EXPECT_EQ(1u, GetStyleEngine()
                    .GetFontSelector()
                    ->GetFontFaceCache()
                    ->GetNumSegmentedFacesForTesting());
  CSSSegmentedFontFace* font_face =
      GetStyleEngine().GetFontSelector()->GetFontFaceCache()->Get(
          t4->GetComputedStyle()->GetFontDescription(),
          AtomicString("Cool Font"));
  EXPECT_TRUE(font_face);
  FontSelectionCapabilities capabilities =
      font_face->GetFontSelectionCapabilities();
  ASSERT_EQ(capabilities.weight,
            FontSelectionRange({kBoldWeightValue, kBoldWeightValue}));
  ASSERT_EQ(capabilities.slope,
            FontSelectionRange({kNormalSlopeValue, kNormalSlopeValue}));

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
                               WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();

  // After injecting a more specific font, now there are two and the
  // bold-italic one is selected.
  EXPECT_EQ(2u, GetStyleEngine()
                    .GetFontSelector()
                    ->GetFontFaceCache()
                    ->GetNumSegmentedFacesForTesting());
  font_face = GetStyleEngine().GetFontSelector()->GetFontFaceCache()->Get(
      t4->GetComputedStyle()->GetFontDescription(), AtomicString("Cool Font"));
  EXPECT_TRUE(font_face);
  capabilities = font_face->GetFontSelectionCapabilities();
  ASSERT_EQ(capabilities.weight,
            FontSelectionRange({kBoldWeightValue, kBoldWeightValue}));
  ASSERT_EQ(capabilities.slope,
            FontSelectionRange({kItalicSlopeValue, kItalicSlopeValue}));

  auto* style_element = MakeGarbageCollected<HTMLStyleElement>(GetDocument());
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
  EXPECT_EQ(3u, GetStyleEngine()
                    .GetFontSelector()
                    ->GetFontFaceCache()
                    ->GetNumSegmentedFacesForTesting());
  font_face = GetStyleEngine().GetFontSelector()->GetFontFaceCache()->Get(
      t4->GetComputedStyle()->GetFontDescription(), AtomicString("Cool Font"));
  EXPECT_TRUE(font_face);
  capabilities = font_face->GetFontSelectionCapabilities();
  ASSERT_EQ(capabilities.weight,
            FontSelectionRange({kBoldWeightValue, kBoldWeightValue}));
  ASSERT_EQ(capabilities.slope,
            FontSelectionRange({kItalicSlopeValue, kItalicSlopeValue}));

  GetStyleEngine().RemoveInjectedSheet(font_face_key, WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();

  // After removing the injected style sheet we're left with a bold-normal and
  // a normal-italic font, and the latter is selected by the matching algorithm
  // as font-style trumps font-weight.
  EXPECT_EQ(2u, GetStyleEngine()
                    .GetFontSelector()
                    ->GetFontFaceCache()
                    ->GetNumSegmentedFacesForTesting());
  font_face = GetStyleEngine().GetFontSelector()->GetFontFaceCache()->Get(
      t4->GetComputedStyle()->GetFontDescription(), AtomicString("Cool Font"));
  EXPECT_TRUE(font_face);
  capabilities = font_face->GetFontSelectionCapabilities();
  ASSERT_EQ(capabilities.weight,
            FontSelectionRange({kNormalWeightValue, kNormalWeightValue}));
  ASSERT_EQ(capabilities.slope,
            FontSelectionRange({kItalicSlopeValue, kItalicSlopeValue}));

  // @keyframes rules

  Element* t5 = GetDocument().getElementById(AtomicString("t5"));
  ASSERT_TRUE(t5);

  // There's no @keyframes rule named dummy-animation
  ASSERT_FALSE(GetStyleEngine()
                   .GetStyleResolver()
                   .FindKeyframesRule(t5, t5, AtomicString("dummy-animation"))
                   .rule);

  auto* keyframes_parsed_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  keyframes_parsed_sheet->ParseString("@keyframes dummy-animation { from {} }");
  StyleSheetKey keyframes_key("keyframes");
  GetStyleEngine().InjectSheet(keyframes_key, keyframes_parsed_sheet,
                               WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();

  // After injecting the style sheet, a @keyframes rule named dummy-animation
  // is found with one keyframe.
  StyleRuleKeyframes* keyframes =
      GetStyleEngine()
          .GetStyleResolver()
          .FindKeyframesRule(t5, t5, AtomicString("dummy-animation"))
          .rule;
  ASSERT_TRUE(keyframes);
  EXPECT_EQ(1u, keyframes->Keyframes().size());

  style_element = MakeGarbageCollected<HTMLStyleElement>(GetDocument());
  style_element->setInnerHTML("@keyframes dummy-animation { from {} to {} }");
  GetDocument().body()->AppendChild(style_element);
  UpdateAllLifecyclePhases();

  // Author @keyframes rules take precedence; now there are two keyframes (from
  // and to).
  keyframes = GetStyleEngine()
                  .GetStyleResolver()
                  .FindKeyframesRule(t5, t5, AtomicString("dummy-animation"))
                  .rule;
  ASSERT_TRUE(keyframes);
  EXPECT_EQ(2u, keyframes->Keyframes().size());

  GetDocument().body()->RemoveChild(style_element);
  UpdateAllLifecyclePhases();

  keyframes = GetStyleEngine()
                  .GetStyleResolver()
                  .FindKeyframesRule(t5, t5, AtomicString("dummy-animation"))
                  .rule;
  ASSERT_TRUE(keyframes);
  EXPECT_EQ(1u, keyframes->Keyframes().size());

  GetStyleEngine().RemoveInjectedSheet(keyframes_key, WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();

  // Injected @keyframes rules are no longer available once removed.
  ASSERT_FALSE(GetStyleEngine()
                   .GetStyleResolver()
                   .FindKeyframesRule(t5, t5, AtomicString("dummy-animation"))
                   .rule);

  // Custom properties

  Element* t6 = GetDocument().getElementById(AtomicString("t6"));
  Element* t7 = GetDocument().getElementById(AtomicString("t7"));
  ASSERT_TRUE(t6);
  ASSERT_TRUE(t7);
  ASSERT_TRUE(t6->GetComputedStyle());
  ASSERT_TRUE(t7->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      t6->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(255, 255, 255),
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
                               WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t6->GetComputedStyle());
  ASSERT_TRUE(t7->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      t6->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(255, 255, 255),
      t7->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(custom_properties_key,
                                       WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t6->GetComputedStyle());
  ASSERT_TRUE(t7->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      t6->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(255, 255, 255),
      t7->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  // Media queries

  Element* t8 = GetDocument().getElementById(AtomicString("t8"));
  ASSERT_TRUE(t8);
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(255, 255, 255),
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
                               media_queries_parsed_sheet, WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      t8->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  gfx::SizeF page_size(400, 400);
  GetDocument().GetFrame()->StartPrinting(WebPrintParams(page_size));
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      t8->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetDocument().GetFrame()->EndPrinting();
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      t8->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(media_queries_sheet_key,
                                       WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t8->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(255, 255, 255),
      t8->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  // Author style sheets

  Element* t9 = GetDocument().getElementById(AtomicString("t9"));
  Element* t10 = GetDocument().getElementById(AtomicString("t10"));
  ASSERT_TRUE(t9);
  ASSERT_TRUE(t10);
  ASSERT_TRUE(t9->GetComputedStyle());
  ASSERT_TRUE(t10->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      t9->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      t10->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

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
                               WebCssOrigin::kAuthor);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t9->GetComputedStyle());
  ASSERT_TRUE(t10->GetComputedStyle());

  // Specificity works within author origin.
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      t9->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  // Important author rules do not override important inline author rules.
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      t10->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(author_sheet_key, WebCssOrigin::kAuthor);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t9->GetComputedStyle());
  ASSERT_TRUE(t10->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      t9->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      t10->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  // Style sheet removal

  Element* t11 = GetDocument().getElementById(AtomicString("t11"));
  ASSERT_TRUE(t11);
  ASSERT_TRUE(t11->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(255, 255, 255),
      t11->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  auto* parsed_removable_red_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  parsed_removable_red_sheet->ParseString("#t11 { color: red !important; }");
  StyleSheetKey removable_red_sheet_key("removable_red_sheet");
  GetStyleEngine().InjectSheet(removable_red_sheet_key,
                               parsed_removable_red_sheet, WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      t11->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  auto* parsed_removable_green_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  parsed_removable_green_sheet->ParseString(
      "#t11 { color: green !important; }");
  StyleSheetKey removable_green_sheet_key("removable_green_sheet");
  GetStyleEngine().InjectSheet(removable_green_sheet_key,
                               parsed_removable_green_sheet,
                               WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      t11->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  auto* parsed_removable_red_sheet2 = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  parsed_removable_red_sheet2->ParseString("#t11 { color: red !important; }");
  GetStyleEngine().InjectSheet(removable_red_sheet_key,
                               parsed_removable_red_sheet2,
                               WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      t11->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(removable_red_sheet_key,
                                       WebCssOrigin::kAuthor);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  // Removal works only within the same origin.
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      t11->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(removable_red_sheet_key,
                                       WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  // The last sheet with the given key is removed.
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      t11->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(removable_green_sheet_key,
                                       WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  // Only the last sheet with the given key is removed.
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      t11->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetStyleEngine().RemoveInjectedSheet(removable_red_sheet_key,
                                       WebCssOrigin::kUser);
  UpdateAllLifecyclePhases();
  ASSERT_TRUE(t11->GetComputedStyle());

  EXPECT_EQ(
      Color::FromRGB(255, 255, 255),
      t11->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, InjectedUserNoAuthorFontFace) {
  UpdateAllLifecyclePhases();

  FontDescription font_description;
  FontFaceCache* cache = GetStyleEngine().GetFontSelector()->GetFontFaceCache();
  EXPECT_FALSE(cache->Get(font_description, AtomicString("User")));

  auto* user_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  user_sheet->ParseString(
      "@font-face {"
      "  font-family: 'User';"
      "  src: url(font.ttf);"
      "}");

  StyleSheetKey user_key("user");
  GetStyleEngine().InjectSheet(user_key, user_sheet, WebCssOrigin::kUser);

  UpdateAllLifecyclePhases();

  EXPECT_TRUE(cache->Get(font_description, AtomicString("User")));
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
  EXPECT_TRUE(cache->Get(font_description, AtomicString("Author")));
  EXPECT_FALSE(cache->Get(font_description, AtomicString("User")));

  auto* user_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  user_sheet->ParseString(
      "@font-face {"
      "  font-family: 'User';"
      "  src: url(author);"
      "}");

  StyleSheetKey user_key("user");
  GetStyleEngine().InjectSheet(user_key, user_sheet, WebCssOrigin::kUser);

  UpdateAllLifecyclePhases();

  EXPECT_TRUE(cache->Get(font_description, AtomicString("Author")));
  EXPECT_TRUE(cache->Get(font_description, AtomicString("User")));
}

TEST_F(StyleEngineTest, IgnoreInvalidPropertyValue) {
  GetDocument().body()->setInnerHTML(
      "<section><div id='t1'>Red</div></section>"
      "<style id='s1'>div { color: red; } section div#t1 { color:rgb(0");
  UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById(AtomicString("t1"));
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      t1->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, TextToSheetCache) {
  auto* element = MakeGarbageCollected<HTMLStyleElement>(GetDocument());

  String sheet_text("div {}");
  TextPosition min_pos = TextPosition::MinimumPosition();

  CSSStyleSheet* sheet1 = GetStyleEngine().CreateSheet(
      *element, sheet_text, min_pos, PendingSheetType::kNonBlocking,
      RenderBlockingBehavior::kNonBlocking);

  // Check that the first sheet is not using a cached StyleSheetContents.
  EXPECT_FALSE(sheet1->Contents()->IsUsedFromTextCache());

  CSSStyleSheet* sheet2 = GetStyleEngine().CreateSheet(
      *element, sheet_text, min_pos, PendingSheetType::kNonBlocking,
      RenderBlockingBehavior::kNonBlocking);

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

  element = MakeGarbageCollected<HTMLStyleElement>(GetDocument());
  sheet1 = GetStyleEngine().CreateSheet(*element, sheet_text, min_pos,
                                        PendingSheetType::kNonBlocking,
                                        RenderBlockingBehavior::kNonBlocking);

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
  ApplyRuleSetInvalidation(GetDocument(), "span { background: green}");
  UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  before_count = after_count;
  ApplyRuleSetInvalidation(GetDocument(), "body div { background: green}");
  UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(2u, after_count - before_count);

  before_count = after_count;
  ApplyRuleSetInvalidation(GetDocument(), "div * { background: green}");
  UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(2u, after_count - before_count);

  before_count = GetStyleEngine().StyleForElementCount();
  ApplyRuleSetInvalidation(GetDocument(), "#i b { background: green}");
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
  ApplyRuleSetInvalidation(GetDocument(),
                           "::-webkit-progress-bar { background: green }");
  UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);
}

TEST_F(StyleEngineTest, RuleSetInvalidationHost) {
  GetDocument().body()->setInnerHTML(
      "<div id=nohost></div><div id=host></div>");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);

  shadow_root.setInnerHTML("<div></div><div></div><div></div>");
  UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  ApplyRuleSetInvalidation(shadow_root,
                           ":host(#nohost), #nohost { background: green}");
  UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(0u, after_count - before_count);

  before_count = after_count;
  ApplyRuleSetInvalidation(shadow_root, ":host(#host) { background: green}");
  UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  before_count = after_count;
  ApplyRuleSetInvalidation(shadow_root, ":host(div) { background: green}");
  UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  before_count = after_count;
  ApplyRuleSetInvalidation(shadow_root, ":host(*) { background: green}");
  UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  before_count = after_count;
  ApplyRuleSetInvalidation(shadow_root, ":host(*) :hover { background: green}");
  UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(3u, after_count - before_count);
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

  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);

  shadow_root.setInnerHTML("<slot name=other></slot><slot></slot>");
  UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  ApplyRuleSetInvalidation(shadow_root, "::slotted(.s1) { background: green}");
  UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(4u, after_count - before_count);

  before_count = GetStyleEngine().StyleForElementCount();
  ApplyRuleSetInvalidation(shadow_root, "::slotted(*) { background: green}");
  UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(4u, after_count - before_count);
}

TEST_F(StyleEngineTest, RuleSetInvalidationHostContext) {
  GetDocument().body()->setInnerHTML(
      "<div class=match><div id=host></div></div>");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);

  shadow_root.setInnerHTML("<div></div><div class=a></div><div></div>");
  UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  ApplyRuleSetInvalidation(shadow_root,
                           ":host-context(.nomatch) .a { background: green}");
  UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(0u, after_count - before_count);

  before_count = after_count;
  ApplyRuleSetInvalidation(shadow_root,
                           ":host-context(.match) .a { background: green}");
  UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  before_count = after_count;
  ApplyRuleSetInvalidation(shadow_root,
                           ":host-context(:hover) { background: green}");
  UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  before_count = after_count;
  ApplyRuleSetInvalidation(shadow_root,
                           ":host-context(#host) { background: green}");
  UpdateAllLifecyclePhases();
  after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);
}

TEST_F(StyleEngineTest, HasViewportDependentMediaQueries) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>div {}</style>
    <style id='sheet' media='(min-width: 200px)'>
      div {}
    </style>
  )HTML");

  Element* style_element = GetDocument().getElementById(AtomicString("sheet"));

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

  Element* t1 = GetDocument().getElementById(AtomicString("t1"));
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      t1->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  unsigned before_count = GetStyleEngine().StyleForElementCount();

  Element* s1 = GetDocument().getElementById(AtomicString("s1"));
  s1->setAttribute(blink::html_names::kMediaAttr,
                   AtomicString("(max-width: 2000px)"));
  UpdateAllLifecyclePhases();

  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(1u, after_count - before_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      t1->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, StyleMediaAttributeNoStyleChange) {
  GetDocument().body()->setInnerHTML(
      "<style id='s1' media='(max-width: 1000px)'>#t1 { color: green }</style>"
      "<div id='t1'>Green</div><div></div>");
  UpdateAllLifecyclePhases();

  Element* t1 = GetDocument().getElementById(AtomicString("t1"));
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      t1->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  unsigned before_count = GetStyleEngine().StyleForElementCount();

  Element* s1 = GetDocument().getElementById(AtomicString("s1"));
  s1->setAttribute(blink::html_names::kMediaAttr,
                   AtomicString("(max-width: 2000px)"));
  UpdateAllLifecyclePhases();

  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(0u, after_count - before_count);

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      t1->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
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

  Element* t1 = GetDocument().getElementById(AtomicString("t1"));
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(0, 0, 255),
      t1->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

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
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      t1->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  style_rule->style()->setProperty(GetDocument().GetExecutionContext(), "color",
                                   "green", "", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhases();

  ASSERT_TRUE(t1->GetComputedStyle());
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      t1->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, VisitedExplicitInheritanceMatchedPropertiesCache) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      :visited { overflow: inherit }
    </style>
    <span id="span"><a href></a></span>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* span = GetDocument().getElementById(AtomicString("span"));
  const ComputedStyle* style = span->GetComputedStyle();
  EXPECT_FALSE(style->ChildHasExplicitInheritance());

  style = span->firstElementChild()->GetComputedStyle();

  ComputedStyleBuilder builder(*style);
  EXPECT_TRUE(MatchedPropertiesCache::IsStyleCacheable(builder));

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

  Element* t1 = GetDocument().getElementById(AtomicString("t1"));
  Element* t2 = GetDocument().getElementById(AtomicString("t2"));
  ASSERT_TRUE(t1);
  ASSERT_TRUE(t2);

  UpdateAllLifecyclePhases();

  // PlatformColorsChanged() triggers SubtreeStyleChange on document(). If that
  // for some reason should change, this test will start failing and the
  // SubtreeStyleChange must be set another way.
  // Calling setNeedsStyleRecalc() explicitly with an arbitrary reason instead
  // requires us to CORE_EXPORT the reason strings.
  GetStyleEngine().PlatformColorsChanged();

  // Check that no invalidations sets are scheduled when the document node is
  // already SubtreeStyleChange.
  t2->setAttribute(blink::html_names::kClassAttr, AtomicString("t2"));
  EXPECT_FALSE(GetDocument().NeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
  UpdateAllLifecyclePhases();  // Mark everything as clean.

  // Toggling the s2 style sheet should normally touch t1 and t2...
  auto* s2 =
      To<HTMLStyleElement>(GetDocument().getElementById(AtomicString("s2")));
  ASSERT_TRUE(s2);
  s2->setDisabled(true);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_TRUE(GetDocument().documentElement()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(t1->NeedsStyleRecalc());
  EXPECT_TRUE(t2->NeedsStyleRecalc());
  UpdateAllLifecyclePhases();  // Mark everything as clean.

  // ...but if the root is marked as kSubtreeRecalc, it should not visit them,
  // and thus not mark them for recalc.
  GetStyleEngine().PlatformColorsChanged();
  s2->setDisabled(false);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().documentElement()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(t1->NeedsStyleRecalc());
  EXPECT_FALSE(t2->NeedsStyleRecalc());
  UpdateAllLifecyclePhases();  // Mark everything as clean.

  // Toggling the s1 stylesheet shouldn't touch either, since it matches
  // nothing.
  auto* s1 =
      To<HTMLStyleElement>(GetDocument().getElementById(AtomicString("s1")));
  ASSERT_TRUE(s1);
  s1->setDisabled(true);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().documentElement()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(t1->NeedsStyleRecalc());
  EXPECT_FALSE(t2->NeedsStyleRecalc());
  UpdateAllLifecyclePhases();  // Mark everything as clean.

  // And thus, kSubtreeRecalc on the root shouldn't make any difference.
  GetStyleEngine().PlatformColorsChanged();
  s1->setDisabled(false);
  GetStyleEngine().UpdateActiveStyle();
  EXPECT_FALSE(GetDocument().documentElement()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(t1->NeedsStyleRecalc());
  EXPECT_FALSE(t2->NeedsStyleRecalc());
}

TEST_F(StyleEngineTest, EmptyHttpEquivDefaultStyle) {
  GetDocument().body()->setInnerHTML(
      "<style>div { color:pink }</style><div id=container></div>");
  UpdateAllLifecyclePhases();

  EXPECT_FALSE(GetStyleEngine().NeedsActiveStyleUpdate());

  Element* container = GetDocument().getElementById(AtomicString("container"));
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
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);

  UpdateAllLifecyclePhases();
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);

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

TEST_F(StyleEngineTest, ViewportDescription) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.Initialize();
  web_view_impl->MainFrameWidget()->SetDeviceScaleFactorForTesting(1.f);
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
  web_view_impl->MainFrameWidget()->SetDeviceScaleFactorForTesting(
      device_scale);
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

  Element* t1 = GetDocument().getElementById(AtomicString("t1"));
  t1->firstChild()->remove();
  EXPECT_TRUE(t1->NeedsStyleInvalidation());

  Element* t2 = GetDocument().getElementById(AtomicString("t2"));
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

  Element* t1 = GetDocument().getElementById(AtomicString("t1"));
  t1->firstChild()->remove();
  EXPECT_FALSE(t1->NeedsStyleInvalidation());

  Element* t2 = GetDocument().getElementById(AtomicString("t2"));
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

  Element* t1 = GetDocument().getElementById(AtomicString("t1"));
  t1->appendChild(Text::Create(GetDocument(), "Text"));
  EXPECT_TRUE(t1->NeedsStyleInvalidation());

  Element* t2 = GetDocument().getElementById(AtomicString("t2"));
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

  Element* t1 = GetDocument().getElementById(AtomicString("t1"));
  t1->appendChild(Text::Create(GetDocument(), "Text"));
  EXPECT_FALSE(t1->NeedsStyleInvalidation());

  Element* t2 = GetDocument().getElementById(AtomicString("t2"));
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

  Element* t1 = GetDocument().getElementById(AtomicString("t1"));
  Element* t2 = GetDocument().getElementById(AtomicString("t2"));
  Element* t3 = GetDocument().getElementById(AtomicString("t3"));

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

  Element* t1 = GetDocument().getElementById(AtomicString("t1"));
  Element* t2 = GetDocument().getElementById(AtomicString("t2"));
  Element* t3 = GetDocument().getElementById(AtomicString("t3"));

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
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetSettings()->SetDefaultFontSize(40);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangeColorScheme) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);

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
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangeColorSchemeForcedDarkMode) {
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @media (prefers-color-scheme: dark) {
        body { color: green }
      }
      @media (prefers-color-scheme: light) {
        body { color: red }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangePrefersContrast) {
  ScopedForcedColorsForTest forced_scoped_feature(true);

  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredContrast(
      mojom::blink::PreferredContrast::kNoPreference);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { color: red; forced-color-adjust: none; }
      @media (prefers-contrast: no-preference) {
        body { color: green }
      }
      @media (prefers-contrast) {
        body { color: blue }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  color_scheme_helper.SetPreferredContrast(
      mojom::blink::PreferredContrast::kMore);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 0, 255),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  color_scheme_helper.SetPreferredContrast(
      mojom::blink::PreferredContrast::kLess);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 0, 255),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  color_scheme_helper.SetPreferredContrast(
      mojom::blink::PreferredContrast::kCustom);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 0, 255),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangeSpecificPrefersContrast) {
  ScopedForcedColorsForTest forced_scoped_feature(true);

  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredContrast(
      mojom::blink::PreferredContrast::kNoPreference);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { color: red; forced-color-adjust: none; }
      @media (prefers-contrast: more) {
        body { color: blue }
      }
      @media (prefers-contrast: less) {
        body { color: orange }
      }
      @media (prefers-contrast: custom) {
        body { color: yellow }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  color_scheme_helper.SetPreferredContrast(
      mojom::blink::PreferredContrast::kMore);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 0, 255),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  color_scheme_helper.SetPreferredContrast(
      mojom::blink::PreferredContrast::kLess);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 165, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  color_scheme_helper.SetPreferredContrast(
      mojom::blink::PreferredContrast::kCustom);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 255, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangePrefersContrastOverride) {
  ScopedForcedColorsForTest forced_scoped_feature(true);

  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredContrast(
      mojom::blink::PreferredContrast::kNoPreference);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { color: red; forced-color-adjust: none; }
      @media (prefers-contrast: more) {
        body { color: blue }
      }
      @media (prefers-contrast: less) {
        body { color: orange }
      }
      @media (prefers-contrast: custom) {
        body { color: yellow }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersContrastMediaFeature, "more");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 0, 255),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersContrastMediaFeature, "no-preference");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersContrastMediaFeature, "less");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 165, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersContrastMediaFeature, "custom");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 255, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearMediaFeatureOverrides();

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersContrastMediaFeature, "more");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 0, 255),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersContrastMediaFeature, "no-preference");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersContrastMediaFeature, "less");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 165, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearPreferenceOverrides();

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersContrastMediaFeature, "less");

  UpdateAllLifecyclePhases();

  // Invalid value resets override
  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersContrastMediaFeature, "invalid");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersContrastMediaFeature, "less");

  UpdateAllLifecyclePhases();

  // Empty value resets override
  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersContrastMediaFeature, "");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
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
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetSettings()->SetPrefersReducedMotion(true);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangePrefersReducedTransparency) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { color: red }
      @media (prefers-reduced-transparency: reduce) {
        body { color: green }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetSettings()->SetPrefersReducedTransparency(true);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
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
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
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
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
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
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetInForcedColors(GetDocument(),
                                        /*in_forced_colors=*/true);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
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

  // InForcedColors = false, PreferredColorScheme = kLight
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetInForcedColors(GetDocument(),
                                        /*in_forced_colors=*/false);
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  // ForcedColors = kNone, PreferredColorScheme = kDark
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  // InForcedColors = true, PreferredColorScheme = kDark
  color_scheme_helper.SetInForcedColors(GetDocument(),
                                        /*in_forced_colors=*/true);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 165, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  // InForcedColors = true, PreferredColorScheme = kLight
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 0, 255),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesForcedColorsOverride) {
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
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  ColorSchemeHelper color_scheme_helper(GetDocument());
  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kForcedColorsMediaFeature, "active");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kForcedColorsMediaFeature, "none");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearMediaFeatureOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesColorSchemeOverride) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);
  EXPECT_EQ(mojom::blink::PreferredColorScheme::kLight,
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
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersColorSchemeMediaFeature, "dark");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearMediaFeatureOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersColorSchemeMediaFeature, "dark");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearPreferenceOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersColorSchemeMediaFeature, "dark");
  UpdateAllLifecyclePhases();

  // Invalid value resets override
  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersColorSchemeMediaFeature, "invalid");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersColorSchemeMediaFeature, "dark");

  UpdateAllLifecyclePhases();

  // Empty value resets override
  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersColorSchemeMediaFeature, "");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesReducedTransparencyOverride) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { color: red }
      @media (prefers-reduced-transparency: reduce) {
        body { color: green }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature, "reduce");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearMediaFeatureOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature, "reduce");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearPreferenceOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature, "reduce");

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature,
      "no-preference");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearMediaFeatureOverrides();
  GetDocument().GetPage()->ClearPreferenceOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature, "reduce");
  UpdateAllLifecyclePhases();

  // Invalid value resets override
  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature, "invalid");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature, "reduce");

  UpdateAllLifecyclePhases();

  // Empty value resets override
  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedTransparencyMediaFeature, "");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesReducedDataOverride) {
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
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersReducedDataMediaFeature, "reduce");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearMediaFeatureOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedDataMediaFeature, "reduce");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearPreferenceOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersReducedDataMediaFeature, "reduce");

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedDataMediaFeature, "no-preference");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearMediaFeatureOverrides();
  GetDocument().GetPage()->ClearPreferenceOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedDataMediaFeature, "reduce");
  UpdateAllLifecyclePhases();

  // Invalid value resets override
  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedDataMediaFeature, "invalid");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedDataMediaFeature, "reduce");

  UpdateAllLifecyclePhases();

  // Empty value resets override
  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedDataMediaFeature, "");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, PreferredColorSchemeMetric) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);
  EXPECT_FALSE(IsUseCounted(WebFeature::kPreferredColorSchemeDark));
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  EXPECT_TRUE(IsUseCounted(WebFeature::kPreferredColorSchemeDark));
}

// The preferred color scheme setting used to differ from the preferred color
// scheme when forced dark mode was enabled. Test that it is no longer the case.
TEST_F(StyleEngineTest, PreferredColorSchemeSettingMetric) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);
  GetDocument().GetSettings()->SetForceDarkModeEnabled(false);
  EXPECT_FALSE(IsUseCounted(WebFeature::kPreferredColorSchemeDark));
  EXPECT_FALSE(IsUseCounted(WebFeature::kPreferredColorSchemeDarkSetting));

  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  // Clear the UseCounters before they are updated by the
  // |SetForceDarkModeEnabled| call, below.
  ClearUseCounter(WebFeature::kPreferredColorSchemeDark);
  ClearUseCounter(WebFeature::kPreferredColorSchemeDarkSetting);
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);

  EXPECT_TRUE(IsUseCounted(WebFeature::kPreferredColorSchemeDark));
  EXPECT_TRUE(IsUseCounted(WebFeature::kPreferredColorSchemeDarkSetting));
}

TEST_F(StyleEngineTest, ForcedDarkModeMetric) {
  GetDocument().GetSettings()->SetForceDarkModeEnabled(false);
  EXPECT_FALSE(IsUseCounted(WebFeature::kForcedDarkMode));
  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
  EXPECT_TRUE(IsUseCounted(WebFeature::kForcedDarkMode));
}

TEST_F(StyleEngineTest, ColorSchemeDarkSupportedOnRootMetricFromMetaDark) {
  EXPECT_FALSE(IsUseCounted(WebFeature::kColorSchemeDarkSupportedOnRoot));
  GetDocument().body()->setInnerHTML(R"HTML(
    <meta name="color-scheme" content="dark">
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsUseCounted(WebFeature::kColorSchemeDarkSupportedOnRoot));
}

TEST_F(StyleEngineTest, ColorSchemeDarkSupportedOnRootMetricFromMetaLightDark) {
  EXPECT_FALSE(IsUseCounted(WebFeature::kColorSchemeDarkSupportedOnRoot));
  GetDocument().body()->setInnerHTML(R"HTML(
    <meta name="color-scheme" content="light dark">
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsUseCounted(WebFeature::kColorSchemeDarkSupportedOnRoot));
}

TEST_F(StyleEngineTest, ColorSchemeDarkSupportedOnRootMetricFromCSSDark) {
  EXPECT_FALSE(IsUseCounted(WebFeature::kColorSchemeDarkSupportedOnRoot));
  GetDocument().body()->setInnerHTML(R"HTML(
    <style> :root { color-scheme: dark; } </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsUseCounted(WebFeature::kColorSchemeDarkSupportedOnRoot));
}

TEST_F(StyleEngineTest, ColorSchemeDarkSupportedOnRootMetricFromCSSLightDark) {
  EXPECT_FALSE(IsUseCounted(WebFeature::kColorSchemeDarkSupportedOnRoot));
  GetDocument().body()->setInnerHTML(R"HTML(
    <style> :root { color-scheme: light dark; } </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsUseCounted(WebFeature::kColorSchemeDarkSupportedOnRoot));
}

TEST_F(StyleEngineTest, ColorSchemeDarkSupportedOnRootMetricFromChildCSSDark) {
  EXPECT_FALSE(IsUseCounted(WebFeature::kColorSchemeDarkSupportedOnRoot));
  GetDocument().body()->setInnerHTML(R"HTML(
    <style> div { color-scheme: dark; } </style>
    <div></div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(IsUseCounted(WebFeature::kColorSchemeDarkSupportedOnRoot));
}

TEST_F(StyleEngineTest, ColorSchemeDarkSupportedOnRootMetricFromLight) {
  EXPECT_FALSE(IsUseCounted(WebFeature::kColorSchemeDarkSupportedOnRoot));
  GetDocument().body()->setInnerHTML(R"HTML(
    <meta name="color-scheme" content="light">
    <style> :root { color-scheme: light; } </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(IsUseCounted(WebFeature::kColorSchemeDarkSupportedOnRoot));
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
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersReducedMotionMediaFeature, "reduce");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearMediaFeatureOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedMotionMediaFeature, "reduce");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearPreferenceOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
  GetDocument().GetPage()->ClearPreferenceOverrides();

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersReducedMotionMediaFeature, "reduce");

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedMotionMediaFeature, "no-preference");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->ClearMediaFeatureOverrides();
  GetDocument().GetPage()->ClearPreferenceOverrides();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedMotionMediaFeature, "reduce");
  UpdateAllLifecyclePhases();

  // Invalid value resets override
  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedMotionMediaFeature, "invalid");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedMotionMediaFeature, "reduce");

  UpdateAllLifecyclePhases();

  // Empty value resets override
  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersReducedMotionMediaFeature, "");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
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
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetSettings()->SetNavigationControls(
      NavigationControls::kBackButton);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, MediaQueriesChangeInvertedColors) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { color: red }
      @media (inverted-colors: inverted) {
        body { color: green }
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  GetDocument().GetSettings()->SetInvertedColors(true);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, ShadowRootStyleRecalcCrash) {
  GetDocument().body()->setInnerHTML("<div id=host></div>");
  auto* host =
      To<HTMLElement>(GetDocument().getElementById(AtomicString("host")));
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);

  shadow_root.setInnerHTML(R"HTML(
    <span id=span></span>
    <style>
      :nth-child(odd) { color: green }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();

  // This should not cause DCHECK errors on style recalc flags.
  shadow_root.getElementById(AtomicString("span"))->remove();
  host->SetInlineStyleProperty(CSSPropertyID::kDisplay, "inline");
  UpdateAllLifecyclePhases();
}

TEST_F(StyleEngineTest, GetComputedStyleOutsideFlatTreeCrash) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body, div { display: contents }
      div::before { display: contents; content: "" }
    </style>
    <div id=host>
      <!-- no slots here -->
    </host>
    <div id=non-slotted></div>
  )HTML");

  GetDocument()
      .getElementById(AtomicString("host"))
      ->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  UpdateAllLifecyclePhases();
  GetDocument().body()->EnsureComputedStyle();
  GetDocument()
      .getElementById(AtomicString("non-slotted"))
      ->SetInlineStyleProperty(CSSPropertyID::kColor, "blue");
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
  // Even if the Stats() were already enabled, the following resets it to 0.
  engine.SetStatsEnabled(true);

  StyleResolverStats* stats = engine.Stats();
  ASSERT_TRUE(stats);
  EXPECT_EQ(0u, stats->rules_fast_rejected);

  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  ASSERT_TRUE(div);
  div->SetInlineStyleProperty(CSSPropertyID::kColor, "green");

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetStyleEngine().RecalcStyle();

  // Should fast reject ".not-in-filter div::before {}" for both the div and its
  // ::before pseudo element.
  EXPECT_EQ(2u, stats->rules_fast_rejected);
}

TEST_F(StyleEngineTest, FirstLetterRemoved) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>.fl::first-letter { color: pink }</style>
    <div class=fl id=d1><div><span id=f1>A</span></div></div>
    <div class=fl id=d2><div><span id=f2>BB</span></div></div>
    <div class=fl id=d3><div><span id=f3>C<!---->C</span></div></div>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* d1 = GetDocument().getElementById(AtomicString("d1"));
  Element* d2 = GetDocument().getElementById(AtomicString("d2"));
  Element* d3 = GetDocument().getElementById(AtomicString("d3"));

  FirstLetterPseudoElement* fl1 =
      To<FirstLetterPseudoElement>(d1->GetPseudoElement(kPseudoIdFirstLetter));
  EXPECT_TRUE(fl1);

  GetDocument().getElementById(AtomicString("f1"))->firstChild()->remove();

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

  GetDocument().getElementById(AtomicString("f2"))->firstChild()->remove();

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

  Element* f3 = GetDocument().getElementById(AtomicString("f3"));
  f3->firstChild()->remove();

  EXPECT_TRUE(d3->firstChild()->ChildNeedsStyleRecalc());
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
  auto* data1 = GetStyleEngine().MaybeCreateAndGetInitialData();
  EXPECT_TRUE(data1);

  // After a full recalc, we should have the same initial data.
  GetDocument().body()->setInnerHTML("<style>* { font-size: 1px; } </style>");
  EXPECT_TRUE(GetDocument().documentElement()->NeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().documentElement()->ChildNeedsStyleRecalc());
  UpdateAllLifecyclePhases();
  auto* data2 = GetStyleEngine().MaybeCreateAndGetInitialData();
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
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSSelectorEmptyWhitespaceOnlyFail));

  auto* div_elements = GetDocument().getElementsByTagName(AtomicString("div"));
  ASSERT_TRUE(div_elements);
  ASSERT_EQ(5u, div_elements->length());

  auto is_counted = [](Element* element) {
    element->setAttribute(blink::html_names::kClassAttr, AtomicString("match"));
    element->GetDocument().View()->UpdateAllLifecyclePhasesForTest();
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

  Element* computed = GetDocument().getElementById(AtomicString("computed"));
  Element* span_outer = GetDocument().getElementById(AtomicString("span"));
  Element* span_inner = span_outer->firstElementChild();

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
  Element* div = GetDocument().getElementById(AtomicString("div"));
  Element* progress = GetDocument().getElementById(AtomicString("progress"));
  ASSERT_TRUE(div);
  ASSERT_TRUE(progress);

  // This causes ProgressShadowElements to get ComputedStyles with
  // IsEnsuredInDisplayNone==true.
  for (Node* node = progress; node;
       node = FlatTreeTraversal::Next(*node, progress)) {
    if (Element* element = DynamicTo<Element>(node)) {
      element->EnsureComputedStyle();
    }
  }

  // This triggers layout tree building.
  div->SetInlineStyleProperty(CSSPropertyID::kDisplay, "inline");
  UpdateAllLifecyclePhases();

  // We must not create LayoutObjects for Nodes with
  // IsEnsuredInDisplayNone==true
  for (Node* node = progress; node;
       node = FlatTreeTraversal::Next(*node, progress)) {
    if (auto* element = DynamicTo<Element>(node)) {
      ASSERT_TRUE(!element->GetComputedStyle() ||
                  !element->ComputedStyleRef().IsEnsuredInDisplayNone() ||
                  !element->GetLayoutObject());
    }
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

  Element* form = GetDocument().getElementById(AtomicString("form"));
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  Element* inner = GetDocument().getElementById(AtomicString("inner"));
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
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  UpdateAllLifecyclePhases();

  EXPECT_EQ(Color::kWhite, GetDocument().View()->BaseBackgroundColor());

  GetDocument().documentElement()->SetInlineStyleProperty(
      CSSPropertyID::kColorScheme, "dark");
  UpdateAllLifecyclePhases();

  EXPECT_EQ(Color(0x12, 0x12, 0x12),
            GetDocument().View()->BaseBackgroundColor());

  color_scheme_helper.SetInForcedColors(GetDocument(),
                                        /*in_forced_colors=*/true);
  UpdateAllLifecyclePhases();
  mojom::blink::ColorScheme color_scheme = mojom::blink::ColorScheme::kLight;
  Color system_background_color = LayoutTheme::GetTheme().SystemColor(
      CSSValueID::kCanvas, color_scheme,
      GetDocument().GetColorProviderForPainting(color_scheme),
      GetDocument().IsInWebAppScope());

  EXPECT_EQ(system_background_color,
            GetDocument().View()->BaseBackgroundColor());
}

TEST_F(StyleEngineTest, ColorSchemeOverride) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);

  GetDocument().documentElement()->SetInlineStyleProperty(
      CSSPropertyID::kColorScheme, "light dark");
  UpdateAllLifecyclePhases();

  EXPECT_EQ(
      mojom::blink::ColorScheme::kLight,
      GetDocument().documentElement()->GetComputedStyle()->UsedColorScheme());

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersColorSchemeMediaFeature, "dark");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(
      mojom::blink::ColorScheme::kDark,
      GetDocument().documentElement()->GetComputedStyle()->UsedColorScheme());

  GetDocument().GetPage()->ClearMediaFeatureOverrides();
  UpdateAllLifecyclePhases();

  EXPECT_EQ(
      mojom::blink::ColorScheme::kLight,
      GetDocument().documentElement()->GetComputedStyle()->UsedColorScheme());

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersColorSchemeMediaFeature, "dark");

  UpdateAllLifecyclePhases();
  EXPECT_EQ(
      mojom::blink::ColorScheme::kDark,
      GetDocument().documentElement()->GetComputedStyle()->UsedColorScheme());

  GetDocument().GetPage()->ClearPreferenceOverrides();
  UpdateAllLifecyclePhases();

  EXPECT_EQ(
      mojom::blink::ColorScheme::kLight,
      GetDocument().documentElement()->GetComputedStyle()->UsedColorScheme());

  GetDocument().GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersColorSchemeMediaFeature, "dark");

  GetDocument().GetPage()->SetPreferenceOverride(
      media_feature_names::kPrefersColorSchemeMediaFeature, "light");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(
      mojom::blink::ColorScheme::kDark,
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

  auto* anim_element = GetDocument().getElementById(AtomicString("anim"));
  auto* before = anim_element->GetPseudoElement(kPseudoIdBefore);
  auto* animations = before->GetElementAnimations();

  ASSERT_TRUE(animations);

  before->SetNeedsAnimationStyleRecalc();
  UpdateAllLifecyclePhases();

  ASSERT_TRUE(before->GetComputedStyle());
  const ComputedStyle* base_computed_style =
      before->GetComputedStyle()->GetBaseComputedStyle();
  EXPECT_TRUE(base_computed_style);

  before->SetNeedsAnimationStyleRecalc();
  UpdateAllLifecyclePhases();

  ASSERT_TRUE(before->GetComputedStyle());
  EXPECT_TRUE(before->GetComputedStyle()->GetBaseComputedStyle());
#if !DCHECK_IS_ON()
  // When DCHECK is enabled, ShouldComputeBaseComputedStyle always returns true
  // and we repeatedly create new instances which means the pointers will be
  // different here.
  EXPECT_EQ(base_computed_style,
            before->GetComputedStyle()->GetBaseComputedStyle());
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

  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  Element* inner = GetDocument().getElementById(AtomicString("inner"));

  outer->SetForceReattachLayoutTree();
  inner->SetInlineStyleProperty(CSSPropertyID::kColor, "blue");

  EXPECT_EQ(outer, GetStyleRecalcRoot());
}

TEST_F(StyleEngineTest, ForceReattachNoStyleForElement) {
  GetDocument().body()->setInnerHTML(R"HTML(<div id="reattach"></div>)HTML");

  auto* reattach = GetDocument().getElementById(AtomicString("reattach"));

  UpdateAllLifecyclePhases();

  unsigned initial_count = GetStyleEngine().StyleForElementCount();

  reattach->SetForceReattachLayoutTree();
  EXPECT_EQ(reattach, GetStyleRecalcRoot());

  UpdateAllLifecyclePhases();
  EXPECT_EQ(GetStyleEngine().StyleForElementCount(), initial_count);
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

  auto* host = GetDocument().getElementById(AtomicString("host"));
  auto* outer = GetDocument().getElementById(AtomicString("outer"));
  auto* inner = GetDocument().getElementById(AtomicString("inner"));
  auto* innermost = GetDocument().getElementById(AtomicString("innermost"));

  host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  UpdateAllLifecyclePhases();

  EXPECT_TRUE(host->GetComputedStyle());
  // ComputedStyle is not generated outside the flat tree.
  EXPECT_FALSE(outer->GetComputedStyle());
  EXPECT_FALSE(inner->GetComputedStyle());
  EXPECT_FALSE(innermost->GetComputedStyle());

  inner->EnsureComputedStyle();
  const ComputedStyle* outer_style = outer->GetComputedStyle();
  const ComputedStyle* inner_style = inner->GetComputedStyle();

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
    <div id="parent">
      <div id="host1"><span style="display:contents"></span></div>
      <div id="host2"></div>
    </div>
  )HTML");

  auto* host1 = GetDocument().getElementById(AtomicString("host1"));
  auto* host2 = GetDocument().getElementById(AtomicString("host2"));
  auto* span = host1->firstChild();

  ShadowRoot& shadow_root =
      host1->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<slot></slot>");
  host2->AttachShadowRootForTesting(ShadowRootMode::kOpen);

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
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
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

  auto* host = GetDocument().getElementById(AtomicString("host"));
  auto* dirty = GetDocument().getElementById(AtomicString("dirty"));
  auto* ensured = GetDocument().getElementById(AtomicString("ensured"));
  auto* span = To<Element>(ensured->firstChild());

  host->AttachShadowRootForTesting(ShadowRootMode::kOpen);

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
    <div id=host><span style="display:contents"></span></div>
  )HTML");

  auto* host = GetDocument().getElementById(AtomicString("host"));
  auto* span = To<Element>(host->firstChild());

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<div><slot></slot></div>");

  UpdateAllLifecyclePhases();

  // Make the span style dirty.
  span->setAttribute(html_names::kStyleAttr, AtomicString("color:green"));

  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_EQ(span, GetStyleRecalcRoot());

  auto* div = shadow_root.firstChild();
  auto* slot = To<Element>(div->firstChild());

  slot->setAttribute(html_names::kNameAttr, AtomicString("x"));
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

  auto* host = GetDocument().getElementById(AtomicString("host"));
  auto* span = To<Element>(host->firstChild());

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(R"HTML(
    <div><slot name="default"></slot></div>
  )HTML");

  UpdateAllLifecyclePhases();

  // Ensure style outside the flat tree.
  const ComputedStyle* style = span->EnsureComputedStyle();
  ASSERT_TRUE(style);
  EXPECT_TRUE(style->IsEnsuredOutsideFlatTree());

  span->setAttribute(html_names::kSlotAttr, AtomicString("default"));
  GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
  EXPECT_EQ(span, GetStyleRecalcRoot());
  EXPECT_FALSE(span->GetComputedStyle());
}

TEST_F(StyleEngineTest, ForceReattachRecalcRootAttachShadow) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="reattach"></div>
    <div id="host"><span style="display:contents"></span></div>
  )HTML");

  auto* reattach = GetDocument().getElementById(AtomicString("reattach"));
  auto* host = GetDocument().getElementById(AtomicString("host"));

  UpdateAllLifecyclePhases();

  reattach->SetForceReattachLayoutTree();
  EXPECT_FALSE(reattach->NeedsStyleRecalc());
  EXPECT_EQ(reattach, GetStyleRecalcRoot());

  // Attaching the shadow root will call FlatTreePositionChanged() on the span
  // child of the host. The style recalc root should still be #reattach.
  host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  EXPECT_EQ(reattach, GetStyleRecalcRoot());
}

TEST_F(StyleEngineTest, InitialColorChange) {
  // Set color scheme to light.
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      :root { color-scheme: light dark }
      #initial { color: initial }
    </style>
    <div id="initial"></div>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* initial = GetDocument().getElementById(AtomicString("initial"));
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
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
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

  Element* div = GetDocument().getElementById(AtomicString("green"));
  EXPECT_EQ(Color::kBlack, div->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  unsigned initial_count = GetStyleEngine().StyleForElementCount();

  GetDocument().View()->SetLayoutSizeFixedToFrameSize(false);
  GetDocument().View()->SetLayoutSize(gfx::Size(1100, 800));
  UpdateAllLifecyclePhases();

  // Only the single div element should have its style recomputed.
  EXPECT_EQ(1u, GetStyleEngine().StyleForElementCount() - initial_count);
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      div->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
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

  Element* div = GetDocument().getElementById(AtomicString("green"));
  EXPECT_EQ(Color::kBlack, div->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  unsigned initial_count = GetStyleEngine().StyleForElementCount();

  GetDocument().GetSettings()->SetMediaTypeOverride("speech");
  UpdateAllLifecyclePhases();

  // Only the single div element should have its style recomputed.
  EXPECT_EQ(1u, GetStyleEngine().StyleForElementCount() - initial_count);
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      div->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
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

  Element* div = GetDocument().getElementById(AtomicString("green"));
  EXPECT_EQ(Color::kBlack, div->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));

  unsigned initial_count = GetStyleEngine().StyleForElementCount();

  GetDocument().GetSettings()->SetPrefersReducedMotion(true);
  UpdateAllLifecyclePhases();

  // Only the single div element should have its style recomputed.
  EXPECT_EQ(1u, GetStyleEngine().StyleForElementCount() - initial_count);
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      div->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, RevertUseCount) {
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

  Element* ref = GetDocument().getElementById(AtomicString("ref"));
  Element* elem = GetDocument().getElementById(AtomicString("elem"));
  ASSERT_TRUE(ref);
  ASSERT_TRUE(elem);

  // This test assumes that the initial color is not 'red'. Verify that
  // assumption.
  ASSERT_NE(ComputedValue(ref, "color")->CssText(),
            ComputedValue(elem, "color")->CssText());

  EXPECT_EQ("rgb(255, 0, 0)", ComputedValue(elem, "color")->CssText());

  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetInForcedColors(GetDocument(),
                                        /*in_forced_colors=*/true);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(ComputedValue(ref, "color")->CssText(),
            ComputedValue(elem, "color")->CssText());

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSKeywordRevert));
}

TEST_F(StyleEngineTest, PrintNoDarkColorScheme) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);

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
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            root->GetComputedStyle()->UsedColorScheme());
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      body->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  gfx::SizeF page_size(400, 400);
  GetDocument().GetFrame()->StartPrinting(WebPrintParams(page_size));
  EXPECT_EQ(Color::kBlack, root->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));
  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            root->GetComputedStyle()->UsedColorScheme());
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      body->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetDocument().GetFrame()->EndPrinting();
  EXPECT_EQ(Color::kWhite, root->GetComputedStyle()->VisitedDependentColor(
                               GetCSSPropertyColor()));
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            root->GetComputedStyle()->UsedColorScheme());
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      body->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, PrintNoForceDarkMode) {
  auto* frame_view = GetDocument().View();
  GetDocument().documentElement()->SetInlineStyleProperty(
      CSSPropertyID::kBackgroundColor, "white");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(frame_view->DocumentBackgroundColor(), Color::kWhite);
  EXPECT_EQ(GetDocument().documentElement()->GetComputedStyle()->ForceDark(),
            false);

  GetDocument().GetSettings()->SetForceDarkModeEnabled(true);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(frame_view->DocumentBackgroundColor(), Color(18, 18, 18));
  EXPECT_EQ(GetDocument().documentElement()->GetComputedStyle()->ForceDark(),
            true);

  gfx::SizeF page_size(400, 400);
  GetDocument().GetFrame()->StartPrinting(WebPrintParams(page_size));
  EXPECT_EQ(frame_view->DocumentBackgroundColor(), Color::kWhite);
  EXPECT_EQ(GetDocument().documentElement()->GetComputedStyle()->ForceDark(),
            false);

  GetDocument().GetFrame()->EndPrinting();
  EXPECT_EQ(frame_view->DocumentBackgroundColor(), Color(18, 18, 18));
  EXPECT_EQ(GetDocument().documentElement()->GetComputedStyle()->ForceDark(),
            true);
}

TEST_F(StyleEngineTest, PrintScriptingEnabled) {
  GetDocument().body()->setInnerHTML(R"HTML(
      <style>
        @media (scripting) {
          body { color: green; }
        }
        @media (scripting: none) {
          body { color: red; }
        }
      </style>
    )HTML");
  GetFrame().GetSettings()->SetScriptEnabled(true);
  UpdateAllLifecyclePhases();
  Element* body = GetDocument().body();

  EXPECT_EQ(true,
            GetDocument().GetExecutionContext()->CanExecuteScripts(
                ReasonForCallingCanExecuteScripts::kNotAboutToExecuteScript));

  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      body->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  gfx::SizeF page_size(400, 400);
  GetDocument().GetFrame()->StartPrinting(WebPrintParams(page_size));
  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      body->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetDocument().GetFrame()->EndPrinting();
}

TEST_F(StyleEngineTest, MediaQueriesChangeScripting) {
  GetDocument().body()->setInnerHTML(R"HTML(
        <style>
          @media (scripting) {
            body { color: green; }
          }
          @media (scripting: none) {
            body { color: red; }
          }
        </style>
      )HTML");
  GetFrame().GetSettings()->SetScriptEnabled(true);
  UpdateAllLifecyclePhases();
  Element* body = GetDocument().body();

  EXPECT_EQ(true,
            GetDocument().GetExecutionContext()->CanExecuteScripts(
                ReasonForCallingCanExecuteScripts::kNotAboutToExecuteScript));

  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      body->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  GetFrame().GetSettings()->SetScriptEnabled(false);
  UpdateAllLifecyclePhases();

  EXPECT_EQ(false,
            GetDocument().GetExecutionContext()->CanExecuteScripts(
                ReasonForCallingCanExecuteScripts::kNotAboutToExecuteScript));

  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      body->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, AtPropertyUseCount) {
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

TEST_F(StyleEngineTest, AtScopeUseCount) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { --x: No @scope rule here; }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSAtRuleScope));

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @scope (.a) {
        body { --x:true; }
      }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSAtRuleScope));
}

TEST_F(StyleEngineTest, RemoveDeclaredPropertiesEmptyRegistry) {
  EXPECT_FALSE(GetDocument().GetPropertyRegistry());
  PropertyRegistration::RemoveDeclaredProperties(GetDocument());
  EXPECT_FALSE(GetDocument().GetPropertyRegistry());
}

TEST_F(StyleEngineTest, AtPropertyInUserOrigin) {
  // @property in the user origin:
  InjectSheet("user1", WebCssOrigin::kUser, R"CSS(
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
  InjectSheet("author", WebCssOrigin::kAuthor, R"CSS(
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
  InjectSheet("user2", WebCssOrigin::kUser, R"CSS(
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

  Element* target = GetDocument().getElementById(AtomicString("target"));
  target->setAttribute(html_names::kMediaAttr, AtomicString("print"));

  // Shouldn't crash.
  UpdateAllLifecyclePhases();
}

// Properties stored for forced colors mode should only be usable by the UA.
TEST_F(StyleEngineTest, InternalForcedProperties) {
  String properties_to_test[] = {
      "-internal-forced-background-color", "-internal-forced-border-color",
      "-internal-forced-color", "-internal-forced-outline-color",
      "-internal-forced-visited-color"};
  for (auto property : properties_to_test) {
    String declaration = property + ":red";
    ASSERT_TRUE(
        css_test_helpers::ParseDeclarationBlock(declaration, kHTMLStandardMode)
            ->IsEmpty());
    ASSERT_TRUE(
        !css_test_helpers::ParseDeclarationBlock(declaration, kUASheetMode)
             ->IsEmpty());
  }
}

TEST_F(StyleEngineTest, HasViewportUnitFlags) {
  struct {
    const char* value;
    bool has_static;
    bool has_dynamic;
  } test_data[] = {
      {"1px", false, false},
      {"1em", false, false},
      {"1rem", false, false},

      {"1vw", true, false},
      {"1vh", true, false},
      {"1vi", true, false},
      {"1vb", true, false},
      {"1vmin", true, false},
      {"1vmax", true, false},

      {"1svw", true, false},
      {"1svh", true, false},
      {"1svi", true, false},
      {"1svb", true, false},
      {"1svmin", true, false},
      {"1svmax", true, false},

      {"1lvw", true, false},
      {"1lvh", true, false},
      {"1lvi", true, false},
      {"1lvb", true, false},
      {"1lvmin", true, false},
      {"1lvmax", true, false},

      {"1dvw", false, true},
      {"1dvh", false, true},
      {"1dvi", false, true},
      {"1dvb", false, true},
      {"1dvmin", false, true},
      {"1dvmax", false, true},

      {"calc(1vh)", true, false},
      {"calc(1dvh)", false, true},
      {"calc(1vh + 1dvh)", true, true},
  };

  for (const auto& data : test_data) {
    SCOPED_TRACE(data.value);
    auto holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
    Document& document = holder->GetDocument();
    document.body()->setInnerHTML(String::Format(R"HTML(
      <style>
        div { width: %s; }
      </style>
      <div id=target></div>
    )HTML",
                                                 data.value));
    document.View()->UpdateAllLifecyclePhasesForTest();

    Element* target = document.getElementById(AtomicString("target"));
    ASSERT_TRUE(target);

    EXPECT_EQ(data.has_static,
              target->GetComputedStyle()->HasStaticViewportUnits());
    EXPECT_EQ(data.has_dynamic,
              target->GetComputedStyle()->HasDynamicViewportUnits());
    EXPECT_EQ(data.has_static, document.HasStaticViewportUnits());
    EXPECT_EQ(data.has_dynamic, document.HasDynamicViewportUnits());
  }
}

TEST_F(StyleEngineTest, DynamicViewportUnitInvalidation) {
  GetDocument().body()->setInnerHTML(R"HTML(
  <style>
    #target_px { width: 1px; }
    #target_svh { width: 1svh; }
    #target_dvh { width: 1dvh; }
  </style>
  <div id=target_px></div>
  <div id=target_svh></div>
  <div id=target_dvh></div>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* target_px = GetDocument().getElementById(AtomicString("target_px"));
  Element* target_svh =
      GetDocument().getElementById(AtomicString("target_svh"));
  Element* target_dvh =
      GetDocument().getElementById(AtomicString("target_dvh"));
  ASSERT_TRUE(target_px);
  ASSERT_TRUE(target_svh);
  ASSERT_TRUE(target_dvh);

  EXPECT_FALSE(target_px->NeedsStyleRecalc());
  EXPECT_FALSE(target_svh->NeedsStyleRecalc());
  EXPECT_FALSE(target_dvh->NeedsStyleRecalc());

  // Only dvh should be affected:
  GetDocument().DynamicViewportUnitsChanged();
  GetStyleEngine().InvalidateViewportUnitStylesIfNeeded();
  EXPECT_FALSE(target_px->NeedsStyleRecalc());
  EXPECT_FALSE(target_svh->NeedsStyleRecalc());
  EXPECT_TRUE(target_dvh->NeedsStyleRecalc());
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(target_px->NeedsStyleRecalc());
  EXPECT_FALSE(target_svh->NeedsStyleRecalc());
  EXPECT_FALSE(target_dvh->NeedsStyleRecalc());

  //  svh/dvh should be affected:
  GetDocument().LayoutViewportWasResized();
  GetStyleEngine().InvalidateViewportUnitStylesIfNeeded();
  EXPECT_FALSE(target_px->NeedsStyleRecalc());
  EXPECT_TRUE(target_svh->NeedsStyleRecalc());
  EXPECT_TRUE(target_dvh->NeedsStyleRecalc());
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(target_px->NeedsStyleRecalc());
  EXPECT_FALSE(target_svh->NeedsStyleRecalc());
  EXPECT_FALSE(target_dvh->NeedsStyleRecalc());
}

TEST_F(StyleEngineTest, DynamicViewportUnitsInMediaQuery) {
  // Changes in the dynamic viewport should not affect NeedsActiveStyleUpdate
  // when we don't use dynamic viewport units.
  {
    auto holder = DummyPageHolderWithHTML(R"HTML(
        <style>
          @media (min-width: 50vh) {
            :root { color: green; }
          }
        </style>
      )HTML");
    Document& document = holder->GetDocument();

    EXPECT_FALSE(document.GetStyleEngine().NeedsActiveStyleUpdate());
    document.DynamicViewportUnitsChanged();
    EXPECT_FALSE(document.GetStyleEngine().NeedsActiveStyleUpdate());
  }

  // NeedsActiveStyleUpdate should be set when dv* units are used.
  {
    auto holder = DummyPageHolderWithHTML(R"HTML(
        <style>
          @media (min-width: 50dvh) {
            :root { color: green; }
          }
        </style>
      )HTML");
    Document& document = holder->GetDocument();

    EXPECT_FALSE(document.GetStyleEngine().NeedsActiveStyleUpdate());
    document.DynamicViewportUnitsChanged();
    EXPECT_TRUE(document.GetStyleEngine().NeedsActiveStyleUpdate());
  }

  // Same as the first test, but with media attribute.
  {
    auto holder = DummyPageHolderWithHTML(R"HTML(
        <style media="(min-width: 50vh)">
          :root { color: green; }
        </style>
      )HTML");
    Document& document = holder->GetDocument();

    EXPECT_FALSE(document.GetStyleEngine().NeedsActiveStyleUpdate());
    document.DynamicViewportUnitsChanged();
    EXPECT_FALSE(document.GetStyleEngine().NeedsActiveStyleUpdate());
  }

  // // Same as the second test, but with media attribute.
  {
    auto holder = DummyPageHolderWithHTML(R"HTML(
      <style media="(min-width: 50dvh)">
        :root { color: green; }
      </style>
    )HTML");
    Document& document = holder->GetDocument();

    EXPECT_FALSE(document.GetStyleEngine().NeedsActiveStyleUpdate());
    document.DynamicViewportUnitsChanged();
    EXPECT_TRUE(document.GetStyleEngine().NeedsActiveStyleUpdate());
  }
}

TEST_F(StyleEngineTest, MediaQueriesChangeDisplayState) {
  ScopedDesktopPWAsAdditionalWindowingControlsForTest scoped_feature(true);
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body {
        background-color: white;
      }
      @media (display-state: normal) {
        body {
          background-color: yellow;
        }
      }
      @media (display-state: minimized) {
        body {
          background-color: cyan;
        }
      }
      @media (display-state: maximized) {
        body {
          background-color: red;
        }
      }
      @media (display-state: fullscreen) {
        body {
          background-color: blue;
        }
      }
    </style>
    <body></body>
  )HTML");

  // display-state: normal
  // Default is set in /third_party/blink/renderer/core/frame/settings.json5.
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(/*yellow*/ 255, 255, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyBackgroundColor()));

  WTF::Vector<std::pair<ui::mojom::blink::WindowShowState, Color>> test_cases =
      {{ui::mojom::blink::WindowShowState::kMinimized,
        Color::FromRGB(/*cyan*/ 0, 255, 255)},
       {ui::mojom::blink::WindowShowState::kMaximized,
        Color::FromRGB(/*red*/ 255, 0, 0)},
       {ui::mojom::blink::WindowShowState::kFullscreen,
        Color::FromRGB(/*blue*/ 0, 0, 255)}};

  for (const auto& [show_state, color] : test_cases) {
    GetFrame().GetSettings()->SetWindowShowState(show_state);
    UpdateAllLifecyclePhases();
    EXPECT_EQ(color,
              GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                  GetCSSPropertyBackgroundColor()));
  }
}

TEST_F(StyleEngineTest, MediaQueriesChangeResizable) {
  ScopedDesktopPWAsAdditionalWindowingControlsForTest scoped_feature(true);
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body {
        background-color: white;
      }
      @media (resizable: true) {
        body {
          background-color: yellow;
        }
      }
      @media (resizable: false) {
        body {
          background-color: cyan;
        }
      }
    </style>
    <body></body>
  )HTML");

  // resizable: true
  // Default is set in /third_party/blink/renderer/core/frame/settings.json5.
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(/*yellow*/ 255, 255, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyBackgroundColor()));

  // resizable: false
  GetFrame().GetSettings()->SetResizable(false);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(/*cyan*/ 0, 255, 255),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyBackgroundColor()));
}

namespace {

class TestMediaQueryListListener : public MediaQueryListListener {
 public:
  void NotifyMediaQueryChanged() override { notified = true; }
  bool notified = false;
};

}  // namespace

TEST_F(StyleEngineTest, DynamicViewportUnitsInMediaQueryMatcher) {
  auto& matcher = GetDocument().GetMediaQueryMatcher();
  auto* listener = MakeGarbageCollected<TestMediaQueryListListener>();
  matcher.AddViewportListener(listener);

  // Note: SimulateFrame is responsible for eventually causing dispatch of
  // pending events to MediaQueryListListener.
  // See step 10.8 (call to CallMediaQueryListListeners) in
  // ScriptedAnimationController::ServiceScriptedAnimations.

  MediaQuerySet* mq_static = MediaQuerySet::Create(
      "(min-width: 50vh)", GetDocument().GetExecutionContext());
  ASSERT_TRUE(mq_static);
  matcher.Evaluate(mq_static);
  GetDocument().DynamicViewportUnitsChanged();
  SimulateFrame();
  EXPECT_FALSE(listener->notified);

  // Evaluating a media query with dv* units will mark the MediaQueryMatcher
  // as dependent on such units, hence we should see events when calling
  // DynamicViewportUnitsChanged after that.
  MediaQuerySet* mq_dynamic = MediaQuerySet::Create(
      "(min-width: 50dvh)", GetDocument().GetExecutionContext());
  ASSERT_TRUE(mq_dynamic);
  matcher.Evaluate(mq_dynamic);
  GetDocument().DynamicViewportUnitsChanged();
  SimulateFrame();
  EXPECT_TRUE(listener->notified);
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

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();
  ASSERT_TRUE(frame_document);
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            frame_document->GetStyleEngine().GetOwnerColorScheme());

  frame_element->SetInlineStyleProperty(CSSPropertyID::kColorScheme, "light");

  test::RunPendingTasks();
  Compositor().BeginFrame();
  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            frame_document->GetStyleEngine().GetOwnerColorScheme());
}

TEST_F(StyleEngineSimTest, OwnerColorSchemeBaseBackground) {
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

  auto* dark_document = To<HTMLIFrameElement>(GetDocument().getElementById(
                                                  AtomicString("dark-frame")))
                            ->contentDocument();
  auto* light_document = To<HTMLIFrameElement>(GetDocument().getElementById(
                                                   AtomicString("light-frame")))
                             ->contentDocument();
  ASSERT_TRUE(dark_document);
  ASSERT_TRUE(light_document);

  EXPECT_TRUE(dark_document->View()->ShouldPaintBaseBackgroundColor());
  EXPECT_EQ(Color(0x12, 0x12, 0x12),
            dark_document->View()->BaseBackgroundColor());
  EXPECT_FALSE(light_document->View()->ShouldPaintBaseBackgroundColor());

  GetDocument().documentElement()->setAttribute(blink::html_names::kClassAttr,
                                                AtomicString("dark"));

  test::RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_FALSE(dark_document->View()->ShouldPaintBaseBackgroundColor());
  EXPECT_TRUE(light_document->View()->ShouldPaintBaseBackgroundColor());
  EXPECT_EQ(Color::kWhite, light_document->View()->BaseBackgroundColor());
}

TEST_F(StyleEngineSimTest, ColorSchemeBaseBackgroundWhileRenderBlocking) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest css_resource("https://example.com/slow.css",
                                     "text/css");

  LoadURL("https://example.com");

  main_resource.Write(R"HTML(
    <!doctype html>
    <meta name="color-scheme" content="dark">
    <link rel="stylesheet" href="slow.css">
    Some content
  )HTML");

  css_resource.Start();
  test::RunPendingTasks();

  // No rendering updates should have happened yet.
  ASSERT_TRUE(GetDocument().documentElement());
  ASSERT_FALSE(GetDocument().documentElement()->GetComputedStyle());
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // The dark color-scheme meta should affect the canvas color.
  EXPECT_EQ(Color(0x12, 0x12, 0x12),
            GetDocument().View()->BaseBackgroundColor());

  main_resource.Finish();
  css_resource.Finish();
}

TEST_F(StyleEngineSimTest, IFramePreferredColorScheme) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);

  SimRequest main_resource("https://example.com", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");

  LoadURL("https://example.com");

  main_resource.Complete(R"HTML(
    <!doctype html>
    <iframe id="frame" src="https://example.com/frame.html"></iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @media (prefers-color-scheme: light) {
        body { background: lime; }
      }
      @media (prefers-color-scheme: dark) {
        body { background: green; }
      }
    </style>
  )HTML");

  test::RunPendingTasks();
  Compositor().BeginFrame();

  auto* frame_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  auto* frame_document = frame_element->contentDocument();
  ASSERT_TRUE(frame_document);
  EXPECT_EQ(mojom::blink::PreferredColorScheme::kLight,
            GetDocument().GetStyleEngine().GetPreferredColorScheme());
  EXPECT_EQ(mojom::blink::PreferredColorScheme::kLight,
            frame_document->GetStyleEngine().GetPreferredColorScheme());

  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  test::RunPendingTasks();
  Compositor().BeginFrame();

  EXPECT_EQ(mojom::blink::PreferredColorScheme::kDark,
            GetDocument().GetStyleEngine().GetPreferredColorScheme());
  EXPECT_EQ(mojom::blink::PreferredColorScheme::kDark,
            frame_document->GetStyleEngine().GetPreferredColorScheme());
}

TEST_F(StyleEngineContainerQueryTest, UpdateStyleAndLayoutTreeForContainer) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .container {
        container-type: size;
        width: 100px;
        height: 100px;
      }
      @container (min-width: 200px) {
        .affected { background-color: green; }
      }
    </style>
    <div id="container1" class="container">
      <span class="affected"></span>
      <div id="container2" class="container affected">
        <span class="affected"></span>
        <span></span>
        <span class="affected"></span>
        <span><span class="affected"></span></span>
        <span class="affected"></span>
        <div style="display:none" class="affected">
          <span class="affected"></span>
        </div>
        <div style="display:none">
          <span class="affected"></span>
          <span class="affected"></span>
        </div>
      </div>
      <span></span>
      <div class="container">
        <span class="affected"></span>
        <span class="affected"></span>
      </div>
      <span class="container" style="display:inline-block">
        <span class="affected"></span>
      </span>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  auto* container1 = GetDocument().getElementById(AtomicString("container1"));
  auto* container2 = GetDocument().getElementById(AtomicString("container2"));
  ASSERT_TRUE(container1);
  ASSERT_TRUE(container2);

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
      *container1, LogicalSize(200, 100), kLogicalAxesBoth);

  // The first span.affected child and #container2
  EXPECT_EQ(2u, GetStyleEngine().StyleForElementCount() - start_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
      *container2, LogicalSize(200, 100), kLogicalAxesBoth);

  // Three direct span.affected children, and the two display:none elements.
  EXPECT_EQ(6u, GetStyleEngine().StyleForElementCount() - start_count);
}

TEST_F(StyleEngineContainerQueryTest, ContainerQueriesContainmentNotApplying) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .container {
        container-type: size;
        width: 100px;
        height: 100px;
      }
      @container (min-width: 200px) {
        .toggle { background-color: green; }
      }
    </style>
    <div id="container" class="container">

      <!-- None of the following should be affected by a change in the
           size of #container. -->
      <div class="container" style="display:contents">
        <span class="toggle"></span>
      </div>
      <span class="container">
        <span class="toggle"></span>
      </span>
      <rt class="container">
        <span class="toggle"></span>
      </rt>
      <div class="container" style="display:table">
        <span class="toggle"></span>
      </div>
      <div class="container" style="display:table-cell">
        <span class="toggle"></span>
      </div>
      <div class="container" style="display:table-row">
        <span class="toggle"></span>
      </div>
      <div class="container" style="display:table-row-group">
        <span class="toggle"></span>
      </div>

      <!-- This should be affected, however. -->
      <div class="toggle">Affected</div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  auto* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_TRUE(container);

  unsigned start_count = GetStyleEngine().StyleForElementCount();

  GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
      *container, LogicalSize(200, 100), kLogicalAxesBoth);

  // Even though none of the inner containers are eligible for containment,
  // they are still containers for the purposes of evaluating container
  // queries. Hence, they should not be affected when the outer container
  // changes its size.
  EXPECT_EQ(1u, GetStyleEngine().StyleForElementCount() - start_count);
}

TEST_F(StyleEngineContainerQueryTest, PseudoElementContainerQueryRecalc) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #container {
        container-type: size;
        width: 100px;
        height: 100px;
      }
      @container (min-width: 200px) {
        #container::before { content: " " }
        span::before { content: " " }
      }
    </style>
    <div id="container">
      <span id="span"></span>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  auto* container = GetDocument().getElementById(AtomicString("container"));
  auto* span = GetDocument().getElementById(AtomicString("span"));
  ASSERT_TRUE(container);
  ASSERT_TRUE(span);

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
      *container, LogicalSize(200, 100), kLogicalAxesBoth);

  // The two ::before elements + #span.
  EXPECT_EQ(3u, GetStyleEngine().StyleForElementCount() - start_count);
}

TEST_F(StyleEngineContainerQueryTest, MarkStyleDirtyFromContainerRecalc) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #container {
        container-type: size;
        width: 100px;
        height: 100px;
      }
      @container (min-width: 200px) {
        #input { background-color: green; }
      }
    </style>
    <div id="container">
      <input id="input" type="text">
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  auto* container = GetDocument().getElementById(AtomicString("container"));
  auto* input = GetDocument().getElementById(AtomicString("input"));
  ASSERT_TRUE(container);
  ASSERT_TRUE(input);
  auto* inner_editor = DynamicTo<HTMLInputElement>(input)->InnerEditorElement();
  ASSERT_TRUE(inner_editor);

  const ComputedStyle* old_inner_style = inner_editor->GetComputedStyle();
  EXPECT_TRUE(old_inner_style);

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
      *container, LogicalSize(200, 100), kLogicalAxesBoth);

  // Input elements mark their InnerEditorElement() style-dirty when they are
  // recalculated. That means the UpdateStyleAndLayoutTreeForContainer() call
  // above will involve marking ChildNeedsStyleRecalc all the way up to the
  // documentElement. Check that we don't leave anything dirty.
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(GetDocument().documentElement()->ChildNeedsStyleRecalc());

  // The input element is recalculated. The inner editor element isn't counted
  // because we don't do normal style resolution to create the ComputedStyle for
  // it, but check that we have a new ComputedStyle object for it.
  EXPECT_EQ(1u, GetStyleEngine().StyleForElementCount() - start_count);

  const ComputedStyle* new_inner_style = inner_editor->GetComputedStyle();
  EXPECT_TRUE(new_inner_style);
  EXPECT_NE(old_inner_style, new_inner_style);
}

TEST_F(StyleEngineContainerQueryTest,
       UpdateStyleAndLayoutTreeWithoutLayoutDependency) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      .toggle { width: 200px; }
    </style>
    <div id=a></div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().View()->NeedsLayout());

  Element* a = GetDocument().getElementById(AtomicString("a"));
  ASSERT_TRUE(a);
  a->classList().Add(AtomicString("toggle"));

  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(GetDocument().View()->NeedsLayout())
      << "No layout if style does not depend on layout";
}

TEST_F(StyleEngineContainerQueryTest,
       UpdateStyleAndLayoutTreeWithLayoutDependency) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #container {
        container-type: inline-size;
      }
      #container.toggle {
        width: 200px;
      }

      @container (min-width: 200px) {
        #a { z-index: 2; }
      }
    </style>
    <main id=container>
      <div id=a></div>
    </main>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().View()->NeedsLayout());

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_TRUE(container);
  container->classList().Add(AtomicString("toggle"));

  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(GetDocument().View()->NeedsLayout())
      << "Layout should happen as part of UpdateStyleAndLayoutTree";

  Element* a = GetDocument().getElementById(AtomicString("a"));
  ASSERT_TRUE(a);
  EXPECT_EQ(2, a->ComputedStyleRef().ZIndex());
}

// https://crbug.com/1343570
TEST_F(StyleEngineContainerQueryTest,
       UpdateStyleAndLayoutTreeWithUpgradeInDisplayNone) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #container {
        container-type: inline-size;
      }
      #container.toggle {
        --x:1;
      }
      #a {
        display: none;
      }
      /* Intentionally no @container rule. */
    </style>
    <main id=container>
      <div id=a></div>
    </main>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().View()->NeedsLayout());
  EXPECT_FALSE(GetStyleEngine().StyleAffectedByLayout());

  Element* container = GetDocument().getElementById(AtomicString("container"));
  Element* a = GetDocument().getElementById(AtomicString("a"));
  ASSERT_TRUE(container);
  ASSERT_TRUE(a);

  EXPECT_FALSE(GetDocument().View()->NeedsLayout());
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*a));
  EXPECT_FALSE(GetStyleEngine().StyleAffectedByLayout());

  // Mutate DOM to invalidate style recalc.
  container->classList().Add(AtomicString("toggle"));
  EXPECT_EQ(Document::StyleAndLayoutTreeUpdate::kAnalyzed,
            GetDocument().CalculateStyleAndLayoutTreeUpdate());

  // Pretend something needs layout.
  GetDocument().View()->SetNeedsLayout();
  EXPECT_TRUE(GetDocument().View()->NeedsLayout());
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdateForNode(*a));

  // Even though style doesn't depend on layout in this case, we still need to
  // do a layout upgrade for elements that are 1) in display:none, and 2)
  // inside a container query container.
  //
  // See implementation of `ElementLayoutUpgrade::ShouldUpgrade` for more
  // information.
  GetDocument().UpdateStyleAndLayoutTreeForElement(a,
                                                   DocumentUpdateReason::kTest);
  EXPECT_FALSE(GetStyleEngine().StyleAffectedByLayout());
  EXPECT_FALSE(GetDocument().View()->NeedsLayout());
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*a));
}

TEST_F(StyleEngineTest, UpdateStyleAndLayoutTreeWithAnchorQuery) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #anchored {
        position: absolute;
        left: anchor(--a left, 42px);
      }
      #anchored.toggle {
        left: anchor(--a left, 84px);
      }

      #inner { left: inherit; }
    </style>
    <main id=anchored>
      <div id=inner></div>
    </main>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().View()->NeedsLayout());

  Element* anchored = GetDocument().getElementById(AtomicString("anchored"));
  ASSERT_TRUE(anchored);
  anchored->classList().Add(AtomicString("toggle"));

  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(GetDocument().View()->NeedsLayout())
      << "Layout should happen as part of UpdateStyleAndLayoutTree";

  Element* inner = GetDocument().getElementById(AtomicString("inner"));
  ASSERT_TRUE(inner);
  EXPECT_EQ("84px", ComputedValue(inner, "left")->CssText());
}

TEST_F(StyleEngineTest, UpdateStyleAndLayoutTreeForElementWithAnchorQuery) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #anchored {
        position: absolute;
        left: anchor(--a left, 42px);
      }
      #anchored.toggle {
        left: anchor(--a left, 84px);
      }

      #inner { left: inherit; }
    </style>
    <main id=anchored>
      <div id=inner></div>
    </main>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().View()->NeedsLayout());

  Element* anchored = GetDocument().getElementById(AtomicString("anchored"));
  ASSERT_TRUE(anchored);
  anchored->classList().Add(AtomicString("toggle"));

  Element* inner = GetDocument().getElementById(AtomicString("inner"));
  ASSERT_TRUE(inner);

  GetDocument().UpdateStyleAndLayoutTreeForElement(inner,
                                                   DocumentUpdateReason::kTest);
  EXPECT_FALSE(GetDocument().View()->NeedsLayout())
      << "Layout should happen as part of UpdateStyleAndLayoutTreeForElement";

  EXPECT_EQ("84px", ComputedValue(inner, "left")->CssText());
}

TEST_F(StyleEngineTest, AnchorQueryComputed) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #anchor {
        anchor-name: --a;
        position: absolute;
        width: 100px;
        height: 100px;
        left: 200px;
        top: 300px;
      }
      #anchored {
        position: absolute;
        width: anchor-size(--a width);
        height: anchor-size(--unknown height, 42px);
        left: anchor(--a right);
        top: anchor(--a bottom);
      }
    </style>
    <div id=anchor>Anchor</div>
    <div id=anchored>Anchored</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* anchored = GetDocument().getElementById(AtomicString("anchored"));
  ASSERT_TRUE(anchored);

  EXPECT_EQ("300px", ComputedValue(anchored, "left")->CssText());
  EXPECT_EQ("400px", ComputedValue(anchored, "top")->CssText());
  EXPECT_EQ("100px", ComputedValue(anchored, "width")->CssText());
  EXPECT_EQ("42px", ComputedValue(anchored, "height")->CssText());
}

TEST_F(StyleEngineTest, AnchorQueryComputedChild) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #anchor {
        anchor-name: --a;
        position: absolute;
        width: 100px;
        height: 100px;
        left: 200px;
        top: 300px;
      }
      #anchored {
        position: absolute;
        width: anchor-size(--a width);
        height: width: anchor-size(--a height);
      }
      #child {
        width: anchor-size(--a width, 42px);
        height: inherit;
      }
    </style>
    <div id=anchor>Anchor</div>
    <div id=anchored>
      <div id=child>Child</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* child = GetDocument().getElementById(AtomicString("child"));
  ASSERT_TRUE(child);

  // Non-absolutely positioned child may not evaluate queries.
  EXPECT_EQ("42px", ComputedValue(child, "width")->CssText());
}

TEST_F(StyleEngineTest, VideoControlsReject) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <video controls></video>
    <div id="target"></div>
  )HTML");
  UpdateAllLifecyclePhases();

  StyleEngine& engine = GetStyleEngine();
  // Even if the Stats() were already enabled, the following resets it to 0.
  engine.SetStatsEnabled(true);

  StyleResolverStats* stats = engine.Stats();
  ASSERT_TRUE(stats);
  EXPECT_EQ(0u, stats->rules_fast_rejected);
  EXPECT_EQ(0u, stats->rules_rejected);

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  target->SetInlineStyleProperty(CSSPropertyID::kColor, "green");

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetStyleEngine().RecalcStyle();

  // There should be no UA rules for a div to reject
  EXPECT_EQ(0u, stats->rules_fast_rejected);
  EXPECT_EQ(0u, stats->rules_rejected);
}

TEST_F(StyleEngineTest, FastRejectForHostChild) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .notfound span {
        color: pink;
      }
    </style>
    <div id="host">
      <span id="slotted"></span>
    </div>
  )HTML");

  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(R"HTML(
    <slot></slot>
  )HTML");
  UpdateAllLifecyclePhases();

  StyleEngine& engine = GetStyleEngine();
  // Even if the Stats() were already enabled, the following resets it to 0.
  engine.SetStatsEnabled(true);

  StyleResolverStats* stats = engine.Stats();
  ASSERT_TRUE(stats);
  EXPECT_EQ(0u, stats->rules_fast_rejected);

  Element* span = GetDocument().getElementById(AtomicString("slotted"));
  ASSERT_TRUE(span);
  span->SetInlineStyleProperty(CSSPropertyID::kColor, "green");

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetStyleEngine().RecalcStyle();

  // Should fast reject ".notfound span"
  EXPECT_EQ(1u, stats->rules_fast_rejected);
}

TEST_F(StyleEngineTest, RejectSlottedSelector) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="host">
      <span id="slotted"></span>
    </div>
  )HTML");

  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(R"HTML(
    <style>
      .notfound ::slotted(span) {
        color: pink;
      }
    </style>
    <slot></slot>
  )HTML");
  UpdateAllLifecyclePhases();

  StyleEngine& engine = GetStyleEngine();
  // Even if the Stats() were already enabled, the following resets it to 0.
  engine.SetStatsEnabled(true);

  StyleResolverStats* stats = engine.Stats();
  ASSERT_TRUE(stats);
  EXPECT_EQ(0u, stats->rules_fast_rejected);

  Element* span = GetDocument().getElementById(AtomicString("slotted"));
  ASSERT_TRUE(span);
  span->SetInlineStyleProperty(CSSPropertyID::kColor, "green");

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetStyleEngine().RecalcStyle();

  // Should fast reject ".notfound ::slotted(span)"
  EXPECT_EQ(1u, stats->rules_fast_rejected);
}

TEST_F(StyleEngineTest, FastRejectForNesting) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .notfound {
        & span {
          color: pink;
        }
      }
    </style>
    <div>
      <span id="child">not pink</span>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  StyleEngine& engine = GetStyleEngine();
  // Even if the Stats() were already enabled, the following resets it to 0.
  engine.SetStatsEnabled(true);

  StyleResolverStats* stats = engine.Stats();
  ASSERT_TRUE(stats);
  EXPECT_EQ(0u, stats->rules_fast_rejected);

  Element* span = GetDocument().getElementById(AtomicString("child"));
  ASSERT_TRUE(span);
  span->SetInlineStyleProperty(CSSPropertyID::kColor, "green");

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetStyleEngine().RecalcStyle();

  // Should fast reject "& span"
  EXPECT_EQ(1u, stats->rules_fast_rejected);
}

TEST_F(StyleEngineTest, FastRejectForComplexSingleIs) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      :is(#parent .notfound) > span {
        color: pink;
      }
    </style>
    <div id="parent">
      <span id="child">not pink</span>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  StyleEngine& engine = GetStyleEngine();
  // Even if the Stats() were already enabled, the following resets it to 0.
  engine.SetStatsEnabled(true);

  StyleResolverStats* stats = engine.Stats();
  ASSERT_TRUE(stats);
  EXPECT_EQ(0u, stats->rules_fast_rejected);

  Element* span = GetDocument().getElementById(AtomicString("child"));
  ASSERT_TRUE(span);
  span->SetInlineStyleProperty(CSSPropertyID::kColor, "green");

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetStyleEngine().RecalcStyle();

  // Should fast reject ":is(#parent .notfound) > span", even though it is not
  // the same as "#parent .notfound > span".
  EXPECT_EQ(1u, stats->rules_fast_rejected);
}

TEST_F(StyleEngineTest, NoFastRejectForMultipleIs) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      :is(#foo, #bar) span {
        color: pink;
      }
    </style>
    <div id="parent">
      <span id="child">not pink</span>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  StyleEngine& engine = GetStyleEngine();
  // Even if the Stats() were already enabled, the following resets it to 0.
  engine.SetStatsEnabled(true);

  StyleResolverStats* stats = engine.Stats();
  ASSERT_TRUE(stats);
  EXPECT_EQ(0u, stats->rules_fast_rejected);

  Element* span = GetDocument().getElementById(AtomicString("child"));
  ASSERT_TRUE(span);
  span->SetInlineStyleProperty(CSSPropertyID::kColor, "green");

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetStyleEngine().RecalcStyle();

  // Should not try to fast reject due to the (multiple-element) selector list.
  EXPECT_EQ(0u, stats->rules_fast_rejected);
}

TEST_F(StyleEngineTest, ScrollbarPartPseudoDoesNotMatchElement) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .parent ::-webkit-scrollbar-button { background-color: red; }
      .parent ::-webkit-scrollbar-corner { background-color: red; }
      .parent ::-webkit-scrollbar-thumb { background-color: red; }
      .parent ::-webkit-scrollbar-track { background-color: red; }
      .parent ::-webkit-scrollbar-track-piece { background-color: red; }
    </style>
    <div class="parent">
      <div class="child"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  StyleEngine& engine = GetStyleEngine();
  // Even if the Stats() were already enabled, the following resets it to 0.
  engine.SetStatsEnabled(true);

  StyleResolverStats* stats = engine.Stats();
  ASSERT_TRUE(stats);
  EXPECT_EQ(0u, stats->rules_matched);

  Element* div = GetDocument().QuerySelector(AtomicString(".child"));
  ASSERT_TRUE(div);
  div->SetInlineStyleProperty(CSSPropertyID::kColor, "green");

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetStyleEngine().RecalcStyle();

  // We have two UA rule for <div> that match:
  //  div { display: block; }
  //  div { unicode-bidi: isolate; }
  EXPECT_EQ(stats->rules_matched, 2u);
}

TEST_F(StyleEngineTest, AudioUAStyleNameSpace) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <audio id="html-audio"></audio>
  )HTML");
  Element* html_audio =
      GetDocument().getElementById(AtomicString("html-audio"));
  Element* audio =
      GetDocument().createElementNS(AtomicString("http://dummyns"),
                                    AtomicString("audio"), ASSERT_NO_EXCEPTION);
  GetDocument().body()->appendChild(audio);
  UpdateAllLifecyclePhases();

  // display:none UA rule for audio element should not apply outside html.
  EXPECT_TRUE(audio->GetComputedStyle());
  EXPECT_FALSE(html_audio->GetComputedStyle());

  gfx::SizeF page_size(400, 400);
  GetDocument().GetFrame()->StartPrinting(WebPrintParams(page_size));

  // Also for printing.
  EXPECT_TRUE(audio->GetComputedStyle());
  EXPECT_FALSE(html_audio->GetComputedStyle());
}

TEST_F(StyleEngineTest, TargetTextUseCount) {
  ClearUseCounter(WebFeature::kCSSSelectorTargetText);
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #nevermatch::target-text { background-color: pink }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSSelectorTargetText));
  ClearUseCounter(WebFeature::kCSSSelectorTargetText);

  // Count ::target-text if we would have matched if the page was loaded with a
  // text fragment url.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      div::target-text { background-color: pink }
    </style>
    <div></div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSSelectorTargetText));
  ClearUseCounter(WebFeature::kCSSSelectorTargetText);
}

TEST_F(StyleEngineTest, NonDirtyStyleRecalcRoot) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="host">
      <span id="slotted"></span>
    </div>
  )HTML");

  auto* host = GetDocument().getElementById(AtomicString("host"));
  auto* slotted = GetDocument().getElementById(AtomicString("slotted"));

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<slot></slot>");
  UpdateAllLifecyclePhases();

  slotted->remove();
  GetDocument().body()->appendChild(slotted);
  host->remove();
  auto* recalc_root = GetStyleRecalcRoot();
  EXPECT_EQ(recalc_root, &GetDocument());
  EXPECT_TRUE(GetDocument().documentElement()->ChildNeedsStyleRecalc());
}

TEST_F(StyleEngineTest, AtCounterStyleUseCounter) {
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSAtRuleCounterStyle));

  GetDocument().body()->setInnerHTML("<style>@counter-style foo {}</style>");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSAtRuleCounterStyle));
}

TEST_F(StyleEngineTest, AtContainerUseCount) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { --x: No @container rule here; }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSAtRuleContainer));

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @container (width > 0px) {
        body { --x: Hello world; }
      }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSAtRuleContainer));
}

TEST_F(StyleEngineTest, StyleQueryUseCount) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @container (width = 200px) {
        body { background: red; }
      }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSAtRuleContainer));
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSStyleContainerQuery));

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @container ((width > 0px) and style(--foo: bar)) {
        body { background: lime; }
      }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSAtRuleContainer));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSStyleContainerQuery));
}

TEST_F(StyleEngineTest, NestingUseCount) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { --x: No @nest or & rule here; }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSNesting));

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body {
        & .foo { color: fuchsia; }
      }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSNesting));
}

TEST_F(StyleEngineTest, NestingUseCountUnsupportedDeclaration) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { unsupported: 100px; }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSNesting));
}

TEST_F(StyleEngineTest, NestingUseCountSupportedDeclaration) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { width: 100px; }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSNesting));
}

TEST_F(StyleEngineTest, NestingUseCountDimensionToken) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { 500px: 300px; }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSNesting));
}

TEST_F(StyleEngineTest, NestingUseCountInvalidSelector) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { & !!! { color: fuchsia; } }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSNesting));
}

TEST_F(StyleEngineTest, NestingUseCountUnknownAtRule) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body {
        @unsupported {
          color: fuchsia;
        }
      }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSNesting));
}

TEST_F(StyleEngineTest, NestingUseCountAtRule) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body {
        @media {
          color: fuchsia;
        }
      }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSNesting));
}

TEST_F(StyleEngineTest, NestingUseCountNotStartingWithAmpersand) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body { --x: No @nest rule or & here; }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kCSSNesting));

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body {
        .foo & { color: lemonchiffon; }
      }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kCSSNesting));
}

TEST_F(StyleEngineTest, SystemFontsObeyDefaultFontSize) {
  // <input> get assigned "font: -webkit-small-control" in the UA sheet.
  Element* body = GetDocument().body();
  body->setInnerHTML("<input>");
  Element* input = GetDocument().QuerySelector(AtomicString("input"));

  // Test the standard font sizes that can be chosen in chrome://settings/
  for (int fontSize : {9, 12, 16, 20, 24}) {
    GetDocument().GetSettings()->SetDefaultFontSize(fontSize);
    UpdateAllLifecyclePhases();
    EXPECT_EQ(fontSize, body->GetComputedStyle()->FontSize());
    EXPECT_EQ(fontSize - 3, input->GetComputedStyle()->FontSize());
  }

  // Now test degenerate cases
  GetDocument().GetSettings()->SetDefaultFontSize(-1);
  GetDocument().GetStyleResolver().InvalidateMatchedPropertiesCache();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(1, body->GetComputedStyle()->FontSize());
  EXPECT_EQ(1, input->GetComputedStyle()->FontSize());

  GetDocument().GetSettings()->SetDefaultFontSize(0);
  GetDocument().GetStyleResolver().InvalidateMatchedPropertiesCache();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(1, body->GetComputedStyle()->FontSize());
  EXPECT_EQ(13, input->GetComputedStyle()->FontSize());

  GetDocument().GetSettings()->SetDefaultFontSize(1);
  GetDocument().GetStyleResolver().InvalidateMatchedPropertiesCache();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(1, body->GetComputedStyle()->FontSize());
  EXPECT_EQ(1, input->GetComputedStyle()->FontSize());

  GetDocument().GetSettings()->SetDefaultFontSize(2);
  GetDocument().GetStyleResolver().InvalidateMatchedPropertiesCache();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(2, body->GetComputedStyle()->FontSize());
  EXPECT_EQ(2, input->GetComputedStyle()->FontSize());

  GetDocument().GetSettings()->SetDefaultFontSize(3);
  GetDocument().GetStyleResolver().InvalidateMatchedPropertiesCache();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(3, body->GetComputedStyle()->FontSize());
  EXPECT_EQ(0, input->GetComputedStyle()->FontSize());

  GetDocument().GetSettings()->SetDefaultFontSize(12345);
  GetDocument().GetStyleResolver().InvalidateMatchedPropertiesCache();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(10000, body->GetComputedStyle()->FontSize());
  EXPECT_EQ(10000, input->GetComputedStyle()->FontSize());
}

TEST_F(StyleEngineTest, CascadeLayersInOriginsAndTreeScopes) {
  // Verifies that user layers and author layers in each tree scope are managed
  // separately. Each have their own layer ordering.

  auto* user_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  user_sheet->ParseString("@layer foo, bar;");
  StyleSheetKey user_key("user_layers");
  GetStyleEngine().InjectSheet(user_key, user_sheet, WebCssOrigin::kUser);

  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <style>
      @layer bar, foo;
    </style>
    <div id="host">
      <template shadowrootmode="open">
        <style>
          @layer foo, bar, foo.baz;
        </style>
      </template>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  // User layer order: foo, bar, (implicit outer layer)
  auto* user_layer_map = GetStyleEngine().GetUserCascadeLayerMap();
  ASSERT_TRUE(user_layer_map);

  const CascadeLayer& user_outer_layer =
      user_sheet->GetRuleSet().CascadeLayers();
  EXPECT_EQ("", user_outer_layer.GetName());
  EXPECT_EQ(CascadeLayerMap::kImplicitOuterLayerOrder,
            user_layer_map->GetLayerOrder(user_outer_layer));

  const CascadeLayer& user_foo = *user_outer_layer.GetDirectSubLayers()[0];
  EXPECT_EQ("foo", user_foo.GetName());
  EXPECT_EQ(0u, user_layer_map->GetLayerOrder(user_foo));

  const CascadeLayer& user_bar = *user_outer_layer.GetDirectSubLayers()[1];
  EXPECT_EQ("bar", user_bar.GetName());
  EXPECT_EQ(1u, user_layer_map->GetLayerOrder(user_bar));

  // Document scope author layer order: bar, foo, (implicit outer layer)
  auto* document_layer_map =
      GetDocument().GetScopedStyleResolver()->GetCascadeLayerMap();
  ASSERT_TRUE(document_layer_map);

  const CascadeLayer& document_outer_layer =
      To<HTMLStyleElement>(GetDocument().QuerySelector(AtomicString("style")))
          ->sheet()
          ->Contents()
          ->GetRuleSet()
          .CascadeLayers();
  EXPECT_EQ("", document_outer_layer.GetName());
  EXPECT_EQ(CascadeLayerMap::kImplicitOuterLayerOrder,
            document_layer_map->GetLayerOrder(document_outer_layer));

  const CascadeLayer& document_bar =
      *document_outer_layer.GetDirectSubLayers()[0];
  EXPECT_EQ("bar", document_bar.GetName());
  EXPECT_EQ(0u, document_layer_map->GetLayerOrder(document_bar));

  const CascadeLayer& document_foo =
      *document_outer_layer.GetDirectSubLayers()[1];
  EXPECT_EQ("foo", document_foo.GetName());
  EXPECT_EQ(1u, document_layer_map->GetLayerOrder(document_foo));

  // Shadow scope author layer order: foo.baz, foo, bar, (implicit outer layer)
  ShadowRoot* shadow =
      GetDocument().getElementById(AtomicString("host"))->GetShadowRoot();
  auto* shadow_layer_map =
      shadow->GetScopedStyleResolver()->GetCascadeLayerMap();
  ASSERT_TRUE(shadow_layer_map);

  const CascadeLayer& shadow_outer_layer =
      To<HTMLStyleElement>(shadow->QuerySelector(AtomicString("style")))
          ->sheet()
          ->Contents()
          ->GetRuleSet()
          .CascadeLayers();
  EXPECT_EQ("", shadow_outer_layer.GetName());
  EXPECT_EQ(CascadeLayerMap::kImplicitOuterLayerOrder,
            shadow_layer_map->GetLayerOrder(shadow_outer_layer));

  const CascadeLayer& shadow_foo = *shadow_outer_layer.GetDirectSubLayers()[0];
  EXPECT_EQ("foo", shadow_foo.GetName());
  EXPECT_EQ(1u, shadow_layer_map->GetLayerOrder(shadow_foo));

  const CascadeLayer& shadow_foo_baz = *shadow_foo.GetDirectSubLayers()[0];
  EXPECT_EQ("baz", shadow_foo_baz.GetName());
  EXPECT_EQ(0u, shadow_layer_map->GetLayerOrder(shadow_foo_baz));

  const CascadeLayer& shadow_bar = *shadow_outer_layer.GetDirectSubLayers()[1];
  EXPECT_EQ("bar", shadow_bar.GetName());
  EXPECT_EQ(2u, shadow_layer_map->GetLayerOrder(shadow_bar));
}

TEST_F(StyleEngineTest, CascadeLayersFromMultipleSheets) {
  // The layer ordering in sheet2 is different from the final ordering.
  GetDocument().body()->setInnerHTML(R"HTML(
    <style id="sheet1">
      @layer foo, bar;
    </style>
    <style id="sheet2">
      @layer baz, bar.qux, foo.quux;
    </style>
  )HTML");

  UpdateAllLifecyclePhases();

  // Final layer ordering:
  // foo.quux, foo, bar.qux, bar, baz, (implicit outer layer)
  auto* layer_map =
      GetDocument().GetScopedStyleResolver()->GetCascadeLayerMap();
  ASSERT_TRUE(layer_map);

  const CascadeLayer& sheet1_outer_layer =
      To<HTMLStyleElement>(GetDocument().getElementById(AtomicString("sheet1")))
          ->sheet()
          ->Contents()
          ->GetRuleSet()
          .CascadeLayers();
  EXPECT_EQ("", sheet1_outer_layer.GetName());
  EXPECT_EQ(CascadeLayerMap::kImplicitOuterLayerOrder,
            layer_map->GetLayerOrder(sheet1_outer_layer));

  const CascadeLayer& sheet1_foo = *sheet1_outer_layer.GetDirectSubLayers()[0];
  EXPECT_EQ("foo", sheet1_foo.GetName());
  EXPECT_EQ(1u, layer_map->GetLayerOrder(sheet1_foo));

  const CascadeLayer& sheet1_bar = *sheet1_outer_layer.GetDirectSubLayers()[1];
  EXPECT_EQ("bar", sheet1_bar.GetName());
  EXPECT_EQ(3u, layer_map->GetLayerOrder(sheet1_bar));

  const CascadeLayer& sheet2_outer_layer =
      To<HTMLStyleElement>(GetDocument().getElementById(AtomicString("sheet2")))
          ->sheet()
          ->Contents()
          ->GetRuleSet()
          .CascadeLayers();
  EXPECT_EQ("", sheet2_outer_layer.GetName());
  EXPECT_EQ(CascadeLayerMap::kImplicitOuterLayerOrder,
            layer_map->GetLayerOrder(sheet2_outer_layer));

  const CascadeLayer& sheet2_baz = *sheet2_outer_layer.GetDirectSubLayers()[0];
  EXPECT_EQ("baz", sheet2_baz.GetName());
  EXPECT_EQ(4u, layer_map->GetLayerOrder(sheet2_baz));

  const CascadeLayer& sheet2_bar = *sheet2_outer_layer.GetDirectSubLayers()[1];
  EXPECT_EQ("bar", sheet2_bar.GetName());
  EXPECT_EQ(3u, layer_map->GetLayerOrder(sheet2_bar));

  const CascadeLayer& sheet2_bar_qux = *sheet2_bar.GetDirectSubLayers()[0];
  EXPECT_EQ("qux", sheet2_bar_qux.GetName());
  EXPECT_EQ(2u, layer_map->GetLayerOrder(sheet2_bar_qux));

  const CascadeLayer& sheet2_foo = *sheet2_outer_layer.GetDirectSubLayers()[2];
  EXPECT_EQ("foo", sheet2_foo.GetName());
  EXPECT_EQ(1u, layer_map->GetLayerOrder(sheet2_foo));

  const CascadeLayer& sheet2_foo_quux = *sheet2_foo.GetDirectSubLayers()[0];
  EXPECT_EQ("quux", sheet2_foo_quux.GetName());
  EXPECT_EQ(0u, layer_map->GetLayerOrder(sheet2_foo_quux));
}

TEST_F(StyleEngineTest, CascadeLayersNotExplicitlyDeclared) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #no-layers { }
    </style>
  )HTML");

  UpdateAllLifecyclePhases();

  // We don't create CascadeLayerMap if no layers are explicitly declared.
  ASSERT_TRUE(GetDocument().GetScopedStyleResolver());
  ASSERT_FALSE(GetDocument().GetScopedStyleResolver()->GetCascadeLayerMap());
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSCascadeLayers));
}

TEST_F(StyleEngineTest, CascadeLayersSheetsRemoved) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <style>
      @layer bar, foo;
    </style>
    <div id="host">
      <template shadowrootmode="open">
        <style>
          @layer foo, bar, foo.baz;
        </style>
      </template>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  ASSERT_TRUE(GetDocument().GetScopedStyleResolver());
  ASSERT_TRUE(GetDocument().GetScopedStyleResolver()->GetCascadeLayerMap());

  ShadowRoot* shadow =
      GetDocument().getElementById(AtomicString("host"))->GetShadowRoot();
  ASSERT_TRUE(shadow->GetScopedStyleResolver());
  ASSERT_TRUE(shadow->GetScopedStyleResolver()->GetCascadeLayerMap());

  GetDocument().QuerySelector(AtomicString("style"))->remove();
  shadow->QuerySelector(AtomicString("style"))->remove();
  UpdateAllLifecyclePhases();

  // When all sheets are removed, document ScopedStyleResolver is not cleared
  // but the CascadeLayerMap should be cleared.
  ASSERT_TRUE(GetDocument().GetScopedStyleResolver());
  ASSERT_FALSE(GetDocument().GetScopedStyleResolver()->GetCascadeLayerMap());

  // When all sheets are removed, shadow tree ScopedStyleResolver is cleared.
  ASSERT_FALSE(shadow->GetScopedStyleResolver());
}

TEST_F(StyleEngineTest, NonSlottedStyleDirty) {
  GetDocument().body()->setInnerHTML("<div id=host></div>");
  auto* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);
  host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  UpdateAllLifecyclePhases();

  // Add a child element to a shadow host with no slots. The inserted element is
  // not marked for style recalc because the GetStyleRecalcParent() returns
  // nullptr.
  auto* span = MakeGarbageCollected<HTMLSpanElement>(GetDocument());
  host->appendChild(span);
  EXPECT_FALSE(host->ChildNeedsStyleRecalc());
  EXPECT_FALSE(span->NeedsStyleRecalc());

  UpdateAllLifecyclePhases();

  // Set a style on the inserted child outside the flat tree.
  // GetStyleRecalcParent() still returns nullptr, and the ComputedStyle of the
  // child outside the flat tree is still null. No need to mark dirty.
  span->SetInlineStyleProperty(CSSPropertyID::kColor, "red");
  EXPECT_FALSE(host->ChildNeedsStyleRecalc());
  EXPECT_FALSE(span->NeedsStyleRecalc());

  // Ensure the ComputedStyle for the child and then change the style.
  // GetStyleRecalcParent() is still null, which means the host is not marked
  // with ChildNeedsStyleRecalc(), but the child needs to be marked dirty to
  // make sure the next EnsureComputedStyle updates the style to reflect the
  // changes.
  const ComputedStyle* old_style = span->EnsureComputedStyle();
  span->SetInlineStyleProperty(CSSPropertyID::kColor, "green");
  EXPECT_FALSE(host->ChildNeedsStyleRecalc());
  EXPECT_TRUE(span->NeedsStyleRecalc());
  UpdateAllLifecyclePhases();

  EXPECT_EQ(span->GetComputedStyle(), old_style);
  const ComputedStyle* new_style = span->EnsureComputedStyle();
  EXPECT_NE(new_style, old_style);

  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            old_style->VisitedDependentColor(GetCSSPropertyColor()));
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            new_style->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, CascadeLayerUseCount) {
  {
    ASSERT_FALSE(IsUseCounted(WebFeature::kCSSCascadeLayers));
    GetDocument().body()->setInnerHTML("<style>@layer foo;</style>");
    EXPECT_TRUE(IsUseCounted(WebFeature::kCSSCascadeLayers));
    ClearUseCounter(WebFeature::kCSSCascadeLayers);
  }

  {
    ASSERT_FALSE(IsUseCounted(WebFeature::kCSSCascadeLayers));
    GetDocument().body()->setInnerHTML("<style>@layer foo { }</style>");
    EXPECT_TRUE(IsUseCounted(WebFeature::kCSSCascadeLayers));
    ClearUseCounter(WebFeature::kCSSCascadeLayers);
  }

  {
    ASSERT_FALSE(IsUseCounted(WebFeature::kCSSCascadeLayers));
    GetDocument().body()->setInnerHTML(
        "<style>@import url(foo.css) layer(foo);</style>");
    EXPECT_TRUE(IsUseCounted(WebFeature::kCSSCascadeLayers));
    ClearUseCounter(WebFeature::kCSSCascadeLayers);
  }
}

TEST_F(StyleEngineTest, UserKeyframesOverrideWithCascadeLayers) {
  auto* user_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  user_sheet->ParseString(R"CSS(
    @layer base, override;

    #target {
      animation: anim 1s paused;
    }

    @layer override {
      @keyframes anim {
        from { width: 100px; }
      }
    }

    @layer base {
      @keyframes anim {
        from { width: 50px; }
      }
    }
  )CSS");
  StyleSheetKey key("user");
  GetStyleEngine().InjectSheet(key, user_sheet, WebCssOrigin::kUser);

  GetDocument().body()->setInnerHTML(
      "<div id=target style='height: 100px'></div>");

  UpdateAllLifecyclePhases();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(100, target->OffsetWidth());
}

TEST_F(StyleEngineTest, UserCounterStyleOverrideWithCascadeLayers) {
  PageTestBase::LoadAhem(*GetDocument().GetFrame());

  auto* user_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  user_sheet->ParseString(R"CSS(
    @layer base, override;

    #target {
      width: min-content;
      font: 10px/1 Ahem;
    }

    #target::before {
      content: counter(dont-care, cnt-style);
    }

    @layer override {
      @counter-style cnt-style {
        system: cyclic;
        symbols: '0000';
      }
    }

    @layer base {
      @counter-style cnt-style {
        system: cyclic;
        symbols: '000';
      }
    }
  )CSS");
  StyleSheetKey key("user");
  GetStyleEngine().InjectSheet(key, user_sheet, WebCssOrigin::kUser);

  GetDocument().body()->setInnerHTML("<div id=target></div>");

  UpdateAllLifecyclePhases();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(40, target->OffsetWidth());
}

TEST_F(StyleEngineTest, UserPropertyOverrideWithCascadeLayers) {
  auto* user_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  user_sheet->ParseString(R"CSS(
    @layer base, override;

    #target {
      width: var(--foo);
    }

    @layer override {
      @property --foo {
        syntax: '<length>';
        initial-value: 100px;
        inherits: false;
      }
    }

    @layer base {
      @property --foo {
        syntax: '<length>';
        initial-value: 50px;
        inherits: false;
      }
    }
  )CSS");
  StyleSheetKey key("user");
  GetStyleEngine().InjectSheet(key, user_sheet, WebCssOrigin::kUser);

  GetDocument().body()->setInnerHTML(
      "<div id=target style='height: 100px'></div>");

  UpdateAllLifecyclePhases();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(100, target->OffsetWidth());
}

TEST_F(StyleEngineTest, UserAndAuthorPropertyOverrideWithCascadeLayers) {
  auto* user_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  user_sheet->ParseString(R"CSS(
    @layer base, override;

    @layer override {
      @property --foo {
        syntax: '<length>';
        initial-value: 50px;
        inherits: false;
      }
    }
  )CSS");
  StyleSheetKey key("user");
  GetStyleEngine().InjectSheet(key, user_sheet, WebCssOrigin::kUser);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @property --foo {
        syntax: '<length>';
        initial-value: 100px;
        inherits: false;
      }

      #target {
        width: var(--foo);
      }
    </style>
    <div id=target style='height: 100px'></div>
  )HTML");

  UpdateAllLifecyclePhases();

  // User-defined custom properties should not override author-defined
  // properties regardless of cascade layers.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(100, target->OffsetWidth());
}

TEST_F(StyleEngineSimTest, UserFontFaceOverrideWithCascadeLayers) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest ahem_resource("https://example.com/ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");

  main_resource.Complete(R"HTML(
    <!doctype html>
    <div id=target>Test</div>
  )HTML");

  auto* user_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  user_sheet->ParseString(R"CSS(
    @layer base, override;

    @layer override {
      @font-face {
        font-family: custom-font;
        src: url('ahem.woff2') format('woff2');
      }
    }

    @layer base {
      @font-face {
        font-family: custom-font;
        src: url('ahem.woff2') format('woff2');
        size-adjust: 200%; /* To distinguish with the other @font-face */
      }
    }

    #target {
      font: 20px/1 custom-font;
      width: min-content;
    }
  )CSS");
  StyleSheetKey key("user");
  GetDocument().GetStyleEngine().InjectSheet(key, user_sheet,
                                             WebCssOrigin::kUser);

  Compositor().BeginFrame();

  ahem_resource.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2")));

  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(80, target->OffsetWidth());
}

TEST_F(StyleEngineSimTest, UserAndAuthorFontFaceOverrideWithCascadeLayers) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest ahem_resource("https://example.com/ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");

  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url('ahem.woff2') format('woff2');
      }

      #target {
        font: 20px/1 custom-font;
        width: min-content;
      }
    </style>
    <div id=target>Test</div>
  )HTML");

  auto* user_sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(GetDocument()));
  user_sheet->ParseString(R"CSS(
    @layer base, override;

    @layer override {
      @font-face {
        font-family: custom-font;
        src: url('ahem.woff2') format('woff2');
        size-adjust: 200%; /* To distinguish with the other @font-face */
      }
    }

  )CSS");
  StyleSheetKey key("user");
  GetDocument().GetStyleEngine().InjectSheet(key, user_sheet,
                                             WebCssOrigin::kUser);

  Compositor().BeginFrame();

  ahem_resource.Complete(
      *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2")));

  test::RunPendingTasks();
  Compositor().BeginFrame();

  // User-defined font faces should not override author-defined font faces
  // regardless of cascade layers.
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(80, target->OffsetWidth());
}

TEST_F(StyleEngineTest, CascadeLayerActiveStyleSheetVectorNullRuleSetCrash) {
  // This creates an ActiveStyleSheetVector where the first entry has no
  // RuleSet, and the second entry has a layer rule difference.
  GetDocument().documentElement()->setInnerHTML(
      "<style media=invalid></style>"
      "<style>@layer {}</style>");

  // Should not crash
  UpdateAllLifecyclePhases();
}

TEST_F(StyleEngineTest, EmptyDetachParent) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <span id="parent"><b>A</b> <i>B</i></span>
  )HTML");
  UpdateAllLifecyclePhases();

  auto* parent = GetDocument().getElementById(AtomicString("parent"));
  parent->setInnerHTML("");

  ASSERT_TRUE(parent->GetLayoutObject());
  EXPECT_FALSE(parent->GetLayoutObject()->WhitespaceChildrenMayChange());
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(StyleEngineTest, LegacyListItemRebuildRootCrash) {
  UpdateAllLifecyclePhases();

  auto* doc_elm = GetDocument().documentElement();
  ASSERT_TRUE(doc_elm);

  doc_elm->SetInlineStyleProperty(CSSPropertyID::kDisplay, "list-item");
  doc_elm->SetInlineStyleProperty(CSSPropertyID::kColumnCount, "1");
  UpdateAllLifecyclePhases();

  doc_elm->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor, "green");
  // Should not crash
  UpdateAllLifecyclePhases();
}

// Regression test for https://crbug.com/1270190
TEST_F(StyleEngineTest, ScrollbarStyleNoExcessiveCaching) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
    .a {
      width: 50px;
      height: 50px;
      background-color: magenta;
      overflow-y: scroll;
      margin: 5px;
      float: left;
    }

    .b {
      height: 100px;
    }

    ::-webkit-scrollbar {
      width: 10px;
    }

    ::-webkit-scrollbar-thumb {
      background: green;
    }

    ::-webkit-scrollbar-thumb:hover {
      background: red;
    }
    </style>
    <div class="a" id="container">
      <div class="b">
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhases();

  // We currently don't cache ::-webkit-scrollbar-* pseudo element styles, so
  // the cache is always empty. If we decide to cache them, we should make sure
  // that the cache size remains bounded.

  Element* container = GetDocument().getElementById(AtomicString("container"));
  EXPECT_FALSE(container->GetComputedStyle()->GetPseudoElementStyleCache());

  PaintLayerScrollableArea* area =
      container->GetLayoutBox()->GetScrollableArea();
  Scrollbar* scrollbar = area->VerticalScrollbar();
  CustomScrollbar* custom_scrollbar = To<CustomScrollbar>(scrollbar);

  scrollbar->SetHoveredPart(kThumbPart);
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(container->GetComputedStyle()->GetPseudoElementStyleCache());
  EXPECT_EQ("rgb(255, 0, 0)", custom_scrollbar->GetPart(kThumbPart)
                                  ->Style()
                                  ->BackgroundColor()
                                  .GetColor()
                                  .SerializeAsCSSColor());

  scrollbar->SetHoveredPart(kNoPart);
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(container->GetComputedStyle()->GetPseudoElementStyleCache());
  EXPECT_EQ("rgb(0, 128, 0)", custom_scrollbar->GetPart(kThumbPart)
                                  ->Style()
                                  ->BackgroundColor()
                                  .GetColor()
                                  .SerializeAsCSSColor());
}

TEST_F(StyleEngineTest, HasPseudoClassInvalidationSkipIrrelevantClassChange) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>.a:has(.b) { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3>
          <div id=div4></div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div4"))
      ->setAttribute(html_names::kClassAttr, AtomicString("c"));
  UpdateAllLifecyclePhases();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div4"))
      ->setAttribute(html_names::kClassAttr, AtomicString("b"));
  UpdateAllLifecyclePhases();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
}

TEST_F(StyleEngineTest, HasPseudoClassInvalidationSkipIrrelevantIdChange) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>.a:has(#b) { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3>
          <div id=div4></div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div4"))
      ->setAttribute(html_names::kIdAttr, AtomicString("c"));
  UpdateAllLifecyclePhases();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("c"))
      ->setAttribute(html_names::kIdAttr, AtomicString("b"));
  UpdateAllLifecyclePhases();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
}

TEST_F(StyleEngineTest,
       HasPseudoClassInvalidationSkipIrrelevantAttributeChange) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>.a:has([b]) { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3>
          <div id=div4></div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div4"))
      ->setAttribute(
          QualifiedName(g_empty_atom, AtomicString("c"), g_empty_atom),
          AtomicString("C"));
  UpdateAllLifecyclePhases();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div4"))
      ->setAttribute(
          QualifiedName(g_empty_atom, AtomicString("b"), g_empty_atom),
          AtomicString("B"));
  UpdateAllLifecyclePhases();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
}

TEST_F(StyleEngineTest,
       HasPseudoClassInvalidationSkipIrrelevantInsertionRemoval) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>.a:has(.b) { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3></div>
        <div id=div4></div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  auto* div5 = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  div5->setAttribute(html_names::kIdAttr, AtomicString("div5"));
  div5->setInnerHTML(R"HTML(<div class='c'></div>)HTML");
  GetDocument().getElementById(AtomicString("div3"))->AppendChild(div5);
  UpdateAllLifecyclePhases();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(2U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  auto* div6 = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  div6->setAttribute(html_names::kIdAttr, AtomicString("div6"));
  div6->setInnerHTML(R"HTML(<div class='b'></div>)HTML");
  GetDocument().getElementById(AtomicString("div4"))->AppendChild(div6);
  UpdateAllLifecyclePhases();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(3U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div3"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div5")));
  UpdateAllLifecyclePhases();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div4"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div6")));
  UpdateAllLifecyclePhases();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
}

TEST_F(StyleEngineTest, HasPseudoClassInvalidationUniversalInArgument) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>.a:has(*) { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  auto* div3 = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  div3->setAttribute(html_names::kIdAttr, AtomicString("div3"));
  GetDocument().getElementById(AtomicString("div2"))->AppendChild(div3);
  UpdateAllLifecyclePhases();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(2U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div2"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div3")));
  UpdateAllLifecyclePhases();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
}

TEST_F(StyleEngineTest,
       HasPseudoClassInvalidationInsertionRemovalWithPseudoInHas) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .a:has(.b:focus) { background-color: lime; }
      .c:has(.d) { background-color: green; }
    </style>
    <div id=div1>
      <div id=div2 class='a'></div>
      <div id=div3 class='c'></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  auto* div4 = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  div4->setAttribute(html_names::kIdAttr, AtomicString("div4"));
  GetDocument().getElementById(AtomicString("div2"))->AppendChild(div4);
  UpdateAllLifecyclePhases();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(2U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  auto* div5 = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  div5->setAttribute(html_names::kIdAttr, AtomicString("div5"));
  GetDocument().getElementById(AtomicString("div3"))->AppendChild(div5);
  UpdateAllLifecyclePhases();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div2"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div4")));
  UpdateAllLifecyclePhases();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div3"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div5")));
  UpdateAllLifecyclePhases();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
}

TEST_F(StyleEngineTest, HasPseudoClassInvalidationLinkInHas) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .a:has(:link) { background-color: lime; }
    </style>
    <div id=div1 class='a'>
      <a href="unvisited"></a>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  auto* anchor = MakeGarbageCollected<HTMLAnchorElement>(GetDocument());
  anchor->setAttribute(html_names::kIdAttr, AtomicString("anchor1"));
  GetDocument().getElementById(AtomicString("div1"))->AppendChild(anchor);
  UpdateAllLifecyclePhases();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(2U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("anchor1")));
  UpdateAllLifecyclePhases();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
}

TEST_F(StyleEngineTest, HasPseudoClassInvalidationIgnoreVisitedPseudoInHas) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .a:has(:visited) { background-color: lime; }
    </style>
    <div id=div1 class='a'>
      <a href="unvisited"></a>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  auto* anchor = MakeGarbageCollected<HTMLAnchorElement>(GetDocument());
  anchor->SetHref(g_empty_atom);
  anchor->setAttribute(html_names::kIdAttr, AtomicString("anchor1"));
  GetDocument().getElementById(AtomicString("div1"))->AppendChild(anchor);
  UpdateAllLifecyclePhases();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("anchor1")));
  UpdateAllLifecyclePhases();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
}

TEST_F(StyleEngineTest, HasPseudoClassInvalidationCheckFiltering) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
    div { color: grey }
    .a:has(.b) { color: red }
    .c:has(.d) { color: green }
    .e:has(.f) .g { color: blue }
    .e:has(.h) .i { color: navy }
    .e:has(.f.h) .j { color: lightgreen }
    </style>
    <div class='a e'>
      <div class=g></div>
      <div class=i></div>
      <div class=j></div>
      <div id=child></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  // TODO(blee@igalia.com) Should be 0U. Need additional filtering
  // - skip invalidation of non-subject :has() rules
  //    - .e:has(.f) .g
  //    - .e:has(.h) .i
  //    - .e:has(.f.h) .j
  // - skip invalidation of the irrelevant ancestor
  //    - .a:has(.b)
  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kClassAttr, AtomicString("d"));
  UpdateAllLifecyclePhases();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  EXPECT_EQ(4U, element_count);

  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kClassAttr, g_empty_atom);
  UpdateAllLifecyclePhases();

  // TODO(blee@igalia.com) Should be 1U. Need additional filtering
  // - skip invalidation of subject :has() rules
  //    - .a:has(.b)
  // - skip invalidation of irrelevant rules
  //    - .e:has(.h) .i
  // - skip invalidation of the mutation on irrelevant element
  //    - .e:has(.f.h) .j
  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("child"))
      ->setAttribute(html_names::kClassAttr, AtomicString("b"));
  UpdateAllLifecyclePhases();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  EXPECT_EQ(4U, element_count);
}

TEST_F(StyleEngineTest, CSSComparisonFunctionsUseCount) {
  ClearUseCounter(WebFeature::kCSSComparisonFunctions);
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      div { width: calc(10px + 20%); }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSComparisonFunctions));
  ClearUseCounter(WebFeature::kCSSComparisonFunctions);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      div { width: calc(min(10px, 20%) + max(20px, 10%)); }
    </style>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSComparisonFunctions));
  ClearUseCounter(WebFeature::kCSSComparisonFunctions);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      div { width: calc(clamp(10px, 20px, 30px)); }
    </style>
    <div></div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSComparisonFunctions));
  ClearUseCounter(WebFeature::kCSSComparisonFunctions);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      div { width: calc(clamp(10px, 20%, 20px + 30%)); }
    </style>
    <div></div>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSComparisonFunctions));
  ClearUseCounter(WebFeature::kCSSComparisonFunctions);
}

TEST_F(StyleEngineTest, MathDepthOverflow) {
  css_test_helpers::RegisterProperty(
      GetDocument(), "--int16-max", "<integer>",
      String::Format("%i", std::numeric_limits<int16_t>::max()), false);

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #parent1 {
        math-style: compact;
        math-depth: var(--int16-max);
      }
      #parent2 {
        math-style: compact;
        math-depth: 1;
      }
      #child1, #control1 {
        math-depth: add(1);
      }
      #child2, #control2 {
        math-depth: auto-add;
      }
      #child3 {
        math-depth: calc(var(--int16-max) + 1);
      }
    </style>
    <div id=parent1>
      <div id=child1></div>
      <div id=child2></div>
      <div id=child3></div>
    </div>
    <div id=parent2>
      <div id=control1></div>
      <div id=control2></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhases();

  Element* control1 = GetDocument().getElementById(AtomicString("control1"));
  Element* control2 = GetDocument().getElementById(AtomicString("control2"));

  ASSERT_TRUE(control1 && control1->GetComputedStyle());
  ASSERT_TRUE(control2 && control2->GetComputedStyle());

  EXPECT_EQ(2, control1->GetComputedStyle()->MathDepth());
  EXPECT_EQ(2, control2->GetComputedStyle()->MathDepth());

  Element* child1 = GetDocument().getElementById(AtomicString("child1"));
  Element* child2 = GetDocument().getElementById(AtomicString("child2"));
  Element* child3 = GetDocument().getElementById(AtomicString("child3"));

  ASSERT_TRUE(child1 && child1->GetComputedStyle());
  ASSERT_TRUE(child2 && child2->GetComputedStyle());
  ASSERT_TRUE(child3 && child3->GetComputedStyle());

  EXPECT_EQ(std::numeric_limits<int16_t>::max(),
            child1->GetComputedStyle()->MathDepth());
  EXPECT_EQ(std::numeric_limits<int16_t>::max(),
            child2->GetComputedStyle()->MathDepth());
  EXPECT_EQ(std::numeric_limits<int16_t>::max(),
            child3->GetComputedStyle()->MathDepth());
}

TEST_F(StyleEngineTest, RemovedBodyToHTMLPropagation) {
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kWritingMode,
                                               "vertical-lr");

  UpdateAllLifecyclePhases();

  Element* root = GetDocument().documentElement();
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->ComputedStyleRef().IsHorizontalWritingMode())
      << "body to html propagation does not affect computed value";
  EXPECT_FALSE(root->GetLayoutObject()->StyleRef().IsHorizontalWritingMode())
      << "body to html propagation affects used value";

  // Make sure that recalculating style for the root element does not trigger a
  // visual diff that requires layout. That is, we take the body -> root
  // propagation of writing-mode into account before setting ComputedStyle on
  // the root LayoutObject.
  GetDocument().body()->remove();

  UpdateAllLifecyclePhases();
  EXPECT_TRUE(root->ComputedStyleRef().IsHorizontalWritingMode())
      << "body to html propagation does not affect computed value";
  EXPECT_TRUE(root->GetLayoutObject()->StyleRef().IsHorizontalWritingMode())
      << "No propagation from removed body";
}

TEST_F(StyleEngineTest, RevertWithPresentationalHints) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      img {
        width: revert;
        height: revert;
      }
    </style>
    <img id="img" width="44" height="33"></img>
  )HTML");
  UpdateAllLifecyclePhases();

  // For the purpose of the 'revert' keyword, presentational hints are
  // considered part of the author origin.
  Element* img = GetElementById("img");
  EXPECT_NE(44, img->OffsetWidth());
  EXPECT_NE(33, img->OffsetHeight());
}

TEST_F(StyleEngineTest, RevertLayerWithPresentationalHints) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      img {
        width: revert-layer;
        height: revert-layer;
      }
    </style>
    <img id="img" width="44" height="33"></img>
  )HTML");
  UpdateAllLifecyclePhases();

  // 'revert-layer' from the lowest author layer should revert to the
  // presentational hints.
  Element* img = GetElementById("img");
  EXPECT_EQ(44, img->OffsetWidth());
  EXPECT_EQ(33, img->OffsetHeight());
}

TEST_F(StyleEngineSimTest, ResizeWithBlockingSheetTransition) {
  WebView().MainFrameWidget()->Resize(gfx::Size(500, 500));

  SimRequest html_request("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_request("https://example.com/slow.css", "text/css");

  LoadURL("https://example.com/test.html");
  html_request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        #trans {
          transition-duration: 30s;
          color: red;
        }
      </style>
      <link rel="stylesheet" href="slow.css">
      <div id="trans"></div>
  )HTML");

  css_request.Start();
  WebView().MainFrameWidget()->Resize(gfx::Size(800, 800));

  css_request.Complete(R"CSS(
    #trans { color: green; }
  )CSS");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  Element* trans = GetDocument().getElementById(AtomicString("trans"));
  ASSERT_TRUE(trans);

  // Completing the linked stylesheet should not start a transition since the
  // sheet is render-blocking.
  EXPECT_EQ(
      trans->ComputedStyleRef().VisitedDependentColor(GetCSSPropertyColor()),
      Color::FromRGB(0, 128, 0));
}

TEST_F(StyleEngineSimTest, FocusWithBlockingSheetTransition) {
  WebView().MainFrameWidget()->Resize(gfx::Size(500, 500));

  SimRequest html_request("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_request("https://example.com/slow.css", "text/css");

  LoadURL("https://example.com/test.html");
  html_request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        #trans {
          transition-duration: 30s;
          color: red;
        }
      </style>
      <link rel="stylesheet" href="slow.css">
      <div id="trans"></div>
  )HTML");

  css_request.Start();

  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  GetDocument().GetPage()->GetFocusController().SetFocusedFrame(
      GetDocument().GetFrame());

  css_request.Complete(R"CSS(
    #trans { color: green; }
  )CSS");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  Element* trans = GetDocument().getElementById(AtomicString("trans"));
  ASSERT_TRUE(trans);

  // Completing the linked stylesheet should not start a transition since the
  // sheet is render-blocking.
  EXPECT_EQ(
      trans->ComputedStyleRef().VisitedDependentColor(GetCSSPropertyColor()),
      Color::FromRGB(0, 128, 0));
}

TEST_F(StyleEngineSimTest,
       ShouldInvalidateSubjectPseudoHasAfterChildrenParsingFinished) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Write(R"HTML(
    <!DOCTYPE html>
    <style>
      .a { color: black }
      .a:not(:has(+ div)) { color: red }
    </style>
    <div id="first" class="a"> First </div>
    <div id="second" class="a"> Second
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  Element* first = GetDocument().getElementById(AtomicString("first"));
  EXPECT_TRUE(first);
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      first->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  Element* second = GetDocument().getElementById(AtomicString("second"));
  EXPECT_TRUE(second);
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      second->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  main_resource.Write(R"HTML(
    </div>
    <div id="third" class="a"> Third
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  first = GetDocument().getElementById(AtomicString("first"));
  EXPECT_TRUE(first);
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      first->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  second = GetDocument().getElementById(AtomicString("second"));
  EXPECT_TRUE(second);
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      second->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  Element* third = GetDocument().getElementById(AtomicString("third"));
  EXPECT_TRUE(third);
  EXPECT_EQ(
      Color::FromRGB(255, 0, 0),
      third->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  main_resource.Complete(R"HTML(
    </div>
    <div id="fourth"> Fourth </div>
  )HTML");

  first = GetDocument().getElementById(AtomicString("first"));
  EXPECT_TRUE(first);
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      first->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  second = GetDocument().getElementById(AtomicString("second"));
  EXPECT_TRUE(second);
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      second->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  third = GetDocument().getElementById(AtomicString("third"));
  EXPECT_TRUE(third);
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      third->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  Element* fourth = GetDocument().getElementById(AtomicString("fourth"));
  EXPECT_TRUE(fourth);
  EXPECT_EQ(
      Color::FromRGB(0, 0, 0),
      fourth->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, StyleElementTypeAttrChange) {
  Element* style = GetDocument().CreateElementForBinding(AtomicString("style"));
  style->setAttribute(html_names::kTypeAttr, AtomicString("invalid"));
  style->setInnerHTML("body { color: red }");
  GetDocument().body()->appendChild(style);

  // <style> has no effect due to invalid type attribute value
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(0, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  // <style> should now be effective with a valid type attribute value
  style->setAttribute(html_names::kTypeAttr, AtomicString("text/css"));
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color::FromRGB(255, 0, 0),
            GetDocument().body()->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(StyleEngineTest, SVGURIValueCacheClipPath) {
  Element* body = GetDocument().body();
  body->setInnerHTML(R"HTML(
    <svg><text clip-path="inset(10px)">CLIPPED</text><svg>
  )HTML");
  UpdateAllLifecyclePhases();

  EXPECT_EQ(FillOrClipPathCacheSize(), 0u);

  body->setInnerHTML(R"HTML(
    <svg><text clip-path="url(#clipped)">CLIPPED</text><svg>
  )HTML");
  UpdateAllLifecyclePhases();

  EXPECT_EQ(FillOrClipPathCacheSize(), 1u);
}

TEST_F(StyleEngineTest, SVGURIValueCacheFill) {
  Element* body = GetDocument().body();
  body->setInnerHTML(R"HTML(
    <svg><rect fill="red">FILLED</rect><svg>
  )HTML");
  UpdateAllLifecyclePhases();

  EXPECT_EQ(FillOrClipPathCacheSize(), 0u);

  body->setInnerHTML(R"HTML(
    <svg><rect fill="url(#fill)">FILLED</rect><svg>
  )HTML");
  UpdateAllLifecyclePhases();

  EXPECT_EQ(FillOrClipPathCacheSize(), 1u);
}

TEST_F(StyleEngineTest, BorderWidthsAreRecalculatedWhenZoomChanges) {
  // Tests that Border Widths are recalculated as expected
  // when Zoom and Device Scale Factor are changed.

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.Initialize();

  WebFrameWidget* mainFrameWidget = web_view_impl->MainFrameWidget();

  const auto setZoom{[&](const float zoomFactor) {
    mainFrameWidget->SetZoomLevelForTesting(ZoomFactorToZoomLevel(zoomFactor));

    mainFrameWidget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  }};

  auto resetZoom{[&]() { setZoom(1.0f); }};

  const auto setDeviceScaleFactor{[&](const float deviceScaleFactor) {
    mainFrameWidget->SetDeviceScaleFactorForTesting(deviceScaleFactor);

    mainFrameWidget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  }};

  auto resetDeviceScaleFactor{[&]() { setDeviceScaleFactor(1.0f); }};

  auto reset{[&]() {
    resetZoom();
    resetDeviceScaleFactor();
  }};

  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();

  document->body()->setInnerHTML(R"HTML(
    <style>
    #square {
      height: 100px;
      width: 100px;
      border: 1.5px solid gray;
    }
    </style>
    <div id='square'></div>
  )HTML");

  mainFrameWidget->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);

  const Element* square = document->getElementById(AtomicString("square"));
  ASSERT_NE(square, nullptr);

  const auto checkBorderWidth{[&](const float expected) {
    const ComputedStyle* computedStyle = square->GetComputedStyle();
    ASSERT_NE(computedStyle, nullptr);

    EXPECT_FLOAT_EQ(expected, computedStyle->BorderTopWidth());
  }};

  // Check initial border width.
  reset();
  checkBorderWidth(1.0f);

  // Check border width with zoom factors.
  setZoom(0.33f);
  checkBorderWidth(1.0f);

  setZoom(1.75f);
  checkBorderWidth(2.0f);

  setZoom(2.0f);
  checkBorderWidth(3.0f);

  // Check border width after zoom is reset.
  resetZoom();
  checkBorderWidth(1.0f);

  // Check border width with device scale factors.
  setDeviceScaleFactor(2.0f);
  checkBorderWidth(3.0f);

  setDeviceScaleFactor(3.0f);
  checkBorderWidth(4.0f);

  // Check border width after device scale factor is reset.
  resetDeviceScaleFactor();
  checkBorderWidth(1.0f);

  // Check border width with a combination
  // of zoom and device scale factors.
  setZoom(2.0f);
  setDeviceScaleFactor(2.0f);
  checkBorderWidth(6.0f);

  setZoom(1.5f);
  checkBorderWidth(4.0f);

  setDeviceScaleFactor(2.6f);
  checkBorderWidth(5.0f);

  setZoom(0.33f);
  checkBorderWidth(1.0f);

  // Check border width after resetting both
  // zoom and device scale factor is reset.
  reset();
  checkBorderWidth(1.0f);
}

TEST_F(StyleEngineTest, AnimationShorthandFlags) {
  String css = "animation: foo 1s";
  {
    ScopedScrollTimelineForTest scroll_timeline_enabled(false);
    ScopedScrollTimelineCurrentTimeForTest current_time_enabled(false);
    const CSSPropertyValueSet* set =
        css_test_helpers::ParseDeclarationBlock(css);
    ASSERT_TRUE(set);
    EXPECT_EQ(8u, set->PropertyCount());
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationDuration));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationTimingFunction));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationDelay));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationIterationCount));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationDirection));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationFillMode));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationPlayState));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationName));
  }
  {
    ScopedScrollTimelineForTest scroll_timeline_enabled(true);
    const CSSPropertyValueSet* set =
        css_test_helpers::ParseDeclarationBlock(css);
    ASSERT_TRUE(set);
    EXPECT_EQ(11u, set->PropertyCount());
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationDuration));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationTimingFunction));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationDelay));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationIterationCount));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationDirection));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationFillMode));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationPlayState));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationName));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationTimeline));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationRangeStart));
    EXPECT_TRUE(set->HasProperty(CSSPropertyID::kAnimationRangeEnd));
  }
}

TEST_F(StyleEngineTest, InitialStyle_Recalc) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #target {
        background-color: green;
        @starting-style { background-color: red; }
      }
    </style>
    <div id="target"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  constexpr Color green = Color::FromRGB(0, 128, 0);
  constexpr Color lime = Color::FromRGB(0, 255, 0);

  Element* target = GetDocument().getElementById(AtomicString("target"));
  unsigned before_count = GetStyleEngine().StyleForElementCount();

  target->SetInlineStyleProperty(CSSPropertyID::kColor, "lime");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetStyleEngine().StyleForElementCount() - before_count, 1u)
      << "The style recalc should not do a separate @starting-style pass since "
         "the element already has a style";
  EXPECT_EQ(target->ComputedStyleRef().VisitedDependentColor(
                GetCSSPropertyBackgroundColor()),
            green)
      << "Make sure @starting-style rules do not apply for the second pass";
  EXPECT_EQ(
      target->ComputedStyleRef().VisitedDependentColor(GetCSSPropertyColor()),
      lime)
      << "Check that the color changed to lime";
}

TEST_F(StyleEngineTest, InitialStyle_FromDisplayNone) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #target {
        background-color: green;
        @starting-style { background-color: red; }
      }
    </style>
    <div id="target" style="display:none"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  constexpr Color green = Color::FromRGB(0, 128, 0);

  Element* target = GetDocument().getElementById(AtomicString("target"));
  unsigned before_count = GetStyleEngine().StyleForElementCount();

  target->SetInlineStyleProperty(CSSPropertyID::kDisplay, "block");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetStyleEngine().StyleForElementCount() - before_count, 2u)
      << "The style recalc needs to do two passes because the element was "
         "display:none and @starting-style styles are matching";
  EXPECT_EQ(target->ComputedStyleRef().VisitedDependentColor(
                GetCSSPropertyBackgroundColor()),
            green)
      << "Make sure @starting-style do not apply for the second pass";
}

TEST_F(StyleEngineTest, InitialStyleCount_EnsureComputedStyle) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #target {
        background-color: green;
        transition: background-color 100s step-end;
        @starting-style { background-color: red; }
      }
    </style>
    <div id="target" style="display:none"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  constexpr Color green = Color::FromRGB(0, 128, 0);

  Element* target = GetDocument().getElementById(AtomicString("target"));
  unsigned before_count = GetStyleEngine().StyleForElementCount();

  ASSERT_FALSE(target->GetComputedStyle())
      << "Initially no ComputedStyle on display:none element";

  const ComputedStyle* none_style = target->EnsureComputedStyle();
  ASSERT_TRUE(none_style);

  EXPECT_EQ(GetStyleEngine().StyleForElementCount() - before_count, 1u)
      << "No @starting-style pass for EnsureComputedStyle";

  EXPECT_EQ(target->ComputedStyleRef().VisitedDependentColor(
                GetCSSPropertyBackgroundColor()),
            green)
      << "Transitions are not started and @starting-style does not apply in "
         "display:none";
}

TEST_F(StyleEngineTest, UseCountCSSAnchorPositioning) {
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSAnchorPositioning));

  SetBodyInnerHTML("<style>#foo { top: anchor(top); }");
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSAnchorPositioning));

  ClearUseCounter(WebFeature::kCSSAnchorPositioning);
  SetBodyInnerHTML("<style>#foo { width: anchor-size(width); }");
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSAnchorPositioning));

  ClearUseCounter(WebFeature::kCSSAnchorPositioning);
  SetBodyInnerHTML("<style>@position-try --pf {}</style>");
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSAnchorPositioning));
}

TEST_F(StyleEngineTest, EnsureAppRegionTriggersRelayout) {
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.Initialize();
  web_view_impl->SetSupportsDraggableRegions(true);
  web_view_impl->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();
  document->body()->setInnerHTML(R"HTML(
    <head>
    <style>
      .drag {
        app-region: drag
      }
      .no-drag {
        app-region: no-drag
      }
    </style>
    </head>
    <body>
       <div id="drag-region"></div>
    </body>
  )HTML");

  Element* drag_element = document->getElementById(AtomicString("drag-region"));

  auto regions = document->DraggableRegions();
  auto it =
      std::find_if(regions.begin(), regions.end(),
                   [](blink::DraggableRegionValue s) { return s.draggable; });
  EXPECT_EQ(it, regions.end()) << "There should be no drag regions";

  drag_element->classList().Add(AtomicString("drag"));
  web_view_impl->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  regions = document->DraggableRegions();
  it = std::find_if(regions.begin(), regions.end(),
                    [](blink::DraggableRegionValue s) { return s.draggable; });
  EXPECT_NE(it, regions.end()) << "There should be one drag region";

  drag_element->classList().Add(AtomicString("no-drag"));
  web_view_impl->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  regions = document->DraggableRegions();
  it = std::find_if(regions.begin(), regions.end(),
                    [](blink::DraggableRegionValue s) { return s.draggable; });

  EXPECT_EQ(it, regions.end()) << "There should be no drag regions";
}

TEST_F(StyleEngineTest, ForcedColorsLightDark) {
  ScopedForcedColorsForTest scoped_feature(true);
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetInForcedColors(GetDocument(),
                                        /*in_forced_colors=*/true);
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      :root { color-scheme: light only; }
      #light-dark {
        color: light-dark(GrayText, red);
      }
      #reference {
        color: GrayText;
      }
    </style>
    <div id="light-dark"></div>
    <div id="reference"></div>
  )HTML");
  UpdateAllLifecyclePhases();

  const ComputedStyle& light_dark =
      GetDocument()
          .getElementById(AtomicString("light-dark"))
          ->ComputedStyleRef();
  const ComputedStyle& reference =
      GetDocument()
          .getElementById(AtomicString("reference"))
          ->ComputedStyleRef();

  EXPECT_EQ(light_dark.VisitedDependentColor(GetCSSPropertyColor()),
            reference.VisitedDependentColor(GetCSSPropertyColor()));
}

}  // namespace blink
