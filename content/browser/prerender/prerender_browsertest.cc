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
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "content/browser/file_system_access/file_system_chooser_test_helpers.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_document_host_user_data.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_mojo_binder_policy_applier_unittest.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "url/gurl.h"

namespace content {
namespace {

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

// TODO(https://crbug.com/1132746): There are two different ways of prerendering
// the page: a dedicated WebContents instance or using a separate FrameTree
// instance (MPArch). The MPArch code is still in its very early stages but will
// eventually completely replace the WebContents approach. In the meantime we
// should try to get all test to pass with both implementations.
enum PrerenderBrowserTestType {
  kWebContents,
  kMPArch,
};

std::string ToString(
    const testing::TestParamInfo<PrerenderBrowserTestType>& info) {
  switch (info.param) {
    case PrerenderBrowserTestType::kWebContents:
      return "WebContents";
    case PrerenderBrowserTestType::kMPArch:
      return "MPArch";
  }
}

class PrerenderHostObserver : public PrerenderHost::Observer {
 public:
  explicit PrerenderHostObserver(PrerenderHost& host) {
    observation_.Observe(&host);
  }

  void OnHostDestroyed() override {
    observation_.Reset();
    if (waiting_)
      std::move(waiting_).Run();
  }

  void WaitForDestroyed() {
    if (!observation_.IsObserving())
      return;
    DCHECK(!waiting_);
    base::RunLoop loop;
    waiting_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  base::ScopedObservation<PrerenderHost, PrerenderHost::Observer> observation_{
      this};
  base::OnceClosure waiting_;
};

class PrerenderHostRegistryObserver : public PrerenderHostRegistry::Observer {
 public:
  explicit PrerenderHostRegistryObserver(PrerenderHostRegistry& registry) {
    observation_.Observe(&registry);
  }

  // Returns immediately if `url` was ever triggered before.
  void WaitForTrigger(const GURL& url) {
    if (triggered_.contains(url)) {
      return;
    }
    DCHECK(!waiting_.contains(url));
    base::RunLoop loop;
    waiting_[url] = loop.QuitClosure();
    loop.Run();
  }

  void OnTrigger(const GURL& url) override {
    auto iter = waiting_.find(url);
    if (iter != waiting_.end()) {
      auto callback = std::move(iter->second);
      waiting_.erase(iter);
      std::move(callback).Run();
    } else {
      DCHECK(!triggered_.contains(url))
          << "this observer doesn't yet support multiple triggers";
      triggered_.insert(url);
    }
  }

  void OnRegistryDestroyed() override {
    DCHECK(waiting_.empty());
    observation_.Reset();
  }

  base::ScopedObservation<PrerenderHostRegistry,
                          PrerenderHostRegistry::Observer>
      observation_{this};

  base::flat_map<GURL, base::OnceClosure> waiting_;
  base::flat_set<GURL> triggered_;
};

class PrerenderBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<PrerenderBrowserTestType> {
 public:
  using LifecycleStateImpl = RenderFrameHostImpl::LifecycleStateImpl;

  PrerenderBrowserTest() {
    std::map<std::string, std::string> parameters;
    switch (GetParam()) {
      case kWebContents:
        parameters["implementation"] = "webcontents";
        break;
      case kMPArch:
        parameters["implementation"] = "mparch";
        break;
    }
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kPrerender2, parameters);
  }
  ~PrerenderBrowserTest() override = default;

  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    host_resolver()->AddRule("*", "127.0.0.1");
    ssl_server_.AddDefaultHandlers(GetTestDataFilePath());
    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    ssl_server_.RegisterRequestMonitor(base::BindRepeating(
        &PrerenderBrowserTest::MonitorResourceRequest, base::Unretained(this)));
    ASSERT_TRUE(ssl_server_.Start());
  }

  void TearDownOnMainThread() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    EXPECT_TRUE(ssl_server_.ShutdownAndWaitUntilComplete());
  }

  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    // This should be called on `EmbeddedTestServer::io_thread_`.
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::AutoLock auto_lock(lock_);
    request_count_by_path_[request.GetURL().PathForRequest()]++;
    if (monitor_callback_)
      std::move(monitor_callback_).Run();
  }

  PrerenderHostRegistry& GetPrerenderHostRegistry() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto* storage_partition = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext()));
    return *storage_partition->GetPrerenderHostRegistry();
  }

  // Waits until the request count for `url` reaches `count`.
  void WaitForRequest(const GURL& url, int count) {
    for (;;) {
      base::RunLoop run_loop;
      {
        base::AutoLock auto_lock(lock_);
        if (request_count_by_path_[url.PathForRequest()] >= count)
          return;
        monitor_callback_ = run_loop.QuitClosure();
      }
      run_loop.Run();
    }
  }

  // Must only be called if the host for `prerendering_url` will be
  // created.
  void WaitForPrerenderLoadCompletion(const GURL& prerendering_url) {
    PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
    PrerenderHost* host = registry.FindHostByUrlForTesting(prerendering_url);
    // Wait for the host to be created if it hasn't yet.
    if (!host) {
      PrerenderHostRegistryObserver observer(registry);
      observer.WaitForTrigger(prerendering_url);
      host = registry.FindHostByUrlForTesting(prerendering_url);
      ASSERT_NE(host, nullptr);
    }
    host->WaitForLoadStopForTesting();
  }

  // Adds <link rel=prerender> in the current main frame and waits until the
  // completion of prerendering.
  void AddPrerender(const GURL& prerendering_url) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Add the link tag that will prerender the URL.
    EXPECT_TRUE(ExecJs(shell()->web_contents(),
                       JsReplace("add_prerender($1)", prerendering_url)));

    WaitForPrerenderLoadCompletion(prerendering_url);
  }

  // Navigates the primary page to the URL and waits until the completion of the
  // navigation.
  //
  // Navigations that could activate a prerendered page on the multiple
  // WebContents architecture (not multiple-pages architecture known as MPArch)
  // should use this function instead of the NavigateToURL() test helper. This
  // is because the test helper accesses the predecessor WebContents to be
  // destroyed during activation and results in crashes.
  // See https://crbug.com/1154501 for the MPArch migration.
  void NavigatePrimaryPage(const GURL& url) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    content::TestNavigationObserver observer(shell()->web_contents());
    // Ignore the result of ExecJs().
    //
    // Depending on timing, activation could destroy the current WebContents
    // before ExecJs() gets a result from the frame that executed scripts. This
    // results in execution failure even when the execution succeeded. See
    // https://crbug.com/1156141 for details.
    //
    // This part will drastically be modified by the MPArch, so we take the
    // approach just to ignore it instead of fixing the timing issue. When
    // ExecJs() actually fails, the remaining test steps should fail, so it
    // should be safe to ignore it.
    ignore_result(ExecJs(shell()->web_contents()->GetMainFrame(),
                         JsReplace("location = $1", url)));
    observer.Wait();
  }

  // Navigates a prerendered page to the URL.
  void NavigatePrerenderedPage(PrerenderHost& prerender_host, const GURL& url) {
    RenderFrameHostImpl* prerender_render_frame_host =
        prerender_host.GetPrerenderedMainFrameHost();
    // Ignore the result of ExecJs().
    //
    // Navigation from the prerendered page could cancel prerendering and
    // destroy the prerendered frame before ExecJs() gets a result from that.
    // This results in execution failure even when the execution succeeded. See
    // https://crbug.com/1186584 for details.
    //
    // This part will drastically be modified by the MPArch, so we take the
    // approach just to ignore it instead of fixing the timing issue. When
    // ExecJs() actually fails, the remaining test steps should fail, so it
    // should be safe to ignore it.
    ignore_result(
        ExecJs(prerender_render_frame_host, JsReplace("location = $1", url)));
  }

  GURL GetUrl(const std::string& path) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return ssl_server_.GetURL("a.test", path);
  }

  GURL GetCrossOriginUrl(const std::string& path) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return ssl_server_.GetURL("b.test", path);
  }

  int GetRequestCount(const GURL& url) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::AutoLock auto_lock(lock_);
    return request_count_by_path_[url.PathForRequest()];
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetMainFrame();
  }

  bool IsMPArchActive() const {
    switch (GetParam()) {
      case kWebContents:
        return false;
      case kMPArch:
        return true;
    }
  }

  void TestHostPrerenderingState(const GURL& prerender_url) {
    const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");

    // Navigate to an initial page.
    ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

    // The initial page should not be in prerendered state.
    RenderFrameHostImpl* initiator_render_frame_host =
        static_cast<RenderFrameHostImpl*>(
            shell()->web_contents()->GetMainFrame());
    EXPECT_EQ(initiator_render_frame_host->frame_tree()->type(),
              FrameTree::Type::kPrimary);
    EXPECT_EQ(initiator_render_frame_host->lifecycle_state(),
              LifecycleStateImpl::kActive);

    // Start a prerender.
    AddPrerender(prerender_url);
    PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
    PrerenderHost* prerender_host =
        registry.FindHostByUrlForTesting(prerender_url);

    // Verify all RenderFrameHostImpl in the prerendered page know the
    // prerendering state.
    RenderFrameHostImpl* prerendered_render_frame_host =
        prerender_host->GetPrerenderedMainFrameHost();
    std::vector<RenderFrameHost*> frames =
        prerendered_render_frame_host->GetFramesInSubtree();
    for (auto* frame : frames) {
      auto* rfhi = static_cast<RenderFrameHostImpl*>(frame);
      // All the subframes should be in LifecycleStateImpl::kPrerendering state
      // before activation.
      EXPECT_EQ(rfhi->lifecycle_state(),
                RenderFrameHostImpl::LifecycleStateImpl::kPrerendering);
      EXPECT_EQ(rfhi->frame_tree()->type(), FrameTree::Type::kPrerender);
    }

    // Activate the prerendered page.
    NavigatePrimaryPage(prerender_url);
    EXPECT_EQ(shell()->web_contents()->GetURL(), prerender_url);

    // The activated page should no longer be in the prerendering state.
    RenderFrameHostImpl* navigated_render_frame_host =
        static_cast<RenderFrameHostImpl*>(
            shell()->web_contents()->GetMainFrame());
    // The new page shouldn't be in the prerendering state.
    frames = navigated_render_frame_host->GetFramesInSubtree();
    for (auto* frame : frames) {
      auto* rfhi = static_cast<RenderFrameHostImpl*>(frame);
      // All the subframes should be transitioned to LifecycleStateImpl::kActive
      // state after activation.
      EXPECT_EQ(rfhi->lifecycle_state(),
                RenderFrameHostImpl::LifecycleStateImpl::kActive);
      EXPECT_FALSE(rfhi->frame_tree()->is_prerendering());
    }
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) final {
    // Useful for testing CSP:prefetch-src
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  // Counts of requests sent to the server. Keyed by path (not by full URL)
  // because the host part of the requests is translated ("a.test" to
  // "127.0.0.1") before the server handles them.
  // This is accessed from the UI thread and `EmbeddedTestServer::io_thread_`.
  std::map<std::string, int> request_count_by_path_ GUARDED_BY(lock_);

  base::OnceClosure monitor_callback_ GUARDED_BY(lock_);

  base::test::ScopedFeatureList feature_list_;

  base::Lock lock_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrerenderBrowserTest,
                         testing::Values(kWebContents, kMPArch),
                         ToString);

IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, LinkRelPrerender) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A prerender host for the URL should be registered.
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_NE(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  // Activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);

  // The prerender host should be consumed.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
}

IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, LinkRelPrerender_Multiple) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl1 = GetUrl("/empty.html?1");
  const GURL kPrerenderingUrl2 = GetUrl("/empty.html?2");

  // TODO(https://crbug.com/1186893): PrerenderHost is not deleted when the
  // page enters BackForwardCache, though it should be. While this functionality
  // is not implemented, disable BackForwardCache for testing and wait for the
  // old RenderFrameHost to be deleted after we navigate away from it.
  DisableBackForwardCacheForTesting(
      shell()->web_contents(), BackForwardCacheImpl::TEST_ASSUMES_NO_CACHING);

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

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
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_NE(registry.FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  EXPECT_NE(registry.FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);

  RenderFrameDeletedObserver delete_observer_rfh(
      shell()->web_contents()->GetMainFrame());

  // Activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl2);
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl2);

  // Other PrerenderHost instances are deleted with the RFH.
  delete_observer_rfh.WaitUntilDeleted();

  // The prerender hosts should be consumed or destroyed for activation.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl1), 1);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl2), 1);
}

IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, LinkRelPrerender_Duplicate) {
  const GURL kInitialUrl = GetUrl("/prerender/duplicate_prerenders.html");
  const GURL kPrerenderingUrl1 = GetUrl("/empty.html?1");
  const GURL kPrerenderingUrl2 = GetUrl("/empty.html?2");

  // TODO(https://crbug.com/1186893): PrerenderHost is not deleted when the
  // page enters BackForwardCache, though it should be. While this functionality
  // is not implemented, disable BackForwardCache for testing and wait for the
  // old RenderFrameHost to be deleted after we navigate away from it.
  DisableBackForwardCacheForTesting(
      shell()->web_contents(), BackForwardCacheImpl::TEST_ASSUMES_NO_CACHING);

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
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_NE(registry.FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  EXPECT_NE(registry.FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);

  RenderFrameDeletedObserver delete_observer_rfh(
      shell()->web_contents()->GetMainFrame());

  // Activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl1);
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl1);

  // Other PrerenderHost instances are deleted with the RFH.
  delete_observer_rfh.WaitUntilDeleted();

  // The prerender hosts should be consumed or destroyed for activation.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl1), 1);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl2), 1);
}

// Regression test for https://crbug.com/1194865.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, CloseOnPrerendering) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A prerender host for the URL should be registered.
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_NE(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  // Should not crash.
  shell()->Close();
}

// Tests that non-http(s) schemes are disallowed for prerendering.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, HttpToBlobUrl) {
  base::HistogramTester histogram_tester;

  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Generate a Blob page and obtain a URL for the Blob page.
  const char kCreateBlobUrlScript[] =
      "URL.createObjectURL("
      "new Blob([\"<h1>hello blob</h1>\"], { type: 'text/html' }));";
  const std::string blob_url =
      EvalJs(shell()->web_contents(), kCreateBlobUrlScript).ExtractString();
  const GURL blob_gurl(blob_url);

  // Add <link rel=prerender> that will prerender the Blob page.
  PrerenderHostRegistryObserver observer(GetPrerenderHostRegistry());
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("add_prerender($1)", blob_url)));
  observer.WaitForTrigger(blob_gurl);

  // A prerender host for the URL should not be registered.
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_FALSE(registry.FindHostByUrlForTesting(blob_gurl));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kInvalidSchemeNavigation, 1);
}

// Tests that non-http(s) schemes are disallowed for prerendering.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, BlobUrlToBlobUrl) {
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
      EvalJs(shell()->web_contents(), kCreateBlobUrlScript).ExtractString();
  ASSERT_TRUE(NavigateToURL(shell(), GURL(initial_blob_url)));

  // Create another Blob URL inside the Blob page.
  const std::string blob_url =
      EvalJs(shell()->web_contents(), kCreateBlobUrlScript).ExtractString();
  const GURL blob_gurl(blob_url);

  // Add <link rel=prerender> that will prerender the Blob page.
  PrerenderHostRegistryObserver observer(GetPrerenderHostRegistry());
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("add_prerender($1)", blob_url)));
  observer.WaitForTrigger(blob_gurl);

  // A prerender host for the URL should not be registered.
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_FALSE(registry.FindHostByUrlForTesting(blob_gurl));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kInvalidSchemeNavigation, 1);
}

IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, SameOriginRedirection) {
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
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_TRUE(registry.FindHostByUrlForTesting(kPrerenderingUrl));
  EXPECT_FALSE(registry.FindHostByUrlForTesting(kRedirectedUrl));
}

IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, CrossOriginRedirection) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin redirection. The
  // cross-origin redirection should fail prerendering.
  const GURL kRedirectedUrl = GetCrossOriginUrl("/empty.html");
  const GURL kPrerenderingUrl =
      GetUrl("/server-redirect?" + kRedirectedUrl.spec());
  PrerenderHostRegistryObserver registry_observer(GetPrerenderHostRegistry());
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("add_prerender($1)", kPrerenderingUrl)));
  registry_observer.WaitForTrigger(kPrerenderingUrl);
  PrerenderHost* prerender_host =
      GetPrerenderHostRegistry().FindHostByUrlForTesting(kPrerenderingUrl);
  PrerenderHostObserver host_observer(*prerender_host);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 0);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_FALSE(registry.FindHostByUrlForTesting(kPrerenderingUrl));
  EXPECT_FALSE(registry.FindHostByUrlForTesting(kRedirectedUrl));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kCrossOriginRedirect, 1);
}

// Makes sure that activation on navigation for an iframes doesn't happen.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, Activation_iFrame) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);

  // Attempt to activate the prerendered page for an iframe. This should fail
  // and fallback to network request.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ("LOADED", EvalJs(shell()->web_contents(),
                             JsReplace("add_iframe($1)", kPrerenderingUrl)));
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);

  // Activation shouldn't happen, so the prerender host should not be consumed.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), prerender_host);
}

// Makes sure that cross-origin subframe navigations are deferred during
// prerendering.
// Flaky https://crbug.com/1190262.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       DISABLED_DeferCrossOriginSubframeNavigation) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html?initial");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/add_prerender.html?prerender");
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);

  const GURL kSameOriginSubframeUrl =
      GetUrl("/prerender/add_prerender.html?same_origin_iframe");
  const GURL kCrossOriginSubframeUrl =
      GetCrossOriginUrl("/prerender/add_prerender.html?cross_origin_iframe");

  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ASSERT_EQ(GetRequestCount(kSameOriginSubframeUrl), 0);
  ASSERT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 0);

  // Add a cross-origin iframe to the prerendering page.
  RenderFrameHost* prerender_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();
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
  ASSERT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);
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
// Flaky https://crbug.com/1190262.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       DISABLED_DeferCrossOriginRedirectsOnSubframeNavigation) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html?initial");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/add_prerender.html?prerender");
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);

  const GURL kCrossOriginSubframeUrl =
      GetCrossOriginUrl("/prerender/add_prerender.html?cross_origin_iframe");
  const GURL kServerRedirectSubframeUrl =
      GetUrl("/server-redirect?" + kCrossOriginSubframeUrl.spec());

  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  ASSERT_EQ(GetRequestCount(kServerRedirectSubframeUrl), 0);
  ASSERT_EQ(GetRequestCount(kCrossOriginSubframeUrl), 0);

  // Add an iframe pointing to a server redirect page to the prerendering page.
  RenderFrameHost* prerender_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();
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
  ASSERT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);
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
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       MainFrameNavigationCancelsPrerendering) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  const GURL kHungUrl = GetUrl("/hung");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);

  // Make the host ready for activation.
  prerender_host->WaitForLoadStopForTesting();

  // Start a navigation in the prerender frame tree that will cancel the
  // initiator's prerendering.
  PrerenderHostObserver observer(*prerender_host);
  NavigatePrerenderedPage(*prerender_host, kHungUrl);
  observer.WaitForDestroyed();
  EXPECT_FALSE(registry.FindHostByUrlForTesting(kPrerenderingUrl));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kMainFrameNavigation, 1);
}

// Makes sure that activation on navigation for a pop-up window doesn't happen.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, Activation_PopUpWindow) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);

  // Attempt to activate the prerendered page for a pop-up window. This should
  // fail and fallback to network request.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ("LOADED", EvalJs(shell()->web_contents(),
                             JsReplace("open_window($1)", kPrerenderingUrl)));
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);

  // Activation shouldn't happen, so the prerender host should not be consumed.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), prerender_host);
}

// Makes sure that activation on navigation for a page that has a pop-up window
// doesn't happen.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, Activation_PageWithPopUpWindow) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html?next");
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  ASSERT_TRUE(registry.FindHostByUrlForTesting(kPrerenderingUrl));

  // Open a pop-up window.
  const GURL kWindowUrl = GetUrl("/empty.html?window");
  EXPECT_EQ("LOADED", EvalJs(shell()->web_contents(),
                             JsReplace("open_window($1)", kWindowUrl)));

  // Attempt to activate the prerendered page for the top-level frame. This
  // should fail and fallback to network request because the pop-up window
  // exists.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);

  // Activation shouldn't happen, so the prerender host should not be consumed.
  // However, we don't check the existence of the prerender host here unlike
  // other activation tests because navigating the frame that triggered
  // prerendering abandons the prerendered page regardless of activation.
}

// Tests that back-forward history is preserved after activation.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, HistoryAfterActivation) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make and activate a prerendered page.
  AddPrerender(kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kPrerenderingUrl);

  // Navigate back to the initial page.
  content::TestNavigationObserver observer(shell()->web_contents());
  shell()->GoBackOrForward(-1);
  observer.Wait();
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), kInitialUrl);
}

// Tests that all RenderFrameHostImpls in the prerendering page know the
// prerendering state.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, PrerenderIframe) {
  TestHostPrerenderingState(GetUrl("/page_with_iframe.html"));
}

// Blank <iframe> is a special case. Tests that the blank iframe knows the
// prerendering state as well.
// TODO(https://crbug.com/1185965): This test is disabled for flakiness.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, DISABLED_PrerenderBlankIframe) {
  TestHostPrerenderingState(GetUrl("/page_with_blank_iframe.html"));
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
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, MojoCapabilityControl) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);

  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();
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
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);

  // Wait until the deferred interface is granted on all frames.
  run_loop.Run();
  EXPECT_EQ(test_browser_client.GetDeferReceiverSetSize(), frames.size());

  SetBrowserClientForTesting(old_browser_client);
}

// Tests that mojo capability control will cancel prerendering if the main frame
// receives a request for a kCancel interface.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       MojoCapabilityControl_CancelMainFrame) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);
  base::HistogramTester histogram_tester;

  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
      prerendered_render_frame_host
          ->browser_interface_broker_receiver_for_testing();
  blink::mojom::BrowserInterfaceBroker* prerender_broker =
      bib.internal_state()->impl();

  // Send a kCancel request to cancel prerendering.
  EXPECT_NE(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
  mojo::Remote<mojom::TestInterfaceForCancel> remote;
  prerender_broker->GetInterface(remote.BindNewPipeAndPassReceiver());
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kDisallowedMojoInterface, 1);
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
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       MojoCapabilityControl_CancelIframe) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);
  base::HistogramTester histogram_tester;

  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/page_with_iframe.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  RenderFrameHostImpl* main_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();
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
  EXPECT_NE(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
  mojo::Remote<mojom::TestInterfaceForCancel> remote;
  prerender_broker->GetInterface(remote.BindNewPipeAndPassReceiver());
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  SetBrowserClientForTesting(old_browser_client);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kDisallowedMojoInterface, 1);
  // `TestInterfaceForCancel` doesn't have a enum value because it is not used
  // in production, so histogram_tester should log
  // PrerenderCancelledInterface::kUnkown here.
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface",
      PrerenderCancelledInterface::kUnknown, 1);
}

// Tests that mojo capability control will crash the prerender if the browser
// process receives a kUnexpected interface.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       MojoCapabilityControl_HandleUnexpected) {
  MojoCapabilityControlTestContentBrowserClient test_browser_client;
  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);

  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

  // Set up the error handler for bad mojo messages.
  std::string bad_message_error;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        EXPECT_FALSE(error.empty());
        EXPECT_TRUE(bad_message_error.empty());
        bad_message_error = error;
      }));

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();

  // Rebind a receiver for testing.
  // mojo::ReportBadMessage must be called within the stack frame derived from
  // mojo IPC calls, so this browser test should call the
  // remote<blink::mojom::BrowserInterfaceBroker>::GetInterface() to test
  // unexpected interfaces. But its remote end is in renderer processes and
  // inaccessible, so the test code has to create another BrowserInterfaceBroker
  // pipe and rebind the receiver end so as to send the request from the remote.
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
      prerendered_render_frame_host
          ->browser_interface_broker_receiver_for_testing();
  auto broker_receiver_of_previous_document = bib.Unbind();
  ASSERT_TRUE(broker_receiver_of_previous_document);
  mojo::Remote<blink::mojom::BrowserInterfaceBroker> remote_broker;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> fake_receiver =
      remote_broker.BindNewPipeAndPassReceiver();
  prerendered_render_frame_host->BindBrowserInterfaceBrokerReceiver(
      std::move(fake_receiver));

  // Send a kUnexpected request.
  EXPECT_NE(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
  mojo::Remote<mojom::TestInterfaceForUnexpected> remote;
  remote_broker->GetInterface(remote.BindNewPipeAndPassReceiver());
  remote_broker.FlushForTesting();
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
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
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, FeatureRestriction_WindowOpen) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/add_prerender.html?prerendering");
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  RenderFrameHostImpl* prerender_frame =
      prerender_host->GetPrerenderedMainFrameHost();

  // Attempt to open a window in the prerendered page. This should fail.
  const GURL kWindowOpenUrl = GetUrl("/empty.html");
  EXPECT_EQ("FAILED", EvalJs(prerender_frame,
                             JsReplace("open_window($1)", kWindowOpenUrl)));
  EXPECT_EQ(GetRequestCount(kWindowOpenUrl), 0);

  // Opening a window shouldn't cancel prerendering.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), prerender_host);
}

// Tests that Clients#matchAll() on ServiceWorkerGlobalScope exposes clients in
// the prerendering state.
// TODO(https://crbug.com/1166470): This can be a tentative behavior. We may
// stop exposing the clients.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       FeatureRestriction_ClientsMatchAll) {
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/clients_matchall.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);

  // Make sure the prerendering page is exposed via Clients#matchAll().
  EvalJsResult results =
      EvalJs(shell()->web_contents(), "get_exposed_client_urls()");
  std::vector<std::string> client_urls;
  for (auto& result : results.ExtractList())
    client_urls.push_back(result.GetString());
  EXPECT_TRUE(base::Contains(client_urls, kInitialUrl));
  EXPECT_TRUE(base::Contains(client_urls, kPrerenderingUrl));
}

IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, RenderFrameHostLifecycleState) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/prerender/add_prerender.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_EQ(current_frame_host()->lifecycle_state(),
            LifecycleStateImpl::kActive);

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);

  // Open an iframe in the prerendered page.
  RenderFrameHostImpl* rfh_a = prerender_host->GetPrerenderedMainFrameHost();
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
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, CSPPrefetchSrc) {
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
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);

    // Prerender will fail. Then FindHostByUrlForTesting() should return null.
    PrerenderHostRegistryObserver observer(GetPrerenderHostRegistry());
    EXPECT_TRUE(ExecJs(shell()->web_contents(),
                       JsReplace("add_prerender($1)", disallowed_url)));
    observer.WaitForTrigger(disallowed_url);
    EXPECT_FALSE(
        GetPrerenderHostRegistry().FindHostByUrlForTesting(disallowed_url));
    console_observer.Wait();
    EXPECT_EQ(1u, console_observer.messages().size());
    EXPECT_EQ(GetRequestCount(disallowed_url), 0);
    histogram_tester.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus",
        PrerenderHost::FinalStatus::kNavigationRequestBlockedByCsp, 1);
  }

  // Check what happens when prerendering isn't blocked.
  {
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);
    GURL kAllowedUrl = GetUrl("/empty.html");
    AddPrerender(kAllowedUrl);
    EXPECT_EQ(0u, console_observer.messages().size());
    EXPECT_EQ(GetRequestCount(kAllowedUrl), 1);
  }
}

// Tests that prerendering is gated behind CSP:default-src
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, CSPDefaultSrc) {
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
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);
    PrerenderHostRegistryObserver observer(GetPrerenderHostRegistry());
    EXPECT_TRUE(ExecJs(shell()->web_contents(),
                       JsReplace("add_prerender($1)", disallowed_url)));
    observer.WaitForTrigger(disallowed_url);
    EXPECT_FALSE(
        GetPrerenderHostRegistry().FindHostByUrlForTesting(disallowed_url));
    console_observer.Wait();
    EXPECT_EQ(1u, console_observer.messages().size());
    EXPECT_EQ(GetRequestCount(disallowed_url), 0);
    histogram_tester.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus",
        PrerenderHost::FinalStatus::kNavigationRequestBlockedByCsp, 1);
  }

  // Check what happens when prerendering isn't blocked.
  {
    WebContentsConsoleObserver console_observer(web_contents());
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
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       MAYBE_DeferPrivateOriginFileSystem) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/restriction_file_system.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  AddPrerender(kPrerenderingUrl);

  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  RenderFrameHostImpl* prerender_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();

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
  for (auto& result : results.ExtractList())
    eventsSeen.push_back(result.GetString());
  EXPECT_THAT(eventsSeen,
              testing::ElementsAreArray(
                  {"accessOriginPrivateFileSystem (prerendering: true)",
                   "prerenderingchange (prerendering: false)",
                   "getDirectory (prerendering: false)"}));
}

// Tests that RenderDocumentHostUserData object is not cleared on activating a
// prerendered page.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, RenderDocumentHostUserData) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();

  // Get the DocumentData associated with prerender RenderFrameHost.
  DocumentData::CreateForCurrentDocument(prerendered_render_frame_host);
  base::WeakPtr<DocumentData> data =
      DocumentData::GetForCurrentDocument(prerendered_render_frame_host)
          ->GetWeakPtr();
  EXPECT_TRUE(data);

  // Activate the prerendered page.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);

  // The prerender host should be consumed.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  // DocumentData associated with document shouldn't have been cleared on
  // activating prerendered page.
  base::WeakPtr<DocumentData> data_after_activation =
      DocumentData::GetForCurrentDocument(current_frame_host())->GetWeakPtr();
  EXPECT_TRUE(data_after_activation);

  // Both the instances of DocumentData before and after activation should point
  // to the same object and make sure they aren't null.
  EXPECT_EQ(data_after_activation.get(), data.get());
}

// Tests that accessing the clipboard via the execCommand API fails because the
// page does not has any user activation.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, ClipboardByExecCommandFail) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  AddPrerender(kPrerenderingUrl);

  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();

  // Access the clipboard and fail.
  EXPECT_EQ(false, EvalJs(prerendered_render_frame_host,
                          "document.execCommand('copy');",
                          EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(false, EvalJs(prerendered_render_frame_host,
                          "document.execCommand('paste');",
                          EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

#if BUILDFLAG(ENABLE_PLUGINS)
void TestPlugin(WebContents* const web_contents,
                PrerenderHostRegistry& registry,
                const GURL prerendering_url) {
  PrerenderHostRegistryObserver registry_observer(registry);
  EXPECT_TRUE(
      ExecJs(web_contents, JsReplace("add_prerender($1)", prerendering_url)));
  registry_observer.WaitForTrigger(prerendering_url);
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(prerendering_url);
  PrerenderHostObserver host_observer(*prerender_host);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(registry.FindHostByUrlForTesting(prerendering_url), nullptr);
}

// Tests that we will cancel the prerendering if the prerendering page attempts
// to use plugins.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, PluginsCancelPrerendering) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  TestPlugin(shell()->web_contents(), registry,
             GetUrl("/prerender/page-with-embedded-plugin.html"));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kPlugin, 1);
  TestPlugin(shell()->web_contents(), registry,
             GetUrl("/prerender/page-with-object-plugin.html"));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kPlugin, 2);
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

// End: Tests for feature restrictions in prerendered pages ====================

// Tests that prerendering doesn't run for low-end devices.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, LowEndDevice) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Set low-end device mode.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableLowEndDeviceMode);

  // Attempt to prerender.
  PrerenderHostRegistryObserver observer(GetPrerenderHostRegistry());
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("add_prerender($1)", kPrerenderingUrl)));

  // It should fail.
  observer.WaitForTrigger(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  EXPECT_FALSE(prerender_host);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kLowEndDevice, 1);
}

IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       IsInactiveAndDisallowActivationCancelsPrerendering) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A prerender host for the URL should be registered.
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  EXPECT_NE(prerender_host, nullptr);

  // Invoke IsInactiveAndDisallowActivation for the prerendered document.
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();
  EXPECT_EQ(prerendered_render_frame_host->lifecycle_state(),
            RenderFrameHostImpl::LifecycleStateImpl::kPrerendering);
  EXPECT_TRUE(prerendered_render_frame_host->IsInactiveAndDisallowActivation());

  // The prerender host for the URL should be destroyed as
  // RenderFrameHost::IsInactiveAndDisallowActivation cancels prerendering in
  // LifecycleStateImpl::kPrerendering state.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  // Cancelling the prerendering disables the activation. The navigation
  // should issue a request again.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kDestroyed, 1);
}

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

INSTANTIATE_TEST_SUITE_P(All,
                         PrerenderWithProactiveBrowsingInstanceSwap,
                         testing::Values(kWebContents, kMPArch),
                         ToString);

// Make sure that we can deal with the speculative RFH that is created during
// the activation navigation.
// TODO(https://crbug.com/1190197): We should try to avoid creating the
// speculative RFH (redirects allowing). Once that is done we should either
// change this test (if redirects allowed) or remove it completely.
IN_PROC_BROWSER_TEST_P(PrerenderWithProactiveBrowsingInstanceSwap,
                       LinkRelPrerender) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A prerender host for the URL should be registered.
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_NE(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  // Activate the prerendered page.
  // The test passes if we don't crash while cleaning up speculative render
  // frame host.
  NavigatePrimaryPage(kPrerenderingUrl);
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);

  // The prerender host should be consumed.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  // Activating the prerendered page should not issue a request.
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
}

}  // namespace
}  // namespace content
