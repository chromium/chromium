// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_IMPL_H_

#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "content/browser/interest_group/auction_runner.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "third_party/blink/public/mojom/parakeet/ad_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class InterestGroupManagerImpl;
class RenderFrameHost;
class RenderFrameHostImpl;

// Implements the AdAuctionService service called by Blink code.
class CONTENT_EXPORT AdAuctionServiceImpl final
    : public DocumentService<blink::mojom::AdAuctionService>,
      public AuctionWorkletManager::Delegate {
 public:
  // Factory method for creating an instance of this interface that is
  // bound to the lifetime of the frame or receiver (whichever is shorter).
  static void CreateMojoService(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver);

  // blink::mojom::AdAuctionService.
  void JoinInterestGroup(const blink::InterestGroup& group) override;
  void LeaveInterestGroup(const url::Origin& owner,
                          const std::string& name) override;
  void UpdateAdInterestGroups() override;
  void RunAdAuction(blink::mojom::AuctionAdConfigPtr config,
                    RunAdAuctionCallback callback) override;
  void DeprecatedGetURLFromURN(
      const GURL& urn_url,
      DeprecatedGetURLFromURNCallback callback) override;
  void CreateAdRequest(blink::mojom::AdRequestConfigPtr config,
                       CreateAdRequestCallback callback) override;
  void FinalizeAd(const std::string& ads_guid,
                  blink::mojom::AuctionAdConfigPtr config,
                  FinalizeAdCallback callback) override;

  // AuctionRunner::Delegate implementation:
  network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory() override;
  network::mojom::URLLoaderFactory* GetTrustedURLLoaderFactory() override;
  RenderFrameHostImpl* GetFrame() override;
  network::mojom::ClientSecurityStatePtr GetClientSecurityState() override;

  using DocumentService::origin;
  using DocumentService::render_frame_host;

 private:
  // `render_frame_host` must not be null, and DocumentService guarantees
  // `this` will not outlive the `render_frame_host`.
  AdAuctionServiceImpl(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver);

  // `this` can only be destroyed by DocumentService.
  ~AdAuctionServiceImpl() override;

  // Returns true if `origin` is allowed to perform the specified
  // `interest_group_api_operation` in this frame. Must be called on worklet /
  // interest group origins before using them in any interest group API.
  bool IsInterestGroupAPIAllowed(ContentBrowserClient::InterestGroupApiOperation
                                     interest_group_api_operation,
                                 const url::Origin& origin) const;

  // Deletes `auction`.
  void OnAuctionComplete(
      RunAdAuctionCallback callback,
      AuctionRunner* auction,
      absl::optional<AuctionRunner::InterestGroupKey> winning_group_id,
      absl::optional<GURL> render_url,
      std::vector<GURL> ad_component_urls,
      std::vector<GURL> report_urls,
      std::vector<GURL> debug_loss_report_urls,
      std::vector<GURL> debug_win_report_urls,
      std::vector<std::string> errors);

  InterestGroupManagerImpl& GetInterestGroupManager() const;

  url::Origin GetTopWindowOrigin() const;

  // To avoid race conditions associated with top frame navigations (mentioned
  // in document_service.h), we need to save the values of the main frame
  // URL and origin in the constructor.
  const url::Origin main_frame_origin_;
  const GURL main_frame_url_;

  mojo::Remote<network::mojom::URLLoaderFactory> frame_url_loader_factory_;
  mojo::Remote<network::mojom::URLLoaderFactory> trusted_url_loader_factory_;

  // This must be before `auctions_`, since auctions may own references to
  // worklets it manages.
  AuctionWorkletManager auction_worklet_manager_;

  std::set<std::unique_ptr<AuctionRunner>, base::UniquePtrComparator> auctions_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_IMPL_H_
