// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/base_switches.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "components/services/storage/public/mojom/test_api.test-mojom.h"
#include "content/browser/file_system_access/file_system_chooser_test_helpers.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_document_host_user_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_mojo_binder_policy_applier_unittest.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "url/gurl.h"

namespace content {
namespace {

enum class BackForwardCacheType {
  kDisabled,
  kEnabled,
  kEnabledWithSameSite,
};

std::string ToString(const testing::TestParamInfo<BackForwardCacheType>& info) {
  switch (info.param) {
    case BackForwardCacheType::kDisabled:
      return "Disabled";
    case BackForwardCacheType::kEnabled:
      return "Enabled";
    case BackForwardCacheType::kEnabledWithSameSite:
      return "EnabledWithSameSite";
  }
}

RenderFrameHost* FindRenderFrameHost(RenderFrameHost& root, const GURL& url) {
  std::vector<RenderFrameHost*> rfhs = root.GetFramesInSubtree();
  for (auto* rfh : rfhs) {
    if (rfh->GetLastCommittedURL() == url)
      return rfh;
  }
  return nullptr;
}

// Example class which inherits the RenderDocumentHostUserData, all the data is
// associated to the lifetime of the document.
class DocumentData : public RenderDocumentHostUserData<DocumentData> {
 public:
  ~DocumentData() override = default;

  base::WeakPtr<DocumentData> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  explicit DocumentData(RenderFrameHost* render_frame_host) {}

  friend class content::RenderDocumentHostUserData<DocumentData>;

  base::WeakPtrFactory<DocumentData> weak_ptr_factory_{this};

  RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL();
};

RENDER_DOCUMENT_HOST_USER_DATA_KEY_IMPL(DocumentData)

class PrerenderBrowserTest : public ContentBrowserTest {
 public:
  using LifecycleStateImpl = RenderFrameHostImpl::LifecycleStateImpl;

  PrerenderBrowserTest() {
    prerender_helper_ =
        std::make_unique<test::PrerenderTestHelper>(base::BindRepeating(
            &PrerenderBrowserTest::web_contents, base::Unretained(this)));
  }
  ~PrerenderBrowserTest() override = default;

  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    host_resolver()->AddRule("*", "127.0.0.1");
    ssl_server_.AddDefaultHandlers(GetTestDataFilePath());
    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    prerender_helper_->SetUpOnMainThread(&ssl_server_);
    ASSERT_TRUE(ssl_server_.Start());
  }

  void TearDownOnMainThread() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    EXPECT_TRUE(ssl_server_.ShutdownAndWaitUntilComplete());
  }

  // Waits until the request count for `url` reaches `count`.
  void WaitForRequest(const GURL& url, int count) {
    prerender_helper_->WaitForRequest(url, count);
  }

  int AddPrerender(const GURL& prerendering_url) {
    return prerender_helper_->AddPrerender(prerendering_url);
  }

  void NavigatePrimaryPage(const GURL& url) {
    prerender_helper_->NavigatePrimaryPage(url);
  }

  int GetHostForUrl(const GURL& url) {
    return prerender_helper_->GetHostForUrl(url);
  }

  RenderFrameHostImpl* GetPrerenderedMainFrameHost(int host_id) {
    return static_cast<RenderFrameHostImpl*>(
        prerender_helper_->GetPrerenderedMainFrameHost(host_id));
  }

  void NavigatePrerenderedPage(int host_id, const GURL& url) {
    return prerender_helper_->NavigatePrerenderedPage(host_id, url);
  }

  bool HasHostForUrl(const GURL& url) {
    int host_id = GetHostForUrl(url);
    return host_id != RenderFrameHost::kNoFrameTreeNodeId;
  }

  void WaitForPrerenderLoadCompleted(int host_id) {
    prerender_helper_->WaitForPrerenderLoadCompletion(host_id);
  }

  void WaitForPrerenderLoadCompletion(const GURL& url) {
    prerender_helper_->WaitForPrerenderLoadCompletion(url);
  }

  GURL GetUrl(const std::string& path) {
    return ssl_server_.GetURL("a.test", path);
  }

  GURL GetCrossOriginUrl(const std::string& path) {
    return ssl_server_.GetURL("b.test", path);
  }

  int GetRequestCount(const GURL& url) {
    return prerender_helper_->GetRequestCount(url);
  }

  WebContents* web_contents() const { return shell()->web_contents(); }

  WebContentsImpl* web_contents_impl() const {
    return static_cast<WebContentsImpl*>(web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents_impl()->GetMainFrame();
  }

  void TestHostPrerenderingState(const GURL& prerender_url) {
    const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");

    // Navigate to an initial page.
    ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

    // The initial page should not be in prerendered state.
    RenderFrameHostImpl* initiator_render_frame_host = current_frame_host();
    EXPECT_EQ(initiator_render_frame_host->frame_tree()->type(),
              FrameTree::Type::kPrimary);
    EXPECT_EQ(initiator_render_frame_host->lifecycle_state(),
              LifecycleStateImpl::kActive);

    // Start a prerender.
    AddPrerender(prerender_url);

    EXPECT_TRUE(prerender_helper_->VerifyPrerenderingState(prerender_url));

    // Activate the prerendered page.
    NavigatePrimaryPage(prerender_url);
    EXPECT_EQ(web_contents()->GetURL(), prerender_url);

    // The activated page should no longer be in the prerendering state.
    RenderFrameHostImpl* navigated_render_frame_host = current_frame_host();
    // The new page shouldn't be in the prerendering state.
    std::vector<RenderFrameHost*> frames =
        navigated_render_frame_host->GetFramesInSubtree();
    for (auto* frame : frames) {
      auto* rfhi = static_cast<RenderFrameHostImpl*>(frame);
      // All the subframes should be transitioned to LifecycleStateImpl::kActive
      // state after activation.
      EXPECT_EQ(rfhi->lifecycle_state(),
                RenderFrameHostImpl::LifecycleStateImpl::kActive);
      EXPECT_FALSE(rfhi->frame_tree()->is_prerendering());
    }
  }

  test::PrerenderTestHelper* prerender_helper() {
    return prerender_helper_.get();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Useful for testing CSP:prefetch-src
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

 private:
  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  std::unique_ptr<test::PrerenderTestHelper> prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, LinkRelPrerender) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetURL(), kInitialUrl);

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A prerender host for the URL should be registered.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));

  // Activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetURL(), kPrerenderingUrl);

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, LinkRelPrerender_Multiple) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl1 = GetUrl("/empty.html?1");
  const GURL kPrerenderingUrl2 = GetUrl("/empty.html?2");

  // TODO(https://crbug.com/1186893): PrerenderHost is not deleted when the
  // page enters BackForwardCache, though it should be. While this functionality
  // is not implemented, disable BackForwardCache for testing and wait for the
  // old RenderFrameHost to be deleted after we navigate away from it.
  DisableBackForwardCacheForTesting(
      web_contents(), BackForwardCacheImpl::TEST_ASSUMES_NO_CACHING);

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetURL(), kInitialUrl);

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl1` and
  // `kPrerenderingUrl2`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl1), 0);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl2), 0);
  AddPrerender(kPrerenderingUrl1);
  AddPrerender(kPrerenderingUrl2);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl1), 1);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl2), 1);

  // Prerender hosts for `kPrerenderingUrl1` and `kPrerenderingUrl2` should be
  // registered.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl1));
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl2));

  RenderFrameDeletedObserver delete_observer_rfh(
      web_contents()->GetMainFrame());

  // Activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl2);
  EXPECT_EQ(web_contents()->GetURL(), kPrerenderingUrl2);

  // Other PrerenderHost instances are deleted with the RFH.
  delete_observer_rfh.WaitUntilDeleted();

  // The prerender hosts should be consumed or destroyed for activation.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl1));
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl2));

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl1), 1);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl2), 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, LinkRelPrerender_Duplicate) {
  const GURL kInitialUrl = GetUrl("/prerender/duplicate_prerenders.html");
  const GURL kPrerenderingUrl1 = GetUrl("/empty.html?1");
  const GURL kPrerenderingUrl2 = GetUrl("/empty.html?2");

  // TODO(https://crbug.com/1186893): PrerenderHost is not deleted when the
  // page enters BackForwardCache, though it should be. While this functionality
  // is not implemented, disable BackForwardCache for testing and wait for the
  // old RenderFrameHost to be deleted after we navigate away from it.
  DisableBackForwardCacheForTesting(
      web_contents(), BackForwardCacheImpl::TEST_ASSUMES_NO_CACHING);

  // Navigate to a page that initiates prerendering for `kPrerenderingUrl1`
  // twice. The second prerendering request should be ignored.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Wait until the completion of prerendering.
  WaitForPrerenderLoadCompletion(kPrerenderingUrl1);
  WaitForPrerenderLoadCompletion(kPrerenderingUrl2);

  // Requests should be issued once per prerendering URL.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl1), 1);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl2), 1);

  // Prerender hosts for `kPrerenderingUrl1` and `kPrerenderingUrl2` should be
  // registered.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl1));
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl2));

  RenderFrameDeletedObserver delete_observer_rfh(
      web_contents()->GetMainFrame());

  // Activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl1);
  EXPECT_EQ(web_contents()->GetURL(), kPrerenderingUrl1);

  // Other PrerenderHost instances are deleted with the RFH.
  delete_observer_rfh.WaitUntilDeleted();

  // The prerender hosts should be consumed or destroyed for activation.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl1));
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl2));

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl1), 1);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl2), 1);
}

// Regression test for https://crbug.com/1194865.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CloseOnPrerendering) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetURL(), kInitialUrl);

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A prerender host for the URL should be registered.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));

  // Should not crash.
  shell()->Close();
}

// Tests that non-http(s) schemes are disallowed for prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, HttpToBlobUrl) {
  base::HistogramTester histogram_tester;

  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Generate a Blob page and obtain a URL for the Blob page.
  const char kCreateBlobUrlScript[] =
      "URL.createObjectURL("
      "new Blob([\"<h1>hello blob</h1>\"], { type: 'text/html' }));";
  const std::string blob_url =
      EvalJs(web_contents(), kCreateBlobUrlScript).ExtractString();
  const GURL blob_gurl(blob_url);

  // Add <link rel=prerender> that will prerender the Blob page.
  test::PrerenderHostRegistryObserver observer(*web_contents_impl());
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("add_prerender($1)", blob_url)));
  observer.WaitForTrigger(blob_gurl);

  // A prerender host for the URL should not be registered.
  EXPECT_FALSE(HasHostForUrl(blob_gurl));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kInvalidSchemeNavigation, 1);
}

// Tests that non-http(s) schemes are disallowed for prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, BlobUrlToBlobUrl) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  // This test can not use `about:blank` as the initial url because created
  // blobs inside the page are populated as opaque and blob to blob prerendering
  // are alerted as cross-origin prerendering.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Navigate to a dynamically constructed Blob page.
  const char kCreateBlobUrlScript[] =
      "URL.createObjectURL(new Blob([\"<script>"
      "function add_prerender(url) {"
      "  const link = document.createElement('link');"
      "  link.rel = 'prerender';"
      "  link.href= url;"
      "  document.head.appendChild(link);"
      "}"
      "</script>\"], { type: 'text/html' }));";
  const std::string initial_blob_url =
      EvalJs(web_contents(), kCreateBlobUrlScript).ExtractString();
  ASSERT_TRUE(NavigateToURL(shell(), GURL(initial_blob_url)));

  // Create another Blob URL inside the Blob page.
  const std::string blob_url =
      EvalJs(web_contents(), kCreateBlobUrlScript).ExtractString();
  const GURL blob_gurl(blob_url);

  // Add <link rel=prerender> that will prerender the Blob page.
  test::PrerenderHostRegistryObserver observer(*web_contents_impl());
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("add_prerender($1)", blob_url)));
  observer.WaitForTrigger(blob_gurl);

  // A prerender host for the URL should not be registered.
  EXPECT_FALSE(HasHostForUrl(blob_gurl));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kInvalidSchemeNavigation, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SameOriginRedirection) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes same-origin redirection.
  const GURL kRedirectedUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl =
      GetUrl("/server-redirect?" + kRedirectedUrl.spec());
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);

  // The prerender host should be registered for the initial request URL, not
  // the redirected URL.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CrossOriginRedirection) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin redirection. The
  // cross-origin redirection should fail prerendering.
  const GURL kRedirectedUrl = GetCrossOriginUrl("/empty.html");
  const GURL kPrerenderingUrl =
      GetUrl("/server-redirect?" + kRedirectedUrl.spec());
  test::PrerenderHostRegistryObserver registry_observer(*web_contents_impl());
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace("add_prerender($1)", kPrerenderingUrl)));
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  int host_id = GetHostForUrl(kPrerenderingUrl);
  test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 0);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_FALSE(HasHostForUrl(kRedirectedUrl));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kCrossOriginRedirect, 1);
}

// Makes sure that activation on navigation for an iframes doesn't happen.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, Activation_iFrame) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  const int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);

  // Attempt to activate the prerendered page for an iframe. This should fail
  // and fallback to network request.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ("LOADED", EvalJs(web_contents(),
                             JsReplace("add_iframe($1)", kPrerenderingUrl)));
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);

  // Activation shouldn't happen, so the prerender host should not be consumed.
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl), host_id);
}

// Makes sure that cross-origin subframe navigations are deferred during
// prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DeferCrossOriginSubframeNavigation) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html?initial");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/add_prerender.html?prerender");
  const int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);

  const GURL kSameOriginSubframeUrl =
      GetUrl("/prerender/add_prerender.html?same_origin_iframe");
  const GURL kCrossOriginSubframeUrl =
      GetCrossOriginUrl("/prerender/add_prerender.html?cross_origin_iframe");

  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ASSERT_EQ(GetRequestCount(kSameOriginSubframeUrl), 0);
  ASSERT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 0);

  // Add a cross-origin iframe to the prerendering page.
  RenderFrameHost* prerender_frame_host = GetPrerenderedMainFrameHost(host_id);
  // Use ExecuteScriptAsync instead of EvalJs as inserted cross-origin iframe
  // navigation would be deferred and script execution does not finish until
  // the activation.
  ExecuteScriptAsync(prerender_frame_host, JsReplace("add_iframe_async($1)",
                                                     kCrossOriginSubframeUrl));
  base::RunLoop().RunUntilIdle();

  // Add a same-origin iframe to the prerendering page.
  ASSERT_EQ("LOADED",
            EvalJs(prerender_frame_host,
                   JsReplace("add_iframe($1)", kSameOriginSubframeUrl)));
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ASSERT_EQ(GetRequestCount(kSameOriginSubframeUrl), 1);
  ASSERT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 0);

  // Activate.
  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(web_contents()->GetURL(), kPrerenderingUrl);
  ASSERT_EQ("LOADED",
            EvalJs(prerender_frame_host, JsReplace("wait_iframe_async($1)",
                                                   kCrossOriginSubframeUrl)));
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ASSERT_EQ(GetRequestCount(kSameOriginSubframeUrl), 1);
  EXPECT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 1);

  const char kInitialDocumentPrerenderingScript[] =
      "initial_document_prerendering";
  const char kCurrentDocumentPrerenderingScript[] = "document.prerendering";
  const char kOnprerenderingchangeObservedScript[] =
      "onprerenderingchange_observed";
  EXPECT_EQ(true,
            EvalJs(prerender_frame_host, kInitialDocumentPrerenderingScript));
  EXPECT_EQ(false,
            EvalJs(prerender_frame_host, kCurrentDocumentPrerenderingScript));
  EXPECT_EQ(true,
            EvalJs(prerender_frame_host, kOnprerenderingchangeObservedScript));

  RenderFrameHost* same_origin_render_frame_host =
      FindRenderFrameHost(*prerender_frame_host, kSameOriginSubframeUrl);
  DCHECK(same_origin_render_frame_host);
  EXPECT_EQ(true, EvalJs(same_origin_render_frame_host,
                         kInitialDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(same_origin_render_frame_host,
                          kCurrentDocumentPrerenderingScript));
  EXPECT_EQ(true, EvalJs(same_origin_render_frame_host,
                         kOnprerenderingchangeObservedScript));

  RenderFrameHost* cross_origin_render_frame_host =
      FindRenderFrameHost(*prerender_frame_host, kCrossOriginSubframeUrl);
  DCHECK(cross_origin_render_frame_host);
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kInitialDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kCurrentDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kOnprerenderingchangeObservedScript));
}

// Makes sure that subframe navigations are deferred if cross-origin redirects
// are observed in a prerendering page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DeferCrossOriginRedirectsOnSubframeNavigation) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html?initial");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/add_prerender.html?prerender");
  const int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);

  const GURL kCrossOriginSubframeUrl =
      GetCrossOriginUrl("/prerender/add_prerender.html?cross_origin_iframe");
  const GURL kServerRedirectSubframeUrl =
      GetUrl("/server-redirect?" + kCrossOriginSubframeUrl.spec());

  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ASSERT_EQ(GetRequestCount(kServerRedirectSubframeUrl), 0);
  ASSERT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 0);

  // Add an iframe pointing to a server redirect page to the prerendering page.
  RenderFrameHost* prerender_frame_host = GetPrerenderedMainFrameHost(host_id);
  // Use ExecuteScriptAsync instead of EvalJs as inserted iframe redirect
  // navigation would be deferred and script execution does not finish until
  // the activation.
  ExecuteScriptAsync(
      prerender_frame_host,
      JsReplace("add_iframe_async($1)", kServerRedirectSubframeUrl));
  WaitForRequest(kServerRedirectSubframeUrl, 1);
  ASSERT_EQ(GetRequestCount(kServerRedirectSubframeUrl), 1);
  ASSERT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 0);

  // Activate.
  NavigatePrimaryPage(kPrerenderingUrl);
  ASSERT_EQ(web_contents()->GetURL(), kPrerenderingUrl);
  ASSERT_EQ("LOADED", EvalJs(prerender_frame_host,
                             JsReplace("wait_iframe_async($1)",
                                       kServerRedirectSubframeUrl)));
  EXPECT_EQ(GetRequestCount(kServerRedirectSubframeUrl), 1);
  EXPECT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 1);

  const char kInitialDocumentPrerenderingScript[] =
      "initial_document_prerendering";
  const char kCurrentDocumentPrerenderingScript[] = "document.prerendering";
  const char kOnprerenderingchangeObservedScript[] =
      "onprerenderingchange_observed";
  EXPECT_EQ(true,
            EvalJs(prerender_frame_host, kInitialDocumentPrerenderingScript));
  EXPECT_EQ(false,
            EvalJs(prerender_frame_host, kCurrentDocumentPrerenderingScript));
  EXPECT_EQ(true,
            EvalJs(prerender_frame_host, kOnprerenderingchangeObservedScript));

  RenderFrameHost* cross_origin_render_frame_host =
      FindRenderFrameHost(*prerender_frame_host, kCrossOriginSubframeUrl);
  DCHECK(cross_origin_render_frame_host);
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kInitialDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kCurrentDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kOnprerenderingchangeObservedScript));
}

// Test main frame navigation in prerendering page cancels the prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MainFrameNavigationCancelsPrerendering) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  const GURL kHungUrl = GetUrl("/hung");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const int host_id = AddPrerender(kPrerenderingUrl);

  // Start a navigation in the prerender frame tree that will cancel the
  // initiator's prerendering.
  test::PrerenderHostObserver observer(*web_contents_impl(), host_id);

  NavigatePrerenderedPage(host_id, kHungUrl);

  observer.WaitForDestroyed();
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kMainFrameNavigation, 1);
}

// Regression test for https://crbug.com/1198051
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MainFrameSamePageNavigation) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl =
      GetUrl("/navigation_controller/hash_anchor_with_iframe.html");
  const GURL kAnchorUrl =
      GetUrl("/navigation_controller/hash_anchor_with_iframe.html#Test");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
  WaitForPrerenderLoadCompleted(host_id);

  // Do a same document navigation
  NavigatePrerenderedPage(host_id, kAnchorUrl);
  WaitForPrerenderLoadCompleted(host_id);

  // Activate
  NavigatePrimaryPage(kPrerenderingUrl);
  // Make sure the render is not dead by doing a same page navigation.
  NavigatePrimaryPage(kAnchorUrl);

  // Make sure we did activate the page and issued no network requests
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
}

// Makes sure that activation on navigation for a pop-up window doesn't happen.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, Activation_PopUpWindow) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  const int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);

  // Attempt to activate the prerendered page for a pop-up window. This should
  // fail and fallback to network request.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ("LOADED", EvalJs(web_contents(),
                             JsReplace("open_window($1)", kPrerenderingUrl)));
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);

  // Activation shouldn't happen, so the prerender host should not be consumed.
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl), host_id);
}

// Makes sure that activation on navigation for a page that has a pop-up window
// doesn't happen.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, Activation_PageWithPopUpWindow) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html?next");
  AddPrerender(kPrerenderingUrl);
  ASSERT_TRUE(HasHostForUrl(kPrerenderingUrl));

  // Open a pop-up window.
  const GURL kWindowUrl = GetUrl("/empty.html?window");
  EXPECT_EQ("LOADED",
            EvalJs(web_contents(), JsReplace("open_window($1)", kWindowUrl)));

  // Attempt to activate the prerendered page for the top-level frame. This
  // should fail and fallback to network request because the pop-up window
  // exists.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetURL(), kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);

  // Activation shouldn't happen, so the prerender host should not be consumed.
  // However, we don't check the existence of the prerender host here unlike
  // other activation tests because navigating the frame that triggered
  // prerendering abandons the prerendered page regardless of activation.
}

// Tests that all RenderFrameHostImpls in the prerendering page know the
// prerendering state.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderIframe) {
  TestHostPrerenderingState(GetUrl("/page_with_iframe.html"));
}

// Blank <iframe> is a special case. Tests that the blank iframe knows the
// prerendering state as well.
// TODO(https://crbug.com/1185965): This test is disabled for flakiness.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DISABLED_PrerenderBlankIframe) {
  TestHostPrerenderingState(GetUrl("/page_with_blank_iframe.html"));
}

// Tests that an inner WebContents can be attached in a prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ActivatePageWithInnerContents) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_blank_iframe.html");
  const GURL kInnerContentsUrl = GetUrl("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  const int host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);
  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(
          prerendered_render_frame_host->child_at(0)->current_frame_host()));
  ASSERT_TRUE(NavigateToURLFromRenderer(inner_contents, kInnerContentsUrl));

  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetURL(), kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kInnerContentsUrl), 1);
}

// Ensures that if we attempt to open a URL while prerendering with a window
// disposition other than CURRENT_TAB, we fail.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, SuppressOpenURL) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  const GURL kSecondUrl = GetUrl("/title1.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetURL(), kInitialUrl);

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  const int host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  auto* web_contents =
      WebContents::FromRenderFrameHost(prerendered_render_frame_host);
  OpenURLParams params(kSecondUrl, Referrer(),
                       prerendered_render_frame_host->GetFrameTreeNodeId(),
                       WindowOpenDisposition::NEW_WINDOW,
                       ui::PAGE_TRANSITION_LINK, true);
  params.initiator_origin =
      prerendered_render_frame_host->GetLastCommittedOrigin();
  params.source_render_process_id =
      prerendered_render_frame_host->GetProcess()->GetID();
  params.source_render_frame_id = prerendered_render_frame_host->GetRoutingID();
  auto* new_web_contents = web_contents->OpenURL(params);
  EXPECT_EQ(nullptr, new_web_contents);
}

// Tests that |RenderFrameHost::ForEachRenderFrameHost| and
// |WebContents::ForEachRenderFrameHost| behave correctly when prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ForEachRenderFrameHost) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  // All frames are same-origin due to prerendering restrictions for
  // cross-origin.
  const GURL kPrerenderingUrl =
      GetUrl("/cross_site_iframe_factory.html?a.test(a.test(a.test),a.test)");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  RenderFrameHostImpl* initiator_render_frame_host = current_frame_host();

  const int host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);
  RenderFrameHostImpl* rfh_sub_1 =
      prerendered_render_frame_host->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_sub_1_1 =
      rfh_sub_1->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_sub_2 =
      prerendered_render_frame_host->child_at(1)->current_frame_host();

  EXPECT_THAT(CollectAllRenderFrameHosts(prerendered_render_frame_host),
              testing::ElementsAre(prerendered_render_frame_host, rfh_sub_1,
                                   rfh_sub_2, rfh_sub_1_1));

  // When iterating over all RenderFrameHosts in a WebContents, we should see
  // the RFHs of both the primary page and the prerendered page.
  EXPECT_THAT(CollectAllRenderFrameHosts(web_contents_impl()),
              testing::UnorderedElementsAre(initiator_render_frame_host,
                                            prerendered_render_frame_host,
                                            rfh_sub_1, rfh_sub_2, rfh_sub_1_1));
}

class MojoCapabilityControlTestContentBrowserClient
    : public TestContentBrowserClient,
      mojom::TestInterfaceForDefer,
      mojom::TestInterfaceForGrant,
      mojom::TestInterfaceForCancel,
      mojom::TestInterfaceForUnexpected {
 public:
  void RegisterBrowserInterfaceBindersForFrame(
      RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<RenderFrameHost*>* map) override {
    map->Add<mojom::TestInterfaceForDefer>(base::BindRepeating(
        &MojoCapabilityControlTestContentBrowserClient::BindDeferInterface,
        base::Unretained(this)));
    map->Add<mojom::TestInterfaceForGrant>(base::BindRepeating(
        &MojoCapabilityControlTestContentBrowserClient::BindGrantInterface,
        base::Unretained(this)));
    map->Add<mojom::TestInterfaceForCancel>(base::BindRepeating(
        &MojoCapabilityControlTestContentBrowserClient::BindCancelInterface,
        base::Unretained(this)));
    map->Add<mojom::TestInterfaceForUnexpected>(base::BindRepeating(
        &MojoCapabilityControlTestContentBrowserClient::BindUnexpectedInterface,
        base::Unretained(this)));
  }

  void RegisterMojoBinderPoliciesForSameOriginPrerendering(
      MojoBinderPolicyMap& policy_map) override {
    policy_map.SetPolicy<mojom::TestInterfaceForGrant>(
        MojoBinderPolicy::kGrant);
    policy_map.SetPolicy<mojom::TestInterfaceForCancel>(
        MojoBinderPolicy::kCancel);
    policy_map.SetPolicy<mojom::TestInterfaceForUnexpected>(
        MojoBinderPolicy::kUnexpected);
  }

  void BindDeferInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<content::mojom::TestInterfaceForDefer> receiver) {
    defer_receiver_set_.Add(this, std::move(receiver));
  }

  void BindGrantInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::TestInterfaceForGrant> receiver) {
    grant_receiver_set_.Add(this, std::move(receiver));
  }

  void BindCancelInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::TestInterfaceForCancel> receiver) {
    cancel_receiver_.Bind(std::move(receiver));
  }

  void BindUnexpectedInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::TestInterfaceForUnexpected> receiver) {
    unexpected_receiver_.Bind(std::move(receiver));
  }

  // mojom::TestInterfaceForDefer implementation.
  void Ping(PingCallback callback) override { std::move(callback).Run(); }

  size_t GetDeferReceiverSetSize() { return defer_receiver_set_.size(); }

  size_t GetGrantReceiverSetSize() { return grant_receiver_set_.size(); }

 private:
  mojo::ReceiverSet<mojom::TestInterfaceForDefer> defer_receiver_set_;
  mojo::ReceiverSet<mojom::TestInterfaceForGrant> grant_receiver_set_;
  mojo::Receiver<mojom::TestInterfaceForCancel> cancel_receiver_{this};
  mojo::Receiver<mojom::TestInterfaceForUnexpected> unexpected_receiver_{this};
};

// Tests that binding requests are handled according to MojoBinderPolicyMap
// during prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MojoCapabilityControl) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);

  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetURL(), kInitialUrl);

  // Start a prerender.
  const int host_id = AddPrerender(kPrerenderingUrl);
  RenderFrameHost* prerendered_render_frame_host =
      GetPrerenderedMainFrameHost(host_id);
  std::vector<RenderFrameHost*> frames =
      prerendered_render_frame_host->GetFramesInSubtree();

  // A barrier closure to wait until a deferred interface is granted on all
  // frames.
  base::RunLoop run_loop;
  auto barrier_closure =
      base::BarrierClosure(frames.size(), run_loop.QuitClosure());

  mojo::RemoteSet<mojom::TestInterfaceForDefer> defer_remote_set;
  mojo::RemoteSet<mojom::TestInterfaceForGrant> grant_remote_set;
  for (auto* frame : frames) {
    auto* rfhi = static_cast<RenderFrameHostImpl*>(frame);
    EXPECT_TRUE(rfhi->frame_tree()->is_prerendering());
    EXPECT_EQ(rfhi->lifecycle_state(), LifecycleStateImpl::kPrerendering);
    EXPECT_EQ(rfhi->GetLifecycleState(),
              RenderFrameHost::LifecycleState::kPrerendering);

    mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
        rfhi->browser_interface_broker_receiver_for_testing();
    blink::mojom::BrowserInterfaceBroker* prerender_broker =
        bib.internal_state()->impl();

    // Try to bind a kDefer interface.
    mojo::Remote<mojom::TestInterfaceForDefer> prerender_defer_remote;
    prerender_broker->GetInterface(
        prerender_defer_remote.BindNewPipeAndPassReceiver());
    // The barrier closure will be called after the deferred interface is
    // granted.
    prerender_defer_remote->Ping(barrier_closure);
    defer_remote_set.Add(std::move(prerender_defer_remote));

    // Try to bind a kGrant interface.
    mojo::Remote<mojom::TestInterfaceForGrant> prerender_grant_remote;
    prerender_broker->GetInterface(
        prerender_grant_remote.BindNewPipeAndPassReceiver());
    grant_remote_set.Add(std::move(prerender_grant_remote));
  }
  // Verify that BrowserInterfaceBrokerImpl defers running binders whose
  // policies are kDefer until the prerendered page is activated.
  EXPECT_EQ(test_browser_client.GetDeferReceiverSetSize(), 0U);
  // Verify that BrowserInterfaceBrokerImpl executes kGrant binders immediately.
  EXPECT_EQ(test_browser_client.GetGrantReceiverSetSize(), frames.size());

  // Activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetURL(), kPrerenderingUrl);

  // Wait until the deferred interface is granted on all frames.
  run_loop.Run();
  EXPECT_EQ(test_browser_client.GetDeferReceiverSetSize(), frames.size());

  SetBrowserClientForTesting(old_browser_client);
}

// Tests that mojo capability control will cancel prerendering if the main frame
// receives a request for a kCancel interface.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MojoCapabilityControl_CancelMainFrame) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);
  base::HistogramTester histogram_tester;

  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetURL(), kInitialUrl);

  // Start a prerender.
  const int host_id = AddPrerender(kPrerenderingUrl);
  auto* prerendered_render_frame_host = GetPrerenderedMainFrameHost(host_id);
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
      prerendered_render_frame_host
          ->browser_interface_broker_receiver_for_testing();
  blink::mojom::BrowserInterfaceBroker* prerender_broker =
      bib.internal_state()->impl();

  // Send a kCancel request to cancel prerendering.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));
  mojo::Remote<mojom::TestInterfaceForCancel> remote;
  prerender_broker->GetInterface(remote.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kMojoBinderPolicy, 1);
  // `TestInterfaceForCancel` doesn't have a enum value because it is not used
  // in production, so histogram_tester should log
  // PrerenderCancelledInterface::kUnkown here.
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface",
      PrerenderCancelledInterface::kUnknown, 1);
  SetBrowserClientForTesting(old_browser_client);
}

// Tests that mojo capability control will cancel prerendering if child frames
// receive a request for a kCancel interface.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MojoCapabilityControl_CancelIframe) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);
  base::HistogramTester histogram_tester;

  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetURL(), kInitialUrl);

  // Start a prerender.
  const int host_id = AddPrerender(kPrerenderingUrl);
  auto* main_render_frame_host = GetPrerenderedMainFrameHost(host_id);
  ASSERT_GE(main_render_frame_host->child_count(), 1U);
  RenderFrameHostImpl* child_render_frame_host =
      main_render_frame_host->child_at(0U)->current_frame_host();
  EXPECT_NE(main_render_frame_host->GetLastCommittedURL(),
            child_render_frame_host->GetLastCommittedURL());
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
      child_render_frame_host->browser_interface_broker_receiver_for_testing();
  blink::mojom::BrowserInterfaceBroker* prerender_broker =
      bib.internal_state()->impl();

  // Send a kCancel request to cancel prerendering.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));

  mojo::Remote<mojom::TestInterfaceForCancel> remote;
  prerender_broker->GetInterface(remote.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  SetBrowserClientForTesting(old_browser_client);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kMojoBinderPolicy, 1);
  // `TestInterfaceForCancel` doesn't have a enum value because it is not used
  // in production, so histogram_tester should log
  // PrerenderCancelledInterface::kUnkown here.
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface",
      PrerenderCancelledInterface::kUnknown, 1);
}

// Tests that mojo capability control will crash the prerender if the browser
// process receives a kUnexpected interface.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MojoCapabilityControl_HandleUnexpected) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);

  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetURL(), kInitialUrl);

  // Set up the error handler for bad mojo messages.
  std::string bad_message_error;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        EXPECT_FALSE(error.empty());
        EXPECT_TRUE(bad_message_error.empty());
        bad_message_error = error;
      }));

  // Start a prerender.
  const int host_id = AddPrerender(kPrerenderingUrl);
  auto* main_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // Rebind a receiver for testing.
  // mojo::ReportBadMessage must be called within the stack frame derived from
  // mojo IPC calls, so this browser test should call the
  // remote<blink::mojom::BrowserInterfaceBroker>::GetInterface() to test
  // unexpected interfaces. But its remote end is in renderer processes and
  // inaccessible, so the test code has to create another BrowserInterfaceBroker
  // pipe and rebind the receiver end so as to send the request from the remote.
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
      main_render_frame_host->browser_interface_broker_receiver_for_testing();
  auto broker_receiver_of_previous_document = bib.Unbind();
  ASSERT_TRUE(broker_receiver_of_previous_document);
  mojo::Remote<blink::mojom::BrowserInterfaceBroker> remote_broker;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> fake_receiver =
      remote_broker.BindNewPipeAndPassReceiver();
  main_render_frame_host->BindBrowserInterfaceBrokerReceiver(
      std::move(fake_receiver));

  // Send a kUnexpected request.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));
  mojo::Remote<mojom::TestInterfaceForUnexpected> remote;
  remote_broker->GetInterface(remote.BindNewPipeAndPassReceiver());
  remote_broker.FlushForTesting();
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  EXPECT_EQ(bad_message_error,
            "MBPA_BAD_INTERFACE: content.mojom.TestInterfaceForUnexpected");

  SetBrowserClientForTesting(old_browser_client);
}

// TODO(https://crbug.com/1132746): Test canceling prerendering when its
// initiator is no longer interested in prerending this page.

// TODO(https://crbug.com/1132746): Test prerendering for 404 page, auth error,
// cross origin, etc.

// Tests for feature restrictions in prerendered pages =========================

// Tests that window.open() in a prerendering page fails.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, FeatureRestriction_WindowOpen) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/add_prerender.html?prerendering");
  const int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
  auto* prerender_frame = GetPrerenderedMainFrameHost(host_id);

  // Attempt to open a window in the prerendered page. This should fail.
  const GURL kWindowOpenUrl = GetUrl("/empty.html");
  EXPECT_EQ("FAILED", EvalJs(prerender_frame,
                             JsReplace("open_window($1)", kWindowOpenUrl)));
  EXPECT_EQ(GetRequestCount(kWindowOpenUrl), 0);

  // Opening a window shouldn't cancel prerendering.
  EXPECT_EQ(GetHostForUrl(kPrerenderingUrl), host_id);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, RenderFrameHostLifecycleState) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/prerender/add_prerender.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_EQ(current_frame_host()->lifecycle_state(),
            LifecycleStateImpl::kActive);

  // Start a prerender.
  const int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);

  // Open an iframe in the prerendered page.
  RenderFrameHostImpl* rfh_a = GetPrerenderedMainFrameHost(host_id);
  EXPECT_EQ("LOADED",
            EvalJs(rfh_a, JsReplace("add_iframe($1)", GetUrl("/empty.html"))));
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  // Both rfh_a and rfh_b lifecycle state's should be kPrerendering.
  EXPECT_EQ(LifecycleStateImpl::kPrerendering, rfh_a->lifecycle_state());
  EXPECT_EQ(LifecycleStateImpl::kPrerendering, rfh_b->lifecycle_state());

  // Activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl);

  // Both rfh_a and rfh_b lifecycle state's should be kActive after activation.
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_a->lifecycle_state());
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_b->lifecycle_state());
}

// Tests that prerendering is gated behind CSP:prefetch-src
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CSPPrefetchSrc) {
  base::HistogramTester histogram_tester;

  GURL initial_url = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Add CSP:prefetch-src */empty.html
  EXPECT_TRUE(ExecJs(current_frame_host(), R"(
    const meta = document.createElement('meta');
    meta.httpEquiv = "Content-Security-Policy";
    meta.content = "prefetch-src https://a.test:*/empty.html";
    document.getElementsByTagName('head')[0].appendChild(meta);
  )"));

  const char* kConsolePattern =
      "Refused to prefetch content from "
      "'https://a.test:*/*.html' because it violates the "
      "following Content Security Policy directive: \"prefetch-src "
      "https://a.test:*/empty.html\"*";

  // Check what happens when a prerendering is blocked:
  {
    GURL disallowed_url = GetUrl("/title1.html");
    WebContentsConsoleObserver console_observer(web_contents_impl());
    console_observer.SetPattern(kConsolePattern);

    // Prerender will fail. Then FindHostByUrlForTesting() should return null.
    test::PrerenderHostRegistryObserver observer(*web_contents_impl());
    EXPECT_TRUE(
        ExecJs(web_contents(), JsReplace("add_prerender($1)", disallowed_url)));
    observer.WaitForTrigger(disallowed_url);
    EXPECT_FALSE(HasHostForUrl(disallowed_url));
    console_observer.Wait();
    EXPECT_EQ(1u, console_observer.messages().size());
    EXPECT_EQ(GetRequestCount(disallowed_url), 0);
    histogram_tester.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus",
        PrerenderHost::FinalStatus::kNavigationRequestBlockedByCsp, 1);
  }

  // Check what happens when prerendering isn't blocked.
  {
    WebContentsConsoleObserver console_observer(web_contents_impl());
    console_observer.SetPattern(kConsolePattern);
    GURL kAllowedUrl = GetUrl("/empty.html");
    AddPrerender(kAllowedUrl);
    EXPECT_EQ(0u, console_observer.messages().size());
    EXPECT_EQ(GetRequestCount(kAllowedUrl), 1);
  }
}

// Tests that prerendering is gated behind CSP:default-src
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CSPDefaultSrc) {
  base::HistogramTester histogram_tester;

  GURL initial_url = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Add CSP:prefetch-src */empty.html
  EXPECT_TRUE(ExecJs(current_frame_host(), R"(
    const meta = document.createElement('meta');
    meta.httpEquiv = "Content-Security-Policy";
    meta.content =
        "default-src https://a.test:*/empty.html; script-src 'unsafe-eval'";
    document.getElementsByTagName('head')[0].appendChild(meta);
  )"));

  const char* kConsolePattern =
      "Refused to prefetch content from "
      "'https://a.test:*/*.html' because it violates the "
      "following Content Security Policy directive: \"default-src "
      "https://a.test:*/empty.html\"*";

  // Check what happens when a prerendering is blocked:
  {
    GURL disallowed_url = GetUrl("/title1.html");
    WebContentsConsoleObserver console_observer(web_contents_impl());
    console_observer.SetPattern(kConsolePattern);
    test::PrerenderHostRegistryObserver observer(*web_contents_impl());
    EXPECT_TRUE(
        ExecJs(web_contents(), JsReplace("add_prerender($1)", disallowed_url)));
    observer.WaitForTrigger(disallowed_url);
    EXPECT_FALSE(HasHostForUrl(disallowed_url));
    console_observer.Wait();
    EXPECT_EQ(1u, console_observer.messages().size());
    EXPECT_EQ(GetRequestCount(disallowed_url), 0);
    histogram_tester.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus",
        PrerenderHost::FinalStatus::kNavigationRequestBlockedByCsp, 1);
  }

  // Check what happens when prerendering isn't blocked.
  {
    WebContentsConsoleObserver console_observer(web_contents_impl());
    console_observer.SetPattern(kConsolePattern);
    GURL kAllowedUrl = GetUrl("/empty.html");
    AddPrerender(kAllowedUrl);
    EXPECT_EQ(0u, console_observer.messages().size());
    EXPECT_EQ(GetRequestCount(kAllowedUrl), 1);
  }
}

// TODO(https://crbug.com/1182032): Now the File System Access API is not
// supported on Android. Enable this browser test after
// https://crbug.com/1011535 is fixed.
#if defined(OS_ANDROID)
#define MAYBE_DeferPrivateOriginFileSystem DISABLED_DeferPrivateOriginFileSystem
#else
#define MAYBE_DeferPrivateOriginFileSystem DeferPrivateOriginFileSystem
#endif

// Tests that access to the origin private file system via the File System
// Access API is deferred until activating the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MAYBE_DeferPrivateOriginFileSystem) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/restriction_file_system.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  const int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  EXPECT_EQ(
      true,
      ExecJs(prerender_render_frame_host, "accessOriginPrivateFileSystem();",
             EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE |
                 EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  // Run a event loop so the page can fail the test.
  EXPECT_TRUE(ExecJs(prerender_render_frame_host, "runLoop();"));

  // Activate the page.
  NavigatePrimaryPage(kPrerenderingUrl);

  // Wait for the completion of `accessOriginPrivateFileSystem`.
  EXPECT_EQ(true, EvalJs(prerender_render_frame_host, "result;"));
  // Check the event sequence seen in the prerendered page.
  EvalJsResult results = EvalJs(prerender_render_frame_host, "eventsSeen");
  std::vector<std::string> eventsSeen;
  const base::Value resultsList = results.ExtractList();
  for (auto& result : resultsList.GetList())
    eventsSeen.push_back(result.GetString());
  EXPECT_THAT(eventsSeen,
              testing::ElementsAreArray(
                  {"accessOriginPrivateFileSystem (prerendering: true)",
                   "prerenderingchange (prerendering: false)",
                   "getDirectory (prerendering: false)"}));
}

// Tests that RenderDocumentHostUserData object is not cleared on activating a
// prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, RenderDocumentHostUserData) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetURL(), kInitialUrl);

  // Start a prerender.
  const int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // Get the DocumentData associated with prerender RenderFrameHost.
  DocumentData::CreateForCurrentDocument(prerender_render_frame_host);
  base::WeakPtr<DocumentData> data =
      DocumentData::GetForCurrentDocument(prerender_render_frame_host)
          ->GetWeakPtr();
  EXPECT_TRUE(data);

  // Activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetURL(), kPrerenderingUrl);

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // DocumentData associated with document shouldn't have been cleared on
  // activating prerendered page.
  base::WeakPtr<DocumentData> data_after_activation =
      DocumentData::GetForCurrentDocument(current_frame_host())->GetWeakPtr();
  EXPECT_TRUE(data_after_activation);

  // Both the instances of DocumentData before and after activation should point
  // to the same object and make sure they aren't null.
  EXPECT_EQ(data_after_activation.get(), data.get());
}

// Tests that executing the GamepadMonitor API on a prerendering before
// navigating to the prerendered page causes cancel prerendering.
// This test cannot be a web test because web tests handles the GamepadMonitor
// interface on the renderer side. See GamepadController::Install().
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, GamepadMonitorCancelPrerendering) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  const int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // Executing `navigator.getGamepads()` to start binding the GamepadMonitor
  // interface.
  ignore_result(EvalJs(prerender_render_frame_host, "navigator.getGamepads()",
                       EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
  // Verify Mojo capability control cancels prerendering.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kMojoBinderPolicy, 1);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface",
      PrerenderCancelledInterface::kGamepadMonitor, 1);
}

// TODO(https://crbug.com/1201980) LaCrOS binds the HidManager interface, which
// might be required by Gamepad Service, in a different way. Disable this test
// before figuring out how to set the test context correctly.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Tests that requesting to bind the GamepadMonitor interface after the
// prerenderingchange event dispatched does not cancel prerendering.
// This test cannot be a web test because web tests handles the GamepadMonitor
// interface on the renderer side. See GamepadController::Install().
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, GamepadMonitorAfterNavigation) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/prerender/restriction-gamepad.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // Activate the prerendered page to dispatch the prerenderingchange event and
  // run the Gamepad API in the event.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);
  // Wait for the completion of the prerenderingchange event to make sure the
  // API is called.
  EXPECT_EQ(true, EvalJs(shell()->web_contents(), "prerenderingChanged"));
  // The API call shouldn't discard the prerendered page and shouldn't restart
  // navigation.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// Tests that accessing the clipboard via the execCommand API fails because the
// page does not has any user activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ClipboardByExecCommandFail) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  const int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // Access the clipboard and fail.
  EXPECT_EQ(false,
            EvalJs(prerender_render_frame_host, "document.execCommand('copy');",
                   EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(false, EvalJs(prerender_render_frame_host,
                          "document.execCommand('paste');",
                          EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

#if !defined(OS_ANDROID) || BUILDFLAG(ENABLE_PLUGINS)
void LoadAndWaitForPrerenderDestroyed(WebContents* const web_contents,
                                      const GURL prerendering_url,
                                      test::PrerenderTestHelper* helper) {
  test::PrerenderHostRegistryObserver registry_observer(*web_contents);
  EXPECT_TRUE(
      ExecJs(web_contents, JsReplace("add_prerender($1)", prerendering_url)));
  registry_observer.WaitForTrigger(prerendering_url);
  int host_id = helper->GetHostForUrl(prerendering_url);
  test::PrerenderHostObserver host_observer(*web_contents, host_id);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(helper->GetHostForUrl(prerendering_url),
            RenderFrameHost::kNoFrameTreeNodeId);
}
#endif  // !defined(OS_ANDROID) || BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(ENABLE_PLUGINS)
// Tests that we will cancel the prerendering if the prerendering page attempts
// to use plugins.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PluginsCancelPrerendering) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  LoadAndWaitForPrerenderDestroyed(
      web_contents(), GetUrl("/prerender/page-with-embedded-plugin.html"),
      prerender_helper());
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kPlugin, 1);
  LoadAndWaitForPrerenderDestroyed(
      web_contents(), GetUrl("/prerender/page-with-object-plugin.html"),
      prerender_helper());
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kPlugin, 2);
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

// This is a browser test and cannot be upstreamed to WPT because it diverges
// from the spec by cancelling prerendering in the Notification constructor,
// whereas the spec says to defer upon use requestPermission().
#if defined(OS_ANDROID)
// On Android the Notification constructor throws an exception regardless of
// whether the page is being prerendered.
// Tests that we will get the exception from the prerendering if the
// prerendering page attempts to use notification.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, NotificationConstructorAndroid) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  const int host_id = AddPrerender(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // Create the Notification and fail.
  EXPECT_EQ(false, EvalJs(prerender_render_frame_host, R"(
    (() => {
      try { new Notification('My Notification'); return true;
      } catch(e) { return false; }
    })();
  )"));
}
#else
// On non-Android the Notification constructor is supported and can be used to
// show a notification, but if used during prerendering it cancels prerendering.
// Tests that we will cancel the prerendering if the prerendering page attempts
// to use notification.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, NotificationConstructor) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  LoadAndWaitForPrerenderDestroyed(web_contents(),
                                   GetUrl("/prerender/notification.html"),
                                   prerender_helper());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kMojoBinderPolicy, 1);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface",
      PrerenderCancelledInterface::kNotificationService, 1);
}
#endif  // defined(OS_ANDROID)

// End: Tests for feature restrictions in prerendered pages ====================

// Tests that prerendering doesn't run for low-end devices.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, LowEndDevice) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Set low-end device mode.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableLowEndDeviceMode);

  // Attempt to prerender.
  test::PrerenderHostRegistryObserver observer(*web_contents_impl());
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace("add_prerender($1)", kPrerenderingUrl)));

  // It should fail.
  observer.WaitForTrigger(kPrerenderingUrl);
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kLowEndDevice, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       IsInactiveAndDisallowActivationCancelsPrerendering) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetURL(), kInitialUrl);

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  const int host_id = AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A prerender host for the URL should be registered.
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);

  // Invoke IsInactiveAndDisallowActivation for the prerendered document.
  EXPECT_EQ(prerender_render_frame_host->lifecycle_state(),
            RenderFrameHostImpl::LifecycleStateImpl::kPrerendering);
  EXPECT_TRUE(prerender_render_frame_host->IsInactiveAndDisallowActivation());

  // The prerender host for the URL should be destroyed as
  // RenderFrameHost::IsInactiveAndDisallowActivation cancels prerendering in
  // LifecycleStateImpl::kPrerendering state.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Cancelling the prerendering disables the activation. The navigation
  // should issue a request again.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kDestroyed, 1);
}

// Make sure input events are routed to the primary FrameTree not the prerender
// one. See https://crbug.com/1197136
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, InputRoutedToPrimaryFrameTree) {
  const GURL kInitialUrl = GetUrl("/prerender/simple_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  WaitForPrerenderLoadCompletion(kPrerenderingUrl);

  // Touch / click the link and wait for the navigation to complete.
  TestNavigationObserver navigation_observer(web_contents());
  SyntheticTapGestureParams params;
  params.gesture_source_type = content::mojom::GestureSourceType::kTouchInput;
  params.position = GetCenterCoordinatesOfElementWithId(web_contents(), "link");
  web_contents_impl()->GetRenderViewHost()->GetWidget()->QueueSyntheticGesture(
      std::make_unique<SyntheticTapGesture>(params), base::DoNothing());
  navigation_observer.Wait();

  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, VisibilityWhilePrerendering) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  const int host_id = AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A prerender host for the URL should be registered.
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);

  // The visibility state must be "hidden" while prerendering.
  auto* prerendered_render_frame_host = GetPrerenderedMainFrameHost(host_id);
  auto* rvh = static_cast<RenderViewHostImpl*>(
      prerendered_render_frame_host->GetRenderViewHost());
  EXPECT_EQ(rvh->GetPageLifecycleStateManager()
                ->CalculatePageLifecycleState()
                ->visibility,
            PageVisibilityState::kHidden);
}

// Tests that prerendering doesn't affect WebContents::GetTitle().
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, TitleWhilePrerendering) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/simple_page.html");
  const std::u16string kInitialTitle(u"title");
  const std::u16string kPrerenderingTitle(u"OK");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("document.title = $1", kInitialTitle)));
  EXPECT_EQ(shell()->web_contents()->GetTitle(), kInitialTitle);

  // Start a prerender to `kPrerenderUrl` that has title `kPrerenderingTitle`.
  ASSERT_NE(AddPrerender(kPrerenderingUrl),
            RenderFrameHost::kNoFrameTreeNodeId);

  // Make sure that WebContents::GetTitle() returns the current title from the
  // primary page.
  EXPECT_EQ(shell()->web_contents()->GetTitle(), kInitialTitle);

  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);
  // The title should be updated with the activated page.
  EXPECT_EQ(shell()->web_contents()->GetTitle(), kPrerenderingTitle);
}

class ScopedDataSaverTestContentBrowserClient
    : public TestContentBrowserClient {
 public:
  ScopedDataSaverTestContentBrowserClient()
      : old_client(SetBrowserClientForTesting(this)) {}
  ~ScopedDataSaverTestContentBrowserClient() override {
    SetBrowserClientForTesting(old_client);
  }

  // ContentBrowserClient overrides:
  bool IsDataSaverEnabled(BrowserContext* context) override { return true; }

  void OverrideWebkitPrefs(WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override {
    prefs->data_saver_enabled = true;
  }

 private:
  ContentBrowserClient* old_client;
};

// Tests that the data saver doesn't prevent image load in a prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DataSaver) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/prerender/image.html");
  const GURL kImageUrl = GetUrl("/blank.jpg");

  // Enable data saver.
  ScopedDataSaverTestContentBrowserClient scoped_content_browser_client;
  shell()->web_contents()->OnWebPreferencesChanged();

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A request for the image in the prerendered page shouldn't be prevented by
  // the data saver.
  EXPECT_EQ(GetRequestCount(kImageUrl), 1);
}

// Tests that loading=lazy doesn't prevent image load in a prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, LazyLoading) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/prerender/image_loading_lazy.html");
  const GURL kImageUrl = GetUrl("/blank.jpg");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A request for the image in the prerendered page shouldn't be prevented by
  // loading=lazy.
  EXPECT_EQ(GetRequestCount(kImageUrl), 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       SessionStorageAfterBackNavigation_NoProcessReuse) {
  // When BackForwardCache feature is enabled, this test doesn't work, because
  // this test is checking the behavior of a new renderer process which is
  // created for a back forward navigation from a prerendered page.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_ASSUMES_NO_CACHING);

  const GURL kInitialUrl = GetUrl("/prerender/session_storage.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/session_storage.html?prerendering=");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  std::unique_ptr<RenderProcessHostWatcher> process_host_watcher =
      std::make_unique<RenderProcessHostWatcher>(
          current_frame_host()->GetProcess(),
          RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  AddPrerender(kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);

  EXPECT_EQ("initial", EvalJs(current_frame_host(),
                              "window.sessionKeysInPrerenderingchange")
                           .ExtractString());
  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());

  // Make sure that the initial renderer process is destroyed. So that the
  // initial renderer process will not be reused after the back forward
  // navigation below.
  process_host_watcher->Wait();

  // Navigate back to the initial page.
  content::TestNavigationObserver observer(shell()->web_contents());
  shell()->GoBackOrForward(-1);
  observer.Wait();
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       SessionStorageAfterBackNavigation_KeepInitialProcess) {
  const GURL kInitialUrl = GetUrl("/prerender/session_storage.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/session_storage.html?prerendering=");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  RenderProcessHostImpl* initial_process_host =
      static_cast<RenderProcessHostImpl*>(current_frame_host()->GetProcess());
  // Increment the keep alive ref count of the renderer process to keep it alive
  // so it is reused on the back navigation below. The test checks that the
  // session storage state changed in the activated page is correctly propagated
  // after a back navigation that uses an existing renderer process. (Note: This
  // is not working correctly now.)
  initial_process_host->IncrementKeepAliveRefCount();

  AddPrerender(kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);

  EXPECT_EQ("initial", EvalJs(current_frame_host(),
                              "window.sessionKeysInPrerenderingchange")
                           .ExtractString());
  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());

  // Navigate back to the initial page.
  content::TestNavigationObserver observer(shell()->web_contents());
  shell()->GoBackOrForward(-1);
  observer.Wait();
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  // There is a known issue that when the initial renderer process is reused
  // after the back navigation, the session storage state changed in the
  // activated is not correctly propagated to the initial renderer process.
  // TODO(crbug.com/1197383): Fix this issue.
  EXPECT_EQ(
      // This should be "activated, initial".
      "initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
}

// Test if the host is abandoned when the renderer page crashes.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, AbandonIfRendererProcessCrashes) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);
  int host_id = GetHostForUrl(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);

  // Crash the relevant renderer.
  {
    test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
    RenderProcessHost* process =
        GetPrerenderedMainFrameHost(host_id)->GetProcess();
    ScopedAllowRendererCrashes allow_renderer_crashes(process);
    // On Android, ForceCrash results in TERMINATION_STATUS_NORMAL_TERMINATION.
    // On other platforms, it does in TERMINATION_STATUS_PROCESS_CRASHED.
    process->ForceCrash();
    host_observer.WaitForDestroyed();
  }

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
#if defined(OS_ANDROID)
      PrerenderHost::FinalStatus::kRendererProcessKilled, 1);
#else
      PrerenderHost::FinalStatus::kRendererProcessCrashed, 1);
#endif  // defined(OS_ANDROID)
}

// Test if the host is abandoned when the renderer page is killed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, AbandonIfRendererProcessIsKilled) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);
  int host_id = GetHostForUrl(kPrerenderingUrl);
  ASSERT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);

  // Shut down the relevant renderer.
  {
    test::PrerenderHostObserver host_observer(*web_contents_impl(), host_id);
    RenderProcessHost* process =
        GetPrerenderedMainFrameHost(host_id)->GetProcess();
    ScopedAllowRendererCrashes allow_renderer_crashes(process);
    EXPECT_TRUE(process->Shutdown(0));
    host_observer.WaitForDestroyed();
  }

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kRendererProcessKilled, 1);
}

class PrerenderSingleProcessBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderSingleProcessBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* cmd_line) override {
    PrerenderBrowserTest::SetUpCommandLine(cmd_line);
    cmd_line->AppendSwitch("single-process");
  }
};

IN_PROC_BROWSER_TEST_F(PrerenderSingleProcessBrowserTest,
                       SessionStorageAfterBackNavigation) {
  // Official Chrome builds (except Android) don't support single-process mode.
  if (!RenderProcessHost::run_renderer_in_process())
    return;

  const GURL kInitialUrl = GetUrl("/prerender/session_storage.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/session_storage.html?prerendering=");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  AddPrerender(kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);

  EXPECT_EQ("initial", EvalJs(current_frame_host(),
                              "window.sessionKeysInPrerenderingchange")
                           .ExtractString());
  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());

  // Navigate back to the initial page.
  content::TestNavigationObserver observer(shell()->web_contents());
  shell()->GoBackOrForward(-1);
  observer.Wait();
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);

  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
}

class PrerenderBackForwardCacheBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderBackForwardCacheBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache, {{"enable_same_site", "true"}}},
         {kBackForwardCacheNoTimeEviction, {}}},
        // Allow BackForwardCache for all devices regardless of their memory.
        {features::kBackForwardCacheMemoryControls});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrerenderBackForwardCacheBrowserTest,
                       SessionStorageAfterBackNavigation) {
  const GURL kInitialUrl = GetUrl("/prerender/session_storage.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/session_storage.html?prerendering=");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  RenderFrameDeletedObserver deleted_observer(
      shell()->web_contents()->GetMainFrame());

  AddPrerender(kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);

  EXPECT_EQ("initial", EvalJs(current_frame_host(),
                              "window.sessionKeysInPrerenderingchange")
                           .ExtractString());
  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());

  // Navigate back to the initial page.
  shell()->GoBackOrForward(-1);
  WaitForLoadStop(shell()->web_contents());

  // Expect the navigation to be served from the back-forward cache to verify
  // the test is testing what is intended.
  ASSERT_EQ(shell()->web_contents()->GetMainFrame(),
            deleted_observer.render_frame_host());

  // There is a known issue that when the initial renderer process is reused
  // after the back navigation, the session storage state changed in the
  // activated is not correctly propagated to the initial renderer process.
  // TODO(crbug.com/1197383): Fix this issue.
  EXPECT_EQ(
      // This should be "activated, initial".
      "initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
}

#if !defined(OS_ANDROID)
// StorageServiceOutOfProcess is not implemented on Android.

class PrerenderRestartStorageServiceBrowserTest : public PrerenderBrowserTest {
 public:
  PrerenderRestartStorageServiceBrowserTest() {
    // These tests only make sense when the service is running
    // out-of-process.
    feature_list_.InitAndEnableFeature(features::kStorageServiceOutOfProcess);
  }

 protected:
  void CrashStorageServiceAndWaitForRestart() {
    mojo::Remote<storage::mojom::StorageService>& service =
        StoragePartitionImpl::GetStorageServiceForTesting();
    base::RunLoop loop;
    service.set_disconnect_handler(base::BindLambdaForTesting([&] {
      loop.Quit();
      service.reset();
    }));
    mojo::Remote<storage::mojom::TestApi> test_api;
    StoragePartitionImpl::GetStorageServiceForTesting()->BindTestApi(
        test_api.BindNewPipeAndPassReceiver().PassPipe());
    test_api->CrashNow();
    loop.Run();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrerenderRestartStorageServiceBrowserTest,
                       RestartStorageServiceBeforePrerendering) {
  const GURL kInitialUrl = GetUrl("/prerender/session_storage.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/session_storage.html?prerendering=");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  CrashStorageServiceAndWaitForRestart();

  EXPECT_EQ(
      "initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());

  AddPrerender(kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);

  EXPECT_EQ("initial", EvalJs(current_frame_host(),
                              "window.sessionKeysInPrerenderingchange")
                           .ExtractString());
  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
}

IN_PROC_BROWSER_TEST_F(PrerenderRestartStorageServiceBrowserTest,
                       RestartStorageServiceWhilePrerendering) {
  const GURL kInitialUrl = GetUrl("/prerender/session_storage.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/session_storage.html?prerendering=");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  const int host_id = AddPrerender(kPrerenderingUrl);

  CrashStorageServiceAndWaitForRestart();

  EXPECT_EQ(
      "initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
  EXPECT_EQ(
      "initial, prerendering",
      EvalJs(GetPrerenderedMainFrameHost(host_id), "getSessionStorageKeys()")
          .ExtractString());

  NavigatePrimaryPage(kPrerenderingUrl);

  EXPECT_EQ("initial", EvalJs(current_frame_host(),
                              "window.sessionKeysInPrerenderingchange")
                           .ExtractString());
  EXPECT_EQ(
      "activated, initial",
      EvalJs(current_frame_host(), "getSessionStorageKeys()").ExtractString());
}
#endif

class PrerenderWithProactiveBrowsingInstanceSwap : public PrerenderBrowserTest {
 public:
  PrerenderWithProactiveBrowsingInstanceSwap() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kProactivelySwapBrowsingInstance,
                               {{"level", "SameSite"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Make sure that we can deal with the speculative RFH that is created during
// the activation navigation.
// TODO(https://crbug.com/1190197): We should try to avoid creating the
// speculative RFH (redirects allowing). Once that is done we should either
// change this test (if redirects allowed) or remove it completely.
IN_PROC_BROWSER_TEST_F(PrerenderWithProactiveBrowsingInstanceSwap,
                       LinkRelPrerender) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(web_contents()->GetURL(), kInitialUrl);

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A prerender host for the URL should be registered.
  EXPECT_TRUE(HasHostForUrl(kPrerenderingUrl));

  // Activate the prerendered page.
  // The test passes if we don't crash while cleaning up speculative render
  // frame host.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetURL(), kPrerenderingUrl);

  // The prerender host should be consumed.
  EXPECT_FALSE(HasHostForUrl(kPrerenderingUrl));

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
}

class PrerenderWithBackForwardCacheBrowserTest
    : public PrerenderBrowserTest,
      public testing::WithParamInterface<BackForwardCacheType> {
 public:
  PrerenderWithBackForwardCacheBrowserTest() {
    // Set up the common params for the BFCache.
    base::FieldTrialParams feature_params;
    feature_params["TimeToLiveInBackForwardCacheInSeconds"] = "3600";

    // Allow the BFCache for all devices regardless of their memory.
    std::vector<base::Feature> disabled_features{
        features::kBackForwardCacheMemoryControls};

    switch (GetParam()) {
      case BackForwardCacheType::kDisabled:
        feature_list_.InitAndDisableFeature(features::kBackForwardCache);
        break;
      case BackForwardCacheType::kEnabled:
        feature_list_.InitWithFeaturesAndParameters(
            {{features::kBackForwardCache, feature_params}}, disabled_features);
        break;
      case BackForwardCacheType::kEnabledWithSameSite:
        feature_params["enable_same_site"] = "true";
        feature_list_.InitWithFeaturesAndParameters(
            {{features::kBackForwardCache, feature_params}}, disabled_features);
        break;
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PrerenderWithBackForwardCacheBrowserTest,
    testing::Values(BackForwardCacheType::kDisabled,
                    BackForwardCacheType::kEnabled,
                    BackForwardCacheType::kEnabledWithSameSite),
    ToString);

// Tests that history navigation works after activation. This runs with variaous
// BFCache configurations that may modify behavior of history navigation.
// This is a regression test for https://crbug.com/1201914.
IN_PROC_BROWSER_TEST_P(PrerenderWithBackForwardCacheBrowserTest,
                       HistoryNavigationAfterActivation) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  RenderFrameHostImpl* initial_frame_host = current_frame_host();
  blink::LocalFrameToken initial_frame_token =
      initial_frame_host->GetFrameToken();

  // When the BFCache is disabled, activation will destroy the initial frame
  // host. This observer will be used for confirming it.
  RenderFrameDeletedObserver delete_observer(initial_frame_host);

  // Make and activate a prerendered page.
  AddPrerender(kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kPrerenderingUrl);

  // Check if the initial page is in the BFCache.
  switch (GetParam()) {
    case BackForwardCacheType::kDisabled:
      EXPECT_NE(current_frame_host(), initial_frame_host);
      // The initial frame host should be deleted after activation because it is
      // not cached in the BFCache.
      delete_observer.WaitUntilDeleted();
      break;
    case BackForwardCacheType::kEnabled:
      // Same-origin prerender activation should allow the initial page to be
      // cached in the BFCache even if the BFCache for same-site (same-origin)
      // is not enabled. This is because prerender activation always swaps
      // BrowsingInstance and it makes the previous page cacheacble unlike
      // regular same-origin navigation.
      ASSERT_FALSE(IsSameSiteBackForwardCacheEnabled());
      EXPECT_TRUE(initial_frame_host->IsInBackForwardCache());
      break;
    case BackForwardCacheType::kEnabledWithSameSite:
      // Same-origin prerender activation should allow the initial page to be
      // cached in the BFCache.
      ASSERT_TRUE(IsSameSiteBackForwardCacheEnabled());
      EXPECT_TRUE(initial_frame_host->IsInBackForwardCache());
      break;
  }

  // Navigate back to the initial page.
  content::TestNavigationObserver observer(web_contents());
  shell()->GoBackOrForward(-1);
  observer.Wait();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), kInitialUrl);

  // Check if the back navigation is served from the BFCache.
  switch (GetParam()) {
    case BackForwardCacheType::kDisabled:
      // The frame host should be created again.
      EXPECT_NE(current_frame_host()->GetFrameToken(), initial_frame_token);
      break;
    case BackForwardCacheType::kEnabled:
    case BackForwardCacheType::kEnabledWithSameSite:
      // The frame host should be restored.
      EXPECT_EQ(current_frame_host()->GetFrameToken(), initial_frame_token);
      EXPECT_FALSE(initial_frame_host->IsInBackForwardCache());
      break;
  }
}

}  // namespace
}  // namespace content
