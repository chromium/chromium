// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

namespace {
constexpr char kBaseDataDir[] = "content/test/data/attribution_reporting/";

constexpr char kAddFencedFrameScript[] = R"(
  const fenced_frame = document.createElement('fencedframe');
  document.body.appendChild(fenced_frame);
)";
}

class PrivacySandboxAdsAPIsBrowserTestBase : public ContentBrowserTest {
 public:
  PrivacySandboxAdsAPIsBrowserTestBase() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [&](URLLoaderInterceptor::RequestParams* params) -> bool {
              last_request_is_topics_request_ =
                  params->url_request.browsing_topics;

              last_resource_request_url_ = params->url_request.url;
              if (resource_request_url_waiter_ &&
                  resource_request_url_waiter_->running() &&
                  last_resource_request_url_ ==
                      expected_last_resource_request_url_) {
                resource_request_url_waiter_->Quit();
              }

              URLLoaderInterceptor::WriteResponse(
                  base::StrCat(
                      {kBaseDataDir, params->url_request.url.path_piece()}),
                  params->client.get());

              return true;
            }));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  WebContents* web_contents() { return shell()->web_contents(); }

  FrameTreeNode* root() {
    return static_cast<WebContentsImpl*>(web_contents())
        ->GetPrimaryFrameTree()
        .root();
  }

  bool last_request_is_topics_request() const {
    return last_request_is_topics_request_;
  }

  void WaitForResourceRequestURL(const GURL& url) {
    DCHECK(!resource_request_url_waiter_);

    if (last_resource_request_url_ == url)
      return;

    expected_last_resource_request_url_ = url;
    resource_request_url_waiter_ = std::make_unique<base::RunLoop>();
    resource_request_url_waiter_->Run();
  }

 private:
  bool last_request_is_topics_request_ = false;

  std::unique_ptr<base::RunLoop> resource_request_url_waiter_;
  GURL expected_last_resource_request_url_;
  GURL last_resource_request_url_;

  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
};

class PrivacySandboxAdsAPIsAllEnabledBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsAllEnabledBrowserTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kPrivacySandboxAdsAPIs,
         blink::features::kBrowsingTopics,
         blink::features::kInterestGroupStorage, blink::features::kFencedFrames,
         blink::features::kSharedStorageAPI},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsAllEnabledBrowserTest,
                       OriginTrialEnabled_FeatureDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));
  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "browsing-topics')"));
  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "join-ad-interest-group')"));

  EXPECT_EQ(true, ExecJs(root(), "sharedStorage !== undefined"));
  EXPECT_EQ(true, EvalJs(shell(), "document.browsingTopics !== undefined"));
  EXPECT_EQ(true, EvalJs(shell(), "navigator.runAdAuction !== undefined"));
  EXPECT_EQ(true,
            EvalJs(shell(), "navigator.joinAdInterestGroup !== undefined"));

  EXPECT_TRUE(ExecJs(root(), kAddFencedFrameScript));
  EXPECT_EQ(1U, root()->child_count());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsAllEnabledBrowserTest,
                       OriginTrialDisabled_FeatureNotDetected) {
  // Navigate to a page without an OT token.
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_without_ads_apis_ot.html")));

  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "attribution-reporting')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "browsing-topics')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "join-ad-interest-group')"));

  EXPECT_EQ(true, ExecJs(shell(), "window.sharedStorage === undefined"));
  EXPECT_EQ(true, EvalJs(shell(), "document.browsingTopics === undefined"));
  EXPECT_EQ(true, EvalJs(shell(), "navigator.runAdAuction === undefined"));
  EXPECT_EQ(true,
            EvalJs(shell(), "navigator.joinAdInterestGroup === undefined"));

  EXPECT_TRUE(ExecJs(root(), kAddFencedFrameScript));
  EXPECT_EQ(0U, root()->child_count());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsAllEnabledBrowserTest,
                       OriginTrialEnabled_TopicsAllowedForFetch) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_TRUE(
      ExecJs(shell()->web_contents(),
             content::JsReplace(
                 "fetch($1, {browsingTopics: true})",
                 GURL("https://example.test/page_without_ads_apis_ot.html"))));

  EXPECT_TRUE(last_request_is_topics_request());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsAllEnabledBrowserTest,
                       OriginTrialDisabled_TopicsNotAllowedForFetch) {
  // Navigate to a page without an OT token.
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_without_ads_apis_ot.html")));

  EXPECT_TRUE(
      ExecJs(shell()->web_contents(),
             content::JsReplace(
                 "fetch($1, {browsingTopics: true})",
                 GURL("https://example.test/page_without_ads_apis_ot.html"))));

  EXPECT_FALSE(last_request_is_topics_request());
}

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxAdsAPIsAllEnabledBrowserTest,
    OriginTrialEnabled_TopicsNotAllowedForServiceWorkerFetch) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ(
      "ok",
      EvalJs(
          shell()->web_contents(),
          JsReplace(
              "setupServiceWorker($1)",
              GURL(
                  "https://example.test/"
                  "fetch_topics.js?fetch_url=page_without_ads_apis_ot.html"))));

  WaitForResourceRequestURL(
      GURL("https://example.test/page_without_ads_apis_ot.html"));

  EXPECT_FALSE(last_request_is_topics_request());
}

class PrivacySandboxAdsAPIsTopicsDisabledBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsTopicsDisabledBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kPrivacySandboxAdsAPIs},
                                   {blink::features::kBrowsingTopics});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsTopicsDisabledBrowserTest,
                       OriginTrialEnabled_CorrectFeaturesDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "browsing-topics')"));

  EXPECT_EQ(false, EvalJs(shell(), "document.browsingTopics !== undefined"));
}

class PrivacySandboxAdsAPIsSharedStorageDisabledBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsSharedStorageDisabledBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kPrivacySandboxAdsAPIs},
                                   {blink::features::kSharedStorageAPI});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsSharedStorageDisabledBrowserTest,
                       OriginTrialEnabled_CorrectFeaturesDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));

  EXPECT_EQ(true, ExecJs(shell(), "window.sharedStorage === undefined"));
}

class PrivacySandboxAdsAPIsFledgeDisabledBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsFledgeDisabledBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kPrivacySandboxAdsAPIs},
                                   {blink::features::kInterestGroupStorage});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsFledgeDisabledBrowserTest,
                       OriginTrialEnabled_CorrectFeaturesDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "join-ad-interest-group')"));

  EXPECT_EQ(false, EvalJs(shell(), "navigator.runAdAuction !== undefined"));
  EXPECT_EQ(false,
            EvalJs(shell(), "navigator.joinAdInterestGroup !== undefined"));
}

class PrivacySandboxAdsAPIsFencedFramesDisabledBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsFencedFramesDisabledBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kPrivacySandboxAdsAPIs},
                                   {blink::features::kFencedFrames});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsFencedFramesDisabledBrowserTest,
                       OriginTrialEnabled_CorrectFeaturesDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));

  EXPECT_TRUE(ExecJs(root(), kAddFencedFrameScript));
  EXPECT_EQ(0U, root()->child_count());
}

class PrivacySandboxAdsAPIsDisabledBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kPrivacySandboxAdsAPIs);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsDisabledBrowserTest,
                       BaseFeatureDisabled_FeatureNotDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "attribution-reporting')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "browsing-topics')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "join-ad-interest-group')"));

  EXPECT_EQ(true, ExecJs(shell(), "window.sharedStorage === undefined"));
  EXPECT_EQ(true, EvalJs(shell(), "document.browsingTopics === undefined"));
  EXPECT_EQ(true, EvalJs(shell(), "navigator.runAdAuction === undefined"));
  EXPECT_EQ(true,
            EvalJs(shell(), "navigator.joinAdInterestGroup === undefined"));

  EXPECT_TRUE(ExecJs(root(), kAddFencedFrameScript));
  EXPECT_EQ(0U, root()->child_count());
}

}  // namespace content
