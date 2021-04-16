// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_BROWSER_CONTEXT_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/shared_cors_origin_access_list.h"

namespace media {
class VideoDecodePerfHistory;
namespace learning {
class LearningSession;
class LearningSessionImpl;
}  // namespace learning
}  // namespace media

namespace storage {
class ExternalMountPoints;
}  // namespace storage

namespace content {

class BackgroundSyncScheduler;
class BrowsingDataRemover;
class BrowsingDataRemoverImpl;
class DownloadManager;
class StoragePartitionImplMap;
class PermissionController;

// //content-internal parts of BrowserContext.
//
// TODO(https://crbug.com/1179776): Evolve the Impl class into a
// full BrowserContextImpl.
class BrowserContext::Impl {
 public:
  explicit Impl(BrowserContext* self);
  ~Impl();

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

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

  BrowsingDataRemover* GetBrowsingDataRemover();

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

  BackgroundSyncScheduler* background_sync_scheduler() {
    return background_sync_scheduler_.get();
  }

 private:
  // TODO(https://crbug.com/1179776): Remove the `self_` field.  In the future
  // BrowserContext::Impl should become BrowserContextImpl that inherits from
  // BrowserContext, making the `self_` member obsolete.
  BrowserContext* self_;

  const std::string unique_id_ = base::UnguessableToken::Create().ToString();
  bool will_be_destroyed_soon_ = false;

  std::unique_ptr<StoragePartitionImplMap> storage_partition_map_;
  scoped_refptr<SharedCorsOriginAccessList> shared_cors_origin_access_list_ =
      SharedCorsOriginAccessList::Create();
  std::unique_ptr<BrowsingDataRemoverImpl> browsing_data_remover_;
  std::unique_ptr<DownloadManager> download_manager_;
  std::unique_ptr<PermissionController> permission_controller_;
  scoped_refptr<BackgroundSyncScheduler> background_sync_scheduler_;

  std::unique_ptr<media::learning::LearningSessionImpl> learning_session_;
  std::unique_ptr<media::VideoDecodePerfHistory> video_decode_perf_history_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  scoped_refptr<storage::ExternalMountPoints> external_mount_points_;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_CONTEXT_IMPL_H_
