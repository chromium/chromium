// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/memory/safety_checks.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "content/public/browser/zoom_level_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-forward.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-forward.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom-forward.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

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

namespace media {
class VideoDecodePerfHistory;
class WebrtcVideoPerfHistory;
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

namespace perfetto {
template <typename>
class TracedProto;

namespace protos::pbzero {
class ChromeBrowserContext;
}  // namespace protos::pbzero

}  // namespace perfetto

namespace content {

class BackgroundFetchDelegate;
class BackgroundSyncController;
class BlobHandle;
class BrowserContextImpl;
class BrowserPluginGuestManager;
class BrowsingDataRemover;
class BrowsingDataRemoverDelegate;
class ClientHintsControllerDelegate;
class ContentIndexProvider;
class DownloadManager;
class DownloadManagerDelegate;
class FederatedIdentityPermissionContextDelegate;
class FederatedIdentityApiPermissionContextDelegate;
class FederatedIdentityAutoReauthnPermissionContextDelegate;
class FileSystemAccessPermissionContext;
class OriginTrialsControllerDelegate;
class PermissionController;
class PermissionControllerDelegate;
class PlatformNotificationService;
class PushMessagingService;
class ReduceAcceptLanguageControllerDelegate;
class ResourceContext;
class SSLHostStateDelegate;
class SharedCorsOriginAccessList;
class SiteInstance;
class StorageNotificationService;
class StoragePartition;
class StoragePartitionConfig;

// This class holds the context needed for a browsing session.
// It lives on the UI thread. All these methods must only be called on the UI
// thread.
class CONTENT_EXPORT BrowserContext : public base::SupportsUserData {
  // Do not remove this macro!
  // The macro is maintained by the memory safety team.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  //////////////////////////////////////////////////////////////////////////////
  // The BrowserContext methods below are provided/implemented by the //content
  // layer (e.g. there is no need to override these methods in layers above
  // //content).
  //
  // The currently recommended practice is to make the methods in this section
  // non-virtual instance methods.
  //
  // TODO(crbug.com/40169693): Consider moving these methods to
  // BrowserContextImpl.

  BrowserContext();
  ~BrowserContext() override;

  DownloadManager* GetDownloadManager();

  // Returns BrowserContext specific external mount points. It may return
  // nullptr if the context doesn't have any BrowserContext specific external
  // mount points. Currently, non-nullptr value is returned only on ChromeOS.
  storage::ExternalMountPoints* GetMountPoints();

  // Returns a BrowsingDataRemover that can schedule data deletion tasks
  // for this |context|.
  BrowsingDataRemover* GetBrowsingDataRemover();

  // Returns the PermissionController associated with this context. There's
  // always a PermissionController instance for each BrowserContext.
  PermissionController* GetPermissionController();

  // Returns a StoragePartition for the given SiteInstance. By default this will
  // create a new StoragePartition if it doesn't exist, unless |can_create| is
  // false.
  StoragePartition* GetStoragePartition(SiteInstance* site_instance,
                                        bool can_create = true);

  // Returns a StoragePartition for the given StoragePartitionConfig. By
  // default this will create a new StoragePartition if it doesn't exist,
  // unless |can_create| is false.
  StoragePartition* GetStoragePartition(
      const StoragePartitionConfig& storage_partition_config,
      bool can_create = true);

  // Deprecated. Do not add new callers. Use the SiteInstance or
  // StoragePartitionConfig methods above instead.
  // Returns a StoragePartition for the given URL. By default this will
  // create a new StoragePartition if it doesn't exist, unless |can_create| is
  // false.
  StoragePartition* GetStoragePartitionForUrl(const GURL& url,
                                              bool can_create = true);

  // Synchronously invokes `fn` for each loaded StoragePartition.
  // Persisted StoragePartitions (not in-memory) are loaded lazily on first
  // use, at which point a StoragePartition object will be created that's
  // backed by the on-disk storage. StoragePartitions will not be unloaded for
  // the remainder of the BrowserContext's lifetime.
  void ForEachLoadedStoragePartition(
      base::FunctionRef<void(StoragePartition*)> fn);

  // Returns the number of loaded StoragePartitions that exist for `this`
  // BrowserContext.
  // See |ForEachLoadedStoragePartition| for details about loaded
  // StoragePartitions.
  size_t GetLoadedStoragePartitionCount();

  // Starts an asynchronous best-effort attempt to delete all on-disk storage
  // related to |partition_domain| and synchronously invokes |done_callback|
  // once all deletable on-disk storage is deleted. |on_gc_required| will be
  // invoked if |partition_domain| corresponds to any StoragePartitions that
  // are loaded and can't safely be deleted. In this case the caller should
  // attempt to delete the StoragePartition again at next browser launch.
  void AsyncObliterateStoragePartition(const std::string& partition_domain,
                                       base::OnceClosure on_gc_required,
                                       base::OnceClosure done_callback);

  // Examines all on-disk StoragePartitions and removes any entries that are
  // not loaded or listed in `active_paths`.
  //
  // The `done` closure is executed on the calling thread when garbage
  // collection is complete.
  void GarbageCollectStoragePartitions(
      std::unordered_set<base::FilePath> active_paths,
      base::OnceClosure done);

  StoragePartition* GetDefaultStoragePartition();

  using BlobCallback = base::OnceCallback<void(std::unique_ptr<BlobHandle>)>;
  using BlobContextGetter =
      base::RepeatingCallback<base::WeakPtr<storage::BlobStorageContext>()>;

  // This method should be called on UI thread and calls back on UI thread
  // as well. Note that retrieving a blob ptr out of BlobHandle can only be
  // done on IO. |callback| returns a nullptr on failure.
  void CreateMemoryBackedBlob(base::span<const uint8_t> data,
                              const std::string& content_type,
                              BlobCallback callback);

  // Get a BlobStorageContext getter that needs to run on IO thread.
  BlobContextGetter GetBlobStorageContext();

  // Returns a mojom::mojo::PendingRemote<blink::mojom::Blob> for a specific
  // blob. If no blob exists with the given UUID, the
  // mojo::PendingRemote<blink::mojom::Blob> pipe will close. This method should
  // be called on the UI thread.
  // TODO(mek): Blob UUIDs should be entirely internal to the blob system, so
  // eliminate this method in favor of just passing around the
  // mojo::PendingRemote<blink::mojom::Blob> directly.
  mojo::PendingRemote<blink::mojom::Blob> GetBlobRemote(
      const std::string& uuid);

  // Delivers a push message with |data| to the Service Worker identified by
  // |origin| and |service_worker_registration_id|.
  void DeliverPushMessage(
      const GURL& origin,
      int64_t service_worker_registration_id,
      const std::string& message_id,
      std::optional<std::string> payload,
      base::OnceCallback<void(blink::mojom::PushEventStatus)> callback);

  // Fires a push subscription change event to the Service Worker identified by
  // |origin| and |service_worker_registration_id| with |new_subscription| and
  // |old_subscription| as event information.
  void FirePushSubscriptionChangeEvent(
      const GURL& origin,
      int64_t service_worker_registration_id,
      blink::mojom::PushSubscriptionPtr new_subscription,
      blink::mojom::PushSubscriptionPtr old_subscription,
      base::OnceCallback<void(blink::mojom::PushEventStatus)> callback);

  void NotifyWillBeDestroyed();

  // Ensures that the corresponding ResourceContext is initialized. Normally the
  // BrowserContext initializs the corresponding getters when its objects are
  // created, but if the embedder wants to pass the ResourceContext to another
  // thread before they use BrowserContext, they should call this to make sure
  // that the ResourceContext is ready.
  void EnsureResourceContextInitialized();

  // Tells the HTML5 objects on this context to persist their session state
  // across the next restart.
  void SaveSessionState();

  void SetDownloadManagerForTesting(
      std::unique_ptr<DownloadManager> download_manager);

  void SetPermissionControllerForTesting(
      std::unique_ptr<PermissionController> permission_controller);

  // The list of CORS exemptions.  This list needs to be 1) replicated when
  // creating or re-creating new network::mojom::NetworkContexts (see
  // network::mojom::NetworkContextParams::cors_origin_access_list) and 2)
  // consulted by CORS-aware factories (e.g. passed when constructing
  // FileURLLoaderFactory).
  SharedCorsOriginAccessList* GetSharedCorsOriginAccessList();

  // Shuts down the storage partitions associated to this browser context.
  // This must be called before the browser context is actually destroyed
  // and before a clean-up task for its corresponding IO thread residents (e.g.
  // ResourceContext) is posted, so that the classes that hung on
  // StoragePartition can have time to do necessary cleanups on IO thread.
  void ShutdownStoragePartitions();

  // Returns true if shutdown has been initiated via a
  // NotifyWillBeDestroyed() call. This is a signal that the object will be
  // destroyed soon and no new references to this object should be created.
  bool ShutdownStarted();

  // Returns a unique string associated with this browser context.
  virtual const std::string& UniqueId();

  // Gets media service for storing/retrieving video decoding performance stats.
  // Exposed here rather than StoragePartition because all SiteInstances should
  // have similar decode performance and stats are not exposed to the web
  // directly, so privacy is not compromised.
  media::VideoDecodePerfHistory* GetVideoDecodePerfHistory();

  // Gets media service for storing/retrieving WebRTC video performance stats.
  // Exposed here rather than StoragePartition because all SiteInstances should
  // have similar encode/decode performance and stats are not exposed to the web
  // directly, so privacy is not compromised.
  media::WebrtcVideoPerfHistory* GetWebrtcVideoPerfHistory();

  // Returns a LearningSession associated with |this|. Used as the central
  // source from which to retrieve LearningTaskControllers for media machine
  // learning.
  // Exposed here rather than StoragePartition because learnings will cover
  // general media trends rather than SiteInstance specific behavior. The
  // learnings are not exposed to the web.
  virtual media::learning::LearningSession* GetLearningSession();

  // Retrieves the InProgressDownloadManager associated with this object if
  // available
  virtual std::unique_ptr<download::InProgressDownloadManager>
  RetrieveInProgressDownloadManager();

  using TraceProto = perfetto::protos::pbzero::ChromeBrowserContext;
  // Write a representation of this object into tracing proto.
  // rvalue ensure that the this method can be called without having access
  // to the declaration of ChromeBrowserContext proto.
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> context) const;

  // Deprecated. Do not add new callers.
  // TODO(crbug.com/40604019): Get rid of ResourceContext.
  ResourceContext* GetResourceContext() const;

  base::WeakPtr<BrowserContext> GetWeakPtr();

  //////////////////////////////////////////////////////////////////////////////
  // The //content embedder can override the methods below to change or extend
  // how the //content layer interacts with a BrowserContext.
  //
  // All the methods below should be virtual.  Most of the methods should be
  // pure (i.e. `= 0`) although it may make sense to provide a default
  // implementation for some of the methods.
  //
  // TODO(crbug.com/40169693): Migrate method declarations from this
  // section into a separate BrowserContextDelegate class.

  // Creates a delegate to initialize a HostZoomMap and persist its information.
  // This is called during creation of each StoragePartition.
  virtual std::unique_ptr<ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) = 0;

  // Returns the path of the directory where this context's data is stored.
  virtual base::FilePath GetPath() = 0;

  // Return whether this context is off the record. Default is false.
  // Note that for Chrome this does not imply Incognito as Guest sessions are
  // also off the record.
  virtual bool IsOffTheRecord() = 0;

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

  // Returns the platform notification service, capable of displaying Web
  // Notifications to the user. The embedder can return a nullptr if they don't
  // support this functionality. Must be called on the UI thread.
  virtual PlatformNotificationService* GetPlatformNotificationService() = 0;

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

  // Returns the ReduceAcceptLanguageControllerDelegate associated with that
  // context if any, nullptr otherwise.
  virtual ReduceAcceptLanguageControllerDelegate*
  GetReduceAcceptLanguageControllerDelegate() = 0;

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

  // Returns the FileSystemAccessPermissionContext associated with this context
  // if any, nullptr otherwise.
  virtual FileSystemAccessPermissionContext*
  GetFileSystemAccessPermissionContext();

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

  // Creates the media service for storing/retrieving video decoding performance
  // stats.  Exposed here rather than StoragePartition because all SiteInstances
  // should have similar decode performance and stats are not exposed to the web
  // directly, so privacy is not compromised.
  virtual std::unique_ptr<media::VideoDecodePerfHistory>
  CreateVideoDecodePerfHistory();

  // Gets the permission context for determining whether the FedCM API is
  // enabled in site settings.
  virtual FederatedIdentityApiPermissionContextDelegate*
  GetFederatedIdentityApiPermissionContext();
  // Gets the permission context for determining whether the FedCM API's auto
  // re-authentication feature is enabled in site settings.
  virtual FederatedIdentityAutoReauthnPermissionContextDelegate*
  GetFederatedIdentityAutoReauthnPermissionContext();
  // Gets the permission context for allowing session management capabilities
  // between an identity provider and a relying party if one exists, or
  // nullptr otherwise.
  virtual FederatedIdentityPermissionContextDelegate*
  GetFederatedIdentityPermissionContext();

  // Gets the KAnonymityServiceDelegate if supported. Returns nullptr if
  // unavailable.
  virtual KAnonymityServiceDelegate* GetKAnonymityServiceDelegate();

  // Returns the OriginTrialsControllerDelegate associated with the context if
  // any, nullptr otherwise.
  virtual OriginTrialsControllerDelegate* GetOriginTrialsControllerDelegate();

 private:
  // Please don't add more fields to BrowserContext.
  //
  // Ideally, BrowserContext would be a pure interface (only pure-virtual
  // methods and no fields), but currently BrowserContext and BrowserContextImpl
  // and BrowserContextDelegate are kind of mixed together in a single class.
  //
  // TODO(crbug.com/40169693): Make BrowserContextImpl to implement
  // BrowserContext instead (Removing afterwards the BrowserContextImpl,
  // fwd-declaration, `impl_` field, `friend` declaration and `impl` accessor
  // below).
  friend class BrowserContextImpl;
  std::unique_ptr<BrowserContextImpl> impl_;
  BrowserContextImpl* impl() { return impl_.get(); }
  const BrowserContextImpl* impl() const { return impl_.get(); }
  base::WeakPtrFactory<BrowserContext> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_CONTEXT_H_
