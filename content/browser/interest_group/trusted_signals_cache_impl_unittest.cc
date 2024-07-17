// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/trusted_signals_cache_impl.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/browser/interest_group/trusted_signals_fetcher.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

// Generic success/error strings used in most tests.
const char kSuccessBody[] = "Successful result";
const char kOtherSuccessBody[] = "Other sucessful result";
const char kSomeOtherSuccessBody[] = "Some other sucessful result";
const char kErrorMessage[] = "Error message";

// The error message received when a compression group is requested over the
// Mojo interface, but no matching CompressionGroupData is found.
const char kRequestCancelledError[] = "Request cancelled";

// Struct with input parameters for RequestTrustedBiddingSignals(). Having a
// struct allows for more easily checking changing a single parameter, and
// validating all parameters passed to the TrustedSignalsFetcher, without
// duplicating a lot of code.
struct BiddingParams {
  url::Origin main_frame_origin;
  url::Origin bidder;

  // Actual requests may only have a single interest group, so only one name.
  // This is a set because this struct is also used to validate fetch
  // parameters, which may include a set of interest groups in the
  // group-by-origin case.
  std::set<std::string> interest_group_names;

  blink::mojom::InterestGroup_ExecutionMode execution_mode =
      blink::mojom::InterestGroup_ExecutionMode::kCompatibilityMode;
  url::Origin joining_origin;
  GURL trusted_bidding_signals_url;
  std::optional<std::vector<std::string>> trusted_bidding_signals_keys;
  base::Value::Dict additional_params;
};

// Subclass of TrustedSignalsCacheImpl that mocks out TrustedSignalsFetcher
// calls, and lets tests monitor and respond to those fetches.
class TestTrustedSignalsCache : public TrustedSignalsCacheImpl {
 public:
  class TestTrustedSignalsFetcher : public TrustedSignalsFetcher {
   public:
    struct PendingBiddingSignalsFetch {
      GURL trusted_bidding_signals_url;
      std::map<int, std::vector<TrustedSignalsFetcher::BiddingPartition>>
          compression_groups;
      TrustedSignalsFetcher::Callback callback;

      // Weak pointer to Fetcher to allow checking if the Fetcher has been
      // destroyed.
      base::WeakPtr<TestTrustedSignalsFetcher> fetcher_alive;
    };

    explicit TestTrustedSignalsFetcher(TestTrustedSignalsCache* cache)
        : TrustedSignalsFetcher(/*url_loader_factory=*/nullptr),
          cache_(cache) {}

    ~TestTrustedSignalsFetcher() override = default;

   private:
    void FetchBiddingSignals(
        const GURL& trusted_bidding_signals_url,
        const std::map<int, std::vector<BiddingPartition>>& compression_groups,
        Callback callback) override {
      // This class is single use. Make sure a Fetcher isn't used more than
      // once.
      EXPECT_FALSE(fetch_started_);
      fetch_started_ = true;

      std::map<int, std::vector<TrustedSignalsFetcher::BiddingPartition>>
          compression_groups_copy;
      for (const auto& compression_group : compression_groups) {
        auto& bidding_partitions_copy =
            compression_groups_copy.try_emplace(compression_group.first)
                .first->second;
        for (const auto& bidding_partition : compression_group.second) {
          // Have to manually copy each BiddingPartition, since BiddingPartition
          // deliberately doesn't have a copy constructor, to avoid accidental
          // copies, which can be resource intensive.
          TrustedSignalsFetcher::BiddingPartition bidding_partition_copy;
          bidding_partition_copy.partition_id = bidding_partition.partition_id;
          bidding_partition_copy.interest_group_names =
              bidding_partition.interest_group_names;
          bidding_partition_copy.keys = bidding_partition.keys;
          bidding_partition_copy.hostname = bidding_partition.hostname;
          bidding_partition_copy.additional_params =
              bidding_partition.additional_params.Clone();
          bidding_partitions_copy.emplace_back(
              std::move(bidding_partition_copy));
        }
      }

      cache_->OnPendingFetch(PendingBiddingSignalsFetch(
          trusted_bidding_signals_url, std::move(compression_groups_copy),
          std::move(callback), weak_ptr_factory_.GetWeakPtr()));
    }

    const raw_ptr<TestTrustedSignalsCache> cache_;
    bool fetch_started_ = false;
    base::WeakPtrFactory<TestTrustedSignalsFetcher> weak_ptr_factory_{this};
  };

  TestTrustedSignalsCache()
      : TrustedSignalsCacheImpl(/*url_loader_factory=*/nullptr) {}

  ~TestTrustedSignalsCache() override {
    // All pending fetches should have been claimed by calls to
    // WaitForBiddingSignalsFetch[es]().
    EXPECT_TRUE(trusted_bidding_signals_fetches_.empty());
  }

  // Waits until there have been `num_fetches` fetches whose
  // FetchBiddingSignals() method has been invoked and returns them all,
  // clearing the list of pending fetches. EXPECTs that number is not exceeded.
  std::vector<TestTrustedSignalsFetcher::PendingBiddingSignalsFetch>
  WaitForBiddingSignalsFetches(size_t num_fetches) {
    DCHECK(!run_loop_);
    while (trusted_bidding_signals_fetches_.size() < num_fetches) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
    EXPECT_EQ(num_fetches, trusted_bidding_signals_fetches_.size());
    std::vector<TestTrustedSignalsFetcher::PendingBiddingSignalsFetch> out;
    std::swap(out, trusted_bidding_signals_fetches_);
    return out;
  }

  // Wrapper around WaitForBiddingSignalsFetches() that waits for a single fetch
  // and returns only it. Expects there to be at most one fetch.
  TestTrustedSignalsFetcher::PendingBiddingSignalsFetch
  WaitForBiddingSignalsFetch() {
    return std::move(WaitForBiddingSignalsFetches(1).at(0));
  }

  size_t num_pending_fetches() const {
    return trusted_bidding_signals_fetches_.size();
  }

 private:
  std::unique_ptr<TrustedSignalsFetcher> CreateFetcher() override {
    return std::make_unique<TestTrustedSignalsFetcher>(this);
  }

  void OnPendingFetch(
      TestTrustedSignalsFetcher::PendingBiddingSignalsFetch fetch) {
    trusted_bidding_signals_fetches_.emplace_back(std::move(fetch));
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  std::vector<TestTrustedSignalsFetcher::PendingBiddingSignalsFetch>
      trusted_bidding_signals_fetches_;
};

// Validates that `partition` has a single partition corresponding to the
// params in `params`.
void ValidateFetchParamsForPartition(
    const TrustedSignalsFetcher::BiddingPartition& partition,
    const BiddingParams& params,
    int expected_partition_id) {
  EXPECT_EQ(partition.hostname, params.main_frame_origin.host());
  EXPECT_THAT(partition.interest_group_names,
              testing::ElementsAreArray(params.interest_group_names));
  if (!params.trusted_bidding_signals_keys) {
    EXPECT_THAT(partition.keys, testing::ElementsAre());
  } else {
    EXPECT_THAT(partition.keys, testing::UnorderedElementsAreArray(
                                    *params.trusted_bidding_signals_keys));
  }
  EXPECT_EQ(partition.partition_id, expected_partition_id);
}

// Validates that `partitions` has a single partition corresponding to the
// params in `params`.
void ValidateFetchParamsForPartitions(
    const std::vector<TrustedSignalsFetcher::BiddingPartition>& partitions,
    const BiddingParams& params,
    int expected_partition_id) {
  ASSERT_EQ(partitions.size(), 1u);
  ValidateFetchParamsForPartition(partitions.at(0), params,
                                  expected_partition_id);
}

// Verifies that all fields of `trusted_bidding_signals_fetch` exactly match
// `params` and the provided IDs. Doesn't handle the case that that multiple
// fetches were merged into a single fetch. Note that `compression_group_id` is
// never exposed externally by the TrustedSignalsCache API nor passed in, so
// relies on information about the internal logic of the cache to provide the
// expected value for.
void ValidateFetchParams(
    const TestTrustedSignalsCache::TestTrustedSignalsFetcher::
        PendingBiddingSignalsFetch& trusted_bidding_signals_fetch,
    const BiddingParams& params,
    int expected_compression_group_id,
    int expected_partition_id) {
  EXPECT_EQ(trusted_bidding_signals_fetch.trusted_bidding_signals_url,
            params.trusted_bidding_signals_url);
  ASSERT_EQ(trusted_bidding_signals_fetch.compression_groups.size(), 1u);
  EXPECT_EQ(trusted_bidding_signals_fetch.compression_groups.begin()->first,
            expected_compression_group_id);

  ValidateFetchParamsForPartitions(
      trusted_bidding_signals_fetch.compression_groups.begin()->second, params,
      expected_partition_id);
}

TrustedSignalsFetcher::CompressionGroupResult CreateCompressionGroupResult(
    auction_worklet::mojom::TrustedSignalsCompressionScheme compression_scheme =
        auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
    std::string_view body = kSuccessBody,
    base::TimeDelta ttl = base::Hours(1)) {
  TrustedSignalsFetcher::CompressionGroupResult result;
  result.compression_group_data =
      std::vector<std::uint8_t>(body.begin(), body.end());
  result.compression_scheme = compression_scheme;
  result.ttl = ttl;
  return result;
}

TrustedSignalsFetcher::CompressionGroupResultMap
CreateCompressionGroupResultMap(
    int compression_group_id,
    auction_worklet::mojom::TrustedSignalsCompressionScheme compression_scheme =
        auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
    std::string_view body = kSuccessBody,
    base::TimeDelta ttl = base::Hours(1)) {
  TrustedSignalsFetcher::CompressionGroupResultMap map;
  map[compression_group_id] =
      CreateCompressionGroupResult(compression_scheme, body, ttl);
  return map;
}

// Respond to the next fetch with a generic successful body. Expects only one
// compression group.
void RespondToFetchWithSuccess(
    TestTrustedSignalsCache::TestTrustedSignalsFetcher::
        PendingBiddingSignalsFetch& trusted_bidding_signals_fetch,
    auction_worklet::mojom::TrustedSignalsCompressionScheme compression_scheme =
        auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
    std::string_view body = kSuccessBody,
    base::TimeDelta ttl = base::Hours(1)) {
  // Shouldn't be calling this after the fetcher was destroyed.
  ASSERT_TRUE(trusted_bidding_signals_fetch.fetcher_alive);

  // Method only supports a single compression group.
  ASSERT_EQ(trusted_bidding_signals_fetch.compression_groups.size(), 1u);

  ASSERT_TRUE(trusted_bidding_signals_fetch.callback);
  std::move(trusted_bidding_signals_fetch.callback)
      .Run(CreateCompressionGroupResultMap(
          trusted_bidding_signals_fetch.compression_groups.begin()->first,
          compression_scheme, body, ttl));
}

// Responds to a two-compression group fetch with two successful responses, with
// different parameters. The first uses a gzip with kSuccessBody, and the second
// uses brotli with kOtherSuccessBody.
void RespondToTwoCompressionGroupFetchWithSuccess(
    TestTrustedSignalsCache::TestTrustedSignalsFetcher::
        PendingBiddingSignalsFetch& trusted_bidding_signals_fetch,
    base::TimeDelta ttl1 = base::Hours(1),
    base::TimeDelta ttl2 = base::Hours(1)) {
  ASSERT_EQ(trusted_bidding_signals_fetch.compression_groups.size(), 2u);
  TrustedSignalsFetcher::CompressionGroupResultMap map;
  map[trusted_bidding_signals_fetch.compression_groups.begin()->first] =
      CreateCompressionGroupResult(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
          kSuccessBody, ttl1);
  map[std::next(trusted_bidding_signals_fetch.compression_groups.begin())
          ->first] =
      CreateCompressionGroupResult(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
          kOtherSuccessBody, ttl2);
  std::move(trusted_bidding_signals_fetch.callback).Run(std::move(map));
}

// Respond to the next fetch with a generic successful body. Does not care about
// number of compression groups, as on error, all groups are failed.
void RespondToFetchWithError(
    TestTrustedSignalsCache::TestTrustedSignalsFetcher::
        PendingBiddingSignalsFetch& trusted_bidding_signals_fetch) {
  CHECK(trusted_bidding_signals_fetch.callback);
  std::move(trusted_bidding_signals_fetch.callback)
      .Run(base::unexpected(TrustedSignalsFetcher::ErrorInfo{kErrorMessage}));
}

// Single use auction_worklet::mojom::TrustedSignalsCacheClient. Requests
// trusted signals on construction.
class TestTrustedSignalsCacheClient
    : public auction_worklet::mojom::TrustedSignalsCacheClient {
 public:
  TestTrustedSignalsCacheClient(
      const base::UnguessableToken& compression_group_id,
      mojo::Remote<auction_worklet::mojom::TrustedSignalsCache>&
          cache_mojo_pipe)
      : receiver_(this) {
    cache_mojo_pipe->GetTrustedSignals(compression_group_id,
                                       receiver_.BindNewPipeAndPassRemote());
  }

  // Overload used by almost all callers, to simplify the call a bit.
  TestTrustedSignalsCacheClient(
      scoped_refptr<TestTrustedSignalsCache::Handle> handle,
      mojo::Remote<auction_worklet::mojom::TrustedSignalsCache>&
          cache_mojo_pipe)
      : TestTrustedSignalsCacheClient(handle->compression_group_token(),
                                      cache_mojo_pipe) {}

  ~TestTrustedSignalsCacheClient() override = default;

  // Waits for OnSuccess() to be called with the provided arguments. Quits loop
  // and has an assert failure if OnError() is called instead.
  void WaitForSuccess(
      auction_worklet::mojom::TrustedSignalsCompressionScheme
          expected_compression_scheme =
              auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      std::string_view expected_compression_group_data = kSuccessBody) {
    ASSERT_TRUE(WaitForResult());
    EXPECT_EQ(compression_scheme_, expected_compression_scheme);
    EXPECT_EQ(compression_group_data_, expected_compression_group_data);
  }

  // Waits for OnError() to be called with the provided arguments. Quits loop
  // and has an assert failure if OnSuccess() is called instead.
  void WaitForError(std::string_view expected_error = kErrorMessage) {
    ASSERT_FALSE(WaitForResult());
    EXPECT_EQ(error_message_, expected_error);
  }

  bool has_result() { return run_loop_.AnyQuitCalled(); }

  // auction_worklet::mojom::TrustedSignalsCacheClient implementation:

  void OnSuccess(auction_worklet::mojom::TrustedSignalsCompressionScheme
                     compression_scheme,
                 mojo_base::BigBuffer compression_group_data) override {
    EXPECT_FALSE(compression_group_data_);
    EXPECT_FALSE(error_message_);

    compression_scheme_ = compression_scheme;
    compression_group_data_ = std::string(compression_group_data.begin(),
                                          compression_group_data.end());

    EXPECT_FALSE(run_loop_.AnyQuitCalled());
    run_loop_.Quit();
  }

  void OnError(const std::string& error_message) override {
    EXPECT_FALSE(compression_group_data_);
    EXPECT_FALSE(error_message_);

    error_message_ = error_message;

    EXPECT_FALSE(run_loop_.AnyQuitCalled());
    run_loop_.Quit();
  }

 private:
  // Waits until OnSuccess() or OnError() has been called, and returns true on
  // success.
  bool WaitForResult() {
    run_loop_.Run();
    return compression_group_data_.has_value();
  }

  base::RunLoop run_loop_;

  std::optional<auction_worklet::mojom::TrustedSignalsCompressionScheme>
      compression_scheme_;
  // Use a string instead of a vector<uint8_t> for more useful error messages on
  // failure comparisons.
  std::optional<std::string> compression_group_data_;

  std::optional<std::string> error_message_;

  mojo::Receiver<auction_worklet::mojom::TrustedSignalsCacheClient> receiver_;
};

class TrustedSignalsCacheTest : public testing::Test {
 public:
  // The expected relationship between two sequential signals requests, if the
  // second request is made without waiting for the first to start its Fetch.
  enum class RequestRelation {
    // Requests cannot share a fetch.
    kDifferentFetches,
    // Requests can use different compression groups within a fetch.
    kDifferentCompressionGroups,
    // Requests can use different partition within a fetch.
    kDifferentPartitions,
    // Requests can use the same partition, but the second request needs to
    // modify the partition (and thus the fetch) to do so. As a result, if the
    // first request's fetch has already been started, the second request cannot
    // reuse it.
    kSamePartitionModified,
    // Requests can use the same partition, with the second request not
    // modifying the partition of the first, which means it can use the same
    // partition even if the first request already as a second request.
    kSamePartitionUnmodified,
  };

  // Test case class shared by a number of tests. Each test makes a request
  // using `bidding_params1` before `bidding_params2`.
  struct BidderTestCase {
    // Used for documentation + useful output on errors.
    const char* description;
    RequestRelation request_relation = RequestRelation::kDifferentFetches;
    BiddingParams bidding_params1;
    BiddingParams bidding_params2;
  };

  TrustedSignalsCacheTest() { CreateCache(); }

  ~TrustedSignalsCacheTest() override = default;

  void CreateCache() {
    trusted_signals_cache_ = std::make_unique<TestTrustedSignalsCache>();
    cache_mojo_pipe_.reset();
    cache_mojo_pipe_.Bind(trusted_signals_cache_->CreateMojoPipe());
  }

  BiddingParams CreateDefaultBiddingParams() const {
    BiddingParams out;
    out.main_frame_origin = kMainFrameOrigin;
    out.bidder = kBidder;
    out.interest_group_names = {kInterestGroupName};
    out.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kCompatibilityMode;
    out.joining_origin = kJoiningOrigin;
    out.trusted_bidding_signals_url = kTrustedBiddingSignalsUrl;
    out.trusted_bidding_signals_keys = {{"key1", "key2"}};
    return out;
  }

  BidderTestCase CreateDefaultTestCase() {
    BidderTestCase out;
    out.bidding_params1 = CreateDefaultBiddingParams();
    out.bidding_params2 = CreateDefaultBiddingParams();
    return out;
  }

  // Returns a shared set of test cases used by a number of different tests.
  std::vector<BidderTestCase> CreateBidderTestCases() {
    std::vector<BidderTestCase> out;

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Different main frame origins";
    out.back().request_relation = RequestRelation::kDifferentFetches;
    out.back().bidding_params2.main_frame_origin =
        url::Origin::Create(GURL("https://other.origin.test/"));

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Different bidders";
    out.back().request_relation = RequestRelation::kDifferentFetches;
    out.back().bidding_params2.bidder =
        url::Origin::Create(GURL("https://other.bidder.test/"));

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Different interest group names";
    out.back().request_relation = RequestRelation::kDifferentPartitions;
    out.back().bidding_params2.interest_group_names = {"other interest group"};

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Different joining origins";
    out.back().request_relation = RequestRelation::kDifferentCompressionGroups;
    out.back().bidding_params2.joining_origin =
        url::Origin::Create(GURL("https://other.joining.origin.test"));

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Different trusted bidding signals URLs";
    out.back().request_relation = RequestRelation::kDifferentFetches;
    out.back().bidding_params2.trusted_bidding_signals_url =
        GURL("https://other.bidder.test/signals");

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "First request has no keys";
    out.back().request_relation = RequestRelation::kSamePartitionModified;
    out.back().bidding_params1.trusted_bidding_signals_keys.reset();

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Second request has no keys";
    out.back().request_relation = RequestRelation::kSamePartitionUnmodified;
    out.back().bidding_params2.trusted_bidding_signals_keys.reset();

    out.emplace_back(CreateDefaultTestCase());
    out.back().description =
        "First request's keys are a subset of the second request's";
    out.back().request_relation = RequestRelation::kSamePartitionModified;
    out.back().bidding_params2.trusted_bidding_signals_keys->emplace_back(
        "other key");

    out.emplace_back(CreateDefaultTestCase());
    out.back().description =
        "Second request's keys are a subset of the first request's";
    out.back().request_relation = RequestRelation::kSamePartitionUnmodified;
    out.back().bidding_params2.trusted_bidding_signals_keys->erase(
        out.back().bidding_params2.trusted_bidding_signals_keys->begin());

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Requests have complete distinct keys";
    out.back().request_relation = RequestRelation::kSamePartitionModified;
    out.back().bidding_params2.trusted_bidding_signals_keys = {{"other key"}};

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Requests have different `additional_params`";
    out.back().request_relation = RequestRelation::kDifferentPartitions;
    out.back().bidding_params2.additional_params.Set("additional", "param");

    // Group-by-origin tests.

    // Same interest group name is unlikely when other fields don't match, but
    // best to test it.
    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Group-by-origin: First request group-by-origin";
    out.back().request_relation = RequestRelation::kDifferentPartitions;
    out.back().bidding_params1.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;

    // Same interest group name is unlikely when other fields don't match, but
    // best to test it.
    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Group-by-origin: Second request group-by-origin";
    out.back().request_relation = RequestRelation::kDifferentPartitions;
    out.back().bidding_params2.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Group-by-origin: Different interest group names";
    out.back().request_relation = RequestRelation::kSamePartitionModified;
    out.back().bidding_params1.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
    out.back().bidding_params2.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
    out.back().bidding_params2.interest_group_names = {"other interest group"};

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Group-by-origin: Different keys.";
    out.back().request_relation = RequestRelation::kSamePartitionModified;
    out.back().bidding_params1.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
    out.back().bidding_params2.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
    out.back().bidding_params2.trusted_bidding_signals_keys = {{"other key"}};

    out.emplace_back(CreateDefaultTestCase());
    out.back().description =
        "Group-by-origin: Different keys and interest group names.";
    out.back().request_relation = RequestRelation::kSamePartitionModified;
    out.back().bidding_params1.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
    out.back().bidding_params2.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
    out.back().bidding_params2.interest_group_names = {"other interest group"};
    out.back().bidding_params2.trusted_bidding_signals_keys = {{"other key"}};

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Group-by-origin: Different main frame origins";
    out.back().request_relation = RequestRelation::kDifferentFetches;
    out.back().bidding_params1.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
    out.back().bidding_params2.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
    out.back().bidding_params2.main_frame_origin =
        url::Origin::Create(GURL("https://other.origin.test/"));

    // It would be unusual to have the same IG with different joining origins,
    // since one would overwrite the other, but if it does happen, the requests
    // should use different compression groups.
    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Group-by-origin: Different joining origin.";
    out.back().request_relation = RequestRelation::kDifferentCompressionGroups;
    out.back().bidding_params1.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
    out.back().bidding_params2.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
    out.back().bidding_params2.joining_origin =
        url::Origin::Create(GURL("https://other.joining.origin.test"));

    // Like above test, but the more common case of different IGs with different
    // joining origins.
    out.emplace_back(CreateDefaultTestCase());
    out.back().description =
        "Group-by-origin: Different joining origin, different IGs.";
    out.back().request_relation = RequestRelation::kDifferentCompressionGroups;
    out.back().bidding_params1.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
    out.back().bidding_params2.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
    out.back().bidding_params2.interest_group_names = {"group2"};
    out.back().bidding_params2.joining_origin =
        url::Origin::Create(GURL("https://other.joining.origin.test"));

    return out;
  }

  // Create set of merged bidding parameters. Useful to use with
  // ValidateFetchParams() when two requests should be merged into a single
  // partition.
  BiddingParams CreateMergedBiddingParams(
      const BiddingParams& bidding_params1,
      const BiddingParams& bidding_params2) {
    // In order to merge two sets of params, only `interest_group_names` and
    // `trusted_bidding_signals_keys` may be different.
    EXPECT_EQ(bidding_params1.main_frame_origin,
              bidding_params2.main_frame_origin);
    EXPECT_EQ(bidding_params1.bidder, bidding_params2.bidder);
    EXPECT_EQ(bidding_params1.execution_mode, bidding_params2.execution_mode);
    EXPECT_EQ(bidding_params1.joining_origin, bidding_params2.joining_origin);
    EXPECT_EQ(bidding_params1.trusted_bidding_signals_url,
              bidding_params2.trusted_bidding_signals_url);
    EXPECT_EQ(bidding_params1.additional_params,
              bidding_params2.additional_params);

    BiddingParams merged_bidding_params{
        bidding_params1.main_frame_origin,
        bidding_params1.bidder,
        bidding_params1.interest_group_names,
        bidding_params1.execution_mode,
        bidding_params1.joining_origin,
        bidding_params1.trusted_bidding_signals_url,
        bidding_params1.trusted_bidding_signals_keys,
        bidding_params1.additional_params.Clone()};

    merged_bidding_params.interest_group_names.insert(
        bidding_params2.interest_group_names.begin(),
        bidding_params2.interest_group_names.end());
    if (bidding_params2.trusted_bidding_signals_keys) {
      if (!merged_bidding_params.trusted_bidding_signals_keys) {
        merged_bidding_params.trusted_bidding_signals_keys.emplace();
      }
      for (const auto& key : *bidding_params2.trusted_bidding_signals_keys) {
        if (!base::Contains(*merged_bidding_params.trusted_bidding_signals_keys,
                            key)) {
          merged_bidding_params.trusted_bidding_signals_keys->push_back(key);
        }
      }
    }
    return merged_bidding_params;
  }

  // Returns a pair of a handle and `partition_id`. This pattern reduces
  // boilerplate a bit, at the cost of making types at callsites a little less
  // clear.
  std::pair<scoped_refptr<TestTrustedSignalsCache::Handle>, int>
  RequestTrustedBiddingSignals(const BiddingParams& bidding_params) {
    int partition_id = -1;
    // There should only be a single name for each request. It's a std::set
    // solely for the ValidateFetchParams family of methods.
    CHECK_EQ(1u, bidding_params.interest_group_names.size());
    auto handle = trusted_signals_cache_->RequestTrustedBiddingSignals(
        bidding_params.main_frame_origin, bidding_params.bidder,
        *bidding_params.interest_group_names.begin(),
        bidding_params.execution_mode, bidding_params.joining_origin,
        bidding_params.trusted_bidding_signals_url,
        bidding_params.trusted_bidding_signals_keys,
        bidding_params.additional_params.Clone(), partition_id);

    // The call should never fail.
    CHECK(handle);
    CHECK(!handle->compression_group_token().is_empty());
    CHECK_GE(partition_id, 0);

    return std::pair(std::move(handle), partition_id);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Defaults used by most tests.
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main.frame.test"));
  const url::Origin kBidder = url::Origin::Create(GURL("https://bidder.test"));
  const std::string kInterestGroupName = "group1";
  const url::Origin kJoiningOrigin =
      url::Origin::Create(GURL("https://joining.origin.test"));
  const GURL kTrustedBiddingSignalsUrl{"https://bidder.test/signals"};

  std::unique_ptr<TestTrustedSignalsCache> trusted_signals_cache_;
  mojo::Remote<auction_worklet::mojom::TrustedSignalsCache> cache_mojo_pipe_;
};

// Test the case where a GetTrustedSignals() request is received before the
// fetch completes.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsGetBeforeFetchCompletes) {
  auto bidding_params = CreateDefaultBiddingParams();
  auto [handle, partition_id] = RequestTrustedBiddingSignals(bidding_params);
  EXPECT_EQ(partition_id, 0);

  // Wait for creation of the Fetcher before requesting over Mojo. Not needed,
  // but ensures the events in the test run in a consistent order.
  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id);

  TestTrustedSignalsCacheClient client(handle, cache_mojo_pipe_);

  // Wait for the GetTrustedSignals call to make it to the cache.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(client.has_result());

  RespondToFetchWithSuccess(fetch);

  client.WaitForSuccess();
}

// Test the case where a GetTrustedSignals() request is received before the
// fetch fails.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsGetBeforeFetchFails) {
  auto bidding_params = CreateDefaultBiddingParams();
  auto [handle, partition_id] = RequestTrustedBiddingSignals(bidding_params);
  EXPECT_EQ(partition_id, 0);

  // Wait for creation of the Fetcher before requesting over Mojo. Not needed,
  // but ensures the events in the test run in a consistent order.
  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id);

  TestTrustedSignalsCacheClient client(handle, cache_mojo_pipe_);

  // Wait for the GetTrustedSignals call to make it to the cache.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(client.has_result());

  RespondToFetchWithError(fetch);
  client.WaitForError();
}

// Test the case where a GetTrustedSignals() request is made after the fetch
// completes.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsGetAfterFetchCompletes) {
  auto bidding_params = CreateDefaultBiddingParams();
  auto [handle, partition_id] = RequestTrustedBiddingSignals(bidding_params);
  EXPECT_EQ(partition_id, 0);

  // Wait for the fetch to be observed and response to it. No need to spin the
  // message loop, since fetch responses at this layer are passed directly to
  // the cache, and don't go through Mojo, as the TrustedSignalsFetcher is
  // entirely mocked out.
  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id);
  RespondToFetchWithSuccess(fetch);

  TestTrustedSignalsCacheClient client(handle, cache_mojo_pipe_);
  client.WaitForSuccess();
}

// Test the case where a GetTrustedSignals() request is made after the fetch
// fails.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsGetAfterFetchFails) {
  auto bidding_params = CreateDefaultBiddingParams();
  auto [handle, partition_id] = RequestTrustedBiddingSignals(bidding_params);
  EXPECT_EQ(partition_id, 0);

  // Wait for the fetch to be observed and response to it. No need to spin the
  // message loop, since fetch responses at this layer are passed directly to
  // the cache, and don't go through Mojo, as the TrustedSignalsFetcher is
  // entirely mocked out.
  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id);
  RespondToFetchWithError(fetch);

  TestTrustedSignalsCacheClient client(handle, cache_mojo_pipe_);
  client.WaitForError();
}

// Test the case where a GetTrustedSignals() request waiting on a fetch when the
// Handle is destroyed.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsHandleDestroyedAfterGet) {
  auto bidding_params = CreateDefaultBiddingParams();
  auto [handle, partition_id] = RequestTrustedBiddingSignals(bidding_params);
  EXPECT_EQ(partition_id, 0);
  // Wait for the fetch.
  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();

  TestTrustedSignalsCacheClient client(handle, cache_mojo_pipe_);
  // Wait fo the request to hit the cache.
  base::RunLoop().RunUntilIdle();

  handle.reset();
  client.WaitForError(kRequestCancelledError);
}

// Test the case where a GetTrustedSignals() request is made after the handle
// has been destroyed.
//
// This test covers three cases:
// 1) The fetch was never started before the handle was destroyed.
// 2) The fetch was started but didn't complete before the handle was destroyed.
// 3) The fetch completed before the handle was destroyed.
//
// Since in all cases the handle was destroyed before the read attempt, all
// cases should return errors.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsGetAfterHandleDestroyed) {
  enum class TestCase { kFetchNotStarted, kFetchNotCompleted, kFetchSucceeded };

  for (auto test_case :
       {TestCase::kFetchNotStarted, TestCase::kFetchNotCompleted,
        TestCase::kFetchSucceeded}) {
    SCOPED_TRACE(static_cast<int>(test_case));

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    CreateCache();
    auto bidding_params = CreateDefaultBiddingParams();
    auto [handle, partition_id] = RequestTrustedBiddingSignals(bidding_params);
    EXPECT_EQ(partition_id, 0);
    base::UnguessableToken compression_group_token =
        handle->compression_group_token();

    if (test_case != TestCase::kFetchNotStarted) {
      // Wait for the fetch to be observed.
      auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
      ValidateFetchParams(fetch, bidding_params,
                          /*expected_compression_group_id=*/0, partition_id);
      if (test_case == TestCase::kFetchSucceeded) {
        // Respond to fetch if needed.
        RespondToFetchWithSuccess(fetch);
      }
    }

    handle.reset();

    TestTrustedSignalsCacheClient client(compression_group_token,
                                         cache_mojo_pipe_);
    client.WaitForError(kRequestCancelledError);
  }
}

// Test requesting response bodies with novel keys that did not come from a
// Handle. Note that there's no need to test empty UnguessableTokens - the Mojo
// serialization code DCHECKs when passed them, and the deserialization code
// rejects them.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsGetWithNovelId) {
  // Novel id with no live cache entries.
  TestTrustedSignalsCacheClient client1(base::UnguessableToken::Create(),
                                        cache_mojo_pipe_);
  client1.WaitForError(kRequestCancelledError);

  auto bidding_params = CreateDefaultBiddingParams();
  auto [handle, partition_id] = RequestTrustedBiddingSignals(bidding_params);

  // Novel id  with a cache entry with a pending fetch.
  TestTrustedSignalsCacheClient client2(base::UnguessableToken::Create(),
                                        cache_mojo_pipe_);
  client2.WaitForError(kRequestCancelledError);

  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id);
  RespondToFetchWithSuccess(fetch);

  // Novel id with a loaded cache entry.
  TestTrustedSignalsCacheClient client3(base::UnguessableToken::Create(),
                                        cache_mojo_pipe_);
  client3.WaitForError(kRequestCancelledError);
}

// Tests multiple GetTrustedSignals calls for a single request, with one live
// handle. Requests are made both before and after the response has been
// received.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsGetMultipleTimes) {
  auto bidding_params = CreateDefaultBiddingParams();
  auto [handle, partition_id] = RequestTrustedBiddingSignals(bidding_params);

  // Wait for creation of the Fetcher before requesting over Mojo. Not needed,
  // but ensures the events in the test run in a consistent order.
  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id);

  TestTrustedSignalsCacheClient client1(handle, cache_mojo_pipe_);
  TestTrustedSignalsCacheClient client2(handle, cache_mojo_pipe_);
  TestTrustedSignalsCacheClient client3(handle, cache_mojo_pipe_);

  // Wait for the GetTrustedSignals call to make it to the cache.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(client1.has_result());
  EXPECT_FALSE(client2.has_result());
  EXPECT_FALSE(client3.has_result());

  RespondToFetchWithSuccess(fetch);
  TestTrustedSignalsCacheClient client4(handle, cache_mojo_pipe_);
  TestTrustedSignalsCacheClient client5(handle, cache_mojo_pipe_);
  TestTrustedSignalsCacheClient client6(handle, cache_mojo_pipe_);
  client1.WaitForSuccess();
  client2.WaitForSuccess();
  client3.WaitForSuccess();
  client4.WaitForSuccess();
  client5.WaitForSuccess();
  client6.WaitForSuccess();
}

// Check that re-requesting trusted bidding with the same arguments returns the
// same handle and IDs, when any Handle is still alive.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsReRequestSignalsReused) {
  auto bidding_params = CreateDefaultBiddingParams();
  auto [handle1, partition_id1] = RequestTrustedBiddingSignals(bidding_params);

  auto [handle2, partition_id2] = RequestTrustedBiddingSignals(bidding_params);
  EXPECT_EQ(handle1, handle2);
  EXPECT_EQ(partition_id1, partition_id2);

  // Destroying the first handle should not cancel the request. This should be
  // implied by `handle1` and `handle2` being references to the same object as
  // well.
  handle1.reset();

  // Wait for Fetcher.
  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id1);

  // Create yet another handle, which should again be merged, and destroy the
  // second handle.
  auto [handle3, partition_id3] = RequestTrustedBiddingSignals(bidding_params);
  EXPECT_EQ(handle2, handle3);
  EXPECT_EQ(partition_id2, partition_id3);
  handle2.reset();

  // Complete the request.
  RespondToFetchWithSuccess(fetch);

  // Create yet another handle, which should again be merged, and destroy the
  // third handle.
  auto [handle4, partition_id4] = RequestTrustedBiddingSignals(bidding_params);
  EXPECT_EQ(handle3, handle4);
  EXPECT_EQ(partition_id3, partition_id4);
  handle3.reset();

  // Finally request the response body, which should succeed.
  TestTrustedSignalsCacheClient client(handle4, cache_mojo_pipe_);
  client.WaitForSuccess();

  // No pending fetches should have been created after the first.
  EXPECT_EQ(trusted_signals_cache_->num_pending_fetches(), 0u);
}

// Check that re-requesting trusted bidding with the same arguments returns a
// different ID, when all Handles have been destroyed. Tests all points at which
// a Handle may be deleted.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsReRequestSignalsNotReused) {
  auto bidding_params = CreateDefaultBiddingParams();

  // Create a Handle, create a request for it, destroy the Handle.
  auto [handle1, partition_id1] = RequestTrustedBiddingSignals(bidding_params);
  base::UnguessableToken compression_group_token1 =
      handle1->compression_group_token();
  TestTrustedSignalsCacheClient client1(handle1, cache_mojo_pipe_);
  handle1.reset();
  EXPECT_EQ(trusted_signals_cache_->num_pending_fetches(), 0u);

  // A new request with the same parameters should get a new
  // `compression_group_id`.
  auto [handle2, partition_id2] = RequestTrustedBiddingSignals(bidding_params);
  base::UnguessableToken compression_group_token2 =
      handle2->compression_group_token();
  EXPECT_NE(compression_group_token1, compression_group_token2);
  TestTrustedSignalsCacheClient client2(handle2, cache_mojo_pipe_);

  // Wait for fetch request, then destroy the second handle.
  auto fetch2 = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch2, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id2);
  handle2.reset();

  // A new request with the same parameters should get a new
  // `compression_group_id`.
  auto [handle3, partition_id3] = RequestTrustedBiddingSignals(bidding_params);
  base::UnguessableToken compression_group_token3 =
      handle3->compression_group_token();
  EXPECT_NE(compression_group_token1, compression_group_token3);
  EXPECT_NE(compression_group_token2, compression_group_token3);
  TestTrustedSignalsCacheClient client3(handle3, cache_mojo_pipe_);
  // Wait for the request from `client3` to make it to the cache.
  base::RunLoop().RunUntilIdle();

  // Wait for another fetch request, send a response, and retrieve it over the
  // Mojo pipe.
  auto fetch3 = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch3, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id3);
  RespondToFetchWithSuccess(fetch3);

  // Destroy the third handle.
  handle3.reset();

  // Create a new request with the same parameters should get a new
  // `compression_group_id`.
  auto [handle4, partition_id4] = RequestTrustedBiddingSignals(bidding_params);
  base::UnguessableToken compression_group_token4 =
      handle4->compression_group_token();
  EXPECT_NE(compression_group_token1, compression_group_token4);
  EXPECT_NE(compression_group_token2, compression_group_token4);
  EXPECT_NE(compression_group_token3, compression_group_token4);
  TestTrustedSignalsCacheClient client4(handle4, cache_mojo_pipe_);
  // Wait for the request from `client4` to make it to the cache.
  base::RunLoop().RunUntilIdle();
  // Wait for the fetch.
  auto fetch4 = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  // Destroy the handle, which should fail the request.
  handle4.reset();

  // All cache clients but the third should receive errorse
  client1.WaitForError(kRequestCancelledError);
  client2.WaitForError(kRequestCancelledError);
  client3.WaitForSuccess();
  client4.WaitForError(kRequestCancelledError);
}

// Test the case where a bidding signals request is made while there's still an
// outstanding Handle, but the response has expired.
TEST_F(TrustedSignalsCacheTest,
       BiddingSignalsOutstandingHandleResponseExpired) {
  const base::TimeDelta kTtl = base::Minutes(10);
  // A small amount of time. Test will wait until this much time before
  // expiration, and then wait for this much time to pass, to check before/after
  // expiration behavior.
  const base::TimeDelta kTinyTime = base::Milliseconds(1);

  auto bidding_params = CreateDefaultBiddingParams();
  auto [handle1, partition_id1] = RequestTrustedBiddingSignals(bidding_params);

  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id1);
  RespondToFetchWithSuccess(
      fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      kSuccessBody, kTtl);

  // Wait until just before the response has expired.
  task_environment_.FastForwardBy(kTtl - kTinyTime);

  // A request for `handle1`'s data should succeed.
  TestTrustedSignalsCacheClient(handle1, cache_mojo_pipe_).WaitForSuccess();

  // Re-requesting the data before expiration time should return the same Handle
  // and partition.
  auto [handle2, partition_id2] = RequestTrustedBiddingSignals(bidding_params);
  EXPECT_EQ(handle1, handle2);
  EXPECT_EQ(partition_id1, partition_id2);

  // Run until the expiration time. When the time exactly equals the expiration
  // time, the entry should be considered expired.
  task_environment_.FastForwardBy(kTinyTime);

  // A request for `handle1`'s data should return the same value as before.
  TestTrustedSignalsCacheClient(handle1, cache_mojo_pipe_).WaitForSuccess();

  // Re-request the data. A different handle should be returned, since the old
  // data has expired.
  auto [handle3, partition_id3] = RequestTrustedBiddingSignals(bidding_params);
  EXPECT_NE(handle1->compression_group_token(),
            handle3->compression_group_token());

  // Give a different response for the second fetch.
  fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id3);
  RespondToFetchWithSuccess(
      fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
      kOtherSuccessBody, kTtl);

  // A request for `handle3`'s data should return the different data.
  TestTrustedSignalsCacheClient(handle3, cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
          kOtherSuccessBody);

  // A request for `handle1`'s data should return the same value as before, even
  // though it has expired.
  TestTrustedSignalsCacheClient(handle1, cache_mojo_pipe_).WaitForSuccess();
}

// Check that bidding signals error responses are not cached beyond the end of
// the fetch.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsOutstandingHandleError) {
  auto bidding_params = CreateDefaultBiddingParams();
  auto [handle1, partition_id1] = RequestTrustedBiddingSignals(bidding_params);

  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id1);

  // Re-requesting the data before the response is received should return the
  // same Handle and partition.
  auto [handle2, partition_id2] = RequestTrustedBiddingSignals(bidding_params);
  EXPECT_EQ(handle1, handle2);
  EXPECT_EQ(partition_id1, partition_id2);

  RespondToFetchWithError(fetch);

  // A request for `handle1`'s data should return the error.
  TestTrustedSignalsCacheClient(handle1, cache_mojo_pipe_).WaitForError();

  // Re-request the data. A different handle should be returned, since the error
  // should not be cached.
  auto [handle3, partition_id3] = RequestTrustedBiddingSignals(bidding_params);
  EXPECT_NE(handle1->compression_group_token(),
            handle3->compression_group_token());

  // Give a success response for the second fetch.
  fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id3);
  RespondToFetchWithSuccess(fetch);

  // A request for `handle3`'s data should return a success.
  TestTrustedSignalsCacheClient(handle3, cache_mojo_pipe_).WaitForSuccess();

  // A request for `handle1`'s data should still return the error.
  TestTrustedSignalsCacheClient(handle1, cache_mojo_pipe_).WaitForError();
}

// Check that zero (and negative) TTL bidding signals responses are handled
// appropriately.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsOutstandingHandleSuccessZeroTTL) {
  for (base::TimeDelta ttl : {base::Seconds(-1), base::Seconds(0)}) {
    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    CreateCache();

    auto bidding_params = CreateDefaultBiddingParams();
    auto [handle1, partition_id1] =
        RequestTrustedBiddingSignals(bidding_params);

    auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
    ValidateFetchParams(fetch, bidding_params,
                        /*expected_compression_group_id=*/0, partition_id1);

    // Re-requesting the data before a response is received should return the
    // same Handle and partition.
    auto [handle2, partition_id2] =
        RequestTrustedBiddingSignals(bidding_params);
    EXPECT_EQ(handle1, handle2);
    EXPECT_EQ(partition_id1, partition_id2);

    RespondToFetchWithSuccess(
        fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
        kSuccessBody, ttl);

    // A request for `handle1`'s data should succeed.
    TestTrustedSignalsCacheClient(handle1, cache_mojo_pipe_).WaitForSuccess();

    // Re-request the data. A different handle should be returned, since the
    // data should not be cached.
    auto [handle3, partition_id3] =
        RequestTrustedBiddingSignals(bidding_params);
    EXPECT_NE(handle1->compression_group_token(),
              handle3->compression_group_token());

    // Give a different response for the second fetch.
    fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
    ValidateFetchParams(fetch, bidding_params,
                        /*expected_compression_group_id=*/0, partition_id3);
    RespondToFetchWithSuccess(
        fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
        kOtherSuccessBody, ttl);

    // A request for `handle3`'s data should return the different data.
    TestTrustedSignalsCacheClient(handle3, cache_mojo_pipe_)
        .WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
            kOtherSuccessBody);

    // A request for `handle1`'s data should return the same value as before,
    // even though it has expired.
    TestTrustedSignalsCacheClient(handle1, cache_mojo_pipe_).WaitForSuccess();
  }
}

// Test the case of expiration of two requests that share the same compression
// group, but are in different partitions.
TEST_F(TrustedSignalsCacheTest,
       BiddingSignalsOutstandingHandleResponseExpiredSharedCompressionGroup) {
  const base::TimeDelta kTtl = base::Minutes(10);
  // A small amount of time. Test will wait until this much time before
  // expiration, and then wait for this much time to pass, to check before/after
  // expiration behavior.
  const base::TimeDelta kTinyTime = base::Milliseconds(1);

  auto bidding_params1 = CreateDefaultBiddingParams();
  auto bidding_params2 = CreateDefaultBiddingParams();
  bidding_params2.interest_group_names = {"other interest group"};

  // Since the two IGs have the same joining origin, but different names, and do
  // not use group-by-origin mode, the requests for the two sets of parameters
  // should be in different partitions in the same compression group, so should
  // share a Handle, but have different partition IDs.
  auto [handle1, partition_id1] = RequestTrustedBiddingSignals(bidding_params1);
  auto [handle2, partition_id2] = RequestTrustedBiddingSignals(bidding_params2);
  EXPECT_EQ(handle1, handle2);
  EXPECT_NE(partition_id1, partition_id2);
  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();

  EXPECT_EQ(fetch.trusted_bidding_signals_url,
            bidding_params1.trusted_bidding_signals_url);
  ASSERT_EQ(fetch.compression_groups.size(), 1u);
  EXPECT_EQ(fetch.compression_groups.begin()->first, 0);

  const auto& partitions = fetch.compression_groups.begin()->second;
  ASSERT_EQ(partitions.size(), 2u);
  ValidateFetchParamsForPartition(partitions[0], bidding_params1,
                                  partition_id1);
  ValidateFetchParamsForPartition(partitions[1], bidding_params2,
                                  partition_id2);
  RespondToFetchWithSuccess(
      fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      kSuccessBody, kTtl);

  // Wait until just before the response has expired.
  task_environment_.FastForwardBy(kTtl - kTinyTime);

  // Re-requesting either set of parameters should return the same Handle and
  // partition as the first requests.
  auto [handle3, partition_id3] = RequestTrustedBiddingSignals(bidding_params1);
  EXPECT_EQ(handle1, handle3);
  EXPECT_EQ(partition_id1, partition_id3);
  auto [handle4, partition_id4] = RequestTrustedBiddingSignals(bidding_params2);
  EXPECT_EQ(handle2, handle4);
  EXPECT_EQ(partition_id2, partition_id4);

  // Run until the expiration time. When the time exactly equals the expiration
  // time, the entry should be considered expired.
  task_environment_.FastForwardBy(kTinyTime);

  // Re-request the data for both parameters. A different Handle should be
  // returned from the original, since the old data has expired. As before, both
  // requests should share a Handle but have distinct partition IDs.
  auto [handle5, partition_id5] = RequestTrustedBiddingSignals(bidding_params1);
  EXPECT_NE(handle1->compression_group_token(),
            handle5->compression_group_token());
  auto [handle6, partition_id6] = RequestTrustedBiddingSignals(bidding_params2);
  EXPECT_NE(handle2->compression_group_token(),
            handle6->compression_group_token());
  EXPECT_EQ(handle5, handle6);
  EXPECT_NE(partition_id5, partition_id6);

  // Give a different response for the second fetch.
  fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  RespondToFetchWithSuccess(
      fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
      kOtherSuccessBody, kTtl);

  // A request for `handle5`'s data should return the second fetch's data. No
  // need to request the data for `handle6`, since it's the same Handle.
  TestTrustedSignalsCacheClient(handle5, cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
          kOtherSuccessBody);

  // A request for `handle1`'s data should return the first fetch's data. No
  // need to request the data for `handle2`, since it's the same Handle.
  TestTrustedSignalsCacheClient(handle1, cache_mojo_pipe_).WaitForSuccess();
}

// Test the case of expiration of two requests that are sent in the same fetch,
// but in different compression groups. The requests have different expiration
// times.
TEST_F(
    TrustedSignalsCacheTest,
    BiddingSignalsOutstandingHandleResponseExpiredDifferentCompressionGroup) {
  const base::TimeDelta kTtl1 = base::Minutes(5);
  const base::TimeDelta kTtl2 = base::Minutes(10);
  // A small amount of time. Test will wait until this much time before
  // expiration, and then wait for this much time to pass, to check before/after
  // expiration behavior.
  const base::TimeDelta kTinyTime = base::Milliseconds(1);

  auto bidding_params1 = CreateDefaultBiddingParams();
  auto bidding_params2 = CreateDefaultBiddingParams();
  bidding_params2.joining_origin =
      url::Origin::Create(GURL("https://other.joining.origin.test"));

  auto [handle1, partition_id1] = RequestTrustedBiddingSignals(bidding_params1);
  auto [handle2, partition_id2] = RequestTrustedBiddingSignals(bidding_params2);
  EXPECT_NE(handle1, handle2);
  EXPECT_NE(handle1->compression_group_token(),
            handle2->compression_group_token());
  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();

  EXPECT_EQ(fetch.trusted_bidding_signals_url,
            bidding_params1.trusted_bidding_signals_url);
  ASSERT_EQ(fetch.compression_groups.size(), 2u);

  // Compression groups are appended in FIFO order.
  ASSERT_EQ(1u, fetch.compression_groups.count(0));
  ValidateFetchParamsForPartitions(fetch.compression_groups.at(0),
                                   bidding_params1, partition_id1);
  ASSERT_EQ(1u, fetch.compression_groups.count(1));
  ValidateFetchParamsForPartitions(fetch.compression_groups.at(1),
                                   bidding_params2, partition_id2);

  // Respond with different results for each compression group.
  RespondToTwoCompressionGroupFetchWithSuccess(fetch, kTtl1, kTtl2);

  // Wait until just before the first compression group's data has expired.
  task_environment_.FastForwardBy(kTtl1 - kTinyTime);

  // Re-request both sets of parameters. The same Handles should be returned.
  auto [handle3, partition_id3] = RequestTrustedBiddingSignals(bidding_params1);
  EXPECT_EQ(handle1, handle3);
  EXPECT_EQ(partition_id1, partition_id3);
  auto [handle4, partition_id4] = RequestTrustedBiddingSignals(bidding_params2);
  EXPECT_EQ(handle2, handle4);
  EXPECT_EQ(partition_id2, partition_id4);

  // Wait until the first compression group's data has expired.
  task_environment_.FastForwardBy(kTinyTime);

  // Re-request both sets of parameters. The first set of parameters should get
  // a new handle, and trigger a new fetch. The second set of parameters should
  // get the same Handle, since it has yet to expire.
  auto [handle5, partition_id5] = RequestTrustedBiddingSignals(bidding_params1);
  EXPECT_NE(handle1, handle5);
  auto [handle6, partition_id6] = RequestTrustedBiddingSignals(bidding_params2);
  EXPECT_EQ(handle2, handle6);
  EXPECT_EQ(partition_id2, partition_id6);

  // Validate there is indeed a new fetch for the first set of parameters, and
  // provide a response.
  fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params1,
                      /*expected_compression_group_id=*/0, partition_id5);
  RespondToFetchWithSuccess(
      fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
      kSomeOtherSuccessBody, kTtl2);

  // Wait until just before the first compression group's data has expired.
  task_environment_.FastForwardBy(kTtl1 - kTinyTime);

  // Re-request both sets of parameters. The same Handles should be returned as
  // the last time.
  auto [handle7, partition_id7] = RequestTrustedBiddingSignals(bidding_params1);
  EXPECT_EQ(handle5, handle7);
  EXPECT_EQ(partition_id5, partition_id7);
  auto [handle8, partition_id8] = RequestTrustedBiddingSignals(bidding_params2);
  EXPECT_EQ(handle2, handle8);
  EXPECT_EQ(partition_id2, partition_id8);

  // Wait until the second compression group's data has expired.
  task_environment_.FastForwardBy(kTinyTime);

  // Re-request both sets of parameters. This time, only the second set of
  // parameters should get a new Handle.
  auto [handle9, partition_id9] = RequestTrustedBiddingSignals(bidding_params1);
  EXPECT_EQ(handle5, handle9);
  EXPECT_EQ(partition_id5, partition_id9);
  auto [handle10, partition_id10] =
      RequestTrustedBiddingSignals(bidding_params2);
  EXPECT_NE(handle2, handle10);

  // Validate there is indeed a new fetch for the second set of parameters, and
  // provide a response.
  fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params2,
                      /*expected_compression_group_id=*/0, partition_id9);
  RespondToFetchWithSuccess(
      fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      kSomeOtherSuccessBody, kTtl2);

  // Validate the responses for each of the distinct Handles. Even the ones
  // associated with expired data should still receive success responses, since
  // data lifetime is scoped to that of the associated Handle.
  TestTrustedSignalsCacheClient(handle1, cache_mojo_pipe_).WaitForSuccess();
  TestTrustedSignalsCacheClient(handle2, cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
          kOtherSuccessBody);
  TestTrustedSignalsCacheClient(handle5, cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
          kSomeOtherSuccessBody);
  TestTrustedSignalsCacheClient(handle10, cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
          kSomeOtherSuccessBody);
}

// Test the case where the response has no compression groups.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsNoCompressionGroup) {
  auto bidding_params = CreateDefaultBiddingParams();
  auto [handle, partition_id] = RequestTrustedBiddingSignals(bidding_params);

  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id);

  // Respond with an empty map with no compression groups.
  std::move(fetch.callback).Run({});

  TestTrustedSignalsCacheClient(handle, cache_mojo_pipe_)
      .WaitForError("Fetched signals missing compression group 0.");
}

// Test the case where only information for the wrong compression group is
// received.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsWrongCompressionGroup) {
  auto bidding_params = CreateDefaultBiddingParams();
  auto [handle, partition_id] = RequestTrustedBiddingSignals(bidding_params);

  auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch, bidding_params,
                      /*expected_compression_group_id=*/0, partition_id);

  // Modify index of the only compression group when generating a response.
  CHECK(base::Contains(fetch.compression_groups, 0));
  std::map<int, std::vector<TrustedSignalsFetcher::BiddingPartition>>
      modified_compression_groups;
  modified_compression_groups.emplace(1,
                                      std::move(fetch.compression_groups[0]));
  fetch.compression_groups = std::move(modified_compression_groups);
  RespondToFetchWithSuccess(fetch);

  // A request for `handle3`'s data should return the different data.
  TestTrustedSignalsCacheClient(handle, cache_mojo_pipe_)
      .WaitForError("Fetched signals missing compression group 0.");
}

// Test the case where only one of two compression groups is returned by the
// server. Both compression groups should fail. Run two test cases, one with the
// first compression group missing, one with the second missing.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsOneCompressionGroupMissing) {
  for (int missing_group : {0, 1}) {
    auto bidding_params1 = CreateDefaultBiddingParams();
    auto bidding_params2 = CreateDefaultBiddingParams();
    bidding_params2.joining_origin =
        url::Origin::Create(GURL("https://other.joining.origin.test"));

    auto [handle1, partition_id1] =
        RequestTrustedBiddingSignals(bidding_params1);
    auto [handle2, partition_id2] =
        RequestTrustedBiddingSignals(bidding_params2);
    EXPECT_NE(handle1->compression_group_token(),
              handle2->compression_group_token());

    auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
    EXPECT_EQ(fetch.trusted_bidding_signals_url,
              bidding_params1.trusted_bidding_signals_url);
    ASSERT_EQ(fetch.compression_groups.size(), 2u);

    // Remove missing compression group from the request, and generate a valid
    // response for the other group.
    ASSERT_TRUE(fetch.compression_groups.erase(missing_group));
    RespondToFetchWithSuccess(fetch);

    std::string expected_error = base::StringPrintf(
        "Fetched signals missing compression group %i.", missing_group);

    // Even though the data for only one Handle was missing, both should have
    // the same error.
    TestTrustedSignalsCacheClient(handle1, cache_mojo_pipe_)
        .WaitForError(expected_error);
    TestTrustedSignalsCacheClient(handle2, cache_mojo_pipe_)
        .WaitForError(expected_error);
  }
}

// Tests the case where request is made, and then a second request with one
// different parameter is issued before any fetch is started. The behavior is
// expected to vary based on which parameter is modified. The possibilities are:
//
// * kDifferentFetches: Different fetches.
//
// * kDifferentCompressionGroups: Different compression groups within a single
// fetch.
//
// * kDifferentPartitions: Different partitions within the same compression
// group.
//
// * kSamePartitionModified, kSamePartitionUnmodified: Same partition is used.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsDifferentParamsBeforeFetchStart) {
  for (const auto& test_case : CreateBidderTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    CreateCache();
    const auto& bidding_params1 = test_case.bidding_params1;
    const auto& bidding_params2 = test_case.bidding_params2;

    auto [handle1, partition_id1] =
        RequestTrustedBiddingSignals(bidding_params1);
    auto [handle2, partition_id2] =
        RequestTrustedBiddingSignals(bidding_params2);

    switch (test_case.request_relation) {
      case RequestRelation::kDifferentFetches: {
        ASSERT_NE(handle1, handle2);
        ASSERT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());
        auto fetches = trusted_signals_cache_->WaitForBiddingSignalsFetches(2);

        // fetches are made in FIFO order.
        ValidateFetchParams(fetches[0], bidding_params1,
                            /*expected_compression_group_id=*/0, partition_id1);
        ValidateFetchParams(fetches[1], bidding_params2,
                            /*expected_compression_group_id=*/0, partition_id2);

        // Make both requests succeed with different bodies, and check that they
        // can be read.
        RespondToFetchWithSuccess(fetches[0]);
        RespondToFetchWithSuccess(
            fetches[1],
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        TestTrustedSignalsCacheClient client1(handle1, cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client2(handle2, cache_mojo_pipe_);
        client1.WaitForSuccess();
        client2.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentCompressionGroups: {
        EXPECT_NE(handle1, handle2);
        EXPECT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());
        auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();

        EXPECT_EQ(fetch.trusted_bidding_signals_url,
                  bidding_params1.trusted_bidding_signals_url);
        ASSERT_EQ(fetch.compression_groups.size(), 2u);

        // Compression groups are appended in FIFO order.
        ASSERT_EQ(1u, fetch.compression_groups.count(0));
        ValidateFetchParamsForPartitions(fetch.compression_groups.at(0),
                                         bidding_params1, partition_id1);
        ASSERT_EQ(1u, fetch.compression_groups.count(1));
        ValidateFetchParamsForPartitions(fetch.compression_groups.at(1),
                                         bidding_params2, partition_id2);

        // Respond with different results for each compression group.
        RespondToTwoCompressionGroupFetchWithSuccess(fetch);
        TestTrustedSignalsCacheClient client1(handle1, cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client2(handle2, cache_mojo_pipe_);
        client1.WaitForSuccess();
        client2.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentPartitions: {
        EXPECT_EQ(handle1, handle2);
        EXPECT_NE(partition_id1, partition_id2);
        auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();

        EXPECT_EQ(fetch.trusted_bidding_signals_url,
                  bidding_params1.trusted_bidding_signals_url);
        ASSERT_EQ(fetch.compression_groups.size(), 1u);
        EXPECT_EQ(fetch.compression_groups.begin()->first, 0);

        const auto& partitions = fetch.compression_groups.begin()->second;
        ASSERT_EQ(partitions.size(), 2u);
        ValidateFetchParamsForPartition(partitions[0], bidding_params1,
                                        partition_id1);
        ValidateFetchParamsForPartition(partitions[1], bidding_params2,
                                        partition_id2);

        // Respond with a single response for the partition, and read it - no
        // need for multiple clients, since the handles are the same.
        RespondToFetchWithSuccess(fetch);
        TestTrustedSignalsCacheClient client(handle1, cache_mojo_pipe_);
        client.WaitForSuccess();
        break;
      }

      case RequestRelation::kSamePartitionModified:
      case RequestRelation::kSamePartitionUnmodified: {
        EXPECT_EQ(handle1, handle2);
        EXPECT_EQ(partition_id1, partition_id2);
        auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();

        auto merged_bidding_params =
            CreateMergedBiddingParams(bidding_params1, bidding_params2);
        // The fetch exactly match the merged parameters.
        ValidateFetchParams(fetch, merged_bidding_params,
                            /*expected_compression_group_id=*/0, partition_id1);

        // Respond with a single response for the partition, and read it - no
        // need for multiple clients, since the handles are the same.
        RespondToFetchWithSuccess(fetch);
        TestTrustedSignalsCacheClient client(handle1, cache_mojo_pipe_);
        client.WaitForSuccess();
        break;
      }
    }
  }
}

// Tests the case where request is made, and then after a fetch starts, a second
// request with one different parameter is issued. The possible behaviors are:
//
// * kDifferentFetches, kDifferentCompressionGroups,
// kDifferentCompressionGroups, kSamePartitionModified: A new fetch is made.
//
// * kSamePartitionUnmodified: Old response is reused.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsDifferentParamsAfterFetchStart) {
  for (const auto& test_case : CreateBidderTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    CreateCache();
    const auto& bidding_params1 = test_case.bidding_params1;
    const auto& bidding_params2 = test_case.bidding_params2;

    auto [handle1, partition_id1] =
        RequestTrustedBiddingSignals(bidding_params1);
    auto fetch1 = trusted_signals_cache_->WaitForBiddingSignalsFetch();
    ValidateFetchParams(fetch1, bidding_params1,
                        /*expected_compression_group_id=*/0, partition_id1);
    auto [handle2, partition_id2] =
        RequestTrustedBiddingSignals(bidding_params2);

    switch (test_case.request_relation) {
      case RequestRelation::kDifferentFetches:
      case RequestRelation::kDifferentCompressionGroups:
      case RequestRelation::kDifferentPartitions:
      case RequestRelation::kSamePartitionModified: {
        ASSERT_NE(handle1, handle2);
        ASSERT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());

        auto fetch2 = trusted_signals_cache_->WaitForBiddingSignalsFetch();
        ValidateFetchParams(fetch2, bidding_params2,
                            /*expected_compression_group_id=*/0, partition_id2);

        // Make both requests succeed with different bodies, and check that they
        // can be read.
        RespondToFetchWithSuccess(fetch1);
        RespondToFetchWithSuccess(
            fetch2,
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        TestTrustedSignalsCacheClient client1(handle1, cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client2(handle2, cache_mojo_pipe_);
        client1.WaitForSuccess();
        client2.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kSamePartitionUnmodified: {
        EXPECT_EQ(handle1, handle2);
        EXPECT_EQ(partition_id1, partition_id2);

        // Respond with a single response for the partition, and read it - no
        // need for multiple clients, since the handles are the same.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client(handle1, cache_mojo_pipe_);
        client.WaitForSuccess();
        break;
      }
    }
  }
}

// Tests the case where request is made, a fetch is made and then completes. The
// a second request with one different parameter is issued. The possibilities
// are:
//
// * kDifferentFetches, kDifferentCompressionGroups,
// kDifferentCompressionGroups, kSamePartitionModified: A new fetch.
//
// * kSamePartitionUnmodified: Old response is reused.
TEST_F(TrustedSignalsCacheTest,
       BiddingSignalsDifferentParamsAfterFetchComplete) {
  for (const auto& test_case : CreateBidderTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    CreateCache();
    const auto& bidding_params1 = test_case.bidding_params1;
    const auto& bidding_params2 = test_case.bidding_params2;

    auto [handle1, partition_id1] =
        RequestTrustedBiddingSignals(bidding_params1);
    auto fetch1 = trusted_signals_cache_->WaitForBiddingSignalsFetch();
    ValidateFetchParams(fetch1, bidding_params1,
                        /*expected_compression_group_id=*/0, partition_id1);
    RespondToFetchWithSuccess(fetch1);
    TestTrustedSignalsCacheClient client1(handle1, cache_mojo_pipe_);
    client1.WaitForSuccess();

    auto [handle2, partition_id2] =
        RequestTrustedBiddingSignals(bidding_params2);

    switch (test_case.request_relation) {
      case RequestRelation::kDifferentFetches:
      case RequestRelation::kDifferentCompressionGroups:
      case RequestRelation::kDifferentPartitions:
      case RequestRelation::kSamePartitionModified: {
        ASSERT_NE(handle1, handle2);
        ASSERT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());

        auto fetch2 = trusted_signals_cache_->WaitForBiddingSignalsFetch();
        ValidateFetchParams(fetch2, bidding_params2,
                            /*expected_compression_group_id=*/0, partition_id2);

        RespondToFetchWithSuccess(
            fetch2,
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        TestTrustedSignalsCacheClient client2(handle2, cache_mojo_pipe_);
        client2.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kSamePartitionUnmodified: {
        EXPECT_EQ(handle1, handle2);
        EXPECT_EQ(partition_id1, partition_id2);
        TestTrustedSignalsCacheClient client2(handle2, cache_mojo_pipe_);
        client2.WaitForSuccess();
        break;
      }
    }
  }
}

// Tests the case where request is made, and then a second request with one
// different parameter is created and canceled before any fetch is started. The
// fetch completes, and then the second request is made again. The possibilities
// are:
//
// kDifferentFetches: The requests weren't merged in the first place. Second
// fetch is cancelled, and then a new one is made.
//
// kDifferentCompressionGroups: The requests were merged into single compression
// groups in a single fetch. The compression group for the second request should
// be removed before the fetch is made, and a new one made. This looks just like
// the kDifferentFetch case externally.
//
// * kDifferentPartitions: Different partitions within the same compression
// group. Since lifetimes are managed at the compression group layer, the
// partition is not removed when the request is cancelled. Only one fetch is
// made.
//
// * kSamePartitionModified / kSamePartitionUnmodified: Same partition is used.
// Only one fetch is made.
TEST_F(TrustedSignalsCacheTest,
       BiddingSignalsDifferentParamsCancelSecondBeforeFetchStart) {
  for (const auto& test_case : CreateBidderTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    CreateCache();
    const auto& bidding_params1 = test_case.bidding_params1;
    const auto& bidding_params2 = test_case.bidding_params2;

    // Don't bother to compare handles here - that's covered by another test.
    auto [handle1, partition_id1] =
        RequestTrustedBiddingSignals(bidding_params1);
    auto [handle2, partition_id2] =
        RequestTrustedBiddingSignals(bidding_params2);

    // Cancel the second request immediately, before any fetch is made.
    handle2.reset();

    // In all cases, that should result in a single fetch being made.
    auto fetch1 = trusted_signals_cache_->WaitForBiddingSignalsFetch();

    switch (test_case.request_relation) {
      // Despite these two cases being different internally, they look the same
      // both to the caller and to the created fetches.
      case RequestRelation::kDifferentFetches:
      case RequestRelation::kDifferentCompressionGroups: {
        // Fetch should not be affected by the second bid.
        ValidateFetchParams(fetch1, bidding_params1,
                            /*expected_compression_group_id=*/0, partition_id1);
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client1(handle1, cache_mojo_pipe_);
        client1.WaitForSuccess();

        // Make a second request using `bidding_params2`. It should result in a
        // new request.
        auto [handle3, partition_id3] =
            RequestTrustedBiddingSignals(bidding_params2);
        EXPECT_NE(handle1, handle3);
        auto fetch3 = trusted_signals_cache_->WaitForBiddingSignalsFetch();
        ValidateFetchParams(fetch3, bidding_params2,
                            /*expected_compression_group_id=*/0, partition_id3);
        RespondToFetchWithSuccess(
            fetch3,
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        TestTrustedSignalsCacheClient client3(handle3, cache_mojo_pipe_);
        client3.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentPartitions: {
        EXPECT_EQ(fetch1.trusted_bidding_signals_url,
                  bidding_params1.trusted_bidding_signals_url);
        ASSERT_EQ(fetch1.compression_groups.size(), 1u);
        EXPECT_EQ(fetch1.compression_groups.begin()->first, 0);

        const auto& partitions = fetch1.compression_groups.begin()->second;
        ASSERT_EQ(partitions.size(), 2u);
        ValidateFetchParamsForPartition(partitions[0], bidding_params1,
                                        partition_id1);
        ValidateFetchParamsForPartition(partitions[1], bidding_params2,
                                        partition_id2);

        // Respond with a single response for the partition, and read it.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client1(handle1, cache_mojo_pipe_);
        client1.WaitForSuccess();

        // Make a second request using `bidding_params2`. It should reuse the
        // response to the initial request.
        auto [handle3, partition_id3] =
            RequestTrustedBiddingSignals(bidding_params2);
        EXPECT_EQ(handle1, handle3);
        EXPECT_NE(partition_id1, partition_id3);
        EXPECT_EQ(partition_id2, partition_id3);
        TestTrustedSignalsCacheClient client3(handle3, cache_mojo_pipe_);
        client3.WaitForSuccess();
        break;
      }

      case RequestRelation::kSamePartitionModified:
      case RequestRelation::kSamePartitionUnmodified: {
        auto merged_bidding_params =
            CreateMergedBiddingParams(bidding_params1, bidding_params2);
        ValidateFetchParams(fetch1, merged_bidding_params,
                            /*expected_compression_group_id=*/0, partition_id1);

        // Respond with a single response for the partition, and read it.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client1(handle1, cache_mojo_pipe_);
        client1.WaitForSuccess();

        // Make a second request using `bidding_params2`. It should reuse the
        // response to the initial request, including the same partition ID.
        auto [handle3, partition_id3] =
            RequestTrustedBiddingSignals(bidding_params2);
        EXPECT_EQ(handle1, handle3);
        EXPECT_EQ(partition_id1, partition_id3);

        // For the sake of completeness, read the response again.
        TestTrustedSignalsCacheClient client3(handle3, cache_mojo_pipe_);
        client3.WaitForSuccess();
        break;
      }
    }
  }
}

// Just like the above test, but the first request is cancelled rather than the
// second one. This is to test that cancelled the compression group 0 or
// partition 0 request doesn't cause issues with the compression group 1 or
// partition 1 request.
TEST_F(TrustedSignalsCacheTest,
       BiddingSignalsDifferentParamsCancelFirstBeforeFetchStart) {
  for (const auto& test_case : CreateBidderTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    CreateCache();
    const auto& bidding_params1 = test_case.bidding_params1;
    const auto& bidding_params2 = test_case.bidding_params2;

    // Don't bother to compare handles here - that's covered by another test.
    auto [handle1, partition_id1] =
        RequestTrustedBiddingSignals(bidding_params1);
    auto [handle2, partition_id2] =
        RequestTrustedBiddingSignals(bidding_params2);

    // Cancel the first request immediately, before any fetch is made.
    handle1.reset();

    // In all cases, that should result in a single fetch being made.
    auto fetch1 = trusted_signals_cache_->WaitForBiddingSignalsFetch();

    switch (test_case.request_relation) {
        // Despite these two cases being different internally, they look the
        // same both to the caller and to the created fetches.
      case RequestRelation::kDifferentFetches:
      case RequestRelation::kDifferentCompressionGroups: {
        // Fetch should not be affected by the first bid.
        ValidateFetchParams(fetch1, bidding_params2,
                            /*expected_compression_group_id=*/0, partition_id2);
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client1(handle2, cache_mojo_pipe_);
        client1.WaitForSuccess();

        // Make a second request using `bidding_params1`. It should result in a
        // new request.
        auto [handle3, partition_id3] =
            RequestTrustedBiddingSignals(bidding_params1);
        EXPECT_NE(handle1, handle3);
        auto fetch3 = trusted_signals_cache_->WaitForBiddingSignalsFetch();
        ValidateFetchParams(fetch3, bidding_params1,
                            /*expected_compression_group_id=*/0, partition_id3);
        RespondToFetchWithSuccess(
            fetch3,
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        TestTrustedSignalsCacheClient client3(handle3, cache_mojo_pipe_);
        client3.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentPartitions: {
        EXPECT_EQ(fetch1.trusted_bidding_signals_url,
                  bidding_params2.trusted_bidding_signals_url);
        ASSERT_EQ(fetch1.compression_groups.size(), 1u);
        EXPECT_EQ(fetch1.compression_groups.begin()->first, 0);

        const auto& partitions = fetch1.compression_groups.begin()->second;
        ASSERT_EQ(partitions.size(), 2u);
        ValidateFetchParamsForPartition(partitions[0], bidding_params1,
                                        partition_id1);
        ValidateFetchParamsForPartition(partitions[1], bidding_params2,
                                        partition_id2);

        // Respond with a single response for the partition, and read it.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client1(handle2, cache_mojo_pipe_);
        client1.WaitForSuccess();

        // Make a second request using `bidding_params1`. It should reuse the
        // response to the initial request.
        auto [handle3, partition_id3] =
            RequestTrustedBiddingSignals(bidding_params1);
        EXPECT_EQ(handle2, handle3);
        EXPECT_EQ(partition_id1, partition_id3);
        EXPECT_NE(partition_id2, partition_id3);
        TestTrustedSignalsCacheClient client3(handle3, cache_mojo_pipe_);
        client3.WaitForSuccess();
        break;
      }

      case RequestRelation::kSamePartitionModified:
      case RequestRelation::kSamePartitionUnmodified: {
        auto merged_bidding_params =
            CreateMergedBiddingParams(bidding_params1, bidding_params2);
        ValidateFetchParams(fetch1, merged_bidding_params,
                            /*expected_compression_group_id=*/0, partition_id1);

        // Respond with a single response for the partition, and read it.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client1(handle2, cache_mojo_pipe_);
        client1.WaitForSuccess();

        // Make a second request using `bidding_params1`. It should reuse the
        // response to the initial request, including the same partition ID.
        auto [handle3, partition_id3] =
            RequestTrustedBiddingSignals(bidding_params1);
        EXPECT_EQ(handle2, handle3);
        EXPECT_EQ(partition_id2, partition_id3);

        // For the sake of completeness, read the response again.
        TestTrustedSignalsCacheClient client3(handle3, cache_mojo_pipe_);
        client3.WaitForSuccess();
        break;
      }
    }
  }
}

// Tests the case where request is made, and then a second request with one
// different parameter is issued before any fetch is started. After the fetch
// starts the second request is cancelled. Once the fetch from the first request
// completes, the second request is made again. The possible behaviors are:
//
// * kDifferentFetches: Two fetches made, one cancelled, and then a new fetch is
// created.
//
// * kDifferentCompressionGroups: A single fetch is made to handle both
// requests. Cancelling the second request throws away its compression group
// when the fetch response is received. A new fetch is created when the second
// request is issued again. Could do better here, but unclear if it's worth the
// investment.
//
// * kDifferentPartitions: Only one fetch is made, as the lifetime of a
// partition is scoped to the lifetime of the compression group.
//
// * kSamePartitionModified / kSamePartitionUnmodified: Only one fetch is made.
TEST_F(TrustedSignalsCacheTest,
       BiddingSignalsDifferentParamsCancelSecondAfterFetchStart) {
  for (const auto& test_case : CreateBidderTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    CreateCache();
    const auto& bidding_params1 = test_case.bidding_params1;
    const auto& bidding_params2 = test_case.bidding_params2;

    auto [handle1, partition_id1] =
        RequestTrustedBiddingSignals(bidding_params1);
    auto [handle2, partition_id2] =
        RequestTrustedBiddingSignals(bidding_params2);

    switch (test_case.request_relation) {
      case RequestRelation::kDifferentFetches: {
        ASSERT_NE(handle1, handle2);
        ASSERT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());
        auto fetches = trusted_signals_cache_->WaitForBiddingSignalsFetches(2);

        // fetches are made in FIFO order.
        ValidateFetchParams(fetches[0], bidding_params1,
                            /*expected_compression_group_id=*/0, partition_id1);
        ValidateFetchParams(fetches[1], bidding_params2,
                            /*expected_compression_group_id=*/0, partition_id2);

        // Cancel the second request. Its fetcher should be destroyed.
        handle2.reset();
        EXPECT_FALSE(fetches[1].fetcher_alive);

        // Reissue second request, which should start a new fetch.
        auto [handle3, partition_id3] =
            RequestTrustedBiddingSignals(bidding_params2);
        auto fetch3 = trusted_signals_cache_->WaitForBiddingSignalsFetch();

        // Make both requests succeed with different bodies, and check that they
        // can be read.
        RespondToFetchWithSuccess(fetches[0]);
        RespondToFetchWithSuccess(
            fetch3,
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        TestTrustedSignalsCacheClient client1(handle1, cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client3(handle3, cache_mojo_pipe_);
        client1.WaitForSuccess();
        client3.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentCompressionGroups: {
        EXPECT_NE(handle1, handle2);
        EXPECT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());
        auto fetch1 = trusted_signals_cache_->WaitForBiddingSignalsFetch();

        EXPECT_EQ(fetch1.trusted_bidding_signals_url,
                  bidding_params1.trusted_bidding_signals_url);
        ASSERT_EQ(fetch1.compression_groups.size(), 2u);

        // Compression groups are appended in FIFO order.
        ASSERT_EQ(1u, fetch1.compression_groups.count(0));
        ValidateFetchParamsForPartitions(fetch1.compression_groups.at(0),
                                         bidding_params1, partition_id1);
        ASSERT_EQ(1u, fetch1.compression_groups.count(1));
        ValidateFetchParamsForPartitions(fetch1.compression_groups.at(1),
                                         bidding_params2, partition_id2);

        // Cancel the second request. The shared fetcher should not be
        // destroyed.
        base::UnguessableToken compression_group_token2 =
            handle2->compression_group_token();
        handle2.reset();
        EXPECT_TRUE(fetch1.fetcher_alive);

        // Reissue second request, which should start a new fetch.
        auto [handle3, partition_id3] =
            RequestTrustedBiddingSignals(bidding_params2);
        EXPECT_NE(handle3->compression_group_token(),
                  handle1->compression_group_token());
        EXPECT_NE(handle3->compression_group_token(), compression_group_token2);
        auto fetch3 = trusted_signals_cache_->WaitForBiddingSignalsFetch();

        // Respond to requests with 3 different results. `fetch[0]` gets
        // responses of `kSuccessBody` and `kOtherSuccessBody` for its two
        // compression groups, and `fetch3` gets a response of
        // `kSomeOtherSuccessBody` for its single group. Using `handle1` should
        // provide a body of `kSuccessBody`, and `handle3` should provide a
        // response of `kSomeOtherSuccessBody`. The other success body should be
        // thrown out.
        RespondToTwoCompressionGroupFetchWithSuccess(fetch1);
        RespondToFetchWithSuccess(
            fetch3,
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
            kSomeOtherSuccessBody);
        TestTrustedSignalsCacheClient client1(handle1, cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client2(compression_group_token2,
                                              cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client3(handle3, cache_mojo_pipe_);
        client1.WaitForSuccess();
        client2.WaitForError(kRequestCancelledError);
        client3.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
            kSomeOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentPartitions: {
        EXPECT_EQ(handle1, handle2);
        EXPECT_NE(partition_id1, partition_id2);
        auto fetch1 = trusted_signals_cache_->WaitForBiddingSignalsFetch();

        EXPECT_EQ(fetch1.trusted_bidding_signals_url,
                  bidding_params1.trusted_bidding_signals_url);
        ASSERT_EQ(fetch1.compression_groups.size(), 1u);
        EXPECT_EQ(fetch1.compression_groups.begin()->first, 0);

        const auto& partitions = fetch1.compression_groups.begin()->second;
        ASSERT_EQ(partitions.size(), 2u);
        ValidateFetchParamsForPartition(partitions[0], bidding_params1,
                                        partition_id1);
        ValidateFetchParamsForPartition(partitions[0], bidding_params1,
                                        partition_id1);

        // Cancel the second request. The shared fetcher should not be
        // destroyed.
        handle2.reset();
        EXPECT_TRUE(fetch1.fetcher_alive);

        // Reissue second request, which should result in the same signals
        // request ID as the other requests, and the same partition ID as the
        // second request.
        auto [handle3, partition_id3] =
            RequestTrustedBiddingSignals(bidding_params2);
        EXPECT_EQ(handle1, handle3);
        EXPECT_EQ(partition_id2, partition_id3);

        // Respond with a single response for the partition, and read it - no
        // need for multiple clients, since the handles are the same.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client(handle1, cache_mojo_pipe_);
        client.WaitForSuccess();
        break;
      }

      case RequestRelation::kSamePartitionModified:
      case RequestRelation::kSamePartitionUnmodified: {
        EXPECT_EQ(handle1, handle2);
        EXPECT_EQ(partition_id1, partition_id2);
        auto fetch1 = trusted_signals_cache_->WaitForBiddingSignalsFetch();

        auto merged_bidding_params =
            CreateMergedBiddingParams(bidding_params1, bidding_params2);
        ValidateFetchParams(fetch1, merged_bidding_params,
                            /*expected_compression_group_id=*/0, partition_id1);

        // Cancel the second request. The shared fetcher should not be
        // destroyed.
        handle2.reset();
        EXPECT_TRUE(fetch1.fetcher_alive);

        // Reissue second request, which should result in the same signals
        // request ID and partition ID as the other requests.
        auto [handle3, partition_id3] =
            RequestTrustedBiddingSignals(bidding_params2);
        EXPECT_EQ(handle1, handle3);
        EXPECT_EQ(partition_id1, partition_id3);

        // Respond with a single request for the partition, and read it - no
        // need for multiple clients, since the handles are the same.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client(handle1, cache_mojo_pipe_);
        client.WaitForSuccess();
        break;
      }
    }
  }
}

// Tests the case where two requests are made and both are cancelled before the
// requests starts. No fetches should be made, regardless of whether the two
// requests would normally share a fetch or not.
TEST_F(TrustedSignalsCacheTest,
       BiddingSignalsDifferentParamsCancelBothBeforeFetchStart) {
  for (const auto& test_case : CreateBidderTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    CreateCache();
    const auto& bidding_params1 = test_case.bidding_params1;
    const auto& bidding_params2 = test_case.bidding_params2;

    auto [handle1, partition_id1] =
        RequestTrustedBiddingSignals(bidding_params1);
    auto [handle2, partition_id2] =
        RequestTrustedBiddingSignals(bidding_params2);

    handle1.reset();
    handle2.reset();

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(trusted_signals_cache_->num_pending_fetches(), 0u);
  }
}

// Tests the case where two requests are made and both are cancelled after the
// fetch(es) start. The fetch(es) should be cancelled.
TEST_F(TrustedSignalsCacheTest,
       BiddingSignalsDifferentParamsCancelBothAfterFetchStart) {
  for (const auto& test_case : CreateBidderTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    CreateCache();
    const auto& bidding_params1 = test_case.bidding_params1;
    const auto& bidding_params2 = test_case.bidding_params2;

    auto [handle1, partition_id1] =
        RequestTrustedBiddingSignals(bidding_params1);
    auto [handle2, partition_id2] =
        RequestTrustedBiddingSignals(bidding_params2);

    switch (test_case.request_relation) {
      case RequestRelation::kDifferentFetches: {
        auto fetches = trusted_signals_cache_->WaitForBiddingSignalsFetches(2);
        handle1.reset();
        handle2.reset();
        EXPECT_FALSE(fetches[0].fetcher_alive);
        EXPECT_FALSE(fetches[1].fetcher_alive);
        break;
      }

      case RequestRelation::kDifferentCompressionGroups:
      case RequestRelation::kDifferentPartitions:
      case RequestRelation::kSamePartitionModified:
      case RequestRelation::kSamePartitionUnmodified: {
        // Don't bother to distinguish these cases - other tests cover the
        // relations between handles and partitions IDs in this case.
        auto fetch = trusted_signals_cache_->WaitForBiddingSignalsFetch();
        handle1.reset();
        handle2.reset();
        EXPECT_FALSE(fetch.fetcher_alive);
        break;
      }
    }
  }
}

// Tests the case of merging multiple requests with the same FetchKey. This test
// serves to make sure that when there are multiple outstanding fetches, the
// last fetch can be modified as long as it has not started.
TEST_F(TrustedSignalsCacheTest, BiddingSignalsMultipleRequestsSameCacheKey) {
  // Start request and wait for its fetch.
  auto bidding_params1 = CreateDefaultBiddingParams();
  auto [handle1, partition_id1] = RequestTrustedBiddingSignals(bidding_params1);
  auto fetch1 = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(fetch1, bidding_params1,
                      /*expected_compression_group_id=*/0, partition_id1);

  // Start another request with same CacheKey as the first, but that can't be
  // merged into the first request, since it has a live fetch.
  auto bidding_params2 = CreateDefaultBiddingParams();
  bidding_params2.trusted_bidding_signals_keys = {{"othey_key2"}};
  auto [handle2, partition_id2] = RequestTrustedBiddingSignals(bidding_params2);
  EXPECT_NE(handle1, handle2);

  // Create another fetch with the default set of parameters. It's merged into
  // the second request, not the first. This is because the first and second
  // request have the same cache key, so the second request overwrite the cache
  // key of the first, though its compression group ID should still be valid.
  auto [handle3, partition_id3] = RequestTrustedBiddingSignals(bidding_params1);
  EXPECT_EQ(handle2, handle3);
  EXPECT_EQ(partition_id2, partition_id3);

  // Wait for the combined fetch.
  auto fetch2 = trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(
      fetch2, CreateMergedBiddingParams(bidding_params2, bidding_params1),
      /*expected_compression_group_id=*/0, partition_id2);

  // Reissuing a request with either previous set of bidding params should reuse
  // the partition shared by the second and third fetches.
  auto [handle4, partition_id4] = RequestTrustedBiddingSignals(bidding_params1);
  EXPECT_EQ(handle2, handle4);
  EXPECT_EQ(partition_id2, partition_id4);
  auto [handle5, partition_id5] = RequestTrustedBiddingSignals(bidding_params2);
  EXPECT_EQ(handle2, handle5);
  EXPECT_EQ(partition_id2, partition_id5);

  // Complete second fetch before first, just to make sure there's no
  // expectation about completion order here.
  RespondToFetchWithSuccess(fetch2);
  TestTrustedSignalsCacheClient client2(handle2, cache_mojo_pipe_);
  client2.WaitForSuccess();

  RespondToFetchWithSuccess(
      fetch1, auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
      kSomeOtherSuccessBody);
  TestTrustedSignalsCacheClient client1(handle1, cache_mojo_pipe_);
  client1.WaitForSuccess(
      auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
      kSomeOtherSuccessBody);
}

}  // namespace
}  // namespace content
