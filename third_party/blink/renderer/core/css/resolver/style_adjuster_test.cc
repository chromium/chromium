// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "ui/base/ui_base_features.h"

namespace blink {

class StyleAdjusterTest : public RenderingTest {
 public:
  StyleAdjusterTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

TEST_F(StyleAdjusterTest, TouchActionPropagatedAcrossIframes) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; } iframe { display: block; } </style>
    <iframe id='owner' src='http://test.com' width='500' height='500'
    style='touch-action: none'>
    </iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>body { margin: 0; } #target { width: 200px; height: 200px; }
    </style>
    <div id='target' style='touch-action: pinch-zoom'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = ChildDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(TouchAction::kNone,
            target->GetComputedStyle()->EffectiveTouchAction());

  Element* owner = GetDocument().getElementById(AtomicString("owner"));
  owner->setAttribute(html_names::kStyleAttr,
                      AtomicString("touch-action: auto"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(TouchAction::kPinchZoom,
            target->GetComputedStyle()->EffectiveTouchAction());
}

TEST_F(StyleAdjusterTest, TouchActionPanningReEnabledByScrollers) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>#ancestor { margin: 0; touch-action: pinch-zoom; }
    #scroller { overflow: scroll; width: 100px; height: 100px; }
    #target { width: 200px; height: 200px; } </style>
    <div id='ancestor'><div id='scroller'><div id='target'>
    </div></div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(TouchAction::kManipulation | TouchAction::kInternalPanXScrolls |
                TouchAction::kInternalNotWritable,
            target->GetComputedStyle()->EffectiveTouchAction());
}

TEST_F(StyleAdjusterTest, TouchActionPropagatedWhenAncestorStyleChanges) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>#ancestor { margin: 0; touch-action: pan-x; }
    #potential-scroller { width: 100px; height: 100px; overflow: hidden; }
    #target { width: 200px; height: 200px; }</style>
    <div id='ancestor'><div id='potential-scroller'><div id='target'>
    </div></div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(TouchAction::kPanX | TouchAction::kInternalPanXScrolls |
                TouchAction::kInternalNotWritable,
            target->GetComputedStyle()->EffectiveTouchAction());

  Element* ancestor = GetDocument().getElementById(AtomicString("ancestor"));
  ancestor->setAttribute(html_names::kStyleAttr,
                         AtomicString("touch-action: pan-y"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(TouchAction::kPanY | TouchAction::kInternalNotWritable,
            target->GetComputedStyle()->EffectiveTouchAction());

  Element* potential_scroller =
      GetDocument().getElementById(AtomicString("potential-scroller"));
  potential_scroller->setAttribute(html_names::kStyleAttr,
                                   AtomicString("overflow: scroll"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(TouchAction::kPan | TouchAction::kInternalPanXScrolls |
                TouchAction::kInternalNotWritable,
            target->GetComputedStyle()->EffectiveTouchAction());
}

TEST_F(StyleAdjusterTest, TouchActionRestrictedByLowerAncestor) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor' style='touch-action: pan'>
    <div id='parent' style='touch-action: pan-right pan-y'>
    <div id='target' style='touch-action: pan-x'>
    </div></div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(TouchAction::kPanRight | TouchAction::kInternalPanXScrolls |
                TouchAction::kInternalNotWritable,
            target->GetComputedStyle()->EffectiveTouchAction());

  Element* parent = GetDocument().getElementById(AtomicString("parent"));
  parent->setAttribute(html_names::kStyleAttr,
                       AtomicString("touch-action: auto"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(TouchAction::kPanX | TouchAction::kInternalPanXScrolls |
                TouchAction::kInternalNotWritable,
            target->GetComputedStyle()->EffectiveTouchAction());
}

TEST_F(StyleAdjusterTest, TouchActionContentEditableArea) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({::features::kSwipeToMoveCursor}, {});
  if (!::features::IsSwipeToMoveCursorEnabled()) {
    return;
  }

  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <div id='editable1' contenteditable='false'></div>
    <input type="text" id='input1' disabled>
    <textarea id="textarea1" readonly></textarea>
    <div id='editable2' contenteditable='true'></div>
    <input type="text" id='input2'>
    <textarea id="textarea2"></textarea>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(TouchAction::kAuto, GetDocument()
                                    .getElementById(AtomicString("editable1"))
                                    ->GetComputedStyle()
                                    ->EffectiveTouchAction());
  EXPECT_EQ(TouchAction::kAuto, GetDocument()
                                    .getElementById(AtomicString("input1"))
                                    ->GetComputedStyle()
                                    ->EffectiveTouchAction());
  EXPECT_EQ(TouchAction::kAuto, GetDocument()
                                    .getElementById(AtomicString("textarea1"))
                                    ->GetComputedStyle()
                                    ->EffectiveTouchAction());
  EXPECT_EQ(TouchAction::kAuto & ~TouchAction::kInternalPanXScrolls,
            GetDocument()
                .getElementById(AtomicString("editable2"))
                ->GetComputedStyle()
                ->EffectiveTouchAction());
  EXPECT_EQ(TouchAction::kAuto & ~TouchAction::kInternalPanXScrolls,
            GetDocument()
                .getElementById(AtomicString("input2"))
                ->GetComputedStyle()
                ->EffectiveTouchAction());
  EXPECT_EQ(TouchAction::kAuto & ~TouchAction::kInternalPanXScrolls,
            GetDocument()
                .getElementById(AtomicString("textarea2"))
                ->GetComputedStyle()
                ->EffectiveTouchAction());

  Element* target = GetDocument().getElementById(AtomicString("editable1"));
  target->setAttribute(html_names::kContenteditableAttr, keywords::kTrue);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(TouchAction::kAuto & ~TouchAction::kInternalPanXScrolls,
            target->GetComputedStyle()->EffectiveTouchAction());
}

TEST_F(StyleAdjusterTest, TouchActionNoPanXScrollsWhenNoPanX) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({::features::kSwipeToMoveCursor}, {});
  if (!::features::IsSwipeToMoveCursorEnabled()) {
    return;
  }

  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <div id='target' contenteditable='false' style='touch-action: pan-y'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(TouchAction::kPanY | TouchAction::kInternalNotWritable,
            target->GetComputedStyle()->EffectiveTouchAction());

  target->setAttribute(html_names::kContenteditableAttr, keywords::kTrue);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(TouchAction::kPanY | TouchAction::kInternalNotWritable,
            target->GetComputedStyle()->EffectiveTouchAction());
}

TEST_F(StyleAdjusterTest, TouchActionNotWritableReEnabledByScrollers) {
  base::test::ScopedFeatureList feature_list;
  ScopedStylusHandwritingForTest stylus_handwriting(true);

  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>#ancestor { margin: 0; touch-action: none; }
    #scroller { overflow: auto; width: 100px; height: 100px; }
    #target { width: 200px; height: 200px; } </style>
    <div id='ancestor'><div id='scroller'><div id='target'>
    </div></div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_TRUE((target->GetComputedStyle()->EffectiveTouchAction() &
               TouchAction::kInternalNotWritable) != TouchAction::kNone);
}

TEST_F(StyleAdjusterTest, TouchActionWritableArea) {
  base::test::ScopedFeatureList feature_list;
  ScopedStylusHandwritingForTest stylus_handwriting(true);

  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <div id='editable1' contenteditable='false'></div>
    <input type="text" id='input1' disabled>
    <input type="password" id='password1' disabled>
    <textarea id="textarea1" readonly></textarea>
    <div id='editable2' contenteditable='true'></div>
    <input type="text" id='input2'>
    <input type="password" id='password2'>
    <textarea id="textarea2"></textarea>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(TouchAction::kAuto, GetDocument()
                                    .getElementById(AtomicString("editable1"))
                                    ->GetComputedStyle()
                                    ->EffectiveTouchAction());
  EXPECT_EQ(TouchAction::kAuto, GetDocument()
                                    .getElementById(AtomicString("input1"))
                                    ->GetComputedStyle()
                                    ->EffectiveTouchAction());
  EXPECT_EQ(TouchAction::kAuto, GetDocument()
                                    .getElementById(AtomicString("password1"))
                                    ->GetComputedStyle()
                                    ->EffectiveTouchAction());
  EXPECT_EQ(TouchAction::kAuto, GetDocument()
                                    .getElementById(AtomicString("textarea1"))
                                    ->GetComputedStyle()
                                    ->EffectiveTouchAction());

  TouchAction expected_input_action =
      (TouchAction::kAuto & ~TouchAction::kInternalNotWritable);
  TouchAction expected_pwd_action = TouchAction::kAuto;
  if (::features::IsSwipeToMoveCursorEnabled()) {
    expected_input_action &= ~TouchAction::kInternalPanXScrolls;
    expected_pwd_action &= ~TouchAction::kInternalPanXScrolls;
  }

  EXPECT_EQ(expected_input_action,
            GetDocument()
                .getElementById(AtomicString("editable2"))
                ->GetComputedStyle()
                ->EffectiveTouchAction());
  EXPECT_EQ(expected_input_action, GetDocument()
                                       .getElementById(AtomicString("input2"))
                                       ->GetComputedStyle()
                                       ->EffectiveTouchAction());
  EXPECT_EQ(expected_pwd_action, GetDocument()
                                     .getElementById(AtomicString("password2"))
                                     ->GetComputedStyle()
                                     ->EffectiveTouchAction());
  EXPECT_EQ(expected_input_action,
            GetDocument()
                .getElementById(AtomicString("textarea2"))
                ->GetComputedStyle()
                ->EffectiveTouchAction());

  Element* target = GetDocument().getElementById(AtomicString("editable1"));
  target->setAttribute(html_names::kContenteditableAttr, keywords::kTrue);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(expected_input_action,
            target->GetComputedStyle()->EffectiveTouchAction());
}

TEST_F(StyleAdjusterTest, OverflowClipUseCount) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <div></div>
    <div style='overflow: hidden'></div>
    <div style='overflow: scroll'></div>
    <div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kOverflowClipAlongEitherAxis));

  SetBodyInnerHTML(R"HTML(
    <div style='overflow: clip'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kOverflowClipAlongEitherAxis));
}

TEST_F(StyleAdjusterTest, SingleAxisScrollerUseCount) {
  SetBodyInnerHTML(R"HTML(
    <div style='overflow: clip'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSingleAxisScroller));
  GetDocument().ClearUseCounterForTesting(WebFeature::kSingleAxisScroller);

  SetBodyInnerHTML(R"HTML(
    <div style='overflow-x: auto; overflow-y: visible'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSingleAxisScroller));
  GetDocument().ClearUseCounterForTesting(WebFeature::kSingleAxisScroller);

  SetBodyInnerHTML(R"HTML(
    <div style='overflow-x: clip; overflow-y: hidden'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSingleAxisScroller));
  GetDocument().ClearUseCounterForTesting(WebFeature::kSingleAxisScroller);

  SetBodyInnerHTML(R"HTML(
    <div style='overflow-x: auto; overflow-y: clip'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSingleAxisScroller));
  GetDocument().ClearUseCounterForTesting(WebFeature::kSingleAxisScroller);
}

TEST_F(StyleAdjusterTest, SingleAxisScrollerOverscrollBehaviorUseCount) {
  SetBodyInnerHTML(R"HTML(
    <div style='overflow-x: clip; overflow-y: auto;
                overscroll-behavior-y: contain'></div>
    <div style='overflow-x: scroll; overflow-y: clip;
                overscroll-behavior-x: contain'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kSingleAxisScrollerOverscrollBehavior));

  SetBodyInnerHTML(R"HTML(
    <div style='overflow-x: clip; overflow-y: auto;
                overscroll-behavior-x: none'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kSingleAxisScrollerOverscrollBehavior));
  GetDocument().ClearUseCounterForTesting(
      WebFeature::kSingleAxisScrollerOverscrollBehavior);

  SetBodyInnerHTML(R"HTML(
    <div style='overflow-x: scroll; overflow-y: clip;
                overscroll-behavior-y: contain'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kSingleAxisScrollerOverscrollBehavior));
}

// crbug.com/392643253
TEST_F(StyleAdjusterTest, AdjustForDisplayInlinify) {
  SetBodyInnerHTML(R"HTML(<ruby><video></video><audio></audio></ruby>)HTML");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

// crbug.com/1216721
TEST_F(StyleAdjusterTest, AdjustForSVGCrash) {
  SetBodyInnerHTML(R"HTML(
<style>
.class1 { dominant-baseline: hanging; }
</style>
<svg>
<tref>
<text id="text5" style="dominant-baseline: no-change;"/>
</svg>
<svg>
<use id="use1" xlink:href="#text5" class="class1" />
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* text = GetDocument()
                      .getElementById(AtomicString("use1"))
                      ->GetShadowRoot()
                      ->getElementById(AtomicString("text5"));
  EXPECT_EQ(EDominantBaseline::kHanging,
            text->GetComputedStyle()->CssDominantBaseline());
}

TEST_F(StyleAdjusterTest, IncrementalStyleBlocker) {
  SetBodyInnerHTML(R"HTML(<div></div>)HTML");
  UpdateAllLifecyclePhasesForTest();

  const ComputedStyle& initial =
      GetDocument().GetStyleResolver().InitialStyle();
  ComputedStyleBuilder metadata_only(initial);
  metadata_only.SetHasIncrementalStyleBlocker(true);
  const ComputedStyle* blocked = metadata_only.TakeStyle();
  EXPECT_TRUE(initial == *blocked);
  ComputedStyleBuilder cloned(*blocked);
  EXPECT_TRUE(cloned.TakeStyle()->HasIncrementalStyleBlocker());
  ComputedStyleBuilder inherited(initial, *blocked);
  EXPECT_FALSE(inherited.TakeStyle()->HasIncrementalStyleBlocker());
}

TEST_F(StyleAdjusterTest, RepeatedSVGAdjustment) {
  ScopedSvgIncrementalStyleForTest incremental_style(true);
  SetBodyInnerHTML(R"HTML(
    <style>
      #outer { display: block; position: relative; }
      #absolute { position: absolute; }
      #relative { position: relative; }
      #foreign, #text { display: inline-block; }
      #inline { display: inline; }
      #contents { display: contents; }
      .appearance { appearance: button; }
      #unchanged, #appearance_unchanged { display: block; }
    </style>
    <svg id="outer"><g style="text-decoration: underline">
      <rect id="absolute"/>
      <rect id="relative"/>
      <text id="text">Text</text>
      <foreignObject id="foreign"/>
      <foreignObject id="inline"/>
      <rect id="contents"/>
      <rect id="appearance" class="appearance"/>
      <rect id="appearance_cached" class="appearance"/>
      <rect id="appearance_unchanged" class="appearance"/>
      <rect id="unchanged"/>
    </g></svg>
    <div id="html" style="position: absolute; display: inline-block"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const bool appearance_adjusts_display =
      !RuntimeEnabledFeatures::
          AppearanceDisplayAdjustmentForWidgetsOnlyEnabled();
  const struct {
    const char* id;
    EDisplay display;
    EPosition position;
    bool non_idempotent;
    AppearanceValue appearance = AppearanceValue::kNone;
  } cases[] = {
      {"absolute", EDisplay::kInline, EPosition::kAbsolute, false},
      {"relative", EDisplay::kInline, EPosition::kRelative, false},
      {"foreign", EDisplay::kInlineBlock, EPosition::kStatic, false},
      {"text", EDisplay::kInlineBlock, EPosition::kStatic, false},
      {"inline", EDisplay::kInline, EPosition::kStatic, false},
      {"contents", EDisplay::kContents, EPosition::kStatic, false},
      {"appearance", EDisplay::kInline, EPosition::kStatic,
       appearance_adjusts_display, AppearanceValue::kButton},
      {"appearance_cached", EDisplay::kInline, EPosition::kStatic,
       appearance_adjusts_display, AppearanceValue::kButton},
      {"appearance_unchanged", EDisplay::kBlock, EPosition::kStatic, false,
       AppearanceValue::kButton},
      {"unchanged", EDisplay::kBlock, EPosition::kStatic, false},
      {"outer", EDisplay::kBlock, EPosition::kRelative, false},
      {"html", EDisplay::kInlineBlock, EPosition::kAbsolute, false},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.id);
    Element* element = GetElementById(test.id);
    const ComputedStyle* original = element->EnsureComputedStyle();
    ASSERT_TRUE(original);
    EXPECT_FALSE(original->HasIncrementalStyleBlocker());

    StyleResolverState state(GetDocument(), *element);
    state.CreateNewClonedStyle(*original);
    StyleAdjuster::AdjustComputedStyle(state, element);
    EXPECT_EQ(test.non_idempotent, !(*original == *state.CloneStyle()));
    EXPECT_EQ(test.non_idempotent,
              original->BaseTextDecorationData() !=
                  state.StyleBuilder().BaseTextDecorationData());

    // Restore the discarded inputs before repeating adjustment.
    state.CreateNewClonedStyle(*original);
    state.StyleBuilder().SetDisplay(test.display);
    state.StyleBuilder().SetPosition(test.position);
    StyleAdjuster::AdjustComputedStyle(state, element);
    if (test.appearance != AppearanceValue::kNone) {
      LayoutTheme::GetTheme().AdjustStyle(*element, state.StyleBuilder());
    }
    EXPECT_TRUE(*original == *state.CloneStyle());
  }
}

TEST_F(StyleAdjusterTest, AdjustForCanvasDrawableDescendant) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);
  SetBodyInnerHTML(R"HTML(
    <style>
      div { width: 100px; height: 100px; }
    </style>
    <canvas id="canvas" width="300" height="300" content=drawable>
      <div id="a">
        <div id="aa" drawable style="background: red;">
          <div id="aaa">a1</div>
          <div id="aab" drawable style="background: green;">
            <div id="aaba">a2</div>
          </div>
          <div id="aac">a3</div>
          <span id="nested_span" drawable>nested</span>
        </div>
        <div id="ab">b1</div>
      </div>
      <div id="b" drawable style="background: blue;">
        <div id="ba" drawable></div>
      </div>
      <span id="immediate_span" drawable>immediate</span>
    </canvas>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetLayoutObjectByElementId("a")->IsStackingContext());
  EXPECT_TRUE(GetLayoutObjectByElementId("aa")->IsStackingContext());
  EXPECT_FALSE(GetLayoutObjectByElementId("aaa")->IsStackingContext());
  EXPECT_TRUE(GetLayoutObjectByElementId("aab")->IsStackingContext());
  EXPECT_FALSE(GetLayoutObjectByElementId("aaba")->IsStackingContext());
  EXPECT_FALSE(GetLayoutObjectByElementId("aac")->IsStackingContext());
  EXPECT_FALSE(GetLayoutObjectByElementId("ab")->IsStackingContext());
  EXPECT_TRUE(GetLayoutObjectByElementId("b")->IsStackingContext());
  EXPECT_TRUE(GetLayoutObjectByElementId("ba")->IsStackingContext());
  EXPECT_EQ(EDisplay::kBlock,
            GetLayoutObjectByElementId("immediate_span")->StyleRef().Display());
  EXPECT_TRUE(
      GetLayoutObjectByElementId("immediate_span")->IsStackingContext());
  EXPECT_TRUE(GetLayoutObjectByElementId("immediate_span")
                  ->CanContainFixedPositionObjects());
  EXPECT_EQ(EDisplay::kInline,
            GetLayoutObjectByElementId("nested_span")->StyleRef().Display());
  EXPECT_TRUE(GetLayoutObjectByElementId("nested_span")->IsStackingContext());
  EXPECT_TRUE(GetLayoutObjectByElementId("nested_span")
                  ->CanContainFixedPositionObjects());
  EXPECT_TRUE(
      GetLayoutObjectByElementId("a")->CanContainFixedPositionObjects());
  EXPECT_TRUE(
      GetLayoutObjectByElementId("aa")->CanContainFixedPositionObjects());
  EXPECT_FALSE(
      GetLayoutObjectByElementId("aaa")->CanContainFixedPositionObjects());

  EXPECT_EQ(EIsolation::kAuto,
            GetLayoutObjectByElementId("a")->StyleRef().Isolation());
  EXPECT_EQ(EIsolation::kIsolate,
            GetLayoutObjectByElementId("aa")->StyleRef().Isolation());
  EXPECT_EQ(EIsolation::kAuto,
            GetLayoutObjectByElementId("aaa")->StyleRef().Isolation());
  EXPECT_EQ(EIsolation::kIsolate,
            GetLayoutObjectByElementId("aab")->StyleRef().Isolation());
  EXPECT_EQ(EIsolation::kAuto,
            GetLayoutObjectByElementId("aaba")->StyleRef().Isolation());
  EXPECT_EQ(EIsolation::kAuto,
            GetLayoutObjectByElementId("aac")->StyleRef().Isolation());
  EXPECT_EQ(EIsolation::kAuto,
            GetLayoutObjectByElementId("ab")->StyleRef().Isolation());
  EXPECT_EQ(EIsolation::kIsolate,
            GetLayoutObjectByElementId("b")->StyleRef().Isolation());
  EXPECT_EQ(EIsolation::kIsolate,
            GetLayoutObjectByElementId("ba")->StyleRef().Isolation());
  EXPECT_EQ(
      EIsolation::kIsolate,
      GetLayoutObjectByElementId("immediate_span")->StyleRef().Isolation());
  EXPECT_EQ(EIsolation::kIsolate,
            GetLayoutObjectByElementId("nested_span")->StyleRef().Isolation());

  EXPECT_EQ(kContainsNone,
            GetLayoutObjectByElementId("aa")->StyleRef().Contain());
  EXPECT_EQ(kContainsNone,
            GetLayoutObjectByElementId("immediate_span")->StyleRef().Contain());
  EXPECT_EQ(kContainsNone,
            GetLayoutObjectByElementId("nested_span")->StyleRef().Contain());
}

}  // namespace blink
