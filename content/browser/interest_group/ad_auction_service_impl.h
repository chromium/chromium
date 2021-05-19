// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "content/browser/interest_group/auction_runner.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_service_base.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class AuctionRunner;
class RenderFrameHost;

// Implements the AdAuctionService service called by Blink code.
class CONTENT_EXPORT AdAuctionServiceImpl final
    : public FrameServiceBase<blink::mojom::AdAuctionService>,
      public AuctionRunner::Delegate {
 public:
  // Factory method for creating an instance of this interface that is
  // bound to the lifetime of the frame or receiver (whichever is shorter).
  static void CreateMojoService(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver);

  // blink::mojom::AdAuctionService.
  void RunAdAuction(blink::mojom::AuctionAdConfigPtr config,
                    RunAdAuctionCallback callback) override;

  // AuctionRunner::Delegate implementation:
  network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory() override;
  network::mojom::URLLoaderFactory* GetTrustedURLLoaderFactory() override;
  auction_worklet::mojom::AuctionWorkletService* GetWorkletService() override;

  using FrameServiceBase::origin;
  using FrameServiceBase::render_frame_host;

 private:
  // `render_frame_host` must not be null, and FrameServiceBase guarantees
  // `this` will not outlive the `render_frame_host`.
  AdAuctionServiceImpl(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver);

  // `this` can only be destroyed by FrameServiceBase.
  ~AdAuctionServiceImpl() override;

  // Deletes `auction`.
  void OnAuctionComplete(RunAdAuctionCallback callback,
                         AuctionRunner* auction,
                         absl::optional<GURL> render_url,
                         absl::optional<GURL> bidder_report_url,
                         absl::optional<GURL> seller_report_url,
                         std::vector<std::string> errors);

  // This must be above `auction_worklet_service_`, since auctions may own
  // callbacks over the AuctionWorkletService pipe, and mojo pipes must be
  // destroyed before any callbacks that are bound to them.
  std::set<std::unique_ptr<AuctionRunner>, base::UniquePtrComparator> auctions_;

  mojo::Remote<auction_worklet::mojom::AuctionWorkletService>
      auction_worklet_service_;

  mojo::Remote<network::mojom::URLLoaderFactory> frame_url_loader_factory_;
  mojo::Remote<network::mojom::URLLoaderFactory> trusted_url_loader_factory_;

  base::WeakPtrFactory<AdAuctionServiceImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_IMPL_H_
