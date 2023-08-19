// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight_style_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/dom_selection.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

Color SelectionWebkitTextFillColor(const Document& document,
                                   Node* node,
                                   const ComputedStyle& originating_style) {
  scoped_refptr<const ComputedStyle> pseudo_style =
      HighlightStyleUtils::HighlightPseudoStyle(node, originating_style,
                                                kPseudoIdSelection);
  return HighlightStyleUtils::ResolveColor(
      document, originating_style, pseudo_style.get(), kPseudoIdSelection,
      GetCSSPropertyWebkitTextFillColor(), Color::kBlack);
}

}  // namespace

class HighlightStyleUtilsTest : public SimTest,
                                private ScopedHighlightInheritanceForTest {
 public:
  // TODO(crbug.com/1024156) remove CachedPseudoStyles tests, but keep
  // SelectedTextInputShadow, when HighlightInheritance becomes stable
  HighlightStyleUtilsTest() : ScopedHighlightInheritanceForTest(false) {}
};

TEST_F(HighlightStyleUtilsTest, CachedPseudoStylesWindowInactive) {
  // Test that we are only caching active selection styles as so that we don't
  // incorrectly use a cached ComputedStyle when the active state changes.

  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      ::selection:window-inactive {color: red }
      ::selection { color: green }
    </style>
    <body>Text to select.</body>
  )HTML");

  auto* body = GetDocument().body();
  auto* text_node = body->firstChild();

  Compositor().BeginFrame();

  const ComputedStyle& body_style = body->ComputedStyleRef();
  const ComputedStyle& text_style = text_node->ComputedStyleRef();

  EXPECT_FALSE(body_style.GetCachedPseudoElementStyle(kPseudoIdSelection));

  // Select some text.
  Window().getSelection()->setBaseAndExtent(body, 0, body, 1);
  Compositor().BeginFrame();

  // We don't cache ::selection styles for :window-inactive.
  EXPECT_FALSE(body_style.GetCachedPseudoElementStyle(kPseudoIdSelection));

  EXPECT_FALSE(GetPage().IsActive());
  EXPECT_EQ(Color(255, 0, 0),
            SelectionWebkitTextFillColor(GetDocument(), text_node, text_style));

  // Focus the window.
  GetPage().SetActive(true);
  Compositor().BeginFrame();
  EXPECT_EQ(Color(0, 128, 0),
            SelectionWebkitTextFillColor(GetDocument(), text_node, text_style));
  const ComputedStyle* active_style =
      body_style.GetCachedPseudoElementStyle(kPseudoIdSelection);
  EXPECT_TRUE(active_style);

  // Unfocus the window.
  GetPage().SetActive(false);
  Compositor().BeginFrame();
  EXPECT_EQ(Color(255, 0, 0),
            SelectionWebkitTextFillColor(GetDocument(), text_node, text_style));
  EXPECT_EQ(active_style,
            body_style.GetCachedPseudoElementStyle(kPseudoIdSelection));
}

TEST_F(HighlightStyleUtilsTest, CachedPseudoStylesNoWindowInactive) {
  // Test that we share a cached ComputedStyle for active and inactive
  // selections when there are no :window-inactive styles.

  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      ::selection { color: green }
    </style>
    <body>Text to select.</body>
  )HTML");

  auto* body = GetDocument().body();
  auto* text_node = body->firstChild();

  Compositor().BeginFrame();

  const ComputedStyle& body_style = body->ComputedStyleRef();
  const ComputedStyle& text_style = text_node->ComputedStyleRef();

  EXPECT_FALSE(body_style.GetCachedPseudoElementStyle(kPseudoIdSelection));

  // Select some text.
  Window().getSelection()->setBaseAndExtent(body, 0, body, 1);
  Compositor().BeginFrame();

  // We cache inactive ::selection styles when there are no :window-inactive
  // selectors.
  const ComputedStyle* active_style =
      body_style.GetCachedPseudoElementStyle(kPseudoIdSelection);
  EXPECT_TRUE(active_style);

  EXPECT_FALSE(GetPage().IsActive());
  EXPECT_EQ(Color(0, 128, 0),
            SelectionWebkitTextFillColor(GetDocument(), text_node, text_style));

  // Focus the window.
  GetPage().SetActive(true);
  Compositor().BeginFrame();
  EXPECT_EQ(Color(0, 128, 0),
            SelectionWebkitTextFillColor(GetDocument(), text_node, text_style));
  EXPECT_EQ(active_style,
            body_style.GetCachedPseudoElementStyle(kPseudoIdSelection));

  // Unfocus the window.
  GetPage().SetActive(false);
  Compositor().BeginFrame();
  EXPECT_EQ(Color(0, 128, 0),
            SelectionWebkitTextFillColor(GetDocument(), text_node, text_style));
  EXPECT_EQ(active_style,
            body_style.GetCachedPseudoElementStyle(kPseudoIdSelection));
}

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
  const ComputedStyle& text_style = text_node->ComputedStyleRef();

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

}  // namespace blink
