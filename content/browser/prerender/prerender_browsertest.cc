// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

class PrerenderBrowserTest : public ContentBrowserTest,
                             public testing::WithParamInterface<bool> {
 public:
  PrerenderBrowserTest() {
    std::map<std::string, std::string> parameters;
    if (IsActivationDisabled())
      parameters["activation"] = "disabled";
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kPrerender2, parameters);
  }
  ~PrerenderBrowserTest() override = default;

  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Make sure the feature param is correctly set before testing.
    if (IsActivationDisabled()) {
      ASSERT_EQ(blink::features::kPrerender2Param.Get(),
                blink::features::Prerender2ActivationMode::kDisabled);
    } else {
      ASSERT_EQ(blink::features::kPrerender2Param.Get(),
                blink::features::Prerender2ActivationMode::kEnabled);
    }

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

  // Adds <link rel=prerender> in the current main frame and waits until the
  // completion of prerendering.
  void AddPrerender(const GURL& prerendering_url) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // Start watching new web contents to be created for prerendering.
    content::TestNavigationObserver observer(prerendering_url);
    observer.StartWatchingNewWebContents();
    // Add the link tag that will prerender the URL.
    EXPECT_TRUE(ExecJs(shell()->web_contents(),
                       JsReplace("add_prerender($1)", prerendering_url)));
    observer.Wait();
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
    EXPECT_EQ(shell()->web_contents()->GetURL(), url);
  }

  GURL GetUrl(const std::string& path) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return ssl_server_.GetURL("a.test", path);
  }

  int GetRequestCount(const GURL& url) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::AutoLock auto_lock(lock_);
    return request_count_by_path_[url.PathForRequest()];
  }

  bool IsActivationDisabled() const { return GetParam(); }

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
                         /*disable_activation=*/testing::Bool());

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

  // Start watching new web contents to be created for prerendering.
  content::TestNavigationObserver navigation_observer1(kPrerenderingUrl1);
  content::TestNavigationObserver navigation_observer2(kPrerenderingUrl2);
  navigation_observer1.StartWatchingNewWebContents();
  navigation_observer2.StartWatchingNewWebContents();

  // Navigate to a page that initiates prerendering for `kPrerenderingUrl1`
  // twice. The second prerendering request should be ignored.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Wait until the completion of prerendering.
  navigation_observer1.Wait();
  navigation_observer2.Wait();

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

IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, InformedRenderFrameHost) {
  const GURL kInitialUrl = GetUrl("/prerender/add_prerender.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));
  ASSERT_EQ(shell()->web_contents()->GetURL(), kInitialUrl);

  // The initial page should not be for prerendering.
  RenderFrameHostImpl* initiator_render_frame_host =
      static_cast<RenderFrameHostImpl*>(
          shell()->web_contents()->GetMainFrame());
  EXPECT_FALSE(initiator_render_frame_host->IsPrerendering());

  // Add <link rel=prerender> that will prerender `kPrerenderingUrl`.
  ASSERT_EQ(GetRequestCount(kPrerenderingUrl), 0);
  AddPrerender(kPrerenderingUrl);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 1);

  // A prerender host for the URL should be registered.
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      registry.FindHostByUrlForTesting(kPrerenderingUrl);
  EXPECT_NE(prerender_host, nullptr);

  // Verify the corresponding RenderFrameHostImpl knows the prerendering state.
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHostForTesting();
  EXPECT_TRUE(prerendered_render_frame_host->IsPrerendering());

  // Activate the prerendered page.
  NavigateWithLocation(kPrerenderingUrl);
  if (IsActivationDisabled()) {
    // Activation is disabled, so the page should newly be rendered instead
    // of the prerendered page.
    RenderFrameHostImpl* new_render_frame_host =
        static_cast<RenderFrameHostImpl*>(
            shell()->web_contents()->GetMainFrame());
    EXPECT_NE(prerendered_render_frame_host, new_render_frame_host);
    // The new page shouldn't be in the prerendering state.
    EXPECT_FALSE(new_render_frame_host->IsPrerendering());
  } else {
    // The prerendered page is activated. The page should no longer be in
    // the prerendering state.
    ASSERT_EQ(prerendered_render_frame_host,
              shell()->web_contents()->GetMainFrame());
    EXPECT_FALSE(prerendered_render_frame_host->IsPrerendering());
  }
}

// Makes sure that activations on navigations for iframes don't happen.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, Activation_iFrame) {
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
  EXPECT_TRUE(prerender_host);

  // Attempt to activate the prerendered page for an iframe. This should fail
  // and fallback to network request.
  EXPECT_EQ("LOADED", EvalJs(shell()->web_contents(),
                             JsReplace("add_iframe($1)", kPrerenderingUrl)));

  // Activation shouldn't happen, so the prerender host should not be consumed,
  // and navigation for the iframe should issue a request again.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), prerender_host);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);
}

// Makes sure that activations on navigations for pop-up windows don't happen.
IN_PROC_BROWSER_TEST_P(PrerenderBrowserTest, Activation_PopUpWindow) {
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
  EXPECT_TRUE(prerender_host);

  // Attempt to activate the prerendered page for a pop-up window. This should
  // fail and fallback to network request.
  EXPECT_EQ("LOADED", EvalJs(shell()->web_contents(),
                             JsReplace("open_window($1)", kPrerenderingUrl)));

  // Activation shouldn't happen, so the prerender host should not be consumed,
  // and navigation for the pop-up window should issue a request again.
  EXPECT_EQ(registry.FindHostByUrlForTesting(kPrerenderingUrl), prerender_host);
  EXPECT_EQ(GetRequestCount(kPrerenderingUrl), 2);
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

// TODO(https://crbug.com/1132746): Test canceling prerendering.

// TODO(https://crbug.com/1132746): Test prerendering for 404 page, redirection,
// auth error, cross origin, etc.

}  // namespace
}  // namespace content
