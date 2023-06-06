// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERIALIZER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERIALIZER_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "content/browser/interest_group/storage_interest_group.h"

namespace content {

// Serializes Bidding and Auction requests
class BiddingAndAuctionSerializer {
 public:
  BiddingAndAuctionSerializer();
  BiddingAndAuctionSerializer(BiddingAndAuctionSerializer&& state);
  ~BiddingAndAuctionSerializer();

  void SetPublisher(std::string publisher) { publisher_ = publisher; }
  // TODO(behamilton): void SetGenerationId(std::string generation_id);
  void AddGroups(std::string owner, std::vector<StorageInterestGroup> groups);
  std::vector<uint8_t> Build();

 private:
  base::Time start_time_;
  std::string publisher_;
  std::vector<std::pair<std::string, std::vector<StorageInterestGroup>>>
      accumulated_groups_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_SERIALIZER_H_
