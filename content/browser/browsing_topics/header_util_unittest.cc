// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/header_util.h"

#include "base/strings/strcat.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_view_host.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"

namespace content {

namespace {

blink::mojom::EpochTopicPtr CreateMojomTopic(int topic,
                                             const std::string& model_version) {
  auto mojom_topic = blink::mojom::EpochTopic::New();
  mojom_topic->topic = topic;
  mojom_topic->config_version = "chrome.1";
  mojom_topic->taxonomy_version = "1";
  mojom_topic->model_version = model_version;
  mojom_topic->version = base::StrCat({mojom_topic->config_version, ":",
                                       mojom_topic->taxonomy_version, ":",
                                       mojom_topic->model_version});
  return mojom_topic;
}

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

  int NumVersionsInTopicsEpochs(
      content::RenderFrameHost* main_frame) const override {
    return 1;
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

TEST_F(BrowsingTopicsUtilTest,
       DeriveTopicsHeaderValue_EmptyTopics_ZeroVersionInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/0);
  EXPECT_EQ(header_value, "();p=P0000000000000000000000000000000");
}

TEST_F(BrowsingTopicsUtilTest,
       DeriveTopicsHeaderValue_EmptyTopics_OneVersionInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/1);
  EXPECT_EQ(header_value, "();p=P0000000000000000000000000000000");
}

TEST_F(BrowsingTopicsUtilTest,
       DeriveTopicsHeaderValue_EmptyTopics_TwoVersionsInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/2);
  EXPECT_EQ(header_value,
            "();p=P00000000000000000000000000000000000000000000000000");
}

TEST_F(BrowsingTopicsUtilTest,
       DeriveTopicsHeaderValue_EmptyTopics_ThreeVersionsInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/3);
  EXPECT_EQ(
      header_value,
      "();p="
      "P000000000000000000000000000000000000000000000000000000000000000000000");
}

TEST_F(BrowsingTopicsUtilTest,
       DeriveTopicsHeaderValue_OneTopic_OneVersionInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(1, /*model_version=*/"2"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/1);

  EXPECT_EQ(header_value, "(1);v=chrome.1:1:2, ();p=P00000000000");
}

TEST_F(BrowsingTopicsUtilTest,
       DeriveTopicsHeaderValue_OneTopic_TwoVersionsInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(1, /*model_version=*/"2"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/2);

  EXPECT_EQ(header_value,
            "(1);v=chrome.1:1:2, ();p=P000000000000000000000000000000");
}

TEST_F(BrowsingTopicsUtilTest,
       DeriveTopicsHeaderValue_OneTopic_ThreeVersionsInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(1, /*model_version=*/"2"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/3);

  EXPECT_EQ(header_value,
            "(1);v=chrome.1:1:2, "
            "();p=P0000000000000000000000000000000000000000000000000");
}

TEST_F(BrowsingTopicsUtilTest,
       DeriveTopicsHeaderValue_OneThreeDigitTopic_OneVersionInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(123,
                                    /*model_version=*/"2"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/1);

  EXPECT_EQ(header_value, "(123);v=chrome.1:1:2, ();p=P000000000");
}

TEST_F(BrowsingTopicsUtilTest,
       DeriveTopicsHeaderValue_TwoTopics_SameTopicVersions_OneVersionInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(1, /*model_version=*/"2"));
  topics.push_back(CreateMojomTopic(2, /*model_version=*/"2"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/1);

  EXPECT_EQ(header_value, "(1 2);v=chrome.1:1:2, ();p=P000000000");
}

TEST_F(
    BrowsingTopicsUtilTest,
    DeriveTopicsHeaderValue_TwoMixedDigitsTopics_SameTopicVersions_OneVersionInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(123, /*model_version=*/"2"));
  topics.push_back(CreateMojomTopic(45, /*model_version=*/"2"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/1);

  EXPECT_EQ(header_value, "(123 45);v=chrome.1:1:2, ();p=P000000");
}

TEST_F(
    BrowsingTopicsUtilTest,
    DeriveTopicsHeaderValue_TwoTopics_SameTopicVersions_TwoVersionsInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(1, /*model_version=*/"2"));
  topics.push_back(CreateMojomTopic(2, /*model_version=*/"2"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/2);

  EXPECT_EQ(header_value,
            "(1 2);v=chrome.1:1:2, ();p=P0000000000000000000000000000");
}

TEST_F(
    BrowsingTopicsUtilTest,
    DeriveTopicsHeaderValue_TwoTopics_DifferentTopicVersions_TwoVersionsInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(1, /*model_version=*/"2"));
  topics.push_back(CreateMojomTopic(1, /*model_version=*/"4"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/2);

  EXPECT_EQ(header_value,
            "(1);v=chrome.1:1:2, (1);v=chrome.1:1:4, ();p=P0000000000");
}

TEST_F(
    BrowsingTopicsUtilTest,
    DeriveTopicsHeaderValue_TwoTopics_DifferentTopicVersions_ThreeVersionsInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(1, /*model_version=*/"2"));
  topics.push_back(CreateMojomTopic(1, /*model_version=*/"4"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/3);
  EXPECT_EQ(header_value,
            "(1);v=chrome.1:1:2, (1);v=chrome.1:1:4, "
            "();p=P00000000000000000000000000000");
}

TEST_F(
    BrowsingTopicsUtilTest,
    DeriveTopicsHeaderValue_ThreeTopics_SameTopicVersions_OneVersionInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(1, /*model_version=*/"2"));
  topics.push_back(CreateMojomTopic(2, /*model_version=*/"2"));
  topics.push_back(CreateMojomTopic(3, /*model_version=*/"2"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/1);

  EXPECT_EQ(header_value, "(1 2 3);v=chrome.1:1:2, ();p=P0000000");
}

TEST_F(
    BrowsingTopicsUtilTest,
    DeriveTopicsHeaderValue_ThreeThreeDigitsTopics_SameTopicVersions_OneVersionInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(100, /*model_version=*/"20"));
  topics.push_back(CreateMojomTopic(200, /*model_version=*/"20"));
  topics.push_back(CreateMojomTopic(300, /*model_version=*/"20"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/1);

  EXPECT_EQ(header_value, "(100 200 300);v=chrome.1:1:20, ();p=P");
}

TEST_F(
    BrowsingTopicsUtilTest,
    DeriveTopicsHeaderValue_ThreeThreeDigitsTopics_FirstTwoTopicVersionsSame_TwoVersionsInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(100, /*model_version=*/"2"));
  topics.push_back(CreateMojomTopic(200, /*model_version=*/"2"));
  topics.push_back(CreateMojomTopic(300, /*model_version=*/"4"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/2);

  EXPECT_EQ(header_value,
            "(100 200);v=chrome.1:1:2, (300);v=chrome.1:1:4, ();p=P00");
}

TEST_F(
    BrowsingTopicsUtilTest,
    DeriveTopicsHeaderValue_ThreeThreeDigitsTopics_LastTwoTopicVersionsSame_TwoVersionsInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(100, /*model_version=*/"2"));
  topics.push_back(CreateMojomTopic(200, /*model_version=*/"4"));
  topics.push_back(CreateMojomTopic(300, /*model_version=*/"4"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/2);

  EXPECT_EQ(header_value,
            "(100);v=chrome.1:1:2, (200 300);v=chrome.1:1:4, ();p=P00");
}

TEST_F(BrowsingTopicsUtilTest,
       DeriveTopicsHeaderValue_ThreeThreeDigitsTopics_ThreeTopicVersions) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(100, /*model_version=*/"20"));
  topics.push_back(CreateMojomTopic(200, /*model_version=*/"40"));
  topics.push_back(CreateMojomTopic(300, /*model_version=*/"60"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/3);

  EXPECT_EQ(header_value,
            "(100);v=chrome.1:1:20, (200);v=chrome.1:1:40, "
            "(300);v=chrome.1:1:60, ();p=P");
}

TEST_F(
    BrowsingTopicsUtilTest,
    DeriveTopicsHeaderValue_InconsistentNumTopicsVersionsAndNumVersionsInEpochs) {
  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(100, /*model_version=*/"20"));
  topics.push_back(CreateMojomTopic(200, /*model_version=*/"40"));
  topics.push_back(CreateMojomTopic(300, /*model_version=*/"60"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/2);

  EXPECT_EQ(header_value,
            "(100);v=chrome.1:1:20, (200);v=chrome.1:1:40, "
            "(300);v=chrome.1:1:60, ();p=P");
}

TEST_F(BrowsingTopicsUtilTest,
       DeriveTopicsHeaderValue_LengthExceedsDefaultMax_NoPadding) {
  std::string config_version = base::StrCat(
      {"chrome.",
       base::NumberToString(browsing_topics::ConfigVersion::kMaxValue)});
  std::string taxonomy_version = base::NumberToString(
      blink::features::kBrowsingTopicsTaxonomyVersion.Get());

  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics.push_back(CreateMojomTopic(100, /*model_version=*/"20"));
  topics.push_back(CreateMojomTopic(200, /*model_version=*/"40"));
  topics.push_back(CreateMojomTopic(300, /*model_version=*/"600"));

  std::string header_value =
      DeriveTopicsHeaderValue(topics, /*num_versions_in_epochs=*/3);

  EXPECT_EQ(header_value,
            "(100);v=chrome.1:1:20, (200);v=chrome.1:1:40, "
            "(300);v=chrome.1:1:600, ();p=P");
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
