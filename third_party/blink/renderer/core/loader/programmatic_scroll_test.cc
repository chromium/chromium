// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class ProgrammaticScrollTest : public testing::Test {
 public:
  ProgrammaticScrollTest() : base_url_("http://www.test.com/") {}

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  void RegisterMockedHttpURLLoad(const String& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via the WebViewHelper instance in each test case.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString(base_url_), test::CoreTestDataPath(), WebString(file_name));
  }

  String base_url_;
};

TEST_F(ProgrammaticScrollTest, RestoreScrollPositionAndViewStateWithScale) {
  RegisterMockedHttpURLLoad("long_scroll.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url_.Utf8() + "long_scroll.html");
  web_view->MainFrameWidget()->Resize(WebSize(1000, 1000));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);

  FrameLoader& loader = web_view->MainFrameImpl()->GetFrame()->Loader();
  loader.GetDocumentLoader()->SetLoadType(WebFrameLoadType::kBackForward);

  web_view->SetPageScaleFactor(3.0f);
  web_view->MainFrameImpl()->SetScrollOffset(WebSize(0, 500));
  loader.GetDocumentLoader()->GetInitialScrollState().was_scrolled_by_user =
      false;
  loader.GetDocumentLoader()->GetHistoryItem()->SetPageScaleFactor(2);
  loader.GetDocumentLoader()->GetHistoryItem()->SetScrollOffset(
      ScrollOffset(0, 200));

  // Flip back the wasScrolledByUser flag which was set to true by
  // setPageScaleFactor because otherwise
  // FrameLoader::restoreScrollPositionAndViewState does nothing.
  loader.GetDocumentLoader()->GetInitialScrollState().was_scrolled_by_user =
      false;
  loader.RestoreScrollPositionAndViewState();

  // Expect that both scroll and scale were restored.
  EXPECT_EQ(2.0f, web_view->PageScaleFactor());
  EXPECT_EQ(200, web_view->MainFrameImpl()->GetScrollOffset().height);
}

TEST_F(ProgrammaticScrollTest, RestoreScrollPositionAndViewStateWithoutScale) {
  RegisterMockedHttpURLLoad("long_scroll.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url_.Utf8() + "long_scroll.html");
  web_view->MainFrameWidget()->Resize(WebSize(1000, 1000));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);

  FrameLoader& loader = web_view->MainFrameImpl()->GetFrame()->Loader();
  loader.GetDocumentLoader()->SetLoadType(WebFrameLoadType::kBackForward);

  web_view->SetPageScaleFactor(3.0f);
  web_view->MainFrameImpl()->SetScrollOffset(WebSize(0, 500));
  loader.GetDocumentLoader()->GetInitialScrollState().was_scrolled_by_user =
      false;
  loader.GetDocumentLoader()->GetHistoryItem()->SetPageScaleFactor(0);
  loader.GetDocumentLoader()->GetHistoryItem()->SetScrollOffset(
      ScrollOffset(0, 400));

  // FrameLoader::restoreScrollPositionAndViewState flows differently if scale
  // is zero.
  loader.RestoreScrollPositionAndViewState();

  // Expect that only the scroll position was restored.
  EXPECT_EQ(3.0f, web_view->PageScaleFactor());
  EXPECT_EQ(400, web_view->MainFrameImpl()->GetScrollOffset().height);
}

TEST_F(ProgrammaticScrollTest, SaveScrollStateClearsAnchor) {
  RegisterMockedHttpURLLoad("long_scroll.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url_.Utf8() + "long_scroll.html");
  web_view->MainFrameWidget()->Resize(WebSize(1000, 1000));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);

  FrameLoader& loader = web_view->MainFrameImpl()->GetFrame()->Loader();
  loader.GetDocumentLoader()->SetLoadType(WebFrameLoadType::kBackForward);

  web_view->MainFrameImpl()->SetScrollOffset(WebSize(0, 500));
  loader.GetDocumentLoader()->GetInitialScrollState().was_scrolled_by_user =
      true;
  loader.SaveScrollState();
  loader.SaveScrollAnchor();

  web_view->MainFrameImpl()->SetScrollOffset(WebSize(0, 0));
  loader.SaveScrollState();
  loader.GetDocumentLoader()->GetInitialScrollState().was_scrolled_by_user =
      false;

  loader.RestoreScrollPositionAndViewState();

  EXPECT_EQ(0, web_view->MainFrameImpl()->GetScrollOffset().height);
}

class ProgrammaticScrollSimTest : public SimTest {};

TEST_F(ProgrammaticScrollSimTest, NavigateToHash) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest main_resource("https://example.com/test.html#target", "text/html");
  SimSubresourceRequest css_resource("https://example.com/test.css",
                                     "text/css");

  LoadURL("https://example.com/test.html#target");

  // Finish loading the main document before the stylesheet is loaded so that
  // rendering is blocked when parsing finishes. This will delay closing the
  // document until the load event.
  main_resource.Write(
      "<!DOCTYPE html><link id=link rel=stylesheet href=test.css>");
  css_resource.Start();
  main_resource.Write(R"HTML(
    <style>
      body {
        height: 4000px;
      }
      div {
        position: absolute;
        top: 3000px;
      }
    </style>
    <div id="target">Target</h2>
  )HTML");
  main_resource.Finish();
  css_resource.Complete();
  Compositor().BeginFrame();

  // Run pending tasks to fire the load event and close the document. This
  // should cause the document to scroll to the hash.
  test::RunPendingTasks();

  ScrollableArea* layout_viewport = GetDocument().View()->LayoutViewport();
  EXPECT_EQ(3000, layout_viewport->GetScrollOffset().Height());
}

}  // namespace blink
