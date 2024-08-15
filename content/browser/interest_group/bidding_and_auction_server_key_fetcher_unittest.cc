// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

const char kDefaultGCPKeyURL[] = "https://example.com/default_keys";
const char kOtherDefaultGCPKeyURL[] = "https://example.com/other_keys";

const char kCoordinator1[] = "https://first.coordinator.test/";
const char kCoordinator1KeyURL[] = "https://example.com/first_keys";

const char kCoordinator2[] = "https://second.coordinator.test/";
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
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    manager_ = std::make_unique<InterestGroupManagerImpl>(
        temp_directory_.GetPath(), false,
        InterestGroupManagerImpl::ProcessMode::kDedicated, nullptr,
        base::NullCallback());
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
  }

  url::Origin CoordinatorOrigin() {
    return url::Origin::Create(
        GURL(kDefaultBiddingAndAuctionGCPCoordinatorOrigin));
  }

 protected:
  std::pair<base::Time, std::vector<BiddingAndAuctionServerKey>>
  GetDBStoredKeysWithExpiration(const url::Origin& coordinator) {
    std::pair<base::Time, std::vector<BiddingAndAuctionServerKey>>
        expiration_and_keys;
    base::RunLoop run_loop;
    manager_->GetBiddingAndAuctionServerKeys(
        coordinator,
        base::BindLambdaForTesting(
            [&](std::pair<base::Time, std::vector<BiddingAndAuctionServerKey>>
                    stored_keys) {
              expiration_and_keys = std::move(stored_keys);
              run_loop.Quit();
            }));
    run_loop.Run();
    return expiration_and_keys;
  }

  void SetDBStoredKeys(const url::Origin& coordinator,
                       std::vector<BiddingAndAuctionServerKey> keys,
                       base::Time expiration) {
    manager_->SetBiddingAndAuctionServerKeys(coordinator, keys, expiration);
  }

  content::BiddingAndAuctionServerKeyFetcher CreateFetcher() {
    return BiddingAndAuctionServerKeyFetcher(manager_.get(),
                                             shared_url_loader_factory_);
  }

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  data_decoder::test::InProcessDataDecoder decoder_;
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<InterestGroupManagerImpl> manager_;
};

TEST_F(BiddingAndAuctionServerKeyFetcherTest, UnknownCoordinator) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{blink::features::kFledgeBiddingAndAuctionServer,
                             {{"FledgeBiddingAndAuctionKeyConfig", ""},
                              {"FledgeBiddingAndAuctionKeyURL", ""}}}},
      /*disabled_features=*/{});
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      url::Origin(),
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
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_FALSE(maybe_key.has_value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, BadResponses) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

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
    // AddResponse overwrites the previous response.
    url_loader_factory_.AddResponse(kDefaultGCPKeyURL, response);
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_FALSE(maybe_key.has_value());
                                run_loop.Quit();
                              }));
    run_loop.Run();
  }
  EXPECT_EQ(bad_responses.size(), url_loader_factory_.total_requests());
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, FailsAll) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
  content::BiddingAndAuctionServerKey key1, key2;
  int completed = 0;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_FALSE(maybe_key.has_value());
        completed++;
      }));
  fetcher.GetOrFetchKey(
      CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_FALSE(maybe_key.has_value());
        completed++;
        run_loop.Quit();
      }));
  url_loader_factory_.AddResponse(kDefaultGCPKeyURL, "", net::HTTP_NOT_FOUND);
  run_loop.Run();
  EXPECT_EQ(2, completed);
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, RequestDuringFailure) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
  content::BiddingAndAuctionServerKey key1, key2;
  int completed = 0;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_FALSE(maybe_key.has_value());
        completed++;
        fetcher.GetOrFetchKey(CoordinatorOrigin(),
                              base::BindLambdaForTesting(
                                  [&](base::expected<BiddingAndAuctionServerKey,
                                                     std::string> maybe_key) {
                                    EXPECT_FALSE(maybe_key.has_value());
                                    completed++;
                                    run_loop.Quit();
                                  }));
      }));
  url_loader_factory_.AddResponse(kDefaultGCPKeyURL, "", net::HTTP_NOT_FOUND);
  run_loop.Run();
  EXPECT_EQ(2, completed);
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, GoodResponse) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  url_loader_factory_.AddResponse(kDefaultGCPKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  content::BiddingAndAuctionServerKey key;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_TRUE(maybe_key.has_value());
        key = *maybe_key;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(0x12, key.id);
  EXPECT_EQ(std::string(32, '\0'), key.key);
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, RequestDuringSuccess) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
  url_loader_factory_.AddResponse(kDefaultGCPKeyURL,
                                  R"({ "keys": [{
      "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
      "id": "12345678-9abc-def0-1234-56789abcdef0"
      }]})");
  int completed = 0;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_TRUE(maybe_key.has_value());
        completed++;
        fetcher.GetOrFetchKey(CoordinatorOrigin(),
                              base::BindLambdaForTesting(
                                  [&](base::expected<BiddingAndAuctionServerKey,
                                                     std::string> maybe_key) {
                                    EXPECT_TRUE(maybe_key.has_value());
                                    completed++;
                                    run_loop.Quit();
                                  }));
      }));
  run_loop.Run();
  EXPECT_EQ(2, completed);
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, CachesValue) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
  url_loader_factory_.AddResponse(kDefaultGCPKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ(0x12, key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }

  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ(0x12, key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    // The response was cached so there was still only 1 request.
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, ReadsValuesCachedInDBIfEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFledgeStoreBandAKeysInDB);
  url_loader_factory_.AddResponse(kDefaultGCPKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
  std::vector<BiddingAndAuctionServerKey> keys;
  BiddingAndAuctionServerKey key;
  key.id = 1;
  key.key = "a";
  keys.push_back(key);
  SetDBStoredKeys(CoordinatorOrigin(), keys, base::Time::Now() + base::Days(2));
  task_environment_.RunUntilIdle();

  {
    content::BiddingAndAuctionServerKey returned_key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                returned_key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ(key.id, returned_key.id);
    EXPECT_EQ(key.key, returned_key.key);
    // Values should have been retrieved from the database, not the
    // url_loader_factory_.
    EXPECT_EQ(0u, url_loader_factory_.total_requests());
  }

  // Fast forward past the expiration time stored in the database to make sure
  // it was respected.
  task_environment_.FastForwardBy(base::Days(2) + base::Seconds(1));

  {
    // This should make a new request to the network now.
    content::BiddingAndAuctionServerKey returned_key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                returned_key = *maybe_key;
                                run_loop.Quit();
                              }));
    task_environment_.RunUntilIdle();
    run_loop.Run();
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
    EXPECT_EQ(0x12, returned_key.id);
    EXPECT_EQ(std::string(32, '\0'), returned_key.key);
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, WritesValuesToDBIfEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFledgeStoreBandAKeysInDB);

  url_loader_factory_.AddResponse(kDefaultGCPKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  content::BiddingAndAuctionServerKey key;
  {
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ(0x12, key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }
  task_environment_.RunUntilIdle();
  std::pair<base::Time, std::vector<content::BiddingAndAuctionServerKey>>
      expiration_and_stored_keys =
          GetDBStoredKeysWithExpiration(CoordinatorOrigin());
  EXPECT_EQ(base::Time::Now() + base::Days(7),
            expiration_and_stored_keys.first);
  ASSERT_EQ(1u, expiration_and_stored_keys.second.size());
  EXPECT_EQ(expiration_and_stored_keys.second[0].key, key.key);
  EXPECT_EQ(expiration_and_stored_keys.second[0].id, key.id);
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest,
       MaybePrefetchKeysFailureFailsPendingGetOrFetchKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFledgePrefetchBandAKeys);

  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  fetcher.MaybePrefetchKeys();

  {
    url_loader_factory_.AddResponse(kDefaultGCPKeyURL, "", net::HTTP_NOT_FOUND);
    bool completed = false;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_FALSE(maybe_key.has_value());
                                completed = true;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_TRUE(completed);
  }

  // The following GetOrFetchKey after the prefetch failure should be
  // successful.
  {
    url_loader_factory_.AddResponse(kDefaultGCPKeyURL,
                                    R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ(0x12, key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, MaybePrefetchKeysCachesValue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFledgePrefetchBandAKeys);

  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  fetcher.MaybePrefetchKeys();
  task_environment_.RunUntilIdle();
  // MaybePrefetchKeys will try to fetch all keys in the
  // FledgeBiddingAndAuctionKeyConfig.
  EXPECT_TRUE(url_loader_factory_.IsPending(kDefaultGCPKeyURL));
  EXPECT_TRUE(url_loader_factory_.IsPending(kCoordinator1KeyURL));
  EXPECT_TRUE(url_loader_factory_.IsPending(kCoordinator2KeyURL));
  EXPECT_TRUE(
      url_loader_factory_.SimulateResponseForPendingRequest(kDefaultGCPKeyURL,
                                                            R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})"));
  task_environment_.RunUntilIdle();

  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
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

  // Keys should not be fetched a second time.
  fetcher.MaybePrefetchKeys();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(url_loader_factory_.IsPending(kDefaultGCPKeyURL));
  EXPECT_FALSE(
      url_loader_factory_.SimulateResponseForPendingRequest(kDefaultGCPKeyURL,
                                                            R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})"));
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest,
       MaybePrefetchKeysUpdatesExpiredValue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFledgePrefetchBandAKeys);

  url_loader_factory_.AddResponse(kDefaultGCPKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  // Get a key that we will let expire.
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));

    run_loop.Run();
    EXPECT_EQ(0x12, key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }

  // Let the key expire.
  task_environment_.RunUntilIdle();
  task_environment_.FastForwardBy(base::Days(7) + base::Seconds(1));
  url_loader_factory_.ClearResponses();

  fetcher.MaybePrefetchKeys();
  task_environment_.RunUntilIdle();
  // We should still try fetching all the keys, including for kDefaultGCPKeyURL.
  EXPECT_TRUE(url_loader_factory_.IsPending(kDefaultGCPKeyURL));
  EXPECT_TRUE(url_loader_factory_.IsPending(kCoordinator1KeyURL));
  EXPECT_TRUE(url_loader_factory_.IsPending(kCoordinator2KeyURL));
  EXPECT_TRUE(
      url_loader_factory_.SimulateResponseForPendingRequest(kDefaultGCPKeyURL,
                                                            R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE\u003d",
        "id": "23456789-abcd-ef01-2345-6789abcdef01"
        }]})"));
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest,
       MaybePrefetchKeysDoesNotCacheValueIfFeatureDisabled) {
  {
    // Disable kFledgePrefetchBandAKeys.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kFledgePrefetchBandAKeys);

    content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
    fetcher.MaybePrefetchKeys();
    task_environment_.RunUntilIdle();
    EXPECT_FALSE(url_loader_factory_.IsPending(kDefaultGCPKeyURL));
    EXPECT_FALSE(url_loader_factory_.IsPending(kCoordinator1KeyURL));
    EXPECT_FALSE(url_loader_factory_.IsPending(kCoordinator2KeyURL));
  }

  {
    // Disable kFledgeBiddingAndAuctionServer.
    base::test::ScopedFeatureList feature_list;

    feature_list.InitWithFeatures(
        {features::kFledgePrefetchBandAKeys},
        {blink::features::kFledgeBiddingAndAuctionServer});
    content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
    fetcher.MaybePrefetchKeys();
    task_environment_.RunUntilIdle();
    EXPECT_FALSE(url_loader_factory_.IsPending(kDefaultGCPKeyURL));
    EXPECT_FALSE(url_loader_factory_.IsPending(kCoordinator1KeyURL));
    EXPECT_FALSE(url_loader_factory_.IsPending(kCoordinator2KeyURL));
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, CoalescesRequests) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  url_loader_factory_.AddResponse(kDefaultGCPKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  {
    content::BiddingAndAuctionServerKey key1, key2;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                key1 = *maybe_key;
                              }));
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                key2 = *maybe_key;
                                EXPECT_TRUE(maybe_key.has_value());
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ(0x12, key1.id);
    EXPECT_EQ(std::string(32, '\0'), key1.key);

    EXPECT_EQ(0x12, key2.id);
    EXPECT_EQ(std::string(32, '\0'), key2.key);

    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, ChoosesRandomKey) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
  url_loader_factory_.AddResponse(kDefaultGCPKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }, {
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE=",
        "id": "23456789-abcd-ef01-2345-6789abcdef01"
        }]})");

  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                EXPECT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
  }

  std::set<uint8_t> ids;
  while (ids.size() < 2) {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(CoordinatorOrigin(),
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
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  url_loader_factory_.AddResponse(kOtherDefaultGCPKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  content::BiddingAndAuctionServerKey key;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_TRUE(maybe_key.has_value());
        key = *maybe_key;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(0x12, key.id);
  EXPECT_EQ(std::string(32, '\0'), key.key);
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, NoConfigOnlyURL) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{blink::features::kFledgeBiddingAndAuctionServer,
                             {{"FledgeBiddingAndAuctionKeyURL",
                               kOtherDefaultGCPKeyURL}}}},
      /*disabled_features=*/{});
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  url_loader_factory_.AddResponse(kOtherDefaultGCPKeyURL,
                                  R"({ "keys": [{
      "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
      "id": "12345678-9abc-def0-1234-56789abcdef0"
      }]})");

  content::BiddingAndAuctionServerKey key;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      CoordinatorOrigin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_TRUE(maybe_key.has_value());
        key = *maybe_key;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(0x12, key.id);
  EXPECT_EQ(std::string(32, '\0'), key.key);
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
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
    NOTREACHED();
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
    NOTREACHED();
  }
};

TEST_P(BiddingAndAuctionServerKeyFetcherCoordinatorTest, GoodResponse) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  url_loader_factory_.AddResponse(GetURL(),
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  content::BiddingAndAuctionServerKey key;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      GetCoordinator(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_TRUE(maybe_key.has_value());
        key = *maybe_key;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(0x12, key.id);
  EXPECT_EQ(std::string(32, '\0'), key.key);
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    BiddingAndAuctionServerKeyFetcherCoordinatorTest,
    ::testing::Range(0, 3));

}  // namespace
}  // namespace content
