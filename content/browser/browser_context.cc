// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/typed_macros.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/push_messaging/push_messaging_router.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace content {

namespace {

void SaveSessionStateOnIOThread(AppCacheServiceImpl* appcache_service) {
  appcache_service->set_force_keep_session_state();
}

base::WeakPtr<storage::BlobStorageContext> BlobStorageContextGetterForBrowser(
    scoped_refptr<ChromeBlobStorageContext> blob_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return blob_context->context()->AsWeakPtr();
}

}  // namespace

// static
void BrowserContext::AsyncObliterateStoragePartition(
    BrowserContext* self,
    const std::string& partition_domain,
    base::OnceClosure on_gc_required) {
  self->impl()->GetOrCreateStoragePartitionMap()->AsyncObliterate(
      partition_domain, std::move(on_gc_required));
}

// static
void BrowserContext::GarbageCollectStoragePartitions(
    BrowserContext* self,
    std::unique_ptr<std::unordered_set<base::FilePath>> active_paths,
    base::OnceClosure done) {
  self->impl()->GetOrCreateStoragePartitionMap()->GarbageCollect(
      std::move(active_paths), std::move(done));
}

// static
DownloadManager* BrowserContext::GetDownloadManager(BrowserContext* self) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return self->impl()->GetDownloadManager();
}

// static
storage::ExternalMountPoints* BrowserContext::GetMountPoints(
    BrowserContext* self) {
  return self->impl()->GetMountPoints();
}

// static
BrowsingDataRemover* BrowserContext::GetBrowsingDataRemover(
    BrowserContext* self) {
  return self->impl()->GetBrowsingDataRemover();
}

// static
PermissionController* BrowserContext::GetPermissionController(
    BrowserContext* self) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return self->impl()->GetPermissionController();
}

// static
StoragePartition* BrowserContext::GetStoragePartition(
    BrowserContext* browser_context,
    SiteInstance* site_instance,
    bool can_create) {
  if (site_instance)
    DCHECK_EQ(browser_context, site_instance->GetBrowserContext());

  auto* site_instance_impl = static_cast<SiteInstanceImpl*>(site_instance);
  auto partition_config =
      site_instance_impl
          ? site_instance_impl->GetSiteInfo().GetStoragePartitionConfig(
                browser_context)
          : StoragePartitionConfig::CreateDefault(browser_context);
  return GetStoragePartition(browser_context, partition_config, can_create);
}

// static
StoragePartition* BrowserContext::GetStoragePartition(
    BrowserContext* self,
    const StoragePartitionConfig& storage_partition_config,
    bool can_create) {
  if (self->IsOffTheRecord()) {
    // An off the record profile MUST only use in memory storage partitions.
    CHECK(storage_partition_config.in_memory());
  }

  return self->impl()->GetOrCreateStoragePartitionMap()->Get(
      storage_partition_config, can_create);
}

StoragePartition* BrowserContext::GetStoragePartitionForUrl(
    BrowserContext* browser_context,
    const GURL& url,
    bool can_create) {
  auto storage_partition_config = SiteInfo::GetStoragePartitionConfigForUrl(
      browser_context, url, /*is_site_url=*/false);

  return GetStoragePartition(browser_context, storage_partition_config,
                             can_create);
}

// static
void BrowserContext::ForEachStoragePartition(
    BrowserContext* self,
    StoragePartitionCallback callback) {
  StoragePartitionImplMap* partition_map =
      self->impl()->storage_partition_map();
  if (!partition_map)
    return;

  partition_map->ForEach(std::move(callback));
}

// static
size_t BrowserContext::GetStoragePartitionCount(BrowserContext* self) {
  StoragePartitionImplMap* partition_map =
      self->impl()->storage_partition_map();
  return partition_map ? partition_map->size() : 0;
}

StoragePartition* BrowserContext::GetDefaultStoragePartition(
    BrowserContext* browser_context) {
  return GetStoragePartition(
      browser_context, StoragePartitionConfig::CreateDefault(browser_context));
}

// static
void BrowserContext::CreateMemoryBackedBlob(BrowserContext* browser_context,
                                            base::span<const uint8_t> data,
                                            const std::string& content_type,
                                            BlobCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChromeBlobStorageContext* blob_context =
      ChromeBlobStorageContext::GetFor(browser_context);
  GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ChromeBlobStorageContext::CreateMemoryBackedBlob,
                     base::WrapRefCounted(blob_context), data, content_type),
      std::move(callback));
}

// static
BrowserContext::BlobContextGetter BrowserContext::GetBlobStorageContext(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context =
      ChromeBlobStorageContext::GetFor(browser_context);
  return base::BindRepeating(&BlobStorageContextGetterForBrowser,
                             chrome_blob_context);
}

// static
mojo::PendingRemote<blink::mojom::Blob> BrowserContext::GetBlobRemote(
    BrowserContext* browser_context,
    const std::string& uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ChromeBlobStorageContext::GetBlobRemote(browser_context, uuid);
}

// static
void BrowserContext::DeliverPushMessage(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& message_id,
    base::Optional<std::string> payload,
    base::OnceCallback<void(blink::mojom::PushEventStatus)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingRouter::DeliverMessage(
      browser_context, origin, service_worker_registration_id, message_id,
      std::move(payload), std::move(callback));
}

// static
void BrowserContext::FirePushSubscriptionChangeEvent(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    blink::mojom::PushSubscriptionPtr new_subscription,
    blink::mojom::PushSubscriptionPtr old_subscription,
    base::OnceCallback<void(blink::mojom::PushEventStatus)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingRouter::FireSubscriptionChangeEvent(
      browser_context, origin, service_worker_registration_id,
      std::move(new_subscription), std::move(old_subscription),
      std::move(callback));
}

// static
void BrowserContext::NotifyWillBeDestroyed(BrowserContext* self) {
  self->impl()->NotifyWillBeDestroyed();
}

bool BrowserContext::ShutdownStarted() {
  return impl()->ShutdownStarted();
}

void BrowserContext::EnsureResourceContextInitialized(BrowserContext* context) {
  // This will be enough to tickle initialization of BrowserContext if
  // necessary, which initializes ResourceContext. The reason we don't call
  // ResourceContext::InitializeResourceContext() directly here is that
  // ResourceContext initialization may call back into BrowserContext
  // and when that call returns it'll end rewriting its Impl. It will
  // end up rewriting the same value but this still causes a race condition.
  //
  // See http://crbug.com/115678.
  GetDefaultStoragePartition(context);
}

void BrowserContext::SaveSessionState(BrowserContext* browser_context) {
  StoragePartition* storage_partition =
      BrowserContext::GetDefaultStoragePartition(browser_context);

  storage::DatabaseTracker* database_tracker =
      storage_partition->GetDatabaseTracker();
  database_tracker->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&storage::DatabaseTracker::SetForceKeepSessionState,
                     base::WrapRefCounted(database_tracker)));

  if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    auto* appcache_service = static_cast<AppCacheServiceImpl*>(
        storage_partition->GetAppCacheService());
    if (appcache_service) {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&SaveSessionStateOnIOThread, appcache_service));
    }
  }

  storage_partition->GetCookieManagerForBrowserProcess()
      ->SetForceKeepSessionState();

  DOMStorageContextWrapper* dom_storage_context_proxy =
      static_cast<DOMStorageContextWrapper*>(
          storage_partition->GetDOMStorageContext());
  dom_storage_context_proxy->SetForceKeepSessionState();

  auto& indexed_db_control = storage_partition->GetIndexedDBControl();
  indexed_db_control.SetForceKeepSessionState();
}

// static
void BrowserContext::SetDownloadManagerForTesting(
    BrowserContext* self,
    std::unique_ptr<DownloadManager> download_manager) {
  self->impl()->SetDownloadManagerForTesting(  // IN-TEST
      std::move(download_manager));
}

// static
void BrowserContext::SetPermissionControllerForTesting(
    BrowserContext* self,
    std::unique_ptr<PermissionController> permission_controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(permission_controller);
  self->impl()->SetPermissionControllerForTesting(  // IN-TEST
      std::move(permission_controller));
}

// static
SharedCorsOriginAccessList* BrowserContext::GetSharedCorsOriginAccessList(
    BrowserContext* self) {
  return self->impl()->shared_cors_origin_access_list();
}

BrowserContext::BrowserContext() {
  TRACE_EVENT("shutdown", "BrowserContext::BrowserContext",
              [&](perfetto::EventContext ctx) {
                auto* event =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
                event->set_chrome_browser_context()->set_ptr(
                    reinterpret_cast<uint64_t>(this));
              });
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("shutdown", "Browser.BrowserContext", this,
                                    "browser_context",
                                    static_cast<void*>(this));

  impl_ = std::make_unique<Impl>(this);
}

BrowserContext::~BrowserContext() {
  TRACE_EVENT("shutdown", "BrowserContext::~BrowserContext",
              [&](perfetto::EventContext ctx) {
                auto* event =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
                event->set_chrome_browser_context()->set_ptr(
                    reinterpret_cast<uint64_t>(this));
              });

  impl_.reset();

  TRACE_EVENT_NESTABLE_ASYNC_END1("shutdown", "Browser.BrowserContext", this,
                                  "browser_context", static_cast<void*>(this));
}

void BrowserContext::ShutdownStoragePartitions() {
  impl()->ShutdownStoragePartitions();
}

std::string BrowserContext::GetMediaDeviceIDSalt() {
  return UniqueId();
}

// static
std::string BrowserContext::CreateRandomMediaDeviceIDSalt() {
  return base::UnguessableToken::Create().ToString();
}

const std::string& BrowserContext::UniqueId() {
  return impl()->UniqueId();
}

media::VideoDecodePerfHistory* BrowserContext::GetVideoDecodePerfHistory() {
  return impl()->GetVideoDecodePerfHistory();
}

media::learning::LearningSession* BrowserContext::GetLearningSession() {
  return impl()->GetLearningSession();
}

download::InProgressDownloadManager*
BrowserContext::RetriveInProgressDownloadManager() {
  return nullptr;
}

FileSystemAccessPermissionContext*
BrowserContext::GetFileSystemAccessPermissionContext() {
  return nullptr;
}

ContentIndexProvider* BrowserContext::GetContentIndexProvider() {
  return nullptr;
}

bool BrowserContext::CanUseDiskWhenOffTheRecord() {
  return false;
}

variations::VariationsClient* BrowserContext::GetVariationsClient() {
  return nullptr;
}

void BrowserContext::WriteIntoTracedValue(perfetto::TracedValue context) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("id", impl()->UniqueId());
}

}  // namespace content
