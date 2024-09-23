// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_text_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

// This test suite verifies that after font changes (e.g., font loaded), we do
// not invalidate the full document's style or layout, but for affected elements
// only.
class FontUpdateInvalidationTest : public SimTest {
 public:
  FontUpdateInvalidationTest() = default;

 protected:
  static Vector<char> ReadAhemWoff2() {
    return *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2"));
  }
};

TEST_F(FontUpdateInvalidationTest, PartialLayoutInvalidationAfterFontLoading) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
      #reference {
        font: 25px/1 monospace;
      }
    </style>
    <div><span id=target>0123456789</span></div>
    <div><span id=reference>0123456789</div>
  )HTML");

  // First rendering the page with fallback
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  Element* reference = GetDocument().getElementById(AtomicString("reference"));

  EXPECT_GT(250, target->OffsetWidth());
  EXPECT_GT(250, reference->OffsetWidth());

  // Finish font loading, and trigger invalidations.
  font_resource.Complete(ReadAhemWoff2());
  GetDocument().GetStyleEngine().InvalidateStyleAndLayoutForFontUpdates();

  // No element is marked for style recalc, since no computed style is changed.
  EXPECT_EQ(kNoStyleChange, target->GetStyleChangeType());
  EXPECT_EQ(kNoStyleChange, reference->GetStyleChangeType());

  // Only elements that had pending custom fonts are marked for relayout.
  EXPECT_TRUE(target->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(reference->GetLayoutObject()->NeedsLayout());

  Compositor().BeginFrame();
  EXPECT_EQ(250, target->OffsetWidth());
  EXPECT_GT(250, reference->OffsetWidth());

  main_resource.Finish();
}

TEST_F(FontUpdateInvalidationTest,
       PartialLayoutInvalidationAfterFontLoadingSVG) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
      #reference {
        font: 25px/1 monospace;
      }
    </style>
    <svg><text id=target dx=0,10 transform="scale(3)">0123456789</text></svg>
    <svg><text id=reference dx=0,10>0123456789</text></svg>
  )HTML");

  // First rendering the page with fallback
  Compositor().BeginFrame();

  auto* target =
      To<SVGTextElement>(GetDocument().getElementById(AtomicString("target")));
  auto* reference = To<SVGTextElement>(
      GetDocument().getElementById(AtomicString("reference")));

  EXPECT_GT(250 + 10, target->GetBBox().width());
  EXPECT_GT(250 + 10, reference->GetBBox().width());

  // Finish font loading, and trigger invalidations.
  font_resource.Complete(ReadAhemWoff2());
  // FontFallbackMap::FontsNeedUpdate() should make the fallback list invalid.
  EXPECT_FALSE(target->firstChild()->GetLayoutObject()->IsFontFallbackValid());
  GetDocument().GetStyleEngine().InvalidateStyleAndLayoutForFontUpdates();

  // No element is marked for style recalc, since no computed style is changed.
  EXPECT_EQ(kNoStyleChange, target->GetStyleChangeType());
  EXPECT_EQ(kNoStyleChange, reference->GetStyleChangeType());

  // Only elements that had pending custom fonts are marked for relayout.
  EXPECT_TRUE(target->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(reference->GetLayoutObject()->NeedsLayout());

  Compositor().BeginFrame();
  EXPECT_EQ(250 + 10, target->GetBBox().width());
  EXPECT_GT(250 + 10, reference->GetBBox().width());

  main_resource.Finish();
}

TEST_F(FontUpdateInvalidationTest,
       PartialLayoutInvalidationAfterFontFaceDeletion) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <script>
    const face = new FontFace('custom-font',
                              'url(https://example.com/Ahem.woff2)');
    face.load();
    document.fonts.add(face);
    </script>
    <style>
      #target {
        font: 25px/1 custom-font, monospace;
      }
      #reference {
        font: 25px/1 monospace;
      }
    </style>
    <div><span id=target>0123456789</span></div>
    <div><span id=reference>0123456789</div>
  )HTML");

  // First render the page with the custom font
  font_resource.Complete(ReadAhemWoff2());
  test::RunPendingTasks();
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  Element* reference = GetDocument().getElementById(AtomicString("reference"));

  EXPECT_EQ(250, target->OffsetWidth());
  EXPECT_GT(250, reference->OffsetWidth());

  // Then delete the custom font, and trigger invalidations
  main_resource.Write("<script>document.fonts.delete(face);</script>");
  GetDocument().GetStyleEngine().InvalidateStyleAndLayoutForFontUpdates();

  // No element is marked for style recalc, since no computed style is changed.
  EXPECT_EQ(kNoStyleChange, target->GetStyleChangeType());
  EXPECT_EQ(kNoStyleChange, reference->GetStyleChangeType());

  // Only elements using custom fonts are marked for relayout.
  EXPECT_TRUE(target->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(reference->GetLayoutObject()->NeedsLayout());

  Compositor().BeginFrame();
  EXPECT_GT(250, target->OffsetWidth());
  EXPECT_GT(250, reference->OffsetWidth());

  main_resource.Finish();
}

// https://crbug.com/1092411
TEST_F(FontUpdateInvalidationTest, LayoutInvalidationOnModalDialog) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <dialog><span id=target>0123456789</span></dialog>
    <script>document.querySelector('dialog').showModal();</script>
  )HTML");

  // First render the page without the custom font
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_GT(250, target->OffsetWidth());

  // Then load the font and invalidate layout
  font_resource.Complete(ReadAhemWoff2());
  GetDocument().GetStyleEngine().InvalidateStyleAndLayoutForFontUpdates();

  // <dialog> descendants should be invalidated
  EXPECT_EQ(kNoStyleChange, target->GetStyleChangeType());
  EXPECT_TRUE(target->GetLayoutObject()->NeedsLayout());

  // <dialog> descendants should be re-rendered with the custom font
  Compositor().BeginFrame();
  EXPECT_EQ(250, target->OffsetWidth());

  main_resource.Finish();
}

TEST_F(FontUpdateInvalidationTest, FallbackBetweenPendingAndLoadedCustomFonts) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest slow_font_resource("https://example.com/nonexist.woff2",
                                           "font/woff2");
  SimSubresourceRequest fast_font_resource("https://example.com/Ahem.woff2",
                                           "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <link rel="preload" href="https://example.com/Ahem.woff2" as="font" crossorigin>
    <style>
      @font-face {
        font-family: slow-font;
        src: url(https://example.com/nonexist.woff2) format("woff2");
      }
      @font-face {
        font-family: fast-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 slow-font, fast-font, monospace;
      }
    </style>
    <span id=target>0123456789</span>
  )HTML");

  fast_font_resource.Complete(ReadAhemWoff2());
  test::RunPendingTasks();

  // While slow-font is pending and fast-font is already available, we should
  // use it to render the page.
  Compositor().BeginFrame();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  DCHECK_EQ(250, target->OffsetWidth());

  slow_font_resource.Complete();

  Compositor().BeginFrame();
  EXPECT_EQ(250, target->OffsetWidth());
}

// https://crrev.com/1397423004
TEST_F(FontUpdateInvalidationTest, NoRedundantLoadingForSegmentedFont) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/font2.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        /* We intentionally leave it unmocked, so that the test fails if it
         * attempts to load font1.woff. */
        src: url(https://example.com/font1.woff2) format("woff2");
        unicode-range: 0x00-0xFF;
      }
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/font2.woff2) format("woff2");
        unicode-range: 0x30-0x39; /* '0' to '9' */
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target>0123456789</span>
  )HTML");

  // Trigger frame to start font loading
  Compositor().BeginFrame();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  DCHECK_GT(250, target->OffsetWidth());

  font_resource.Complete(ReadAhemWoff2());

  Compositor().BeginFrame();
  DCHECK_EQ(250, target->OffsetWidth());

  // Test finishes without triggering a redundant load of font1.woff.
}

}  // namespace blink
