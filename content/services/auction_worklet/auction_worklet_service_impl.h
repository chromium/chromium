// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_WORKLET_SERVICE_IMPL_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_WORKLET_SERVICE_IMPL_H_

#include <vector>

#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"

namespace auction_worklet {

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

  // mojom::AuctionWorkletService implementation:
  void RunAuction(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
      blink::mojom::AuctionAdConfigPtr auction_config,
      std::vector<mojom::BiddingInterestGroupPtr> bidders,
      mojom::BrowserSignalsPtr browser_signals,
      mojom::AuctionWorkletService::RunAuctionCallback callback) override;

 private:
  mojo::Receiver<mojom::AuctionWorkletService> receiver_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_WORKLET_SERVICE_IMPL_H_
