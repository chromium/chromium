// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_KVV2_HELPER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_KVV2_HELPER_H_

#include <stdint.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/cbor/values.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_request_manager.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

// Encapsulates the logic for generating trusted signals key-value version 2
// requests.
// TODO(crbug.com/349651946): Remove after KVv2 is migrated to browser process.

namespace auction_worklet {

class CONTENT_EXPORT TrustedSignalsKVv2RequestHelper {
 public:
  explicit TrustedSignalsKVv2RequestHelper(std::string post_request_body);

  TrustedSignalsKVv2RequestHelper(TrustedSignalsKVv2RequestHelper&&);
  TrustedSignalsKVv2RequestHelper& operator=(TrustedSignalsKVv2RequestHelper&&);

  TrustedSignalsKVv2RequestHelper& operator=(
      const TrustedSignalsKVv2RequestHelper&) = delete;
  TrustedSignalsKVv2RequestHelper(const TrustedSignalsKVv2RequestHelper&) =
      delete;

  ~TrustedSignalsKVv2RequestHelper();

  std::string TakePostRequestBody();

 private:
  std::string post_request_body_;
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

    bool operator==(const IsolationIndex& other) const = default;
  };

  // Build the request helper using the helper builder to construct the POST
  // body string, noting that the partition IDs will not be sequential.
  virtual TrustedSignalsKVv2RequestHelper Build() = 0;

 protected:
  TrustedSignalsKVv2RequestHelperBuilder(
      std::string hostname,
      GURL trusted_signals_url,
      std::optional<int> experiment_group_id);

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

  std::map<url::Origin, int>& join_origin_compression_id_map() {
    return join_origin_compression_id_map_;
  }

  const std::string& hostname() const { return hostname_; }

  const GURL& trusted_signals_url() const { return trusted_signals_url_; }

  const std::optional<int>& experiment_group_id() const {
    return experiment_group_id_;
  }

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
  // Joining origin to compression group id map
  std::map<url::Origin, int> join_origin_compression_id_map_;

  const std::string hostname_;
  const GURL trusted_signals_url_;
  const std::optional<int> experiment_group_id_;

  // Initial id for compression groups.
  int next_compression_group_id_ = 0;
};

class CONTENT_EXPORT TrustedBiddingSignalsKVv2RequestHelperBuilder
    : public TrustedSignalsKVv2RequestHelperBuilder {
 public:
  TrustedBiddingSignalsKVv2RequestHelperBuilder(
      const std::string& hostname,
      const GURL& trusted_signals_url,
      std::optional<int> experiment_group_id,
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
  // Adds a request for the specified information to the trusted bidding signals
  // helper builder. Returns the IsolationIndex indicating where the requested
  // information can be found in the response to the fully assembled request
  // once it becomes available.
  IsolationIndex AddTrustedSignalsRequest(
      base::optional_ref<const std::string> interest_group_name,
      base::optional_ref<const std::set<std::string>> bidding_keys,
      base::optional_ref<const url::Origin> interest_group_join_origin,
      std::optional<blink::mojom::InterestGroup::ExecutionMode> execution_mode);

  TrustedSignalsKVv2RequestHelper Build() override;

 private:
  cbor::Value::MapValue BuildMapForPartition(const Partition& partition,
                                             int partition_id,
                                             int compression_group_id) override;

  // Using a pair to store key and value for a trusted bidding signals slot
  // size parameter. Valid parameter key are "slotSize" or
  // "allSlotsRequestedSizes"
  std::pair<std::string, std::string> trusted_bidding_signals_slot_size_param_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_KVV2_HELPER_H_
