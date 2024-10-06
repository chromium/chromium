// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_worklet_manager.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/not_fatal_until.h"
#include "base/strings/strcat.h"
#include "content/browser/interest_group/auction_metrics_recorder.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/auction_shared_storage_host.h"
#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/subresource_url_authorizations.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_shared_storage_host.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// If on, worklet assignment/failure callbacks will be executed in chunks rather
// than all at once.
BASE_FEATURE(kFledgeSplitUpWorkletAssignment,
             "FledgeSplitUpWorkletAssignment",
             base::FEATURE_ENABLED_BY_DEFAULT);

// We use sequence numbers with handles to make sure they are assigned in FIFO
// order.
using HandleKey = std::pair<uint64_t, AuctionWorkletManager::WorkletHandle*>;

auction_worklet::mojom::AuctionWorkletPermissionsPolicyStatePtr
GetAuctionWorkletPermissionsPolicyState(RenderFrameHostImpl* auction_runner_rfh,
                                        const GURL& worklet_script_url) {
  const blink::PermissionsPolicy* permissions_policy =
      auction_runner_rfh->permissions_policy();

  url::Origin worklet_origin = url::Origin::Create(worklet_script_url);

  return auction_worklet::mojom::AuctionWorkletPermissionsPolicyState::New(
      permissions_policy->IsFeatureEnabledForOrigin(
          blink::mojom::PermissionsPolicyFeature::kPrivateAggregation,
          worklet_origin),
      permissions_policy->IsFeatureEnabledForOrigin(
          blink::mojom::PermissionsPolicyFeature::kSharedStorage,
          worklet_origin));
}

}  // namespace

const size_t AuctionWorkletManager::kBatchSize;

FrameTreeNodeId AuctionWorkletManager::GetFrameTreeNodeID() {
  return delegate_->GetFrame()->frame_tree_node()->frame_tree_node_id();
}

class AuctionWorkletManager::WorkletOwner
    : public base::RefCounted<AuctionWorkletManager::WorkletOwner> {
 public:
  // Attempts to immediately create a worklet. If that fails, the WorkletOwner
  // will immediately start waiting for a process to be available, and once one
  // is, create a worklet, informing all associated WorkletHandles.
  //
  // If this is for a bidder workelt, `number_of_bidder_threads` specifies
  // the number of threads to allocate to the bidder.
  WorkletOwner(AuctionWorkletManager* worklet_manager,
               WorkletKey worklet_info,
               size_t number_of_bidder_threads);

  // Registers/unregisters a WorkletHandle for the worklet `this` owns.
  void RegisterHandle(HandleKey handle);
  void UnregisterHandle(HandleKey handle);

  uint64_t GetNextSeqNum() { return next_handle_seq_num_++; }

  auction_worklet::mojom::BidderWorklet* bidder_worklet() {
    DCHECK(bidder_worklet_);
    return bidder_worklet_.get();
  }

  auction_worklet::mojom::SellerWorklet* seller_worklet() {
    DCHECK(seller_worklet_);
    return seller_worklet_.get();
  }

  const WorkletKey& worklet_info() const { return worklet_info_; }

  // Whether or not a worklet has been created. Once a worklet has been created,
  // always returns true, even after disconnect or error.
  bool worklet_created() const {
    return bidder_worklet_.is_bound() || seller_worklet_.is_bound();
  }

  SubresourceUrlAuthorizations* subresource_url_authorizations() {
    if (!url_loader_factory_proxy_) {
      return nullptr;
    }
    return &url_loader_factory_proxy_->subresource_url_authorizations();
  }

  std::vector<std::string> ComputeDevtoolsAuctionIds();

  // Adds `auction_metrics_recorder` to the list of AuctionMetricsRecorders
  // on which this will call OnWorkletReady when this worklet is ready.
  // If this worklet is already ready when this is called, it'll call it
  // immediately instead.
  void NotifyAuctionMetricsRecorderWhenReady(
      AuctionMetricsRecorder* auction_metrics_recorder);

 private:
  friend class base::RefCounted<WorkletOwner>;

  ~WorkletOwner();

  void MaybeQueueNotifications();

  void DispatchSomeNotifications();
  void DispatchSomeSuccessNotifications();
  void DispatchSomeFailureNotifications();

  // Called if the worklet becomes unusable. This happens on destruction (once
  // all refs have been released) or when the Mojo pipe is closed. Removes
  // `this` from `worklet_manager_`, so that future requests using the same
  // `worklet_info` will trigger creation of a new worklet, rather than trying
  // to use the unusable one.
  void WorkletNoLongerUsable();

  // Check if the process is assigned, then attempt to load the worklet after
  // `trusted_signals_kvv2_public_key_` is fetched.
  void OnTrustedSignalsKVv2KeyFetched(
      size_t number_of_bidder_threads,
      base::expected<BiddingAndAuctionServerKey, std::string> key_or_error);

  // Called once the AuctionProcessManager provides a process to load a worklet
  // in. If feature `kFledgeTrustedSignalsKVv2Support` is enabled, it will be
  // interrupted if `trusted_signals_kvv2_public_key_` is still in fetching
  // progress.
  void OnProcessAssigned(size_t number_of_bidder_threads);

  // Immediately load the worklet and inform WorkletHandles. For a bidder
  // worklet, `number_of_bidder_threads` specifies the number of threads to
  // allocate to the bidder. When the `kFledgeTrustedSignalsKVv2Support` feature
  // is enabled, this will only be executed when the process is assigned, and
  // `trusted_signals_kvv2_public_key_` is fetched.
  void LoadWorkletIfReady(size_t number_of_bidder_threads);

  // Called when a PID for a worklet process thread has a PID assigned, a proxy
  // for its readiness to begin processing requests. This is used to record
  // the OnWorkletReady event to the AuctionMetricsRecorder for phase start/end
  // metrics. For worklets that have multiple threads, this will be called once
  // for each of those threads.
  void OnThreadReady(base::ProcessId pid);

  // Mojo disconnect with reason handler. If there's a description, it's a load
  // error. Otherwise, it's a crash. Passes error information on to all
  // associated WorkletHandles.
  void OnWorkletDisconnected(uint32_t /* custom_reason */,
                             const std::string& description);

  static std::vector<std::string> GetDevtoolsAuctionIds(
      base::WeakPtr<WorkletOwner> self);

  // Set to null once `this` is removed from AuctionWorkletManager's
  // WorkletOwner list, which happens on destruction or on Mojo pipe closure.
  // The latter allows a handle to still exist and refer to a WorkletOwner with
  // a broken Worklet pipe, while new requests for the same worklet will result
  // in creating a fresh Mojo worklet.
  raw_ptr<AuctionWorkletManager> worklet_manager_;

  const WorkletKey worklet_info_;

  AuctionProcessManager::ProcessHandle process_handle_;

  // These are handles that have not yet been notified of having a process,
  // either because the process isn't available yet or because we haven't
  // gotten around to dispatching the notification. If the process fails before
  // handle assignment, it's removed from this set, too.
  std::set<HandleKey> handles_waiting_for_process_;

  // These are handles that have been notified of having a process, and not of
  // a process failure.
  std::set<raw_ptr<WorkletHandle, SetExperimental>> handles_with_process_;

  std::unique_ptr<AuctionURLLoaderFactoryProxy> url_loader_factory_proxy_;
  mojo::Remote<auction_worklet::mojom::BidderWorklet> bidder_worklet_;
  mojo::Remote<auction_worklet::mojom::SellerWorklet> seller_worklet_;
  // This must be destroyed before the worklet it's passed, since it hangs on to
  // a raw pointer to it.
  std::vector<std::unique_ptr<DebuggableAuctionWorklet>> worklet_debugs_;

  // If true, we will split callback notifications into small batches.
  bool split_up_notifications_;
  bool notifications_pending_ = false;
  std::optional<FatalErrorType> notify_error_type_;
  std::vector<std::string> notify_errors_;

  uint64_t next_handle_seq_num_ = 0;

  // Map from devtools auction ID to number of handles from that auction.
  std::map<std::string, int> registered_devtools_auction_ids_;

  // Public key that can be loaded into the worklet for trusted signals KVv2
  // support.
  auction_worklet::mojom::TrustedSignalsPublicKeyPtr
      trusted_signals_kvv2_public_key_ = nullptr;
  // A flag to indicate if the `trusted_signals_kvv2_public_key_` has been
  // fetched or not.
  bool waiting_on_trusted_signals_kvv2_public_key_ = false;
  bool process_assigned_ = false;

  // When a worklet is requested before it's ready, we store the
  // AuctionMetricsRecorder here so that it can be notified when the worklet
  // is ready. There's an AuctionMetricsRecorder for each auction, and since
  // multiple auctions can reuse the same worklet, there may in fact be multiple
  // auctions waiting for the same worklet, which is why this is a list.
  std::vector<raw_ptr<AuctionMetricsRecorder>>
      auction_metrics_recorders_to_notify_;

  // When the requested worklet is ready, we can immediately record this to the
  // AuctionMetricsRecorder instead of adding the AuctionMetricsRecorder to
  // `auction_metrics_recorders_to_notify_`, defined above.
  bool is_worklet_ready_ = false;

  base::WeakPtrFactory<WorkletOwner> weak_ptr_factory_{this};
};

AuctionWorkletManager::WorkletOwner::WorkletOwner(
    AuctionWorkletManager* worklet_manager,
    WorkletKey worklet_info,
    size_t number_of_bidder_threads)
    : worklet_manager_(worklet_manager),
      worklet_info_(std::move(worklet_info)),
      split_up_notifications_(
          base::FeatureList::IsEnabled(kFledgeSplitUpWorkletAssignment)) {
  // If `trusted_signals_coordinator` in `worklet_info_` has a value and
  // `kFledgeTrustedSignalsKVv2Support` is enabled, call
  // `GetBiddingAndAuctionServerKey` to fetch `trusted_signals_kvv2_public_key_`
  // with the bound callback `OnTrustedSignalsKVv2KeyFetched()`.
  if (worklet_info_.trusted_signals_coordinator.has_value() &&
      base::FeatureList::IsEnabled(
          blink::features::kFledgeTrustedSignalsKVv2Support)) {
    DCHECK(!waiting_on_trusted_signals_kvv2_public_key_);
    DCHECK(!trusted_signals_kvv2_public_key_);

    waiting_on_trusted_signals_kvv2_public_key_ = true;
    worklet_manager->delegate()->GetBiddingAndAuctionServerKey(
        std::move(worklet_info_.trusted_signals_coordinator),
        base::BindOnce(&AuctionWorkletManager::WorkletOwner::
                           OnTrustedSignalsKVv2KeyFetched,
                       weak_ptr_factory_.GetWeakPtr(),
                       number_of_bidder_threads));
  }

  if (worklet_manager_->auction_process_manager()->RequestWorkletService(
          worklet_info_.type, url::Origin::Create(worklet_info_.script_url),
          worklet_manager_->delegate()->GetFrameSiteInstance(),
          &process_handle_,
          base::BindOnce(
              &AuctionWorkletManager::WorkletOwner::OnProcessAssigned,
              base::Unretained(this), number_of_bidder_threads))) {
    OnProcessAssigned(number_of_bidder_threads);
  }
}

void AuctionWorkletManager::WorkletOwner::RegisterHandle(HandleKey handle) {
  handles_waiting_for_process_.insert(handle);
  ++registered_devtools_auction_ids_[handle.second->devtools_auction_id_];
  if (worklet_created()) {
    MaybeQueueNotifications();
  }
}

void AuctionWorkletManager::WorkletOwner::UnregisterHandle(HandleKey handle) {
  auto it = registered_devtools_auction_ids_.find(
      handle.second->devtools_auction_id_);
  CHECK(it != registered_devtools_auction_ids_.end(),
        base::NotFatalUntil::M130);
  --it->second;
  if (it->second == 0) {
    registered_devtools_auction_ids_.erase(it);
  }

  if (!handles_waiting_for_process_.erase(handle)) {
    // The handle should only be in one of the sets, so only need to search
    // `handles_with_process_` if it wasn't in `handles_waiting_for_process_`.
    handles_with_process_.erase(handle.second);
  }
  DCHECK_EQ(handles_waiting_for_process_.count(handle), 0u);
}

std::vector<std::string>
AuctionWorkletManager::WorkletOwner::ComputeDevtoolsAuctionIds() {
  std::vector<std::string> result;
  for (const auto& [id, count] : registered_devtools_auction_ids_) {
    result.push_back(id);
  }
  return result;
}

void AuctionWorkletManager::WorkletOwner::NotifyAuctionMetricsRecorderWhenReady(
    AuctionMetricsRecorder* auction_metrics_recorder) {
  if (is_worklet_ready_) {
    auction_metrics_recorder->OnWorkletReady();
  } else {
    auction_metrics_recorders_to_notify_.push_back(auction_metrics_recorder);
  }
}

AuctionWorkletManager::WorkletOwner::~WorkletOwner() {
  DCHECK(handles_waiting_for_process_.empty());
  DCHECK(handles_with_process_.empty());
  WorkletNoLongerUsable();
}

void AuctionWorkletManager::WorkletOwner::MaybeQueueNotifications() {
  if (notifications_pending_) {
    return;
  }
  notifications_pending_ = true;

  // This uses a weak pointer and not a ref-count holding one to avoid extending
  // lifetime of `this` beyond the handles.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WorkletOwner::DispatchSomeNotifications,
                                weak_ptr_factory_.GetWeakPtr()));
}

void AuctionWorkletManager::WorkletOwner::DispatchSomeNotifications() {
  // Make sure `this` isn't released in the middle of dispatch loops if they
  // drop all the handles.
  scoped_refptr<WorkletOwner> guard(this);

  notifications_pending_ = false;

  // Failure/success is checked here and not at queuing time since things may
  // change by the time this method is invoked.
  if (notify_error_type_.has_value()) {
    DispatchSomeFailureNotifications();
  } else {
    DispatchSomeSuccessNotifications();
  }
}

void AuctionWorkletManager::WorkletOwner::DispatchSomeFailureNotifications() {
  // In case of an error, we notify both of `handles_with_process_` and
  // `handles_waiting_for_process_`.  It's OK to remove things from these
  // sets here since we won't do anything else with them anyway, and it
  // protects us from re-entrancy weirdness.
  size_t num_notified = 0;
  size_t to_notify =
      handles_with_process_.size() + handles_waiting_for_process_.size();
  if (split_up_notifications_) {
    to_notify = std::min(to_notify, kBatchSize);
  }

  while (num_notified < to_notify && !handles_with_process_.empty()) {
    auto node = handles_with_process_.extract(handles_with_process_.begin());
    node.value()->OnFatalError(*notify_error_type_, notify_errors_);
    ++num_notified;
  }
  while (num_notified < to_notify && !handles_waiting_for_process_.empty()) {
    auto node = handles_waiting_for_process_.extract(
        handles_waiting_for_process_.begin());
    node.value().second->OnFatalError(*notify_error_type_, notify_errors_);
    ++num_notified;
  }

  if (!handles_with_process_.empty() || !handles_waiting_for_process_.empty()) {
    MaybeQueueNotifications();
  }
}

void AuctionWorkletManager::WorkletOwner::DispatchSomeSuccessNotifications() {
  // In case of success, we only notify `handles_waiting_for_process_`, and
  // also add them to `handles_with_process_`; if the next time we run
  // DispatchSomeNotifications() we're in failure state we want them to get
  // the failure notification.
  size_t num_notified = 0;
  size_t to_notify = handles_waiting_for_process_.size();
  if (split_up_notifications_) {
    to_notify = std::min(to_notify, kBatchSize);
  }
  // This loops needs to check `handles_waiting_for_process_.empty()` for the
  // case where items are removed in response to callbacks.
  while (num_notified < to_notify && !handles_waiting_for_process_.empty()) {
    auto node = handles_waiting_for_process_.extract(
        handles_waiting_for_process_.begin());
    // Must do the insert before the callback in case the callback deletes
    // the handle.
    handles_with_process_.insert(node.value().second);
    node.value().second->OnWorkletAvailable();
    ++num_notified;
  }

  if (!handles_waiting_for_process_.empty()) {
    MaybeQueueNotifications();
  }
}

void AuctionWorkletManager::WorkletOwner::WorkletNoLongerUsable() {
  if (worklet_manager_) {
    worklet_manager_->OnWorkletNoLongerUsable(this);
    worklet_manager_ = nullptr;
  }
}

void AuctionWorkletManager::WorkletOwner::OnTrustedSignalsKVv2KeyFetched(
    size_t number_of_bidder_threads,
    base::expected<BiddingAndAuctionServerKey, std::string> key_or_error) {
  waiting_on_trusted_signals_kvv2_public_key_ = false;

  // Pass an empty trusted signals KVv2 key to the worklet even if the fetching
  // process fails. The error will be handled in the worklet process when a key
  // is required but is empty.
  //
  // TODO(crbug.com/337917489): Find a better way to handle the fetch error with
  // more debugging information rather than just pass a nullptr to bidder/seller
  // worklet.
  if (key_or_error.has_value()) {
    trusted_signals_kvv2_public_key_ =
        auction_worklet::mojom::TrustedSignalsPublicKey::New(key_or_error->key,
                                                             key_or_error->id);
  }

  LoadWorkletIfReady(number_of_bidder_threads);
}

void AuctionWorkletManager::WorkletOwner::OnProcessAssigned(
    size_t number_of_bidder_threads) {
  process_assigned_ = true;
  LoadWorkletIfReady(number_of_bidder_threads);
}

void AuctionWorkletManager::WorkletOwner::LoadWorkletIfReady(
    size_t number_of_bidder_threads) {
  // Still waiting on the process handle.
  if (!process_assigned_) {
    return;
  }

  // Still waiting on fetching public key.
  if (waiting_on_trusted_signals_kvv2_public_key_) {
    return;
  }

  DCHECK(!bidder_worklet_.is_bound());
  DCHECK(!seller_worklet_.is_bound());

  Delegate* delegate = worklet_manager_->delegate();
  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
  mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
      auction_network_events_handler;
  RenderFrameHostImpl* const rfh = delegate->GetFrame();
  worklet_manager_->auction_network_events_proxy_->Clone(
      auction_network_events_handler.InitWithNewPipeAndPassReceiver());
  url_loader_factory_proxy_ = std::make_unique<AuctionURLLoaderFactoryProxy>(
      url_loader_factory.InitWithNewPipeAndPassReceiver(),
      base::BindRepeating(&Delegate::GetFrameURLLoaderFactory,
                          base::Unretained(delegate)),
      base::BindRepeating(&Delegate::GetTrustedURLLoaderFactory,
                          base::Unretained(delegate)),
      base::BindOnce(&Delegate::PreconnectSocket, base::Unretained(delegate)),
      base::BindRepeating(&Delegate::GetCookieDeprecationLabel,
                          base::Unretained(delegate)),
      base::BindRepeating(&WorkletOwner::GetDevtoolsAuctionIds,
                          weak_ptr_factory_.GetWeakPtr()),
      /*force_reload=*/rfh->reload_type() == ReloadType::BYPASSING_CACHE,
      worklet_manager_->top_window_origin(), worklet_manager_->frame_origin(),
      // NOTE: `rfh` can be null in tests.
      /*renderer_process_id=*/
      rfh ? std::optional<int>(rfh->GetProcess()->GetID()) : std::nullopt,
      /*is_for_seller_=*/worklet_info_.type == WorkletType::kSeller,
      delegate->GetClientSecurityState(), worklet_info_.script_url,
      worklet_info_.wasm_url, worklet_info_.signals_url,
      worklet_info_.needs_cors_for_additional_bid,
      rfh->frame_tree_node()->frame_tree_node_id());

  CHECK(worklet_debugs_.empty());

  switch (worklet_info_.type) {
    case WorkletType::kBidder: {
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          worklet_receiver = bidder_worklet_.BindNewPipeAndPassReceiver();

      std::vector<
          mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost>>
          shared_storage_hosts;

      for (size_t i = 0; i < number_of_bidder_threads; ++i) {
        worklet_debugs_.emplace_back(new DebuggableAuctionWorklet(
            delegate->GetFrame(), process_handle_, worklet_info_.script_url,
            bidder_worklet_.get(),
            /*thread_index=*/i));

        if (std::optional<base::ProcessId> maybe_pid =
                worklet_debugs_.back()->GetPid(
                    base::BindOnce(&WorkletOwner::OnThreadReady,
                                   weak_ptr_factory_.GetWeakPtr()))) {
          OnThreadReady(*maybe_pid);
        }

        // For `DebuggableAuctionWorklet` created synchronously for the same
        // frame, they should have the same `should_pause_on_start()` state.
        CHECK_EQ(worklet_debugs_[i]->should_pause_on_start(),
                 worklet_debugs_[0]->should_pause_on_start());

        shared_storage_hosts.push_back(
            worklet_manager_->MaybeBindAuctionSharedStorageHost(
                delegate->GetFrame(),
                url::Origin::Create(worklet_info_.script_url)));
      }

      process_handle_.GetService()->LoadBidderWorklet(
          std::move(worklet_receiver), std::move(shared_storage_hosts),
          worklet_debugs_[0]->should_pause_on_start(),
          std::move(url_loader_factory),
          std::move(auction_network_events_handler), worklet_info_.script_url,
          worklet_info_.wasm_url, worklet_info_.signals_url,
          worklet_info_.trusted_bidding_signals_slot_size_param,
          worklet_manager_->top_window_origin(),
          GetAuctionWorkletPermissionsPolicyState(delegate->GetFrame(),
                                                  worklet_info_.script_url),
          worklet_info_.experiment_group_id,
          std::move(trusted_signals_kvv2_public_key_));
      bidder_worklet_.set_disconnect_with_reason_handler(base::BindOnce(
          &WorkletOwner::OnWorkletDisconnected, base::Unretained(this)));
      break;
    }

    case WorkletType::kSeller: {
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          worklet_receiver = seller_worklet_.BindNewPipeAndPassReceiver();

      std::vector<
          mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost>>
          shared_storage_hosts;

      for (size_t i = 0;
           i < static_cast<size_t>(
                   features::kFledgeSellerWorkletThreadPoolSize.Get());
           ++i) {
        worklet_debugs_.emplace_back(new DebuggableAuctionWorklet(
            delegate->GetFrame(), process_handle_, worklet_info_.script_url,
            seller_worklet_.get(),
            /*thread_index=*/i));

        if (std::optional<base::ProcessId> maybe_pid =
                worklet_debugs_.back()->GetPid(
                    base::BindOnce(&WorkletOwner::OnThreadReady,
                                   weak_ptr_factory_.GetWeakPtr()))) {
          OnThreadReady(*maybe_pid);
        }

        // For `DebuggableAuctionWorklet` created synchronously for the same
        // frame, they should have the same `should_pause_on_start()` state.
        CHECK_EQ(worklet_debugs_[i]->should_pause_on_start(),
                 worklet_debugs_[0]->should_pause_on_start());

        shared_storage_hosts.push_back(
            worklet_manager_->MaybeBindAuctionSharedStorageHost(
                delegate->GetFrame(),
                url::Origin::Create(worklet_info_.script_url)));
      }

      process_handle_.GetService()->LoadSellerWorklet(
          std::move(worklet_receiver), std::move(shared_storage_hosts),
          worklet_debugs_[0]->should_pause_on_start(),
          std::move(url_loader_factory),
          std::move(auction_network_events_handler), worklet_info_.script_url,
          worklet_info_.signals_url, worklet_manager_->top_window_origin(),
          GetAuctionWorkletPermissionsPolicyState(delegate->GetFrame(),
                                                  worklet_info_.script_url),
          worklet_info_.experiment_group_id,
          std::move(trusted_signals_kvv2_public_key_));
      seller_worklet_.set_disconnect_with_reason_handler(base::BindOnce(
          &WorkletOwner::OnWorkletDisconnected, base::Unretained(this)));
      break;
    }
  }

  MaybeQueueNotifications();
}

void AuctionWorkletManager::WorkletOwner::OnThreadReady(
    base::ProcessId unused_pid) {
  // OnThreadReady may be called multiple times, since there may be multiple
  // threads for this worklet. We consider the *first* thread ready to be the
  // point at which the worklet is ready, since that's the point at which the
  // worklet can begin processing requests.
  if (is_worklet_ready_) {
    return;
  }
  for (AuctionMetricsRecorder* auction_metrics_recorder :
       auction_metrics_recorders_to_notify_) {
    auction_metrics_recorder->OnWorkletReady();
  }
  auction_metrics_recorders_to_notify_.clear();
  is_worklet_ready_ = true;
}

void AuctionWorkletManager::WorkletOwner::OnWorkletDisconnected(
    uint32_t /* custom_reason */,
    const std::string& description) {
  // This may only be invoked once per worklet, and `worklet_manager_` is only
  // cleared by this method and on destruction, so it should not be null.
  DCHECK(worklet_manager_);

  WorkletNoLongerUsable();

  // If there's a description, it's a load failure. Otherwise, it's a crash.
  if (!description.empty()) {
    notify_error_type_ = FatalErrorType::kScriptLoadFailed;
    notify_errors_.push_back(description);
  } else {
    notify_error_type_ = FatalErrorType::kWorkletCrash;
    notify_errors_.emplace_back(
        base::StrCat({worklet_info_.script_url.spec(), " crashed."}));
  }

  MaybeQueueNotifications();
}

// static
std::vector<std::string>
AuctionWorkletManager::WorkletOwner::GetDevtoolsAuctionIds(
    base::WeakPtr<WorkletOwner> self) {
  if (self) {
    return self->ComputeDevtoolsAuctionIds();
  }

  return std::vector<std::string>();
}

AuctionWorkletManager::WorkletKey::WorkletKey(
    WorkletType type,
    const GURL& script_url,
    const std::optional<GURL>& wasm_url,
    const std::optional<GURL>& signals_url,
    bool needs_cors_for_additional_bid,
    std::optional<uint16_t> experiment_group_id,
    const std::string& trusted_bidding_signals_slot_size_param,
    const std::optional<url::Origin>& trusted_signals_coordinator)
    : type(type),
      script_url(script_url),
      wasm_url(wasm_url),
      signals_url(signals_url),
      needs_cors_for_additional_bid(needs_cors_for_additional_bid),
      experiment_group_id(experiment_group_id),
      trusted_bidding_signals_slot_size_param(
          trusted_bidding_signals_slot_size_param),
      trusted_signals_coordinator(trusted_signals_coordinator) {}

AuctionWorkletManager::WorkletKey::WorkletKey(const WorkletKey&) = default;
AuctionWorkletManager::WorkletKey::WorkletKey(WorkletKey&&) = default;
AuctionWorkletManager::WorkletKey::~WorkletKey() = default;

namespace {
size_t CombineHash(size_t hash, size_t new_value) {
  static constexpr size_t kMagic =
      sizeof(size_t) > 4 ? 0x9e3779b97f4a7c15 : 0x9e3779b9;
  hash ^= new_value + kMagic + (hash << 6) + (hash >> 2);
  return hash;
}
}  // namespace

size_t AuctionWorkletManager::WorkletKey::GetHash() const {
  using base::FastHash;
  size_t hash = (type == WorkletType::kBidder ? 0x1ee4cafc : 0x8dfe2bb7);
  hash = CombineHash(hash, FastHash(script_url.spec()));
  hash = CombineHash(hash, wasm_url ? FastHash(wasm_url->spec()) : 0xaf57570a);
  hash = CombineHash(hash,
                     signals_url ? FastHash(signals_url->spec()) : 0xbee1271e);
  hash = CombineHash(hash,
                     needs_cors_for_additional_bid ? 0x6748ee16 : 0xc2a13cd1);
  hash = CombineHash(hash,
                     experiment_group_id ? *experiment_group_id : 0xd60fc235);
  hash = CombineHash(hash, FastHash(trusted_bidding_signals_slot_size_param));
  hash = CombineHash(
      hash, trusted_signals_coordinator
                ? FastHash(trusted_signals_coordinator->GetURL().spec())
                : 0xf3a287b1);
  return hash;
}

bool AuctionWorkletManager::WorkletKey::WorkletKey::operator<(
    const WorkletKey& other) const {
  return std::tie(type, script_url, wasm_url, signals_url,
                  needs_cors_for_additional_bid, experiment_group_id,
                  trusted_bidding_signals_slot_size_param,
                  trusted_signals_coordinator) <
         std::tie(other.type, other.script_url, other.wasm_url,
                  other.signals_url, other.needs_cors_for_additional_bid,
                  other.experiment_group_id,
                  other.trusted_bidding_signals_slot_size_param,
                  other.trusted_signals_coordinator);
}

AuctionWorkletManager::WorkletHandle::~WorkletHandle() {
  // We register with subresource_url_authorizations() only if
  // AuthorizeSubresourceUrls() was called, so deregister if that's the case.
  //
  // This also must imply that the object should exist --- the proxy that owns
  // the SubresourceUrlAuthorizations gets created when a process is assigned,
  // and that's a precondition for calling AuthorizeSubresourceUrls().
  if (authorized_subresources_) {
    DCHECK(worklet_owner_->subresource_url_authorizations());
    worklet_owner_->subresource_url_authorizations()
        ->OnWorkletHandleDestruction(this);
  }
  worklet_owner_->UnregisterHandle(HandleKey(seq_num_, this));
}

auction_worklet::mojom::BidderWorklet*
AuctionWorkletManager::WorkletHandle::GetBidderWorklet() {
  DCHECK_EQ(WorkletType::kBidder, worklet_owner_->worklet_info().type);
  DCHECK(worklet_owner_->bidder_worklet());
  return worklet_owner_->bidder_worklet();
}

auction_worklet::mojom::SellerWorklet*
AuctionWorkletManager::WorkletHandle::GetSellerWorklet() {
  DCHECK_EQ(WorkletType::kSeller, worklet_owner_->worklet_info().type);
  DCHECK(worklet_owner_->seller_worklet());
  return worklet_owner_->seller_worklet();
}

const SubresourceUrlAuthorizations& AuctionWorkletManager::WorkletHandle::
    GetSubresourceUrlAuthorizationsForTesting() {
  DCHECK(authorized_subresources_);
  DCHECK(worklet_owner_->subresource_url_authorizations());
  return *worklet_owner_->subresource_url_authorizations();
}

std::vector<std::string>
AuctionWorkletManager::WorkletHandle::GetDevtoolsAuctionIdsForTesting() {
  return worklet_owner_->ComputeDevtoolsAuctionIds();
}

AuctionWorkletManager::WorkletHandle::WorkletHandle(
    std::string devtools_auction_id,
    scoped_refptr<WorkletOwner> worklet_owner,
    base::OnceClosure worklet_available_callback,
    FatalErrorCallback fatal_error_callback)
    : worklet_owner_(std::move(worklet_owner)),
      devtools_auction_id_(std::move(devtools_auction_id)),
      worklet_available_callback_(std::move(worklet_available_callback)),
      fatal_error_callback_(std::move(fatal_error_callback)),
      seq_num_(worklet_owner_->GetNextSeqNum()) {
  DCHECK(worklet_available_callback_);
  DCHECK(fatal_error_callback_);

  worklet_owner_->RegisterHandle(HandleKey(seq_num_, this));
}

void AuctionWorkletManager::WorkletHandle::OnWorkletAvailable() {
  DCHECK(worklet_available_callback_);
  std::move(worklet_available_callback_).Run();
}

void AuctionWorkletManager::WorkletHandle::OnFatalError(
    FatalErrorType type,
    const std::vector<std::string>& errors) {
  DCHECK(fatal_error_callback_);
  std::move(fatal_error_callback_).Run(type, errors);
}

void AuctionWorkletManager::WorkletHandle::AuthorizeSubresourceUrls(
    const SubresourceUrlBuilder& subresource_url_builder) {
  if (authorized_subresources_) {
    return;
  }

  authorized_subresources_ = true;

  DCHECK(worklet_owner_->subresource_url_authorizations());
  std::vector<SubresourceUrlBuilder::BundleSubresourceInfo>
      authorized_subresource_urls;
  if (subresource_url_builder.auction_signals()) {
    authorized_subresource_urls.push_back(
        *subresource_url_builder.auction_signals());
  }
  switch (worklet_owner_->worklet_info().type) {
    case WorkletType::kBidder: {
      const url::Origin bidder_origin =
          url::Origin::Create(worklet_owner_->worklet_info().script_url);
      auto it = subresource_url_builder.per_buyer_signals().find(bidder_origin);
      if (it != subresource_url_builder.per_buyer_signals().end()) {
        authorized_subresource_urls.push_back(it->second);
      }
      break;
    }
    case WorkletType::kSeller: {
      if (subresource_url_builder.seller_signals()) {
        authorized_subresource_urls.push_back(
            *subresource_url_builder.seller_signals());
      }
      break;
    }
  }
  worklet_owner_->subresource_url_authorizations()->AuthorizeSubresourceUrls(
      this, std::move(authorized_subresource_urls));
}

bool AuctionWorkletManager::WorkletHandle::worklet_created() const {
  return worklet_owner_->worklet_created();
}

AuctionWorkletManager::AuctionWorkletManager(
    AuctionProcessManager* auction_process_manager,
    url::Origin top_window_origin,
    url::Origin frame_origin,
    Delegate* delegate)
    : auction_process_manager_(auction_process_manager),
      top_window_origin_(std::move(top_window_origin)),
      frame_origin_(std::move(frame_origin)),
      delegate_(delegate),
      auction_network_events_proxy_(
          std::make_unique<AuctionNetworkEventsProxy>(GetFrameTreeNodeID())) {
  if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
    auction_shared_storage_host_ = std::make_unique<AuctionSharedStorageHost>(
        static_cast<StoragePartitionImpl*>(
            delegate_->GetFrame()->GetProcess()->GetStoragePartition())
            ->GetSharedStorageManager());
  }
}

AuctionWorkletManager::~AuctionWorkletManager() = default;

// static
AuctionWorkletManager::WorkletKey AuctionWorkletManager::BidderWorkletKey(
    const GURL& bidding_logic_url,
    const std::optional<GURL>& wasm_url,
    const std::optional<GURL>& trusted_bidding_signals_url,
    bool needs_cors_for_additional_bid,
    std::optional<uint16_t> experiment_group_id,
    const std::string& trusted_bidding_signals_slot_size_param,
    const std::optional<url::Origin>& trusted_bidding_signals_coordinator) {
  return WorkletKey(WorkletType::kBidder,
                    /*script_url=*/bidding_logic_url, wasm_url,
                    /*signals_url=*/trusted_bidding_signals_url,
                    needs_cors_for_additional_bid,
                    trusted_bidding_signals_url.has_value()
                        ? experiment_group_id
                        : std::nullopt,
                    trusted_bidding_signals_url.has_value()
                        ? trusted_bidding_signals_slot_size_param
                        : "",
                    trusted_bidding_signals_url.has_value()
                        ? trusted_bidding_signals_coordinator
                        : std::nullopt);
}

void AuctionWorkletManager::RequestBidderWorklet(
    std::string devtools_auction_id,
    const GURL& bidding_logic_url,
    const std::optional<GURL>& wasm_url,
    const std::optional<GURL>& trusted_bidding_signals_url,
    bool needs_cors_for_additional_bid,
    std::optional<uint16_t> experiment_group_id,
    const std::string& trusted_bidding_signals_slot_size_param,
    const std::optional<url::Origin>& trusted_bidding_signals_coordinator,
    base::OnceClosure worklet_available_callback,
    FatalErrorCallback fatal_error_callback,
    std::unique_ptr<WorkletHandle>& out_worklet_handle,
    AuctionMetricsRecorder* auction_metrics_recorder) {
  RequestWorkletByKey(
      BidderWorkletKey(bidding_logic_url, wasm_url, trusted_bidding_signals_url,
                       needs_cors_for_additional_bid, experiment_group_id,
                       trusted_bidding_signals_slot_size_param,
                       trusted_bidding_signals_coordinator),
      std::move(devtools_auction_id), std::move(worklet_available_callback),
      std::move(fatal_error_callback), out_worklet_handle,
      /*number_of_bidder_threads=*/1, auction_metrics_recorder);
}

void AuctionWorkletManager::RequestSellerWorklet(
    std::string devtools_auction_id,
    const GURL& decision_logic_url,
    const std::optional<GURL>& trusted_scoring_signals_url,
    std::optional<uint16_t> experiment_group_id,
    const std::optional<url::Origin>& trusted_scoring_signals_coordinator,
    base::OnceClosure worklet_available_callback,
    FatalErrorCallback fatal_error_callback,
    std::unique_ptr<WorkletHandle>& out_worklet_handle,
    AuctionMetricsRecorder* auction_metrics_recorder) {
  WorkletKey worklet_info(WorkletType::kSeller,
                          /*script_url=*/decision_logic_url,
                          /*wasm_url=*/std::nullopt,
                          /*signals_url=*/trusted_scoring_signals_url,
                          /*needs_cors_for_additional_bid=*/false,
                          experiment_group_id,
                          /*trusted_bidding_signals_slot_size_param=*/"",
                          trusted_scoring_signals_coordinator);
  RequestWorkletByKey(std::move(worklet_info), std::move(devtools_auction_id),
                      std::move(worklet_available_callback),
                      std::move(fatal_error_callback), out_worklet_handle,
                      /*number_of_bidder_threads=*/0, auction_metrics_recorder);
}

void AuctionWorkletManager::RequestWorkletByKey(
    WorkletKey worklet_info,
    std::string devtools_auction_id,
    base::OnceClosure worklet_available_callback,
    FatalErrorCallback fatal_error_callback,
    std::unique_ptr<WorkletHandle>& out_worklet_handle,
    size_t number_of_bidder_threads,
    AuctionMetricsRecorder* auction_metrics_recorder) {
  DCHECK(!out_worklet_handle);
  auto worklet_it = worklets_.find(worklet_info);
  scoped_refptr<WorkletOwner> worklet;
  if (worklet_it != worklets_.end()) {
    worklet = worklet_it->second;
  } else {
    // Can't just insert in the map and put a reference in `worklet_it`, since
    // need to keep a live reference.
    worklet = base::MakeRefCounted<WorkletOwner>(this, worklet_info,
                                                 number_of_bidder_threads);
    worklets_.emplace(std::pair(std::move(worklet_info), worklet.get()));
  }
  if (auction_metrics_recorder) {
    auction_metrics_recorder->OnWorkletRequested();
    worklet->NotifyAuctionMetricsRecorderWhenReady(auction_metrics_recorder);
  }
  out_worklet_handle.reset(new WorkletHandle(
      std::move(devtools_auction_id), std::move(worklet),
      std::move(worklet_available_callback), std::move(fatal_error_callback)));
}

void AuctionWorkletManager::OnWorkletNoLongerUsable(WorkletOwner* worklet) {
  DCHECK(worklets_.count(worklet->worklet_info()));
  DCHECK_EQ(worklet, worklets_[worklet->worklet_info()]);

  worklets_.erase(worklet->worklet_info());
}

mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost>
AuctionWorkletManager::MaybeBindAuctionSharedStorageHost(
    RenderFrameHostImpl* auction_runner_rfh,
    const url::Origin& worklet_origin) {
  mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost> remote;

  const blink::PermissionsPolicy* permissions_policy =
      auction_runner_rfh->permissions_policy();

  if (auction_shared_storage_host_ &&
      permissions_policy->IsFeatureEnabledForOrigin(
          blink::mojom::PermissionsPolicyFeature::kSharedStorage,
          worklet_origin)) {
    auction_shared_storage_host_->BindNewReceiver(
        auction_runner_rfh, worklet_origin,
        remote.InitWithNewPipeAndPassReceiver());
  }

  return remote;
}

}  // namespace content
