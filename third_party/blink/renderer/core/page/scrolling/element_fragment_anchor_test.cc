// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/element_fragment_anchor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

using test::RunPendingTasks;

class ElementFragmentAnchorTest : public SimTest {
  void SetUp() override {
    SimTest::SetUp();

    // Focus handlers aren't run unless the page is focused.
    GetDocument().GetPage()->GetFocusController().SetActive(true);
    GetDocument().GetPage()->GetFocusController().SetFocused(true);

    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }
};

// Ensure that the focus event handler is run before the rAF callback. We'll
// change the background color from a rAF set in the focus handler and make
// sure the computed background color of that frame was changed. See:
// https://groups.google.com/a/chromium.org/d/msg/blink-dev/5BJSTl-FMGY/JMtaKqGhBAAJ
TEST_F(ElementFragmentAnchorTest, FocusHandlerRunBeforeRaf) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_resource("https://example.com/sheet.css",
                                     "text/css");
  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
          background-color: red;
        }
      </style>
      <a id="anchorlink" href="#bottom">Link to bottom of the page</a>
      <div style="height: 1000px;"></div>
      <link rel="stylesheet" type="text/css" href="sheet.css">
      <input id="bottom">Bottom of the page</input>
      <script>
        document.getElementById("bottom").addEventListener('focus', () => {
          requestAnimationFrame(() => {
            document.body.style.backgroundColor = '#00FF00';
          });
        });
      </script>
    )HTML");

  // We're still waiting on the stylesheet to load so the load event shouldn't
  // yet dispatch.
  ASSERT_FALSE(GetDocument().IsLoadCompleted());

  // Click on the anchor element. This will cause a synchronous same-document
  // navigation. The fragment shouldn't activate yet as parsing will be blocked
  // due to the unloaded stylesheet.
  auto* anchor = To<HTMLAnchorElement>(
      GetDocument().getElementById(AtomicString("anchorlink")));
  anchor->click();
  ASSERT_EQ(GetDocument().body(), GetDocument().ActiveElement())
      << "Active element changed while rendering is blocked";

  // Complete the CSS stylesheet load so the document can finish parsing.
  css_resource.Complete("");
  test::RunPendingTasks();

  // Now that the document has fully parsed the anchor should invoke at this
  // point.
  ASSERT_EQ(GetDocument().getElementById(AtomicString("bottom")),
            GetDocument().ActiveElement());

  // The background color shouldn't yet be updated.
  ASSERT_EQ(GetDocument()
                .body()
                ->GetLayoutObject()
                ->Style()
                ->VisitedDependentColor(GetCSSPropertyBackgroundColor())
                .NameForLayoutTreeAsText(),
            Color(255, 0, 0).NameForLayoutTreeAsText());

  Compositor().BeginFrame();

  // Make sure the background color is updated from the rAF without requiring a
  // second BeginFrame.
  EXPECT_EQ(GetDocument()
                .body()
                ->GetLayoutObject()
                ->Style()
                ->VisitedDependentColor(GetCSSPropertyBackgroundColor())
                .NameForLayoutTreeAsText(),
            Color(0, 255, 0).NameForLayoutTreeAsText());
}

// This test ensures that when an iframe's document is closed, and the parent
// has dirty layout, the iframe is laid out prior to invoking its fragment
// anchor. Without performing this layout, the anchor cannot scroll to the
// correct location and it will be cleared since the document is closed.
TEST_F(ElementFragmentAnchorTest, IframeFragmentNoLayoutUntilLoad) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest child_resource("https://example.com/child.html#fragment",
                            "text/html");
  LoadURL("https://example.com/test.html");

  // Don't clcose the main document yet, since that'll cause it to layout.
  main_resource.Write(R"HTML(
      <!DOCTYPE html>
      <style>
        iframe {
          border: 0;
          width: 300px;
          height: 200px;
        }
      </style>
      <iframe id="child" src="child.html#fragment"></iframe>
    )HTML");

  // When the iframe document is loaded, it'll try to scroll the fragment into
  // view. Ensure it does so correctly by laying out first.
  child_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <div style="height:500px;">content</div>
      <div id="fragment">fragment content</div>
    )HTML");
  Compositor().BeginFrame();

  HTMLFrameOwnerElement* iframe = To<HTMLFrameOwnerElement>(
      GetDocument().getElementById(AtomicString("child")));
  ScrollableArea* child_viewport =
      iframe->contentDocument()->View()->LayoutViewport();
  Element* fragment =
      iframe->contentDocument()->getElementById(AtomicString("fragment"));

  gfx::Rect fragment_rect_in_frame =
      fragment->GetLayoutObject()->AbsoluteBoundingBoxRect();
  gfx::Rect viewport_rect(child_viewport->VisibleContentRect().size());

  EXPECT_TRUE(viewport_rect.Contains(fragment_rect_in_frame))
      << "Fragment element at [" << fragment_rect_in_frame.ToString()
      << "] was not scrolled into viewport rect [" << viewport_rect.ToString()
      << "]";

  main_resource.Finish();
}

// This test ensures that we correctly scroll the fragment into view in the
// case that the iframe has finished load but layout becomes dirty (in both
// parent and iframe) before we've had a chance to scroll the fragment into
// view.
TEST_F(ElementFragmentAnchorTest, IframeFragmentDirtyLayoutAfterLoad) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest child_resource("https://example.com/child.html#fragment",
                            "text/html");
  LoadURL("https://example.com/test.html");

  // Don't clcose the main document yet, since that'll cause it to layout.
  main_resource.Write(R"HTML(
      <!DOCTYPE html>
      <style>
        iframe {
          border: 0;
          width: 300px;
          height: 200px;
        }
      </style>
      <iframe id="child" src="child.html#fragment"></iframe>
    )HTML");

  // Use text so that changing the iframe width will change the y-location of
  // the fragment.
  child_resource.Complete(R"HTML(
      <!DOCTYPE html>
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum Lorem Ipsum
      <div id="fragment">fragment content</div>
    )HTML");

  HTMLFrameOwnerElement* iframe = To<HTMLFrameOwnerElement>(
      GetDocument().getElementById(AtomicString("child")));
  iframe->setAttribute(html_names::kStyleAttr, AtomicString("width:100px"));

  Compositor().BeginFrame();

  ScrollableArea* child_viewport =
      iframe->contentDocument()->View()->LayoutViewport();
  Element* fragment =
      iframe->contentDocument()->getElementById(AtomicString("fragment"));

  gfx::Rect fragment_rect_in_frame =
      fragment->GetLayoutObject()->AbsoluteBoundingBoxRect();
  gfx::Rect viewport_rect(child_viewport->VisibleContentRect().size());

  EXPECT_TRUE(viewport_rect.Contains(fragment_rect_in_frame))
      << "Fragment element at [" << fragment_rect_in_frame.ToString()
      << "] was not scrolled into viewport rect [" << viewport_rect.ToString()
      << "]";

  main_resource.Finish();
}

// Ensure that a BeginFrame after the element-to-focus is removed from the
// document doesn't cause a nullptr crash when the fragment anchor element has
// been removed and garbage collected.
TEST_F(ElementFragmentAnchorTest, AnchorRemovedBeforeBeginFrameCrash) {
  SimRequest main_resource("https://example.com/test.html#anchor", "text/html");
  SimSubresourceRequest css_resource("https://example.com/sheet.css",
                                     "text/css");
  LoadURL("https://example.com/test.html#anchor");

  main_resource.Complete(R"HTML(
        <!DOCTYPE html>
        <link rel="stylesheet" type="text/css" href="sheet.css">
        <div style="height: 1000px;"></div>
        <input id="anchor">Bottom of the page</input>
      )HTML");

  // We're still waiting on the stylesheet to load so the load event shouldn't
  // yet dispatch and parsing is deferred. This will install the anchor.
  ASSERT_FALSE(GetDocument().IsLoadCompleted());

  ASSERT_TRUE(GetDocument().View()->GetFragmentAnchor());
  ASSERT_TRUE(static_cast<ElementFragmentAnchor*>(
                  GetDocument().View()->GetFragmentAnchor())
                  ->anchor_node_.Get());

  // Remove the fragment anchor from the DOM and perform GC.
  GetDocument().getElementById(AtomicString("anchor"))->remove();
  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_TRUE(GetDocument().View()->GetFragmentAnchor());
  EXPECT_FALSE(static_cast<ElementFragmentAnchor*>(
                   GetDocument().View()->GetFragmentAnchor())
                   ->anchor_node_.Get());

  // Now that the element has been removed and GC'd, unblock parsing. The
  // anchor should be installed at this point. When parsing finishes, a
  // synchronous layout update will run, which will invoke the fragment anchor.
  css_resource.Complete("");
  test::RunPendingTasks();

  // When the document finishes loading, it does a synchronous layout update,
  // which should clear LocalFrameView::fragment_anchor_ ...
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());

  // Allow any enqueued animation frame tasks to run
  // so their resources can be cleaned up.
  Compositor().BeginFrame();

  // Non-crash is considered a pass.
}

// Ensure that an SVG document doesn't automatically create a fragment anchor
// without the URL actually having a fragment.
TEST_F(ElementFragmentAnchorTest, SVGDocumentDoesntCreateFragment) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest svg_resource("https://example.com/file.svg", "image/svg+xml");

  LoadURL("https://example.com/test.html");

  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <img id="image" src=file.svg>
    )HTML");

  // Load an SVG that's transformed outside of the container rect. Ensure that
  // we don't scroll it into view since we didn't specify a hash fragment.
  svg_resource.Complete(R"SVG(
      <svg id="svg" width="50" height="50" xmlns="http://www.w3.org/2000/svg">
         <style>
          #svg{
            transform: translateX(200px) translateY(200px);
          }
         </style>
         <circle class="path" cx="50" cy="50" r="20" fill="red"/>
      </svg>
    )SVG");

  auto* img =
      To<HTMLImageElement>(GetDocument().getElementById(AtomicString("image")));
  auto* svg = To<SVGImage>(img->CachedImage()->GetImage());
  auto* view =
      DynamicTo<LocalFrameView>(svg->GetPageForTesting()->MainFrame()->View());

  // Scroll should remain unchanged and no anchor should be set.
  ASSERT_EQ(ScrollOffset(), view->GetScrollableArea()->GetScrollOffset());
  ASSERT_FALSE(view->GetFragmentAnchor());

  // Check after a BeginFrame as well since SVG documents appear to process the
  // fragment at this time as well.
  Compositor().BeginFrame();
  ASSERT_EQ(ScrollOffset(), view->GetScrollableArea()->GetScrollOffset());
  ASSERT_FALSE(view->GetFragmentAnchor());
}

// This test ensures that we correctly scroll the fragment into view in the
// case that the fragment has characters which need to be URL encoded.
TEST_F(ElementFragmentAnchorTest, HasURLEncodedCharacters) {
  SimRequest main_resource(u"https://example.com/t.html#\u00F6", "text/html");
  LoadURL(u"https://example.com/t.html#\u00F6");

  main_resource.Complete(
      u"<html>\n"
      // SimRequest sends UTF-8 to parser but the parser defaults to UTF-16.
      u"    <head><meta charset=\"UTF-8\"></head>\n"
      u"    <body>\n"
      u"        <div style=\"height: 50cm;\">blank space</div>\n"
      u"        <h1 id=\"\u00F6\">\u00D6</h1>\n"
      // TODO(1117212): The escaped version currently takes precedence.
      // u"     <div style=\"height: 50cm;\">blank space</div>\n"
      // u"     <h1 id=\"%C3%B6\">\u00D62</h1>\n"
      u"        <div style=\"height: 50cm;\">blank space</div>\n"
      u"        <h1 id=\"non-umlaut\">non-umlaut</h1>\n"
      u"    </body>\n"
      u"</html>");

  Compositor().BeginFrame();

  ScrollableArea* viewport = GetDocument().View()->LayoutViewport();
  Element* fragment = GetDocument().getElementById(AtomicString(u"\u00F6"));
  ASSERT_NE(nullptr, fragment);

  gfx::Rect fragment_rect_in_frame =
      fragment->GetLayoutObject()->AbsoluteBoundingBoxRect();
  gfx::Rect viewport_rect(viewport->VisibleContentRect().size());

  EXPECT_TRUE(viewport_rect.Contains(fragment_rect_in_frame))
      << "Fragment element at [" << fragment_rect_in_frame.ToString()
      << "] was not scrolled into viewport rect [" << viewport_rect.ToString()
      << "]";
}

}  // namespace blink
