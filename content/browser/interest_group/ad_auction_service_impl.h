// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_IMPL_H_

#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
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

class AuctionURLLoaderFactoryProxy;
class RenderFrameHost;

// Implements the AdAuctionService service called by Blink code.
class CONTENT_EXPORT AdAuctionServiceImpl final
    : public FrameServiceBase<blink::mojom::AdAuctionService> {
 public:
  // Factory method for creating an instance of this interface that is
  // bound to the lifetime of the frame or receiver (whichever is shorter).
  static void CreateMojoService(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver);

  // blink::mojom::AdAuctionService.
  void RunAdAuction(blink::mojom::AuctionAdConfigPtr config,
                    RunAdAuctionCallback callback) override;

 private:
  // `render_frame_host` must not be null, and FrameServiceBase guarantees
  // `this` will not outlive the `render_frame_host`.
  AdAuctionServiceImpl(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver);

  // `this` can only be destroyed by FrameServiceBase.
  ~AdAuctionServiceImpl() override;

  InterestGroupManager* GetInterestGroupManager();

  // Launches worklet service if not already launched.
  void LaunchWorkletServiceIfNeeded();
  // Gets a list of all interest groups with their bidding information
  // associated with the provided owners. These interest groups will
  // be the bidders in the ad auction.
  void GetInterestGroupsFromStorage(blink::mojom::AuctionAdConfigPtr config,
                                    const std::vector<url::Origin>& buyers,
                                    RunAdAuctionCallback callback);

  // Reads interest groups from the storage for the next buyer, then either
  // reschedules itself or calls StartAuction (if all interest groups are
  // retrieved).
  void GetInterestGroup(
      std::vector<url::Origin> buyers,
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders,
      blink::mojom::AuctionAdConfigPtr config,
      RunAdAuctionCallback callback,
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
          interest_groups);

  // Calls auction worklet service's runAuction.
  void StartAuction(
      blink::mojom::AuctionAdConfigPtr config,
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders,
      RunAdAuctionCallback callback);

  // Deals with works such as reporting auction results after auction worklet
  // service's runAuction finishes.
  //
  // `url_loader_factory_proxy` is the proxy for network requests issued by the
  // auctions worklet, and is an argument so that it will be destroywed when an
  // auction completes, or the callback is destroyed (e.g., on navigation away).
  void WorkletComplete(
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders_copy,
      RunAdAuctionCallback* callback,
      std::unique_ptr<AuctionURLLoaderFactoryProxy> url_loader_factory_proxy,
      base::ScopedClosureRunner on_crash,
      const GURL& render_url,
      const url::Origin& owner,
      const std::string& name,
      auction_worklet::mojom::WinningBidderReportPtr bidder_report,
      auction_worklet::mojom::SellerReportPtr seller_report);

  // Returns an untrusted URLLoaderFactory created by the RenderFrameHost,
  // suitable for loading URLs like subresources. Caches the factory in
  // `frame_url_loader_factory_` for reuse.
  network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory();

  // Returns a trusted URLLoaderFactory. Consumers should set
  // ResourceRequest::TrustedParams to specify a NetworkIsolationKey when using
  // the returned factory. Caches the factory in `trusted_url_loader_factory_`
  // for reuse.
  network::mojom::URLLoaderFactory* GetTrustedURLLoaderFactory();

  mojo::Remote<auction_worklet::mojom::AuctionWorkletService>
      auction_worklet_service_;

  mojo::Remote<network::mojom::URLLoaderFactory> frame_url_loader_factory_;
  mojo::Remote<network::mojom::URLLoaderFactory> trusted_url_loader_factory_;

  base::WeakPtrFactory<AdAuctionServiceImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_IMPL_H_
