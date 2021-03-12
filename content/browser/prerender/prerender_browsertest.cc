// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "content/browser/file_system_access/file_system_chooser_test_helpers.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_document_host_user_data.h"
#include "content/public/common/content_client.h"
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
  using LifecycleState = RenderFrameHostImpl::LifecycleState;

  PrerenderBrowserTest() {
    std::map<std::string, std::string> parameters;
    if (IsMPArchActive()) {
      parameters["implementation"] = "mparch";
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
  }

  PrerenderHostRegistry& GetPrerenderHostRegistry() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto* storage_partition = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext()));
    return *storage_partition->GetPrerenderHostRegistry();
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

  // Navigates to the URL and waits until the completion of navigation.
  //
  // Navigations that could activate a prerendered page on the multiple
  // WebContents architecture (not multiple-pages architecture known as MPArch)
  // should use this function instead of the NavigateToURL() test helper. This
  // is because the test helper accesses the predecessor WebContents to be
  // destroyed during activation and results in crashes.
  // See https://crbug.com/1154501 for the MPArch migration.
  void NavigateWithLocation(const GURL& url) {
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
    ignore_result(
        ExecJs(shell()->web_contents(), JsReplace("location = $1", url)));
    observer.Wait();
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

  bool IsActivationDisabled() const { return IsMPArchActive(); }

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

    // The initial page should not be for prerendering.
    RenderFrameHostImpl* initiator_render_frame_host =
        static_cast<RenderFrameHostImpl*>(
            shell()->web_contents()->GetMainFrame());
    EXPECT_FALSE(initiator_render_frame_host->frame_tree()->is_prerendering());
    EXPECT_NE(initiator_render_frame_host->lifecycle_state(),
              LifecycleState::kPrerendering);

    // Start a prerender.
    AddPrerender(prerender_url);
    PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
    PrerenderHost* prerender_host =
        registry.FindHostByUrlForTesting(prerender_url);

    // Verify all RenderFrameHostImpl in the prerendered page know the
    // prerendering state.
    RenderFrameHostImpl* prerendered_render_frame_host =
        prerender_host->GetPrerenderedMainFrameHostForTesting();
    std::vector<RenderFrameHost*> frames =
        prerendered_render_frame_host->GetFramesInSubtree();
    for (auto* frame : frames) {
      auto* rfhi = static_cast<RenderFrameHostImpl*>(frame);
      // All the subframes should be in LifecycleState::kPrerendering state
      // before activation.
      EXPECT_EQ(rfhi->lifecycle_state(),
                RenderFrameHostImpl::LifecycleState::kPrerendering);
      EXPECT_TRUE(rfhi->frame_tree()->is_prerendering());
    }

    // Activate the prerendered page.
    NavigateWithLocation(prerender_url);
    EXPECT_EQ(shell()->web_contents()->GetURL(), prerender_url);

    // The activated page should no longer be in the prerendering state.
    RenderFrameHostImpl* navigated_render_frame_host =
        static_cast<RenderFrameHostImpl*>(
            shell()->web_contents()->GetMainFrame());
    // The new page shouldn't be in the prerendering state.
    frames = navigated_render_frame_host->GetFramesInSubtree();
    for (auto* frame : frames) {
      auto* rfhi = static_cast<RenderFrameHostImpl*>(frame);
      // All the subframes should be transitioned to LifecycleState::kActive
      // state after activation.
      EXPECT_EQ(rfhi->lifecycle_state(),
                RenderFrameHostImpl::LifecycleState::kActive);
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
  NavigateWithLocation(kPrerenderingUrl);
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);

  // The prerender host should be consumed.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  if (IsActivationDisabled()) {
    // Activation is disabled. The navigation should issue a request again.
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);
  } else {
    // Activating the prerendered page should not issue a request.
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  }
}

IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, LinkRelPrerender_Multiple) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl1 = GetUrl("/empty.html?1");
  const GURL kPrerenderingUrl2 = GetUrl("/empty.html?2");

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

  // Activate the prerendered page.
  NavigateWithLocation(kPrerenderingUrl2);
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl2);

  // The prerender hosts should be consumed or destroyed for activation.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);

  if (IsActivationDisabled()) {
    // Activation is disabled. The navigation should issue a request again.
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl1), 1);
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl2), 2);
  } else {
    // Activating the prerendered page should not issue a request.
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl1), 1);
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl2), 1);
  }
}

IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, LinkRelPrerender_Duplicate) {
  const GURL kInitialUrl = GetUrl("/prerender/duplicate_prerenders.html");
  const GURL kPrerenderingUrl1 = GetUrl("/empty.html?1");
  const GURL kPrerenderingUrl2 = GetUrl("/empty.html?2");

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

  // Activate the prerendered page.
  NavigateWithLocation(kPrerenderingUrl1);
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl1);

  // The prerender hosts should be consumed or destroyed for activation.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);

  if (IsActivationDisabled()) {
    // Activation is disabled. The navigation should issue a request again.
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl1), 2);
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl2), 1);
  } else {
    // Activating the prerendered page should not issue a request.
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl1), 1);
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl2), 1);
  }
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

  // Start prerendering a URL that causes cross-origin redirection.
  const GURL kRedirectedUrl = GetCrossOriginUrl("/empty.html");
  const GURL kPrerenderingUrl =
      GetUrl("/server-redirect?" + kRedirectedUrl.spec());
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // Cross-origin redirection should fail prerendering.
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 0);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_FALSE(registry.FindHostByUrlForTesting(kPrerenderingUrl));
  EXPECT_FALSE(registry.FindHostByUrlForTesting(kRedirectedUrl));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kCrossOriginRedirect, 1);
}

// TODO(https://crbug.com/1158248): Add tests for activation with a redirected
// URL.

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
// TODO(crbug.com/1186209): Add redirect test cases.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       DeferCrossOriginSubframeNavigation) {
  // TODO(toyoshim, bokan): Enable this test with MPArch.
  // It seems NavigationThrottles are not constructed for iframe navigation
  // under MPArch environment. It needs some investigation to enable this with
  // MPArch.
  if (IsMPArchActive())
    return;

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
      prerender_host->GetPrerenderedMainFrameHostForTesting();
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
  NavigateWithLocation(kPrerenderingUrl);
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
  // TODO(toyoshim): Enable the following EXPECT_EQs once the relevant bug is
  // fixed. Currently, deferred frame creates a document after the activation,
  // but with is_prerendering being true due to an existing bug.
  //EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
  //                        kInitialDocumentPrerenderingScript));
  //EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
  //                        kCurrentDocumentPrerenderingScript));
  EXPECT_EQ(false, EvalJs(cross_origin_render_frame_host,
                          kOnprerenderingchangeObservedScript));
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
  NavigateWithLocation(kPrerenderingUrl);
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);

  // Activation shouldn't happen, so the prerender host should not be consumed.
  // However, we don't check the existence of the prerender host here unlike
  // other activation tests because navigating the frame that triggered
  // prerendering abandons the prerendered page regardless of activation.
}

// Tests that back-forward history is preserved after activation.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, HistoryAfterActivation) {
  // This test is only meaningful with activation.
  if (IsActivationDisabled())
    return;

  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make and activate a prerendered page.
  AddPrerender(kPrerenderingUrl);
  NavigateWithLocation(kPrerenderingUrl);
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
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, PrerenderBlankIframe) {
  TestHostPrerenderingState(GetUrl("/page_with_blank_iframe.html"));
}

class MojoCapabilityControlTestContentBrowserClient
    : public TestContentBrowserClient,
      mojom::TestInterfaceForDefer,
      mojom::TestInterfaceForGrant,
      mojom::TestInterfaceForCancel {
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
  }

  void RegisterMojoBinderPoliciesForSameOriginPrerendering(
      MojoBinderPolicyMap& policy_map) override {
    policy_map.SetPolicy<mojom::TestInterfaceForGrant>(
        MojoBinderPolicy::kGrant);
    policy_map.SetPolicy<mojom::TestInterfaceForCancel>(
        MojoBinderPolicy::kCancel);
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

  size_t GetDeferReceiverSetSize() { return defer_receiver_set_.size(); }

  size_t GetGrantReceiverSetSize() { return grant_receiver_set_.size(); }

 private:
  mojo::ReceiverSet<mojom::TestInterfaceForDefer> defer_receiver_set_;
  mojo::ReceiverSet<mojom::TestInterfaceForGrant> grant_receiver_set_;
  mojo::Receiver<mojom::TestInterfaceForCancel> cancel_receiver_{this};
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
      prerender_host->GetPrerenderedMainFrameHostForTesting();
  std::vector<RenderFrameHost*> frames =
      prerendered_render_frame_host->GetFramesInSubtree();

  mojo::RemoteSet<mojom::TestInterfaceForDefer> defer_remote_set;
  mojo::RemoteSet<mojom::TestInterfaceForGrant> grant_remote_set;
  for (auto* frame : frames) {
    auto* rfhi = static_cast<RenderFrameHostImpl*>(frame);
    EXPECT_TRUE(rfhi->frame_tree()->is_prerendering());
    EXPECT_EQ(rfhi->lifecycle_state(), LifecycleState::kPrerendering);

    mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
        rfhi->browser_interface_broker_receiver_for_testing();
    blink::mojom::BrowserInterfaceBroker* prerender_broker =
        bib.internal_state()->impl();
    // Try to bind a kDefer interface.
    mojo::Remote<mojom::TestInterfaceForDefer> prerender_defer_remote;
    prerender_broker->GetInterface(
        prerender_defer_remote.BindNewPipeAndPassReceiver());
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

  // The rest of this test is only meaningful with activation.
  if (IsActivationDisabled()) {
    SetBrowserClientForTesting(old_browser_client);
    return;
  }

  // Activate the prerendered page.
  NavigateWithLocation(kPrerenderingUrl);
  EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);
  EXPECT_EQ(test_browser_client.GetDeferReceiverSetSize(), frames.size());

  SetBrowserClientForTesting(old_browser_client);
}

// Tests that mojo capability control will cancel prerendering if the main frame
// receives a request for a kCancel interface.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       MojoCapabilityControl_CancelMainFrame) {
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
      prerender_host->GetPrerenderedMainFrameHostForTesting();
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

  SetBrowserClientForTesting(old_browser_client);
}

// Tests that mojo capability control will cancel prerendering if child frames
// receive a request for a kCancel interface.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       MojoCapabilityControl_CancelIframe) {
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
  RenderFrameHostImpl* main_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHostForTesting();
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
}

// TODO(https://crbug.com/1132746): Test canceling prerendering when its
// initiator is no longer interested in prerending this page.

// TODO(https://crbug.com/1132746): Test prerendering for 404 page, auth error,
// cross origin, etc.

// Tests for feature restrictions in prerendered pages =========================

// - Tests for feature-specific code methodology restrictions ==================

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
      prerender_host->GetPrerenderedMainFrameHostForTesting();

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

// Tests that same-origin prerendering pages have the access to Broadcast
// Channel API.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, GrantBroadcastChannel) {
  const GURL kInitialUrl =
      GetUrl("/prerender/restriction_broadcast_channel.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/restriction_broadcast_channel.html?prerendering");
  const std::string initial_message =
      "This is a message sent from the initial page";
  const std::string prerender_message =
      "This is a message sent from the prerendering page.";

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a same-origin prerendering page.
  AddPrerender(kPrerenderingUrl);

  // Send a message to the channel from the initial page.
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("bc.postMessage($1);", initial_message)));

  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHostForTesting();
  ASSERT_TRUE(prerender_host);

  // Check the prerendering page received the message sent by the initial page.
  EXPECT_EQ(initial_message,
            EvalJs(prerendered_render_frame_host, "messageReceived;"));

  // Send a message to the channel from the prerendering page.
  EXPECT_TRUE(ExecJs(prerendered_render_frame_host,
                     JsReplace("bc.postMessage($1);", prerender_message)));

  // Check the initial page received the message sent by the prerendering page.
  EXPECT_EQ(prerender_message,
            EvalJs(shell()->web_contents(), "messageReceived;"));

  // Disconnect from the channel.
  EXPECT_TRUE(ExecJs(shell()->web_contents(), "bc.close();"));
  EXPECT_TRUE(ExecJs(prerendered_render_frame_host, "bc.close();"));
}

// - End: Tests for feature-specific code methodology restrictions =============

// - Tests for Mojo capability control methodology restrictions ================

// Tests that prerendering pages can access cookies.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, CookieAccess) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  // Set a cookie to the origin.
  const std::string initial_cookie = "initial_cookie=exist";
  const std::string prerender_cookie = "prerender_cookie=exist";
  EvalJsResult result =
      EvalJs(shell()->web_contents(),
             "document.cookie='" + initial_cookie + "; path=/'");
  EXPECT_TRUE(result.error.empty()) << result.error;

  // Make a prerendered page.
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHostForTesting();

  // Verify the prerendered page can read the cookie.
  EXPECT_EQ(initial_cookie,
            EvalJs(prerendered_render_frame_host, "document.cookie"));

  // Verify the prerendered page can update cookies.
  EvalJsResult prerender_result =
      EvalJs(prerendered_render_frame_host,
             "document.cookie='" + prerender_cookie + "; path=/'");
  EXPECT_TRUE(prerender_result.error.empty()) << prerender_result.error;
  // Read the updated cookie from the initial page.
  EXPECT_EQ(initial_cookie + "; " + prerender_cookie,
            EvalJs(shell()->web_contents(), "document.cookie"));
}

// TODO(crbug.com/1186584) Test is flaky.
// Test that a cross-site navigation from prerendering browser context will
// cancel prerendering.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       DISABLED_PrerenderedPageCrossSiteNavigation) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  const GURL kCrossSitePrerenderingUrl = GetCrossOriginUrl("/title1.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHostForTesting();

  // Run cross-site navigation from the prerendering browser context.
  EXPECT_TRUE(ExecJs(
      prerendered_render_frame_host,
      JsReplace("window.location.href = $1", kCrossSitePrerenderingUrl)));

  // The cross-site navigation should cancel prerendering.
  EXPECT_FALSE(registry.FindHostByUrlForTesting(kPrerenderingUrl));
  EXPECT_FALSE(registry.FindHostByUrlForTesting(kCrossSitePrerenderingUrl));
  EXPECT_EQ(GetRequestCount(kCrossSitePrerenderingUrl), 0);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus",
      PrerenderHost::FinalStatus::kCrossOriginNavigation, 1);
}

// Test that a same-site navigation from prerendering browser context will
// load a new page and this page will later be activated on activating the
// prerendered page.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       PrerenderedPageSameSiteNavigation) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  const GURL kSameSitePrerenderingUrl = GetUrl("/title1.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHostForTesting();

  // Navigate same-site from the prerendered page.
  EXPECT_TRUE(
      ExecJs(prerendered_render_frame_host,
             JsReplace("window.location.href = $1", kSameSitePrerenderingUrl)));
  prerender_host->WaitForLoadStopForTesting();

  // The prerender host should be registered for the initial request URL, not
  // the navigated same-site URL.
  EXPECT_TRUE(registry.FindHostByUrlForTesting(kPrerenderingUrl));
  EXPECT_FALSE(registry.FindHostByUrlForTesting(kSameSitePrerenderingUrl));

  // Activate the prerendered page.
  NavigateWithLocation(kPrerenderingUrl);
  if (IsActivationDisabled()) {
    // Activation is disabled. The navigation should issue a request again
    // pointing to kPrerenderingUrl.
    EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);
  } else {
    // Activating the prerendered page should point to the navigated same-site
    // URL without issuing a request again.
    EXPECT_EQ(shell()->web_contents()->GetURL(), kSameSitePrerenderingUrl);
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  }

  // The prerender host should be consumed.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, RenderFrameHostLifecycleState) {
  // This test is only meaningful with activation.
  if (IsActivationDisabled())
    return;

  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl1 = GetUrl("/prerender/add_prerender.html?1");
  const GURL kPrerenderingUrl2 = GetUrl("/prerender/add_prerender.html?2");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  EXPECT_EQ(current_frame_host()->lifecycle_state(), LifecycleState::kActive);

  // Start a prerender.
  AddPrerender(kPrerenderingUrl1);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl1);
  ASSERT_TRUE(prerender_host);

  // Open an iframe in the prerendered page.
  RenderFrameHostImpl* rfh_a =
      prerender_host->GetPrerenderedMainFrameHostForTesting();
  EXPECT_EQ("LOADED",
            EvalJs(rfh_a, JsReplace("add_iframe($1)", GetUrl("/empty.html"))));
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  // Both rfh_a and rfh_b lifecycle state's should be kPrerendering.
  EXPECT_EQ(LifecycleState::kPrerendering, rfh_a->lifecycle_state());
  EXPECT_EQ(LifecycleState::kPrerendering, rfh_b->lifecycle_state());

  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHostForTesting();

  // Navigate same-origin from the prerendered page.
  EXPECT_TRUE(
      ExecJs(prerendered_render_frame_host,
             JsReplace("window.location.href = $1", kPrerenderingUrl2)));
  prerender_host->WaitForLoadStopForTesting();

  // Open an iframe in the new prerendered page.
  RenderFrameHostImpl* rfh_c =
      prerender_host->GetPrerenderedMainFrameHostForTesting();
  EXPECT_EQ("LOADED",
            EvalJs(rfh_c, JsReplace("add_iframe($1)", GetUrl("/empty.html"))));
  RenderFrameHostImpl* rfh_d = rfh_c->child_at(0)->current_frame_host();

  // Both rfh_c and rfh_d lifecycle state's should be kPrerendering.
  EXPECT_EQ(LifecycleState::kPrerendering, rfh_c->lifecycle_state());
  EXPECT_EQ(LifecycleState::kPrerendering, rfh_d->lifecycle_state());

  // Activate the prerendered page.
  NavigateWithLocation(kPrerenderingUrl1);

  // Both rfh_c and rfh_d lifecycle state's should be kActive after activation.
  EXPECT_EQ(LifecycleState::kActive, rfh_c->lifecycle_state());
  EXPECT_EQ(LifecycleState::kActive, rfh_d->lifecycle_state());
}

// Tests that prerendering pages can access local storage.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, LocalStorageAccess) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  // Add an item to local storage from the initial page.
  const std::string key = "set_by";
  const std::string initial_value = "initial";
  const std::string prerender_value = "prerender";
  EvalJsResult result = EvalJs(
      shell()->web_contents(),
      JsReplace("window.localStorage.setItem($1, $2)", key, initial_value));
  EXPECT_TRUE(result.error.empty()) << result.error;

  // Make a prerendered page.
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHostForTesting();

  // Verify the prerendered page can read the item that the initial page wrote.
  EXPECT_EQ(initial_value,
            EvalJs(prerendered_render_frame_host,
                   JsReplace("window.localStorage.getItem($1)", key)));

  // Verify the prerendered page can update local storage.
  EvalJsResult prerender_result = EvalJs(
      prerendered_render_frame_host,
      JsReplace("window.localStorage.setItem($1, $2)", key, prerender_value));
  EXPECT_TRUE(prerender_result.error.empty()) << prerender_result.error;
  // Read the updated item value from the initial page.
  EXPECT_EQ(prerender_value,
            EvalJs(shell()->web_contents(),
                   JsReplace("window.localStorage.getItem($1)", key)));
}

// Tests that prerendering pages can access Indexed Database.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, IndexedDBAccess) {
  const GURL kInitialUrl = GetUrl("/prerender/restriction_indexeddb.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/restriction_indexeddb.html?prerendering");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  const std::string initial_key = "initial";
  const std::string initial_value = initial_key + "_set";
  const std::string prerender_key = "prerender";
  const std::string prerender_value = prerender_key + "_set";

  // Write an object to Indexed Database.
  EXPECT_EQ(true,
            EvalJs(shell()->web_contents(),
                   JsReplace("addData($1, $2);", initial_key, initial_value)));

  // Make a prerendered page.
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  RenderFrameHostImpl* prerender_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHostForTesting();

  // Verify the prerendered page can read the object that the initial page
  // wrote.
  EXPECT_EQ(initial_value, EvalJs(prerender_render_frame_host,
                                  JsReplace("readData($1);", initial_key)));

  // The prerendered page writes another object to Indexed Database.
  EXPECT_EQ(true, EvalJs(prerender_render_frame_host,
                         JsReplace("addData($1, $2);", prerender_key,
                                   prerender_value)));

  // Read the added object from the initial page.
  EXPECT_EQ(prerender_value, EvalJs(shell()->web_contents(),
                                    JsReplace("readData($1);", prerender_key)));
}

// Tests that prerendering is gated behind CSP:prefetch-src
// TODO(https://crbug.com/1185679) This is currently not the case. Fix this.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, CSPPrefetchSrc) {
  GURL initial_url = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Add CSP:prefetch-src */empty.html
  EXPECT_TRUE(ExecJs(current_frame_host(), R"(
    const meta = document.createElement('meta');
    meta.httpEquiv = "Content-Security-Policy";
    meta.content = "prefetch-src */empty.html";
    document.getElementsByTagName('head')[0].appendChild(meta);
  )"));

  const char* kConsolePattern =
      "Refused to prefetch content from "
      "'https://a.test:*/prerender/add_prerender.html' because it violates the "
      "following Content Security Policy directive: \"prefetch-src "
      "*/empty.html\"*";

  // Check what happens when a prerendering is blocked:
  {
    GURL disallowed_url = GetUrl("/title1.html");
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);
    PrerenderHostRegistryObserver observer(GetPrerenderHostRegistry());
    EXPECT_TRUE(ExecJs(shell()->web_contents(),
                       JsReplace("add_prerender($1)", disallowed_url)));
    observer.WaitForTrigger(disallowed_url);
    // TODO(https://crbug.com/1185679): This should be false:
    EXPECT_TRUE(
        GetPrerenderHostRegistry().FindHostByUrlForTesting(disallowed_url));
    // TODO(https://crbug.com/1185679): This should be 1.
    EXPECT_EQ(0u, console_observer.messages().size());
  }

  // Check what happens when prerendering isn't blocked.
  {
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);
    AddPrerender(GetUrl("/empty.html"));
    EXPECT_EQ(0u, console_observer.messages().size());
  }
}

// Tests that prerendering is gated behind CSP:default-src
// TODO(https://crbug.com/1185679) This is currently not the case. Fix this.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, CSPDefaultSrc) {
  GURL initial_url = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Add CSP:prefetch-src */empty.html
  EXPECT_TRUE(ExecJs(current_frame_host(), R"(
    const meta = document.createElement('meta');
    meta.httpEquiv = "Content-Security-Policy";
    meta.content = "default-src */empty.html; script-src 'unsafe-eval'";
    document.getElementsByTagName('head')[0].appendChild(meta);
  )"));

  const char* kConsolePattern =
      "Refused to prefetch content from "
      "'https://a.test:*/prerender/add_prerender.html' because it violates the "
      "following Content Security Policy directive: \"default-src "
      "*/empty.html\"*";

  // Check what happens when a prerendering is blocked:
  {
    GURL disallowed_url = GetUrl("/title1.html");
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);
    PrerenderHostRegistryObserver observer(GetPrerenderHostRegistry());
    EXPECT_TRUE(ExecJs(shell()->web_contents(),
                       JsReplace("add_prerender($1)", disallowed_url)));
    observer.WaitForTrigger(disallowed_url);
    // TODO(https://crbug.com/1185679): This should be false:
    EXPECT_TRUE(
        GetPrerenderHostRegistry().FindHostByUrlForTesting(disallowed_url));
    // TODO(https://crbug.com/1185679): This should be 1.
    EXPECT_EQ(0u, console_observer.messages().size());
  }

  // Check what happens when prerendering isn't blocked.
  {
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);
    AddPrerender(GetUrl("/empty.html"));
    EXPECT_EQ(0u, console_observer.messages().size());
  }
}

class PrerenderFileSystemAccessBrowserTest : public PrerenderBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ContentBrowserTest::SetUp();
  }

 protected:
  base::ScopedTempDir temp_dir_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrerenderFileSystemAccessBrowserTest,
                         testing::Values(kWebContents, kMPArch),
                         ToString);

// TODO(https://crbug.com/1182032): Now File System Access API is not supported
// on Android. Enable this browser test after https://crbug.com/1011535 is
// fixed.
#if defined(OS_ANDROID)
#define MAYBE_DeferFileSystemAccess DISABLED_DeferFileSystemAccess
#else
#define MAYBE_DeferFileSystemAccess DeferFileSystemAccess
#endif

// Tests that access to local file system is deferred on prerendering pages.
IN_PROC_BROWSER_TEST_P(PrerenderFileSystemAccessBrowserTest,
                       MAYBE_DeferFileSystemAccess) {
  // This test is only meaningful with activation.
  if (IsActivationDisabled())
    return;
  base::FilePath temp_file;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file));
  }
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({temp_file}, &dialog_params));

  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl =
      GetUrl("/prerender/restriction_file_system.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  RenderFrameHostImpl* prerender_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHostForTesting();

  // Access `temp_file` on the prerendered page.
  ExecuteScriptAsync(prerender_render_frame_host, "startShowOpenFilePicker();");
  // Run a event loop so the page can fail the test.
  EXPECT_TRUE(ExecJs(prerender_render_frame_host, "runLoop();"));

  // Inform the prerendered page that it will be activated and activate it.
  EXPECT_TRUE(ExecJs(prerender_render_frame_host, "setWillActivate();"));
  NavigateWithLocation(kPrerenderingUrl);

  // `temp_file` should be selected after `willActivate` was set to true,
  // otherwise the prerendered page will throw an error.
  EXPECT_EQ(temp_file.BaseName().AsUTF8Unsafe(),
            EvalJs(prerender_render_frame_host, "result;"));

  ui::SelectFileDialog::SetFactory(nullptr);
}

// Tests that RenderDocumentHostUserData object is not cleared on activating a
// prerendered page.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, RenderDocumentHostUserData) {
  // This test is only meaningful with activation.
  if (IsActivationDisabled())
    return;

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
      prerender_host->GetPrerenderedMainFrameHostForTesting();

  // Get the DocumentData associated with prerender RenderFrameHost.
  DocumentData::CreateForCurrentDocument(prerendered_render_frame_host);
  base::WeakPtr<DocumentData> data =
      DocumentData::GetForCurrentDocument(prerendered_render_frame_host)
          ->GetWeakPtr();
  EXPECT_TRUE(data);

  // Activate the prerendered page.
  NavigateWithLocation(kPrerenderingUrl);
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

// - End: Tests for Mojo capability control methodology restrictions ===========

// Tests that prerendering pages cannot access Clipboard.
// This cannot be upstreamed as a WPT test because the spec (probably) will
// require that no error is thrown until activation.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, ClipboardAccessError) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  AddPrerender(kPrerenderingUrl);

  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHostForTesting();

  // Accessing Clipboard on prerendering pages should fail because the
  // prerendering documents are not focused.
  // (https://w3c.github.io/clipboard-apis/#privacy-async)
  auto result = EvalJs(prerendered_render_frame_host,
                       "navigator.clipboard.writeText(location.href);");
  EXPECT_THAT(result.error, ::testing::HasSubstr(
                                "NotAllowedError: Document is not focused."));
}

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
      prerender_host->GetPrerenderedMainFrameHostForTesting();
  EXPECT_EQ(prerendered_render_frame_host->lifecycle_state(),
            RenderFrameHostImpl::LifecycleState::kPrerendering);
  EXPECT_TRUE(prerendered_render_frame_host->IsInactiveAndDisallowActivation());

  // The prerender host for the URL should be destroyed as
  // RenderFrameHost::IsInactiveAndDisallowActivation cancels prerendering in
  // LifecycleState::kPrerendering state.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  // Cancelling the prerendering disables the activation. The navigation
  // should issue a request again.
  NavigateWithLocation(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);
}

class PrerenderWithBackForwardCacheTest : public PrerenderBrowserTest {
 public:
  PrerenderWithBackForwardCacheTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kBackForwardCache,
                               {{"TimeToLiveInBackForwardCacheInSeconds",
                                 "3600"},  // Prevent evictions for long running
                                           // tests.
                                {"enable_same_site", "true"}}}},
        // Allow BackForwardCache for all devices regardless of their memory.
        /*disabled_features=*/{features::kBackForwardCacheMemoryControls});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrerenderWithBackForwardCacheTest,
                         testing::Values(kWebContents, kMPArch),
                         ToString);

// Make sure that when a navigation commits in the prerenderer frame tree, the
// old page is not stored in back/forward cache and the appropriate reason is
// recorded in the metrics.
IN_PROC_BROWSER_TEST_P(PrerenderWithBackForwardCacheTest,
                       BackForwardCacheDisabled) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  const GURL kSameSitePrerenderingUrl = GetUrl("/title1.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start a prerender.
  AddPrerender(kPrerenderingUrl);

  PrerenderHost* prerender_host =
      GetPrerenderHostRegistry().FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHostForTesting();

  // Navigate the Prerender page to a new URL.
  EXPECT_TRUE(
      ExecJs(prerendered_render_frame_host,
             JsReplace("window.location.href = $1", kSameSitePrerenderingUrl)));
  prerender_host->WaitForLoadStopForTesting();

  prerender_host =
      GetPrerenderHostRegistry().FindHostByUrlForTesting(kPrerenderingUrl);
  ASSERT_TRUE(prerender_host);
  prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHostForTesting();

  // Go back. The page should not be restored from the bfcache.
  EXPECT_TRUE(ExecJs(prerendered_render_frame_host, "history.back();"));
  prerender_host->WaitForLoadStopForTesting();

  // Make sure that page is not cacheable for the right reason.
  histogram_tester.ExpectBucketCount(
      "BackForwardCache.HistoryNavigationOutcome."
      "NotRestoredReason",
      base::HistogramBase::Sample(BackForwardCacheMetrics::NotRestoredReason::
                                      kBackForwardCacheDisabledForPrerender),
      1);
}

}  // namespace
}  // namespace content
