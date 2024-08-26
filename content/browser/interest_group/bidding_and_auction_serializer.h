// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERIALIZER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERIALIZER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "content/browser/interest_group/interest_group_caching_storage.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "url/origin.h"

namespace content {

struct CONTENT_EXPORT BiddingAndAuctionData {
  BiddingAndAuctionData();
  BiddingAndAuctionData(BiddingAndAuctionData&& other);
  ~BiddingAndAuctionData();

  BiddingAndAuctionData& operator=(BiddingAndAuctionData&& other);

  std::vector<uint8_t> request;
  base::flat_map<url::Origin, std::vector<std::string>> group_names;
  base::flat_map<blink::InterestGroupKey, url::Origin> group_pagg_coordinators;
};

// Serializes Bidding and Auction requests
class CONTENT_EXPORT BiddingAndAuctionSerializer {
 public:
  // Helper class for BiddingAndAuctionSerializer to allocate buyer space
  // allocations. Only public for testing.
  class CONTENT_EXPORT TargetSizeEstimator {
   public:
    TargetSizeEstimator(size_t total_size_before_groups,
                        const blink::mojom::AuctionDataConfig* config);
    ~TargetSizeEstimator();

    // Updates the estimator with the maximum serialized size for this bidder.
    void UpdatePerBuyerMaxSize(const url::Origin& bidder, size_t max_size);

    // Estimate the maximum compressed size (bytes) in the request that can be
    // used to store compressed serialized interest groups for the given bidder.
    // If there is no maximum, return std::nullopt.
    std::optional<uint64_t> EstimateTargetSize(
        const url::Origin& bidder,
        base::CheckedNumeric<size_t> bidders_elements_size);

   private:
    // Called to perform proportional allocation of remaining space among
    // sized groups (buyers with a targetSize), taking into account groups that
    // can't use their entire share. Uses all of `remaining_size` for sized
    // groups.
    void UpdateSizedGroupSizes(size_t remaining_size);

    // Allocates space for unsized groups (buyers without a targetSize), taking
    // into account groups that can't use their entire share. Uses all of
    // `remaining_size` for unsized groups.
    void UpdateUnsizedGroupSizes(size_t remaining_size);

    const size_t total_size_before_groups_ = 0;

    // Size to use for equally-sized buyers (without a targetSize). Set to
    // nullopt until calculated by `UpdateUnsizedGroupSizes`.
    std::optional<size_t> unsized_buyer_size_;
    // Size used by unsized buyers that use less than
    // `unsized_buyer_size_`.
    base::CheckedNumeric<size_t> remaining_allocated_unsized_buyer_size_ = 0;
    // Number of remaining groups that could use more than
    // `unsized_buyer_size_`
    size_t remaining_unallocated_unsized_buyers_ = 0;
    size_t total_unsized_buyers_ = 0;

    // Total proportional size (used in proportional size allocation).
    base::CheckedNumeric<uint64_t> per_buyer_total_allowed_size_ = 0;
    // Running sum of proportional size (used in proportional size allocation).
    base::CheckedNumeric<uint64_t> per_buyer_current_allowed_size_ = 0;

    // Contains either the maximum size that would be used by a buyer (after
    // calls to `UpdatePerBuyerMaxSize`), or the allocated size for this buyer
    // (for proportional allocations, after the call to
    // `UpdateSizedGroupSizes`).
    std::map<url::Origin, size_t> per_buyer_size_;

    raw_ptr<const blink::mojom::AuctionDataConfig> config_;
  };

  BiddingAndAuctionSerializer();
  BiddingAndAuctionSerializer(BiddingAndAuctionSerializer&& other);
  ~BiddingAndAuctionSerializer();

  void SetPublisher(std::string publisher) { publisher_ = publisher; }
  void SetGenerationId(base::Uuid generation_id) {
    generation_id_ = generation_id;
  }
  void SetTimestamp(base::Time timestamp) { timestamp_ = timestamp; }
  void SetConfig(blink::mojom::AuctionDataConfigPtr config) {
    config_ = std::move(config);
  }
  void SetDebugReportInLockout(bool debug_report_in_lockout) {
    debug_report_in_lockout_ = debug_report_in_lockout;
  }
  void AddGroups(const url::Origin& owner,
                 scoped_refptr<StorageInterestGroups> groups);
  BiddingAndAuctionData Build();

 private:
  base::Uuid generation_id_;
  std::string publisher_;
  base::Time timestamp_;
  blink::mojom::AuctionDataConfigPtr config_;
  bool debug_report_in_lockout_;
  std::vector<std::pair<url::Origin, std::vector<SingleStorageInterestGroup>>>
      accumulated_groups_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERIALIZER_H_
