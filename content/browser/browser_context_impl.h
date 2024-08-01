// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_BROWSER_CONTEXT_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/shared_cors_origin_access_list.h"

namespace media {
class VideoDecodePerfHistory;
class WebrtcVideoPerfHistory;
namespace learning {
class LearningSession;
class LearningSessionImpl;
}  // namespace learning
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
class NavigationEntryScreenshotManager;
class PermissionController;
class PrefetchService;
class StoragePartitionImplMap;

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

  media::learning::LearningSession* GetLearningSession();

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

  NavigationEntryScreenshotManager* GetNavigationEntryScreenshotManager();

  InMemoryFederatedPermissionContext* GetFederatedPermissionContext();
  void ResetFederatedPermissionContext();

  using TraceProto = perfetto::protos::pbzero::ChromeBrowserContext;
  // Write a representation of this object into a trace.
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> context) const;

  ResourceContext* GetResourceContext() const {
    return resource_context_.get();
  }

 private:
  // Creates the media service for storing/retrieving WebRTC encoding and
  // decoding performance stats.  Exposed here rather than StoragePartition
  // because all SiteInstances should have similar performance and stats are not
  // exposed to the web directly, so privacy is not compromised.
  std::unique_ptr<media::WebrtcVideoPerfHistory> CreateWebrtcVideoPerfHistory();

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
  std::unique_ptr<NavigationEntryScreenshotManager>
      nav_entry_screenshot_manager_;
  std::unique_ptr<InMemoryFederatedPermissionContext>
      federated_permission_context_;

  std::unique_ptr<media::learning::LearningSessionImpl> learning_session_;
  std::unique_ptr<media::VideoDecodePerfHistory> video_decode_perf_history_;
  std::unique_ptr<media::WebrtcVideoPerfHistory> webrtc_video_perf_history_;

  // TODO(crbug.com/40604019): Get rid of ResourceContext.
  // Created on the UI thread, otherwise lives on and is destroyed on the IO
  // thread.
  std::unique_ptr<ResourceContext> resource_context_ =
      std::make_unique<ResourceContext>();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  scoped_refptr<storage::ExternalMountPoints> external_mount_points_;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_CONTEXT_IMPL_H_
