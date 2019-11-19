// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink.h"
#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/bindings/core/v8/scroll_into_view_options_or_boolean.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/find_in_page.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/scroll_into_view_options.h"
#include "third_party/blink/renderer/core/frame/scroll_to_options.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

namespace {

class ScrollIntoViewTest : public SimTest {};

TEST_F(ScrollIntoViewTest, InstantScroll) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='space' style='height: 1000px'></div>"
      "<div id='content' style='height: 1000px'></div>");

  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);
  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
  options->setBlock("start");
  arg.SetScrollIntoViewOptions(options);
  content->scrollIntoView(arg);

  ASSERT_EQ(Window().scrollY(), content->OffsetTop());
}

TEST_F(ScrollIntoViewTest, ScrollPaddingOnDocumentElWhenBodyDefinesViewport) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(300, 300));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <style>
      html {
        scroll-padding: 10px;
      }
      body {
        margin: 0px;
        height: 300px;
        overflow: scroll;
      }
      </style>
      <div id='space' style='height: 1000px'></div>
      <div id='target' style='height: 200px;'></div>
      <div id='space' style='height: 1000px'></div>
    )HTML");

  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);
  Element* target = GetDocument().getElementById("target");
  target->scrollIntoView();

  // Sanity check that document element is the viewport defining element
  ASSERT_EQ(GetDocument().body(), GetDocument().ViewportDefiningElement());
  ASSERT_EQ(Window().scrollY(), target->OffsetTop() - 10);
}

TEST_F(ScrollIntoViewTest,
       ScrollPaddingOnDocumentElWhenDocumentElDefinesViewport) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(300, 300));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <style>
      :root {
        height: 300px;
        overflow: scroll;
        scroll-padding: 10px;
      }
      </style>
      <div id='space' style='height: 1000px'></div>
      <div id='target' style='height: 200px;'></div>
      <div id='space' style='height: 1000px'></div>
    )HTML");

  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);
  Element* target = GetDocument().getElementById("target");
  target->scrollIntoView();

  // Sanity check that document element is the viewport defining element
  ASSERT_EQ(GetDocument().documentElement(),
            GetDocument().ViewportDefiningElement());
  ASSERT_EQ(Window().scrollY(), target->OffsetTop() - 10);
}

TEST_F(ScrollIntoViewTest, ScrollPaddingOnBodyWhenDocumentElDefinesViewport) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(300, 300));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <style>
      :root {
        height: 300px;
        overflow: scroll;
        scroll-padding: 2px;
      }
      body {
        margin: 0px;
        height: 400px;
        overflow: scroll;
        scroll-padding: 10px;
      }
      </style>
      <div id='space' style='height: 1000px'></div>
      <div id='target' style='height: 200px;'></div>
      <div id='space' style='height: 1000px'></div>
    )HTML");

  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);
  Element* target = GetDocument().getElementById("target");
  target->scrollIntoView();

  // Sanity check that document element is the viewport defining element
  ASSERT_EQ(GetDocument().documentElement(),
            GetDocument().ViewportDefiningElement());

  // When body and document elements are both scrollable then both the body and
  // element should scroll and align with its padding.
  Element* body = GetDocument().body();
  ASSERT_EQ(body->scrollTop(), target->OffsetTop() - 10);
  ASSERT_EQ(Window().scrollY(), 10 - 2);
}

TEST_F(ScrollIntoViewTest, SmoothScroll) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='space' style='height: 1000px'></div>"
      "<div id='content' style='height: 1000px'></div>");

  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
  options->setBlock("start");
  options->setBehavior("smooth");
  arg.SetScrollIntoViewOptions(options);
  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);

  content->scrollIntoView(arg);
  // Scrolling the container
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_NEAR(Window().scrollY(), 299, 1);

  // Finish scrolling the container
  Compositor().BeginFrame(1);
  ASSERT_EQ(Window().scrollY(), content->OffsetTop());
}

TEST_F(ScrollIntoViewTest, NestedContainer) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <div id='space' style='height: 1000px'></div>
    <div id='container' style='height: 600px; overflow: scroll'>
      <div id='space1' style='height: 1000px'></div>
      <div id='content' style='height: 1000px'></div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById("container");
  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
  options->setBlock("start");
  options->setBehavior("smooth");
  arg.SetScrollIntoViewOptions(options);
  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);
  ASSERT_EQ(container->scrollTop(), 0);

  content->scrollIntoView(arg);
  // Scrolling the outer container
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_NEAR(Window().scrollY(), 299, 1);
  ASSERT_EQ(container->scrollTop(), 0);

  // Finish scrolling the outer container
  Compositor().BeginFrame(1);
  ASSERT_EQ(Window().scrollY(), container->OffsetTop());
  ASSERT_EQ(container->scrollTop(), 0);

  // Scrolling the inner container
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_NEAR(container->scrollTop(), 299, 1);

  // Finish scrolling the inner container
  Compositor().BeginFrame(1);
  ASSERT_EQ(container->scrollTop(),
            content->OffsetTop() - container->OffsetTop());
}

TEST_F(ScrollIntoViewTest, NewScrollIntoViewAbortsCurrentAnimation) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <div id='container2' style='height: 1000px; overflow: scroll'>
      <div id='space2' style='height: 1200px'></div>
      <div id='content2' style='height: 1000px'></div>
    </div>
    <div id='container1' style='height: 600px; overflow: scroll'>
      <div id='space1' style='height: 1000px'></div>
      <div id='content1' style='height: 1000px'></div>
    </div>
  )HTML");

  Element* container1 = GetDocument().getElementById("container1");
  Element* container2 = GetDocument().getElementById("container2");
  Element* content1 = GetDocument().getElementById("content1");
  Element* content2 = GetDocument().getElementById("content2");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
  options->setBlock("start");
  options->setBehavior("smooth");
  arg.SetScrollIntoViewOptions(options);

  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);
  ASSERT_EQ(container1->scrollTop(), 0);
  ASSERT_EQ(container2->scrollTop(), 0);

  content1->scrollIntoView(arg);
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_NEAR(Window().scrollY(), 299, 1);
  ASSERT_EQ(container1->scrollTop(), 0);

  content2->scrollIntoView(arg);
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_NEAR(Window().scrollY(), 61, 1);
  ASSERT_EQ(container1->scrollTop(), 0);  // container1 should not scroll.

  Compositor().BeginFrame(1);
  ASSERT_EQ(Window().scrollY(), container2->OffsetTop());
  ASSERT_EQ(container2->scrollTop(), 0);

  // Scrolling content2 in container2
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_NEAR(container2->scrollTop(), 300, 1);

  // Finish all the animation to make sure there is no another animation queued
  // on container1.
  while (Compositor().NeedsBeginFrame()) {
    Compositor().BeginFrame();
  }
  ASSERT_EQ(Window().scrollY(), container2->OffsetTop());
  ASSERT_EQ(container2->scrollTop(),
            content2->OffsetTop() - container2->OffsetTop());
  ASSERT_EQ(container1->scrollTop(), 0);
}

TEST_F(ScrollIntoViewTest, ScrollWindowAbortsCurrentAnimation) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <div id='space' style='height: 1000px'></div>
    <div id='container' style='height: 600px; overflow: scroll'>
      <div id='space1' style='height: 1000px'></div>
      <div id='content' style='height: 1000px'></div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById("container");
  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
  options->setBlock("start");
  options->setBehavior("smooth");
  arg.SetScrollIntoViewOptions(options);
  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);
  ASSERT_EQ(container->scrollTop(), 0);

  content->scrollIntoView(arg);
  // Scrolling the outer container
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_NEAR(Window().scrollY(), 299, 1);
  ASSERT_EQ(container->scrollTop(), 0);

  ScrollToOptions* window_option = ScrollToOptions::Create();
  window_option->setLeft(0);
  window_option->setTop(0);
  window_option->setBehavior("smooth");
  Window().scrollTo(window_option);
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_NEAR(Window().scrollY(), 58, 1);

  Compositor().BeginFrame(1);
  ASSERT_EQ(Window().scrollY(), 0);
  ASSERT_EQ(container->scrollTop(), 0);
}

TEST_F(ScrollIntoViewTest, BlockAndInlineSettings) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <div id='container' style='height: 2500px; width: 2500px;'>
    <div id='content' style='height: 500px; width: 500px;
    margin-left: 1000px; margin-right: 1000px; margin-top: 1000px;
    margin-bottom: 1000px'></div></div>
  )HTML");

  int content_height = 500;
  int content_width = 500;
  int window_height = 600;
  int window_width = 800;

  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg1, arg2, arg3, arg4;
  ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
  ASSERT_EQ(Window().scrollY(), 0);

  options->setBlock("nearest");
  options->setInlinePosition("nearest");
  arg1.SetScrollIntoViewOptions(options);
  content->scrollIntoView(arg1);
  ASSERT_EQ(Window().scrollX(),
            content->OffsetLeft() + content_width - window_width);
  ASSERT_EQ(Window().scrollY(),
            content->OffsetTop() + content_height - window_height);

  options->setBlock("start");
  options->setInlinePosition("start");
  arg2.SetScrollIntoViewOptions(options);
  content->scrollIntoView(arg2);
  ASSERT_EQ(Window().scrollX(), content->OffsetLeft());
  ASSERT_EQ(Window().scrollY(), content->OffsetTop());

  options->setBlock("center");
  options->setInlinePosition("center");
  arg3.SetScrollIntoViewOptions(options);
  content->scrollIntoView(arg3);
  ASSERT_EQ(Window().scrollX(),
            content->OffsetLeft() + (content_width - window_width) / 2);
  ASSERT_EQ(Window().scrollY(),
            content->OffsetTop() + (content_height - window_height) / 2);

  options->setBlock("end");
  options->setInlinePosition("end");
  arg4.SetScrollIntoViewOptions(options);
  content->scrollIntoView(arg4);
  ASSERT_EQ(Window().scrollX(),
            content->OffsetLeft() + content_width - window_width);
  ASSERT_EQ(Window().scrollY(),
            content->OffsetTop() + content_height - window_height);
}

TEST_F(ScrollIntoViewTest, SmoothAndInstantInChain) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <div id='space' style='height: 1000px'></div>
    <div id='container' style='height: 600px; overflow: scroll;
      scroll-behavior: smooth'>
      <div id='space1' style='height: 1000px'></div>
      <div id='inner_container' style='height: 1000px; overflow: scroll;'>
        <div id='space2' style='height: 1000px'></div>
        <div id='content' style='height: 1000px;'></div>
      </div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById("container");
  Element* inner_container = GetDocument().getElementById("inner_container");
  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
  options->setBlock("start");
  arg.SetScrollIntoViewOptions(options);
  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);
  ASSERT_EQ(container->scrollTop(), 0);

  content->scrollIntoView(arg);
  // Instant scroll of the window should have finished.
  ASSERT_EQ(Window().scrollY(), container->OffsetTop());
  // Instant scroll of the inner container should not have started.
  ASSERT_EQ(container->scrollTop(), 0);
  // Smooth scroll should not have started.
  ASSERT_EQ(container->scrollTop(), 0);

  // Scrolling the container
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_NEAR(container->scrollTop(), 299, 1);

  // Finish scrolling the container
  Compositor().BeginFrame(1);
  ASSERT_EQ(container->scrollTop(),
            inner_container->OffsetTop() - container->OffsetTop());
  // Instant scroll of the inner container should have finished.
  ASSERT_EQ(inner_container->scrollTop(),
            content->OffsetTop() - inner_container->OffsetTop());
}

TEST_F(ScrollIntoViewTest, SmoothScrollAnchor) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html#link", "text/html");
  LoadURL("https://example.com/test.html#link");
  request.Complete(R"HTML(
    <div id='container' style='height: 600px; overflow: scroll;
      scroll-behavior: smooth'>
      <div id='space' style='height: 1000px'></div>
      <div style='height: 1000px'><a name='link'
    id='content'>hello</a></div>
    </div>
  )HTML");

  Element* content = GetDocument().getElementById("content");
  Element* container = GetDocument().getElementById("container");
  ASSERT_EQ(container->scrollTop(), 0);

  // Scrolling the container
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_NEAR(container->scrollTop(), 299, 1);

  // Finish scrolling the container
  Compositor().BeginFrame(1);
  ASSERT_EQ(container->scrollTop(),
            content->OffsetTop() - container->OffsetTop());
}

TEST_F(ScrollIntoViewTest, FindDoesNotScrollOverflowHidden) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <div id='container' style='height: 400px; overflow: hidden;'>
      <div id='space' style='height: 500px'></div>
      <div style='height: 500px'>hello</div>
    </div>
  )HTML");
  Element* container = GetDocument().getElementById("container");
  Compositor().BeginFrame();
  ASSERT_EQ(container->scrollTop(), 0);
  const int kFindIdentifier = 12345;
  auto options = mojom::blink::FindOptions::New();
  options->run_synchronously_for_testing = true;
  MainFrame().GetFindInPage()->FindInternal(
      kFindIdentifier, WebString::FromUTF8("hello"), *options, false);
  ASSERT_EQ(container->scrollTop(), 0);
}

TEST_F(ScrollIntoViewTest, ApplyRootElementScrollBehaviorToViewport) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<html style='scroll-behavior: smooth'>"
      "<div id='space' style='height: 1000px'></div>"
      "<div id='content' style='height: 1000px'></div></html>");

  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
  options->setBlock("start");
  arg.SetScrollIntoViewOptions(options);
  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);

  content->scrollIntoView(arg);
  // Scrolling the container
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_NEAR(Window().scrollY(), 299, 1);

  // Finish scrolling the container
  Compositor().BeginFrame(1);
  ASSERT_EQ(Window().scrollY(), content->OffsetTop());
}

// This test ensures the stop-at-layout viewport option works correctly when a
// non-default root scroller is set as the layout viewport.
TEST_F(ScrollIntoViewTest, StopAtLayoutViewportOption) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body,html {
        margin: 0;
        width: 100%;
        height: 100%;
      }
      #root {
        width: 100%;
        height: 100%;
        overflow: auto;
      }
      #inner {
        width: 100%;
        height: 100%;
        overflow: auto;
        margin-top: 1000px;
      }
      #target {
        margin-top: 1000px;
        margin-bottom: 1000px;
        width: 100px;
        height: 100px;
      }
    </style>
    <div id='root'>
      <div id='inner'>
        <div id='target'></div>
      <div>
    </div>
  )HTML");

  Compositor().BeginFrame();

  Element* root = GetDocument().getElementById("root");
  Element* inner = GetDocument().getElementById("inner");

  // Make sure the root scroller is set since that's what we're trying to test
  // here.
  {
    TopDocumentRootScrollerController& rs_controller =
        GetDocument().GetPage()->GlobalRootScrollerController();
    ASSERT_EQ(root, rs_controller.GlobalRootScroller());
  }

  // Use ScrollRectToVisible on the #target element, specifying
  // stop_at_main_frame_layout_viewport.
  LayoutObject* target =
      GetDocument().getElementById("target")->GetLayoutObject();
  WebScrollIntoViewParams params(
      ScrollAlignment::kAlignLeftAlways, ScrollAlignment::kAlignTopAlways,
      kProgrammaticScroll, false, kScrollBehaviorInstant);
  params.stop_at_main_frame_layout_viewport = true;
  target->ScrollRectToVisible(PhysicalRect(target->AbsoluteBoundingBoxRect()),
                              params);

  ScrollableArea* root_scroller =
      ToLayoutBox(root->GetLayoutObject())->GetScrollableArea();
  ScrollableArea* inner_scroller =
      ToLayoutBox(inner->GetLayoutObject())->GetScrollableArea();

  // Only the inner scroller should have scrolled. The root_scroller shouldn't
  // scroll because it is the layout viewport.
  ASSERT_EQ(root_scroller,
            &GetDocument().View()->GetRootFrameViewport()->LayoutViewport());
  EXPECT_EQ(ScrollOffset(), root_scroller->GetScrollOffset());
  EXPECT_EQ(ScrollOffset(0, 1000), inner_scroller->GetScrollOffset());
}

// This test passes if it doesn't crash/hit an ASAN check.
TEST_F(ScrollIntoViewTest, RemoveSequencedScrollableArea) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    .scroller {
      scroll-behavior: smooth;
      overflow: scroll;
      position: absolute;
      z-index: 0;
      border: 10px solid #cce;
    }
    #outer {
      width: 350px;
      height: 200px;
      left: 50px;
      top: 50px;
    }
    #inner {
      width: 200px;
      height: 100px;
      left: 50px;
      top: 200px;
    }
    #target {
      margin: 200px 0 20px 200px;
      width: 50px;
      height: 30px;
      background-color: #c88;
    }
    </style>
    <body>
    <div class='scroller' id='outer'>
      <div class='scroller' id='inner'>
        <div id='target'></div>
      </div>
    </div>
  )HTML");

  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById("target");
  target->scrollIntoView();

  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.

  Element* inner = GetDocument().getElementById("inner");
  Element* outer = GetDocument().getElementById("outer");
  outer->removeChild(inner);

  // Make sure that we don't try to animate the removed scroller.
  Compositor().BeginFrame(1);
}

TEST_F(ScrollIntoViewTest, SmoothUserScrollNotAbortedByProgrammaticScrolls) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='space' style='height: 1000px'></div>"
      "<div id='content' style='height: 1000px'></div>");

  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);

  // A smooth UserScroll.
  Element* content = GetDocument().getElementById("content");
  content->GetLayoutObject()->ScrollRectToVisible(
      content->BoundingBoxForScrollIntoView(),
      {ScrollAlignment::kAlignToEdgeIfNeeded, ScrollAlignment::kAlignTopAlways,
       kUserScroll, false, kScrollBehaviorSmooth, true});

  // Animating the container
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_NEAR(Window().scrollY(), 299, 1);

  // ProgrammaticScroll that could interrupt the current smooth scroll.
  Window().scrollTo(0, 0);

  // Finish scrolling the container
  Compositor().BeginFrame(1);
  // The programmatic scroll of Window shouldn't abort the user scroll.
  ASSERT_EQ(Window().scrollY(), content->OffsetTop());
}

TEST_F(ScrollIntoViewTest, LongDistanceSmoothScrollFinishedInThreeSeconds) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='space' style='height: 100000px'></div>"
      "<div id='target' style='height: 1000px'></div>");

  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);

  Element* target = GetDocument().getElementById("target");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
  options->setBlock("start");
  options->setBehavior("smooth");
  arg.SetScrollIntoViewOptions(options);
  target->scrollIntoView(arg);

  // Scrolling the window
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_NEAR(Window().scrollY(), 864, 1);

  // Finish scrolling the container
  Compositor().BeginFrame(2.8);
  ASSERT_EQ(Window().scrollY(), target->OffsetTop());
}

}  // namespace

}  // namespace blink
