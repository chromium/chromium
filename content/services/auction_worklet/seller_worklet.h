// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
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

  // Warning: The caller may need to spin the event loop for this to get
  // initialized to a value different from kNoDebugContextGroupId.
  int context_group_id_for_testing() const { return context_group_id_; }

  // mojom::SellerWorklet implementation:
  void ScoreAd(const std::string& ad_metadata_json,
               double bid,
               blink::mojom::AuctionAdConfigPtr auction_config,
               const url::Origin& browser_signal_top_window_origin,
               const url::Origin& browser_signal_interest_group_owner,
               const std::string& browser_signal_ad_render_fingerprint,
               uint32_t browser_signal_bidding_duration_msecs,
               ScoreAdCallback callback) override;
  void ReportResult(blink::mojom::AuctionAdConfigPtr auction_config,
                    const url::Origin& browser_signal_top_window_origin,
                    const url::Origin& browser_signal_interest_group_owner,
                    const GURL& browser_signal_render_url,
                    const std::string& browser_signal_ad_render_fingerprint,
                    double browser_signal_bid,
                    double browser_signal_desirability,
                    ReportResultCallback callback) override;
  void ConnectDevToolsAgent(
      mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent) override;

 private:
  // Portion of SellerWorklet that deals with V8 execution, and therefore lives
  // on the v8 thread --- everything except the constructor must be run there.
  class V8State {
   public:
    V8State(scoped_refptr<AuctionV8Helper> v8_helper,
            GURL script_source_url,
            base::WeakPtr<SellerWorklet> parent);

    void SetWorkletScript(WorkletLoader::Result worklet_script);

    void ScoreAd(const std::string& ad_metadata_json,
                 double bid,
                 blink::mojom::AuctionAdConfigPtr auction_config,
                 const url::Origin& browser_signal_top_window_origin,
                 const url::Origin& browser_signal_interest_group_owner,
                 const std::string& browser_signal_ad_render_fingerprint,
                 uint32_t browser_signal_bidding_duration_msecs,
                 ScoreAdCallback callback);

    void ReportResult(blink::mojom::AuctionAdConfigPtr auction_config,
                      const url::Origin& browser_signal_top_window_origin,
                      const url::Origin& browser_signal_interest_group_owner,
                      const GURL& browser_signal_render_url,
                      const std::string& browser_signal_ad_render_fingerprint,
                      double browser_signal_bid,
                      double browser_signal_desirability,
                      ReportResultCallback callback);

    void ConnectDevToolsAgent(
        mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent);

   private:
    friend class base::DeleteHelper<V8State>;
    ~V8State();

    void FinishInit();

    void PostScoreAdCallbackToUserThread(ScoreAdCallback callback,
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
    const base::WeakPtr<SellerWorklet> parent_;
    const scoped_refptr<base::SequencedTaskRunner> user_thread_;

    // Compiled script, not bound to any context. Can be repeatedly bound to
    // different context and executed, without persisting any state.
    v8::Global<v8::UnboundScript> worklet_script_;

    const GURL script_source_url_;

    int context_group_id_;

    SEQUENCE_CHECKER(v8_sequence_checker_);
  };

  void ResumeIfPaused();
  void StartIfReady();

  void OnDownloadComplete(WorkletLoader::Result worklet_script,
                          absl::optional<std::string> error_msg);

  void DeliverContextGroupIdOnUserThread(int context_group_id);
  void DeliverScoreAdCallbackOnUserThread(ScoreAdCallback callback,
                                          double score,
                                          std::vector<std::string> errors);
  void DeliverReportResultCallbackOnUserThread(
      ReportResultCallback callback,
      absl::optional<std::string> signals_for_winner,
      absl::optional<GURL> report_url,
      std::vector<std::string> errors);

  scoped_refptr<base::SequencedTaskRunner> v8_runner_;

  // Kept around until Start().
  scoped_refptr<AuctionV8Helper> v8_helper_;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_url_loader_factory_;

  const GURL script_source_url_;
  bool paused_;

  // `context_group_id_` starts at kNoDebugContextGroupId, but then gets
  // initialized after some thread hops.
  int context_group_id_;

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
