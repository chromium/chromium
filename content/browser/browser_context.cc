// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/supports_user_data.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browsing_data/browsing_data_remover_impl.h"
#include "content/browser/content_service_delegate_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/shared_cors_origin_access_list_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/push_messaging/push_messaging_router.h"
#include "content/browser/service_manager/common_browser_interfaces.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "media/capabilities/video_decode_stats_db_impl.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "net/cookies/cookie_store.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/content/public/mojom/constants.mojom.h"
#include "services/content/service.h"
#include "services/file/file_service.h"
#include "services/file/public/mojom/constants.mojom.h"
#include "services/file/user_id_map.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/fileapi/external_mount_points.h"

using base::UserDataAdapter;

namespace content {

namespace {

base::LazyInstance<std::map<std::string, BrowserContext*>>::DestructorAtExit
    g_user_id_to_context = LAZY_INSTANCE_INITIALIZER;

class ServiceUserIdHolder : public base::SupportsUserData::Data {
 public:
  explicit ServiceUserIdHolder(const std::string& user_id)
      : user_id_(user_id) {}
  ~ServiceUserIdHolder() override {}

  const std::string& user_id() const { return user_id_; }

 private:
  std::string user_id_;

  DISALLOW_COPY_AND_ASSIGN(ServiceUserIdHolder);
};

class ContentServiceDelegateHolder : public base::SupportsUserData::Data {
 public:
  explicit ContentServiceDelegateHolder(BrowserContext* browser_context)
      : delegate_(browser_context) {}
  ~ContentServiceDelegateHolder() override = default;

  ContentServiceDelegateImpl* delegate() { return &delegate_; }

 private:
  ContentServiceDelegateImpl delegate_;

  DISALLOW_COPY_AND_ASSIGN(ContentServiceDelegateHolder);
};

// A class used to make an asynchronous Mojo call with cloned patterns for each
// StoragePartition iteration. |this| instance will be destructed when all
// existing asynchronous Mojo calls made in SetLists() are done, and |closure|
// will be invoked on destructing |this|.
class CorsOriginPatternSetter
    : public base::RefCounted<CorsOriginPatternSetter> {
 public:
  CorsOriginPatternSetter(
      const url::Origin& source_origin,
      std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
      base::OnceClosure closure)
      : source_origin_(source_origin),
        allow_patterns_(std::move(allow_patterns)),
        block_patterns_(std::move(block_patterns)),
        closure_(std::move(closure)) {}

  void SetLists(StoragePartition* partition) {
    partition->GetNetworkContext()->SetCorsOriginAccessListsForOrigin(
        source_origin_, ClonePatterns(allow_patterns_),
        ClonePatterns(block_patterns_),
        base::BindOnce([](scoped_refptr<CorsOriginPatternSetter> setter) {},
                       base::RetainedRef(this)));
  }

 private:
  friend class base::RefCounted<CorsOriginPatternSetter>;

  static std::vector<network::mojom::CorsOriginPatternPtr> ClonePatterns(
      const std::vector<network::mojom::CorsOriginPatternPtr>& patterns) {
    std::vector<network::mojom::CorsOriginPatternPtr> cloned_patterns;
    cloned_patterns.reserve(patterns.size());
    for (const auto& item : patterns)
      cloned_patterns.push_back(item.Clone());
    return cloned_patterns;
  }

  ~CorsOriginPatternSetter() { std::move(closure_).Run(); }

  const url::Origin source_origin_;
  const std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns_;
  const std::vector<network::mojom::CorsOriginPatternPtr> block_patterns_;

  base::OnceClosure closure_;
};

// Key names on BrowserContext.
const char kBrowsingDataRemoverKey[] = "browsing-data-remover";
const char kContentServiceDelegateKey[] = "content-service-delegate";
const char kDownloadManagerKeyName[] = "download_manager";
const char kMojoWasInitialized[] = "mojo-was-initialized";
const char kPermissionControllerKey[] = "permission-controller";
const char kServiceManagerConnection[] = "service-manager-connection";
const char kServiceUserId[] = "service-user-id";
const char kSharedCorsOriginAccessListKey[] = "shared-cors-origin-access-list";
const char kStoragePartitionMapKeyName[] = "content_storage_partition_map";
const char kVideoDecodePerfHistoryId[] = "video-decode-perf-history";

#if defined(OS_CHROMEOS)
const char kMountPointsKey[] = "mount_points";
#endif  // defined(OS_CHROMEOS)

void RemoveBrowserContextFromUserIdMap(BrowserContext* browser_context) {
  ServiceUserIdHolder* holder = static_cast<ServiceUserIdHolder*>(
      browser_context->GetUserData(kServiceUserId));
  if (holder) {
    auto it = g_user_id_to_context.Get().find(holder->user_id());
    if (it != g_user_id_to_context.Get().end())
      g_user_id_to_context.Get().erase(it);
  }
}

StoragePartitionImplMap* GetStoragePartitionMap(
    BrowserContext* browser_context) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStoragePartitionMapKeyName));
  if (!partition_map) {
    auto partition_map_owned =
        std::make_unique<StoragePartitionImplMap>(browser_context);
    partition_map = partition_map_owned.get();
    browser_context->SetUserData(kStoragePartitionMapKeyName,
                                 std::move(partition_map_owned));
  }
  return partition_map;
}

StoragePartition* GetStoragePartitionFromConfig(
    BrowserContext* browser_context,
    const std::string& partition_domain,
    const std::string& partition_name,
    bool in_memory,
    bool can_create) {
  StoragePartitionImplMap* partition_map =
      GetStoragePartitionMap(browser_context);

  if (browser_context->IsOffTheRecord())
    in_memory = true;

  return partition_map->Get(partition_domain, partition_name, in_memory,
                            can_create);
}

void SaveSessionStateOnIOThread(
    const scoped_refptr<net::URLRequestContextGetter>& context_getter,
    AppCacheServiceImpl* appcache_service) {
  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  context->cookie_store()->SetForceKeepSessionState();
  context->channel_id_service()->GetChannelIDStore()->
      SetForceKeepSessionState();
  appcache_service->set_force_keep_session_state();
}

void SaveSessionStateOnIndexedDBThread(
    scoped_refptr<IndexedDBContextImpl> indexed_db_context) {
  indexed_db_context->SetForceKeepSessionState();
}

void ShutdownServiceWorkerContext(StoragePartition* partition) {
  ServiceWorkerContextWrapper* wrapper =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  wrapper->process_manager()->Shutdown();
}

void SetDownloadManager(
    BrowserContext* context,
    std::unique_ptr<content::DownloadManager> download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(download_manager);
  context->SetUserData(kDownloadManagerKeyName, std::move(download_manager));
}

class BrowserContextServiceManagerConnectionHolder
    : public base::SupportsUserData::Data {
 public:
  explicit BrowserContextServiceManagerConnectionHolder(
      service_manager::mojom::ServiceRequest request)
      : service_manager_connection_(ServiceManagerConnection::Create(
            std::move(request),
            base::CreateSingleThreadTaskRunnerWithTraits(
                {BrowserThread::IO}))) {}
  ~BrowserContextServiceManagerConnectionHolder() override {}

  ServiceManagerConnection* service_manager_connection() {
    return service_manager_connection_.get();
  }

 private:
  std::unique_ptr<ServiceManagerConnection> service_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(BrowserContextServiceManagerConnectionHolder);
};

base::WeakPtr<storage::BlobStorageContext> BlobStorageContextGetterForBrowser(
    scoped_refptr<ChromeBlobStorageContext> blob_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return blob_context->context()->AsWeakPtr();
}

}  // namespace

// static
void BrowserContext::AsyncObliterateStoragePartition(
    BrowserContext* browser_context,
    const GURL& site,
    const base::Closure& on_gc_required) {
  GetStoragePartitionMap(browser_context)->AsyncObliterate(site,
                                                           on_gc_required);
}

// static
void BrowserContext::GarbageCollectStoragePartitions(
    BrowserContext* browser_context,
    std::unique_ptr<base::hash_set<base::FilePath>> active_paths,
    const base::Closure& done) {
  GetStoragePartitionMap(browser_context)
      ->GarbageCollect(std::move(active_paths), done);
}

DownloadManager* BrowserContext::GetDownloadManager(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context->GetUserData(kDownloadManagerKeyName)) {
    DownloadManager* download_manager = new DownloadManagerImpl(context);

    SetDownloadManager(context, base::WrapUnique(download_manager));
    download_manager->SetDelegate(context->GetDownloadManagerDelegate());
  }

  return static_cast<DownloadManager*>(
      context->GetUserData(kDownloadManagerKeyName));
}

// static
storage::ExternalMountPoints* BrowserContext::GetMountPoints(
    BrowserContext* context) {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

#if defined(OS_CHROMEOS)
  if (!context->GetUserData(kMountPointsKey)) {
    scoped_refptr<storage::ExternalMountPoints> mount_points =
        storage::ExternalMountPoints::CreateRefCounted();
    context->SetUserData(
        kMountPointsKey,
        std::make_unique<UserDataAdapter<storage::ExternalMountPoints>>(
            mount_points.get()));
  }

  return UserDataAdapter<storage::ExternalMountPoints>::Get(context,
                                                            kMountPointsKey);
#else
  return nullptr;
#endif
}

// static
content::BrowsingDataRemover* content::BrowserContext::GetBrowsingDataRemover(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context->GetUserData(kBrowsingDataRemoverKey)) {
    std::unique_ptr<BrowsingDataRemoverImpl> remover =
        std::make_unique<BrowsingDataRemoverImpl>(context);
    remover->SetEmbedderDelegate(context->GetBrowsingDataRemoverDelegate());
    context->SetUserData(kBrowsingDataRemoverKey, std::move(remover));
  }

  return static_cast<BrowsingDataRemoverImpl*>(
      context->GetUserData(kBrowsingDataRemoverKey));
}

// static
content::PermissionController* content::BrowserContext::GetPermissionController(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context->GetUserData(kPermissionControllerKey)) {
    context->SetUserData(kPermissionControllerKey,
                         std::make_unique<PermissionControllerImpl>(context));
  }

  return static_cast<PermissionControllerImpl*>(
      context->GetUserData(kPermissionControllerKey));
}

StoragePartition* BrowserContext::GetStoragePartition(
    BrowserContext* browser_context,
    SiteInstance* site_instance,
    bool can_create) {
  std::string partition_domain;
  std::string partition_name;
  bool in_memory = false;

  if (site_instance) {
    GetContentClient()->browser()->GetStoragePartitionConfigForSite(
        browser_context, site_instance->GetSiteURL(), true,
        &partition_domain, &partition_name, &in_memory);
  }

  return GetStoragePartitionFromConfig(browser_context, partition_domain,
                                       partition_name, in_memory, can_create);
}

StoragePartition* BrowserContext::GetStoragePartitionForSite(
    BrowserContext* browser_context,
    const GURL& site,
    bool can_create) {
  std::string partition_domain;
  std::string partition_name;
  bool in_memory;

  GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      browser_context, site, true, &partition_domain, &partition_name,
      &in_memory);

  return GetStoragePartitionFromConfig(browser_context, partition_domain,
                                       partition_name, in_memory, can_create);
}

void BrowserContext::ForEachStoragePartition(
    BrowserContext* browser_context,
    const StoragePartitionCallback& callback) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStoragePartitionMapKeyName));
  if (!partition_map)
    return;

  partition_map->ForEach(callback);
}

StoragePartition* BrowserContext::GetDefaultStoragePartition(
    BrowserContext* browser_context) {
  return GetStoragePartition(browser_context, nullptr);
}

// static
void BrowserContext::CreateMemoryBackedBlob(BrowserContext* browser_context,
                                            const char* data,
                                            size_t length,
                                            const std::string& content_type,
                                            BlobCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChromeBlobStorageContext* blob_context =
      ChromeBlobStorageContext::GetFor(browser_context);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ChromeBlobStorageContext::CreateMemoryBackedBlob,
                     base::WrapRefCounted(blob_context), data, length,
                     content_type),
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
blink::mojom::BlobPtr BrowserContext::GetBlobPtr(
    BrowserContext* browser_context,
    const std::string& uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ChromeBlobStorageContext::GetBlobPtr(browser_context, uuid);
}

// static
void BrowserContext::DeliverPushMessage(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    base::Optional<std::string> payload,
    const base::Callback<void(mojom::PushDeliveryStatus)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingRouter::DeliverMessage(browser_context, origin,
                                      service_worker_registration_id,
                                      std::move(payload), callback);
}

// static
void BrowserContext::NotifyWillBeDestroyed(BrowserContext* browser_context) {
  // Make sure NotifyWillBeDestroyed is idempotent.  This helps facilitate the
  // pattern where NotifyWillBeDestroyed is called from *both*
  // ShellBrowserContext and its derived classes (e.g.
  // LayoutTestBrowserContext).
  if (browser_context->was_notify_will_be_destroyed_called_)
    return;
  browser_context->was_notify_will_be_destroyed_called_ = true;

  // Subclasses of BrowserContext may expect there to be no more
  // RenderProcessHosts using them by the time this function returns. We
  // therefore explicitly tear down embedded Content Service instances now to
  // ensure that all their WebContents (and therefore RPHs) are torn down too.
  browser_context->RemoveUserData(kContentServiceDelegateKey);

  // Service Workers must shutdown before the browser context is destroyed,
  // since they keep render process hosts alive and the codebase assumes that
  // render process hosts die before their profile (browser context) dies.
  ForEachStoragePartition(browser_context,
                          base::Bind(ShutdownServiceWorkerContext));

  // Shared workers also keep render process hosts alive, and are expected to
  // return ref counts to 0 after documents close. However, to ensure that
  // hosts are destructed now, forcibly release their ref counts here.
  for (RenderProcessHost::iterator host_iterator =
           RenderProcessHost::AllHostsIterator();
       !host_iterator.IsAtEnd(); host_iterator.Advance()) {
    RenderProcessHost* host = host_iterator.GetCurrentValue();
    if (host->GetBrowserContext() == browser_context) {
      // This will also clean up spare RPH references.
      host->DisableKeepAliveRefCount();
    }
  }
}

void BrowserContext::EnsureResourceContextInitialized(BrowserContext* context) {
  // This will be enough to tickle initialization of BrowserContext if
  // necessary, which initializes ResourceContext. The reason we don't call
  // ResourceContext::InitializeResourceContext() directly here is that
  // ResourceContext initialization may call back into BrowserContext
  // and when that call returns it'll end rewriting its UserData map. It will
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
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &SaveSessionStateOnIOThread,
            base::WrapRefCounted(storage_partition->GetURLRequestContext()),
            static_cast<AppCacheServiceImpl*>(
                storage_partition->GetAppCacheService())));
  }

  storage_partition->GetCookieManagerForBrowserProcess()
      ->SetForceKeepSessionState();

  DOMStorageContextWrapper* dom_storage_context_proxy =
      static_cast<DOMStorageContextWrapper*>(
          storage_partition->GetDOMStorageContext());
  dom_storage_context_proxy->SetForceKeepSessionState();

  IndexedDBContextImpl* indexed_db_context_impl =
      static_cast<IndexedDBContextImpl*>(
        storage_partition->GetIndexedDBContext());
  // No task runner in unit tests.
  if (indexed_db_context_impl->TaskRunner()) {
    indexed_db_context_impl->TaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SaveSessionStateOnIndexedDBThread,
                       base::WrapRefCounted(indexed_db_context_impl)));
  }
}

void BrowserContext::SetDownloadManagerForTesting(
    BrowserContext* browser_context,
    std::unique_ptr<content::DownloadManager> download_manager) {
  SetDownloadManager(browser_context, std::move(download_manager));
}

// static
void BrowserContext::Initialize(
    BrowserContext* browser_context,
    const base::FilePath& path) {
  std::string new_id;
  if (GetContentClient() && GetContentClient()->browser()) {
    new_id = GetContentClient()->browser()->GetServiceUserIdForBrowserContext(
        browser_context);
  } else {
    // Some test scenarios initialize a BrowserContext without a content client.
    new_id = base::GenerateGUID();
  }

  ServiceUserIdHolder* holder = static_cast<ServiceUserIdHolder*>(
      browser_context->GetUserData(kServiceUserId));
  if (holder)
    file::ForgetServiceUserIdUserDirAssociation(holder->user_id());
  file::AssociateServiceUserIdWithUserDir(new_id, path);
  RemoveBrowserContextFromUserIdMap(browser_context);
  g_user_id_to_context.Get()[new_id] = browser_context;
  browser_context->SetUserData(kServiceUserId,
                               std::make_unique<ServiceUserIdHolder>(new_id));

  browser_context->SetUserData(
      kMojoWasInitialized, std::make_unique<base::SupportsUserData::Data>());

  ServiceManagerConnection* service_manager_connection =
      ServiceManagerConnection::GetForProcess();
  if (service_manager_connection && base::ThreadTaskRunnerHandle::IsSet()) {
    // NOTE: Many unit tests create a TestBrowserContext without initializing
    // Mojo or the global service manager connection.

    service_manager::mojom::ServicePtr service;
    auto service_request = mojo::MakeRequest(&service);

    service_manager::mojom::PIDReceiverPtr pid_receiver;
    service_manager::Identity identity(mojom::kBrowserServiceName, new_id);
    service_manager_connection->GetConnector()->StartService(
        identity, std::move(service), mojo::MakeRequest(&pid_receiver));
    pid_receiver->SetPID(base::GetCurrentProcId());

    service_manager_connection->GetConnector()->StartService(identity);
    BrowserContextServiceManagerConnectionHolder* connection_holder =
        new BrowserContextServiceManagerConnectionHolder(
            std::move(service_request));
    browser_context->SetUserData(kServiceManagerConnection,
                                 base::WrapUnique(connection_holder));

    ServiceManagerConnection* connection =
        connection_holder->service_manager_connection();

    // New embedded service factories should be added to |connection| here.

    {
      service_manager::EmbeddedServiceInfo info;
      info.factory = base::BindRepeating(&file::CreateFileService);
      connection->AddEmbeddedService(file::mojom::kServiceName, info);
    }

    browser_context->SetUserData(
        kContentServiceDelegateKey,
        std::make_unique<ContentServiceDelegateHolder>(browser_context));

    {
      service_manager::EmbeddedServiceInfo info;
      info.task_runner = base::SequencedTaskRunnerHandle::Get();
      info.factory = base::BindRepeating(
          [](BrowserContext* context)
              -> std::unique_ptr<service_manager::Service> {
            auto* holder = static_cast<ContentServiceDelegateHolder*>(
                context->GetUserData(kContentServiceDelegateKey));
            auto* delegate = holder->delegate();
            auto service = std::make_unique<content::Service>(delegate);
            delegate->AddService(service.get());
            return service;
          },
          browser_context);
      connection->AddEmbeddedService(content::mojom::kServiceName, info);
    }

    ContentBrowserClient::StaticServiceMap services;
    browser_context->RegisterInProcessServices(&services);
    for (const auto& entry : services) {
      connection->AddEmbeddedService(entry.first, entry.second);
    }

    RegisterCommonBrowserInterfaces(connection);
    connection->Start();
  }
}

// static
const std::string& BrowserContext::GetServiceUserIdFor(
    BrowserContext* browser_context) {
  CHECK(browser_context->GetUserData(kMojoWasInitialized))
      << "Attempting to get the mojo user id for a BrowserContext that was "
      << "never Initialize()ed.";

  ServiceUserIdHolder* holder = static_cast<ServiceUserIdHolder*>(
      browser_context->GetUserData(kServiceUserId));
  return holder->user_id();
}

// static
BrowserContext* BrowserContext::GetBrowserContextForServiceUserId(
    const std::string& user_id) {
  auto it = g_user_id_to_context.Get().find(user_id);
  return it != g_user_id_to_context.Get().end() ? it->second : nullptr;
}

// static
service_manager::Connector* BrowserContext::GetConnectorFor(
    BrowserContext* browser_context) {
  ServiceManagerConnection* connection =
      GetServiceManagerConnectionFor(browser_context);
  return connection ? connection->GetConnector() : nullptr;
}

// static
ServiceManagerConnection* BrowserContext::GetServiceManagerConnectionFor(
    BrowserContext* browser_context) {
  BrowserContextServiceManagerConnectionHolder* connection_holder =
      static_cast<BrowserContextServiceManagerConnectionHolder*>(
          browser_context->GetUserData(kServiceManagerConnection));
  return connection_holder ? connection_holder->service_manager_connection()
                           : nullptr;
}

// static
const SharedCorsOriginAccessList* BrowserContext::GetSharedCorsOriginAccessList(
    BrowserContext* browser_context) {
  return UserDataAdapter<SharedCorsOriginAccessList>::Get(
      browser_context, kSharedCorsOriginAccessListKey);
}

// static
void BrowserContext::SetCorsOriginAccessListsForOrigin(
    BrowserContext* browser_context,
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    UserDataAdapter<SharedCorsOriginAccessList>::Get(
        browser_context, kSharedCorsOriginAccessListKey)
        ->SetForOrigin(source_origin, std::move(allow_patterns),
                       std::move(block_patterns), std::move(closure));
  } else {
    auto setter = base::MakeRefCounted<CorsOriginPatternSetter>(
        source_origin, std::move(allow_patterns), std::move(block_patterns),
        std::move(closure));
    ForEachStoragePartition(
        browser_context, base::BindRepeating(&CorsOriginPatternSetter::SetLists,
                                             base::RetainedRef(setter.get())));
  }
}

BrowserContext::BrowserContext()
    : unique_id_(base::UnguessableToken::Create().ToString()) {
  SetUserData(kSharedCorsOriginAccessListKey,
              std::make_unique<UserDataAdapter<SharedCorsOriginAccessList>>(
                  new SharedCorsOriginAccessListImpl()));
}

BrowserContext::~BrowserContext() {
  CHECK(GetUserData(kMojoWasInitialized))
      << "Attempting to destroy a BrowserContext that never called "
      << "Initialize()";

  DCHECK(!GetUserData(kStoragePartitionMapKeyName))
      << "StoragePartitionMap is not shut down properly";

  DCHECK(was_notify_will_be_destroyed_called_);

  RemoveBrowserContextFromUserIdMap(this);

  if (GetUserData(kDownloadManagerKeyName))
    GetDownloadManager(this)->Shutdown();
}

void BrowserContext::ShutdownStoragePartitions() {
  if (GetUserData(kStoragePartitionMapKeyName))
    RemoveUserData(kStoragePartitionMapKeyName);
}

std::string BrowserContext::GetMediaDeviceIDSalt() {
  return unique_id_;
}

// static
std::string BrowserContext::CreateRandomMediaDeviceIDSalt() {
  return base::UnguessableToken::Create().ToString();
}

const std::string& BrowserContext::UniqueId() const {
  return unique_id_;
}

media::VideoDecodePerfHistory* BrowserContext::GetVideoDecodePerfHistory() {
  media::VideoDecodePerfHistory* decode_history =
      static_cast<media::VideoDecodePerfHistory*>(
          GetUserData(kVideoDecodePerfHistoryId));

  // Lazily created. Note, this does not trigger loading the DB from disk. That
  // occurs later upon first VideoDecodePerfHistory API request that requires DB
  // access. DB operations will not block the UI thread.
  if (!decode_history) {
    std::unique_ptr<media::VideoDecodeStatsDBImpl> stats_db =
        media::VideoDecodeStatsDBImpl::Create(
            GetPath().Append(FILE_PATH_LITERAL("VideoDecodeStats")));
    auto new_decode_history =
        std::make_unique<media::VideoDecodePerfHistory>(std::move(stats_db));
    decode_history = new_decode_history.get();

    SetUserData(kVideoDecodePerfHistoryId, std::move(new_decode_history));
  }

  return decode_history;
}

download::InProgressDownloadManager*
BrowserContext::RetriveInProgressDownloadManager() {
  return nullptr;
}

}  // namespace content
