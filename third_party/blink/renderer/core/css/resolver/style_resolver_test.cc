// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/web/web_print_page_description.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/cascade_layer_map.h"
#include "third_party/blink/renderer/core/css/css_flip_revert_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/out_of_flow_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/post_style_update_scope.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using animation_test_helpers::CreateSimpleKeyframeEffectForTest;

class StyleResolverTest : public PageTestBase {
 protected:
  const ComputedStyle* StyleForId(
      const char* id,
      StyleRecalcContext style_recalc_context = {}) {
    Element* element = GetElementById(id);
    style_recalc_context.old_style = element->GetComputedStyle();
    const auto* style = GetStyleEngine().GetStyleResolver().ResolveStyle(
        element, style_recalc_context);
    DCHECK(style);
    return style;
  }

  String ComputedValue(String name, const ComputedStyle& style) {
    CSSPropertyRef ref(name, GetDocument());
    DCHECK(ref.IsValid());
    return ref.GetProperty()
        .CSSValueFromComputedStyle(style, nullptr, false,
                                   CSSValuePhase::kComputedValue)
        ->CssText();
  }

  void MatchAllRules(StyleResolverState& state,
                     ElementRuleCollector& collector) {
    GetDocument().GetStyleEngine().GetStyleResolver().MatchAllRules(
        state, collector, false /* include_smil_properties */);
  }

  bool IsUseCounted(mojom::WebFeature feature) {
    return GetDocument().IsUseCounted(feature);
  }

  // Access protected inset and sizing property getters
  const Length& GetTop(const ComputedStyle& style) const { return style.Top(); }
  const Length& GetBottom(const ComputedStyle& style) const {
    return style.Bottom();
  }
  const Length& GetLeft(const ComputedStyle& style) const {
    return style.Left();
  }
  const Length& GetRight(const ComputedStyle& style) const {
    return style.Right();
  }
  const Length& GetWidth(const ComputedStyle& style) const {
    return style.Width();
  }
  const Length& GetMinWidth(const ComputedStyle& style) const {
    return style.MinWidth();
  }
  const Length& GetMaxWidth(const ComputedStyle& style) const {
    return style.MaxWidth();
  }
  const Length& GetHeight(const ComputedStyle& style) const {
    return style.Height();
  }
  const Length& GetMinHeight(const ComputedStyle& style) const {
    return style.MinHeight();
  }
  const Length& GetMaxHeight(const ComputedStyle& style) const {
    return style.MaxHeight();
  }

  void UpdateStyleForOutOfFlow(Element& element, AtomicString try_name) {
    ScopedCSSName* scoped_name =
        MakeGarbageCollected<ScopedCSSName>(try_name, &GetDocument());
    StyleRulePositionTry* rule =
        GetStyleEngine().GetPositionTryRule(*scoped_name);
    CHECK(rule);
    GetStyleEngine().UpdateStyleForOutOfFlow(
        element, /* try_set */ &rule->Properties(), kNoTryTactics,
        /* anchor_evaluator */ nullptr);
  }

  size_t GetCurrentOldStylesCount() {
    return PostStyleUpdateScope::CurrentAnimationData()->old_styles_.size();
  }
};

class StyleResolverTestCQ : public StyleResolverTest {
 protected:
  StyleResolverTestCQ() = default;
};

TEST_F(StyleResolverTest, StyleForTextInDisplayNone) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <body style="display:none">Text</body>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  GetDocument().body()->EnsureComputedStyle();

  ASSERT_TRUE(GetDocument().body()->GetComputedStyle());
  EXPECT_TRUE(
      GetDocument().body()->GetComputedStyle()->IsEnsuredInDisplayNone());
  EXPECT_FALSE(GetStyleEngine().GetStyleResolver().StyleForText(
      To<Text>(GetDocument().body()->firstChild())));
}

TEST_F(StyleResolverTest, AnimationBaseComputedStyle) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      html { font-size: 10px; }
      body { font-size: 20px; }
      @keyframes fade { to { opacity: 0; }}
      #div { animation: fade 1s; }
    </style>
    <div id="div">Test</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById(AtomicString("div"));
  ElementAnimations& animations = div->EnsureElementAnimations();
  animations.SetAnimationStyleChange(true);

  StyleResolver& resolver = GetStyleEngine().GetStyleResolver();
  StyleRecalcContext recalc_context;
  recalc_context.old_style = div->GetComputedStyle();
  const auto* style1 = resolver.ResolveStyle(div, recalc_context);
  ASSERT_TRUE(style1);
  EXPECT_EQ(20, style1->FontSize());
  ASSERT_TRUE(style1->GetBaseComputedStyle());
  EXPECT_EQ(20, style1->GetBaseComputedStyle()->FontSize());

  // Getting style with customized parent style should not affect previously
  // produced animation base computed style.
  const ComputedStyle* parent_style =
      GetDocument().documentElement()->GetComputedStyle();
  StyleRequest style_request;
  style_request.parent_override = parent_style;
  style_request.layout_parent_override = parent_style;
  style_request.can_trigger_animations = false;
  EXPECT_EQ(
      10,
      resolver.ResolveStyle(div, recalc_context, style_request)->FontSize());
  ASSERT_TRUE(style1->GetBaseComputedStyle());
  EXPECT_EQ(20, style1->GetBaseComputedStyle()->FontSize());
  EXPECT_EQ(20, resolver.ResolveStyle(div, recalc_context)->FontSize());
}

TEST_F(StyleResolverTest, HasEmUnits) {
  GetDocument().documentElement()->setInnerHTML("<div id=div>Test</div>");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(StyleForId("div")->HasEmUnits());

  GetDocument().documentElement()->setInnerHTML(
      "<div id=div style='width:1em'>Test</div>");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(StyleForId("div")->HasEmUnits());
}

TEST_F(StyleResolverTest, BaseReusableIfFontRelativeUnitsAbsent) {
  GetDocument().documentElement()->setInnerHTML("<div id=div>Test</div>");
  UpdateAllLifecyclePhasesForTest();
  Element* div = GetDocument().getElementById(AtomicString("div"));

  auto* effect = CreateSimpleKeyframeEffectForTest(
      div, CSSPropertyID::kFontSize, "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ("50px", ComputedValue("font-size", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  StyleForId("div");

  StyleResolverState state(GetDocument(), *div);
  EXPECT_TRUE(StyleResolver::CanReuseBaseComputedStyle(state));
}

TEST_F(StyleResolverTest, AnimationNotMaskedByImportant) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      div {
        width: 10px;
        height: 10px !important;
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* div = GetDocument().getElementById(AtomicString("div"));

  auto* effect = CreateSimpleKeyframeEffectForTest(div, CSSPropertyID::kWidth,
                                                   "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ("50px", ComputedValue("width", *StyleForId("div")));
  EXPECT_EQ("10px", ComputedValue("height", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  const auto* style = StyleForId("div");

  const CSSBitset* bitset = style->GetBaseImportantSet();
  EXPECT_FALSE(CSSAnimations::IsAnimatingStandardProperties(
      div->GetElementAnimations(), bitset, KeyframeEffect::kDefaultPriority));
  EXPECT_TRUE(style->GetBaseComputedStyle());
  EXPECT_FALSE(bitset && bitset->Has(CSSPropertyID::kWidth));
  EXPECT_TRUE(bitset && bitset->Has(CSSPropertyID::kHeight));
}

TEST_F(StyleResolverTest, AnimationNotMaskedWithoutElementAnimations) {
  EXPECT_FALSE(CSSAnimations::IsAnimatingStandardProperties(
      /* ElementAnimations */ nullptr, std::make_unique<CSSBitset>().get(),
      KeyframeEffect::kDefaultPriority));
}

TEST_F(StyleResolverTest, AnimationNotMaskedWithoutBitset) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      div {
        width: 10px;
        height: 10px !important;
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* div = GetDocument().getElementById(AtomicString("div"));

  auto* effect = CreateSimpleKeyframeEffectForTest(div, CSSPropertyID::kWidth,
                                                   "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ("50px", ComputedValue("width", *StyleForId("div")));
  EXPECT_EQ("10px", ComputedValue("height", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  StyleForId("div");

  ASSERT_TRUE(div->GetElementAnimations());
  EXPECT_FALSE(CSSAnimations::IsAnimatingStandardProperties(
      div->GetElementAnimations(), /* CSSBitset */ nullptr,
      KeyframeEffect::kDefaultPriority));
}

TEST_F(StyleResolverTest, AnimationMaskedByImportant) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      div {
        width: 10px;
        height: 10px !important;
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* div = GetDocument().getElementById(AtomicString("div"));

  auto* effect = CreateSimpleKeyframeEffectForTest(div, CSSPropertyID::kHeight,
                                                   "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ("10px", ComputedValue("width", *StyleForId("div")));
  EXPECT_EQ("10px", ComputedValue("height", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  const auto* style = StyleForId("div");

  EXPECT_TRUE(style->GetBaseComputedStyle());
  EXPECT_TRUE(style->GetBaseImportantSet());

  StyleResolverState state(GetDocument(), *div);
  EXPECT_FALSE(StyleResolver::CanReuseBaseComputedStyle(state));
}

TEST_F(StyleResolverTest,
       TransitionRetargetRelativeFontSizeOnParentlessElement) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      html {
        font-size: 20px;
        transition: font-size 100ms;
      }
      .adjust { font-size: 50%; }
    </style>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* element = GetDocument().documentElement();
  element->setAttribute(html_names::kIdAttr, AtomicString("target"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("20px", ComputedValue("font-size", *StyleForId("target")));
  ElementAnimations* element_animations = element->GetElementAnimations();
  EXPECT_FALSE(element_animations);

  // Trigger a transition with a dependency on the parent style.
  element->setAttribute(html_names::kClassAttr, AtomicString("adjust"));
  UpdateAllLifecyclePhasesForTest();
  element_animations = element->GetElementAnimations();
  EXPECT_TRUE(element_animations);
  Animation* transition = (*element_animations->Animations().begin()).key;
  EXPECT_TRUE(transition);
  EXPECT_EQ("20px", ComputedValue("font-size", *StyleForId("target")));

  // Bump the animation time to ensure a transition reversal.
  transition->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(50),
                             ASSERT_NO_EXCEPTION);
  transition->pause();
  UpdateAllLifecyclePhasesForTest();
  const String before_reversal_font_size =
      ComputedValue("font-size", *StyleForId("target"));

  // Verify there is no discontinuity in the font-size on transition reversal.
  element->setAttribute(html_names::kClassAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  element_animations = element->GetElementAnimations();
  EXPECT_TRUE(element_animations);
  Animation* reverse_transition =
      (*element_animations->Animations().begin()).key;
  EXPECT_TRUE(reverse_transition);
  EXPECT_EQ(before_reversal_font_size,
            ComputedValue("font-size", *StyleForId("target")));
}

class StyleResolverFontRelativeUnitTest
    : public testing::WithParamInterface<const char*>,
      public StyleResolverTest {};

TEST_P(StyleResolverFontRelativeUnitTest,
       BaseNotReusableIfFontRelativeUnitPresent) {
  GetDocument().documentElement()->setInnerHTML(
      String::Format("<div id=div style='width:1%s'>Test</div>", GetParam()));
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById(AtomicString("div"));
  auto* effect = CreateSimpleKeyframeEffectForTest(
      div, CSSPropertyID::kFontSize, "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("50px", ComputedValue("font-size", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  const auto* computed_style = StyleForId("div");

  EXPECT_TRUE(computed_style->HasFontRelativeUnits());
  EXPECT_TRUE(computed_style->GetBaseComputedStyle());

  StyleResolverState state(GetDocument(), *div);
  EXPECT_FALSE(StyleResolver::CanReuseBaseComputedStyle(state));
}

TEST_P(StyleResolverFontRelativeUnitTest,
       BaseReusableIfNoFontAffectingAnimation) {
  GetDocument().documentElement()->setInnerHTML(
      String::Format("<div id=div style='width:1%s'>Test</div>", GetParam()));
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById(AtomicString("div"));
  auto* effect = CreateSimpleKeyframeEffectForTest(div, CSSPropertyID::kHeight,
                                                   "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("50px", ComputedValue("height", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  const auto* computed_style = StyleForId("div");

  EXPECT_TRUE(computed_style->HasFontRelativeUnits());
  EXPECT_TRUE(computed_style->GetBaseComputedStyle());

  StyleResolverState state(GetDocument(), *div);
  EXPECT_TRUE(StyleResolver::CanReuseBaseComputedStyle(state));
}

INSTANTIATE_TEST_SUITE_P(All,
                         StyleResolverFontRelativeUnitTest,
                         testing::Values("em", "rem", "ex", "ch"));

namespace {

const CSSImageValue& GetBackgroundImageValue(const ComputedStyle& style) {
  const CSSValue* computed_value = ComputedStyleUtils::ComputedPropertyValue(
      GetCSSPropertyBackgroundImage(), style);

  const CSSValueList* bg_img_list = To<CSSValueList>(computed_value);

  return To<CSSImageValue>(bg_img_list->Item(0));
}

const CSSImageValue& GetBackgroundImageValue(const Element* element) {
  DCHECK(element);
  return GetBackgroundImageValue(element->ComputedStyleRef());
}

const CSSImageSetValue& GetBackgroundImageSetValue(const ComputedStyle& style) {
  const CSSValue* computed_value = ComputedStyleUtils::ComputedPropertyValue(
      GetCSSPropertyBackgroundImage(), style);

  const CSSValueList* bg_img_list = To<CSSValueList>(computed_value);

  return To<CSSImageSetValue>(bg_img_list->Item(0));
}

const CSSImageSetValue& GetBackgroundImageSetValue(const Element* element) {
  DCHECK(element);
  return GetBackgroundImageSetValue(element->ComputedStyleRef());
}

}  // namespace

TEST_F(StyleResolverTest, BackgroundImageFetch) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #none {
        display: none;
        background-image: url(img-none.png);
      }
      #inside-none {
        background-image: url(img-inside-none.png);
      }
      #none-image-set {
        display: none;
        background-image: image-set(url(img-none.png) 1x);
      }
      #hidden {
        visibility: hidden;
        background-image: url(img-hidden.png);
      }
      #inside-hidden {
        background-image: url(img-inside-hidden.png);
      }
      #contents {
        display: contents;
        background-image: url(img-contents.png);
      }
      #inside-contents-parent {
        display: contents;
        background-image: url(img-inside-contents.png);
      }
      #inside-contents {
        background-image: inherit;
      }
      #non-slotted {
        background-image: url(img-non-slotted.png);
      }
      #no-pseudo::before {
        background-image: url(img-no-pseudo.png);
      }
      #first-line::first-line {
        background-image: url(first-line.png);
      }
      #first-line-span::first-line {
        background-image: url(first-line-span.png);
      }
      #first-line-none { display: none; }
      #first-line-none::first-line {
        background-image: url(first-line-none.png);
      }
      frameset {
        display: none;
        border-color: currentColor; /* UA inherit defeats caching */
        background-image: url(frameset-none.png);
      }
    </style>
    <div id="none">
      <div id="inside-none"></div>
    </div>
    <div id="none-image-set">
    </div>
    <div id="hidden">
      <div id="inside-hidden"></div>
    </div>
    <div id="contents"></div>
    <div id="inside-contents-parent">
      <div id="inside-contents"></div>
    </div>
    <div id="host">
      <div id="non-slotted"></div>
    </div>
    <div id="no-pseudo"></div>
    <div id="first-line">XXX</div>
    <span id="first-line-span">XXX</span>
    <div id="first-line-none">XXX</div>
  )HTML");

  auto* frameset1 = GetDocument().CreateRawElement(html_names::kFramesetTag);
  auto* frameset2 = GetDocument().CreateRawElement(html_names::kFramesetTag);
  GetDocument().documentElement()->AppendChild(frameset1);
  GetDocument().documentElement()->AppendChild(frameset2);

  GetDocument()
      .getElementById(AtomicString("host"))
      ->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  UpdateAllLifecyclePhasesForTest();

  auto* none = GetDocument().getElementById(AtomicString("none"));
  auto* inside_none = GetDocument().getElementById(AtomicString("inside-none"));
  auto* none_image_set =
      GetDocument().getElementById(AtomicString("none-image-set"));
  auto* hidden = GetDocument().getElementById(AtomicString("hidden"));
  auto* inside_hidden =
      GetDocument().getElementById(AtomicString("inside-hidden"));
  auto* contents = GetDocument().getElementById(AtomicString("contents"));
  auto* inside_contents =
      GetDocument().getElementById(AtomicString("inside-contents"));
  auto* non_slotted = GetDocument().getElementById(AtomicString("non-slotted"));
  auto* no_pseudo = GetDocument().getElementById(AtomicString("no-pseudo"));
  auto* first_line = GetDocument().getElementById(AtomicString("first-line"));
  auto* first_line_span =
      GetDocument().getElementById(AtomicString("first-line-span"));
  auto* first_line_none =
      GetDocument().getElementById(AtomicString("first-line-none"));

  inside_none->EnsureComputedStyle();
  non_slotted->EnsureComputedStyle();
  none_image_set->EnsureComputedStyle();
  auto* before_style = no_pseudo->EnsureComputedStyle(kPseudoIdBefore);
  auto* first_line_style = first_line->EnsureComputedStyle(kPseudoIdFirstLine);
  auto* first_line_span_style =
      first_line_span->EnsureComputedStyle(kPseudoIdFirstLine);
  auto* first_line_none_style =
      first_line_none->EnsureComputedStyle(kPseudoIdFirstLine);

  ASSERT_TRUE(before_style);
  EXPECT_TRUE(GetBackgroundImageValue(*before_style).IsCachePending())
      << "No fetch for non-generated ::before";
  ASSERT_TRUE(first_line_style);
  EXPECT_FALSE(GetBackgroundImageValue(*first_line_style).IsCachePending())
      << "Fetched by layout of ::first-line";
  ASSERT_TRUE(first_line_span_style);
  EXPECT_TRUE(GetBackgroundImageValue(*first_line_span_style).IsCachePending())
      << "No fetch for inline with ::first-line";
  ASSERT_TRUE(first_line_none_style);
  EXPECT_TRUE(GetBackgroundImageValue(*first_line_none_style).IsCachePending())
      << "No fetch for display:none with ::first-line";
  EXPECT_TRUE(GetBackgroundImageValue(none).IsCachePending())
      << "No fetch for display:none";
  EXPECT_TRUE(GetBackgroundImageValue(inside_none).IsCachePending())
      << "No fetch inside display:none";
  EXPECT_TRUE(GetBackgroundImageSetValue(none_image_set).IsCachePending(1.0f))
      << "No fetch for display:none";
  EXPECT_FALSE(GetBackgroundImageValue(hidden).IsCachePending())
      << "Fetch for visibility:hidden";
  EXPECT_FALSE(GetBackgroundImageValue(inside_hidden).IsCachePending())
      << "Fetch for inherited visibility:hidden";
  EXPECT_FALSE(GetBackgroundImageValue(contents).IsCachePending())
      << "Fetch for display:contents";
  EXPECT_FALSE(GetBackgroundImageValue(inside_contents).IsCachePending())
      << "Fetch for image inherited from display:contents";
  EXPECT_TRUE(GetBackgroundImageValue(non_slotted).IsCachePending())
      << "No fetch for element outside the flat tree";

  // Added two frameset elements to hit the MatchedPropertiesCache for the
  // second one. Frameset adjusts style to display:block in StyleAdjuster, but
  // adjustments are not run before ComputedStyle is added to the
  // MatchedPropertiesCache leaving the cached style with StylePendingImage
  // unless we also check for LayoutObjectIsNeeded in
  // StyleResolverState::LoadPendingImages.
  EXPECT_FALSE(GetBackgroundImageValue(frameset1).IsCachePending())
      << "Fetch for display:none frameset";
  EXPECT_FALSE(GetBackgroundImageValue(frameset2).IsCachePending())
      << "Fetch for display:none frameset - cached";
}

TEST_F(StyleResolverTest, FetchForAtPage) {
  // Without PageMarginBoxes enabled, only a thimbleful of properties are
  // supported, and background-image is not one of them.
  ScopedPageMarginBoxesForTest enable(true);

  // The background-image property applies in an @page context, according to
  // https://drafts.csswg.org/css-page-3/#page-property-list
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      @page {
        background-image: url(bg-img.png);
      }
    </style>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  const ComputedStyle* page_style =
      GetDocument().GetStyleResolver().StyleForPage(0, g_empty_atom);
  ASSERT_TRUE(page_style);
  const CSSValue* computed_value = ComputedStyleUtils::ComputedPropertyValue(
      GetCSSPropertyBackgroundImage(), *page_style);

  const CSSValueList* bg_img_list = To<CSSValueList>(computed_value);
  EXPECT_FALSE(To<CSSImageValue>(bg_img_list->Item(0)).IsCachePending());
}

TEST_F(StyleResolverTest, NoFetchForAtPage) {
  ScopedPageMarginBoxesForTest enable(true);

  // The list-style-image property doesn't apply in an @page context, since
  // it's not in https://drafts.csswg.org/css-page-3/#page-property-list
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      @page {
        list-style-image: url(bg-img.png);
      }
    </style>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  const ComputedStyle* page_style =
      GetDocument().GetStyleResolver().StyleForPage(0, g_empty_atom);
  ASSERT_TRUE(page_style);
  const CSSValue* computed_value = ComputedStyleUtils::ComputedPropertyValue(
      GetCSSPropertyListStyleImage(), *page_style);
  const auto* keyword = DynamicTo<CSSIdentifierValue>(computed_value);
  ASSERT_TRUE(keyword);
  EXPECT_EQ(keyword->GetValueID(), CSSValueID::kNone);
}

// The computed style for a page context isn't web-exposed, so here's a unit
// test for it. See https://drafts.csswg.org/css-page-3/#page-property-list for
// applicable properties within a page context.
TEST_F(StyleResolverTest, PageComputedStyle) {
  ScopedPageMarginBoxesForTest enable(true);

  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      html {
        font-size: 32px;
        margin: 66px;
        width: 123px;
      }
      body {
        /* Note: @page inherits from html, but not body. */
        font-size: 13px;
        margin: 13px;
      }
      @page {
        size: 100px 150px;
        margin: inherit;
        margin-top: 11px;
        margin-inline-end: 12px;
        page-orientation: rotate-left;
        padding-top: 7px;
        line-height: 2em;
        font-family: cursive,fantasy,monospace,sans-serif,serif,UnquotedFont,"QuotedFont\",";

        /* Non-applicable properties will be ignored. */
        columns: 100px 7;
        column-gap: 13px;
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  const ComputedStyle* style =
      GetDocument().GetStyleResolver().StyleForPage(0, g_empty_atom);
  ASSERT_TRUE(style);

  EXPECT_EQ(style->GetPageSizeType(), PageSizeType::kFixed);
  gfx::SizeF page_size = style->PageSize();
  EXPECT_EQ(page_size.width(), 100);
  EXPECT_EQ(page_size.height(), 150);

  EXPECT_EQ(style->MarginTop(), Length::Fixed(11));
  EXPECT_EQ(style->MarginRight(), Length::Fixed(12));
  EXPECT_EQ(style->MarginBottom(), Length::Fixed(66));
  EXPECT_EQ(style->MarginLeft(), Length::Fixed(66));
  EXPECT_EQ(style->GetPageOrientation(), PageOrientation::kRotateLeft);

  EXPECT_EQ(style->PaddingTop(), Length::Fixed(7));

  EXPECT_EQ(style->Width(), Length::Auto());

  EXPECT_EQ(style->LineHeight(), Length::Fixed(64));
  EXPECT_EQ(style->FontSize(), 32);
  String font_family = ComputedStyleUtils::ValueForFontFamily(
                           style->GetFontDescription().Family())
                           ->CssText();
  EXPECT_EQ(
      font_family,
      R"(cursive, fantasy, monospace, sans-serif, serif, UnquotedFont, "QuotedFont\",")");

  // Non-applicable properties:
  EXPECT_TRUE(style->HasAutoColumnCount());
  EXPECT_TRUE(style->HasAutoColumnWidth());
  EXPECT_FALSE(style->ColumnGap().has_value());
}

TEST_F(StyleResolverTest, PageComputedStyleLimited) {
  ScopedPageMarginBoxesForTest enable(false);

  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      html {
        margin: 77px;
      }
      body {
        /* Note: @page inherits from html, but not body. */
        margin: 13px;
      }
      @page {
        size: 100px 150px;
        margin: inherit;
        margin-top: 11px;
        margin-inline-end: 12px;
        page-orientation: rotate-left;
        padding-top: 7px;
      }
    </style>
    <body></body>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  const ComputedStyle* style =
      GetDocument().GetStyleResolver().StyleForPage(0, g_empty_atom);
  ASSERT_TRUE(style);

  EXPECT_EQ(style->GetPageSizeType(), PageSizeType::kFixed);
  gfx::SizeF page_size = style->PageSize();
  EXPECT_EQ(page_size.width(), 100);
  EXPECT_EQ(page_size.height(), 150);

  EXPECT_EQ(style->MarginTop(), Length::Fixed(11));
  EXPECT_EQ(style->MarginRight(), Length::Fixed(12));
  EXPECT_EQ(style->MarginBottom(), Length::Fixed(77));
  EXPECT_EQ(style->MarginLeft(), Length::Fixed(77));
  EXPECT_EQ(style->GetPageOrientation(), PageOrientation::kRotateLeft);

  // The padding-top declaration should be ignored.
  EXPECT_EQ(style->PaddingTop(), Length::Fixed(0));
}

TEST_F(StyleResolverTest, NoFetchForHighlightPseudoElements) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      body::target-text, body::selection {
        color: green;
        background-image: url(bg-img.png);
        cursor: url(cursor.ico), auto;
      }
    </style>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* body = GetDocument().body();
  ASSERT_TRUE(body);
  const auto* element_style = body->GetComputedStyle();
  ASSERT_TRUE(element_style);

  StyleRequest pseudo_style_request;
  pseudo_style_request.parent_override = element_style;
  pseudo_style_request.layout_parent_override = element_style;
  pseudo_style_request.originating_element_style = element_style;

  StyleRequest target_text_style_request = pseudo_style_request;
  target_text_style_request.pseudo_id = kPseudoIdTargetText;

  const ComputedStyle* target_text_style =
      GetDocument().GetStyleResolver().ResolveStyle(GetDocument().body(),
                                                    StyleRecalcContext(),
                                                    target_text_style_request);
  ASSERT_TRUE(target_text_style);

  StyleRequest selection_style_style_request = pseudo_style_request;
  selection_style_style_request.pseudo_id = kPseudoIdSelection;

  const ComputedStyle* selection_style =
      GetDocument().GetStyleResolver().ResolveStyle(
          GetDocument().body(), StyleRecalcContext(),
          selection_style_style_request);
  ASSERT_TRUE(selection_style);

  // Check that the cursor does not apply to ::selection.
  ASSERT_FALSE(selection_style->Cursors());

  // Check that the cursor does not apply to ::target-text.
  ASSERT_FALSE(target_text_style->Cursors());

  // Check that we don't fetch the cursor url() for ::target-text.
  CursorList* cursor_list = target_text_style->Cursors();
  ASSERT_FALSE(cursor_list);

  for (const auto* pseudo_style : {target_text_style, selection_style}) {
    // Check that the color applies.
    EXPECT_EQ(Color(0, 128, 0),
              pseudo_style->VisitedDependentColor(GetCSSPropertyColor()));

    // Check that the background-image does not apply.
    const CSSValue* computed_value = ComputedStyleUtils::ComputedPropertyValue(
        GetCSSPropertyBackgroundImage(), *pseudo_style);
    const CSSValueList* list = DynamicTo<CSSValueList>(computed_value);
    ASSERT_TRUE(list);
    ASSERT_EQ(1u, list->length());
    const auto* keyword = DynamicTo<CSSIdentifierValue>(list->Item(0));
    ASSERT_TRUE(keyword);
    EXPECT_EQ(CSSValueID::kNone, keyword->GetValueID());
  }
}

TEST_F(StyleResolverTest, CSSMarkerPseudoElement) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      b::before {
        content: "[before]";
        display: list-item;
      }
      #marker ::marker {
        color: blue;
      }
    </style>
    <ul>
      <li style="list-style: decimal outside"><b></b></li>
      <li style="list-style: decimal inside"><b></b></li>
      <li style="list-style: disc outside"><b></b></li>
      <li style="list-style: disc inside"><b></b></li>
      <li style="list-style: '- ' outside"><b></b></li>
      <li style="list-style: '- ' inside"><b></b></li>
      <li style="list-style: linear-gradient(blue, cyan) outside"><b></b></li>
      <li style="list-style: linear-gradient(blue, cyan) inside"><b></b></li>
      <li style="list-style: none outside"><b></b></li>
      <li style="list-style: none inside"><b></b></li>
    </ul>
  )HTML");
  StaticElementList* lis = GetDocument().QuerySelectorAll(AtomicString("li"));
  EXPECT_EQ(lis->length(), 10U);

  UpdateAllLifecyclePhasesForTest();
  for (unsigned i = 0; i < lis->length(); ++i) {
    Element* li = lis->item(i);
    PseudoElement* marker = li->GetPseudoElement(kPseudoIdMarker);
    PseudoElement* before =
        li->QuerySelector(AtomicString("b"))->GetPseudoElement(kPseudoIdBefore);
    PseudoElement* nested_marker = before->GetPseudoElement(kPseudoIdMarker);

    // Check that UA styles for list markers don't set HasPseudoElementStyle
    const ComputedStyle* li_style = li->GetComputedStyle();
    EXPECT_FALSE(li_style->HasPseudoElementStyle(kPseudoIdMarker));
    EXPECT_FALSE(li_style->HasAnyPseudoElementStyles());
    const ComputedStyle* before_style = before->GetComputedStyle();
    EXPECT_FALSE(before_style->HasPseudoElementStyle(kPseudoIdMarker));
    EXPECT_FALSE(before_style->HasAnyPseudoElementStyles());

    if (i >= 8) {
      EXPECT_FALSE(marker);
      EXPECT_FALSE(nested_marker);
      continue;
    }

    // Check that list markers have UA styles
    EXPECT_TRUE(marker);
    EXPECT_TRUE(nested_marker);
    EXPECT_EQ(marker->GetComputedStyle()->GetUnicodeBidi(),
              UnicodeBidi::kIsolate);
    EXPECT_EQ(nested_marker->GetComputedStyle()->GetUnicodeBidi(),
              UnicodeBidi::kIsolate);
  }

  GetDocument().body()->SetIdAttribute(AtomicString("marker"));
  UpdateAllLifecyclePhasesForTest();
  for (unsigned i = 0; i < lis->length(); ++i) {
    Element* li = lis->item(i);
    PseudoElement* before =
        li->QuerySelector(AtomicString("b"))->GetPseudoElement(kPseudoIdBefore);

    // Check that author styles for list markers do set HasPseudoElementStyle
    const ComputedStyle* li_style = li->GetComputedStyle();
    EXPECT_TRUE(li_style->HasPseudoElementStyle(kPseudoIdMarker));
    EXPECT_TRUE(li_style->HasAnyPseudoElementStyles());

    // But ::marker styles don't match a ::before::marker
    const ComputedStyle* before_style = before->GetComputedStyle();
    EXPECT_FALSE(before_style->HasPseudoElementStyle(kPseudoIdMarker));
    EXPECT_FALSE(before_style->HasAnyPseudoElementStyles());
  }
}

TEST_F(StyleResolverTest, ApplyInheritedOnlyCustomPropertyChange) {
  // This test verifies that when we get a "apply inherited only"-type
  // hit in the MatchesPropertiesCache, we're able to detect that custom
  // properties changed, and that we therefore need to apply the non-inherited
  // properties as well.

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #parent1 { --a: 10px; }
      #parent2 { --a: 20px; }
      #child1, #child2 {
        --b: var(--a);
        width: var(--b);
      }
    </style>
    <div id=parent1><div id=child1></div></div>
    <div id=parent2><div id=child2></div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ("10px", ComputedValue("width", *StyleForId("child1")));
  EXPECT_EQ("20px", ComputedValue("width", *StyleForId("child2")));
}

TEST_F(StyleResolverTest, CssRulesForElementIncludedRules) {
  UpdateAllLifecyclePhasesForTest();

  Element* body = GetDocument().body();
  ASSERT_TRUE(body);

  // Don't crash when only getting one type of rule.
  auto& resolver = GetDocument().GetStyleResolver();
  resolver.CssRulesForElement(body, StyleResolver::kUACSSRules);
  resolver.CssRulesForElement(body, StyleResolver::kUserCSSRules);
  resolver.CssRulesForElement(body, StyleResolver::kAuthorCSSRules);
}

TEST_F(StyleResolverTest, NestedPseudoElement) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      div::before { content: "Hello"; display: list-item; }
      div::before::marker { color: green; }
    </style>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  // Don't crash when calculating style for nested pseudo elements.
}

TEST_F(StyleResolverTest, CascadedValuesForElement) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #div {
        top: 1em;
      }
      div {
        top: 10em;
        right: 20em;
        bottom: 30em;
        left: 40em;

        width: 50em;
        width: 51em;
        height: 60em !important;
        height: 61em;
      }
    </style>
    <div id=div style="bottom:300em;"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto& resolver = GetDocument().GetStyleResolver();
  Element* div = GetDocument().getElementById(AtomicString("div"));
  ASSERT_TRUE(div);

  auto map = resolver.CascadedValuesForElement(div, kPseudoIdNone);

  CSSPropertyName top(CSSPropertyID::kTop);
  CSSPropertyName right(CSSPropertyID::kRight);
  CSSPropertyName bottom(CSSPropertyID::kBottom);
  CSSPropertyName left(CSSPropertyID::kLeft);
  CSSPropertyName width(CSSPropertyID::kWidth);
  CSSPropertyName height(CSSPropertyID::kHeight);

  ASSERT_TRUE(map.at(top));
  ASSERT_TRUE(map.at(right));
  ASSERT_TRUE(map.at(bottom));
  ASSERT_TRUE(map.at(left));
  ASSERT_TRUE(map.at(width));
  ASSERT_TRUE(map.at(height));

  EXPECT_EQ("1em", map.at(top)->CssText());
  EXPECT_EQ("20em", map.at(right)->CssText());
  EXPECT_EQ("300em", map.at(bottom)->CssText());
  EXPECT_EQ("40em", map.at(left)->CssText());
  EXPECT_EQ("51em", map.at(width)->CssText());
  EXPECT_EQ("60em", map.at(height)->CssText());
}

TEST_F(StyleResolverTest, CascadedValuesForPseudoElement) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #div::before {
        top: 1em;
      }
      div::before {
        top: 10em;
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto& resolver = GetDocument().GetStyleResolver();
  Element* div = GetDocument().getElementById(AtomicString("div"));
  ASSERT_TRUE(div);

  auto map = resolver.CascadedValuesForElement(div, kPseudoIdBefore);

  CSSPropertyName top(CSSPropertyID::kTop);
  ASSERT_TRUE(map.at(top));
  EXPECT_EQ("1em", map.at(top)->CssText());
}

TEST_F(StyleResolverTestCQ, CascadedValuesForElementInContainer) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #container { container-type: inline-size; }
      @container (min-width: 1px) {
        #inner {
          top: 1em;
        }
      }
      div {
        top: 10em;
      }
    </style>
    <div id="container">
      <div id="inner"></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto& resolver = GetDocument().GetStyleResolver();
  Element* inner = GetDocument().getElementById(AtomicString("inner"));
  ASSERT_TRUE(inner);

  auto map = resolver.CascadedValuesForElement(inner, kPseudoIdNone);

  CSSPropertyName top(CSSPropertyID::kTop);
  ASSERT_TRUE(map.at(top));
  EXPECT_EQ("1em", map.at(top)->CssText());
}

TEST_F(StyleResolverTestCQ, CascadedValuesForPseudoElementInContainer) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #container { container-type: inline-size; }
      @container (min-width: 1px) {
        #inner::before {
          top: 1em;
        }
      }
      div::before {
        top: 10em;
      }
    </style>
    <div id="container">
      <div id="inner"></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto& resolver = GetDocument().GetStyleResolver();
  Element* inner = GetDocument().getElementById(AtomicString("inner"));
  ASSERT_TRUE(inner);

  auto map = resolver.CascadedValuesForElement(inner, kPseudoIdBefore);

  CSSPropertyName top(CSSPropertyID::kTop);
  ASSERT_TRUE(map.at(top));
  EXPECT_EQ("1em", map.at(top)->CssText());
}

TEST_F(StyleResolverTest, EnsureComputedStyleSlotFallback) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="host"><span></span></div>
  )HTML");

  ShadowRoot& shadow_root =
      GetDocument()
          .getElementById(AtomicString("host"))
          ->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(R"HTML(
    <style>
      slot { color: red }
    </style>
    <slot><span id="fallback"></span></slot>
  )HTML");
  Element* fallback = shadow_root.getElementById(AtomicString("fallback"));
  ASSERT_TRUE(fallback);

  UpdateAllLifecyclePhasesForTest();

  // Elements outside the flat tree does not get styles computed during the
  // lifecycle update.
  EXPECT_FALSE(fallback->GetComputedStyle());

  // We are currently allowed to query the computed style of elements outside
  // the flat tree, but slot fallback does not inherit from the slot.
  const ComputedStyle* fallback_style = fallback->EnsureComputedStyle();
  ASSERT_TRUE(fallback_style);
  EXPECT_EQ(Color::kBlack,
            fallback_style->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(StyleResolverTest, EnsureComputedStyleOutsideFlatTree) {
  GetDocument().documentElement()->setHTMLUnsafe(R"HTML(
    <div id=host>
      <template shadowrootmode=open>
      </template>
      <div id=a>
        <div id=b>
          <div id=c>
            <div id=d>
              <div id=e>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* host = GetElementById("host");
  ASSERT_TRUE(host);
  ASSERT_TRUE(host->GetShadowRoot());

  Element* a = GetElementById("a");
  Element* b = GetElementById("b");
  Element* c = GetElementById("c");
  Element* d = GetElementById("d");
  Element* e = GetElementById("e");
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(c);
  ASSERT_TRUE(d);
  ASSERT_TRUE(e);

  EXPECT_FALSE(a->GetComputedStyle());
  EXPECT_FALSE(b->GetComputedStyle());
  EXPECT_FALSE(c->GetComputedStyle());
  EXPECT_FALSE(d->GetComputedStyle());
  EXPECT_FALSE(e->GetComputedStyle());

  c->EnsureComputedStyle();

  const ComputedStyle* a_style = a->GetComputedStyle();
  const ComputedStyle* b_style = b->GetComputedStyle();
  const ComputedStyle* c_style = c->GetComputedStyle();

  ASSERT_TRUE(a_style);
  ASSERT_TRUE(b_style);
  ASSERT_TRUE(c_style);
  EXPECT_FALSE(d->GetComputedStyle());
  EXPECT_FALSE(e->GetComputedStyle());

  // Dirty style of #a.
  a->SetInlineStyleProperty(CSSPropertyID::kZIndex, "42");

  // Note that there is no call to UpdateAllLifecyclePhasesForTest here,
  // because #a is outside the flat tree, hence that process would anyway not
  // reach #a.

  // Ensuring the style of some deep descendant must discover that some ancestor
  // is marked for recalc.
  e->EnsureComputedStyle();
  EXPECT_TRUE(a->GetComputedStyle());
  EXPECT_TRUE(b->GetComputedStyle());
  EXPECT_TRUE(c->GetComputedStyle());
  EXPECT_TRUE(d->GetComputedStyle());
  EXPECT_TRUE(e->GetComputedStyle());
  EXPECT_NE(a_style, a->GetComputedStyle());
  EXPECT_NE(b_style, b->GetComputedStyle());
  EXPECT_NE(c_style, c->GetComputedStyle());
}

TEST_F(StyleResolverTest, ComputeValueStandardProperty) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #target { --color: green }
    </style>
    <div id="target"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);

  // Unable to parse a variable reference with css_test_helpers::ParseLonghand.
  CSSPropertyID property_id = CSSPropertyID::kColor;
  auto* set =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  MutableCSSPropertyValueSet::SetResult result = set->ParseAndSetProperty(
      property_id, "var(--color)", false, SecureContextMode::kInsecureContext,
      /*context_style_sheet=*/nullptr);
  ASSERT_NE(MutableCSSPropertyValueSet::kParseError, result);
  const CSSValue* parsed_value = set->GetPropertyCSSValue(property_id);
  ASSERT_TRUE(parsed_value);
  const CSSValue* computed_value = StyleResolver::ComputeValue(
      target, CSSPropertyName(property_id), *parsed_value);
  ASSERT_TRUE(computed_value);
  EXPECT_EQ("rgb(0, 128, 0)", computed_value->CssText());
}

namespace {

const CSSValue* ParseCustomProperty(Document& document,
                                    const CustomProperty& property,
                                    const String& value) {
  const auto* context = MakeGarbageCollected<CSSParserContext>(document);
  CSSParserLocalContext local_context;

  return property.Parse(value, *context, local_context);
}

}  // namespace

TEST_F(StyleResolverTest, ComputeValueCustomProperty) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #target { --color: green }
    </style>
    <div id="target"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);

  AtomicString custom_property_name("--color");
  const CSSValue* parsed_value = ParseCustomProperty(
      GetDocument(), CustomProperty(custom_property_name, GetDocument()),
      "blue");
  ASSERT_TRUE(parsed_value);
  const CSSValue* computed_value = StyleResolver::ComputeValue(
      target, CSSPropertyName(custom_property_name), *parsed_value);
  ASSERT_TRUE(computed_value);
  EXPECT_EQ("blue", computed_value->CssText());
}

TEST_F(StyleResolverTest, TreeScopedReferences) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #host { animation-name: anim }
    </style>
    <div id="host">
      <span id="slotted"></span>
    </host>
  )HTML");

  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);
  ShadowRoot& root = host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  root.setInnerHTML(R"HTML(
    <style>
      ::slotted(span) { animation-name: anim-slotted }
      :host { font-family: myfont }
    </style>
    <div id="inner-host">
      <slot></slot>
    </div>
  )HTML");

  Element* inner_host = root.getElementById(AtomicString("inner-host"));
  ASSERT_TRUE(inner_host);
  ShadowRoot& inner_root =
      inner_host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  inner_root.setInnerHTML(R"HTML(
    <style>
      ::slotted(span) { animation-name: anim-inner-slotted }
    </style>
    <slot></slot>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  {
    StyleResolverState state(GetDocument(), *host);
    SelectorFilter filter;
    MatchResult match_result;
    ElementRuleCollector collector(state.ElementContext(), StyleRecalcContext(),
                                   filter, match_result,
                                   EInsideLink::kNotInsideLink);
    GetDocument().GetStyleEngine().GetStyleResolver().MatchAllRules(
        state, collector, false /* include_smil_properties */);
    const auto& properties = match_result.GetMatchedProperties();
    ASSERT_EQ(properties.size(), 4u);

    // div { display: block }
    EXPECT_EQ(properties[0].data_.origin, CascadeOrigin::kUserAgent);

    // div { unicode-bidi: isolate; }
    EXPECT_EQ(properties[1].data_.origin, CascadeOrigin::kUserAgent);

    // :host { font-family: myfont }
    EXPECT_EQ(match_result.ScopeFromTreeOrder(properties[2].data_.tree_order),
              root.GetTreeScope());
    EXPECT_EQ(properties[2].data_.origin, CascadeOrigin::kAuthor);

    // #host { animation-name: anim }
    EXPECT_EQ(properties[3].data_.origin, CascadeOrigin::kAuthor);
    EXPECT_EQ(match_result.ScopeFromTreeOrder(properties[3].data_.tree_order),
              host->GetTreeScope());
  }

  {
    auto* span = GetDocument().getElementById(AtomicString("slotted"));
    StyleResolverState state(GetDocument(), *span);
    SelectorFilter filter;
    MatchResult match_result;
    ElementRuleCollector collector(state.ElementContext(), StyleRecalcContext(),
                                   filter, match_result,
                                   EInsideLink::kNotInsideLink);
    GetDocument().GetStyleEngine().GetStyleResolver().MatchAllRules(
        state, collector, false /* include_smil_properties */);
    const auto& properties = match_result.GetMatchedProperties();
    ASSERT_EQ(properties.size(), 2u);

    // ::slotted(span) { animation-name: anim-inner-slotted }
    EXPECT_EQ(properties[0].data_.origin, CascadeOrigin::kAuthor);
    EXPECT_EQ(match_result.ScopeFromTreeOrder(properties[0].data_.tree_order),
              inner_root.GetTreeScope());

    // ::slotted(span) { animation-name: anim-slotted }
    EXPECT_EQ(properties[1].data_.origin, CascadeOrigin::kAuthor);
    EXPECT_EQ(match_result.ScopeFromTreeOrder(properties[1].data_.tree_order),
              root.GetTreeScope());
  }
}

TEST_F(StyleResolverTest, InheritStyleImagesFromDisplayContents) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #parent {
        display: contents;

        background-image: url(1.png);
        border-image-source: url(2.png);
        cursor: url(3.ico), text;
        list-style-image: url(4.png);
        shape-outside: url(5.png);
        -webkit-box-reflect: below 0 url(6.png);
        -webkit-mask-box-image-source: url(7.png);
        -webkit-mask-image: url(8.png);
      }
      #child {
        background-image: inherit;
        border-image-source: inherit;
        cursor: inherit;
        list-style-image: inherit;
        shape-outside: inherit;
        -webkit-box-reflect: inherit;
        -webkit-mask-box-image-source: inherit;
        -webkit-mask-image: inherit;
      }
    </style>
    <div id="parent">
      <div id="child"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* child = GetDocument().getElementById(AtomicString("child"));
  auto* style = child->GetComputedStyle();
  ASSERT_TRUE(style);

  ASSERT_TRUE(style->BackgroundLayers().GetImage());
  EXPECT_FALSE(style->BackgroundLayers().GetImage()->IsPendingImage())
      << "background-image is fetched";

  ASSERT_TRUE(style->BorderImageSource());
  EXPECT_FALSE(style->BorderImageSource()->IsPendingImage())
      << "border-image-source is fetched";

  ASSERT_TRUE(style->Cursors());
  ASSERT_TRUE(style->Cursors()->size());
  ASSERT_TRUE(style->Cursors()->at(0).GetImage());
  EXPECT_FALSE(style->Cursors()->at(0).GetImage()->IsPendingImage())
      << "cursor is fetched";

  ASSERT_TRUE(style->ListStyleImage());
  EXPECT_FALSE(style->ListStyleImage()->IsPendingImage())
      << "list-style-image is fetched";

  ASSERT_TRUE(style->ShapeOutside());
  ASSERT_TRUE(style->ShapeOutside()->GetImage());
  EXPECT_FALSE(style->ShapeOutside()->GetImage()->IsPendingImage())
      << "shape-outside is fetched";

  ASSERT_TRUE(style->BoxReflect());
  ASSERT_TRUE(style->BoxReflect()->Mask().GetImage());
  EXPECT_FALSE(style->BoxReflect()->Mask().GetImage()->IsPendingImage())
      << "-webkit-box-reflect is fetched";

  ASSERT_TRUE(style->MaskBoxImageSource());
  EXPECT_FALSE(style->MaskBoxImageSource()->IsPendingImage())
      << "-webkit-mask-box-image-source";

  ASSERT_TRUE(style->MaskLayers().GetImage());
  EXPECT_FALSE(style->MaskLayers().GetImage()->IsPendingImage())
      << "-webkit-mask-image is fetched";
}

TEST_F(StyleResolverTest, TextShadowInHighlightPseudoNotCounted1) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kTextShadowInHighlightPseudo));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kTextShadowNotNoneInHighlightPseudo));

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      * {
        text-shadow: 5px 5px green;
      }
    </style>
    <div id="target">target</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  const auto* element_style = target->GetComputedStyle();
  ASSERT_TRUE(element_style);

  StyleRequest pseudo_style_request;
  pseudo_style_request.parent_override = element_style;
  pseudo_style_request.layout_parent_override = element_style;
  pseudo_style_request.originating_element_style = element_style;
  pseudo_style_request.pseudo_id = kPseudoIdSelection;
  const ComputedStyle* selection_style =
      GetDocument().GetStyleResolver().ResolveStyle(
          target, StyleRecalcContext(), pseudo_style_request);
  ASSERT_FALSE(selection_style);

  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kTextShadowInHighlightPseudo));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kTextShadowNotNoneInHighlightPseudo));
}

TEST_F(StyleResolverTest, TextShadowInHighlightPseudoNotCounted2) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kTextShadowInHighlightPseudo));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kTextShadowNotNoneInHighlightPseudo));

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      * {
        text-shadow: 5px 5px green;
      }
      ::selection {
        color: white;
        background: blue;
      }
    </style>
    <div id="target">target</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  const auto* element_style = target->GetComputedStyle();
  ASSERT_TRUE(element_style);

  StyleRequest pseudo_style_request;
  pseudo_style_request.parent_override = element_style;
  pseudo_style_request.layout_parent_override = element_style;
  pseudo_style_request.originating_element_style = element_style;
  pseudo_style_request.pseudo_id = kPseudoIdSelection;
  const ComputedStyle* selection_style =
      GetDocument().GetStyleResolver().ResolveStyle(
          target, StyleRecalcContext(), pseudo_style_request);
  ASSERT_TRUE(selection_style);

  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kTextShadowInHighlightPseudo));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kTextShadowNotNoneInHighlightPseudo));
}

TEST_F(StyleResolverTest, TextShadowInHighlightPseudotNone) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kTextShadowInHighlightPseudo));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kTextShadowNotNoneInHighlightPseudo));

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      * {
        text-shadow: 5px 5px green;
      }
      ::selection {
        text-shadow: none;
      }
    </style>
    <div id="target">target</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  const auto* element_style = target->GetComputedStyle();
  ASSERT_TRUE(element_style);

  StyleRequest pseudo_style_request;
  pseudo_style_request.parent_override = element_style;
  pseudo_style_request.layout_parent_override = element_style;
  pseudo_style_request.originating_element_style = element_style;
  pseudo_style_request.pseudo_id = kPseudoIdSelection;
  const ComputedStyle* selection_style =
      GetDocument().GetStyleResolver().ResolveStyle(
          target, StyleRecalcContext(), pseudo_style_request);
  ASSERT_TRUE(selection_style);

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kTextShadowInHighlightPseudo));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kTextShadowNotNoneInHighlightPseudo));
}

TEST_F(StyleResolverTest, TextShadowInHighlightPseudoNotNone1) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kTextShadowInHighlightPseudo));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kTextShadowNotNoneInHighlightPseudo));

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      ::selection {
        text-shadow: 5px 5px green;
      }
    </style>
    <div id="target">target</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  const auto* element_style = target->GetComputedStyle();
  ASSERT_TRUE(element_style);

  StyleRequest pseudo_style_request;
  pseudo_style_request.parent_override = element_style;
  pseudo_style_request.layout_parent_override = element_style;
  pseudo_style_request.originating_element_style = element_style;
  pseudo_style_request.pseudo_id = kPseudoIdSelection;
  const ComputedStyle* selection_style =
      GetDocument().GetStyleResolver().ResolveStyle(
          target, StyleRecalcContext(), pseudo_style_request);
  ASSERT_TRUE(selection_style);

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kTextShadowInHighlightPseudo));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kTextShadowNotNoneInHighlightPseudo));
}

TEST_F(StyleResolverTest, TextShadowInHighlightPseudoNotNone2) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kTextShadowInHighlightPseudo));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kTextShadowNotNoneInHighlightPseudo));

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      * {
        text-shadow: 5px 5px green;
      }
      ::selection {
        text-shadow: 5px 5px green;
      }
    </style>
    <div id="target">target</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  const auto* element_style = target->GetComputedStyle();
  ASSERT_TRUE(element_style);

  StyleRequest pseudo_style_request;
  pseudo_style_request.parent_override = element_style;
  pseudo_style_request.layout_parent_override = element_style;
  pseudo_style_request.originating_element_style = element_style;
  pseudo_style_request.pseudo_id = kPseudoIdSelection;
  const ComputedStyle* selection_style =
      GetDocument().GetStyleResolver().ResolveStyle(
          target, StyleRecalcContext(), pseudo_style_request);
  ASSERT_TRUE(selection_style);

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kTextShadowInHighlightPseudo));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kTextShadowNotNoneInHighlightPseudo));
}

TEST_F(StyleResolverTestCQ, DependsOnSizeContainerQueries) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #a { color: red; }
      @container (min-width: 0px) {
        #b { color: blue; }
        span { color: green; }
        #d { color: coral; }
      }
    </style>
    <div id=a></div>
    <span id=b></span>
    <span id=c></span>
    <div id=d></div>
    <div id=e></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* a = GetDocument().getElementById(AtomicString("a"));
  auto* b = GetDocument().getElementById(AtomicString("b"));
  auto* c = GetDocument().getElementById(AtomicString("c"));
  auto* d = GetDocument().getElementById(AtomicString("d"));
  auto* e = GetDocument().getElementById(AtomicString("e"));

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(c);
  ASSERT_TRUE(d);
  ASSERT_TRUE(e);

  EXPECT_FALSE(a->ComputedStyleRef().DependsOnSizeContainerQueries());
  EXPECT_TRUE(b->ComputedStyleRef().DependsOnSizeContainerQueries());
  EXPECT_TRUE(c->ComputedStyleRef().DependsOnSizeContainerQueries());
  EXPECT_TRUE(d->ComputedStyleRef().DependsOnSizeContainerQueries());
  EXPECT_FALSE(e->ComputedStyleRef().DependsOnSizeContainerQueries());

  EXPECT_FALSE(a->ComputedStyleRef().DependsOnStyleContainerQueries());
  EXPECT_FALSE(b->ComputedStyleRef().DependsOnStyleContainerQueries());
  EXPECT_FALSE(c->ComputedStyleRef().DependsOnStyleContainerQueries());
  EXPECT_FALSE(d->ComputedStyleRef().DependsOnStyleContainerQueries());
  EXPECT_FALSE(e->ComputedStyleRef().DependsOnStyleContainerQueries());
}

TEST_F(StyleResolverTestCQ, DependsOnSizeContainerQueriesPseudo) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      main { container-type: size; width: 100px; }
      #a::before { content: "before"; }
      @container (min-width: 0px) {
        #a::after { content: "after"; }
      }
    </style>
    <main>
      <div id=a></div>
    </main>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* a = GetDocument().getElementById(AtomicString("a"));
  auto* before = a->GetPseudoElement(kPseudoIdBefore);
  auto* after = a->GetPseudoElement(kPseudoIdAfter);

  ASSERT_TRUE(a);
  ASSERT_TRUE(before);
  ASSERT_TRUE(after);

  EXPECT_TRUE(a->ComputedStyleRef().DependsOnSizeContainerQueries());
  EXPECT_FALSE(before->ComputedStyleRef().DependsOnSizeContainerQueries());
  EXPECT_TRUE(after->ComputedStyleRef().DependsOnSizeContainerQueries());
}

// Verify that the ComputedStyle::DependsOnSizeContainerQuery flag does
// not end up in the MatchedPropertiesCache (MPC).
TEST_F(StyleResolverTestCQ, DependsOnSizeContainerQueriesMPC) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      @container (min-width: 9999999px) {
        #a { color: green; }
      }
    </style>
    <div id=a></div>
    <div id=b></div>
  )HTML");

  // In the above example, both <div id=a> and <div id=b> match the same
  // rules (i.e. whatever is provided by UA style). The selector inside
  // the @container rule does ultimately _not_ match <div id=a> (because the
  // container query evaluates to 'false'), however, it _does_ cause the
  // ComputedStyle::DependsOnSizeContainerQuery flag to be set on #a.
  //
  // We must ensure that we don't add the DependsOnSizeContainerQuery-flagged
  // style to the MPC, otherwise the subsequent cache hit for #b would result
  // in the flag being (incorrectly) set for that element.

  UpdateAllLifecyclePhasesForTest();

  auto* a = GetDocument().getElementById(AtomicString("a"));
  auto* b = GetDocument().getElementById(AtomicString("b"));

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_TRUE(a->ComputedStyleRef().DependsOnSizeContainerQueries());
  EXPECT_FALSE(b->ComputedStyleRef().DependsOnSizeContainerQueries());
}

TEST_F(StyleResolverTestCQ, DependsOnStyleContainerQueries) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #a { color: red; }
      @container style(--foo: bar) {
        #b { color: blue; }
        span { color: green; }
        #d { color: coral; }
      }
    </style>
    <div id=a></div>
    <span id=b></span>
    <span id=c></span>
    <div id=d></div>
    <div id=e></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* a = GetDocument().getElementById(AtomicString("a"));
  auto* b = GetDocument().getElementById(AtomicString("b"));
  auto* c = GetDocument().getElementById(AtomicString("c"));
  auto* d = GetDocument().getElementById(AtomicString("d"));
  auto* e = GetDocument().getElementById(AtomicString("e"));

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(c);
  ASSERT_TRUE(d);
  ASSERT_TRUE(e);

  EXPECT_FALSE(a->ComputedStyleRef().DependsOnStyleContainerQueries());
  EXPECT_TRUE(b->ComputedStyleRef().DependsOnStyleContainerQueries());
  EXPECT_TRUE(c->ComputedStyleRef().DependsOnStyleContainerQueries());
  EXPECT_TRUE(d->ComputedStyleRef().DependsOnStyleContainerQueries());
  EXPECT_FALSE(e->ComputedStyleRef().DependsOnStyleContainerQueries());

  EXPECT_FALSE(a->ComputedStyleRef().DependsOnSizeContainerQueries());
  EXPECT_FALSE(b->ComputedStyleRef().DependsOnSizeContainerQueries());
  EXPECT_FALSE(c->ComputedStyleRef().DependsOnSizeContainerQueries());
  EXPECT_FALSE(d->ComputedStyleRef().DependsOnSizeContainerQueries());
  EXPECT_FALSE(e->ComputedStyleRef().DependsOnSizeContainerQueries());
}

TEST_F(StyleResolverTest, AnchorQueriesMPC) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      .anchor {
        position: absolute;
        width: 100px;
        height: 100px;
      }
      #anchor1 { left: 100px; }
      #anchor2 { left: 150px; }
      .anchored {
        position: absolute;
        left: anchor(left);
      }
    </style>
    <div class=anchor id=anchor1>X</div>
    <div class=anchor id=anchor2>Y</div>
    <div class=anchored id=a anchor=anchor1>A</div>
    <div class=anchored id=b anchor=anchor2>B</div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  // #a and #b have identical styles, but the implicit anchor makes
  // the anchor() queries give two different answers.

  auto* a = GetDocument().getElementById(AtomicString("a"));
  auto* b = GetDocument().getElementById(AtomicString("b"));

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  EXPECT_EQ("100px", ComputedValue("left", a->ComputedStyleRef()));
  EXPECT_EQ("150px", ComputedValue("left", b->ComputedStyleRef()));
}

TEST_F(StyleResolverTest, AnchorQueryNoOldStyle) {
  // This captures any calls to StoreOldStyleIfNeeded made during
  // StyleResolver::ResolveStyle.
  PostStyleUpdateScope post_style_update_scope(GetDocument());

  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #anchored {
        position: absolute;
        left: anchor(--a left, 42px);
      }
    </style>
    <div id=anchored>A</div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetCurrentOldStylesCount());
}

TEST_F(StyleResolverTest, AnchorQueryStoreOldStyle) {
  // This captures any calls to StoreOldStyleIfNeeded made during
  // StyleResolver::ResolveStyle.
  PostStyleUpdateScope post_style_update_scope(GetDocument());

  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #anchored {
        position: absolute;
        left: anchor(--a left, 42px);
        transition: left 1s;
      }
    </style>
    <div id=anchored>A</div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1u, GetCurrentOldStylesCount());
}

TEST_F(StyleResolverTest, AnchorQueryBaseComputedStyle) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #div {
        position: absolute;
        left: anchor(--a left, 42px);
      }
    </style>
    <div id=div>A</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* div = GetDocument().getElementById(AtomicString("div"));

  // Create a situation where the base computed style optimization
  // would normally be used.
  auto* effect = CreateSimpleKeyframeEffectForTest(div, CSSPropertyID::kWidth,
                                                   "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("50px", ComputedValue("width", *StyleForId("div")));
  div->SetNeedsAnimationStyleRecalc();

  // TODO(crbug.com/41483417): Enable this optimization for styles with
  // anchor queries.
  StyleResolverState state(GetDocument(), *div);
  EXPECT_FALSE(StyleResolver::CanReuseBaseComputedStyle(state));
}

TEST_F(StyleResolverTest, NoCascadeLayers) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #a { color: green; }
      .b { font-size: 16px; }
    </style>
    <div id=a class=b></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  StyleResolverState state(GetDocument(),
                           *GetDocument().getElementById(AtomicString("a")));
  SelectorFilter filter;
  MatchResult match_result;
  ElementRuleCollector collector(state.ElementContext(), StyleRecalcContext(),
                                 filter, match_result,
                                 EInsideLink::kNotInsideLink);
  MatchAllRules(state, collector);
  const auto& properties = match_result.GetMatchedProperties();
  ASSERT_EQ(properties.size(), 4u);

  const uint16_t kImplicitOuterLayerOrder =
      ClampTo<uint16_t>(CascadeLayerMap::kImplicitOuterLayerOrder);

  // div { display: block; }
  EXPECT_TRUE(properties[0].properties->HasProperty(CSSPropertyID::kDisplay));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[0].data_.layer_order);
  EXPECT_EQ(properties[0].data_.origin, CascadeOrigin::kUserAgent);

  // div { unicode-bidi: isolate; }
  EXPECT_TRUE(
      properties[1].properties->HasProperty(CSSPropertyID::kUnicodeBidi));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[1].data_.layer_order);
  EXPECT_EQ(properties[1].data_.origin, CascadeOrigin::kUserAgent);

  // .b { font-size: 16px; }
  EXPECT_TRUE(properties[2].properties->HasProperty(CSSPropertyID::kFontSize));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[2].data_.layer_order);
  EXPECT_EQ(properties[2].data_.origin, CascadeOrigin::kAuthor);

  // #a { color: green; }
  EXPECT_TRUE(properties[3].properties->HasProperty(CSSPropertyID::kColor));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[3].data_.layer_order);
  EXPECT_EQ(properties[3].data_.origin, CascadeOrigin::kAuthor);
}

TEST_F(StyleResolverTest, CascadeLayersInDifferentSheets) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      @layer foo, bar;
      @layer bar {
        .b { color: green; }
      }
    </style>
    <style>
      @layer foo {
        #a { font-size: 16px; }
      }
    </style>
    <div id=a class=b style="font-family: custom"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  StyleResolverState state(GetDocument(),
                           *GetDocument().getElementById(AtomicString("a")));
  SelectorFilter filter;
  MatchResult match_result;
  ElementRuleCollector collector(state.ElementContext(), StyleRecalcContext(),
                                 filter, match_result,
                                 EInsideLink::kNotInsideLink);
  MatchAllRules(state, collector);
  const auto& properties = match_result.GetMatchedProperties();
  ASSERT_EQ(properties.size(), 5u);

  const uint16_t kImplicitOuterLayerOrder =
      ClampTo<uint16_t>(CascadeLayerMap::kImplicitOuterLayerOrder);

  // div { display: block; }
  EXPECT_TRUE(properties[0].properties->HasProperty(CSSPropertyID::kDisplay));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[0].data_.layer_order);
  EXPECT_EQ(properties[0].data_.origin, CascadeOrigin::kUserAgent);

  // div { unicode-bidi: isolate; }
  EXPECT_TRUE(
      properties[1].properties->HasProperty(CSSPropertyID::kUnicodeBidi));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[1].data_.layer_order);
  EXPECT_EQ(properties[1].data_.origin, CascadeOrigin::kUserAgent);

  // @layer foo { #a { font-size: 16px } }"
  EXPECT_TRUE(properties[2].properties->HasProperty(CSSPropertyID::kFontSize));
  EXPECT_EQ(0u, properties[2].data_.layer_order);
  EXPECT_EQ(properties[2].data_.origin, CascadeOrigin::kAuthor);

  // @layer bar { .b { color: green } }"
  EXPECT_TRUE(properties[3].properties->HasProperty(CSSPropertyID::kColor));
  EXPECT_EQ(1u, properties[3].data_.layer_order);
  EXPECT_EQ(properties[3].data_.origin, CascadeOrigin::kAuthor);

  // style="font-family: custom"
  EXPECT_TRUE(
      properties[4].properties->HasProperty(CSSPropertyID::kFontFamily));
  EXPECT_TRUE(properties[4].data_.is_inline_style);
  EXPECT_EQ(properties[4].data_.origin, CascadeOrigin::kAuthor);
  // There's no layer order for inline style; it's always above all layers.
}

TEST_F(StyleResolverTest, CascadeLayersInDifferentTreeScopes) {
  GetDocument().documentElement()->setHTMLUnsafe(R"HTML(
    <style>
      @layer foo {
        #host { color: green; }
      }
    </style>
    <div id=host>
      <template shadowrootmode=open>
        <style>
          @layer bar {
            :host { font-size: 16px; }
          }
        </style>
      </template>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  StyleResolverState state(GetDocument(),
                           *GetDocument().getElementById(AtomicString("host")));
  SelectorFilter filter;
  MatchResult match_result;
  ElementRuleCollector collector(state.ElementContext(), StyleRecalcContext(),
                                 filter, match_result,
                                 EInsideLink::kNotInsideLink);
  MatchAllRules(state, collector);
  const auto& properties = match_result.GetMatchedProperties();
  ASSERT_EQ(properties.size(), 4u);

  const uint16_t kImplicitOuterLayerOrder =
      ClampTo<uint16_t>(CascadeLayerMap::kImplicitOuterLayerOrder);

  // div { display: block }
  EXPECT_TRUE(properties[0].properties->HasProperty(CSSPropertyID::kDisplay));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[0].data_.layer_order);
  EXPECT_EQ(properties[0].data_.origin, CascadeOrigin::kUserAgent);

  // div { unicode-bidi: isolate; }
  EXPECT_TRUE(
      properties[1].properties->HasProperty(CSSPropertyID::kUnicodeBidi));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[1].data_.layer_order);
  EXPECT_EQ(properties[1].data_.origin, CascadeOrigin::kUserAgent);

  // @layer bar { :host { font-size: 16px } }
  EXPECT_TRUE(properties[2].properties->HasProperty(CSSPropertyID::kFontSize));
  EXPECT_EQ(0u, properties[2].data_.layer_order);
  EXPECT_EQ(properties[2].data_.origin, CascadeOrigin::kAuthor);
  EXPECT_EQ(
      match_result.ScopeFromTreeOrder(properties[2].data_.tree_order),
      GetDocument().getElementById(AtomicString("host"))->GetShadowRoot());

  // @layer foo { #host { color: green } }
  EXPECT_TRUE(properties[3].properties->HasProperty(CSSPropertyID::kColor));
  EXPECT_EQ(0u, properties[3].data_.layer_order);
  EXPECT_EQ(match_result.ScopeFromTreeOrder(properties[3].data_.tree_order),
            &GetDocument());
}

// https://crbug.com/1313357
TEST_F(StyleResolverTest, CascadeLayersAfterModifyingAnotherSheet) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      @layer {
        target { color: red; }
      }
    </style>
    <style id="addrule"></style>
    <target></target>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  GetDocument()
      .getElementById(AtomicString("addrule"))
      ->appendChild(
          GetDocument().createTextNode("target { font-size: 10px; }"));

  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(GetDocument().GetScopedStyleResolver()->GetCascadeLayerMap());

  StyleResolverState state(
      GetDocument(), *GetDocument().QuerySelector(AtomicString("target")));
  SelectorFilter filter;
  MatchResult match_result;
  ElementRuleCollector collector(state.ElementContext(), StyleRecalcContext(),
                                 filter, match_result,
                                 EInsideLink::kNotInsideLink);
  MatchAllRules(state, collector);
  const auto& properties = match_result.GetMatchedProperties();
  ASSERT_EQ(properties.size(), 2u);

  const uint16_t kImplicitOuterLayerOrder =
      ClampTo<uint16_t>(CascadeLayerMap::kImplicitOuterLayerOrder);

  // @layer { target { color: red } }"
  EXPECT_TRUE(properties[0].properties->HasProperty(CSSPropertyID::kColor));
  EXPECT_EQ(0u, properties[0].data_.layer_order);
  EXPECT_EQ(properties[0].data_.origin, CascadeOrigin::kAuthor);

  // target { font-size: 10px }
  EXPECT_TRUE(properties[1].properties->HasProperty(CSSPropertyID::kFontSize));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[1].data_.layer_order);
  EXPECT_EQ(properties[1].data_.origin, CascadeOrigin::kAuthor);
}

// https://crbug.com/1326791
TEST_F(StyleResolverTest, CascadeLayersAddLayersWithImportantDeclarations) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style id="addrule"></style>
    <target></target>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  GetDocument()
      .getElementById(AtomicString("addrule"))
      ->appendChild(GetDocument().createTextNode(
          "@layer { target { font-size: 20px !important; } }"
          "@layer { target { font-size: 10px !important; } }"));

  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(GetDocument().GetScopedStyleResolver()->GetCascadeLayerMap());

  StyleResolverState state(
      GetDocument(), *GetDocument().QuerySelector(AtomicString("target")));
  SelectorFilter filter;
  MatchResult match_result;
  ElementRuleCollector collector(state.ElementContext(), StyleRecalcContext(),
                                 filter, match_result,
                                 EInsideLink::kNotInsideLink);
  MatchAllRules(state, collector);
  const auto& properties = match_result.GetMatchedProperties();
  ASSERT_EQ(properties.size(), 2u);

  // @layer { target { font-size: 20px !important } }
  EXPECT_TRUE(properties[0].properties->HasProperty(CSSPropertyID::kFontSize));
  EXPECT_TRUE(
      properties[0].properties->PropertyIsImportant(CSSPropertyID::kFontSize));
  EXPECT_EQ("20px", properties[0].properties->GetPropertyValue(
                        CSSPropertyID::kFontSize));
  EXPECT_EQ(0u, properties[0].data_.layer_order);
  EXPECT_EQ(properties[0].data_.origin, CascadeOrigin::kAuthor);

  // @layer { target { font-size: 10px !important } }
  EXPECT_TRUE(properties[1].properties->HasProperty(CSSPropertyID::kFontSize));
  EXPECT_TRUE(
      properties[1].properties->PropertyIsImportant(CSSPropertyID::kFontSize));
  EXPECT_EQ("10px", properties[1].properties->GetPropertyValue(
                        CSSPropertyID::kFontSize));
  EXPECT_EQ(1u, properties[1].data_.layer_order);
  EXPECT_EQ(properties[1].data_.origin, CascadeOrigin::kAuthor);
}

TEST_F(StyleResolverTest, BodyPropagationLayoutImageContain) {
  GetDocument().documentElement()->setAttribute(
      html_names::kStyleAttr,
      AtomicString("contain:size; display:inline-table; content:url(img);"));
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor,
                                               "red");

  // Should not trigger DCHECK
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(Color::kTransparent,
            GetDocument().GetLayoutView()->StyleRef().VisitedDependentColor(
                GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleResolverTest, IsInertWithAttributeAndDialog) {
  Document& document = GetDocument();
  NonThrowableExceptionState exception_state;

  document.body()->setInnerHTML(R"HTML(
    <div inert>
      div_text
      <dialog>dialog_text</dialog>
    </div>
  )HTML");
  Element* html = document.documentElement();
  Element* body = document.body();
  Element* div = document.QuerySelector(AtomicString("div"));
  Node* div_text = div->firstChild();
  auto* dialog =
      To<HTMLDialogElement>(document.QuerySelector(AtomicString("dialog")));
  Node* dialog_text = dialog->firstChild();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div_text->GetLayoutObject()->StyleRef().IsInert());
  EXPECT_EQ(dialog->GetComputedStyle(), nullptr);
  EXPECT_EQ(dialog_text->GetLayoutObject(), nullptr);

  div->SetBooleanAttribute(html_names::kInertAttr, false);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div_text->GetLayoutObject()->StyleRef().IsInert());
  EXPECT_EQ(dialog->GetComputedStyle(), nullptr);
  EXPECT_EQ(dialog_text->GetLayoutObject(), nullptr);

  dialog->showModal(exception_state);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div_text->GetLayoutObject()->StyleRef().IsInert());
  EXPECT_FALSE(dialog->GetComputedStyle()->IsInert());
  EXPECT_FALSE(dialog_text->GetLayoutObject()->StyleRef().IsInert());

  div->SetBooleanAttribute(html_names::kInertAttr, true);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div_text->GetLayoutObject()->StyleRef().IsInert());
  EXPECT_FALSE(dialog->GetComputedStyle()->IsInert());
  EXPECT_FALSE(dialog_text->GetLayoutObject()->StyleRef().IsInert());

  dialog->close();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div_text->GetLayoutObject()->StyleRef().IsInert());
  EXPECT_EQ(dialog->GetComputedStyle(), nullptr);
  EXPECT_EQ(dialog_text->GetLayoutObject(), nullptr);
}

TEST_F(StyleResolverTest, IsInertWithDialogs) {
  Document& document = GetDocument();
  NonThrowableExceptionState exception_state;

  document.body()->setInnerHTML(R"HTML(
    <dialog>
      dialog1_text
      <dialog>dialog2_text</dialog>
    </dialog>
    <div>
      <dialog>dialog3_text</dialog>
    </div>
  )HTML");
  StaticElementList* dialogs =
      document.QuerySelectorAll(AtomicString("dialog"));
  Element* html = document.documentElement();
  Element* body = document.body();
  auto* dialog1 = To<HTMLDialogElement>(dialogs->item(0));
  Node* dialog1_text = dialog1->firstChild();
  auto* dialog2 = To<HTMLDialogElement>(dialogs->item(1));
  Node* dialog2_text = dialog2->firstChild();
  Element* div = document.QuerySelector(AtomicString("div"));
  auto* dialog3 = To<HTMLDialogElement>(dialogs->item(2));
  Node* dialog3_text = dialog3->firstChild();
  UpdateAllLifecyclePhasesForTest();

  auto ExpectState0 = [&]() {
    EXPECT_FALSE(html->GetComputedStyle()->IsInert());
    EXPECT_FALSE(body->GetComputedStyle()->IsInert());
    EXPECT_EQ(dialog1->GetComputedStyle(), nullptr);
    EXPECT_EQ(dialog1_text->GetLayoutObject(), nullptr);
    EXPECT_EQ(dialog2->GetComputedStyle(), nullptr);
    EXPECT_EQ(dialog2_text->GetLayoutObject(), nullptr);
    EXPECT_FALSE(div->GetComputedStyle()->IsInert());
    EXPECT_EQ(dialog3->GetComputedStyle(), nullptr);
    EXPECT_EQ(dialog3_text->GetLayoutObject(), nullptr);
  };
  ExpectState0();

  dialog1->showModal(exception_state);
  UpdateAllLifecyclePhasesForTest();

  auto ExpectState1 = [&]() {
    EXPECT_TRUE(html->GetComputedStyle()->IsInert());
    EXPECT_TRUE(body->GetComputedStyle()->IsInert());
    EXPECT_FALSE(dialog1->GetComputedStyle()->IsInert());
    EXPECT_FALSE(dialog1_text->GetLayoutObject()->StyleRef().IsInert());
    EXPECT_EQ(dialog2->GetComputedStyle(), nullptr);
    EXPECT_EQ(dialog2_text->GetLayoutObject(), nullptr);
    EXPECT_TRUE(div->GetComputedStyle()->IsInert());
    EXPECT_EQ(dialog3->GetComputedStyle(), nullptr);
    EXPECT_EQ(dialog3_text->GetLayoutObject(), nullptr);
  };
  ExpectState1();

  dialog2->showModal(exception_state);
  UpdateAllLifecyclePhasesForTest();

  auto ExpectState2 = [&]() {
    EXPECT_TRUE(html->GetComputedStyle()->IsInert());
    EXPECT_TRUE(body->GetComputedStyle()->IsInert());
    EXPECT_TRUE(dialog1->GetComputedStyle()->IsInert());
    EXPECT_TRUE(dialog1_text->GetLayoutObject()->StyleRef().IsInert());
    EXPECT_FALSE(dialog2->GetComputedStyle()->IsInert());
    EXPECT_FALSE(dialog2_text->GetLayoutObject()->StyleRef().IsInert());
    EXPECT_TRUE(div->GetComputedStyle()->IsInert());
    EXPECT_EQ(dialog3->GetComputedStyle(), nullptr);
    EXPECT_EQ(dialog3_text->GetLayoutObject(), nullptr);
  };
  ExpectState2();

  dialog3->showModal(exception_state);
  UpdateAllLifecyclePhasesForTest();

  auto ExpectState3 = [&]() {
    EXPECT_TRUE(html->GetComputedStyle()->IsInert());
    EXPECT_TRUE(body->GetComputedStyle()->IsInert());
    EXPECT_TRUE(dialog1->GetComputedStyle()->IsInert());
    EXPECT_TRUE(dialog1_text->GetLayoutObject()->StyleRef().IsInert());
    EXPECT_TRUE(dialog2->GetComputedStyle()->IsInert());
    EXPECT_TRUE(dialog2_text->GetLayoutObject()->StyleRef().IsInert());
    EXPECT_TRUE(div->GetComputedStyle()->IsInert());
    EXPECT_FALSE(dialog3->GetComputedStyle()->IsInert());
    EXPECT_FALSE(dialog3_text->GetLayoutObject()->StyleRef().IsInert());
  };
  ExpectState3();

  dialog3->close();
  UpdateAllLifecyclePhasesForTest();

  ExpectState2();

  dialog2->close();
  UpdateAllLifecyclePhasesForTest();

  ExpectState1();

  dialog1->close();
  UpdateAllLifecyclePhasesForTest();

  ExpectState0();
}

static void EnterFullscreen(Document& document, Element& element) {
  LocalFrame::NotifyUserActivation(
      document.GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(element);
  Fullscreen::DidResolveEnterFullscreenRequest(document, /*granted*/ true);
  EXPECT_EQ(Fullscreen::FullscreenElementFrom(document), element);
}

static void ExitFullscreen(Document& document) {
  Fullscreen::FullyExitFullscreen(document);
  Fullscreen::DidExitFullscreen(document);
  EXPECT_EQ(Fullscreen::FullscreenElementFrom(document), nullptr);
}

TEST_F(StyleResolverTest, IsInertWithFullscreen) {
  Document& document = GetDocument();
  document.body()->setInnerHTML(R"HTML(
    <div>
      div_text
      <span>span_text</span>
    </div>
    <p>p_text</p>
  )HTML");
  Element* html = document.documentElement();
  Element* body = document.body();
  Element* div = document.QuerySelector(AtomicString("div"));
  Node* div_text = div->firstChild();
  Element* span = document.QuerySelector(AtomicString("span"));
  Node* span_text = span->firstChild();
  Element* p = document.QuerySelector(AtomicString("p"));
  Node* p_text = p->firstChild();
  UpdateAllLifecyclePhasesForTest();

  auto ExpectState0 = [&]() {
    EXPECT_FALSE(html->GetComputedStyle()->IsInert());
    EXPECT_FALSE(body->GetComputedStyle()->IsInert());
    EXPECT_FALSE(div->GetComputedStyle()->IsInert());
    EXPECT_FALSE(div_text->GetLayoutObject()->StyleRef().IsInert());
    EXPECT_FALSE(span->GetComputedStyle()->IsInert());
    EXPECT_FALSE(span_text->GetLayoutObject()->StyleRef().IsInert());
    EXPECT_FALSE(p->GetComputedStyle()->IsInert());
    EXPECT_FALSE(p_text->GetLayoutObject()->StyleRef().IsInert());
  };
  ExpectState0();

  EnterFullscreen(document, *div);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div_text->GetLayoutObject()->StyleRef().IsInert());
  EXPECT_FALSE(span->GetComputedStyle()->IsInert());
  EXPECT_FALSE(span_text->GetLayoutObject()->StyleRef().IsInert());
  EXPECT_TRUE(p->GetComputedStyle()->IsInert());
  EXPECT_TRUE(p_text->GetLayoutObject()->StyleRef().IsInert());

  EnterFullscreen(document, *span);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div_text->GetLayoutObject()->StyleRef().IsInert());
  EXPECT_FALSE(span->GetComputedStyle()->IsInert());
  EXPECT_FALSE(span_text->GetLayoutObject()->StyleRef().IsInert());
  EXPECT_TRUE(p->GetComputedStyle()->IsInert());
  EXPECT_TRUE(p_text->GetLayoutObject()->StyleRef().IsInert());

  EnterFullscreen(document, *p);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div_text->GetLayoutObject()->StyleRef().IsInert());
  EXPECT_TRUE(span->GetComputedStyle()->IsInert());
  EXPECT_TRUE(span_text->GetLayoutObject()->StyleRef().IsInert());
  EXPECT_FALSE(p->GetComputedStyle()->IsInert());
  EXPECT_FALSE(p_text->GetLayoutObject()->StyleRef().IsInert());

  ExitFullscreen(document);
  UpdateAllLifecyclePhasesForTest();

  ExpectState0();
}

TEST_F(StyleResolverTest, IsInertWithFrameAndFullscreen) {
  Document& document = GetDocument();
  document.body()->setInnerHTML(R"HTML(
    <div>div_text</div>
  )HTML");
  Element* html = document.documentElement();
  Element* body = document.body();
  Element* div = document.QuerySelector(AtomicString("div"));
  Node* div_text = div->firstChild();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div_text->GetLayoutObject()->StyleRef().IsInert());

  EnterFullscreen(document, *div);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div_text->GetLayoutObject()->StyleRef().IsInert());

  EnterFullscreen(document, *body);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div_text->GetLayoutObject()->StyleRef().IsInert());

  EnterFullscreen(document, *html);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div_text->GetLayoutObject()->StyleRef().IsInert());
  ExitFullscreen(document);
}

TEST_F(StyleResolverTest, IsInertWithBackdrop) {
  Document& document = GetDocument();
  NonThrowableExceptionState exception_state;

  document.documentElement()->setInnerHTML(R"HTML(
    <style>:root:fullscreen::backdrop { --enable: true }</style>
    <dialog></dialog>
  )HTML");
  Element* html = document.documentElement();
  Element* body = document.body();
  auto* dialog =
      To<HTMLDialogElement>(document.QuerySelector(AtomicString("dialog")));

  auto IsBackdropInert = [](Element* element) {
    PseudoElement* backdrop = element->GetPseudoElement(kPseudoIdBackdrop);
    EXPECT_NE(backdrop, nullptr) << element;
    return backdrop->GetComputedStyle()->IsInert();
  };

  EnterFullscreen(document, *body);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(html->GetPseudoElement(kPseudoIdBackdrop), nullptr);
  EXPECT_FALSE(IsBackdropInert(body));
  EXPECT_EQ(dialog->GetPseudoElement(kPseudoIdBackdrop), nullptr);

  dialog->showModal(exception_state);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(html->GetPseudoElement(kPseudoIdBackdrop), nullptr);
  EXPECT_TRUE(IsBackdropInert(body));
  EXPECT_FALSE(IsBackdropInert(dialog));

  dialog->close();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(html->GetPseudoElement(kPseudoIdBackdrop), nullptr);
  EXPECT_FALSE(IsBackdropInert(body));
  EXPECT_EQ(dialog->GetPseudoElement(kPseudoIdBackdrop), nullptr);

  EnterFullscreen(document, *html);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(IsBackdropInert(html));
  EXPECT_FALSE(IsBackdropInert(body));
  EXPECT_EQ(dialog->GetPseudoElement(kPseudoIdBackdrop), nullptr);

  dialog->showModal(exception_state);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(IsBackdropInert(html));
  EXPECT_TRUE(IsBackdropInert(body));
  EXPECT_FALSE(IsBackdropInert(dialog));
  ExitFullscreen(document);
}

TEST_F(StyleResolverTest, IsInertWithDialogAndFullscreen) {
  Document& document = GetDocument();
  NonThrowableExceptionState exception_state;

  document.body()->setInnerHTML(R"HTML(
    <div></div>
    <dialog></dialog>
  )HTML");
  Element* html = document.documentElement();
  Element* body = document.body();
  Element* div = document.QuerySelector(AtomicString("div"));
  auto* dialog =
      To<HTMLDialogElement>(document.QuerySelector(AtomicString("dialog")));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_EQ(dialog->GetComputedStyle(), nullptr);

  EnterFullscreen(document, *div);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_EQ(dialog->GetComputedStyle(), nullptr);

  dialog->showModal(exception_state);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(dialog->GetComputedStyle()->IsInert());

  dialog->close();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_EQ(dialog->GetComputedStyle(), nullptr);

  ExitFullscreen(document);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_EQ(dialog->GetComputedStyle(), nullptr);

  dialog->showModal(exception_state);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(dialog->GetComputedStyle()->IsInert());

  EnterFullscreen(document, *div);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(dialog->GetComputedStyle()->IsInert());

  ExitFullscreen(document);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(dialog->GetComputedStyle()->IsInert());

  dialog->close();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_EQ(dialog->GetComputedStyle(), nullptr);
}

TEST_F(StyleResolverTestCQ, StyleRulesForElementContainerQuery) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #container { container-type: inline-size }
      @container (min-width: 1px) {
        #target { }
      }
      @container (min-width: 99999px) {
        #target { color: red }
      }
    </style>
    <div id="container">
      <div id="target"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto& resolver = GetDocument().GetStyleResolver();

  auto* rule_list =
      resolver.StyleRulesForElement(target, StyleResolver::kAuthorCSSRules);
  ASSERT_TRUE(rule_list);
  ASSERT_EQ(rule_list->size(), 1u)
      << "The empty #target rule in the container query should be collected";
  EXPECT_TRUE(rule_list->at(0)->Properties().IsEmpty())
      << "Check that it is in fact the empty rule";
}

TEST_F(StyleResolverTest, LegacyOverlapPerspectiveOrigin_Single) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          perspective-origin: 1px 2px;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSLegacyPerspectiveOrigin))
      << "Not counted when only perspective-origin is used";
}

TEST_F(StyleResolverTest, LegacyOverlapPerspectiveOrigin_Order) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          -webkit-perspective-origin-x: 1px;
          -webkit-perspective-origin-y: 2px;
          perspective-origin: 3px 4px;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSLegacyPerspectiveOrigin))
      << "Not counted when perspective-origin is last";
}

TEST_F(StyleResolverTest, LegacyOverlapPerspectiveOrigin_Values) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          perspective-origin: 1px 2px;
          -webkit-perspective-origin-x: 1px;
          -webkit-perspective-origin-y: 2px;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSLegacyPerspectiveOrigin))
      << "Not counted when values are the same";
}

TEST_F(StyleResolverTest, LegacyOverlapPerspectiveOrigin_Last) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          perspective-origin: 1px 2px;
          -webkit-perspective-origin-x: 3px;
          -webkit-perspective-origin-y: 4px;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSLegacyPerspectiveOrigin))
      << "Counted when -webkit-perspective-* is last with different values";
}

TEST_F(StyleResolverTest, LegacyOverlapTransformOrigin_Single) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          transform-origin: 1px 2px 3px;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSLegacyTransformOrigin))
      << "Not counted when only transform-origin is used";
}

TEST_F(StyleResolverTest, LegacyOverlapTransformOrigin_Order) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          -webkit-transform-origin-x: 1px;
          -webkit-transform-origin-y: 2px;
          -webkit-transform-origin-z: 3px;
          transform-origin: 4px 5px 6px;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSLegacyTransformOrigin))
      << "Not counted when transform-origin is last";
}

TEST_F(StyleResolverTest, LegacyOverlapTransformOrigin_Values) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          transform-origin: 1px 2px 3px;
          -webkit-transform-origin-x: 1px;
          -webkit-transform-origin-y: 2px;
          -webkit-transform-origin-z: 3px;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSLegacyTransformOrigin))
      << "Not counted when values are the same";
}

TEST_F(StyleResolverTest, LegacyOverlapTransformOrigin_Last) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          transform-origin: 1px 2px 3px;
          -webkit-transform-origin-x: 4px;
          -webkit-transform-origin-y: 5px;
          -webkit-transform-origin-z: 6px;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSLegacyTransformOrigin))
      << "Counted when -webkit-transform-origin-* is last with different "
         "values";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImage_Single) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          border-image: url("#a") 1 fill / 2 / 3 round;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSLegacyBorderImage))
      << "Not counted when only border-image is used";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImage_Order) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          -webkit-border-image: url("#b") 2 fill / 3 / 4 round;
          border-image: url("#a") 1 fill / 2 / 3 round;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSLegacyBorderImage))
      << "Not counted when border-image is last";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImage_Values) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          border-image: url("#a") 1 fill / 2 / 3 round;
          -webkit-border-image: url("#a") 1 fill / 2 / 3 round;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSLegacyBorderImage))
      << "Not counted when values are the same";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImage_Last_Source) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          border-image: url("#a") 1 fill / 2 / 3 round;
          -webkit-border-image: url("#b") 1 fill / 2 / 3 round;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSLegacyBorderImage))
      << "Counted when border-image-source differs";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImage_Last_Slice) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          border-image: url("#a") 1 fill / 2 / 3 round;
          -webkit-border-image: url("#a") 2 fill / 2 / 3 round;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSLegacyBorderImage))
      << "Counted when border-image-slice differs";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImage_Last_SliceFill) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          border-image: url("#a") 1 / 2 / 3 round;
          -webkit-border-image: url("#a") 1 fill / 2 / 3 round;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSLegacyBorderImage))
      << "Counted when the fill keyword of border-image-slice differs";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImage_SliceFillImplicit) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          border-image: url("#a") 1 / 2 / 3 round;
          -webkit-border-image: url("#a") 1 / 2 / 3 round;
        }
      </style>
      <div>target</div>
    )HTML");
  // Note that -webkit-border-image implicitly adds "fill", but
  // border-image does not.
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSLegacyBorderImage))
      << "Counted when fill-less values are the same";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImage_Last_Width) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          border-image: url("#a") 1 fill / 2 / 3 round;
          -webkit-border-image: url("#a") 1 fill / 5 / 3 round;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSLegacyBorderImage))
      << "Counted when border-image-slice differs";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImage_Last_Outset) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          border-image: url("#a") 1 fill / 2 / 3 round;
          -webkit-border-image: url("#a") 1 fill / 2 / 5 round;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSLegacyBorderImage))
      << "Counted when border-image-outset differs";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImage_Last_Repeat) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          border-image: url("#a") 1 fill / 2 / 3 round;
          -webkit-border-image: url("#a") 1 fill / 2 / 3 space;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSLegacyBorderImage))
      << "Counted when border-image-repeat differs";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImageWidth_Single) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        border: 1px solid black;
      }
    </style>
    <div>target</div>
  )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSLegacyBorderImageWidth))
      << "Not counted when only border is used";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImageWidth_Order) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        -webkit-border-image: url("#b") 2 fill / 3px / 4 round;
        border: 1px solid black;
      }
    </style>
    <div>target</div>
  )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSLegacyBorderImageWidth))
      << "Not counted when border is last";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImageWidth_Values) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        border: 1px solid black;
        -webkit-border-image: url("#b") 2 fill / 1px / 4 round;
      }
    </style>
    <div>target</div>
  )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSLegacyBorderImageWidth))
      << "Not counted when values are the same";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImageWidth_Last_Border) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          border: 1px solid black;
          -webkit-border-image: url("#a") 1 fill / 2px / 3 round;
        }
      </style>
      <div>target</div>
    )HTML");
  // Since -webkit-border-image also sets border-width, we would normally
  // expect TRUE here. However, StyleCascade always applies
  // -webkit-border-image *first*, and does not do anything to prevent
  // border-width properties from also being applied. Hence border-width
  // always wins.
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSLegacyBorderImageWidth))
      << "Not even counted when -webkit-border-image is last";
}

TEST_F(StyleResolverTest, LegacyOverlapBorderImageWidth_Last_Style) {
  // Note that border-style is relevant here because the used border-width
  // is 0px if we don'y have any border-style. See e.g.
  // ComputedStyle::BorderLeftWidth.
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          border-style: solid;
          -webkit-border-image: url("#b") 1 fill / 2px / 3 round;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSLegacyBorderImageWidth))
      << "Counted when -webkit-border-image is last and there's no "
         "border-width";
}

TEST_F(StyleResolverTest, PositionTryStylesBasic_Cascade) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @position-try --f1 { left: 100px; }
      @position-try --f2 { top: 100px; }
      @position-try --f3 { inset: 50px; }
      #target {
        position: absolute;
        position-try-fallbacks: --f1, --f2, --f3;
      }
    </style>
    <div id="target"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* target = GetElementById("target");
  const ComputedStyle* base_style = target->GetComputedStyle();
  ASSERT_TRUE(base_style);
  EXPECT_EQ(Length::Auto(), GetTop(*base_style));
  EXPECT_EQ(Length::Auto(), GetLeft(*base_style));

  UpdateStyleForOutOfFlow(*target, AtomicString("--f1"));
  const ComputedStyle* try1 = target->GetComputedStyle();
  ASSERT_TRUE(try1);
  EXPECT_EQ(Length::Auto(), GetTop(*try1));
  EXPECT_EQ(Length::Fixed(100), GetLeft(*try1));

  UpdateStyleForOutOfFlow(*target, AtomicString("--f2"));
  const ComputedStyle* try2 = target->GetComputedStyle();
  ASSERT_TRUE(try2);
  EXPECT_EQ(Length::Fixed(100), GetTop(*try2));
  EXPECT_EQ(Length::Auto(), GetLeft(*try2));

  // Shorthand should also work
  UpdateStyleForOutOfFlow(*target, AtomicString("--f3"));
  const ComputedStyle* try3 = target->GetComputedStyle();
  ASSERT_TRUE(try3);
  EXPECT_EQ(Length::Fixed(50), GetTop(*try3));
  EXPECT_EQ(Length::Fixed(50), GetLeft(*try3));
  EXPECT_EQ(Length::Fixed(50), GetBottom(*try3));
  EXPECT_EQ(Length::Fixed(50), GetRight(*try3));
}

TEST_F(StyleResolverTest, PositionTryStylesResolveLogicalProperties_Cascade) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @position-try --f1 { inset-inline-start: 100px; }
      @position-try --f2 { inset-block: 100px 90px; }
      #target {
        position: absolute;
        writing-mode: vertical-rl;
        direction: rtl;
        inset: 50px;
        position-try-fallbacks: --f1, --f2;
      }
    </style>
    <div id="target"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* target = GetElementById("target");
  const ComputedStyle* base_style = target->GetComputedStyle();
  ASSERT_TRUE(base_style);
  EXPECT_EQ(Length::Fixed(50), GetTop(*base_style));
  EXPECT_EQ(Length::Fixed(50), GetLeft(*base_style));
  EXPECT_EQ(Length::Fixed(50), GetBottom(*base_style));
  EXPECT_EQ(Length::Fixed(50), GetRight(*base_style));

  // 'inset-inline-start' should resolve to 'bottom'
  UpdateStyleForOutOfFlow(*target, AtomicString("--f1"));
  const ComputedStyle* try1 = target->GetComputedStyle();
  ASSERT_TRUE(try1);
  EXPECT_EQ(Length::Fixed(50), GetTop(*try1));
  EXPECT_EQ(Length::Fixed(50), GetLeft(*try1));
  EXPECT_EQ(Length::Fixed(100), GetBottom(*try1));
  EXPECT_EQ(Length::Fixed(50), GetRight(*try1));

  // 'inset-block' with two parameters should set 'right' and then 'left'
  UpdateStyleForOutOfFlow(*target, AtomicString("--f2"));
  const ComputedStyle* try2 = target->GetComputedStyle();
  ASSERT_TRUE(try2);
  EXPECT_EQ(Length::Fixed(50), GetTop(*try2));
  EXPECT_EQ(Length::Fixed(90), GetLeft(*try2));
  EXPECT_EQ(Length::Fixed(50), GetBottom(*try2));
  EXPECT_EQ(Length::Fixed(100), GetRight(*try2));
}

TEST_F(StyleResolverTest, PositionTryStylesResolveRelativeLengthUnits_Cascade) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @position-try --f1 { top: 2em; }
      #target {
        position: absolute;
        font-size: 20px;
        position-try-fallbacks: --f1;
      }
    </style>
    <div id="target"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* target = GetElementById("target");
  const ComputedStyle* base_style = target->GetComputedStyle();
  ASSERT_TRUE(base_style);
  EXPECT_EQ(Length::Auto(), GetTop(*base_style));

  // '2em' should resolve to '40px'
  UpdateStyleForOutOfFlow(*target, AtomicString("--f1"));
  const ComputedStyle* try1 = target->GetComputedStyle();
  ASSERT_TRUE(try1);
  EXPECT_EQ(Length::Fixed(40), GetTop(*try1));
}

TEST_F(StyleResolverTest, PositionTryStylesInBeforePseudoElement_Cascade) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @position-try --f1 { top: 50px; }
      #target::before {
        display: block;
        content: 'before';
        position: absolute;
        position-try-fallbacks: --f1;
      }
    </style>
    <div id="target"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* target = GetElementById("target");
  Element* before = target->GetPseudoElement(kPseudoIdBefore);
  ASSERT_TRUE(before);

  const ComputedStyle* base_style = before->GetComputedStyle();
  ASSERT_TRUE(base_style);
  EXPECT_EQ(Length::Auto(), GetTop(*base_style));

  // 'position-try-fallbacks' applies to ::before pseudo-element.
  UpdateStyleForOutOfFlow(*before, AtomicString("--f1"));
  const ComputedStyle* try1 = before->GetComputedStyle();
  ASSERT_TRUE(try1);
  EXPECT_EQ(Length::Fixed(50), GetTop(*try1));
}

TEST_F(StyleResolverTest, PositionTryStylesCSSWideKeywords_Cascade) {
  SetBodyInnerHTML(R"HTML(
    <style>
      /* 'revert' and 'revert-layer' are already rejected by parser */
      @position-try --f1 { top: initial }
      @position-try --f2 { left: inherit }
      @position-try --f3 { right: unset }
      #target {
        position: absolute;
        inset: 50px;
        position-try-fallbacks: --f1, --f2, --f3;
      }
      #container {
        position: absolute;
        inset: 100px;
      }
    </style>
    <div id="container">
      <div id="target"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* target = GetElementById("target");
  const ComputedStyle* base_style = target->GetComputedStyle();
  ASSERT_TRUE(base_style);
  EXPECT_EQ(Length::Fixed(50), GetTop(*base_style));
  EXPECT_EQ(Length::Fixed(50), GetLeft(*base_style));
  EXPECT_EQ(Length::Fixed(50), GetBottom(*base_style));
  EXPECT_EQ(Length::Fixed(50), GetRight(*base_style));

  UpdateStyleForOutOfFlow(*target, AtomicString("--f1"));
  const ComputedStyle* try1 = target->GetComputedStyle();
  ASSERT_TRUE(try1);
  EXPECT_EQ(Length::Auto(), GetTop(*try1));
  EXPECT_EQ(Length::Fixed(50), GetLeft(*try1));
  EXPECT_EQ(Length::Fixed(50), GetBottom(*try1));
  EXPECT_EQ(Length::Fixed(50), GetRight(*try1));

  UpdateStyleForOutOfFlow(*target, AtomicString("--f2"));
  const ComputedStyle* try2 = target->GetComputedStyle();
  ASSERT_TRUE(try2);
  EXPECT_EQ(Length::Fixed(50), GetTop(*try2));
  EXPECT_EQ(Length::Fixed(100), GetLeft(*try2));
  EXPECT_EQ(Length::Fixed(50), GetBottom(*try2));
  EXPECT_EQ(Length::Fixed(50), GetRight(*try2));

  UpdateStyleForOutOfFlow(*target, AtomicString("--f3"));
  const ComputedStyle* try3 = target->GetComputedStyle();
  ASSERT_TRUE(try3);
  EXPECT_EQ(Length::Fixed(50), GetTop(*try3));
  EXPECT_EQ(Length::Fixed(50), GetLeft(*try3));
  EXPECT_EQ(Length::Fixed(50), GetBottom(*try3));
  EXPECT_EQ(Length::Auto(), GetRight(*try3));
}

TEST_F(StyleResolverTest, PositionTryPropertyValueChange_Cascade) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @position-try --foo { top: 100px }
      @position-try --bar { left: 100px }
      #target {
        position: absolute;
        position-try-fallbacks: --foo;
      }
    </style>
    <div id="target"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* target = GetElementById("target");

  {
    const ComputedStyle* base_style = target->GetComputedStyle();
    ASSERT_TRUE(base_style);
    EXPECT_EQ(Length::Auto(), GetTop(*base_style));
    EXPECT_EQ(Length::Auto(), GetLeft(*base_style));

    UpdateStyleForOutOfFlow(*target, AtomicString("--foo"));
    const ComputedStyle* fallback = target->GetComputedStyle();
    ASSERT_TRUE(fallback);
    EXPECT_EQ(Length::Fixed(100), GetTop(*fallback));
    EXPECT_EQ(Length::Auto(), GetLeft(*fallback));
  }

  target->SetInlineStyleProperty(CSSPropertyID::kPositionTryFallbacks, "--bar");
  UpdateAllLifecyclePhasesForTest();

  {
    const ComputedStyle* base_style = target->GetComputedStyle();
    ASSERT_TRUE(base_style);
    EXPECT_EQ(Length::Auto(), GetTop(*base_style));
    EXPECT_EQ(Length::Auto(), GetLeft(*base_style));

    UpdateStyleForOutOfFlow(*target, AtomicString("--bar"));
    const ComputedStyle* fallback = target->GetComputedStyle();
    ASSERT_TRUE(fallback);
    ASSERT_TRUE(fallback);
    EXPECT_EQ(Length::Auto(), GetTop(*fallback));
    EXPECT_EQ(Length::Fixed(100), GetLeft(*fallback));
  }
}

TEST_F(StyleResolverTest, PositionTry_PaintInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @position-try --f1 { left: 2222222px; }
      @position-try --f2 { left: 3333333px; }
      @position-try --f3 { top: 100px; left: 0; }
      #target {
        position: absolute;
        left: 1111111px;
        position-try-fallbacks: --f1, --f2, --f3;
      }
    </style>
    <div id="target"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* target = GetElementById("target");
  const ComputedStyle* style = target->GetComputedStyle();
  ASSERT_TRUE(style);
  EXPECT_EQ(Length::Fixed(100), GetTop(*style));
  EXPECT_EQ(Length::Fixed(0), GetLeft(*style));

  EXPECT_FALSE(target->GetLayoutObject()->NeedsLayout());

  // Invalidate paint (but not layout).
  target->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor, "green");
  target->GetDocument().UpdateStyleAndLayoutTreeForThisDocument();

  EXPECT_FALSE(target->GetLayoutObject()->NeedsLayout());
  EXPECT_TRUE(target->GetLayoutObject()->ShouldCheckForPaintInvalidation());
}

TEST_F(StyleResolverTest, TrySet_Basic) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        position: absolute;
        left: 10px;
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetElementById("div");
  ASSERT_TRUE(div);
  EXPECT_EQ("10px", ComputedValue("left", div->ComputedStyleRef()));
  EXPECT_EQ("auto", ComputedValue("right", div->ComputedStyleRef()));

  // Resolving a style with some try set stored on Element,
  // should cause that set to be added to the cascade.

  const CSSPropertyValueSet* try_set =
      css_test_helpers::ParseDeclarationBlock(R"CSS(
      left: 20px;
      right: 30px;
  )CSS");
  ASSERT_TRUE(try_set);

  const ComputedStyle* try_style = StyleForId(
      "div",
      StyleRecalcContext{.try_set = try_set, .is_interleaved_oof = true});
  ASSERT_TRUE(try_style);
  EXPECT_EQ("20px", ComputedValue("left", *try_style));
  EXPECT_EQ("30px", ComputedValue("right", *try_style));
}

TEST_F(StyleResolverTest, TrySet_RevertLayer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        position: absolute;
        left: 10px;
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetElementById("div");
  ASSERT_TRUE(div);

  // Declarations from the try set should appear in a separate layer.

  const CSSPropertyValueSet* try_set =
      css_test_helpers::ParseDeclarationBlock(R"CSS(
      left: revert-layer;
      right: 30px;
  )CSS");
  ASSERT_TRUE(try_set);

  const ComputedStyle* try_style = StyleForId(
      "div",
      StyleRecalcContext{.try_set = try_set, .is_interleaved_oof = true});
  ASSERT_TRUE(try_style);
  EXPECT_EQ("10px", ComputedValue("left", *try_style));
  EXPECT_EQ("30px", ComputedValue("right", *try_style));
}

TEST_F(StyleResolverTest, TrySet_Revert) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        position: absolute;
        left: 10px;
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetElementById("div");
  ASSERT_TRUE(div);

  // Declarations from the try set should appear in the author origin.

  const CSSPropertyValueSet* try_set =
      css_test_helpers::ParseDeclarationBlock(R"CSS(
      left: revert;
      right: 30px;
  )CSS");
  ASSERT_TRUE(try_set);

  const ComputedStyle* try_style = StyleForId(
      "div",
      StyleRecalcContext{.try_set = try_set, .is_interleaved_oof = true});
  ASSERT_TRUE(try_style);
  EXPECT_EQ("auto", ComputedValue("left", *try_style));
  EXPECT_EQ("30px", ComputedValue("right", *try_style));
}

TEST_F(StyleResolverTest, TrySet_NonAbsPos) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        position: static;
        left: 10px;
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetElementById("div");
  ASSERT_TRUE(div);

  // Declarations from the try set should only apply when absolutely positioned.
  // If not absolutely positioned, they should behave as 'revert-layer'.

  const CSSPropertyValueSet* try_set =
      css_test_helpers::ParseDeclarationBlock(R"CSS(
      left: 20px;
      right: 30px;
  )CSS");
  ASSERT_TRUE(try_set);

  const ComputedStyle* try_style = StyleForId(
      "div",
      StyleRecalcContext{.try_set = try_set, .is_interleaved_oof = true});
  ASSERT_TRUE(try_style);
  EXPECT_EQ("10px", ComputedValue("left", *try_style));
  EXPECT_EQ("auto", ComputedValue("right", *try_style));
}

TEST_F(StyleResolverTest, TrySet_NonAbsPosDynamic) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        position: absolute;
        left: 10px;
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetElementById("div");
  ASSERT_TRUE(div);
  EXPECT_EQ("10px", ComputedValue("left", div->ComputedStyleRef()));
  EXPECT_EQ("auto", ComputedValue("right", div->ComputedStyleRef()));

  // Declarations from the try set should only apply when absolutely positioned,
  // including the cases where 'position' changes in the same style resolve.

  const CSSPropertyValueSet* try_set =
      css_test_helpers::ParseDeclarationBlock(R"CSS(
      left: 20px;
      right: 30px;
  )CSS");
  ASSERT_TRUE(try_set);

  div->SetInlineStyleProperty(CSSPropertyID::kPosition, "static");
  const ComputedStyle* try_style = StyleForId(
      "div",
      StyleRecalcContext{.try_set = try_set, .is_interleaved_oof = true});
  ASSERT_TRUE(try_style);
  EXPECT_EQ("10px", ComputedValue("left", *try_style));
  EXPECT_EQ("auto", ComputedValue("right", *try_style));
}

TEST_F(StyleResolverTest, TryTacticsSet_Flip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        position: absolute;
        left: 10px;
        right: 20px;
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetElementById("div");
  ASSERT_TRUE(div);
  EXPECT_EQ("10px", ComputedValue("left", div->ComputedStyleRef()));
  EXPECT_EQ("20px", ComputedValue("right", div->ComputedStyleRef()));

  const CSSPropertyValueSet* try_set =
      css_test_helpers::ParseDeclarationBlock(R"CSS(
      left: 100px;
      right: 200px;
  )CSS");
  ASSERT_TRUE(try_set);

  // Add a try-tactics set which flips left and right.
  auto* try_tactics_set =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  try_tactics_set->SetProperty(
      CSSPropertyID::kLeft, *MakeGarbageCollected<cssvalue::CSSFlipRevertValue>(
                                CSSPropertyID::kRight, TryTacticTransform()));
  try_tactics_set->SetProperty(
      CSSPropertyID::kRight,
      *MakeGarbageCollected<cssvalue::CSSFlipRevertValue>(
          CSSPropertyID::kLeft, TryTacticTransform()));
  ASSERT_TRUE(try_tactics_set);

  const ComputedStyle* try_style =
      StyleForId("div", StyleRecalcContext{.try_set = try_set,
                                           .try_tactics_set = try_tactics_set,
                                           .is_interleaved_oof = true});
  ASSERT_TRUE(try_style);
  EXPECT_EQ("200px", ComputedValue("left", *try_style));
  EXPECT_EQ("100px", ComputedValue("right", *try_style));
}

TEST_F(StyleResolverTest,
       PseudoElementWithAnimationAndOriginatingElementStyleChange) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          width:100px;
          height:100px;
          background:red;
        }
        div:before {
          content:"blahblahblah";
          background:blue;
          transition:all 1s;
        }
        .content:before {
          content:"blahblah";
        }
        .color:before {
          background:red;
        }
      </style>
      <div class="content color" id="target"></div>
    )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* element = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(element);
  auto* before = element->GetPseudoElement(kPseudoIdBefore);
  ASSERT_TRUE(before);

  // Remove the color class to start an animation.
  NonThrowableExceptionState exception_state;
  element->classList().remove({"color"}, exception_state);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(before->GetElementAnimations());

  // Trigger a style invalidation for the transition animation and remove the
  // class from the originating element. The latter should reset the animation
  // bit.
  before->SetNeedsAnimationStyleRecalc();
  EXPECT_TRUE(before->GetElementAnimations()->IsAnimationStyleChange());
  element->classList().remove({"content"}, exception_state);
  EXPECT_TRUE(element->NeedsStyleRecalc());

  // Element::RecalcOwnStyle should detect that the style change on the
  // "target" ancestor node requires re-computing the base style for the
  // pseudo element and skip the optimization for animation style change.
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(StyleResolverTestCQ, ContainerUnitContext) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container, #div { container-type:size; }
      #container {
        width: 200px;
        height: 200px;
      }
      #div {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="div"></div>
    </div>
  )HTML");

  Element* div = GetDocument().getElementById(AtomicString("div"));
  ASSERT_TRUE(div);

  // Don't provide a StyleRecalcContext here.
  StyleResolverState state(GetDocument(), *div);

  // To make UpdateLengthConversionData happen.
  state.SetStyle(div->ComputedStyleRef());

  EXPECT_DOUBLE_EQ(200.0, state.CssToLengthConversionData().ContainerWidth());
  EXPECT_DOUBLE_EQ(200.0, state.CssToLengthConversionData().ContainerHeight());
}

TEST_F(StyleResolverTest, ScopedAnchorName) {
  GetDocument().documentElement()->setHTMLUnsafe(R"HTML(
    <div id="outer-anchor" style="anchor-name: --outer"></div>
    <style>#host::part(anchor) { anchor-name: --part; }</style>
    <div id="host">
      <template shadowrootmode=open>
        <style>:host { anchor-name: --host; }</style>
        <div id="part" part="anchor"></div>
        <div id="inner-anchor" style="anchor-name: --inner"></div>
      </template>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* outer_anchor = GetElementById("outer-anchor");
  Element* host = GetElementById("host");
  ShadowRoot* shadow = host->GetShadowRoot();
  Element* part = shadow->getElementById(AtomicString("part"));
  Element* inner_anchor = shadow->getElementById(AtomicString("inner-anchor"));

  EXPECT_EQ(*MakeGarbageCollected<ScopedCSSName>(AtomicString("--outer"),
                                                 &GetDocument()),
            *outer_anchor->ComputedStyleRef().AnchorName()->GetNames()[0]);
  EXPECT_EQ(
      *MakeGarbageCollected<ScopedCSSName>(AtomicString("--host"), shadow),
      *host->ComputedStyleRef().AnchorName()->GetNames()[0]);
  EXPECT_EQ(*MakeGarbageCollected<ScopedCSSName>(AtomicString("--part"),
                                                 &GetDocument()),
            *part->ComputedStyleRef().AnchorName()->GetNames()[0]);
  EXPECT_EQ(
      *MakeGarbageCollected<ScopedCSSName>(AtomicString("--inner"), shadow),
      *inner_anchor->ComputedStyleRef().AnchorName()->GetNames()[0]);
}

TEST_F(StyleResolverTest, ScopedPositionAnchor) {
  GetDocument().documentElement()->setHTMLUnsafe(R"HTML(
    <div id="outer-anchor" style="position-anchor: --outer"></div>
    <style>#host::part(anchor) { position-anchor: --part; }</style>
    <div id="host">
      <template shadowrootmode=open>
        <style>:host { position-anchor: --host; }</style>
        <div id="part" part="anchor"></div>
        <div id="inner-anchor" style="position-anchor: --inner"></div>
      </template>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* outer_anchor = GetElementById("outer-anchor");
  Element* host = GetElementById("host");
  ShadowRoot* shadow = host->GetShadowRoot();
  Element* part = shadow->getElementById(AtomicString("part"));
  Element* inner_anchor = shadow->getElementById(AtomicString("inner-anchor"));

  EXPECT_EQ(*MakeGarbageCollected<ScopedCSSName>(AtomicString("--outer"),
                                                 &GetDocument()),
            *outer_anchor->ComputedStyleRef().PositionAnchor());
  EXPECT_EQ(
      *MakeGarbageCollected<ScopedCSSName>(AtomicString("--host"), shadow),
      *host->ComputedStyleRef().PositionAnchor());
  EXPECT_EQ(*MakeGarbageCollected<ScopedCSSName>(AtomicString("--part"),
                                                 &GetDocument()),
            *part->ComputedStyleRef().PositionAnchor());
  EXPECT_EQ(
      *MakeGarbageCollected<ScopedCSSName>(AtomicString("--inner"), shadow),
      *inner_anchor->ComputedStyleRef().PositionAnchor());
}

TEST_F(StyleResolverTest, NoAnchorFunction) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      div {
        left: 10px;
      }
    </style>
    <div id=div></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* div = GetDocument().getElementById(AtomicString("div"));
  ASSERT_TRUE(div);
  EXPECT_FALSE(div->ComputedStyleRef().HasAnchorFunctions());
}

TEST_F(StyleResolverTest, HasAnchorFunction) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      div {
        left: anchor(--a left);
      }
    </style>
    <div id=div></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* div = GetDocument().getElementById(AtomicString("div"));
  ASSERT_TRUE(div);
  EXPECT_TRUE(div->ComputedStyleRef().HasAnchorFunctions());
}

TEST_F(StyleResolverTest, HasAnchorFunctionImplicit) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      div {
        left: anchor(left);
      }
    </style>
    <div id=div></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* div = GetDocument().getElementById(AtomicString("div"));
  ASSERT_TRUE(div);
  EXPECT_TRUE(div->ComputedStyleRef().HasAnchorFunctions());
}

TEST_F(StyleResolverTest, HasAnchorSizeFunction) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      div {
        width: anchor-size(--a width);
      }
    </style>
    <div id=div></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* div = GetDocument().getElementById(AtomicString("div"));
  ASSERT_TRUE(div);
  EXPECT_TRUE(div->ComputedStyleRef().HasAnchorFunctions());
}

TEST_F(StyleResolverTest, HasAnchorSizeFunctionImplicit) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      div {
        width: anchor-size(width);
      }
    </style>
    <div id=div></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* div = GetDocument().getElementById(AtomicString("div"));
  ASSERT_TRUE(div);
  EXPECT_TRUE(div->ComputedStyleRef().HasAnchorFunctions());
}

TEST_F(StyleResolverTestCQ, CanAffectAnimationsMPC) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #a { transition: color 1s; }
      @container (width > 100000px) {
        #b { animation-name: anim; }
      }
    </style>
    <div id=a></div>
    <div id=b></div>
    <div id=c></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* a = GetDocument().getElementById(AtomicString("a"));
  auto* b = GetDocument().getElementById(AtomicString("b"));
  auto* c = GetDocument().getElementById(AtomicString("c"));

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(c);

  EXPECT_TRUE(a->ComputedStyleRef().CanAffectAnimations());
  EXPECT_FALSE(b->ComputedStyleRef().CanAffectAnimations());
  EXPECT_FALSE(c->ComputedStyleRef().CanAffectAnimations());
}

TEST_F(StyleResolverTest, CssRulesForElementExcludeStartingStyle) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @starting-style {
        #target {
          color: red;
        }
      }
    </style>
    <div id="wrapper" hidden>
      <span id="target"></span>
    </div>
  )HTML");

  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(target->GetComputedStyle(), nullptr);
  EXPECT_EQ(GetStyleEngine().GetStyleResolver().CssRulesForElement(target),
            nullptr);

  GetElementById("wrapper")->removeAttribute(html_names::kHiddenAttr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_NE(target->GetComputedStyle(), nullptr);
  EXPECT_EQ(GetStyleEngine().GetStyleResolver().CssRulesForElement(target),
            nullptr);
}

TEST_F(StyleResolverTest, PseudoCSSRulesForElementExcludeStartingStyle) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @starting-style {
        #target::before {
          color: red;
        }
      }
      #target::before {
        content: "X";
        color: green;
      }
    </style>
    <div id="wrapper" hidden>
      <span id="target"></span>
    </div>
  )HTML");

  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ(target->GetComputedStyle(), nullptr);
  EXPECT_EQ(target->GetPseudoElement(kPseudoIdBefore), nullptr);

  RuleIndexList* pseudo_rules =
      GetStyleEngine().GetStyleResolver().PseudoCSSRulesForElement(
          target, kPseudoIdBefore, g_null_atom);
  ASSERT_NE(pseudo_rules, nullptr);
  EXPECT_EQ(pseudo_rules->size(), 1u);

  GetElementById("wrapper")->removeAttribute(html_names::kHiddenAttr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_NE(target->GetComputedStyle(), nullptr);
  EXPECT_NE(target->GetPseudoElement(kPseudoIdBefore), nullptr);

  pseudo_rules = GetStyleEngine().GetStyleResolver().PseudoCSSRulesForElement(
      target, kPseudoIdBefore, g_null_atom);
  ASSERT_NE(pseudo_rules, nullptr);
  EXPECT_EQ(pseudo_rules->size(), 1u);
  EXPECT_EQ(pseudo_rules->at(0).first->cssText(),
            "#target::before { content: \"X\"; color: green; }");
}

TEST_F(StyleResolverTest, ResizeAutoInUANotCounted) {
  SetBodyInnerHTML(R"HTML(<textarea></textarea>)HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kCSSResizeAuto))
      << "resize:auto UA rule for textarea should not be counted";
}

TEST_F(StyleResolverTest, ResizeAutoCounted) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #resize {
        width: 100px;
        height: 100px;
        overflow: scroll;
        resize: auto;
      }
    </style>
    <div id="resize"></div>
  )HTML");
  EXPECT_TRUE(IsUseCounted(WebFeature::kCSSResizeAuto))
      << "Author style resize:auto applied to div should be counted";
}

TEST_F(StyleResolverTest, NoCursorHandIfNoCursor) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          color: blue;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kQuirksModeCursorHand));
}

TEST_F(StyleResolverTest, CursorHandIsCounted) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          cursor: hand;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_TRUE(IsUseCounted(WebFeature::kQuirksModeCursorHand));
  EXPECT_TRUE(IsUseCounted(WebFeature::kQuirksModeCursorHandApplied));
}

TEST_F(StyleResolverTest, CursorHandInStandardsModeIsIgnored) {
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          cursor: hand;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kQuirksModeCursorHand));
  EXPECT_FALSE(IsUseCounted(WebFeature::kQuirksModeCursorHandApplied));
}

TEST_F(StyleResolverTest, IEIgnoreSyntaxForCursorHandIsIgnored) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          * cursor: hand;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kQuirksModeCursorHand));
  EXPECT_FALSE(IsUseCounted(WebFeature::kQuirksModeCursorHandApplied));
}

TEST_F(StyleResolverTest, CursorHandThatLoses) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          color: blue;
          cursor: hand;
          cursor: pointer;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kQuirksModeCursorHand));
  EXPECT_FALSE(IsUseCounted(WebFeature::kQuirksModeCursorHandApplied));
}

TEST_F(StyleResolverTest, CursorHandThatWouldNotMatterIfWeIgnored) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
      <style>
        div {
          cursor: pointer;
          color: blue;
          cursor: hand;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_FALSE(IsUseCounted(WebFeature::kQuirksModeCursorHand));
  EXPECT_FALSE(IsUseCounted(WebFeature::kQuirksModeCursorHandApplied));
}

TEST_F(StyleResolverTest, CursorHandNotApplied) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
      <style>
        .doesnotexist {
          cursor: hand;
        }
      </style>
      <div>target</div>
    )HTML");
  EXPECT_TRUE(IsUseCounted(WebFeature::kQuirksModeCursorHand));
  EXPECT_FALSE(IsUseCounted(WebFeature::kQuirksModeCursorHandApplied));
}

TEST_F(StyleResolverTest, TextSizeAdjustUseCounter) {
  EXPECT_FALSE(IsUseCounted(WebFeature::kTextSizeAdjustNotAuto));
  EXPECT_FALSE(IsUseCounted(WebFeature::kTextSizeAdjustPercentNot100));

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .text-size-adjust-100 { text-size-adjust: 100%; }
      .text-size-adjust-101 { text-size-adjust: 101%; }
    </style>
    <div id="target">target</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(IsUseCounted(WebFeature::kTextSizeAdjustNotAuto));
  EXPECT_FALSE(IsUseCounted(WebFeature::kTextSizeAdjustPercentNot100));

  Element* target = GetDocument().getElementById(AtomicString("target"));
  target->setAttribute(html_names::kClassAttr,
                       AtomicString("text-size-adjust-100"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(IsUseCounted(WebFeature::kTextSizeAdjustNotAuto));
  EXPECT_FALSE(IsUseCounted(WebFeature::kTextSizeAdjustPercentNot100));

  target->setAttribute(html_names::kClassAttr,
                       AtomicString("text-size-adjust-101"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(IsUseCounted(WebFeature::kTextSizeAdjustNotAuto));
  EXPECT_TRUE(IsUseCounted(WebFeature::kTextSizeAdjustPercentNot100));
}

}  // namespace blink
