// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

const char kDefaultGCPKeyURL[] = "https://example.com/default_keys";
const char kOtherDefaultGCPKeyURL[] = "https://example.com/other_keys";

const char kCoordinator1[] = "https://first.coordinator.test";
const char kCoordinator1KeyURL[] = "https://example.com/first_keys";

const char kCoordinator2[] = "https://second.coordinator.test";
const char kCoordinator2KeyURL[] = "https://example.com/second_keys";

class BiddingAndAuctionServerKeyFetcherTest : public testing::Test {
 public:
  void SetUp() override {
    std::string kKeyConfig = base::StringPrintf(
        R"({
"%s": "%s",
"%s": "%s",
"%s": "%s"
      })",
        kCoordinator1, kCoordinator1KeyURL, kCoordinator2, kCoordinator2KeyURL,
        kDefaultBiddingAndAuctionGCPCoordinatorOrigin, kDefaultGCPKeyURL);
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{blink::features::kFledgeBiddingAndAuctionServer,
                               {{"FledgeBiddingAndAuctionKeyConfig",
                                 kKeyConfig}}}},
        /*disabled_features=*/{});
  }

  url::Origin CoordinatorOrigin() {
    return url::Origin::Create(
        GURL(kDefaultBiddingAndAuctionGCPCoordinatorOrigin));
  }

 protected:
  network::TestURLLoaderFactory url_loader_factory_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  data_decoder::test::InProcessDataDecoder decoder_;
};

TEST_F(BiddingAndAuctionServerKeyFetcherTest, UnknownCoordinator) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{blink::features::kFledgeBiddingAndAuctionServer,
                             {{"FledgeBiddingAndAuctionKeyConfig", ""},
                              {"FledgeBiddingAndAuctionKeyURL", ""}}}},
      /*disabled_features=*/{});
  content::BiddingAndAuctionServerKeyFetcher fetcher;

  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      &url_loader_factory_, url::Origin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_FALSE(maybe_key.has_value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, NoURL) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{blink::features::kFledgeBiddingAndAuctionServer,
                             {{"FledgeBiddingAndAuctionKeyConfig", ""},
                              {"FledgeBiddingAndAuctionKeyURL", ""}}}},
      /*disabled_features=*/{});
  content::BiddingAndAuctionServerKeyFetcher fetcher;

  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      &url_loader_factory_, CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_FALSE(maybe_key.has_value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, BadResponses) {
  content::BiddingAndAuctionServerKeyFetcher fetcher;

  std::vector<std::string> bad_responses = {
      "",
      "abc",  // Not valid JSON
      "1",
      R"("keys")",
      "[]",
      "{}",
      R"({ "keys": {}})",
      R"({ "keys": []})",
      R"({ "keys": [{ "key": 1}]})",
      R"({ "keys": [{ "key": "abc/_#$\n\n"}]})",
      R"({ "keys": [{
      "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="}]})",
      R"({ "keys": [{
      "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
      "id": 1
      }]})",
      R"({ "keys": [{
      "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
      "id": ""
      }]})"};

  for (const auto& response : bad_responses) {
    SCOPED_TRACE(response);
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(&url_loader_factory_, CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_FALSE(maybe_key.has_value());
                                run_loop.Quit();
                              }));
    EXPECT_TRUE(url_loader_factory_.SimulateResponseForPendingRequest(
        kDefaultGCPKeyURL, response));
    run_loop.Run();
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, FailsAll) {
  content::BiddingAndAuctionServerKeyFetcher fetcher;
  content::BiddingAndAuctionServerKey key1, key2;
  int completed = 0;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      &url_loader_factory_, CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_FALSE(maybe_key.has_value());
        completed++;
      }));
  fetcher.GetOrFetchKey(
      &url_loader_factory_, CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_FALSE(maybe_key.has_value());
        completed++;
        run_loop.Quit();
      }));
  EXPECT_TRUE(url_loader_factory_.SimulateResponseForPendingRequest(
      kDefaultGCPKeyURL, "", net::HTTP_NOT_FOUND));
  run_loop.Run();
  EXPECT_EQ(2, completed);
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, RequestDuringFailure) {
  content::BiddingAndAuctionServerKeyFetcher fetcher;
  content::BiddingAndAuctionServerKey key1, key2;
  int completed = 0;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      &url_loader_factory_, CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_FALSE(maybe_key.has_value());
        completed++;
        fetcher.GetOrFetchKey(&url_loader_factory_, CoordinatorOrigin(),
                              base::BindLambdaForTesting(
                                  [&](base::expected<BiddingAndAuctionServerKey,
                                                     std::string> maybe_key) {
                                    EXPECT_FALSE(maybe_key.has_value());
                                    completed++;
                                    run_loop.Quit();
                                  }));
      }));
  EXPECT_TRUE(url_loader_factory_.SimulateResponseForPendingRequest(
      kDefaultGCPKeyURL, "", net::HTTP_NOT_FOUND));
  run_loop.Run();
  EXPECT_EQ(2, completed);
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, GoodResponse) {
  content::BiddingAndAuctionServerKeyFetcher fetcher;

  content::BiddingAndAuctionServerKey key;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      &url_loader_factory_, CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_TRUE(maybe_key.has_value());
        key = *maybe_key;
        run_loop.Quit();
      }));
  EXPECT_TRUE(
      url_loader_factory_.SimulateResponseForPendingRequest(kDefaultGCPKeyURL,
                                                            R"({ "keys": [{
      "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
      "id": "12345678-9abc-def0-1234-56789abcdef0"
      }]})"));
  run_loop.Run();
  EXPECT_EQ(0x12, key.id);
  EXPECT_EQ(std::string(32, '\0'), key.key);
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, RequestDuringSuccess) {
  content::BiddingAndAuctionServerKeyFetcher fetcher;
  int completed = 0;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      &url_loader_factory_, CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_TRUE(maybe_key.has_value());
        completed++;
        fetcher.GetOrFetchKey(&url_loader_factory_, CoordinatorOrigin(),
                              base::BindLambdaForTesting(
                                  [&](base::expected<BiddingAndAuctionServerKey,
                                                     std::string> maybe_key) {
                                    EXPECT_TRUE(maybe_key.has_value());
                                    completed++;
                                    run_loop.Quit();
                                  }));
      }));
  EXPECT_TRUE(
      url_loader_factory_.SimulateResponseForPendingRequest(kDefaultGCPKeyURL,
                                                            R"({ "keys": [{
      "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
      "id": "12345678-9abc-def0-1234-56789abcdef0"
      }]})"));
  run_loop.Run();
  EXPECT_EQ(2, completed);
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, CachesValue) {
  content::BiddingAndAuctionServerKeyFetcher fetcher;

  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(&url_loader_factory_, CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    EXPECT_TRUE(
        url_loader_factory_.SimulateResponseForPendingRequest(kDefaultGCPKeyURL,
                                                              R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})"));
    run_loop.Run();
    EXPECT_EQ(0x12, key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
  }

  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(&url_loader_factory_, CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    // Shouldn't use this response (it should still be cached).
    EXPECT_FALSE(
        url_loader_factory_.SimulateResponseForPendingRequest(kDefaultGCPKeyURL,
                                                              R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE\u003d",
        "id": "23456789-abcd-ef01-2345-6789abcdef01"
        }]})"));
    run_loop.Run();
    EXPECT_EQ(0x12, key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, CoalescesRequests) {
  content::BiddingAndAuctionServerKeyFetcher fetcher;

  {
    content::BiddingAndAuctionServerKey key1, key2;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(&url_loader_factory_, CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                key1 = *maybe_key;
                              }));
    fetcher.GetOrFetchKey(&url_loader_factory_, CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                key2 = *maybe_key;
                                EXPECT_TRUE(maybe_key.has_value());
                                run_loop.Quit();
                              }));
    EXPECT_TRUE(
        url_loader_factory_.SimulateResponseForPendingRequest(kDefaultGCPKeyURL,
                                                              R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})"));
    run_loop.Run();
    EXPECT_EQ(0x12, key1.id);
    EXPECT_EQ(std::string(32, '\0'), key1.key);

    EXPECT_EQ(0x12, key2.id);
    EXPECT_EQ(std::string(32, '\0'), key2.key);
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, ChoosesRandomKey) {
  content::BiddingAndAuctionServerKeyFetcher fetcher;

  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(&url_loader_factory_, CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    EXPECT_TRUE(
        url_loader_factory_.SimulateResponseForPendingRequest(kDefaultGCPKeyURL,
                                                              R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }, {
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE=",
        "id": "23456789-abcd-ef01-2345-6789abcdef01"
        }]})"));
    run_loop.Run();
  }

  std::set<uint8_t> ids;
  while (ids.size() < 2) {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(&url_loader_factory_, CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    ids.insert(key.id);
  }
  EXPECT_THAT(ids, testing::ElementsAre(0x12, 0x23));
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, OverridesConfig) {
  base::test::ScopedFeatureList feature_list;
  std::string kKeyConfig = base::StringPrintf(
      R"({
"%s": "%s",
"%s": "%s",
"%s": "%s"
      })",
      kCoordinator1, kCoordinator1KeyURL, kCoordinator2, kCoordinator2KeyURL,
      kDefaultBiddingAndAuctionGCPCoordinatorOrigin, kDefaultGCPKeyURL);
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{blink::features::kFledgeBiddingAndAuctionServer,
                             {{"FledgeBiddingAndAuctionKeyConfig", kKeyConfig},
                              {"FledgeBiddingAndAuctionKeyURL",
                               kOtherDefaultGCPKeyURL}}}},
      /*disabled_features=*/{});
  content::BiddingAndAuctionServerKeyFetcher fetcher;

  content::BiddingAndAuctionServerKey key;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      &url_loader_factory_, CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_TRUE(maybe_key.has_value());
        key = *maybe_key;
        run_loop.Quit();
      }));
  EXPECT_TRUE(url_loader_factory_.SimulateResponseForPendingRequest(
      kOtherDefaultGCPKeyURL,
      R"({ "keys": [{
      "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
      "id": "12345678-9abc-def0-1234-56789abcdef0"
      }]})"));
  run_loop.Run();
  EXPECT_EQ(0x12, key.id);
  EXPECT_EQ(std::string(32, '\0'), key.key);
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, NoConfigOnlyURL) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{blink::features::kFledgeBiddingAndAuctionServer,
                             {{"FledgeBiddingAndAuctionKeyURL",
                               kOtherDefaultGCPKeyURL}}}},
      /*disabled_features=*/{});
  content::BiddingAndAuctionServerKeyFetcher fetcher;

  content::BiddingAndAuctionServerKey key;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      &url_loader_factory_, CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_TRUE(maybe_key.has_value());
        key = *maybe_key;
        run_loop.Quit();
      }));
  EXPECT_TRUE(url_loader_factory_.SimulateResponseForPendingRequest(
      kOtherDefaultGCPKeyURL,
      R"({ "keys": [{
      "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
      "id": "12345678-9abc-def0-1234-56789abcdef0"
      }]})"));
  run_loop.Run();
  EXPECT_EQ(0x12, key.id);
  EXPECT_EQ(std::string(32, '\0'), key.key);
}

class BiddingAndAuctionServerKeyFetcherCoordinatorTest
    : public BiddingAndAuctionServerKeyFetcherTest,
      public ::testing::WithParamInterface<int> {
 public:
  url::Origin GetCoordinator() {
    switch (GetParam()) {
      case 0:
        return url::Origin::Create(
            GURL(kDefaultBiddingAndAuctionGCPCoordinatorOrigin));
      case 1:
        return url::Origin::Create(GURL(kCoordinator1));
      case 2:
        return url::Origin::Create(GURL(kCoordinator2));
    }
    NOTREACHED_NORETURN();
  }

  std::string GetURL() {
    switch (GetParam()) {
      case 0:
        return kDefaultGCPKeyURL;
      case 1:
        return kCoordinator1KeyURL;
      case 2:
        return kCoordinator2KeyURL;
    }
    NOTREACHED_NORETURN();
  }
};

TEST_P(BiddingAndAuctionServerKeyFetcherCoordinatorTest, GoodResponse) {
  content::BiddingAndAuctionServerKeyFetcher fetcher;

  content::BiddingAndAuctionServerKey key;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      &url_loader_factory_, GetCoordinator(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_TRUE(maybe_key.has_value());
        key = *maybe_key;
        run_loop.Quit();
      }));
  EXPECT_TRUE(
      url_loader_factory_.SimulateResponseForPendingRequest(GetURL(),
                                                            R"({ "keys": [{
      "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
      "id": "12345678-9abc-def0-1234-56789abcdef0"
      }]})"));
  run_loop.Run();
  EXPECT_EQ(0x12, key.id);
  EXPECT_EQ(std::string(32, '\0'), key.key);
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    BiddingAndAuctionServerKeyFetcherCoordinatorTest,
    ::testing::Range(0, 3));

}  // namespace
}  // namespace content
