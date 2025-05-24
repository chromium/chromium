// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"

#include "base/base64.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_storage.pb.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {

namespace {

using TrustedServerAPIType =
    BiddingAndAuctionServerKeyFetcher::TrustedServerAPIType;

const char kTestScope[] = "https://scope.origin.test/";
const char kTestScope2[] = "https://other.origin2.test/";
const char kOtherDefaultGCPKeyURL[] = "https://example.com/other_keys";

class BiddingAndAuctionServerKeyFetcherTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    manager_ = std::make_unique<InterestGroupManagerImpl>(
        temp_directory_.GetPath(), false,
        InterestGroupManagerImpl::ProcessMode::kDedicated, nullptr,
        base::NullCallback());
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
  }

 protected:
  std::pair<base::Time, std::string> GetDBStoredKeysWithExpiration(
      const url::Origin& coordinator) {
    std::pair<base::Time, std::string> expiration_and_keys;
    base::RunLoop run_loop;
    manager_->GetBiddingAndAuctionServerKeys(
        coordinator, base::BindLambdaForTesting(
                         [&](std::pair<base::Time, std::string> stored_keys) {
                           expiration_and_keys = std::move(stored_keys);
                           run_loop.Quit();
                         }));
    run_loop.Run();
    return expiration_and_keys;
  }

  void SetDBStoredKeys(const url::Origin& coordinator,
                       std::string serialized_keys,
                       base::Time expiration) {
    manager_->SetBiddingAndAuctionServerKeys(coordinator, serialized_keys,
                                             expiration);
  }

  void ExpectNoDBStoredKeys(const url::Origin& coordinator) {
    auto [time, val] = GetDBStoredKeysWithExpiration(coordinator);
    EXPECT_EQ(val, "") << coordinator;
  }

  content::BiddingAndAuctionServerKeyFetcher CreateFetcher() {
    return BiddingAndAuctionServerKeyFetcher(manager_.get(),
                                             shared_url_loader_factory_);
  }

  const url::Origin kTestScopeOrigin = url::Origin::Create(GURL(kTestScope));
  const url::Origin kTestScopeOrigin2 = url::Origin::Create(GURL(kTestScope2));
  const url::Origin kCoordinatorOrigin =
      url::Origin::Create(GURL(kDefaultBiddingAndAuctionGCPCoordinatorOrigin));
  const url::Origin kGCPCoordinatorOrigin =
      url::Origin::Create(GURL(kBiddingAndAuctionGCPCoordinatorOrigin));
  const url::Origin kAWSCoordinatorOrigin =
      url::Origin::Create(GURL(kBiddingAndAuctionAWSCoordinatorOrigin));

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  data_decoder::test::InProcessDataDecoder decoder_;
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<InterestGroupManagerImpl> manager_;
};

TEST_F(BiddingAndAuctionServerKeyFetcherTest, UnknownCoordinator) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      TrustedServerAPIType::kBiddingAndAuction, kTestScopeOrigin, url::Origin(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_FALSE(maybe_key.has_value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, WrongAPIType) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      TrustedServerAPIType::kInvalid, kTestScopeOrigin, kCoordinatorOrigin,
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        ASSERT_FALSE(maybe_key.has_value());
        EXPECT_EQ("API not supported by coordinator", maybe_key.error());
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
      TrustedServerAPIType::kBiddingAndAuction, kTestScopeOrigin,
      kCoordinatorOrigin,
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
    url_loader_factory_.AddResponse(kBiddingAndAuctionGCPCoordinatorKeyURL,
                                    response);
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
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

TEST_F(BiddingAndAuctionServerKeyFetcherTest, BadResponsesV2) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kFledgeOriginScopedKeys,
      {{"FledgeOriginScopedKeyConfig",
        base::StringPrintf(R"({"%s": "%s"})", kCoordinatorOrigin.Serialize(),
                           kOtherDefaultGCPKeyURL)}});

  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  std::vector<std::string> bad_responses = {
      "",
      "abc",  // Not valid JSON
      // Wrong types
      "1",
      R"("keys")",
      "[]",
      // Missing required "originScopedKeys" field.
      "{}",
      // Wrong type for "originScopedKeys".
      R"({ "originScopedKeys": []})",
      // No origins specified
      R"({ "originScopedKeys": {}})",
      // Invalid scoped origin
      R"({
        "originScopedKeys": {
          "notAnOrigin" : {
            "keys": [{
              "id": "AA arbitrary key beginning with two hex chars",
              "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d"
        }]}})",
      // Invalid type for scoped origin.
      R"({"originScopedKeys": {"https://scope.origin.test/" : []}})",
      // Missing required "keys" field.
      R"({"originScopedKeys": {"https://scope.origin.test/" : {}}})",
      // Wrong type for "keys".
      R"({
        "originScopedKeys": {
          "https://scope.origin.test/" : {
            "keys": {}
      }}})",
      // Empty list for "keys"
      R"({
        "originScopedKeys": {
          "https://scope.origin.test/" : {
            "keys": []
      }}})",
      // Key missing required "id" field.
      R"({
        "originScopedKeys": {
          "https://scope.origin.test/" : {
            "keys": [{
              "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d"
              }]
      }}})",
      // Wrong type for "id" field.
      R"({
        "originScopedKeys": {
          "https://scope.origin.test/" : {
            "keys": [{
              "id": 1,
              "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d"
              }]
      }}})",
      // "id" field must begin with two hex characters
      R"({
        "originScopedKeys": {
          "https://scope.origin.test/" : {
            "keys": [{
              "id": "No hex here",
              "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d"
              }]
      }}})",
      // Key missing required "key" field.
      R"({
        "originScopedKeys": {
          "https://scope.origin.test/" : {
            "keys": [{
              "id": "AA arbitrary key beginning with two hex chars",
              }]
      }}})",
      // Wrong type for "key" field.
      R"({
        "originScopedKeys": {
          "https://scope.origin.test/" : {
            "keys": [{
              "id": "AA arbitrary key beginning with two hex chars",
              "key": [0,1,2,3,4]
              }]
      }}})",
      // Two origins, one invalid
      R"({
        "originScopedKeys": {
          "https://scope.origin.test/" : {
            "keys": [{
                "id": "AA arbitrary key beginning with two hex chars",
                "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d"
              }]
          },
          "https://other.origin2.test": {}
      }}})",
  };

  for (const auto& response : bad_responses) {
    SCOPED_TRACE(response);
    base::RunLoop run_loop;
    // AddResponse overwrites the previous response.
    url_loader_factory_.AddResponse(kOtherDefaultGCPKeyURL, response);
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
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
      TrustedServerAPIType::kBiddingAndAuction, kTestScopeOrigin,
      kCoordinatorOrigin,
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_FALSE(maybe_key.has_value());
        completed++;
      }));
  fetcher.GetOrFetchKey(
      TrustedServerAPIType::kBiddingAndAuction, kTestScopeOrigin,
      kCoordinatorOrigin,
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_FALSE(maybe_key.has_value());
        completed++;
        run_loop.Quit();
      }));
  url_loader_factory_.AddResponse(kBiddingAndAuctionGCPCoordinatorKeyURL, "",
                                  net::HTTP_NOT_FOUND);
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
      TrustedServerAPIType::kBiddingAndAuction, kTestScopeOrigin,
      kCoordinatorOrigin,
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        EXPECT_FALSE(maybe_key.has_value());
        completed++;
        fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                              kTestScopeOrigin, kCoordinatorOrigin,
                              base::BindLambdaForTesting(
                                  [&](base::expected<BiddingAndAuctionServerKey,
                                                     std::string> maybe_key) {
                                    EXPECT_FALSE(maybe_key.has_value());
                                    completed++;
                                    run_loop.Quit();
                                  }));
      }));
  url_loader_factory_.AddResponse(kBiddingAndAuctionGCPCoordinatorKeyURL, "",
                                  net::HTTP_NOT_FOUND);
  run_loop.Run();
  EXPECT_EQ(2, completed);
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, GoodResponse) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  url_loader_factory_.AddResponse(kBiddingAndAuctionGCPCoordinatorKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  content::BiddingAndAuctionServerKey key;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      TrustedServerAPIType::kBiddingAndAuction, kTestScopeOrigin,
      kCoordinatorOrigin,
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        ASSERT_TRUE(maybe_key.has_value());
        key = *maybe_key;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
  EXPECT_EQ(std::string(32, '\0'), key.key);
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, GoodResponseV2) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kFledgeOriginScopedKeys,
      {{"FledgeOriginScopedKeyConfig",
        base::StringPrintf(R"({"%s": "%s"})", kCoordinatorOrigin.Serialize(),
                           kOtherDefaultGCPKeyURL)}});

  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  url_loader_factory_.AddResponse(kOtherDefaultGCPKeyURL,
                                  R"(
{
  "originScopedKeys": {
    "https://scope.origin.test/" : {
      "keys" : [{
        "id": "AA arbitrary key beginning with two hex chars",
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d"
    }]},
    "https://other.origin2.test" : {
      "keys" : [{
        "id": "AA another key beginning with two hex chars",
        "key": "BBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d"
    }]}
  }
}
)");

  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("AA arbitrary key beginning with two hex chars", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }

  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin2, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("AA another key beginning with two hex chars", key.id);
    EXPECT_EQ(std::string("\x04\x10") + std::string(30, '\0'), key.key);
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, GoodResponseV1V2Coexist) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kFledgeOriginScopedKeys,
      {{"FledgeOriginScopedKeyConfig",
        base::StringPrintf(R"({"%s": "%s"})", kGCPCoordinatorOrigin.Serialize(),
                           kOtherDefaultGCPKeyURL)}});

  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  url_loader_factory_.AddResponse(kOtherDefaultGCPKeyURL,
                                  R"(
{
  "originScopedKeys": {
    "https://scope.origin.test/" : {
      "keys" : [{
        "id": "AA arbitrary key beginning with two hex chars",
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d"
    }]},
    "https://other.origin2.test" : {
      "keys" : [{
        "id": "AA another key beginning with two hex chars",
        "key": "BBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d"
    }]}
  }
}
)");

  url_loader_factory_.AddResponse(kBiddingAndAuctionAWSCoordinatorKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  // A v2 coordinator
  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kGCPCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("AA arbitrary key beginning with two hex chars", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }

  // A v1 coordinator
  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kAWSCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    EXPECT_EQ(2u, url_loader_factory_.total_requests());
  }

  // A v2 coordinator again
  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin2, kGCPCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("AA another key beginning with two hex chars", key.id);
    EXPECT_EQ(std::string("\x04\x10") + std::string(30, '\0'), key.key);
    // Reuse old cached value for this coordinator
    EXPECT_EQ(2u, url_loader_factory_.total_requests());
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, RequestDuringSuccess) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
  url_loader_factory_.AddResponse(kBiddingAndAuctionGCPCoordinatorKeyURL,
                                  R"({ "keys": [{
      "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
      "id": "12345678-9abc-def0-1234-56789abcdef0"
      }]})");
  int completed = 0;
  base::RunLoop run_loop;
  fetcher.GetOrFetchKey(
      TrustedServerAPIType::kBiddingAndAuction, kTestScopeOrigin,
      kCoordinatorOrigin,
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        ASSERT_TRUE(maybe_key.has_value());
        completed++;
        fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                              kTestScopeOrigin, kCoordinatorOrigin,
                              base::BindLambdaForTesting(
                                  [&](base::expected<BiddingAndAuctionServerKey,
                                                     std::string> maybe_key) {
                                    ASSERT_TRUE(maybe_key.has_value());
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
  url_loader_factory_.AddResponse(kBiddingAndAuctionGCPCoordinatorKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }

  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    // The response was cached so there was still only 1 request.
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, ReadsValuesCachedInDBIfEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFledgeStoreBandAKeysInDB);
  url_loader_factory_.AddResponse(kBiddingAndAuctionGCPCoordinatorKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
  std::vector<BiddingAndAuctionServerKey> keys;
  BiddingAndAuctionServerKey key;
  key.id = "12345678-9abc-def0-1234-56789abcdef0";
  key.key = "a";
  keys.push_back(key);
  BiddingAndAuctionKeySet keyset(std::move(keys));
  SetDBStoredKeys(kCoordinatorOrigin, keyset.AsBinaryProto(),
                  base::Time::Now() + base::Days(2));
  task_environment_.RunUntilIdle();

  {
    content::BiddingAndAuctionServerKey returned_key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
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
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                returned_key = *maybe_key;
                                run_loop.Quit();
                              }));
    task_environment_.RunUntilIdle();
    run_loop.Run();
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", returned_key.id);
    EXPECT_EQ(std::string(32, '\0'), returned_key.key);
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, WritesValuesToDBIfEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFledgeStoreBandAKeysInDB);

  url_loader_factory_.AddResponse(kBiddingAndAuctionGCPCoordinatorKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  content::BiddingAndAuctionServerKey key;
  {
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }
  task_environment_.RunUntilIdle();
  std::pair<base::Time, std::string> expiration_and_stored_keys =
      GetDBStoredKeysWithExpiration(kCoordinatorOrigin);
  EXPECT_EQ(base::Time::Now() + base::Days(7),
            expiration_and_stored_keys.first);
  BiddingAndAuctionKeySet keyset({key});
  EXPECT_EQ(expiration_and_stored_keys.second, keyset.AsBinaryProto());
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, PersistsKeysV2) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {
          {blink::features::kFledgeOriginScopedKeys,
           {{"FledgeOriginScopedKeyConfig",
             base::StringPrintf(R"({"%s": "%s"})",
                                kCoordinatorOrigin.Serialize(),
                                kOtherDefaultGCPKeyURL)}}},
          {features::kFledgeStoreBandAKeysInDB, {}},
      },
      {});

  // Initial fetch
  {
    content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

    url_loader_factory_.AddResponse(kOtherDefaultGCPKeyURL,
                                    R"(
{
  "originScopedKeys": {
    "https://scope.origin.test/" : {
      "keys" : [{
        "id": "AA arbitrary key beginning with two hex chars",
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d"
    }]},
    "https://other.origin2.test" : {
      "keys" : [{
        "id": "AA another key beginning with two hex chars",
        "key": "BBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d"
    }]}
  }
}
)");

    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("AA arbitrary key beginning with two hex chars", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }

  // Should be cached in the DB
  {
    // ensure any network fetches would fail.
    url_loader_factory_.ClearResponses();

    content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin2, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("AA another key beginning with two hex chars", key.id);
    EXPECT_EQ(std::string("\x04\x10") + std::string(30, '\0'), key.key);
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, HandlesV1ToV2Migration) {
  base::test::ScopedFeatureList outer_feature_list;
  outer_feature_list.InitAndEnableFeature(features::kFledgeStoreBandAKeysInDB);

  // V1 fetch
  {
    url_loader_factory_.AddResponse(kBiddingAndAuctionGCPCoordinatorKeyURL,
                                    R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

    content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }

  // V1->V2 migration - discard result from DB and fetch again
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        blink::features::kFledgeOriginScopedKeys,
        {{"FledgeOriginScopedKeyConfig",
          base::StringPrintf(R"({"%s": "%s"})", kCoordinatorOrigin.Serialize(),
                             kOtherDefaultGCPKeyURL)}});

    content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

    url_loader_factory_.AddResponse(kOtherDefaultGCPKeyURL,
                                    R"(
{
  "originScopedKeys": {
    "https://scope.origin.test/" : {
      "keys" : [{
        "id": "AA arbitrary key beginning with two hex chars",
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d"
    }]},
    "https://other.origin2.test" : {
      "keys" : [{
        "id": "AA another key beginning with two hex chars",
        "key": "BBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d"
    }]}
  }
}
)");
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("AA arbitrary key beginning with two hex chars", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    EXPECT_EQ(2u, url_loader_factory_.total_requests());
  }

  // V2->V1 migration - discard result from DB and fetch again.
  {
    url_loader_factory_.AddResponse(kBiddingAndAuctionGCPCoordinatorKeyURL,
                                    R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

    content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    EXPECT_EQ(3u, url_loader_factory_.total_requests());
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, ProtoParsingUpgradeID) {
  std::string key_string;
  // base 64 encoded protobuf serialized data.
  // I used `protoscope -s | base64` to encode this.
  /*1: {
          1:{"Thirty-two arbitrary bytes......"}
          2:  16
        }
  */
  std::string base64_key_proto =
      "CiQKIFRoaXJ0eS10d28gYXJiaXRyYXJ5IGJ5dGVzLi4uLi4uEBA=";
  ASSERT_TRUE(base::Base64Decode(base64_key_proto, &key_string));
  std::optional<BiddingAndAuctionKeySet> keyset =
      BiddingAndAuctionKeySet::FromBinaryProto(key_string);
  ASSERT_TRUE(keyset);
  std::optional<BiddingAndAuctionServerKey> key =
      keyset->GetRandomKey(kTestScopeOrigin);
  ASSERT_TRUE(key);
  EXPECT_EQ(key->id, "10");
  EXPECT_EQ(key->key, "Thirty-two arbitrary bytes......");
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest,
       MaybePrefetchKeysFailureFailsPendingGetOrFetchKey) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  // Use the AWS coordinator for this prefetch test since the GCP key URL is
  // used by two coordinators.
  url::Origin coordinator =
      url::Origin::Create(GURL(kBiddingAndAuctionAWSCoordinatorOrigin));

  fetcher.MaybePrefetchKeys();

  {
    url_loader_factory_.AddResponse(kBiddingAndAuctionAWSCoordinatorKeyURL, "",
                                    net::HTTP_NOT_FOUND);
    bool completed = false;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(
        TrustedServerAPIType::kBiddingAndAuction, kTestScopeOrigin, coordinator,
        base::BindLambdaForTesting(
            [&](base::expected<BiddingAndAuctionServerKey, std::string>
                    maybe_key) {
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
    url_loader_factory_.AddResponse(kBiddingAndAuctionAWSCoordinatorKeyURL,
                                    R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(
        TrustedServerAPIType::kBiddingAndAuction, kTestScopeOrigin, coordinator,
        base::BindLambdaForTesting(
            [&](base::expected<BiddingAndAuctionServerKey, std::string>
                    maybe_key) {
              ASSERT_TRUE(maybe_key.has_value());
              key = *maybe_key;
              run_loop.Quit();
            }));
    run_loop.Run();
    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, MaybePrefetchKeysCachesValue) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  // Use the AWS coordinator for this prefetch test since the GCP key URL is
  // used by two coordinators.
  url::Origin coordinator =
      url::Origin::Create(GURL(kBiddingAndAuctionAWSCoordinatorOrigin));

  fetcher.MaybePrefetchKeys();
  task_environment_.RunUntilIdle();
  // MaybePrefetchKeys will try to fetch all keys in the
  // FledgeBiddingAndAuctionKeyConfig.
  EXPECT_TRUE(
      url_loader_factory_.IsPending(kBiddingAndAuctionGCPCoordinatorKeyURL));
  EXPECT_TRUE(
      url_loader_factory_.IsPending(kBiddingAndAuctionAWSCoordinatorKeyURL));
  EXPECT_TRUE(url_loader_factory_.SimulateResponseForPendingRequest(
      kBiddingAndAuctionAWSCoordinatorKeyURL,
      R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})"));
  task_environment_.RunUntilIdle();

  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(
        TrustedServerAPIType::kBiddingAndAuction, kTestScopeOrigin, coordinator,
        base::BindLambdaForTesting(
            [&](base::expected<BiddingAndAuctionServerKey, std::string>
                    maybe_key) {
              ASSERT_TRUE(maybe_key.has_value());
              key = *maybe_key;
              run_loop.Quit();
            }));
    // Shouldn't use this response (it should still be cached).
    EXPECT_FALSE(url_loader_factory_.SimulateResponseForPendingRequest(
        kBiddingAndAuctionAWSCoordinatorKeyURL,
        R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE\u003d",
        "id": "23456789-abcd-ef01-2345-6789abcdef01"
        }]})"));
    run_loop.Run();
    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
  }

  // Keys should not be fetched a second time.
  fetcher.MaybePrefetchKeys();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(
      url_loader_factory_.IsPending(kBiddingAndAuctionAWSCoordinatorKeyURL));
  EXPECT_FALSE(url_loader_factory_.SimulateResponseForPendingRequest(
      kBiddingAndAuctionAWSCoordinatorKeyURL,
      R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})"));
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest,
       MaybePrefetchKeysUpdatesExpiredValue) {
  // Use the AWS coordinator for this prefetch test since the GCP key URL is
  // used by two coordinators.
  url::Origin coordinator =
      url::Origin::Create(GURL(kBiddingAndAuctionAWSCoordinatorOrigin));

  url_loader_factory_.AddResponse(kBiddingAndAuctionAWSCoordinatorKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  // Get a key that we will let expire.
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
  {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(
        TrustedServerAPIType::kBiddingAndAuction, kTestScopeOrigin, coordinator,
        base::BindLambdaForTesting(
            [&](base::expected<BiddingAndAuctionServerKey, std::string>
                    maybe_key) {
              ASSERT_TRUE(maybe_key.has_value());
              key = *maybe_key;
              run_loop.Quit();
            }));

    run_loop.Run();
    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }

  // Let the key expire.
  task_environment_.RunUntilIdle();
  task_environment_.FastForwardBy(base::Days(7) + base::Seconds(1));
  url_loader_factory_.ClearResponses();

  fetcher.MaybePrefetchKeys();
  task_environment_.RunUntilIdle();
  // We should still try fetching all the keys, including for
  // kBiddingAndAuctionGCPCoordinatorKeyURL.
  EXPECT_TRUE(
      url_loader_factory_.IsPending(kBiddingAndAuctionGCPCoordinatorKeyURL));
  EXPECT_TRUE(
      url_loader_factory_.IsPending(kBiddingAndAuctionAWSCoordinatorKeyURL));
  EXPECT_TRUE(url_loader_factory_.SimulateResponseForPendingRequest(
      kBiddingAndAuctionAWSCoordinatorKeyURL,
      R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE\u003d",
        "id": "23456789-abcd-ef01-2345-6789abcdef01"
        }]})"));
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, CoalescesRequests) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  url_loader_factory_.AddResponse(kBiddingAndAuctionGCPCoordinatorKeyURL,
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  {
    content::BiddingAndAuctionServerKey key1, key2;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key1 = *maybe_key;
                              }));
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                key2 = *maybe_key;
                                ASSERT_TRUE(maybe_key.has_value());
                                run_loop.Quit();
                              }));
    run_loop.Run();
    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key1.id);
    EXPECT_EQ(std::string(32, '\0'), key1.key);

    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key2.id);
    EXPECT_EQ(std::string(32, '\0'), key2.key);

    EXPECT_EQ(1u, url_loader_factory_.total_requests());
  }
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, ChoosesRandomKey) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
  url_loader_factory_.AddResponse(kBiddingAndAuctionGCPCoordinatorKeyURL,
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
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
  }

  std::set<std::string> ids;
  while (ids.size() < 2) {
    content::BiddingAndAuctionServerKey key;
    base::RunLoop run_loop;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, kCoordinatorOrigin,
                          base::BindLambdaForTesting(
                              [&](base::expected<BiddingAndAuctionServerKey,
                                                 std::string> maybe_key) {
                                ASSERT_TRUE(maybe_key.has_value());
                                key = *maybe_key;
                                run_loop.Quit();
                              }));
    run_loop.Run();
    ids.insert(key.id);
  }
  EXPECT_THAT(ids,
              testing::ElementsAre("12345678-9abc-def0-1234-56789abcdef0",
                                   "23456789-abcd-ef01-2345-6789abcdef01"));
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, OverridesConfig) {
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
      TrustedServerAPIType::kBiddingAndAuction, kTestScopeOrigin,
      kCoordinatorOrigin,
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        ASSERT_TRUE(maybe_key.has_value());
        key = *maybe_key;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
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
      TrustedServerAPIType::kBiddingAndAuction, kTestScopeOrigin,
      kCoordinatorOrigin,
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        ASSERT_TRUE(maybe_key.has_value());
        key = *maybe_key;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
  EXPECT_EQ(std::string(32, '\0'), key.key);
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(BiddingAndAuctionServerKeyFetcherTest, DebugOverride) {
  std::string key_config = R"({
    "originScopedKeys": {
      "https://scope.origin.test/": {
        "keys": [{
          "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
          "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]
      }
    }
  })";

  std::string key_config2 = R"({
    "originScopedKeys": {
      "https://scope.origin.test/": {
        "keys": [{
          "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
          "id": "22345678-9abc-def0-1234-56789abcdef0"
        }]
      }
    }
  })";

  url::Origin origin_a = url::Origin::Create(GURL("https://a.test"));
  url::Origin origin_b = url::Origin::Create(GURL("https://b.test"));
  url::Origin origin_c = url::Origin::Create(GURL("https://c.test"));

  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();
  {
    base::test::TestFuture<std::optional<std::string>> future_1;
    fetcher.AddKeysDebugOverride(TrustedServerAPIType::kBiddingAndAuction,
                                 origin_a, key_config, future_1.GetCallback());
    EXPECT_EQ(std::nullopt, future_1.Get());
    ExpectNoDBStoredKeys(origin_a);
    base::test::TestFuture<
        base::expected<BiddingAndAuctionServerKey, std::string>>
        future_get;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, origin_a, future_get.GetCallback());
    ASSERT_TRUE(future_get.Get().has_value());
    auto& key = *future_get.Get();
    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);

    // Not available as a different API.
    base::test::TestFuture<
        base::expected<BiddingAndAuctionServerKey, std::string>>
        future_get2;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kTrustedKeyValue,
                          kTestScopeOrigin, origin_a,
                          future_get2.GetCallback());
    ASSERT_FALSE(future_get2.Get().has_value());
    EXPECT_EQ("API not supported by coordinator", future_get2.Get().error());
  }

  // Re-configuring the same should fail, but old config is still around.
  {
    base::test::TestFuture<std::optional<std::string>> future_2;
    fetcher.AddKeysDebugOverride(TrustedServerAPIType::kBiddingAndAuction,
                                 origin_a, key_config, future_2.GetCallback());
    EXPECT_EQ(
        "Can't add debug override because coordinator with origin "
        "already exists",
        future_2.Get());
    ExpectNoDBStoredKeys(origin_a);
    base::test::TestFuture<
        base::expected<BiddingAndAuctionServerKey, std::string>>
        future_get;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, origin_a, future_get.GetCallback());
    ASSERT_TRUE(future_get.Get().has_value());
    auto& key = *future_get.Get();
    EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);
  }

  // Errors in config are reported.
  {
    std::string broken_config = key_config;
    broken_config[0] = '!';
    base::test::TestFuture<std::optional<std::string>> future_3;
    fetcher.AddKeysDebugOverride(TrustedServerAPIType::kBiddingAndAuction,
                                 origin_b, broken_config,
                                 future_3.GetCallback());
    EXPECT_EQ("Key config decoding failed", future_3.Get());
    ExpectNoDBStoredKeys(origin_b);

    // Get afterwards should fail.
    base::test::TestFuture<
        base::expected<BiddingAndAuctionServerKey, std::string>>
        future_get;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, origin_b, future_get.GetCallback());
    ASSERT_FALSE(future_get.Get().has_value());
    EXPECT_EQ("Invalid Coordinator", future_get.Get().error());
  }

  // Different domain & API can be configured.
  {
    base::test::TestFuture<std::optional<std::string>> future_4;
    fetcher.AddKeysDebugOverride(TrustedServerAPIType::kTrustedKeyValue,
                                 origin_c, key_config2, future_4.GetCallback());
    EXPECT_EQ(std::nullopt, future_4.Get());
    ExpectNoDBStoredKeys(origin_c);
    base::test::TestFuture<
        base::expected<BiddingAndAuctionServerKey, std::string>>
        future_get;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kTrustedKeyValue,
                          kTestScopeOrigin, origin_c, future_get.GetCallback());
    ASSERT_TRUE(future_get.Get().has_value());
    auto& key = *future_get.Get();
    EXPECT_EQ("22345678-9abc-def0-1234-56789abcdef0", key.id);
    EXPECT_EQ(std::string(32, '\0'), key.key);

    // Not available as a different API.
    base::test::TestFuture<
        base::expected<BiddingAndAuctionServerKey, std::string>>
        future_get2;
    fetcher.GetOrFetchKey(TrustedServerAPIType::kBiddingAndAuction,
                          kTestScopeOrigin, origin_c,
                          future_get2.GetCallback());
    ASSERT_FALSE(future_get2.Get().has_value());
    EXPECT_EQ("API not supported by coordinator", future_get2.Get().error());
  }
}

class BiddingAndAuctionServerKeyFetcherCoordinatorTest
    : public BiddingAndAuctionServerKeyFetcherTest,
      public ::testing::WithParamInterface<
          std::tuple<int, TrustedServerAPIType>> {
 public:
  url::Origin GetCoordinator() {
    switch (std::get<0>(GetParam())) {
      case 0:
        return url::Origin::Create(
            GURL(kDefaultBiddingAndAuctionGCPCoordinatorOrigin));
      case 1:
        return url::Origin::Create(
            GURL(kBiddingAndAuctionGCPCoordinatorOrigin));
      case 2:
        return url::Origin::Create(
            GURL(kBiddingAndAuctionAWSCoordinatorOrigin));
    }
    NOTREACHED();
  }

  std::string GetURL() {
    switch (std::get<0>(GetParam())) {
      case 0:
        return kBiddingAndAuctionGCPCoordinatorKeyURL;
      case 1:
        return kBiddingAndAuctionGCPCoordinatorKeyURL;
      case 2:
        return kBiddingAndAuctionAWSCoordinatorKeyURL;
    }
    NOTREACHED();
  }

  TrustedServerAPIType GetAPI() { return std::get<1>(GetParam()); }
};

TEST_P(BiddingAndAuctionServerKeyFetcherCoordinatorTest, GoodResponse) {
  content::BiddingAndAuctionServerKeyFetcher fetcher = CreateFetcher();

  url_loader_factory_.AddResponse(GetURL(),
                                  R"({ "keys": [{
        "key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u003d",
        "id": "12345678-9abc-def0-1234-56789abcdef0"
        }]})");

  base::RunLoop run_loop;
  content::BiddingAndAuctionServerKey key;
  fetcher.GetOrFetchKey(
      GetAPI(), kTestScopeOrigin, GetCoordinator(),
      base::BindLambdaForTesting([&](base::expected<BiddingAndAuctionServerKey,
                                                    std::string> maybe_key) {
        ASSERT_TRUE(maybe_key.has_value());
        key = *maybe_key;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ("12345678-9abc-def0-1234-56789abcdef0", key.id);
  EXPECT_EQ(std::string(32, '\0'), key.key);
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    BiddingAndAuctionServerKeyFetcherCoordinatorTest,
    ::testing::Combine(
        ::testing::Range(0, 3),
        ::testing::Values(TrustedServerAPIType::kBiddingAndAuction,
                          TrustedServerAPIType::kTrustedKeyValue)));

}  // namespace
}  // namespace content
