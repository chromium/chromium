// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/storage_handler.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/browser/devtools/dedicated_worker_devtools_agent_host.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/protocol/browser_handler.h"
#include "content/browser/devtools/protocol/handler_helpers.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/storage.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_manager_observer.mojom-forward.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/interest_group/devtools_serialization.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom-shared.h"
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
    int64_t usage_value = breakdown_ptr->*(initializer.usage_member);
    if (initializer.type == Storage::StorageTypeEnum::Cache_storage) {
      usage_value += breakdown_ptr->backgroundFetch;
    }
    std::unique_ptr<Storage::UsageForType> entry =
        Storage::UsageForType::Create()
            .SetStorageType(initializer.type)
            .SetUsage(usage_value)
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
      storage_key,
      base::BindOnce(&GotUsageAndQuotaDataCallback, std::move(callback)));
}

std::unique_ptr<protocol::Storage::StorageBucketInfo> BuildBucketInfo(
    const storage::BucketInfo& bucket) {
  std::string durability_enum;
  switch (bucket.durability) {
    case blink::mojom::BucketDurability::kRelaxed:
      durability_enum = Storage::StorageBucketsDurabilityEnum::Relaxed;
      break;
    case blink::mojom::BucketDurability::kStrict:
      durability_enum = Storage::StorageBucketsDurabilityEnum::Strict;
      break;
  }

  std::unique_ptr<protocol::Storage::StorageBucket> storage_bucket;
  if (bucket.is_default()) {
    storage_bucket = protocol::Storage::StorageBucket::Create()
                         .SetStorageKey(bucket.storage_key.Serialize())
                         .Build();
  } else {
    storage_bucket = protocol::Storage::StorageBucket::Create()
                         .SetStorageKey(bucket.storage_key.Serialize())
                         .SetName(bucket.name)
                         .Build();
  }

  return protocol::Storage::StorageBucketInfo::Create()
      .SetBucket(std::move(storage_bucket))
      .SetId(base::NumberToString(bucket.id.value()))
      .SetExpiration(bucket.expiration.InSecondsFSinceUnixEpoch())
      .SetQuota(bucket.quota)
      .SetPersistent(bucket.persistent)
      .SetDurability(durability_enum)
      .Build();
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
    if (storage_keys_.find(storage_key) != storage_keys_.end()) {
      return;
    }
    storage_keys_.insert(storage_key);
  }

  void UntrackStorageKey(const blink::StorageKey& storage_key) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    storage_keys_.erase(storage_key);
  }

  void OnCacheListChanged(
      const storage::BucketLocator& bucket_locator) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto found = storage_keys_.find(bucket_locator.storage_key);
    if (found == storage_keys_.end()) {
      return;
    }
    owner_->NotifyCacheStorageListChanged(bucket_locator);
  }

  void OnCacheContentChanged(const storage::BucketLocator& bucket_locator,
                             const std::string& cache_name) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (storage_keys_.find(bucket_locator.storage_key) == storage_keys_.end()) {
      return;
    }
    owner_->NotifyCacheStorageContentChanged(bucket_locator, cache_name);
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

  void TrackStorageKey(const blink::StorageKey& storage_key) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (storage_keys_.find(storage_key) != storage_keys_.end()) {
      return;
    }
    storage_keys_.insert(storage_key);
  }

  void UntrackStorageKey(const blink::StorageKey& storage_key) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    storage_keys_.erase(storage_key);
  }

  void OnIndexedDBListChanged(
      const storage::BucketLocator& bucket_locator) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!owner_) {
      return;
    }
    // TODO(crbug.com/40221733): Allow custom bucket names.
    auto found = storage_keys_.find(bucket_locator.storage_key);
    if (found == storage_keys_.end()) {
      return;
    }

    owner_->NotifyIndexedDBListChanged(bucket_locator);
  }

  void OnIndexedDBContentChanged(
      const storage::BucketLocator& bucket_locator,
      const std::u16string& database_name,
      const std::u16string& object_store_name) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!owner_) {
      return;
    }
    // TODO(crbug.com/40221733): Allow custom bucket names.
    auto found = storage_keys_.find(bucket_locator.storage_key);
    if (found == storage_keys_.end()) {
      return;
    }

    owner_->NotifyIndexedDBContentChanged(bucket_locator, database_name,
                                          object_store_name);
  }

 private:
  void ReconnectObserver() {
    DCHECK(!receiver_.is_bound());
    if (!owner_) {
      return;
    }

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

class StorageHandler::QuotaManagerObserver
    : storage::mojom::QuotaManagerObserver {
 public:
  QuotaManagerObserver(base::WeakPtr<StorageHandler> owner_storage_handler,
                       storage::QuotaManagerProxy* quota_manager_proxy)
      : owner_(owner_storage_handler) {
    quota_manager_proxy->AddObserver(receiver_.BindNewPipeAndPassRemote());
  }

  QuotaManagerObserver(const QuotaManagerObserver&) = delete;
  QuotaManagerObserver& operator=(const QuotaManagerObserver&) = delete;

  ~QuotaManagerObserver() override = default;

  void TrackStorageKey(const blink::StorageKey& storage_key,
                       storage::QuotaManagerProxy* manager) {
    if (!storage_keys_.insert(storage_key).second) {
      return;
    }
    manager->GetBucketsForStorageKey(
        storage_key, /*delete_expired=*/false,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::BindOnce(
            [](base::WeakPtr<StorageHandler> owner_storage_handler,
               storage::QuotaErrorOr<std::set<storage::BucketInfo>> buckets) {
              if (!owner_storage_handler || !buckets.has_value()) {
                return;
              }

              for (const storage::BucketInfo& bucket : buckets.value()) {
                owner_storage_handler->NotifyCreateOrUpdateBucket(bucket);
              }
            },
            owner_));
  }

  void UntrackStorageKey(const blink::StorageKey& storage_key) {
    storage_keys_.erase(storage_key);
  }

 private:
  void OnCreateOrUpdateBucket(const storage::BucketInfo& bucket_info) override {
    auto found = storage_keys_.find(bucket_info.storage_key);
    if (found == storage_keys_.end()) {
      return;
    }
    owner_->NotifyCreateOrUpdateBucket(bucket_info);
  }

  void OnDeleteBucket(const storage::BucketLocator& bucket_locator) override {
    auto found = storage_keys_.find(bucket_locator.storage_key);
    if (found == storage_keys_.end()) {
      return;
    }
    owner_->NotifyDeleteBucket(bucket_locator);
  }

  base::flat_set<blink::StorageKey> storage_keys_;

  base::WeakPtr<StorageHandler> owner_;
  mojo::Receiver<storage::mojom::QuotaManagerObserver> receiver_{this};
};

StorageHandler::StorageHandler(DevToolsAgentHostImpl* host,
                               DevToolsAgentHostClient* client)
    : DevToolsDomainHandler(Storage::Metainfo::domainName),
      host_(host),
      client_(client) {}

StorageHandler::~StorageHandler() {
  DCHECK(!cache_storage_observer_);
  DCHECK(!indexed_db_observer_);
}

// static
std::vector<StorageHandler*> StorageHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<StorageHandler>(Storage::Metainfo::domainName);
}

void StorageHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Storage::Frontend>(dispatcher->channel());
  Storage::Dispatcher::wire(dispatcher, this);
}

void StorageHandler::SetRenderer(int process_host_id,
                                 RenderFrameHostImpl* frame_host) {
  RenderProcessHost* process = RenderProcessHost::FromID(process_host_id);
  StoragePartition* new_storage_partition =
      process ? process->GetStoragePartition() : nullptr;

  storage_partition_ = new_storage_partition;
  frame_host_ = frame_host;
}

Response StorageHandler::Disable() {
  cache_storage_observer_.reset();
  indexed_db_observer_.reset();
  quota_override_handle_.reset();

  SetSharedStorageTracking(false);
  quota_manager_observer_.reset();
  return Response::Success();
}

void StorageHandler::GetCookies(std::optional<std::string> browser_context_id,
                                std::unique_ptr<GetCookiesCallback> callback) {
  StoragePartition* storage_partition = nullptr;
  Response response = StorageHandler::FindStoragePartition(browser_context_id,
                                                           &storage_partition);
  if (!response.IsSuccess()) {
    callback->sendFailure(std::move(response));
    return;
  }

  storage_partition->GetCookieManagerForBrowserProcess()->GetAllCookies(
      base::BindOnce(&StorageHandler::GotAllCookies,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void StorageHandler::GotAllCookies(
    std::unique_ptr<GetCookiesCallback> callback,
    const std::vector<net::CanonicalCookie>& cookies) {
  bool is_webui = frame_host_ && frame_host_->web_ui();
  std::vector<net::CanonicalCookie> filtered_cookies;
  for (const auto& cookie : cookies) {
    if (NetworkHandler::CanAccessCookie(CHECK_DEREF(client_.get()), is_webui,
                                        cookie)) {
      filtered_cookies.emplace_back(std::move(cookie));
    }
  }
  callback->sendSuccess(NetworkHandler::BuildCookieArray(filtered_cookies));
}

void StorageHandler::SetCookies(
    std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
    std::optional<std::string> browser_context_id,
    std::unique_ptr<SetCookiesCallback> callback) {
  StoragePartition* storage_partition = nullptr;
  Response response = StorageHandler::FindStoragePartition(browser_context_id,
                                                           &storage_partition);
  if (!response.IsSuccess()) {
    callback->sendFailure(std::move(response));
    return;
  }

  NetworkHandler::SetCookies(
      storage_partition, std::move(cookies), CHECK_DEREF(client_.get()),
      frame_host_ && frame_host_->web_ui(),
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

bool StorageHandler::CanAccessCookie(const net::CanonicalCookie& cookie) const {
  return NetworkHandler::CanAccessCookie(
      CHECK_DEREF(client_.get()), frame_host_ && frame_host_->web_ui(), cookie);
}

void StorageHandler::ClearCookies(
    std::optional<std::string> browser_context_id,
    std::unique_ptr<ClearCookiesCallback> callback) {
  StoragePartition* storage_partition = nullptr;
  Response response = StorageHandler::FindStoragePartition(browser_context_id,
                                                           &storage_partition);
  if (!response.IsSuccess()) {
    callback->sendFailure(std::move(response));
    return;
  }

  NetworkHandler::ClearCookies(
      storage_partition, CHECK_DEREF(client_.get()),
      base::BindRepeating(
          [](base::WeakPtr<StorageHandler> handler,
             const net::CanonicalCookie& cookie) {
            return handler && handler->CanAccessCookie(cookie);
          },
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ClearCookiesCallback::sendSuccess, std::move(callback)));
}

Response StorageHandler::GetStorageKeyForFrameInternal(
    const std::string& frame_id,
    std::string* serialized_storage_key) {
  if (!frame_host_) {
    return Response::InvalidParams("Frame host not found");
  }
  FrameTreeNode* node = protocol::FrameTreeNodeFromDevToolsFrameToken(
      frame_host_->frame_tree_node(), frame_id);
  if (!node) {
    return Response::InvalidParams("Frame tree node for given frame not found");
  }
  RenderFrameHostImpl* rfh = node->current_frame_host();
  if (rfh->GetStorageKey().origin().opaque()) {
    return Response::ServerError(
        "Frame corresponds to an opaque origin and its storage key cannot be "
        "serialized");
  }
  *serialized_storage_key = rfh->GetStorageKey().Serialize();
  return Response::Success();
}

// TODO(crbug.com/445966299): This method is deprecated and
// will be removed once all clients, including the DevTools frontend, have
// migrated to using GetStorageKey.
Response StorageHandler::GetStorageKeyForFrame(
    const std::string& frame_id,
    std::string* serialized_storage_key) {
  return GetStorageKeyForFrameInternal(frame_id, serialized_storage_key);
}

Response StorageHandler::GetStorageKey(std::optional<std::string> frame_id,
                                       std::string* serialized_storage_key) {
  if (frame_id.has_value()) {
    return GetStorageKeyForFrameInternal(frame_id.value(),
                                         serialized_storage_key);
  }

  if (!host_) {
    return Response::InvalidParams("DevToolsAgentHost not found");
  }

  std::optional<blink::StorageKey> storage_key;

  const std::string& type = host_->GetType();

  if (type == content::DevToolsAgentHost::kTypeServiceWorker) {
    auto* service_worker_agent_host =
        static_cast<content::ServiceWorkerDevToolsAgentHost*>(host_.get());
    storage_key = service_worker_agent_host->GetStorageKey();
  } else if (type == content::DevToolsAgentHost::kTypeDedicatedWorker) {
    auto* dedicated_worker_agent_host =
        static_cast<content::DedicatedWorkerDevToolsAgentHost*>(host_.get());
    storage_key = dedicated_worker_agent_host->GetStorageKey();
  } else if (type == content::DevToolsAgentHost::kTypeSharedWorker) {
    auto* shared_worker_agent_host =
        static_cast<content::SharedWorkerDevToolsAgentHost*>(host_.get());
    storage_key = shared_worker_agent_host->GetStorageKey();
  } else {
    return Response::InvalidParams(
        "Target is not a supported worker type for storage inspection.");
  }

  if (!storage_key.has_value()) {
    return Response::ServerError(
        "Could not determine storage key for the target.");
  }
  if (storage_key->origin().opaque()) {
    return Response::ServerError(
        "Target corresponds to an opaque origin and its storage key cannot be "
        "serialized");
  }

  *serialized_storage_key = storage_key.value().Serialize();
  return Response::Success();
}

namespace {
uint32_t GetRemoveDataMask(const std::string& storage_types) {
  std::vector<std::string_view> types = base::SplitStringPiece(
      storage_types, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  absl::flat_hash_set<std::string_view> set = {types.begin(), types.end()};
  uint32_t remove_mask = 0;
  if (set.contains(Storage::StorageTypeEnum::Cookies)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_COOKIES;
  }
  if (set.contains(Storage::StorageTypeEnum::File_systems)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS;
  }
  if (set.contains(Storage::StorageTypeEnum::Indexeddb)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  }
  if (set.contains(Storage::StorageTypeEnum::Local_storage)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  }
  if (set.contains(Storage::StorageTypeEnum::Shader_cache)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE;
  }
  if (set.contains(Storage::StorageTypeEnum::Service_workers)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  }
  if (set.contains(Storage::StorageTypeEnum::Cache_storage)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE;
  }
  if (set.contains(Storage::StorageTypeEnum::Interest_groups)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_INTEREST_GROUPS;
  }
  if (set.contains(Storage::StorageTypeEnum::Shared_storage)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SHARED_STORAGE;
  }
  if (set.contains(Storage::StorageTypeEnum::All)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_ALL;
  }
  return remove_mask;
}
}  // namespace

void StorageHandler::ClearDataForOrigin(
    const std::string& origin,
    const std::string& storage_types,
    std::unique_ptr<ClearDataForOriginCallback> callback) {
  if (!storage_partition_) {
    return callback->sendFailure(Response::InternalError());
  }

  uint32_t remove_mask = GetRemoveDataMask(storage_types);

  if (!remove_mask) {
    return callback->sendFailure(
        Response::InvalidParams("No valid storage type specified"));
  }

  storage_partition_->ClearData(
      remove_mask,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(GURL(origin))),
      base::Time(), base::Time::Max(),
      base::BindOnce(&ClearDataForOriginCallback::sendSuccess,
                     std::move(callback)));
}

void StorageHandler::ClearDataForStorageKey(
    const std::string& storage_key,
    const std::string& storage_types,
    std::unique_ptr<ClearDataForStorageKeyCallback> callback) {
  if (!storage_partition_) {
    return callback->sendFailure(Response::InternalError());
  }

  uint32_t remove_mask = GetRemoveDataMask(storage_types);

  if (!remove_mask) {
    return callback->sendFailure(
        Response::InvalidParams("No valid storage type specified"));
  }

  std::optional<blink::StorageKey> key =
      blink::StorageKey::Deserialize(storage_key);
  if (!key) {
    return callback->sendFailure(
        Response::InvalidParams("Unable to deserialize storage key"));
  }
  storage_partition_->ClearData(
      remove_mask, *key, base::Time(), base::Time::Max(),
      base::BindOnce(&ClearDataForStorageKeyCallback::sendSuccess,
                     std::move(callback)));
}

void StorageHandler::GetUsageAndQuota(
    const String& origin_string,
    std::unique_ptr<GetUsageAndQuotaCallback> callback) {
  if (!storage_partition_) {
    return callback->sendFailure(Response::InternalError());
  }

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
                     blink::StorageKey::CreateFirstParty(origin),
                     std::move(callback)));
}

void StorageHandler::OverrideQuotaForOrigin(
    const String& origin_string,
    std::optional<double> quota_size,
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
      blink::StorageKey::CreateFirstParty(origin),
      quota_size.has_value() ? std::make_optional(quota_size.value())
                             : std::nullopt,
      base::BindOnce(&OverrideQuotaForOriginCallback::sendSuccess,
                     std::move(callback)));
}

Response StorageHandler::TrackCacheStorageForOrigin(
    const std::string& origin_string) {
  if (!storage_partition_) {
    return Response::InternalError();
  }

  GURL origin_url(origin_string);
  url::Origin origin = url::Origin::Create(origin_url);
  if (!origin_url.is_valid() || origin.opaque()) {
    return Response::InvalidParams(origin_string + " is not a valid URL");
  }

  GetCacheStorageObserver()->TrackStorageKey(
      blink::StorageKey::CreateFirstParty(origin));
  return Response::Success();
}

Response StorageHandler::TrackCacheStorageForStorageKey(
    const std::string& storage_key) {
  if (!storage_partition_) {
    return Response::InternalError();
  }

  std::optional<blink::StorageKey> key =
      blink::StorageKey::Deserialize(storage_key);
  if (!key) {
    return Response::InvalidParams("Unable to deserialize storage key");
  }

  GetCacheStorageObserver()->TrackStorageKey(*key);
  return Response::Success();
}

Response StorageHandler::UntrackCacheStorageForOrigin(
    const std::string& origin_string) {
  if (!storage_partition_) {
    return Response::InternalError();
  }

  GURL origin_url(origin_string);
  url::Origin origin = url::Origin::Create(origin_url);
  if (!origin_url.is_valid() || origin.opaque()) {
    return Response::InvalidParams(origin_string + " is not a valid URL");
  }

  GetCacheStorageObserver()->UntrackStorageKey(
      blink::StorageKey::CreateFirstParty(origin));
  return Response::Success();
}

Response StorageHandler::UntrackCacheStorageForStorageKey(
    const std::string& storage_key) {
  if (!storage_partition_) {
    return Response::InternalError();
  }

  std::optional<blink::StorageKey> key =
      blink::StorageKey::Deserialize(storage_key);
  if (!key) {
    return Response::InvalidParams("Unable to deserialize storage key");
  }

  GetCacheStorageObserver()->UntrackStorageKey(*key);
  return Response::Success();
}

Response StorageHandler::TrackIndexedDBForOrigin(
    const std::string& origin_string) {
  if (!storage_partition_) {
    return Response::InternalError();
  }

  GURL origin_url(origin_string);
  url::Origin origin = url::Origin::Create(origin_url);
  if (!origin_url.is_valid() || origin.opaque()) {
    return Response::InvalidParams(origin_string + " is not a valid URL");
  }

  GetIndexedDBObserver()->TrackStorageKey(
      blink::StorageKey::CreateFirstParty(origin));
  return Response::Success();
}

Response StorageHandler::TrackIndexedDBForStorageKey(
    const std::string& storage_key) {
  if (!storage_partition_) {
    return Response::InternalError();
  }

  std::optional<blink::StorageKey> key =
      blink::StorageKey::Deserialize(storage_key);
  if (!key) {
    return Response::InvalidParams("Unable to deserialize storage key");
  }

  GetIndexedDBObserver()->TrackStorageKey(*key);
  return Response::Success();
}

Response StorageHandler::UntrackIndexedDBForOrigin(
    const std::string& origin_string) {
  if (!storage_partition_) {
    return Response::InternalError();
  }

  GURL origin_url(origin_string);
  url::Origin origin = url::Origin::Create(origin_url);
  if (!origin_url.is_valid() || origin.opaque()) {
    return Response::InvalidParams(origin_string + " is not a valid URL");
  }

  GetIndexedDBObserver()->UntrackStorageKey(
      blink::StorageKey::CreateFirstParty(origin));
  return Response::Success();
}

Response StorageHandler::UntrackIndexedDBForStorageKey(
    const std::string& storage_key) {
  if (!storage_partition_) {
    return Response::InternalError();
  }

  std::optional<blink::StorageKey> key =
      blink::StorageKey::Deserialize(storage_key);
  if (!key) {
    return Response::InvalidParams("Unable to deserialize storage key");
  }

  GetIndexedDBObserver()->UntrackStorageKey(*key);
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

storage::QuotaManagerProxy* StorageHandler::GetQuotaManagerProxy() {
  DCHECK(storage_partition_);

  return static_cast<StoragePartitionImpl*>(storage_partition_)
      ->GetQuotaManagerProxy();
}

void StorageHandler::NotifyCacheStorageListChanged(
    const storage::BucketLocator& bucket_locator) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frontend_->CacheStorageListUpdated(
      bucket_locator.storage_key.origin().Serialize(),
      bucket_locator.storage_key.Serialize(),
      base::NumberToString(bucket_locator.id.value()));
}

void StorageHandler::NotifyCacheStorageContentChanged(
    const storage::BucketLocator& bucket_locator,
    const std::string& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frontend_->CacheStorageContentUpdated(
      bucket_locator.storage_key.origin().Serialize(),
      bucket_locator.storage_key.Serialize(),
      base::NumberToString(bucket_locator.id.value()), name);
}

void StorageHandler::NotifyIndexedDBListChanged(
    storage::BucketLocator bucket_locator) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frontend_->IndexedDBListUpdated(
      bucket_locator.storage_key.origin().Serialize(),
      bucket_locator.storage_key.Serialize(),
      base::NumberToString(bucket_locator.id.value()));
}

void StorageHandler::NotifyIndexedDBContentChanged(
    storage::BucketLocator bucket_locator,
    const std::u16string& database_name,
    const std::u16string& object_store_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frontend_->IndexedDBContentUpdated(
      bucket_locator.storage_key.origin().Serialize(),
      bucket_locator.storage_key.Serialize(),
      base::NumberToString(bucket_locator.id.value()),
      base::UTF16ToUTF8(database_name), base::UTF16ToUTF8(object_store_name));
}

Response StorageHandler::FindStoragePartition(
    const std::optional<std::string>& browser_context_id,
    StoragePartition** storage_partition) {
  if (browser_context_id.has_value() &&
      host_->GetType() != DevToolsAgentHost::kTypeBrowser) {
    return Response::InvalidParams(
        "browserContextId is only allowed for Browser target");
  }

  BrowserContext* browser_context = nullptr;
  Response response =
      BrowserHandler::FindBrowserContext(browser_context_id, &browser_context);
  if (!response.IsSuccess()) {
    return response;
  }
  *storage_partition = browser_context->GetDefaultStoragePartition();
  if (!*storage_partition) {
    return Response::InternalError();
  }
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



void StorageHandler::GetSharedStorageMetadata(
    const std::string& owner_origin_string,
    std::unique_ptr<GetSharedStorageMetadataCallback> callback) {
  callback->sendFailure(Response::ServerError("Shared storage is disabled."));
}

void StorageHandler::GetSharedStorageEntries(
    const std::string& owner_origin_string,
    std::unique_ptr<GetSharedStorageEntriesCallback> callback) {
  callback->sendFailure(Response::ServerError("Shared storage is disabled."));
}

void StorageHandler::SetSharedStorageEntry(
    const std::string& owner_origin_string,
    const std::string& key,
    const std::string& value,
    std::optional<bool> ignore_if_present,
    std::unique_ptr<SetSharedStorageEntryCallback> callback) {
  callback->sendFailure(Response::ServerError("Shared storage is disabled."));
}

void StorageHandler::DeleteSharedStorageEntry(
    const std::string& owner_origin_string,
    const std::string& key,
    std::unique_ptr<DeleteSharedStorageEntryCallback> callback) {
  callback->sendFailure(Response::ServerError("Shared storage is disabled."));
}

void StorageHandler::ClearSharedStorageEntries(
    const std::string& owner_origin_string,
    std::unique_ptr<ClearSharedStorageEntriesCallback> callback) {
  callback->sendFailure(Response::ServerError("Shared storage is disabled."));
}

Response StorageHandler::SetSharedStorageTracking(bool enable) {
  return Response::ServerError("Shared storage is disabled.");
}

void StorageHandler::ResetSharedStorageBudget(
    const std::string& owner_origin_string,
    std::unique_ptr<ResetSharedStorageBudgetCallback> callback) {
  callback->sendFailure(Response::ServerError("Shared storage is disabled."));
}



DispatchResponse StorageHandler::SetStorageBucketTracking(
    const std::string& serialized_storage_key,
    bool enable) {
  if (!storage_partition_) {
    return Response::InternalError();
  }

  auto storage_key = blink::StorageKey::Deserialize(serialized_storage_key);
  if (!storage_key.has_value()) {
    return Response::InvalidParams("Invalid Storage Key given.");
  }

  if (enable) {
    storage::QuotaManagerProxy* manager = GetQuotaManagerProxy();
    if (!quota_manager_observer_) {
      quota_manager_observer_ =
          std::make_unique<StorageHandler::QuotaManagerObserver>(
              weak_ptr_factory_.GetWeakPtr(), manager);
    }
    quota_manager_observer_->TrackStorageKey(storage_key.value(), manager);
  } else if (quota_manager_observer_) {
    quota_manager_observer_->UntrackStorageKey(storage_key.value());
  }
  return Response::Success();
}

DispatchResponse StorageHandler::DeleteStorageBucket(
    std::unique_ptr<protocol::Storage::StorageBucket> bucket) {
  storage::QuotaManagerProxy* manager = GetQuotaManagerProxy();
  DCHECK(manager);

  if (!bucket->HasName()) {
    return Response::InvalidParams("Can't delete the default bucket.");
  }

  auto storage_key = blink::StorageKey::Deserialize(bucket->GetStorageKey());
  if (!storage_key.has_value()) {
    return Response::InvalidParams("Invalid Storage Key given.");
  }

  manager->DeleteBucket(storage_key.value(), bucket->GetName(""),
                        base::SingleThreadTaskRunner::GetCurrentDefault(),
                        base::DoNothing());
  return Response::Success();
}

void StorageHandler::NotifyCreateOrUpdateBucket(
    const storage::BucketInfo& bucket_info) {
  frontend_->StorageBucketCreatedOrUpdated(BuildBucketInfo(bucket_info));
}

void StorageHandler::NotifyDeleteBucket(
    const storage::BucketLocator& bucket_locator) {
  frontend_->StorageBucketDeleted(
      base::NumberToString(bucket_locator.id.value()));
}



}  // namespace protocol
}  // namespace content
