// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_context_impl.h"

#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/background_sync/background_sync_scheduler.h"
#include "content/browser/browsing_data/browsing_data_remover_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/speech/tts_controller_impl.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "media/learning/common/media_learning_tasks.h"
#include "media/learning/impl/learning_session_impl.h"
#include "media/mojo/services/video_decode_perf_history.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "storage/browser/file_system/external_mount_points.h"
#endif

namespace content {

namespace {

void ShutdownServiceWorkerContext(StoragePartition* partition) {
  ServiceWorkerContextWrapper* wrapper =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  wrapper->process_manager()->Shutdown();
}

void ShutdownSharedWorkerContext(StoragePartition* partition) {
  partition->GetSharedWorkerService()->Shutdown();
}

void RegisterMediaLearningTask(
    media::learning::LearningSessionImpl* learning_session,
    const media::learning::LearningTask& task) {
  // The RegisterTask method cannot be directly used in base::Bind, because it
  // provides a default argument value for the 2nd parameter
  // (`feature_provider`).
  learning_session->RegisterTask(task);
}

}  // namespace

BrowserContext::Impl::Impl(BrowserContext* self) : self_(self) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  background_sync_scheduler_ = base::MakeRefCounted<BackgroundSyncScheduler>();
}

BrowserContext::Impl::~Impl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!storage_partition_map_)
      << "StoragePartitionMap is not shut down properly";

  if (!will_be_destroyed_soon_) {
    NOTREACHED();
    base::debug::DumpWithoutCrashing();
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
    NOTREACHED() << "rph_with_bc_reference : " << rph_crash_key_value;

    SCOPED_CRASH_KEY_STRING256("BrowserContext", "dangling_rph",
                               rph_crash_key_value);
    base::debug::DumpWithoutCrashing();
  }

  // Clean up any isolated origins and other security state associated with this
  // BrowserContext.
  policy->RemoveStateForBrowserContext(*self_);

  if (download_manager_)
    download_manager_->Shutdown();

  TtsControllerImpl::GetInstance()->OnBrowserContextDestroyed(self_);

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "shutdown", "BrowserContext::Impl::NotifyWillBeDestroyed() called.", this,
      "browser_context_impl", static_cast<void*>(this));
}

bool BrowserContext::Impl::ShutdownStarted() {
  return will_be_destroyed_soon_;
}

void BrowserContext::Impl::NotifyWillBeDestroyed() {
  TRACE_EVENT1("shutdown", "BrowserContext::Impl::NotifyWillBeDestroyed",
               "browser_context_impl", static_cast<void*>(this));
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "shutdown", "BrowserContext::Impl::NotifyWillBeDestroyed() called.", this,
      "browser_context_impl", static_cast<void*>(this));
  // Make sure NotifyWillBeDestroyed is idempotent.  This helps facilitate the
  // pattern where NotifyWillBeDestroyed is called from *both*
  // ShellBrowserContext and its derived classes (e.g. WebTestBrowserContext).
  if (will_be_destroyed_soon_)
    return;
  will_be_destroyed_soon_ = true;

  // Shut down service worker and shared worker machinery because these can keep
  // RenderProcessHosts and SiteInstances alive, and the codebase assumes these
  // are destroyed before the BrowserContext is destroyed.
  self_->ForEachStoragePartition(
      base::BindRepeating(ShutdownServiceWorkerContext));
  self_->ForEachStoragePartition(
      base::BindRepeating(ShutdownSharedWorkerContext));

  // Also forcibly release keep alive refcounts on RenderProcessHosts, to ensure
  // they destruct before the BrowserContext does.
  for (RenderProcessHost::iterator host_iterator =
           RenderProcessHost::AllHostsIterator();
       !host_iterator.IsAtEnd(); host_iterator.Advance()) {
    RenderProcessHost* host = host_iterator.GetCurrentValue();
    if (host->GetBrowserContext() == self_) {
      // This will also clean up spare RPH references.
      host->DisableKeepAliveRefCount();
    }
  }
}

StoragePartitionImplMap*
BrowserContext::Impl::GetOrCreateStoragePartitionMap() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!storage_partition_map_)
    storage_partition_map_ = std::make_unique<StoragePartitionImplMap>(self_);

  return storage_partition_map_.get();
}

BrowsingDataRemover* BrowserContext::Impl::GetBrowsingDataRemover() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!browsing_data_remover_) {
    browsing_data_remover_ = std::make_unique<BrowsingDataRemoverImpl>(self_);
    browsing_data_remover_->SetEmbedderDelegate(
        self_->GetBrowsingDataRemoverDelegate());
  }

  return browsing_data_remover_.get();
}

media::learning::LearningSession* BrowserContext::Impl::GetLearningSession() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!learning_session_) {
    learning_session_ = std::make_unique<media::learning::LearningSessionImpl>(
        base::SequencedTaskRunnerHandle::Get());

    // Using base::Unretained is safe below, because the callback here will not
    // be called or retained after the Register method below returns.
    media::learning::MediaLearningTasks::Register(base::BindRepeating(
        &RegisterMediaLearningTask, base::Unretained(learning_session_.get())));
  }

  return learning_session_.get();
}

media::VideoDecodePerfHistory*
BrowserContext::Impl::GetVideoDecodePerfHistory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!video_decode_perf_history_)
    video_decode_perf_history_ = self_->CreateVideoDecodePerfHistory();

  return video_decode_perf_history_.get();
}

void BrowserContext::Impl::ShutdownStoragePartitions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The BackgroundSyncScheduler keeps raw pointers to partitions; clear it
  // first.
  DCHECK(background_sync_scheduler_->HasOneRef());
  background_sync_scheduler_.reset();

  storage_partition_map_.reset();
}

DownloadManager* BrowserContext::Impl::GetDownloadManager() {
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

void BrowserContext::Impl::SetDownloadManagerForTesting(
    std::unique_ptr<DownloadManager> download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  download_manager_ = std::move(download_manager);
}

PermissionController* BrowserContext::Impl::GetPermissionController() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!permission_controller_)
    permission_controller_ = std::make_unique<PermissionControllerImpl>(self_);

  return permission_controller_.get();
}

void BrowserContext::Impl::SetPermissionControllerForTesting(
    std::unique_ptr<PermissionController> permission_controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  permission_controller_ = std::move(permission_controller);
}

storage::ExternalMountPoints* BrowserContext::Impl::GetMountPoints() {
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

}  // namespace content
