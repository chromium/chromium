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
#include "base/time/time.h"
#include "base/uuid.h"
#include "content/browser/interest_group/header_direct_from_seller_signals.h"
#include "content/public/browser/page_user_data.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_client.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/origin.h"

namespace data_decoder {
class DataDecoder;
}  // namespace data_decoder

namespace content {

struct CONTENT_EXPORT AdAuctionRequestContext {
  AdAuctionRequestContext(
      url::Origin seller,
      base::flat_map<url::Origin, std::vector<std::string>> group_names,
      quiche::ObliviousHttpRequest::Context context,
      base::TimeTicks start_time,
      base::flat_map<blink::InterestGroupKey, url::Origin>
          group_pagg_coordinators);
  AdAuctionRequestContext(AdAuctionRequestContext&& other);
  ~AdAuctionRequestContext();

  url::Origin seller;
  base::flat_map<url::Origin, std::vector<std::string>> group_names;
  quiche::ObliviousHttpRequest::Context context;
  base::TimeTicks start_time;
  base::flat_map<blink::InterestGroupKey, url::Origin> group_pagg_coordinators;
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

  void ParseAndFindAdAuctionSignals(
      const url::Origin& origin,
      const std::string& ad_slot,
      HeaderDirectFromSellerSignals::ParseAndFindCompletedCallback callback);

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

  // Returns a pointer to a DataDecoder owned by this AdAuctionPageData instance
  // The DataDecoder is only valid for the life of the page.
  data_decoder::DataDecoder* GetDecoderFor(const url::Origin& origin);

  // Returns real time reporting quota left for `origin`.
  std::optional<std::pair<base::TimeTicks, double>> GetRealTimeReportingQuota(
      const url::Origin& origin);

  // Update real time reporting quota for `origin`.
  void UpdateRealTimeReportingQuota(const url::Origin& origin,
                                    std::pair<base::TimeTicks, double> quota);

 private:
  explicit AdAuctionPageData(Page& page);

  friend class PageUserData<AdAuctionPageData>;
  PAGE_USER_DATA_KEY_DECL();

  void OnAddAuctionSignalsWitnessForOriginCompleted(
      std::vector<std::string> errors);

  std::map<url::Origin, std::set<std::string>> origin_auction_result_map_;
  HeaderDirectFromSellerSignals header_direct_from_seller_signals_;
  std::map<url::Origin, std::map<std::string, std::vector<std::string>>>
      origin_nonce_additional_bids_map_;
  std::map<base::Uuid, AdAuctionRequestContext> context_map_;

  // The real time reporting quota left for origin at a certain timestamp. Used
  // to do per page per reporting origin rate limiting on real time reporting.
  // TODO(crbug.com/337132755): Clean this up after long enough time, in case
  // some pages live for a while.
  std::map<url::Origin, std::pair<base::TimeTicks, double>>
      real_time_reporting_quota_;

  // Must be declared last -- DataDecoder destruction cancels decoding
  // completion callbacks.
  std::map<url::Origin, std::unique_ptr<data_decoder::DataDecoder>>
      decoder_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_PAGE_DATA_H_
