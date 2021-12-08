// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
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

// Represents a seller worklet for FLEDGE
// (https://github.com/WICG/turtledove/blob/main/FLEDGE.md). Loads and runs the
// seller worklet's Javascript.
class SellerWorklet : public mojom::SellerWorklet {
 public:
  // Starts loading the worklet script on construction. Callback will be invoked
  // asynchronously once the data has been fetched or an error has occurred.
  SellerWorklet(scoped_refptr<AuctionV8Helper> v8_helper,
                bool pause_for_debugger_on_start,
                mojo::PendingRemote<network::mojom::URLLoaderFactory>
                    pending_url_loader_factory,
                const GURL& script_source_url,
                mojom::AuctionWorkletService::LoadSellerWorkletCallback
                    load_worklet_callback);

  explicit SellerWorklet(const SellerWorklet&) = delete;
  SellerWorklet& operator=(const SellerWorklet&) = delete;

  ~SellerWorklet() override;

  int context_group_id_for_testing() const;

  // mojom::SellerWorklet implementation:
  void ScoreAd(const std::string& ad_metadata_json,
               double bid,
               blink::mojom::AuctionAdConfigPtr auction_config,
               const url::Origin& browser_signal_top_window_origin,
               const url::Origin& browser_signal_interest_group_owner,
               const GURL& browser_signal_render_url,
               const std::vector<GURL>& browser_signal_ad_components,
               uint32_t browser_signal_bidding_duration_msecs,
               ScoreAdCallback callback) override;
  void ReportResult(blink::mojom::AuctionAdConfigPtr auction_config,
                    const url::Origin& browser_signal_top_window_origin,
                    const url::Origin& browser_signal_interest_group_owner,
                    const GURL& browser_signal_render_url,
                    double browser_signal_bid,
                    double browser_signal_desirability,
                    ReportResultCallback callback) override;
  void ConnectDevToolsAgent(
      mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent) override;

 private:
  // Contains all data needed for a ScoreAd() call. Destroyed only when its
  // `callback` is invoked.
  struct ScoreAdTask {
    ScoreAdTask();
    ~ScoreAdTask();

    // These fields all correspond to the arguments of ScoreAd(). They're
    // std::move()ed when calling out to V8State to run Javascript, so are not
    // safe to access after that happens.
    std::string ad_metadata_json;
    double bid;
    blink::mojom::AuctionAdConfigPtr auction_config;
    url::Origin browser_signal_top_window_origin;
    url::Origin browser_signal_interest_group_owner;
    GURL browser_signal_render_url;
    // While these are URLs, it's more concenient to store these as strings
    // rather than GURLs, both for creating a v8 array from, and for sharing
    // ScoringSignals code with BidderWorklets.
    std::vector<std::string> browser_signal_ad_components;
    uint32_t browser_signal_bidding_duration_msecs;

    ScoreAdCallback callback;

    std::unique_ptr<TrustedSignals> trusted_scoring_signals;

    // Error message from downloading trusted scoring signals, if any. Prepended
    // to errors passed to the ScoreAdCallback.
    absl::optional<std::string> trusted_scoring_signals_error_msg;
  };

  using ScoreAdTaskList = std::list<ScoreAdTask>;

  // Portion of SellerWorklet that deals with V8 execution, and therefore lives
  // on the v8 thread --- everything except the constructor must be run there.
  class V8State {
   public:
    // Matches auction_worklet::mojom::SellerWorklet::ScoreAdCallback,
    // except the errors vectors are passed by value. Must be invoked on the
    // user thread. Different signitures also protects against passing the
    // wrong callback to V8State.
    using ScoreAdCallbackInternal =
        base::OnceCallback<void(double score, std::vector<std::string> errors)>;

    V8State(scoped_refptr<AuctionV8Helper> v8_helper,
            scoped_refptr<AuctionV8Helper::DebugId> debug_id,
            GURL script_source_url,
            base::WeakPtr<SellerWorklet> parent);

    void SetWorkletScript(WorkletLoader::Result worklet_script);

    void ScoreAd(
        const std::string& ad_metadata_json,
        double bid,
        blink::mojom::AuctionAdConfigPtr auction_config,
        std::unique_ptr<TrustedSignals::Result> trusted_scoring_signals,
        const url::Origin& browser_signal_top_window_origin,
        const url::Origin& browser_signal_interest_group_owner,
        const GURL& browser_signal_render_url,
        const std::vector<std::string>& browser_signal_ad_components,
        uint32_t browser_signal_bidding_duration_msecs,
        ScoreAdCallbackInternal callback);

    void ReportResult(blink::mojom::AuctionAdConfigPtr auction_config,
                      const url::Origin& browser_signal_top_window_origin,
                      const url::Origin& browser_signal_interest_group_owner,
                      const GURL& browser_signal_render_url,
                      double browser_signal_bid,
                      double browser_signal_desirability,
                      ReportResultCallback callback);

    void ConnectDevToolsAgent(
        mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent);

   private:
    friend class base::DeleteHelper<V8State>;
    ~V8State();

    void FinishInit();

    void PostScoreAdCallbackToUserThread(ScoreAdCallbackInternal callback,
                                         double score,
                                         std::vector<std::string> errors);

    void PostReportResultCallbackToUserThread(
        ReportResultCallback callback,
        absl::optional<std::string> signals_for_winner,
        absl::optional<GURL> report_url,
        std::vector<std::string> errors);

    static void PostResumeToUserThread(
        base::WeakPtr<SellerWorklet> parent,
        scoped_refptr<base::SequencedTaskRunner> user_thread);

    const scoped_refptr<AuctionV8Helper> v8_helper_;
    const scoped_refptr<AuctionV8Helper::DebugId> debug_id_;
    const base::WeakPtr<SellerWorklet> parent_;
    const scoped_refptr<base::SequencedTaskRunner> user_thread_;

    // Compiled script, not bound to any context. Can be repeatedly bound to
    // different context and executed, without persisting any state.
    v8::Global<v8::UnboundScript> worklet_script_;

    const GURL script_source_url_;

    SEQUENCE_CHECKER(v8_sequence_checker_);
  };

  void ResumeIfPaused();
  void Start();

  void OnDownloadComplete(WorkletLoader::Result worklet_script,
                          absl::optional<std::string> error_msg);

  // Called when trusted scoring signals have finished downloading, or when
  // there are no scoring signals to download. Starts running scoreAd() on the
  // V8 thread.
  void OnTrustedScoringSignalsDownloaded(
      ScoreAdTaskList::iterator task,
      std::unique_ptr<TrustedSignals::Result> result,
      absl::optional<std::string> error_msg);

  void DeliverScoreAdCallbackOnUserThread(ScoreAdTaskList::iterator task,
                                          double score,
                                          std::vector<std::string> errors);

  void DeliverReportResultCallbackOnUserThread(
      ReportResultCallback callback,
      absl::optional<std::string> signals_for_winner,
      absl::optional<GURL> report_url,
      std::vector<std::string> errors);

  scoped_refptr<base::SequencedTaskRunner> v8_runner_;
  scoped_refptr<AuctionV8Helper> v8_helper_;
  scoped_refptr<AuctionV8Helper::DebugId> debug_id_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  const GURL script_source_url_;

  bool paused_;

  // Pending calls to the corresponding Javascript method. Only accessed on
  // main thread, but iterators to its elements are bound to callbacks passed
  // to the v8 thread, so it needs to be an std::lists rather than an
  // std::vector.
  ScoreAdTaskList score_ad_tasks_;

  std::unique_ptr<WorkletLoader> worklet_loader_;

  // Lives on `v8_runner_`. Since it's deleted there, tasks can be safely
  // posted from main thread to it with an Unretained pointer.
  std::unique_ptr<V8State, base::OnTaskRunnerDeleter> v8_state_;

  mojom::AuctionWorkletService::LoadSellerWorkletCallback
      load_worklet_callback_;

  SEQUENCE_CHECKER(user_sequence_checker_);

  // Used when posting callbacks back from V8State.
  base::WeakPtrFactory<SellerWorklet> weak_ptr_factory_{this};
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_
