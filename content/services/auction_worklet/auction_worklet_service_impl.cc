// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_worklet_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/features.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/bidder_worklet.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/seller_worklet.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"

namespace auction_worklet {

namespace {

static size_t g_next_seller_worklet_thread_index = 0;

}  // namespace

// V8HelperHolder exists to make sure we don't end up creating a fresh V8 thread
// every time a new service instance is created. It itself must be accessed from
// a single thread, and should be destroyed before task scheduling is shut down.
//
// It has two modes:
//
// 1) Dedicated process mode, for running as a service. In that case, it only
//    ends up using one V8 thread, as service process isolation will ensure that
//    SellerInstance() and BidderInstance() will not both be called in the same
///   process.  It also makes sure to wait to fully unwind the V8 thread
//    in its destructor, since that must be done before task scheduler shutdown
//    to avoid crashes from V8 GC trying to post tasks.
//
// 2) Shared process mode, for running inside a renderer. In that case, it
//    provides up two V8 threads --- one for seller, one for bidder --- to
//    provide the base level of parallelism one would have with those getting
//    separate processes.
class AuctionWorkletServiceImpl::V8HelperHolder
    : public base::RefCounted<V8HelperHolder> {
 public:
  enum class WorkletType {
    kBidder,
    kSeller,
  };

  static scoped_refptr<V8HelperHolder> BidderInstance(
      ProcessModel process_model,
      size_t thread_index) {
    if (!g_bidder_instances) {
      g_bidder_instances = new std::vector<V8HelperHolder*>();
    }

    while (g_bidder_instances->size() <= thread_index) {
      g_bidder_instances->push_back(
          new V8HelperHolder(process_model, WorkletType::kBidder,
                             /*thread_index=*/g_bidder_instances->size()));
    }

    if (!g_bidder_instances->at(thread_index)) {
      g_bidder_instances->at(thread_index) =
          new V8HelperHolder(process_model, WorkletType::kBidder, thread_index);
    }

    DCHECK_CALLED_ON_VALID_SEQUENCE(
        g_bidder_instances->at(thread_index)->sequence_checker_);

    return g_bidder_instances->at(thread_index);
  }

  static scoped_refptr<V8HelperHolder> SellerInstance(
      ProcessModel process_model,
      size_t thread_index) {
    if (!g_seller_instances) {
      g_seller_instances = new std::vector<V8HelperHolder*>();
    }

    while (g_seller_instances->size() <= thread_index) {
      g_seller_instances->push_back(
          new V8HelperHolder(process_model, WorkletType::kSeller,
                             /*thread_index=*/g_seller_instances->size()));
    }

    if (!g_seller_instances->at(thread_index)) {
      g_seller_instances->at(thread_index) =
          new V8HelperHolder(process_model, WorkletType::kSeller, thread_index);
    }

    DCHECK_CALLED_ON_VALID_SEQUENCE(
        g_seller_instances->at(thread_index)->sequence_checker_);

    return g_seller_instances->at(thread_index);
  }

  const scoped_refptr<AuctionV8Helper>& V8Helper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!auction_v8_helper_) {
      auction_v8_helper_ = AuctionV8Helper::Create(
          AuctionV8Helper::CreateTaskRunner(),
          /*init_v8=*/process_model_ == ProcessModel::kDedicated);
      if (process_model_ == ProcessModel::kDedicated) {
        auction_v8_helper_->SetDestroyedCallback(base::BindOnce(
            &V8HelperHolder::FinishedDestroying, base::Unretained(this)));
      }
    }
    return auction_v8_helper_;
  }

 private:
  friend class base::RefCounted<V8HelperHolder>;

  explicit V8HelperHolder(ProcessModel process_model,
                          WorkletType worklet_type,
                          size_t thread_index = 0)
      : process_model_(process_model),
        worklet_type_(worklet_type),
        thread_index_(thread_index) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  ~V8HelperHolder() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (process_model_ == ProcessModel::kDedicated && auction_v8_helper_) {
      // ~V8HelperHolder running means there are no more instances of the
      // service, so the service process itself may be about to be shutdown.
      // Wait until `auction_v8_helper_` is destroyed to make sure no V8 things
      // are running on its thread, since they may crash if task scheduling
      // suddenly becomes impossible.
      auction_v8_helper_.reset();
      wait_for_v8_shutdown_.Wait();
    }

    if (worklet_type_ == WorkletType::kBidder) {
      DCHECK_EQ(g_bidder_instances->at(thread_index_), this);
      g_bidder_instances->at(thread_index_) = nullptr;

      size_t alive_instances_count =
          std::count_if(g_bidder_instances->begin(), g_bidder_instances->end(),
                        [](V8HelperHolder* instance) { return !!instance; });

      if (alive_instances_count == 0) {
        g_bidder_instances->clear();
        delete g_bidder_instances;
        g_bidder_instances = nullptr;
      }
    } else {
      DCHECK_EQ(g_seller_instances->at(thread_index_), this);
      g_seller_instances->at(thread_index_) = nullptr;

      size_t alive_instances_count =
          std::count_if(g_seller_instances->begin(), g_seller_instances->end(),
                        [](V8HelperHolder* instance) { return !!instance; });

      if (alive_instances_count == 0) {
        g_seller_instances->clear();
        delete g_seller_instances;
        g_seller_instances = nullptr;

        g_next_seller_worklet_thread_index = 0;
      }
    }
  }

  void FinishedDestroying() {
    DCHECK_EQ(process_model_, ProcessModel::kDedicated);
    // This method runs on the V8 thread, while the object lives on the
    // service's mojo thread.
    wait_for_v8_shutdown_.Signal();
  }

  static std::vector<V8HelperHolder*>* g_bidder_instances;
  static std::vector<V8HelperHolder*>* g_seller_instances;

  scoped_refptr<AuctionV8Helper> auction_v8_helper_;
  const ProcessModel process_model_;
  const WorkletType worklet_type_;
  size_t thread_index_ = 0;

  base::WaitableEvent wait_for_v8_shutdown_;
  SEQUENCE_CHECKER(sequence_checker_);
};

std::vector<AuctionWorkletServiceImpl::V8HelperHolder*>*
    AuctionWorkletServiceImpl::V8HelperHolder::g_bidder_instances = nullptr;

std::vector<AuctionWorkletServiceImpl::V8HelperHolder*>*
    AuctionWorkletServiceImpl::V8HelperHolder::g_seller_instances = nullptr;

// static
void AuctionWorkletServiceImpl::CreateForRenderer(
    mojo::PendingReceiver<mojom::AuctionWorkletService> receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new AuctionWorkletServiceImpl(
          ProcessModel::kShared,
          mojo::PendingReceiver<mojom::AuctionWorkletService>())),
      std::move(receiver));
}

// static
std::unique_ptr<AuctionWorkletServiceImpl>
AuctionWorkletServiceImpl::CreateForService(
    mojo::PendingReceiver<mojom::AuctionWorkletService> receiver) {
  return base::WrapUnique(new AuctionWorkletServiceImpl(
      ProcessModel::kDedicated, std::move(receiver)));
}

AuctionWorkletServiceImpl::AuctionWorkletServiceImpl(
    ProcessModel process_model,
    mojo::PendingReceiver<mojom::AuctionWorkletService> receiver)
    : process_model_(process_model), receiver_(this, std::move(receiver)) {
  for (size_t i = 0;
       i <
       static_cast<size_t>(features::kFledgeSellerWorkletThreadPoolSize.Get());
       ++i) {
    auction_seller_v8_helper_holders_.push_back(
        V8HelperHolder::SellerInstance(process_model, /*thread_index=*/i));
  }
}

AuctionWorkletServiceImpl::~AuctionWorkletServiceImpl() = default;

std::vector<scoped_refptr<AuctionV8Helper>>
AuctionWorkletServiceImpl::AuctionV8HelpersForTesting() {
  std::vector<scoped_refptr<AuctionV8Helper>> result;
  for (const auto& v8_helper_holder : auction_bidder_v8_helper_holders_) {
    result.push_back(v8_helper_holder->V8Helper());
  }
  for (const auto& v8_helper_holder : auction_seller_v8_helper_holders_) {
    result.push_back(v8_helper_holder->V8Helper());
  }
  return result;
}

void AuctionWorkletServiceImpl::LoadBidderWorklet(
    mojo::PendingReceiver<mojom::BidderWorklet> bidder_worklet_receiver,
    std::vector<mojo::PendingRemote<mojom::AuctionSharedStorageHost>>
        shared_storage_hosts,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        auction_network_events_handler,
    const GURL& script_source_url,
    const std::optional<GURL>& wasm_helper_url,
    const std::optional<GURL>& trusted_bidding_signals_url,
    const std::string& trusted_bidding_signals_slot_size_param,
    const url::Origin& top_window_origin,
    mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
    std::optional<uint16_t> experiment_group_id,
    mojom::TrustedSignalsPublicKeyPtr public_key) {
  // If needed, expand the thread pool to match the number of threads requested.
  for (size_t i = auction_bidder_v8_helper_holders_.size();
       i < shared_storage_hosts.size(); ++i) {
    auction_bidder_v8_helper_holders_.push_back(
        V8HelperHolder::BidderInstance(process_model_, /*thread_index=*/i));
  }

  std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers;
  for (size_t i = 0; i < shared_storage_hosts.size(); ++i) {
    v8_helpers.push_back(auction_bidder_v8_helper_holders_[i]->V8Helper());
  }

  auto bidder_worklet = std::make_unique<BidderWorklet>(
      std::move(v8_helpers), std::move(shared_storage_hosts),
      pause_for_debugger_on_start, std::move(pending_url_loader_factory),
      std::move(auction_network_events_handler), script_source_url,
      wasm_helper_url, trusted_bidding_signals_url,
      trusted_bidding_signals_slot_size_param, top_window_origin,
      std::move(permissions_policy_state), experiment_group_id,
      std::move(public_key));
  auto* bidder_worklet_ptr = bidder_worklet.get();

  mojo::ReceiverId receiver_id = bidder_worklets_.Add(
      std::move(bidder_worklet), std::move(bidder_worklet_receiver));

  bidder_worklet_ptr->set_close_pipe_callback(
      base::BindOnce(&AuctionWorkletServiceImpl::DisconnectBidderWorklet,
                     base::Unretained(this), receiver_id));
}

void AuctionWorkletServiceImpl::LoadSellerWorklet(
    mojo::PendingReceiver<mojom::SellerWorklet> seller_worklet_receiver,
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
    mojom::TrustedSignalsPublicKeyPtr public_key) {
  std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers;
  for (size_t i = 0; i < auction_seller_v8_helper_holders_.size(); ++i) {
    v8_helpers.push_back(auction_seller_v8_helper_holders_[i]->V8Helper());
  }

  CHECK_EQ(v8_helpers.size(), shared_storage_hosts.size());

  auto seller_worklet = std::make_unique<SellerWorklet>(
      std::move(v8_helpers), std::move(shared_storage_hosts),
      pause_for_debugger_on_start, std::move(pending_url_loader_factory),
      std::move(auction_network_events_handler), decision_logic_url,
      trusted_scoring_signals_url, top_window_origin,
      std::move(permissions_policy_state), experiment_group_id,
      std::move(public_key),
      base::BindRepeating(
          &AuctionWorkletServiceImpl::GetNextSellerWorkletThreadIndex,
          base::Unretained(this)));
  auto* seller_worklet_ptr = seller_worklet.get();

  mojo::ReceiverId receiver_id = seller_worklets_.Add(
      std::move(seller_worklet), std::move(seller_worklet_receiver));

  seller_worklet_ptr->set_close_pipe_callback(
      base::BindOnce(&AuctionWorkletServiceImpl::DisconnectSellerWorklet,
                     base::Unretained(this), receiver_id));
}

size_t AuctionWorkletServiceImpl::GetNextSellerWorkletThreadIndex() {
  size_t result = g_next_seller_worklet_thread_index++;
  g_next_seller_worklet_thread_index %=
      auction_seller_v8_helper_holders_.size();
  return result;
}

void AuctionWorkletServiceImpl::DisconnectSellerWorklet(
    mojo::ReceiverId receiver_id,
    const std::string& reason) {
  seller_worklets_.RemoveWithReason(receiver_id, /*custom_reason_code=*/0,
                                    reason);
}

void AuctionWorkletServiceImpl::DisconnectBidderWorklet(
    mojo::ReceiverId receiver_id,
    const std::string& reason) {
  bidder_worklets_.RemoveWithReason(receiver_id, /*custom_reason_code=*/0,
                                    reason);
}

}  // namespace auction_worklet
