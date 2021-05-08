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
#include "base/optional.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
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
//
// Each worklet object can only be used to load and run a single script's
// generateBid() and (if the bid is won) reportWin() once.
//
// TODO(mmenke): Make worklets reuseable. Allow a single BidderWorklet instance
// to both be used for two generateBid() calls for different interest groups
// with the same owner in the same auction, and to be used to bid for the same
// interest group in different auctions.
class BidderWorklet {
 public:
  // Structure containing information about a bid returned by invoking a
  // worklet's generageBid() method. If no bid is made, no Bid is constructed.
  struct Bid {
    Bid(std::string ad,
        double bid,
        GURL render_url,
        base::TimeDelta bid_duration);

    Bid(const Bid& other);
    Bid(Bid&& other);

    ~Bid();

    Bid& operator=(const Bid&);
    Bid& operator=(Bid&&);

    // JSON string to be passed to the scoring function.
    std::string ad;

    // Offered bid value.
    double bid;

    // Render URL, if any bid was made.
    GURL render_url;

    // How long it took to run the script that generated the bid.
    base::TimeDelta bid_duration;
  };

  // If no bid is generated, `bid` is null.
  //
  // `error_msgs` contains error messages for debugging. This isn't guaranteed
  // to be produced for all failures, so should not be checked to identify
  // bidding failures errors. It's also possible for there to be an error
  // message on success, in the case the trusted bidding signals failed to load
  // - auctions will still be run without it, but `error_msgs` will be populated
  // with information about the load failure.
  using LoadScriptAndGenerateBidCallback =
      base::OnceCallback<void(base::Optional<Bid> bid,
                              std::vector<std::string> error_msgs)>;

  // `report_url` is the URL to request to report displaying the ad. It is
  // nullopt on error or if report is requested. `error_msgs` is a list of
  // errors that occurred, if any. `error_msgs` may be non-empty on success, or
  // empty on failure.
  using ReportWinCallback =
      base::OnceCallback<void(const base::Optional<GURL>& report_url,
                              const std::vector<std::string>& error_msgs)>;

  // Starts loading the worklet script on construction, as well as the trusted
  // bidding data, if necessary. Will then call the script's generateBid()
  // function and invoke the callback with the results. Callback will always be
  // invoked asynchronously, once a bid has been generated or a fatal error has
  // occurred. Must be destroyed before `v8_helper`.
  //
  // Data is cached and will be reused ReportWin().
  BidderWorklet(
      network::mojom::URLLoaderFactory* url_loader_factory,
      mojom::BiddingInterestGroupPtr bidding_interest_group,
      const base::Optional<std::string>& auction_signals_json,
      const base::Optional<std::string>& per_buyer_signals_json,
      const url::Origin& browser_signal_top_window_origin,
      const std::string& browser_signal_seller,
      base::Time auction_start_time,
      AuctionV8Helper* v8_helper,
      LoadScriptAndGenerateBidCallback load_script_and_generate_bid_callback);
  explicit BidderWorklet(const BidderWorklet&) = delete;
  BidderWorklet& operator=(const BidderWorklet&) = delete;
  ~BidderWorklet();

  // Calls reportWin(), and asynchronously invokes `callback` with reporting
  // information. May only be called once the worklet has successfully loaded.
  void ReportWin(const std::string& seller_signals_json,
                 const GURL& browser_signal_render_url,
                 const std::string& browser_signal_ad_render_fingerprint,
                 double browser_signal_bid,
                 ReportWinCallback callback);

 private:
  void OnScriptDownloaded(
      std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script,
      base::Optional<std::string> error_msg);

  void OnTrustedBiddingSignalsDownloaded(bool load_result,
                                         base::Optional<std::string> error_msg);

  // Checks if the script has been loaded successfully, and the
  // TrustedBiddingSignals load has finished (successfully or not). If so, calls
  // generateBid(), and invokes `load_script_and_generate_bid_callback_` with
  // the resulting bid, if any. May only be called once BidderWorklet has
  // successfully loaded.
  void GenerateBidIfReady();

  // Utility function to invoke `load_script_and_generate_bid_callback_` with
  // `error_msg` and `trusted_bidding_signals_error_msg_`.
  void InvokeBidCallbackOnError(
      base::Optional<std::string> error_msg = base::nullopt);

  const GURL script_source_url_;
  AuctionV8Helper* const v8_helper_;
  const mojom::BiddingInterestGroupPtr bidding_interest_group_;

  LoadScriptAndGenerateBidCallback load_script_and_generate_bid_callback_;

  const base::Optional<std::string> auction_signals_json_;
  const base::Optional<std::string> per_buyer_signals_json_;
  const std::string browser_signal_top_window_hostname_;
  const std::string browser_signal_seller_;
  const base::Time auction_start_time_;

  std::unique_ptr<WorkletLoader> worklet_loader_;

  bool trusted_bidding_signals_loading_ = false;
  std::unique_ptr<TrustedBiddingSignals> trusted_bidding_signals_;
  // Error message returned by attempt to load `trusted_bidding_signals_`.
  // Errors loading it are not fatal, so such errors are cached here and only
  // reported on bid completion.
  base::Optional<std::string> trusted_bidding_signals_error_msg_;

  // Compiled script, not bound to any context. Can be repeatedly bound to
  // different context and executed, without persisting any state.
  std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_WORKLET_H_
