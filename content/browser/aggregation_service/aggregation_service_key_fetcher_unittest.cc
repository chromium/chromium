// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kExampleOrigin[] = "https://a.com";

// NetworkFetcher that manages the public keys in memory.
class MockNetworkFetcher : public AggregationServiceKeyFetcher::NetworkFetcher {
 public:
  // AggregationServiceKeyFetcher::NetworkFetcher:
  void FetchPublicKeys(const url::Origin& origin,
                       NetworkFetchCallback callback) override {
    pending_callbacks_[origin].push_back(std::move(callback));
    ++num_fetches_;

    if (quit_closure_ && num_fetches_ >= expected_num_fetches_)
      std::move(quit_closure_).Run();
  }

  int num_fetches() const { return num_fetches_; }

  void WaitForNumFetches(int expected_num_fetches) {
    if (num_fetches_ >= expected_num_fetches)
      return;

    base::RunLoop run_loop;
    expected_num_fetches_ = expected_num_fetches;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void TriggerResponse(const url::Origin& origin,
                       const absl::optional<PublicKeyset>& response) {
    ASSERT_TRUE(pending_callbacks_.contains(origin))
        << "No corresponding FetchPublicKeys call for origin " << origin;

    std::vector<NetworkFetchCallback> callbacks =
        std::move(pending_callbacks_[origin]);
    pending_callbacks_.erase(origin);

    for (auto& callback : callbacks) {
      std::move(callback).Run(response);
    }
  }

 private:
  base::flat_map<url::Origin, std::vector<NetworkFetchCallback>>
      pending_callbacks_;
  int num_fetches_ = 0;
  int expected_num_fetches_ = 0;
  base::OnceClosure quit_closure_;
};

}  // namespace

class AggregationServiceKeyFetcherTest : public testing::Test {
 public:
  AggregationServiceKeyFetcherTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        storage_context_(task_environment_.GetMockClock()) {
    auto network_fetcher = std::make_unique<MockNetworkFetcher>();
    network_fetcher_ = network_fetcher.get();
    fetcher_ = std::make_unique<AggregationServiceKeyFetcher>(
        &storage_context_, std::move(network_fetcher));
  }

  void SetPublicKeysInStorage(const url::Origin& origin, PublicKeyset keyset) {
    storage_context_.GetKeyStorage()
        .AsyncCall(&AggregationServiceKeyStorage::SetPublicKeys)
        .WithArgs(origin, std::move(keyset));
  }

  void ExpectPublicKeysInStorage(const url::Origin& origin,
                                 const std::vector<PublicKey>& expected_keys) {
    base::RunLoop run_loop;
    storage_context_.GetKeyStorage()
        .AsyncCall(&AggregationServiceKeyStorage::GetPublicKeys)
        .WithArgs(origin)
        .Then(
            base::BindLambdaForTesting([&](std::vector<PublicKey> actual_keys) {
              EXPECT_TRUE(aggregation_service::PublicKeysEqual(expected_keys,
                                                               actual_keys));
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  void GetPublicKey(const url::Origin& origin) {
    // This method might rely on MockNetworkFetcher::WaitForNumFetches() for
    // waiting on responses from the storage and fetching from the network
    // fetcher.
    fetcher_->GetPublicKey(
        origin,
        base::BindLambdaForTesting(
            [&](absl::optional<PublicKey> key,
                AggregationServiceKeyFetcher::PublicKeyFetchStatus status) {
              ++num_callbacks_run_;
              last_fetched_key_ = key;
              last_fetch_status_ = status;
            }));
  }

  void ResetKeyFetcher() { fetcher_.reset(); }

 protected:
  const base::Clock& clock() const { return *task_environment_.GetMockClock(); }

  base::test::TaskEnvironment task_environment_;
  TestAggregationServiceStorageContext storage_context_;
  std::unique_ptr<AggregationServiceKeyFetcher> fetcher_;
  raw_ptr<MockNetworkFetcher> network_fetcher_;

  int num_callbacks_run_ = 0;
  absl::optional<PublicKey> last_fetched_key_ = absl::nullopt;
  absl::optional<AggregationServiceKeyFetcher::PublicKeyFetchStatus>
      last_fetch_status_ = absl::nullopt;
};

TEST_F(AggregationServiceKeyFetcherTest, GetPublicKeysFromStorage_Succeed) {
  url::Origin origin = url::Origin::Create(GURL(kExampleOrigin));
  PublicKey expected_key = aggregation_service::GenerateKey().public_key;

  SetPublicKeysInStorage(
      origin,
      PublicKeyset(/*keys=*/{expected_key}, /*fetch_time=*/clock().Now(),
                   /*expiry_time=*/base::Time::Max()));

  base::RunLoop run_loop;
  fetcher_->GetPublicKey(
      origin,
      base::BindLambdaForTesting(
          [&](absl::optional<PublicKey> key,
              AggregationServiceKeyFetcher::PublicKeyFetchStatus status) {
            ASSERT_TRUE(key.has_value());
            EXPECT_TRUE(aggregation_service::PublicKeysEqual({expected_key},
                                                             {key.value()}));
            EXPECT_EQ(status,
                      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_EQ(network_fetcher_->num_fetches(), 0);
}

TEST_F(AggregationServiceKeyFetcherTest,
       GetPublicKeysWithNoKeysForOrigin_Failed) {
  url::Origin origin = url::Origin::Create(GURL(kExampleOrigin));
  GetPublicKey(origin);
  network_fetcher_->WaitForNumFetches(1);
  network_fetcher_->TriggerResponse(origin, /*response=*/absl::nullopt);

  ASSERT_TRUE(last_fetch_status_.has_value());
  EXPECT_EQ(last_fetch_status_.value(),
            AggregationServiceKeyFetcher::PublicKeyFetchStatus::
                kPublicKeyFetchFailed);
  EXPECT_FALSE(last_fetched_key_.has_value());
  EXPECT_EQ(num_callbacks_run_, 1);
}

TEST_F(AggregationServiceKeyFetcherTest, FetchPublicKeysFromNetwork_Succeed) {
  url::Origin origin = url::Origin::Create(GURL(kExampleOrigin));
  PublicKey expected_key = aggregation_service::GenerateKey().public_key;

  GetPublicKey(origin);
  network_fetcher_->WaitForNumFetches(1);
  network_fetcher_->TriggerResponse(
      origin, /*response=*/PublicKeyset(/*keys=*/{expected_key},
                                        /*fetch_time=*/clock().Now(),
                                        /*expiry_time=*/base::Time::Max()));

  ASSERT_TRUE(last_fetch_status_.has_value());
  EXPECT_EQ(last_fetch_status_.value(),
            AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);
  ASSERT_TRUE(last_fetched_key_.has_value());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      {expected_key}, {last_fetched_key_.value()}));
  EXPECT_EQ(num_callbacks_run_, 1);

  // Verify that the fetched public keys are stored to storage.
  ExpectPublicKeysInStorage(origin, /*expected_keys=*/{expected_key});
}

TEST_F(AggregationServiceKeyFetcherTest,
       FetchPublicKeysFromNetworkNoStore_NotStored) {
  url::Origin origin = url::Origin::Create(GURL(kExampleOrigin));
  PublicKey expected_key = aggregation_service::GenerateKey().public_key;

  GetPublicKey(origin);
  network_fetcher_->WaitForNumFetches(1);
  network_fetcher_->TriggerResponse(
      origin, /*response=*/PublicKeyset(/*keys=*/{expected_key},
                                        /*fetch_time=*/clock().Now(),
                                        /*expiry_time=*/base::Time()));

  ASSERT_TRUE(last_fetch_status_.has_value());
  EXPECT_EQ(last_fetch_status_.value(),
            AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);
  ASSERT_TRUE(last_fetched_key_.has_value());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      {expected_key}, {last_fetched_key_.value()}));
  EXPECT_EQ(num_callbacks_run_, 1);

  // Verify that the fetched public keys are not stored to storage.
  ExpectPublicKeysInStorage(origin, /*expected_keys=*/{});
}

TEST_F(AggregationServiceKeyFetcherTest,
       FetchPublicKeysFromNetworkError_StorageCleared) {
  url::Origin origin = url::Origin::Create(GURL(kExampleOrigin));
  base::Time now = clock().Now();

  PublicKey key = aggregation_service::GenerateKey().public_key;
  SetPublicKeysInStorage(origin,
                         PublicKeyset(/*keys=*/{key}, /*fetch_time=*/now,
                                      /*expiry_time=*/now + base::Days(1)));

  task_environment_.FastForwardBy(base::Days(2));

  GetPublicKey(origin);
  network_fetcher_->WaitForNumFetches(1);
  network_fetcher_->TriggerResponse(origin, /*response=*/absl::nullopt);

  ASSERT_TRUE(last_fetch_status_.has_value());
  EXPECT_EQ(last_fetch_status_.value(),
            AggregationServiceKeyFetcher::PublicKeyFetchStatus::
                kPublicKeyFetchFailed);
  EXPECT_FALSE(last_fetched_key_.has_value());
  EXPECT_EQ(num_callbacks_run_, 1);

  // Verify that the public keys in storage are cleared.
  ExpectPublicKeysInStorage(origin, /*expected_keys=*/{});
}

TEST_F(AggregationServiceKeyFetcherTest,
       SimultaneousFetches_NoDuplicateNetworkRequest) {
  url::Origin origin = url::Origin::Create(GURL(kExampleOrigin));
  PublicKey expected_key = aggregation_service::GenerateKey().public_key;

  for (int i = 0; i < 10; ++i) {
    GetPublicKey(origin);
  }

  network_fetcher_->WaitForNumFetches(1);
  network_fetcher_->TriggerResponse(
      origin, /*response=*/PublicKeyset(/*keys=*/{expected_key},
                                        /*fetch_time=*/clock().Now(),
                                        /*expiry_time=*/base::Time::Max()));

  EXPECT_EQ(num_callbacks_run_, 10);
  EXPECT_EQ(network_fetcher_->num_fetches(), 1);
}

TEST_F(AggregationServiceKeyFetcherTest,
       SimultaneousFetchesInvalidKeysFromNetwork_NoDuplicateNetworkRequest) {
  url::Origin origin = url::Origin::Create(GURL(kExampleOrigin));

  for (int i = 0; i < 10; ++i) {
    GetPublicKey(origin);
  }

  network_fetcher_->WaitForNumFetches(1);
  network_fetcher_->TriggerResponse(origin, /*response=*/absl::nullopt);

  EXPECT_EQ(num_callbacks_run_, 10);
  EXPECT_EQ(network_fetcher_->num_fetches(), 1);
}

TEST_F(AggregationServiceKeyFetcherTest,
       KeyFetcherDeleted_PendingRequestsNotRun) {
  url::Origin origin = url::Origin::Create(GURL(kExampleOrigin));

  GetPublicKey(origin);
  network_fetcher_->WaitForNumFetches(1);
  EXPECT_EQ(network_fetcher_->num_fetches(), 1);

  ResetKeyFetcher();
  EXPECT_EQ(num_callbacks_run_, 0);
}

}  // namespace content
