// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <optional>
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
#include "content/services/auction_worklet/context_recycler.h"
#include "content/services/auction_worklet/direct_from_seller_signals_requester.h"
#include "content/services/auction_worklet/public/mojom/auction_shared_storage_host.mojom.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_request_manager.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
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

  using RealTimeReportingContributions =
      std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>;

  using GetNextThreadIndexCallback = base::RepeatingCallback<size_t()>;

  // Classification of how trusted signals related to this worklet.
  // This is used for histograms, so entries should not be reordered or
  // otherwise renumbered.
  enum class SignalsOriginRelation {
    kNoTrustedSignals,
    kSameOriginSignals,

    // If trusted signals are cross-origin, their classification starts at
    // kUnknownPermissionCrossOriginSignals and gets changed to permitted or
    // forbidden once the permission header is received (or is found missing).
    kUnknownPermissionCrossOriginSignals,
    kPermittedCrossOriginSignals,
    kForbiddenCrossOriginSignals,

    kMaxValue = kForbiddenCrossOriginSignals
  };

  // Starts loading the worklet script on construction.
  SellerWorklet(
      std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers,
      std::vector<mojo::PendingRemote<mojom::AuctionSharedStorageHost>>
          shared_storage_hosts,
      bool pause_for_debugger_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
          auction_network_events_handler,
      const GURL& decision_logic_url,
      const std::optional<GURL>& trusted_scoring_signals_url,
      const url::Origin& top_window_origin,
      mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
      std::optional<uint16_t> experiment_group_id,
      mojom::TrustedSignalsPublicKeyPtr public_key,
      GetNextThreadIndexCallback next_thread_index_callback);

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

  std::vector<int> context_group_ids_for_testing() const;

  // mojom::SellerWorklet implementation:
  void ScoreAd(
      const std::string& ad_metadata_json,
      double bid,
      const std::optional<blink::AdCurrency>& bid_currency,
      const blink::AuctionConfig::NonSharedParams&
          auction_ad_config_non_shared_params,
      const std::optional<GURL>& direct_from_seller_seller_signals,
      const std::optional<std::string>&
          direct_from_seller_seller_signals_header_ad_slot,
      const std::optional<GURL>& direct_from_seller_auction_signals,
      const std::optional<std::string>&
          direct_from_seller_auction_signals_header_ad_slot,
      mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
      const std::optional<blink::AdCurrency>& component_expect_bid_currency,
      const url::Origin& browser_signal_interest_group_owner,
      const GURL& browser_signal_render_url,
      const std::optional<std::string>&
          browser_signal_selected_buyer_and_seller_reporting_id,
      const std::optional<std::string>&
          browser_signal_buyer_and_seller_reporting_id,
      const std::vector<GURL>& browser_signal_ad_components,
      uint32_t browser_signal_bidding_duration_msecs,
      const std::optional<blink::AdSize>& browser_signal_render_size,
      bool browser_signal_for_debugging_only_in_cooldown_or_lockout,
      const std::optional<base::TimeDelta> seller_timeout,
      uint64_t trace_id,
      const url::Origin& bidder_joining_origin,
      mojo::PendingRemote<auction_worklet::mojom::ScoreAdClient>
          score_ad_client) override;
  void SendPendingSignalsRequests() override;
  void ReportResult(
      const blink::AuctionConfig::NonSharedParams&
          auction_ad_config_non_shared_params,
      const std::optional<GURL>& direct_from_seller_seller_signals,
      const std::optional<std::string>&
          direct_from_seller_seller_signals_header_ad_slot,
      const std::optional<GURL>& direct_from_seller_auction_signals,
      const std::optional<std::string>&
          direct_from_seller_auction_signals_header_ad_slot,
      mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
      const url::Origin& browser_signal_interest_group_owner,
      const std::optional<std::string>&
          browser_signal_buyer_and_seller_reporting_id,
      const std::optional<std::string>&
          browser_signal_selected_buyer_and_seller_reporting_id,
      const GURL& browser_signal_render_url,
      double browser_signal_bid,
      const std::optional<blink::AdCurrency>& browser_signal_bid_currency,
      double browser_signal_desirability,
      double browser_signal_highest_scoring_other_bid,
      const std::optional<blink::AdCurrency>&
          browser_signal_highest_scoring_other_bid_currency,
      auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
          browser_signals_component_auction_report_result_params,
      std::optional<uint32_t> scoring_signals_data_version,
      uint64_t trace_id,
      ReportResultCallback callback) override;
  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
      uint32_t thread_index) override;

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
    std::optional<blink::AdCurrency> bid_currency;
    blink::AuctionConfig::NonSharedParams auction_ad_config_non_shared_params;
    mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller;
    std::optional<blink::AdCurrency> component_expect_bid_currency;
    url::Origin browser_signal_interest_group_owner;
    url::Origin bidder_joining_origin;
    GURL browser_signal_render_url;
    std::optional<std::string>
        browser_signal_selected_buyer_and_seller_reporting_id;
    std::optional<std::string> browser_signal_buyer_and_seller_reporting_id;
    // While these are URLs, it's more convenient to store these as strings
    // rather than GURLs, both for creating a v8 array from, and for sharing
    // ScoringSignals code with BidderWorklets.
    std::vector<std::string> browser_signal_ad_components;
    uint32_t browser_signal_bidding_duration_msecs;
    std::optional<blink::AdSize> browser_signal_render_size;
    bool browser_signal_for_debugging_only_in_cooldown_or_lockout;
    std::optional<base::TimeDelta> seller_timeout;
    uint64_t trace_id;

    // Time where tracing for wait_score_ad_deps began.
    base::TimeTicks trace_wait_deps_start;
    // How long various inputs were waited for.
    base::TimeDelta wait_code;
    base::TimeDelta wait_trusted_signals;
    base::TimeDelta wait_direct_from_seller_signals;

    // Time where the SellerWorklet finished waiting for ScoreAd dependencies,
    // used to compute start and end times for latency phase UKMs.
    base::TimeTicks score_ad_start_time;

    mojo::Remote<auction_worklet::mojom::ScoreAdClient> score_ad_client;

    std::unique_ptr<TrustedSignalsRequestManager::Request>
        trusted_scoring_signals_request;
    scoped_refptr<TrustedSignals::Result> trusted_scoring_signals_result;

    // True if failed loading valid trusted scoring signals.
    bool trusted_bidding_signals_fetch_failed = false;

    // Error message from downloading trusted scoring signals, if any. Prepended
    // to errors passed to the ScoreAdCallback.
    std::optional<std::string> trusted_scoring_signals_error_msg;

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

    // Header-based DirectFromSellerSignals.
    std::optional<std::string> direct_from_seller_seller_signals_header_ad_slot;
    std::optional<std::string>
        direct_from_seller_auction_signals_header_ad_slot;
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
    std::optional<std::string> browser_signal_buyer_and_seller_reporting_id;
    std::optional<std::string>
        browser_signal_selected_buyer_and_seller_reporting_id;
    GURL browser_signal_render_url;
    double browser_signal_bid;
    std::optional<blink::AdCurrency> browser_signal_bid_currency;
    double browser_signal_desirability;
    double browser_signal_highest_scoring_other_bid;
    std::optional<blink::AdCurrency>
        browser_signal_highest_scoring_other_bid_currency;
    auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
        browser_signals_component_auction_report_result_params;
    std::optional<uint32_t> scoring_signals_data_version;
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

    // Header-based DirectFromSellerSignals.
    std::optional<std::string> direct_from_seller_seller_signals_header_ad_slot;
    std::optional<std::string>
        direct_from_seller_auction_signals_header_ad_slot;

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
        std::optional<double> bid_in_seller_currency,
        std::optional<uint32_t> scoring_signals_data_version,
        std::optional<GURL> debug_loss_report_url,
        std::optional<GURL> debug_win_report_url,
        PrivateAggregationRequests pa_requests,
        RealTimeReportingContributions real_time_contributions,
        base::TimeDelta scoring_latency,
        bool script_timed_out,
        std::vector<std::string> errors)>;
    using ReportResultCallbackInternal =
        base::OnceCallback<void(std::optional<std::string> signals_for_winner,
                                std::optional<GURL> report_url,
                                base::flat_map<std::string, GURL> ad_beacon_map,
                                PrivateAggregationRequests pa_requests,
                                base::TimeDelta reporting_latency,
                                bool script_timed_out,
                                std::vector<std::string> errors)>;

    V8State(
        scoped_refptr<AuctionV8Helper> v8_helper,
        scoped_refptr<AuctionV8Helper::DebugId> debug_id,
        mojo::PendingRemote<mojom::AuctionSharedStorageHost>
            shared_storage_host_remote,
        const GURL& decision_logic_url,
        const std::optional<GURL>& trusted_scoring_signals_url,
        const std::optional<url::Origin>& trusted_scoring_signals_origin,
        const url::Origin& top_window_origin,
        mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
        std::optional<uint16_t> experiment_group_id,
        base::WeakPtr<SellerWorklet> parent);

    void SetWorkletScript(WorkletLoader::Result worklet_script,
                          SignalsOriginRelation trusted_signals_relation);

    void ScoreAd(
        const std::string& ad_metadata_json,
        double bid,
        const std::optional<blink::AdCurrency>& bid_currency,
        const blink::AuctionConfig::NonSharedParams&
            auction_ad_config_non_shared_params,
        DirectFromSellerSignalsRequester::Result
            direct_from_seller_result_seller_signals,
        const std::optional<std::string>&
            direct_from_seller_seller_signals_header_ad_slot,
        DirectFromSellerSignalsRequester::Result
            direct_from_seller_result_auction_signals,
        const std::optional<std::string>&
            direct_from_seller_auction_signals_header_ad_slot,
        scoped_refptr<TrustedSignals::Result> trusted_scoring_signals,
        bool trusted_scoring_signals_fetch_failed,
        mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
        const std::optional<blink::AdCurrency>& component_expect_bid_currency,
        const url::Origin& browser_signal_interest_group_owner,
        const GURL& browser_signal_render_url,
        const std::optional<std::string>&
            browser_signal_selected_buyer_and_seller_reporting_id,
        const std::optional<std::string>&
            browser_signal_buyer_and_seller_reporting_id,
        const std::vector<std::string>& browser_signal_ad_components,
        uint32_t browser_signal_bidding_duration_msecs,
        const std::optional<blink::AdSize>& browser_signal_render_size,
        bool browser_signal_for_debugging_only_in_cooldown_or_lockout,
        const std::optional<base::TimeDelta> seller_timeout,
        uint64_t trace_id,
        base::ScopedClosureRunner cleanup_score_ad_task,
        base::TimeTicks task_enqueued_time,
        ScoreAdCallbackInternal callback);

    void ReportResult(
        const blink::AuctionConfig::NonSharedParams&
            auction_ad_config_non_shared_params,
        DirectFromSellerSignalsRequester::Result
            direct_from_seller_result_seller_signals,
        const std::optional<std::string>&
            direct_from_seller_seller_signals_header_ad_slot,
        DirectFromSellerSignalsRequester::Result
            direct_from_seller_result_auction_signals,
        const std::optional<std::string>&
            direct_from_seller_auction_signals_header_ad_slot,
        mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
        const url::Origin& browser_signal_interest_group_owner,
        const std::optional<std::string>&
            browser_signal_buyer_and_seller_reporting_id,
        const std::optional<std::string>&
            browser_signal_selected_buyer_and_seller_reporting_id,
        const GURL& browser_signal_render_url,
        double browser_signal_bid,
        const std::optional<blink::AdCurrency>& browser_signal_bid_currency,
        double browser_signal_desirability,
        double browser_signal_highest_scoring_other_bid,
        const std::optional<blink::AdCurrency>&
            browser_signal_highest_scoring_other_bid_currency,
        auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
            browser_signals_component_auction_report_result_params,
        std::optional<uint32_t> scoring_signals_data_version,
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
        bool script_timed_out,
        std::vector<std::string> errors,
        PrivateAggregationRequests pa_requests = {},
        RealTimeReportingContributions real_time_contributions = {});

    void PostScoreAdCallbackToUserThread(
        ScoreAdCallbackInternal callback,
        double score,
        mojom::RejectReason reject_reason,
        mojom::ComponentAuctionModifiedBidParamsPtr
            component_auction_modified_bid_params,
        std::optional<double> bid_in_seller_currency,
        std::optional<uint32_t> scoring_signals_data_version,
        std::optional<GURL> debug_loss_report_url,
        std::optional<GURL> debug_win_report_url,
        PrivateAggregationRequests pa_requests,
        RealTimeReportingContributions real_time_contributions,
        base::TimeDelta scoring_latency,
        bool script_timed_out,
        std::vector<std::string> errors);

    void PostReportResultCallbackToUserThread(
        ReportResultCallbackInternal callback,
        std::optional<std::string> signals_for_winner,
        std::optional<GURL> report_url,
        base::flat_map<std::string, GURL> ad_beacon_map,
        PrivateAggregationRequests pa_requests,
        base::TimeDelta reporting_latency,
        bool script_timed_out,
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
    const std::optional<GURL> trusted_scoring_signals_url_;
    const std::optional<url::Origin> trusted_scoring_signals_origin_;
    SignalsOriginRelation trusted_signals_relation_ =
        SignalsOriginRelation::kNoTrustedSignals;
    const url::Origin top_window_origin_;
    mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state_;
    const std::optional<uint16_t> experiment_group_id_;

    mojo::Remote<mojom::AuctionSharedStorageHost> shared_storage_host_remote_;

    // If `kFledgeAlwaysReuseSellerContext` is enabled, use this pointer to
    // store our `ContextRecycler`. This `ContextRecycler` will be used on all
    // calls to `ScoreAd`, but not for `ReportResult`. If
    // `kFledgeAlwaysReuseSellerContext` is disabled, a fresh `ContextRecycler`
    // will be created as needed.
    std::unique_ptr<ContextRecycler> context_recycler_for_context_reuse_;

    SEQUENCE_CHECKER(v8_sequence_checker_);
  };

  void ResumeIfPaused();
  void Start();

  void OnDownloadComplete(std::vector<WorkletLoader::Result> worklet_scripts,
                          std::optional<std::string> error_msg);
  void MaybeRecordCodeWait();

  void OnGotCrossOriginTrustedSignalsPermissions(
      std::vector<url::Origin> permit_origins);

  // Starts fetching signals for `score_ad_task`.
  void StartFetchingSignalsForTask(ScoreAdTaskList::iterator score_ad_task);

  // Called when trusted scoring signals have finished downloading, or when
  // there are no scoring signals to download. Starts running scoreAd() on the
  // V8 thread.
  void OnTrustedScoringSignalsDownloaded(
      ScoreAdTaskList::iterator task,
      scoped_refptr<TrustedSignals::Result> result,
      std::optional<std::string> error_msg);

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
      std::optional<double> bid_in_seller_currency,
      std::optional<uint32_t> scoring_signals_data_version,
      std::optional<GURL> debug_loss_report_url,
      std::optional<GURL> debug_win_report_url,
      PrivateAggregationRequests pa_requests,
      RealTimeReportingContributions real_time_contributions,
      base::TimeDelta scoring_latency,
      bool script_timed_out,
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
      std::optional<std::string> signals_for_winner,
      std::optional<GURL> report_url,
      base::flat_map<std::string, GURL> ad_beacon_map,
      PrivateAggregationRequests pa_requests,
      base::TimeDelta reporting_latency,
      bool script_timed_out,
      std::vector<std::string> errors);

  // Returns true if unpaused and the script has loaded.
  bool IsCodeReady() const;

  std::vector<scoped_refptr<base::SequencedTaskRunner>> v8_runners_;
  std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers_;
  std::vector<scoped_refptr<AuctionV8Helper::DebugId>> debug_ids_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  const GURL script_source_url_;
  mojom::TrustedSignalsPublicKeyPtr public_key_;

  // Populated only if `this` was created with a non-null
  // `trusted_scoring_signals_url`.
  std::unique_ptr<TrustedSignalsRequestManager>
      trusted_signals_request_manager_;

  // If any trusted signals requests were deferred because of waiting
  // on trusted signals classification, this is the first time it
  // happened.
  std::optional<base::TimeTicks> first_deferred_trusted_signals_time_;

  const std::optional<url::Origin> trusted_scoring_signals_origin_;
  SignalsOriginRelation trusted_signals_relation_ =
      SignalsOriginRelation::kNoTrustedSignals;

  // Used for fetching DirectFromSellerSignals from subresource bundles (and
  // caching responses).
  DirectFromSellerSignalsRequester direct_from_seller_requester_seller_signals_;
  DirectFromSellerSignalsRequester
      direct_from_seller_requester_auction_signals_;

  bool paused_;

  size_t resumed_count_ = 0;

  // Pending calls to the corresponding Javascript method. Only accessed on
  // main thread, but iterators to its elements are bound to callbacks passed
  // to the v8 thread, so it needs to be an std::lists rather than an
  // std::vector.
  ScoreAdTaskList score_ad_tasks_;
  ReportResultTaskList report_result_tasks_;

  // Deleted once load has completed.
  std::unique_ptr<WorkletLoader> worklet_loader_;
  base::TimeTicks code_download_start_;
  std::optional<base::TimeDelta> js_fetch_latency_;

  // Lives on `v8_runners_`. Since it's deleted there, tasks can be safely
  // posted from main thread to it with an Unretained pointer.
  std::vector<std::unique_ptr<V8State, base::OnTaskRunnerDeleter>> v8_state_;

  ClosePipeCallback close_pipe_callback_;

  // Error that occurred while loading the worklet script, if any.
  std::optional<std::string> load_script_error_msg_;

  base::CancelableTaskTracker cancelable_task_tracker_;

  mojo::Remote<auction_worklet::mojom::AuctionNetworkEventsHandler>
      auction_network_events_handler_;

  GetNextThreadIndexCallback get_next_thread_index_callback_;

  SEQUENCE_CHECKER(user_sequence_checker_);

  // Used when posting callbacks back from V8State.
  base::WeakPtrFactory<SellerWorklet> weak_ptr_factory_{this};
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_
