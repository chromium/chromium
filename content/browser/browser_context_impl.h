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

  // (See BrowserContext::BackfillPopupHeuristicGrants().)
  void BackfillPopupHeuristicGrants(base::OnceCallback<void(bool)> callback);

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

  const std::string unique_id_ = base::UnguessableToken::Create().ToString();
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

  // TODO: crbug.com/40169693 - BrowserContext and BrowserContextImpl both have
  // WeakPtrFactories. Remove one once the inheritance is sorted out.
  base::WeakPtrFactory<BrowserContextImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_CONTEXT_IMPL_H_
