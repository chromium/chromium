// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PRERENDER_TEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_PRERENDER_TEST_UTIL_H_

#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"
#include "url/gurl.h"

namespace content {

namespace test {

class PrerenderHostRegistryObserverImpl;

// The PrerenderHostRegistryObserver permits waiting for a host to be created
// for a given URL.
class PrerenderHostRegistryObserver {
 public:
  explicit PrerenderHostRegistryObserver(WebContents& web_contents);
  ~PrerenderHostRegistryObserver();
  PrerenderHostRegistryObserver(const PrerenderHostRegistryObserver&) = delete;
  PrerenderHostRegistryObserver& operator=(
      const PrerenderHostRegistryObserver&) = delete;

  // Returns immediately if |gurl| was ever triggered before. Otherwise blocks
  // on a RunLoop until a prerender of |gurl| is triggered.
  void WaitForTrigger(const GURL& gurl);

  // Blocks on a RunLoop until a next prerender is triggered. Returns a URL of
  // the prerender.
  GURL WaitForNextTrigger();

  // Invokes |callback| immediately if |gurl| was ever triggered before.
  // Otherwise invokes |callback| when a prerender for |gurl| is triggered.
  void NotifyOnTrigger(const GURL& gurl, base::OnceClosure callback);

  // Returns a set of URLs that have been triggered so far.
  base::flat_set<GURL> GetTriggeredUrls() const;

 private:
  std::unique_ptr<PrerenderHostRegistryObserverImpl> impl_;
};

class PrerenderHostObserverImpl;

// The PrerenderHostObserver permits listening for host activation and
// destruction
class PrerenderHostObserver {
 public:
  // Begins observing the given PrerenderHost immediately. DCHECKs if |host_id|
  // does not identify a live PrerenderHost.
  PrerenderHostObserver(WebContents& web_contents, FrameTreeNodeId host_id);

  // Will start observing a PrerenderHost for |gurl| as soon as it is
  // triggered.
  PrerenderHostObserver(WebContents& web_contents, const GURL& gurl);

  ~PrerenderHostObserver();
  PrerenderHostObserver(const PrerenderHostObserver&) = delete;
  PrerenderHostObserver& operator=(const PrerenderHostObserver&) = delete;

  // Returns immediately if the PrerenderHost was already activated, otherwise
  // spins a RunLoop until the observed host is activated.
  void WaitForActivation();

  // Returns immediately if the PrerenderHost has already received headers,
  // otherwise spins a RunLoop until the observed host receives headers.
  void WaitForHeaders();

  // Returns immediately if the PrerenderHost was already destroyed, otherwise
  // spins a RunLoop until the observed host is destroyed.
  void WaitForDestroyed();

  // True if the PrerenderHost was activated to be the primary page.
  bool was_activated() const;

 private:
  std::unique_ptr<PrerenderHostObserverImpl> impl_;
};

// This waits for creation of PrerenderHost and then returns its host id.
class PrerenderHostCreationWaiter {
 public:
  PrerenderHostCreationWaiter();
  ~PrerenderHostCreationWaiter() = default;

  FrameTreeNodeId Wait();

 private:
  base::RunLoop run_loop_;
  FrameTreeNodeId created_host_id_;
};

// Enables appropriate features for Prerender2.
// This also disables the memory requirement of Prerender2 on Android so that
// test can run on any bot.
class ScopedPrerenderFeatureList {
 public:
  ScopedPrerenderFeatureList();
  ScopedPrerenderFeatureList(const ScopedPrerenderFeatureList&) = delete;
  ScopedPrerenderFeatureList& operator=(const ScopedPrerenderFeatureList&) =
      delete;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Browser tests can use this class to more conveniently leverage prerendering.
class PrerenderTestHelper {
 public:
  explicit PrerenderTestHelper(const WebContents::Getter& fn);
  ~PrerenderTestHelper();
  PrerenderTestHelper(const PrerenderTestHelper&) = delete;
  PrerenderTestHelper& operator=(const PrerenderTestHelper&) = delete;

  // This installs a network monitor on the http server. Be sure to call this
  // before starting the server. This is typically done from SetUp, but it is
  // fine to call from SetUpOnMainThread if ordering constraints make that
  // impossible (eg, if the test helper is created later to avoid problematic
  // creation/destruction relative to other ScopedFeatureLists or if the fixture
  // creates test server after SetUp).
  void RegisterServerRequestMonitor(
      net::test_server::EmbeddedTestServer* http_server);
  void RegisterServerRequestMonitor(
      net::test_server::EmbeddedTestServer& test_server);

  // Attempts to lookup the host for the given |gurl|. Returns an invalid frame
  // id upon failure.
  static FrameTreeNodeId GetHostForUrl(WebContents& web_contents,
                                       const GURL& gurl);
  FrameTreeNodeId GetHostForUrl(const GURL& gurl);

  // Returns whether the registry holds the handler for prerender-into-new-tab.
  bool HasNewTabHandle(FrameTreeNodeId host_id);

  // Waits until a prerender has finished loading. Note: this may not be called
  // when the load fails (e.g. because it was blocked by a NavigationThrottle,
  // or the WebContents is destroyed). If the prerender doesn't yet exist, this
  // will wait until it is triggered.
  static void WaitForPrerenderLoadCompletion(WebContents& web_contents,
                                             const GURL& gurl);
  void WaitForPrerenderLoadCompletion(const GURL& gurl);
  void WaitForPrerenderLoadCompletion(FrameTreeNodeId host_id);

  // Adds <script type="speculationrules"> in the current main frame and waits
  // until the completion of prerendering. Returns the id of the resulting
  // prerendering host.
  FrameTreeNodeId AddPrerender(const GURL& prerendering_url,
                               int32_t world_id = ISOLATED_WORLD_ID_GLOBAL);
  FrameTreeNodeId AddPrerender(
      const GURL& prerendering_url,
      std::optional<blink::mojom::SpeculationEagerness> eagerness,
      const std::string& target_hint,
      int32_t world_id = ISOLATED_WORLD_ID_GLOBAL);
  FrameTreeNodeId AddPrerender(
      const GURL& prerendering_url,
      std::optional<blink::mojom::SpeculationEagerness> eagerness,
      std::optional<std::string> no_vary_search_hint,
      const std::string& target_hint,
      int32_t world_id = ISOLATED_WORLD_ID_GLOBAL);
  // AddPrerenderAsync() is the same as AddPrerender(), but does not wait until
  // the completion of prerendering.
  void AddPrerenderAsync(const GURL& prerendering_url,
                         int32_t world_id = ISOLATED_WORLD_ID_GLOBAL);
  void AddPrerendersAsync(
      const std::vector<GURL>& prerendering_urls,
      std::optional<blink::mojom::SpeculationEagerness> eagerness,
      const std::string& target_hint,
      int32_t world_id = ISOLATED_WORLD_ID_GLOBAL);
  void AddPrerendersAsync(
      const std::vector<GURL>& prerendering_urls,
      std::optional<blink::mojom::SpeculationEagerness> eagerness,
      std::optional<std::string> no_vary_search_hint,
      const std::string& target_hint,
      int32_t world_id = ISOLATED_WORLD_ID_GLOBAL);

  void AddPrefetchAsync(const GURL& prefetch_url);

  // Starts prerendering and returns a PrerenderHandle that should be kept alive
  // until prerender activation. Note that it returns before the completion of
  // the prerendering navigation.
  std::unique_ptr<PrerenderHandle> AddEmbedderTriggeredPrerenderAsync(
      const GURL& prerendering_url,
      PreloadingTriggerType trigger_type,
      const std::string& embedder_histogram_suffix,
      ui::PageTransition page_transition);

  // This navigates, but does not activate, the prerendered page.
  void NavigatePrerenderedPage(FrameTreeNodeId host_id, const GURL& gurl);

  // This cancels the prerendered page.
  void CancelPrerenderedPage(FrameTreeNodeId host_id);

  // Navigates the primary page to the URL and waits until the completion of
  // the navigation.
  //
  // Navigations that could activate a prerendered page should use this function
  // instead of the NavigateToURL() test helper. This is because the test helper
  // could access a navigating frame being destroyed during activation and fail.
  static void NavigatePrimaryPage(WebContents& web_contents, const GURL& gurl);
  void NavigatePrimaryPage(const GURL& gurl);

  // Navigates the primary page to the URL but does not wait until the
  // completion of the navigation. Instead it returns a
  // content::TestNavigationObserver.
  static std::unique_ptr<content::TestNavigationObserver>
  NavigatePrimaryPageAsync(WebContents& web_contents, const GURL& gurl);
  std::unique_ptr<content::TestNavigationObserver> NavigatePrimaryPageAsync(
      const GURL& gurl);

  // Opens a new window without an opener on the primary page of `web_contents`.
  // This is intended for activating a prerendered page initiated for a new
  // window.
  static void OpenNewWindowWithoutOpener(WebContents& web_contents,
                                         const GURL& url);

  // Confirms that, internally, appropriate subframes report that they are
  // prerendering (and that each frame tree type is kPrerender).
  [[nodiscard]] ::testing::AssertionResult VerifyPrerenderingState(
      const GURL& gurl);

  // Returns RenderFrameHost corresponding to `host_id` or `url`.
  static RenderFrameHost* GetPrerenderedMainFrameHost(WebContents& web_contents,
                                                      FrameTreeNodeId host_id);
  static RenderFrameHost* GetPrerenderedMainFrameHost(WebContents& web_contents,
                                                      const GURL& url);
  RenderFrameHost* GetPrerenderedMainFrameHost(FrameTreeNodeId host_id);
  RenderFrameHost* GetPrerenderedMainFrameHost(const GURL& url);

  int GetRequestCount(const GURL& url);
  net::test_server::HttpRequest::HeaderMap GetRequestHeaders(const GURL& url);

  // Waits until the request count for `url` reaches `count`.
  void WaitForRequest(const GURL& gurl, int count);

  // Generates the histogram name by appending the trigger type and the embedder
  // suffix to the base name.
  std::string GenerateHistogramName(const std::string& histogram_base_name,
                                    content::PreloadingTriggerType trigger_type,
                                    const std::string& embedder_suffix);

  // Updates the settings in PreloadingConfigOverride.
  void SetHoldback(PreloadingType preloading_type,
                   PreloadingPredictor predictor,
                   bool holdback);
  void SetHoldback(std::string_view preloading_type,
                   std::string_view predictor,
                   bool holdback);

 private:
  void MonitorResourceRequest(const net::test_server::HttpRequest& request);

  WebContents* GetWebContents();

  // Counts of requests sent to the server. Keyed by path (not by full URL)
  // because the host part of the requests is translated ("a.test" to
  // "127.0.0.1") before the server handles them.
  // This is accessed from the UI thread and `EmbeddedTestServer::io_thread_`.
  std::map<std::string, int> request_count_by_path_ GUARDED_BY(lock_);
  std::map<std::string, net::test_server::HttpRequest::HeaderMap>
      request_headers_by_path_ GUARDED_BY(lock_);
  ScopedPrerenderFeatureList feature_list_;
  base::OnceClosure monitor_callback_ GUARDED_BY(lock_);
  base::Lock lock_;
  PreloadingConfigOverride preloading_config_override_;
  WebContents::Getter get_web_contents_fn_;
};

// This test delegate is used for prerender-tests, in order to support
// prerendering going through the WebContentsDelegate.
class ScopedPrerenderWebContentsDelegate : public WebContentsDelegate {
 public:
  explicit ScopedPrerenderWebContentsDelegate(WebContents& web_contents);

  ~ScopedPrerenderWebContentsDelegate() override;

  // WebContentsDelegate override.
  PreloadingEligibility IsPrerender2Supported(
      WebContents& web_contents) override;

 private:
  base::WeakPtr<WebContents> web_contents_;
};

// This test delegate is used for link preview tests, in order to check
// whether the delegate receives `InitiatePreview` function call.
class MockLinkPreviewWebContentsDelegate : public WebContentsDelegate {
 public:
  MockLinkPreviewWebContentsDelegate();

  MockLinkPreviewWebContentsDelegate(
      const MockLinkPreviewWebContentsDelegate&) = delete;
  MockLinkPreviewWebContentsDelegate& operator=(
      const MockLinkPreviewWebContentsDelegate&) = delete;

  ~MockLinkPreviewWebContentsDelegate() override;

  MOCK_METHOD(void,
              InitiatePreview,
              (WebContents & web_contents, const GURL& url),
              (override));
};

}  // namespace test

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_PRERENDER_TEST_UTIL_H_
