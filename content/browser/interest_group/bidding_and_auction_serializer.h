// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERIALIZER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERIALIZER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "content/browser/interest_group/interest_group_caching_storage.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

struct CONTENT_EXPORT BiddingAndAuctionData {
  BiddingAndAuctionData();
  BiddingAndAuctionData(BiddingAndAuctionData&& other);
  ~BiddingAndAuctionData();

  BiddingAndAuctionData& operator=(BiddingAndAuctionData&& other);

  std::vector<uint8_t> request;
  base::flat_map<url::Origin, std::vector<std::string>> group_names;
};

// Serializes Bidding and Auction requests
class BiddingAndAuctionSerializer {
 public:
  BiddingAndAuctionSerializer();
  BiddingAndAuctionSerializer(BiddingAndAuctionSerializer&& other);
  ~BiddingAndAuctionSerializer();

  void SetPublisher(std::string publisher) { publisher_ = publisher; }
  void SetGenerationId(base::Uuid generation_id) {
    generation_id_ = generation_id;
  }
  void AddGroups(const url::Origin& owner,
                 scoped_refptr<StorageInterestGroups> groups);
  BiddingAndAuctionData Build();

 private:
  base::Uuid generation_id_;
  base::Time start_time_;
  std::string publisher_;
  std::vector<std::pair<url::Origin, std::vector<SingleStorageInterestGroup>>>
      accumulated_groups_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERIALIZER_H_
