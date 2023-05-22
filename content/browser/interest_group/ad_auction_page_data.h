// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_PAGE_DATA_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_PAGE_DATA_H_

#include <map>
#include <set>
#include <string>

#include "content/public/browser/page_user_data.h"
#include "url/origin.h"

namespace content {

// Contains auction header responses within a page. This will only be created
// for the outermost page (i.e. not within a fenced frame).
class CONTENT_EXPORT AdAuctionPageData
    : public PageUserData<AdAuctionPageData> {
 public:
  ~AdAuctionPageData() override;

  void AddAuctionResponseWitnessForOrigin(const url::Origin& origin,
                                          const std::string& response);

  bool WitnessedAuctionResponseForOrigin(const url::Origin& origin,
                                         const std::string& response) const;

 private:
  explicit AdAuctionPageData(Page& page);

  friend class PageUserData<AdAuctionPageData>;
  PAGE_USER_DATA_KEY_DECL();

  std::map<url::Origin, std::set<std::string>> origin_auction_responses_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_PAGE_DATA_H_
