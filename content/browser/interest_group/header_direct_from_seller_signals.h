// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_HEADER_DIRECT_FROM_SELLER_SIGNALS_H_
#define CONTENT_BROWSER_INTEREST_GROUP_HEADER_DIRECT_FROM_SELLER_SIGNALS_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace data_decoder {
class DataDecoder;
}  // namespace data_decoder

namespace content {

// Parses the results of Ad-Auction-Signals header values, as returned by
// AdAuctionPageData::GetAuctionSignalsForOrigin().
//
// Note that since JSON parsing is asynchronous, the parser *cannot* assume that
// the Page, and therefore the AdAuctionPageData is still live after the JSON
// callback completes.
//
// JSON format is described in https://github.com/WICG/turtledove/pull/695.
class CONTENT_EXPORT HeaderDirectFromSellerSignals {
 public:
  // Returns the result and a vector of errors (which will be empty if none
  // occurred). The HeaderDirectFromSellerSignals pointer will *never* be
  // null -- if no match was found, a default-constructed
  // HeaderDirectFromSellerSignals will be passed. Note that a match may
  // be found even if errors occur (for instance, if invalid responses are
  // skipped before a matching response was found).
  using CompletionCallback =
      base::OnceCallback<void(std::unique_ptr<HeaderDirectFromSellerSignals>,
                              std::vector<std::string>)>;

  // This may return null.
  using GetDecoderCallback =
      base::RepeatingCallback<data_decoder::DataDecoder*()>;

  // The default constructor provides null / empty signals when no ad slot was
  // specified on the page.
  HeaderDirectFromSellerSignals();

  // Public for std::make_unique.
  HeaderDirectFromSellerSignals(
      absl::optional<std::string> seller_signals,
      absl::optional<std::string> auction_signals,
      base::flat_map<url::Origin, std::string> per_buyer_signals);

  ~HeaderDirectFromSellerSignals();

  HeaderDirectFromSellerSignals(HeaderDirectFromSellerSignals&) = delete;
  HeaderDirectFromSellerSignals& operator=(HeaderDirectFromSellerSignals&) =
      delete;

  // Asynchronously parses the JSON responses in `responses` -- each of which
  // should represent an array of dictionaries -- until such a dictionary is
  // found that matches the adSlot specified by `ad_slot`. Results are provided
  // to `callback` -- if no match is found, a default-constructed
  // HeaderDirectFromSellerSignals will be passed.
  //
  // `responses` should be the return value of:
  //
  //  const std::set<std::string>& GetAuctionSignalsForOrigin(
  //      const url::Origin& origin) const;
  //
  // That is, they are the signals returned only from a single (seller) origin.
  //
  // If `get_decoder` ever returns null, `callback` will not be invoked.
  static void ParseAndFind(GetDecoderCallback get_decoder,
                           const std::set<std::string>& responses,
                           std::string ad_slot,
                           CompletionCallback callback);

  // Results of the `sellerSignals` JSON dictionary field.
  const absl::optional<std::string>& seller_signals() const {
    return seller_signals_;
  }

  // Results of the `auctionSignals` JSON dictionary field.
  const absl::optional<std::string>& auction_signals() const {
    return auction_signals_;
  }

  // Results of the `perBuyerSignals` JSON dictionary field.
  const base::flat_map<url::Origin, std::string>& per_buyer_signals() const {
    return per_buyer_signals_;
  }

 private:
  const absl::optional<std::string> seller_signals_;

  const absl::optional<std::string> auction_signals_;

  const base::flat_map<url::Origin, std::string> per_buyer_signals_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_HEADER_DIRECT_FROM_SELLER_SIGNALS_H_
