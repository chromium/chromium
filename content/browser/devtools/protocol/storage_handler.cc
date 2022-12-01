// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/storage_handler.h"

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/scoped_observation.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/browser/devtools/protocol/browser_handler.h"
#include "content/browser/devtools/protocol/handler_helpers.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/storage.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace protocol {

using ClearCookiesCallback = Storage::Backend::ClearCookiesCallback;
using GetCookiesCallback = Storage::Backend::GetCookiesCallback;
using SetCookiesCallback = Storage::Backend::SetCookiesCallback;

struct UsageListInitializer {
  const char* type;
  int64_t blink::mojom::UsageBreakdown::*usage_member;
};

UsageListInitializer initializers[] = {
    {Storage::StorageTypeEnum::File_systems,
     &blink::mojom::UsageBreakdown::fileSystem},
    {Storage::StorageTypeEnum::Websql, &blink::mojom::UsageBreakdown::webSql},
    {Storage::StorageTypeEnum::Indexeddb,
     &blink::mojom::UsageBreakdown::indexedDatabase},
    {Storage::StorageTypeEnum::Cache_storage,
     &blink::mojom::UsageBreakdown::serviceWorkerCache},
    {Storage::StorageTypeEnum::Service_workers,
     &blink::mojom::UsageBreakdown::serviceWorker},
};

namespace {

void ReportUsageAndQuotaDataOnUIThread(
    std::unique_ptr<StorageHandler::GetUsageAndQuotaCallback> callback,
    blink::mojom::QuotaStatusCode code,
    int64_t usage,
    int64_t quota,
    bool is_override_enabled,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (code != blink::mojom::QuotaStatusCode::kOk) {
    return callback->sendFailure(
        Response::ServerError("Quota information is not available"));
  }

  auto usageList = std::make_unique<Array<Storage::UsageForType>>();

  blink::mojom::UsageBreakdown* breakdown_ptr = usage_breakdown.get();
  for (const auto initializer : initializers) {
    std::unique_ptr<Storage::UsageForType> entry =
        Storage::UsageForType::Create()
            .SetStorageType(initializer.type)
            .SetUsage(breakdown_ptr->*(initializer.usage_member))
            .Build();
    usageList->emplace_back(std::move(entry));
  }

  callback->sendSuccess(usage, quota, is_override_enabled,
                        std::move(usageList));
}

void GotUsageAndQuotaDataCallback(
    std::unique_ptr<StorageHandler::GetUsageAndQuotaCallback> callback,
    blink::mojom::QuotaStatusCode code,
    int64_t usage,
    int64_t quota,
    bool is_override_enabled,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(ReportUsageAndQuotaDataOnUIThread, std::move(callback),
                     code, usage, quota, is_override_enabled,
                     std::move(usage_breakdown)));
}

void GetUsageAndQuotaOnIOThread(
    storage::QuotaManager* manager,
    const blink::StorageKey& storage_key,
    std::unique_ptr<StorageHandler::GetUsageAndQuotaCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  manager->GetUsageAndQuotaForDevtools(
      storage_key, blink::mojom::StorageType::kTemporary,
      base::BindOnce(&GotUsageAndQuotaDataCallback, std::move(callback)));
}

}  // namespace

// Observer that listens on the UI thread for cache storage notifications and
// informs the StorageHandler on the UI thread for origins of interest.
// Created and used exclusively on the UI thread.
class StorageHandler::CacheStorageObserver
    : storage::mojom::CacheStorageObserver {
 public:
  CacheStorageObserver(
      base::WeakPtr<StorageHandler> owner_storage_handler,
      mojo::PendingReceiver<storage::mojom::CacheStorageObserver> observer)
      : owner_(owner_storage_handler), receiver_(this, std::move(observer)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  CacheStorageObserver(const CacheStorageObserver&) = delete;
  CacheStorageObserver& operator=(const CacheStorageObserver&) = delete;

  ~CacheStorageObserver() override { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

  void TrackStorageKey(const blink::StorageKey& storage_key) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (storage_keys_.find(storage_key) != storage_keys_.end())
      return;
    storage_keys_.insert(storage_key);
  }

  void UntrackStorageKey(const blink::StorageKey& storage_key) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    storage_keys_.erase(storage_key);
  }

  void OnCacheListChanged(const blink::StorageKey& storage_key) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto found = storage_keys_.find(storage_key);
    if (found == storage_keys_.end())
      return;
    owner_->NotifyCacheStorageListChanged(storage_key);
  }

  void OnCacheContentChanged(const blink::StorageKey& storage_key,
                             const std::string& cache_name) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (storage_keys_.find(storage_key) == storage_keys_.end())
      return;
    owner_->NotifyCacheStorageContentChanged(storage_key, cache_name);
  }

 private:
  // Maintained on the IO thread to avoid thread contention.
  base::flat_set<blink::StorageKey> storage_keys_;

  base::WeakPtr<StorageHandler> owner_;
  mojo::Receiver<storage::mojom::CacheStorageObserver> receiver_;
};

// Observer that listens on the IDB thread for IndexedDB notifications and
// informs the StorageHandler on the UI thread for storage_keys of interest.
// Created on the UI thread but predominantly used and deleted on the IDB
// thread.
class StorageHandler::IndexedDBObserver
    : public storage::mojom::IndexedDBObserver {
 public:
  explicit IndexedDBObserver(
      base::WeakPtr<StorageHandler> owner_storage_handler)
      : owner_(owner_storage_handler), receiver_(this) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    ReconnectObserver();
  }

  IndexedDBObserver(const IndexedDBObserver&) = delete;
  IndexedDBObserver& operator=(const IndexedDBObserver&) = delete;

  ~IndexedDBObserver() override { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

  void TrackOrigin(const blink::StorageKey& storage_key) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (storage_keys_.find(storage_key) != storage_keys_.end())
      return;
    storage_keys_.insert(storage_key);
  }

  void UntrackOrigin(const blink::StorageKey& storage_key) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    storage_keys_.erase(storage_key);
  }

  void OnIndexedDBListChanged(
      const storage::BucketLocator& bucket_locator) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!owner_)
      return;
    // TODO(crbug.com/1315371): Allow custom bucket names.
    auto found = storage_keys_.find(bucket_locator.storage_key);
    if (found == storage_keys_.end())
      return;

    owner_->NotifyIndexedDBListChanged(
        bucket_locator.storage_key.origin().Serialize(),
        bucket_locator.storage_key.Serialize());
  }

  void OnIndexedDBContentChanged(
      const storage::BucketLocator& bucket_locator,
      const std::u16string& database_name,
      const std::u16string& object_store_name) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!owner_)
      return;
    // TODO(crbug.com/1315371): Allow custom bucket names.
    auto found = storage_keys_.find(bucket_locator.storage_key);
    if (found == storage_keys_.end())
      return;

    owner_->NotifyIndexedDBContentChanged(
        bucket_locator.storage_key.origin().Serialize(),
        bucket_locator.storage_key.Serialize(), database_name,
        object_store_name);
  }

 private:
  void ReconnectObserver() {
    DCHECK(!receiver_.is_bound());
    if (!owner_)
      return;

    auto& control = owner_->storage_partition_->GetIndexedDBControl();
    mojo::PendingRemote<storage::mojom::IndexedDBObserver> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    receiver_.set_disconnect_handler(base::BindOnce(
        [](IndexedDBObserver* observer) {
          // If this observer disconnects because IndexedDB or the storage
          // service goes away, reconnect again.
          observer->ReconnectObserver();
        },
        this));
    control.AddObserver(std::move(remote));
  }

  base::flat_set<blink::StorageKey> storage_keys_;
  base::WeakPtr<StorageHandler> owner_;
  mojo::Receiver<storage::mojom::IndexedDBObserver> receiver_;
};

// Observer that listens on the UI thread for shared storage notifications and
// informs the StorageHandler on the UI thread for origins of interest.
// Created and used exclusively on the UI thread.
class StorageHandler::SharedStorageObserver
    : content::SharedStorageWorkletHostManager::SharedStorageObserverInterface {
 public:
  explicit SharedStorageObserver(StorageHandler* owner_storage_handler)
      : owner_(owner_storage_handler) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto* manager = owner_->GetSharedStorageWorkletHostManager();
    DCHECK(manager);
    scoped_observation_.Observe(manager);
  }

  SharedStorageObserver(const SharedStorageObserver&) = delete;
  SharedStorageObserver& operator=(const SharedStorageObserver&) = delete;

  ~SharedStorageObserver() override { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

  // content::SharedStorageObserverInterface
  void OnSharedStorageAccessed(
      const base::Time& access_time,
      AccessType type,
      const std::string& main_frame_id,
      const std::string& owner_origin,
      const SharedStorageEventParams& params) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    owner_->NotifySharedStorageAccessed(access_time, type, main_frame_id,
                                        owner_origin, params);
  }

 private:
  raw_ptr<StorageHandler> const owner_;
  base::ScopedObservation<
      content::SharedStorageWorkletHostManager,
      content::SharedStorageWorkletHostManager::SharedStorageObserverInterface>
      scoped_observation_{this};
};

StorageHandler::StorageHandler(bool client_is_trusted)
    : DevToolsDomainHandler(Storage::Metainfo::domainName),
      client_is_trusted_(client_is_trusted) {}

StorageHandler::~StorageHandler() {
  DCHECK(!cache_storage_observer_);
  DCHECK(!indexed_db_observer_);
}

void StorageHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Storage::Frontend>(dispatcher->channel());
  Storage::Dispatcher::wire(dispatcher, this);
}

void StorageHandler::SetRenderer(int process_host_id,
                                 RenderFrameHostImpl* frame_host) {
  RenderProcessHost* process = RenderProcessHost::FromID(process_host_id);
  storage_partition_ = process ? process->GetStoragePartition() : nullptr;
  frame_host_ = frame_host;
}

Response StorageHandler::Disable() {
  cache_storage_observer_.reset();
  indexed_db_observer_.reset();
  quota_override_handle_.reset();
  SetInterestGroupTracking(false);
  shared_storage_observer_.reset();
  return Response::Success();
}

void StorageHandler::GetCookies(Maybe<std::string> browser_context_id,
                                std::unique_ptr<GetCookiesCallback> callback) {
  if (!client_is_trusted_)
    callback->sendFailure(Response::ServerError("Permission denied"));
  StoragePartition* storage_partition = nullptr;
  Response response = StorageHandler::FindStoragePartition(browser_context_id,
                                                           &storage_partition);
  if (!response.IsSuccess()) {
    callback->sendFailure(std::move(response));
    return;
  }

  storage_partition->GetCookieManagerForBrowserProcess()->GetAllCookies(
      base::BindOnce(
          [](std::unique_ptr<GetCookiesCallback> callback,
             const std::vector<net::CanonicalCookie>& cookies) {
            callback->sendSuccess(NetworkHandler::BuildCookieArray(cookies));
          },
          std::move(callback)));
}

void StorageHandler::SetCookies(
    std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
    Maybe<std::string> browser_context_id,
    std::unique_ptr<SetCookiesCallback> callback) {
  StoragePartition* storage_partition = nullptr;
  Response response = StorageHandler::FindStoragePartition(browser_context_id,
                                                           &storage_partition);
  if (!response.IsSuccess()) {
    callback->sendFailure(std::move(response));
    return;
  }

  NetworkHandler::SetCookies(
      storage_partition, std::move(cookies),
      base::BindOnce(
          [](std::unique_ptr<SetCookiesCallback> callback, bool success) {
            if (success) {
              callback->sendSuccess();
            } else {
              callback->sendFailure(
                  Response::InvalidParams("Invalid cookie fields"));
            }
          },
          std::move(callback)));
}

void StorageHandler::ClearCookies(
    Maybe<std::string> browser_context_id,
    std::unique_ptr<ClearCookiesCallback> callback) {
  StoragePartition* storage_partition = nullptr;
  Response response = StorageHandler::FindStoragePartition(browser_context_id,
                                                           &storage_partition);
  if (!response.IsSuccess()) {
    callback->sendFailure(std::move(response));
    return;
  }

  storage_partition->GetCookieManagerForBrowserProcess()->DeleteCookies(
      network::mojom::CookieDeletionFilter::New(),
      base::BindOnce([](std::unique_ptr<ClearCookiesCallback> callback,
                        uint32_t) { callback->sendSuccess(); },
                     std::move(callback)));
}

Response StorageHandler::GetStorageKeyForFrame(
    const std::string& frame_id,
    std::string* serialized_storage_key) {
  if (!frame_host_)
    return Response::InvalidParams("Frame host not found");
  FrameTreeNode* node = protocol::FrameTreeNodeFromDevToolsFrameToken(
      frame_host_->frame_tree_node(), frame_id);
  if (!node)
    return Response::InvalidParams("Frame tree node for given frame not found");
  RenderFrameHostImpl* rfh = node->current_frame_host();
  if (rfh->storage_key().origin().opaque())
    return Response::ServerError(
        "Frame corresponds to an opaque origin and its storage key cannot be "
        "serialized");
  *serialized_storage_key = rfh->storage_key().Serialize();
  return Response::Success();
}

namespace {
uint32_t GetRemoveDataMask(const std::string& storage_types) {
  std::vector<std::string> types = base::SplitString(
      storage_types, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::unordered_set<std::string> set(types.begin(), types.end());
  uint32_t remove_mask = 0;
  if (set.count(Storage::StorageTypeEnum::Cookies))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_COOKIES;
  if (set.count(Storage::StorageTypeEnum::File_systems))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS;
  if (set.count(Storage::StorageTypeEnum::Indexeddb))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  if (set.count(Storage::StorageTypeEnum::Local_storage))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  if (set.count(Storage::StorageTypeEnum::Shader_cache))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE;
  if (set.count(Storage::StorageTypeEnum::Websql))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_WEBSQL;
  if (set.count(Storage::StorageTypeEnum::Service_workers))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  if (set.count(Storage::StorageTypeEnum::Cache_storage))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE;
  if (set.count(Storage::StorageTypeEnum::Interest_groups))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_INTEREST_GROUPS;
  if (set.count(Storage::StorageTypeEnum::Shared_storage))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SHARED_STORAGE;
  if (set.count(Storage::StorageTypeEnum::All))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_ALL;
  return remove_mask;
}
}  // namespace

void StorageHandler::ClearDataForOrigin(
    const std::string& origin,
    const std::string& storage_types,
    std::unique_ptr<ClearDataForOriginCallback> callback) {
  if (!storage_partition_)
    return callback->sendFailure(Response::InternalError());

  uint32_t remove_mask = GetRemoveDataMask(storage_types);

  if (!remove_mask) {
    return callback->sendFailure(
        Response::InvalidParams("No valid storage type specified"));
  }

  storage_partition_->ClearData(
      remove_mask, StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      blink::StorageKey(url::Origin::Create(GURL(origin))), base::Time(),
      base::Time::Max(),
      base::BindOnce(&ClearDataForOriginCallback::sendSuccess,
                     std::move(callback)));
}

void StorageHandler::ClearDataForStorageKey(
    const std::string& storage_key,
    const std::string& storage_types,
    std::unique_ptr<ClearDataForStorageKeyCallback> callback) {
  if (!storage_partition_)
    return callback->sendFailure(Response::InternalError());

  uint32_t remove_mask = GetRemoveDataMask(storage_types);

  if (!remove_mask) {
    return callback->sendFailure(
        Response::InvalidParams("No valid storage type specified"));
  }

  absl::optional<blink::StorageKey> key =
      blink::StorageKey::Deserialize(storage_key);
  storage_partition_->ClearData(
      remove_mask, StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, *key,
      base::Time(), base::Time::Max(),
      base::BindOnce(&ClearDataForStorageKeyCallback::sendSuccess,
                     std::move(callback)));
}

void StorageHandler::GetUsageAndQuota(
    const String& origin_string,
    std::unique_ptr<GetUsageAndQuotaCallback> callback) {
  if (!storage_partition_)
    return callback->sendFailure(Response::InternalError());

  GURL origin_url(origin_string);
  url::Origin origin = url::Origin::Create(origin_url);
  if (!origin_url.is_valid() || origin.opaque()) {
    return callback->sendFailure(
        Response::ServerError(origin_string + " is not a valid URL"));
  }

  storage::QuotaManager* manager = storage_partition_->GetQuotaManager();
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetUsageAndQuotaOnIOThread, base::RetainedRef(manager),
                     blink::StorageKey(origin), std::move(callback)));
}

void StorageHandler::OverrideQuotaForOrigin(
    const String& origin_string,
    Maybe<double> quota_size,
    std::unique_ptr<OverrideQuotaForOriginCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  GURL url(origin_string);
  url::Origin origin = url::Origin::Create(url);
  if (!url.is_valid() || origin.opaque()) {
    callback->sendFailure(
        Response::InvalidParams(origin_string + " is not a valid URL"));
    return;
  }

  if (!quota_override_handle_) {
    scoped_refptr<storage::QuotaManagerProxy> manager_proxy =
        storage_partition_->GetQuotaManager()->proxy();
    quota_override_handle_ = manager_proxy->GetQuotaOverrideHandle();
  }

  quota_override_handle_->OverrideQuotaForStorageKey(
      blink::StorageKey(origin),
      quota_size.isJust() ? absl::make_optional(quota_size.fromJust())
                          : absl::nullopt,
      base::BindOnce(&OverrideQuotaForOriginCallback::sendSuccess,
                     std::move(callback)));
}

// TODO(https://crbug.com/1199077): We should think about how this function
// should be exposed when migrating to storage keys.
Response StorageHandler::TrackCacheStorageForOrigin(
    const std::string& origin_string) {
  if (!storage_partition_)
    return Response::InternalError();

  GURL origin_url(origin_string);
  url::Origin origin = url::Origin::Create(origin_url);
  if (!origin_url.is_valid() || origin.opaque())
    return Response::InvalidParams(origin_string + " is not a valid URL");

  GetCacheStorageObserver()->TrackStorageKey(blink::StorageKey(origin));
  return Response::Success();
}

Response StorageHandler::TrackCacheStorageForStorageKey(
    const std::string& storage_key) {
  if (!storage_partition_)
    return Response::InternalError();

  absl::optional<blink::StorageKey> key =
      blink::StorageKey::Deserialize(storage_key);
  if (!key)
    return Response::InvalidParams("Unable to deserialize storage key");

  GetCacheStorageObserver()->TrackStorageKey(*key);
  return Response::Success();
}

// TODO(https://crbug.com/1199077): We should think about how this function
// should be exposed when migrating to storage keys.
Response StorageHandler::UntrackCacheStorageForOrigin(
    const std::string& origin_string) {
  if (!storage_partition_)
    return Response::InternalError();

  GURL origin_url(origin_string);
  url::Origin origin = url::Origin::Create(origin_url);
  if (!origin_url.is_valid() || origin.opaque())
    return Response::InvalidParams(origin_string + " is not a valid URL");

  GetCacheStorageObserver()->UntrackStorageKey(blink::StorageKey(origin));
  return Response::Success();
}

Response StorageHandler::UntrackCacheStorageForStorageKey(
    const std::string& storage_key) {
  if (!storage_partition_)
    return Response::InternalError();

  absl::optional<blink::StorageKey> key =
      blink::StorageKey::Deserialize(storage_key);
  if (!key)
    return Response::InvalidParams("Unable to deserialize storage key");

  GetCacheStorageObserver()->UntrackStorageKey(*key);
  return Response::Success();
}

Response StorageHandler::TrackIndexedDBForOrigin(
    const std::string& origin_string) {
  if (!storage_partition_)
    return Response::InternalError();

  GURL origin_url(origin_string);
  url::Origin origin = url::Origin::Create(origin_url);
  if (!origin_url.is_valid() || origin.opaque())
    return Response::InvalidParams(origin_string + " is not a valid URL");

  // TODO(https://crbug.com/1199077): Pass the real StorageKey into this
  // function once the Chrome DevTools Protocol (CDP) supports StorageKey.
  GetIndexedDBObserver()->TrackOrigin(blink::StorageKey(origin));
  return Response::Success();
}

Response StorageHandler::TrackIndexedDBForStorageKey(
    const std::string& storage_key) {
  if (!storage_partition_)
    return Response::InternalError();

  absl::optional<blink::StorageKey> key =
      blink::StorageKey::Deserialize(storage_key);
  if (!key)
    return Response::InvalidParams("Unable to deserialize storage key");

  GetIndexedDBObserver()->TrackOrigin(*key);
  return Response::Success();
}

Response StorageHandler::UntrackIndexedDBForOrigin(
    const std::string& origin_string) {
  if (!storage_partition_)
    return Response::InternalError();

  GURL origin_url(origin_string);
  url::Origin origin = url::Origin::Create(origin_url);
  if (!origin_url.is_valid() || origin.opaque())
    return Response::InvalidParams(origin_string + " is not a valid URL");

  // TODO(https://crbug.com/1199077): Pass the real StorageKey into this
  // function once the Chrome DevTools Protocol (CDP) supports StorageKey.
  GetIndexedDBObserver()->UntrackOrigin(blink::StorageKey(origin));
  return Response::Success();
}

Response StorageHandler::UntrackIndexedDBForStorageKey(
    const std::string& storage_key) {
  if (!storage_partition_)
    return Response::InternalError();

  absl::optional<blink::StorageKey> key =
      blink::StorageKey::Deserialize(storage_key);
  if (!key)
    return Response::InvalidParams("Unable to deserialize storage key");

  GetIndexedDBObserver()->UntrackOrigin(*key);
  return Response::Success();
}

StorageHandler::CacheStorageObserver*
StorageHandler::GetCacheStorageObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!cache_storage_observer_) {
    mojo::PendingRemote<storage::mojom::CacheStorageObserver> observer;
    cache_storage_observer_ = std::make_unique<CacheStorageObserver>(
        weak_ptr_factory_.GetWeakPtr(),
        observer.InitWithNewPipeAndPassReceiver());
    storage_partition_->GetCacheStorageControl()->AddObserver(
        std::move(observer));
  }
  return cache_storage_observer_.get();
}

StorageHandler::IndexedDBObserver* StorageHandler::GetIndexedDBObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!indexed_db_observer_) {
    indexed_db_observer_ =
        std::make_unique<IndexedDBObserver>(weak_ptr_factory_.GetWeakPtr());
  }
  return indexed_db_observer_.get();
}

SharedStorageWorkletHostManager*
StorageHandler::GetSharedStorageWorkletHostManager() {
  DCHECK(storage_partition_);
  return static_cast<StoragePartitionImpl*>(storage_partition_)
      ->GetSharedStorageWorkletHostManager();
}

absl::variant<protocol::Response, storage::SharedStorageManager*>
StorageHandler::GetSharedStorageManager() {
  if (!storage_partition_)
    return Response::InternalError();

  if (auto* manager = static_cast<StoragePartitionImpl*>(storage_partition_)
                          ->GetSharedStorageManager()) {
    return manager;
  }
  return Response::ServerError("Shared storage is disabled");
}

void StorageHandler::NotifyCacheStorageListChanged(
    const blink::StorageKey& storage_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frontend_->CacheStorageListUpdated(storage_key.origin().Serialize(),
                                     storage_key.Serialize());
}

void StorageHandler::NotifyCacheStorageContentChanged(
    const blink::StorageKey& storage_key,
    const std::string& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frontend_->CacheStorageContentUpdated(storage_key.origin().Serialize(),
                                        storage_key.Serialize(), name);
}

void StorageHandler::NotifyIndexedDBListChanged(
    const std::string& origin,
    const std::string& storage_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frontend_->IndexedDBListUpdated(origin, storage_key);
}

void StorageHandler::NotifyIndexedDBContentChanged(
    const std::string& origin,
    const std::string& storage_key,
    const std::u16string& database_name,
    const std::u16string& object_store_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frontend_->IndexedDBContentUpdated(origin, storage_key,
                                     base::UTF16ToUTF8(database_name),
                                     base::UTF16ToUTF8(object_store_name));
}

Response StorageHandler::FindStoragePartition(
    const Maybe<std::string>& browser_context_id,
    StoragePartition** storage_partition) {
  BrowserContext* browser_context = nullptr;
  Response response =
      BrowserHandler::FindBrowserContext(browser_context_id, &browser_context);
  if (!response.IsSuccess())
    return response;
  *storage_partition = browser_context->GetDefaultStoragePartition();
  if (!*storage_partition)
    return Response::InternalError();
  return Response::Success();
}

namespace {

void SendTrustTokens(
    std::unique_ptr<StorageHandler::GetTrustTokensCallback> callback,
    std::vector<::network::mojom::StoredTrustTokensForIssuerPtr> tokens) {
  auto result =
      std::make_unique<protocol::Array<protocol::Storage::TrustTokens>>();
  for (auto const& token : tokens) {
    auto protocol_token = protocol::Storage::TrustTokens::Create()
                              .SetIssuerOrigin(token->issuer.Serialize())
                              .SetCount(token->count)
                              .Build();
    result->push_back(std::move(protocol_token));
  }

  callback->sendSuccess(std::move(result));
}

}  // namespace

void StorageHandler::GetTrustTokens(
    std::unique_ptr<GetTrustTokensCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  storage_partition_->GetNetworkContext()->GetStoredTrustTokenCounts(
      base::BindOnce(&SendTrustTokens, std::move(callback)));
}

namespace {

void SendClearTrustTokensStatus(
    std::unique_ptr<StorageHandler::ClearTrustTokensCallback> callback,
    network::mojom::DeleteStoredTrustTokensStatus status) {
  switch (status) {
    case network::mojom::DeleteStoredTrustTokensStatus::kSuccessTokensDeleted:
      callback->sendSuccess(/* didDeleteTokens */ true);
      break;
    case network::mojom::DeleteStoredTrustTokensStatus::kSuccessNoTokensDeleted:
      callback->sendSuccess(/* didDeleteTokens */ false);
      break;
    case network::mojom::DeleteStoredTrustTokensStatus::kFailureFeatureDisabled:
      callback->sendFailure(
          Response::ServerError("The Trust Tokens feature is disabled."));
      break;
    case network::mojom::DeleteStoredTrustTokensStatus::kFailureInvalidOrigin:
      callback->sendFailure(
          Response::InvalidParams("The provided issuerOrigin is invalid. It "
                                  "must be a HTTP/HTTPS trustworthy origin."));
      break;
  }
}

}  // namespace

void StorageHandler::ClearTrustTokens(
    const std::string& issuerOrigin,
    std::unique_ptr<ClearTrustTokensCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  storage_partition_->GetNetworkContext()->DeleteStoredTrustTokens(
      url::Origin::Create(GURL(issuerOrigin)),
      base::BindOnce(&SendClearTrustTokensStatus, std::move(callback)));
}

void StorageHandler::OnInterestGroupAccessed(
    const base::Time& access_time,
    InterestGroupManagerImpl::InterestGroupObserverInterface::AccessType type,
    const std::string& owner_origin,
    const std::string& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  using AccessType =
      InterestGroupManagerImpl::InterestGroupObserverInterface::AccessType;
  std::string type_enum;
  switch (type) {
    case AccessType::kJoin:
      type_enum = Storage::InterestGroupAccessTypeEnum::Join;
      break;
    case AccessType::kLeave:
      type_enum = Storage::InterestGroupAccessTypeEnum::Leave;
      break;
    case AccessType::kUpdate:
      type_enum = Storage::InterestGroupAccessTypeEnum::Update;
      break;
    case AccessType::kLoaded:
      type_enum = Storage::InterestGroupAccessTypeEnum::Loaded;
      break;
    case AccessType::kBid:
      type_enum = Storage::InterestGroupAccessTypeEnum::Bid;
      break;
    case AccessType::kWin:
      type_enum = Storage::InterestGroupAccessTypeEnum::Win;
      break;
  };
  frontend_->InterestGroupAccessed(access_time.ToDoubleT(), type_enum,
                                   owner_origin, name);
}

namespace {
void SendGetInterestGroup(
    std::unique_ptr<StorageHandler::GetInterestGroupDetailsCallback> callback,
    absl::optional<StorageInterestGroup> storage_group) {
  if (!storage_group) {
    callback->sendFailure(Response::ServerError("Interest group not found"));
    return;
  }

  const blink::InterestGroup& group = storage_group->interest_group;
  auto trusted_bidding_signals_keys =
      std::make_unique<protocol::Array<std::string>>();
  if (group.trusted_bidding_signals_keys) {
    for (const auto& key : group.trusted_bidding_signals_keys.value())
      trusted_bidding_signals_keys->push_back(key);
  }
  auto ads =
      std::make_unique<protocol::Array<protocol::Storage::InterestGroupAd>>();
  if (group.ads) {
    for (const auto& ad : *group.ads) {
      auto protocol_ad = protocol::Storage::InterestGroupAd::Create()
                             .SetRenderUrl(ad.render_url.spec())
                             .Build();
      if (ad.metadata)
        protocol_ad->SetMetadata(*ad.metadata);
      ads->push_back(std::move(protocol_ad));
    }
  }
  auto ad_components =
      std::make_unique<protocol::Array<protocol::Storage::InterestGroupAd>>();
  if (group.ad_components) {
    for (const auto& ad : *group.ad_components) {
      auto protocol_ad = protocol::Storage::InterestGroupAd::Create()
                             .SetRenderUrl(ad.render_url.spec())
                             .Build();
      if (ad.metadata)
        protocol_ad->SetMetadata(*ad.metadata);
      ad_components->push_back(std::move(protocol_ad));
    }
  }
  auto protocol_group =
      protocol::Storage::InterestGroupDetails::Create()
          .SetOwnerOrigin(group.owner.Serialize())
          .SetName(group.name)
          .SetExpirationTime(group.expiry.ToDoubleT())
          .SetJoiningOrigin(storage_group->joining_origin.Serialize())
          .SetTrustedBiddingSignalsKeys(std::move(trusted_bidding_signals_keys))
          .SetAds(std::move(ads))
          .SetAdComponents(std::move(ad_components))
          .Build();
  if (group.bidding_url)
    protocol_group->SetBiddingUrl(group.bidding_url->spec());
  if (group.bidding_wasm_helper_url)
    protocol_group->SetBiddingWasmHelperUrl(
        group.bidding_wasm_helper_url->spec());
  if (group.daily_update_url)
    protocol_group->SetUpdateUrl(group.daily_update_url->spec());
  if (group.trusted_bidding_signals_url)
    protocol_group->SetTrustedBiddingSignalsUrl(
        group.trusted_bidding_signals_url->spec());
  if (group.user_bidding_signals)
    protocol_group->SetUserBiddingSignals(*group.user_bidding_signals);

  callback->sendSuccess(std::move(protocol_group));
}

}  // namespace

void StorageHandler::GetInterestGroupDetails(
    const std::string& owner_origin_string,
    const std::string& name,
    std::unique_ptr<GetInterestGroupDetailsCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  InterestGroupManagerImpl* manager = static_cast<InterestGroupManagerImpl*>(
      storage_partition_->GetInterestGroupManager());
  if (!manager) {
    callback->sendFailure(
        Response::ServerError("Interest group storage is disabled"));
    return;
  }

  GURL owner_origin_url(owner_origin_string);
  if (!owner_origin_url.is_valid()) {
    callback->sendFailure(Response::ServerError("Invalid Owner Origin"));
    return;
  }
  url::Origin owner_origin = url::Origin::Create(GURL(owner_origin_string));
  DCHECK(!owner_origin.opaque());

  manager->GetInterestGroup(
      owner_origin, name,
      base::BindOnce(&SendGetInterestGroup, std::move(callback)));
}

Response StorageHandler::SetInterestGroupTracking(bool enable) {
  if (!storage_partition_)
    return Response::InternalError();

  InterestGroupManagerImpl* manager = static_cast<InterestGroupManagerImpl*>(
      storage_partition_->GetInterestGroupManager());
  if (!manager)
    return Response::ServerError("Interest group storage is disabled.");

  if (enable) {
    // Only add if we are not already registered as an observer. We only
    // observe the interest group manager, so if we're observing anything then
    // we are already registered.
    if (!IsInObserverList())
      manager->AddInterestGroupObserver(this);
  } else {
    // Removal doesn't care if we are not registered.
    manager->RemoveInterestGroupObserver(this);
  }
  return Response::Success();
}

namespace {

void SendSharedStorageMetadata(
    std::unique_ptr<StorageHandler::GetSharedStorageMetadataCallback> callback,
    storage::SharedStorageManager::MetadataResult metadata) {
  if (metadata.time_result ==
      storage::SharedStorageManager::OperationResult::kNotFound) {
    callback->sendFailure(Response::ServerError("Origin not found."));
    return;
  }

  std::string error_message;

  if (metadata.length == -1) {
    error_message += "Unable to retrieve `length`. ";
  }

  if (metadata.time_result !=
      storage::SharedStorageManager::OperationResult::kSuccess) {
    error_message += "Unable to retrieve `creationTime`. ";
  }

  if (metadata.budget_result !=
      storage::SharedStorageManager::OperationResult::kSuccess) {
    error_message += "Unable to retrieve `remainingBudget`. ";
  }

  if (!error_message.empty()) {
    callback->sendFailure(Response::ServerError(error_message));
    return;
  }

  auto protocol_metadata =
      protocol::Storage::SharedStorageMetadata::Create()
          .SetLength(metadata.length)
          .SetCreationTime(metadata.creation_time.ToDoubleT())
          .SetRemainingBudget(metadata.remaining_budget)
          .Build();

  callback->sendSuccess(std::move(protocol_metadata));
}

}  // namespace

void StorageHandler::GetSharedStorageMetadata(
    const std::string& owner_origin_string,
    std::unique_ptr<GetSharedStorageMetadataCallback> callback) {
  auto manager_or_response = GetSharedStorageManager();
  if (absl::holds_alternative<protocol::Response>(manager_or_response)) {
    callback->sendFailure(absl::get<protocol::Response>(manager_or_response));
    return;
  }

  storage::SharedStorageManager* manager =
      absl::get<storage::SharedStorageManager*>(manager_or_response);
  DCHECK(manager);

  GURL owner_origin_url(owner_origin_string);
  if (!owner_origin_url.is_valid()) {
    callback->sendFailure(Response::InvalidParams("Invalid owner origin"));
    return;
  }
  url::Origin owner_origin = url::Origin::Create(owner_origin_url);
  DCHECK(!owner_origin.opaque());

  manager->GetMetadata(
      std::move(owner_origin),
      base::BindOnce(&SendSharedStorageMetadata, std::move(callback)));
}

namespace {

void RetrieveSharedStorageEntries(
    std::unique_ptr<StorageHandler::GetSharedStorageEntriesCallback> callback,
    storage::SharedStorageManager::EntriesResult entries_result) {
  if (entries_result.result !=
      storage::SharedStorageManager::OperationResult::kSuccess) {
    callback->sendFailure(Response::ServerError("Database error"));
    return;
  }

  auto entries = std::make_unique<
      protocol::Array<protocol::Storage::SharedStorageEntry>>();

  for (const auto& entry : entries_result.entries) {
    auto protocol_entry = protocol::Storage::SharedStorageEntry::Create()
                              .SetKey(entry.first)
                              .SetValue(entry.second)
                              .Build();
    entries->push_back(std::move(protocol_entry));
  }

  callback->sendSuccess(std::move(entries));
}

}  // namespace

void StorageHandler::GetSharedStorageEntries(
    const std::string& owner_origin_string,
    std::unique_ptr<GetSharedStorageEntriesCallback> callback) {
  auto manager_or_response = GetSharedStorageManager();
  if (absl::holds_alternative<protocol::Response>(manager_or_response)) {
    callback->sendFailure(absl::get<protocol::Response>(manager_or_response));
    return;
  }

  storage::SharedStorageManager* manager =
      absl::get<storage::SharedStorageManager*>(manager_or_response);
  DCHECK(manager);

  GURL owner_origin_url(owner_origin_string);
  if (!owner_origin_url.is_valid()) {
    callback->sendFailure(Response::InvalidParams("Invalid owner origin"));
    return;
  }
  url::Origin owner_origin = url::Origin::Create(owner_origin_url);
  DCHECK(!owner_origin.opaque());

  manager->GetEntriesForDevTools(
      owner_origin,
      base::BindOnce(&RetrieveSharedStorageEntries, std::move(callback)));
}

namespace {

void DispatchSharedStorageSetCallback(
    std::unique_ptr<Storage::Backend::SetSharedStorageEntryCallback> callback,
    storage::SharedStorageManager::OperationResult result) {
  if (result != storage::SharedStorageManager::OperationResult::kSet &&
      result != storage::SharedStorageManager::OperationResult::kIgnored) {
    callback->sendFailure(Response::ServerError("Database error"));
    return;
  }

  callback->sendSuccess();
}

}  // namespace

void StorageHandler::SetSharedStorageEntry(
    const std::string& owner_origin_string,
    const std::string& key,
    const std::string& value,
    Maybe<bool> ignore_if_present,
    std::unique_ptr<SetSharedStorageEntryCallback> callback) {
  auto manager_or_response = GetSharedStorageManager();
  if (absl::holds_alternative<protocol::Response>(manager_or_response)) {
    callback->sendFailure(absl::get<protocol::Response>(manager_or_response));
    return;
  }

  storage::SharedStorageManager* manager =
      absl::get<storage::SharedStorageManager*>(manager_or_response);
  DCHECK(manager);

  GURL owner_origin_url(owner_origin_string);
  if (!owner_origin_url.is_valid()) {
    callback->sendFailure(Response::InvalidParams("Invalid owner origin"));
    return;
  }
  url::Origin owner_origin = url::Origin::Create(owner_origin_url);
  DCHECK(!owner_origin.opaque());

  auto set_behavior =
      ignore_if_present.fromMaybe(false)
          ? storage::SharedStorageManager::SetBehavior::kIgnoreIfPresent
          : storage::SharedStorageManager::SetBehavior::kDefault;

  manager->Set(
      owner_origin, base::UTF8ToUTF16(key), base::UTF8ToUTF16(value),
      base::BindOnce(&DispatchSharedStorageSetCallback, std::move(callback)),
      set_behavior);
}

namespace {

template <typename CallbackType>
void DispatchSharedStorageCallback(
    std::unique_ptr<CallbackType> callback,
    storage::SharedStorageManager::OperationResult result) {
  if (result != storage::SharedStorageManager::OperationResult::kSuccess) {
    callback->sendFailure(Response::ServerError("Database error"));
    return;
  }

  callback->sendSuccess();
}

}  // namespace

void StorageHandler::DeleteSharedStorageEntry(
    const std::string& owner_origin_string,
    const std::string& key,
    std::unique_ptr<DeleteSharedStorageEntryCallback> callback) {
  auto manager_or_response = GetSharedStorageManager();
  if (absl::holds_alternative<protocol::Response>(manager_or_response)) {
    callback->sendFailure(absl::get<protocol::Response>(manager_or_response));
    return;
  }

  storage::SharedStorageManager* manager =
      absl::get<storage::SharedStorageManager*>(manager_or_response);
  DCHECK(manager);

  GURL owner_origin_url(owner_origin_string);
  if (!owner_origin_url.is_valid()) {
    callback->sendFailure(Response::InvalidParams("Invalid owner origin"));
    return;
  }
  url::Origin owner_origin = url::Origin::Create(owner_origin_url);
  DCHECK(!owner_origin.opaque());

  manager->Delete(
      owner_origin, base::UTF8ToUTF16(key),
      base::BindOnce(
          &DispatchSharedStorageCallback<DeleteSharedStorageEntryCallback>,
          std::move(callback)));
}

void StorageHandler::ClearSharedStorageEntries(
    const std::string& owner_origin_string,
    std::unique_ptr<ClearSharedStorageEntriesCallback> callback) {
  auto manager_or_response = GetSharedStorageManager();
  if (absl::holds_alternative<protocol::Response>(manager_or_response)) {
    callback->sendFailure(absl::get<protocol::Response>(manager_or_response));
    return;
  }

  storage::SharedStorageManager* manager =
      absl::get<storage::SharedStorageManager*>(manager_or_response);
  DCHECK(manager);

  GURL owner_origin_url(owner_origin_string);
  if (!owner_origin_url.is_valid()) {
    callback->sendFailure(Response::InvalidParams("Invalid owner origin"));
    return;
  }
  url::Origin owner_origin = url::Origin::Create(owner_origin_url);
  DCHECK(!owner_origin.opaque());

  manager->Clear(
      owner_origin,
      base::BindOnce(
          &DispatchSharedStorageCallback<ClearSharedStorageEntriesCallback>,
          std::move(callback)));
}

Response StorageHandler::SetSharedStorageTracking(bool enable) {
  if (enable) {
    if (!GetSharedStorageWorkletHostManager())
      return Response::ServerError("Shared storage is disabled.");
    shared_storage_observer_ = std::make_unique<SharedStorageObserver>(this);
  } else {
    shared_storage_observer_.reset();
  }
  return Response::Success();
}

void StorageHandler::NotifySharedStorageAccessed(
    const base::Time& access_time,
    SharedStorageWorkletHostManager::SharedStorageObserverInterface::AccessType
        type,
    const std::string& main_frame_id,
    const std::string& owner_origin,
    const SharedStorageEventParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using AccessType = SharedStorageWorkletHostManager::
      SharedStorageObserverInterface::AccessType;
  std::string type_enum;
  switch (type) {
    case AccessType::kDocumentAddModule:
      type_enum = Storage::SharedStorageAccessTypeEnum::DocumentAddModule;
      break;
    case AccessType::kDocumentSelectURL:
      type_enum = Storage::SharedStorageAccessTypeEnum::DocumentSelectURL;
      break;
    case AccessType::kDocumentRun:
      type_enum = Storage::SharedStorageAccessTypeEnum::DocumentRun;
      break;
    case AccessType::kDocumentSet:
      type_enum = Storage::SharedStorageAccessTypeEnum::DocumentSet;
      break;
    case AccessType::kDocumentAppend:
      type_enum = Storage::SharedStorageAccessTypeEnum::DocumentAppend;
      break;
    case AccessType::kDocumentDelete:
      type_enum = Storage::SharedStorageAccessTypeEnum::DocumentDelete;
      break;
    case AccessType::kDocumentClear:
      type_enum = Storage::SharedStorageAccessTypeEnum::DocumentClear;
      break;
    case AccessType::kWorkletSet:
      type_enum = Storage::SharedStorageAccessTypeEnum::WorkletSet;
      break;
    case AccessType::kWorkletAppend:
      type_enum = Storage::SharedStorageAccessTypeEnum::WorkletAppend;
      break;
    case AccessType::kWorkletDelete:
      type_enum = Storage::SharedStorageAccessTypeEnum::WorkletDelete;
      break;
    case AccessType::kWorkletClear:
      type_enum = Storage::SharedStorageAccessTypeEnum::WorkletClear;
      break;
    case AccessType::kWorkletGet:
      type_enum = Storage::SharedStorageAccessTypeEnum::WorkletGet;
      break;
    case AccessType::kWorkletKeys:
      type_enum = Storage::SharedStorageAccessTypeEnum::WorkletKeys;
      break;
    case AccessType::kWorkletEntries:
      type_enum = Storage::SharedStorageAccessTypeEnum::WorkletEntries;
      break;
    case AccessType::kWorkletLength:
      type_enum = Storage::SharedStorageAccessTypeEnum::WorkletLength;
      break;
    case AccessType::kWorkletRemainingBudget:
      type_enum = Storage::SharedStorageAccessTypeEnum::WorkletRemainingBudget;
      break;
  };

  auto protocol_params =
      protocol::Storage::SharedStorageAccessParams::Create().Build();

  if (params.script_source_url)
    protocol_params->SetScriptSourceUrl(*params.script_source_url);
  if (params.operation_name)
    protocol_params->SetOperationName(*params.operation_name);
  if (params.serialized_data)
    protocol_params->SetSerializedData(*params.serialized_data);
  if (params.key)
    protocol_params->SetKey(*params.key);
  if (params.value)
    protocol_params->SetValue(*params.value);

  if (params.urls_with_metadata) {
    auto protocol_urls = std::make_unique<
        protocol::Array<protocol::Storage::SharedStorageUrlWithMetadata>>();

    for (const auto& url_with_metadata : *params.urls_with_metadata) {
      auto reporting_metadata = std::make_unique<
          protocol::Array<protocol::Storage::SharedStorageReportingMetadata>>();

      for (const auto& metadata_pair : url_with_metadata.reporting_metadata) {
        auto reporting_pair =
            protocol::Storage::SharedStorageReportingMetadata::Create()
                .SetEventType(metadata_pair.first)
                .SetReportingUrl(metadata_pair.second)
                .Build();
        reporting_metadata->push_back(std::move(reporting_pair));
      }

      auto protocol_url =
          protocol::Storage::SharedStorageUrlWithMetadata::Create()
              .SetUrl(url_with_metadata.url)
              .SetReportingMetadata(std::move(reporting_metadata))
              .Build();
      protocol_urls->push_back(std::move(protocol_url));
    }

    protocol_params->SetUrlsWithMetadata(std::move(protocol_urls));
  }

  frontend_->SharedStorageAccessed(access_time.ToDoubleT(), type_enum,
                                   main_frame_id, owner_origin,
                                   std::move(protocol_params));
}

}  // namespace protocol
}  // namespace content
