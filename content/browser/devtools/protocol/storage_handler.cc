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
  if (interest_group_tracking_enabled_) {
    // Transfer observer registration from old frame's StoragePartition to new;
    // SetInterestGroupTrackingInternal() will handle any nulls.
    SetInterestGroupTrackingInternal(storage_partition_, false);
    SetInterestGroupTrackingInternal(new_storage_partition, true);
  }
  storage_partition_ = new_storage_partition;
  frame_host_ = frame_host;
}

Response StorageHandler::Disable() {
  cache_storage_observer_.reset();
  indexed_db_observer_.reset();
  quota_override_handle_.reset();
  SetInterestGroupTracking(false);
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

SharedStorageRuntimeManager* StorageHandler::GetSharedStorageRuntimeManager() {
  if (!storage_partition_) {
    return nullptr;
  }
  return static_cast<StoragePartitionImpl*>(storage_partition_)
      ->GetSharedStorageRuntimeManager();
}

std::variant<protocol::Response, storage::SharedStorageManager*>
StorageHandler::GetSharedStorageManager() {
  if (!storage_partition_) {
    return Response::InternalError();
  }

  if (auto* manager = static_cast<StoragePartitionImpl*>(storage_partition_)
                          ->GetSharedStorageManager()) {
    return manager;
  }
  return Response::ServerError("Shared storage is disabled");
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

void StorageHandler::OnInterestGroupAccessed(
    base::optional_ref<const std::string> auction_id,
    base::Time access_time,
    InterestGroupManagerImpl::InterestGroupObserver::AccessType type,
    const url::Origin& owner_origin,
    const std::string& name,
    base::optional_ref<const url::Origin> component_seller_origin,
    std::optional<double> bid,
    base::optional_ref<const std::string> bid_currency) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  using AccessType =
      InterestGroupManagerImpl::InterestGroupObserver::AccessType;
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
    case AccessType::kAdditionalBid:
      type_enum = Storage::InterestGroupAccessTypeEnum::AdditionalBid;
      break;
    case AccessType::kWin:
      type_enum = Storage::InterestGroupAccessTypeEnum::Win;
      break;
    case AccessType::kAdditionalBidWin:
      type_enum = Storage::InterestGroupAccessTypeEnum::AdditionalBidWin;
      break;
    case AccessType::kClear:
      type_enum = Storage::InterestGroupAccessTypeEnum::Clear;
      break;
    case AccessType::kTopLevelBid:
      type_enum = Storage::InterestGroupAccessTypeEnum::TopLevelBid;
      break;
    case AccessType::kTopLevelAdditionalBid:
      type_enum = Storage::InterestGroupAccessTypeEnum::TopLevelAdditionalBid;
      break;
  };
  frontend_->InterestGroupAccessed(
      access_time.InSecondsFSinceUnixEpoch(), type_enum,
      owner_origin.Serialize(), name,
      component_seller_origin.has_value()
          ? std::optional<String>(component_seller_origin->Serialize())
          : std::nullopt,
      bid, bid_currency.CopyAsOptional(), auction_id.CopyAsOptional());
}

void StorageHandler::GetInterestGroupDetails(
    const std::string& owner_origin_string,
    const std::string& name,
    std::unique_ptr<GetInterestGroupDetailsCallback> callback) {
  // TODO(crbug.com/496189510): Remove this completely once the DevTools
  // frontend usage is gone.
  callback->sendSuccess(std::make_unique<base::DictValue>());
}

Response StorageHandler::SetInterestGroupTracking(bool enable) {
  // TODO(crbug.com/496189510): Remove this completely once the DevTools
  // frontend usage is gone.
  return Response::Success();
}

Response StorageHandler::SetInterestGroupTrackingInternal(
    StoragePartition* storage_partition,
    bool enable) {
  // TODO(crbug.com/496189510): Remove this completely once the DevTools
  // frontend usage is gone.
  return Response::Success();
}

Response StorageHandler::SetInterestGroupAuctionTracking(bool enable) {
  interest_group_auction_tracking_enabled_ = enable;
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

  if (metadata.bytes_used == -1) {
    error_message += "Unable to retrieve `bytes_used`. ";
  }

  if (!error_message.empty()) {
    callback->sendFailure(Response::ServerError(error_message));
    return;
  }

  auto protocol_metadata =
      protocol::Storage::SharedStorageMetadata::Create()
          .SetLength(metadata.length)
          .SetCreationTime(metadata.creation_time.InSecondsFSinceUnixEpoch())
          .SetRemainingBudget(metadata.remaining_budget)
          .SetBytesUsed(metadata.bytes_used)
          .Build();

  callback->sendSuccess(std::move(protocol_metadata));
}

}  // namespace

void StorageHandler::GetSharedStorageMetadata(
    const std::string& owner_origin_string,
    std::unique_ptr<GetSharedStorageMetadataCallback> callback) {
  auto manager_or_response = GetSharedStorageManager();
  if (std::holds_alternative<protocol::Response>(manager_or_response)) {
    callback->sendFailure(std::get<protocol::Response>(manager_or_response));
    return;
  }

  storage::SharedStorageManager* manager =
      std::get<storage::SharedStorageManager*>(manager_or_response);
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
  if (std::holds_alternative<protocol::Response>(manager_or_response)) {
    callback->sendFailure(std::get<protocol::Response>(manager_or_response));
    return;
  }

  storage::SharedStorageManager* manager =
      std::get<storage::SharedStorageManager*>(manager_or_response);
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
    std::optional<bool> ignore_if_present,
    std::unique_ptr<SetSharedStorageEntryCallback> callback) {
  auto manager_or_response = GetSharedStorageManager();
  if (std::holds_alternative<protocol::Response>(manager_or_response)) {
    callback->sendFailure(std::get<protocol::Response>(manager_or_response));
    return;
  }

  storage::SharedStorageManager* manager =
      std::get<storage::SharedStorageManager*>(manager_or_response);
  DCHECK(manager);

  GURL owner_origin_url(owner_origin_string);
  if (!owner_origin_url.is_valid()) {
    callback->sendFailure(Response::InvalidParams("Invalid owner origin"));
    return;
  }
  url::Origin owner_origin = url::Origin::Create(owner_origin_url);
  DCHECK(!owner_origin.opaque());

  auto set_behavior =
      ignore_if_present.value_or(false)
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
  if (std::holds_alternative<protocol::Response>(manager_or_response)) {
    callback->sendFailure(std::get<protocol::Response>(manager_or_response));
    return;
  }

  storage::SharedStorageManager* manager =
      std::get<storage::SharedStorageManager*>(manager_or_response);
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
  if (std::holds_alternative<protocol::Response>(manager_or_response)) {
    callback->sendFailure(std::get<protocol::Response>(manager_or_response));
    return;
  }

  storage::SharedStorageManager* manager =
      std::get<storage::SharedStorageManager*>(manager_or_response);
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
  // FIXME: this should remember the state and restore it
  // once the StorageRunTimeManager or the storage partition is available.
  if (enable) {
    auto* manager = GetSharedStorageRuntimeManager();
    if (!manager) {
      return Response::ServerError("Shared storage is disabled.");
    }
    // Only enable tracking if this handler is associated with a main render
    // frame host, and if tracking isn't already enabled.
    if (frame_host_ && frame_host_->IsOutermostMainFrame() &&
        !shared_storage_observation_.IsObserving()) {
      shared_storage_observation_.Observe(manager);
    }
  } else {
    shared_storage_observation_.Reset();
  }
  return Response::Success();
}

void StorageHandler::ResetSharedStorageBudget(
    const std::string& owner_origin_string,
    std::unique_ptr<ResetSharedStorageBudgetCallback> callback) {
  auto manager_or_response = GetSharedStorageManager();
  if (std::holds_alternative<protocol::Response>(manager_or_response)) {
    callback->sendFailure(std::get<protocol::Response>(manager_or_response));
    return;
  }

  storage::SharedStorageManager* manager =
      std::get<storage::SharedStorageManager*>(manager_or_response);
  DCHECK(manager);

  GURL owner_origin_url(owner_origin_string);
  if (!owner_origin_url.is_valid()) {
    callback->sendFailure(Response::InvalidParams("Invalid owner origin"));
    return;
  }
  url::Origin owner_origin = url::Origin::Create(owner_origin_url);
  DCHECK(!owner_origin.opaque());

  manager->ResetBudgetForDevTools(
      owner_origin,
      base::BindOnce(
          &DispatchSharedStorageCallback<ResetSharedStorageBudgetCallback>,
          std::move(callback)));
}

GlobalRenderFrameHostId StorageHandler::AssociatedFrameHostId() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return frame_host_ ? frame_host_->GetGlobalId() : GlobalRenderFrameHostId();
}

bool StorageHandler::ShouldReceiveAllSharedStorageReports() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return false;
}

namespace {

std::string GetFrameTokenFromGlobalRenderFrameHostId(
    GlobalRenderFrameHostId frame_id) {
  auto* rfh = frame_id ? RenderFrameHostImpl::FromID(frame_id) : nullptr;
  return rfh ? rfh->devtools_frame_token().ToString() : std::string();
}

const char* GetSharedStorageAccessMethodEnum(
    SharedStorageRuntimeManager::SharedStorageObserverInterface::AccessMethod
        method) {
  using AccessMethod =
      SharedStorageRuntimeManager::SharedStorageObserverInterface::AccessMethod;
  switch (method) {
    case AccessMethod::kAddModule:
      return Storage::SharedStorageAccessMethodEnum::AddModule;
    case AccessMethod::kCreateWorklet:
      return Storage::SharedStorageAccessMethodEnum::CreateWorklet;
    case AccessMethod::kSelectURL:
      return Storage::SharedStorageAccessMethodEnum::SelectURL;
    case AccessMethod::kRun:
      return Storage::SharedStorageAccessMethodEnum::Run;
    case AccessMethod::kBatchUpdate:
      return Storage::SharedStorageAccessMethodEnum::BatchUpdate;
    case AccessMethod::kSet:
      return Storage::SharedStorageAccessMethodEnum::Set;
    case AccessMethod::kAppend:
      return Storage::SharedStorageAccessMethodEnum::Append;
    case AccessMethod::kDelete:
      return Storage::SharedStorageAccessMethodEnum::Delete;
    case AccessMethod::kClear:
      return Storage::SharedStorageAccessMethodEnum::Clear;
    case AccessMethod::kGet:
      return Storage::SharedStorageAccessMethodEnum::Get;
    case AccessMethod::kKeys:
      return Storage::SharedStorageAccessMethodEnum::Keys;
    case AccessMethod::kValues:
      return Storage::SharedStorageAccessMethodEnum::Values;
    case AccessMethod::kEntries:
      return Storage::SharedStorageAccessMethodEnum::Entries;
    case AccessMethod::kLength:
      return Storage::SharedStorageAccessMethodEnum::Length;
    case AccessMethod::kRemainingBudget:
      return Storage::SharedStorageAccessMethodEnum::RemainingBudget;
  };
  NOTREACHED();
}

}  // namespace

void StorageHandler::OnSharedStorageAccessed(
    base::Time access_time,
    blink::SharedStorageAccessScope scope,
    SharedStorageRuntimeManager::SharedStorageObserverInterface::AccessMethod
        method,
    GlobalRenderFrameHostId main_frame_id,
    const std::string& owner_origin,
    const SharedStorageEventParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using AccessScope = blink::SharedStorageAccessScope;

  std::string scope_enum;
  switch (scope) {
    case AccessScope::kWindow:
      scope_enum = Storage::SharedStorageAccessScopeEnum::Window;
      break;
    case AccessScope::kSharedStorageWorklet:
      scope_enum = Storage::SharedStorageAccessScopeEnum::SharedStorageWorklet;
      break;
    case AccessScope::kProtectedAudienceWorklet:
      scope_enum =
          Storage::SharedStorageAccessScopeEnum::ProtectedAudienceWorklet;
      break;
    case AccessScope::kHeader:
      scope_enum = Storage::SharedStorageAccessScopeEnum::Header;
      break;
  };

  auto protocol_params =
      protocol::Storage::SharedStorageAccessParams::Create().Build();

  if (params.script_source_url) {
    protocol_params->SetScriptSourceUrl(*params.script_source_url);
  }
  if (params.data_origin) {
    protocol_params->SetDataOrigin(*params.data_origin);
  }
  if (params.operation_name) {
    protocol_params->SetOperationName(*params.operation_name);
  }
  if (params.operation_id) {
    protocol_params->SetOperationId(base::NumberToString(*params.operation_id));
  }
  if (params.keep_alive) {
    protocol_params->SetKeepAlive(*params.keep_alive);
  }
  if (params.serialized_data) {
    protocol_params->SetSerializedData(*params.serialized_data);
  }
  if (params.urn_uuid) {
    protocol_params->SetUrnUuid(*params.urn_uuid);
  }
  if (params.key) {
    protocol_params->SetKey(*params.key);
  }
  if (params.value) {
    protocol_params->SetValue(*params.value);
  }
  if (params.ignore_if_present) {
    protocol_params->SetIgnoreIfPresent(*params.ignore_if_present);
  }
  if (params.worklet_ordinal) {
    protocol_params->SetWorkletOrdinal(*params.worklet_ordinal);
  }
  if (!params.worklet_devtools_token.is_empty()) {
    protocol_params->SetWorkletTargetId(
        params.worklet_devtools_token.ToString());
  }
  if (params.with_lock) {
    protocol_params->SetWithLock(*params.with_lock);
  }
  if (params.batch_update_id) {
    protocol_params->SetBatchUpdateId(
        base::NumberToString(*params.batch_update_id));
  }
  if (params.batch_size) {
    protocol_params->SetBatchSize(*params.batch_size);
  }

  if (params.private_aggregation_config) {
    auto protocol_private_aggregation_config =
        protocol::Storage::SharedStoragePrivateAggregationConfig::Create()
            .SetFilteringIdMaxBytes(params.private_aggregation_config->config
                                        ->filtering_id_max_bytes)
            .Build();
    if (params.private_aggregation_config->config
            ->aggregation_coordinator_origin) {
      protocol_private_aggregation_config->SetAggregationCoordinatorOrigin(
          params.private_aggregation_config->config
              ->aggregation_coordinator_origin->Serialize());
    }
    if (params.private_aggregation_config->config->context_id) {
      protocol_private_aggregation_config->SetContextId(
          params.private_aggregation_config->config->context_id.value());
    }
    if (params.private_aggregation_config->config->max_contributions) {
      protocol_private_aggregation_config->SetMaxContributions(
          params.private_aggregation_config->config->max_contributions.value());
    }

    protocol_params->SetPrivateAggregationConfig(
        std::move(protocol_private_aggregation_config));
  }

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

  frontend_->SharedStorageAccessed(
      access_time.InSecondsFSinceUnixEpoch(), scope_enum,
      GetSharedStorageAccessMethodEnum(method),
      GetFrameTokenFromGlobalRenderFrameHostId(main_frame_id), owner_origin,
      net::SchemefulSite(GURL(owner_origin)).Serialize(),
      std::move(protocol_params));
}

void StorageHandler::OnSharedStorageSelectUrlUrnUuidGenerated(
    const GURL& urn_uuid) {}
void StorageHandler::OnSharedStorageSelectUrlConfigPopulated(
    const std::optional<FencedFrameConfig>& config) {}

void StorageHandler::OnSharedStorageWorkletOperationExecutionFinished(
    base::Time finished_time,
    base::TimeDelta execution_time,
    SharedStorageRuntimeManager::SharedStorageObserverInterface::AccessMethod
        method,
    int operation_id,
    const base::UnguessableToken& worklet_devtools_token,
    GlobalRenderFrameHostId main_frame_id,
    const std::string& owner_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  frontend_->SharedStorageWorkletOperationExecutionFinished(
      finished_time.InSecondsFSinceUnixEpoch(), execution_time.InMicroseconds(),
      GetSharedStorageAccessMethodEnum(method),
      base::NumberToString(operation_id), worklet_devtools_token.ToString(),
      GetFrameTokenFromGlobalRenderFrameHostId(main_frame_id), owner_origin);
}

DispatchResponse StorageHandler::SetStorageBucketTracking(
    const std::string& serialized_storage_key,
    bool enable) {
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

void StorageHandler::NotifyInterestGroupAuctionEventOccurred(
    base::Time event_time,
    content::InterestGroupAuctionEventType type,
    const std::string& unique_auction_id,
    base::optional_ref<const std::string> parent_auction_id,
    const base::DictValue& auction_config) {
  if (!interest_group_auction_tracking_enabled_) {
    return;
  }
  std::string type_enum;
  switch (type) {
    case content::InterestGroupAuctionEventType::kStarted:
      type_enum = Storage::InterestGroupAuctionEventTypeEnum::Started;
      break;
    case content::InterestGroupAuctionEventType::kConfigResolved:
      type_enum = Storage::InterestGroupAuctionEventTypeEnum::ConfigResolved;
      break;
  };
  frontend_->InterestGroupAuctionEventOccurred(
      event_time.InSecondsFSinceUnixEpoch(), type_enum, unique_auction_id,
      parent_auction_id.CopyAsOptional(),
      std::make_unique<base::DictValue>(auction_config.Clone()));
}

void StorageHandler::NotifyInterestGroupAuctionNetworkRequestCreated(
    content::InterestGroupAuctionFetchType type,
    const std::string& request_id,
    const std::vector<std::string>& devtools_auction_ids) {
  if (!interest_group_auction_tracking_enabled_) {
    return;
  }
  std::string type_enum;
  switch (type) {
    case content::InterestGroupAuctionFetchType::kBidderJs:
      type_enum = Storage::InterestGroupAuctionFetchTypeEnum::BidderJs;
      break;
    case content::InterestGroupAuctionFetchType::kBidderWasm:
      type_enum = Storage::InterestGroupAuctionFetchTypeEnum::BidderWasm;
      break;
    case content::InterestGroupAuctionFetchType::kSellerJs:
      type_enum = Storage::InterestGroupAuctionFetchTypeEnum::SellerJs;
      break;
    case content::InterestGroupAuctionFetchType::kBidderTrustedSignals:
      type_enum =
          Storage::InterestGroupAuctionFetchTypeEnum::BidderTrustedSignals;
      break;
    case content::InterestGroupAuctionFetchType::kSellerTrustedSignals:
      type_enum =
          Storage::InterestGroupAuctionFetchTypeEnum::SellerTrustedSignals;
      break;
  };
  frontend_->InterestGroupAuctionNetworkRequestCreated(
      type_enum, request_id,
      std::make_unique<std::vector<std::string>>(devtools_auction_ids));
}

Response StorageHandler::SetProtectedAudienceKAnonymity(
    const std::string& in_owner_origin,
    const std::string& in_group_name,
    std::unique_ptr<std::vector<Binary>> in_hashes) {
  url::Origin owner_origin = url::Origin::Create(GURL(in_owner_origin));

  // Ensure we are in "test" mode.
  // For now we just make sure the interest group owner is a .test domain.
  if (!base::EndsWith(owner_origin.host(), ".test")) {
    return Response::ServerError("owner origin must be on a .test domain");
  }

  std::vector<std::string> hashes;
  for (const auto& in_hash : *in_hashes) {
    hashes.emplace_back(base::as_string_view(in_hash));
  }

  InterestGroupManagerImpl* manager = static_cast<InterestGroupManagerImpl*>(
      storage_partition_->GetInterestGroupManager());
  if (!manager) {
    return Response::ServerError("Protected Audience not enabled");
  }
  manager->UpdateKAnonymity(
      blink::InterestGroupKey(std::move(owner_origin), in_group_name),
      /*positive_hashed_keys=*/std::move(hashes),
      /*update_time=*/base::Time::Now(),
      /*replace_existing_values=*/true);
  return Response::Success();
}

}  // namespace protocol
}  // namespace content
