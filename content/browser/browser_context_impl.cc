// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_context_impl.h"

#include <utility>

#include "base/check_is_test.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/features.h"
#include "content/browser/background_sync/background_sync_scheduler.h"
#include "content/browser/browsing_data/browsing_data_remover_impl.h"
#include "content/browser/btm/btm_service_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/in_memory_federated_permission_context.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/renderer_host/initiator_navigation_state_impl.h"
#include "content/browser/renderer_host/navigation_state_keep_alive.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/speech/tts_controller_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/common/content_client.h"
#include "media/capabilities/webrtc_video_stats_db_impl.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "media/mojo/services/webrtc_video_perf_history.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "storage/browser/file_system/external_mount_points.h"
#endif

namespace content {

namespace {

void NotifyContextWillBeDestroyed(StoragePartition* partition) {
  static_cast<StoragePartitionImpl*>(partition)
      ->OnBrowserContextWillBeDestroyed();
}

// Kill switch that controls whether to cancel navigations as part of
// BrowserContext shutdown. See https://crbug.com/40274462.
BASE_FEATURE(kCancelNavigationsDuringBrowserContextShutdown,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

// static
BrowserContextImpl* BrowserContextImpl::From(BrowserContext* self) {
  return self->impl();
}

void BrowserContextImpl::MaybeCleanupBtm() {
  base::ScopedClosureRunner quit_runner(btm_cleanup_loop_.QuitClosure());
  // Don't attempt to delete the database if the BTM feature is enabled; we need
  // it.
  if (base::FeatureList::IsEnabled(features::kBtm)) {
    return;
  }

  // Don't attempt to delete the database if this browser context should never
  // have BTM enabled. (This is important for embedders like ChromeOS, which
  // have internal non-user-facing browser contexts. We don't want to touch
  // them.)
  if (!GetContentClient()->browser()->ShouldEnableBtm(self_)) {
    return;
  }

  // Don't attempt to delete the database if this browser context doesn't write
  // to disk. (This is important for embedders like Chrome, which can make OTR
  // browser contexts share the same data directory as a non-OTR context.)
  if (self_->IsOffTheRecord()) {
    return;
  }

  BtmStorage::DeleteDatabaseFiles(GetBtmFilePath(self_), quit_runner.Release());
}

void BrowserContextImpl::WaitForBtmCleanupForTesting() {
  btm_cleanup_loop_.Run();
}

BrowserContextImpl::BrowserContextImpl(BrowserContext* self) : self_(self) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  background_sync_scheduler_ = base::MakeRefCounted<BackgroundSyncScheduler>();

  // Run MaybeCleanupBtm() very soon. We can't call it right now because it
  // calls a virtual function (BrowserContext::IsOffTheRecord()), which causes
  // undefined behavior since we're called by the BrowserContext constructor
  // and the method is not implemented by that class.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BrowserContextImpl::MaybeCleanupBtm,
                                weak_factory_.GetWeakPtr()));
}

BrowserContextImpl::~BrowserContextImpl() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  DCHECK(!storage_partition_map_)
      << "StoragePartitionMap is not shut down properly";

  if (!will_be_destroyed_soon_) {
    NOTREACHED();
  }

  // Verify that there are no outstanding RenderProcessHosts that reference
  // this context. Trigger a crash report if there are still references so
  // we can detect/diagnose potential UAFs.
  std::string rph_crash_key_value;
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

  if (download_manager_) {
    download_manager_->Shutdown();
  }

  TtsControllerImpl::GetInstance()->OnBrowserContextDestroyed(self_);

  // Corresponds to the TRACE_EVENT_BEGIN in NotifyWillBeDestroyed.
  TRACE_EVENT_END("shutdown", perfetto::Track::FromPointer(this),
                  "browser_context_impl", static_cast<void*>(this));
}

bool BrowserContextImpl::ShutdownStarted() {
  return will_be_destroyed_soon_;
}

void BrowserContextImpl::NotifyWillBeDestroyed() {
  TRACE_EVENT1("shutdown", "BrowserContextImpl::NotifyWillBeDestroyed",
               "browser_context_impl", static_cast<void*>(this));
  TRACE_EVENT_BEGIN("shutdown",
                    "BrowserContextImpl::NotifyWillBeDestroyed() called.",
                    perfetto::Track::FromPointer(this), "browser_context_impl",
                    static_cast<void*>(this));
  // Make sure NotifyWillBeDestroyed is idempotent.  This helps facilitate the
  // pattern where NotifyWillBeDestroyed is called from *both*
  // ShellBrowserContext and its derived classes (e.g. WebTestBrowserContext).
  if (will_be_destroyed_soon_) {
    return;
  }
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

  // Clean up any isolated origins and other security state associated with this
  // BrowserContext.
  ChildProcessSecurityPolicyImpl::GetInstance()->RemoveStateForBrowserContext(
      *self_);
}

StoragePartitionImplMap* BrowserContextImpl::GetOrCreateStoragePartitionMap() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  if (!storage_partition_map_) {
    storage_partition_map_ = std::make_unique<StoragePartitionImplMap>(self_);
  }

  return storage_partition_map_.get();
}

BrowsingDataRemoverImpl* BrowserContextImpl::GetBrowsingDataRemover() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  if (!browsing_data_remover_) {
    browsing_data_remover_ = std::make_unique<BrowsingDataRemoverImpl>(self_);
    browsing_data_remover_->SetEmbedderDelegate(
        self_->GetBrowsingDataRemoverDelegate());
  }

  return browsing_data_remover_.get();
}

media::VideoDecodePerfHistory* BrowserContextImpl::GetVideoDecodePerfHistory() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  if (!video_decode_perf_history_) {
    video_decode_perf_history_ = self_->CreateVideoDecodePerfHistory();
  }

  return video_decode_perf_history_.get();
}

std::unique_ptr<media::WebrtcVideoPerfHistory>
BrowserContextImpl::CreateWebrtcVideoPerfHistory() {
  // TODO(crbug.com/40172952): Implement in memory path in
  // off_the_record_profile_impl.cc and web_engine_browser_context.cc

  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  auto* db_provider =
      self_->GetDefaultStoragePartition()->GetProtoDatabaseProvider();

  std::unique_ptr<media::WebrtcVideoStatsDB> stats_db =
      media::WebrtcVideoStatsDBImpl::Create(
          self_->GetPath().Append(FILE_PATH_LITERAL("WebrtcVideoStats")),
          db_provider);

  return std::make_unique<media::WebrtcVideoPerfHistory>(std::move(stats_db));
}

media::WebrtcVideoPerfHistory* BrowserContextImpl::GetWebrtcVideoPerfHistory() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  if (!webrtc_video_perf_history_) {
    webrtc_video_perf_history_ = CreateWebrtcVideoPerfHistory();
  }

  return webrtc_video_perf_history_.get();
}

void BrowserContextImpl::ShutdownStoragePartitions() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  // The BackgroundSyncScheduler keeps raw pointers to partitions; clear it
  // first.
  CHECK(background_sync_scheduler_->HasOneRef(), base::NotFatalUntil::M159);
  background_sync_scheduler_.reset();

  storage_partition_map_.reset();

  // Delete the BtmService, causing its SQLite database file to be closed. This
  // is necessary for TestBrowserContext to be able to delete its temporary
  // directory.
  btm_service_.reset();
}

DownloadManager* BrowserContextImpl::GetDownloadManager() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

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
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  if (download_manager_) {
    download_manager_->Shutdown();
  }
  download_manager_ = std::move(download_manager);
}

PermissionController* BrowserContextImpl::GetPermissionController() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  if (!permission_controller_) {
    permission_controller_ = std::make_unique<PermissionControllerImpl>(self_);
  }

  return permission_controller_.get();
}

void BrowserContextImpl::SetPermissionControllerForTesting(
    std::unique_ptr<PermissionController> permission_controller) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  permission_controller_ = std::move(permission_controller);
}

storage::ExternalMountPoints* BrowserContextImpl::GetMountPoints() {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
            !BrowserThread::IsThreadInitialized(BrowserThread::UI),
        base::NotFatalUntil::M159);

#if BUILDFLAG(IS_CHROMEOS)
  if (!external_mount_points_) {
    external_mount_points_ = storage::ExternalMountPoints::CreateRefCounted();
  }
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

#if BUILDFLAG(IS_ANDROID)
NavigationEntryScreenshotManager*
BrowserContextImpl::GetNavigationEntryScreenshotManager() {
  if (!nav_entry_screenshot_manager_ &&
      BackForwardTransitionAnimationManager::
          ShouldAnimateBackForwardTransitions()) {
    nav_entry_screenshot_manager_ =
        std::make_unique<NavigationEntryScreenshotManager>();
  }
  return nav_entry_screenshot_manager_.get();
}
#endif  // BUILDFLAG(IS_ANDROID)

void BrowserContextImpl::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto->set_id(UniqueId());
}

namespace {
bool ShouldEnableBtm(BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(features::kBtm)) {
    return false;
  }

  if (!GetContentClient()->browser()->ShouldEnableBtm(browser_context)) {
    return false;
  }

  return true;
}
}  // namespace

BtmServiceImpl* BrowserContextImpl::GetBtmService() {
  if (!btm_service_) {
    if (!ShouldEnableBtm(self_)) {
      return nullptr;
    }
    btm_service_ = std::make_unique<BtmServiceImpl>(
        base::PassKey<BrowserContextImpl>(), self_);
    GetContentClient()->browser()->OnBtmServiceCreated(self_,
                                                       btm_service_.get());
  }

  return btm_service_.get();
}

void BrowserContextImpl::RegisterKeepAliveHandle(
    mojo::PendingReceiver<blink::mojom::NavigationStateKeepAliveHandle>
        receiver,
    std::unique_ptr<NavigationStateKeepAlive> handle) {
  auto frame_token = static_cast<InitiatorNavigationStateImpl*>(
                         handle->initiator_navigation_state().get())
                         ->frame_token();
  navigation_state_keep_alive_map_[frame_token] = handle.get();
  keep_alive_handles_receiver_set_.Add(std::move(handle), std::move(receiver));
}

NavigationStateKeepAlive* BrowserContextImpl::GetNavigationStateKeepAlive(
    blink::LocalFrameToken frame_token) {
  return base::FindPtrOrNull(navigation_state_keep_alive_map_, frame_token);
}

void BrowserContextImpl::RemoveKeepAliveHandleFromMap(
    blink::LocalFrameToken frame_token,
    NavigationStateKeepAlive* keep_alive) {
  // The NavigationStateKeepAlive associated with `frame_token` may have
  // changed. Make sure the specified one is removed from the map.
  auto it = navigation_state_keep_alive_map_.find(frame_token);
  if (it != navigation_state_keep_alive_map_.end() &&
      it->second == keep_alive) {
    navigation_state_keep_alive_map_.erase(it);
  }
}

}  // namespace content
