// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_BROWSER_CONTEXT_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/browser/btm/btm_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom.h"

namespace media {
class VideoDecodePerfHistory;
class WebrtcVideoPerfHistory;
}  // namespace media

namespace storage {
class ExternalMountPoints;
}  // namespace storage

namespace perfetto::protos::pbzero {
class ChromeBrowserContext;
}  // namespace perfetto::protos::pbzero

namespace content {

class BackgroundSyncScheduler;
class BrowserContextImpl;
class BrowsingDataRemoverImpl;
class DownloadManager;
class InMemoryFederatedPermissionContext;
class NavigationStateKeepAlive;
class PermissionController;
class PrefetchService;
class StoragePartitionImplMap;

#if BUILDFLAG(IS_ANDROID)
class NavigationEntryScreenshotManager;
#endif  // BUILDFLAG(IS_ANDROID)

// content-internal parts of BrowserContext.
//
// TODO(crbug.com/40169693): Make BrowserContextImpl to implement
// BrowserContext, instead of being a member.
class CONTENT_EXPORT BrowserContextImpl {
 public:
  static BrowserContextImpl* From(BrowserContext* self);
  ~BrowserContextImpl();

  BrowserContextImpl(const BrowserContextImpl&) = delete;
  BrowserContextImpl& operator=(const BrowserContextImpl&) = delete;

  const std::string& UniqueId() const { return unique_id_; }
  const base::UnguessableToken& UniqueToken() const { return unique_token_; }

  bool ShutdownStarted();
  void NotifyWillBeDestroyed();

  StoragePartitionImplMap* GetOrCreateStoragePartitionMap();
  StoragePartitionImplMap* storage_partition_map() {
    return storage_partition_map_.get();
  }

  SharedCorsOriginAccessList* shared_cors_origin_access_list() {
    return shared_cors_origin_access_list_.get();
  }

  BrowsingDataRemoverImpl* GetBrowsingDataRemover();

  storage::ExternalMountPoints* GetMountPoints();

  DownloadManager* GetDownloadManager();
  void SetDownloadManagerForTesting(
      std::unique_ptr<DownloadManager> download_manager);
  PermissionController* GetPermissionController();
  void SetPermissionControllerForTesting(
      std::unique_ptr<PermissionController> permission_controller);

  void ShutdownStoragePartitions();

  media::VideoDecodePerfHistory* GetVideoDecodePerfHistory();

  // Gets media service for storing/retrieving WebRTC encoding and decoding
  // performance stats. Exposed here rather than StoragePartition because all
  // SiteInstances should have similar performance and stats are not exposed to
  // the web directly, so privacy is not compromised.
  media::WebrtcVideoPerfHistory* GetWebrtcVideoPerfHistory();

  BackgroundSyncScheduler* background_sync_scheduler() {
    return background_sync_scheduler_.get();
  }

  PrefetchService* GetPrefetchService();
  void SetPrefetchServiceForTesting(
      std::unique_ptr<PrefetchService> prefetch_service);

#if BUILDFLAG(IS_ANDROID)
  NavigationEntryScreenshotManager* GetNavigationEntryScreenshotManager();
#endif  // BUILDFLAG(IS_ANDROID)

  InMemoryFederatedPermissionContext* GetFederatedPermissionContext();
  void ResetFederatedPermissionContext();

  using TraceProto = perfetto::protos::pbzero::ChromeBrowserContext;
  // Write a representation of this object into a trace.
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> context) const;

  BtmServiceImpl* GetBtmService();
  // If the BTM database file should be deleted, wait for it. Otherwise, return
  // immediately.
  //
  // TODO: crbug.com/356624038 - delete this method when the BTM feature flag is
  // removed.
  void WaitForBtmCleanupForTesting();

  // Store `receiver` and its corresponding `handle`. These will be kept alive
  // as long as the remote endpoint of `receiver` is still alive on the renderer
  // side. The receiver will be automatically deleted when the endpoint is
  // disconnected.
  void RegisterKeepAliveHandle(
      mojo::PendingReceiver<blink::mojom::NavigationStateKeepAliveHandle>
          receiver,
      std::unique_ptr<NavigationStateKeepAlive> handle);

  // Get the NavigationStateKeepAlive associated with `frame_token`. See
  // `navigation_state_keep_alive_map_`.
  NavigationStateKeepAlive* GetNavigationStateKeepAlive(
      blink::LocalFrameToken frame_token);

  // Removes the NavigationStateKeepAlive associated with `frame_token`. This
  // should be called when the keep alive is destructed.
  void RemoveKeepAliveHandleFromMap(blink::LocalFrameToken frame_token,
                                    NavigationStateKeepAlive* keep_alive);

 private:
  // Creates the media service for storing/retrieving WebRTC encoding and
  // decoding performance stats.  Exposed here rather than StoragePartition
  // because all SiteInstances should have similar performance and stats are not
  // exposed to the web directly, so privacy is not compromised.
  std::unique_ptr<media::WebrtcVideoPerfHistory> CreateWebrtcVideoPerfHistory();

  // Delete any existing BTM database file if BTM is disabled (because it's not
  // possible for the user to clear it through the browser UI).
  //
  // TODO: crbug.com/356624038 - delete this method when the BTM feature flag is
  // removed.
  void MaybeCleanupBtm();

  // BrowserContextImpl is owned and build from BrowserContext constructor.
  // TODO(crbug.com/40169693): Invert the dependency. Make BrowserContext
  // a pure interface and BrowserContextImpl implements it. Remove the `self_`
  // field and 'friend' declaration.
  friend BrowserContext;
  explicit BrowserContextImpl(BrowserContext* self);
  raw_ptr<BrowserContext> self_;

  const base::UnguessableToken unique_token_ = base::UnguessableToken::Create();
  const std::string unique_id_ = unique_token_.ToString();
  bool will_be_destroyed_soon_ = false;

  std::unique_ptr<StoragePartitionImplMap> storage_partition_map_;
  scoped_refptr<SharedCorsOriginAccessList> shared_cors_origin_access_list_ =
      SharedCorsOriginAccessList::Create();
  std::unique_ptr<BrowsingDataRemoverImpl> browsing_data_remover_;
  std::unique_ptr<DownloadManager> download_manager_;
  std::unique_ptr<PermissionController> permission_controller_;
  scoped_refptr<BackgroundSyncScheduler> background_sync_scheduler_;
  std::unique_ptr<PrefetchService> prefetch_service_;
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<NavigationEntryScreenshotManager>
      nav_entry_screenshot_manager_;
#endif  // BUILDFLAG(IS_ANDROID)
  std::unique_ptr<InMemoryFederatedPermissionContext>
      federated_permission_context_;

  std::unique_ptr<media::VideoDecodePerfHistory> video_decode_perf_history_;
  std::unique_ptr<media::WebrtcVideoPerfHistory> webrtc_video_perf_history_;

  // Manages BTM for all WebContentses using this browser context.
  std::unique_ptr<BtmServiceImpl> btm_service_;
  // If BTM is disabled, any existing database file is asynchronously deleted
  // when the BrowserContextImpl is created. This RunLoop allows tests to wait
  // for the deletion to complete.
  //
  // TODO: crbug.com/356624038 - delete this when the BTM feature flag is
  // removed.
  base::RunLoop btm_cleanup_loop_;

#if BUILDFLAG(IS_CHROMEOS)
  scoped_refptr<storage::ExternalMountPoints> external_mount_points_;
#endif

  // Maps frame tokens to NavigationStateKeepAlives. There is one
  // NavigationStateKeepAlive per LocalFrameToken. It's possible to have
  // multiple keep alives per LocalFrameToken (e.g., multiple in-flight
  // navigations per RenderFrameHost), but this map will store the most recent
  // NavigationStateKeepAlive.
  // In the case of multiple navigations for a RenderFrameHost,
  // it is assumed that they are handled in order, with the latest navigation's
  // keep alive storing the state for that RenderFrameHost.
  // Note: This member must be above `keep_alive_handles_receiver_set_`. During
  // destruction, when NavigationStateKeepAlives get removed from the receiver
  // set, they will then remove themselves from
  // `navigation_state_keep_alive_map_`, so this map must still be alive when
  // that happens.
  using TokenNavigationStateKeepAliveMap =
      absl::flat_hash_map<blink::LocalFrameToken, NavigationStateKeepAlive*>;
  TokenNavigationStateKeepAliveMap navigation_state_keep_alive_map_;

  // Active keepalive handles for in-flight navigations. They are retained
  // on `BrowserContextImpl` because, by design, they may need to outlive the
  // `RenderFrameHostImpl` that initiated the navigation, but shouldn't be used
  // in a different BrowserContext.
  // Note that this set may contain in-flight navigations for different
  // RenderFrameHosts, and furthermore, there may even be multiple in-flight
  // navigations for a single RenderFrameHost.
  mojo::UniqueReceiverSet<blink::mojom::NavigationStateKeepAliveHandle>
      keep_alive_handles_receiver_set_;

  // TODO: crbug.com/40169693 - BrowserContext and BrowserContextImpl both have
  // WeakPtrFactories. Remove one once the inheritance is sorted out.
  base::WeakPtrFactory<BrowserContextImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_CONTEXT_IMPL_H_
