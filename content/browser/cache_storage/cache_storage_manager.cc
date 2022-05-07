// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_manager.h"

#include <stdint.h>

#include <map>
#include <numeric>
#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/id_map.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/constants.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "storage/browser/quota/quota_manager_proxy.h"
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

void DeleteStorageKeyDidDeleteDir(
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
    bool rv) {
  // On scheduler sequence.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     rv ? blink::mojom::QuotaStatusCode::kOk
                        : blink::mojom::QuotaStatusCode::kErrorAbort));
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
  // Add new enums above
  kMaxValue = kPathFileInfoFailed,
};

IndexResult ValidateIndex(proto::CacheStorageIndex index) {
  if (!index.has_origin())
    return IndexResult::kMissingOrigin;

  GURL url(index.origin());
  if (url.is_empty())
    return IndexResult::kEmptyOriginUrl;

  return IndexResult::kOk;
}

void RecordIndexValidationResult(IndexResult value) {
  base::UmaHistogramEnumeration("ServiceWorkerCache.ListOriginsIndexValidity",
                                value);
}

// Open the various cache directories' index files and extract their storage
// keys, sizes (if current), and last modified times.
std::vector<storage::mojom::StorageUsageInfoPtr>
GetStorageKeysAndLastModifiedOnTaskRunner(
    std::vector<storage::mojom::StorageUsageInfoPtr> usages,
    base::FilePath root_path,
    storage::mojom::CacheStorageOwner owner) {
  base::FileEnumerator file_enum(root_path, false /* recursive */,
                                 base::FileEnumerator::DIRECTORIES);

  base::FilePath path;
  while (!(path = file_enum.Next()).empty()) {
    base::FilePath index_path = path.AppendASCII(CacheStorage::kIndexFileName);
    base::File::Info file_info;
    base::Time index_last_modified;
    if (GetFileInfo(index_path, &file_info))
      index_last_modified = file_info.last_modified;
    std::string protobuf;
    base::ReadFileToString(path.AppendASCII(CacheStorage::kIndexFileName),
                           &protobuf);

    proto::CacheStorageIndex index;
    if (!index.ParseFromString(protobuf)) {
      RecordIndexValidationResult(IndexResult::kFailedToParse);
      continue;
    }

    IndexResult rv = ValidateIndex(index);
    if (rv != IndexResult::kOk) {
      RecordIndexValidationResult(rv);
      continue;
    }

    auto storage_key =
        blink::StorageKey(url::Origin::Create(GURL(index.origin())));
    DCHECK(!storage_key.origin().GetURL().is_empty());

    auto origin_path = CacheStorageManager::ConstructStorageKeyPath(
        root_path, storage_key, owner);
    if (path != origin_path) {
      storage::mojom::CacheStorageOwner other_owner =
          owner == storage::mojom::CacheStorageOwner::kCacheAPI
              ? storage::mojom::CacheStorageOwner::kBackgroundFetch
              : storage::mojom::CacheStorageOwner::kCacheAPI;
      auto other_owner_path = CacheStorageManager::ConstructStorageKeyPath(
          root_path, storage_key, other_owner);
      // Some of the paths in the |root_path| directory are for a different
      // |owner|.  That is valid and expected, but if the path doesn't match
      // the calculated path for either |owner|, then it is invalid.
      if (path != other_owner_path)
        RecordIndexValidationResult(IndexResult::kPathMismatch);
      continue;
    }

    int64_t storage_size =
        GetCacheStorageSize(path, index_last_modified, index);

    usages.emplace_back(storage::mojom::StorageUsageInfo::New(
        storage_key.origin(), storage_size, file_info.last_modified));
    RecordIndexValidationResult(IndexResult::kOk);
  }

  return usages;
}

// Used by QuotaClient which only wants the storage keys that have data in the
// default bucket. Keep this function to return a vector of StorageKeys, instead
// of buckets.
std::vector<blink::StorageKey> ListStorageKeysOnTaskRunner(
    base::FilePath root_path,
    storage::mojom::CacheStorageOwner owner) {
  std::vector<storage::mojom::StorageUsageInfoPtr> usages =
      GetStorageKeysAndLastModifiedOnTaskRunner(
          std::vector<storage::mojom::StorageUsageInfoPtr>(), root_path, owner);

  std::vector<blink::StorageKey> out_storage_keys;
  for (const storage::mojom::StorageUsageInfoPtr& usage : usages)
    out_storage_keys.emplace_back(blink::StorageKey(usage->origin));

  return out_storage_keys;
}

void AllOriginSizesReported(
    std::vector<storage::mojom::StorageUsageInfoPtr> usages,
    storage::mojom::CacheStorageControl::GetAllStorageKeysInfoCallback
        callback) {
  // On scheduler sequence.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(usages)));
}

void OneOriginSizeReported(base::OnceClosure callback,
                           storage::mojom::StorageUsageInfoPtr* usage,
                           int64_t size) {
  // On scheduler sequence.
  DCHECK_NE(size, CacheStorage::kSizeUnknown);
  (*usage)->total_size_bytes = size;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   std::move(callback));
}

}  // namespace

// static
scoped_refptr<CacheStorageManager> CacheStorageManager::Create(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
    scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context) {
  DCHECK(cache_task_runner);
  DCHECK(scheduler_task_runner);
  DCHECK(quota_manager_proxy);
  DCHECK(blob_storage_context);

  base::FilePath root_path = path;
  if (!path.empty()) {
    root_path = path.Append(storage::kServiceWorkerDirectory)
                    .AppendASCII("CacheStorage");
  }

  return base::WrapRefCounted(new CacheStorageManager(
      root_path, std::move(cache_task_runner), std::move(scheduler_task_runner),
      std::move(quota_manager_proxy), std::move(blob_storage_context)));
}

// static
scoped_refptr<CacheStorageManager> CacheStorageManager::CreateForTesting(
    CacheStorageManager* old_manager) {
  scoped_refptr<CacheStorageManager> manager(new CacheStorageManager(
      old_manager->root_path(), old_manager->cache_task_runner(),
      old_manager->scheduler_task_runner(), old_manager->quota_manager_proxy_,
      old_manager->blob_storage_context_));
  return manager;
}

CacheStorageManager::~CacheStorageManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CacheStorageHandle CacheStorageManager::OpenCacheStorage(
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner) {
  // TODO(https://crbug.com/1304786): unify two OpenCacheStorage
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Wait to create the MemoryPressureListener until the first CacheStorage
  // object is needed.  This ensures we create the listener on the correct
  // thread.
  if (!memory_pressure_listener_) {
    memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
        FROM_HERE, base::BindRepeating(&CacheStorageManager::OnMemoryPressure,
                                       base::Unretained(this)));
  }

  const blink::StorageKey& storage_key = bucket_locator.storage_key;
  CacheStorageMap::const_iterator it =
      cache_storage_map_.find({storage_key, owner});
  if (it == cache_storage_map_.end()) {
    CacheStorage* cache_storage = new CacheStorage(
        ConstructBucketPath(root_path_, bucket_locator, owner),
        IsMemoryBacked(), cache_task_runner_.get(), scheduler_task_runner_,
        quota_manager_proxy_, blob_storage_context_, this, storage_key, owner);
    cache_storage_map_[{storage_key, owner}] = base::WrapUnique(cache_storage);
    return cache_storage->CreateHandle();
  }
  return it->second.get()->CreateHandle();
}

CacheStorageHandle CacheStorageManager::OpenCacheStorage(
    const blink::StorageKey& storage_key,
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
      cache_storage_map_.find({storage_key, owner});
  if (it == cache_storage_map_.end()) {
    CacheStorage* cache_storage = new CacheStorage(
        ConstructStorageKeyPath(root_path_, storage_key, owner),
        IsMemoryBacked(), cache_task_runner_.get(), scheduler_task_runner_,
        quota_manager_proxy_, blob_storage_context_, this, storage_key, owner);
    cache_storage_map_[{storage_key, owner}] = base::WrapUnique(cache_storage);
    return cache_storage->CreateHandle();
  }
  return it->second.get()->CreateHandle();
}

// TODO(https://crbug.com/1304786): replace StorageKey with BucketLocator
void CacheStorageManager::NotifyCacheListChanged(
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& observer : observers_)
    observer->OnCacheListChanged(storage_key);
}

// TODO(https://crbug.com/1304786): replace StorageKey with BucketLocator
void CacheStorageManager::NotifyCacheContentChanged(
    const blink::StorageKey& storage_key,
    const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& observer : observers_)
    observer->OnCacheContentChanged(storage_key, name);
}

// TODO(https://crbug.com/1304786): replace StorageKey with BucketLocator
void CacheStorageManager::CacheStorageUnreferenced(
    CacheStorage* cache_storage,
    const blink::StorageKey& storage_key,
    storage::mojom::CacheStorageOwner owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cache_storage);
  cache_storage->AssertUnreferenced();
  auto it = cache_storage_map_.find({storage_key, owner});
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

  std::vector<storage::mojom::StorageUsageInfoPtr> usages;

  if (IsMemoryBacked()) {
    for (const auto& storage_keys_details : cache_storage_map_) {
      if (storage_keys_details.first.second != owner)
        continue;
      usages.emplace_back(storage::mojom::StorageUsageInfo::New(
          storage_keys_details.first.first.origin(),
          /*total_size_bytes=*/0,
          /*last_modified=*/base::Time()));
    }
    GetAllStorageKeysUsageGetSizes(std::move(callback), std::move(usages));
    return;
  }

  cache_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetStorageKeysAndLastModifiedOnTaskRunner,
                     std::move(usages), root_path_, owner),
      base::BindOnce(&CacheStorageManager::GetAllStorageKeysUsageGetSizes,
                     base::WrapRefCounted(this), std::move(callback)));
}

// TODO(https://crbug.com/1304786):  Rename to or add GetAllBucketsUsageGetSizes
void CacheStorageManager::GetAllStorageKeysUsageGetSizes(
    storage::mojom::CacheStorageControl::GetAllStorageKeysInfoCallback callback,
    std::vector<storage::mojom::StorageUsageInfoPtr> usages) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The origin GURL and last modified times are set in |usages| but not the
  // size in bytes. Call each CacheStorage's Size() function to fill that out.

  if (usages.empty()) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(usages)));
    return;
  }

  auto* usages_ptr = &usages[0];
  size_t usages_count = usages.size();
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      usages_count, base::BindOnce(&AllOriginSizesReported, std::move(usages),
                                   std::move(callback)));

  for (size_t i = 0; i < usages_count; ++i) {
    auto& usage = usages_ptr[i];
    if (usage->total_size_bytes != CacheStorage::kSizeUnknown ||
        !IsValidQuotaStorageKey(blink::StorageKey(usage->origin))) {
      scheduler_task_runner_->PostTask(FROM_HERE, barrier_closure);
      continue;
    }
    CacheStorageHandle cache_storage =
        OpenCacheStorage(blink::StorageKey(usage->origin),
                         storage::mojom::CacheStorageOwner::kCacheAPI);
    CacheStorage::From(cache_storage)
        ->Size(base::BindOnce(&OneOriginSizeReported, barrier_closure, &usage));
  }
}

// TODO(https://crbug.com/1304786):  rename to or add GetBucketUsage
void CacheStorageManager::GetStorageKeyUsage(
    const blink::StorageKey& storage_key,
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::GetBucketUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsMemoryBacked()) {
    auto it = cache_storage_map_.find({storage_key, owner});
    if (it == cache_storage_map_.end()) {
      scheduler_task_runner_->PostTask(FROM_HERE,
                                       base::BindOnce(std::move(callback),
                                                      /*usage=*/0));
      return;
    }
    CacheStorageHandle cache_storage = OpenCacheStorage(storage_key, owner);
    CacheStorage::From(cache_storage)->Size(std::move(callback));
    return;
  }
  cache_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&base::PathExists,
                     ConstructStorageKeyPath(root_path_, storage_key, owner)),
      base::BindOnce(&CacheStorageManager::GetStorageKeyUsageDidGetExists,
                     base::WrapRefCounted(this), storage_key, owner,
                     std::move(callback)));
}

// TODO(https://crbug.com/1304786): Rename to or add GetBucketUsageDidGetExists
void CacheStorageManager::GetStorageKeyUsageDidGetExists(
    const blink::StorageKey& storage_key,
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::GetBucketUsageCallback callback,
    bool exists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!exists) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*usage=*/0));
    return;
  }
  CacheStorageHandle cache_storage = OpenCacheStorage(storage_key, owner);
  CacheStorage::From(cache_storage)->Size(std::move(callback));
}

// TODO(https://crbug.com/1304786): remove or keep for bucket migration
void CacheStorageManager::GetStorageKeys(
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::GetStorageKeysForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsMemoryBacked()) {
    std::vector<blink::StorageKey> storage_keys;
    for (const auto& key_value : cache_storage_map_)
      if (key_value.first.second == owner)
        storage_keys.push_back(key_value.first.first);

    scheduler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(storage_keys)));
    return;
  }

  PostTaskAndReplyWithResult(
      cache_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ListStorageKeysOnTaskRunner, root_path_, owner),
      std::move(callback));
}

// TODO(https://crbug.com/1304786): rename to or add DeleteBucketData
void CacheStorageManager::DeleteStorageKeyData(
    const blink::StorageKey& storage_key,
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsMemoryBacked()) {
    auto it = cache_storage_map_.find({storage_key, owner});
    if (it == cache_storage_map_.end()) {
      scheduler_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    blink::mojom::QuotaStatusCode::kOk));
      return;
    }
    DeleteStorageKeyDataDidGetExists(storage_key, owner, std::move(callback),
                                     /*exists=*/true);
    return;
  }

  cache_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&base::PathExists,
                     ConstructStorageKeyPath(root_path_, storage_key, owner)),
      base::BindOnce(&CacheStorageManager::DeleteStorageKeyDataDidGetExists,
                     base::WrapRefCounted(this), storage_key, owner,
                     std::move(callback)));
}

// TODO(https://crbug.com/1304786): rename to or add
// DeleteBucketDataDidGetExists
void CacheStorageManager::DeleteStorageKeyDataDidGetExists(
    const blink::StorageKey& storage_key,
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
    bool exists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!exists) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::mojom::QuotaStatusCode::kOk));
    return;
  }

  // Create the CacheStorage for the storage key if it hasn't been loaded yet.
  CacheStorageHandle handle = OpenCacheStorage(storage_key, owner);

  auto it = cache_storage_map_.find({storage_key, owner});
  DCHECK(it != cache_storage_map_.end());

  CacheStorage* cache_storage = it->second.release();
  cache_storage->ResetManager();
  cache_storage_map_.erase({storage_key, owner});
  cache_storage->GetSizeThenCloseAllCaches(
      base::BindOnce(&CacheStorageManager::DeleteStorageKeyDidClose,
                     base::WrapRefCounted(this), storage_key, owner,
                     std::move(callback), base::WrapUnique(cache_storage)));
}

// TODO(https://crbug.com/1304786): rename to or add
// DeleteBucketDataDidGetExists
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

// TODO(https://crbug.com/1304786): rename to or add DeleteBucketDidClose
void CacheStorageManager::DeleteStorageKeyDidClose(
    const blink::StorageKey& storage_key,
    storage::mojom::CacheStorageOwner owner,
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
    std::unique_ptr<CacheStorage> cache_storage,
    int64_t origin_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(jkarlin): Deleting the storage leaves any unfinished operations
  // hanging, resulting in unresolved promises. Fix this by returning early from
  // CacheStorage operations posted after GetSizeThenCloseAllCaches is called.
  cache_storage.reset();

  quota_manager_proxy_->NotifyStorageModified(
      CacheStorageQuotaClient::GetClientTypeFromOwner(owner), storage_key,
      blink::mojom::StorageType::kTemporary, -origin_size, base::Time::Now(),
      base::SequencedTaskRunnerHandle::Get(), base::DoNothing());

  if (owner == storage::mojom::CacheStorageOwner::kCacheAPI)
    NotifyCacheListChanged(storage_key);

  if (IsMemoryBacked()) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  blink::mojom::QuotaStatusCode::kOk));
    return;
  }

  PostTaskAndReplyWithResult(
      cache_task_runner_.get(), FROM_HERE,
      base::BindOnce(&DeleteDir,
                     ConstructStorageKeyPath(root_path_, storage_key, owner)),
      base::BindOnce(&DeleteStorageKeyDidDeleteDir, std::move(callback)));
}

CacheStorageManager::CacheStorageManager(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
    scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context)
    : root_path_(path),
      cache_task_runner_(std::move(cache_task_runner)),
      scheduler_task_runner_(std::move(scheduler_task_runner)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      blob_storage_context_(std::move(blob_storage_context)) {
  DCHECK(cache_task_runner_);
  DCHECK(scheduler_task_runner_);
  DCHECK(quota_manager_proxy_);
  DCHECK(blob_storage_context_);
}

// static
base::FilePath CacheStorageManager::ConstructStorageKeyPath(
    const base::FilePath& root_path,
    const blink::StorageKey& storage_key,
    storage::mojom::CacheStorageOwner owner) {
  // TODO(https://crbug.com/1199077): This identifier needs to be updated to
  // include the full serialization of `storage_key`.
  std::string identifier =
      storage::GetIdentifierFromOrigin(storage_key.origin());
  if (owner != storage::mojom::CacheStorageOwner::kCacheAPI) {
    identifier += "-" + std::to_string(static_cast<int>(owner));
  }
  const std::string origin_hash = base::SHA1HashString(identifier);
  const std::string origin_hash_hex = base::ToLowerASCII(
      base::HexEncode(origin_hash.c_str(), origin_hash.length()));
  return root_path.AppendASCII(origin_hash_hex);
}

base::FilePath CacheStorageManager::ConstructBucketPath(
    const base::FilePath& root_path,
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner) {
  if (bucket_locator.is_default &&
      bucket_locator.storage_key.IsFirstPartyContext()) {
    // Default-bucket & first-party partition:
    // {{storage_partition_path}}/Service Worker/CacheStorage/{origin_hash}/...
    return CacheStorageManager::ConstructStorageKeyPath(
        root_path, bucket_locator.storage_key, owner);
  }
  // Non-default bucket & first/third-party partition:
  // {{storage_partition_path}}/WebStorage/{{bucket_id}}/CacheStorage/...
  return quota_manager_proxy_->GetClientBucketPath(
      bucket_locator, storage::QuotaClientType::kServiceWorkerCache);
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

}  // namespace content
