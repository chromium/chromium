// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/browsing_topics/test_util.h"
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
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {
constexpr char kBaseDataDir[] = "content/test/data/attribution_reporting/";

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
              last_request_is_topics_request_ =
                  params->url_request.browsing_topics;

              last_topics_header_.reset();
              std::string topics_header;
              if (params->url_request.headers.GetHeader("Sec-Browsing-Topics",
                                                        &topics_header)) {
                last_topics_header_ = topics_header;
              }

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

  bool last_request_is_topics_request() const {
    return last_request_is_topics_request_;
  }

  const absl::optional<std::string>& last_topics_header() const {
    return last_topics_header_;
  }

 private:
  bool last_request_is_topics_request_ = false;
  absl::optional<std::string> last_topics_header_;

  std::unique_ptr<FixedTopicsContentBrowserClient> browser_client_;

  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
};

class PrivacySandboxAdsAPIsAllEnabledBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsAllEnabledBrowserTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kPrivacySandboxAdsAPIs,
         blink::features::kBrowsingTopics, blink::features::kBrowsingTopicsXHR,
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
  EXPECT_TRUE(last_topics_header());
  EXPECT_EQ(last_topics_header().value(),
            "1;version=\"chrome.1:1:2\";config_version=\"chrome.1\";model_"
            "version=\"2\";taxonomy_version=\"1\"");
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
  EXPECT_FALSE(last_topics_header());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsAllEnabledBrowserTest,
                       OriginTrialEnabled_TopicsAllowedForXhr) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ("success", EvalJs(shell()->web_contents(),
                              R"(
    const xhr = new XMLHttpRequest();

    xhr.onreadystatechange = function() {
      if (xhr.readyState == XMLHttpRequest.DONE) {
        domAutomationController.send('success');
      }
    }

    xhr.open('GET', 'https://example.test/page_without_ads_apis_ot.html');
    xhr.deprecatedBrowsingTopics = true;
    xhr.send();)",
                              EXECUTE_SCRIPT_USE_MANUAL_REPLY));

  EXPECT_TRUE(last_request_is_topics_request());
  EXPECT_TRUE(last_topics_header());
  EXPECT_EQ(last_topics_header().value(),
            "1;version=\"chrome.1:1:2\";config_version=\"chrome.1\";model_"
            "version=\"2\";taxonomy_version=\"1\"");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsAllEnabledBrowserTest,
                       OriginTrialDisabled_TopicsNotAllowedForXhr) {
  // Navigate to a page without an OT token.
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_without_ads_apis_ot.html")));

  EXPECT_EQ("success", EvalJs(shell()->web_contents(),
                              R"(
    const xhr = new XMLHttpRequest();

    xhr.onreadystatechange = function() {
      if (xhr.readyState == XMLHttpRequest.DONE) {
        domAutomationController.send('success');
      }
    }

    xhr.open('GET', 'https://example.test/page_without_ads_apis_ot.html');
    xhr.deprecatedBrowsingTopics = true;
    xhr.send();)",
                              EXECUTE_SCRIPT_USE_MANUAL_REPLY));

  EXPECT_FALSE(last_request_is_topics_request());
  EXPECT_FALSE(last_topics_header());
}

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxAdsAPIsAllEnabledBrowserTest,
    OriginTrialEnabled_HasBrowsingTopicsIframeAttr_ConsideredForEmbedderOptIn) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  // Wait for the iframe navigation.
  IframeBrowsingTopicsAttributeWatcher navigation_observer(
      shell()->web_contents());

  ExecuteScriptAsync(shell()->web_contents(), R"(
    const iframe = document.createElement("iframe");
    iframe.browsingTopics = true;
    iframe.src = 'https://example.test/page_without_ads_apis_ot.html';
    document.body.appendChild(iframe);
              )");

  navigation_observer.WaitForNavigationFinished();

  EXPECT_EQ(navigation_observer.last_navigation_url(),
            "https://example.test/page_without_ads_apis_ot.html");
  EXPECT_TRUE(navigation_observer
                  .last_navigation_has_iframe_browsing_topics_attribute());
}

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxAdsAPIsAllEnabledBrowserTest,
    OriginTrialDisabled_HasBrowsingTopicsIframeAttr_NotConsideredForEmbedderOptIn) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_without_ads_apis_ot.html")));

  // Wait for the iframe navigation.
  IframeBrowsingTopicsAttributeWatcher navigation_observer(
      shell()->web_contents());

  ExecuteScriptAsync(shell()->web_contents(), R"(
    const iframe = document.createElement("iframe");
    iframe.browsingTopics = true;
    iframe.src = 'https://example.test/page_without_ads_apis_ot.html';
    document.body.appendChild(iframe);
              )");

  navigation_observer.WaitForNavigationFinished();

  EXPECT_EQ(navigation_observer.last_navigation_url(),
            "https://example.test/page_without_ads_apis_ot.html");
  EXPECT_FALSE(navigation_observer
                   .last_navigation_has_iframe_browsing_topics_attribute());
}

class PrivacySandboxAdsAPIsTopicsDisabledBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsTopicsDisabledBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kPrivacySandboxAdsAPIs,
                              blink::features::kBrowsingTopicsXHR},
        /*disabled_features=*/{blink::features::kBrowsingTopics});
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

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsTopicsDisabledBrowserTest,
                       OriginTrialEnabled_TopicsNotAllowedForFetch) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_TRUE(
      ExecJs(shell()->web_contents(),
             content::JsReplace(
                 "fetch($1, {browsingTopics: true})",
                 GURL("https://example.test/page_without_ads_apis_ot.html"))));

  EXPECT_FALSE(last_request_is_topics_request());
  EXPECT_FALSE(last_topics_header());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsTopicsDisabledBrowserTest,
                       OriginTrialEnabled_TopicsNotAllowedForXhr) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ("success", EvalJs(shell()->web_contents(),
                              R"(
    const xhr = new XMLHttpRequest();

    xhr.onreadystatechange = function() {
      if (xhr.readyState == XMLHttpRequest.DONE) {
        domAutomationController.send('success');
      }
    }

    xhr.open('GET', 'https://example.test/page_without_ads_apis_ot.html');
    xhr.deprecatedBrowsingTopics = true;
    xhr.send();)",
                              EXECUTE_SCRIPT_USE_MANUAL_REPLY));

  EXPECT_FALSE(last_request_is_topics_request());
  EXPECT_FALSE(last_topics_header());
}

class PrivacySandboxAdsAPIsTopicsXHRDisabledBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsTopicsXHRDisabledBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kPrivacySandboxAdsAPIs,
                              blink::features::kBrowsingTopics},
        /*disabled_features=*/{blink::features::kBrowsingTopicsXHR});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsTopicsXHRDisabledBrowserTest,
                       OriginTrialEnabled_TopicsFeatureDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "browsing-topics')"));

  EXPECT_EQ(true, EvalJs(shell(), "document.browsingTopics !== undefined"));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsTopicsXHRDisabledBrowserTest,
                       OriginTrialEnabled_TopicsAllowedForFetch) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_TRUE(
      ExecJs(shell()->web_contents(),
             content::JsReplace(
                 "fetch($1, {browsingTopics: true})",
                 GURL("https://example.test/page_without_ads_apis_ot.html"))));

  EXPECT_TRUE(last_request_is_topics_request());
  EXPECT_TRUE(last_topics_header());
  EXPECT_EQ(last_topics_header().value(),
            "1;version=\"chrome.1:1:2\";config_version=\"chrome.1\";model_"
            "version=\"2\";taxonomy_version=\"1\"");
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsTopicsXHRDisabledBrowserTest,
                       OriginTrialEnabled_TopicsNotAllowedForXhr) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ("success", EvalJs(shell()->web_contents(),
                              R"(
    const xhr = new XMLHttpRequest();

    xhr.onreadystatechange = function() {
      if (xhr.readyState == XMLHttpRequest.DONE) {
        domAutomationController.send('success');
      }
    }

    xhr.open('GET', 'https://example.test/page_without_ads_apis_ot.html');
    xhr.deprecatedBrowsingTopics = true;
    xhr.send();)",
                              EXECUTE_SCRIPT_USE_MANUAL_REPLY));

  EXPECT_FALSE(last_request_is_topics_request());
  EXPECT_FALSE(last_topics_header());
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
