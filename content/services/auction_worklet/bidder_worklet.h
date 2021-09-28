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
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/trusted_bidding_signals.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-persistent-handle.h"

namespace v8 {
class UnboundScript;
}  // namespace v8

namespace auction_worklet {

class AuctionV8Helper;

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
class BidderWorklet : public mojom::BidderWorklet {
 public:
  // Starts loading the worklet script on construction, as well as the trusted
  // bidding data, if necessary. Will then call the script's generateBid()
  // function and invoke the callback with the results. Callback will always be
  // invoked asynchronously, once a bid has been generated or a fatal error has
  // occurred.
  //
  // Data is cached and will be reused ReportWin().
  BidderWorklet(
      scoped_refptr<AuctionV8Helper> v8_helper,
      bool pause_for_debugger_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      mojom::BiddingInterestGroupPtr bidding_interest_group,
      const absl::optional<std::string>& auction_signals_json,
      const absl::optional<std::string>& per_buyer_signals_json,
      const url::Origin& browser_signal_top_window_origin,
      const url::Origin& browser_signal_seller_origin,
      base::Time auction_start_time,
      mojom::AuctionWorkletService::LoadBidderWorkletAndGenerateBidCallback
          load_bidder_worklet_and_generate_bid_callback);
  explicit BidderWorklet(const BidderWorklet&) = delete;
  ~BidderWorklet() override;
  BidderWorklet& operator=(const BidderWorklet&) = delete;

  // Warning: The caller may need to spin the event loop for this to get
  // initialized to a value different from kNoDebugContextGroupId.
  int context_group_id_for_testing() const { return context_group_id_; }

  // mojom::BidderWorklet implementation:
  void ReportWin(const std::string& seller_signals_json,
                 const GURL& browser_signal_render_url,
                 const std::string& browser_signal_ad_render_fingerprint,
                 double browser_signal_bid,
                 ReportWinCallback callback) override;

  void ConnectDevToolsAgent(
      mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent) override;

 private:
  // Portion of BidderWorklet that deals with V8 execution, and therefore lives
  // on the v8 thread --- everything except the constructor must be run there.
  class V8State {
   public:
    V8State(scoped_refptr<AuctionV8Helper> v8_helper,
            GURL script_source_url,
            base::WeakPtr<BidderWorklet> parent,
            mojom::BiddingInterestGroupPtr bidding_interest_group,
            const absl::optional<std::string>& auction_signals_json,
            const absl::optional<std::string>& per_buyer_signals_json,
            const url::Origin& browser_signal_top_window_origin,
            const url::Origin& browser_signal_seller_origin,
            base::Time auction_start_time);

    void SetWorkletScript(WorkletLoader::Result worklet_script);

    void SetTrustedSignalsResult(std::unique_ptr<TrustedBiddingSignals::Result>
                                     trusted_bidding_signals_result);

    void ReportWin(const std::string& seller_signals_json,
                   const GURL& browser_signal_render_url,
                   const std::string& browser_signal_ad_render_fingerprint,
                   double browser_signal_bid,
                   ReportWinCallback callback);

    void GenerateBid();

    void ConnectDevToolsAgent(
        mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent);

   private:
    friend class base::DeleteHelper<V8State>;
    ~V8State();

    void FinishInit();

    void PostReportWinCallbackToUserThread(ReportWinCallback callback,
                                           absl::optional<GURL> report_url,
                                           std::vector<std::string> errors);
    void PostErrorBidCallbackToUserThread(
        std::vector<std::string> error_msgs = std::vector<std::string>());

    static void PostResumeToUserThread(
        base::WeakPtr<BidderWorklet> parent,
        scoped_refptr<base::SequencedTaskRunner> user_thread);

    const scoped_refptr<AuctionV8Helper> v8_helper_;
    const base::WeakPtr<BidderWorklet> parent_;
    const scoped_refptr<base::SequencedTaskRunner> user_thread_;

    const mojom::BiddingInterestGroupPtr bidding_interest_group_;
    const absl::optional<std::string> auction_signals_json_;
    const absl::optional<std::string> per_buyer_signals_json_;
    const url::Origin browser_signal_top_window_origin_;
    const url::Origin browser_signal_seller_origin_;
    const base::Time auction_start_time_;

    // Compiled script, not bound to any context. Can be repeatedly bound to
    // different context and executed, without persisting any state.
    v8::Global<v8::UnboundScript> worklet_script_;

    std::unique_ptr<TrustedBiddingSignals::Result>
        trusted_bidding_signals_result_;

    const GURL script_source_url_;

    int context_group_id_;

    SEQUENCE_CHECKER(v8_sequence_checker_);
  };

  void ResumeIfPaused();
  void StartIfReady();

  void OnScriptDownloaded(WorkletLoader::Result worklet_script,
                          absl::optional<std::string> error_msg);

  void OnTrustedBiddingSignalsDownloaded(
      std::unique_ptr<TrustedBiddingSignals::Result> result,
      absl::optional<std::string> error_msg);

  // Checks if the script has been loaded successfully, and the
  // TrustedBiddingSignals load has finished (successfully or not). If so, calls
  // generateBid(), and invokes `load_script_and_generate_bid_callback_` with
  // the resulting bid, if any. May only be called once BidderWorklet has
  // successfully loaded.
  void GenerateBidIfReady();

  void DeliverContextGroupIdOnUserThread(int context_group_id);

  // Utility function to invoke `load_script_and_generate_bid_callback_` with
  // `error_msgs` and `trusted_bidding_signals_error_msg_`.
  void InvokeBidCallbackOnError(
      std::vector<std::string> error_msgs = std::vector<std::string>());

  // Likewise but for a success.
  void DeliverBidCallbackOnUserThread(mojom::BidderWorkletBidPtr bid,
                                      std::vector<std::string> error_msgs);

  void DeliverReportWinOnUserThread(ReportWinCallback callback,
                                    absl::optional<GURL> report_url,
                                    std::vector<std::string> errors);

  scoped_refptr<base::SequencedTaskRunner> v8_runner_;

  // Kept around until Start().
  scoped_refptr<AuctionV8Helper> v8_helper_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  bool paused_;

  // `context_group_id_` starts at kNoDebugContextGroupId, but then gets
  // initialized after some thread hops.
  int context_group_id_;

  const GURL script_source_url_;
  mojom::AuctionWorkletService::LoadBidderWorkletAndGenerateBidCallback
      load_bidder_worklet_and_generate_bid_callback_;

  std::unique_ptr<WorkletLoader> worklet_loader_;
  bool have_worklet_script_ = false;

  // Set while loading is in progress.
  std::unique_ptr<TrustedBiddingSignals> trusted_bidding_signals_;
  // Error message returned by attempt to load `trusted_bidding_signals_`.
  // Errors loading it are not fatal, so such errors are cached here and only
  // reported on bid completion.
  absl::optional<std::string> trusted_bidding_signals_error_msg_;

  // Lives on `v8_runner_`. Since it's deleted there via DeleteSoon, tasks can
  // be safely posted from main thread to it with an Unretained pointer.
  std::unique_ptr<V8State, base::OnTaskRunnerDeleter> v8_state_;

  SEQUENCE_CHECKER(user_sequence_checker_);

  // Used when posting callbacks back from V8State.
  base::WeakPtrFactory<BidderWorklet> weak_ptr_factory_{this};
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_WORKLET_H_
