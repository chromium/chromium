// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PRERENDER_TEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_PRERENDER_TEST_UTIL_H_

#include "base/test/scoped_feature_list.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class WebContents;

namespace test {

class PrerenderHostRegistryObserverImpl;

// The PrerenderHostRegistryObserver permits waiting for a host to be created
// for a given URL.
class PrerenderHostRegistryObserver {
 public:
  explicit PrerenderHostRegistryObserver(content::WebContents& web_contents);
  ~PrerenderHostRegistryObserver();
  PrerenderHostRegistryObserver(const PrerenderHostRegistryObserver&) = delete;
  PrerenderHostRegistryObserver& operator=(
      const PrerenderHostRegistryObserver&) = delete;

  // Returns immediately if |gurl| was ever triggered before. Otherwise blocks
  // on a RunLoop until a prerender of |gurl| is triggered.
  void WaitForTrigger(const GURL& gurl);

  // Invokes |callback| immediately if |gurl| was ever triggered before.
  // Otherwise invokes |callback| when a prerender for |gurl| is triggered.
  void NotifyOnTrigger(const GURL& gurl, base::OnceClosure callback);

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
  PrerenderHostObserver(content::WebContents& web_contents, int host_id);

  // Will start observing a PrerenderHost for |gurl| as soon as it is
  // triggered.
  PrerenderHostObserver(content::WebContents& web_contents, const GURL& gurl);

  ~PrerenderHostObserver();
  PrerenderHostObserver(const PrerenderHostObserver&) = delete;
  PrerenderHostObserver& operator=(const PrerenderHostObserver&) = delete;

  // Returns immediately if the PrerenderHost was already activated, otherwise
  // spins a RunLoop until the observed host is activated.
  void WaitForActivation();

  // Returns immediately if the PrerenderHost was already destroyed, otherwise
  // spins a RunLoop until the observed host is destroyed.
  void WaitForDestroyed();

  // True if the PrerenderHost was activated to be the primary page.
  bool was_activated() const;

 private:
  std::unique_ptr<PrerenderHostObserverImpl> impl_;
};

// Browser tests can use this class to more conveniently leverage prerendering.
class PrerenderTestHelper {
 public:
  explicit PrerenderTestHelper(const content::WebContents::Getter& fn);
  ~PrerenderTestHelper();
  PrerenderTestHelper(const PrerenderTestHelper&) = delete;
  PrerenderTestHelper& operator=(const PrerenderTestHelper&) = delete;

  // This installs a network monitor on the http server. Be sure to call this
  // before starting the server.
  void SetUpOnMainThread(net::test_server::EmbeddedTestServer* http_server);

  // Attempts to lookup the host for the given |gurl|. Returns
  // RenderFrameHost::kNoFrameTreeNodeId upon failure.
  int GetHostForUrl(const GURL& gurl);

  void WaitForPrerenderLoadCompletion(const GURL& gurl);
  void WaitForPrerenderLoadCompletion(int host_id);

  // Adds <link rel=prerender> in the current main frame and waits until the
  // completion of prerendering. Returns the id of the resulting prerendering
  // host.
  //
  // AddPrerenderAsync() is the same as AddPrerender(), but does not wait until
  // the completion of prerendering.
  //
  // NOTE: this function requires that the add_prerender.html has been
  // loaded. This is most easily accomplished by using PrerenderBrowserTest,
  // but if that's not possible, ensure that you have this file loaded before
  // making this call.
  int AddPrerender(const GURL& gurl);
  void AddPrerenderAsync(const GURL& gurl);

  // Adds <link rel=prerender> in the current main frame without loading
  // add_prerender.html and waits until the completion of prerendering.
  int AddPrerenderWithTestUtilJS(const GURL& gurl);

  // This navigates, but does not activate, the prerendered page.
  void NavigatePrerenderedPage(int host_id, const GURL& gurl);

  // Navigates the primary page to the URL and waits until the completion of
  // the navigation.
  //
  // Navigations that could activate a prerendered page on the multiple
  // WebContents architecture (not multiple-pages architecture known as
  // MPArch) should use this function instead of the NavigateToURL() test
  // helper. This is because the test helper accesses the predecessor
  // WebContents to be destroyed during activation and results in crashes.
  // See https://crbug.com/1154501 for the MPArch migration.
  // TODO(crbug.com/1198960): remove this once the migration is complete.
  void NavigatePrimaryPage(const GURL& gurl);

  // Confirms that, internally, appropriate subframes report that they are
  // prerendering (and that each frame tree type is kPrerender).
  ::testing::AssertionResult VerifyPrerenderingState(const GURL& gurl)
      WARN_UNUSED_RESULT;

  RenderFrameHost* GetPrerenderedMainFrameHost(int host_id);

  int GetRequestCount(const GURL& url);

  // Waits until the request count for `url` reaches `count`.
  void WaitForRequest(const GURL& gurl, int count);

 private:
  void MonitorResourceRequest(const net::test_server::HttpRequest& request);

  content::WebContents* GetWebContents();

  // Counts of requests sent to the server. Keyed by path (not by full URL)
  // because the host part of the requests is translated ("a.test" to
  // "127.0.0.1") before the server handles them.
  // This is accessed from the UI thread and `EmbeddedTestServer::io_thread_`.
  std::map<std::string, int> request_count_by_path_ GUARDED_BY(lock_);
  base::test::ScopedFeatureList feature_list_;
  base::OnceClosure monitor_callback_ GUARDED_BY(lock_);
  base::Lock lock_;
  content::WebContents::Getter get_web_contents_fn_;
};

}  // namespace test

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_PRERENDER_TEST_UTIL_H_
