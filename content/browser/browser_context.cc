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
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/supports_user_data.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browsing_data/browsing_data_remover_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/content_service_delegate_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/media/browser_feature_provider.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/push_messaging/push_messaging_router.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/indexed_db_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "media/base/media_switches.h"
#include "media/capabilities/in_memory_video_decode_stats_db_impl.h"
#include "media/capabilities/video_decode_stats_db_impl.h"
#include "media/learning/common/media_learning_tasks.h"
#include "media/learning/impl/learning_session_impl.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "services/content/public/mojom/constants.mojom.h"
#include "services/content/service.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/file_system/external_mount_points.h"

using base::UserDataAdapter;

namespace content {

namespace {

using TokenToContextMap = std::map<base::Token, BrowserContext*>;
TokenToContextMap& GetTokenToContextMap() {
  static base::NoDestructor<TokenToContextMap> map;
  return *map;
}

class ServiceInstanceGroupHolder : public base::SupportsUserData::Data {
 public:
  explicit ServiceInstanceGroupHolder(const base::Token& instance_group)
      : instance_group_(instance_group) {}
  ~ServiceInstanceGroupHolder() override {}

  const base::Token& instance_group() const { return instance_group_; }

 private:
  base::Token instance_group_;

  DISALLOW_COPY_AND_ASSIGN(ServiceInstanceGroupHolder);
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

// Key names on BrowserContext.
const char kBrowsingDataRemoverKey[] = "browsing-data-remover";
const char kContentServiceDelegateKey[] = "content-service-delegate";
const char kDownloadManagerKeyName[] = "download_manager";
const char kPermissionControllerKey[] = "permission-controller";
const char kServiceManagerConnection[] = "service-manager-connection";
const char kServiceInstanceGroup[] = "service-instance-group";
const char kStoragePartitionMapKeyName[] = "content_storage_partition_map";
const char kVideoDecodePerfHistoryId[] = "video-decode-perf-history";
const char kLearningSession[] = "learning-session";

#if defined(OS_CHROMEOS)
const char kMountPointsKey[] = "mount_points";
#endif  // defined(OS_CHROMEOS)

void RemoveBrowserContextFromInstanceGroupMap(BrowserContext* browser_context) {
  ServiceInstanceGroupHolder* holder = static_cast<ServiceInstanceGroupHolder*>(
      browser_context->GetUserData(kServiceInstanceGroup));
  if (holder) {
    auto it = GetTokenToContextMap().find(holder->instance_group());
    if (it != GetTokenToContextMap().end())
      GetTokenToContextMap().erase(it);
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

void SaveSessionStateOnIOThread(AppCacheServiceImpl* appcache_service) {
  appcache_service->set_force_keep_session_state();
}

void SaveSessionStateOnIndexedDBThread(
    scoped_refptr<IndexedDBContext> indexed_db_context) {
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

std::unique_ptr<service_manager::Service>
CreateMainThreadServiceForBrowserContext(
    BrowserContext* browser_context,
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  if (service_name == content::mojom::kServiceName) {
    auto* delegate_holder = static_cast<ContentServiceDelegateHolder*>(
        browser_context->GetUserData(kContentServiceDelegateKey));
    auto* delegate = delegate_holder->delegate();
    auto service =
        std::make_unique<content::Service>(delegate, std::move(request));
    delegate->AddService(service.get());
    return service;
  }

  return browser_context->HandleServiceRequest(service_name,
                                               std::move(request));
}

class BrowserContextServiceManagerConnectionHolder
    : public base::SupportsUserData::Data {
 public:
  explicit BrowserContextServiceManagerConnectionHolder(
      BrowserContext* browser_context,
      service_manager::mojom::ServiceRequest request)
      : browser_context_(browser_context),
        service_manager_connection_(ServiceManagerConnection::Create(
            std::move(request),
            base::CreateSingleThreadTaskRunner({BrowserThread::IO}))) {
    service_manager_connection_->SetDefaultServiceRequestHandler(
        base::BindRepeating(
            &BrowserContextServiceManagerConnectionHolder::OnServiceRequest,
            weak_ptr_factory_.GetWeakPtr()));
  }
  ~BrowserContextServiceManagerConnectionHolder() override {}

  ServiceManagerConnection* service_manager_connection() {
    return service_manager_connection_.get();
  }

  void DestroyRunningServices() { running_services_.clear(); }

 private:
  void OnServiceRequest(const std::string& service_name,
                        service_manager::mojom::ServiceRequest request) {
    std::unique_ptr<service_manager::Service> service =
        CreateMainThreadServiceForBrowserContext(browser_context_, service_name,
                                                 std::move(request));
    if (!service) {
      LOG(ERROR) << "Ignoring request for unknown per-browser-context service:"
                 << service_name;
      return;
    }

    auto* raw_service = service.get();
    service->set_termination_closure(base::BindOnce(
        &BrowserContextServiceManagerConnectionHolder::OnServiceQuit,
        base::Unretained(this), raw_service));
    running_services_.emplace(raw_service, std::move(service));
  }

  void OnServiceQuit(service_manager::Service* service) {
    running_services_.erase(service);
  }

  BrowserContext* const browser_context_;
  std::unique_ptr<ServiceManagerConnection> service_manager_connection_;
  std::map<service_manager::Service*, std::unique_ptr<service_manager::Service>>
      running_services_;

  base::WeakPtrFactory<BrowserContextServiceManagerConnectionHolder>
      weak_ptr_factory_{this};

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
  GetStoragePartitionMap(browser_context)
      ->AsyncObliterate(site, on_gc_required);
}

// static
void BrowserContext::GarbageCollectStoragePartitions(
    BrowserContext* browser_context,
    std::unique_ptr<std::unordered_set<base::FilePath>> active_paths,
    const base::Closure& done) {
  GetStoragePartitionMap(browser_context)
      ->GarbageCollect(std::move(active_paths), done);
}

DownloadManager* BrowserContext::GetDownloadManager(BrowserContext* context) {
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
        browser_context, site_instance->GetSiteURL(), true, &partition_domain,
        &partition_name, &in_memory);
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
                                            base::span<const uint8_t> data,
                                            const std::string& content_type,
                                            BlobCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChromeBlobStorageContext* blob_context =
      ChromeBlobStorageContext::GetFor(browser_context);
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO},
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
    const base::Callback<void(blink::mojom::PushDeliveryStatus)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingRouter::DeliverMessage(browser_context, origin,
                                      service_worker_registration_id,
                                      message_id, std::move(payload), callback);
}

// static
void BrowserContext::NotifyWillBeDestroyed(BrowserContext* browser_context) {
  // Make sure NotifyWillBeDestroyed is idempotent.  This helps facilitate the
  // pattern where NotifyWillBeDestroyed is called from *both*
  // ShellBrowserContext and its derived classes (e.g. WebTestBrowserContext).
  if (browser_context->was_notify_will_be_destroyed_called_)
    return;
  browser_context->was_notify_will_be_destroyed_called_ = true;

  // Stop the ServiceManagerConnection from handling any new incoming requests
  // before we tear anything down. This prevents races at shutdown.
  BrowserContextServiceManagerConnectionHolder* connection_holder =
      static_cast<BrowserContextServiceManagerConnectionHolder*>(
          browser_context->GetUserData(kServiceManagerConnection));
  if (connection_holder)
    connection_holder->service_manager_connection()->Stop();

  // Subclasses of BrowserContext may expect there to be no more
  // RenderProcessHosts using them by the time this function returns. We
  // therefore explicitly tear down embedded Content Service instances now to
  // ensure that all their WebContents (and therefore RPHs) are torn down too.
  browser_context->RemoveUserData(kContentServiceDelegateKey);

  // Tear down all running service instances which were started on behalf of
  // this BrowserContext. Note that we leave the UserData itself in place
  // because it's possible for someone to call
  // |GetServiceManagerConnectionFor()| between now and actual BrowserContext
  // destruction.
  if (connection_holder)
    connection_holder->DestroyRunningServices();

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

  // Clean up any isolated origins associated with this BrowserContext.  This
  // should be safe now that all RenderProcessHosts are destroyed, since future
  // navigations or security decisions shouldn't ever need to consult these
  // isolated origins.
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  policy->RemoveIsolatedOriginsForBrowserContext(*browser_context);
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
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&SaveSessionStateOnIOThread,
                       static_cast<AppCacheServiceImpl*>(
                           storage_partition->GetAppCacheService())));
  }

  storage_partition->GetCookieManagerForBrowserProcess()
      ->SetForceKeepSessionState();

  DOMStorageContextWrapper* dom_storage_context_proxy =
      static_cast<DOMStorageContextWrapper*>(
          storage_partition->GetDOMStorageContext());
  dom_storage_context_proxy->SetForceKeepSessionState();

  scoped_refptr<IndexedDBContext> indexed_db_context =
      storage_partition->GetIndexedDBContext();
  IndexedDBContext* const indexed_db_context_ptr = indexed_db_context.get();
  indexed_db_context_ptr->TaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SaveSessionStateOnIndexedDBThread,
                                std::move(indexed_db_context)));
}

void BrowserContext::SetDownloadManagerForTesting(
    BrowserContext* browser_context,
    std::unique_ptr<content::DownloadManager> download_manager) {
  SetDownloadManager(browser_context, std::move(download_manager));
}

// static
void BrowserContext::Initialize(BrowserContext* browser_context,
                                const base::FilePath& path) {
  const base::Token new_group = base::Token::CreateRandom();
  RemoveBrowserContextFromInstanceGroupMap(browser_context);
  GetTokenToContextMap()[new_group] = browser_context;
  browser_context->SetUserData(
      kServiceInstanceGroup,
      std::make_unique<ServiceInstanceGroupHolder>(new_group));

  auto* system_connector = GetSystemConnector();
  if (system_connector && base::ThreadTaskRunnerHandle::IsSet()) {
    // NOTE: Many unit tests create a TestBrowserContext without initializing
    // Mojo or the global service manager connection.

    mojo::PendingRemote<service_manager::mojom::Service> service;
    auto service_receiver = service.InitWithNewPipeAndPassReceiver();

    mojo::Remote<service_manager::mojom::ProcessMetadata> metadata;
    service_manager::Identity identity(mojom::kBrowserServiceName, new_group,
                                       base::Token{},
                                       base::Token::CreateRandom());
    system_connector->RegisterServiceInstance(
        identity, std::move(service), metadata.BindNewPipeAndPassReceiver());
    metadata->SetPID(base::GetCurrentProcId());

    BrowserContextServiceManagerConnectionHolder* connection_holder =
        new BrowserContextServiceManagerConnectionHolder(
            browser_context, std::move(service_receiver));
    browser_context->SetUserData(kServiceManagerConnection,
                                 base::WrapUnique(connection_holder));
    ServiceManagerConnection* connection =
        connection_holder->service_manager_connection();

    browser_context->SetUserData(
        kContentServiceDelegateKey,
        std::make_unique<ContentServiceDelegateHolder>(browser_context));

    connection->Start();
  }
}

// static
const base::Token& BrowserContext::GetServiceInstanceGroupFor(
    BrowserContext* browser_context) {
  ServiceInstanceGroupHolder* holder = static_cast<ServiceInstanceGroupHolder*>(
      browser_context->GetUserData(kServiceInstanceGroup));
  CHECK(holder) << "Attempting to get the instance group for a BrowserContext "
                << "that was never Initialized().";
  return holder->instance_group();
}

// static
BrowserContext* BrowserContext::GetBrowserContextForServiceInstanceGroup(
    const base::Token& instance_group) {
  auto it = GetTokenToContextMap().find(instance_group);
  return it != GetTokenToContextMap().end() ? it->second : nullptr;
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

BrowserContext::BrowserContext()
    : unique_id_(base::UnguessableToken::Create().ToString()) {}

BrowserContext::~BrowserContext() {
  CHECK(GetUserData(kServiceInstanceGroup))
      << "Attempting to destroy a BrowserContext that never called "
      << "Initialize()";

  DCHECK(!GetUserData(kStoragePartitionMapKeyName))
      << "StoragePartitionMap is not shut down properly";

  DCHECK(was_notify_will_be_destroyed_called_);

  RemoveBrowserContextFromInstanceGroupMap(this);

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

std::unique_ptr<service_manager::Service> BrowserContext::HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  return nullptr;
}

const std::string& BrowserContext::UniqueId() {
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
    const char kUseInMemoryDBParamName[] = "db_in_memory";
    const bool kUseInMemoryDBDefault = false;
    bool use_in_memory_db = base::GetFieldTrialParamByFeatureAsBool(
        media::kMediaCapabilitiesWithParameters, kUseInMemoryDBParamName,
        kUseInMemoryDBDefault);

    std::unique_ptr<media::VideoDecodeStatsDB> stats_db;
    if (use_in_memory_db) {
      stats_db =
          std::make_unique<media::InMemoryVideoDecodeStatsDBImpl>(nullptr);
    } else {
      auto* db_provider =
          GetDefaultStoragePartition(this)->GetProtoDatabaseProvider();

      stats_db = media::VideoDecodeStatsDBImpl::Create(
          GetPath().Append(FILE_PATH_LITERAL("VideoDecodeStats")), db_provider);
    }

    auto new_decode_history = std::make_unique<media::VideoDecodePerfHistory>(
        std::move(stats_db), BrowserFeatureProvider::GetFactoryCB());
    decode_history = new_decode_history.get();

    SetUserData(kVideoDecodePerfHistoryId, std::move(new_decode_history));
  }

  return decode_history;
}

media::learning::LearningSession* BrowserContext::GetLearningSession() {
  media::learning::LearningSession* learning_session =
      static_cast<media::learning::LearningSession*>(
          GetUserData(kLearningSession));

  if (!learning_session) {
    auto new_learning_session =
        std::make_unique<media::learning::LearningSessionImpl>(
            base::SequencedTaskRunnerHandle::Get());

    // Register all the LearningTasks.
    auto cb = base::BindRepeating(
        [](media::learning::LearningSessionImpl* session,
           const media::learning::LearningTask& task) {
          session->RegisterTask(task);
        },
        new_learning_session.get());
    media::learning::MediaLearningTasks::Register(std::move(cb));

    learning_session = new_learning_session.get();

    SetUserData(kLearningSession, std::move(new_learning_session));
  }

  return learning_session;
}

download::InProgressDownloadManager*
BrowserContext::RetriveInProgressDownloadManager() {
  return nullptr;
}

void BrowserContext::SetCorsOriginAccessListForOrigin(
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure) {
  NOTREACHED() << "Sub-classes should implement this method to communicate "
                  "with NetworkService to bypass CORS checks.";
}

SharedCorsOriginAccessList* BrowserContext::GetSharedCorsOriginAccessList() {
  // Need to return a valid instance regardless of CORS bypass supports.
  static const base::NoDestructor<scoped_refptr<SharedCorsOriginAccessList>>
      empty_list(SharedCorsOriginAccessList::Create());
  return empty_list->get();
}

bool BrowserContext::ShouldEnableOutOfBlinkCors() {
  return base::FeatureList::IsEnabled(network::features::kOutOfBlinkCors);
}

NativeFileSystemPermissionContext*
BrowserContext::GetNativeFileSystemPermissionContext() {
  return nullptr;
}

ContentIndexProvider* BrowserContext::GetContentIndexProvider() {
  return nullptr;
}

}  // namespace content
