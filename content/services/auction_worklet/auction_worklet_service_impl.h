// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_WORKLET_SERVICE_IMPL_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_WORKLET_SERVICE_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"

class GURL;

namespace url {
class Origin;
}

namespace auction_worklet {

namespace mojom {
class BidderWorklet;
class SellerWorklet;
}  // namespace mojom

// mojom::AuctionWorkletService implementation. This is intended to run in a
// sandboxed utility process.
class CONTENT_EXPORT AuctionWorkletServiceImpl
    : public mojom::AuctionWorkletService {
 public:
  explicit AuctionWorkletServiceImpl(const AuctionWorkletServiceImpl&) = delete;
  AuctionWorkletServiceImpl& operator=(const AuctionWorkletServiceImpl&) =
      delete;
  ~AuctionWorkletServiceImpl() override;

  // Factory method intended for use when running in the renderer.
  // Creates an instance owned by (and bound to) `receiver`.
  static void CreateForRenderer(
      mojo::PendingReceiver<mojom::AuctionWorkletService> receiver);

  // Factory method intended for use when running as a service.
  // Will be bound to `receiver` but owned by the return value
  // (which will normally be placed in care of a ServiceFactory).
  static std::unique_ptr<AuctionWorkletServiceImpl> CreateForService(
      mojo::PendingReceiver<mojom::AuctionWorkletService> receiver);

  std::vector<scoped_refptr<AuctionV8Helper>> AuctionV8HelpersForTesting();

  int NumBidderWorkletsForTesting() const { return bidder_worklets_.size(); }
  int NumSellerWorkletsForTesting() const { return seller_worklets_.size(); }

  // mojom::AuctionWorkletService implementation:
  void LoadBidderWorklet(
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
      mojom::TrustedSignalsPublicKeyPtr public_key) override;
  void LoadSellerWorklet(
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
      mojom::TrustedSignalsPublicKeyPtr public_key) override;

  // Returns an index in the seller thread pool, where the corresponding V8
  // thread will be used to execute the next task.
  size_t GetNextSellerWorkletThreadIndex();

 private:
  class V8HelperHolder;
  enum class ProcessModel { kDedicated, kShared };

  // Receiver may be null
  AuctionWorkletServiceImpl(
      ProcessModel process_model,
      mojo::PendingReceiver<mojom::AuctionWorkletService> receiver);

  void DisconnectSellerWorklet(mojo::ReceiverId receiver_id,
                               const std::string& reason);
  void DisconnectBidderWorklet(mojo::ReceiverId receiver_id,
                               const std::string& reason);

  ProcessModel process_model_;

  // These should be before `bidder_worklets_` and `seller_worklets_` as they
  // need to be destroyed after them, as the actual destruction of
  // V8HelperHolder may need to block to get V8 shutdown cleanly, which is
  // helped by worklets not being around to produce more work.
  std::vector<scoped_refptr<V8HelperHolder>> auction_bidder_v8_helper_holders_;
  std::vector<scoped_refptr<V8HelperHolder>> auction_seller_v8_helper_holders_;

  // This is bound when created via CreateForService(); in case of
  // CreateForRenderer() an external SelfOwnedReceiver is used instead.
  mojo::Receiver<mojom::AuctionWorkletService> receiver_;

  mojo::UniqueReceiverSet<mojom::BidderWorklet> bidder_worklets_;
  mojo::UniqueReceiverSet<mojom::SellerWorklet> seller_worklets_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_WORKLET_SERVICE_IMPL_H_
