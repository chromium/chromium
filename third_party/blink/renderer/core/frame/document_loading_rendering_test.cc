// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class DocumentLoadingRenderingTest : public SimTest {};

TEST_F(DocumentLoadingRenderingTest,
       ShouldResumeCommitsAfterBodyParsedWithoutSheets) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  // Still in the head, should not resume commits.
  main_resource.Write("<!DOCTYPE html>");
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  main_resource.Write("<title>Test</title><style>div { color red; }</style>");
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Implicitly inserts the body. Since there's no loading stylesheets we
  // should resume commits.
  main_resource.Write("<p>Hello World</p>");
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());

  // Finish the load, should stay resumed.
  main_resource.Finish();
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
}

TEST_F(DocumentLoadingRenderingTest,
       ShouldResumeCommitsAfterBodyIfSheetsLoaded) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_resource("https://example.com/test.css",
                                     "text/css");

  LoadURL("https://example.com/test.html");

  // Still in the head, should not resume commits.
  main_resource.Write("<!DOCTYPE html><link rel=stylesheet href=test.css>");
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Sheet is streaming in, but not ready yet.
  css_resource.Start();
  css_resource.Write("a { color: red; }");
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Sheet finished, but no body yet, so don't resume.
  css_resource.Finish();
  test::RunPendingTasks();
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Body inserted and sheet is loaded so resume commits.
  main_resource.Write("<body>");
  test::RunPendingTasks();
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());

  // Finish the load, should stay resumed.
  main_resource.Finish();
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterSheetsLoaded) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_resource("https://example.com/test.css",
                                     "text/css");

  LoadURL("https://example.com/test.html");

  // Still in the head, should not resume commits.
  main_resource.Write("<!DOCTYPE html><link rel=stylesheet href=test.css>");
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Sheet is streaming in, but not ready yet.
  css_resource.Start();
  css_resource.Write("a { color: red; }");
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Body inserted, but sheet is still loading so don't resume.
  main_resource.Write("<body>");
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Sheet finished and there's a body so resume.
  css_resource.Finish();
  test::RunPendingTasks();
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());

  // Finish the load, should stay resumed.
  main_resource.Finish();
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
}

TEST_F(DocumentLoadingRenderingTest,
       ShouldResumeCommitsAfterDocumentElementWithNoSheets) {
  SimRequest main_resource("https://example.com/test.svg", "image/svg+xml");
  SimSubresourceRequest css_resource("https://example.com/test.css",
                                     "text/css");

  LoadURL("https://example.com/test.svg");

  // Sheet loading and no documentElement, so don't resume.
  main_resource.Write("<?xml-stylesheet type='text/css' href='test.css'?>");
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Sheet finishes loading, but no documentElement yet so don't resume.
  css_resource.Complete("a { color: red; }");
  test::RunPendingTasks();
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Root inserted so resume.
  main_resource.Write("<svg xmlns='http://www.w3.org/2000/svg'></svg>");
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());

  // Finish the load, should stay resumed.
  main_resource.Finish();
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterSheetsLoadForXml) {
  SimRequest main_resource("https://example.com/test.svg", "image/svg+xml");
  SimSubresourceRequest css_resource("https://example.com/test.css",
                                     "text/css");

  LoadURL("https://example.com/test.svg");

  // Not done parsing.
  main_resource.Write("<?xml-stylesheet type='text/css' href='test.css'?>");
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Sheet is streaming in, but not ready yet.
  css_resource.Start();
  css_resource.Write("a { color: red; }");
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Root inserted, but sheet is still loading so don't resume.
  main_resource.Write("<svg xmlns='http://www.w3.org/2000/svg'></svg>");
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Finish the load, but sheets still loading so don't resume.
  main_resource.Finish();
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Sheet finished, so resume commits.
  css_resource.Finish();
  test::RunPendingTasks();
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterFinishParsingXml) {
  SimRequest main_resource("https://example.com/test.svg", "image/svg+xml");

  LoadURL("https://example.com/test.svg");

  // Finish parsing, no sheets loading so resume.
  main_resource.Finish();
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeImmediatelyForImageDocuments) {
  SimRequest main_resource("https://example.com/test.png", "image/png");

  LoadURL("https://example.com/test.png");

  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  // Not really a valid image but enough for the test. ImageDocuments should
  // resume painting as soon as the first bytes arrive.
  main_resource.Write("image data");
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());

  main_resource.Finish();
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
}

TEST_F(DocumentLoadingRenderingTest, ShouldScheduleFrameAfterSheetsLoaded) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest first_css_resource("https://example.com/first.css",
                                           "text/css");
  SimSubresourceRequest second_css_resource("https://example.com/second.css",
                                            "text/css");

  LoadURL("https://example.com/test.html");

  // Load a stylesheet.
  main_resource.Write(
      "<!DOCTYPE html><link id=link rel=stylesheet href=first.css>");
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());

  first_css_resource.Start();
  first_css_resource.Write("body { color: red; }");
  main_resource.Write("<body>");
  first_css_resource.Finish();
  test::RunPendingTasks();

  // Sheet finished and there's a body so resume.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());

  main_resource.Finish();
  Compositor().BeginFrame();

  // Replace the stylesheet by changing href.
  auto* element = GetDocument().getElementById(AtomicString("link"));
  EXPECT_NE(nullptr, element);
  element->setAttribute(html_names::kHrefAttr, AtomicString("second.css"));
  EXPECT_FALSE(Compositor().NeedsBeginFrame());

  second_css_resource.Complete("body { color: red; }");
  EXPECT_TRUE(Compositor().NeedsBeginFrame());
}

TEST_F(DocumentLoadingRenderingTest,
       ShouldNotPaintIframeContentWithPendingSheets) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");
  SimSubresourceRequest css_resource("https://example.com/test.css",
                                     "text/css");

  LoadURL("https://example.com/test.html");

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <body style='background: white'>
    <iframe id=frame src=frame.html style='border: none'></iframe>
    <p style='transform: translateZ(0)'>Hello World</p>
  )HTML");

  // Main page is ready to begin painting as there's no pending sheets.
  // The frame is not yet loaded, so we only paint the main frame.
  auto frame1 = Compositor().BeginFrame();
  EXPECT_EQ(2u, frame1.DrawCount());
  EXPECT_TRUE(frame1.Contains(SimCanvas::kText, "black"));
  EXPECT_TRUE(frame1.Contains(SimCanvas::kRect, "white"));

  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>html { background: pink; color: gray; }</style>
    <link rel=stylesheet href=test.css>
    <p style='background: yellow;'>Hello World</p>
    <div style='transform: translateZ(0); background: green;'>
        <p style='background: blue;'>Hello Layer</p>
        <div style='position: relative; background: red;'>Hello World</div>
    </div>
  )HTML");

  // Trigger a layout with a blocking sheet. For example, a parent frame
  // executing a script that reads offsetTop in the child frame could do this.
  auto* child_frame = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  child_frame->contentDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kTest);

  auto frame2 = Compositor().BeginFrame();

  // The child frame still has a sheet blocking in head, so nothing is painted.
  // Still only paint the main frame.
  EXPECT_EQ(2u, frame2.DrawCount());
  EXPECT_TRUE(frame2.Contains(SimCanvas::kText, "black"));
  EXPECT_TRUE(frame2.Contains(SimCanvas::kRect, "white"));

  // Finish loading the sheets in the child frame. After it we should continue
  // parsing and paint the frame contents.
  css_resource.Complete();
  test::RunPendingTasks();

  // First frame where all frames are loaded, should paint the text in the
  // child frame.
  auto frame3 = Compositor().BeginFrame();
  EXPECT_EQ(10u, frame3.DrawCount());
  // Paint commands for the main frame.
  EXPECT_TRUE(frame3.Contains(SimCanvas::kText, "black"));
  EXPECT_TRUE(frame3.Contains(SimCanvas::kRect, "white"));
  // Paint commands for the child frame.
  EXPECT_EQ(3u, frame3.DrawCount(SimCanvas::kText, "gray"));
  EXPECT_TRUE(frame3.Contains(SimCanvas::kRect, "pink"));
  EXPECT_TRUE(frame3.Contains(SimCanvas::kRect, "yellow"));
  EXPECT_TRUE(frame3.Contains(SimCanvas::kRect, "green"));
  EXPECT_TRUE(frame3.Contains(SimCanvas::kRect, "blue"));
  EXPECT_TRUE(frame3.Contains(SimCanvas::kRect, "red"));
}

namespace {

class CheckRafCallback final : public FrameCallback {
 public:
  void Invoke(double high_res_time_ms) override { was_called_ = true; }
  bool WasCalled() const { return was_called_; }

 private:
  bool was_called_ = false;
};

}  // namespace

TEST_F(DocumentLoadingRenderingTest,
       ShouldThrottleIframeLifecycleUntilPendingSheetsLoaded) {
  SimRequest main_resource("https://example.com/main.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");
  SimSubresourceRequest css_resource("https://example.com/frame.css",
                                     "text/css");

  LoadURL("https://example.com/main.html");

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <body style='background: red'>
    <iframe id=frame src=frame.html></iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <link rel=stylesheet href=frame.css>
    <body style='background: blue'>
  )HTML");

  auto* child_frame = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));

  // Frame while the child frame still has pending sheets.
  auto* frame1_callback = MakeGarbageCollected<CheckRafCallback>();
  child_frame->contentDocument()->RequestAnimationFrame(frame1_callback);
  auto frame1 = Compositor().BeginFrame();
  EXPECT_FALSE(frame1_callback->WasCalled());
  EXPECT_TRUE(frame1.Contains(SimCanvas::kRect, "red"));
  EXPECT_FALSE(frame1.Contains(SimCanvas::kRect, "blue"));

  // Finish loading the sheets in the child frame. Should enable lifecycle
  // updates and raf callbacks.
  css_resource.Complete();
  test::RunPendingTasks();

  // Frame with all lifecycle updates enabled.
  auto* frame2_callback = MakeGarbageCollected<CheckRafCallback>();
  child_frame->contentDocument()->RequestAnimationFrame(frame2_callback);
  auto frame2 = Compositor().BeginFrame();
  EXPECT_TRUE(frame1_callback->WasCalled());
  EXPECT_TRUE(frame2_callback->WasCalled());
  EXPECT_TRUE(frame2.Contains(SimCanvas::kRect, "red"));
  EXPECT_TRUE(frame2.Contains(SimCanvas::kRect, "blue"));
}

TEST_F(DocumentLoadingRenderingTest,
       ShouldContinuePaintingWhenSheetsStartedAfterBody) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_head_resource("https://example.com/testHead.css",
                                          "text/css");
  SimSubresourceRequest css_body_resource("https://example.com/testBody.css",
                                          "text/css");

  LoadURL("https://example.com/test.html");

  // Still in the head, should not paint.
  main_resource.Write("<!DOCTYPE html><link rel=stylesheet href=testHead.css>");
  EXPECT_FALSE(GetDocument().HaveRenderBlockingResourcesLoaded());

  // Sheet is streaming in, but not ready yet.
  css_head_resource.Start();
  css_head_resource.Write("a { color: red; }");
  EXPECT_FALSE(GetDocument().HaveRenderBlockingResourcesLoaded());

  // Body inserted but sheet is still pending so don't paint.
  main_resource.Write("<body>");
  EXPECT_FALSE(GetDocument().HaveRenderBlockingResourcesLoaded());

  // Sheet finished and body inserted, ok to paint.
  css_head_resource.Finish();
  EXPECT_TRUE(GetDocument().HaveRenderBlockingResourcesLoaded());

  // In the body, should not stop painting.
  main_resource.Write("<link rel=stylesheet href=testBody.css>");
  EXPECT_TRUE(GetDocument().HaveRenderBlockingResourcesLoaded());

  // Finish loading the CSS resource (no change to painting).
  css_body_resource.Complete("a { color: red; }");
  EXPECT_TRUE(GetDocument().HaveRenderBlockingResourcesLoaded());

  // Finish the load, painting should stay enabled.
  main_resource.Finish();
  EXPECT_TRUE(GetDocument().HaveRenderBlockingResourcesLoaded());
}

}  // namespace blink
