// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/trusted_signals_cache_impl.h"

#include <stdint.h>

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"
#include "content/browser/interest_group/data_decoder_manager.h"
#include "content/browser/interest_group/trusted_signals_fetcher.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

// Some tests need a small, non-zero time, so tests can simulate time passing
// until just before something should happen, and then skip forward to exactly
// when it should happen, to make sure the passage of time is handled correctly.
// Time is mocked out in these tests, so a small value will never cause flake.
const base::TimeDelta kTinyTime = base::Milliseconds(1);

// Generic success/error strings used in most tests.
const char kSuccessBody[] = "Successful result";
const char kOtherSuccessBody[] = "Other successful result";
const char kSomeOtherSuccessBody[] = "Some other successful result";
const char kErrorMessage[] = "Error message";

// The error message received when a compression group is requested over the
// Mojo interface, but no matching CompressionGroupData is found.
const char kRequestCancelledError[] = "Request cancelled";

// Creates a consistent fixed-size URL from a given integer from 0 to 999,
// inclusive.
GURL CreateUrl(std::uint32_t i) {
  CHECK_LE(i, 999u);
  // Always return a fixed size string. While the size doesn't currently matter,
  // the URL length may be included in size computations in the future, at which
  // point a constant size string may be useful in making the computed size
  // consistent.
  return GURL(base::StringPrintf("https://%03u.test", i));
}

// Creates a string of size `length`. Each value of `i` creates a distinct but
// consistent string. `i` may be in the range 0 to 999.
std::string CreateString(std::uint32_t i, size_t length = 3) {
  CHECK_LE(i, 999u);
  std::string out = base::StringPrintf("%03u", i);
  out.append(length - 3, 'a');
  CHECK_EQ(out.size(), length);
  return out;
}

// Struct with input parameters for RequestTrustedBiddingSignals(). Having a
// struct allows for more easily checking changing a single parameter, and
// validating all parameters passed to the TrustedSignalsFetcher, without
// duplicating a lot of code.
struct BiddingParams {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  FrameTreeNodeId frame_tree_node_id;
  // Actual requests may only have a single devtools ID, but this struct is also
  // be used to represent the result of multiple merged requests.
  std::set<std::string> devtools_auction_ids{"devtools_auction_id1"};
  url::Origin main_frame_origin;
  network::mojom::IPAddressSpace ip_address_space =
      network::mojom::IPAddressSpace::kPublic;
  // The bidder / interest group owner.
  url::Origin script_origin;

  // Actual requests may only have a single interest group, so only one name.
  // This is a set because this struct is also used to validate fetch
  // parameters, which may include a set of interest groups in the
  // group-by-origin case.
  std::set<std::string> interest_group_names;

  blink::mojom::InterestGroup_ExecutionMode execution_mode =
      blink::mojom::InterestGroup_ExecutionMode::kCompatibilityMode;
  url::Origin joining_origin;
  GURL trusted_signals_url;
  url::Origin coordinator;
  std::optional<std::vector<std::string>> trusted_bidding_signals_keys;
  base::Value::Dict additional_params;
  std::optional<std::string> buyer_tkv_signals;
};

// Struct with input parameters for RequestTrustedScoringSignals().
struct ScoringParams {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  FrameTreeNodeId frame_tree_node_id;
  // While ScoringParams are never merged, so this will always only have one ID,
  // use a set here to mirror BiddingParams.
  std::set<std::string> devtools_auction_ids{"devtools_auction_id1"};
  url::Origin main_frame_origin;
  network::mojom::IPAddressSpace ip_address_space =
      network::mojom::IPAddressSpace::kPublic;
  // The seller.
  url::Origin script_origin;
  GURL trusted_signals_url;
  url::Origin coordinator;
  url::Origin interest_group_owner;
  url::Origin joining_origin;
  GURL render_url;
  std::vector<GURL> component_render_urls;
  base::Value::Dict additional_params;
  std::optional<std::string> seller_tkv_signals;
};

// Just like TrustedSignalsFetcher::BiddingPartition, but owns its arguments.
struct FetcherBiddingPartitionArgs {
  int partition_id;
  std::set<std::string> interest_group_names;
  std::set<std::string> keys;
  base::Value::Dict additional_params;
  std::optional<std::string> buyer_tkv_signals;
};

// Just like TrustedSignalsFetcher::ScoringPartition, but owns its arguments.
struct FetcherScoringPartitionArgs {
  int partition_id;
  GURL render_url;
  std::set<GURL> component_render_urls;
  base::Value::Dict additional_params;
  std::optional<std::string> seller_tkv_signals;
};

// Creates a BiddingAndAuctionServerKey that embeds the signals and coordinator
// origins in it, so the mock fetcher can calidate that its parameters match
// those used to get the BiddingAndAuctionServerKey.
BiddingAndAuctionServerKey CreateServerKey(const url::Origin& signals_origin,
                                           const url::Origin& coordinator) {
  return BiddingAndAuctionServerKey{
      /*key=*/base::StrCat(
          {signals_origin.Serialize(), " ", coordinator.Serialize()}),
      /*id=*/"01"};
}

// Subclass of TrustedSignalsCacheImpl that mocks out TrustedSignalsFetcher
// calls, and lets tests monitor and respond to those fetches.
class TestTrustedSignalsCache : public TrustedSignalsCacheImpl {
 public:
  static constexpr std::string_view kKeyFetchFailed = "Key fetch failed";

  // Controls how coordinator key requests are handled.
  enum class GetCoordinatorKeyMode {
    kSync,
    kAsync,
    kSyncFail,
    kAsyncFail,
    // Grabs the callback and lets the consumer wait for it and invoke it
    // directly via WaitForCoordinatorKeyCallback(). Checks that all received
    // callbacks have been consumed on destruction.
    kStashCallback
  };

  class TestTrustedSignalsFetcher : public TrustedSignalsFetcher {
   public:
    struct PendingBiddingSignalsFetch {
      GURL trusted_signals_url;
      BiddingAndAuctionServerKey bidding_and_auction_key;
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
      FrameTreeNodeId frame_tree_node_id;
      base::flat_set<std::string> devtools_auction_ids;
      url::Origin main_frame_origin;
      network::mojom::IPAddressSpace ip_address_space;
      base::UnguessableToken network_partition_nonce;
      url::Origin script_origin;
      std::map<int, std::vector<FetcherBiddingPartitionArgs>>
          compression_groups;
      TrustedSignalsFetcher::Callback callback;

      // Weak pointer to Fetcher to allow checking if the Fetcher has been
      // destroyed.
      base::WeakPtr<TestTrustedSignalsFetcher> fetcher_alive;
    };

    struct PendingScoringSignalsFetch {
      GURL trusted_signals_url;
      BiddingAndAuctionServerKey bidding_and_auction_key;
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
      FrameTreeNodeId frame_tree_node_id;
      base::flat_set<std::string> devtools_auction_ids;
      url::Origin main_frame_origin;
      network::mojom::IPAddressSpace ip_address_space;
      base::UnguessableToken network_partition_nonce;
      url::Origin script_origin;
      std::map<int, std::vector<FetcherScoringPartitionArgs>>
          compression_groups;
      TrustedSignalsFetcher::Callback callback;

      // Weak pointer to Fetcher to allow checking if the Fetcher has been
      // destroyed.
      base::WeakPtr<TestTrustedSignalsFetcher> fetcher_alive;
    };

    explicit TestTrustedSignalsFetcher(TestTrustedSignalsCache* cache)
        : cache_(cache) {}

    ~TestTrustedSignalsFetcher() override = default;

   private:
    void FetchBiddingSignals(
        DataDecoderManager& data_decoder_manager,
        network::mojom::URLLoaderFactory* url_loader_factory,
        FrameTreeNodeId frame_tree_node_id,
        base::flat_set<std::string> devtools_auction_ids,
        const url::Origin& main_frame_origin,
        network::mojom::IPAddressSpace ip_address_space,
        base::UnguessableToken network_partition_nonce,
        const url::Origin& script_origin,
        const GURL& trusted_signals_url,
        const BiddingAndAuctionServerKey& bidding_and_auction_key,
        const std::map<int, std::vector<BiddingPartition>>& compression_groups,
        Callback callback) override {
      // This class is single use. Make sure a Fetcher isn't used more than
      // once.
      EXPECT_FALSE(fetch_started_);
      fetch_started_ = true;

      std::map<int, std::vector<FetcherBiddingPartitionArgs>>
          compression_groups_copy;
      for (const auto& compression_group : compression_groups) {
        auto& bidding_partitions_copy =
            compression_groups_copy.try_emplace(compression_group.first)
                .first->second;
        for (const auto& bidding_partition : compression_group.second) {
          bidding_partitions_copy.emplace_back(
              bidding_partition.partition_id,
              *bidding_partition.interest_group_names, *bidding_partition.keys,
              bidding_partition.additional_params->Clone(),
              bidding_partition.buyer_tkv_signals
                  ? std::make_optional(*bidding_partition.buyer_tkv_signals)
                  : std::nullopt);
        }
      }

      cache_->OnPendingBiddingSignalsFetch(PendingBiddingSignalsFetch(
          trusted_signals_url, bidding_and_auction_key,
          static_cast<network::SharedURLLoaderFactory*>(url_loader_factory),
          frame_tree_node_id, std::move(devtools_auction_ids),
          main_frame_origin, ip_address_space, network_partition_nonce,
          script_origin, std::move(compression_groups_copy),
          std::move(callback), weak_ptr_factory_.GetWeakPtr()));
    }

    void FetchScoringSignals(
        DataDecoderManager& data_decoder_manager,
        network::mojom::URLLoaderFactory* url_loader_factory,
        FrameTreeNodeId frame_tree_node_id,
        base::flat_set<std::string> devtools_auction_ids,
        const url::Origin& main_frame_origin,
        network::mojom::IPAddressSpace ip_address_space,
        base::UnguessableToken network_partition_nonce,
        const url::Origin& script_origin,
        const GURL& trusted_signals_url,
        const BiddingAndAuctionServerKey& bidding_and_auction_key,
        const std::map<int, std::vector<ScoringPartition>>& compression_groups,
        Callback callback) override {
      // This class is single use. Make sure a Fetcher isn't used more than
      // once.
      EXPECT_FALSE(fetch_started_);
      fetch_started_ = true;

      std::map<int, std::vector<FetcherScoringPartitionArgs>>
          compression_groups_copy;
      for (const auto& compression_group : compression_groups) {
        auto& scoring_partitions_copy =
            compression_groups_copy.try_emplace(compression_group.first)
                .first->second;
        for (const auto& scoring_partition : compression_group.second) {
          scoring_partitions_copy.emplace_back(
              scoring_partition.partition_id, *scoring_partition.render_url,
              *scoring_partition.component_render_urls,
              scoring_partition.additional_params->Clone(),
              scoring_partition.seller_tkv_signals
                  ? std::make_optional(*scoring_partition.seller_tkv_signals)
                  : std::nullopt);
        }
      }

      cache_->OnPendingScoringSignalsFetch(PendingScoringSignalsFetch(
          trusted_signals_url, bidding_and_auction_key,
          reinterpret_cast<network::SharedURLLoaderFactory*>(
              url_loader_factory),
          frame_tree_node_id, std::move(devtools_auction_ids),
          main_frame_origin, ip_address_space, network_partition_nonce,
          script_origin, std::move(compression_groups_copy),
          std::move(callback), weak_ptr_factory_.GetWeakPtr()));
    }

    const raw_ptr<TestTrustedSignalsCache> cache_;
    bool fetch_started_ = false;
    base::WeakPtrFactory<TestTrustedSignalsFetcher> weak_ptr_factory_{this};
  };

  explicit TestTrustedSignalsCache(DataDecoderManager* data_decoder_manager)
      : TrustedSignalsCacheImpl(
            data_decoder_manager,
            // The use of base::Unretained here means that all async calls must
            // be accounted for before a test completes. The base class always
            // invokes this
            // method synchronously, however, and never on destruction.
            base::BindRepeating(&TestTrustedSignalsCache::GetCoordinatorKey,
                                base::Unretained(this))) {}

  ~TestTrustedSignalsCache() override {
    // All pending fetches should have been claimed by calls to
    // WaitForBiddingSignalsFetch[es]().
    EXPECT_TRUE(trusted_bidding_signals_fetches_.empty());
    EXPECT_TRUE(pending_coordinator_key_callbacks_.empty());
  }

  void set_get_coordinator_key_mode(
      GetCoordinatorKeyMode get_coordinator_key_mode) {
    get_coordinator_key_mode_ = get_coordinator_key_mode;
  }

  // Callback to handle coordinator key requests by the base
  // TrustedSignalsCacheImpl class.
  void GetCoordinatorKey(
      const url::Origin& scope_origin,
      const std::optional<url::Origin>& coordinator,
      base::OnceCallback<void(
          base::expected<BiddingAndAuctionServerKey, std::string>)> callback) {
    switch (get_coordinator_key_mode_) {
      case GetCoordinatorKeyMode::kSync:
        std::move(callback).Run(CreateServerKey(scope_origin, *coordinator));
        break;
      case GetCoordinatorKeyMode::kAsync:
        // This should be safe, as the base class guards this callback with a
        // weak pointer.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback),
                           CreateServerKey(scope_origin, *coordinator)));
        break;
      case GetCoordinatorKeyMode::kSyncFail:
        std::move(callback).Run(base::unexpected(std::string(kKeyFetchFailed)));
        break;
      case GetCoordinatorKeyMode::kAsyncFail:
        // This should be safe, as the base class guards this callback with a
        // weak pointer.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback),
                           base::unexpected(std::string(kKeyFetchFailed))));
        break;
      case GetCoordinatorKeyMode::kStashCallback:
        pending_coordinator_key_callbacks_.emplace_back(base::BindOnce(
            std::move(callback), CreateServerKey(scope_origin, *coordinator)));
        if (wait_for_coordinator_key_callback_run_loop_) {
          wait_for_coordinator_key_callback_run_loop_->Quit();
        }
    }
  }

  // Allows invoking a callback asynchronously to be guarded by
  // `weak_ptr_factory_`.
  void InvokeCallback(base::OnceClosure callback) { std::move(callback).Run(); }

  // Waits for the next attempt to retrieve a coordinator key, and returns the
  // passed in callback with the return value bound to the result of
  // CreateServerKey(). The GetCoordinatorKeyMode must be kStashCallback.
  base::OnceClosure WaitForCoordinatorKeyCallback() {
    CHECK_EQ(get_coordinator_key_mode_, GetCoordinatorKeyMode::kStashCallback);
    if (pending_coordinator_key_callbacks_.empty()) {
      wait_for_coordinator_key_callback_run_loop_ =
          std::make_unique<base::RunLoop>();
      wait_for_coordinator_key_callback_run_loop_->Run();
      wait_for_coordinator_key_callback_run_loop_.reset();
    }

    auto out = std::move(pending_coordinator_key_callbacks_.front());
    pending_coordinator_key_callbacks_.pop_front();
    return out;
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

  // Waits until there have been `num_fetches` fetches whose
  // FetchScoringSignals() method has been invoked and returns them all,
  // clearing the list of pending fetches. EXPECTs that number is not exceeded.
  std::vector<TestTrustedSignalsFetcher::PendingScoringSignalsFetch>
  WaitForScoringSignalsFetches(size_t num_fetches) {
    DCHECK(!run_loop_);
    while (trusted_scoring_signals_fetches_.size() < num_fetches) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
    EXPECT_EQ(num_fetches, trusted_scoring_signals_fetches_.size());
    std::vector<TestTrustedSignalsFetcher::PendingScoringSignalsFetch> out;
    std::swap(out, trusted_scoring_signals_fetches_);
    return out;
  }

  // Wrapper around WaitForScoringSignalsFetches() that waits for a single fetch
  // and returns only it. Expects there to be at most one fetch.
  TestTrustedSignalsFetcher::PendingScoringSignalsFetch
  WaitForScoringSignalsFetch() {
    return std::move(WaitForScoringSignalsFetches(1).at(0));
  }

  size_t num_pending_fetches() const {
    return trusted_bidding_signals_fetches_.size() +
           trusted_scoring_signals_fetches_.size();
  }

 private:
  std::unique_ptr<TrustedSignalsFetcher> CreateFetcher() override {
    return std::make_unique<TestTrustedSignalsFetcher>(this);
  }

  void OnPendingBiddingSignalsFetch(
      TestTrustedSignalsFetcher::PendingBiddingSignalsFetch fetch) {
    trusted_bidding_signals_fetches_.emplace_back(std::move(fetch));
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void OnPendingScoringSignalsFetch(
      TestTrustedSignalsFetcher::PendingScoringSignalsFetch fetch) {
    trusted_scoring_signals_fetches_.emplace_back(std::move(fetch));
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  std::list<base::OnceClosure> pending_coordinator_key_callbacks_;
  std::unique_ptr<base::RunLoop> wait_for_coordinator_key_callback_run_loop_;

  std::vector<TestTrustedSignalsFetcher::PendingBiddingSignalsFetch>
      trusted_bidding_signals_fetches_;
  std::vector<TestTrustedSignalsFetcher::PendingScoringSignalsFetch>
      trusted_scoring_signals_fetches_;

  GetCoordinatorKeyMode get_coordinator_key_mode_ =
      GetCoordinatorKeyMode::kSync;
};

// Validates that `partition` has a single partition corresponding to the
// BiddingParams in `params`.
void ValidateFetchParamsForPartition(
    const FetcherBiddingPartitionArgs& partition,
    const BiddingParams& params,
    int expected_partition_id) {
  EXPECT_THAT(partition.interest_group_names,
              testing::ElementsAreArray(params.interest_group_names));
  if (!params.trusted_bidding_signals_keys) {
    EXPECT_THAT(partition.keys, testing::ElementsAre());
  } else {
    EXPECT_THAT(partition.keys, testing::UnorderedElementsAreArray(
                                    *params.trusted_bidding_signals_keys));
  }
  EXPECT_EQ(partition.additional_params, params.additional_params);
  EXPECT_EQ(partition.partition_id, expected_partition_id);
  EXPECT_EQ(partition.buyer_tkv_signals, params.buyer_tkv_signals);
}

// Validates that `partition` has a single partition corresponding to the
// ScoringParams in `params`.
void ValidateFetchParamsForPartition(
    const FetcherScoringPartitionArgs& partition,
    const ScoringParams& params,
    int expected_partition_id) {
  EXPECT_EQ(partition.render_url, params.render_url);
  EXPECT_THAT(partition.component_render_urls,
              testing::ElementsAreArray(params.component_render_urls));
  EXPECT_EQ(partition.additional_params, params.additional_params);
  EXPECT_EQ(partition.partition_id, expected_partition_id);
  EXPECT_EQ(partition.seller_tkv_signals, params.seller_tkv_signals);
}

// Validates that `partitions` has a single partition corresponding to the
// params in `params`. `ParamType` is expected to be BiddingParams or
// ScoringParams, and `FetcherPartitionType` one of
// TrustedSignalsFetcher::BiddingPartition or
// TrustedSignalsFetcher::ScoringPartition.
template <typename ParamType, typename FetcherPartitionType>
void ValidateFetchParamsForPartitions(
    const std::vector<FetcherPartitionType>& partitions,
    const ParamType& params,
    int expected_partition_id) {
  ASSERT_EQ(partitions.size(), 1u);
  ValidateFetchParamsForPartition(partitions.at(0), params,
                                  expected_partition_id);
}

// Verifies that all fields of `trusted_signals_fetch` exactly match `params`
// and the provided IDs. Doesn't handle the case that that multiple fetches were
// merged into a single fetch. Note that `compression_group_id` is never exposed
// externally by the TrustedSignalsCache API nor passed in, so relies on
// information about the internal logic of the cache to provide the expected
// value for.
//
// `ParamType` is expected to be BiddingParams or ScoringParams, and
// `FetcherFetchType` one of
// TestTrustedSignalsFetcher::PendingBiddingSignalsFetch or
// TestTrustedSignalsFetcher::PendingScoringSignalsFetch.
template <typename ParamType, typename FetcherFetchType>
void ValidateFetchParams(const FetcherFetchType& fetch,
                         const ParamType& params,
                         int expected_compression_group_id,
                         int expected_partition_id) {
  EXPECT_EQ(fetch.url_loader_factory, params.url_loader_factory);
  EXPECT_EQ(fetch.frame_tree_node_id, params.frame_tree_node_id);
  EXPECT_THAT(fetch.devtools_auction_ids,
              testing::ElementsAreArray(params.devtools_auction_ids));
  EXPECT_EQ(fetch.main_frame_origin, params.main_frame_origin);
  EXPECT_EQ(fetch.ip_address_space, params.ip_address_space);
  EXPECT_EQ(fetch.trusted_signals_url, params.trusted_signals_url);
  EXPECT_EQ(fetch.script_origin, params.script_origin);
  auto expected_key = CreateServerKey(
      url::Origin::Create(params.trusted_signals_url), params.coordinator);
  EXPECT_EQ(fetch.bidding_and_auction_key.key, expected_key.key);
  EXPECT_EQ(fetch.bidding_and_auction_key.id, expected_key.id);
  ASSERT_EQ(fetch.compression_groups.size(), 1u);
  EXPECT_EQ(fetch.compression_groups.begin()->first,
            expected_compression_group_id);

  ValidateFetchParamsForPartitions(fetch.compression_groups.begin()->second,
                                   params, expected_partition_id);
}

TrustedSignalsFetcher::CompressionGroupResult CreateCompressionGroupResult(
    auction_worklet::mojom::TrustedSignalsCompressionScheme compression_scheme =
        auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
    std::string_view body = kSuccessBody,
    base::TimeDelta ttl = base::Days(1)) {
  TrustedSignalsFetcher::CompressionGroupResult result;
  result.compression_group_data =
      base::Value::BlobStorage(body.begin(), body.end());
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
    base::TimeDelta ttl = base::Days(1)) {
  TrustedSignalsFetcher::CompressionGroupResultMap map;
  map[compression_group_id] =
      CreateCompressionGroupResult(compression_scheme, body, ttl);
  return map;
}

// Respond to the next fetch with a generic successful body. Expects only one
// compression group. Uses a template so it can handle both
// PendingBiddingSignalsFetches and PendingScoringSignalsFetches.
template <class PendingSignalsFetch>
void RespondToFetchWithSuccess(
    PendingSignalsFetch& trusted_signals_fetch,
    auction_worklet::mojom::TrustedSignalsCompressionScheme compression_scheme =
        auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
    std::string_view body = kSuccessBody,
    base::TimeDelta ttl = base::Days(1)) {
  // Shouldn't be calling this after the fetcher was destroyed.
  ASSERT_TRUE(trusted_signals_fetch.fetcher_alive);

  // Method only supports a single compression group.
  ASSERT_EQ(trusted_signals_fetch.compression_groups.size(), 1u);

  ASSERT_TRUE(trusted_signals_fetch.callback);
  std::move(trusted_signals_fetch.callback)
      .Run(CreateCompressionGroupResultMap(
          trusted_signals_fetch.compression_groups.begin()->first,
          compression_scheme, body, ttl));
}

// Responds to a two-compression group fetch with two successful responses, with
// different parameters. The first uses a gzip with kSuccessBody, and the second
// uses brotli with kOtherSuccessBody. Uses a template so it can handle both
// PendingBiddingSignalsFetches and PendingScoringSignalsFetches.
template <class PendingSignalsFetch>
void RespondToTwoCompressionGroupFetchWithSuccess(
    PendingSignalsFetch& trusted_signals_fetch,
    base::TimeDelta ttl1 = base::Hours(1),
    base::TimeDelta ttl2 = base::Hours(1)) {
  ASSERT_EQ(trusted_signals_fetch.compression_groups.size(), 2u);
  TrustedSignalsFetcher::CompressionGroupResultMap map;
  map[trusted_signals_fetch.compression_groups.begin()->first] =
      CreateCompressionGroupResult(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
          kSuccessBody, ttl1);
  map[std::next(trusted_signals_fetch.compression_groups.begin())->first] =
      CreateCompressionGroupResult(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
          kOtherSuccessBody, ttl2);
  std::move(trusted_signals_fetch.callback).Run(std::move(map));
}

// Respond to the next fetch with a generic successful body. Does not care about
// number of compression groups, as on error, all groups are failed. Uses a
// template so it can handle both PendingBiddingSignalsFetches and
// PendingScoringSignalsFetches.
template <class PendingSignalsFetch>
void RespondToFetchWithError(PendingSignalsFetch& trusted_signals_fetch) {
  CHECK(trusted_signals_fetch.callback);
  std::move(trusted_signals_fetch.callback)
      .Run(base::unexpected(kErrorMessage));
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
    receiver_.set_disconnect_handler(
        base::BindOnce(&TestTrustedSignalsCacheClient::OnReceiverDisconnected,
                       base::Unretained(this)));
  }

  // Overload used by almost all callers, to simplify the call a bit.
  TestTrustedSignalsCacheClient(
      TestTrustedSignalsCache::Handle* handle,
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

  bool IsReceiverDisconnected() {
    receiver_.FlushForTesting();
    return received_disconnected_;
  }

 private:
  // Waits until OnSuccess() or OnError() has been called, and returns true on
  // success.
  bool WaitForResult() {
    run_loop_.Run();
    return compression_group_data_.has_value();
  }

  void OnReceiverDisconnected() { received_disconnected_ = true; }

  base::RunLoop run_loop_;

  std::optional<auction_worklet::mojom::TrustedSignalsCompressionScheme>
      compression_scheme_;
  // Use a string instead of a vector<uint8_t> for more useful error messages on
  // failure comparisons.
  std::optional<std::string> compression_group_data_;

  std::optional<std::string> error_message_;

  // True if the receiver has been disconnected. Unlike Remotes, Receivers don't
  // have an is_connected() method, so have to set a disconnect handler to get
  // this information.
  bool received_disconnected_ = false;

  mojo::Receiver<auction_worklet::mojom::TrustedSignalsCacheClient> receiver_;
};

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

template <typename ParamsType>
class TrustedSignalsCacheTest : public testing::Test {
 public:
  // Test case class shared by a number of tests. Each test makes a request
  // using `params1` before `params2`.
  struct TestCase {
    // Used for documentation + useful output on errors.
    const char* description;
    RequestRelation request_relation = RequestRelation::kDifferentFetches;
    // This is primarily relevant for kDifferentFetches. If there's a single
    // fetch, there's necessarily a single nonce. Default to true since in some
    // tests with other RequestRelations, multiple network fetches end up being
    // made, and they should use the same nonce.
    bool expect_same_network_partition_nonce = true;
    ParamsType params1;
    ParamsType params2;
  };

  TrustedSignalsCacheTest() { CreateCache(); }

  ~TrustedSignalsCacheTest() override = default;

  void CreateCache() {
    trusted_signals_cache_ =
        std::make_unique<TestTrustedSignalsCache>(&data_decoder_manager_);
    cache_mojo_pipe_.reset();
    other_cache_mojo_pipe_.reset();
    // This is a little awkward, but works for both bidders and sellers.
    cache_mojo_pipe_.Bind(
        CreateMojoPendingRemoteForOrigin(CreateDefaultParams().script_origin));
  }

  // Creates a pending scoring or bidding TrustedSignalsCache pipe for the given
  // origin, using the SignalsType corresponding to the current test type.
  mojo::PendingRemote<auction_worklet::mojom::TrustedSignalsCache>
  CreateMojoPendingRemoteForOrigin(const url::Origin& script_origin) {
    if constexpr (std::is_same<ParamsType, BiddingParams>::value) {
      return trusted_signals_cache_->CreateRemote(
          TrustedSignalsCacheImpl::SignalsType::kBidding, script_origin);
    } else {
      return trusted_signals_cache_->CreateRemote(
          TrustedSignalsCacheImpl::SignalsType::kScoring, script_origin);
    }
  }

  // If `script_origin` matches the default origin, returns `cache_mojo_pipe_`.
  // Otherwise, creates a new pipe for `script_origin`. Unconditionally destroys
  // any previous pipe created by this method. Primarily used in the case of a
  // second set of parameters with RequestRelation::kDifferentFetches, which
  // sometimes don't share an origin.
  mojo::Remote<auction_worklet::mojom::TrustedSignalsCache>&
  CreateOrGetMojoPipeGivenParams(const ParamsType& params) {
    other_cache_mojo_pipe_.reset();
    const url::Origin& origin = params.script_origin;
    if (origin == CreateDefaultParams().script_origin) {
      return cache_mojo_pipe_;
    }
    other_cache_mojo_pipe_.Bind(CreateMojoPendingRemoteForOrigin(origin));
    return other_cache_mojo_pipe_;
  }

  // Waits for the next `num_fetches`
  // TestTrustedSignalsFetcher::PendingBiddingSignalsFetches or
  // TestTrustedSignalsFetcher::PendingScoringSignalsFetches, depending on
  // ParamsType.
  auto WaitForSignalsFetches(int num_fetches) {
    if constexpr (std::is_same<ParamsType, BiddingParams>::value) {
      return trusted_signals_cache_->WaitForBiddingSignalsFetches(num_fetches);
    }
    if constexpr (std::is_same<ParamsType, ScoringParams>::value) {
      return trusted_signals_cache_->WaitForScoringSignalsFetches(num_fetches);
    }
  }

  // Waits for the next TestTrustedSignalsFetcher::PendingBiddingSignalsFetch or
  // TestTrustedSignalsFetcher::PendingScoringSignalsFetch, depending on
  // ParamsType.
  auto WaitForSignalsFetch() {
    if constexpr (std::is_same<ParamsType, BiddingParams>::value) {
      return trusted_signals_cache_->WaitForBiddingSignalsFetch();
    }
    if constexpr (std::is_same<ParamsType, ScoringParams>::value) {
      return trusted_signals_cache_->WaitForScoringSignalsFetch();
    }
  }

  // Separate methods to create default bidding and scoring params, for the few
  // tests that need both.

  BiddingParams CreateDefaultBiddingParams() const {
    BiddingParams out;
    out.url_loader_factory = url_loader_factory_;
    out.frame_tree_node_id = kFrameTreeNodeId;
    out.main_frame_origin = kMainFrameOrigin;
    out.ip_address_space = network::mojom::IPAddressSpace::kPublic;
    out.script_origin = kBidder;
    out.interest_group_names = {kInterestGroupName};
    out.execution_mode =
        blink::mojom::InterestGroup_ExecutionMode::kCompatibilityMode;
    out.joining_origin = kJoiningOrigin;
    out.trusted_signals_url = kTrustedBiddingSignalsUrl;
    out.coordinator = kCoordinator;
    out.trusted_bidding_signals_keys = {{"key1", "key2"}};
    return out;
  }

  ScoringParams CreateDefaultScoringParams() const {
    ScoringParams out;
    out.url_loader_factory = url_loader_factory_;
    out.frame_tree_node_id = kFrameTreeNodeId;
    out.main_frame_origin = kMainFrameOrigin;
    out.ip_address_space = network::mojom::IPAddressSpace::kPublic;
    out.script_origin = kSeller;
    out.trusted_signals_url = kTrustedScoringSignalsUrl;
    out.coordinator = kCoordinator;
    out.interest_group_owner = kBidder;
    out.joining_origin = kJoiningOrigin;
    out.render_url = kRenderUrl;
    out.component_render_urls = kComponentRenderUrls;
    return out;
  }

  // Creates the default parameters of ParamsType.
  ParamsType CreateDefaultParams() const {
    if constexpr (std::is_same<ParamsType, BiddingParams>::value) {
      return CreateDefaultBiddingParams();
    }
    if constexpr (std::is_same<ParamsType, ScoringParams>::value) {
      return CreateDefaultScoringParams();
    }
  }

  TestCase CreateDefaultTestCase() {
    TestCase out;
    out.params1 = CreateDefaultParams();
    out.params2 = CreateDefaultParams();
    out.params2.devtools_auction_ids = {"devtools_auction_id2"};

    return out;
  }

  // Returns a shared set of test cases used by a number of different tests.
  std::vector<TestCase> CreateTestCases() {
    std::vector<TestCase> out;

    if constexpr (std::is_same<ParamsType, BiddingParams>::value) {
      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Different main frame origins";
      out.back().request_relation = RequestRelation::kDifferentFetches;
      out.back().params2.main_frame_origin =
          url::Origin::Create(GURL("https://other.origin.test/"));

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Different bidders";
      out.back().request_relation = RequestRelation::kDifferentFetches;
      out.back().expect_same_network_partition_nonce = false;
      out.back().params2.script_origin =
          url::Origin::Create(GURL("https://other.bidder.test/"));

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Different interest group names";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params2.interest_group_names = {"other interest group"};

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Different joining origins";
      out.back().request_relation =
          RequestRelation::kDifferentCompressionGroups;
      out.back().params2.joining_origin =
          url::Origin::Create(GURL("https://other.joining.origin.test"));

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Different trusted bidding signals URLs";
      out.back().request_relation = RequestRelation::kDifferentFetches;
      out.back().expect_same_network_partition_nonce = false;
      out.back().params2.trusted_signals_url =
          GURL("https://other.bidder.test/signals");

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "First request has no keys";
      out.back().request_relation = RequestRelation::kSamePartitionModified;
      out.back().params1.trusted_bidding_signals_keys.reset();

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Second request has no keys";
      out.back().request_relation = RequestRelation::kSamePartitionUnmodified;
      out.back().params2.trusted_bidding_signals_keys.reset();

      out.emplace_back(CreateDefaultTestCase());
      out.back().description =
          "First request's keys are a subset of the second request's";
      out.back().request_relation = RequestRelation::kSamePartitionModified;
      out.back().params2.trusted_bidding_signals_keys->emplace_back(
          "other key");

      out.emplace_back(CreateDefaultTestCase());
      out.back().description =
          "Second request's keys are a subset of the first request's";
      out.back().request_relation = RequestRelation::kSamePartitionUnmodified;
      out.back().params2.trusted_bidding_signals_keys->erase(
          out.back().params2.trusted_bidding_signals_keys->begin());

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Requests have complete distinct keys";
      out.back().request_relation = RequestRelation::kSamePartitionModified;
      out.back().params2.trusted_bidding_signals_keys = {{"other key"}};

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Requests have different `additional_params`";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params2.additional_params.Set("additional", "param");

      out.emplace_back(CreateDefaultTestCase());
      out.back().description =
          "Requests have one null and one valid buyer_tkv_signals";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params2.buyer_tkv_signals = "signals";

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Requests have two different buyer_tkv_signals";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params1.buyer_tkv_signals = "signals1";
      out.back().params2.buyer_tkv_signals = "signals2";

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Requests have same buyer_tkv_signals";
      out.back().request_relation = RequestRelation::kSamePartitionUnmodified;
      out.back().params1.buyer_tkv_signals = "signals";
      out.back().params2.buyer_tkv_signals = "signals";

      // Group-by-origin tests.

      // Same interest group name is unlikely when other fields don't match, but
      // best to test it.
      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Group-by-origin: First request group-by-origin";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params1.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;

      // Same interest group name is unlikely when other fields don't match, but
      // best to test it.
      out.emplace_back(CreateDefaultTestCase());
      out.back().description =
          "Group-by-origin: Second request group-by-origin";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params2.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;

      out.emplace_back(CreateDefaultTestCase());
      out.back().description =
          "Group-by-origin: Different interest group names";
      out.back().request_relation = RequestRelation::kSamePartitionModified;
      out.back().params1.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.interest_group_names = {"other interest group"};

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Group-by-origin: Different keys.";
      out.back().request_relation = RequestRelation::kSamePartitionModified;
      out.back().params1.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.trusted_bidding_signals_keys = {{"other key"}};

      out.emplace_back(CreateDefaultTestCase());
      out.back().description =
          "Group-by-origin: Different keys and interest group names.";
      out.back().request_relation = RequestRelation::kSamePartitionModified;
      out.back().params1.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.interest_group_names = {"other interest group"};
      out.back().params2.trusted_bidding_signals_keys = {{"other key"}};

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Group-by-origin: Different main frame origins";
      out.back().request_relation = RequestRelation::kDifferentFetches;
      out.back().params1.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.main_frame_origin =
          url::Origin::Create(GURL("https://other.origin.test/"));

      // It would be unusual to have the same IG with different joining origins,
      // since one would overwrite the other, but if it does happen, the
      // requests should use different compression groups.
      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Group-by-origin: Different joining origin.";
      out.back().request_relation =
          RequestRelation::kDifferentCompressionGroups;
      out.back().params1.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.joining_origin =
          url::Origin::Create(GURL("https://other.joining.origin.test"));

      // Like above test, but the more common case of different IGs with
      // different joining origins.
      out.emplace_back(CreateDefaultTestCase());
      out.back().description =
          "Group-by-origin: Different joining origin, different IGs.";
      out.back().request_relation =
          RequestRelation::kDifferentCompressionGroups;
      out.back().params1.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.interest_group_names = {"group2"};
      out.back().params2.joining_origin =
          url::Origin::Create(GURL("https://other.joining.origin.test"));

      out.emplace_back(CreateDefaultTestCase());
      out.back().description =
          "Group-by-origin: Requests have one null and one valid "
          "buyer_tkv_signals";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params1.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.buyer_tkv_signals = "signals";

      out.emplace_back(CreateDefaultTestCase());
      out.back().description =
          "Group-by-origin: Requests have two different buyer_tkv_signals";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params1.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params1.buyer_tkv_signals = "signals1";
      out.back().params2.buyer_tkv_signals = "signals2";

      out.emplace_back(CreateDefaultTestCase());
      out.back().description =
          "Group-by-origin: Requests have same buyer_tkv_signals";
      out.back().request_relation = RequestRelation::kSamePartitionUnmodified;
      out.back().params1.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params2.execution_mode =
          blink::mojom::InterestGroup_ExecutionMode::kGroupedByOriginMode;
      out.back().params1.buyer_tkv_signals = "signals";
      out.back().params2.buyer_tkv_signals = "signals";

      // Different coordinators.
      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Requests have different coordinators.";
      out.back().request_relation = RequestRelation::kDifferentFetches;
      out.back().params2.coordinator =
          url::Origin::Create(GURL("https://other.coordinator.test"));

      // Different IP address spaces. Nonce is shared, because merging nonces in
      // the case that a more local and less local frame are running auctions at
      // the same time would only leak data to hosts on the more-local network,
      // the leak doesn't include much data, and the situation is very unlikely
      // to occur in practice.
      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Different IP address spaces";
      out.back().request_relation = RequestRelation::kDifferentFetches;
      out.back().params2.ip_address_space =
          network::mojom::IPAddressSpace::kLocal;
    }

    if constexpr (std::is_same<ParamsType, ScoringParams>::value) {
      // Note that no ScoringParams case currently results in
      // RequestRelation::kSamePartitionModified.

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Different main frame origins";
      out.back().request_relation = RequestRelation::kDifferentFetches;
      out.back().params2.main_frame_origin =
          url::Origin::Create(GURL("https://other.origin.test/"));

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Different sellers";
      out.back().request_relation = RequestRelation::kDifferentFetches;
      out.back().expect_same_network_partition_nonce = false;
      out.back().params2.script_origin =
          url::Origin::Create(GURL("https://other.seller.test/"));

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Different trusted scoring signals URLs";
      out.back().request_relation = RequestRelation::kDifferentFetches;
      out.back().expect_same_network_partition_nonce = false;
      out.back().params2.trusted_signals_url =
          GURL("https://seller.test/signals2");

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Different interest group owners";
      out.back().request_relation =
          RequestRelation::kDifferentCompressionGroups;
      out.back().params2.interest_group_owner =
          url::Origin::Create(GURL("https://other.bidder.test/"));

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Different joining origins";
      out.back().request_relation =
          RequestRelation::kDifferentCompressionGroups;
      out.back().params2.joining_origin =
          url::Origin::Create(GURL("https://other.joining.origin.test"));

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Different render URLs";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params2.render_url = GURL("https://other.render.test/foo");

      // Currently only exact matches of all URLs are results in reuse. Could do
      // better, but it would require more complicated searching routine, and
      // either a multimap potentially searching through multiple entries or
      // std::map and a willingness to throw away entries, like with bidding
      // signals. It's not clear if either approach is worth doing.

      out.emplace_back(CreateDefaultTestCase());
      out.back().description =
          "First request's component render URLs are a subset of the second "
          "request's";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params2.component_render_urls.emplace_back(
          GURL("https://component3.test/d"));

      out.emplace_back(CreateDefaultTestCase());
      out.back().description =
          "Second request's component render URLs are a subset of the first "
          "request's";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params2.component_render_urls.erase(
          out.back().params2.component_render_urls.begin());

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Requests have completely distinct keys";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params2.component_render_urls = {
          GURL("https://component3.test/d")};

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Requests have different `additional_params`";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params2.additional_params.Set("additional", "param");

      out.emplace_back(CreateDefaultTestCase());
      out.back().description =
          "Requests have one null and one valid seller_tkv_signals";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params2.seller_tkv_signals = "signals";

      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Requests have two different seller_tkv_signals";
      out.back().request_relation = RequestRelation::kDifferentPartitions;
      out.back().params1.seller_tkv_signals = "signals1";
      out.back().params2.seller_tkv_signals = "signals2";

      // Different coordinators.
      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Requests have different coordinators.";
      out.back().request_relation = RequestRelation::kDifferentFetches;
      out.back().params2.coordinator =
          url::Origin::Create(GURL("https://other.coordinator.test"));

      // Different IP address spaces. Nonce is shared, because merging nonces in
      // the case that a more local and less local frame are running auctions at
      // the same time would only leak data to hosts on the more-local network,
      // the leak doesn't include much data, and the situation is very unlikely
      // to occur in practice.
      out.emplace_back(CreateDefaultTestCase());
      out.back().description = "Different IP address spaces";
      out.back().request_relation = RequestRelation::kDifferentFetches;
      out.back().params2.ip_address_space =
          network::mojom::IPAddressSpace::kLocal;
    }

    // Cases shared by bidder and seller tests.

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Different SharedURLLoaderFactories";
    out.back().request_relation = RequestRelation::kDifferentFetches;
    out.back().params2.url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            /*factory_ptr=*/nullptr);

    out.emplace_back(CreateDefaultTestCase());
    out.back().description = "Different FrameTreeNodeIds";
    out.back().request_relation = RequestRelation::kDifferentFetches;
    out.back().params2.frame_tree_node_id = FrameTreeNodeId(2);

    return out;
  }

  // Create set of merged bidding parameters. Useful to use with
  // ValidateFetchParams() when two requests should be merged into a single
  // partition.
  BiddingParams CreateMergedParams(const BiddingParams& bidding_params1,
                                   const BiddingParams& bidding_params2) {
    // In order to merge two sets of params, only `interest_group_names` and
    // `trusted_bidding_signals_keys` may be different.
    EXPECT_EQ(bidding_params1.url_loader_factory,
              bidding_params2.url_loader_factory);
    EXPECT_EQ(bidding_params1.frame_tree_node_id,
              bidding_params2.frame_tree_node_id);
    EXPECT_EQ(bidding_params1.main_frame_origin,
              bidding_params2.main_frame_origin);
    EXPECT_EQ(bidding_params1.ip_address_space,
              bidding_params2.ip_address_space);
    EXPECT_EQ(bidding_params1.script_origin, bidding_params2.script_origin);
    EXPECT_EQ(bidding_params1.execution_mode, bidding_params2.execution_mode);
    EXPECT_EQ(bidding_params1.joining_origin, bidding_params2.joining_origin);
    EXPECT_EQ(bidding_params1.trusted_signals_url,
              bidding_params2.trusted_signals_url);
    EXPECT_EQ(bidding_params1.coordinator, bidding_params2.coordinator);
    EXPECT_EQ(bidding_params1.additional_params,
              bidding_params2.additional_params);
    EXPECT_EQ(bidding_params1.buyer_tkv_signals,
              bidding_params2.buyer_tkv_signals);

    BiddingParams merged_bidding_params{
        bidding_params1.url_loader_factory,
        bidding_params1.frame_tree_node_id,
        bidding_params1.devtools_auction_ids,
        bidding_params1.main_frame_origin,
        bidding_params1.ip_address_space,
        bidding_params1.script_origin,
        bidding_params1.interest_group_names,
        bidding_params1.execution_mode,
        bidding_params1.joining_origin,
        bidding_params1.trusted_signals_url,
        bidding_params1.coordinator,
        bidding_params1.trusted_bidding_signals_keys,
        bidding_params1.additional_params.Clone(),
        bidding_params1.buyer_tkv_signals};

    merged_bidding_params.devtools_auction_ids.insert(
        bidding_params2.devtools_auction_ids.begin(),
        bidding_params2.devtools_auction_ids.end());
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

  // Method to create set of merged scoring parameters. Currently this should
  // never happen, so adds a failure. Needed for the kSamePartitionUnmodified
  // case, which never happens for scoring, currently.
  ScoringParams CreateMergedParams(const ScoringParams& scoring_params1,
                                   const ScoringParams& params2) {
    ADD_FAILURE() << "This should not be reached";
    return ScoringParams();
  }

  // Returns a pair of a handle and `partition_id`. This pattern reduces
  // boilerplate a bit, at the cost of making types at callsites a little less
  // clear.
  //
  // If `start_fetch` is true, calls StartFetch() on the handle.
  std::pair<std::unique_ptr<TestTrustedSignalsCache::Handle>, int>
  RequestTrustedSignals(const BiddingParams& bidding_params,
                        bool start_fetch = true) {
    int partition_id = -1;
    // There should only be a single name for each request. It's a std::set
    // solely for the ValidateFetchParams family of methods.
    CHECK_EQ(1u, bidding_params.interest_group_names.size());
    auto handle = trusted_signals_cache_->RequestTrustedBiddingSignals(
        bidding_params.url_loader_factory, bidding_params.frame_tree_node_id,
        *bidding_params.devtools_auction_ids.begin(),
        bidding_params.main_frame_origin, bidding_params.ip_address_space,
        bidding_params.script_origin,
        *bidding_params.interest_group_names.begin(),
        bidding_params.execution_mode, bidding_params.joining_origin,
        bidding_params.trusted_signals_url, bidding_params.coordinator,
        bidding_params.trusted_bidding_signals_keys,
        bidding_params.additional_params.Clone(),
        bidding_params.buyer_tkv_signals, partition_id);

    // The call should never fail.
    CHECK(handle);
    CHECK(!handle->compression_group_token().is_empty());
    CHECK_GE(partition_id, 0);
    if (start_fetch) {
      handle->StartFetch();
    }

    return std::pair(std::move(handle), partition_id);
  }

  // Same as above, but for scoring signals.
  std::pair<std::unique_ptr<TestTrustedSignalsCache::Handle>, int>
  RequestTrustedSignals(const ScoringParams& scoring_params,
                        bool start_fetch = true) {
    int partition_id = -1;
    auto handle = trusted_signals_cache_->RequestTrustedScoringSignals(
        scoring_params.url_loader_factory, scoring_params.frame_tree_node_id,
        *scoring_params.devtools_auction_ids.begin(),
        scoring_params.main_frame_origin, scoring_params.ip_address_space,
        scoring_params.script_origin, scoring_params.trusted_signals_url,
        scoring_params.coordinator, scoring_params.interest_group_owner,
        scoring_params.joining_origin, scoring_params.render_url,
        scoring_params.component_render_urls,
        scoring_params.additional_params.Clone(),
        scoring_params.seller_tkv_signals, partition_id);

    // The call should never fail.
    CHECK(handle);
    CHECK(!handle->compression_group_token().is_empty());
    CHECK_GE(partition_id, 0);
    if (start_fetch) {
      handle->StartFetch();
    }

    return std::pair(std::move(handle), partition_id);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Defaults used by most tests.

  static constexpr FrameTreeNodeId kFrameTreeNodeId{1};
  const url::Origin kMainFrameOrigin =
      url::Origin::Create(GURL("https://main.frame.test"));
  const url::Origin kBidder = url::Origin::Create(GURL("https://bidder.test"));
  const std::string kInterestGroupName = "group1";
  const url::Origin kJoiningOrigin =
      url::Origin::Create(GURL("https://joining.origin.test"));
  const GURL kTrustedBiddingSignalsUrl{"https://bidder.test/signals"};

  const url::Origin kSeller = url::Origin::Create(GURL("https://seller.test"));
  const GURL kTrustedScoringSignalsUrl{"https://seller.test/signals"};
  const GURL kRenderUrl{"https://render.test/foo"};
  const std::vector<GURL> kComponentRenderUrls{
      GURL("https://component1.test/a"), GURL("https://component2.test/b")};

  const url::Origin kCoordinator =
      url::Origin::Create(GURL("https://coordinator.test"));

  // Use a SharedURLLoaderFactory that can't be used to make requests. This is
  // only used in pointer equality tests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          /*factory_ptr=*/nullptr);

  DataDecoderManager data_decoder_manager_;
  std::unique_ptr<TestTrustedSignalsCache> trusted_signals_cache_;
  mojo::Remote<auction_worklet::mojom::TrustedSignalsCache> cache_mojo_pipe_;
  // Some test cases need a second Mojo pipe for use with a second script
  // origin. This holds such a pipe. See CreateOrGetMojoPipeGivenParams(). Tests
  // that need more than one such pipe manage the extra pipes themselves.
  mojo::Remote<auction_worklet::mojom::TrustedSignalsCache>
      other_cache_mojo_pipe_;
};

// Used by gtest to provide clearer names for tests.
class SignalsCacheTestNames {
 public:
  template <typename T>
  static std::string GetName(int) {
    if constexpr (std::is_same<T, BiddingParams>::value) {
      return "BiddingSignals";
    }
    if constexpr (std::is_same<T, ScoringParams>::value) {
      return "ScoringSignals";
    }
  }
};

using ParamTypes = ::testing::Types<BiddingParams, ScoringParams>;
TYPED_TEST_SUITE(TrustedSignalsCacheTest, ParamTypes, SignalsCacheTestNames);

// Test the case where a GetTrustedSignals() request is received before the
// fetch completes.
TYPED_TEST(TrustedSignalsCacheTest, GetBeforeFetchCompletes) {
  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);
  EXPECT_EQ(partition_id, 0);

  // Wait for creation of the Fetcher before requesting over Mojo. Not needed,
  // but ensures the events in the test run in a consistent order.
  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id);

  TestTrustedSignalsCacheClient client(handle.get(), this->cache_mojo_pipe_);

  // Wait for the GetTrustedSignals call to make it to the cache.
  this->task_environment_.RunUntilIdle();
  EXPECT_FALSE(client.has_result());

  RespondToFetchWithSuccess(fetch);

  client.WaitForSuccess();
}

// Test the case where a GetTrustedSignals() request is received before the
// fetch fails.
TYPED_TEST(TrustedSignalsCacheTest, GetBeforeFetchFails) {
  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);
  EXPECT_EQ(partition_id, 0);

  // Wait for creation of the Fetcher before requesting over Mojo. Not needed,
  // but ensures the events in the test run in a consistent order.
  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id);

  TestTrustedSignalsCacheClient client(handle.get(), this->cache_mojo_pipe_);

  // Wait for the GetTrustedSignals call to make it to the cache.
  this->task_environment_.RunUntilIdle();
  EXPECT_FALSE(client.has_result());

  RespondToFetchWithError(fetch);
  client.WaitForError();
}

// Test the case where a GetTrustedSignals() request is made after the fetch
// completes.
TYPED_TEST(TrustedSignalsCacheTest, GetAfterFetchCompletes) {
  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);
  EXPECT_EQ(partition_id, 0);

  // Wait for the fetch to be observed and respond to it. No need to spin the
  // message loop, since fetch responses at this layer are passed directly to
  // the cache, and don't go through Mojo, as the TrustedSignalsFetcher is
  // entirely mocked out.
  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id);
  RespondToFetchWithSuccess(fetch);

  TestTrustedSignalsCacheClient client(handle.get(), this->cache_mojo_pipe_);
  client.WaitForSuccess();
}

// Test the case where a GetTrustedSignals() request is made after the fetch
// fails.
TYPED_TEST(TrustedSignalsCacheTest, GetAfterFetchFails) {
  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);
  EXPECT_EQ(partition_id, 0);

  // Wait for the fetch to be observed and respond to it. No need to spin the
  // message loop, since fetch responses at this layer are passed directly to
  // the cache, and don't go through Mojo, as the TrustedSignalsFetcher is
  // entirely mocked out.
  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id);
  RespondToFetchWithError(fetch);

  TestTrustedSignalsCacheClient client(handle.get(), this->cache_mojo_pipe_);
  client.WaitForError();
}

// Check that fetches are automatically started after kAutoStartDelay has
// elapsed.
TYPED_TEST(TrustedSignalsCacheTest, AutoStart) {
  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] =
      this->RequestTrustedSignals(params, /*start_fetch=*/false);

  // Request should only start once `kAutoStartDelay` has elapsed
  this->task_environment_.FastForwardBy(
      TrustedSignalsCacheImpl::kAutoStartDelay - kTinyTime);
  EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 0u);
  this->task_environment_.FastForwardBy(kTinyTime);
  EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 1u);

  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id);
  RespondToFetchWithSuccess(fetch);

  TestTrustedSignalsCacheClient client(handle.get(), this->cache_mojo_pipe_);
  client.WaitForSuccess();
}

// Check that fetches are automatically started after kAutoStartDelay, even if a
// second request is merged into it.
TYPED_TEST(TrustedSignalsCacheTest, AutoStartTwoRequests) {
  auto params = this->CreateDefaultParams();
  auto [handle1, partition_id1] =
      this->RequestTrustedSignals(params, /*start_fetch=*/false);

  // After a delay less than `kAutoStartDelay`, create a second request that
  // matches the first one. The old fetch should be reused.
  this->task_environment_.FastForwardBy(
      TrustedSignalsCacheImpl::kAutoStartDelay - kTinyTime);
  auto [handle2, partition_id2] =
      this->RequestTrustedSignals(params, /*start_fetch=*/false);
  EXPECT_EQ(handle1->compression_group_token(),
            handle2->compression_group_token());
  EXPECT_EQ(partition_id1, partition_id2);

  // No fetches should have been started.
  EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 0u);

  // After exactly `kAutoStartDelay`, the fetch should be started.
  this->task_environment_.FastForwardBy(kTinyTime);
  EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 1u);

  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id1);
  RespondToFetchWithSuccess(fetch);

  TestTrustedSignalsCacheClient client(handle1.get(), this->cache_mojo_pipe_);
  client.WaitForSuccess();
}

// Check that manually starting a request that has already automatically started
// doesn't cause any issues.
TYPED_TEST(TrustedSignalsCacheTest, AutoStartThenManuallyStart) {
  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] =
      this->RequestTrustedSignals(params, /*start_fetch=*/false);

  this->task_environment_.FastForwardBy(
      TrustedSignalsCacheImpl::kAutoStartDelay);
  EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 1u);

  // This should not cause another fetch to be started, nor cause a crash.
  handle->StartFetch();
  EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 1u);

  // Can safely call StartFetch() more than once, and no new fetches should be
  // started.
  handle->StartFetch();
  handle->StartFetch();
  handle->StartFetch();
  handle->StartFetch();
  EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 1u);

  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id);
  RespondToFetchWithSuccess(fetch);

  TestTrustedSignalsCacheClient client(handle.get(), this->cache_mojo_pipe_);
  client.WaitForSuccess();
}

// Check that the auto-start delay passing after a request was manually started
// doesn't cause issues. Test 3 cases: Auto start duration passes while Fetch is
// live, after Fetch has completed but cache entry is still live, and after
// cache entry has been destroyed.
TYPED_TEST(TrustedSignalsCacheTest, ManuallyStartThenAutoStart) {
  enum class TestCase {
    kAutoStartDuringFetch,
    kAutoStartAfterFetch,
    kAutoStartAfterHandleDestroyed,
  };

  for (TestCase test_case :
       {TestCase::kAutoStartDuringFetch, TestCase::kAutoStartAfterFetch,
        TestCase::kAutoStartAfterHandleDestroyed}) {
    SCOPED_TRACE(static_cast<int>(test_case));

    // Start with a clean slate for each test.
    this->CreateCache();

    auto params = this->CreateDefaultParams();
    // Create request and start the fetch.
    auto [handle, partition_id] = this->RequestTrustedSignals(params);

    // Wait for fetch creation.
    auto fetch = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                        partition_id);

    if (test_case == TestCase::kAutoStartDuringFetch) {
      this->task_environment_.FastForwardBy(
          TrustedSignalsCacheImpl::kAutoStartDelay);
    }
    EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 0u);

    RespondToFetchWithSuccess(fetch);
    if (test_case == TestCase::kAutoStartAfterFetch) {
      this->task_environment_.FastForwardBy(
          TrustedSignalsCacheImpl::kAutoStartDelay);
    }
    EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 0u);

    TestTrustedSignalsCacheClient client(handle.get(), this->cache_mojo_pipe_);
    client.WaitForSuccess();

    handle.reset();
    if (test_case == TestCase::kAutoStartAfterHandleDestroyed) {
      this->task_environment_.FastForwardBy(
          TrustedSignalsCacheImpl::kAutoStartDelay);
    }
    EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 0u);
  }
}

// Test the case where a Handle is destroyed without ever calling StartFetch()
// on it.
TYPED_TEST(TrustedSignalsCacheTest, HandleDestroyedWithoutStartingFetch) {
  enum class TestCase {
    kCancelBeforeCoordinatorKeyCallback,

    // Two cases where the Handle is cancelled while waiting on the
    // GetCoordinatorKeyCallback:
    // 1) The case where the callback is never invoked
    // 2) The case where it's invoked after cancellation.
    kCancelDuringCoordinatorKeyCallback,
    kCancelDuringCoordinatorKeyCallbackAndInvokeCallback,

    kCancelAfterCoordinatorKeyCallback,
  };

  for (TestCase test_case :
       {TestCase::kCancelBeforeCoordinatorKeyCallback,
        TestCase::kCancelDuringCoordinatorKeyCallback,
        TestCase::kCancelDuringCoordinatorKeyCallbackAndInvokeCallback,
        TestCase::kCancelAfterCoordinatorKeyCallback}) {
    SCOPED_TRACE(static_cast<int>(test_case));

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    this->CreateCache();
    this->trusted_signals_cache_->set_get_coordinator_key_mode(
        TestTrustedSignalsCache::GetCoordinatorKeyMode::kStashCallback);

    auto [handle, partition_id] = this->RequestTrustedSignals(
        this->CreateDefaultParams(), /*start_fetch=*/false);
    base::OnceClosure callback;
    if (test_case != TestCase::kCancelBeforeCoordinatorKeyCallback) {
      callback = this->trusted_signals_cache_->WaitForCoordinatorKeyCallback();
      if (test_case == TestCase::kCancelAfterCoordinatorKeyCallback) {
        std::move(callback).Run();
      }
    }

    // Destroy the handle, after getting a copy of the
    // `compression_group_token`.
    base::UnguessableToken compression_group_token =
        handle->compression_group_token();
    handle.reset();

    // No fetches should have been started.
    this->task_environment_.FastForwardBy(
        TrustedSignalsCacheImpl::kAutoStartDelay - base::Milliseconds(1));
    EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 0u);

    TestTrustedSignalsCacheClient client(compression_group_token,
                                         this->cache_mojo_pipe_);
    client.WaitForError(kRequestCancelledError);

    if (test_case ==
        TestCase::kCancelDuringCoordinatorKeyCallbackAndInvokeCallback) {
      // Invoking the GetCoordinatorKeyCallback late should not crash.
      std::move(callback).Run();
    }
  }
}

// Test the case where a GetTrustedSignals() request waiting on a fetch when the
// Handle is destroyed.
TYPED_TEST(TrustedSignalsCacheTest, HandleDestroyedAfterGet) {
  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);
  EXPECT_EQ(partition_id, 0);
  // Wait for the fetch.
  auto fetch = this->WaitForSignalsFetch();

  TestTrustedSignalsCacheClient client(handle.get(), this->cache_mojo_pipe_);
  // Wait fo the request to hit the cache.
  this->task_environment_.RunUntilIdle();

  handle.reset();
  client.WaitForError(kRequestCancelledError);
}

// Test the case where a GetTrustedSignals() request is made after the Handle
// has been destroyed.
//
// This test covers three cases:
// 1) The fetch was never started before the handle was destroyed.
// 2) The fetch was started but didn't complete before the handle was destroyed.
// 3) The fetch completed before the handle was destroyed.
//
// In the first two cases, since the fetch never completed, the entry is not
// cached, and the request should fail. In the third case, the entry is still
// cached, and should succeed.
TYPED_TEST(TrustedSignalsCacheTest, GetAfterHandleDestroyed) {
  enum class TestCase { kFetchNotStarted, kFetchNotCompleted, kFetchSucceeded };

  for (auto test_case :
       {TestCase::kFetchNotStarted, TestCase::kFetchNotCompleted,
        TestCase::kFetchSucceeded}) {
    SCOPED_TRACE(static_cast<int>(test_case));

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    this->CreateCache();
    auto params = this->CreateDefaultParams();
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    EXPECT_EQ(partition_id, 0);
    base::UnguessableToken compression_group_token =
        handle->compression_group_token();

    if (test_case != TestCase::kFetchNotStarted) {
      // Wait for the fetch to be observed.
      auto fetch = this->WaitForSignalsFetch();
      ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                          partition_id);
      if (test_case == TestCase::kFetchSucceeded) {
        // Respond to fetch if needed.
        RespondToFetchWithSuccess(fetch);
      }
    }

    handle.reset();

    TestTrustedSignalsCacheClient client(compression_group_token,
                                         this->cache_mojo_pipe_);
    if (test_case == TestCase::kFetchSucceeded) {
      client.WaitForSuccess();
    } else {
      client.WaitForError(kRequestCancelledError);
    }
  }
}

// Test requesting response bodies with novel keys that did not come from a
// Handle. Note that there's no need to test empty UnguessableTokens - the Mojo
// serialization code DCHECKs when passed them, and the deserialization code
// rejects them.
TYPED_TEST(TrustedSignalsCacheTest, GetWithNovelId) {
  // Novel id with no live cache entries.
  TestTrustedSignalsCacheClient client1(base::UnguessableToken::Create(),
                                        this->cache_mojo_pipe_);
  client1.WaitForError(kRequestCancelledError);

  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);

  // Novel id  with a cache entry with a pending fetch.
  TestTrustedSignalsCacheClient client2(base::UnguessableToken::Create(),
                                        this->cache_mojo_pipe_);
  client2.WaitForError(kRequestCancelledError);

  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id);
  RespondToFetchWithSuccess(fetch);

  // Novel id with a loaded cache entry.
  TestTrustedSignalsCacheClient client3(base::UnguessableToken::Create(),
                                        this->cache_mojo_pipe_);
  client3.WaitForError(kRequestCancelledError);
}

// Tests multiple GetTrustedSignals calls for a single request, with one live
// handle. Requests are made both before and after the response has been
// received.
TYPED_TEST(TrustedSignalsCacheTest, GetMultipleTimes) {
  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);

  // Wait for creation of the Fetcher before requesting over Mojo. Not needed,
  // but ensures the events in the test run in a consistent order.
  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id);

  TestTrustedSignalsCacheClient client1(handle.get(), this->cache_mojo_pipe_);
  TestTrustedSignalsCacheClient client2(handle.get(), this->cache_mojo_pipe_);
  TestTrustedSignalsCacheClient client3(handle.get(), this->cache_mojo_pipe_);

  // Wait for the GetTrustedSignals call to make it to the cache.
  this->task_environment_.RunUntilIdle();
  EXPECT_FALSE(client1.has_result());
  EXPECT_FALSE(client2.has_result());
  EXPECT_FALSE(client3.has_result());

  RespondToFetchWithSuccess(fetch);
  TestTrustedSignalsCacheClient client4(handle.get(), this->cache_mojo_pipe_);
  TestTrustedSignalsCacheClient client5(handle.get(), this->cache_mojo_pipe_);
  TestTrustedSignalsCacheClient client6(handle.get(), this->cache_mojo_pipe_);
  client1.WaitForSuccess();
  client2.WaitForSuccess();
  client3.WaitForSuccess();
  client4.WaitForSuccess();
  client5.WaitForSuccess();
  client6.WaitForSuccess();
}

// Check that re-requesting trusted bidding with the same arguments returns the
// same handle and IDs, when any Handle is still alive.
TYPED_TEST(TrustedSignalsCacheTest, ReRequestSignalsReusedHandleAlive) {
  auto params = this->CreateDefaultParams();
  auto [handle1, partition_id1] = this->RequestTrustedSignals(params);

  auto [handle2, partition_id2] = this->RequestTrustedSignals(params);
  EXPECT_EQ(handle1->compression_group_token(),
            handle2->compression_group_token());
  EXPECT_EQ(partition_id1, partition_id2);

  // Destroying the first handle should not cancel the request. This should be
  // implied by `handle1` and `handle2` being references to the same object as
  // well.
  handle1.reset();

  // Wait for Fetcher.
  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id1);

  // Create yet another handle, which should again be merged, and destroy the
  // second handle.
  auto [handle3, partition_id3] = this->RequestTrustedSignals(params);
  EXPECT_EQ(handle2->compression_group_token(),
            handle3->compression_group_token());
  EXPECT_EQ(partition_id2, partition_id3);
  handle2.reset();

  // Complete the request.
  RespondToFetchWithSuccess(fetch);

  // Create yet another handle, which should again be merged, and destroy the
  // third handle.
  auto [handle4, partition_id4] = this->RequestTrustedSignals(params);
  EXPECT_EQ(handle3->compression_group_token(),
            handle4->compression_group_token());
  EXPECT_EQ(partition_id3, partition_id4);
  handle3.reset();

  // Finally request the response body, which should succeed.
  TestTrustedSignalsCacheClient client(handle4.get(), this->cache_mojo_pipe_);
  client.WaitForSuccess();

  // No pending fetches should have been created after the first.
  EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 0u);
}

// Check that re-requesting trusted bidding with the same arguments returns the
// same handle and IDs, when there's no live Handle.
TYPED_TEST(TrustedSignalsCacheTest, ReRequestSignalsReusedHandleNotAlive) {
  auto params = this->CreateDefaultParams();
  auto [handle1, partition_id1] = this->RequestTrustedSignals(params);
  base::UnguessableToken token = handle1->compression_group_token();

  // Wait for Fetcher and complete the request, so the result will be kept alive
  // when the Handle is destroyed.
  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id1);
  RespondToFetchWithSuccess(fetch);
  TestTrustedSignalsCacheClient(handle1.get(), this->cache_mojo_pipe_)
      .WaitForSuccess();

  // Destroying the only Handle should not destroy the underlying data.
  handle1.reset();

  // Re-requesting the data should return the same entry.
  auto [handle2, partition_id2] = this->RequestTrustedSignals(params);
  EXPECT_EQ(token, handle2->compression_group_token());
  EXPECT_EQ(partition_id1, partition_id2);
  TestTrustedSignalsCacheClient(handle2.get(), this->cache_mojo_pipe_)
      .WaitForSuccess();

  // Destroying the only Handle should not destroy the underlying data, even if
  // some time passes, if the time is less than the TTL.
  handle2.reset();
  this->task_environment_.FastForwardBy(base::Minutes(1));

  // Re-requesting the data yet again should return the same entry.
  auto [handle3, partition_id3] = this->RequestTrustedSignals(params);
  EXPECT_EQ(token, handle3->compression_group_token());
  EXPECT_EQ(partition_id1, partition_id3);
  TestTrustedSignalsCacheClient(handle3.get(), this->cache_mojo_pipe_)
      .WaitForSuccess();
}

// Check that re-requesting trusted bidding with the same arguments returns the
// same handle and IDs. Only starts the fetch after the second request.
TYPED_TEST(TrustedSignalsCacheTest, ReRequestSignalsReusedLateStartFetch) {
  auto params = this->CreateDefaultParams();
  auto [handle1, partition_id1] =
      this->RequestTrustedSignals(params, /*start_fetch=*/false);
  auto [handle2, partition_id2] =
      this->RequestTrustedSignals(params, /*start_fetch=*/true);
  EXPECT_EQ(handle1->compression_group_token(),
            handle2->compression_group_token());
  EXPECT_EQ(partition_id1, partition_id2);

  // Wait for Fetcher.
  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id1);

  // Complete the request.
  RespondToFetchWithSuccess(fetch);

  TestTrustedSignalsCacheClient client(handle1.get(), this->cache_mojo_pipe_);
  client.WaitForSuccess();

  // No pending fetches should have been created after the first.
  EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 0u);
}

// Check that re-requesting trusted bidding with the same arguments returns a
// different ID, when all Handles have been destroyed and the original fetch did
// not complete.
TYPED_TEST(TrustedSignalsCacheTest, ReRequestSignalsNotReused) {
  auto params = this->CreateDefaultParams();

  // Create a Handle, create a request for it, destroy the Handle.
  auto [handle1, partition_id1] = this->RequestTrustedSignals(params);
  base::UnguessableToken compression_group_token1 =
      handle1->compression_group_token();
  TestTrustedSignalsCacheClient client1(handle1.get(), this->cache_mojo_pipe_);
  handle1.reset();
  EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 0u);

  // A new request with the same parameters should get a new
  // `compression_group_id`.
  auto [handle2, partition_id2] = this->RequestTrustedSignals(params);
  base::UnguessableToken compression_group_token2 =
      handle2->compression_group_token();
  EXPECT_NE(compression_group_token1, compression_group_token2);
  TestTrustedSignalsCacheClient client2(handle2.get(), this->cache_mojo_pipe_);

  // Wait for fetch request, then destroy the second handle.
  auto fetch2 = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch2, params, /*expected_compression_group_id=*/0,
                      partition_id2);
  handle2.reset();

  // A new request with the same parameters should get a new
  // `compression_group_id`.
  auto [handle3, partition_id3] = this->RequestTrustedSignals(params);
  base::UnguessableToken compression_group_token3 =
      handle3->compression_group_token();
  EXPECT_NE(compression_group_token1, compression_group_token3);
  EXPECT_NE(compression_group_token2, compression_group_token3);
  TestTrustedSignalsCacheClient client3(handle3.get(), this->cache_mojo_pipe_);
  // Wait for another fetch request, send a response, and retrieve it over the
  // Mojo pipe.
  auto fetch3 = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch3, params, /*expected_compression_group_id=*/0,
                      partition_id3);
  RespondToFetchWithSuccess(fetch3);

  // All cache clients but the last should receive errors.
  client1.WaitForError(kRequestCancelledError);
  client2.WaitForError(kRequestCancelledError);
  client3.WaitForSuccess();
}

// Test the case where a bidding signals request is made while there's still an
// outstanding Handle, but the response has expired.
TYPED_TEST(TrustedSignalsCacheTest, OutstandingHandleResponseExpired) {
  const base::TimeDelta kTtl = base::Minutes(10);

  auto params = this->CreateDefaultParams();
  auto [handle1, partition_id1] = this->RequestTrustedSignals(params);

  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id1);
  RespondToFetchWithSuccess(
      fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      kSuccessBody, kTtl);

  // Wait until just before the response has expired.
  this->task_environment_.FastForwardBy(kTtl - kTinyTime);

  // A request for `handle1`'s data should succeed.
  TestTrustedSignalsCacheClient(handle1.get(), this->cache_mojo_pipe_)
      .WaitForSuccess();

  // Re-requesting the data before expiration time should return the same Handle
  // and partition.
  auto [handle2, partition_id2] = this->RequestTrustedSignals(params);
  EXPECT_EQ(handle1->compression_group_token(),
            handle2->compression_group_token());
  EXPECT_EQ(partition_id1, partition_id2);

  // Run until the expiration time. When the time exactly equals the expiration
  // time, the entry should be considered expired.
  this->task_environment_.FastForwardBy(kTinyTime);

  // A request for `handle1`'s data should return the same value as before.
  TestTrustedSignalsCacheClient(handle1.get(), this->cache_mojo_pipe_)
      .WaitForSuccess();

  // Re-request the data. A different handle should be returned, since the old
  // data has expired.
  auto [handle3, partition_id3] = this->RequestTrustedSignals(params);
  EXPECT_NE(handle1->compression_group_token(),
            handle3->compression_group_token());

  // Give a different response for the second fetch.
  fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id3);
  RespondToFetchWithSuccess(
      fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
      kOtherSuccessBody, kTtl);

  // A request for `handle3`'s data should return the different data.
  TestTrustedSignalsCacheClient(handle3.get(), this->cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
          kOtherSuccessBody);

  // A request for `handle1`'s data should return the same value as before, even
  // though it has expired.
  TestTrustedSignalsCacheClient(handle1.get(), this->cache_mojo_pipe_)
      .WaitForSuccess();
}

// Check that bidding signals error responses are not cached beyond the end of
// the fetch.
TYPED_TEST(TrustedSignalsCacheTest, OutstandingHandleError) {
  auto params = this->CreateDefaultParams();
  auto [handle1, partition_id1] = this->RequestTrustedSignals(params);

  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id1);

  // Re-requesting the data before the response is received should return the
  // same Handle and partition.
  auto [handle2, partition_id2] = this->RequestTrustedSignals(params);
  EXPECT_EQ(handle1->compression_group_token(),
            handle2->compression_group_token());
  EXPECT_EQ(partition_id1, partition_id2);

  RespondToFetchWithError(fetch);

  // A request for `handle1`'s data should return the error.
  TestTrustedSignalsCacheClient(handle1.get(), this->cache_mojo_pipe_)
      .WaitForError();

  // Re-request the data. A different handle should be returned, since the error
  // should not be cached.
  auto [handle3, partition_id3] = this->RequestTrustedSignals(params);
  EXPECT_NE(handle1->compression_group_token(),
            handle3->compression_group_token());

  // Give a success response for the second fetch.
  fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id3);
  RespondToFetchWithSuccess(fetch);

  // A request for `handle3`'s data should return a success.
  TestTrustedSignalsCacheClient(handle3.get(), this->cache_mojo_pipe_)
      .WaitForSuccess();

  // A request for `handle1`'s data should still return the error.
  TestTrustedSignalsCacheClient(handle1.get(), this->cache_mojo_pipe_)
      .WaitForError();
}

// Check that zero (and negative) TTL bidding signals responses are handled
// appropriately. Test both the case the original Handles are kept alive and the
// case they're not.
TYPED_TEST(TrustedSignalsCacheTest, OutstandingHandleSuccessZeroTTL) {
  for (bool keep_handles_alive : {false, true}) {
    for (base::TimeDelta ttl : {base::Seconds(-1), base::Seconds(0)}) {
      // Start with a clean slate for each test. Not strictly necessary, but
      // limits what's under test a bit.
      this->CreateCache();

      auto params = this->CreateDefaultParams();
      auto [handle1, partition_id1] = this->RequestTrustedSignals(params);

      auto fetch = this->WaitForSignalsFetch();
      ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                          partition_id1);

      // Re-requesting the data before a response is received should return the
      // same compression group and partition.
      auto [handle2, partition_id2] = this->RequestTrustedSignals(params);
      EXPECT_EQ(handle1->compression_group_token(),
                handle2->compression_group_token());
      EXPECT_EQ(partition_id1, partition_id2);

      RespondToFetchWithSuccess(
          fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
          kSuccessBody, ttl);

      // A request for `handle1's` data should succeed.
      TestTrustedSignalsCacheClient(handle1.get(), this->cache_mojo_pipe_)
          .WaitForSuccess();

      base::UnguessableToken handle1_token = handle1->compression_group_token();
      if (!keep_handles_alive) {
        handle1.reset();
        handle2.reset();
        // If not keeping the Handles alive, the underlying data should be
        // destroyed, since it's not reusable.
        EXPECT_EQ(this->trusted_signals_cache_->size_for_testing(), 0u);
        EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 0u);
      } else {
        // Otherwise, the data should still be available in the cache.
        EXPECT_GT(this->trusted_signals_cache_->size_for_testing(), 0u);
        EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 1u);
      }

      // Re-request the data. A different Handle should be returned, since the
      // data should not be cached.
      auto [handle3, partition_id3] = this->RequestTrustedSignals(params);
      EXPECT_NE(handle1_token, handle3->compression_group_token());

      // Give a different response for the second fetch.
      fetch = this->WaitForSignalsFetch();
      ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                          partition_id3);
      RespondToFetchWithSuccess(
          fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
          kOtherSuccessBody, ttl);

      // A request for `handle3`'s data should return the different data.
      TestTrustedSignalsCacheClient(handle3.get(), this->cache_mojo_pipe_)
          .WaitForSuccess(
              auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
              kOtherSuccessBody);

      TestTrustedSignalsCacheClient client(handle1_token,
                                           this->cache_mojo_pipe_);
      if (keep_handles_alive) {
        // If `handle1` is still alive, a request for `handle1`'s data should
        // return the same value as before, even though it has expired.
        client.WaitForSuccess();
      } else {
        // Otherwise, the data should have been destroyed to free up memory, so
        // requesting it should fail.
        client.WaitForError(kRequestCancelledError);
      }
    }
  }
}

// Test that a cache entry is reusable until it expires. Tests both the case
// that handles are kept alive and the case that they're not.
TYPED_TEST(TrustedSignalsCacheTest, ReusableUntilExpires) {
  const base::TimeDelta kTtl = base::Seconds(10);
  // Time to wait before checking if entry is still accessible.
  const base::TimeDelta kWaitTime = kTtl / 10;

  for (bool keep_handles_alive : {false, true}) {
    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    this->CreateCache();

    auto params = this->CreateDefaultParams();

    base::UnguessableToken token;
    std::vector<std::unique_ptr<TestTrustedSignalsCache::Handle>> handles;
    for (int i = 0; i < 10; ++i) {
      this->task_environment_.FastForwardBy(kWaitTime);

      // For all loop iterations but the first, there should be a single
      // compression group in the cache.
      EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(),
                i == 0 ? 0u : 1u);

      auto [handle, partition_id] = this->RequestTrustedSignals(params);
      if (i == 0) {
        // Only the first request should trigger a fetch.
        token = handle->compression_group_token();
        auto fetch = this->WaitForSignalsFetch();
        ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                            partition_id);
        RespondToFetchWithSuccess(
            fetch,
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
            kSuccessBody, kTtl);
      } else {
        // Others should reuse the compression group from the first request.
        EXPECT_EQ(token, handle->compression_group_token());
      }

      // A request for the data should succeed.
      TestTrustedSignalsCacheClient(handle.get(), this->cache_mojo_pipe_)
          .WaitForSuccess();

      if (keep_handles_alive) {
        handles.emplace_back(std::move(handle));
      }
    }

    // This should result in the data finally expiring.
    this->task_environment_.FastForwardBy(kWaitTime);

    // A new request for the same data should start a new fetch.
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    EXPECT_NE(handle->compression_group_token(), token);
    auto fetch = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                        partition_id);
    // Respond with different data, and make sure it can be retrieved.
    RespondToFetchWithSuccess(
        fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
        kOtherSuccessBody);
    TestTrustedSignalsCacheClient(handle.get(), this->cache_mojo_pipe_)
        .WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
            kOtherSuccessBody);
  }
}

// Test the case of expiration of two requests that share the same compression
// group, but are in different partitions.
TYPED_TEST(TrustedSignalsCacheTest,
           OutstandingHandleResponseExpiredSharedCompressionGroup) {
  const base::TimeDelta kTtl = base::Minutes(10);

  auto params1 = this->CreateDefaultParams();
  auto params2 = this->CreateDefaultParams();

  // Modify `params2` so that the requests share the same compression group but
  // not the same partition. Need separate bidder and seller code.
  if constexpr (std::is_same<TypeParam, BiddingParams>::value) {
    params2.interest_group_names = {"other interest group"};
  }
  if constexpr (std::is_same<TypeParam, ScoringParams>::value) {
    params2.render_url = GURL("https://render.other.test/");
  }

  auto [handle1, partition_id1] = this->RequestTrustedSignals(params1);
  auto [handle2, partition_id2] = this->RequestTrustedSignals(params2);
  EXPECT_EQ(handle1->compression_group_token(),
            handle2->compression_group_token());
  EXPECT_NE(partition_id1, partition_id2);
  auto fetch = this->WaitForSignalsFetch();

  EXPECT_EQ(fetch.trusted_signals_url, params1.trusted_signals_url);
  ASSERT_EQ(fetch.compression_groups.size(), 1u);
  EXPECT_EQ(fetch.compression_groups.begin()->first, 0);

  const auto& partitions = fetch.compression_groups.begin()->second;
  ASSERT_EQ(partitions.size(), 2u);
  ValidateFetchParamsForPartition(partitions[0], params1, partition_id1);
  ValidateFetchParamsForPartition(partitions[1], params2, partition_id2);
  RespondToFetchWithSuccess(
      fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      kSuccessBody, kTtl);

  // Wait until just before the response has expired.
  this->task_environment_.FastForwardBy(kTtl - kTinyTime);

  // Re-requesting either set of parameters should return the same Handle and
  // partition as the first requests.
  auto [handle3, partition_id3] = this->RequestTrustedSignals(params1);
  EXPECT_EQ(handle1->compression_group_token(),
            handle3->compression_group_token());
  EXPECT_EQ(partition_id1, partition_id3);
  auto [handle4, partition_id4] = this->RequestTrustedSignals(params2);
  EXPECT_EQ(handle2->compression_group_token(),
            handle4->compression_group_token());
  EXPECT_EQ(partition_id2, partition_id4);

  // Run until the expiration time. When the time exactly equals the expiration
  // time, the entry should be considered expired.
  this->task_environment_.FastForwardBy(kTinyTime);

  // Re-request the data for both parameters. A different Handle should be
  // returned from the original, since the old data has expired. As before, both
  // requests should share a Handle but have distinct partition IDs.
  auto [handle5, partition_id5] = this->RequestTrustedSignals(params1);
  EXPECT_NE(handle1->compression_group_token(),
            handle5->compression_group_token());
  auto [handle6, partition_id6] = this->RequestTrustedSignals(params2);
  EXPECT_NE(handle2->compression_group_token(),
            handle6->compression_group_token());
  EXPECT_EQ(handle5->compression_group_token(),
            handle6->compression_group_token());
  EXPECT_NE(partition_id5, partition_id6);

  // Give a different response for the second fetch.
  fetch = this->WaitForSignalsFetch();
  RespondToFetchWithSuccess(
      fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
      kOtherSuccessBody, kTtl);

  // A request for `handle5`'s data should return the second fetch's data. No
  // need to request the data for `handle6`, since it's the same Handle.
  TestTrustedSignalsCacheClient(handle5.get(), this->cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
          kOtherSuccessBody);

  // A request for `handle1`'s data should return the first fetch's data. No
  // need to request the data for `handle2`, since it's the same Handle.
  TestTrustedSignalsCacheClient(handle1.get(), this->cache_mojo_pipe_)
      .WaitForSuccess();
}

// Test the case of expiration of two requests that are sent in the same fetch,
// but in different compression groups. The requests have different expiration
// times.
TYPED_TEST(TrustedSignalsCacheTest,
           OutstandingHandleResponseExpiredDifferentCompressionGroup) {
  const base::TimeDelta kTtl1 = base::Minutes(5);
  const base::TimeDelta kTtl2 = base::Minutes(10);

  auto params1 = this->CreateDefaultParams();
  auto params2 = this->CreateDefaultParams();
  params2.joining_origin =
      url::Origin::Create(GURL("https://other.joining.origin.test"));

  auto [handle1, partition_id1] = this->RequestTrustedSignals(params1);
  auto [handle2, partition_id2] = this->RequestTrustedSignals(params2);
  EXPECT_NE(handle1->compression_group_token(),
            handle2->compression_group_token());
  auto fetch = this->WaitForSignalsFetch();

  EXPECT_EQ(fetch.trusted_signals_url, params1.trusted_signals_url);
  ASSERT_EQ(fetch.compression_groups.size(), 2u);

  // Compression groups are appended in FIFO order.
  ASSERT_EQ(1u, fetch.compression_groups.count(0));
  ValidateFetchParamsForPartitions(fetch.compression_groups.at(0), params1,
                                   partition_id1);
  ASSERT_EQ(1u, fetch.compression_groups.count(1));
  ValidateFetchParamsForPartitions(fetch.compression_groups.at(1), params2,
                                   partition_id2);

  // Respond with different results for each compression group.
  RespondToTwoCompressionGroupFetchWithSuccess(fetch, kTtl1, kTtl2);

  // Wait until just before the first compression group's data has expired.
  this->task_environment_.FastForwardBy(kTtl1 - kTinyTime);

  // Re-request both sets of parameters. The same Handles should be returned.
  auto [handle3, partition_id3] = this->RequestTrustedSignals(params1);
  EXPECT_EQ(handle1->compression_group_token(),
            handle3->compression_group_token());
  EXPECT_EQ(partition_id1, partition_id3);
  auto [handle4, partition_id4] = this->RequestTrustedSignals(params2);
  EXPECT_EQ(handle2->compression_group_token(),
            handle4->compression_group_token());
  EXPECT_EQ(partition_id2, partition_id4);

  // Wait until the first compression group's data has expired.
  this->task_environment_.FastForwardBy(kTinyTime);

  // Re-request both sets of parameters. The first set of parameters should get
  // a new handle, and trigger a new fetch. The second set of parameters should
  // get the same Handle, since it has yet to expire.
  auto [handle5, partition_id5] = this->RequestTrustedSignals(params1);
  EXPECT_NE(handle1->compression_group_token(),
            handle5->compression_group_token());
  auto [handle6, partition_id6] = this->RequestTrustedSignals(params2);
  EXPECT_EQ(handle2->compression_group_token(),
            handle6->compression_group_token());
  EXPECT_EQ(partition_id2, partition_id6);

  // Validate there is indeed a new fetch for the first set of parameters, and
  // provide a response.
  fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params1, /*expected_compression_group_id=*/0,
                      partition_id5);
  RespondToFetchWithSuccess(
      fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
      kSomeOtherSuccessBody, kTtl2);

  // Wait until just before the first compression group's data has expired.
  this->task_environment_.FastForwardBy(kTtl1 - kTinyTime);

  // Re-request both sets of parameters. The same Handles should be returned as
  // the last time.
  auto [handle7, partition_id7] = this->RequestTrustedSignals(params1);
  EXPECT_EQ(handle5->compression_group_token(),
            handle7->compression_group_token());
  EXPECT_EQ(partition_id5, partition_id7);
  auto [handle8, partition_id8] = this->RequestTrustedSignals(params2);
  EXPECT_EQ(handle2->compression_group_token(),
            handle8->compression_group_token());
  EXPECT_EQ(partition_id2, partition_id8);

  // Wait until the second compression group's data has expired.
  this->task_environment_.FastForwardBy(kTinyTime);

  // Re-request both sets of parameters. This time, only the second set of
  // parameters should get a new Handle.
  auto [handle9, partition_id9] = this->RequestTrustedSignals(params1);
  EXPECT_EQ(handle5->compression_group_token(),
            handle9->compression_group_token());
  EXPECT_EQ(partition_id5, partition_id9);
  auto [handle10, partition_id10] = this->RequestTrustedSignals(params2);
  EXPECT_NE(handle2->compression_group_token(),
            handle10->compression_group_token());

  // Validate there is indeed a new fetch for the second set of parameters, and
  // provide a response.
  fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params2, /*expected_compression_group_id=*/0,
                      partition_id9);
  RespondToFetchWithSuccess(
      fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      kSomeOtherSuccessBody, kTtl2);

  // Validate the responses for each of the distinct Handles. Even the ones
  // associated with expired data should still receive success responses, since
  // data lifetime is scoped to that of the associated Handle.
  TestTrustedSignalsCacheClient(handle1.get(), this->cache_mojo_pipe_)
      .WaitForSuccess();
  TestTrustedSignalsCacheClient(handle2.get(), this->cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
          kOtherSuccessBody);
  TestTrustedSignalsCacheClient(handle5.get(), this->cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
          kSomeOtherSuccessBody);
  TestTrustedSignalsCacheClient(handle10.get(), this->cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
          kSomeOtherSuccessBody);
}

// Test the case where the response has no compression groups.
TYPED_TEST(TrustedSignalsCacheTest, NoCompressionGroup) {
  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);

  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id);

  // Respond with an empty map with no compression groups.
  std::move(fetch.callback).Run({});

  TestTrustedSignalsCacheClient(handle.get(), this->cache_mojo_pipe_)
      .WaitForError("Fetched signals missing compression group 0.");
}

// Test the case where only information for the wrong compression group is
// received.
TYPED_TEST(TrustedSignalsCacheTest, WrongCompressionGroup) {
  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);

  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id);

  // Modify index of the only compression group when generating a response.
  CHECK(base::Contains(fetch.compression_groups, 0));
  auto compression_group_node = fetch.compression_groups.extract(0);
  compression_group_node.key() = 1;
  fetch.compression_groups.insert(std::move(compression_group_node));
  RespondToFetchWithSuccess(fetch);

  // A request for `handle3`'s data should return the different data.
  TestTrustedSignalsCacheClient(handle.get(), this->cache_mojo_pipe_)
      .WaitForError("Fetched signals missing compression group 0.");
}

// Test the case where only one of two compression groups is returned by the
// server. Both compression groups should fail. Run two test cases, one with the
// first compression group missing, one with the second missing.
TYPED_TEST(TrustedSignalsCacheTest, OneCompressionGroupMissing) {
  for (int missing_group : {0, 1}) {
    auto params1 = this->CreateDefaultParams();
    auto params2 = this->CreateDefaultParams();
    params2.joining_origin =
        url::Origin::Create(GURL("https://other.joining.origin.test"));

    auto [handle1, partition_id1] = this->RequestTrustedSignals(params1);
    auto [handle2, partition_id2] = this->RequestTrustedSignals(params2);
    EXPECT_NE(handle1->compression_group_token(),
              handle2->compression_group_token());

    auto fetch = this->WaitForSignalsFetch();
    ASSERT_EQ(fetch.compression_groups.size(), 2u);

    // Remove missing compression group from the request, and generate a valid
    // response for the other group.
    ASSERT_TRUE(fetch.compression_groups.erase(missing_group));
    RespondToFetchWithSuccess(fetch);

    std::string expected_error = base::StringPrintf(
        "Fetched signals missing compression group %i.", missing_group);

    // Even though the data for only one Handle was missing, both should have
    // the same error.
    TestTrustedSignalsCacheClient(handle1.get(), this->cache_mojo_pipe_)
        .WaitForError(expected_error);
    TestTrustedSignalsCacheClient(handle2.get(), this->cache_mojo_pipe_)
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
TYPED_TEST(TrustedSignalsCacheTest, DifferentParamsBeforeFetchStart) {
  for (const auto& test_case : this->CreateTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    this->CreateCache();
    const auto& params1 = test_case.params1;
    const auto& params2 = test_case.params2;

    auto [handle1, partition_id1] = this->RequestTrustedSignals(params1);
    auto [handle2, partition_id2] = this->RequestTrustedSignals(params2);

    switch (test_case.request_relation) {
      case RequestRelation::kDifferentFetches: {
        ASSERT_NE(handle1, handle2);
        ASSERT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());
        auto fetches = this->WaitForSignalsFetches(2);

        // fetches are made in FIFO order.
        ValidateFetchParams(fetches[0], params1,
                            /*expected_compression_group_id=*/0, partition_id1);
        ValidateFetchParams(fetches[1], params2,
                            /*expected_compression_group_id=*/0, partition_id2);
        EXPECT_EQ(fetches[0].network_partition_nonce ==
                      fetches[1].network_partition_nonce,
                  test_case.expect_same_network_partition_nonce);

        // Make both requests succeed with different bodies, and check that they
        // can be read.
        RespondToFetchWithSuccess(fetches[0]);
        RespondToFetchWithSuccess(
            fetches[1],
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        TestTrustedSignalsCacheClient client1(handle1.get(),
                                              this->cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client2(
            handle2.get(), this->CreateOrGetMojoPipeGivenParams(params2));
        client1.WaitForSuccess();
        client2.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentCompressionGroups: {
        EXPECT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());
        auto fetch = this->WaitForSignalsFetch();

        EXPECT_EQ(fetch.trusted_signals_url, params1.trusted_signals_url);
        ASSERT_EQ(fetch.compression_groups.size(), 2u);

        // Compression groups are appended in FIFO order.
        ASSERT_EQ(1u, fetch.compression_groups.count(0));
        ValidateFetchParamsForPartitions(fetch.compression_groups.at(0),
                                         params1, partition_id1);
        ASSERT_EQ(1u, fetch.compression_groups.count(1));
        ValidateFetchParamsForPartitions(fetch.compression_groups.at(1),
                                         params2, partition_id2);

        // Respond with different results for each compression group.
        RespondToTwoCompressionGroupFetchWithSuccess(fetch);
        TestTrustedSignalsCacheClient client1(handle1.get(),
                                              this->cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client2(handle2.get(),
                                              this->cache_mojo_pipe_);
        client1.WaitForSuccess();
        client2.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentPartitions: {
        EXPECT_EQ(handle1->compression_group_token(),
                  handle2->compression_group_token());
        EXPECT_NE(partition_id1, partition_id2);
        auto fetch = this->WaitForSignalsFetch();

        EXPECT_EQ(fetch.trusted_signals_url, params1.trusted_signals_url);
        ASSERT_EQ(fetch.compression_groups.size(), 1u);
        EXPECT_EQ(fetch.compression_groups.begin()->first, 0);

        const auto& partitions = fetch.compression_groups.begin()->second;
        ASSERT_EQ(partitions.size(), 2u);
        ValidateFetchParamsForPartition(partitions[0], params1, partition_id1);
        ValidateFetchParamsForPartition(partitions[1], params2, partition_id2);

        // Respond with a single response for the partition, and read it - no
        // need for multiple clients, since the handles are the same.
        RespondToFetchWithSuccess(fetch);
        TestTrustedSignalsCacheClient client(handle1.get(),
                                             this->cache_mojo_pipe_);
        client.WaitForSuccess();
        break;
      }

      case RequestRelation::kSamePartitionModified:
      case RequestRelation::kSamePartitionUnmodified: {
        EXPECT_EQ(handle1->compression_group_token(),
                  handle2->compression_group_token());
        EXPECT_EQ(partition_id1, partition_id2);
        auto fetch = this->WaitForSignalsFetch();

        auto merged_params = this->CreateMergedParams(params1, params2);
        // The fetch exactly match the merged parameters.
        ValidateFetchParams(fetch, merged_params,
                            /*expected_compression_group_id=*/0, partition_id1);

        // Respond with a single response for the partition, and read it - no
        // need for multiple clients, since the handles are the same.
        RespondToFetchWithSuccess(fetch);
        TestTrustedSignalsCacheClient client(handle1.get(),
                                             this->cache_mojo_pipe_);
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
TYPED_TEST(TrustedSignalsCacheTest, DifferentParamsAfterFetchStart) {
  for (const auto& test_case : this->CreateTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    this->CreateCache();
    const auto& params1 = test_case.params1;
    const auto& params2 = test_case.params2;

    auto [handle1, partition_id1] = this->RequestTrustedSignals(params1);
    auto fetch1 = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch1, params1, /*expected_compression_group_id=*/0,
                        partition_id1);
    auto [handle2, partition_id2] = this->RequestTrustedSignals(params2);

    switch (test_case.request_relation) {
      case RequestRelation::kDifferentFetches:
      case RequestRelation::kDifferentCompressionGroups:
      case RequestRelation::kDifferentPartitions:
      case RequestRelation::kSamePartitionModified: {
        ASSERT_NE(handle1, handle2);
        ASSERT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());

        auto fetch2 = this->WaitForSignalsFetch();
        ValidateFetchParams(fetch2, params2,
                            /*expected_compression_group_id=*/0, partition_id2);
        EXPECT_EQ(
            fetch1.network_partition_nonce == fetch2.network_partition_nonce,
            test_case.expect_same_network_partition_nonce);

        // Make both requests succeed with different bodies, and check that they
        // can be read.
        RespondToFetchWithSuccess(fetch1);
        RespondToFetchWithSuccess(
            fetch2,
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        TestTrustedSignalsCacheClient client1(handle1.get(),
                                              this->cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client2(
            handle2.get(), this->CreateOrGetMojoPipeGivenParams(params2));
        client1.WaitForSuccess();
        client2.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kSamePartitionUnmodified: {
        EXPECT_EQ(handle1->compression_group_token(),
                  handle2->compression_group_token());
        EXPECT_EQ(partition_id1, partition_id2);

        // Respond with a single response for the partition, and read it - no
        // need for multiple clients, since the handles are the same.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client(handle1.get(),
                                             this->cache_mojo_pipe_);
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
TYPED_TEST(TrustedSignalsCacheTest, DifferentParamsAfterFetchComplete) {
  for (const auto& test_case : this->CreateTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    this->CreateCache();
    const auto& params1 = test_case.params1;
    const auto& params2 = test_case.params2;

    auto [handle1, partition_id1] = this->RequestTrustedSignals(params1);
    auto fetch1 = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch1, params1, /*expected_compression_group_id=*/0,
                        partition_id1);
    RespondToFetchWithSuccess(fetch1);
    TestTrustedSignalsCacheClient client1(handle1.get(),
                                          this->cache_mojo_pipe_);
    client1.WaitForSuccess();

    auto [handle2, partition_id2] = this->RequestTrustedSignals(params2);

    switch (test_case.request_relation) {
      case RequestRelation::kDifferentFetches:
      case RequestRelation::kDifferentCompressionGroups:
      case RequestRelation::kDifferentPartitions:
      case RequestRelation::kSamePartitionModified: {
        ASSERT_NE(handle1, handle2);
        ASSERT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());

        auto fetch2 = this->WaitForSignalsFetch();
        ValidateFetchParams(fetch2, params2,
                            /*expected_compression_group_id=*/0, partition_id2);
        EXPECT_EQ(
            fetch1.network_partition_nonce == fetch2.network_partition_nonce,
            test_case.expect_same_network_partition_nonce);

        RespondToFetchWithSuccess(
            fetch2,
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        TestTrustedSignalsCacheClient client2(
            handle2.get(), this->CreateOrGetMojoPipeGivenParams(params2));
        client2.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kSamePartitionUnmodified: {
        EXPECT_EQ(handle1->compression_group_token(),
                  handle2->compression_group_token());
        EXPECT_EQ(partition_id1, partition_id2);
        TestTrustedSignalsCacheClient client2(handle2.get(),
                                              this->cache_mojo_pipe_);
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
TYPED_TEST(TrustedSignalsCacheTest,
           DifferentParamsCancelSecondBeforeFetchStart) {
  for (auto& test_case : this->CreateTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    this->CreateCache();
    auto params1 = std::move(test_case.params1);
    auto params2 = std::move(test_case.params2);

    // Don't bother to compare handles here - that's covered by another test.
    auto [handle1, partition_id1] = this->RequestTrustedSignals(params1);
    auto [handle2, partition_id2] = this->RequestTrustedSignals(params2);

    // Cancel the second request immediately, before any fetch is made.
    handle2.reset();

    // In all cases, that should result in a single fetch being made.
    auto fetch1 = this->WaitForSignalsFetch();

    switch (test_case.request_relation) {
      // Despite these two cases being different internally, they look the same
      // both to the caller and to the created fetches.
      case RequestRelation::kDifferentFetches:
      case RequestRelation::kDifferentCompressionGroups: {
        // Fetch should not be affected by the first (cancelled) fetch, other
        // than including its devtools auction ID in the different compression
        // group case.
        if (test_case.request_relation ==
            RequestRelation::kDifferentCompressionGroups) {
          params1.devtools_auction_ids.insert(
              params2.devtools_auction_ids.begin(),
              params2.devtools_auction_ids.end());
        }
        ValidateFetchParams(fetch1, params1,
                            /*expected_compression_group_id=*/0, partition_id1);
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client1(handle1.get(),
                                              this->cache_mojo_pipe_);
        client1.WaitForSuccess();

        // Make a second request using `params2`. It should result in a new
        // request.
        auto [handle3, partition_id3] = this->RequestTrustedSignals(params2);
        EXPECT_NE(handle1->compression_group_token(),
                  handle3->compression_group_token());
        auto fetch3 = this->WaitForSignalsFetch();
        ValidateFetchParams(fetch3, params2,
                            /*expected_compression_group_id=*/0, partition_id3);
        EXPECT_EQ(
            fetch1.network_partition_nonce == fetch3.network_partition_nonce,
            test_case.expect_same_network_partition_nonce);
        RespondToFetchWithSuccess(
            fetch3,
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        TestTrustedSignalsCacheClient client3(
            handle3.get(), this->CreateOrGetMojoPipeGivenParams(params2));
        client3.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentPartitions: {
        EXPECT_EQ(fetch1.trusted_signals_url, params1.trusted_signals_url);
        ASSERT_EQ(fetch1.compression_groups.size(), 1u);
        EXPECT_EQ(fetch1.compression_groups.begin()->first, 0);
        EXPECT_THAT(fetch1.devtools_auction_ids,
                    testing::UnorderedElementsAre(
                        *params1.devtools_auction_ids.begin(),
                        *params2.devtools_auction_ids.begin()));

        const auto& partitions = fetch1.compression_groups.begin()->second;
        ASSERT_EQ(partitions.size(), 2u);
        ValidateFetchParamsForPartition(partitions[0], params1, partition_id1);
        ValidateFetchParamsForPartition(partitions[1], params2, partition_id2);

        // Respond with a single response for the partition, and read it.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client1(handle1.get(),
                                              this->cache_mojo_pipe_);
        client1.WaitForSuccess();

        // Make a second request using `params2`. It should reuse the response
        // to the initial request.
        auto [handle3, partition_id3] = this->RequestTrustedSignals(params2);
        EXPECT_EQ(handle1->compression_group_token(),
                  handle3->compression_group_token());
        EXPECT_NE(partition_id1, partition_id3);
        EXPECT_EQ(partition_id2, partition_id3);
        TestTrustedSignalsCacheClient client3(handle3.get(),
                                              this->cache_mojo_pipe_);
        client3.WaitForSuccess();
        break;
      }

      case RequestRelation::kSamePartitionModified:
      case RequestRelation::kSamePartitionUnmodified: {
        auto merged_params = this->CreateMergedParams(params1, params2);
        ValidateFetchParams(fetch1, merged_params,
                            /*expected_compression_group_id=*/0, partition_id1);

        // Respond with a single response for the partition, and read it.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client1(handle1.get(),
                                              this->cache_mojo_pipe_);
        client1.WaitForSuccess();

        // Make a second request using `params2`. It should reuse the response
        // to the initial request, including the same partition ID.
        auto [handle3, partition_id3] = this->RequestTrustedSignals(params2);
        EXPECT_EQ(handle1->compression_group_token(),
                  handle3->compression_group_token());
        EXPECT_EQ(partition_id1, partition_id3);

        // For the sake of completeness, read the response again.
        TestTrustedSignalsCacheClient client3(handle3.get(),
                                              this->cache_mojo_pipe_);
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
TYPED_TEST(TrustedSignalsCacheTest,
           DifferentParamsCancelFirstBeforeFetchStart) {
  for (auto& test_case : this->CreateTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    this->CreateCache();
    auto params1 = std::move(test_case.params1);
    auto params2 = std::move(test_case.params2);

    // Don't bother to compare handles here - that's covered by another test.
    auto [handle1, partition_id1] = this->RequestTrustedSignals(params1);
    auto [handle2, partition_id2] = this->RequestTrustedSignals(params2);

    // Cancel the first request immediately, before any fetch is made.
    handle1.reset();

    // In all cases, that should result in a single fetch being made.
    auto fetch1 = this->WaitForSignalsFetch();

    switch (test_case.request_relation) {
        // Despite these two cases being different internally, they look the
        // same both to the caller and to the created fetches.
      case RequestRelation::kDifferentFetches:
      case RequestRelation::kDifferentCompressionGroups: {
        // Fetch should not be affected by the first (cancelled) fetch, other
        // than including its devtools auction ID in the different compression
        // group case.
        if (test_case.request_relation ==
            RequestRelation::kDifferentCompressionGroups) {
          params2.devtools_auction_ids.insert(
              params1.devtools_auction_ids.begin(),
              params1.devtools_auction_ids.end());
        }
        ValidateFetchParams(fetch1, params2,
                            /*expected_compression_group_id=*/0, partition_id2);
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client1(
            handle2.get(), this->CreateOrGetMojoPipeGivenParams(params2));
        client1.WaitForSuccess();

        // Make a second request using `params1`. It should result in a new
        // request.
        auto [handle3, partition_id3] = this->RequestTrustedSignals(params1);
        auto fetch3 = this->WaitForSignalsFetch();
        ValidateFetchParams(fetch3, params1,
                            /*expected_compression_group_id=*/0, partition_id3);
        EXPECT_EQ(
            fetch1.network_partition_nonce == fetch3.network_partition_nonce,
            test_case.expect_same_network_partition_nonce);
        RespondToFetchWithSuccess(
            fetch3,
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        TestTrustedSignalsCacheClient client3(handle3.get(),
                                              this->cache_mojo_pipe_);
        client3.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentPartitions: {
        EXPECT_EQ(fetch1.trusted_signals_url, params2.trusted_signals_url);
        ASSERT_EQ(fetch1.compression_groups.size(), 1u);
        EXPECT_EQ(fetch1.compression_groups.begin()->first, 0);
        EXPECT_THAT(fetch1.devtools_auction_ids,
                    testing::UnorderedElementsAre(
                        *params1.devtools_auction_ids.begin(),
                        *params2.devtools_auction_ids.begin()));

        const auto& partitions = fetch1.compression_groups.begin()->second;
        ASSERT_EQ(partitions.size(), 2u);
        ValidateFetchParamsForPartition(partitions[0], params1, partition_id1);
        ValidateFetchParamsForPartition(partitions[1], params2, partition_id2);

        // Respond with a single response for the partition, and read it.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client1(handle2.get(),
                                              this->cache_mojo_pipe_);
        client1.WaitForSuccess();

        // Make a second request using `params1`. It should reuse the response
        // to the initial request.
        auto [handle3, partition_id3] = this->RequestTrustedSignals(params1);
        EXPECT_EQ(handle2->compression_group_token(),
                  handle3->compression_group_token());
        EXPECT_EQ(partition_id1, partition_id3);
        EXPECT_NE(partition_id2, partition_id3);
        TestTrustedSignalsCacheClient client3(handle3.get(),
                                              this->cache_mojo_pipe_);
        client3.WaitForSuccess();
        break;
      }

      case RequestRelation::kSamePartitionModified:
      case RequestRelation::kSamePartitionUnmodified: {
        auto merged_params = this->CreateMergedParams(params1, params2);
        ValidateFetchParams(fetch1, merged_params,
                            /*expected_compression_group_id=*/0, partition_id1);

        // Respond with a single response for the partition, and read it.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client1(handle2.get(),
                                              this->cache_mojo_pipe_);
        client1.WaitForSuccess();

        // Make a second request using `params1`. It should reuse the response
        // to the initial request, including the same partition ID.
        auto [handle3, partition_id3] = this->RequestTrustedSignals(params1);
        EXPECT_EQ(handle2->compression_group_token(),
                  handle3->compression_group_token());
        EXPECT_EQ(partition_id2, partition_id3);

        // For the sake of completeness, read the response again.
        TestTrustedSignalsCacheClient client3(handle3.get(),
                                              this->cache_mojo_pipe_);
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
TYPED_TEST(TrustedSignalsCacheTest,
           DifferentParamsCancelSecondAfterFetchStart) {
  for (const auto& test_case : this->CreateTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    this->CreateCache();
    const auto& params1 = test_case.params1;
    const auto& params2 = test_case.params2;

    auto [handle1, partition_id1] = this->RequestTrustedSignals(params1);
    auto [handle2, partition_id2] = this->RequestTrustedSignals(params2);

    switch (test_case.request_relation) {
      case RequestRelation::kDifferentFetches: {
        ASSERT_NE(handle1, handle2);
        ASSERT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());
        auto fetches = this->WaitForSignalsFetches(2);

        // fetches are made in FIFO order.
        ValidateFetchParams(fetches[0], params1,
                            /*expected_compression_group_id=*/0, partition_id1);
        ValidateFetchParams(fetches[1], params2,
                            /*expected_compression_group_id=*/0, partition_id2);
        EXPECT_EQ(fetches[0].network_partition_nonce ==
                      fetches[1].network_partition_nonce,
                  test_case.expect_same_network_partition_nonce);

        // Cancel the second request. Its fetcher should be destroyed.
        handle2.reset();
        EXPECT_FALSE(fetches[1].fetcher_alive);

        // Reissue second request, which should start a new fetch.
        auto [handle3, partition_id3] = this->RequestTrustedSignals(params2);
        auto fetch3 = this->WaitForSignalsFetch();
        ValidateFetchParams(fetch3, params2,
                            /*expected_compression_group_id=*/0, partition_id3);
        EXPECT_EQ(fetches[1].network_partition_nonce,
                  fetch3.network_partition_nonce);

        // Make both requests succeed with different bodies, and check that they
        // can be read.
        RespondToFetchWithSuccess(fetches[0]);
        RespondToFetchWithSuccess(
            fetch3,
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        TestTrustedSignalsCacheClient client1(handle1.get(),
                                              this->cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client3(
            handle3.get(), this->CreateOrGetMojoPipeGivenParams(params2));
        client1.WaitForSuccess();
        client3.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentCompressionGroups: {
        EXPECT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());
        auto fetch1 = this->WaitForSignalsFetch();

        EXPECT_EQ(fetch1.trusted_signals_url, params1.trusted_signals_url);
        ASSERT_EQ(fetch1.compression_groups.size(), 2u);

        // Compression groups are appended in FIFO order.
        ASSERT_EQ(1u, fetch1.compression_groups.count(0));
        ValidateFetchParamsForPartitions(fetch1.compression_groups.at(0),
                                         params1, partition_id1);
        ASSERT_EQ(1u, fetch1.compression_groups.count(1));
        ValidateFetchParamsForPartitions(fetch1.compression_groups.at(1),
                                         params2, partition_id2);

        // Cancel the second request. The shared fetcher should not be
        // destroyed.
        base::UnguessableToken compression_group_token2 =
            handle2->compression_group_token();
        handle2.reset();
        EXPECT_TRUE(fetch1.fetcher_alive);

        // Reissue second request, which should start a new fetch.
        auto [handle3, partition_id3] = this->RequestTrustedSignals(params2);
        EXPECT_NE(handle3->compression_group_token(),
                  handle1->compression_group_token());
        EXPECT_NE(handle3->compression_group_token(), compression_group_token2);
        auto fetch3 = this->WaitForSignalsFetch();
        ValidateFetchParams(fetch3, params2,
                            /*expected_compression_group_id=*/0, partition_id3);
        EXPECT_EQ(fetch1.network_partition_nonce,
                  fetch3.network_partition_nonce);

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
        TestTrustedSignalsCacheClient client1(handle1.get(),
                                              this->cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client2(compression_group_token2,
                                              this->cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client3(handle3.get(),
                                              this->cache_mojo_pipe_);
        client1.WaitForSuccess();
        client2.WaitForError(kRequestCancelledError);
        client3.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
            kSomeOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentPartitions: {
        EXPECT_EQ(handle1->compression_group_token(),
                  handle2->compression_group_token());
        EXPECT_NE(partition_id1, partition_id2);
        auto fetch1 = this->WaitForSignalsFetch();

        EXPECT_EQ(fetch1.trusted_signals_url, params1.trusted_signals_url);
        ASSERT_EQ(fetch1.compression_groups.size(), 1u);
        EXPECT_EQ(fetch1.compression_groups.begin()->first, 0);

        const auto& partitions = fetch1.compression_groups.begin()->second;
        ASSERT_EQ(partitions.size(), 2u);
        ValidateFetchParamsForPartition(partitions[0], params1, partition_id1);
        ValidateFetchParamsForPartition(partitions[1], params2, partition_id2);

        // Cancel the second request. The shared fetcher should not be
        // destroyed.
        handle2.reset();
        EXPECT_TRUE(fetch1.fetcher_alive);

        // Reissue second request, which should result in the same signals
        // request ID as the other requests, and the same partition ID as the
        // second request.
        auto [handle3, partition_id3] = this->RequestTrustedSignals(params2);
        EXPECT_EQ(handle1->compression_group_token(),
                  handle3->compression_group_token());
        EXPECT_EQ(partition_id2, partition_id3);

        // Respond with a single response for the partition, and read it - no
        // need for multiple clients, since the handles are the same.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client(handle1.get(),
                                             this->cache_mojo_pipe_);
        client.WaitForSuccess();
        break;
      }

      case RequestRelation::kSamePartitionModified:
      case RequestRelation::kSamePartitionUnmodified: {
        EXPECT_EQ(handle1->compression_group_token(),
                  handle2->compression_group_token());
        EXPECT_EQ(partition_id1, partition_id2);
        auto fetch1 = this->WaitForSignalsFetch();

        auto merged_params = this->CreateMergedParams(params1, params2);
        ValidateFetchParams(fetch1, merged_params,
                            /*expected_compression_group_id=*/0, partition_id1);

        // Cancel the second request. The shared fetcher should not be
        // destroyed.
        handle2.reset();
        EXPECT_TRUE(fetch1.fetcher_alive);

        // Reissue second request, which should result in the same signals
        // request ID and partition ID as the other requests.
        auto [handle3, partition_id3] = this->RequestTrustedSignals(params2);
        EXPECT_EQ(handle1->compression_group_token(),
                  handle3->compression_group_token());
        EXPECT_EQ(partition_id1, partition_id3);

        // Respond with a single request for the partition, and read it - no
        // need for multiple clients, since the handles are the same.
        RespondToFetchWithSuccess(fetch1);
        TestTrustedSignalsCacheClient client(handle1.get(),
                                             this->cache_mojo_pipe_);
        client.WaitForSuccess();
        break;
      }
    }
  }
}

// Tests the case where two requests are made and both are cancelled before the
// requests starts. No fetches should be made, regardless of whether the two
// requests would normally share a fetch or not.
TYPED_TEST(TrustedSignalsCacheTest, DifferentParamsCancelBothBeforeFetchStart) {
  for (const auto& test_case : this->CreateTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    this->CreateCache();
    const auto& params1 = test_case.params1;
    const auto& params2 = test_case.params2;

    auto [handle1, partition_id1] = this->RequestTrustedSignals(params1);
    auto [handle2, partition_id2] = this->RequestTrustedSignals(params2);

    handle1.reset();
    handle2.reset();

    this->task_environment_.RunUntilIdle();
    EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 0u);
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 0u);
  }
}

// Tests the case where two requests are made and both are cancelled after the
// fetch(es) start. The fetch(es) should be cancelled.
TYPED_TEST(TrustedSignalsCacheTest, DifferentParamsCancelBothAfterFetchStart) {
  for (const auto& test_case : this->CreateTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    this->CreateCache();
    const auto& params1 = test_case.params1;
    const auto& params2 = test_case.params2;

    auto [handle1, partition_id1] = this->RequestTrustedSignals(params1);
    auto [handle2, partition_id2] = this->RequestTrustedSignals(params2);

    switch (test_case.request_relation) {
      case RequestRelation::kDifferentFetches: {
        auto fetches = this->WaitForSignalsFetches(2);
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
        auto fetch = this->WaitForSignalsFetch();
        handle1.reset();
        handle2.reset();
        EXPECT_FALSE(fetch.fetcher_alive);
        break;
      }
    }
  }
}

// Test the case where the attempt to get the coordinator key completes
// asynchronously.
TYPED_TEST(TrustedSignalsCacheTest, CoordinatorKeyReceivedAsync) {
  this->trusted_signals_cache_->set_get_coordinator_key_mode(
      TestTrustedSignalsCache::GetCoordinatorKeyMode::kAsync);

  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);
  // Wait for the fetch to be observed and respond to it. No need to spin the
  // message loop, since fetch responses at this layer are passed directly to
  // the cache, and don't go through Mojo, as the TrustedSignalsFetcher is
  // entirely mocked out.
  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id);
  RespondToFetchWithSuccess(fetch);

  TestTrustedSignalsCacheClient client(handle.get(), this->cache_mojo_pipe_);
  client.WaitForSuccess();
}

// Test the case where the attempt to get the coordinator key fails
// synchronously.
TYPED_TEST(TrustedSignalsCacheTest, CoordinatorKeyFailsSync) {
  this->trusted_signals_cache_->set_get_coordinator_key_mode(
      TestTrustedSignalsCache::GetCoordinatorKeyMode::kSyncFail);

  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);
  TestTrustedSignalsCacheClient client(handle.get(), this->cache_mojo_pipe_);
  client.WaitForError(TestTrustedSignalsCache::kKeyFetchFailed);
  // Teardown will check that the fetcher saw no unaccounted for requests.
}

// Test the case where the attempt to get the coordinator key fails
// asynchronously.
TYPED_TEST(TrustedSignalsCacheTest, CoordinatorKeyFailsAsync) {
  this->trusted_signals_cache_->set_get_coordinator_key_mode(
      TestTrustedSignalsCache::GetCoordinatorKeyMode::kAsyncFail);

  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);
  TestTrustedSignalsCacheClient client(handle.get(), this->cache_mojo_pipe_);
  client.WaitForError(TestTrustedSignalsCache::kKeyFetchFailed);
  // Teardown will check that the fetcher saw no unaccounted for requests.
}

// Test the case where the fetch is cancelled while attempting to get the
// coordinator key.
TYPED_TEST(TrustedSignalsCacheTest, CancelledDuringGetCoordinatorKey) {
  for (bool invoke_callback : {false, true}) {
    // Use a fresh cache each time.
    this->CreateCache();
    this->trusted_signals_cache_->set_get_coordinator_key_mode(
        TestTrustedSignalsCache::GetCoordinatorKeyMode::kStashCallback);

    auto params = this->CreateDefaultParams();

    auto [handle, partition_id] = this->RequestTrustedSignals(params);

    auto callback =
        this->trusted_signals_cache_->WaitForCoordinatorKeyCallback();
    handle.reset();
    if (invoke_callback) {
      std::move(callback).Run();
    }

    // Let any pending async callbacks complete.
    this->task_environment_.RunUntilIdle();

    // Teardown will check that the fetcher saw no unaccounted for requests.
  }
}

// Test the case where a second request is made while waiting on the
// coordinator. The requests should behave just as in the
// DifferentParamsBeforeFetchStart test - that is, if the requests can
// theoretically use the same fetch (possibly sharing a partition or compression
// group), they should still be able to do so if one of the requests is started
// while the first request is waiting for the coordinator's key.
TYPED_TEST(TrustedSignalsCacheTest,
           SecondRequestStartedWhileWaitingOnCoordinatorKey) {
  for (const auto& test_case : this->CreateTestCases()) {
    SCOPED_TRACE(test_case.description);

    // Start with a clean slate for each test. Not strictly necessary, but
    // limits what's under test a bit.
    this->CreateCache();
    this->trusted_signals_cache_->set_get_coordinator_key_mode(
        TestTrustedSignalsCache::GetCoordinatorKeyMode::kStashCallback);

    const auto& params1 = test_case.params1;
    const auto& params2 = test_case.params2;

    auto [handle1, partition_id1] = this->RequestTrustedSignals(params1);
    auto callback1 =
        this->trusted_signals_cache_->WaitForCoordinatorKeyCallback();

    auto [handle2, partition_id2] = this->RequestTrustedSignals(params2);

    switch (test_case.request_relation) {
      case RequestRelation::kDifferentFetches: {
        ASSERT_NE(handle1, handle2);
        ASSERT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());

        // There should be separate callbacks in this case.
        auto callback2 =
            this->trusted_signals_cache_->WaitForCoordinatorKeyCallback();

        // Invoke both callbacks, with the usual key (the serialized
        // coordinator).
        std::move(callback1).Run();
        std::move(callback2).Run();

        auto fetches = this->WaitForSignalsFetches(2);

        // fetches are made in FIFO order.
        ValidateFetchParams(fetches[0], params1,
                            /*expected_compression_group_id=*/0, partition_id1);
        ValidateFetchParams(fetches[1], params2,
                            /*expected_compression_group_id=*/0, partition_id2);
        EXPECT_EQ(fetches[0].network_partition_nonce ==
                      fetches[1].network_partition_nonce,
                  test_case.expect_same_network_partition_nonce);

        // Make both requests succeed with different bodies, and check that they
        // can be read.
        RespondToFetchWithSuccess(fetches[0]);
        RespondToFetchWithSuccess(
            fetches[1],
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        TestTrustedSignalsCacheClient client1(handle1.get(),
                                              this->cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client2(
            handle2.get(), this->CreateOrGetMojoPipeGivenParams(params2));
        client1.WaitForSuccess();
        client2.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentCompressionGroups: {
        EXPECT_NE(handle1->compression_group_token(),
                  handle2->compression_group_token());
        std::move(callback1).Run();

        auto fetch = this->WaitForSignalsFetch();

        EXPECT_EQ(fetch.trusted_signals_url, params1.trusted_signals_url);
        ASSERT_EQ(fetch.compression_groups.size(), 2u);

        // Compression groups are appended in FIFO order.
        ASSERT_EQ(1u, fetch.compression_groups.count(0));
        ValidateFetchParamsForPartitions(fetch.compression_groups.at(0),
                                         params1, partition_id1);
        ASSERT_EQ(1u, fetch.compression_groups.count(1));
        ValidateFetchParamsForPartitions(fetch.compression_groups.at(1),
                                         params2, partition_id2);

        // Respond with different results for each compression group.
        RespondToTwoCompressionGroupFetchWithSuccess(fetch);
        TestTrustedSignalsCacheClient client1(handle1.get(),
                                              this->cache_mojo_pipe_);
        TestTrustedSignalsCacheClient client2(handle2.get(),
                                              this->cache_mojo_pipe_);
        client1.WaitForSuccess();
        client2.WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kBrotli,
            kOtherSuccessBody);
        break;
      }

      case RequestRelation::kDifferentPartitions: {
        EXPECT_EQ(handle1->compression_group_token(),
                  handle2->compression_group_token());
        EXPECT_NE(partition_id1, partition_id2);
        std::move(callback1).Run();

        auto fetch = this->WaitForSignalsFetch();

        EXPECT_EQ(fetch.trusted_signals_url, params1.trusted_signals_url);
        ASSERT_EQ(fetch.compression_groups.size(), 1u);
        EXPECT_EQ(fetch.compression_groups.begin()->first, 0);

        const auto& partitions = fetch.compression_groups.begin()->second;
        ASSERT_EQ(partitions.size(), 2u);
        ValidateFetchParamsForPartition(partitions[0], params1, partition_id1);
        ValidateFetchParamsForPartition(partitions[1], params2, partition_id2);

        // Respond with a single response for the partition, and read it - no
        // need for multiple clients, since the handles are the same.
        RespondToFetchWithSuccess(fetch);
        TestTrustedSignalsCacheClient client(handle1.get(),
                                             this->cache_mojo_pipe_);
        client.WaitForSuccess();
        break;
      }

      case RequestRelation::kSamePartitionModified:
      case RequestRelation::kSamePartitionUnmodified: {
        EXPECT_EQ(handle1->compression_group_token(),
                  handle2->compression_group_token());
        EXPECT_EQ(partition_id1, partition_id2);
        std::move(callback1).Run();

        auto fetch = this->WaitForSignalsFetch();

        auto merged_params = this->CreateMergedParams(params1, params2);
        // The fetch exactly match the merged parameters.
        ValidateFetchParams(fetch, merged_params,
                            /*expected_compression_group_id=*/0, partition_id1);

        // Respond with a single response for the partition, and read it - no
        // need for multiple clients, since the handles are the same.
        RespondToFetchWithSuccess(fetch);
        TestTrustedSignalsCacheClient client(handle1.get(),
                                             this->cache_mojo_pipe_);
        client.WaitForSuccess();
        break;
      }
    }
  }
}

// Test the case where the attempt to get the coordinator key is received before
// StartFetch is called on a Handle.
TYPED_TEST(TrustedSignalsCacheTest,
           CoordinatorKeyReceivedBeforeStartFetchCalled) {
  this->trusted_signals_cache_->set_get_coordinator_key_mode(
      TestTrustedSignalsCache::GetCoordinatorKeyMode::kStashCallback);

  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] =
      this->RequestTrustedSignals(params, /*start_fetch=*/false);

  auto callback = this->trusted_signals_cache_->WaitForCoordinatorKeyCallback();
  std::move(callback).Run();

  // No fetch should have been started yet.
  EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 0u);

  // Calling StartFetch() on the handle should immediately trigger a fetch.
  handle->StartFetch();
  EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 1u);

  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id);
  RespondToFetchWithSuccess(fetch);

  TestTrustedSignalsCacheClient client(handle.get(), this->cache_mojo_pipe_);
  client.WaitForSuccess();
}

// Check that requesting signals over a pipe with the wrong `script_origin`
// results in the pipe being closed, with nothing sent.
TYPED_TEST(TrustedSignalsCacheTest, RequestWithWrongScriptOrigin) {
  mojo::test::BadMessageObserver bad_message_observer;
  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);
  auto fetch = this->WaitForSignalsFetch();

  // Create a remote associated with some other origin.
  mojo::Remote<auction_worklet::mojom::TrustedSignalsCache> remote(
      this->CreateMojoPendingRemoteForOrigin(
          url::Origin::Create(GURL("https://not.seller.or.buyer.test"))));

  // Trying to use the remote to get data from another origin's request should
  // result in a bad message and the TrustedSignalsCache pipe being closed.
  TestTrustedSignalsCacheClient client(handle.get(), remote);
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Data from wrong compression group requested.");
  remote.FlushForTesting();
  EXPECT_FALSE(remote.is_connected());

  // The client pipe should also have been closed without receiving any data.
  EXPECT_TRUE(client.IsReceiverDisconnected());
  EXPECT_FALSE(client.has_result());
}

// Check that requesting signals over a pipe with the wrong signals type
// results in the pipe being closed, with nothing sent.
TYPED_TEST(TrustedSignalsCacheTest, RequestWithWrongSignalsType) {
  mojo::test::BadMessageObserver bad_message_observer;
  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);
  auto fetch = this->WaitForSignalsFetch();

  TrustedSignalsCacheImpl::SignalsType wrong_signals_type;
  if constexpr (std::is_same<TypeParam, BiddingParams>::value) {
    wrong_signals_type = TrustedSignalsCacheImpl::SignalsType::kScoring;
  } else {
    wrong_signals_type = TrustedSignalsCacheImpl::SignalsType::kBidding;
  }

  // Create a remote associated with the right origin, but wrong signals type.
  mojo::Remote<auction_worklet::mojom::TrustedSignalsCache> remote(
      this->trusted_signals_cache_->CreateRemote(wrong_signals_type,
                                                 params.script_origin));

  // Trying to use the remote to get data using the wrong type should result in
  // a bad message and the TrustedSignalsCache pipe being closed.
  TestTrustedSignalsCacheClient client(handle.get(), remote);
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Data from wrong compression group requested.");
  remote.FlushForTesting();
  EXPECT_FALSE(remote.is_connected());

  // The client pipe should also have been closed without receiving any data.
  EXPECT_TRUE(client.IsReceiverDisconnected());
  EXPECT_FALSE(client.has_result());
}

// Tests the case of merging multiple requests with the same FetchKey. This test
// serves to make sure that when there are multiple outstanding fetches, the
// last fetch can be modified as long as it has not started.
using TrustedBiddingSignalsCacheTest = TrustedSignalsCacheTest<BiddingParams>;
TEST_F(TrustedBiddingSignalsCacheTest, MultipleRequestsSameCacheKey) {
  // Start request and wait for its fetch.
  auto params1 = this->CreateDefaultParams();
  auto [handle1, partition_id1] = this->RequestTrustedSignals(params1);
  auto fetch1 = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch1, params1, /*expected_compression_group_id=*/0,
                      partition_id1);

  // Start another request with same CacheKey as the first, but that can't be
  // merged into the first request, since it has a live fetch.
  auto params2 = this->CreateDefaultParams();
  params2.trusted_bidding_signals_keys = {{"othey_key2"}};
  auto [handle2, partition_id2] = this->RequestTrustedSignals(params2);
  EXPECT_NE(handle1->compression_group_token(),
            handle2->compression_group_token());

  // Create another fetch with the default set of parameters. It's merged into
  // the second request, not the first. This is because the first and second
  // request have the same cache key, so the second request overwrite the cache
  // key of the first, though its compression group ID should still be valid.
  auto [handle3, partition_id3] = this->RequestTrustedSignals(params1);
  EXPECT_EQ(handle2->compression_group_token(),
            handle3->compression_group_token());
  EXPECT_EQ(partition_id2, partition_id3);

  // Wait for the combined fetch.
  auto fetch2 = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch2, CreateMergedParams(params2, params1),
                      /*expected_compression_group_id=*/0, partition_id2);

  // Reissuing a request with either previous set of params should reuse the
  // partition shared by the second and third fetches.
  auto [handle4, partition_id4] = this->RequestTrustedSignals(params1);
  EXPECT_EQ(handle2->compression_group_token(),
            handle4->compression_group_token());
  EXPECT_EQ(partition_id2, partition_id4);
  auto [handle5, partition_id5] = this->RequestTrustedSignals(params2);
  EXPECT_EQ(handle2->compression_group_token(),
            handle5->compression_group_token());
  EXPECT_EQ(partition_id2, partition_id5);

  // Complete second fetch before first, just to make sure there's no
  // expectation about completion order here.
  RespondToFetchWithSuccess(fetch2);
  TestTrustedSignalsCacheClient client2(handle2.get(), this->cache_mojo_pipe_);
  client2.WaitForSuccess();

  RespondToFetchWithSuccess(
      fetch1, auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
      kSomeOtherSuccessBody);
  TestTrustedSignalsCacheClient client1(handle1.get(), this->cache_mojo_pipe_);
  client1.WaitForSuccess(
      auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone,
      kSomeOtherSuccessBody);
}

TYPED_TEST(TrustedSignalsCacheTest, NonceCache) {
  auto params = this->CreateDefaultParams();
  std::array<base::UnguessableToken, TrustedSignalsCacheImpl::kNonceCacheSize>
      nonces;
  // Fill nonce cache, make sure all nonces are unique.
  for (uint32_t i = 0; i < TrustedSignalsCacheImpl::kNonceCacheSize; ++i) {
    params.trusted_signals_url =
        GURL(base::StringPrintf("https://%u.test/signals", i));
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    auto fetch = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                        partition_id);
    nonces[i] = fetch.network_partition_nonce;
    for (uint32_t j = 0; j < i; ++j) {
      EXPECT_NE(nonces[j], nonces[i]);
    }
  }

  // Create requests reusing signals URLs. Make sure all nonces are reused, as
  // expected.
  for (uint32_t i = 0; i < TrustedSignalsCacheImpl::kNonceCacheSize; ++i) {
    params.trusted_signals_url =
        GURL(base::StringPrintf("https://%u.test/signals", i));
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    auto fetch = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                        partition_id);
    EXPECT_EQ(fetch.network_partition_nonce, nonces[i]);
  }

  // Make request with a unique trusted signals URL. It should not reuse any of
  // the existing tokens. This should result in the oldest token (for
  // https://0.test) being evicted.
  params.trusted_signals_url = GURL("https://unique.test/signals");
  auto [handle_unique, partition_id_unique] =
      this->RequestTrustedSignals(params);
  auto fetch_unique = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch_unique, params, /*expected_compression_group_id=*/0,
                      partition_id_unique);
  base::UnguessableToken nonce_unique = fetch_unique.network_partition_nonce;
  for (auto nonce : nonces) {
    EXPECT_NE(nonce_unique, nonce);
  }

  // Check all tokens but the first are still be in the cache.
  for (uint32_t i = 1; i < TrustedSignalsCacheImpl::kNonceCacheSize; ++i) {
    params.trusted_signals_url =
        GURL(base::StringPrintf("https://%u.test/signals", i));
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    auto fetch = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                        partition_id);
    EXPECT_EQ(fetch.network_partition_nonce, nonces[i]);
  }

  // Token for "https://0.test" should have been evicted. A new fetch for it
  // should now use a new nonce.
  params.trusted_signals_url = GURL("https://0.test/signals");
  auto [handle0, partition_id0] = this->RequestTrustedSignals(params);
  auto fetch0 = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch0, params, /*expected_compression_group_id=*/0,
                      partition_id0);
  EXPECT_NE(fetch0.network_partition_nonce, nonce_unique);
  for (auto nonce : nonces) {
    EXPECT_NE(fetch0.network_partition_nonce, nonce);
  }
}

// Make sure bidding and scoring requests to the same URL don't share fetches,
// and use their own nonces.
TYPED_TEST(TrustedSignalsCacheTest, DifferentTypes) {
  auto bidding_params = this->CreateDefaultBiddingParams();
  auto scoring_params = this->CreateDefaultScoringParams();
  // Use the same script origin, so the keys end up being largely the same,
  // other than signals type.
  scoring_params.script_origin = bidding_params.script_origin;
  // By default, bidding and scoring parameters use different URLs, but need to
  // use the same ones for this test.
  scoring_params.trusted_signals_url = bidding_params.trusted_signals_url;

  std::unique_ptr<TestTrustedSignalsCache::Handle> bidding_handle;
  int bidding_partition_id;
  std::unique_ptr<TestTrustedSignalsCache::Handle> scoring_handle;
  int scoring_partition_id;
  // Vary which request is made first depending on the templatization of the
  // test.
  if constexpr (std::is_same<TypeParam, BiddingParams>::value) {
    std::tie(bidding_handle, bidding_partition_id) =
        this->RequestTrustedSignals(bidding_params);
    std::tie(scoring_handle, scoring_partition_id) =
        this->RequestTrustedSignals(scoring_params);
  } else {
    std::tie(scoring_handle, scoring_partition_id) =
        this->RequestTrustedSignals(scoring_params);
    std::tie(bidding_handle, bidding_partition_id) =
        this->RequestTrustedSignals(bidding_params);
  }
  EXPECT_NE(bidding_handle, scoring_handle);

  // There should be two fetches.
  auto bidding_fetch =
      this->trusted_signals_cache_->WaitForBiddingSignalsFetch();
  ValidateFetchParams(bidding_fetch, bidding_params,
                      /*expected_compression_group_id=*/0,
                      bidding_partition_id);
  auto scoring_fetch =
      this->trusted_signals_cache_->WaitForScoringSignalsFetch();
  ValidateFetchParams(scoring_fetch, scoring_params,
                      /*expected_compression_group_id=*/0,
                      scoring_partition_id);

  // And the fetches should not share nonces.
  EXPECT_NE(bidding_fetch.network_partition_nonce,
            scoring_fetch.network_partition_nonce);
}

// Test the cache size limit in the case Handles are not kept alive.
TYPED_TEST(TrustedSignalsCacheTest, SizeLimitReachedNoLiveHandles) {
  // A body size large enough so that only 10 entries may be in the cache. This
  // doesn't take into account overhead due to fields other than the body
  // contributing to the total size - that's assumed to be less than
  // TrustedSignalsCacheImpl::kMaxCacheSizeBytes / 11 for each entry.
  const size_t kEntrySize =
      TrustedSignalsCacheImpl::kMaxCacheSizeBytes / 11 + 1;
  auto params = this->CreateDefaultParams();
  for (size_t i = 0; i < 10u; ++i) {
    params.trusted_signals_url = CreateUrl(i);
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    auto fetch = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                        partition_id);
    RespondToFetchWithSuccess(
        fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
        CreateString(i, kEntrySize));
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 1 + i);
    // The cache size limit should never be exceeded.
    EXPECT_LT(this->trusted_signals_cache_->size_for_testing(),
              TrustedSignalsCacheImpl::kMaxCacheSizeBytes);
  }

  // Start an 11th request, which corresponds to `i = 10`, since numbering
  // starts at 0. Nothing should be evicted yet.
  params.trusted_signals_url = CreateUrl(10);
  auto [handle10, partition_id10] = this->RequestTrustedSignals(params);
  auto fetch10 = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch10, params, /*expected_compression_group_id=*/0,
                      partition_id10);
  // 11 entries are in the cache, but only 10 have bodies.
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 11u);
  EXPECT_LT(this->trusted_signals_cache_->size_for_testing(),
            TrustedSignalsCacheImpl::kMaxCacheSizeBytes);

  // Check that all previous entries are still in the cache.
  for (size_t i = 0; i < 10u; ++i) {
    params.trusted_signals_url = CreateUrl(i);
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    TestTrustedSignalsCacheClient(handle.get(), this->cache_mojo_pipe_)
        .WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
            CreateString(i, kEntrySize));
  }

  // Respond to fetch 10. This should evict the entry least recently closed
  // entry, which is entry 0.
  RespondToFetchWithSuccess(
      fetch10, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      CreateString(10, kEntrySize));
  // Close handle, to avoid impacting eviction logic.
  handle10.reset();
  // Entry 0 should have been evicted.
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 10u);
  EXPECT_LT(this->trusted_signals_cache_->size_for_testing(),
            TrustedSignalsCacheImpl::kMaxCacheSizeBytes);

  // Trying to request entry 0 should trigger a new fetch, without evicting
  // anything.
  params.trusted_signals_url = CreateUrl(0);
  auto [handle0, partition_id0] = this->RequestTrustedSignals(params);
  auto fetch0 = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch0, params, /*expected_compression_group_id=*/0,
                      partition_id0);
  // 11 entries should be in the cache again, but entry 0 should have no body
  // yet.
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 11u);
  EXPECT_LT(this->trusted_signals_cache_->size_for_testing(),
            TrustedSignalsCacheImpl::kMaxCacheSizeBytes);

  // Check that all other entries are still in the cache. Check in reverse
  // order, so that entry 10 is the least recently accessed, and thus will
  // be the one that's evicted when a new entry is added. This serves to
  // double-check that eviction order is based on time last used, rather than on
  // creation time.
  for (size_t i = 10u; i >= 1u; --i) {
    params.trusted_signals_url = CreateUrl(i);
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    TestTrustedSignalsCacheClient(handle.get(), this->cache_mojo_pipe_)
        .WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
            CreateString(i, kEntrySize));
  }

  // Respond to the second fetch for entry 0. This should evict the entry least
  // recently closed entry, which is entry 10.
  RespondToFetchWithSuccess(
      fetch0, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      CreateString(0, kEntrySize));
  // Close handle for entry 0.
  handle0.reset();

  // Entry 10 should have been evicted.
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 10u);
  EXPECT_LT(this->trusted_signals_cache_->size_for_testing(),
            TrustedSignalsCacheImpl::kMaxCacheSizeBytes);

  // Trying to request entry 10 again should trigger a new fetch, without
  // evicting anything.
  params.trusted_signals_url = CreateUrl(10);
  auto [handle10_2, partition_id10_2] = this->RequestTrustedSignals(params);
  auto fetch10_2 = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch10_2, params, /*expected_compression_group_id=*/0,
                      partition_id10_2);
  // 11 entries should be in the cache again.
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 11u);

  // Check that entries 0 through 9 are all in the cache.
  for (size_t i = 0; i < 10u; ++i) {
    params.trusted_signals_url = CreateUrl(i);
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    TestTrustedSignalsCacheClient(handle.get(), this->cache_mojo_pipe_)
        .WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
            CreateString(i, kEntrySize));
  }
}

// Test the cache size limit in the case Handles are kept alive. The size limit
// may be exceeded as long as there are live Handles.
TYPED_TEST(TrustedSignalsCacheTest, SizeLimitReachedLiveHandles) {
  // A body size large enough so that only 10 entries may be in the cache. This
  // doesn't take into account overhead due to fields other than the body
  // contributing to the total size - that's assumed to be less than
  // TrustedSignalsCacheImpl::kMaxCacheSizeBytes / 11 for each entry.
  const size_t kEntrySize =
      TrustedSignalsCacheImpl::kMaxCacheSizeBytes / 11 + 1;
  auto params = this->CreateDefaultParams();
  std::vector<std::unique_ptr<TestTrustedSignalsCache::Handle>> handles;
  // Make 15 entries.  All should remain in the cache as long as they're live,
  // though the cache limit will be exceeded.
  for (size_t i = 0; i < 15u; ++i) {
    params.trusted_signals_url = CreateUrl(i);
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    auto fetch = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                        partition_id);
    RespondToFetchWithSuccess(
        fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
        CreateString(i, kEntrySize));
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 1 + i);
    handles.emplace_back(std::move(handle));
  }

  // Delete all the Handles that exceeded the limit. The underlying cache data
  // should be destroyed as soon as they're released.
  for (size_t i = 14u; i >= 10; --i) {
    // There should be `i` entries in the cache, and the size limit should be
    // exceeded.
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), i + 1);
    EXPECT_GT(this->trusted_signals_cache_->size_for_testing(),
              TrustedSignalsCacheImpl::kMaxCacheSizeBytes);

    // Destroying a Handle should immediately destroy the entry, as long as the
    // max size is exceeded.
    handles.pop_back();
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), i);
  }

  // Deleting the other Handles shouldn't result in removing anything.
  while (!handles.empty()) {
    handles.pop_back();
    EXPECT_LT(this->trusted_signals_cache_->size_for_testing(),
              TrustedSignalsCacheImpl::kMaxCacheSizeBytes);
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 10u);
  }

  // Check that entries 0 through 9 are all in the cache.
  for (size_t i = 0; i < 10u; ++i) {
    params.trusted_signals_url = CreateUrl(i);
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    TestTrustedSignalsCacheClient(handle.get(), this->cache_mojo_pipe_)
        .WaitForSuccess(
            auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
            CreateString(i, kEntrySize));
  }

  // Check that entries 10 through 14 are not in the cache, by checking that
  // requesting them triggers network requests.
  for (size_t i = 10; i < 15u; ++i) {
    params.trusted_signals_url = CreateUrl(i);
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    auto fetch = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                        partition_id);
  }
}

TYPED_TEST(TrustedSignalsCacheTest, SingleEntryExceedsSizeLimit) {
  // Value above the maximum allowed size.
  const size_t kLargeEntrySize =
      TrustedSignalsCacheImpl::kMaxCacheSizeBytes + 1;

  // Add a single entry exceeding the maximum size, and mmake sure it can be
  // retrieved.
  auto params = this->CreateDefaultParams();
  auto [handle, partition_id] = this->RequestTrustedSignals(params);
  auto fetch = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                      partition_id);
  RespondToFetchWithSuccess(
      fetch, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      CreateString(0, kLargeEntrySize));
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 1u);
  EXPECT_GT(this->trusted_signals_cache_->size_for_testing(),
            TrustedSignalsCacheImpl::kMaxCacheSizeBytes);
  TestTrustedSignalsCacheClient(handle.get(), this->cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
          CreateString(0, kLargeEntrySize));

  // Destroying the Handle should result in the entry being immediately evicted,
  // even though nothing else is in the cache.
  handle.reset();
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 0u);
  EXPECT_EQ(this->trusted_signals_cache_->size_for_testing(), 0u);

  // Re-requesting the data should result in a new fetch, since it was evicted.
  auto [handle2, partition_id2] = this->RequestTrustedSignals(params);
  auto fetch2 = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch2, params, /*expected_compression_group_id=*/0,
                      partition_id2);
}

// Test that when there are multiple Handles, entries are kept around until all
// of them are destroyed.
TYPED_TEST(TrustedSignalsCacheTest, MultipleLiveHandles) {
  // Size large enough that two entries will exceed the limit.
  const size_t kEntrySize = TrustedSignalsCacheImpl::kMaxCacheSizeBytes / 2 + 1;

  auto params1 = this->CreateDefaultParams();
  auto params2 = this->CreateDefaultParams();
  params2.trusted_signals_url = CreateUrl(2);

  // Make a request using the first set of parameters, and provide a response.
  auto [handle1_1, partition_id1_1] = this->RequestTrustedSignals(params1);
  auto fetch1_1 = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch1_1, params1, /*expected_compression_group_id=*/0,
                      partition_id1_1);
  RespondToFetchWithSuccess(
      fetch1_1, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      CreateString(0, kEntrySize));
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 1u);

  // Make a request using the second set of parameters, and provide a response.
  auto [handle2_1, partition_id2_1] = this->RequestTrustedSignals(params2);
  auto fetch2_1 = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch2_1, params2, /*expected_compression_group_id=*/0,
                      partition_id2_1);
  RespondToFetchWithSuccess(
      fetch2_1, auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
      CreateString(1, kEntrySize));
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 2u);

  // Send a second request using each set of parameters. The responses should be
  // reused.
  auto [handle1_2, partition_id1_2] = this->RequestTrustedSignals(params1);
  EXPECT_EQ(handle1_1->compression_group_token(),
            handle1_2->compression_group_token());
  EXPECT_EQ(partition_id1_1, partition_id1_2);
  auto [handle2_2, partition_id2_2] = this->RequestTrustedSignals(params2);
  EXPECT_EQ(handle2_1->compression_group_token(),
            handle2_2->compression_group_token());
  EXPECT_EQ(partition_id2_1, partition_id2_2);
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 2u);
  EXPECT_EQ(this->trusted_signals_cache_->num_pending_fetches(), 0u);

  // Destroy the original two Handles. The data should be retained.
  handle1_1.reset();
  handle2_1.reset();
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 2u);

  // Verify the responses are still cached.
  TestTrustedSignalsCacheClient(handle1_2.get(), this->cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
          CreateString(0, kEntrySize));
  TestTrustedSignalsCacheClient(handle2_2.get(), this->cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
          CreateString(1, kEntrySize));

  // Destroy the second Handle for request 2. This should result in it being
  // removed from the cache, since the size of the two entries combined exceeds
  // the maximum size.
  handle2_2.reset();
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 1u);
  // Verify the first response is still in the cache.
  TestTrustedSignalsCacheClient(handle1_2.get(), this->cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
          CreateString(0, kEntrySize));

  // Making another request with `params2` should result in a new fetch.
  auto [handle2_3, partition_id2_3] = this->RequestTrustedSignals(params2);
  auto fetch2_3 = this->WaitForSignalsFetch();
  ValidateFetchParams(fetch2_3, params2, /*expected_compression_group_id=*/0,
                      partition_id2_3);

  // Destroy the second Handle for request 1. The data should not be evicted
  // from the cache, since it's the only thing in the cache, and does not exceed
  // the maximum size.
  handle1_2.reset();
  // Note that this includes the pending fetch for the second group.
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 2u);

  // Verify the first response is still in the cache by re-requesting it, and
  // retrieving it.
  auto [handle1_3, partition_id1_3] = this->RequestTrustedSignals(params1);
  TestTrustedSignalsCacheClient(handle1_3.get(), this->cache_mojo_pipe_)
      .WaitForSuccess(
          auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip,
          CreateString(0, kEntrySize));
}

// Test that a single entry is still usable after the Handle has been destroyed
// for less than `kMinUnusedCleanupTime` or `kMaxUnusedCleanupTime`. The latter
// won't always be the case - the guarantee is just that unused entries will be
// cleaned sometime between `kMinUnusedCleanupTime` and `kMaxUnusedCleanupTime`,
// but when there's a single entry, it will be expired at exactly
// `kMaxUnusedCleanupTime`.
//
// Also, wait a variable amount of time before destroying the Handle, to make
// sure that the timer doesn't start until the Handle is released
TYPED_TEST(TrustedSignalsCacheTest, CleanupTimerBeforeTimerTriggers) {
  auto params = this->CreateDefaultParams();

  for (base::TimeDelta time_to_wait_before_destroying_handle :
       {base::TimeDelta(), TrustedSignalsCacheImpl::kMinUnusedCleanupTime,
        TrustedSignalsCacheImpl::kMaxUnusedCleanupTime,
        20 * TrustedSignalsCacheImpl::kMaxUnusedCleanupTime}) {
    for (base::TimeDelta time_to_wait_after_destroying_handle :
         {TrustedSignalsCacheImpl::kMinUnusedCleanupTime - kTinyTime,
          TrustedSignalsCacheImpl::kMaxUnusedCleanupTime - kTinyTime}) {
      // Start with a clean slate for each test.
      this->CreateCache();

      auto [handle, partition_id] = this->RequestTrustedSignals(params);
      auto fetch = this->WaitForSignalsFetch();
      ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                          partition_id);
      RespondToFetchWithSuccess(fetch);
      EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 1u);

      // Destroy the Handle and wait for `time_to_wait`, get a new Handle, and
      // verify the data is accessible. Do this several times, to check that
      // accessing the entry resets the timer.
      for (int i = 0; i < 5; ++i) {
        this->task_environment_.FastForwardBy(
            time_to_wait_before_destroying_handle);
        handle.reset();
        this->task_environment_.FastForwardBy(
            time_to_wait_after_destroying_handle);
        EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 1u);

        // Check the result is still usable.
        handle = std::move(this->RequestTrustedSignals(params).first);
        TestTrustedSignalsCacheClient(handle.get(), this->cache_mojo_pipe_)
            .WaitForSuccess();
      }
    }
  }
}

// Test that the cleanup timer triggers at exactly `kMaxUnusedCleanupTime` after
// a lone entry has been destroyed. Adding multiple entries would potentially
// affect when the timeout timer triggers.
TYPED_TEST(TrustedSignalsCacheTest, CleanupTimerTriggers) {
  auto params = this->CreateDefaultParams();

  for (base::TimeDelta time_to_wait_before_destroying_handle :
       {base::TimeDelta(), TrustedSignalsCacheImpl::kMinUnusedCleanupTime,
        TrustedSignalsCacheImpl::kMaxUnusedCleanupTime,
        20 * TrustedSignalsCacheImpl::kMaxUnusedCleanupTime}) {
    // Start with a clean slate for each test.
    this->CreateCache();

    auto [handle1, partition_id1] = this->RequestTrustedSignals(params);
    auto fetch1 = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch1, params, /*expected_compression_group_id=*/0,
                        partition_id1);
    RespondToFetchWithSuccess(fetch1);
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 1u);

    // Destroy the Handle and wait for `time_to_wait_before_destroying_handle`.
    // The timer should only start after the Handle has been destroyed.
    this->task_environment_.FastForwardBy(
        time_to_wait_before_destroying_handle);
    handle1.reset();
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 1u);

    // Wait until just before the timer triggers. The compression group should
    // still exist. Can't request it through a new Handle, since that would
    // extend the lifetime. Could access it through a cached compression group
    // token with no live Handle, but that's not a guaranteed part of the API.
    this->task_environment_.FastForwardBy(
        TrustedSignalsCacheImpl::kMaxUnusedCleanupTime - kTinyTime);
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 1u);

    // Check that the data is destroyed on schedule, and that trying to access
    // it results in a new fetch.
    this->task_environment_.FastForwardBy(kTinyTime);
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 0u);
    auto [handle2, partition_id2] = this->RequestTrustedSignals(params);
    auto fetch2 = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch2, params, /*expected_compression_group_id=*/0,
                        partition_id2);
  }
}

// Add several entries within `kCleanupInterval` of each other, and test that
// the cleanup timer clears them all at once.
TYPED_TEST(TrustedSignalsCacheTest, CleanupTimerCleansMultipleEntries) {
  auto params = this->CreateDefaultParams();

  // Create 3 requests within `kCleanupInterval` of each other. Destroy each
  // Handle immediately after providing the response.
  for (size_t i = 0u; i < 3u; ++i) {
    SCOPED_TRACE(i);

    // It doesn't matter if time is fast forwarded or not before the first entry
    // is added.
    if (i > 0u) {
      this->task_environment_.FastForwardBy(
          TrustedSignalsCacheImpl::kCleanupInterval / 2);
    }

    params.trusted_signals_url = CreateUrl(i);
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    auto fetch = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                        partition_id);
    RespondToFetchWithSuccess(fetch);
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), i + 1);
  }

  // Wait until just before `kMaxUnusedCleanupTime` (which is `kCleanupInterval`
  // + `kMinUnusedCleanupTime`)  has passed since the first request's Handle was
  // destroyed. The cleanup timer should not have triggered yet.
  this->task_environment_.FastForwardBy(
      TrustedSignalsCacheImpl::kMinUnusedCleanupTime - kTinyTime);
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 3u);

  // Wait just long enough for the cleanup timer to trigger, which should delete
  // all entries.
  this->task_environment_.FastForwardBy(kTinyTime);
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 0u);

  // Verify no entries were in the cache by making sure re-requesting them
  // triggers a signals fetch.
  for (size_t i = 0u; i < 3u; ++i) {
    SCOPED_TRACE(i);
    params.trusted_signals_url = CreateUrl(i);
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    auto fetch = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                        partition_id);
  }
}

// Add several entries that each expire just after `kMaxUnusedCleanupTime` has
// passed since the previous entry has expired, so when the cleanup timer
// triggers, only a single entry will be destroyed.
TYPED_TEST(TrustedSignalsCacheTest,
           CleanupTimerCleansSingleEntriesSuccessively) {
  // Near the smallest time between Handle destruction of different entries
  // where the cleanup timer for the previous entry won't destroy the next one
  // as well.
  const base::TimeDelta kTimeDelta =
      TrustedSignalsCacheImpl::kCleanupInterval + kTinyTime;
  auto params = this->CreateDefaultParams();

  // Create 3 requests `kTimeDelta` apart. Destroy each Handle immediately after
  // providing the response.
  for (size_t i = 0u; i < 3u; ++i) {
    SCOPED_TRACE(i);

    // It doesn't matter if time is fast forwarded or not before the first entry
    // is added.
    if (i > 0u) {
      this->task_environment_.FastForwardBy(kTimeDelta);
    }

    params.trusted_signals_url = CreateUrl(i);
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    auto fetch = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                        partition_id);
    RespondToFetchWithSuccess(fetch);
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), i + 1);
  }

  // Wait until just before `kMinUnusedCleanupTime - kTinyTime`  has passed
  // since the first request's Handle was destroyed. At this point, the first
  // entry should be removed at `kTimeDelta`, and each of the other two should
  // be removed `kTimeDelta` after the previous entry.
  this->task_environment_.FastForwardBy(
      TrustedSignalsCacheImpl::kMinUnusedCleanupTime - 2 * kTimeDelta -
      kTinyTime);
  EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 3u);

  // Wait for each entry to be destroyed, one-by-one, and check that a new
  // request for it will indeed trigger a fetch. Can't check that other entries
  // are still in the cache, other than probing `num_groups_for_testing()`,
  // since actually creating a new Handle and fetching the response will extend
  // the life of the response in the cache. Could cache the compression group
  // tokens and partition IDs the Handle before destroying it the first time,
  // but prefer not to relying on IDs persisting and being fetchable when there
  // are no matching Handles.
  for (size_t i = 0; i < 3u; ++i) {
    SCOPED_TRACE(i);
    this->task_environment_.FastForwardBy(
        TrustedSignalsCacheImpl::kCleanupInterval);
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 3u - i);
    this->task_environment_.FastForwardBy(kTinyTime);
    EXPECT_EQ(this->trusted_signals_cache_->num_groups_for_testing(), 2u - i);

    params.trusted_signals_url = CreateUrl(i);
    auto [handle, partition_id] = this->RequestTrustedSignals(params);
    auto fetch = this->WaitForSignalsFetch();
    ValidateFetchParams(fetch, params, /*expected_compression_group_id=*/0,
                        partition_id);
  }
}

}  // namespace
}  // namespace content
