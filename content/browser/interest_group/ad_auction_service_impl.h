// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/auction_runner.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/document_service.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "third_party/blink/public/mojom/parakeet/ad_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class InterestGroupManagerImpl;
class RenderFrameHost;
class RenderFrameHostImpl;
class PrivateAggregationManager;

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
  void JoinInterestGroup(const blink::InterestGroup& group,
                         JoinInterestGroupCallback callback) override;
  void LeaveInterestGroup(const url::Origin& owner,
                          const std::string& name,
                          LeaveInterestGroupCallback callback) override;
  void LeaveInterestGroupForDocument() override;
  void UpdateAdInterestGroups() override;
  void RunAdAuction(
      const blink::AuctionConfig& config,
      mojo::PendingReceiver<blink::mojom::AbortableAdAuction> abort_receiver,
      RunAdAuctionCallback callback) override;
  void DeprecatedGetURLFromURN(
      const GURL& urn_url,
      bool send_reports,
      DeprecatedGetURLFromURNCallback callback) override;
  void DeprecatedReplaceInURN(
      const GURL& urn_url,
      std::vector<blink::mojom::ReplacementPtr> replacements,
      DeprecatedReplaceInURNCallback callback) override;
  void CreateAdRequest(blink::mojom::AdRequestConfigPtr config,
                       CreateAdRequestCallback callback) override;
  void FinalizeAd(const std::string& ads_guid,
                  const blink::AuctionConfig& config,
                  FinalizeAdCallback callback) override;

  scoped_refptr<network::WrapperSharedURLLoaderFactory>
  GetRefCountedTrustedURLLoaderFactory();

  // AuctionWorkletManager::Delegate implementation:
  network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory() override;
  network::mojom::URLLoaderFactory* GetTrustedURLLoaderFactory() override;
  void PreconnectSocket(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key) override;
  RenderFrameHostImpl* GetFrame() override;
  scoped_refptr<SiteInstance> GetFrameSiteInstance() override;
  network::mojom::ClientSecurityStatePtr GetClientSecurityState() override;

  using DocumentService::origin;
  using DocumentService::render_frame_host;

 private:
  using ReporterList = std::list<std::unique_ptr<InterestGroupAuctionReporter>>;

  // `render_frame_host` must not be null, and DocumentService guarantees
  // `this` will not outlive the `render_frame_host`.
  AdAuctionServiceImpl(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver);

  // `this` can only be destroyed by DocumentService.
  ~AdAuctionServiceImpl() override;

  // Checks if a join or leave interest group is allowed to be sent from the
  // current renderer. If not, returns false and invokes
  // ReportBadMessageAndDeleteThis().
  bool JoinOrLeaveApiAllowedFromRenderer(const url::Origin& owner);

  // Returns true if `origin` is allowed to perform the specified
  // `interest_group_api_operation` in this frame. Must be called on worklet /
  // interest group origins before using them in any interest group API.
  bool IsInterestGroupAPIAllowed(ContentBrowserClient::InterestGroupApiOperation
                                     interest_group_api_operation,
                                 const url::Origin& origin) const;

  // Deletes `auction`.
  void OnAuctionComplete(
      RunAdAuctionCallback callback,
      GURL urn_uuid,
      AuctionRunner* auction,
      bool manually_aborted,
      absl::optional<blink::InterestGroupKey> winning_group_key,
      absl::optional<GURL> render_url,
      std::vector<GURL> ad_component_urls,
      std::string winning_group_ad_metadata,
      std::vector<GURL> debug_loss_report_urls,
      std::vector<GURL> debug_win_report_urls,
      std::map<
          url::Origin,
          std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>>
          private_aggregation_requests,
      blink::InterestGroupSet interest_groups_that_bid,
      base::flat_set<std::string> k_anon_keys_to_join,
      std::vector<std::string> errors,
      std::unique_ptr<InterestGroupAuctionReporter> reporter);

  void OnReporterComplete(ReporterList::iterator reporter_it,
                          RunAdAuctionCallback callback,
                          GURL urn_uuid,
                          blink::InterestGroupKey winning_group_key,
                          GURL render_url,
                          std::vector<GURL> ad_component_urls,
                          std::string winning_group_ad_metadata,
                          std::vector<GURL> debug_loss_report_urls,
                          std::vector<GURL> debug_win_report_urls,
                          blink::InterestGroupSet interest_groups_that_bid,
                          base::flat_set<std::string> k_anon_keys_to_join);

  // Calls LogWebFeatureForCurrentPage() for the frame to inform it of FLEDGE
  // private aggregation API usage, if `private_aggregation_requests` is
  // non-empty.
  void MaybeLogPrivateAggregationFeature(
      const std::map<
          url::Origin,
          std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>>&
          private_aggregation_requests);

  InterestGroupManagerImpl& GetInterestGroupManager() const;

  url::Origin GetTopWindowOrigin() const;

  // To avoid race conditions associated with top frame navigations (mentioned
  // in document_service.h), we need to save the values of the main frame
  // URL and origin in the constructor.
  const url::Origin main_frame_origin_;
  const GURL main_frame_url_;

  mojo::Remote<network::mojom::URLLoaderFactory> frame_url_loader_factory_;
  mojo::Remote<network::mojom::URLLoaderFactory> trusted_url_loader_factory_;

  // Ref counted wrapper of `trusted_url_loader_factory_`. This will be used for
  // reporting requests, which might happen after the frame is destroyed, when
  // `trusted_url_loader_factory_` no longer being available.
  scoped_refptr<network::WrapperSharedURLLoaderFactory>
      ref_counted_trusted_url_loader_factory_;

  // This must be before `auctions_`, since auctions may own references to
  // worklets it manages.
  AuctionWorkletManager auction_worklet_manager_;

  // Use a map instead of a list so can remove entries without destroying them.
  // TODO(mmenke): Switch to std::set() and use extract() once that's allowed.
  std::map<AuctionRunner*, std::unique_ptr<AuctionRunner>> auctions_;
  ReporterList reporters_;

  // Safe to keep as it will outlive the associated `RenderFrameHost` and
  // therefore `this`, being tied to the lifetime of the `StoragePartition`.
  const raw_ptr<PrivateAggregationManager> private_aggregation_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_IMPL_H_
