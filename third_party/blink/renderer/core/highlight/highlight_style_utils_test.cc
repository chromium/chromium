// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight_style_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
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
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class HighlightStyleUtilsTest : public SimTest,
                                private ScopedHighlightInheritanceForTest {
 public:
  // TODO(crbug.com/1024156) remove CachedPseudoStyles tests, but keep
  // SelectedTextInputShadow, when HighlightInheritance becomes stable
  HighlightStyleUtilsTest() : ScopedHighlightInheritanceForTest(false) {}
};

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

  const ComputedStyle* pseudo_style = HighlightStyleUtils::HighlightPseudoStyle(
      text_node, text_style, kPseudoIdSelection);
  paint_style =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), text_style, pseudo_style, text_node,
          kPseudoIdSelection, paint_style, paint_info, SearchTextIsCurrent::kNo)
          .style;

  EXPECT_EQ(Color(0, 128, 0), paint_style.fill_color);
  EXPECT_TRUE(paint_style.shadow);
}

TEST_F(HighlightStyleUtilsTest, SelectedTextIsRespected) {
  ScopedSelectionRespectsColorsForTest selection_respects_color_enabled(true);
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
      HighlightStyleUtils::HighlightPseudoStyle(div1_text, div1_style,
                                                kPseudoIdSelection);
  paint_style =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), div1_style, div1_pseudo_style, div1_text,
          kPseudoIdSelection, paint_style, paint_info, SearchTextIsCurrent::kNo)
          .style;
  background_color = HighlightStyleUtils::HighlightBackgroundColor(
      GetDocument(), div1_style, div1_text, std::nullopt, kPseudoIdSelection,
      SearchTextIsCurrent::kNo);
  EXPECT_EQ(Color(0, 128, 0), paint_style.fill_color);
  EXPECT_EQ(Color(0, 128, 0), background_color);

  auto* div2_text =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("#div2")))
          ->firstChild();
  const ComputedStyle& div2_style = div1_text->GetLayoutObject()->StyleRef();
  const ComputedStyle* div2_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div2_text, div2_style,
                                                kPseudoIdSelection);
  paint_style =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), div2_style, div2_pseudo_style, div2_text,
          kPseudoIdSelection, paint_style, paint_info, SearchTextIsCurrent::kNo)
          .style;
  background_color = HighlightStyleUtils::HighlightBackgroundColor(
      GetDocument(), div2_style, div2_text, std::nullopt, kPseudoIdSelection,
      SearchTextIsCurrent::kNo);
  EXPECT_EQ(default_highlight_background, paint_style.current_color);
  // Paired defaults means this is transparent
  EXPECT_EQ(Color(0, 0, 0, 0), background_color);

  auto* div3_text =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("#div3")))
          ->firstChild();
  const ComputedStyle& div3_style = div1_text->GetLayoutObject()->StyleRef();
  const ComputedStyle* div3_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div3_text, div3_style,
                                                kPseudoIdSelection);
  paint_style =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), div3_style, div3_pseudo_style, div3_text,
          kPseudoIdSelection, paint_style, paint_info, SearchTextIsCurrent::kNo)
          .style;
  std::optional<Color> current_layer_color = default_highlight_background;
  background_color = HighlightStyleUtils::HighlightBackgroundColor(
      GetDocument(), div3_style, div3_text, current_layer_color,
      kPseudoIdSelection, SearchTextIsCurrent::kNo);
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(default_highlight_background, paint_style.current_color);
  EXPECT_EQ(Color(255, 255, 255), background_color);
#else
  Color default_highlight_foreground =
      LayoutTheme::GetTheme().InactiveSelectionForegroundColor(
          mojom::blink::ColorScheme::kLight);
  EXPECT_EQ(default_highlight_foreground, paint_style.current_color);
  EXPECT_EQ(Color(92, 92, 92), background_color);
#endif
}

TEST_F(HighlightStyleUtilsTest, CurrentColorReportingAll) {
  ScopedHighlightInheritanceForTest highlight_inheritance_enabled(true);
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
      HighlightStyleUtils::HighlightPseudoStyle(
          div_text, div_style, kPseudoIdHighlight, AtomicString("highlight1"));
  HighlightStyleUtils::HighlightTextPaintStyle highlight_paint_style =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), div_style, div_pseudo_style, div_text,
          kPseudoIdHighlight, paint_style, paint_info,
          SearchTextIsCurrent::kNo);

  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kCurrentColor));
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kFillColor));
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kStrokeColor));
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
      HighlightStyleUtils::HighlightColorProperty::kStrokeColor));
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kEmphasisColor));
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kSelectionDecorationColor));
#else
  const ComputedStyle* selection_pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(div_text, div_style,
                                                kPseudoIdSelection);
  HighlightStyleUtils::HighlightTextPaintStyle selection_paint_style =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), div_style, selection_pseudo_style, div_text,
          kPseudoIdSelection, paint_style, paint_info,
          SearchTextIsCurrent::kNo);
  // Selection uses explicit default colors.
  EXPECT_TRUE(selection_paint_style.properties_using_current_color.empty());
#endif
}

TEST_F(HighlightStyleUtilsTest, CurrentColorReportingSome) {
  ScopedHighlightInheritanceForTest highlight_inheritance_enabled(true);
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  String html_content =
      R"HTML(
      <!doctype html>
      <style>
        ::highlight(highlight1) {
          text-decoration-line: underline;
          text-decoration-color: red;
          -webkit-text-fill-color: blue;
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
      HighlightStyleUtils::HighlightPseudoStyle(
          div_text, div_style, kPseudoIdHighlight, AtomicString("highlight1"));
  HighlightStyleUtils::HighlightTextPaintStyle highlight_paint_style =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), div_style, div_pseudo_style, div_text,
          kPseudoIdHighlight, paint_style, paint_info,
          SearchTextIsCurrent::kNo);

  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kCurrentColor));
  EXPECT_FALSE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kFillColor));
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kStrokeColor));
  EXPECT_TRUE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kEmphasisColor));
  EXPECT_FALSE(highlight_paint_style.properties_using_current_color.Has(
      HighlightStyleUtils::HighlightColorProperty::kSelectionDecorationColor));
}

TEST_F(HighlightStyleUtilsTest, CustomPropertyInheritance) {
  ScopedHighlightInheritanceForTest highlight_inheritance_enabled(true);
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      :root {
        --root-color: green;
      }
      ::selection {
        /* This rule should not apply */
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
      HighlightStyleUtils::HighlightPseudoStyle(div_node, div_style,
                                                kPseudoIdSelection);
  paint_style =
      HighlightStyleUtils::HighlightPaintingStyle(
          GetDocument(), div_style, div_pseudo_style, div_node,
          kPseudoIdSelection, paint_style, paint_info, SearchTextIsCurrent::kNo)
          .style;

  EXPECT_EQ(Color(255, 0, 0), paint_style.fill_color);

  Color background_color = HighlightStyleUtils::HighlightBackgroundColor(
      GetDocument(), div_style, div_node, previous_layer_color,
      kPseudoIdSelection, SearchTextIsCurrent::kNo);

  EXPECT_EQ(Color(0, 128, 0), background_color);
}

TEST_F(HighlightStyleUtilsTest,
       CustomPropertyOriginatingInheritanceUniversal) {
  ScopedHighlightInheritanceForTest highlight_inheritance_enabled(true);
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
      kPseudoIdSelection, SearchTextIsCurrent::kNo);
  EXPECT_EQ(Color(0, 128, 0), div_background_color);

  auto* div_inherited_vars = div_style.InheritedVariables();

  auto* first_p_node = To<HTMLElement>(div_node->firstChild()->nextSibling());
  const ComputedStyle& first_p_style = first_p_node->ComputedStyleRef();
  Color first_p_background_color =
      HighlightStyleUtils::HighlightBackgroundColor(
          GetDocument(), first_p_style, first_p_node, previous_layer_color,
          kPseudoIdSelection, SearchTextIsCurrent::kNo);
  EXPECT_EQ(Color(0, 128, 0), first_p_background_color);
  auto* first_p_inherited_vars = first_p_style.InheritedVariables();
  EXPECT_EQ(div_inherited_vars, first_p_inherited_vars);

  auto* second_p_node =
      To<HTMLElement>(first_p_node->nextSibling()->nextSibling());
  const ComputedStyle& second_p_style = second_p_node->ComputedStyleRef();
  Color second_p_background_color =
      HighlightStyleUtils::HighlightBackgroundColor(
          GetDocument(), second_p_style, second_p_node, previous_layer_color,
          kPseudoIdSelection, SearchTextIsCurrent::kNo);
  EXPECT_EQ(Color(0, 0, 255), second_p_background_color);
  auto* second_p_inherited_vars = second_p_style.InheritedVariables();
  EXPECT_NE(second_p_inherited_vars, first_p_inherited_vars);

  auto* second_strong_node =
      To<HTMLElement>(second_p_node->firstChild()->nextSibling());
  const ComputedStyle& second_strong_style =
      second_strong_node->ComputedStyleRef();
  Color second_strong_background_color =
      HighlightStyleUtils::HighlightBackgroundColor(
          GetDocument(), second_strong_style, second_strong_node,
          previous_layer_color, kPseudoIdSelection, SearchTextIsCurrent::kNo);
  EXPECT_EQ(Color(0, 0, 255), second_strong_background_color);
  auto* second_strong_inherited_vars = second_strong_style.InheritedVariables();
  EXPECT_EQ(second_p_inherited_vars, second_strong_inherited_vars);
}

TEST_F(HighlightStyleUtilsTest, FontMetricsFromOriginatingElement) {
  ScopedHighlightInheritanceForTest highlight_inheritance_enabled(true);
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
      div_node, div_style, kPseudoIdHighlight, AtomicString("highlight1"));

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
  ScopedHighlightInheritanceForTest highlight_inheritance_enabled(true);
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
  ScopedHighlightInheritanceForTest highlight_inheritance_enabled(true);
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
      HighlightStyleUtils::HighlightPseudoStyle(
          div_node, div_style, kPseudoIdHighlight, AtomicString("highlight1"));

  EXPECT_TRUE(div_pseudo_style->HasAppliedTextDecorations());
  const AppliedTextDecoration& text_decoration =
      div_pseudo_style->AppliedTextDecorations()[0];
  TextDecorationThickness thickness = text_decoration.Thickness();
  EXPECT_EQ(FloatValueForLength(thickness.Thickness(), 1), 4);
  Length offset = text_decoration.UnderlineOffset();
  EXPECT_EQ(FloatValueForLength(offset, 1), 4);
}

TEST_F(HighlightStyleUtilsTest, ContainerIsOriginatingElement) {
  ScopedHighlightInheritanceForTest highlight_inheritance_enabled(true);
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
      HighlightStyleUtils::HighlightPseudoStyle(
          div_node, div_style, kPseudoIdHighlight, AtomicString("highlight1"));

  EXPECT_TRUE(div_pseudo_style);
  EXPECT_TRUE(div_pseudo_style->HasAppliedTextDecorations());
  const AppliedTextDecoration& text_decoration =
      div_pseudo_style->AppliedTextDecorations()[0];
  TextDecorationThickness thickness = text_decoration.Thickness();
  EXPECT_EQ(FloatValueForLength(thickness.Thickness(), 1), 4);
  Length offset = text_decoration.UnderlineOffset();
  EXPECT_EQ(FloatValueForLength(offset, 1), 4);
}

}  // namespace blink
