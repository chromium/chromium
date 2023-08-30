// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_PAGE_DATA_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_PAGE_DATA_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/uuid.h"
#include "content/public/browser/page_user_data.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_client.h"
#include "url/origin.h"

namespace content {

struct CONTENT_EXPORT AdAuctionRequestContext {
  AdAuctionRequestContext(
      url::Origin seller,
      base::flat_map<url::Origin, std::vector<std::string>> group_names,
      quiche::ObliviousHttpRequest::Context context);
  AdAuctionRequestContext(AdAuctionRequestContext&& other);
  ~AdAuctionRequestContext();

  url::Origin seller;
  base::flat_map<url::Origin, std::vector<std::string>> group_names;
  quiche::ObliviousHttpRequest::Context context;
};

// Contains auction header responses within a page. This will only be created
// for the outermost page (i.e. not within a fenced frame).
class CONTENT_EXPORT AdAuctionPageData
    : public PageUserData<AdAuctionPageData> {
 public:
  ~AdAuctionPageData() override;

  void AddAuctionResultWitnessForOrigin(const url::Origin& origin,
                                        const std::string& response);

  bool WitnessedAuctionResultForOrigin(const url::Origin& origin,
                                       const std::string& response) const;

  void AddAuctionSignalsWitnessForOrigin(const url::Origin& origin,
                                         const std::string& response);

  const std::set<std::string>& GetAuctionSignalsForOrigin(
      const url::Origin& origin) const;

  void AddAuctionAdditionalBidsWitnessForOrigin(
      const url::Origin& origin,
      const std::map<std::string, std::vector<std::string>>&
          nonce_additional_bids_map);

  std::vector<std::string> TakeAuctionAdditionalBidsForOriginAndNonce(
      const url::Origin& origin,
      const std::string& nonce);

  void RegisterAdAuctionRequestContext(const base::Uuid& id,
                                       AdAuctionRequestContext context);
  AdAuctionRequestContext* GetContextForAdAuctionRequest(const base::Uuid& id);

 private:
  explicit AdAuctionPageData(Page& page);

  friend class PageUserData<AdAuctionPageData>;
  PAGE_USER_DATA_KEY_DECL();

  std::map<url::Origin, std::set<std::string>> origin_auction_result_map_;
  std::map<url::Origin, std::set<std::string>> origin_auction_signals_map_;
  std::map<url::Origin, std::map<std::string, std::vector<std::string>>>
      origin_nonce_additional_bids_map_;
  std::map<base::Uuid, AdAuctionRequestContext> context_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_PAGE_DATA_H_
