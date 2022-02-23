// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/xr/metrics/session_metrics_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/shell/browser/shell.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class SessionMetricsPrerenderingBrowserTest : public ContentBrowserTest {
 public:
  SessionMetricsPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &SessionMetricsPrerenderingBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~SessionMetricsPrerenderingBrowserTest() override = default;

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUpOnMainThread();
  }

  WebContents* web_contents() { return shell()->web_contents(); }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Tests that prerendering doesn't record immersive session.
IN_PROC_BROWSER_TEST_F(SessionMetricsPrerenderingBrowserTest,
                       DoNotRecordImmersiveSessionInPrerendering) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));

  SessionMetricsHelper* metrics_helper =
      SessionMetricsHelper::CreateForWebContents(web_contents());
  auto session_options = device::mojom::XRSessionOptions::New();
  std::unordered_set<device::mojom::XRSessionFeature> enabled_features;
  metrics_helper->StartImmersiveSession(*(session_options), enabled_features);
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::XR_WebXR_Session::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Load a page in the prerender.
  auto prerender_url = embedded_test_server()->GetURL("/title1.html");
  int host_id = prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  EXPECT_FALSE(host_observer.was_activated());
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::XR_WebXR_Session::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Activate the page from the prerendering.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::XR_WebXR_Session::kEntryName);
  EXPECT_EQ(1u, entries.size());
}

}  // namespace content
