// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom-forward.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-forward.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-forward.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom-forward.h"

#if !defined(OS_ANDROID)
#include "content/public/browser/zoom_level_delegate.h"
#endif

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace download {
class InProgressDownloadManager;
}

namespace storage {
class ExternalMountPoints;
}

namespace url {
class Origin;
}

namespace media {
class VideoDecodePerfHistory;
namespace learning {
class LearningSession;
}
}  // namespace media

namespace storage {
class BlobStorageContext;
class SpecialStoragePolicy;
}  // namespace storage

namespace variations {
class VariationsClient;
}  // namespace variations

namespace content {

class BackgroundFetchDelegate;
class BackgroundSyncController;
class BlobHandle;
class BrowserPluginGuestManager;
class BrowsingDataRemover;
class BrowsingDataRemoverDelegate;
class DownloadManager;
class ClientHintsControllerDelegate;
class ContentIndexProvider;
class DownloadManagerDelegate;
class NativeFileSystemPermissionContext;
class PermissionController;
class PermissionControllerDelegate;
class PushMessagingService;
class ResourceContext;
class SharedCorsOriginAccessList;
class SiteInstance;
class StorageNotificationService;
class StoragePartition;
class StoragePartitionConfig;
class SSLHostStateDelegate;

// This class holds the context needed for a browsing session.
// It lives on the UI thread. All these methods must only be called on the UI
// thread.
class CONTENT_EXPORT BrowserContext : public base::SupportsUserData {
 public:
  static DownloadManager* GetDownloadManager(BrowserContext* browser_context);

  // Returns BrowserContext specific external mount points. It may return
  // nullptr if the context doesn't have any BrowserContext specific external
  // mount points. Currenty, non-nullptr value is returned only on ChromeOS.
  static storage::ExternalMountPoints* GetMountPoints(BrowserContext* context);

  // Returns a BrowsingDataRemover that can schedule data deletion tasks
  // for this |context|.
  static BrowsingDataRemover* GetBrowsingDataRemover(BrowserContext* context);

  // Returns the PermissionController associated with this context. There's
  // always a PermissionController instance for each BrowserContext.
  static PermissionController* GetPermissionController(BrowserContext* context);

  // Returns a StoragePartition for the given SiteInstance. By default this will
  // create a new StoragePartition if it doesn't exist, unless |can_create| is
  // false.
  static StoragePartition* GetStoragePartition(BrowserContext* browser_context,
                                               SiteInstance* site_instance,
                                               bool can_create = true);

  // Returns a StoragePartition for the given StoragePartitionConfig. By
  // default this will create a new StoragePartition if it doesn't exist,
  // unless |can_create| is false.
  static StoragePartition* GetStoragePartition(
      BrowserContext* browser_context,
      const StoragePartitionConfig& storage_partition_config,
      bool can_create = true);

  // Deprecated. Do not add new callers. Use the SiteInstance or
  // StoragePartitionConfig methods above instead.
  // Returns a StoragePartition for the given site URL. By default this will
  // create a new StoragePartition if it doesn't exist, unless |can_create| is
  // false.
  static StoragePartition* GetStoragePartitionForSite(
      BrowserContext* browser_context,
      const GURL& site,
      bool can_create = true);

  using StoragePartitionCallback =
      base::RepeatingCallback<void(StoragePartition*)>;
  static void ForEachStoragePartition(BrowserContext* browser_context,
                                      StoragePartitionCallback callback);
  // Returns the number of StoragePartitions that exist for the given
  // |browser_context|.
  static size_t GetStoragePartitionCount(BrowserContext* browser_context);
  static void AsyncObliterateStoragePartition(
      BrowserContext* browser_context,
      const std::string& partition_domain,
      base::OnceClosure on_gc_required);

  // This function clears the contents of |active_paths| but does not take
  // ownership of the pointer.
  static void GarbageCollectStoragePartitions(
      BrowserContext* browser_context,
      std::unique_ptr<std::unordered_set<base::FilePath>> active_paths,
      base::OnceClosure done);

  static StoragePartition* GetDefaultStoragePartition(
      BrowserContext* browser_context);

  using BlobCallback = base::OnceCallback<void(std::unique_ptr<BlobHandle>)>;
  using BlobContextGetter =
      base::RepeatingCallback<base::WeakPtr<storage::BlobStorageContext>()>;

  // This method should be called on UI thread and calls back on UI thread
  // as well. Note that retrieving a blob ptr out of BlobHandle can only be
  // done on IO. |callback| returns a nullptr on failure.
  static void CreateMemoryBackedBlob(BrowserContext* browser_context,
                                     base::span<const uint8_t> data,
                                     const std::string& content_type,
                                     BlobCallback callback);

  // Get a BlobStorageContext getter that needs to run on IO thread.
  static BlobContextGetter GetBlobStorageContext(
      BrowserContext* browser_context);

  // Returns a mojom::mojo::PendingRemote<blink::mojom::Blob> for a specific
  // blob. If no blob exists with the given UUID, the
  // mojo::PendingRemote<blink::mojom::Blob> pipe will close. This method should
  // be called on the UI thread.
  // TODO(mek): Blob UUIDs should be entirely internal to the blob system, so
  // eliminate this method in favor of just passing around the
  // mojo::PendingRemote<blink::mojom::Blob> directly.
  static mojo::PendingRemote<blink::mojom::Blob> GetBlobRemote(
      BrowserContext* browser_context,
      const std::string& uuid);

  // Delivers a push message with |data| to the Service Worker identified by
  // |origin| and |service_worker_registration_id|.
  static void DeliverPushMessage(
      BrowserContext* browser_context,
      const GURL& origin,
      int64_t service_worker_registration_id,
      const std::string& message_id,
      base::Optional<std::string> payload,
      base::OnceCallback<void(blink::mojom::PushEventStatus)> callback);

  // Fires a push subscription change event to the Service Worker identified by
  // |origin| and |service_worker_registration_id| with |new_subscription| and
  // |old_subscription| as event information.
  static void FirePushSubscriptionChangeEvent(
      BrowserContext* browser_context,
      const GURL& origin,
      int64_t service_worker_registration_id,
      blink::mojom::PushSubscriptionPtr new_subscription,
      blink::mojom::PushSubscriptionPtr old_subscription,
      base::OnceCallback<void(blink::mojom::PushEventStatus)> callback);

  static void NotifyWillBeDestroyed(BrowserContext* browser_context);

  // Ensures that the corresponding ResourceContext is initialized. Normally the
  // BrowserContext initializs the corresponding getters when its objects are
  // created, but if the embedder wants to pass the ResourceContext to another
  // thread before they use BrowserContext, they should call this to make sure
  // that the ResourceContext is ready.
  static void EnsureResourceContextInitialized(BrowserContext* browser_context);

  // Tells the HTML5 objects on this context to persist their session state
  // across the next restart.
  static void SaveSessionState(BrowserContext* browser_context);

  static void SetDownloadManagerForTesting(
      BrowserContext* browser_context,
      std::unique_ptr<content::DownloadManager> download_manager);

  static void SetPermissionControllerForTesting(
      BrowserContext* browser_context,
      std::unique_ptr<PermissionController> permission_controller);

  BrowserContext();

  ~BrowserContext() override;

  // Shuts down the storage partitions associated to this browser context.
  // This must be called before the browser context is actually destroyed
  // and before a clean-up task for its corresponding IO thread residents (e.g.
  // ResourceContext) is posted, so that the classes that hung on
  // StoragePartition can have time to do necessary cleanups on IO thread.
  void ShutdownStoragePartitions();

  // Returns true if shutdown has been initiated via a
  // NotifyWillBeDestroyed() call. This is a signal that the object will be
  // destroyed soon and no new references to this object should be created.
  bool ShutdownStarted() { return was_notify_will_be_destroyed_called_; }

#if !defined(OS_ANDROID)
  // Creates a delegate to initialize a HostZoomMap and persist its information.
  // This is called during creation of each StoragePartition.
  virtual std::unique_ptr<ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) = 0;
#endif

  // Returns the path of the directory where this context's data is stored.
  virtual base::FilePath GetPath() = 0;

  // Return whether this context is off the record. Default is false.
  // Note that for Chrome this does not imply Incognito as Guest sessions are
  // also off the record.
  virtual bool IsOffTheRecord() = 0;

  // Returns the resource context.
  virtual ResourceContext* GetResourceContext() = 0;

  // Returns the DownloadManagerDelegate for this context. This will be called
  // once per context. The embedder owns the delegate and is responsible for
  // ensuring that it outlives DownloadManager. Note in particular that it is
  // unsafe to destroy the delegate in the destructor of a subclass of
  // BrowserContext, since it needs to be alive in ~BrowserContext.
  // It's valid to return nullptr.
  virtual DownloadManagerDelegate* GetDownloadManagerDelegate() = 0;

  // Returns the guest manager for this context.
  virtual BrowserPluginGuestManager* GetGuestManager() = 0;

  // Returns a special storage policy implementation, or nullptr.
  virtual storage::SpecialStoragePolicy* GetSpecialStoragePolicy() = 0;

  // Returns a push messaging service. The embedder owns the service, and is
  // responsible for ensuring that it outlives RenderProcessHost. It's valid to
  // return nullptr.
  virtual PushMessagingService* GetPushMessagingService() = 0;

  // Returns a storage notification service associated with that context,
  // nullptr otherwise. In the case that nullptr is returned, QuotaManager
  // and the rest of the storage layer will have no connection to the Chrome
  // layer for UI purposes.
  virtual StorageNotificationService* GetStorageNotificationService() = 0;

  // Returns the SSL host state decisions for this context. The context may
  // return nullptr, implementing the default exception storage strategy.
  virtual SSLHostStateDelegate* GetSSLHostStateDelegate() = 0;

  // Returns the PermissionControllerDelegate associated with this context if
  // any, nullptr otherwise.
  //
  // Note: if you want to check a permission status, you probably need
  // BrowserContext::GetPermissionController() instead.
  virtual PermissionControllerDelegate* GetPermissionControllerDelegate() = 0;

  // Returns the ClientHintsControllerDelegate associated with that context if
  // any, nullptr otherwise.
  virtual ClientHintsControllerDelegate* GetClientHintsControllerDelegate() = 0;

  // Returns the BackgroundFetchDelegate associated with that context if any,
  // nullptr otherwise.
  virtual BackgroundFetchDelegate* GetBackgroundFetchDelegate() = 0;

  // Returns the BackgroundSyncController associated with that context if any,
  // nullptr otherwise.
  virtual BackgroundSyncController* GetBackgroundSyncController() = 0;

  // Returns the BrowsingDataRemoverDelegate for this context. This will be
  // called once per context. It's valid to return nullptr.
  virtual BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate() = 0;

  // Sets CORS origin access lists.
  virtual void SetCorsOriginAccessListForOrigin(
      const url::Origin& source_origin,
      std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
      base::OnceClosure closure);

  // Returns a SharedCorsOriginAccessList instance.
  virtual SharedCorsOriginAccessList* GetSharedCorsOriginAccessList();

  // Binds a NavigableContentsFactory interface receiver to this browser
  // context.
  virtual void BindNavigableContentsFactory(
      mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver);

  // Returns a unique string associated with this browser context.
  virtual const std::string& UniqueId();

  // Returns a random salt string that is used for creating media device IDs.
  // Returns a random string by default.
  virtual std::string GetMediaDeviceIDSalt();

  // Utility function useful for embedders. Only needs to be called if
  // 1) The embedder needs to use a new salt, and
  // 2) The embedder saves its salt across restarts.
  static std::string CreateRandomMediaDeviceIDSalt();

  // Media service for storing/retrieving video decoding performance stats.
  // Exposed here rather than StoragePartition because all SiteInstances should
  // have similar decode performance and stats are not exposed to the web
  // directly, so privacy is not compromised.
  virtual media::VideoDecodePerfHistory* GetVideoDecodePerfHistory();

  // Returns a LearningSession associated with |this|. Used as the central
  // source from which to retrieve LearningTaskControllers for media machine
  // learning.
  // Exposed here rather than StoragePartition because learnings will cover
  // general media trends rather than SiteInstance specific behavior. The
  // learnings are not exposed to the web.
  virtual media::learning::LearningSession* GetLearningSession();

  // Retrieves the InProgressDownloadManager associated with this object if
  // available
  virtual download::InProgressDownloadManager*
  RetriveInProgressDownloadManager();

  // Returns the NativeFileSystemPermissionContext associated with this context
  // if any, nullptr otherwise.
  virtual NativeFileSystemPermissionContext*
  GetNativeFileSystemPermissionContext();

  // Returns the ContentIndexProvider associated with that context if any,
  // nullptr otherwise.
  virtual ContentIndexProvider* GetContentIndexProvider();

  // Returns true iff the sandboxed file system implementation should be disk
  // backed, even if this browser context is off the record. By default this
  // returns false, an embedded could override this to return true if for
  // example the off-the-record browser context is stored in a in-memory file
  // system anyway, in which case using the disk backed sandboxed file system
  // API implementation can give some benefits over the in-memory
  // implementation.
  virtual bool CanUseDiskWhenOffTheRecord();

  // Returns the VariationsClient associated with the context if any, or
  // nullptr if there isn't one.
  virtual variations::VariationsClient* GetVariationsClient();

 private:
  const std::string unique_id_;
  bool was_notify_will_be_destroyed_called_ = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_
