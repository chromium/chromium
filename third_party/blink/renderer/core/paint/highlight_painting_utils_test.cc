// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/highlight_painting_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
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

namespace blink {

class HighlightPaintingUtilsTest : public SimTest {};

TEST_F(HighlightPaintingUtilsTest, CachedPseudoStylesWindowInactive) {
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
  GlobalPaintFlags flags{0};

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
  EXPECT_EQ(Color(255, 0, 0), HighlightPaintingUtils::HighlightForegroundColor(
                                  GetDocument(), text_style, text_node,
                                  kPseudoIdSelection, flags));

  // Focus the window.
  GetPage().SetActive(true);
  Compositor().BeginFrame();
  EXPECT_EQ(Color(0, 128, 0), HighlightPaintingUtils::HighlightForegroundColor(
                                  GetDocument(), text_style, text_node,
                                  kPseudoIdSelection, flags));
  const ComputedStyle* active_style =
      body_style.GetCachedPseudoElementStyle(kPseudoIdSelection);
  EXPECT_TRUE(active_style);

  // Unfocus the window.
  GetPage().SetActive(false);
  Compositor().BeginFrame();
  EXPECT_EQ(Color(255, 0, 0), HighlightPaintingUtils::HighlightForegroundColor(
                                  GetDocument(), text_style, text_node,
                                  kPseudoIdSelection, flags));
  EXPECT_EQ(active_style,
            body_style.GetCachedPseudoElementStyle(kPseudoIdSelection));
}

TEST_F(HighlightPaintingUtilsTest, CachedPseudoStylesNoWindowInactive) {
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
  GlobalPaintFlags flags{0};

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
  EXPECT_EQ(Color(0, 128, 0), HighlightPaintingUtils::HighlightForegroundColor(
                                  GetDocument(), text_style, text_node,
                                  kPseudoIdSelection, flags));

  // Focus the window.
  GetPage().SetActive(true);
  Compositor().BeginFrame();
  EXPECT_EQ(Color(0, 128, 0), HighlightPaintingUtils::HighlightForegroundColor(
                                  GetDocument(), text_style, text_node,
                                  kPseudoIdSelection, flags));
  EXPECT_EQ(active_style,
            body_style.GetCachedPseudoElementStyle(kPseudoIdSelection));

  // Unfocus the window.
  GetPage().SetActive(false);
  Compositor().BeginFrame();
  EXPECT_EQ(Color(0, 128, 0), HighlightPaintingUtils::HighlightForegroundColor(
                                  GetDocument(), text_style, text_node,
                                  kPseudoIdSelection, flags));
  EXPECT_EQ(active_style,
            body_style.GetCachedPseudoElementStyle(kPseudoIdSelection));
}

TEST_F(HighlightPaintingUtilsTest, SelectedTextInputShadow) {
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

  auto* text_node = To<HTMLInputElement>(GetDocument().QuerySelector("input"))
                        ->InnerEditorElement()
                        ->firstChild();
  const ComputedStyle& text_style = text_node->ComputedStyleRef();

  std::unique_ptr<PaintController> controller{
      std::make_unique<PaintController>()};
  GraphicsContext context(*controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground,
                       kGlobalPaintNormalPhase, 0 /* paint_flags */);
  TextPaintStyle paint_style;

  paint_style = HighlightPaintingUtils::HighlightPaintingStyle(
      GetDocument(), text_style, text_node, kPseudoIdSelection, paint_style,
      paint_info);

  EXPECT_EQ(Color(0, 128, 0), paint_style.fill_color);
  EXPECT_TRUE(paint_style.shadow);
}

}  // namespace blink
