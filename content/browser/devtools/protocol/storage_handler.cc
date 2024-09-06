// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/storage_handler.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/debug_types.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_type.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/browser/attribution_reporting/aggregatable_result.mojom.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/event_level_result.mojom.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.mojom.h"
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
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_manager_observer.mojom-forward.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/quota_override_handle.h"
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
      FrameTreeNodeId main_frame_id,
      const std::string& owner_origin,
      const SharedStorageEventParams& params) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    owner_->NotifySharedStorageAccessed(access_time, type, main_frame_id,
                                        owner_origin, params);
  }

  void OnUrnUuidGenerated(const GURL& urn_uuid) override {}

  void OnConfigPopulated(
      const std::optional<FencedFrameConfig>& config) override {}

 private:
  raw_ptr<StorageHandler> const owner_;
  base::ScopedObservation<
      content::SharedStorageWorkletHostManager,
      content::SharedStorageWorkletHostManager::SharedStorageObserverInterface>
      scoped_observation_{this};
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
        storage_key, blink::mojom::StorageType::kTemporary, false,
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

StorageHandler::StorageHandler(DevToolsAgentHostClient* client)
    : DevToolsDomainHandler(Storage::Metainfo::domainName), client_(client) {}

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
  shared_storage_observer_.reset();
  quota_manager_observer_.reset();
  ResetAttributionReporting();
  return Response::Success();
}

void StorageHandler::GetCookies(Maybe<std::string> browser_context_id,
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
    if (client_->MayAttachToURL(
            GURL(base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator,
                               cookie.DomainWithoutDot()})),
            is_webui) &&
        client_->MayAttachToURL(
            GURL(base::StrCat({url::kHttpScheme, url::kStandardSchemeSeparator,
                               cookie.DomainWithoutDot()})),
            is_webui)) {
      filtered_cookies.emplace_back(std::move(cookie));
    }
  }
  callback->sendSuccess(NetworkHandler::BuildCookieArray(filtered_cookies));
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

namespace {
uint32_t GetRemoveDataMask(const std::string& storage_types) {
  std::vector<std::string> types = base::SplitString(
      storage_types, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::unordered_set<std::string> set(types.begin(), types.end());
  uint32_t remove_mask = 0;
  if (set.count(Storage::StorageTypeEnum::Cookies)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_COOKIES;
  }
  if (set.count(Storage::StorageTypeEnum::File_systems)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS;
  }
  if (set.count(Storage::StorageTypeEnum::Indexeddb)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  }
  if (set.count(Storage::StorageTypeEnum::Local_storage)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  }
  if (set.count(Storage::StorageTypeEnum::Shader_cache)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE;
  }
  if (set.count(Storage::StorageTypeEnum::Websql)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_WEBSQL;
  }
  if (set.count(Storage::StorageTypeEnum::Service_workers)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  }
  if (set.count(Storage::StorageTypeEnum::Cache_storage)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE;
  }
  if (set.count(Storage::StorageTypeEnum::Interest_groups)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_INTEREST_GROUPS;
  }
  if (set.count(Storage::StorageTypeEnum::Shared_storage)) {
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SHARED_STORAGE;
  }
  if (set.count(Storage::StorageTypeEnum::All)) {
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
      remove_mask, StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
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
      remove_mask, StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, *key,
      base::Time(), base::Time::Max(),
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

SharedStorageWorkletHostManager*
StorageHandler::GetSharedStorageWorkletHostManager() {
  DCHECK(storage_partition_);
  return static_cast<StoragePartitionImpl*>(storage_partition_)
      ->GetSharedStorageWorkletHostManager();
}

absl::variant<protocol::Response, storage::SharedStorageManager*>
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
    const Maybe<std::string>& browser_context_id,
    StoragePartition** storage_partition) {
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
          ? Maybe<String>(component_seller_origin->Serialize())
          : Maybe<String>(),
      bid.has_value() ? Maybe<double>(*bid) : Maybe<double>(),
      bid_currency.has_value() ? Maybe<String>(*bid_currency) : Maybe<String>(),
      auction_id.has_value() ? Maybe<String>(*auction_id) : Maybe<String>());
}

namespace {
void SendGetInterestGroup(
    std::unique_ptr<StorageHandler::GetInterestGroupDetailsCallback> callback,
    std::optional<SingleStorageInterestGroup> storage_group) {
  if (!storage_group) {
    callback->sendFailure(Response::ServerError("Interest group not found"));
    return;
  }

  base::Value::Dict ig_serialization =
      SerializeInterestGroupForDevtools(storage_group.value()->interest_group);

  // "joiningOrigin" is in StorageInterestGroup, not InterestGroup, so it needs
  // to be added in separately.
  ig_serialization.Set("joiningOrigin",
                       storage_group.value()->joining_origin.Serialize());
  callback->sendSuccess(
      std::make_unique<base::Value::Dict>(std::move(ig_serialization)));
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
  interest_group_tracking_enabled_ = enable;
  return SetInterestGroupTrackingInternal(storage_partition_, enable);
}

Response StorageHandler::SetInterestGroupTrackingInternal(
    StoragePartition* storage_partition,
    bool enable) {
  if (!storage_partition) {
    return Response::InternalError();
  }

  InterestGroupManagerImpl* manager = static_cast<InterestGroupManagerImpl*>(
      storage_partition->GetInterestGroupManager());
  if (!manager) {
    return Response::ServerError("Interest group storage is disabled.");
  }

  if (enable) {
    // Only add if we are not already registered as an observer. We only
    // observe the interest group manager, so if we're observing anything then
    // we are already registered.
    if (!InterestGroupManagerImpl::InterestGroupObserver::IsInObserverList()) {
      manager->AddInterestGroupObserver(this);
    }
  } else {
    // Removal doesn't care if we are not registered.
    manager->RemoveInterestGroupObserver(this);
  }
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
    if (!GetSharedStorageWorkletHostManager()) {
      return Response::ServerError("Shared storage is disabled.");
    }
    shared_storage_observer_ = std::make_unique<SharedStorageObserver>(this);
  } else {
    shared_storage_observer_.reset();
  }
  return Response::Success();
}

void StorageHandler::ResetSharedStorageBudget(
    const std::string& owner_origin_string,
    std::unique_ptr<ResetSharedStorageBudgetCallback> callback) {
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

  manager->ResetBudgetForDevTools(
      owner_origin,
      base::BindOnce(
          &DispatchSharedStorageCallback<ResetSharedStorageBudgetCallback>,
          std::move(callback)));
}

namespace {

std::string GetFrameTokenFromFrameTreeNodeId(FrameTreeNodeId frame_id) {
  if (frame_id.is_null()) {
    return std::string();
  }
  auto* frame_tree_node = FrameTreeNode::GloballyFindByID(frame_id);
  return frame_tree_node ? frame_tree_node->current_frame_host()
                               ->devtools_frame_token()
                               .ToString()
                         : std::string();
}

}  // namespace

void StorageHandler::NotifySharedStorageAccessed(
    const base::Time& access_time,
    SharedStorageWorkletHostManager::SharedStorageObserverInterface::AccessType
        type,
    FrameTreeNodeId main_frame_id,
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
    case AccessType::kDocumentGet:
      type_enum = Storage::SharedStorageAccessTypeEnum::DocumentGet;
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
    case AccessType::kHeaderSet:
      type_enum = Storage::SharedStorageAccessTypeEnum::HeaderSet;
      break;
    case AccessType::kHeaderAppend:
      type_enum = Storage::SharedStorageAccessTypeEnum::HeaderAppend;
      break;
    case AccessType::kHeaderDelete:
      type_enum = Storage::SharedStorageAccessTypeEnum::HeaderDelete;
      break;
    case AccessType::kHeaderClear:
      type_enum = Storage::SharedStorageAccessTypeEnum::HeaderClear;
      break;
  };

  auto protocol_params =
      protocol::Storage::SharedStorageAccessParams::Create().Build();

  if (params.script_source_url) {
    protocol_params->SetScriptSourceUrl(*params.script_source_url);
  }
  if (params.operation_name) {
    protocol_params->SetOperationName(*params.operation_name);
  }
  if (params.serialized_data) {
    protocol_params->SetSerializedData(*params.serialized_data);
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
      access_time.InSecondsFSinceUnixEpoch(), type_enum,
      GetFrameTokenFromFrameTreeNodeId(main_frame_id), owner_origin,
      std::move(protocol_params));
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

AttributionManager* StorageHandler::GetAttributionManager() {
  if (!storage_partition_) {
    return nullptr;
  }
  return static_cast<StoragePartitionImpl*>(storage_partition_)
      ->GetAttributionManager();
}

void StorageHandler::SetAttributionReportingLocalTestingMode(
    bool enabled,
    std::unique_ptr<SetAttributionReportingLocalTestingModeCallback> callback) {
  auto* manager = GetAttributionManager();
  if (!manager) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  manager->SetDebugMode(
      enabled,
      base::BindOnce(
          &SetAttributionReportingLocalTestingModeCallback::sendSuccess,
          std::move(callback)));
}

void StorageHandler::SendPendingAttributionReports(
    std::unique_ptr<SendPendingAttributionReportsCallback> callback) {
  auto* manager = GetAttributionManager();
  if (!manager) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  manager->GetPendingReportsForInternalUse(
      /*limit=*/-1,
      base::BindOnce(
          [](base::WeakPtr<StorageHandler> storage_handler,
             std::unique_ptr<SendPendingAttributionReportsCallback> callback,
             std::vector<AttributionReport> reports) {
            if (!storage_handler) {
              callback->sendFailure(Response::InternalError());
              return;
            }
            auto* manager = storage_handler->GetAttributionManager();
            if (!manager) {
              callback->sendFailure(Response::InternalError());
              return;
            }
            auto barrier = base::BarrierClosure(
                reports.size(),
                base::BindOnce(
                    &SendPendingAttributionReportsCallback::sendSuccess,
                    std::move(callback), reports.size()));
            for (const auto& report : reports) {
              manager->SendReportForWebUI(report.id(), barrier);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void StorageHandler::ResetAttributionReporting() {
  attribution_observation_.Reset();

  auto* manager = GetAttributionManager();
  if (!manager) {
    return;
  }

  manager->SetDebugMode(/*enabled=*/std::nullopt, base::DoNothing());
}

namespace {

using ::attribution_reporting::mojom::AggregatableResult;
using ::attribution_reporting::mojom::EventLevelResult;
using ::attribution_reporting::mojom::StoreSourceResult;

Storage::AttributionReportingSourceRegistrationResult
ToSourceRegistrationResult(StoreSourceResult result) {
  switch (result) {
    case StoreSourceResult::kSuccess:
      return Storage::AttributionReportingSourceRegistrationResultEnum::Success;
    case StoreSourceResult::kInternalError:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          InternalError;
    case StoreSourceResult::kInsufficientSourceCapacity:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          InsufficientSourceCapacity;
    case StoreSourceResult::kInsufficientUniqueDestinationCapacity:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          InsufficientUniqueDestinationCapacity;
    case StoreSourceResult::kExcessiveReportingOrigins:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          ExcessiveReportingOrigins;
    case StoreSourceResult::kProhibitedByBrowserPolicy:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          ProhibitedByBrowserPolicy;
    case StoreSourceResult::kSuccessNoised:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          SuccessNoised;
    case StoreSourceResult::kDestinationReportingLimitReached:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          DestinationReportingLimitReached;
    case StoreSourceResult::kDestinationGlobalLimitReached:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          DestinationGlobalLimitReached;
    case StoreSourceResult::kDestinationBothLimitsReached:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          DestinationBothLimitsReached;
    case StoreSourceResult::kReportingOriginsPerSiteLimitReached:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          ReportingOriginsPerSiteLimitReached;
    case StoreSourceResult::kExceedsMaxChannelCapacity:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          ExceedsMaxChannelCapacity;
    case StoreSourceResult::kExceedsMaxScopesChannelCapacity:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          ExceedsMaxScopesChannelCapacity;
    case StoreSourceResult::kExceedsMaxTriggerStateCardinality:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          ExceedsMaxTriggerStateCardinality;
    case StoreSourceResult::kExceedsMaxEventStatesLimit:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          ExceedsMaxEventStatesLimit;
    case StoreSourceResult::kDestinationPerDayReportingLimitReached:
      return Storage::AttributionReportingSourceRegistrationResultEnum::
          DestinationPerDayReportingLimitReached;
  }
}

Storage::AttributionReportingEventLevelResult ToEventLevelResult(
    EventLevelResult result) {
  switch (result) {
    case EventLevelResult::kSuccess:
      return Storage::AttributionReportingEventLevelResultEnum::Success;
    case EventLevelResult::kSuccessDroppedLowerPriority:
      return Storage::AttributionReportingEventLevelResultEnum::
          SuccessDroppedLowerPriority;
    case EventLevelResult::kInternalError:
      return Storage::AttributionReportingEventLevelResultEnum::InternalError;
    case EventLevelResult::kNoCapacityForConversionDestination:
      return Storage::AttributionReportingEventLevelResultEnum::
          NoCapacityForAttributionDestination;
    case EventLevelResult::kNoMatchingImpressions:
      return Storage::AttributionReportingEventLevelResultEnum::
          NoMatchingSources;
    case EventLevelResult::kDeduplicated:
      return Storage::AttributionReportingEventLevelResultEnum::Deduplicated;
    case EventLevelResult::kExcessiveAttributions:
      return Storage::AttributionReportingEventLevelResultEnum::
          ExcessiveAttributions;
    case EventLevelResult::kPriorityTooLow:
      return Storage::AttributionReportingEventLevelResultEnum::PriorityTooLow;
    case EventLevelResult::kNeverAttributedSource:
      return Storage::AttributionReportingEventLevelResultEnum::
          NeverAttributedSource;
    case EventLevelResult::kExcessiveReportingOrigins:
      return Storage::AttributionReportingEventLevelResultEnum::
          ExcessiveReportingOrigins;
    case EventLevelResult::kNoMatchingSourceFilterData:
      return Storage::AttributionReportingEventLevelResultEnum::
          NoMatchingSourceFilterData;
    case EventLevelResult::kProhibitedByBrowserPolicy:
      return Storage::AttributionReportingEventLevelResultEnum::
          ProhibitedByBrowserPolicy;
    case EventLevelResult::kNoMatchingConfigurations:
      return Storage::AttributionReportingEventLevelResultEnum::
          NoMatchingConfigurations;
    case EventLevelResult::kExcessiveReports:
      return Storage::AttributionReportingEventLevelResultEnum::
          ExcessiveReports;
    case EventLevelResult::kFalselyAttributedSource:
      return Storage::AttributionReportingEventLevelResultEnum::
          FalselyAttributedSource;
    case EventLevelResult::kReportWindowPassed:
      return Storage::AttributionReportingEventLevelResultEnum::
          ReportWindowPassed;
    case EventLevelResult::kNotRegistered:
      return Storage::AttributionReportingEventLevelResultEnum::NotRegistered;
    case EventLevelResult::kReportWindowNotStarted:
      return Storage::AttributionReportingEventLevelResultEnum::
          ReportWindowNotStarted;
    case EventLevelResult::kNoMatchingTriggerData:
      return Storage::AttributionReportingEventLevelResultEnum::
          NoMatchingTriggerData;
  }
}

Storage::AttributionReportingAggregatableResult ToAggregatableResult(
    AggregatableResult result) {
  switch (result) {
    case AggregatableResult::kSuccess:
      return Storage::AttributionReportingAggregatableResultEnum::Success;
    case AggregatableResult::kInternalError:
      return Storage::AttributionReportingAggregatableResultEnum::InternalError;
    case AggregatableResult::kNoCapacityForConversionDestination:
      return Storage::AttributionReportingAggregatableResultEnum::
          NoCapacityForAttributionDestination;
    case AggregatableResult::kNoMatchingImpressions:
      return Storage::AttributionReportingAggregatableResultEnum::
          NoMatchingSources;
    case AggregatableResult::kExcessiveAttributions:
      return Storage::AttributionReportingAggregatableResultEnum::
          ExcessiveAttributions;
    case AggregatableResult::kExcessiveReportingOrigins:
      return Storage::AttributionReportingAggregatableResultEnum::
          ExcessiveReportingOrigins;
    case AggregatableResult::kNoHistograms:
      return Storage::AttributionReportingAggregatableResultEnum::NoHistograms;
    case AggregatableResult::kInsufficientBudget:
      return Storage::AttributionReportingAggregatableResultEnum::
          InsufficientBudget;
    case AggregatableResult::kNoMatchingSourceFilterData:
      return Storage::AttributionReportingAggregatableResultEnum::
          NoMatchingSourceFilterData;
    case AggregatableResult::kNotRegistered:
      return Storage::AttributionReportingAggregatableResultEnum::NotRegistered;
    case AggregatableResult::kProhibitedByBrowserPolicy:
      return Storage::AttributionReportingAggregatableResultEnum::
          ProhibitedByBrowserPolicy;
    case AggregatableResult::kDeduplicated:
      return Storage::AttributionReportingAggregatableResultEnum::Deduplicated;
    case AggregatableResult::kReportWindowPassed:
      return Storage::AttributionReportingAggregatableResultEnum::
          ReportWindowPassed;
    case AggregatableResult::kExcessiveReports:
      return Storage::AttributionReportingAggregatableResultEnum::
          ExcessiveReports;
  }
}

std::unique_ptr<Array<Storage::AttributionReportingFilterDataEntry>>
ToFilterDataEntries(const attribution_reporting::FilterData& filter_data) {
  auto out =
      std::make_unique<Array<Storage::AttributionReportingFilterDataEntry>>();

  for (const auto& [key, values] : filter_data.filter_values()) {
    out->emplace_back(Storage::AttributionReportingFilterDataEntry::Create()
                          .SetKey(key)
                          .SetValues(std::make_unique<Array<String>>(values))
                          .Build());
  }

  return out;
}

std::unique_ptr<Array<Storage::AttributionReportingFilterConfig>>
ToFilterConfigs(
    const std::vector<attribution_reporting::FilterConfig>& filter_configs) {
  auto out =
      std::make_unique<Array<Storage::AttributionReportingFilterConfig>>();

  for (const auto& config : filter_configs) {
    auto filter_data =
        std::make_unique<Array<Storage::AttributionReportingFilterDataEntry>>();
    for (const auto& [key, values] : config.filter_values()) {
      filter_data->emplace_back(
          Storage::AttributionReportingFilterDataEntry::Create()
              .SetKey(key)
              .SetValues(std::make_unique<Array<String>>(values))
              .Build());
    }

    auto config_entry = Storage::AttributionReportingFilterConfig::Create()
                            .SetFilterValues(std::move(filter_data))
                            .Build();
    if (auto lookback_window = config.lookback_window();
        lookback_window.has_value()) {
      config_entry->SetLookbackWindow(lookback_window->InSeconds());
    }

    out->emplace_back(std::move(config_entry));
  }

  return out;
}

std::unique_ptr<Storage::AttributionReportingFilterPair> ToFilterPair(
    const attribution_reporting::FilterPair& filters) {
  return Storage::AttributionReportingFilterPair::Create()
      .SetFilters(ToFilterConfigs(filters.positive))
      .SetNotFilters(ToFilterConfigs(filters.negative))
      .Build();
}

std::unique_ptr<Array<Storage::AttributionReportingAggregationKeysEntry>>
ToAggregationKeysEntries(const attribution_reporting::AggregationKeys& keys) {
  auto out = std::make_unique<
      Array<Storage::AttributionReportingAggregationKeysEntry>>();

  for (const auto& [key, value] : keys.keys()) {
    out->emplace_back(
        Storage::AttributionReportingAggregationKeysEntry::Create()
            .SetKey(key)
            .SetValue(attribution_reporting::HexEncodeAggregationKey(value))
            .Build());
  }

  return out;
}

std::unique_ptr<Storage::AttributionReportingEventReportWindows>
ToEventReportWindows(const attribution_reporting::EventReportWindows& windows) {
  auto end_times = std::make_unique<Array<int>>();
  for (base::TimeDelta end_time : windows.end_times()) {
    end_times->emplace_back(end_time.InSeconds());
  }
  return Storage::AttributionReportingEventReportWindows::Create()
      .SetStart(windows.start_time().InSeconds())
      .SetEnds(std::move(end_times))
      .Build();
}

std::unique_ptr<Array<Storage::AttributionReportingTriggerSpec>> ToTriggerSpecs(
    const attribution_reporting::TriggerSpecs& specs) {
  auto array =
      std::make_unique<Array<Storage::AttributionReportingTriggerSpec>>();

  for (const auto& spec : specs.specs()) {
    array->emplace_back(Storage::AttributionReportingTriggerSpec::Create()
                            .SetTriggerData(std::make_unique<Array<double>>())
                            .SetEventReportWindows(ToEventReportWindows(
                                spec.event_report_windows()))
                            .Build());
  }

  for (const auto& [trigger_data, spec_index] : specs.trigger_data_indices()) {
    array->at(spec_index)->GetTriggerData()->push_back(trigger_data);
  }

  return array;
}

Storage::AttributionReportingTriggerDataMatching ToTriggerDataMatching(
    attribution_reporting::mojom::TriggerDataMatching value) {
  switch (value) {
    case attribution_reporting::mojom::TriggerDataMatching::kExact:
      return Storage::AttributionReportingTriggerDataMatchingEnum::Exact;
    case attribution_reporting::mojom::TriggerDataMatching::kModulus:
      return Storage::AttributionReportingTriggerDataMatchingEnum::Modulus;
  }
}

std::unique_ptr<Array<Storage::AttributionReportingAggregatableDedupKey>>
ToAggregatableDedupKeys(
    const std::vector<attribution_reporting::AggregatableDedupKey>&
        dedup_keys) {
  auto out = std::make_unique<
      Array<Storage::AttributionReportingAggregatableDedupKey>>();
  for (const auto& dedup_key : dedup_keys) {
    auto dedup_key_entry =
        Storage::AttributionReportingAggregatableDedupKey::Create()
            .SetFilters(ToFilterPair(dedup_key.filters))
            .Build();
    if (dedup_key.dedup_key.has_value()) {
      dedup_key_entry->SetDedupKey(base::NumberToString(*dedup_key.dedup_key));
    }
    out->push_back(std::move(dedup_key_entry));
  }

  return out;
}

std::unique_ptr<Array<Storage::AttributionReportingEventTriggerData>>
ToEventTriggerData(const std::vector<attribution_reporting::EventTriggerData>&
                       event_triggers) {
  auto out =
      std::make_unique<Array<Storage::AttributionReportingEventTriggerData>>();
  for (const auto& event_trigger : event_triggers) {
    auto event_trigger_entry =
        Storage::AttributionReportingEventTriggerData::Create()
            .SetData(base::NumberToString(event_trigger.data))
            .SetPriority(base::NumberToString(event_trigger.priority))
            .SetFilters(ToFilterPair(event_trigger.filters))
            .Build();
    if (event_trigger.dedup_key.has_value()) {
      event_trigger_entry->SetDedupKey(
          base::NumberToString(*event_trigger.dedup_key));
    }
    out->push_back(std::move(event_trigger_entry));
  }

  return out;
}

std::unique_ptr<Array<Storage::AttributionReportingAggregatableTriggerData>>
ToAggregatableTriggerData(
    const std::vector<attribution_reporting::AggregatableTriggerData>&
        aggregatable_triggers) {
  auto out = std::make_unique<
      Array<Storage::AttributionReportingAggregatableTriggerData>>();
  for (const auto& aggregatable_trigger : aggregatable_triggers) {
    out->emplace_back(
        Storage::AttributionReportingAggregatableTriggerData::Create()
            .SetKeyPiece(attribution_reporting::HexEncodeAggregationKey(
                aggregatable_trigger.key_piece()))
            .SetSourceKeys(std::make_unique<Array<String>>(
                aggregatable_trigger.source_keys().begin(),
                aggregatable_trigger.source_keys().end()))
            .SetFilters(ToFilterPair(aggregatable_trigger.filters()))
            .Build());
  }

  return out;
}

std::unique_ptr<Array<Storage::AttributionReportingAggregatableValueDictEntry>>
ToAggregatableValueDictEntries(
    const attribution_reporting::AggregatableValues::Values&
        aggregatable_value) {
  auto out = std::make_unique<
      Array<Storage::AttributionReportingAggregatableValueDictEntry>>();
  out->reserve(aggregatable_value.size());
  for (const auto& [key, value] : aggregatable_value) {
    out->emplace_back(
        Storage::AttributionReportingAggregatableValueDictEntry::Create()
            .SetKey(key)
            .SetValue(value.value())
            .SetFilteringId(base::NumberToString(value.filtering_id()))
            .Build());
  }

  return out;
}

std::unique_ptr<Array<Storage::AttributionReportingAggregatableValueEntry>>
ToAggregatableValueEntries(
    const std::vector<attribution_reporting::AggregatableValues>&
        aggregatable_values) {
  auto out = std::make_unique<
      Array<Storage::AttributionReportingAggregatableValueEntry>>();
  out->reserve(aggregatable_values.size());
  for (const auto& aggregatable_value : aggregatable_values) {
    out->emplace_back(
        Storage::AttributionReportingAggregatableValueEntry::Create()
            .SetValues(
                ToAggregatableValueDictEntries(aggregatable_value.values()))
            .SetFilters(ToFilterPair(aggregatable_value.filters()))
            .Build());
  }

  return out;
}

Storage::AttributionReportingSourceRegistrationTimeConfig
ToSourceRegistrationTimeConfig(
    attribution_reporting::mojom::SourceRegistrationTimeConfig
        source_registration_time_config) {
  switch (source_registration_time_config) {
    case attribution_reporting::mojom::SourceRegistrationTimeConfig::kInclude:
      return Storage::AttributionReportingSourceRegistrationTimeConfigEnum::
          Include;
    case attribution_reporting::mojom::SourceRegistrationTimeConfig::kExclude:
      return Storage::AttributionReportingSourceRegistrationTimeConfigEnum::
          Exclude;
  }
}

std::unique_ptr<
    Array<Storage::AttributionReportingAggregatableDebugReportingData>>
ToAggregatableDebugReportingDataArray(
    const attribution_reporting::AggregatableDebugReportingConfig::DebugData&
        data) {
  auto out = std::make_unique<
      Array<Storage::AttributionReportingAggregatableDebugReportingData>>();
  for (const auto& [type, contribution] : data) {
    auto types = std::make_unique<Array<String>>();
    types->emplace_back(attribution_reporting::SerializeDebugDataType(type));
    out->emplace_back(
        Storage::AttributionReportingAggregatableDebugReportingData::Create()
            .SetKeyPiece(attribution_reporting::HexEncodeAggregationKey(
                contribution.key_piece()))
            .SetValue(contribution.value())
            .SetTypes(std::move(types))
            .Build());
  }
  return out;
}

std::unique_ptr<Storage::AttributionReportingAggregatableDebugReportingConfig>
ToAggregatableDebugReportingConfig(
    std::optional<double> budget,
    const attribution_reporting::AggregatableDebugReportingConfig& config) {
  auto out_config =
      Storage::AttributionReportingAggregatableDebugReportingConfig::Create()
          .SetKeyPiece(
              attribution_reporting::HexEncodeAggregationKey(config.key_piece))
          .SetDebugData(
              ToAggregatableDebugReportingDataArray(config.debug_data))
          .Build();
  if (budget.has_value()) {
    out_config->SetBudget(*budget);
  }
  if (config.aggregation_coordinator_origin.has_value()) {
    out_config->SetAggregationCoordinatorOrigin(
        config.aggregation_coordinator_origin->Serialize());
  }
  return out_config;
}

}  // namespace

void StorageHandler::OnSourceHandled(
    const StorableSource& source,
    base::Time source_time,
    std::optional<uint64_t> cleared_debug_key,
    attribution_reporting::mojom::StoreSourceResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto& registration = source.registration();

  auto destination_sites = std::make_unique<Array<String>>();
  for (const auto& site : registration.destination_set.destinations()) {
    destination_sites->emplace_back(site.Serialize());
  }

  const auto& common_info = source.common_info();
  const auto& aggregatable_debug_reporting_config =
      registration.aggregatable_debug_reporting_config;
  auto out_source =
      Storage::AttributionReportingSourceRegistration::Create()
          .SetTime(source_time.InSecondsFSinceUnixEpoch())
          .SetType(
              attribution_reporting::SourceTypeName(common_info.source_type()))
          .SetSourceOrigin(common_info.source_origin()->Serialize())
          .SetReportingOrigin(common_info.reporting_origin()->Serialize())
          .SetDestinationSites(std::move(destination_sites))
          .SetEventId(base::NumberToString(registration.source_event_id))
          .SetPriority(base::NumberToString(registration.priority))
          .SetFilterData(ToFilterDataEntries(registration.filter_data))
          .SetAggregationKeys(
              ToAggregationKeysEntries(registration.aggregation_keys))
          .SetExpiry(registration.expiry.InSeconds())
          .SetTriggerSpecs(ToTriggerSpecs(registration.trigger_specs))
          .SetAggregatableReportWindow(
              registration.aggregatable_report_window.InSeconds())
          .SetTriggerDataMatching(
              ToTriggerDataMatching(registration.trigger_data_matching))
          .SetDestinationLimitPriority(
              base::NumberToString(registration.destination_limit_priority))
          .SetAggregatableDebugReportingConfig(
              ToAggregatableDebugReportingConfig(
                  aggregatable_debug_reporting_config.budget(),
                  aggregatable_debug_reporting_config.config()))
          .Build();

  if (registration.debug_key.has_value()) {
    out_source->SetDebugKey(base::NumberToString(*registration.debug_key));
  }

  if (const std::optional<attribution_reporting::AttributionScopesData>&
          attribution_scopes_data = registration.attribution_scopes_data) {
    out_source->SetScopesData(
        Storage::AttributionScopesData::Create()
            .SetValues(std::make_unique<Array<String>>(
                attribution_scopes_data->attribution_scopes_set()
                    .scopes()
                    .begin(),
                attribution_scopes_data->attribution_scopes_set()
                    .scopes()
                    .end()))
            .SetLimit(attribution_scopes_data->attribution_scope_limit())
            .SetMaxEventStates(attribution_scopes_data->max_event_states())
            .Build());
  }

  frontend_->AttributionReportingSourceRegistered(
      std::move(out_source), ToSourceRegistrationResult(result));
}

void StorageHandler::OnTriggerHandled(std::optional<uint64_t> cleared_debug_key,
                                      const CreateReportResult& result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto& registration = result.trigger().registration();

  auto out_trigger =
      Storage::AttributionReportingTriggerRegistration::Create()
          .SetFilters(ToFilterPair(registration.filters))
          .SetAggregatableDedupKeys(
              ToAggregatableDedupKeys(registration.aggregatable_dedup_keys))
          .SetEventTriggerData(ToEventTriggerData(registration.event_triggers))
          .SetAggregatableTriggerData(
              ToAggregatableTriggerData(registration.aggregatable_trigger_data))
          .SetAggregatableValues(
              ToAggregatableValueEntries(registration.aggregatable_values))
          .SetAggregatableFilteringIdMaxBytes(
              registration.aggregatable_trigger_config
                  .aggregatable_filtering_id_max_bytes()
                  .value())
          .SetDebugReporting(registration.debug_reporting)
          .SetSourceRegistrationTimeConfig(ToSourceRegistrationTimeConfig(
              registration.aggregatable_trigger_config
                  .source_registration_time_config()))
          .SetAggregatableDebugReportingConfig(
              ToAggregatableDebugReportingConfig(
                  /*budget=*/std::nullopt,
                  registration.aggregatable_debug_reporting_config))
          .SetScopes(std::make_unique<Array<String>>(
              registration.attribution_scopes.scopes().begin(),
              registration.attribution_scopes.scopes().end()))
          .Build();

  if (registration.debug_key.has_value()) {
    out_trigger->SetDebugKey(base::NumberToString(*registration.debug_key));
  }
  if (registration.aggregation_coordinator_origin.has_value()) {
    out_trigger->SetAggregationCoordinatorOrigin(
        registration.aggregation_coordinator_origin->Serialize());
  }
  if (registration.aggregatable_trigger_config.trigger_context_id()
          .has_value()) {
    out_trigger->SetTriggerContextId(
        *registration.aggregatable_trigger_config.trigger_context_id());
  }

  frontend_->AttributionReportingTriggerRegistered(
      std::move(out_trigger), ToEventLevelResult(result.event_level_status()),
      ToAggregatableResult(result.aggregatable_status()));
}

Response StorageHandler::SetAttributionReportingTracking(bool enable) {
  if (enable) {
    auto* manager = GetAttributionManager();
    if (!manager) {
      return Response::ServerError("Attribution Reporting is disabled.");
    }
    // Prevent `DCHECK` crashes in `base::ScopedObservation::Observe()` when we
    // are already observing.
    if (!attribution_observation_.IsObserving()) {
      attribution_observation_.Observe(manager);
    }
  } else {
    attribution_observation_.Reset();
  }
  return Response::Success();
}

void StorageHandler::NotifyInterestGroupAuctionEventOccurred(
    base::Time event_time,
    content::InterestGroupAuctionEventType type,
    const std::string& unique_auction_id,
    base::optional_ref<const std::string> parent_auction_id,
    const base::Value::Dict& auction_config) {
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
      parent_auction_id.has_value() ? Maybe<String>(*parent_auction_id)
                                    : Maybe<String>(),
      std::make_unique<base::Value::Dict>(auction_config.Clone()));
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

}  // namespace protocol
}  // namespace content
