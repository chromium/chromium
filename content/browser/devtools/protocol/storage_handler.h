// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/optional_ref.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/storage.h"
#include "content/browser/interest_group/devtools_enums.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "storage/browser/quota/quota_manager.h"

namespace storage {
class QuotaOverrideHandle;
}

namespace content {
class AttributionManager;
class StoragePartition;

namespace protocol {

class StorageHandler
    : public DevToolsDomainHandler,
      public Storage::Backend,
      public content::InterestGroupManagerImpl::InterestGroupObserver,
      public AttributionObserver {
 public:
  explicit StorageHandler(DevToolsAgentHostClient* client);

  StorageHandler(const StorageHandler&) = delete;
  StorageHandler& operator=(const StorageHandler&) = delete;

  ~StorageHandler() override;

  static std::vector<StorageHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  // content::protocol::DevToolsDomainHandler
  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  Response Disable() override;

  bool interest_group_auction_tracking_enabled() const {
    return interest_group_auction_tracking_enabled_;
  }

  // content::protocol::storage::Backend
  Response GetStorageKeyForFrame(const std::string& frame_id,
                                 std::string* serialized_storage_key) override;
  void ClearDataForOrigin(
      const std::string& origin,
      const std::string& storage_types,
      std::unique_ptr<ClearDataForOriginCallback> callback) override;
  void ClearDataForStorageKey(
      const std::string& storage_key,
      const std::string& storage_types,
      std::unique_ptr<ClearDataForStorageKeyCallback> callback) override;
  void GetUsageAndQuota(
      const String& origin,
      std::unique_ptr<GetUsageAndQuotaCallback> callback) override;

  // Storage Quota Override
  void GetQuotaOverrideHandle();
  void OverrideQuotaForOrigin(
      const String& origin,
      Maybe<double> quota_size,
      std::unique_ptr<OverrideQuotaForOriginCallback> callback) override;

  // Cookies management
  void GetCookies(
      Maybe<std::string> browser_context_id,
      std::unique_ptr<Storage::Backend::GetCookiesCallback> callback) override;

  void SetCookies(
      std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
      Maybe<std::string> browser_context_id,
      std::unique_ptr<Storage::Backend::SetCookiesCallback> callback) override;

  void ClearCookies(Maybe<std::string> browser_context_id,
                    std::unique_ptr<Storage::Backend::ClearCookiesCallback>
                        callback) override;

  // Ignores all double calls to track an origin.
  Response TrackCacheStorageForOrigin(const std::string& origin) override;
  Response TrackCacheStorageForStorageKey(
      const std::string& storage_key) override;
  Response UntrackCacheStorageForOrigin(const std::string& origin) override;
  Response UntrackCacheStorageForStorageKey(
      const std::string& storage_key) override;
  Response TrackIndexedDBForOrigin(const std::string& origin) override;
  Response TrackIndexedDBForStorageKey(const std::string& storage_key) override;
  Response UntrackIndexedDBForOrigin(const std::string& origin) override;
  Response UntrackIndexedDBForStorageKey(
      const std::string& storage_key) override;

  void GetTrustTokens(
      std::unique_ptr<GetTrustTokensCallback> callback) override;
  void ClearTrustTokens(
      const std::string& issuerOrigin,
      std::unique_ptr<ClearTrustTokensCallback> callback) override;

  void GetInterestGroupDetails(
      const std::string& owner_origin_string,
      const std::string& name,
      std::unique_ptr<GetInterestGroupDetailsCallback> callback) override;
  Response SetInterestGroupTracking(bool enable) override;
  Response SetInterestGroupAuctionTracking(bool enable) override;

  void GetSharedStorageMetadata(
      const std::string& owner_origin_string,
      std::unique_ptr<GetSharedStorageMetadataCallback> callback) override;
  void GetSharedStorageEntries(
      const std::string& owner_origin_string,
      std::unique_ptr<GetSharedStorageEntriesCallback> callback) override;
  void SetSharedStorageEntry(
      const std::string& owner_origin_string,
      const std::string& key,
      const std::string& value,
      Maybe<bool> ignore_if_present,
      std::unique_ptr<SetSharedStorageEntryCallback> callback) override;
  void DeleteSharedStorageEntry(
      const std::string& owner_origin_string,
      const std::string& key,
      std::unique_ptr<DeleteSharedStorageEntryCallback> callback) override;
  void ClearSharedStorageEntries(
      const std::string& owner_origin_string,
      std::unique_ptr<ClearSharedStorageEntriesCallback> callback) override;
  Response SetSharedStorageTracking(bool enable) override;
  void ResetSharedStorageBudget(
      const std::string& owner_origin_string,
      std::unique_ptr<ResetSharedStorageBudgetCallback> callback) override;

  DispatchResponse SetStorageBucketTracking(
      const std::string& serialized_storage_key,
      bool enable) override;

  DispatchResponse DeleteStorageBucket(
      std::unique_ptr<protocol::Storage::StorageBucket> bucket) override;

  void SetAttributionReportingLocalTestingMode(
      bool enabled,
      std::unique_ptr<SetAttributionReportingLocalTestingModeCallback>)
      override;
  Response SetAttributionReportingTracking(bool enable) override;
  void SendPendingAttributionReports(
      std::unique_ptr<SendPendingAttributionReportsCallback>) override;

  void NotifyInterestGroupAuctionEventOccurred(
      base::Time event_time,
      content::InterestGroupAuctionEventType type,
      const std::string& unique_auction_id,
      base::optional_ref<const std::string> parent_auction_id,
      const base::Value::Dict& auction_config);

  void NotifyInterestGroupAuctionNetworkRequestCreated(
      content::InterestGroupAuctionFetchType type,
      const std::string& request_id,
      const std::vector<std::string>& devtools_auction_ids);

 private:
  // See definition for lifetime information.
  class CacheStorageObserver;
  class IndexedDBObserver;
  class InterestGroupObserver;
  class SharedStorageObserver;
  class QuotaManagerObserver;

  // Not thread safe.
  CacheStorageObserver* GetCacheStorageObserver();
  IndexedDBObserver* GetIndexedDBObserver();

  SharedStorageWorkletHostManager* GetSharedStorageWorkletHostManager();
  absl::variant<protocol::Response, storage::SharedStorageManager*>
  GetSharedStorageManager();
  storage::QuotaManagerProxy* GetQuotaManagerProxy();
  AttributionManager* GetAttributionManager();

  // content::InterestGroupManagerImpl::InterestGroupObserver
  void OnInterestGroupAccessed(
      base::optional_ref<const std::string> auction_id,
      base::Time access_time,
      InterestGroupManagerImpl::InterestGroupObserver::AccessType type,
      const url::Origin& owner_origin,
      const std::string& name,
      base::optional_ref<const url::Origin> component_seller_origin,
      std::optional<double> bid,
      base::optional_ref<const std::string> bid_currency) override;

  // AttributionObserver
  void OnSourceHandled(
      const StorableSource&,
      base::Time source_time,
      std::optional<uint64_t> cleared_debug_key,
      attribution_reporting::mojom::StoreSourceResult) override;
  void OnTriggerHandled(std::optional<uint64_t> cleared_debug_key,
                        const CreateReportResult&) override;

  void NotifySharedStorageAccessed(
      const base::Time& access_time,
      SharedStorageWorkletHostManager::SharedStorageObserverInterface::
          AccessType type,
      FrameTreeNodeId main_frame_id,
      const std::string& owner_origin,
      const SharedStorageEventParams& params);

  void NotifyCacheStorageListChanged(
      const storage::BucketLocator& bucket_locator);
  void NotifyCacheStorageContentChanged(
      const storage::BucketLocator& bucket_locator,
      const std::string& name);
  void NotifyIndexedDBListChanged(storage::BucketLocator bucket_locator);
  void NotifyIndexedDBContentChanged(storage::BucketLocator bucket_locator,
                                     const std::u16string& database_name,
                                     const std::u16string& object_store_name);
  void NotifyCreateOrUpdateBucket(const storage::BucketInfo& bucket_info);
  void NotifyDeleteBucket(const storage::BucketLocator& bucket_locator);

  Response FindStoragePartition(const Maybe<std::string>& browser_context_id,
                                StoragePartition** storage_partition);

  void ResetAttributionReporting();

  // This doesn't update `interest_group_auction_tracking_enabled_` and does not
  // have to work on `storage_partition_`, unlike the public version.
  Response SetInterestGroupTrackingInternal(StoragePartition* storage_partition,
                                            bool enable);
  void GotAllCookies(
      std::unique_ptr<Storage::Backend::GetCookiesCallback> callback,
      const std::vector<net::CanonicalCookie>& cookies);

  std::unique_ptr<Storage::Frontend> frontend_;
  raw_ptr<StoragePartition> storage_partition_{nullptr};
  raw_ptr<RenderFrameHostImpl> frame_host_ = nullptr;
  std::unique_ptr<CacheStorageObserver> cache_storage_observer_;
  std::unique_ptr<IndexedDBObserver> indexed_db_observer_;
  std::unique_ptr<SharedStorageObserver> shared_storage_observer_;
  std::unique_ptr<QuotaManagerObserver> quota_manager_observer_;

  // Exposes the API for managing storage quota overrides.
  std::unique_ptr<storage::QuotaOverrideHandle> quota_override_handle_;
  raw_ptr<DevToolsAgentHostClient> client_;

  bool interest_group_tracking_enabled_ = false;
  bool interest_group_auction_tracking_enabled_ = false;

  base::ScopedObservation<AttributionManager, AttributionObserver>
      attribution_observation_{this};

  base::WeakPtrFactory<StorageHandler> weak_ptr_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
