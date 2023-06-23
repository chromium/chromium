// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/direct_from_seller_signals_requester.h"
#include "content/services/auction_worklet/public/mojom/auction_shared_storage_host.mojom.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_request_manager.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
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
class CONTENT_EXPORT SellerWorklet : public mojom::SellerWorklet {
 public:
  // Deletes the worklet immediately and resets the SellerWorklet's Mojo pipe
  // with the provided description. See mojo::Receiver::ResetWithReason().
  using ClosePipeCallback =
      base::OnceCallback<void(const std::string& description)>;

  using PrivateAggregationRequests =
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>;

  // Starts loading the worklet script on construction.
  SellerWorklet(
      scoped_refptr<AuctionV8Helper> v8_helper,
      mojo::PendingRemote<mojom::AuctionSharedStorageHost>
          shared_storage_host_remote,
      bool pause_for_debugger_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& decision_logic_url,
      const absl::optional<GURL>& trusted_scoring_signals_url,
      const url::Origin& top_window_origin,
      mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
      absl::optional<uint16_t> experiment_group_id);

  explicit SellerWorklet(const SellerWorklet&) = delete;
  SellerWorklet& operator=(const SellerWorklet&) = delete;

  ~SellerWorklet() override;

  // Sets the callback to be invoked on errors which require closing the pipe.
  // Callback will also immediately delete `this`. Not an argument to
  // constructor because the Mojo ReceiverId needs to be bound to the callback,
  // but can only get that after creating the worklet. Must be called
  // immediately after creating a SellerWorklet.
  void set_close_pipe_callback(ClosePipeCallback close_pipe_callback) {
    close_pipe_callback_ = std::move(close_pipe_callback);
  }

  int context_group_id_for_testing() const;

  // mojom::SellerWorklet implementation:
  void ScoreAd(
      const std::string& ad_metadata_json,
      double bid,
      const absl::optional<blink::AdCurrency>& bid_currency,
      const blink::AuctionConfig::NonSharedParams&
          auction_ad_config_non_shared_params,
      const absl::optional<GURL>& direct_from_seller_seller_signals,
      const absl::optional<GURL>& direct_from_seller_auction_signals,
      mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
      const absl::optional<blink::AdCurrency>& component_expect_bid_currency,
      const url::Origin& browser_signal_interest_group_owner,
      const GURL& browser_signal_render_url,
      const std::vector<GURL>& browser_signal_ad_components,
      uint32_t browser_signal_bidding_duration_msecs,
      const absl::optional<base::TimeDelta> seller_timeout,
      uint64_t trace_id,
      mojo::PendingRemote<auction_worklet::mojom::ScoreAdClient>
          score_ad_client) override;
  void SendPendingSignalsRequests() override;
  void ReportResult(
      const blink::AuctionConfig::NonSharedParams&
          auction_ad_config_non_shared_params,
      const absl::optional<GURL>& direct_from_seller_seller_signals,
      const absl::optional<GURL>& direct_from_seller_auction_signals,
      mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
      const url::Origin& browser_signal_interest_group_owner,
      const absl::optional<std::string>&
          browser_signal_buyer_and_seller_reporting_id,
      const GURL& browser_signal_render_url,
      double browser_signal_bid,
      const absl::optional<blink::AdCurrency>& browser_signal_bid_currency,
      double browser_signal_desirability,
      double browser_signal_highest_scoring_other_bid,
      const absl::optional<blink::AdCurrency>&
          browser_signal_highest_scoring_other_bid_currency,
      auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
          browser_signals_component_auction_report_result_params,
      uint32_t scoring_signals_data_version,
      bool browser_signal_has_data_version,
      uint64_t trace_id,
      ReportResultCallback callback) override;
  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent)
      override;

 private:
  // Contains all data needed for a ScoreAd() call. Destroyed only when its
  // `callback` is invoked.
  struct ScoreAdTask {
    ScoreAdTask();
    ~ScoreAdTask();

    base::CancelableTaskTracker::TaskId task_id =
        base::CancelableTaskTracker::kBadTaskId;

    // These fields all correspond to the arguments of ScoreAd(). They're
    // std::move()ed when calling out to V8State to run Javascript, so are not
    // safe to access after that happens.
    std::string ad_metadata_json;
    double bid;
    absl::optional<blink::AdCurrency> bid_currency;
    blink::AuctionConfig::NonSharedParams auction_ad_config_non_shared_params;
    mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller;
    absl::optional<blink::AdCurrency> component_expect_bid_currency;
    url::Origin browser_signal_interest_group_owner;
    GURL browser_signal_render_url;
    // While these are URLs, it's more concenient to store these as strings
    // rather than GURLs, both for creating a v8 array from, and for sharing
    // ScoringSignals code with BidderWorklets.
    std::vector<std::string> browser_signal_ad_components;
    uint32_t browser_signal_bidding_duration_msecs;
    absl::optional<base::TimeDelta> seller_timeout;
    uint64_t trace_id;

    // Time where tracing for wait_score_ad_deps began.
    base::TimeTicks trace_wait_deps_start;
    // How long various inputs were waited for.
    base::TimeDelta wait_code;
    base::TimeDelta wait_trusted_signals;
    base::TimeDelta wait_direct_from_seller_signals;

    mojo::Remote<auction_worklet::mojom::ScoreAdClient> score_ad_client;

    std::unique_ptr<TrustedSignalsRequestManager::Request>
        trusted_scoring_signals_request;
    scoped_refptr<TrustedSignals::Result> trusted_scoring_signals_result;

    // Error message from downloading trusted scoring signals, if any. Prepended
    // to errors passed to the ScoreAdCallback.
    absl::optional<std::string> trusted_scoring_signals_error_msg;

    // Set while loading is in progress.
    std::unique_ptr<DirectFromSellerSignalsRequester::Request>
        direct_from_seller_request_seller_signals;
    std::unique_ptr<DirectFromSellerSignalsRequester::Request>
        direct_from_seller_request_auction_signals;
    // Results of loading DirectFromSellerSignals.
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_seller_signals;
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_auction_signals;
    // DirectFromSellerSignals errors are fatal, so no error information is
    // stored here.
  };

  using ScoreAdTaskList = std::list<ScoreAdTask>;

  // Contains all data needed for a ReportResult() call. Destroyed only when its
  // `callback` is invoked.
  struct ReportResultTask {
    ReportResultTask();
    ~ReportResultTask();

    // These fields all correspond to the arguments of ReportResult(). They're
    // std::move()ed when calling out to V8State to run Javascript, so are not
    // safe to access after that happens.
    blink::AuctionConfig::NonSharedParams auction_ad_config_non_shared_params;
    mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller;
    url::Origin browser_signal_interest_group_owner;
    absl::optional<std::string> browser_signal_buyer_and_seller_reporting_id;
    GURL browser_signal_render_url;
    double browser_signal_bid;
    absl::optional<blink::AdCurrency> browser_signal_bid_currency;
    double browser_signal_desirability;
    double browser_signal_highest_scoring_other_bid;
    absl::optional<blink::AdCurrency>
        browser_signal_highest_scoring_other_bid_currency;
    auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
        browser_signals_component_auction_report_result_params;
    absl::optional<uint32_t> scoring_signals_data_version;
    uint64_t trace_id;

    // Time where tracing for wait_report_result_deps began.
    base::TimeTicks trace_wait_deps_start;
    // How long various inputs were waited for.
    base::TimeDelta wait_code;
    base::TimeDelta wait_direct_from_seller_signals;

    // Set while loading is in progress.
    std::unique_ptr<DirectFromSellerSignalsRequester::Request>
        direct_from_seller_request_seller_signals;
    std::unique_ptr<DirectFromSellerSignalsRequester::Request>
        direct_from_seller_request_auction_signals;
    // Results of loading DirectFromSellerSignals.
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_seller_signals;
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_auction_signals;
    // DirectFromSellerSignals errors are fatal, so no error information is
    // stored here.

    ReportResultCallback callback;
  };

  using ReportResultTaskList = std::list<ReportResultTask>;

  // Portion of SellerWorklet that deals with V8 execution, and therefore lives
  // on the v8 thread --- everything except the constructor must be run there.
  class V8State {
   public:
    // Match corresponding auction_worklet::mojom::SellerWorklet callback types,
    // except arguments are passed by value. Must be invoked on the user thread.
    // Different signatures protect against passing the wrong callback to
    // V8State, and avoids having to make a copy of the errors vector.
    using ScoreAdCallbackInternal = base::OnceCallback<void(
        double score,
        mojom::RejectReason reject_reason,
        mojom::ComponentAuctionModifiedBidParamsPtr
            component_auction_modified_bid_params,
        absl::optional<double> bid_in_seller_currency,
        absl::optional<uint32_t> scoring_signals_data_version,
        absl::optional<GURL> debug_loss_report_url,
        absl::optional<GURL> debug_win_report_url,
        PrivateAggregationRequests pa_requests,
        base::TimeDelta scoring_latency,
        std::vector<std::string> errors)>;
    using ReportResultCallbackInternal =
        base::OnceCallback<void(absl::optional<std::string> signals_for_winner,
                                absl::optional<GURL> report_url,
                                base::flat_map<std::string, GURL> ad_beacon_map,
                                PrivateAggregationRequests pa_requests,
                                base::TimeDelta reporting_latency,
                                std::vector<std::string> errors)>;

    V8State(
        scoped_refptr<AuctionV8Helper> v8_helper,
        scoped_refptr<AuctionV8Helper::DebugId> debug_id,
        mojo::PendingRemote<mojom::AuctionSharedStorageHost>
            shared_storage_host_remote,
        const GURL& decision_logic_url,
        const absl::optional<GURL>& trusted_scoring_signals_url,
        const url::Origin& top_window_origin,
        mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
        absl::optional<uint16_t> experiment_group_id,
        base::WeakPtr<SellerWorklet> parent);

    void SetWorkletScript(WorkletLoader::Result worklet_script);

    void ScoreAd(
        const std::string& ad_metadata_json,
        double bid,
        const absl::optional<blink::AdCurrency>& bid_currency,
        const blink::AuctionConfig::NonSharedParams&
            auction_ad_config_non_shared_params,
        DirectFromSellerSignalsRequester::Result
            direct_from_seller_result_seller_signals,
        DirectFromSellerSignalsRequester::Result
            direct_from_seller_result_auction_signals,
        scoped_refptr<TrustedSignals::Result> trusted_scoring_signals,
        mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
        const absl::optional<blink::AdCurrency>& component_expect_bid_currency,
        const url::Origin& browser_signal_interest_group_owner,
        const GURL& browser_signal_render_url,
        const std::vector<std::string>& browser_signal_ad_components,
        uint32_t browser_signal_bidding_duration_msecs,
        const absl::optional<base::TimeDelta> seller_timeout,
        uint64_t trace_id,
        base::ScopedClosureRunner cleanup_score_ad_task,
        ScoreAdCallbackInternal callback);

    void ReportResult(
        const blink::AuctionConfig::NonSharedParams&
            auction_ad_config_non_shared_params,
        DirectFromSellerSignalsRequester::Result
            direct_from_seller_result_seller_signals,
        DirectFromSellerSignalsRequester::Result
            direct_from_seller_result_auction_signals,
        mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
        const url::Origin& browser_signal_interest_group_owner,
        const absl::optional<std::string>&
            browser_signal_buyer_and_seller_reporting_id,
        const GURL& browser_signal_render_url,
        double browser_signal_bid,
        const absl::optional<blink::AdCurrency>& browser_signal_bid_currency,
        double browser_signal_desirability,
        double browser_signal_highest_scoring_other_bid,
        const absl::optional<blink::AdCurrency>&
            browser_signal_highest_scoring_other_bid_currency,
        auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
            browser_signals_component_auction_report_result_params,
        absl::optional<uint32_t> scoring_signals_data_version,
        uint64_t trace_id,
        ReportResultCallbackInternal callback);

    void ConnectDevToolsAgent(
        mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent);

   private:
    friend class base::DeleteHelper<V8State>;
    ~V8State();

    void FinishInit(mojo::PendingRemote<mojom::AuctionSharedStorageHost>
                        shared_storage_host_remote);

    // Calls `PostScoreAdCallbackToUserThread`, passing in a 0 score and null
    // for all other values. Used on errors, which cause us to drop win/loss
    // report URLs, even if the methods to set them have been invoked.
    void PostScoreAdCallbackToUserThreadOnError(
        ScoreAdCallbackInternal callback,
        base::TimeDelta scoring_latency,
        std::vector<std::string> errors,
        PrivateAggregationRequests pa_requests = {});

    void PostScoreAdCallbackToUserThread(
        ScoreAdCallbackInternal callback,
        double score,
        mojom::RejectReason reject_reason,
        mojom::ComponentAuctionModifiedBidParamsPtr
            component_auction_modified_bid_params,
        absl::optional<double> bid_in_seller_currency,
        absl::optional<uint32_t> scoring_signals_data_version,
        absl::optional<GURL> debug_loss_report_url,
        absl::optional<GURL> debug_win_report_url,
        PrivateAggregationRequests pa_requests,
        base::TimeDelta scoring_latency,
        std::vector<std::string> errors);

    void PostReportResultCallbackToUserThread(
        ReportResultCallbackInternal callback,
        absl::optional<std::string> signals_for_winner,
        absl::optional<GURL> report_url,
        base::flat_map<std::string, GURL> ad_beacon_map,
        PrivateAggregationRequests pa_requests,
        base::TimeDelta reporting_latency,
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

    const GURL decision_logic_url_;
    const absl::optional<GURL> trusted_scoring_signals_url_;
    const url::Origin top_window_origin_;
    mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state_;
    const absl::optional<uint16_t> experiment_group_id_;

    mojo::Remote<mojom::AuctionSharedStorageHost> shared_storage_host_remote_;

    SEQUENCE_CHECKER(v8_sequence_checker_);
  };

  void ResumeIfPaused();
  void Start();

  void OnDownloadComplete(WorkletLoader::Result worklet_script,
                          absl::optional<std::string> error_msg);
  void MaybeRecordCodeWait();

  // Called when trusted scoring signals have finished downloading, or when
  // there are no scoring signals to download. Starts running scoreAd() on the
  // V8 thread.
  void OnTrustedScoringSignalsDownloaded(
      ScoreAdTaskList::iterator task,
      scoped_refptr<TrustedSignals::Result> result,
      absl::optional<std::string> error_msg);

  // Invoked when the ScoreAdClient associated with `task` is destroyed.
  // Cancels bid generation.
  void OnScoreAdClientDestroyed(ScoreAdTaskList::iterator task);

  void OnDirectFromSellerSellerSignalsDownloadedScoreAd(
      ScoreAdTaskList::iterator task,
      DirectFromSellerSignalsRequester::Result result);

  void OnDirectFromSellerAuctionSignalsDownloadedScoreAd(
      ScoreAdTaskList::iterator task,
      DirectFromSellerSignalsRequester::Result result);

  // Returns true iff all scoreAd()'s prerequisite loading tasks have
  // completed.
  bool IsReadyToScoreAd(const ScoreAdTask& task) const;

  // Checks if the script has been loaded successfully, the
  // DirectFromSellerSignals loads have finished and the TrustedSignals load has
  // finished, if needed (successfully or not). If so, calls scoreAd().
  void ScoreAdIfReady(const ScoreAdTaskList::iterator task);

  void DeliverScoreAdCallbackOnUserThread(
      ScoreAdTaskList::iterator task,
      double score,
      mojom::RejectReason reject_reason,
      mojom::ComponentAuctionModifiedBidParamsPtr
          component_auction_modified_bid_params,
      absl::optional<double> bid_in_seller_currency,
      absl::optional<uint32_t> scoring_signals_data_version,
      absl::optional<GURL> debug_loss_report_url,
      absl::optional<GURL> debug_win_report_url,
      PrivateAggregationRequests pa_requests,
      base::TimeDelta scoring_latency,
      std::vector<std::string> errors);

  // Removes `task` from `score_ad_tasks_` only. Used in case where the
  // V8 work for task was cancelled only (via the `cleanup_score_ad_task`
  // parameter getting destroyed.)
  void CleanUpScoreAdTaskOnUserThread(ScoreAdTaskList::iterator task);

  void OnDirectFromSellerSellerSignalsDownloadedReportResult(
      ReportResultTaskList::iterator task,
      DirectFromSellerSignalsRequester::Result result);

  void OnDirectFromSellerAuctionSignalsDownloadedReportResult(
      ReportResultTaskList::iterator task,
      DirectFromSellerSignalsRequester::Result result);

  // Returns true iff all reportResult()'s prerequisite loading tasks have
  // completed.
  bool IsReadyToReportResult(const ReportResultTask& task) const;

  // Checks if the code is ready, and the DirectFromSellerSignals loads have
  // finished. If so, runs the specified queued ReportResultTask.
  void RunReportResultIfReady(ReportResultTaskList::iterator task);

  void DeliverReportResultCallbackOnUserThread(
      ReportResultTaskList::iterator task,
      absl::optional<std::string> signals_for_winner,
      absl::optional<GURL> report_url,
      base::flat_map<std::string, GURL> ad_beacon_map,
      PrivateAggregationRequests pa_requests,
      base::TimeDelta reporting_latency,
      std::vector<std::string> errors);

  // Returns true if unpaused and the script has loaded.
  bool IsCodeReady() const;

  scoped_refptr<base::SequencedTaskRunner> v8_runner_;
  scoped_refptr<AuctionV8Helper> v8_helper_;
  scoped_refptr<AuctionV8Helper::DebugId> debug_id_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  const GURL script_source_url_;

  // Populated only if `this` was created with a non-null
  // `trusted_scoring_signals_url`.
  std::unique_ptr<TrustedSignalsRequestManager>
      trusted_signals_request_manager_;

  // Used for fetching DirectFromSellerSignals from subresource bundles (and
  // caching responses).
  DirectFromSellerSignalsRequester direct_from_seller_requester_seller_signals_;
  DirectFromSellerSignalsRequester
      direct_from_seller_requester_auction_signals_;

  bool paused_;

  // Pending calls to the corresponding Javascript method. Only accessed on
  // main thread, but iterators to its elements are bound to callbacks passed
  // to the v8 thread, so it needs to be an std::lists rather than an
  // std::vector.
  ScoreAdTaskList score_ad_tasks_;
  ReportResultTaskList report_result_tasks_;

  // Deleted once load has completed.
  std::unique_ptr<WorkletLoader> worklet_loader_;

  // Lives on `v8_runner_`. Since it's deleted there, tasks can be safely
  // posted from main thread to it with an Unretained pointer.
  std::unique_ptr<V8State, base::OnTaskRunnerDeleter> v8_state_;

  ClosePipeCallback close_pipe_callback_;

  // Error that occurred while loading the worklet script, if any.
  absl::optional<std::string> load_script_error_msg_;

  base::CancelableTaskTracker cancelable_task_tracker_;

  SEQUENCE_CHECKER(user_sequence_checker_);

  // Used when posting callbacks back from V8State.
  base::WeakPtrFactory<SellerWorklet> weak_ptr_factory_{this};
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_
