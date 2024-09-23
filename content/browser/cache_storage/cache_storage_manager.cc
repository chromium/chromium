// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_manager.h"

#include <stdint.h>

#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/id_map.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/sha1.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/constants.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/storage_directory_util.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

bool DeleteDir(const base::FilePath& path) {
  return base::DeletePathRecursively(path);
}

void DeleteBucketDidDeleteDir(
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
    bool rv) {
  // On scheduler sequence.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     rv ? blink::mojom::QuotaStatusCode::kOk
                        : blink::mojom::QuotaStatusCode::kErrorAbort));
}

void DidDeleteAllBuckets(
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
    std::vector<blink::mojom::QuotaStatusCode> results) {
  auto status = blink::mojom::QuotaStatusCode::kOk;
  for (auto result : results) {
    if (result != blink::mojom::QuotaStatusCode::kOk) {
      status = result;
      break;
    }
  }
  // On scheduler sequence.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status));
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IndexResult {
  kOk = 0,
  kFailedToParse = 1,
  kMissingOrigin = 2,
  kEmptyOriginUrl = 3,
  kPathMismatch = 4,
  kPathFileInfoFailed = 5,
  kInvalidStorageKey = 6,
  // Add new enums above
  kMaxValue = kInvalidStorageKey,
};

IndexResult ValidateIndex(proto::CacheStorageIndex index) {
  if (!index.has_origin()) {
    return IndexResult::kMissingOrigin;
  }

  GURL url(index.origin());
  if (url.is_empty()) {
    return IndexResult::kEmptyOriginUrl;
  }

  // TODO(crbug.com/40177656): Consider adding a
  // 'index.has_storage_key()' check here once we've ensured that a
  // sufficient number of CacheStorage instances have been migrated (or
  // verified that `ValidateIndex` won't be passed an unmigrated `index`).
  return IndexResult::kOk;
}

void RecordIndexValidationResult(IndexResult value) {
  base::UmaHistogramEnumeration("ServiceWorkerCache.ListOriginsIndexValidity",
                                value);
}

base::FilePath ConstructOriginPath(const base::FilePath& profile_path,
                                   const url::Origin& origin,
                                   storage::mojom::CacheStorageOwner owner) {
  base::FilePath first_party_default_root_path =
      CacheStorageManager::ConstructFirstPartyDefaultRootPath(profile_path);

  std::string identifier = storage::GetIdentifierFromOrigin(origin);
  if (owner != storage::mojom::CacheStorageOwner::kCacheAPI) {
    identifier += "-" + base::NumberToString(static_cast<int>(owner));
  }
  const std::string origin_hash_hex = base::ToLowerASCII(
      base::HexEncode(base::SHA1Hash(base::as_byte_span(identifier))));
  return first_party_default_root_path.AppendASCII(origin_hash_hex);
}

void ValidateAndAddBucketFromPath(
    const base::FilePath& index_file_directory_path,
    storage::mojom::CacheStorageOwner owner,
    const base::FilePath& profile_path,
    std::vector<storage::BucketLocator>& buckets,
    bool is_origin_path = false) {
  if (!base::PathExists(index_file_directory_path)) {
    return;
  }
  base::FilePath index_path =
      index_file_directory_path.AppendASCII(CacheStorage::kIndexFileName);
  std::string protobuf;
  base::ReadFileToString(index_path, &protobuf);

  proto::CacheStorageIndex index;
  if (!index.ParseFromString(protobuf)) {
    RecordIndexValidationResult(IndexResult::kFailedToParse);
    return;
  }

  IndexResult rv = ValidateIndex(index);
  if (rv != IndexResult::kOk) {
    RecordIndexValidationResult(rv);
    return;
  }

  blink::StorageKey storage_key;
  if (index.has_storage_key()) {
    std::optional<blink::StorageKey> result =
        blink::StorageKey::Deserialize(index.storage_key());
    if (!result) {
      RecordIndexValidationResult(IndexResult::kInvalidStorageKey);
      return;
    }
    storage_key = result.value();
  } else {
    // TODO(crbug.com/40177656): Since index file migrations happen
    // lazily, it's plausible that the index file we are reading doesn't have
    // a storage key yet. For now, fall back to creating the storage key
    // from the origin. Once enough time has passed it should be safe to treat
    // this case as an index validation error.
    storage_key = blink::StorageKey::CreateFirstParty(
        url::Origin::Create(GURL(index.origin())));
  }
  DCHECK(!storage_key.origin().GetURL().is_empty());

  storage::BucketLocator bucket_locator{};

  if (index.has_bucket_id() && index.has_bucket_is_default()) {
    // We'll populate the bucket locator using the information from the index
    // file, but it's not guaranteed that this will be valid.
    bucket_locator = storage::BucketLocator(
        storage::BucketId(index.bucket_id()), storage_key,
        blink::mojom::StorageType::kTemporary, index.bucket_is_default());
  } else {
    // If the index file has no bucket information then it's from before we
    // had non-default buckets and third-party storage partitioning
    // implemented. That means these index files will always use the
    // origin-based path format. Populate our BucketLocator with enough
    // data to construct the appropriate path from it below.
    bucket_locator = storage::BucketLocator::ForDefaultBucket(storage_key);
    // TODO(crbug.com/40185498): Once enough time has passed it should be
    // safe to treat this case as an index validation error.
  }

  auto bucket_path = CacheStorageManager::ConstructBucketPath(
      profile_path, bucket_locator, owner);
  if (index_file_directory_path != bucket_path) {
    if (is_origin_path) {
      // For paths corresponding to the legacy Cache Storage directory structure
      // (where the bucket ID is not in the path),
      // `ValidateAndAddBucketFromPath()` can get called with an
      // `index_file_directory_path` that corresponds to a Cache Storage
      // instance from a different `owner`. That is valid and expected because
      // the directory entries from different owners are stored alongside each
      // other and are not easily distinguishable (the directory name is a
      // hash of the origin + the owner), so it's easiest to just compute the
      // two possible origin paths here and compare. If the path doesn't match
      // the calculated path for either `owner` then it is invalid. With the new
      // directory structure for non-default buckets and third-party contexts,
      // we only call `ValidateAndAddBucketFromPath()` with Cache Storage
      // instances for the appropriate owner, so this check isn't needed in that
      // case. We return early if this instance corresponds to another owner to
      // avoid recording an index validation error and polluting the metrics
      // derived from them.
      storage::mojom::CacheStorageOwner other_owner =
          owner == storage::mojom::CacheStorageOwner::kCacheAPI
              ? storage::mojom::CacheStorageOwner::kBackgroundFetch
              : storage::mojom::CacheStorageOwner::kCacheAPI;
      auto other_owner_path = CacheStorageManager::ConstructBucketPath(
          profile_path, bucket_locator, other_owner);
      if (index_file_directory_path == other_owner_path) {
        return;
      }
    }
    RecordIndexValidationResult(IndexResult::kPathMismatch);
    return;
  }

  buckets.emplace_back(bucket_locator);
  RecordIndexValidationResult(IndexResult::kOk);
}

// Open the various cache directories' index files and extract their bucket
// locators.
void GetBucketsFromDiskOnTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
    std::vector<storage::BucketLocator> buckets,
    base::FilePath profile_path,
    storage::mojom::CacheStorageOwner owner,
    base::OnceCallback<void(std::vector<storage::BucketLocator>)> callback) {
  // Add entries to `buckets` from the directory for default buckets
  // corresponding to first-party contexts.
  {
    base::FilePath first_party_default_buckets_root_path =
        CacheStorageManager::ConstructFirstPartyDefaultRootPath(profile_path);

    base::FileEnumerator file_enum(first_party_default_buckets_root_path,
                                   false /* recursive */,
                                   base::FileEnumerator::DIRECTORIES);

    base::FilePath path;
    while (!(path = file_enum.Next()).empty()) {
      ValidateAndAddBucketFromPath(path, owner, profile_path, buckets,
                                   true /* is_origin_path */);
    }
  }

  // Add entries to `buckets` from the directory for non-default
  // buckets and for buckets corresponding to third-party contexts.
  base::FilePath third_party_and_non_default_root_path =
      CacheStorageManager::ConstructThirdPartyAndNonDefaultRootPath(
          profile_path);
  {
    base::FileEnumerator file_enum(third_party_and_non_default_root_path,
                                   false /* recursive */,
                                   base::FileEnumerator::DIRECTORIES);
    base::FilePath path;
    while (!(path = file_enum.Next()).empty()) {
      base::FilePath cache_storage_path =
          owner == storage::mojom::CacheStorageOwner::kCacheAPI
              ? path.Append(storage::kCacheStorageDirectory)
              : path.Append(storage::kBackgroundFetchDirectory);
      if (!base::PathExists(cache_storage_path)) {
        continue;
      }
      ValidateAndAddBucketFromPath(cache_storage_path, owner, profile_path,
                                   buckets);
    }
  }

  // Don't attempt to resolve any missing bucket IDs.
  scheduler_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(buckets)));
  return;
}

// Match a bucket for deletion if its storage key matches any of the given
// storage keys.
//
// This function considers a bucket to match a storage key if either the
// bucket's key's origin matches the storage key's origin or the bucket's key is
// third-party and its top-level site matches the origin.
bool BucketMatchesOriginsForDeletion(
    const storage::BucketLocator& bucket_locator,
    const std::set<url::Origin>& origins) {
  auto& bucket_key = bucket_locator.storage_key;

  for (auto& requested_origin : origins) {
    if (bucket_key.origin() == requested_origin ||
        (bucket_key.IsThirdPartyContext() &&
         bucket_key.top_level_site() == net::SchemefulSite(requested_origin))) {
      return true;
    }
  }

  return false;
}

}  // namespace

// static
scoped_refptr<CacheStorageManager> CacheStorageManager::Create(
    const base::FilePath& profile_path,
    scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
    scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
    base::WeakPtr<CacheStorageDispatcherHost> cache_storage_dispatcher_host) {
  DCHECK(cache_task_runner);
  DCHECK(scheduler_task_runner);
  DCHECK(quota_manager_proxy);
  DCHECK(blob_storage_context);

  return base::WrapRefCounted(new CacheStorageManager(
      profile_path, std::move(cache_task_runner),
      std::move(scheduler_task_runner), std::move(quota_manager_proxy),
      std::move(blob_storage_context),
      std::move(cache_storage_dispatcher_host)));
}

// static
scoped_refptr<CacheStorageManager> CacheStorageManager::CreateForTesting(
    CacheStorageManager* old_manager) {
  scoped_refptr<CacheStorageManager> manager(new CacheStorageManager(
      old_manager->profile_path(), old_manager->cache_task_runner(),
      old_manager->scheduler_task_runner(), old_manager->quota_manager_proxy_,
      old_manager->blob_storage_context_,
      old_manager->cache_storage_dispatcher_host_));
  return manager;
}

CacheStorageManager::~CacheStorageManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

#if DCHECK_IS_ON()
bool CacheStorageManager::CacheStoragePathIsUnique(const base::FilePath& path) {
  for (const auto& bucket_info : cache_storage_map_) {
    const auto& bucket_locator = bucket_info.first.first;
    const auto owner = bucket_info.first.second;
    if (path == CacheStorageManager::ConstructBucketPath(
                    profile_path_, bucket_locator, owner)) {
      return false;
    }
  }
  return true;
}
#endif

// Like `CacheStorageManager::CacheStoragePathIsUnique()`, this checks whether
// there's an existing entry in `cache_storage_map_` that would share the same
// directory path for the given `owner` and `bucket_locator`.
bool CacheStorageManager::ConflictingInstanceExistsInMap(
    storage::mojom::CacheStorageOwner owner,
    const storage::BucketLocator& bucket_locator) {
  DCHECK(bucket_locator.type == blink::mojom::StorageType::kTemporary);

  if (IsMemoryBacked() || !bucket_locator.is_default ||
      !bucket_locator.storage_key.IsFirstPartyContext()) {
    return false;
  }
  CacheStorageMap::const_iterator it =
      cache_storage_map_.find({bucket_locator, owner});
  if (it != cache_storage_map_.end()) {
    // If there's an entry in the map for a given BucketLocator then assume
    // there are no conflicts.
    return false;
  }
  // Note: since the number of CacheStorage instances is usually small, just
  // search for any `storage::BucketLocator` keys with a matching
  // `blink::StorageKey`.
  for (const auto& key_value : cache_storage_map_) {
    if (key_value.first.second != owner) {
      continue;
    }
    if (!key_value.first.first.is_default ||
        key_value.first.first.storage_key != bucket_locator.storage_key) {
      continue;
    }
    DCHECK(key_value.first.first.type == blink::mojom::StorageType::kTemporary);

    // An existing entry has a different bucket ID and/or type, which means
    // these entries will use the same directory path.
    return true;
  }
  return false;
}

CacheStorageHandle CacheStorageManager::OpenCacheStorage(
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Wait to create the MemoryPressureListener until the first CacheStorage
  // object is needed.  This ensures we create the listener on the correct
  // thread.
  if (!memory_pressure_listener_) {
    memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
        FROM_HERE, base::BindRepeating(&CacheStorageManager::OnMemoryPressure,
                                       base::Unretained(this)));
  }

  CacheStorageMap::const_iterator it =
      cache_storage_map_.find({bucket_locator, owner});
  if (it == cache_storage_map_.end()) {
    const auto bucket_path = CacheStorageManager::ConstructBucketPath(
        profile_path_, bucket_locator, owner);
#if DCHECK_IS_ON()
    // Each CacheStorage instance expects to exclusively own it's corresponding
    // origin / bucket path, and if it doesn't then the underlying scheduler for
    // that instance will block, maybe indefinitely, if an existing instance
    // using that directory is alive. The effects of a stalled scheduler can
    // manifest in peculiar ways, so to make debugging easier emit a warning
    // here if we observe that there will be path conflicts. One case where this
    // can happen is when a bucket was deleted very recently and a CacheStorage
    // instance is created using a new bucket id (for the same storage key /
    // default bucket). If we haven't yet deleted the existing CacheStorage
    // instance via `DeleteBucketData()` then we will temporarily hit this
    // condition. This should be fine, though, because once the original
    // CacheStorage instance is deleted the scheduler of the second instance
    // will no longer be blocked.
    DLOG_IF(WARNING, !CacheStoragePathIsUnique(bucket_path))
        << "Multiple CacheStorage instances using the same directory detected";
#endif
    CacheStorage* cache_storage = new CacheStorage(
        bucket_path, IsMemoryBacked(), cache_task_runner_.get(),
        scheduler_task_runner_, quota_manager_proxy_, blob_storage_context_,
        this, bucket_locator, owner);
    cache_storage_map_[{bucket_locator, owner}] =
        base::WrapUnique(cache_storage);
    return cache_storage->CreateHandle();
  }
  return it->second.get()->CreateHandle();
}

void CacheStorageManager::NotifyCacheListChanged(
    const storage::BucketLocator& bucket_locator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& observer : observers_) {
    observer->OnCacheListChanged(bucket_locator);
  }
}

void CacheStorageManager::NotifyCacheContentChanged(
    const storage::BucketLocator& bucket_locator,
    const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& observer : observers_) {
    observer->OnCacheContentChanged(bucket_locator, name);
  }
}

void CacheStorageManager::CacheStorageUnreferenced(
    CacheStorage* cache_storage,
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cache_storage);
  cache_storage->AssertUnreferenced();
  auto it = cache_storage_map_.find({bucket_locator, owner});
  CHECK(it != cache_storage_map_.end(), base::NotFatalUntil::M130);
  DCHECK(it->second.get() == cache_storage);

  // Currently we don't do anything when a CacheStorage instance becomes
  // unreferenced.  In the future we will deallocate some or all of the
  // CacheStorage's state.
}

void CacheStorageManager::GetBucketUsage(
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::GetBucketUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsMemoryBacked()) {
    auto it = cache_storage_map_.find({bucket_locator, owner});
    if (it == cache_storage_map_.end()) {
      scheduler_task_runner_->PostTask(FROM_HERE,
                                       base::BindOnce(std::move(callback),
                                                      /*usage=*/0));
      return;
    }
    CacheStorageHandle cache_storage = OpenCacheStorage(bucket_locator, owner);
    CacheStorage::From(cache_storage)->Size(std::move(callback));
    return;
  }
  cache_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&base::PathExists,
                     CacheStorageManager::ConstructBucketPath(
                         profile_path_, bucket_locator, owner)),
      base::BindOnce(&CacheStorageManager::GetBucketUsageDidGetExists,
                     weak_ptr_factory_.GetWeakPtr(), bucket_locator, owner,
                     std::move(callback)));
}

void CacheStorageManager::GetBucketUsageDidGetExists(
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::GetBucketUsageCallback callback,
    bool exists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!exists || ConflictingInstanceExistsInMap(owner, bucket_locator)) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*usage=*/0));
    return;
  }

  CacheStorageHandle cache_storage = OpenCacheStorage(bucket_locator, owner);
  CacheStorage::From(cache_storage)->Size(std::move(callback));
}

// Used by QuotaClient which only wants the storage keys that have data in the
// default bucket.
void CacheStorageManager::GetStorageKeys(
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::GetStorageKeysForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsMemoryBacked()) {
    std::vector<blink::StorageKey> storage_keys;
    for (const auto& key_value : cache_storage_map_) {
      if (key_value.first.second != owner) {
        continue;
      }

      const storage::BucketLocator& bucket_locator = key_value.first.first;
      if (!bucket_locator.is_default) {
        continue;
      }

      storage_keys.push_back(bucket_locator.storage_key);
    }

    scheduler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(storage_keys)));
    return;
  }

  std::vector<storage::BucketLocator> buckets;
  cache_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetBucketsFromDiskOnTaskRunner,
          base::WrapRefCounted(scheduler_task_runner_.get()),
          std::move(buckets), profile_path_, owner,
          base::BindOnce(&CacheStorageManager::ListStorageKeysOnTaskRunner,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void CacheStorageManager::DeleteOriginsDataGotAllBucketInfo(
    const std::set<url::Origin>& origins,
    storage::mojom::CacheStorageOwner owner,
    base::OnceCallback<void(blink::mojom::QuotaStatusCode)> callback,
    std::vector<storage::BucketLocator> buckets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (buckets.empty()) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::mojom::QuotaStatusCode::kOk));
    return;
  }

  const auto barrier_callback =
      base::BarrierCallback<blink::mojom::QuotaStatusCode>(
          buckets.size(),
          base::BindOnce(&DidDeleteAllBuckets, std::move(callback)));

  for (const storage::BucketLocator& bucket_locator : buckets) {
    if (!BucketMatchesOriginsForDeletion(bucket_locator, origins)) {
      barrier_callback.Run(blink::mojom::QuotaStatusCode::kOk);
      continue;
    }
    if (!bucket_locator.is_null()) {
      // The bucket locator is fully formed, so use the same steps to delete as
      // `DeleteBucketData()`.
      DeleteBucketDataDidGetExists(owner, barrier_callback, bucket_locator,
                                   /*exists=*/true);
    } else {
      // This must be for an unmigrated cache storage instance using an origin
      // path, so just directly delete the directory.
      cache_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&DeleteDir, CacheStorageManager::ConstructBucketPath(
                                         profile_path_, bucket_locator, owner)),
          base::BindOnce(&DeleteBucketDidDeleteDir, barrier_callback));
    }
  }
}

void CacheStorageManager::DeleteOriginData(
    const std::set<url::Origin>& origins,
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (origins.empty()) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::mojom::QuotaStatusCode::kOk));
    return;
  }

  if (IsMemoryBacked()) {
    std::vector<storage::BucketLocator> to_delete;
    to_delete.reserve(origins.size());

    // Note: since the number of CacheStorage instances is usually small, just
    // search for the corresponding `storage::BucketLocator` keys, given a
    // `blink::StorageKey`.
    for (const auto& key_value : cache_storage_map_) {
      if (key_value.first.second != owner) {
        continue;
      }
      to_delete.emplace_back(key_value.first.first);
    }

    DeleteOriginsDataGotAllBucketInfo(origins, owner, std::move(callback),
                                      std::move(to_delete));
    return;
  }

  std::vector<storage::BucketLocator> buckets;
  cache_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetBucketsFromDiskOnTaskRunner,
          base::WrapRefCounted(scheduler_task_runner_.get()),
          std::move(buckets), profile_path_, owner,
          base::BindOnce(
              &CacheStorageManager::DeleteOriginsDataGotAllBucketInfo,
              weak_ptr_factory_.GetWeakPtr(), origins, owner,
              std::move(callback))));
}

void CacheStorageManager::DeleteBucketData(
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // `DeleteBucketData()` is called when the bucket corresponding to
  // `bucket_locator` has been destroyed, which means we should notify
  // `cache_storage_dispatcher_host_` that any `CacheStorageImpl` objects with a
  // cached version of this `BucketLocator` should invalidate the copy they
  // have.
  if (cache_storage_dispatcher_host_) {
    cache_storage_dispatcher_host_->NotifyBucketDataDeleted(bucket_locator);
  }

  if (IsMemoryBacked()) {
    auto it = cache_storage_map_.find({bucket_locator, owner});
    if (it == cache_storage_map_.end()) {
      scheduler_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    blink::mojom::QuotaStatusCode::kOk));
      return;
    }
    DeleteBucketDataDidGetExists(owner, std::move(callback), bucket_locator,
                                 /*exists=*/true);
    return;
  }

  cache_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&base::PathExists,
                     CacheStorageManager::ConstructBucketPath(
                         profile_path_, bucket_locator, owner)),
      base::BindOnce(&CacheStorageManager::DeleteBucketDataDidGetExists,
                     weak_ptr_factory_.GetWeakPtr(), owner, std::move(callback),
                     bucket_locator));
}

void CacheStorageManager::DeleteBucketDataDidGetExists(
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
    storage::BucketLocator bucket_locator,
    bool exists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!exists || ConflictingInstanceExistsInMap(owner, bucket_locator)) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::mojom::QuotaStatusCode::kOk));
    return;
  }

  // Create the CacheStorage for the bucket if it hasn't been loaded yet.
  CacheStorageHandle handle = OpenCacheStorage(bucket_locator, owner);

  auto it = cache_storage_map_.find({bucket_locator, owner});
  CHECK(it != cache_storage_map_.end(), base::NotFatalUntil::M130);

  CacheStorage* cache_storage = it->second.release();
  cache_storage->ResetManager();
  cache_storage_map_.erase({bucket_locator, owner});

  cache_storage->GetSizeThenCloseAllCaches(
      base::BindOnce(&CacheStorageManager::DeleteBucketDidClose,
                     weak_ptr_factory_.GetWeakPtr(), bucket_locator, owner,
                     std::move(callback), base::WrapUnique(cache_storage)));
}

void CacheStorageManager::AddObserver(
    mojo::PendingRemote<storage::mojom::CacheStorageObserver> observer) {
  observers_.Add(std::move(observer));
}

void CacheStorageManager::DeleteBucketDidClose(
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
    std::unique_ptr<CacheStorage> cache_storage,
    int64_t bucket_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(jkarlin): Deleting the storage leaves any unfinished operations
  // hanging, resulting in unresolved promises. Fix this by returning early from
  // CacheStorage operations posted after GetSizeThenCloseAllCaches is called.
  cache_storage.reset();

  quota_manager_proxy_->NotifyBucketModified(
      CacheStorageQuotaClient::GetClientTypeFromOwner(owner), bucket_locator,
      -bucket_size, base::Time::Now(),
      base::SequencedTaskRunner::GetCurrentDefault(), base::DoNothing());

  if (owner == storage::mojom::CacheStorageOwner::kCacheAPI) {
    NotifyCacheListChanged(bucket_locator);
  }

  if (IsMemoryBacked()) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::mojom::QuotaStatusCode::kOk));
    return;
  }

  cache_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DeleteDir, CacheStorageManager::ConstructBucketPath(
                                     profile_path_, bucket_locator, owner)),
      base::BindOnce(&DeleteBucketDidDeleteDir, std::move(callback)));
}

CacheStorageManager::CacheStorageManager(
    const base::FilePath& profile_path,
    scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
    scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
    base::WeakPtr<CacheStorageDispatcherHost> cache_storage_dispatcher_host)
    : profile_path_(profile_path),
      cache_task_runner_(std::move(cache_task_runner)),
      scheduler_task_runner_(std::move(scheduler_task_runner)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      blob_storage_context_(std::move(blob_storage_context)),
      cache_storage_dispatcher_host_(std::move(cache_storage_dispatcher_host)) {
  DCHECK(cache_task_runner_);
  DCHECK(scheduler_task_runner_);
  DCHECK(quota_manager_proxy_);
  DCHECK(blob_storage_context_);
}

base::FilePath CacheStorageManager::ConstructBucketPath(
    const base::FilePath& profile_path,
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner) {
  if (bucket_locator.is_default &&
      bucket_locator.storage_key.IsFirstPartyContext()) {
    // Default-bucket & first-party partition:
    // {{storage_partition_path}}/Service Worker/CacheStorage/{origin_hash}/...
    return ConstructOriginPath(profile_path,
                               bucket_locator.storage_key.origin(), owner);
  }
  // Non-default bucket & first/third-party partition:
  // {{storage_partition_path}}/WebStorage/{{bucket_id}}/CacheStorage/... and
  // {{storage_partition_path}}/WebStorage/{{bucket_id}}/BackgroundFetch/...
  // TODO(estade): this ought to use
  // `quota_manager_proxy_->GetClientBucketPath()`
  switch (owner) {
    case storage::mojom::CacheStorageOwner::kCacheAPI:
      return storage::CreateClientBucketPath(
          profile_path, bucket_locator,
          storage::QuotaClientType::kServiceWorkerCache);
    case storage::mojom::CacheStorageOwner::kBackgroundFetch:
      return storage::CreateClientBucketPath(
          profile_path, bucket_locator,
          storage::QuotaClientType::kBackgroundFetch);
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

// static
bool CacheStorageManager::IsValidQuotaStorageKey(
    const blink::StorageKey& storage_key) {
  // Disallow opaque storage keys at the quota boundary because we DCHECK that
  // we don't get an opaque key in lower code layers.
  return !storage_key.origin().opaque();
}

void CacheStorageManager::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (level != base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    return;
  }

  for (auto& entry : cache_storage_map_) {
    entry.second->ReleaseUnreferencedCaches();
  }
}

// static
base::FilePath CacheStorageManager::ConstructFirstPartyDefaultRootPath(
    const base::FilePath& profile_path) {
  return profile_path.Append(storage::kServiceWorkerDirectory)
      .Append(storage::kCacheStorageDirectory);
}

// static
base::FilePath CacheStorageManager::ConstructThirdPartyAndNonDefaultRootPath(
    const base::FilePath& profile_path) {
  return profile_path.Append(storage::kWebStorageDirectory);
}

// Used by QuotaClient which only wants the storage keys that have data in the
// default bucket. Keep this function to return a vector of StorageKeys, instead
// of buckets.
void CacheStorageManager::ListStorageKeysOnTaskRunner(
    storage::mojom::QuotaClient::GetStorageKeysForTypeCallback callback,
    std::vector<storage::BucketLocator> buckets) {
  // Note that bucket IDs will not be populated in the `buckets` entries.
  std::vector<blink::StorageKey> out_storage_keys;
  for (const storage::BucketLocator& bucket_locator : buckets) {
    if (!bucket_locator.is_default) {
      continue;
    }
    out_storage_keys.emplace_back(bucket_locator.storage_key);
  }

  scheduler_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(out_storage_keys)));
}
}  // namespace content
