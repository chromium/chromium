// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_

#include <memory>
#include <string>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/optional_ref.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/storage.h"
#include "content/public/browser/global_routing_id.h"
#include "storage/browser/quota/quota_manager.h"

namespace net {
class CanonicalCookie;
}

namespace storage {
class QuotaOverrideHandle;
}

namespace content {
class DevToolsAgentHostClient;
class DevToolsAgentHostImpl;
class RenderFrameHostImpl;
class StoragePartition;

namespace protocol {

class StorageHandler : public DevToolsDomainHandler, public Storage::Backend {
 public:
  explicit StorageHandler(DevToolsAgentHostImpl* host,
                          DevToolsAgentHostClient* client);

  StorageHandler(const StorageHandler&) = delete;
  StorageHandler& operator=(const StorageHandler&) = delete;

  ~StorageHandler() override;

  static std::vector<StorageHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  // content::protocol::DevToolsDomainHandler
  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  Response Disable() override;

  // content::protocol::storage::Backend
  Response GetStorageKeyForFrame(const std::string& frame_id,
                                 std::string* serialized_storage_key) override;
  Response GetStorageKey(std::optional<std::string> frame_id,
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
      std::optional<double> quota_size,
      std::unique_ptr<OverrideQuotaForOriginCallback> callback) override;

  // Cookies management
  void GetCookies(
      std::optional<std::string> browser_context_id,
      std::unique_ptr<Storage::Backend::GetCookiesCallback> callback) override;

  void SetCookies(
      std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
      std::optional<std::string> browser_context_id,
      std::unique_ptr<Storage::Backend::SetCookiesCallback> callback) override;

  void ClearCookies(std::optional<std::string> browser_context_id,
                    std::unique_ptr<Storage::Backend::ClearCookiesCallback>
                        callback) override;

  bool CanAccessCookie(const net::CanonicalCookie& cookie) const;

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
      std::optional<bool> ignore_if_present,
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

 private:
  // See definition for lifetime information.
  class CacheStorageObserver;
  class IndexedDBObserver;
  class QuotaManagerObserver;

  // Not thread safe.
  CacheStorageObserver* GetCacheStorageObserver();
  IndexedDBObserver* GetIndexedDBObserver();

  storage::QuotaManagerProxy* GetQuotaManagerProxy();

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

  Response FindStoragePartition(
      const std::optional<std::string>& browser_context_id,
      StoragePartition** storage_partition);

  Response GetStorageKeyForFrameInternal(const std::string& frame_id,
                                         std::string* serialized_storage_key);

  void GotAllCookies(
      std::unique_ptr<Storage::Backend::GetCookiesCallback> callback,
      const std::vector<net::CanonicalCookie>& cookies);

  const raw_ptr<DevToolsAgentHostImpl> host_;

  std::unique_ptr<Storage::Frontend> frontend_;
  raw_ptr<StoragePartition> storage_partition_{nullptr};
  raw_ptr<RenderFrameHostImpl> frame_host_ = nullptr;
  std::unique_ptr<CacheStorageObserver> cache_storage_observer_;
  std::unique_ptr<IndexedDBObserver> indexed_db_observer_;
  std::unique_ptr<QuotaManagerObserver> quota_manager_observer_;

  // Exposes the API for managing storage quota overrides.
  std::unique_ptr<storage::QuotaOverrideHandle> quota_override_handle_;
  raw_ptr<DevToolsAgentHostClient> client_;

  base::WeakPtrFactory<StorageHandler> weak_ptr_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
