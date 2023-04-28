// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/header_util.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_view_host.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"

namespace content {

namespace {

class TopicsInterceptingContentBrowserClient : public ContentBrowserClient {
 public:
  bool HandleTopicsWebApi(
      const url::Origin& context_origin,
      content::RenderFrameHost* main_frame,
      browsing_topics::ApiCallerSource caller_source,
      bool get_topics,
      bool observe,
      std::vector<blink::mojom::EpochTopicPtr>& topics) override {
    handle_topics_web_api_called_ = true;
    last_get_topics_param_ = get_topics;
    last_observe_param_ = observe;
    return true;
  }

  bool handle_topics_web_api_called() const {
    return handle_topics_web_api_called_;
  }

  bool last_get_topics_param() const { return last_get_topics_param_; }

  bool last_observe_param() const { return last_observe_param_; }

 private:
  bool handle_topics_web_api_called_ = false;
  bool last_get_topics_param_ = false;
  bool last_observe_param_ = false;
};

}  // namespace

class BrowsingTopicsUtilTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    original_client_ = content::SetBrowserClientForTesting(&browser_client_);

    GURL url("https://foo.com");
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(url, web_contents());
    simulator->Commit();
  }

  void TearDown() override {
    SetBrowserClientForTesting(original_client_);

    content::RenderViewHostTestHarness::TearDown();
  }

  const TopicsInterceptingContentBrowserClient& browser_client() const {
    return browser_client_;
  }

 private:
  TopicsInterceptingContentBrowserClient browser_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
};

TEST_F(BrowsingTopicsUtilTest, DeriveTopicsHeaderValue_EmptyTopics) {
  std::vector<blink::mojom::EpochTopicPtr> topics;

  std::string header_value = DeriveTopicsHeaderValue(topics);

  EXPECT_TRUE(header_value.empty());
}

TEST_F(BrowsingTopicsUtilTest, DeriveTopicsHeaderValue_OneTopic) {
  std::vector<blink::mojom::EpochTopicPtr> topics;

  blink::mojom::EpochTopicPtr topic0 = blink::mojom::EpochTopic::New();
  topic0->topic = 1;
  topic0->config_version = "chrome.1";
  topic0->taxonomy_version = "1";
  topic0->model_version = "2";
  topic0->version = "chrome.1:1:2";

  topics.push_back(std::move(topic0));

  std::string header_value = DeriveTopicsHeaderValue(topics);

  EXPECT_EQ(
      header_value,
      "1;version=\"chrome.1:1:2\";config_version=\"chrome.1\";model_version="
      "\"2\";taxonomy_version=\"1\"");
}

TEST_F(BrowsingTopicsUtilTest, DeriveTopicsHeaderValue_TwoTopics) {
  std::vector<blink::mojom::EpochTopicPtr> topics;

  {
    blink::mojom::EpochTopicPtr topic = blink::mojom::EpochTopic::New();
    topic->topic = 1;
    topic->config_version = "chrome.1";
    topic->taxonomy_version = "1";
    topic->model_version = "2";
    topic->version = "chrome.1:1:2";
    topics.push_back(std::move(topic));
  }
  {
    blink::mojom::EpochTopicPtr topic = blink::mojom::EpochTopic::New();
    topic->topic = 2;
    topic->config_version = "chrome.1";
    topic->taxonomy_version = "3";
    topic->model_version = "4";
    topic->version = "chrome.1:3:4";
    topics.push_back(std::move(topic));
  }

  std::string header_value = DeriveTopicsHeaderValue(topics);

  EXPECT_EQ(header_value,
            "1;version=\"chrome.1:1:2\";config_version=\"chrome.1\";model_"
            "version=\"2\";taxonomy_version=\"1\", "
            "2;version=\"chrome.1:3:4\";config_version=\"chrome.1\";model_"
            "version=\"4\";taxonomy_version=\"3\"");
}

TEST_F(BrowsingTopicsUtilTest,
       HandleTopicsEligibleResponse_TrueValueObserveTopicsHeader) {
  network::mojom::ParsedHeadersPtr parsed_headers =
      network::mojom::ParsedHeaders::New();
  parsed_headers->observe_browsing_topics = true;
  HandleTopicsEligibleResponse(
      parsed_headers,
      /*caller_origin=*/url::Origin::Create(GURL("https://bar.com")),
      *web_contents()->GetPrimaryMainFrame(),
      browsing_topics::ApiCallerSource::kFetch);

  EXPECT_TRUE(browser_client().handle_topics_web_api_called());
  EXPECT_FALSE(browser_client().last_get_topics_param());
  EXPECT_TRUE(browser_client().last_observe_param());
}

TEST_F(BrowsingTopicsUtilTest,
       HandleTopicsEligibleResponse_FalseValueObserveTopicsHeader) {
  network::mojom::ParsedHeadersPtr parsed_headers =
      network::mojom::ParsedHeaders::New();
  parsed_headers->observe_browsing_topics = false;
  HandleTopicsEligibleResponse(
      parsed_headers,
      /*caller_origin=*/url::Origin::Create(GURL("https://bar.com")),
      *web_contents()->GetPrimaryMainFrame(),
      browsing_topics::ApiCallerSource::kFetch);

  EXPECT_FALSE(browser_client().handle_topics_web_api_called());
}

TEST_F(BrowsingTopicsUtilTest, HandleTopicsEligibleResponse_InactiveFrame) {
  network::mojom::ParsedHeadersPtr parsed_headers =
      network::mojom::ParsedHeaders::New();
  parsed_headers->observe_browsing_topics = true;
  RenderFrameHostImpl& rfh =
      static_cast<RenderFrameHostImpl&>(*web_contents()->GetPrimaryMainFrame());
  rfh.SetLifecycleState(
      RenderFrameHostImpl::LifecycleStateImpl::kReadyToBeDeleted);

  HandleTopicsEligibleResponse(
      parsed_headers,
      /*caller_origin=*/url::Origin::Create(GURL("https://bar.com")), rfh,
      browsing_topics::ApiCallerSource::kFetch);

  EXPECT_FALSE(browser_client().handle_topics_web_api_called());
}

}  // namespace content
