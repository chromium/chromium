// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_H_
#define CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_H_

#include <stdint.h>

#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom-forward.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-forward.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"

class GURL;

namespace base {
class Time;
}

namespace storage {
class FileSystemContext;
}

namespace leveldb_proto {
class ProtoDatabaseProvider;
}

namespace network {
namespace mojom {
class CookieManager;
class NetworkContext;
}
}  // namespace network

namespace storage {
class QuotaManager;
class SpecialStoragePolicy;
struct QuotaSettings;
}

namespace storage {
class DatabaseTracker;
}

namespace content {

class AppCacheService;
class BackgroundSyncContext;
class BrowserContext;
class CacheStorageContext;
class ContentIndexContext;
class DedicatedWorkerService;
class DevToolsBackgroundServicesContext;
class DOMStorageContext;
class GeneratedCodeCacheContext;
class NativeFileSystemEntryFactory;
class PlatformNotificationContext;
class ServiceWorkerContext;
class SharedWorkerService;

#if !defined(OS_ANDROID)
class HostZoomLevelContext;
class HostZoomMap;
class ZoomLevelDelegate;
#endif  // !defined(OS_ANDROID)

// Defines what persistent state a child process can access.
//
// The StoragePartition defines the view each child process has of the
// persistent state inside the BrowserContext. This is used to implement
// isolated storage where a renderer with isolated storage cannot see
// the cookies, localStorage, etc., that normal web renderers have access to.
class CONTENT_EXPORT StoragePartition {
 public:
  virtual base::FilePath GetPath() = 0;

  // Returns a raw mojom::NetworkContext pointer. When network service crashes
  // or restarts, the raw pointer will not be valid or safe to use. Therefore,
  // caller should not hold onto this pointer beyond the same message loop task.
  virtual network::mojom::NetworkContext* GetNetworkContext() = 0;

  // Returns a pointer/info to a URLLoaderFactory/CookieManager owned by
  // the storage partition. Prefer to use this instead of creating a new
  // URLLoaderFactory when issuing requests from the Browser process, to
  // share resources and preserve ordering.
  // The returned info from |GetURLLoaderFactoryForBrowserProcessIOThread()|
  // must be consumed on the IO thread to get the actual factory, and is safe to
  // use after StoragePartition has gone.
  // The returned SharedURLLoaderFactory can be held on and will work across
  // network process restarts.
  //
  // SECURITY NOTE: This browser-process factory relaxes many security features
  // (e.g. may disable CORB, won't set |request_initiator_origin_lock| or
  // IsolationInfo, etc.).  Network requests that may be initiated or influenced
  // by a web origin should typically use a different factory (e.g.  the one
  // from RenderFrameHost::CreateNetworkServiceDefaultFactory).
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcess() = 0;
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcessWithCORBEnabled() = 0;
  virtual std::unique_ptr<network::PendingSharedURLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcessIOThread() = 0;
  virtual network::mojom::CookieManager*
  GetCookieManagerForBrowserProcess() = 0;

  virtual void CreateHasTrustTokensAnswerer(
      mojo::PendingReceiver<network::mojom::HasTrustTokensAnswerer> receiver,
      const url::Origin& top_frame_origin) = 0;

  virtual storage::QuotaManager* GetQuotaManager() = 0;
  virtual AppCacheService* GetAppCacheService() = 0;
  virtual BackgroundSyncContext* GetBackgroundSyncContext() = 0;
  virtual storage::FileSystemContext* GetFileSystemContext() = 0;
  virtual storage::DatabaseTracker* GetDatabaseTracker() = 0;
  virtual DOMStorageContext* GetDOMStorageContext() = 0;
  virtual storage::mojom::IndexedDBControl& GetIndexedDBControl() = 0;
  virtual NativeFileSystemEntryFactory* GetNativeFileSystemEntryFactory() = 0;
  virtual ServiceWorkerContext* GetServiceWorkerContext() = 0;
  virtual DedicatedWorkerService* GetDedicatedWorkerService() = 0;
  virtual SharedWorkerService* GetSharedWorkerService() = 0;
  virtual CacheStorageContext* GetCacheStorageContext() = 0;
  virtual GeneratedCodeCacheContext* GetGeneratedCodeCacheContext() = 0;
  virtual DevToolsBackgroundServicesContext*
  GetDevToolsBackgroundServicesContext() = 0;
  virtual ContentIndexContext* GetContentIndexContext() = 0;
#if !defined(OS_ANDROID)
  virtual HostZoomMap* GetHostZoomMap() = 0;
  virtual HostZoomLevelContext* GetHostZoomLevelContext() = 0;
  virtual ZoomLevelDelegate* GetZoomLevelDelegate() = 0;
#endif  // !defined(OS_ANDROID)
  virtual PlatformNotificationContext* GetPlatformNotificationContext() = 0;

  virtual leveldb_proto::ProtoDatabaseProvider* GetProtoDatabaseProvider() = 0;
  // Must be set before the first call to GetProtoDatabaseProvider(), or a new
  // one will be created by get.
  virtual void SetProtoDatabaseProvider(
      std::unique_ptr<leveldb_proto::ProtoDatabaseProvider>
          optional_proto_db_provider) = 0;

  enum : uint32_t {
    REMOVE_DATA_MASK_APPCACHE = 1 << 0,
    REMOVE_DATA_MASK_COOKIES = 1 << 1,
    REMOVE_DATA_MASK_FILE_SYSTEMS = 1 << 2,
    REMOVE_DATA_MASK_INDEXEDDB = 1 << 3,
    REMOVE_DATA_MASK_LOCAL_STORAGE = 1 << 4,
    REMOVE_DATA_MASK_SHADER_CACHE = 1 << 5,
    REMOVE_DATA_MASK_WEBSQL = 1 << 6,
    REMOVE_DATA_MASK_SERVICE_WORKERS = 1 << 7,
    REMOVE_DATA_MASK_CACHE_STORAGE = 1 << 8,
    REMOVE_DATA_MASK_PLUGIN_PRIVATE_DATA = 1 << 9,
    REMOVE_DATA_MASK_BACKGROUND_FETCH = 1 << 10,
    REMOVE_DATA_MASK_CONVERSIONS = 1 << 11,
    REMOVE_DATA_MASK_ALL = 0xFFFFFFFF,

    // Corresponds to storage::kStorageTypeTemporary.
    QUOTA_MANAGED_STORAGE_MASK_TEMPORARY = 1 << 0,
    // Corresponds to storage::kStorageTypePersistent.
    QUOTA_MANAGED_STORAGE_MASK_PERSISTENT = 1 << 1,
    // Corresponds to storage::kStorageTypeSyncable.
    QUOTA_MANAGED_STORAGE_MASK_SYNCABLE = 1 << 2,
    QUOTA_MANAGED_STORAGE_MASK_ALL = 0xFFFFFFFF,
  };

  // Starts an asynchronous task that does a best-effort clear the data
  // corresponding to the given |remove_mask| and |quota_storage_remove_mask|
  // inside this StoragePartition for the given |storage_origin|.
  // Note session dom storage is not cleared even if you specify
  // REMOVE_DATA_MASK_LOCAL_STORAGE.
  // No notification is dispatched upon completion.
  //
  // TODO(ajwong): Right now, the embedder may have some
  // URLRequestContextGetter objects that the StoragePartition does not know
  // about.  This will no longer be the case when we resolve
  // http://crbug.com/159193. Remove |request_context_getter| when that bug
  // is fixed.
  virtual void ClearDataForOrigin(uint32_t remove_mask,
                                  uint32_t quota_storage_remove_mask,
                                  const GURL& storage_origin) = 0;

  // A callback type to check if a given origin matches a storage policy.
  // Can be passed empty/null where used, which means the origin will always
  // match.
  using OriginMatcherFunction =
      base::RepeatingCallback<bool(const url::Origin&,
                                   storage::SpecialStoragePolicy*)>;

  // Observer interface that is notified of specific data clearing events which
  // which were facilitated by the StoragePartition. Notification occurs on the
  // UI thread, observer life time is not managed by the StoragePartition.
  class DataRemovalObserver : public base::CheckedObserver {
   public:
    // Called on a deletion event for origin keyed storage APIs.
    virtual void OnOriginDataCleared(
        uint32_t remove_mask,
        base::RepeatingCallback<bool(const url::Origin&)> origin_matcher,
        const base::Time begin,
        const base::Time end) = 0;
  };

  // Similar to ClearDataForOrigin().
  // Deletes all data out for the StoragePartition if |storage_origin| is empty.
  // |callback| is called when data deletion is done or at least the deletion is
  // scheduled.
  virtual void ClearData(uint32_t remove_mask,
                         uint32_t quota_storage_remove_mask,
                         const GURL& storage_origin,
                         const base::Time begin,
                         const base::Time end,
                         base::OnceClosure callback) = 0;

  // Similar to ClearData().
  // Deletes all data out for the StoragePartition.
  // * |origin_matcher| is present if special storage policy is to be handled,
  //   otherwise the callback should be null (base::Callback::is_null()==true).
  //   The origin matcher does not apply to cookies, instead use:
  // * |cookie_deletion_filter| identifies the cookies to delete and will be
  //   used if |remove_mask| has the REMOVE_DATA_MASK_COOKIES bit set. Note:
  //   CookieDeletionFilterPtr also contains a time interval
  //   (created_after_time/created_before_time), so when deleting cookies
  //   |begin| and |end| will be used ignoring the interval in
  //   |cookie_deletion_filter|.
  //   If |perform_storage_cleanup| is true, the storage will try to remove
  //   traces about deleted data from disk. This is an expensive operation that
  //   should only be performed if we are sure that almost all data will be
  //   deleted anyway.
  // * |callback| is called when data deletion is done or at least the deletion
  //   is scheduled.
  // Note: Make sure you know what you are doing before clearing cookies
  // selectively. You don't want to break the web.
  virtual void ClearData(
      uint32_t remove_mask,
      uint32_t quota_storage_remove_mask,
      OriginMatcherFunction origin_matcher,
      network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
      bool perform_storage_cleanup,
      const base::Time begin,
      const base::Time end,
      base::OnceClosure callback) = 0;

  // Clears code caches associated with this StoragePartition.
  // If |begin| and |end| are not null, only entries with
  // timestamps inbetween are deleted. If |url_matcher| is not null, only
  // entries with URLs for which the |url_matcher| returns true are deleted.
  virtual void ClearCodeCaches(
      base::Time begin,
      base::Time end,
      const base::RepeatingCallback<bool(const GURL&)>& url_matcher,
      base::OnceClosure callback) = 0;

  // Write any unwritten data to disk.
  // Note: this method does not sync the data - it only ensures that any
  // unwritten data has been written out to the filesystem.
  virtual void Flush() = 0;

  // Resets all URLLoaderFactories bound to this partition's network context.
  virtual void ResetURLLoaderFactories() = 0;

  virtual void AddObserver(DataRemovalObserver* observer) = 0;

  virtual void RemoveObserver(DataRemovalObserver* observer) = 0;

  // Clear the bluetooth allowed devices map. For test use only.
  virtual void ClearBluetoothAllowedDevicesMapForTesting() = 0;

  // Call |FlushForTesting()| on Network Service related interfaces. For test
  // use only.
  virtual void FlushNetworkInterfaceForTesting() = 0;

  // Wait until all deletions tasks are finished. For test use only.
  virtual void WaitForDeletionTasksForTesting() = 0;

  // Wait until code cache's shutdown is complete. For test use only.
  virtual void WaitForCodeCacheShutdownForTesting() = 0;

  virtual void SetNetworkContextForTesting(
      mojo::PendingRemote<network::mojom::NetworkContext>
          network_context_remote) = 0;

  // Returns the same provider as GetProtoDatabaseProvider() but doesn't create
  // a new instance and returns nullptr instead.
  virtual leveldb_proto::ProtoDatabaseProvider*
  GetProtoDatabaseProviderForTesting() = 0;

  // The value pointed to by |settings| should remain valid until the
  // the function is called again with a new value or a nullptr.
  static void SetDefaultQuotaSettingsForTesting(
      const storage::QuotaSettings* settings);

  static bool IsAppCacheEnabled();

 protected:
  virtual ~StoragePartition() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_H_
