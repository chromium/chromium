// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/render_widget_host_connector_browsertest.h"

#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace content {

namespace {

class TestInterstitialDelegate : public InterstitialPageDelegate {
 private:
  // InterstitialPageDelegate implementation.
  std::string GetHTMLContents() override { return "<p>Interstitial</p>"; }
};

}  // namespace

RenderWidgetHostConnectorTest::RenderWidgetHostConnectorTest() {}

void RenderWidgetHostConnectorTest::SetUpOnMainThread() {
  ASSERT_TRUE(embedded_test_server()->Start());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostConnectorTest,
                       RenderViewCreatedBeforeConnector) {
  GURL main_url(embedded_test_server()->GetURL("/page_with_popup.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Navigate to the enclosed <iframe>.
  FrameTreeNode* iframe = web_contents()->GetFrameTree()->root()->child_at(0);
  GURL frame_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(iframe, frame_url);

  // Open a popup from the iframe. This creates a render widget host view
  // view before the corresponding web contents. Tests if rwhva gets connected.
  Shell* new_shell = OpenPopup(iframe, GURL(url::kAboutBlankURL), "");
  EXPECT_TRUE(new_shell);

  auto* rwhva_popup = static_cast<RenderWidgetHostViewAndroid*>(
      new_shell->web_contents()->GetRenderWidgetHostView());
  EXPECT_TRUE(connector_in_rwhva(rwhva_popup) != nullptr);
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostConnectorTest,
                       UpdateRWHVAInConnectorAtRenderViewHostSwapping) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());

  GURL http_url(embedded_test_server()->GetURL("/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), http_url));
  RenderWidgetHostViewAndroid* old_rwhva = render_widget_host_view_android();
  RenderWidgetHostConnector* connector = render_widget_host_connector();
  EXPECT_EQ(old_rwhva, connector->GetRWHVAForTesting());

  // Forces RVH change within |web_contents| by navigating to an https page.
  GURL https_url(https_server.GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), https_url));
  RenderWidgetHostViewAndroid* new_rwhva = render_widget_host_view_android();
  EXPECT_EQ(new_rwhva, connector->GetRWHVAForTesting());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostConnectorTest,
                       RestoreMainRWHVAAfterInterstitial) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  RenderWidgetHostViewAndroid* main_rwhva = render_widget_host_view_android();
  RenderWidgetHostConnector* connector = render_widget_host_connector();
  EXPECT_EQ(main_rwhva, connector->GetRWHVAForTesting());

  // Show an interstitial and then hide. Then interstitial RWHVA should be
  // updated accordingly, and the old main RWHVA reference should be restored.
  WebContentsImpl* contents = web_contents();
  GURL interstitial_url("http://interstitial");
  InterstitialPageImpl* interstitial = new InterstitialPageImpl(
      contents, static_cast<RenderWidgetHostDelegate*>(contents), true,
      interstitial_url, new TestInterstitialDelegate);
  interstitial->Show();
  WaitForInterstitialAttach(contents);
  EXPECT_NE(main_rwhva, connector->GetRWHVAForTesting());

  interstitial->Hide();
  EXPECT_EQ(main_rwhva, connector->GetRWHVAForTesting());
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostConnectorTest,
                       MainPageDestroyedWhileInBackground) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  RenderWidgetHostViewAndroid* main_rwhva = render_widget_host_view_android();
  RenderWidgetHostConnector* connector = render_widget_host_connector();
  EXPECT_EQ(main_rwhva, connector->GetRWHVAForTesting());

  // Show an interstitial and destroy the main page in the background.
  WebContentsImpl* contents = web_contents();
  GURL interstitial_url("http://interstitial");
  InterstitialPageImpl* interstitial = new InterstitialPageImpl(
      contents, static_cast<RenderWidgetHostDelegate*>(contents), true,
      interstitial_url, new TestInterstitialDelegate);
  interstitial->Show();
  WaitForInterstitialAttach(contents);
  main_rwhva->Destroy();

  // Ensure active RWHVA set to null even when the main one is destroyed first.
  interstitial->Hide();
  EXPECT_EQ(nullptr, connector->GetRWHVAForTesting());
}

}  // namespace content
