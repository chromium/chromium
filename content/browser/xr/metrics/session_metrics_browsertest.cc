// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/xr/metrics/session_metrics_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/shell/browser/shell.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_device.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
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
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
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
  metrics_helper->StartImmersiveSession(
      device::mojom::XRDeviceId::FAKE_DEVICE_ID, *(session_options),
      enabled_features);
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::XR_WebXR_Session::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Load a page in the prerender.
  auto prerender_url = embedded_test_server()->GetURL("/title1.html");
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(prerender_url);
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

class SessionMetricsFencedFrameBrowserTest : public ContentBrowserTest {
 public:
  SessionMetricsFencedFrameBrowserTest() = default;
  ~SessionMetricsFencedFrameBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUpOnMainThread();
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// Tests that a fenced frame doesn't record immersive session.
IN_PROC_BROWSER_TEST_F(SessionMetricsFencedFrameBrowserTest,
                       DoNotRecordImmersiveSessionInFencedFrame) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));

  SessionMetricsHelper* metrics_helper =
      SessionMetricsHelper::CreateForWebContents(web_contents());
  auto session_options = device::mojom::XRSessionOptions::New();
  std::unordered_set<device::mojom::XRSessionFeature> enabled_features;
  metrics_helper->StartImmersiveSession(
      device::mojom::XRDeviceId::FAKE_DEVICE_ID, *(session_options),
      enabled_features);
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::XR_WebXR_Session::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Create a fenced frame and check the immersive session is not recorded.
  GURL fenced_frame_url(
      embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::XR_WebXR_Session::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

}  // namespace content
