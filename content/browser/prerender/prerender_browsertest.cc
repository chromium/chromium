// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_mojo_binder_policy_applier_unittest.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "url/gurl.h"

namespace content {
namespace {

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

  bool IsActivationDisabled() const { return IsMPArchActive(); }

  bool IsMPArchActive() const {
    switch (GetParam()) {
      case kWebContents:
        return false;
      case kMPArch:
        return true;
    }
  }

  void TestRenderFrameHostPrerenderingState(const GURL& prerender_url) {
    const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");

    // Navigate to an initial page.
    ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

    // The initial page should not be for prerendering.
    RenderFrameHostImpl* initiator_render_frame_host =
        static_cast<RenderFrameHostImpl*>(
            shell()->web_contents()->GetMainFrame());
    EXPECT_FALSE(initiator_render_frame_host->IsPrerendering());
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
      EXPECT_TRUE(rfhi->IsPrerendering());
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
      EXPECT_FALSE(rfhi->IsPrerendering());
    }
  }

 private:
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
  // Navigate to an initial page.
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Start prerendering a URL that causes cross-origin redirection.
  const GURL kRedirectedUrl = GetCrossOriginUrl("/empty.html");
  const GURL kPrerenderingUrl =
      GetUrl("/server-redirect?" + kRedirectedUrl.spec());
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  EXPECT_EQ(GetRequestCount(kRedirectedUrl), 1);

  // The prerender host should be registered for the initial request URL, not
  // the redirected URL.
  // TODO(https://crbug.com/1132746): Disallow cross-origin redirection on
  // prerendering navigation for the initial milestone.
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  EXPECT_TRUE(registry.FindHostByUrlForTesting(kPrerenderingUrl));
  EXPECT_FALSE(registry.FindHostByUrlForTesting(kRedirectedUrl));
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
  TestRenderFrameHostPrerenderingState(GetUrl("/page_with_iframe.html"));
}

// Blank <iframe> is a special case. Tests that the blank iframe knows the
// prerendering state as well.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, PrerenderBlankIframe) {
  TestRenderFrameHostPrerenderingState(GetUrl("/page_with_blank_iframe.html"));
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

  void RegisterMojoBinderPoliciesForPrerendering(
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
    EXPECT_TRUE(rfhi->IsPrerendering());

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

// Test that a cross-site navigation from prerendering browser context will
// load a new page and this page will later be activated on activating the
// prerendered page.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       PrerenderedPageCrossSiteNavigation) {
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

  EXPECT_TRUE(ExecJs(
      prerendered_render_frame_host,
      JsReplace("window.location.href = $1", kCrossSitePrerenderingUrl)));
  prerender_host->WaitForLoadStopForTesting();

  // The prerender host should be registered for the initial request URL, not
  // the navigated cross-site URL.
  EXPECT_TRUE(registry.FindHostByUrlForTesting(kPrerenderingUrl));
  EXPECT_FALSE(registry.FindHostByUrlForTesting(kCrossSitePrerenderingUrl));

  // Activate the prerendered page.
  NavigateWithLocation(kPrerenderingUrl);

  if (IsActivationDisabled()) {
    // Activation is disabled. The navigation should issue a request again
    // pointing to kPrerenderingUrl.
    EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);
  } else {
    // Activating the prerendered page should point to the navigated cross-site
    // URL without issuing a request again.
    EXPECT_EQ(shell()->web_contents()->GetURL(), kCrossSitePrerenderingUrl);
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  }

  // The prerender host should be consumed.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

// Test that a cross-site navigation with subframes from prerendering browser
// context will load a new page and this page will later be activated on
// activating the prerendered page.
//
// This test covers the same scenario has PrerenderedPageCrossSiteNavigation,
// except that the navigating URL has subframes. This shouldn't make any
// difference as prerendering is a page level concept.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest,
                       PrerenderedPageCrossSiteNavigationWithSubframes) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");
  const GURL kCrossSitePrerenderingUrl =
      GetCrossOriginUrl("/cross_site_iframe_factory.html?a(b)");

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

  // Navigate cross-site from the prerendered page.
  EXPECT_TRUE(ExecJs(
      prerendered_render_frame_host,
      JsReplace("window.location.href = $1", kCrossSitePrerenderingUrl)));
  prerender_host->WaitForLoadStopForTesting();

  // The prerender host should be registered for the initial request URL, not
  // the navigated cross-site URL.
  EXPECT_TRUE(registry.FindHostByUrlForTesting(kPrerenderingUrl));
  EXPECT_FALSE(registry.FindHostByUrlForTesting(kCrossSitePrerenderingUrl));

  // Activate the prerendered page.
  NavigateWithLocation(kPrerenderingUrl);

  if (IsActivationDisabled()) {
    // Activation is disabled. The navigation should issue a request again
    // pointing to kPrerenderingUrl.
    EXPECT_EQ(shell()->web_contents()->GetURL(), kPrerenderingUrl);
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);
  } else {
    // Activating the prerendered page should point to the navigated cross-site
    // URL without issuing a request again.
    EXPECT_EQ(shell()->web_contents()->GetURL(), kCrossSitePrerenderingUrl);
    EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);
  }

  // The prerender host should be consumed.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
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
  WebContents* prerender_contents = WebContents::FromRenderFrameHost(
      prerender_host->GetPrerenderedMainFrameHostForTesting());

  // Verify the prerendered page can read the object that the initial page
  // wrote.
  EXPECT_EQ(initial_value, EvalJs(prerender_contents,
                                  JsReplace("readData($1);", initial_key)));

  // The prerendered page writes another object to Indexed Database.
  EXPECT_EQ(true, EvalJs(prerender_contents,
                         JsReplace("addData($1, $2);", prerender_key,
                                   prerender_value)));

  // Read the added object from the initial page.
  EXPECT_EQ(prerender_value, EvalJs(shell()->web_contents(),
                                    JsReplace("readData($1);", prerender_key)));
}

// - End: Tests for Mojo capability control methodology restrictions ===========

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
