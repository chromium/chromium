// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_KVV2_HELPER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_KVV2_HELPER_H_

#include <stdint.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/cbor/values.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom-shared.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_client.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

// Encapsulates the logic for generating trusted signals key-value version 2
// requests.
// TODO(crbug.com/349651946): Remove after KVv2 is migrated to browser process.

namespace auction_worklet {

inline constexpr char kTrustedSignalsKVv2EncryptionRequestMediaType[] =
    "message/ad-auction-trusted-signals-request";

inline constexpr char kTrustedSignalsKVv2EncryptionResponseMediaType[] =
    "message/ad-auction-trusted-signals-response";

class CONTENT_EXPORT TrustedSignalsKVv2RequestHelper {
 public:
  explicit TrustedSignalsKVv2RequestHelper(
      std::string post_request_body,
      quiche::ObliviousHttpRequest::Context context);

  TrustedSignalsKVv2RequestHelper(TrustedSignalsKVv2RequestHelper&&);
  TrustedSignalsKVv2RequestHelper& operator=(TrustedSignalsKVv2RequestHelper&&);

  TrustedSignalsKVv2RequestHelper& operator=(
      const TrustedSignalsKVv2RequestHelper&) = delete;
  TrustedSignalsKVv2RequestHelper(const TrustedSignalsKVv2RequestHelper&) =
      delete;

  ~TrustedSignalsKVv2RequestHelper();

  std::string TakePostRequestBody();

  quiche::ObliviousHttpRequest::Context TakeOHttpRequestContext();

 private:
  std::string post_request_body_;
  // Save request's encryption context for later decryption usage.
  quiche::ObliviousHttpRequest::Context context_;
};

// A single-use class within `TrustedSignalsRequestManager` is designed to
// gather interest group names, bidding keys, render URLs, and ad component URLs
// for trusted bidding or scoring signals. It encodes this information into CBOR
// format as the POST request body. All data will be structured into a
// `TrustedSignalsKVv2RequestHelper`.
//
// TODO(crbug.com/337917489): Consider to add a cache for compression group id
// to handle missing compression group in response cases.
class CONTENT_EXPORT TrustedSignalsKVv2RequestHelperBuilder {
 public:
  TrustedSignalsKVv2RequestHelperBuilder& operator=(
      const TrustedSignalsKVv2RequestHelperBuilder&);
  TrustedSignalsKVv2RequestHelperBuilder(
      const TrustedSignalsKVv2RequestHelperBuilder&);

  virtual ~TrustedSignalsKVv2RequestHelperBuilder();

  // Used in trusted signals requests to store the partition and compression
  // group it belongs to, as partition IDs can be duplicated across multiple
  // compression groups.
  struct CONTENT_EXPORT IsolationIndex {
    int compression_group_id;
    int partition_id;

    auto operator<=>(const IsolationIndex&) const = default;
  };

  // Build the request helper using the helper builder to construct the POST
  // body string, noting that the partition IDs will not be sequential for
  // bidding signals.
  std::unique_ptr<TrustedSignalsKVv2RequestHelper> Build();

 protected:
  TrustedSignalsKVv2RequestHelperBuilder(
      std::string hostname,
      std::optional<int> experiment_group_id,
      mojom::TrustedSignalsPublicKeyPtr public_key);

  // All the data needed to request a particular bidding or scoring signals
  // partition.
  struct Partition {
    Partition();
    // Create a new partition for bidding signals based on interest group's
    // name, bidding keys, hostname, experiment group id and slot size
    // parameter.
    Partition(int partition_id,
              const std::string& interest_group_name,
              const std::set<std::string>& bidding_keys,
              const std::string& hostname,
              const std::optional<int>& experiment_group_id,
              std::pair<std::string, std::string>
                  trusted_bidding_signals_slot_size_param);

    // Create a new partition for scoring signals based on render url
    // name, ad component render urls, hostname and experiment group id.
    Partition(int partition_id,
              const std::string& render_url,
              const std::set<std::string>& ad_component_render_urls,
              const std::string& hostname,
              const std::optional<int>& experiment_group_id);
    Partition(Partition&&);
    ~Partition();
    Partition& operator=(Partition&&);

    int partition_id;

    // Parameters for building a bidding signals URL.
    std::set<std::string> interest_group_names;
    std::set<std::string> bidding_signals_keys;

    // Parameters for building a scoring signals URL.
    std::set<std::string> render_urls;
    std::set<std::string> ad_component_render_urls;

    // Valid keys are "hostname", "experimentGroupId",
    // "slotSize", and "allSlotsRequestedSizes".
    base::Value::Dict additional_params;
  };

  // A map of partition IDs to partition to indicate a compression group.
  using CompressionGroup = std::map<int, Partition>;

  std::map<int, CompressionGroup>& compression_groups() {
    return compression_groups_;
  }

  const std::string& hostname() const { return hostname_; }

  const std::optional<int>& experiment_group_id() const {
    return experiment_group_id_;
  }

  const mojom::TrustedSignalsPublicKey& public_key() { return *public_key_; }

  // Return next compression group id and increase it by 1.
  int next_compression_group_id() { return next_compression_group_id_++; }

 private:
  // Build a CBOR map for the partition with the provided data and IDs.
  virtual cbor::Value::MapValue BuildMapForPartition(
      const Partition& partition,
      int partition_id,
      int compression_group_id) = 0;

  // Multiple partitions are keyed by compression group ID. For the Partition
  // vector, always place interest groups with the execution mode
  // group-by-origin in index-0 position, and then expand for other modes at
  // the end.
  std::map<int, CompressionGroup> compression_groups_;

  const std::string hostname_;
  const std::optional<int> experiment_group_id_;
  mojom::TrustedSignalsPublicKeyPtr public_key_;

  // Initial id for compression groups.
  int next_compression_group_id_ = 0;
};

class CONTENT_EXPORT TrustedBiddingSignalsKVv2RequestHelperBuilder
    : public TrustedSignalsKVv2RequestHelperBuilder {
 public:
  TrustedBiddingSignalsKVv2RequestHelperBuilder(
      const std::string& hostname,
      std::optional<int> experiment_group_id,
      mojom::TrustedSignalsPublicKeyPtr public_key,
      const std::string& trusted_bidding_signals_slot_size_param);

  TrustedBiddingSignalsKVv2RequestHelperBuilder(
      const TrustedBiddingSignalsKVv2RequestHelperBuilder&) = delete;
  TrustedBiddingSignalsKVv2RequestHelperBuilder& operator=(
      const TrustedBiddingSignalsKVv2RequestHelperBuilder&) = delete;

  ~TrustedBiddingSignalsKVv2RequestHelperBuilder() override;

  // TODO(crbug.com/337917489): Consider a better way to handle identical
  // trusted signals requests (e.g., with the same IG name and bidding keys).
  // Duplicate requests should be merged with the existing ones, likely
  // requiring a map to record the isolation index for IG names to avoid
  // searching in partitions.
  //
  // Add a trusted bidding signals request to helper builder. Return an
  // isolation index indicating where the fetched trusted signals for the
  // added request can be found in the response body.
  IsolationIndex AddTrustedSignalsRequest(
      const std::string& interest_group_name,
      const std::set<std::string>& bidding_keys,
      const url::Origin& interest_group_join_origin,
      blink::mojom::InterestGroup::ExecutionMode execution_mode);

  std::map<url::Origin, int>& join_origin_compression_id_map() {
    return join_origin_compression_id_map_;
  }

  const std::pair<std::string, std::string>&
  trusted_bidding_signals_slot_size_param() {
    return trusted_bidding_signals_slot_size_param_;
  }

 private:
  cbor::Value::MapValue BuildMapForPartition(const Partition& partition,
                                             int partition_id,
                                             int compression_group_id) override;

  // Joining origin to compression group id map.
  std::map<url::Origin, int> join_origin_compression_id_map_;

  // Using a pair to store key and value for a trusted bidding signals slot
  // size parameter. Valid parameter key are "slotSize" or
  // "allSlotsRequestedSizes".
  std::pair<std::string, std::string> trusted_bidding_signals_slot_size_param_;
};

class CONTENT_EXPORT TrustedScoringSignalsKVv2RequestHelperBuilder
    : public TrustedSignalsKVv2RequestHelperBuilder {
 public:
  TrustedScoringSignalsKVv2RequestHelperBuilder(
      const std::string& hostname,
      std::optional<int> experiment_group_id,
      mojom::TrustedSignalsPublicKeyPtr public_key);

  TrustedScoringSignalsKVv2RequestHelperBuilder(
      const TrustedScoringSignalsKVv2RequestHelperBuilder&) = delete;
  TrustedScoringSignalsKVv2RequestHelperBuilder& operator=(
      const TrustedScoringSignalsKVv2RequestHelperBuilder&) = delete;

  ~TrustedScoringSignalsKVv2RequestHelperBuilder() override;

  // Add a trusted scoring signals request to helper builder. Return an
  // isolation index indicating where the fetched trusted signals for the
  // added request can be found in the response body.
  IsolationIndex AddTrustedSignalsRequest(
      const GURL& render_url,
      const std::set<std::string>& ad_component_render_urls,
      const url::Origin& owner_origin,
      const url::Origin& interest_group_join_origin);

 private:
  // Use the serialized `owner_origin` and `interest_group_join_origin` for each
  // trusted scoring signals request to distribute the requests to different
  // compression groups.
  struct CompressionGroupMapKey {
    const url::Origin owner_origin;
    const url::Origin interest_group_join_origin;

    bool operator<(const CompressionGroupMapKey& other) const {
      return std::tie(owner_origin, interest_group_join_origin) <
             std::tie(other.owner_origin, other.interest_group_join_origin);
    }
  };

  cbor::Value::MapValue BuildMapForPartition(const Partition& partition,
                                             int partition_id,
                                             int compression_group_id) override;

  // Store different compression group ids keyed by `CompressionGroupMapKey`.
  std::map<CompressionGroupMapKey, int> compression_group_map;
};

// The received result for a particular compression group, returned only on
// success. CONTENT_EXPORT is added for use in testing purposes.
struct CONTENT_EXPORT CompressionGroupResult {
  CompressionGroupResult();
  CompressionGroupResult(mojom::TrustedSignalsCompressionScheme scheme,
                         std::vector<uint8_t> content,
                         base::TimeDelta ttl);
  CompressionGroupResult(CompressionGroupResult&&);
  CompressionGroupResult& operator=(CompressionGroupResult&&);
  ~CompressionGroupResult();

  // The compression scheme used by `content`, as indicated by the
  // server.
  mojom::TrustedSignalsCompressionScheme compression_scheme;

  // The compressed content string.
  std::vector<uint8_t> content;

  // Time until the response expires.
  base::TimeDelta ttl;
};

class CONTENT_EXPORT TrustedSignalsKVv2ResponseParser {
 public:
  struct ErrorInfo {
    std::string error_msg;
  };

  // A map of compression group ids to results, in the case of success.
  using CompressionGroupResultMap = std::map<int, CompressionGroupResult>;

  // The result of a fetch. Either the entire fetch succeeds or it fails with a
  // single error.
  using SignalsFetchResult =
      base::expected<CompressionGroupResultMap, ErrorInfo>;

  // Result map for response parser. The key is an `IsolationIndex` indicates
  // compression group ID and partition ID.
  using TrustedSignalsResultMap =
      std::map<TrustedBiddingSignalsKVv2RequestHelperBuilder::IsolationIndex,
               scoped_refptr<TrustedSignals::Result>>;

  using TrustedSignalsResultMapOrError =
      base::expected<TrustedSignalsResultMap, ErrorInfo>;

  // Parse response body to `SignalsFetchResult` for integration with cache call
  // flow in browser process.
  static SignalsFetchResult ParseResponseToSignalsFetchResult(
      const std::string& body,
      quiche::ObliviousHttpRequest::Context& context);

  // Parse trusted bidding signals fetch result to result map for integration
  // with bidder worklet trusted bidding signals fetch call flow.
  //
  // TODO(crbug.com/337917489): Use a map for `interest_group_names` and `keys`,
  // where the key is the isolation index and the value is a set of strings.
  // This allows searching for each string within a specific compression group
  // and partition.
  static TrustedSignalsResultMapOrError
  ParseBiddingSignalsFetchResultToResultMap(
      AuctionV8Helper* v8_helper,
      const std::set<std::string>& interest_group_names,
      const std::set<std::string>& keys,
      const CompressionGroupResultMap& compression_group_result_map);

  // Parse trusted scoring signals fetch result to result map for integration
  // with seller worklet trusted scoring signals fetch call flow.
  //
  // TODO(crbug.com/337917489): Use a map for `render_urls` and
  // `ad_component_render_urls`, where the key is the isolation index and the
  // value is a set of strings. This allows searching for each string within a
  // specific compression group and partition.
  static TrustedSignalsResultMapOrError
  ParseScoringSignalsFetchResultToResultMap(
      AuctionV8Helper* v8_helper,
      const std::set<std::string>& render_urls,
      const std::set<std::string>& ad_component_render_urls,
      const CompressionGroupResultMap& compression_group_result_map);

  // Code below this point is for use by the TrustedSignalsKVv2Manager, which
  // gets KVv2 responses on a per-compression group basis through the
  // mojom::TrustedSignalsCache API, instead of making network requests itself.

  enum class SignalsType {
    kBidding,
    kScoring,
  };

  // A map of partition IDs to the result of parsing each partition.
  using PartitionMap = std::map<int, scoped_refptr<TrustedSignals::Result>>;

  // The result of trying to parse a compression group. Failures are global, so
  // either there's a PartitionMap or a single global error string.
  using PartitionMapOrError = base::expected<PartitionMap, std::string>;

  // Parses a compression group. Unlike
  // ParseBiddingSignalsFetchResultToResultMap(), parses all fields of all
  // partitions, so if requests for the compression group arrive after it's
  // already been parsed, there's no need to parse the data again.
  //
  // TODO(https://crbug.com/368241694): Consider only parsing the portion of the
  // compression group that's needed, while keeping the rest around in case it's
  // needed down the line. Parsing on a per-partition basis may be the best
  // balance of practicality and compatibility in the case of multiple V8
  // threads.
  static PartitionMapOrError ParseEntireCompressionGroup(
      AuctionV8Helper* v8_helper,
      SignalsType signals_type,
      mojom::TrustedSignalsCompressionScheme compression_scheme,
      base::span<const uint8_t> compression_group_bytes);
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_KVV2_HELPER_H_
