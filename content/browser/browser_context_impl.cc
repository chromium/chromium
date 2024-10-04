// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_context_impl.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/background_sync/background_sync_scheduler.h"
#include "content/browser/browsing_data/browsing_data_remover_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/in_memory_federated_permission_context.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_manager.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/speech/tts_controller_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/shared_worker_service.h"
#include "media/capabilities/webrtc_video_stats_db_impl.h"
#include "media/learning/common/media_learning_tasks.h"
#include "media/learning/impl/learning_session_impl.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "media/mojo/services/webrtc_video_perf_history.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "storage/browser/file_system/external_mount_points.h"
#endif

namespace content {

namespace {

void NotifyContextWillBeDestroyed(StoragePartition* partition) {
  static_cast<StoragePartitionImpl*>(partition)
      ->OnBrowserContextWillBeDestroyed();
}

void RegisterMediaLearningTask(
    media::learning::LearningSessionImpl* learning_session,
    const media::learning::LearningTask& task) {
  // The RegisterTask method cannot be directly used in base::Bind, because it
  // provides a default argument value for the 2nd parameter
  // (`feature_provider`).
  learning_session->RegisterTask(task);
}

// Kill switch that controls whether to cancel navigations as part of
// BrowserContext shutdown. See https://crbug.com/40274462.
BASE_FEATURE(kCancelNavigationsDuringBrowserContextShutdown,
             "CancelNavigationsDuringBrowserContextShutdown",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

// static
BrowserContextImpl* BrowserContextImpl::From(BrowserContext* self) {
  return self->impl();
}

BrowserContextImpl::BrowserContextImpl(BrowserContext* self) : self_(self) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  background_sync_scheduler_ = base::MakeRefCounted<BackgroundSyncScheduler>();
}

BrowserContextImpl::~BrowserContextImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!storage_partition_map_)
      << "StoragePartitionMap is not shut down properly";

  if (!will_be_destroyed_soon_) {
    NOTREACHED_IN_MIGRATION();
  }

  // Verify that there are no outstanding RenderProcessHosts that reference
  // this context. Trigger a crash report if there are still references so
  // we can detect/diagnose potential UAFs.
  std::string rph_crash_key_value;
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  for (RenderProcessHost::iterator host_iterator =
           RenderProcessHost::AllHostsIterator();
       !host_iterator.IsAtEnd(); host_iterator.Advance()) {
    RenderProcessHost* host = host_iterator.GetCurrentValue();
    if (host->GetBrowserContext() == self_) {
      rph_crash_key_value +=
          "{ " + host->GetInfoForBrowserContextDestructionCrashReporting() +
          " }";
    }
  }
  if (!rph_crash_key_value.empty()) {
    SCOPED_CRASH_KEY_STRING256("BrowserContext", "dangling_rph",
                               rph_crash_key_value);
    DUMP_WILL_BE_NOTREACHED()
        << "rph_with_bc_reference : " << rph_crash_key_value;
  }

  // Clean up any isolated origins and other security state associated with this
  // BrowserContext.
  policy->RemoveStateForBrowserContext(*self_);

  if (download_manager_)
    download_manager_->Shutdown();

  TtsControllerImpl::GetInstance()->OnBrowserContextDestroyed(self_);

  if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                          std::move(resource_context_));
  }

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "shutdown", "BrowserContextImpl::NotifyWillBeDestroyed() called.", this,
      "browser_context_impl", static_cast<void*>(this));
}

bool BrowserContextImpl::ShutdownStarted() {
  return will_be_destroyed_soon_;
}

void BrowserContextImpl::NotifyWillBeDestroyed() {
  TRACE_EVENT1("shutdown", "BrowserContextImpl::NotifyWillBeDestroyed",
               "browser_context_impl", static_cast<void*>(this));
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "shutdown", "BrowserContextImpl::NotifyWillBeDestroyed() called.", this,
      "browser_context_impl", static_cast<void*>(this));
  // Make sure NotifyWillBeDestroyed is idempotent.  This helps facilitate the
  // pattern where NotifyWillBeDestroyed is called from *both*
  // ShellBrowserContext and its derived classes (e.g. WebTestBrowserContext).
  if (will_be_destroyed_soon_)
    return;
  will_be_destroyed_soon_ = true;

  self_->ForEachLoadedStoragePartition(&NotifyContextWillBeDestroyed);

  // Cancel navigations that are happening in the BrowserContext that's going
  // away.
  if (base::FeatureList::IsEnabled(
          kCancelNavigationsDuringBrowserContextShutdown)) {
    RenderFrameHostImpl::CancelAllNavigationsForBrowserContextShutdown(self_);
  }

  // Also forcibly release keep alive refcounts on RenderProcessHosts, to ensure
  // they destruct before the BrowserContext does.
  for (RenderProcessHost::iterator host_iterator =
           RenderProcessHost::AllHostsIterator();
       !host_iterator.IsAtEnd(); host_iterator.Advance()) {
    RenderProcessHost* host = host_iterator.GetCurrentValue();
    if (host->GetBrowserContext() == self_) {
      // This will also clean up spare RPH references.
      host->DisableRefCounts();
    }
  }
}

StoragePartitionImplMap* BrowserContextImpl::GetOrCreateStoragePartitionMap() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!storage_partition_map_)
    storage_partition_map_ = std::make_unique<StoragePartitionImplMap>(self_);

  return storage_partition_map_.get();
}

BrowsingDataRemoverImpl* BrowserContextImpl::GetBrowsingDataRemover() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!browsing_data_remover_) {
    browsing_data_remover_ = std::make_unique<BrowsingDataRemoverImpl>(self_);
    browsing_data_remover_->SetEmbedderDelegate(
        self_->GetBrowsingDataRemoverDelegate());
  }

  return browsing_data_remover_.get();
}

media::learning::LearningSession* BrowserContextImpl::GetLearningSession() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!learning_session_) {
    learning_session_ = std::make_unique<media::learning::LearningSessionImpl>(
        base::SequencedTaskRunner::GetCurrentDefault());

    // Using base::Unretained is safe below, because the callback here will not
    // be called or retained after the Register method below returns.
    media::learning::MediaLearningTasks::Register(base::BindRepeating(
        &RegisterMediaLearningTask, base::Unretained(learning_session_.get())));
  }

  return learning_session_.get();
}

media::VideoDecodePerfHistory* BrowserContextImpl::GetVideoDecodePerfHistory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!video_decode_perf_history_)
    video_decode_perf_history_ = self_->CreateVideoDecodePerfHistory();

  return video_decode_perf_history_.get();
}

std::unique_ptr<media::WebrtcVideoPerfHistory>
BrowserContextImpl::CreateWebrtcVideoPerfHistory() {
  // TODO(crbug.com/40172952): Implement in memory path in
  // off_the_record_profile_impl.cc and web_engine_browser_context.cc

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* db_provider =
      self_->GetDefaultStoragePartition()->GetProtoDatabaseProvider();

  std::unique_ptr<media::WebrtcVideoStatsDB> stats_db =
      media::WebrtcVideoStatsDBImpl::Create(
          self_->GetPath().Append(FILE_PATH_LITERAL("WebrtcVideoStats")),
          db_provider);

  return std::make_unique<media::WebrtcVideoPerfHistory>(std::move(stats_db));
}

media::WebrtcVideoPerfHistory* BrowserContextImpl::GetWebrtcVideoPerfHistory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!webrtc_video_perf_history_)
    webrtc_video_perf_history_ = CreateWebrtcVideoPerfHistory();

  return webrtc_video_perf_history_.get();
}

void BrowserContextImpl::ShutdownStoragePartitions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The BackgroundSyncScheduler keeps raw pointers to partitions; clear it
  // first.
  DCHECK(background_sync_scheduler_->HasOneRef());
  background_sync_scheduler_.reset();

  storage_partition_map_.reset();
}

DownloadManager* BrowserContextImpl::GetDownloadManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Lazily populate `download_manager_`.  This is important to
  // 1) Avoid constructing DownloadManagerImpl when a test might have provided
  //    an alternative object via SetDownloadManagerForTesting.
  // 2) Avoiding calling into DownloadManagerImpl's constructor with a partially
  //    constructed BrowserContext.
  if (!download_manager_) {
    download_manager_ = std::make_unique<DownloadManagerImpl>(self_);

    // Note that GetDownloadManagerDelegate might call into GetDownloadManager,
    // leading to re-entrancy concerns.  We avoid re-entrancy by making sure
    // `download_manager_` is set earlier, above.
    download_manager_->SetDelegate(self_->GetDownloadManagerDelegate());
  }

  return download_manager_.get();
}

void BrowserContextImpl::SetDownloadManagerForTesting(
    std::unique_ptr<DownloadManager> download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (download_manager_)
    download_manager_->Shutdown();
  download_manager_ = std::move(download_manager);
}

PermissionController* BrowserContextImpl::GetPermissionController() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!permission_controller_)
    permission_controller_ = std::make_unique<PermissionControllerImpl>(self_);

  return permission_controller_.get();
}

void BrowserContextImpl::SetPermissionControllerForTesting(
    std::unique_ptr<PermissionController> permission_controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  permission_controller_ = std::move(permission_controller);
}

storage::ExternalMountPoints* BrowserContextImpl::GetMountPoints() {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!external_mount_points_)
    external_mount_points_ = storage::ExternalMountPoints::CreateRefCounted();
  return external_mount_points_.get();
#else
  return nullptr;
#endif
}

PrefetchService* BrowserContextImpl::GetPrefetchService() {
  if (!prefetch_service_) {
    prefetch_service_ = std::make_unique<PrefetchService>(self_);
  }

  return prefetch_service_.get();
}

InMemoryFederatedPermissionContext*
BrowserContextImpl::GetFederatedPermissionContext() {
  if (!federated_permission_context_) {
    federated_permission_context_ =
        std::make_unique<InMemoryFederatedPermissionContext>();
  }
  return federated_permission_context_.get();
}

void BrowserContextImpl::ResetFederatedPermissionContext() {
  federated_permission_context_.reset();
}

void BrowserContextImpl::SetPrefetchServiceForTesting(
    std::unique_ptr<PrefetchService> prefetch_service) {
  prefetch_service_ = std::move(prefetch_service);
}

NavigationEntryScreenshotManager*
BrowserContextImpl::GetNavigationEntryScreenshotManager() {
  if (!nav_entry_screenshot_manager_ &&
      NavigationTransitionConfig::AreBackForwardTransitionsEnabled()) {
    nav_entry_screenshot_manager_ =
        std::make_unique<NavigationEntryScreenshotManager>();
  }
  return nav_entry_screenshot_manager_.get();
}

void BrowserContextImpl::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto->set_id(UniqueId());
}

}  // namespace content
