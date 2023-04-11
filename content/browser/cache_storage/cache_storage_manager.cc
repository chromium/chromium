// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_manager.h"

#include <stdint.h>

#include <map>
#include <numeric>
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
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom-forward.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/storage_directory_util.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

void DeleteStorageKeyDidDeleteAllData(
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
    std::vector<blink::mojom::QuotaStatusCode> results) {
  // On scheduler sequence.
  for (auto result : results) {
    if (result != blink::mojom::QuotaStatusCode::kOk) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), result));
      return;
    }
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), blink::mojom::QuotaStatusCode::kOk));
}

// Calculate the sum of all cache sizes in this store, but only if all sizes are
// known. If one or more sizes are not known then return kSizeUnknown.
int64_t GetCacheStorageSize(const base::FilePath& base_path,
                            const base::Time& index_time,
                            const proto::CacheStorageIndex& index) {
  // Note, do not use the base path time modified to invalidate the index file.
  // On some platforms the directory modified time will be slightly later than
  // the last modified time of a file within it.  This means any write to the
  // index file will also update the directory modify time slightly after
  // immediately invalidating it.  To avoid this we only look at the cache
  // directories and not the base directory containing the index itself.
  int64_t storage_size = 0;
  for (int i = 0, max = index.cache_size(); i < max; ++i) {
    const proto::CacheStorageIndex::Cache& cache = index.cache(i);
    if (!cache.has_cache_dir() || !cache.has_size() ||
        cache.size() == CacheStorage::kSizeUnknown || !cache.has_padding() ||
        cache.padding() == CacheStorage::kSizeUnknown) {
      return CacheStorage::kSizeUnknown;
    }

    // Check the modified time on each cache directory.  If one of the
    // directories has the same or newer modified time as the index file, then
    // its size is most likely not accounted for in the index file.  The
    // cache can have a newer time here in spite of our base path time check
    // above since simple disk_cache writes to these directories from a
    // different thread.
    base::FilePath path = base_path.AppendASCII(cache.cache_dir());
    base::File::Info file_info;
    if (!base::GetFileInfo(path, &file_info) ||
        file_info.last_modified >= index_time) {
      return CacheStorage::kSizeUnknown;
    }

    storage_size += (cache.size() + cache.padding());
  }

  return storage_size;
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
  if (!index.has_origin())
    return IndexResult::kMissingOrigin;

  GURL url(index.origin());
  if (url.is_empty())
    return IndexResult::kEmptyOriginUrl;

  // TODO(https://crbug.com/1199077): Consider adding a
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
  const std::string origin_hash = base::SHA1HashString(identifier);
  const std::string origin_hash_hex = base::ToLowerASCII(
      base::HexEncode(origin_hash.c_str(), origin_hash.length()));
  return first_party_default_root_path.AppendASCII(origin_hash_hex);
}

void ValidateAndAddUsageFromPath(
    const base::FilePath& index_file_directory_path,
    storage::mojom::CacheStorageOwner owner,
    const base::FilePath& profile_path,
    std::vector<std::tuple<storage::BucketLocator,
                           storage::mojom::StorageUsageInfoPtr>>& usage_tuples,
    bool is_origin_path = false) {
  if (!base::PathExists(index_file_directory_path)) {
    return;
  }
  base::FilePath index_path =
      index_file_directory_path.AppendASCII(CacheStorage::kIndexFileName);
  base::File::Info file_info;
  base::Time index_last_modified;
  if (GetFileInfo(index_path, &file_info))
    index_last_modified = file_info.last_modified;
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
    absl::optional<blink::StorageKey> result =
        blink::StorageKey::Deserialize(index.storage_key());
    if (!result) {
      RecordIndexValidationResult(IndexResult::kInvalidStorageKey);
      return;
    }
    storage_key = result.value();
  } else {
    // TODO(https://crbug.com/1199077): Since index file migrations happen
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
    // TODO(https://crbug.com/1218097): Once enough time has passed it should be
    // safe to treat this case as an index validation error.
  }

  auto bucket_path = CacheStorageManager::ConstructBucketPath(
      profile_path, bucket_locator, owner);
  if (index_file_directory_path != bucket_path) {
    if (is_origin_path) {
      // For paths corresponding to the legacy Cache Storage directory structure
      // (where the bucket ID is not in the path),
      // `ValidateAndAddUsageFromPath()` can get called with an
      // `index_file_directory_path` that corresponds to a Cache Storage
      // instance from a different `owner`. That is valid and expected because
      // the directory entries from different owners are stored alongside each
      // other and are not easily distinguishable (the directory name is a
      // hash of the origin + the owner), so it's easiest to just compute the
      // two possible origin paths here and compare. If the path doesn't match
      // the calculated path for either `owner` then it is invalid. With the new
      // directory structure for non-default buckets and third-party contexts,
      // we only call `ValidateAndAddUsageFromPath()` with Cache Storage
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
      if (index_file_directory_path == other_owner_path)
        return;
    }
    RecordIndexValidationResult(IndexResult::kPathMismatch);
    return;
  }

  int64_t storage_size = GetCacheStorageSize(index_file_directory_path,
                                             index_last_modified, index);

  usage_tuples.emplace_back(
      bucket_locator, storage::mojom::StorageUsageInfo::New(
                          storage_key, storage_size, file_info.last_modified));
  RecordIndexValidationResult(IndexResult::kOk);
}

void GetStorageKeyAndLastModifiedGotBucket(
    storage::mojom::StorageUsageInfoPtr info,
    base::OnceCallback<void(std::tuple<storage::BucketLocator,
                                       storage::mojom::StorageUsageInfoPtr>)>
        callback,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  storage::BucketLocator bucket_locator{};
  if (result.ok()) {
    bucket_locator = result->ToBucketLocator();
    DCHECK_EQ(info->storage_key, result->storage_key);
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          std::make_tuple(bucket_locator,
                          storage::mojom::StorageUsageInfo::New(
                              info->storage_key, info->total_size_bytes,
                              info->last_modified))));
}

// Open the various cache directories' index files and extract their bucket
// locators, sizes (if current), and last modified times.
void GetStorageKeysAndLastModifiedOnTaskRunner(
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
    std::vector<std::tuple<storage::BucketLocator,
                           storage::mojom::StorageUsageInfoPtr>> usage_tuples,

    base::FilePath profile_path,
    storage::mojom::CacheStorageOwner owner,
    base::OnceCallback<
        void(std::vector<std::tuple<storage::BucketLocator,
                                    storage::mojom::StorageUsageInfoPtr>>)>
        callback) {
  // Add entries to `usage_tuples` from the directory for default buckets
  // corresponding to first-party contexts.
  {
    base::FilePath first_party_default_buckets_root_path =
        CacheStorageManager::ConstructFirstPartyDefaultRootPath(profile_path);

    base::FileEnumerator file_enum(first_party_default_buckets_root_path,
                                   false /* recursive */,
                                   base::FileEnumerator::DIRECTORIES);

    base::FilePath path;
    while (!(path = file_enum.Next()).empty()) {
      ValidateAndAddUsageFromPath(path, owner, profile_path, usage_tuples,
                                  true /* is_origin_path */);
    }
  }

  // Add entries to `usage_tuples` from the directory for non-default
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
      ValidateAndAddUsageFromPath(cache_storage_path, owner, profile_path,
                                  usage_tuples);
    }
  }

  if (usage_tuples.empty()) {
    scheduler_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(usage_tuples)));
    return;
  }

  if (!quota_manager_proxy) {
    // If we don't have a `QuotaManagerProxy` then don't attempt to resolve
    // any missing bucket IDs.
    scheduler_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(usage_tuples)));
    return;
  }

  // If the quota manager proxy is available, query it for the correct
  // bucket information regardless of whether the index file has bucket
  // information. If we recreate a stale bucket locator here, a side effect is
  // that our CacheStorageCache instance map could get populated with entries
  // that map to the same file path (for instances where the bucket ID isn't a
  // part of the directory path), triggering an infinite hang.
  const auto barrier_callback = base::BarrierCallback<
      std::tuple<storage::BucketLocator, storage::mojom::StorageUsageInfoPtr>>(
      usage_tuples.size(), std::move(callback));

  for (const auto& usage_tuple : usage_tuples) {
    const storage::BucketLocator& bucket_locator = std::get<0>(usage_tuple);
    const storage::mojom::StorageUsageInfoPtr& info = std::get<1>(usage_tuple);
    if (bucket_locator.is_default) {
      quota_manager_proxy->UpdateOrCreateBucket(
          storage::BucketInitParams::ForDefaultBucket(
              bucket_locator.storage_key),
          scheduler_task_runner,
          base::BindOnce(&GetStorageKeyAndLastModifiedGotBucket,
                         storage::mojom::StorageUsageInfo::New(
                             info->storage_key, info->total_size_bytes,
                             info->last_modified),
                         barrier_callback));
    } else {
      quota_manager_proxy->GetBucketById(
          bucket_locator.id, scheduler_task_runner,
          base::BindOnce(&GetStorageKeyAndLastModifiedGotBucket,
                         storage::mojom::StorageUsageInfo::New(
                             info->storage_key, info->total_size_bytes,
                             info->last_modified),
                         barrier_callback));
    }
  }
}

void AllStorageKeySizesReported(
    storage::mojom::CacheStorageControl::GetAllStorageKeysInfoCallback callback,
    std::vector<storage::mojom::StorageUsageInfoPtr> usages) {
  // We should return only one entry per StorageKey, so condense down all
  // results before passing them to the callback. We condense by adding total
  // size bytes and using the latest last_modified value.
  std::map<blink::StorageKey, int64_t> storage_key_to_total_size_bytes;
  std::map<blink::StorageKey, base::Time> storage_key_to_last_modified;
  for (const auto& usage : usages) {
    storage_key_to_total_size_bytes[usage->storage_key] +=
        usage->total_size_bytes;
    // Save off the most recent valid last modified time.
    if (storage_key_to_last_modified.count(usage->storage_key) == 0 ||
        (!usage->last_modified.is_null() &&
         (storage_key_to_last_modified[usage->storage_key].is_null() ||
          usage->last_modified >
              storage_key_to_last_modified[usage->storage_key]))) {
      storage_key_to_last_modified[usage->storage_key] = usage->last_modified;
    }
  }

  std::vector<storage::mojom::StorageUsageInfoPtr> new_usages;
  new_usages.reserve(storage_key_to_total_size_bytes.size());

  for (const auto& storage_key_usage_info : storage_key_to_total_size_bytes) {
    new_usages.emplace_back(storage::mojom::StorageUsageInfo::New(
        storage_key_usage_info.first, storage_key_usage_info.second,
        storage_key_to_last_modified[storage_key_usage_info.first]));
  }

  // On scheduler sequence.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(new_usages)));
}

void OneStorageKeySizeReported(
    base::OnceCallback<void(storage::mojom::StorageUsageInfoPtr)> callback,
    const blink::StorageKey storage_key,
    const base::Time last_modified,
    int64_t size) {
  // On scheduler sequence.
  DCHECK_NE(size, CacheStorage::kSizeUnknown);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                storage::mojom::StorageUsageInfo::New(
                                    storage_key, size, last_modified)));
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
  for (const auto& observer : observers_)
    observer->OnCacheListChanged(bucket_locator.storage_key);
}

void CacheStorageManager::NotifyCacheContentChanged(
    const storage::BucketLocator& bucket_locator,
    const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& observer : observers_)
    observer->OnCacheContentChanged(bucket_locator.storage_key, name);
}

void CacheStorageManager::CacheStorageUnreferenced(
    CacheStorage* cache_storage,
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cache_storage);
  cache_storage->AssertUnreferenced();
  auto it = cache_storage_map_.find({bucket_locator, owner});
  DCHECK(it != cache_storage_map_.end());
  DCHECK(it->second.get() == cache_storage);

  // Currently we don't do anything when a CacheStorage instance becomes
  // unreferenced.  In the future we will deallocate some or all of the
  // CacheStorage's state.
}

void CacheStorageManager::GetAllStorageKeysUsage(
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::CacheStorageControl::GetAllStorageKeysInfoCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<
      std::tuple<storage::BucketLocator, storage::mojom::StorageUsageInfoPtr>>
      usages;

  if (IsMemoryBacked()) {
    for (const auto& bucket_details : cache_storage_map_) {
      if (bucket_details.first.second != owner)
        continue;
      const storage::BucketLocator& bucket_locator = bucket_details.first.first;
      usages.emplace_back(bucket_locator, storage::mojom::StorageUsageInfo::New(
                                              bucket_locator.storage_key,
                                              /*total_size_bytes=*/0,
                                              /*last_modified=*/base::Time()));
    }
    GetAllStorageKeysUsageGetSizes(owner, std::move(callback),
                                   std::move(usages));
    return;
  }

  cache_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetStorageKeysAndLastModifiedOnTaskRunner,
          base::WrapRefCounted(quota_manager_proxy_.get()),
          base::WrapRefCounted(scheduler_task_runner_.get()), std::move(usages),
          profile_path_, owner,
          base::BindOnce(&CacheStorageManager::GetAllStorageKeysUsageGetSizes,
                         weak_ptr_factory_.GetWeakPtr(), owner,
                         std::move(callback))));
}

void CacheStorageManager::GetAllStorageKeysUsageGetSizes(
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::CacheStorageControl::GetAllStorageKeysInfoCallback callback,
    std::vector<std::tuple<storage::BucketLocator,
                           storage::mojom::StorageUsageInfoPtr>> usage_tuples) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The origin GURL and last modified times are set in |usages| but not the
  // size in bytes. Call each CacheStorage's Size() function to fill that out.
  std::vector<storage::mojom::StorageUsageInfoPtr> usages;
  if (usage_tuples.empty()) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(usages)));
    return;
  }

  // If we weren't able to lookup a bucket ID that corresponds to this
  // CacheStorage instance, skip reporting usage information about it.
  int non_null_count = std::count_if(
      usage_tuples.begin(), usage_tuples.end(),
      [](const std::tuple<storage::BucketLocator,
                          storage::mojom::StorageUsageInfoPtr>& usage_tuple) {
        const storage::BucketLocator bucket_locator = std::get<0>(usage_tuple);
        return !bucket_locator.is_null();
      });

  const auto barrier_callback =
      base::BarrierCallback<storage::mojom::StorageUsageInfoPtr>(
          non_null_count,
          base::BindOnce(&AllStorageKeySizesReported, std::move(callback)));

  for (const auto& usage_tuple : usage_tuples) {
    const storage::BucketLocator& bucket_locator = std::get<0>(usage_tuple);
    const storage::mojom::StorageUsageInfoPtr& info = std::get<1>(usage_tuple);
    if (bucket_locator.is_null()) {
      continue;
    }
    if (info->total_size_bytes != CacheStorage::kSizeUnknown ||
        !IsValidQuotaStorageKey(bucket_locator.storage_key)) {
      scheduler_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(barrier_callback,
                         storage::mojom::StorageUsageInfo::New(
                             info->storage_key, info->total_size_bytes,
                             info->last_modified)));
      continue;
    }
    CacheStorageHandle cache_storage = OpenCacheStorage(bucket_locator, owner);
    CacheStorage::From(cache_storage)
        ->Size(base::BindOnce(&OneStorageKeySizeReported, barrier_callback,
                              info->storage_key, info->last_modified));
  }
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
      if (key_value.first.second != owner)
        continue;

      const storage::BucketLocator& bucket_locator = key_value.first.first;
      if (!bucket_locator.is_default)
        continue;

      storage_keys.push_back(bucket_locator.storage_key);
    }

    scheduler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(storage_keys)));
    return;
  }

  std::vector<
      std::tuple<storage::BucketLocator, storage::mojom::StorageUsageInfoPtr>>
      usage_tuples;

  // Note that we don't want `GetStorageKeysAndLastModifiedOnTaskRunner()` to
  // call `QuotaManagerProxy::UpdateOrCreateBucket()` because doing so creates
  // a deadlock. Specifically, `GetStorageKeys()` would wait for the bucket
  // information to be returned and the QuotaManager won't respond with
  // bucket information until the `GetStorageKeys()` call finishes (as part of
  // the QuotaDatabase bootstrapping process). We don't need the bucket ID to
  // build a list of StorageKeys anyway.
  cache_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetStorageKeysAndLastModifiedOnTaskRunner, nullptr,
          base::WrapRefCounted(scheduler_task_runner_.get()),
          std::move(usage_tuples), profile_path_, owner,
          base::BindOnce(&CacheStorageManager::ListStorageKeysOnTaskRunner,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void CacheStorageManager::DeleteStorageKeyDataGotAllBucketInfo(
    const blink::StorageKey storage_key,
    storage::mojom::CacheStorageOwner owner,
    base::OnceCallback<void(std::vector<blink::mojom::QuotaStatusCode>)>
        callback,
    std::vector<std::tuple<storage::BucketLocator,
                           storage::mojom::StorageUsageInfoPtr>> usage_tuples) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (usage_tuples.empty()) {
    std::vector<blink::mojom::QuotaStatusCode> results{
        blink::mojom::QuotaStatusCode::kOk};
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(results)));
    return;
  }

  int instance_count = 0;
  for (const std::tuple<storage::BucketLocator,
                        storage::mojom::StorageUsageInfoPtr>& usage_tuple :
       usage_tuples) {
    const storage::BucketLocator bucket_locator = std::get<0>(usage_tuple);
    if (!bucket_locator.is_default ||
        storage_key != bucket_locator.storage_key) {
      continue;
    }
    instance_count += 1;
  }

  const auto barrier_callback =
      base::BarrierCallback<blink::mojom::QuotaStatusCode>(instance_count,
                                                           std::move(callback));

  for (const std::tuple<storage::BucketLocator,
                        storage::mojom::StorageUsageInfoPtr>& usage_tuple :
       usage_tuples) {
    const storage::BucketLocator bucket_locator = std::get<0>(usage_tuple);
    if (!bucket_locator.is_default ||
        storage_key != bucket_locator.storage_key) {
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

void CacheStorageManager::DeleteStorageKeyData(
    const blink::StorageKey& storage_key,
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsMemoryBacked()) {
    // Note: since the number of CacheStorage instances is usually small, just
    // search for the corresponding `storage::BucketLocator` key given a
    // `blink::StorageKey`.
    for (const auto& key_value : cache_storage_map_) {
      if (key_value.first.second != owner)
        continue;
      const storage::BucketLocator& bucket_locator = key_value.first.first;
      if (!bucket_locator.is_default ||
          bucket_locator.storage_key != storage_key)
        continue;
      DeleteBucketDataDidGetExists(owner, std::move(callback), bucket_locator,
                                   /*exists=*/true);
      return;
    }
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::mojom::QuotaStatusCode::kOk));
    return;
  }
  // Note that we can't call `QuotaManagerProxy::GetBucket()` and then use the
  // same steps as `DeleteBucketData()` here because this method is called
  // during shutdown and the quota code might be at various stages of shutting
  // down as well. Instead, build a list of cache storage instances from disk
  // and either use the bucket locators from index files or directly delete any
  // unmigrated cache storage origin paths.
  std::vector<
      std::tuple<storage::BucketLocator, storage::mojom::StorageUsageInfoPtr>>
      usage_tuples;
  cache_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetStorageKeysAndLastModifiedOnTaskRunner, nullptr,
          base::WrapRefCounted(scheduler_task_runner_.get()),
          std::move(usage_tuples), profile_path_, owner,
          base::BindOnce(
              &CacheStorageManager::DeleteStorageKeyDataGotAllBucketInfo,
              weak_ptr_factory_.GetWeakPtr(), storage_key, owner,
              base::BindOnce(&DeleteStorageKeyDidDeleteAllData,
                             std::move(callback)))));
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
  DCHECK(it != cache_storage_map_.end());

  CacheStorage* cache_storage = it->second.release();
  cache_storage->ResetManager();
  cache_storage_map_.erase({bucket_locator, owner});

  cache_storage->GetSizeThenCloseAllCaches(
      base::BindOnce(&CacheStorageManager::DeleteBucketDidClose,
                     weak_ptr_factory_.GetWeakPtr(), bucket_locator, owner,
                     std::move(callback), base::WrapUnique(cache_storage)));
}

// Note: This only deletes data associated with the default bucket for a given
// `blink::StorageKey`.
void CacheStorageManager::DeleteStorageKeyData(
    const blink::StorageKey& storage_key,
    storage::mojom::CacheStorageOwner owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DeleteStorageKeyData(storage_key, owner, base::DoNothing());
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

  if (owner == storage::mojom::CacheStorageOwner::kCacheAPI)
    NotifyCacheListChanged(bucket_locator);

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
      NOTREACHED();
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
  if (level != base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL)
    return;

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
    std::vector<std::tuple<storage::BucketLocator,
                           storage::mojom::StorageUsageInfoPtr>> usage_tuples) {
  // Note that bucket IDs will not be populated in the `usage_tuples` entries.
  std::vector<blink::StorageKey> out_storage_keys;
  for (const std::tuple<storage::BucketLocator,
                        storage::mojom::StorageUsageInfoPtr>& usage_tuple :
       usage_tuples) {
    const storage::BucketLocator bucket_locator = std::get<0>(usage_tuple);
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
