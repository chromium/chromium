// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/global_privacy_control/global_privacy_control_util.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace content {
namespace {

constexpr char kGlobalPrivacyControlSourceHistogram[] =
    "Network.GlobalPrivacyControlSource.Subsampled";

class GlobalPrivacyControlBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kGlobalPrivacyControlForce, GetParam());
    ContentBrowserTest::SetUp();
  }

  void ExpectPageTextEq(const std::string& expected_content) {
    EXPECT_EQ(expected_content,
              EvalJs(GetWebContents(), "document.body.innerText;"));
  }

  WebContents* GetWebContents() { return shell()->web_contents(); }

  bool GetDOMGlobalPrivacyControlProperty() {
    // Compare the result to true to handle the case where the property is
    // disabled, which would result in 'undefined'.
    return EvalJs(GetWebContents(), "navigator.globalPrivacyControl === true")
        .ExtractBool();
  }

  GURL GetURL(const std::string& relative_url) {
    return embedded_test_server()->GetURL(relative_url);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Checks that navigator.globalPrivacyControl is accessible in a dedicated
// worker created from a dedicated worker.
IN_PROC_BROWSER_TEST_P(GlobalPrivacyControlBrowserTest, GPCInNestedWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(GetWebContents(),
                            GetURL("/global_privacy_control/gpc_from_worker."
                                   "html?script=gpc_from_nested_worker.js")));
  EXPECT_EQ(GetParam(),
            EvalJs(GetWebContents(), "gpc_from_worker();").ExtractBool());
}

// Checks that navigator.globalPrivacyControl is accessible in a shared worker.
IN_PROC_BROWSER_TEST_P(GlobalPrivacyControlBrowserTest, GPCInSharedWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(
      NavigateToURL(GetWebContents(), GetURL("/global_privacy_control/"
                                             "gpc_from_shared_worker.html")));
  EXPECT_EQ(
      GetParam(),
      EvalJs(GetWebContents(), "gpc_from_shared_worker();").ExtractBool());
}

// Checks that navigator.globalPrivacyControl is accessible in a service worker.
IN_PROC_BROWSER_TEST_P(GlobalPrivacyControlBrowserTest, GPCInServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      GetWebContents(),
      GetURL("/global_privacy_control/gpc_from_service_worker.html")));
  EXPECT_EQ("ready", EvalJs(GetWebContents(), "setup_gpc_service_worker();"));
  EXPECT_EQ(
      GetParam(),
      EvalJs(GetWebContents(), "gpc_from_service_worker();").ExtractBool());
}

// Checks that the Sec-GPC header is preserved when fetching from a dedicated
// worker created from a dedicated worker.
IN_PROC_BROWSER_TEST_P(GlobalPrivacyControlBrowserTest, FetchFromNestedWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      GetWebContents(),
      GetURL("/workers/fetch_from_worker.html?script=fetch_from_nested_worker."
             "js")));
  EXPECT_EQ(
      GetParam() ? "1" : "None",
      EvalJs(GetWebContents(), "fetch_from_worker('/echoheader?Sec-GPC');"));
}

// Checks that the Sec-GPC header is preserved when fetching from a shared
// worker.
IN_PROC_BROWSER_TEST_P(GlobalPrivacyControlBrowserTest, FetchFromSharedWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(GetWebContents(),
                            GetURL("/workers/fetch_from_shared_worker.html")));
  EXPECT_EQ(GetParam() ? "1" : "None",
            EvalJs(GetWebContents(),
                   "fetch_from_shared_worker('/echoheader?Sec-GPC');"));
}

// Checks that the Sec-GPC header is preserved when fetching from a service
// worker.
IN_PROC_BROWSER_TEST_P(GlobalPrivacyControlBrowserTest,
                       FetchFromServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(
      NavigateToURL(GetWebContents(),
                    GetURL("/service_worker/fetch_from_service_worker.html")));
  EXPECT_EQ("ready", EvalJs(GetWebContents(), "setup();"));
  EXPECT_EQ(GetParam() ? "1" : "None",
            EvalJs(GetWebContents(),
                   "fetch_from_service_worker('/echoheader?Sec-GPC');"));
}

// Checks that the Sec-GPC header is sent for subframe (iframe) navigations.
IN_PROC_BROWSER_TEST_P(GlobalPrivacyControlBrowserTest, HeaderInSubFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(GetWebContents(),
                            GetURL("/global_privacy_control/gpc_iframe.html")));

  RenderFrameHost* iframe =
      ChildFrameAt(GetWebContents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe);

  EXPECT_TRUE(NavigateToURLFromRenderer(iframe, GetURL("/echoheader?Sec-GPC")));

  iframe = ChildFrameAt(GetWebContents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe);

  EXPECT_EQ(GetParam() ? "1" : "None",
            EvalJs(iframe, "document.body.innerText;"));
}

// Checks that navigator.globalPrivacyControl is accessible in a subframe
// (iframe).
IN_PROC_BROWSER_TEST_P(GlobalPrivacyControlBrowserTest, GPCInSubFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(GetWebContents(),
                            GetURL("/global_privacy_control/gpc_iframe.html")));

  RenderFrameHost* iframe =
      ChildFrameAt(GetWebContents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe);

  EXPECT_EQ(
      GetParam(),
      EvalJs(iframe, "navigator.globalPrivacyControl === true").ExtractBool());
}

// Checks that adding the Sec-GPC header during a navigation leaves a sample in
// the kFrameNavigation bucket.
IN_PROC_BROWSER_TEST_P(GlobalPrivacyControlBrowserTest,
                       SourceHistogramSampledOnNavigation) {
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(GetWebContents(), GetURL("/echoheader?Sec-GPC")));
  content::FetchHistogramsFromChildProcesses();

  if (GetParam()) {
    EXPECT_GE(
        histogram_tester.GetBucketCount(
            kGlobalPrivacyControlSourceHistogram,
            static_cast<int>(blink::GPCSignalSourceType::kFrameNavigation)),
        1);
  } else {
    histogram_tester.ExpectTotalCount(kGlobalPrivacyControlSourceHistogram, 0);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlobalPrivacyControlBrowserTest,
                         ::testing::Values(false, true));

// Fixture to run renderer-source GPC tests with histogram subsampling disabled.
class GlobalPrivacyControlSourceInRendererBrowserTest
    : public GlobalPrivacyControlBrowserTest {
 protected:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {blink::features::kGlobalPrivacyControlForce,
           blink::features::kGlobalPrivacyControlAlwaysSample},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {blink::features::kGlobalPrivacyControlAlwaysSample},
          {blink::features::kGlobalPrivacyControlForce});
    }
    ContentBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Checks that adding the Sec-GPC header to a renderer-initiated subresource
// fetch leaves a sample in the kSubresourceFetch bucket.
IN_PROC_BROWSER_TEST_P(GlobalPrivacyControlSourceInRendererBrowserTest,
                       SubresourceFetchSource) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(GetWebContents(), GetURL("/empty.html")));
  EXPECT_EQ(GetParam() ? "1" : "None",
            EvalJs(GetWebContents(),
                   "fetch('/echoheader?Sec-GPC').then(r => r.text());"));
  content::FetchHistogramsFromChildProcesses();

  if (GetParam()) {
    EXPECT_GE(
        histogram_tester.GetBucketCount(
            kGlobalPrivacyControlSourceHistogram,
            static_cast<int>(blink::GPCSignalSourceType::kSubresourceFetch)),
        1);
  } else {
    histogram_tester.ExpectTotalCount(kGlobalPrivacyControlSourceHistogram, 0);
  }
}

// Checks that adding the Sec-GPC header to a service-worker-controlled
// navigation leaves a sample in the kWorkerNavigation bucket.
IN_PROC_BROWSER_TEST_P(GlobalPrivacyControlSourceInRendererBrowserTest,
                       WorkerNavigationSource) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(GetWebContents(), GetURL("/empty.html")));
  EXPECT_TRUE(
      NavigateToURL(GetWebContents(),
                    GetURL("/service_worker/fetch_from_service_worker.html")));
  EXPECT_EQ("ready", EvalJs(GetWebContents(), "setup();"));
  EXPECT_TRUE(
      NavigateToURL(GetWebContents(), GetURL("/service_worker/empty.html")));
  content::FetchHistogramsFromChildProcesses();

  if (GetParam()) {
    EXPECT_GE(
        histogram_tester.GetBucketCount(
            kGlobalPrivacyControlSourceHistogram,
            static_cast<int>(blink::GPCSignalSourceType::kWorkerNavigation)),
        1);
  } else {
    histogram_tester.ExpectTotalCount(kGlobalPrivacyControlSourceHistogram, 0);
  }
}

// Checks that adding the Sec-GPC header to a fetch from a dedicated worker
// leaves a sample in the kWorkerSubresourceFetch bucket.
IN_PROC_BROWSER_TEST_P(GlobalPrivacyControlSourceInRendererBrowserTest,
                       WorkerSubresourceFetchSource) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(GetWebContents(),
                            GetURL("/workers/fetch_from_worker.html")));
  EXPECT_EQ(
      GetParam() ? "1" : "None",
      EvalJs(GetWebContents(), "fetch_from_worker('/echoheader?Sec-GPC');"));
  content::FetchHistogramsFromChildProcesses();

  if (GetParam()) {
    EXPECT_GE(histogram_tester.GetBucketCount(
                  kGlobalPrivacyControlSourceHistogram,
                  static_cast<int>(
                      blink::GPCSignalSourceType::kWorkerSubresourceFetch)),
              1);
  } else {
    histogram_tester.ExpectTotalCount(kGlobalPrivacyControlSourceHistogram, 0);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlobalPrivacyControlSourceInRendererBrowserTest,
                         ::testing::Values(false, true));

}  // namespace
}  // namespace content
