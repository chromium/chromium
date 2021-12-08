// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_WORKLET_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_WORKLET_H_

#include <cmath>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/trusted_signals.h"
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

// Represents a bidder worklet for FLEDGE
// (https://github.com/WICG/turtledove/blob/main/FLEDGE.md). Loads and runs the
// bidder worklet's Javascript.
//
// Each worklet object can only be used to load and run a single script's
// generateBid() and (if the bid is won) reportWin() once.
//
// The BidderWorklet is non-threadsafe, and lives entirely on the main / user
// sequence. It has an internal V8State object that runs scripts, and is only
// used on the V8 sequence.
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
  BidderWorklet(scoped_refptr<AuctionV8Helper> v8_helper,
                bool pause_for_debugger_on_start,
                mojo::PendingRemote<network::mojom::URLLoaderFactory>
                    pending_url_loader_factory,
                mojom::BiddingInterestGroupPtr bidding_interest_group);
  explicit BidderWorklet(const BidderWorklet&) = delete;
  ~BidderWorklet() override;
  BidderWorklet& operator=(const BidderWorklet&) = delete;

  int context_group_id_for_testing() const;

  // mojom::BidderWorklet implementation:
  void GenerateBid(const absl::optional<std::string>& auction_signals_json,
                   const absl::optional<std::string>& per_buyer_signals_json,
                   const url::Origin& top_window_origin,
                   const url::Origin& seller_origin,
                   base::Time auction_start_time,
                   GenerateBidCallback generate_bid_callback) override;
  void ReportWin(const absl::optional<std::string>& auction_signals_json,
                 const absl::optional<std::string>& per_buyer_signals_json,
                 const url::Origin& top_window_origin,
                 const std::string& seller_signals_json,
                 const GURL& browser_signal_render_url,
                 double browser_signal_bid,
                 ReportWinCallback callback) override;
  void ConnectDevToolsAgent(
      mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent) override;

 private:
  struct GenerateBidTask {
    GenerateBidTask();
    ~GenerateBidTask();

    absl::optional<std::string> auction_signals_json;
    absl::optional<std::string> per_buyer_signals_json;
    url::Origin top_window_origin;
    url::Origin seller_origin;
    base::Time auction_start_time;

    // Set while loading is in progress.
    std::unique_ptr<TrustedSignals> trusted_bidding_signals;
    // Results of loading trusted bidding signals.
    std::unique_ptr<TrustedSignals::Result> trusted_bidding_signals_result;
    // Error message returned by attempt to load `trusted_bidding_signals_`.
    // Errors loading it are not fatal, so such errors are cached here and only
    // reported on bid completion.
    absl::optional<std::string> trusted_bidding_signals_error_msg;

    GenerateBidCallback callback;
  };

  using GenerateBidTaskList = std::list<GenerateBidTask>;

  struct ReportWinTask {
    ReportWinTask();
    ~ReportWinTask();

    absl::optional<std::string> auction_signals_json;
    absl::optional<std::string> per_buyer_signals_json;
    url::Origin top_window_origin;
    std::string seller_signals_json;
    GURL browser_signal_render_url;
    double browser_signal_bid;

    ReportWinCallback callback;
  };

  using ReportWinTaskList = std::list<ReportWinTask>;

  // Portion of BidderWorklet that deals with V8 execution, and therefore lives
  // on the v8 thread --- everything except the constructor must be run there.
  class V8State {
   public:
    V8State(scoped_refptr<AuctionV8Helper> v8_helper,
            scoped_refptr<AuctionV8Helper::DebugId> debug_id,
            const GURL& script_source_url,
            base::WeakPtr<BidderWorklet> parent,
            mojom::BiddingInterestGroupPtr bidding_interest_group);

    void SetWorkletScript(WorkletLoader::Result worklet_script);

    // These match the mojom GenerateBidCallback / ReportWinCallback functions,
    // except the errors vectors are passed by value. They're callbacks that
    // must be invoked on the main sequence, and passed to the V8State.
    using GenerateBidCallbackInternal =
        base::OnceCallback<void(mojom::BidderWorkletBidPtr bid,
                                std::vector<std::string> error_msgs)>;
    using ReportWinCallbackInternal =
        base::OnceCallback<void(absl::optional<GURL> report_url,
                                std::vector<std::string> errors)>;

    void ReportWin(const absl::optional<std::string>& auction_signals_json,
                   const absl::optional<std::string>& per_buyer_signals_json,
                   const url::Origin& browser_signal_top_window_origin,
                   const std::string& seller_signals_json,
                   const GURL& browser_signal_render_url,
                   double browser_signal_bid,
                   ReportWinCallbackInternal callback);

    void GenerateBid(
        const absl::optional<std::string>& auction_signals_json,
        const absl::optional<std::string>& per_buyer_signals_json,
        const url::Origin& browser_signal_top_window_origin,
        const url::Origin& browser_signal_seller_origin,
        base::Time auction_start_time,
        std::unique_ptr<TrustedSignals::Result> trusted_bidding_signals_result,
        GenerateBidCallbackInternal callback);

    void ConnectDevToolsAgent(
        mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent);

   private:
    friend class base::DeleteHelper<V8State>;
    ~V8State();

    void FinishInit();

    void PostReportWinCallbackToUserThread(
        ReportWinCallbackInternal callback,
        const absl::optional<GURL>& report_url,
        std::vector<std::string> errors);
    void PostErrorBidCallbackToUserThread(
        GenerateBidCallbackInternal callback,
        std::vector<std::string> error_msgs = std::vector<std::string>());

    static void PostResumeToUserThread(
        base::WeakPtr<BidderWorklet> parent,
        scoped_refptr<base::SequencedTaskRunner> user_thread);

    const scoped_refptr<AuctionV8Helper> v8_helper_;
    const scoped_refptr<AuctionV8Helper::DebugId> debug_id_;
    const base::WeakPtr<BidderWorklet> parent_;
    const scoped_refptr<base::SequencedTaskRunner> user_thread_;

    const mojom::BiddingInterestGroupPtr bidding_interest_group_;

    // Compiled script, not bound to any context. Can be repeatedly bound to
    // different context and executed, without persisting any state.
    v8::Global<v8::UnboundScript> worklet_script_;

    const GURL script_source_url_;

    SEQUENCE_CHECKER(v8_sequence_checker_);
  };

  void ResumeIfPaused();
  void Start();

  void OnScriptDownloaded(WorkletLoader::Result worklet_script,
                          absl::optional<std::string> error_msg);

  void OnTrustedBiddingSignalsDownloaded(
      GenerateBidTaskList::iterator task,
      std::unique_ptr<TrustedSignals::Result> result,
      absl::optional<std::string> error_msg);

  // Checks if the script has been loaded successfully, and the
  // TrustedSignals load has finished (successfully or not). If so, calls
  // generateBid(), and invokes `load_script_and_generate_bid_callback_` with
  // the resulting bid, if any. May only be called once BidderWorklet has
  // successfully loaded.
  void GenerateBidIfReady(GenerateBidTaskList::iterator task);

  void RunReportWin(ReportWinTaskList::iterator task);

  // Fails all pending GenerateBid() and ReportWin() tasks, removing all tasks
  // from both lists.
  void FailAllPendingTasks();

  // Invokes the `callback` of `task` with the provided values, and removes
  // `task` from `generate_bid_tasks_`.
  void DeliverBidCallbackOnUserThread(GenerateBidTaskList::iterator task,
                                      mojom::BidderWorkletBidPtr bid,
                                      std::vector<std::string> error_msgs);

  // Invokes the `callback` of `task` with the provided values, and removes
  // `task` from `report_win_tasks_`.
  void DeliverReportWinOnUserThread(ReportWinTaskList::iterator task,
                                    absl::optional<GURL> report_url,
                                    std::vector<std::string> errors);

  scoped_refptr<base::SequencedTaskRunner> v8_runner_;

  const scoped_refptr<AuctionV8Helper> v8_helper_;
  const scoped_refptr<AuctionV8Helper::DebugId> debug_id_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  bool paused_;

  const GURL script_source_url_;

  // True until `worklet_loader_` has completed loading (successfully or
  // otherwise).
  bool is_loading_ = true;

  std::unique_ptr<WorkletLoader> worklet_loader_;
  bool have_worklet_script_ = false;

  // Values copied from the interest group used to create the BidderWorklet.
  const absl::optional<GURL> trusted_bidding_signals_url_;
  const absl::optional<std::vector<std::string>> trusted_bidding_signals_keys_;

  // Lives on `v8_runner_`. Since it's deleted there via DeleteSoon, tasks can
  // be safely posted from main thread to it with an Unretained pointer.
  std::unique_ptr<V8State, base::OnTaskRunnerDeleter> v8_state_;

  // Pending calls to the corresponding Javascript methods. Only accessed on
  // main thread, but iterators to their elements are bound to callbacks passed
  // to the v8 thread, so these need to be std::lists rather than std::vectors.
  GenerateBidTaskList generate_bid_tasks_;
  ReportWinTaskList report_win_tasks_;

  // Error that occurred while loading the bidder worklet script, if any.
  absl::optional<std::string> load_script_error_msg_;

  SEQUENCE_CHECKER(user_sequence_checker_);

  // Used when posting callbacks back from V8State.
  base::WeakPtrFactory<BidderWorklet> weak_ptr_factory_{this};
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_BIDDER_WORKLET_H_
