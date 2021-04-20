// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_WORKLET_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_WORKLET_H_

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8.h"

namespace auction_worklet {

class AuctionV8Helper;
class TrustedBiddingSignals;
class WorkletLoader;

// Represents a bidder worklet for FLEDGE
// (https://github.com/WICG/turtledove/blob/main/FLEDGE.md). Loads and runs the
// bidder worklet's Javascript.
class BidderWorklet {
 public:
  struct BidResult {
    // Constructor for when there is no bid, either due to an error or the
    // script not offering one.
    BidResult();

    // Constructor when a bid is made. `bid` must be > 0, and the URL must be
    // valid.
    BidResult(std::string ad, double bid, GURL render_url);

    // `success` will be false on any type of failure, and other values will be
    // empty or 0. Inability to extract ad string, bid value, or valid render
    // URL are all considered errors.
    //
    // TODO(mmenke): Pass along some sort of error string instead, to make
    // debugging easier.
    bool success = false;

    // JSON string to be passed to the scoring function.
    std::string ad;

    // Bid. 0 if no bid is offered (even if underlying script returned a
    // negative value).
    double bid = 0;

    // Render URL, if any bid was made.
    GURL render_url;
  };

  struct ReportWinResult {
    // Constructor for when there is a failure of some sort, or no URL to report
    // to.
    ReportWinResult();

    // Constructor when a report was requested.
    explicit ReportWinResult(GURL report_url);

    // `success` will be false on any type of failure. Neither lack or reporting
    // function nor lack of report URL is considered an error.
    //
    // TODO(mmenke): Pass along some sort of error string instead, to make
    // debugging easier.
    bool success = false;

    // Report URL, if one is provided. Empty on failure, or if no report URL is
    // provided.
    GURL report_url;
  };

  using LoadWorkletCallback = base::OnceCallback<void(bool success)>;

  // Starts loading the worklet script on construction. Callback will be invoked
  // asynchronously once the data has been fetched or an error has occurred.
  // Must be destroyed before `v8_helper`. No data is leaked between consecutive
  // invocations of this method, or between invocations of this method and
  // GenerateBid().
  BidderWorklet(network::mojom::URLLoaderFactory* url_loader_factory,
                const GURL& script_source_url,
                AuctionV8Helper* v8_helper,
                LoadWorkletCallback load_worklet_callback);
  explicit BidderWorklet(const BidderWorklet&) = delete;
  BidderWorklet& operator=(const BidderWorklet&) = delete;
  ~BidderWorklet();

  // Calls generateBid(), and returns resulting bid, if any. May only be called
  // once BidderWorklet has successfully loaded.
  BidResult GenerateBid(
      const blink::mojom::InterestGroup& interest_group,
      const base::Optional<std::string>& auction_signals_json,
      const base::Optional<std::string>& per_buyer_signals_json,
      const std::vector<std::string>& trusted_bidding_signals_keys,
      TrustedBiddingSignals* trusted_bidding_signals,
      const std::string& browser_signal_top_window_hostname,
      const std::string& browser_signal_seller,
      int browser_signal_join_count,
      int browser_signal_bid_count,
      const std::vector<mojo::StructPtr<mojom::PreviousWin>>&
          browser_signal_prev_wins,
      base::Time auction_start_time);

  // Calls reportWin(), and returns reporting information. May only be called
  // once the worklet has successfully loaded.
  ReportWinResult ReportWin(
      const base::Optional<std::string>& auction_signals_json,
      const base::Optional<std::string>& per_buyer_signals_json,
      const std::string& seller_signals_json,
      const std::string& browser_signal_top_window_hostname,
      const url::Origin& browser_signal_interest_group_owner,
      const std::string& browser_signal_interest_group_name,
      const GURL& browser_signal_render_url,
      const std::string& browser_signal_ad_render_fingerprint,
      double browser_signal_bid);

 private:
  void OnDownloadComplete(
      LoadWorkletCallback load_worklet_callback,
      std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script);

  AuctionV8Helper* const v8_helper_;
  std::unique_ptr<WorkletLoader> worklet_loader_;

  // Compiled script, not bound to any context. Can be repeatedly bound to
  // different context and executed, without persisting any state.
  std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_WORKLET_H_
