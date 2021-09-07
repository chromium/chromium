// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class AggregationServiceKeyFetcherTest : public testing::Test {
 public:
  AggregationServiceKeyFetcherTest()
      : fetcher_(std::make_unique<AggregationServiceKeyFetcher>(&manager_)) {}

  void SetPublicKeys(const PublicKeysForOrigin& keys) {
    manager_.GetKeyStorage()
        .AsyncCall(&AggregationServiceKeyStorage::SetPublicKeys)
        .WithArgs(keys);
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
              EXPECT_EQ(AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk,
                        status);
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
              EXPECT_EQ(expected_status, status);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestAggregatableReportManager manager_;
  std::unique_ptr<AggregationServiceKeyFetcher> fetcher_;
};

TEST_F(AggregationServiceKeyFetcherTest, GetPublicKeys_Succeed) {
  url::Origin origin = url::Origin::Create(GURL("https://a.com"));
  PublicKey expected_key(/*id=*/"abcd", /*key=*/kABCD1234AsBytes);
  PublicKeysForOrigin origin_keys(origin, {expected_key});

  SetPublicKeys(origin_keys);
  ExpectPublicKeyFetched(origin, expected_key);
}

TEST_F(AggregationServiceKeyFetcherTest,
       GetPublicKeysUntrustworthyOrigin_Failed) {
  url::Origin origin = url::Origin::Create(GURL("http://a.com"));
  PublicKey key(/*id=*/"abcd", /*key=*/kABCD1234AsBytes);
  PublicKeysForOrigin origin_keys(origin, {key});

  SetPublicKeys(origin_keys);

  ExpectKeyFetchFailure(
      origin,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kUntrustworthyOrigin);
}

TEST_F(AggregationServiceKeyFetcherTest, GetPublicKeysOpaqueOrigin_Failed) {
  url::Origin origin = url::Origin::Create(GURL("about:blank"));
  PublicKey key(/*id=*/"abcd", /*key=*/kABCD1234AsBytes);
  PublicKeysForOrigin origin_keys(origin, {key});

  SetPublicKeys(origin_keys);

  ExpectKeyFetchFailure(
      origin,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus::kUntrustworthyOrigin);
}

TEST_F(AggregationServiceKeyFetcherTest,
       GetPublicKeysWithNoKeysForOrigin_Failed) {
  url::Origin origin = url::Origin::Create(GURL("https://a.com"));
  PublicKey key(/*id=*/"abcd", /*key=*/kABCD1234AsBytes);
  PublicKeysForOrigin origin_keys(origin, {key});

  SetPublicKeys(origin_keys);

  url::Origin other_origin = url::Origin::Create(GURL("https://b.com"));
  ExpectKeyFetchFailure(other_origin,
                        AggregationServiceKeyFetcher::PublicKeyFetchStatus::
                            kPublicKeyFetchFailed);
}

}  // namespace content
