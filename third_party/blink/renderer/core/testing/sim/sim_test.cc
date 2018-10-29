// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

#include "content/test/test_blink_web_unit_test_support.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/layout_test_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

SimTest::SimTest()
    : web_frame_client_(*this),
      // SimCompositor overrides the LayerTreeViewDelegate to respond to
      // BeginMainFrame(), which will update and paint the WebViewImpl given to
      // SetWebView().
      web_view_client_(&compositor_) {
  Document::SetThreadedParsingEnabledForTesting(false);
  // Use the mock theme to get more predictable code paths, this also avoids
  // the OS callbacks in ScrollAnimatorMac which can schedule frames
  // unpredictably since the OS will randomly call into blink for
  // updateScrollerStyleForNewRecommendedScrollerStyle which then does
  // FrameView::scrollbarStyleChanged and will adjust the scrollbar existence
  // in the middle of a test.
  LayoutTestSupport::SetMockThemeEnabledForTest(true);
  ScrollbarTheme::SetMockScrollbarsEnabled(true);
  // Threaded animations are usually enabled for blink. However these tests use
  // synchronous compositing, which can not run threaded animations.
  bool was_threaded_animation_enabled =
      content::TestBlinkWebUnitTestSupport::SetThreadedAnimationEnabled(false);
  // If this fails, we'd be resetting IsThreadedAnimationEnabled() to the wrong
  // thing in the destructor.
  DCHECK(was_threaded_animation_enabled);
}

SimTest::~SimTest() {
  // Pump the message loop to process the load event.
  test::RunPendingTasks();

  Document::SetThreadedParsingEnabledForTesting(true);
  LayoutTestSupport::SetMockThemeEnabledForTest(false);
  ScrollbarTheme::SetMockScrollbarsEnabled(false);
  content::TestBlinkWebUnitTestSupport::SetThreadedAnimationEnabled(true);
  WebCache::Clear();
}

void SimTest::SetUp() {
  Test::SetUp();

  web_view_helper_.Initialize(&web_frame_client_, &web_view_client_);
  compositor_.SetWebView(WebView(), *web_view_client_.layer_tree_view());
  page_.SetPage(WebView().GetPage());
}

void SimTest::LoadURL(const String& url) {
  WebURLRequest request{KURL(url)};
  WebView().MainFrameImpl()->CommitNavigation(
      request, WebFrameLoadType::kStandard, WebHistoryItem(), false,
      base::UnguessableToken::Create(), nullptr /* navigation_params */,
      nullptr /* extra_data */);
}

LocalDOMWindow& SimTest::Window() {
  return *GetDocument().domWindow();
}

SimPage& SimTest::Page() {
  return page_;
}

Document& SimTest::GetDocument() {
  return *WebView().MainFrameImpl()->GetFrame()->GetDocument();
}

WebViewImpl& SimTest::WebView() {
  return *web_view_helper_.GetWebView();
}

WebLocalFrameImpl& SimTest::MainFrame() {
  return *WebView().MainFrameImpl();
}

const SimWebViewClient& SimTest::WebViewClient() const {
  return web_view_client_;
}

SimCompositor& SimTest::Compositor() {
  return compositor_;
}

void SimTest::SetEffectiveConnectionTypeForTesting(
    WebEffectiveConnectionType effective_connection_type) {
  web_frame_client_.SetEffectiveConnectionTypeForTesting(
      effective_connection_type);
}

void SimTest::AddConsoleMessage(const String& message) {
  console_messages_.push_back(message);
}

}  // namespace blink
