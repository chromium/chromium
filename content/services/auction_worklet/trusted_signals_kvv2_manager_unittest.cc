// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_signals_kvv2_manager.h"

#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/cpp/cbor_test_util.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-value.h"

namespace auction_worklet {
namespace {

// Using these with Get<> is more readable than writing out the full types, and
// clearer than using numeric indices.
using ResultType = scoped_refptr<TrustedSignals::Result>;
using ErrorType = std::optional<std::string>;

using SignalsFuture = base::test::TestFuture<ResultType, ErrorType>;

// Checks that a SignalsFuture receive a successful result with the specified
// data version.
MATCHER_P(SucceededWithDataVersion, expected_value, "") {
  // `arg` is taken as a const, but calling methods on a TestFuture can wait
  // until a result is received, to Get() functions are not const.
  SignalsFuture& signals_future = const_cast<SignalsFuture&>(arg);

  // Check if there's an error message, which there should not be in the case of
  // a success.
  if (signals_future.Get<ErrorType>()) {
    *result_listener << "is unexpectedly an error: \""
                     << *signals_future.Get<ErrorType>() << "\"";
    return false;
  }

  // This shouldn't be possible, since there should always be an error message
  // or a non-null Result.
  if (!signals_future.Get<ResultType>()) {
    *result_listener << "unexpectedly received not result.";
    return false;
  }

  return testing::ExplainMatchResult(
      testing::Eq(expected_value),
      signals_future.Get<ResultType>()->GetDataVersion(), result_listener);
}

// Checks that a SignalsFuture receive the specified error message.
MATCHER_P(FailedWithError, expected_error, "") {
  // `arg` is taken as a const, but calling methods on a TestFuture can wait
  // until a result is received, to Get() functions are not const.
  SignalsFuture& signals_future = const_cast<SignalsFuture&>(arg);

  if (signals_future.Get<ResultType>()) {
    *result_listener << "unexpectedly succeeded.";
    return false;
  }

  if (!signals_future.Get<ErrorType>()) {
    *result_listener << "unexpectedly had no error.";
    return false;
  }

  return testing::ExplainMatchResult(testing::Eq(expected_error),
                                     signals_future.Get<ErrorType>().value(),
                                     result_listener);
}

class TrustedSignalsKVv2ManagerTest : public mojom::TrustedSignalsCache,
                                      public testing::Test {
 public:
  struct PendingCacheRequest {
    base::UnguessableToken compression_group_token;
    mojo::PendingRemote<mojom::TrustedSignalsCacheClient> client;
  };

  TrustedSignalsKVv2ManagerTest() = default;

  ~TrustedSignalsKVv2ManagerTest() override {
    // All pending requests should have been handled before the end of the test.
    EXPECT_TRUE(pending_requests_.empty());
  }

  // Waits for a single request to the cache, expecting it to be for
  // `expected_compression_group_token`.
  PendingCacheRequest WaitForCacheRequest(
      std::optional<base::UnguessableToken> expected_compression_group_token =
          std::nullopt) {
    if (pending_requests_.empty()) {
      cache_request_run_loop_ = std::make_unique<base::RunLoop>();
      cache_request_run_loop_->Run();
      cache_request_run_loop_.reset();
    }

    CHECK(!pending_requests_.empty());
    PendingCacheRequest request = std::move(pending_requests_.front());
    pending_requests_.pop();

    EXPECT_EQ(
        request.compression_group_token,
        expected_compression_group_token.value_or(compression_group_token_));
    EXPECT_TRUE(request.client.is_valid());

    return request;
  }

  // Waits for the cache to receive a request and returns the provided response.
  void WaitForCacheRequestAndSendResponse(
      const std::vector<std::uint8_t>& response,
      std::optional<base::UnguessableToken> expected_compression_group_token =
          std::nullopt) {
    auto cache_request = WaitForCacheRequest(expected_compression_group_token);
    mojo::Remote<mojom::TrustedSignalsCacheClient> client(
        std::move(cache_request.client));
    client->OnSuccess(mojom::TrustedSignalsCompressionScheme::kNone, response);
  }

  // Returns the results of calling TrustedSignals::Result::GetBiddingSignals()
  // with `trusted_bidding_signals_keys`. Returns value as a JSON std::string,
  // for easy testing.
  std::string ExtractBiddingSignals(
      const TrustedSignals::Result& signals,
      std::vector<std::string> trusted_bidding_signals_keys) {
    base::RunLoop run_loop;

    std::string result;
    v8_helper_->v8_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
          v8::Isolate* isolate = v8_helper_->isolate();
          // Could use the scratch context, but using a separate one more
          // closely resembles actual use.
          v8::Local<v8::Context> context = v8::Context::New(isolate);
          v8::Context::Scope context_scope(context);

          v8::Local<v8::Value> value = signals.GetBiddingSignals(
              v8_helper_.get(), context, trusted_bidding_signals_keys);

          if (v8_helper_->ExtractJson(context, value,
                                      /*script_timeout=*/nullptr, &result) !=
              AuctionV8Helper::Result::kSuccess) {
            result = "JSON extraction failed.";
          }
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  // Returns a response with a fully populated partition 0.
  static std::vector<std::uint8_t> OnePartitionResponse() {
    return test::ToCborVector(
        R"([{
          "id": 0,
          "dataVersion": 1,
          "keyGroupOutputs": [
            {
              "tags": ["interestGroupNames"],
              "keyValues": {
                "group1": {
                  "value": "{
                    \"priorityVector\": {\"signal1\":2},
                    \"updateIfOlderThanMs\":3
                  }"
                }
              }
            },
            {
              "tags": ["keys"],
              "keyValues": {
                "key1": {"value":"\"4\""}
              }
            }
          ]
        }])");
  }

  // Returns a response with a partition 0 only, response is different from.
  static std::vector<std::uint8_t> DifferentOnePartitionResponse() {
    return test::ToCborVector(
        R"([{
          "id": 0,
          "dataVersion": 2,
          "keyGroupOutputs": []
        }])");
  }

  // Response with two minimally populated partitions.
  static std::vector<std::uint8_t> TwoPartitionResponse() {
    return test::ToCborVector(
        R"([{
          "id": 0,
          "dataVersion": 3,
          "keyGroupOutputs": []
        },
        {
          "id": 1,
          "dataVersion": 4,
          "keyGroupOutputs": []
        }])");
  }

 protected:
  // mojom::TrustedSignalsCache implementation:
  void GetTrustedSignals(
      const base::UnguessableToken& compression_group_token,
      mojo::PendingRemote<mojom::TrustedSignalsCacheClient> client) override {
    pending_requests_.emplace(compression_group_token, std::move(client));
    if (cache_request_run_loop_) {
      cache_request_run_loop_->Quit();
    }
  }

  base::test::TaskEnvironment task_environment_;

  mojo::Receiver<mojom::TrustedSignalsCache> cache_receiver_{this};
  scoped_refptr<AuctionV8Helper> v8_helper_ = AuctionV8Helper::Create(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  TrustedSignalsKVv2Manager manager_{cache_receiver_.BindNewPipeAndPassRemote(),
                                     v8_helper_.get()};

  std::unique_ptr<base::RunLoop> cache_request_run_loop_;

  const base::UnguessableToken compression_group_token_ =
      base::UnguessableToken::Create();

  // All requests to the TrustedSignalsCacheClient.
  std::queue<PendingCacheRequest> pending_requests_;
};

// Test the case where a request is cancelled before receiving a result from the
// cache. The purpose of this test is to make sure there's no crash.
TEST_F(TrustedSignalsKVv2ManagerTest, Cancel) {
  SignalsFuture future;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future.GetCallback());
  // Wait for cache request so the test is in a consistent state. Also, the test
  // would fail if the cache receives the request but WaitForCacheRequest() is
  // not called.
  auto cache_request = WaitForCacheRequest();

  // Destroy the request, and check if the request to the cache was cancelled.
  request.reset();
  mojo::Remote<mojom::TrustedSignalsCacheClient> client(
      std::move(cache_request.client));
  client.FlushForTesting();
  EXPECT_FALSE(client.is_connected());
  EXPECT_FALSE(future.IsReady());
}

TEST_F(TrustedSignalsKVv2ManagerTest, CacheReturnsError) {
  const char kErrorString[] = "This is an error. It really is. Honest.";
  SignalsFuture future;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future.GetCallback());

  auto cache_request = WaitForCacheRequest();
  mojo::Remote<mojom::TrustedSignalsCacheClient> client(
      std::move(cache_request.client));
  client->OnError(kErrorString);

  EXPECT_THAT(future, FailedWithError(kErrorString));
}

TEST_F(TrustedSignalsKVv2ManagerTest, BadData) {
  SignalsFuture future;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future.GetCallback());
  WaitForCacheRequestAndSendResponse({});
  EXPECT_THAT(future, FailedWithError("Failed to parse content as CBOR."));
}

// Request partition 1, response only has partition 0.
TEST_F(TrustedSignalsKVv2ManagerTest, WrongPartition) {
  SignalsFuture future;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/1, future.GetCallback());
  WaitForCacheRequestAndSendResponse(OnePartitionResponse());
  EXPECT_THAT(future,
              FailedWithError(R"(Partition "1" is missing from response.)"));
}

TEST_F(TrustedSignalsKVv2ManagerTest, Success) {
  SignalsFuture future;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future.GetCallback());
  WaitForCacheRequestAndSendResponse(OnePartitionResponse());

  ASSERT_THAT(future, SucceededWithDataVersion(1));

  TrustedSignals::Result& result = *future.Get<ResultType>();
  auto* per_group1_data = result.GetPerGroupData("group1");
  ASSERT_TRUE(per_group1_data);
  const TrustedSignals::Result::PriorityVector kExpectedPriorityVector{
      {"signal1", 2}};
  EXPECT_EQ(per_group1_data->priority_vector, kExpectedPriorityVector);
  EXPECT_EQ(per_group1_data->update_if_older_than, base::Milliseconds(3));

  EXPECT_EQ(ExtractBiddingSignals(result, {"key1"}), R"({"key1":"4"})");
}

TEST_F(TrustedSignalsKVv2ManagerTest, Gzip) {
  SignalsFuture future;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future.GetCallback());

  std::string compressed_string;
  EXPECT_TRUE(
      compression::GzipCompress(OnePartitionResponse(), &compressed_string));
  auto cache_request = WaitForCacheRequest(compression_group_token_);
  mojo::Remote<mojom::TrustedSignalsCacheClient> client(
      std::move(cache_request.client));
  client->OnSuccess(
      auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      std::vector<std::uint8_t>(compressed_string.begin(),
                                compressed_string.end()));

  EXPECT_THAT(future, SucceededWithDataVersion(1));
}

// In the case fetching a partition fails, the error is cached.
TEST_F(TrustedSignalsKVv2ManagerTest,
       MultipleRequestsReuseOnePartitionWithError) {
  const char kErrorString[] = "This is an error. It really is. Honest.";

  // Start two requests for the same partition.
  SignalsFuture future1;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request1 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future1.GetCallback());
  SignalsFuture future2;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request2 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future2.GetCallback());

  // There should only be a single request to the cache.
  auto cache_request = WaitForCacheRequest();

  // Send a response.
  mojo::Remote<mojom::TrustedSignalsCacheClient> client(
      std::move(cache_request.client));
  client->OnError(kErrorString);

  // Both callbacks should get the same error.
  EXPECT_THAT(future1, FailedWithError(kErrorString));
  EXPECT_THAT(future2, FailedWithError(kErrorString));

  // A third request sent after the previous two completed should get the same
  // result, as long as the previous requests are still alive.
  SignalsFuture future3;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request3 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future3.GetCallback());
  EXPECT_THAT(future3, FailedWithError(kErrorString));

  // If the first two requests are destroyed, the third request should still
  // keep the response alive for a 4th callback.
  request1.reset();
  request2.reset();

  SignalsFuture future4;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request4 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future4.GetCallback());
  EXPECT_THAT(future4, FailedWithError(kErrorString));
}

TEST_F(TrustedSignalsKVv2ManagerTest, MultipleRequestsReuseOnePartition) {
  // Start two requests for the same partition.
  SignalsFuture future1;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request1 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future1.GetCallback());
  SignalsFuture future2;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request2 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future2.GetCallback());

  // There should only be a single request to the cache.
  WaitForCacheRequestAndSendResponse(OnePartitionResponse());

  // Both callbacks should get the same signals.
  EXPECT_THAT(future1, SucceededWithDataVersion(1));
  EXPECT_THAT(future2, SucceededWithDataVersion(1));
  EXPECT_EQ(future1.Get<ResultType>(), future2.Get<ResultType>());

  // A third request sent after the previous two completed should get the same
  // result, as long as the previous requests are still alive.
  SignalsFuture future3;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request3 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future3.GetCallback());
  EXPECT_EQ(future1.Get<ResultType>(), future3.Get<ResultType>());

  // If the first two requests are destroyed, the third request should still
  // keep the response alive for a 4th callback.
  request1.reset();
  request2.reset();

  SignalsFuture future4;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request4 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future4.GetCallback());
  EXPECT_EQ(future1.Get<ResultType>(), future4.Get<ResultType>());
}

TEST_F(TrustedSignalsKVv2ManagerTest, ReRequestOnePartition) {
  SignalsFuture future1;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request1 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future1.GetCallback());

  WaitForCacheRequestAndSendResponse(OnePartitionResponse());
  EXPECT_THAT(future1, SucceededWithDataVersion(1));

  // Destroying the only request for the partition should make the manager
  // release the Result.
  request1.reset();

  // A new request with the same token should send a new request to the cache.
  SignalsFuture future2;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request2 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future2.GetCallback());

  // Send a different response, and make sure it's received. In production, the
  // same token should always return the same response, this test uses a
  // different one just to make sure the origin response is not reused.
  WaitForCacheRequestAndSendResponse(DifferentOnePartitionResponse());
  EXPECT_THAT(future2, SucceededWithDataVersion(2));
  EXPECT_NE(future1.Get<ResultType>(), future2.Get<ResultType>());
}

// Test the case of multiple requests for different partitions in the same
// compression group, one partition is missing from the response. Keeping alive
// the request for either partition should keep the entire response alive.
TEST_F(TrustedSignalsKVv2ManagerTest, MultipleRequestsOnePartitionMissing) {
  SignalsFuture future1;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request1 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future1.GetCallback());
  SignalsFuture future2;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request2 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/1, future2.GetCallback());

  // There should only be a single request to the cache.
  WaitForCacheRequestAndSendResponse(OnePartitionResponse());

  // Request 1 gets signals.
  EXPECT_THAT(future1, SucceededWithDataVersion(1));

  // Callback 2 gets an error due to there being no such partition.
  EXPECT_THAT(future2,
              FailedWithError(R"(Partition "1" is missing from response.)"));

  // Destroy request 2 and re-request the data for partition 1, which should be
  // reported as missing without a new request to the cache.
  request2.reset();
  SignalsFuture future3;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request3 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/1, future3.GetCallback());
  EXPECT_THAT(future3,
              FailedWithError(R"(Partition "1" is missing from response.)"));

  // Destroy request 1 and re-request the data for partition 0, which should
  // return the same result as the original request for partition 0 without a
  // new request to the cache.
  request1.reset();
  SignalsFuture future4;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request4 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future4.GetCallback());
  EXPECT_EQ(future1.Get<ResultType>(), future4.Get<ResultType>());

  // Destroying all remaining requests should destroy the cached signals data.
  request3.reset();
  request4.reset();
  EXPECT_TRUE(pending_requests_.empty());

  // Re-requesting data for one of the partitions above should result in a new
  // request.
  SignalsFuture future5;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request5 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future5.GetCallback());
  auto cache_request2 = WaitForCacheRequest();
}

// Test the case of multiple requests for different partitions in the same
// compression group, response includes both partitions. Keeping alive the
// request for either partition should keep the entire response alive.
TEST_F(TrustedSignalsKVv2ManagerTest, MultipleRequestsTwoPartitions) {
  SignalsFuture future1;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request1 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future1.GetCallback());
  SignalsFuture future2;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request2 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/1, future2.GetCallback());

  // There should only be a single request to the cache.
  WaitForCacheRequestAndSendResponse(TwoPartitionResponse());

  // Callback 1 and two get different signals.
  EXPECT_THAT(future1, SucceededWithDataVersion(3));
  EXPECT_THAT(future2, SucceededWithDataVersion(4));

  // Destroy request 2 and re-request the data for partition 1, which should be
  // reported as missing without a new request to the cache.
  request2.reset();
  SignalsFuture future3;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request3 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/1, future3.GetCallback());
  EXPECT_EQ(future2.Get<ResultType>(), future3.Get<ResultType>());

  // Destroy request 1 and re-request the data for partition 0, which should
  // return the same result as the original request for partition 0 without a
  // new request to the cache.
  request1.reset();
  SignalsFuture future4;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request4 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future4.GetCallback());
  EXPECT_EQ(future1.Get<ResultType>(), future4.Get<ResultType>());

  // Destroying all remaining requests should destroy the cached signals data.
  request3.reset();
  request4.reset();
  EXPECT_TRUE(pending_requests_.empty());

  // Re-request data for one of the partitions above should result in a new
  // request.
  SignalsFuture future5;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request5 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future5.GetCallback());
  auto cache_request2 = WaitForCacheRequest();
}

// Test the case of multiple requests for different partitions in the same
// compression group, where the second request is made after the first has
// received results. Even though the second request is for a different
// partition, it should still be in the manager's in-memory cache.
TEST_F(TrustedSignalsKVv2ManagerTest,
       MultipleRequestsTwoPartitionsSequentialRequests) {
  SignalsFuture future1;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request1 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future1.GetCallback());

  WaitForCacheRequestAndSendResponse(TwoPartitionResponse());
  EXPECT_THAT(future1, SucceededWithDataVersion(3));

  // Request another partition from the same compression group. It should
  // succeed without another request to the cache.
  SignalsFuture future2;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request2 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/1, future2.GetCallback());
  EXPECT_THAT(future2, SucceededWithDataVersion(4));
  EXPECT_NE(future1.Get<ResultType>(), future2.Get<ResultType>());
  EXPECT_TRUE(pending_requests_.empty());

  // Request another partition from the same compression group that is not
  // actually present in the response. It should fail without another request to
  // the cache.
  SignalsFuture future3;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request3 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/2, future3.GetCallback());
  EXPECT_THAT(future3,
              FailedWithError(R"(Partition "2" is missing from response.)"));
}

TEST_F(TrustedSignalsKVv2ManagerTest, IndependentRequests) {
  base::UnguessableToken compression_group_token2 =
      base::UnguessableToken::Create();

  // Start one request.
  SignalsFuture future1;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request1 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future1.GetCallback());
  auto cache_request1 = WaitForCacheRequest(compression_group_token_);

  // Start another request for a different compression group.
  SignalsFuture future2;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request2 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token2,
                              /*partition_id=*/0, future2.GetCallback());
  auto cache_request2 = WaitForCacheRequest(compression_group_token2);

  // Send a response for the second request.
  mojo::Remote<mojom::TrustedSignalsCacheClient> client2(
      std::move(cache_request2.client));
  client2->OnSuccess(mojom::TrustedSignalsCompressionScheme::kNone,
                     DifferentOnePartitionResponse());
  EXPECT_THAT(future2, SucceededWithDataVersion(2));

  // Send a response for the first request.
  mojo::Remote<mojom::TrustedSignalsCacheClient> client1(
      std::move(cache_request1.client));
  client1->OnSuccess(mojom::TrustedSignalsCompressionScheme::kNone,
                     OnePartitionResponse());
  EXPECT_THAT(future1, SucceededWithDataVersion(1));
  EXPECT_NE(future1.Get<ResultType>(), future2.Get<ResultType>());

  // Delete `request1`. The second response should still be cached, the first
  // response should not.
  request1.reset();

  // Check the second response was cached.
  SignalsFuture future3;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request3 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token2,
                              /*partition_id=*/0, future3.GetCallback());
  EXPECT_EQ(future3.Get<ResultType>(), future2.Get<ResultType>());

  // Check the first response is not in the cache.
  SignalsFuture future4;
  std::unique_ptr<TrustedSignalsKVv2Manager::Request> request4 =
      manager_.RequestSignals(TrustedSignalsKVv2Manager::SignalsType::kBidding,
                              compression_group_token_,
                              /*partition_id=*/0, future4.GetCallback());
  auto cache_request4 = WaitForCacheRequest();
}

}  // namespace
}  // namespace auction_worklet
