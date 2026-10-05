// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight_style_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/dom_selection.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

class HighlightStyleUtilsTest : public SimTest {};

TEST_F(HighlightStyleUtilsTest, SelectedTextInputShadow) {
  // Test that we apply input ::selection style to the value text.
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      input::selection {
        color: green;
        text-shadow: 2px 2px;
      }
    </style>
    <input type="text" value="Selected">
  )HTML");

  Compositor().BeginFrame();

  auto* text_node =
      To<HTMLInputElement>(GetDocument().QuerySelector(AtomicString("input")))
          ->InnerEditorElement()
          ->firstChild();
  const ComputedStyle& text_style = text_node->GetLayoutObject()->StyleRef();

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground,
                       /*descendant_painting_blocked=*/false);
  TextPaintStyle paint_style;

  const ComputedStyle* pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(text_style, kPseudoIdSelection);
  paint_style = HighlightStyleUtils::HighlightPaintingStyle(
                    GetDocument(), text_style, pseudo_style, text_node,
                    kPseudoIdSelection, paint_style, paint_info,
                    SearchTextIsActiveMatch::kNo)
                    .style;

  EXPECT_EQ(Color(0, 128, 0), paint_style.fill_color);
  EXPECT_TRUE(paint_style.shadow);
}

TEST_F(HighlightStyleUtilsTest, TransparentTextGetsSelectionForeground) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <div style="color: transparent">Select me</div>
  )HTML");

  Compositor().BeginFrame();
  auto* div =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));
  Node* text = div->firstChild();
  const ComputedStyle& text_style = text->GetLayoutObject()->StyleRef();
  const ComputedStyle* pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(text_style, kPseudoIdSelection);

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground,
                       /*descendant_painting_blocked=*/false);
  TextPaintStyle paint_style =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), text_style, pseudo_style, text, kPseudoIdSelection,
          TextPaintStyle(), paint_info, SearchTextIsActiveMatch::kNo)
          .style;

  EXPECT_TRUE(paint_style.fill_color ==
                  LayoutTheme::GetTheme().ActiveSelectionForegroundColor(
                      mojom::blink::ColorScheme::kLight) ||
              paint_style.fill_color ==
                  LayoutTheme::GetTheme().InactiveSelectionForegroundColor(
                      mojom::blink::ColorScheme::kLight));
}

TEST_F(HighlightStyleUtilsTest, SelectedTextIsRespected) {
  // Test that we respect the author's colors in ::selection
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  Color default_highlight_background =
      LayoutTheme::GetTheme().InactiveSelectionBackgroundColor(
          mojom::blink::ColorScheme::kLight);
  String html_content =
      R"HTML(
      <!doctype html>
      <style>
        #div1::selection {
          background-color: green;
          color: green;
        }
        #div2::selection {
          color: )HTML" +
      default_highlight_background.SerializeAsCSSColor() + R"HTML(;
        }
        #div3 {
          color: )HTML" +
      default_highlight_background.SerializeAsCSSColor() + R"HTML(;
        }
      }
      </style>
      <div id="div1">Green text selection color and background</div>
      <div id="div2">Foreground matches default background color</div>
      <div id="div3">No selection pseudo colors matching text color</div>
    )HTML";
  main_resource.Complete(html_content);

  Compositor().BeginFrame();

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground,
                       /*descendant_painting_blocked=*/false);
  TextPaintStyle paint_style;
  Color background_color;

  auto* div1_text =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("#div1")))
          ->firstChild();
  const ComputedStyle& div1_style = div1_text->GetLayoutObject()->StyleRef();
  const ComputedStyle* div1_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div1_style, kPseudoIdSelection);
  paint_style = HighlightStyleUtils::HighlightPaintingStyle(
                    GetDocument(), div1_style, div1_pseudo_style, div1_text,
                    kPseudoIdSelection, paint_style, paint_info,
                    SearchTextIsActiveMatch::kNo)
                    .style;
  background_color = HighlightStyleUtils::HighlightBackgroundColor(
      GetDocument(), div1_style, div1_text, std::nullopt, kPseudoIdSelection,
      false, SearchTextIsActiveMatch::kNo);
  EXPECT_EQ(Color(0, 128, 0), paint_style.fill_color);
  EXPECT_EQ(Color(0, 128, 0), background_color);

  auto* div2_text =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("#div2")))
          ->firstChild();
  const ComputedStyle& div2_style = div2_text->GetLayoutObject()->StyleRef();
  const ComputedStyle* div2_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div2_style, kPseudoIdSelection);
  paint_style = HighlightStyleUtils::HighlightPaintingStyle(
                    GetDocument(), div2_style, div2_pseudo_style, div2_text,
                    kPseudoIdSelection, paint_style, paint_info,
                    SearchTextIsActiveMatch::kNo)
                    .style;
  background_color = HighlightStyleUtils::HighlightBackgroundColor(
      GetDocument(), div2_style, div2_text, std::nullopt, kPseudoIdSelection,
      false, SearchTextIsActiveMatch::kNo);
  EXPECT_EQ(default_highlight_background, paint_style.fill_color);
  // Paired defaults means this is transparent
  EXPECT_EQ(Color(0, 0, 0, 0), background_color);

  auto* div3_text =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("#div3")))
          ->firstChild();
  const ComputedStyle& div3_style = div3_text->GetLayoutObject()->StyleRef();
  const ComputedStyle* div3_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div3_style, kPseudoIdSelection);
  paint_style = HighlightStyleUtils::HighlightPaintingStyle(
                    GetDocument(), div3_style, div3_pseudo_style, div3_text,
                    kPseudoIdSelection, paint_style, paint_info,
                    SearchTextIsActiveMatch::kNo)
                    .style;
  std::optional<Color> current_layer_color = default_highlight_background;
  background_color = HighlightStyleUtils::HighlightBackgroundColor(
      GetDocument(), div3_style, div3_text, current_layer_color,
      kPseudoIdSelection, false, SearchTextIsActiveMatch::kNo);
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(default_highlight_background, paint_style.fill_color);
  EXPECT_EQ(Color::FromColorSpace(Color::ColorSpace::kSRGB, 1, 1, 1),
            background_color);
#else
  Color default_highlight_foreground =
      LayoutTheme::GetTheme().InactiveSelectionForegroundColor(
          mojom::blink::ColorScheme::kLight);
  EXPECT_EQ(default_highlight_foreground, paint_style.fill_color);
  EXPECT_EQ(default_highlight_background.MakeOpaque().InvertSRGB(),
            background_color);
#endif
}

TEST_F(HighlightStyleUtilsTest, CurrentColorReportingAll) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  String html_content =
      R"HTML(
      <!doctype html>
      <style>
        ::selection {
          text-decoration-line: underline;
        }
        ::highlight(highlight1) {
          text-decoration-line: underline;
        }
        div {
          text-decoration-line: underline;
        }
      </style>
      <div id="div">Some text</div>
      <script>
        let r1 = new Range();
        r1.setStart(div, 0);
        r1.setEnd(div, 1);
        CSS.highlights.set("highlight1", new Highlight(r1));
      </script>
    )HTML";
  main_resource.Complete(html_content);

  auto* div_node =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("#div")));
  Window().getSelection()->setBaseAndExtent(div_node, 0, div_node, 1);

  Compositor().BeginFrame();

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground,
                       /*descendant_painting_blocked=*/false);
  TextPaintStyle paint_style;

  auto* div_text = div_node->firstChild();
  const ComputedStyle& div_style = div_text->GetLayoutObject()->StyleRef();
  const ComputedStyle* div_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div_style, kPseudoIdHighlight,
                                                AtomicString("highlight1"));
  HighlightStyleUtils::HighlightTextPaintStyle highlight_paint_style =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), div_style, div_pseudo_style, div_text,
          kPseudoIdHighlight, paint_style, paint_info,
          SearchTextIsActiveMatch::kNo);

  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kCurrentColor));
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kFillColor));
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kEmphasisColor));
#if BUILDFLAG(IS_MAC)
  // Mac does not have default selection in tests
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kSelectionDecorationColor));
#else
  EXPECT_FALSE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kSelectionDecorationColor));
#endif

#if BUILDFLAG(IS_MAC)
  // Mac does not have default selection colors in testing
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kCurrentColor));
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kFillColor));
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kEmphasisColor));
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kSelectionDecorationColor));
#else
  const ComputedStyle* selection_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div_style, kPseudoIdSelection);
  HighlightStyleUtils::HighlightTextPaintStyle selection_paint_style =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), div_style, selection_pseudo_style, div_text,
          kPseudoIdSelection, paint_style, paint_info,
          SearchTextIsActiveMatch::kNo);
  // Selection uses explicit default colors.
  EXPECT_TRUE(selection_paint_style.properties_using_current_color.empty());
#endif
}

TEST_F(HighlightStyleUtilsTest, CurrentColorReportingSome) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  String html_content =
      R"HTML(
      <!doctype html>
      <style>
        ::highlight(highlight1) {
          text-decoration-line: underline;
          text-decoration-color: red;
        }
      </style>
      <div id="div">Some text</div>
      <script>
        let r1 = new Range();
        r1.setStart(div, 0);
        r1.setEnd(div, 1);
        CSS.highlights.set("highlight1", new Highlight(r1));
      </script>
    )HTML";
  main_resource.Complete(html_content);

  Compositor().BeginFrame();

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground,
                       /*descendant_painting_blocked=*/false);
  TextPaintStyle paint_style;

  auto* div_text =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("#div")))
          ->firstChild();
  const ComputedStyle& div_style = div_text->GetLayoutObject()->StyleRef();
  const ComputedStyle* div_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div_style, kPseudoIdHighlight,
                                                AtomicString("highlight1"));
  HighlightStyleUtils::HighlightTextPaintStyle highlight_paint_style =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), div_style, div_pseudo_style, div_text,
          kPseudoIdHighlight, paint_style, paint_info,
          SearchTextIsActiveMatch::kNo);

  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kCurrentColor));
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kFillColor));
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kEmphasisColor));
  EXPECT_FALSE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kSelectionDecorationColor));
}

// Regression test for crbug.com/516004705. When text is user-select:none the
// ::selection overlay is ignored, but it must not contribute any color of its
// own. The foreground colors (current/fill/emphasis) are seeded from the layer
// below during layer construction, so they must be flagged for per-part
// re-resolution; otherwise the selection layer freezes the color of whatever
// layer preceded it (e.g. a custom highlight) and leaks it onto text that layer
// does not cover. The background must stay transparent (selection backgrounds
// are suppressed for non-selectable text), and the decoration colors -- which
// are not seeded from the previous layer -- must likewise never become the
// custom highlight color.
TEST_F(HighlightStyleUtilsTest, IgnoredSelectionDoesNotLeakColorsFromBelow) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      ::highlight(highlight1) {
        background-color: black;
        color: rgb(11, 22, 33);
      }
      ::selection {
        text-decoration-line: underline;
      }
      #div {
        user-select: none;
        text-decoration-line: underline;
      }
    </style>
    <div id="div">Some text</div>
    <script>
      let r1 = new Range();
      r1.setStart(div.firstChild, 0);
      r1.setEnd(div.firstChild, 1);
      CSS.highlights.set("highlight1", new Highlight(r1));
    </script>
  )HTML");

  Compositor().BeginFrame();

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground,
                       /*descendant_painting_blocked=*/false);

  auto* div_text =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("#div")))
          ->firstChild();
  const ComputedStyle& div_style = div_text->GetLayoutObject()->StyleRef();
  ASSERT_FALSE(div_style.IsSelectable());

  // The originating layer paints black text.
  const Color kOriginatingColor(0, 0, 0);
  const Color kCustomColor(11, 22, 33);
  TextPaintStyle originating_paint_style;
  originating_paint_style.current_color = kOriginatingColor;
  originating_paint_style.fill_color = kOriginatingColor;

  // Build the custom highlight layer on top of the originating layer.
  const ComputedStyle* highlight_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div_style, kPseudoIdHighlight,
                                                AtomicString("highlight1"));
  HighlightStyleUtils::HighlightTextPaintStyle custom_layer =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), div_style, highlight_pseudo_style, div_text,
          kPseudoIdHighlight, originating_paint_style, paint_info,
          SearchTextIsActiveMatch::kNo);
  ASSERT_EQ(kCustomColor, custom_layer.style.current_color);

  // Build the ::selection layer on top of the custom highlight layer, just as
  // ComputeLayers() does. Because the text is user-select:none, the selection
  // is ignored.
  const ComputedStyle* selection_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div_style, kPseudoIdSelection);
  HighlightStyleUtils::HighlightTextPaintStyle selection_layer =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), div_style, selection_pseudo_style, div_text,
          kPseudoIdSelection, custom_layer.style, paint_info,
          SearchTextIsActiveMatch::kNo);

  // The ignored selection defers its foreground colors (current/fill/emphasis)
  // to the layer below so they can be re-resolved per-part, but it must NOT
  // introduce a background (it stays transparent for non-selectable text).
  EXPECT_TRUE(selection_layer.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kCurrentColor));
  EXPECT_TRUE(selection_layer.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kFillColor));
  EXPECT_TRUE(selection_layer.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kEmphasisColor));
  EXPECT_FALSE(selection_layer.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kBackgroundColor));
  EXPECT_EQ(Color::kTransparent, selection_layer.background_color);

  // The originating decoration makes the ignored selection carry a selection
  // decoration. Its color must never be *painted* as the custom highlight color
  // on text the highlight does not cover. Depending on the platform, the
  // selection decoration color is either resolved to a default (e.g. on
  // Windows) or -- when no default selection color is available, as in tests on
  // Mac -- seeded from the previous layer and flagged for per-part resolution.
  // In the latter case the pre-fold value is the previous (custom) color, so
  // the meaningful invariant is checked after folding below. The overlay
  // decoration override color (text_decoration_color), used by ComputeParts()
  // for the selection layer, is left at its default for an ignored selection
  // and is never sourced from the previous layer, so it can never leak the
  // custom highlight color.
  ASSERT_NE(TextDecorationLine::kNone,
            selection_layer.style.selection_decoration_lines);
  EXPECT_NE(kCustomColor, selection_layer.text_decoration_color);

  // For text the custom highlight does not cover, the previous active layer is
  // the originating layer. Folding must resolve every foreground color to the
  // originating color -- never the leaked custom highlight color -- and must
  // never produce the custom color for any decoration color.
  HighlightStyleUtils::HighlightTextPaintStyle originating_layer{
      originating_paint_style, kOriginatingColor, Color::kTransparent, {}};
  HighlightStyleUtils::ResolveColorsFromPreviousLayer(selection_layer,
                                                      originating_layer);
  EXPECT_EQ(kOriginatingColor, selection_layer.style.current_color);
  EXPECT_EQ(kOriginatingColor, selection_layer.style.fill_color);
  EXPECT_EQ(kOriginatingColor, selection_layer.style.emphasis_mark_color);
  EXPECT_NE(kCustomColor, selection_layer.style.selection_decoration_color);
  EXPECT_NE(kCustomColor, selection_layer.text_decoration_color);
}

TEST_F(HighlightStyleUtilsTest, CustomPropertyInheritance) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      :root {
        --root-color: green;
      }
      :root::selection {
        /* Should not affect div::selection */
        --selection-color: blue;
      }
      div::selection {
        /* Use the fallback */
        color: var(--selection-color, red);
        /* Use the :root inherited via originating */
        background-color: var(--root-color, red);
      }
    </style>
    <div>Selected</div>
  )HTML");

  // Select some text.
  auto* div_node =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));
  Window().getSelection()->setBaseAndExtent(div_node, 0, div_node, 1);
  Compositor().BeginFrame();
  std::optional<Color> previous_layer_color;

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground,
                       /*descendant_painting_blocked=*/false);
  TextPaintStyle paint_style;
  const ComputedStyle& div_style = div_node->ComputedStyleRef();
  const ComputedStyle* div_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div_style, kPseudoIdSelection);
  paint_style = HighlightStyleUtils::HighlightPaintingStyle(
                    GetDocument(), div_style, div_pseudo_style, div_node,
                    kPseudoIdSelection, paint_style, paint_info,
                    SearchTextIsActiveMatch::kNo)
                    .style;

  EXPECT_EQ(Color(255, 0, 0), paint_style.fill_color);

  Color background_color = HighlightStyleUtils::HighlightBackgroundColor(
      GetDocument(), div_style, div_node, previous_layer_color,
      kPseudoIdSelection, false, SearchTextIsActiveMatch::kNo);

  EXPECT_EQ(Color(0, 128, 0), background_color);
}

TEST_F(HighlightStyleUtilsTest, CustomPropertyOriginatingInheritanceUniversal) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      :root {
        --selection-color: green;
      }
      ::selection {
        background-color: var(--selection-color);
      }
      .blue {
        --selection-color: blue;
      }
    </style>
    <div>
      <p>Some <strong>green</strong> highlight</p>
      <p class="blue">Some <strong>still blue</strong> highlight</p>
    </div>
  )HTML");

  // Select some text.
  auto* div_node =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));
  Window().getSelection()->setBaseAndExtent(div_node, 0, div_node, 1);
  Compositor().BeginFrame();

  const ComputedStyle& div_style = div_node->ComputedStyleRef();
  std::optional<Color> previous_layer_color;
  Color div_background_color = HighlightStyleUtils::HighlightBackgroundColor(
      GetDocument(), div_style, div_node, previous_layer_color,
      kPseudoIdSelection, false, SearchTextIsActiveMatch::kNo);
  EXPECT_EQ(Color(0, 128, 0), div_background_color);

  auto& div_inherited_vars = div_style.InheritedVariables();

  auto* first_p_node = To<HTMLElement>(div_node->firstChild()->nextSibling());
  const ComputedStyle& first_p_style = first_p_node->ComputedStyleRef();
  Color first_p_background_color =
      HighlightStyleUtils::HighlightBackgroundColor(
          GetDocument(), first_p_style, first_p_node, previous_layer_color,
          kPseudoIdSelection, false, SearchTextIsActiveMatch::kNo);
  EXPECT_EQ(Color(0, 128, 0), first_p_background_color);
  auto& first_p_inherited_vars = first_p_style.InheritedVariables();
  EXPECT_EQ(div_inherited_vars, first_p_inherited_vars);

  auto* second_p_node =
      To<HTMLElement>(first_p_node->nextSibling()->nextSibling());
  const ComputedStyle& second_p_style = second_p_node->ComputedStyleRef();
  Color second_p_background_color =
      HighlightStyleUtils::HighlightBackgroundColor(
          GetDocument(), second_p_style, second_p_node, previous_layer_color,
          kPseudoIdSelection, false, SearchTextIsActiveMatch::kNo);
  EXPECT_EQ(Color(0, 0, 255), second_p_background_color);
  auto& second_p_inherited_vars = second_p_style.InheritedVariables();
  EXPECT_NE(second_p_inherited_vars, first_p_inherited_vars);

  auto* second_strong_node =
      To<HTMLElement>(second_p_node->firstChild()->nextSibling());
  const ComputedStyle& second_strong_style =
      second_strong_node->ComputedStyleRef();
  Color second_strong_background_color =
      HighlightStyleUtils::HighlightBackgroundColor(
          GetDocument(), second_strong_style, second_strong_node,
          previous_layer_color, kPseudoIdSelection, false,
          SearchTextIsActiveMatch::kNo);
  EXPECT_EQ(Color(0, 0, 255), second_strong_background_color);
  auto& second_strong_inherited_vars = second_strong_style.InheritedVariables();
  EXPECT_EQ(second_p_inherited_vars, second_strong_inherited_vars);
}

TEST_F(HighlightStyleUtilsTest, FontMetricsFromOriginatingElement) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      :root {
        font-size: 16px;
      }
      div {
        font-size: 40px;
      }
      ::highlight(highlight1) {
        text-underline-offset: 0.5em;
        text-decoration-line: underline;
        text-decoration-color: green;
        text-decoration-thickness: 0.25rem;
      }
    </style>
    <div id="h1">Font-dependent lengths</div>
    <script>
      let r1 = new Range();
      r1.setStart(h1, 0);
      r1.setEnd(h1, 1);
      CSS.highlights.set("highlight1", new Highlight(r1));
    </script>
  )HTML");

  Compositor().BeginFrame();

  auto* div_node =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));
  const ComputedStyle& div_style = div_node->ComputedStyleRef();
  EXPECT_EQ(div_style.SpecifiedFontSize(), 40);

  const ComputedStyle* pseudo_style = HighlightStyleUtils::HighlightPseudoStyle(
      div_style, kPseudoIdHighlight, AtomicString("highlight1"));

  EXPECT_TRUE(pseudo_style->HasAppliedTextDecorations());
  const AppliedTextDecoration& text_decoration =
      pseudo_style->AppliedTextDecorations()[0];
  TextDecorationThickness thickness = text_decoration.Thickness();
  EXPECT_EQ(FloatValueForLength(thickness.Thickness(), 1), 4);
  Length offset = text_decoration.UnderlineOffset();
  EXPECT_EQ(FloatValueForLength(offset, 1), 20);
}

TEST_F(HighlightStyleUtilsTest, CustomHighlightsNotOverlapping) {
  // Not really a style utils test, but this is the only Pseudo Highlights
  // unit test suite making use of SimTest.
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      ::highlight(highlight1) {
        background-color: red;
      }
      ::highlight(highlight2) {
        background-color: green;
      }
      ::highlight(highlight3) {
        background-color: blue;
      }
    </style>
    <div id="h1">0123456789</div>
    <script>
      let text = h1.firstChild;
      let r1 = new Range();
      r1.setStart(text, 0);
      r1.setEnd(text, 5);
      let r2 = new Range();
      r2.setStart(text, 4);
      r2.setEnd(text, 10);
      CSS.highlights.set("highlight1", new Highlight(r1, r2));
      let r3 = new Range();
      r3.setStart(text, 3);
      r3.setEnd(text, 6);
      let r4 = new Range();
      r4.setStart(text, 1);
      r4.setEnd(text, 9);
      CSS.highlights.set("highlight2", new Highlight(r3, r4));
      let r5 = new Range();
      r5.setStart(text, 2);
      r5.setEnd(text, 4);
      let r6 = new Range();
      r6.setStart(text, 5);
      r6.setEnd(text, 9);
      CSS.highlights.set("highlight3", new Highlight(r5, r6));
    </script>
  )HTML");

  Compositor().BeginFrame();

  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* node = div->firstChild();
  EXPECT_TRUE(node->IsTextNode());
  Text* text = To<Text>(node);

  auto& marker_controller = GetDocument().Markers();

  DocumentMarkerVector markers = marker_controller.MarkersFor(*text);
  EXPECT_EQ(4u, markers.size());

  DocumentMarker* marker = markers[0];
  EXPECT_EQ(DocumentMarker::MarkerType::kCustomHighlight, marker->GetType());
  EXPECT_EQ(AtomicString("highlight1"),
            To<CustomHighlightMarker>(marker)->GetHighlightName());
  EXPECT_EQ(0u, marker->StartOffset());
  EXPECT_EQ(10u, marker->EndOffset());

  marker = markers[1];
  EXPECT_EQ(DocumentMarker::MarkerType::kCustomHighlight, marker->GetType());
  EXPECT_EQ(AtomicString("highlight2"),
            To<CustomHighlightMarker>(marker)->GetHighlightName());
  EXPECT_EQ(1u, marker->StartOffset());
  EXPECT_EQ(9u, marker->EndOffset());

  marker = markers[2];
  EXPECT_EQ(DocumentMarker::MarkerType::kCustomHighlight, marker->GetType());
  EXPECT_EQ(AtomicString("highlight3"),
            To<CustomHighlightMarker>(marker)->GetHighlightName());
  EXPECT_EQ(2u, marker->StartOffset());
  EXPECT_EQ(4u, marker->EndOffset());

  marker = markers[3];
  EXPECT_EQ(DocumentMarker::MarkerType::kCustomHighlight, marker->GetType());
  EXPECT_EQ(AtomicString("highlight3"),
            To<CustomHighlightMarker>(marker)->GetHighlightName());
  EXPECT_EQ(5u, marker->StartOffset());
  EXPECT_EQ(9u, marker->EndOffset());
}

TEST_F(HighlightStyleUtilsTest, ContainerMetricsFromOriginatingElement) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <head>
      <style>
        .wrapper {
          container: wrapper / size;
          width: 200px;
          height: 100px;
        }
        @container wrapper (width > 100px) {
          ::highlight(highlight1) {
            text-underline-offset: 2cqw;
            text-decoration-line: underline;
            text-decoration-color: green;
            text-decoration-thickness: 4cqh;
          }
        }
      </style>
    </head>
    <body>
      <div class="wrapper">
        <div id="h1">With container size</div>
      </div>
      <script>
        let r1 = new Range();
        r1.setStart(h1, 0);
        r1.setEnd(h1, 1);
        CSS.highlights.set("highlight1", new Highlight(r1));
      </script>
    </body>
  )HTML");

  Compositor().BeginFrame();

  auto* div_node =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("#h1")));
  EXPECT_TRUE(div_node);
  const ComputedStyle& div_style = div_node->ComputedStyleRef();

  const ComputedStyle* div_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div_style, kPseudoIdHighlight,
                                                AtomicString("highlight1"));

  EXPECT_TRUE(div_pseudo_style->HasAppliedTextDecorations());
  const AppliedTextDecoration& text_decoration =
      div_pseudo_style->AppliedTextDecorations()[0];
  TextDecorationThickness thickness = text_decoration.Thickness();
  EXPECT_EQ(FloatValueForLength(thickness.Thickness(), 1), 4);
  Length offset = text_decoration.UnderlineOffset();
  EXPECT_EQ(FloatValueForLength(offset, 1), 4);
}

TEST_F(HighlightStyleUtilsTest, ContainerIsOriginatingElement) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <head>
      <style>
        .wrapper {
          container: wrapper / size;
          width: 200px;
          height: 100px;
        }
        @container (width > 100px) {
          .wrapper::highlight(highlight1) {
            text-underline-offset: 2cqw;
            text-decoration-line: underline;
            text-decoration-color: green;
            text-decoration-thickness: 4cqh;
          }
        }
      </style>
    </head>
    <body>
      <div id="h1" class="wrapper">With container size</div>
      <script>
        let r1 = new Range();
        r1.setStart(h1, 0);
        r1.setEnd(h1, 1);
        CSS.highlights.set("highlight1", new Highlight(r1));
      </script>
    </body>
  )HTML");

  Compositor().BeginFrame();

  auto* div_node =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("#h1")));
  EXPECT_TRUE(div_node);
  const ComputedStyle& div_style = div_node->ComputedStyleRef();

  const ComputedStyle* div_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div_style, kPseudoIdHighlight,
                                                AtomicString("highlight1"));

  EXPECT_TRUE(div_pseudo_style);
  EXPECT_TRUE(div_pseudo_style->HasAppliedTextDecorations());
  const AppliedTextDecoration& text_decoration =
      div_pseudo_style->AppliedTextDecorations()[0];
  TextDecorationThickness thickness = text_decoration.Thickness();
  EXPECT_EQ(FloatValueForLength(thickness.Thickness(), 1), 4);
  Length offset = text_decoration.UnderlineOffset();
  EXPECT_EQ(FloatValueForLength(offset, 1), 4);
}

TEST_F(HighlightStyleUtilsTest, LigthDarkColor) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <style>
      :root {
        color-scheme: light dark;
      }

      .dark {
         color-scheme: dark;
      }

      div::selection {
        color: light-dark(green, blue);
      }
    </style>
    <div class="dark">Dark</div>
  )HTML");

  // Select some text.
  auto* div_node =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));
  Window().getSelection()->setBaseAndExtent(div_node, 0, div_node, 1);
  Compositor().BeginFrame();

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground,
                       /*descendant_painting_blocked=*/false);
  TextPaintStyle paint_style;
  const ComputedStyle& div_style = div_node->ComputedStyleRef();
  const ComputedStyle* div_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div_style, kPseudoIdSelection);
  paint_style = HighlightStyleUtils::HighlightPaintingStyle(
                    GetDocument(), div_style, div_pseudo_style, div_node,
                    kPseudoIdSelection, paint_style, paint_info,
                    SearchTextIsActiveMatch::kNo)
                    .style;

  EXPECT_EQ(Color(0, 0, 255), paint_style.fill_color);
}

}  // namespace blink
