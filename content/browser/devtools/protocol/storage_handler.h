// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/storage.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"

namespace storage {
class QuotaOverrideHandle;
}

namespace content {
class StoragePartition;

namespace protocol {

class StorageHandler : public DevToolsDomainHandler,
                       public Storage::Backend,
                       private content::InterestGroupManagerImpl::
                           InterestGroupObserverInterface {
 public:
  explicit StorageHandler(bool client_is_trusted);

  StorageHandler(const StorageHandler&) = delete;
  StorageHandler& operator=(const StorageHandler&) = delete;

  ~StorageHandler() override;

  // content::protocol::DevToolsDomainHandler
  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  Response Disable() override;

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

 private:
  // See definition for lifetime information.
  class CacheStorageObserver;
  class IndexedDBObserver;
  class InterestGroupObserver;
  class SharedStorageObserver;

  // Not thread safe.
  CacheStorageObserver* GetCacheStorageObserver();
  IndexedDBObserver* GetIndexedDBObserver();

  SharedStorageWorkletHostManager* GetSharedStorageWorkletHostManager();
  absl::variant<protocol::Response, storage::SharedStorageManager*>
  GetSharedStorageManager();

  // content::InterestGroupManagerImpl::InterestGroupObserverInterface
  void OnInterestGroupAccessed(
      const base::Time& accessTime,
      InterestGroupManagerImpl::InterestGroupObserverInterface::AccessType type,
      const std::string& owner_origin,
      const std::string& name) override;

  void NotifySharedStorageAccessed(
      const base::Time& access_time,
      SharedStorageWorkletHostManager::SharedStorageObserverInterface::
          AccessType type,
      const std::string& main_frame_id,
      const std::string& owner_origin,
      const SharedStorageEventParams& params);

  void NotifyCacheStorageListChanged(const blink::StorageKey& storage_key);
  void NotifyCacheStorageContentChanged(const blink::StorageKey& storage_key,
                                        const std::string& name);
  void NotifyIndexedDBListChanged(const std::string& origin,
                                  const std::string& storage_key);
  void NotifyIndexedDBContentChanged(const std::string& origin,
                                     const std::string& storage_key,
                                     const std::u16string& database_name,
                                     const std::u16string& object_store_name);

  Response FindStoragePartition(const Maybe<std::string>& browser_context_id,
                                StoragePartition** storage_partition);

  std::unique_ptr<Storage::Frontend> frontend_;
  StoragePartition* storage_partition_{nullptr};
  RenderFrameHostImpl* frame_host_ = nullptr;
  std::unique_ptr<CacheStorageObserver> cache_storage_observer_;
  std::unique_ptr<IndexedDBObserver> indexed_db_observer_;
  std::unique_ptr<SharedStorageObserver> shared_storage_observer_;

  // Exposes the API for managing storage quota overrides.
  std::unique_ptr<storage::QuotaOverrideHandle> quota_override_handle_;
  bool client_is_trusted_;

  base::WeakPtrFactory<StorageHandler> weak_ptr_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
