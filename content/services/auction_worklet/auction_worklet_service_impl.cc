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
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/bidder_worklet.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/seller_worklet.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"

namespace auction_worklet {

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
      ProcessModel process_model) {
    if (!g_bidder_instance) {
      g_bidder_instance =
          new V8HelperHolder(process_model, WorkletType::kBidder);
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(g_bidder_instance->sequence_checker_);
    return g_bidder_instance;
  }

  static scoped_refptr<V8HelperHolder> SellerInstance(
      ProcessModel process_model) {
    if (!g_seller_instance) {
      g_seller_instance =
          new V8HelperHolder(process_model, WorkletType::kSeller);
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(g_seller_instance->sequence_checker_);
    return g_seller_instance;
  }

  const scoped_refptr<AuctionV8Helper>& V8Helper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!auction_v8_helper_) {
      auction_v8_helper_ =
          AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
      if (process_model_ == ProcessModel::kDedicated) {
        auction_v8_helper_->SetDestroyedCallback(base::BindOnce(
            &V8HelperHolder::FinishedDestroying, base::Unretained(this)));
      }
    }
    return auction_v8_helper_;
  }

 private:
  friend class base::RefCounted<V8HelperHolder>;

  explicit V8HelperHolder(ProcessModel process_model, WorkletType worklet_type)
      : process_model_(process_model), worklet_type_(worklet_type) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(*GetHelperHolderInstance(), nullptr);
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

    V8HelperHolder** instance = GetHelperHolderInstance();
    DCHECK_EQ(*instance, this);
    *instance = nullptr;
  }

  void FinishedDestroying() {
    DCHECK_EQ(process_model_, ProcessModel::kDedicated);
    // This method runs on the V8 thread, while the object lives on the
    // service's mojo thread.
    wait_for_v8_shutdown_.Signal();
  }

  V8HelperHolder** GetHelperHolderInstance() {
    if (worklet_type_ == WorkletType::kBidder)
      return &g_bidder_instance;
    else
      return &g_seller_instance;
  }

  static V8HelperHolder* g_bidder_instance;
  static V8HelperHolder* g_seller_instance;

  scoped_refptr<AuctionV8Helper> auction_v8_helper_;
  const ProcessModel process_model_;
  const WorkletType worklet_type_;

  base::WaitableEvent wait_for_v8_shutdown_;
  SEQUENCE_CHECKER(sequence_checker_);
};

AuctionWorkletServiceImpl::V8HelperHolder*
    AuctionWorkletServiceImpl::V8HelperHolder::g_bidder_instance = nullptr;
AuctionWorkletServiceImpl::V8HelperHolder*
    AuctionWorkletServiceImpl::V8HelperHolder::g_seller_instance = nullptr;

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
    : auction_bidder_v8_helper_holder_(
          V8HelperHolder::BidderInstance(process_model)),
      auction_seller_v8_helper_holder_(
          V8HelperHolder::SellerInstance(process_model)),
      receiver_(this, std::move(receiver)) {}

AuctionWorkletServiceImpl::~AuctionWorkletServiceImpl() = default;

std::vector<scoped_refptr<AuctionV8Helper>>
AuctionWorkletServiceImpl::AuctionV8HelpersForTesting() {
  std::vector<scoped_refptr<AuctionV8Helper>> result;
  result.push_back(auction_bidder_v8_helper_holder_->V8Helper());
  result.push_back(auction_seller_v8_helper_holder_->V8Helper());
  return result;
}

void AuctionWorkletServiceImpl::LoadBidderWorklet(
    mojo::PendingReceiver<mojom::BidderWorklet> bidder_worklet_receiver,
    mojo::PendingRemote<mojom::AuctionSharedStorageHost>
        shared_storage_host_remote,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& script_source_url,
    const absl::optional<GURL>& wasm_helper_url,
    const absl::optional<GURL>& trusted_bidding_signals_url,
    const url::Origin& top_window_origin,
    mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
    bool has_experiment_group_id,
    uint16_t experiment_group_id) {
  auto bidder_worklet = std::make_unique<BidderWorklet>(
      auction_bidder_v8_helper_holder_->V8Helper(),
      std::move(shared_storage_host_remote), pause_for_debugger_on_start,
      std::move(pending_url_loader_factory), script_source_url, wasm_helper_url,
      trusted_bidding_signals_url, top_window_origin,
      std::move(permissions_policy_state),
      has_experiment_group_id ? absl::make_optional(experiment_group_id)
                              : absl::nullopt);
  auto* bidder_worklet_ptr = bidder_worklet.get();

  mojo::ReceiverId receiver_id = bidder_worklets_.Add(
      std::move(bidder_worklet), std::move(bidder_worklet_receiver));

  bidder_worklet_ptr->set_close_pipe_callback(
      base::BindOnce(&AuctionWorkletServiceImpl::DisconnectBidderWorklet,
                     base::Unretained(this), receiver_id));
}

void AuctionWorkletServiceImpl::LoadSellerWorklet(
    mojo::PendingReceiver<mojom::SellerWorklet> seller_worklet_receiver,
    mojo::PendingRemote<mojom::AuctionSharedStorageHost>
        shared_storage_host_remote,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& decision_logic_url,
    const absl::optional<GURL>& trusted_scoring_signals_url,
    const url::Origin& top_window_origin,
    mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
    bool has_experiment_group_id,
    uint16_t experiment_group_id) {
  auto seller_worklet = std::make_unique<SellerWorklet>(
      auction_seller_v8_helper_holder_->V8Helper(),
      std::move(shared_storage_host_remote), pause_for_debugger_on_start,
      std::move(pending_url_loader_factory), decision_logic_url,
      trusted_scoring_signals_url, top_window_origin,
      std::move(permissions_policy_state),
      has_experiment_group_id ? absl::make_optional(experiment_group_id)
                              : absl::nullopt);
  auto* seller_worklet_ptr = seller_worklet.get();

  mojo::ReceiverId receiver_id = seller_worklets_.Add(
      std::move(seller_worklet), std::move(seller_worklet_receiver));

  seller_worklet_ptr->set_close_pipe_callback(
      base::BindOnce(&AuctionWorkletServiceImpl::DisconnectSellerWorklet,
                     base::Unretained(this), receiver_id));
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
