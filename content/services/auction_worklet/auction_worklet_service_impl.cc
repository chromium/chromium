// Copyright 2021 The Chromium Authors. All rights reserved.
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

// Make sure we only use a single AuctionV8Helper, including V8 thread and V8
// isolate, no matter how many services there are. Assumes usages from a single
// thread. In kDedicated mode, assumes this will be destroyed strictly before
// task scheduling is shut down.
class AuctionWorkletServiceImpl::V8HelperHolder
    : public base::RefCounted<V8HelperHolder> {
 public:
  static V8HelperHolder* Instance(ProcessModel process_model) {
    if (!g_instance)
      g_instance = new V8HelperHolder(process_model);
    DCHECK_CALLED_ON_VALID_SEQUENCE(g_instance->sequence_checker_);
    return g_instance;
  }

  const scoped_refptr<AuctionV8Helper>& v8_helper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return auction_v8_helper_;
  }

 private:
  friend class base::RefCounted<V8HelperHolder>;

  explicit V8HelperHolder(ProcessModel process_model)
      : auction_v8_helper_(
            AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner())),
        process_model_(process_model) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (process_model_ == ProcessModel::kDedicated) {
      auction_v8_helper_->SetDestroyedCallback(base::BindOnce(
          &V8HelperHolder::FinishedDestroying, base::Unretained(this)));
    }
  }

  ~V8HelperHolder() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (process_model_ == ProcessModel::kDedicated) {
      // ~V8HelperHolder running means there are no more instances of the
      // service, so the service process itself may be about to be shutdown.
      // Wait until `auction_v8_helper_` is destroyed to make sure no V8 things
      // are running on its thread, since they may crash if task scheduling
      // suddenly becomes impossible.
      auction_v8_helper_.reset();
      wait_for_v8_shutdown_.Wait();
    }
    g_instance = nullptr;
  }

  void FinishedDestroying() {
    DCHECK_EQ(process_model_, ProcessModel::kDedicated);
    // This method runs on the V8 thread, while the object lives on the
    // service's mojo thread.
    wait_for_v8_shutdown_.Signal();
  }

  static V8HelperHolder* g_instance;

  scoped_refptr<AuctionV8Helper> auction_v8_helper_;
  const ProcessModel process_model_;
  base::WaitableEvent wait_for_v8_shutdown_;

  SEQUENCE_CHECKER(sequence_checker_);
};

AuctionWorkletServiceImpl::V8HelperHolder*
    AuctionWorkletServiceImpl::V8HelperHolder::g_instance = nullptr;

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
    : auction_v8_helper_holder_(V8HelperHolder::Instance(process_model)),
      receiver_(this, std::move(receiver)) {}

AuctionWorkletServiceImpl::~AuctionWorkletServiceImpl() = default;

scoped_refptr<AuctionV8Helper>
AuctionWorkletServiceImpl::AuctionV8HelperForTesting() {
  return auction_v8_helper_holder_->v8_helper();
}

void AuctionWorkletServiceImpl::LoadBidderWorklet(
    mojo::PendingReceiver<mojom::BidderWorklet> bidder_worklet_receiver,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& script_source_url,
    const absl::optional<GURL>& wasm_helper_url,
    const absl::optional<GURL>& trusted_bidding_signals_url,
    const url::Origin& top_window_origin,
    bool has_experiment_group_id,
    uint16_t experiment_group_id) {
  auto bidder_worklet = std::make_unique<BidderWorklet>(
      auction_v8_helper_holder_->v8_helper(), pause_for_debugger_on_start,
      std::move(pending_url_loader_factory), script_source_url, wasm_helper_url,
      trusted_bidding_signals_url, top_window_origin,
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
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& decision_logic_url,
    const absl::optional<GURL>& trusted_scoring_signals_url,
    const url::Origin& top_window_origin,
    bool has_experiment_group_id,
    uint16_t experiment_group_id) {
  auto seller_worklet = std::make_unique<SellerWorklet>(
      auction_v8_helper_holder_->v8_helper(), pause_for_debugger_on_start,
      std::move(pending_url_loader_factory), decision_logic_url,
      trusted_scoring_signals_url, top_window_origin,
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
