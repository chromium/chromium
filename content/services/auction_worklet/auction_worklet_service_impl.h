// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_WORKLET_SERVICE_IMPL_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_WORKLET_SERVICE_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
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

class BidderWorklet;
class SellerWorklet;

// mojom::AuctionWorkletService implementation. This is intended to run in a
// sandboxed utility process.
class AuctionWorkletServiceImpl : public mojom::AuctionWorkletService {
 public:
  explicit AuctionWorkletServiceImpl(
      mojo::PendingReceiver<mojom::AuctionWorkletService> receiver);
  explicit AuctionWorkletServiceImpl(const AuctionWorkletServiceImpl&) = delete;
  AuctionWorkletServiceImpl& operator=(const AuctionWorkletServiceImpl&) =
      delete;
  ~AuctionWorkletServiceImpl() override;

  const scoped_refptr<AuctionV8Helper>& AuctionV8HelperForTesting() {
    return auction_v8_helper_;
  }

  // mojom::AuctionWorkletService implementation:
  void LoadBidderWorklet(
      mojo::PendingReceiver<mojom::BidderWorklet> bidder_worklet_receiver,
      bool pause_for_debugger_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& script_source_url,
      const absl::optional<GURL>& wasm_helper_url,
      const absl::optional<GURL>& trusted_bidding_signals_url,
      const url::Origin& top_window_origin) override;
  void LoadSellerWorklet(
      mojo::PendingReceiver<mojom::SellerWorklet> seller_worklet_receiver,
      bool pause_for_debugger_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& decision_logic_url,
      const absl::optional<GURL>& trusted_scoring_signals_url,
      const url::Origin& top_window_origin) override;

 private:
  void DisconnectSellerWorklet(mojo::ReceiverId receiver_id,
                               const std::string& reason);
  void DisconnectBidderWorklet(mojo::ReceiverId receiver_id,
                               const std::string& reason);

  mojo::Receiver<mojom::AuctionWorkletService> receiver_;

  scoped_refptr<AuctionV8Helper> auction_v8_helper_;

  mojo::UniqueReceiverSet<mojom::BidderWorklet> bidder_worklets_;
  mojo::UniqueReceiverSet<mojom::SellerWorklet> seller_worklets_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_WORKLET_SERVICE_IMPL_H_
