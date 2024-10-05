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
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/auction_nonce_manager.h"
#include "content/browser/interest_group/auction_runner.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/bidding_and_auction_serializer.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/document_service.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "third_party/blink/public/mojom/parakeet/ad_request.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class InterestGroupManagerImpl;
class PrivateAggregationManager;
class ReconnectableURLLoaderFactory;
class RenderFrameHost;
class RenderFrameHostImpl;
struct BiddingAndAuctionServerKey;

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
  void ClearOriginJoinedInterestGroups(
      const url::Origin& owner,
      const std::vector<std::string>& interest_groups_to_keep,
      ClearOriginJoinedInterestGroupsCallback callback) override;
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
      const std::vector<blink::AuctionConfig::AdKeywordReplacement>&
          replacements,
      DeprecatedReplaceInURNCallback callback) override;
  void GetInterestGroupAdAuctionData(
      const url::Origin& seller,
      const std::optional<url::Origin>& coordinator,
      blink::mojom::AuctionDataConfigPtr config,
      GetInterestGroupAdAuctionDataCallback callback) override;
  void CreateAdRequest(blink::mojom::AdRequestConfigPtr config,
                       CreateAdRequestCallback callback) override;
  void FinalizeAd(const std::string& ads_guid,
                  const blink::AuctionConfig& config,
                  FinalizeAdCallback callback) override;

  scoped_refptr<network::SharedURLLoaderFactory>
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
  std::optional<std::string> GetCookieDeprecationLabel() override;
  void GetBiddingAndAuctionServerKey(
      const std::optional<url::Origin>& coordinator,
      base::OnceCallback<void(base::expected<BiddingAndAuctionServerKey,
                                             std::string>)> callback) override;

  using DocumentService::origin;
  using DocumentService::render_frame_host;

 private:
  using ReporterList = std::list<std::unique_ptr<InterestGroupAuctionReporter>>;

  class BiddingAndAuctionDataConstructionState {
   public:
    BiddingAndAuctionDataConstructionState();
    BiddingAndAuctionDataConstructionState(
        BiddingAndAuctionDataConstructionState&& other);
    ~BiddingAndAuctionDataConstructionState();

    base::TimeTicks start_time;  // time used for metrics
    std::unique_ptr<BiddingAndAuctionServerKey> key;
    std::unique_ptr<BiddingAndAuctionData> data;
    base::Uuid request_id;
    url::Origin seller;
    std::optional<url::Origin> coordinator;
    base::Time timestamp;  // timestamp to include in the request.
    blink::mojom::AuctionDataConfigPtr config;
    GetInterestGroupAdAuctionDataCallback callback;
  };

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
  bool JoinOrLeaveApiAllowedFromRenderer(const url::Origin& owner,
                                         const char* invoked_method);

  // Checks if `feature` is enabled for the frame, and returns true if so, and
  // false if not. Additionally, if the feature is enabled, prints a warning to
  // the console if the feature would not be enabled if the default state of the
  // feature across cross-origin frames were switched to disabled instead of
  // enabled.
  bool IsPermissionPolicyEnabledAndWarnIfNeeded(
      blink::mojom::PermissionsPolicyFeature feature,
      const char* method);

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
      bool aborted_by_script,
      std::optional<blink::InterestGroupKey> winning_group_key,
      std::optional<blink::AdSize> requested_ad_size,
      std::optional<blink::AdDescriptor> ad_descriptor,
      std::vector<blink::AdDescriptor> ad_component_descriptors,
      std::vector<std::string> errors,
      std::unique_ptr<InterestGroupAuctionReporter> reporter,
      bool contained_server_auction,
      bool contained_on_device_auction,
      AuctionResult result);

  void OnReporterComplete(ReporterList::iterator reporter_it);

  void MaybeLogPrivateAggregationFeatures(
      const std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>&
          private_aggregation_requests);

  // On failing to fetch ad auction data, call the first callback in
  // ba_data_callbacks_ & start loading the next following request in
  // ba_data_callbacks_.
  void ReturnEmptyGetInterestGroupAdAuctionDataCallback(const std::string& msg);
  void LoadAuctionDataAndKeyForNextQueuedRequest();
  void OnGotAuctionData(base::Uuid request_id, BiddingAndAuctionData data);
  void OnGotBiddingAndAuctionServerKey(
      base::Uuid request_id,
      base::expected<BiddingAndAuctionServerKey, std::string> maybe_key);
  void OnGotAuctionDataAndKey(base::Uuid request_id);

  InterestGroupManagerImpl& GetInterestGroupManager() const;

  url::Origin GetTopWindowOrigin() const;

  void CreateUnderlyingTrustedURLLoaderFactory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory);

  AdAuctionPageData* GetAdAuctionPageData();

  // For each buyer in `config`, preconnect to its origin and bidding signals
  // origin if the origins have been cached from previous interest group joins
  // or auctions. This function needs to be called separately to preconnect to
  // origins for `config`'s component auctions. Returns the number of buyers
  // that were preconnected.
  size_t PreconnectToBuyerOrigins(const blink::AuctionConfig& config);

  // To avoid race conditions associated with top frame navigations (mentioned
  // in document_service.h), we need to save the values of the main frame
  // URL and origin in the constructor.
  const url::Origin main_frame_origin_;
  const GURL main_frame_url_;

  mojo::Remote<network::mojom::URLLoaderFactory> frame_url_loader_factory_;

  // A URLLoaderFactory connecting to the underlying factory created by
  // CreateUnderlyingTrustedURLLoaderFactory(), with reconnecting support. This
  // can be used for reporting requests, which might happen after the frame is
  // destroyed.
  scoped_refptr<ReconnectableURLLoaderFactory>
      ref_counted_trusted_url_loader_factory_;

  // Used to create AuctionMetricsRecorders, which store data needed to record
  // UKM. This must be before `auction_worklet_manager_`, since worklet owners
  // may keep references to the AuctionMetricsRecorders owned by the
  // `auction_metrics_recorder_manager_`.
  AuctionMetricsRecorderManager auction_metrics_recorder_manager_;

  // This must be before `auctions_`, since auctions may own references to
  // worklets it manages.
  AuctionWorkletManager auction_worklet_manager_;

  // Manages auction nonces issued by prior calls to CreateAuctionNonce,
  // which are used by subsequent calls to RunAdAuction.
  AuctionNonceManager auction_nonce_manager_;

  // Use a map instead of a list so can remove entries without destroying them.
  // TODO(mmenke): Switch to std::set() and use extract() once that's allowed.
  std::map<AuctionRunner*, std::unique_ptr<AuctionRunner>> auctions_;
  ReporterList reporters_;

  // Safe to keep as it will outlive the associated `RenderFrameHost` and
  // therefore `this`, being tied to the lifetime of the `StoragePartition`.
  const raw_ptr<PrivateAggregationManager> private_aggregation_manager_;

  // Whether a UseCounter has already been logged for usage of the Private
  // Aggregation API in general, the extended Private Aggregation API and the
  // Private Aggregation API's enableDebugMode(), respectively.
  bool has_logged_private_aggregation_web_features_ = false;
  bool has_logged_extended_private_aggregation_web_feature_ = false;
  bool has_logged_private_aggregation_enable_debug_mode_web_feature_ = false;
  bool has_logged_private_aggregation_filtering_id_web_feature_ = false;

  // Track the state of GetInterestGroupAdAuctionData calls. One request will be
  // handled at a time (the first in the queue). The first
  // BiddingAndAuctionDataConstructionState's data and key will be edited in
  // place as they are loaded.
  base::queue<BiddingAndAuctionDataConstructionState> ba_data_callbacks_;

  // True if a feature is currently enabled, but would be disabled if the
  // default policy for the feature were switched to EnableForSelf. Lazily
  // populated.
  std::map<blink::mojom::PermissionsPolicyFeature, bool>
      should_warn_about_feature_;

  base::WeakPtrFactory<AdAuctionServiceImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_SERVICE_IMPL_H_
