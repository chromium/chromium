// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/browsing_topics/test_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

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

  StoragePartitionConfig GetStoragePartitionConfigForSite(
      BrowserContext* browser_context,
      const GURL& site) override {
    // Use a different StoragePartition for URLs from b.test (to test the fix
    // for crbug.com/40855090). Note that the port should be removed from site
    // to simplify the comparison, because non-default ports are included in
    // site URLs when kOriginKeyedProcessesByDefault is enabled.
    GURL::Replacements replacements;
    replacements.ClearPort();
    if (site.ReplaceComponents(replacements) == GURL("https://b.test/")) {
      return StoragePartitionConfig::Create(browser_context,
                                            /*partition_domain=*/"b.test",
                                            /*partition_name=*/"test_partition",
                                            /*in_memory=*/false);
    }
    return StoragePartitionConfig::CreateDefault(browser_context);
  }
};

}  // namespace

class BrowsingTopicsBrowserTest : public ContentBrowserTest {
 public:
  BrowsingTopicsBrowserTest() {
    feature_list_.InitWithFeatures({features::kPrivacySandboxAdsAPIsOverride,
                                    blink::features::kBrowsingTopics},
                                   /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(&https_server_);
    https_server_.ServeFilesFromSourceDirectory("content/test/data");

    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());

    browser_client_ = std::make_unique<FixedTopicsContentBrowserClient>();

    url_loader_monitor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [&](URLLoaderInterceptor::RequestParams* params) -> bool {
              last_request_is_topics_request_ =
                  params->url_request.browsing_topics;

              last_topics_header_ =
                  params->url_request.headers.GetHeader("Sec-Browsing-Topics");

              return false;
            }));
  }

  void TearDownOnMainThread() override {
    browser_client_.reset();
    url_loader_monitor_.reset();
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  bool last_request_is_topics_request() const {
    return last_request_is_topics_request_;
  }

  const std::optional<std::string>& last_topics_header() const {
    return last_topics_header_;
  }

  std::string InvokeTopicsAPI(const ToRenderFrameHost& adapter) {
    return EvalJs(adapter, R"(
      if (!(document.browsingTopics instanceof Function)) {
        'not a function';
      } else {
        document.browsingTopics()
        .then(topics => {
          let result = "[";
          for (const topic of topics) {
            result += JSON.stringify(topic, Object.keys(topic).sort()) + ";"
          }
          result += "]";
          return result;
        })
        .catch(error => error.message);
      }
    )")
        .ExtractString();
    ;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

  net::EmbeddedTestServer https_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  std::unique_ptr<FixedTopicsContentBrowserClient> browser_client_;

  bool last_request_is_topics_request_ = false;

  std::unique_ptr<base::RunLoop> resource_request_url_waiter_;
  std::optional<std::string> last_topics_header_;

  std::unique_ptr<URLLoaderInterceptor> url_loader_monitor_;
};

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, TopicsAPI) {
  // a.test will end up on the default storage partition.
  GURL main_frame_url = https_server_.GetURL("a.test", "/hello.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  EXPECT_EQ(
      "[{\"configVersion\":\"chrome.1\",\"modelVersion\":\"2\","
      "\"taxonomyVersion\":\"1\",\"topic\":1,\"version\":\"chrome.1:1:2\"};]",
      InvokeTopicsAPI(web_contents()));
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    TopicsAPI_InvokedFromFrameWithNonDefaultStoragePartition) {
  // b.test will end up on a non-default storage partition.
  GURL main_frame_url = https_server_.GetURL("b.test", "/hello.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  EXPECT_EQ("[]", InvokeTopicsAPI(web_contents()));
}

// TODO(crbug.com/40245082): migrate to WPT.
IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       Fetch_TopicsHeaderNotVisibleInServiceWorker) {
  GURL main_frame_url = https_server_.GetURL(
      "a.test", "/browsing_topics/service_worker_factory.html");
  GURL worker_script_url = https_server_.GetURL(
      "a.test", "/browsing_topics/topics_service_worker.js");
  GURL fetch_url = https_server_.GetURL("a.test", "/empty.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  EXPECT_EQ("ok",
            EvalJs(shell()->web_contents(),
                   JsReplace("setupServiceWorker($1)", worker_script_url)));

  // Reload the page to let it be controlled by the service worker.
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  // Initiate a topics fetch() request from the Window context. Verify that the
  // topics header is not visible in the service worker during the interception.
  EXPECT_EQ("null", EvalJs(shell()->web_contents(), content::JsReplace(
                                                        R"(
                new Promise((resolve, reject) => {
                  navigator.serviceWorker.addEventListener('message', e => {
                    if (e.data.url == $1) {
                      resolve(e.data.topicsHeader);
                    }
                  });

                  fetch($1, {browsingTopics: true});
                });
              )",
                                                        fetch_url)));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, TopicsHeaderForWindowFetch) {
  GURL main_frame_url = https_server_.GetURL(
      "a.test", "/browsing_topics/service_worker_factory.html");
  GURL fetch_url = https_server_.GetURL("a.test", "/empty.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  EXPECT_TRUE(ExecJs(
      shell()->web_contents(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  EXPECT_TRUE(last_request_is_topics_request());
  EXPECT_TRUE(last_topics_header());
  EXPECT_EQ(last_topics_header().value(),
            "(1);v=chrome.1:1:2, ();p=P00000000000");
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsNotAllowedForServiceWorkerFetch) {
  GURL main_frame_url = https_server_.GetURL(
      "a.test", "/browsing_topics/service_worker_factory.html");
  GURL worker_script_url = https_server_.GetURL(
      "a.test", "/browsing_topics/topics_service_worker.js");
  GURL fetch_url = https_server_.GetURL("a.test", "/empty.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  EXPECT_EQ("ok",
            EvalJs(shell()->web_contents(),
                   JsReplace("setupServiceWorker($1)", worker_script_url)));

  // Reload the page to let it be controlled by the service worker.
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  // Initiate a topics fetch request from the service worker. Verify that it
  // doesn't contain the topics header.
  EXPECT_TRUE(ExecJs(shell()->web_contents(), content::JsReplace(
                                                  R"(
                new Promise((resolve, reject) => {
                  navigator.serviceWorker.addEventListener('message', e => {
                    if (e.data.finishedFetch) {
                      resolve();
                    }
                  });

                  navigator.serviceWorker.controller.postMessage({
                    fetchUrl: $1
                  });
                });
              )",
                                                  fetch_url)));

  EXPECT_FALSE(last_request_is_topics_request());
  EXPECT_FALSE(last_topics_header());
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       EmbedderOptInStatus_StaticIframe_NoBrowsingTopicsAttr) {
  base::StringPairs topics_attribute_replacement;
  topics_attribute_replacement.emplace_back(
      "{{MAYBE_BROWSING_TOPICS_ATTRIBUTE}}", "");

  GURL iframe_url = https_server_.GetURL("b.test", "/title1.html");
  topics_attribute_replacement.emplace_back("{{SRC_URL}}", iframe_url.spec());

  GURL main_frame_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/page_with_iframe.html",
                    topics_attribute_replacement));

  // Wait for the main page and the iframe navigation.
  IframeBrowsingTopicsAttributeWatcher navigation_observer(
      shell()->web_contents(),
      /*expected_number_of_navigations=*/2);

  NavigationController::LoadURLParams params(main_frame_url);
  shell()->web_contents()->GetController().LoadURLWithParams(params);

  navigation_observer.WaitForNavigationFinished();

  EXPECT_EQ(navigation_observer.last_navigation_url(), iframe_url);
  EXPECT_FALSE(navigation_observer
                   .last_navigation_has_iframe_browsing_topics_attribute());
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       EmbedderOptInStatus_StaticIframe_HasBrowsingTopicsAttr) {
  base::StringPairs topics_attribute_replacement;
  topics_attribute_replacement.emplace_back(
      "{{MAYBE_BROWSING_TOPICS_ATTRIBUTE}}", "browsingtopics");

  GURL iframe_url = https_server_.GetURL("b.test", "/title1.html");
  topics_attribute_replacement.emplace_back("{{SRC_URL}}", iframe_url.spec());

  GURL main_frame_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/page_with_iframe.html",
                    topics_attribute_replacement));

  // Wait for the main page and the iframe navigation.
  IframeBrowsingTopicsAttributeWatcher navigation_observer(
      shell()->web_contents(),
      /*expected_number_of_navigations=*/2);

  NavigationController::LoadURLParams params(main_frame_url);
  shell()->web_contents()->GetController().LoadURLWithParams(params);

  navigation_observer.WaitForNavigationFinished();

  EXPECT_EQ(navigation_observer.last_navigation_url(), iframe_url);
  EXPECT_TRUE(navigation_observer
                  .last_navigation_has_iframe_browsing_topics_attribute());
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    EmbedderOptInStatus_AppendIframeElement_BrowsingTopicsAttrIsFalse) {
  GURL main_frame_url = https_server_.GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  GURL iframe_url = https_server_.GetURL("b.test", "/title1.html");

  // Wait for the iframe navigation.
  IframeBrowsingTopicsAttributeWatcher navigation_observer(
      shell()->web_contents());

  ExecuteScriptAsync(shell()->web_contents(), content::JsReplace(R"(
    const iframe = document.createElement("iframe");
    iframe.browsingTopics = false;
    iframe.src = $1;
    document.body.appendChild(iframe);
              )",
                                                                 iframe_url));

  navigation_observer.WaitForNavigationFinished();

  EXPECT_EQ(navigation_observer.last_navigation_url(), iframe_url);
  EXPECT_FALSE(navigation_observer
                   .last_navigation_has_iframe_browsing_topics_attribute());
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    EmbedderOptInStatus_AppendIframeElement_BrowsingTopicsAttrIsTrue) {
  GURL main_frame_url = https_server_.GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  GURL iframe_url = https_server_.GetURL("b.test", "/title1.html");

  // Wait for the iframe navigation.
  IframeBrowsingTopicsAttributeWatcher navigation_observer(
      shell()->web_contents());

  ExecuteScriptAsync(shell()->web_contents(), content::JsReplace(R"(
    const iframe = document.createElement("iframe");
    iframe.browsingTopics = true;
    iframe.src = $1;
    document.body.appendChild(iframe);
              )",
                                                                 iframe_url));

  navigation_observer.WaitForNavigationFinished();

  EXPECT_EQ(navigation_observer.last_navigation_url(), iframe_url);
  EXPECT_TRUE(navigation_observer
                  .last_navigation_has_iframe_browsing_topics_attribute());
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       EmbedderOptInStatus_BrowsingTopicsAttrUpdated) {
  GURL main_frame_url = https_server_.GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  // Append a frame without a `browsingTopics` attribute. Expect that the
  // navigation doesn't see the flag.
  {
    GURL iframe_url = https_server_.GetURL("b.test", "/title1.html");

    // Wait for the iframe navigation.
    IframeBrowsingTopicsAttributeWatcher navigation_observer(
        shell()->web_contents());

    ExecuteScriptAsync(shell()->web_contents(), content::JsReplace(R"(
      const iframe0 = document.createElement("iframe");
      iframe0.src = $1;
      document.body.appendChild(iframe0);
                )",
                                                                   iframe_url));

    navigation_observer.WaitForNavigationFinished();

    EXPECT_EQ(navigation_observer.last_navigation_url(), iframe_url);
    EXPECT_FALSE(navigation_observer
                     .last_navigation_has_iframe_browsing_topics_attribute());
  }

  // Set the `browsingTopics` attribute to true. Expect that the next navigation
  // sees the flag.
  {
    GURL iframe_url = https_server_.GetURL("c.test", "/title1.html");

    // Wait for the iframe navigation.
    IframeBrowsingTopicsAttributeWatcher navigation_observer(
        shell()->web_contents());

    ExecuteScriptAsync(shell()->web_contents(), content::JsReplace(R"(
      iframe0.browsingTopics = true;
      iframe0.src = $1;
                )",
                                                                   iframe_url));

    navigation_observer.WaitForNavigationFinished();

    EXPECT_EQ(navigation_observer.last_navigation_url(), iframe_url);
    EXPECT_TRUE(navigation_observer
                    .last_navigation_has_iframe_browsing_topics_attribute());
  }
}

}  // namespace content
