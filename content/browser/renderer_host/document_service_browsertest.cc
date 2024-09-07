// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "content/browser/renderer_host/document_service_echo_impl.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/echo.test-mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class DocumentServiceBrowserTest : public ContentBrowserTest {
 public:
  DocumentServiceBrowserTest() = default;
  ~DocumentServiceBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());
  }

  WebContents* web_contents() const { return shell()->web_contents(); }

 private:
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
};

class DocumentServicePrerenderingBrowserTest
    : public DocumentServiceBrowserTest {
 public:
  DocumentServicePrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &DocumentServicePrerenderingBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~DocumentServicePrerenderingBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    DocumentServiceBrowserTest::SetUp();
  }

  test::PrerenderTestHelper* prerender_helper() { return &prerender_helper_; }

 private:
  test::PrerenderTestHelper prerender_helper_;
};

// Tests that DocumentService is not destroyed on prerendering activation.
IN_PROC_BROWSER_TEST_F(DocumentServicePrerenderingBrowserTest,
                       NotClosedInPrerenderingActivation) {
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl = embedded_test_server()->GetURL("/title1.html");
  // The test assumes documents and their DocumentServices get deleted after
  // non-activation navigations. To ensure this, disable back/forward cache.
  DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(kPrerenderingUrl);
  RenderFrameHost* prerendered_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  mojo::Remote<mojom::Echo> echo_remote;
  bool echo_deleted = false;
  new DocumentServiceEchoImpl(
      *prerendered_frame_host, echo_remote.BindNewPipeAndPassReceiver(),
      base::BindOnce([](bool* deleted) { *deleted = true; }, &echo_deleted));

  // Activate the prerendered page.
  prerender_helper()->NavigatePrimaryPage(kPrerenderingUrl);
  // DocumentService should not be destroyed.
  EXPECT_FALSE(echo_deleted);

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  // It should be destroyed on navigation.
  EXPECT_TRUE(echo_deleted);
}

class DocumentServiceBFCacheBrowserTest : public DocumentServiceBrowserTest {
 public:
  DocumentServiceBFCacheBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }
  ~DocumentServiceBFCacheBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DocumentServiceBFCacheBrowserTest, DocumentService) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHost* rfh_a =
      web_contents()->GetPrimaryMainFrame();  // current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  mojo::Remote<mojom::Echo> echo_remote;
  bool echo_deleted = false;
  new DocumentServiceEchoImpl(
      *rfh_a, echo_remote.BindNewPipeAndPassReceiver(),
      base::BindOnce([](bool* deleted) { *deleted = true; }, &echo_deleted));

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // - Page A should be in the cache.
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            RenderFrameHost::LifecycleState::kInBackForwardCache);
  EXPECT_FALSE(echo_deleted);

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(echo_deleted);

  // 4) Prevent caching and navigate to B.
  DisableBFCacheForRFHForTesting(rfh_a);
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_TRUE(echo_deleted);
}

}  // namespace content
