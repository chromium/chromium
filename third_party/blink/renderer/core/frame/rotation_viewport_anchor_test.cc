// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class RotationViewportAnchorTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().GetSettings()->SetViewportEnabled(true);
    WebView().GetSettings()->SetMainFrameResizesAreOrientationChanges(true);
  }
};

TEST_F(RotationViewportAnchorTest, SimpleAbsolutePosition) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
          width: 10000px;
          height: 10000px;
          margin: 0px;
        }

        #target {
          width: 100px;
          height: 100px;
          position: absolute;
          left: 3000px;
          top: 4000px;
        }
      </style>
      <div id="target"></div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  ScrollableArea* layout_viewport = document.View()->LayoutViewport();

  // Place the target at the top-center of the viewport. This is where the
  // rotation anchor finds the node to anchor to.
  layout_viewport->SetScrollOffset(ScrollOffset(3050 - 200, 4050),
                                   mojom::blink::ScrollType::kProgrammatic);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(600, 400));
  Compositor().BeginFrame();

  EXPECT_EQ(3050 - 200, layout_viewport->GetScrollOffset().x());
  EXPECT_EQ(4050, layout_viewport->GetScrollOffset().y());
}

TEST_F(RotationViewportAnchorTest, PositionRelativeToViewportSize) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(100, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
          width: 10000px;
          height: 10000px;
          margin: 0px;
        }

        #target {
          width: 50px;
          height: 50px;
          position: absolute;
          left: 500%;
          top: 500%;
        }
      </style>
      <div id="target"></div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  ScrollableArea* layout_viewport = document.View()->LayoutViewport();

  gfx::Point target_position(
      5 * WebView().MainFrameViewWidget()->Size().width(),
      5 * WebView().MainFrameViewWidget()->Size().height());

  // Place the target at the top-center of the viewport. This is where the
  // rotation anchor finds the node to anchor to.
  layout_viewport->SetScrollOffset(
      ScrollOffset(target_position.x() -
                       WebView().MainFrameViewWidget()->Size().width() / 2 + 25,
                   target_position.y()),
      mojom::blink::ScrollType::kProgrammatic);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(600, 100));
  Compositor().BeginFrame();

  target_position =
      gfx::Point(5 * WebView().MainFrameViewWidget()->Size().width(),
                 5 * WebView().MainFrameViewWidget()->Size().height());

  gfx::Point expected_offset(
      target_position.x() -
          WebView().MainFrameViewWidget()->Size().width() / 2 + 25,
      target_position.y());

  EXPECT_EQ(expected_offset.x(), layout_viewport->GetScrollOffset().x());
  EXPECT_EQ(expected_offset.y(), layout_viewport->GetScrollOffset().y());
}

}  // namespace

}  // namespace blink
