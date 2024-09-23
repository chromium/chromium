// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/attribution_reporting/features.h"
#include "content/browser/browsing_topics/test_util.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {
constexpr char kBaseDataDir[] = "content/test/data/";

constexpr char kAddFencedFrameScript[] = R"(
  const fenced_frame = document.createElement('fencedframe');
  document.body.appendChild(fenced_frame);
)";

class FixedTopicsContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  bool HandleTopicsWebApi(
      const url::Origin& context_origin,
      content::RenderFrameHost* main_frame,
      browsing_topics::ApiCallerSource caller_source,
      bool get_topics,
      bool observe,
      std::vector<blink::mojom::EpochTopicPtr>& topics) override {
    blink::mojom::EpochTopicPtr result_topic = blink::mojom::EpochTopic::New();
    result_topic->topic = 1;
    result_topic->config_version = "chrome.1";
    result_topic->taxonomy_version = "1";
    result_topic->model_version = "2";
    result_topic->version = "chrome.1:1:2";

    topics.push_back(std::move(result_topic));

    return true;
  }

  int NumVersionsInTopicsEpochs(
      content::RenderFrameHost* main_frame) const override {
    return 1;
  }
};
}  // namespace

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
              URLLoaderInterceptor::WriteResponse(
                  base::StrCat(
                      {kBaseDataDir, params->url_request.url.path_piece()}),
                  params->client.get());

              return true;
            }));

    browser_client_ = std::make_unique<FixedTopicsContentBrowserClient>();
  }

  void TearDownOnMainThread() override {
    browser_client_.reset();
    url_loader_interceptor_.reset();
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  FrameTreeNode* root() {
    return static_cast<WebContentsImpl*>(web_contents())
        ->GetPrimaryFrameTree()
        .root();
  }

 private:
  std::unique_ptr<FixedTopicsContentBrowserClient> browser_client_;

  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
};

class PrivacySandboxAdsAPIsM1OverrideBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsM1OverrideBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kPrivacySandboxAdsAPIsM1Override,
         blink::features::kBrowsingTopics,
         blink::features::kBrowsingTopicsDocumentAPI,
         blink::features::kInterestGroupStorage, blink::features::kFencedFrames,
         blink::features::kSharedStorageAPI},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsM1OverrideBrowserTest,
                       NoOT_FeatureDetected) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("https://example.test/title1.html")));

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

class PrivacySandboxAdsAPIsM1OverrideNoFeatureBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsM1OverrideNoFeatureBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kPrivacySandboxAdsAPIsM1Override},
        {attribution_reporting::features::kConversionMeasurement,
         blink::features::kBrowsingTopics,
         blink::features::kBrowsingTopicsDocumentAPI,
         blink::features::kInterestGroupStorage, blink::features::kFencedFrames,
         blink::features::kSharedStorageAPI});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsM1OverrideNoFeatureBrowserTest,
                       OverrideWithoutFeature_IDLNotExposed) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("https://example.test/title1.html")));

  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "attribution-reporting')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "browsing-topics')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "join-ad-interest-group')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "run-ad-auction')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "shared-storage')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "private-aggregation')"));
  EXPECT_TRUE(ExecJs(root(), kAddFencedFrameScript));
  EXPECT_EQ(0U, root()->child_count());
}

}  // namespace content
