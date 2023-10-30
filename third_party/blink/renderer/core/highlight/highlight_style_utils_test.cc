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

  std::unique_ptr<PaintController> controller{
      std::make_unique<PaintController>()};
  GraphicsContext context(*controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground);
  TextPaintStyle paint_style;

  paint_style = HighlightStyleUtils::HighlightPaintingStyle(
      GetDocument(), text_style, text_node, kPseudoIdSelection, paint_style,
      paint_info);

  EXPECT_EQ(Color(0, 128, 0), paint_style.fill_color);
  EXPECT_TRUE(paint_style.shadow);
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
        --selection-color: blue;
      }
      div::selection {
        color: var(--selection-color, red);
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
  absl::optional<Color> previous_layer_color;

  std::unique_ptr<PaintController> controller{
      std::make_unique<PaintController>()};
  GraphicsContext context(*controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground);
  TextPaintStyle paint_style;
  const ComputedStyle& div_style = div_node->ComputedStyleRef();

  paint_style = HighlightStyleUtils::HighlightPaintingStyle(
      GetDocument(), div_style, div_node, kPseudoIdSelection, paint_style,
      paint_info);

  EXPECT_EQ(Color(0, 0, 255), paint_style.fill_color);

  Color background_color = HighlightStyleUtils::HighlightBackgroundColor(
      GetDocument(), div_style, div_node, previous_layer_color,
      kPseudoIdSelection);

  EXPECT_EQ(Color(0, 128, 0), background_color);
}

TEST_F(HighlightStyleUtilsTest, CustomPropertyInheritanceNoRoot) {
  ScopedHighlightInheritanceForTest highlight_inheritance_enabled(true);
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      :root {
        --background-color: green;
      }
      div::selection {
        background-color: var(--background-color, red);
      }
    </style>
    <div>Selected</div>
  )HTML");

  // Select some text.
  auto* div_node =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));
  Window().getSelection()->setBaseAndExtent(div_node, 0, div_node, 1);
  Compositor().BeginFrame();

  const ComputedStyle& div_style = div_node->ComputedStyleRef();
  absl::optional<Color> previous_layer_color;
  Color background_color = HighlightStyleUtils::HighlightBackgroundColor(
      GetDocument(), div_style, div_node, previous_layer_color,
      kPseudoIdSelection);

  EXPECT_EQ(Color(0, 128, 0), background_color);
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

}  // namespace blink
