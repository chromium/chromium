// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
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

// NetworkFetcher that manages the public keys in memory.
class MockNetworkFetcher : public AggregationServiceKeyFetcher::NetworkFetcher {
 public:
  // AggregationServiceKeyFetcher::NetworkFetcher:
  void FetchPublicKeys(const url::Origin& origin,
                       NetworkFetchCallback callback) override {
    auto it = public_keys_map_.find(origin);
    if (it != public_keys_map_.end()) {
      std::move(callback).Run(it->second);
    } else {
      std::move(callback).Run(absl::nullopt);
    }

    ++num_fetches_;
  }

  void SetPublicKeys(url::Origin origin, PublicKeyset keyset) {
    public_keys_map_.insert_or_assign(std::move(origin), std::move(keyset));
  }

  int num_fetches() const { return num_fetches_; }

 private:
  base::flat_map<url::Origin, PublicKeyset> public_keys_map_;
  int num_fetches_ = 0;
};

}  // namespace

class AggregationServiceKeyFetcherTest : public testing::Test {
 public:
  AggregationServiceKeyFetcherTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        manager_(task_environment_.GetMockClock()) {
    auto network_fetcher = std::make_unique<MockNetworkFetcher>();
    network_fetcher_ = network_fetcher.get();
    fetcher_ = std::make_unique<AggregationServiceKeyFetcher>(
        &manager_, std::move(network_fetcher));
  }

  void SetPublicKeysInStorage(const url::Origin& origin, PublicKeyset keyset) {
    manager_.GetKeyStorage()
        .AsyncCall(&AggregationServiceKeyStorage::SetPublicKeys)
        .WithArgs(origin, std::move(keyset));
  }

  void ExpectPublicKeyFetched(const url::Origin& origin,
                              PublicKey& expected_key) {
    base::RunLoop run_loop;
    fetcher_->GetPublicKey(
        origin,
        base::BindLambdaForTesting(
            [&](absl::optional<PublicKey> key,
                AggregationServiceKeyFetcher::PublicKeyFetchStatus status) {
              EXPECT_TRUE(key.has_value());
              EXPECT_TRUE(aggregation_service::PublicKeysEqual({expected_key},
                                                               {key.value()}));
              EXPECT_EQ(
                  status,
                  AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  void ExpectKeyFetchFailure(
      const url::Origin& origin,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus expected_status) {
    base::RunLoop run_loop;
    fetcher_->GetPublicKey(
        origin,
        base::BindLambdaForTesting(
            [&](absl::optional<PublicKey> key,
                AggregationServiceKeyFetcher::PublicKeyFetchStatus status) {
              EXPECT_FALSE(key.has_value());
              EXPECT_EQ(status, expected_status);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  void ExpectPublicKeysInStorage(const url::Origin& origin,
                                 const std::vector<PublicKey>& expected_keys) {
    base::RunLoop run_loop;
    manager_.GetKeyStorage()
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

 protected:
  const base::Clock& clock() const { return *task_environment_.GetMockClock(); }

  base::test::TaskEnvironment task_environment_;
  TestAggregatableReportManager manager_;
  std::unique_ptr<AggregationServiceKeyFetcher> fetcher_;
  MockNetworkFetcher* network_fetcher_;
};

TEST_F(AggregationServiceKeyFetcherTest, GetPublicKeysFromStorage_Succeed) {
  url::Origin origin = url::Origin::Create(GURL("https://a.com"));
  PublicKey expected_key(/*id=*/"abcd", /*key=*/kABCD1234AsBytes);

  SetPublicKeysInStorage(
      origin,
      PublicKeyset(/*keys=*/{expected_key}, /*fetch_time=*/clock().Now(),
                   /*expiry_time=*/base::Time::Max()));
  ExpectPublicKeyFetched(origin, expected_key);
  EXPECT_EQ(network_fetcher_->num_fetches(), 0);
}

TEST_F(AggregationServiceKeyFetcherTest,
       GetPublicKeysUntrustworthyOrigin_Failed) {
  url::Origin origin = url::Origin::Create(GURL("http://a.com"));
  ExpectKeyFetchFailure(
      origin,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kUntrustworthyOrigin);
}

TEST_F(AggregationServiceKeyFetcherTest, GetPublicKeysOpaqueOrigin_Failed) {
  url::Origin origin = url::Origin::Create(GURL("about:blank"));
  ExpectKeyFetchFailure(
      origin,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kUntrustworthyOrigin);
}

TEST_F(AggregationServiceKeyFetcherTest,
       GetPublicKeysWithNoKeysForOrigin_Failed) {
  url::Origin origin = url::Origin::Create(GURL("https://a.com"));
  SetPublicKeysInStorage(
      origin,
      PublicKeyset(
          /*keys=*/{PublicKey(/*id=*/"abcd", /*key=*/kABCD1234AsBytes)},
          /*fetch_time=*/clock().Now(), /*expiry_time=*/base::Time::Max()));

  url::Origin other_origin = url::Origin::Create(GURL("https://b.com"));
  ExpectKeyFetchFailure(other_origin,
                        AggregationServiceKeyFetcher::PublicKeyFetchStatus::
                            kPublicKeyFetchFailed);
}

TEST_F(AggregationServiceKeyFetcherTest, FetchPublicKeysFromNetwork_Succeed) {
  url::Origin origin = url::Origin::Create(GURL("https://a.com"));
  PublicKey expected_key(/*id=*/"abcd", /*key=*/kABCD1234AsBytes);

  network_fetcher_->SetPublicKeys(
      origin, PublicKeyset(/*keys=*/{expected_key},
                           /*fetch_time=*/clock().Now(),
                           /*expiry_time=*/base::Time::Max()));
  ExpectPublicKeyFetched(origin, expected_key);

  // Verify that the fetched public keys are stored to storage.
  ExpectPublicKeysInStorage(origin, /*expected_keys=*/{expected_key});
}

TEST_F(AggregationServiceKeyFetcherTest,
       FetchPublicKeysFromNetworkNoStore_NotStored) {
  url::Origin origin = url::Origin::Create(GURL("https://a.com"));
  PublicKey expected_key(/*id=*/"abcd", /*key=*/kABCD1234AsBytes);

  network_fetcher_->SetPublicKeys(origin,
                                  PublicKeyset(/*keys=*/{expected_key},
                                               /*fetch_time=*/clock().Now(),
                                               /*expiry_time=*/base::Time()));
  ExpectPublicKeyFetched(origin, expected_key);

  // Verify that the fetched public keys are not stored to storage.
  ExpectPublicKeysInStorage(origin, /*expected_keys=*/{});
}

TEST_F(AggregationServiceKeyFetcherTest,
       FetchPublicKeysFromNetworkError_StorageCleared) {
  url::Origin origin = url::Origin::Create(GURL("https://a.com"));
  base::Time now = clock().Now();

  PublicKey key(/*id=*/"abcd", /*key=*/kABCD1234AsBytes);
  SetPublicKeysInStorage(origin,
                         PublicKeyset(/*keys=*/{key}, /*fetch_time=*/now,
                                      /*expiry_time=*/now + base::Days(1)));

  task_environment_.FastForwardBy(base::Days(2));

  ExpectKeyFetchFailure(origin,
                        AggregationServiceKeyFetcher::PublicKeyFetchStatus::
                            kPublicKeyFetchFailed);

  // Verify that the public keys in storage are cleared.
  ExpectPublicKeysInStorage(origin, /*expected_keys=*/{});
}

TEST_F(AggregationServiceKeyFetcherTest,
       SimultaneousFetches_NoDuplicateNetworkRequest) {
  url::Origin origin = url::Origin::Create(GURL("https://a.com"));
  PublicKey expected_key(/*id=*/"abcd", /*key=*/kABCD1234AsBytes);

  network_fetcher_->SetPublicKeys(
      origin, PublicKeyset(/*keys=*/{expected_key},
                           /*fetch_time=*/clock().Now(),
                           /*expiry_time=*/base::Time::Max()));
  ExpectPublicKeyFetched(origin, expected_key);

  base::RunLoop run_loop;
  int num_callbacks = 0;
  for (int i = 0; i < 10; i++) {
    fetcher_->GetPublicKey(
        origin,
        base::BindLambdaForTesting(
            [&](absl::optional<PublicKey> key,
                AggregationServiceKeyFetcher::PublicKeyFetchStatus status) {
              EXPECT_TRUE(key.has_value());
              EXPECT_TRUE(aggregation_service::PublicKeysEqual({expected_key},
                                                               {key.value()}));
              EXPECT_EQ(
                  status,
                  AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);
              if (++num_callbacks == 10)
                run_loop.Quit();
            }));
  }

  run_loop.Run();

  EXPECT_EQ(network_fetcher_->num_fetches(), 1);
}

TEST_F(AggregationServiceKeyFetcherTest,
       SimultaneousFetchesInvalidKeysFromNetwork_NoDuplicateNetworkRequest) {
  url::Origin origin = url::Origin::Create(GURL("https://a.com"));

  // Keys not set in network fetcher, will return empty keys.

  base::RunLoop run_loop;
  int num_callbacks = 0;
  for (int i = 0; i < 10; i++) {
    fetcher_->GetPublicKey(
        origin,
        base::BindLambdaForTesting(
            [&](absl::optional<PublicKey> key,
                AggregationServiceKeyFetcher::PublicKeyFetchStatus status) {
              EXPECT_FALSE(key.has_value());
              EXPECT_EQ(status,
                        AggregationServiceKeyFetcher::PublicKeyFetchStatus::
                            kPublicKeyFetchFailed);
              if (++num_callbacks == 10)
                run_loop.Quit();
            }));
  }

  run_loop.Run();

  EXPECT_EQ(network_fetcher_->num_fetches(), 1);
}

}  // namespace content
