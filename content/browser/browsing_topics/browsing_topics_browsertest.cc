// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

class FixedTopicsContentBrowserClient : public ContentBrowserClient {
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

  StoragePartitionConfig GetStoragePartitionConfigForSite(
      BrowserContext* browser_context,
      const GURL& site) override {
    if (site == GURL("https://b.test/")) {
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

    original_client_ = SetBrowserClientForTesting(&browser_client_);
  }

  void TearDownOnMainThread() override {
    SetBrowserClientForTesting(original_client_);
  }

  WebContents* web_contents() { return shell()->web_contents(); }

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

  FixedTopicsContentBrowserClient browser_client_;

  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
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

}  // namespace content
