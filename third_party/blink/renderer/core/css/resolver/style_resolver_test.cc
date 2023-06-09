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
#include "third_party/blink/renderer/core/css/calculation_expression_anchor_query_node.h"
#include "third_party/blink/renderer/core/css/cascade_layer_map.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
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
  scoped_refptr<const ComputedStyle> StyleForId(AtomicString id) {
    Element* element = GetDocument().getElementById(id);
    StyleRecalcContext recalc_context;
    recalc_context.old_style = element->GetComputedStyle();
    auto style = GetStyleEngine().GetStyleResolver().ResolveStyle(
        element, recalc_context);
    DCHECK(style);
    return style;
  }

  String ComputedValue(String name, const ComputedStyle& style) {
    CSSPropertyRef ref(name, GetDocument());
    DCHECK(ref.IsValid());
    return ref.GetProperty()
        .CSSValueFromComputedStyle(style, nullptr, false)
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

  Element* div = GetDocument().getElementById("div");
  ElementAnimations& animations = div->EnsureElementAnimations();
  animations.SetAnimationStyleChange(true);

  StyleResolver& resolver = GetStyleEngine().GetStyleResolver();
  StyleRecalcContext recalc_context;
  recalc_context.old_style = div->GetComputedStyle();
  auto style1 = resolver.ResolveStyle(div, recalc_context);
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
  Element* div = GetDocument().getElementById("div");

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
  Element* div = GetDocument().getElementById("div");

  auto* effect = CreateSimpleKeyframeEffectForTest(div, CSSPropertyID::kWidth,
                                                   "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ("50px", ComputedValue("width", *StyleForId("div")));
  EXPECT_EQ("10px", ComputedValue("height", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  auto style = StyleForId("div");

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
  Element* div = GetDocument().getElementById("div");

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
  Element* div = GetDocument().getElementById("div");

  auto* effect = CreateSimpleKeyframeEffectForTest(div, CSSPropertyID::kHeight,
                                                   "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ("10px", ComputedValue("width", *StyleForId("div")));
  EXPECT_EQ("10px", ComputedValue("height", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  auto style = StyleForId("div");

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
  element->setAttribute(html_names::kIdAttr, "target");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("20px", ComputedValue("font-size", *StyleForId("target")));
  ElementAnimations* element_animations = element->GetElementAnimations();
  EXPECT_FALSE(element_animations);

  // Trigger a transition with a dependency on the parent style.
  element->setAttribute(html_names::kClassAttr, "adjust");
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
  element->setAttribute(html_names::kClassAttr, "");
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

  Element* div = GetDocument().getElementById("div");
  auto* effect = CreateSimpleKeyframeEffectForTest(
      div, CSSPropertyID::kFontSize, "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("50px", ComputedValue("font-size", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  auto computed_style = StyleForId("div");

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

  Element* div = GetDocument().getElementById("div");
  auto* effect = CreateSimpleKeyframeEffectForTest(div, CSSPropertyID::kHeight,
                                                   "50px", "100px");
  GetDocument().Timeline().Play(effect);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("50px", ComputedValue("height", *StyleForId("div")));

  div->SetNeedsAnimationStyleRecalc();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  auto computed_style = StyleForId("div");

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

  GetDocument().getElementById("host")->AttachShadowRootInternal(
      ShadowRootType::kOpen);
  UpdateAllLifecyclePhasesForTest();

  auto* none = GetDocument().getElementById("none");
  auto* inside_none = GetDocument().getElementById("inside-none");
  auto* none_image_set = GetDocument().getElementById("none-image-set");
  auto* hidden = GetDocument().getElementById("hidden");
  auto* inside_hidden = GetDocument().getElementById("inside-hidden");
  auto* contents = GetDocument().getElementById("contents");
  auto* inside_contents = GetDocument().getElementById("inside-contents");
  auto* non_slotted = GetDocument().getElementById("non-slotted");
  auto* no_pseudo = GetDocument().getElementById("no-pseudo");
  auto* first_line = GetDocument().getElementById("first-line");
  auto* first_line_span = GetDocument().getElementById("first-line-span");
  auto* first_line_none = GetDocument().getElementById("first-line-none");

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

TEST_F(StyleResolverTest, NoFetchForAtPage) {
  // Strictly, we should drop descriptors from @page rules which are not valid
  // descriptors, but as long as we apply them to ComputedStyle we should at
  // least not trigger fetches. The display:contents is here to make sure we
  // don't hit a DCHECK in StylePendingImage::ComputedCSSValue().
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      @page {
        display: contents;
        background-image: url(bg-img.png);
      }
    </style>
  )HTML");

  GetDocument().GetStyleEngine().UpdateActiveStyle();
  scoped_refptr<const ComputedStyle> page_style =
      GetDocument().GetStyleResolver().StyleForPage(0, "");
  ASSERT_TRUE(page_style);
  const CSSValue* computed_value = ComputedStyleUtils::ComputedPropertyValue(
      GetCSSPropertyBackgroundImage(), *page_style);

  const CSSValueList* bg_img_list = To<CSSValueList>(computed_value);
  EXPECT_TRUE(To<CSSImageValue>(bg_img_list->Item(0)).IsCachePending());
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

  scoped_refptr<const ComputedStyle> target_text_style =
      GetDocument().GetStyleResolver().ResolveStyle(GetDocument().body(),
                                                    StyleRecalcContext(),
                                                    target_text_style_request);
  ASSERT_TRUE(target_text_style);

  StyleRequest selection_style_style_request = pseudo_style_request;
  selection_style_style_request.pseudo_id = kPseudoIdSelection;

  scoped_refptr<const ComputedStyle> selection_style =
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

  for (const auto* pseudo_style :
       {target_text_style.get(), selection_style.get()}) {
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
  StaticElementList* lis = GetDocument().QuerySelectorAll("li");
  EXPECT_EQ(lis->length(), 10U);

  UpdateAllLifecyclePhasesForTest();
  for (unsigned i = 0; i < lis->length(); ++i) {
    Element* li = lis->item(i);
    PseudoElement* marker = li->GetPseudoElement(kPseudoIdMarker);
    PseudoElement* before =
        li->QuerySelector("b")->GetPseudoElement(kPseudoIdBefore);
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

  GetDocument().body()->SetIdAttribute("marker");
  UpdateAllLifecyclePhasesForTest();
  for (unsigned i = 0; i < lis->length(); ++i) {
    Element* li = lis->item(i);
    PseudoElement* before =
        li->QuerySelector("b")->GetPseudoElement(kPseudoIdBefore);

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
  Element* div = GetDocument().getElementById("div");
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
  Element* div = GetDocument().getElementById("div");
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
  Element* inner = GetDocument().getElementById("inner");
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
  Element* inner = GetDocument().getElementById("inner");
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
      GetDocument().getElementById("host")->AttachShadowRootInternal(
          ShadowRootType::kOpen);
  shadow_root.setInnerHTML(R"HTML(
    <style>
      slot { color: red }
    </style>
    <slot><span id="fallback"></span></slot>
  )HTML");
  Element* fallback = shadow_root.getElementById("fallback");
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
  GetDocument()
      .documentElement()
      ->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <div id=host>
      <template shadowroot=open>
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

  scoped_refptr<const ComputedStyle> a_style = a->GetComputedStyle();
  scoped_refptr<const ComputedStyle> b_style = b->GetComputedStyle();
  scoped_refptr<const ComputedStyle> c_style = c->GetComputedStyle();

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
  EXPECT_NE(a_style.get(), a->GetComputedStyle());
  EXPECT_NE(b_style.get(), b->GetComputedStyle());
  EXPECT_NE(c_style.get(), c->GetComputedStyle());
}

TEST_F(StyleResolverTest, ComputeValueStandardProperty) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #target { --color: green }
    </style>
    <div id="target"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById("target");
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
  auto tokens = CSSTokenizer(value).TokenizeToEOF();
  CSSParserTokenRange range(tokens);

  return property.Parse(CSSTokenizedValue{range, value}, *context,
                        local_context);
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

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);

  AtomicString custom_property_name = "--color";
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

  Element* host = GetDocument().getElementById("host");
  ASSERT_TRUE(host);
  ShadowRoot& root = host->AttachShadowRootInternal(ShadowRootType::kOpen);
  root.setInnerHTML(R"HTML(
    <style>
      ::slotted(span) { animation-name: anim-slotted }
      :host { font-family: myfont }
    </style>
    <div id="inner-host">
      <slot></slot>
    </div>
  )HTML");

  Element* inner_host = root.getElementById("inner-host");
  ASSERT_TRUE(inner_host);
  ShadowRoot& inner_root =
      inner_host->AttachShadowRootInternal(ShadowRootType::kOpen);
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
    ASSERT_EQ(properties.size(), 3u);

    // div { display: block }
    EXPECT_EQ(properties[0].types_.origin, CascadeOrigin::kUserAgent);

    // :host { font-family: myfont }
    EXPECT_EQ(match_result.ScopeFromTreeOrder(properties[1].types_.tree_order),
              root.GetTreeScope());
    EXPECT_EQ(properties[1].types_.origin, CascadeOrigin::kAuthor);

    // #host { animation-name: anim }
    EXPECT_EQ(properties[2].types_.origin, CascadeOrigin::kAuthor);
    EXPECT_EQ(match_result.ScopeFromTreeOrder(properties[2].types_.tree_order),
              host->GetTreeScope());
  }

  {
    auto* span = GetDocument().getElementById("slotted");
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
    EXPECT_EQ(properties[0].types_.origin, CascadeOrigin::kAuthor);
    EXPECT_EQ(match_result.ScopeFromTreeOrder(properties[0].types_.tree_order),
              inner_root.GetTreeScope());

    // ::slotted(span) { animation-name: anim-slotted }
    EXPECT_EQ(properties[1].types_.origin, CascadeOrigin::kAuthor);
    EXPECT_EQ(match_result.ScopeFromTreeOrder(properties[1].types_.tree_order),
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

  auto* child = GetDocument().getElementById("child");
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

  ASSERT_TRUE(style->MaskImage());
  EXPECT_FALSE(style->MaskImage()->IsPendingImage())
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

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  const auto* element_style = target->GetComputedStyle();
  ASSERT_TRUE(element_style);

  StyleRequest pseudo_style_request;
  pseudo_style_request.parent_override = element_style;
  pseudo_style_request.layout_parent_override = element_style;
  pseudo_style_request.originating_element_style = element_style;
  pseudo_style_request.pseudo_id = kPseudoIdSelection;
  scoped_refptr<const ComputedStyle> selection_style =
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

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  const auto* element_style = target->GetComputedStyle();
  ASSERT_TRUE(element_style);

  StyleRequest pseudo_style_request;
  pseudo_style_request.parent_override = element_style;
  pseudo_style_request.layout_parent_override = element_style;
  pseudo_style_request.originating_element_style = element_style;
  pseudo_style_request.pseudo_id = kPseudoIdSelection;
  scoped_refptr<const ComputedStyle> selection_style =
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

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  const auto* element_style = target->GetComputedStyle();
  ASSERT_TRUE(element_style);

  StyleRequest pseudo_style_request;
  pseudo_style_request.parent_override = element_style;
  pseudo_style_request.layout_parent_override = element_style;
  pseudo_style_request.originating_element_style = element_style;
  pseudo_style_request.pseudo_id = kPseudoIdSelection;
  scoped_refptr<const ComputedStyle> selection_style =
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

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  const auto* element_style = target->GetComputedStyle();
  ASSERT_TRUE(element_style);

  StyleRequest pseudo_style_request;
  pseudo_style_request.parent_override = element_style;
  pseudo_style_request.layout_parent_override = element_style;
  pseudo_style_request.originating_element_style = element_style;
  pseudo_style_request.pseudo_id = kPseudoIdSelection;
  scoped_refptr<const ComputedStyle> selection_style =
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

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  const auto* element_style = target->GetComputedStyle();
  ASSERT_TRUE(element_style);

  StyleRequest pseudo_style_request;
  pseudo_style_request.parent_override = element_style;
  pseudo_style_request.layout_parent_override = element_style;
  pseudo_style_request.originating_element_style = element_style;
  pseudo_style_request.pseudo_id = kPseudoIdSelection;
  scoped_refptr<const ComputedStyle> selection_style =
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

  auto* a = GetDocument().getElementById("a");
  auto* b = GetDocument().getElementById("b");
  auto* c = GetDocument().getElementById("c");
  auto* d = GetDocument().getElementById("d");
  auto* e = GetDocument().getElementById("e");

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

  auto* a = GetDocument().getElementById("a");
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

  auto* a = GetDocument().getElementById("a");
  auto* b = GetDocument().getElementById("b");

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

  auto* a = GetDocument().getElementById("a");
  auto* b = GetDocument().getElementById("b");
  auto* c = GetDocument().getElementById("c");
  auto* d = GetDocument().getElementById("d");
  auto* e = GetDocument().getElementById("e");

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

TEST_F(StyleResolverTest, NoCascadeLayers) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #a { color: green; }
      .b { font-size: 16px; }
    </style>
    <div id=a class=b></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  StyleResolverState state(GetDocument(), *GetDocument().getElementById("a"));
  SelectorFilter filter;
  MatchResult match_result;
  ElementRuleCollector collector(state.ElementContext(), StyleRecalcContext(),
                                 filter, match_result,
                                 EInsideLink::kNotInsideLink);
  MatchAllRules(state, collector);
  const auto& properties = match_result.GetMatchedProperties();
  ASSERT_EQ(properties.size(), 3u);

  const uint16_t kImplicitOuterLayerOrder =
      ClampTo<uint16_t>(CascadeLayerMap::kImplicitOuterLayerOrder);

  // div { display: block; }
  EXPECT_TRUE(properties[0].properties->HasProperty(CSSPropertyID::kDisplay));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[0].types_.layer_order);
  EXPECT_EQ(properties[0].types_.origin, CascadeOrigin::kUserAgent);

  // .b { font-size: 16px; }
  EXPECT_TRUE(properties[1].properties->HasProperty(CSSPropertyID::kFontSize));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[1].types_.layer_order);
  EXPECT_EQ(properties[1].types_.origin, CascadeOrigin::kAuthor);

  // #a { color: green; }
  EXPECT_TRUE(properties[2].properties->HasProperty(CSSPropertyID::kColor));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[2].types_.layer_order);
  EXPECT_EQ(properties[2].types_.origin, CascadeOrigin::kAuthor);
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

  StyleResolverState state(GetDocument(), *GetDocument().getElementById("a"));
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
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[0].types_.layer_order);
  EXPECT_EQ(properties[0].types_.origin, CascadeOrigin::kUserAgent);

  // @layer foo { #a { font-size: 16px } }"
  EXPECT_TRUE(properties[1].properties->HasProperty(CSSPropertyID::kFontSize));
  EXPECT_EQ(0u, properties[1].types_.layer_order);
  EXPECT_EQ(properties[1].types_.origin, CascadeOrigin::kAuthor);

  // @layer bar { .b { color: green } }"
  EXPECT_TRUE(properties[2].properties->HasProperty(CSSPropertyID::kColor));
  EXPECT_EQ(1u, properties[2].types_.layer_order);
  EXPECT_EQ(properties[2].types_.origin, CascadeOrigin::kAuthor);

  // style="font-family: custom"
  EXPECT_TRUE(
      properties[3].properties->HasProperty(CSSPropertyID::kFontFamily));
  EXPECT_TRUE(properties[3].types_.is_inline_style);
  EXPECT_EQ(properties[3].types_.origin, CascadeOrigin::kAuthor);
  // There's no layer order for inline style; it's always above all layers.
}

TEST_F(StyleResolverTest, CascadeLayersInDifferentTreeScopes) {
  GetDocument()
      .documentElement()
      ->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <style>
      @layer foo {
        #host { color: green; }
      }
    </style>
    <div id=host>
      <template shadowroot=open>
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
                           *GetDocument().getElementById("host"));
  SelectorFilter filter;
  MatchResult match_result;
  ElementRuleCollector collector(state.ElementContext(), StyleRecalcContext(),
                                 filter, match_result,
                                 EInsideLink::kNotInsideLink);
  MatchAllRules(state, collector);
  const auto& properties = match_result.GetMatchedProperties();
  ASSERT_EQ(properties.size(), 3u);

  const uint16_t kImplicitOuterLayerOrder =
      ClampTo<uint16_t>(CascadeLayerMap::kImplicitOuterLayerOrder);

  // div { display: block }
  EXPECT_TRUE(properties[0].properties->HasProperty(CSSPropertyID::kDisplay));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[0].types_.layer_order);
  EXPECT_EQ(properties[0].types_.origin, CascadeOrigin::kUserAgent);

  // @layer bar { :host { font-size: 16px } }
  EXPECT_TRUE(properties[1].properties->HasProperty(CSSPropertyID::kFontSize));
  EXPECT_EQ(0u, properties[1].types_.layer_order);
  EXPECT_EQ(properties[1].types_.origin, CascadeOrigin::kAuthor);
  EXPECT_EQ(match_result.ScopeFromTreeOrder(properties[1].types_.tree_order),
            GetDocument().getElementById("host")->GetShadowRoot());

  // @layer foo { #host { color: green } }
  EXPECT_TRUE(properties[2].properties->HasProperty(CSSPropertyID::kColor));
  EXPECT_EQ(0u, properties[2].types_.layer_order);
  EXPECT_EQ(match_result.ScopeFromTreeOrder(properties[2].types_.tree_order),
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

  GetDocument().getElementById("addrule")->appendChild(
      GetDocument().createTextNode("target { font-size: 10px; }"));

  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(GetDocument().GetScopedStyleResolver()->GetCascadeLayerMap());

  StyleResolverState state(GetDocument(),
                           *GetDocument().QuerySelector("target"));
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
  EXPECT_EQ(0u, properties[0].types_.layer_order);
  EXPECT_EQ(properties[0].types_.origin, CascadeOrigin::kAuthor);

  // target { font-size: 10px }
  EXPECT_TRUE(properties[1].properties->HasProperty(CSSPropertyID::kFontSize));
  EXPECT_EQ(kImplicitOuterLayerOrder, properties[1].types_.layer_order);
  EXPECT_EQ(properties[1].types_.origin, CascadeOrigin::kAuthor);
}

// https://crbug.com/1326791
TEST_F(StyleResolverTest, CascadeLayersAddLayersWithImportantDeclarations) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style id="addrule"></style>
    <target></target>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  GetDocument().getElementById("addrule")->appendChild(
      GetDocument().createTextNode(
          "@layer { target { font-size: 20px !important; } }"
          "@layer { target { font-size: 10px !important; } }"));

  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(GetDocument().GetScopedStyleResolver()->GetCascadeLayerMap());

  StyleResolverState state(GetDocument(),
                           *GetDocument().QuerySelector("target"));
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
  EXPECT_EQ(0u, properties[0].types_.layer_order);
  EXPECT_EQ(properties[0].types_.origin, CascadeOrigin::kAuthor);

  // @layer { target { font-size: 10px !important } }
  EXPECT_TRUE(properties[1].properties->HasProperty(CSSPropertyID::kFontSize));
  EXPECT_TRUE(
      properties[1].properties->PropertyIsImportant(CSSPropertyID::kFontSize));
  EXPECT_EQ("10px", properties[1].properties->GetPropertyValue(
                        CSSPropertyID::kFontSize));
  EXPECT_EQ(1u, properties[1].types_.layer_order);
  EXPECT_EQ(properties[1].types_.origin, CascadeOrigin::kAuthor);
}

// TODO(crbug.com/1095765): We should have a WPT for this test case, but
// currently Blink web test runner can't test @page rules in WPT.
TEST_F(StyleResolverTest, CascadeLayersAndPageRules) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
    @page { margin-top: 100px; }
    @layer {
      @page { margin-top: 50px; }
    }
    </style>
  )HTML");

  constexpr gfx::SizeF initial_page_size(800, 600);

  GetDocument().GetFrame()->StartPrinting(initial_page_size, initial_page_size);
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();

  WebPrintPageDescription description;
  GetDocument().GetPageDescription(0, &description);

  // The layered declaraion should win the cascading.
  EXPECT_EQ(100, description.margin_top);
}

TEST_F(StyleResolverTest, BodyPropagationLayoutImageContain) {
  GetDocument().documentElement()->setAttribute(
      html_names::kStyleAttr,
      "contain:size; display:inline-table; content:url(img);");
  GetDocument().body()->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor,
                                               "red");

  // Should not trigger DCHECK
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(Color::kTransparent,
            GetDocument().GetLayoutView()->StyleRef().VisitedDependentColor(
                GetCSSPropertyBackgroundColor()));
}

TEST_F(StyleResolverTest, IsInertWithAttributeAndDialog) {
  ScopedInertAttributeForTest enabled_scope(true);
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
  Element* div = document.QuerySelector("div");
  Node* div_text = div->firstChild();
  auto* dialog = To<HTMLDialogElement>(document.QuerySelector("dialog"));
  Node* dialog_text = dialog->firstChild();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document.GetComputedStyle()->IsInert());
  EXPECT_FALSE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div_text->GetComputedStyle()->IsInert());
  EXPECT_EQ(dialog->GetComputedStyle(), nullptr);
  EXPECT_EQ(dialog_text->GetComputedStyle(), nullptr);

  div->SetBooleanAttribute(html_names::kInertAttr, false);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document.GetComputedStyle()->IsInert());
  EXPECT_FALSE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div_text->GetComputedStyle()->IsInert());
  EXPECT_EQ(dialog->GetComputedStyle(), nullptr);
  EXPECT_EQ(dialog_text->GetComputedStyle(), nullptr);

  dialog->showModal(exception_state);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document.GetComputedStyle()->IsInert());
  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div_text->GetComputedStyle()->IsInert());
  EXPECT_FALSE(dialog->GetComputedStyle()->IsInert());
  EXPECT_FALSE(dialog_text->GetComputedStyle()->IsInert());

  div->SetBooleanAttribute(html_names::kInertAttr, true);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document.GetComputedStyle()->IsInert());
  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div_text->GetComputedStyle()->IsInert());
  EXPECT_FALSE(dialog->GetComputedStyle()->IsInert());
  EXPECT_FALSE(dialog_text->GetComputedStyle()->IsInert());

  dialog->close();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document.GetComputedStyle()->IsInert());
  EXPECT_FALSE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div_text->GetComputedStyle()->IsInert());
  EXPECT_EQ(dialog->GetComputedStyle(), nullptr);
  EXPECT_EQ(dialog_text->GetComputedStyle(), nullptr);
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
  StaticElementList* dialogs = document.QuerySelectorAll("dialog");
  Element* html = document.documentElement();
  Element* body = document.body();
  auto* dialog1 = To<HTMLDialogElement>(dialogs->item(0));
  Node* dialog1_text = dialog1->firstChild();
  auto* dialog2 = To<HTMLDialogElement>(dialogs->item(1));
  Node* dialog2_text = dialog2->firstChild();
  Element* div = document.QuerySelector("div");
  auto* dialog3 = To<HTMLDialogElement>(dialogs->item(2));
  Node* dialog3_text = dialog3->firstChild();
  UpdateAllLifecyclePhasesForTest();

  auto ExpectState0 = [&]() {
    EXPECT_FALSE(document.GetComputedStyle()->IsInert());
    EXPECT_FALSE(html->GetComputedStyle()->IsInert());
    EXPECT_FALSE(body->GetComputedStyle()->IsInert());
    EXPECT_EQ(dialog1->GetComputedStyle(), nullptr);
    EXPECT_EQ(dialog1_text->GetComputedStyle(), nullptr);
    EXPECT_EQ(dialog2->GetComputedStyle(), nullptr);
    EXPECT_EQ(dialog2_text->GetComputedStyle(), nullptr);
    EXPECT_FALSE(div->GetComputedStyle()->IsInert());
    EXPECT_EQ(dialog3->GetComputedStyle(), nullptr);
    EXPECT_EQ(dialog3_text->GetComputedStyle(), nullptr);
  };
  ExpectState0();

  dialog1->showModal(exception_state);
  UpdateAllLifecyclePhasesForTest();

  auto ExpectState1 = [&]() {
    EXPECT_FALSE(document.GetComputedStyle()->IsInert());
    EXPECT_TRUE(html->GetComputedStyle()->IsInert());
    EXPECT_TRUE(body->GetComputedStyle()->IsInert());
    EXPECT_FALSE(dialog1->GetComputedStyle()->IsInert());
    EXPECT_FALSE(dialog1_text->GetComputedStyle()->IsInert());
    EXPECT_EQ(dialog2->GetComputedStyle(), nullptr);
    EXPECT_EQ(dialog2_text->GetComputedStyle(), nullptr);
    EXPECT_TRUE(div->GetComputedStyle()->IsInert());
    EXPECT_EQ(dialog3->GetComputedStyle(), nullptr);
    EXPECT_EQ(dialog3_text->GetComputedStyle(), nullptr);
  };
  ExpectState1();

  dialog2->showModal(exception_state);
  UpdateAllLifecyclePhasesForTest();

  auto ExpectState2 = [&]() {
    EXPECT_FALSE(document.GetComputedStyle()->IsInert());
    EXPECT_TRUE(html->GetComputedStyle()->IsInert());
    EXPECT_TRUE(body->GetComputedStyle()->IsInert());
    EXPECT_TRUE(dialog1->GetComputedStyle()->IsInert());
    EXPECT_TRUE(dialog1_text->GetComputedStyle()->IsInert());
    EXPECT_FALSE(dialog2->GetComputedStyle()->IsInert());
    EXPECT_FALSE(dialog2_text->GetComputedStyle()->IsInert());
    EXPECT_TRUE(div->GetComputedStyle()->IsInert());
    EXPECT_EQ(dialog3->GetComputedStyle(), nullptr);
    EXPECT_EQ(dialog3_text->GetComputedStyle(), nullptr);
  };
  ExpectState2();

  dialog3->showModal(exception_state);
  UpdateAllLifecyclePhasesForTest();

  auto ExpectState3 = [&]() {
    EXPECT_FALSE(document.GetComputedStyle()->IsInert());
    EXPECT_TRUE(html->GetComputedStyle()->IsInert());
    EXPECT_TRUE(body->GetComputedStyle()->IsInert());
    EXPECT_TRUE(dialog1->GetComputedStyle()->IsInert());
    EXPECT_TRUE(dialog1_text->GetComputedStyle()->IsInert());
    EXPECT_TRUE(dialog2->GetComputedStyle()->IsInert());
    EXPECT_TRUE(dialog2_text->GetComputedStyle()->IsInert());
    EXPECT_TRUE(div->GetComputedStyle()->IsInert());
    EXPECT_FALSE(dialog3->GetComputedStyle()->IsInert());
    EXPECT_FALSE(dialog3_text->GetComputedStyle()->IsInert());
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
  Element* div = document.QuerySelector("div");
  Node* div_text = div->firstChild();
  Element* span = document.QuerySelector("span");
  Node* span_text = span->firstChild();
  Element* p = document.QuerySelector("p");
  Node* p_text = p->firstChild();
  UpdateAllLifecyclePhasesForTest();

  auto ExpectState0 = [&]() {
    EXPECT_FALSE(document.GetComputedStyle()->IsInert());
    EXPECT_FALSE(html->GetComputedStyle()->IsInert());
    EXPECT_FALSE(body->GetComputedStyle()->IsInert());
    EXPECT_FALSE(div->GetComputedStyle()->IsInert());
    EXPECT_FALSE(div_text->GetComputedStyle()->IsInert());
    EXPECT_FALSE(span->GetComputedStyle()->IsInert());
    EXPECT_FALSE(span_text->GetComputedStyle()->IsInert());
    EXPECT_FALSE(p->GetComputedStyle()->IsInert());
    EXPECT_FALSE(p_text->GetComputedStyle()->IsInert());
  };
  ExpectState0();

  EnterFullscreen(document, *div);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document.GetComputedStyle()->IsInert());
  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div_text->GetComputedStyle()->IsInert());
  EXPECT_FALSE(span->GetComputedStyle()->IsInert());
  EXPECT_FALSE(span_text->GetComputedStyle()->IsInert());
  EXPECT_TRUE(p->GetComputedStyle()->IsInert());
  EXPECT_TRUE(p_text->GetComputedStyle()->IsInert());

  EnterFullscreen(document, *span);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document.GetComputedStyle()->IsInert());
  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div_text->GetComputedStyle()->IsInert());
  EXPECT_FALSE(span->GetComputedStyle()->IsInert());
  EXPECT_FALSE(span_text->GetComputedStyle()->IsInert());
  EXPECT_TRUE(p->GetComputedStyle()->IsInert());
  EXPECT_TRUE(p_text->GetComputedStyle()->IsInert());

  EnterFullscreen(document, *p);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document.GetComputedStyle()->IsInert());
  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div->GetComputedStyle()->IsInert());
  EXPECT_TRUE(div_text->GetComputedStyle()->IsInert());
  EXPECT_TRUE(span->GetComputedStyle()->IsInert());
  EXPECT_TRUE(span_text->GetComputedStyle()->IsInert());
  EXPECT_FALSE(p->GetComputedStyle()->IsInert());
  EXPECT_FALSE(p_text->GetComputedStyle()->IsInert());

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
  Element* div = document.QuerySelector("div");
  Node* div_text = div->firstChild();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document.GetComputedStyle()->IsInert());
  EXPECT_FALSE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div_text->GetComputedStyle()->IsInert());

  EnterFullscreen(document, *div);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document.GetComputedStyle()->IsInert());
  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_TRUE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div_text->GetComputedStyle()->IsInert());

  EnterFullscreen(document, *body);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document.GetComputedStyle()->IsInert());
  EXPECT_TRUE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div_text->GetComputedStyle()->IsInert());

  EnterFullscreen(document, *html);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document.GetComputedStyle()->IsInert());
  EXPECT_FALSE(html->GetComputedStyle()->IsInert());
  EXPECT_FALSE(body->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div->GetComputedStyle()->IsInert());
  EXPECT_FALSE(div_text->GetComputedStyle()->IsInert());
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
  auto* dialog = To<HTMLDialogElement>(document.QuerySelector("dialog"));

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
  EXPECT_FALSE(IsBackdropInert(body));
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

  EXPECT_FALSE(IsBackdropInert(html));
  EXPECT_FALSE(IsBackdropInert(body));
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
  Element* div = document.QuerySelector("div");
  auto* dialog = To<HTMLDialogElement>(document.QuerySelector("dialog"));
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

  auto* target = GetDocument().getElementById("target");
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

TEST_F(StyleResolverTest, PositionFallbackStylesBasic) {
  ScopedCSSAnchorPositioningForTest enabled(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      @position-fallback --fallback {
        @try { left: 100px; }
        @try { top: 100px; }
        @try { inset: 50px; }
      }
      #target {
        position: absolute;
        position-fallback: --fallback;
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

  const ComputedStyle* try1 = target->StyleForPositionFallback(0);
  ASSERT_TRUE(try1);
  EXPECT_EQ(Length::Auto(), GetTop(*try1));
  EXPECT_EQ(Length::Fixed(100), GetLeft(*try1));

  const ComputedStyle* try2 = target->StyleForPositionFallback(1);
  ASSERT_TRUE(try2);
  EXPECT_EQ(Length::Fixed(100), GetTop(*try2));
  EXPECT_EQ(Length::Auto(), GetLeft(*try2));

  // Shorthand should also work
  const ComputedStyle* try3 = target->StyleForPositionFallback(2);
  ASSERT_TRUE(try3);
  EXPECT_EQ(Length::Fixed(50), GetTop(*try3));
  EXPECT_EQ(Length::Fixed(50), GetLeft(*try3));
  EXPECT_EQ(Length::Fixed(50), GetBottom(*try3));
  EXPECT_EQ(Length::Fixed(50), GetRight(*try3));

  // Returns nullptr when index is out of bound.
  EXPECT_FALSE(target->StyleForPositionFallback(3));
}

TEST_F(StyleResolverTest, PositionFallbackNameInvalid) {
  ScopedCSSAnchorPositioningForTest enabled(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        position: absolute;
        position-fallback: --invalid;
      }
    </style>
    <div id="target"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* target = GetElementById("target");
  EXPECT_FALSE(target->StyleForPositionFallback(0));
}

TEST_F(StyleResolverTest, PositionFallbackStylesResolveLogicalProperties) {
  ScopedCSSAnchorPositioningForTest enabled(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      @position-fallback --fallback {
        @try { inset-inline-start: 100px; }
        @try { inset-block: 100px 90px; }
      }
      #target {
        position: absolute;
        writing-mode: vertical-rl;
        direction: rtl;
        inset: 50px;
        position-fallback: --fallback;
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
  const ComputedStyle* try1 = target->StyleForPositionFallback(0);
  ASSERT_TRUE(try1);
  EXPECT_EQ(Length::Fixed(50), GetTop(*try1));
  EXPECT_EQ(Length::Fixed(50), GetLeft(*try1));
  EXPECT_EQ(Length::Fixed(100), GetBottom(*try1));
  EXPECT_EQ(Length::Fixed(50), GetRight(*try1));

  // 'inset-block' with two parameters should set 'right' and then 'left'
  const ComputedStyle* try2 = target->StyleForPositionFallback(1);
  ASSERT_TRUE(try2);
  EXPECT_EQ(Length::Fixed(50), GetTop(*try2));
  EXPECT_EQ(Length::Fixed(90), GetLeft(*try2));
  EXPECT_EQ(Length::Fixed(50), GetBottom(*try2));
  EXPECT_EQ(Length::Fixed(100), GetRight(*try2));

  EXPECT_FALSE(target->StyleForPositionFallback(2));
}

TEST_F(StyleResolverTest, PositionFallbackStylesResolveRelativeLengthUnits) {
  ScopedCSSAnchorPositioningForTest enabled(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      @position-fallback --fallback {
        @try { top: 2em; }
      }
      #target {
        position: absolute;
        font-size: 20px;
        position-fallback: --fallback;
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
  const ComputedStyle* try1 = target->StyleForPositionFallback(0);
  ASSERT_TRUE(try1);
  EXPECT_EQ(Length::Fixed(40), GetTop(*try1));

  EXPECT_FALSE(target->StyleForPositionFallback(1));
}

TEST_F(StyleResolverTest, PositionFallbackStylesInBeforePseudoElement) {
  ScopedCSSAnchorPositioningForTest enabled(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      @position-fallback --fallback {
        @try { top: 50px; }
      }
      #target::before {
        display: block;
        content: 'before';
        position: absolute;
        position-fallback: --fallback;
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

  // 'position-fallback' applies to ::before pseudo-element.
  const ComputedStyle* try1 = before->StyleForPositionFallback(0);
  ASSERT_TRUE(try1);
  EXPECT_EQ(Length::Fixed(50), GetTop(*try1));

  EXPECT_FALSE(before->StyleForPositionFallback(1));
}

TEST_F(StyleResolverTest, PositionFallbackStylesCSSWideKeywords) {
  ScopedCSSAnchorPositioningForTest enabled(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      @position-fallback --fallback {
        @try { top: initial }
        @try { left: inherit }
        @try { right: unset }
        /* 'revert' and 'revert-layer' are already rejected by parser */
      }
      #target {
        position: absolute;
        inset: 50px;
        position-fallback: --fallback;
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

  const ComputedStyle* try1 = target->StyleForPositionFallback(0);
  ASSERT_TRUE(try1);
  EXPECT_EQ(Length::Auto(), GetTop(*try1));
  EXPECT_EQ(Length::Fixed(50), GetLeft(*try1));
  EXPECT_EQ(Length::Fixed(50), GetBottom(*try1));
  EXPECT_EQ(Length::Fixed(50), GetRight(*try1));

  const ComputedStyle* try2 = target->StyleForPositionFallback(1);
  ASSERT_TRUE(try2);
  EXPECT_EQ(Length::Fixed(50), GetTop(*try2));
  EXPECT_EQ(Length::Fixed(100), GetLeft(*try2));
  EXPECT_EQ(Length::Fixed(50), GetBottom(*try2));
  EXPECT_EQ(Length::Fixed(50), GetRight(*try2));

  const ComputedStyle* try3 = target->StyleForPositionFallback(2);
  ASSERT_TRUE(try3);
  EXPECT_EQ(Length::Fixed(50), GetTop(*try3));
  EXPECT_EQ(Length::Fixed(50), GetLeft(*try3));
  EXPECT_EQ(Length::Fixed(50), GetBottom(*try3));
  EXPECT_EQ(Length::Auto(), GetRight(*try3));

  EXPECT_FALSE(target->StyleForPositionFallback(3));
}

TEST_F(StyleResolverTest, PositionFallbackPropertyValueChange) {
  ScopedCSSAnchorPositioningForTest enabled(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      @position-fallback --foo {
        @try { top: 100px }
      }
      @position-fallback --bar {
        @try { left: 100px }
      }
      #target {
        position: absolute;
        position-fallback: --foo;
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

    const ComputedStyle* fallback = target->StyleForPositionFallback(0);
    ASSERT_TRUE(fallback);
    EXPECT_EQ(Length::Fixed(100), GetTop(*fallback));
    EXPECT_EQ(Length::Auto(), GetLeft(*fallback));

    EXPECT_FALSE(target->StyleForPositionFallback(1));
  }

  target->SetInlineStyleProperty(CSSPropertyID::kPositionFallback, "--bar");
  UpdateAllLifecyclePhasesForTest();

  {
    const ComputedStyle* base_style = target->GetComputedStyle();
    ASSERT_TRUE(base_style);
    EXPECT_EQ(Length::Auto(), GetTop(*base_style));
    EXPECT_EQ(Length::Auto(), GetLeft(*base_style));

    const ComputedStyle* fallback = target->StyleForPositionFallback(0);
    ASSERT_TRUE(fallback);
    ASSERT_TRUE(fallback);
    EXPECT_EQ(Length::Auto(), GetTop(*fallback));
    EXPECT_EQ(Length::Fixed(100), GetLeft(*fallback));

    EXPECT_FALSE(target->StyleForPositionFallback(1));
  }
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

  auto* element = GetDocument().getElementById("target");
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

  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);

  // Don't provide a StyleRecalcContext here.
  StyleResolverState state(GetDocument(), *div);

  // To make UpdateLengthConversionData happen.
  state.SetStyle(div->ComputedStyleRef());

  EXPECT_DOUBLE_EQ(200.0, state.CssToLengthConversionData().ContainerWidth());
  EXPECT_DOUBLE_EQ(200.0, state.CssToLengthConversionData().ContainerHeight());
}

TEST_F(StyleResolverTest, ScopedAnchorName) {
  GetDocument()
      .documentElement()
      ->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <div id="outer-anchor" style="anchor-name: --outer"></div>
    <style>#host::part(anchor) { anchor-name: --part; }</style>
    <div id="host">
      <template shadowroot=open>
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
  Element* part = shadow->getElementById("part");
  Element* inner_anchor = shadow->getElementById("inner-anchor");

  EXPECT_EQ(*MakeGarbageCollected<ScopedCSSName>("--outer", &GetDocument()),
            *outer_anchor->ComputedStyleRef().AnchorName());
  EXPECT_EQ(*MakeGarbageCollected<ScopedCSSName>("--host", shadow),
            *host->ComputedStyleRef().AnchorName());
  EXPECT_EQ(*MakeGarbageCollected<ScopedCSSName>("--part", &GetDocument()),
            *part->ComputedStyleRef().AnchorName());
  EXPECT_EQ(*MakeGarbageCollected<ScopedCSSName>("--inner", shadow),
            *inner_anchor->ComputedStyleRef().AnchorName());
}

TEST_F(StyleResolverTest, ScopedAnchorScroll) {
  GetDocument()
      .documentElement()
      ->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <div id="outer-anchor" style="anchor-scroll: --outer"></div>
    <style>#host::part(anchor) { anchor-scroll: --part; }</style>
    <div id="host">
      <template shadowroot=open>
        <style>:host { anchor-scroll: --host; }</style>
        <div id="part" part="anchor"></div>
        <div id="inner-anchor" style="anchor-scroll: --inner"></div>
      </template>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* outer_anchor = GetElementById("outer-anchor");
  Element* host = GetElementById("host");
  ShadowRoot* shadow = host->GetShadowRoot();
  Element* part = shadow->getElementById("part");
  Element* inner_anchor = shadow->getElementById("inner-anchor");

  EXPECT_EQ(*MakeGarbageCollected<ScopedCSSName>("--outer", &GetDocument()),
            outer_anchor->ComputedStyleRef().AnchorScroll()->GetName());
  EXPECT_EQ(*MakeGarbageCollected<ScopedCSSName>("--host", shadow),
            host->ComputedStyleRef().AnchorScroll()->GetName());
  EXPECT_EQ(*MakeGarbageCollected<ScopedCSSName>("--part", &GetDocument()),
            part->ComputedStyleRef().AnchorScroll()->GetName());
  EXPECT_EQ(*MakeGarbageCollected<ScopedCSSName>("--inner", shadow),
            inner_anchor->ComputedStyleRef().AnchorScroll()->GetName());
}

// |length| must be a calculated value of a single anchor query node.
static const TreeScope* GetAnchorQueryTreeScope(const Length& length) {
  DCHECK(length.IsCalculated());
  DCHECK(length.GetCalculationValue().IsExpression());
  const auto& query = To<CalculationExpressionAnchorQueryNode>(
      *length.GetCalculationValue().GetOrCreateExpression());
  return query.AnchorSpecifier().IsNamed()
             ? query.AnchorSpecifier().GetName().GetTreeScope()
             : nullptr;
}

TEST_F(StyleResolverTest, ScopedAnchorFunction) {
  GetDocument()
      .documentElement()
      ->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <style>
      div { position: absolute; }
      #left { left: anchor(--a left); }
      #bottom::part(right) { right: anchor(--a right); }
    </style>
    <div id="left"></div>
    <div id="bottom">
      <template shadowroot=open>
        <style>
          div { position: absolute; }
          #top { top: anchor(--a top); }
          :host { bottom: anchor(--a bottom); }
        </style>
        <div id="top"></div>
        <div id="right" part="right"></div>
      </template>
    </div>

    <style>
      #inline-start { inset-inline-start: anchor(--a left); }
      #block-end::part(inline-end) { inset-inline-end: anchor(--a right); }
    </style>
    <div id="inline-start"></div>
    <div id="block-end">
      <template shadowroot=open>
        <style>
          div { position: absolute }
          :host { inset-block-end: anchor(--a bottom); }
          #block-start { inset-block-start: anchor(--a top); }
        </style>
        <div id="block-start"></div>
        <div id="inline-end" part="inline-end"></div>
      </template>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  {
    Element* left = GetElementById("left");
    Element* bottom = GetElementById("bottom");
    ShadowRoot* shadow = bottom->GetShadowRoot();
    Element* top = shadow->getElementById("top");
    Element* right = shadow->getElementById("right");

    EXPECT_EQ(&GetDocument(),
              GetAnchorQueryTreeScope(GetLeft(left->ComputedStyleRef())));
    EXPECT_EQ(&GetDocument(),
              GetAnchorQueryTreeScope(GetRight(right->ComputedStyleRef())));
    EXPECT_EQ(shadow, GetAnchorQueryTreeScope(GetTop(top->ComputedStyleRef())));
    EXPECT_EQ(shadow,
              GetAnchorQueryTreeScope(GetBottom(bottom->ComputedStyleRef())));
  }

  {
    // Verify that it also works for logical properties.
    Element* inline_start = GetElementById("inline-start");
    Element* block_end = GetElementById("block-end");
    ShadowRoot* shadow = block_end->GetShadowRoot();
    Element* block_start = shadow->getElementById("block-start");
    Element* inline_end = shadow->getElementById("inline-end");

    EXPECT_EQ(&GetDocument(), GetAnchorQueryTreeScope(
                                  GetLeft(inline_start->ComputedStyleRef())));
    EXPECT_EQ(&GetDocument(), GetAnchorQueryTreeScope(
                                  GetRight(inline_end->ComputedStyleRef())));
    EXPECT_EQ(shadow,
              GetAnchorQueryTreeScope(GetTop(block_start->ComputedStyleRef())));
    EXPECT_EQ(shadow, GetAnchorQueryTreeScope(
                          GetBottom(block_end->ComputedStyleRef())));
  }
}

TEST_F(StyleResolverTest, ScopedAnchorSizeFunction) {
  GetDocument()
      .documentElement()
      ->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <style>
      div { position: absolute; }
      #width { width: anchor-size(--a width); }
    </style>
    <div id="width">
      <template shadowroot=open>
        <style>
          div { position: absolute; }
          #height { height: anchor-size(--a height); }
        </style>
        <div id="height"></div>
      </template>
    </div>

    <style>
      #min-width { min-width: anchor-size(--a width); }
      #max-width::part(max-height) { max-height: anchor-size(--a height); }
    </style>
    <div id="min-width"></div>
    <div id="max-width">
      <template shadowroot=open>
        <style>
          div { position: absolute; }
          #min-height { min-height: anchor-size(--a height); }
          :host { max-width: anchor-size(--a width); }
        </style>
        <div id="min-height"></div>
        <div id="max-height" part="max-height"></div>
      </template>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* width = GetElementById("width");
  Element* min_width = GetElementById("min-width");
  Element* max_width = GetElementById("max-width");
  ShadowRoot* shadow1 = width->GetShadowRoot();
  ShadowRoot* shadow2 = max_width->GetShadowRoot();
  Element* height = shadow1->getElementById("height");
  Element* min_height = shadow2->getElementById("min-height");
  Element* max_height = shadow2->getElementById("max-height");

  EXPECT_EQ(&GetDocument(),
            GetAnchorQueryTreeScope(GetWidth(width->ComputedStyleRef())));
  EXPECT_EQ(shadow1,
            GetAnchorQueryTreeScope(GetHeight(height->ComputedStyleRef())));
  EXPECT_EQ(&GetDocument(), GetAnchorQueryTreeScope(
                                GetMinWidth(min_width->ComputedStyleRef())));
  EXPECT_EQ(shadow2, GetAnchorQueryTreeScope(
                         GetMaxWidth(max_width->ComputedStyleRef())));
  EXPECT_EQ(shadow2, GetAnchorQueryTreeScope(
                         GetMinHeight(min_height->ComputedStyleRef())));
  EXPECT_EQ(&GetDocument(), GetAnchorQueryTreeScope(
                                GetMaxHeight(max_height->ComputedStyleRef())));
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

  auto* a = GetDocument().getElementById("a");
  auto* b = GetDocument().getElementById("b");
  auto* c = GetDocument().getElementById("c");

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(c);

  EXPECT_TRUE(a->ComputedStyleRef().CanAffectAnimations());
  EXPECT_FALSE(b->ComputedStyleRef().CanAffectAnimations());
  EXPECT_FALSE(c->ComputedStyleRef().CanAffectAnimations());
}

TEST_F(StyleResolverTest, HasAutoAnchorPositioning) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #target {
        position: absolute;
        position-fallback: --pf;
        left: anchor(auto);
      }
      @position-fallback --pf {
        @try { width: 100px; }
        @try { top: anchor(auto); }
        @try { left: anchor(auto); }
        @try { left: anchor(auto); top: anchor(auto); }
      }
    </style>
    <div id="target"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById("target");

  const ComputedStyle& base_style = target->ComputedStyleRef();
  EXPECT_TRUE(base_style.HasAutoAnchorPositioning());
  EXPECT_FALSE(base_style.HasAutoAnchorPositioningInXAxisFromTryBlock());
  EXPECT_FALSE(base_style.HasAutoAnchorPositioningInYAxisFromTryBlock());

  // First @try block doesn't have any auto anchor positioning in it.
  const ComputedStyle& fallback1 = *target->StyleForPositionFallback(0);
  EXPECT_TRUE(fallback1.HasAutoAnchorPositioning());
  EXPECT_FALSE(fallback1.HasAutoAnchorPositioningInXAxisFromTryBlock());
  EXPECT_FALSE(fallback1.HasAutoAnchorPositioningInYAxisFromTryBlock());

  // Second @try block has auto anchor positioning only in y axis.
  const ComputedStyle& fallback2 = *target->StyleForPositionFallback(1);
  EXPECT_TRUE(fallback2.HasAutoAnchorPositioning());
  EXPECT_FALSE(fallback2.HasAutoAnchorPositioningInXAxisFromTryBlock());
  EXPECT_TRUE(fallback2.HasAutoAnchorPositioningInYAxisFromTryBlock());

  // Third @try block has auto anchor positioning only in x axis, even if the
  // resolved computed style is equal to the base style.
  const ComputedStyle& fallback3 = *target->StyleForPositionFallback(2);
  EXPECT_TRUE(fallback3.HasAutoAnchorPositioning());
  EXPECT_TRUE(fallback3.HasAutoAnchorPositioningInXAxisFromTryBlock());
  EXPECT_FALSE(fallback3.HasAutoAnchorPositioningInYAxisFromTryBlock());

  // Fourth @try block has auto anchor positioning in both axes.
  const ComputedStyle& fallback4 = *target->StyleForPositionFallback(3);
  EXPECT_TRUE(fallback4.HasAutoAnchorPositioning());
  EXPECT_TRUE(fallback4.HasAutoAnchorPositioningInXAxisFromTryBlock());
  EXPECT_TRUE(fallback4.HasAutoAnchorPositioningInYAxisFromTryBlock());
}

}  // namespace blink
