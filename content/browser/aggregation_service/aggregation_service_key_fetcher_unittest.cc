// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::DoAll;

using FetchCallback = AggregationServiceKeyFetcher::FetchCallback;
using NetworkFetchCallback =
    AggregationServiceKeyFetcher::NetworkFetcher::NetworkFetchCallback;

constexpr std::string_view kExampleUrl = "https://a.com/keys";

// NetworkFetcher that manages the public keys in memory.
class MockNetworkFetcher : public AggregationServiceKeyFetcher::NetworkFetcher {
 public:
  MOCK_METHOD(void,
              FetchPublicKeys,
              (const GURL& url, NetworkFetchCallback callback),
              (override));
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

  void SetPublicKeysInStorage(const GURL& url, PublicKeyset keyset) {
    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::SetPublicKeys)
        .WithArgs(url, std::move(keyset));
  }

  void ExpectPublicKeysInStorage(const GURL& url,
                                 const std::vector<PublicKey>& expected_keys) {
    base::RunLoop run_loop;
    storage_context_.GetStorage()
        .AsyncCall(&AggregationServiceStorage::GetPublicKeys)
        .WithArgs(url)
        .Then(
            base::BindLambdaForTesting([&](std::vector<PublicKey> actual_keys) {
              EXPECT_TRUE(aggregation_service::PublicKeysEqual(expected_keys,
                                                               actual_keys));
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  void ResetKeyFetcher() {
    network_fetcher_ = nullptr;
    fetcher_.reset();
  }

 protected:
  const base::Clock& clock() const { return *task_environment_.GetMockClock(); }

  base::test::TaskEnvironment task_environment_;
  TestAggregationServiceStorageContext storage_context_;
  std::unique_ptr<AggregationServiceKeyFetcher> fetcher_;
  raw_ptr<MockNetworkFetcher> network_fetcher_;
  base::MockCallback<FetchCallback> callback_;
};

TEST_F(AggregationServiceKeyFetcherTest, GetPublicKeysFromStorage_Succeed) {
  GURL url(kExampleUrl);
  PublicKey expected_key = aggregation_service::TestHpkeKey().GetPublicKey();

  SetPublicKeysInStorage(
      url, PublicKeyset(/*keys=*/{expected_key}, /*fetch_time=*/clock().Now(),
                        /*expiry_time=*/base::Time::Max()));

  EXPECT_CALL(*network_fetcher_, FetchPublicKeys(_, _)).Times(0);

  base::RunLoop run_loop;
  EXPECT_CALL(callback_,
              Run(std::optional<PublicKey>(std::move(expected_key)),
                  AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  fetcher_->GetPublicKey(url, callback_.Get());
  run_loop.Run();
}

TEST_F(AggregationServiceKeyFetcherTest, GetPublicKeysWithNoKeysForUrl_Failed) {
  GURL url(kExampleUrl);

  base::RunLoop run_loop;
  EXPECT_CALL(*network_fetcher_, FetchPublicKeys(url, _))
      .WillOnce(
          testing::DoAll(base::test::RunOnceClosure(run_loop.QuitClosure()),
                         base::test::RunOnceCallback<1>(std::nullopt)));
  EXPECT_CALL(callback_, Run(std::optional<PublicKey>(std::nullopt),
                             AggregationServiceKeyFetcher::
                                 PublicKeyFetchStatus::kPublicKeyFetchFailed));

  fetcher_->GetPublicKey(url, callback_.Get());
  run_loop.Run();
}

TEST_F(AggregationServiceKeyFetcherTest, FetchPublicKeysFromNetwork_Succeed) {
  GURL url(kExampleUrl);
  PublicKey expected_key = aggregation_service::TestHpkeKey().GetPublicKey();

  base::RunLoop run_loop;
  EXPECT_CALL(*network_fetcher_, FetchPublicKeys(url, _))
      .WillOnce(testing::DoAll(
          base::test::RunOnceClosure(run_loop.QuitClosure()),
          base::test::RunOnceCallback<1>(PublicKeyset(
              /*keys=*/{expected_key}, /*fetch_time=*/clock().Now(),
              /*expiry_time=*/base::Time::Max()))));
  EXPECT_CALL(callback_,
              Run(std::optional<PublicKey>(expected_key),
                  AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk));

  fetcher_->GetPublicKey(url, callback_.Get());
  run_loop.Run();

  // Verify that the fetched public keys are stored to storage.
  ExpectPublicKeysInStorage(url, /*expected_keys=*/{expected_key});
}

TEST_F(AggregationServiceKeyFetcherTest,
       FetchPublicKeysFromNetworkNoStore_NotStored) {
  GURL url(kExampleUrl);
  PublicKey expected_key = aggregation_service::TestHpkeKey().GetPublicKey();

  base::RunLoop run_loop;
  EXPECT_CALL(*network_fetcher_, FetchPublicKeys(url, _))
      .WillOnce(
          testing::DoAll(base::test::RunOnceClosure(run_loop.QuitClosure()),
                         base::test::RunOnceCallback<1>(
                             PublicKeyset(/*keys=*/{expected_key},
                                          /*fetch_time=*/clock().Now(),
                                          /*expiry_time=*/base::Time()))));
  EXPECT_CALL(callback_,
              Run(std::optional<PublicKey>(std::move(expected_key)),
                  AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk));

  fetcher_->GetPublicKey(url, callback_.Get());
  run_loop.Run();

  // Verify that the fetched public keys are not stored to storage.
  ExpectPublicKeysInStorage(url, /*expected_keys=*/{});
}

TEST_F(AggregationServiceKeyFetcherTest,
       FetchPublicKeysFromNetworkError_StorageCleared) {
  GURL url(kExampleUrl);
  base::Time now = clock().Now();

  PublicKey key = aggregation_service::TestHpkeKey().GetPublicKey();
  SetPublicKeysInStorage(url,
                         PublicKeyset(/*keys=*/{key}, /*fetch_time=*/now,
                                      /*expiry_time=*/now + base::Days(1)));

  task_environment_.FastForwardBy(base::Days(2));

  base::RunLoop run_loop;
  EXPECT_CALL(*network_fetcher_, FetchPublicKeys(url, _))
      .WillOnce(
          testing::DoAll(base::test::RunOnceClosure(run_loop.QuitClosure()),
                         base::test::RunOnceCallback<1>(std::nullopt)));
  EXPECT_CALL(callback_, Run(std::optional<PublicKey>(std::nullopt),
                             AggregationServiceKeyFetcher::
                                 PublicKeyFetchStatus::kPublicKeyFetchFailed));

  fetcher_->GetPublicKey(url, callback_.Get());
  run_loop.Run();

  // Verify that the public keys in storage are cleared.
  ExpectPublicKeysInStorage(url, /*expected_keys=*/{});
}

TEST_F(AggregationServiceKeyFetcherTest,
       SimultaneousFetches_NoDuplicateNetworkRequest) {
  GURL url(kExampleUrl);
  PublicKey expected_key = aggregation_service::TestHpkeKey().GetPublicKey();

  base::RunLoop run_loop;
  EXPECT_CALL(*network_fetcher_, FetchPublicKeys(url, _))
      .WillOnce(
          testing::DoAll(base::test::RunOnceClosure(run_loop.QuitClosure()),
                         base::test::RunOnceCallback<1>(
                             PublicKeyset(/*keys=*/{expected_key},
                                          /*fetch_time=*/clock().Now(),
                                          /*expiry_time=*/base::Time::Max()))));
  EXPECT_CALL(callback_,
              Run(std::optional<PublicKey>(std::move(expected_key)),
                  AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk))
      .Times(10);

  for (int i = 0; i < 10; ++i) {
    fetcher_->GetPublicKey(url, callback_.Get());
  }
  run_loop.Run();
}

TEST_F(AggregationServiceKeyFetcherTest,
       SimultaneousFetchesInvalidKeysFromNetwork_NoDuplicateNetworkRequest) {
  GURL url(kExampleUrl);

  base::RunLoop run_loop;
  EXPECT_CALL(*network_fetcher_, FetchPublicKeys(url, _))
      .WillOnce(
          testing::DoAll(base::test::RunOnceClosure(run_loop.QuitClosure()),
                         base::test::RunOnceCallback<1>(std::nullopt)));
  EXPECT_CALL(callback_, Run(std::optional<PublicKey>(std::nullopt),
                             AggregationServiceKeyFetcher::
                                 PublicKeyFetchStatus::kPublicKeyFetchFailed))
      .Times(10);

  for (int i = 0; i < 10; ++i) {
    fetcher_->GetPublicKey(url, callback_.Get());
  }
  run_loop.Run();
}

TEST_F(AggregationServiceKeyFetcherTest,
       KeyFetcherDeleted_PendingRequestsNotRun) {
  GURL url(kExampleUrl);

  base::RunLoop run_loop;
  EXPECT_CALL(*network_fetcher_, FetchPublicKeys(url, _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  EXPECT_CALL(callback_, Run(_, _)).Times(0);

  fetcher_->GetPublicKey(url, callback_.Get());
  run_loop.Run();

  ResetKeyFetcher();
}

}  // namespace content
