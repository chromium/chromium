// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_context_impl.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/values.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "components/services/storage/privileged/mojom/indexed_db_bucket_types.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/cpp/quota_client_callback_wrapper.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/indexed_db_bucket_context.h"
#include "content/browser/indexed_db/indexed_db_quota_client.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "third_party/zlib/google/zip.h"
#include "url/origin.h"

namespace content {

using storage::BucketLocator;

namespace {

bool IsAllowedPath(const std::vector<base::FilePath>& allowed_paths,
                   const base::FilePath& candidate_path) {
  for (const base::FilePath& allowed_path : allowed_paths) {
    if (candidate_path == allowed_path || allowed_path.IsParent(candidate_path))
      return true;
  }
  return false;
}

// Used to field IDBFactory requests when the quota system failed to
// find/return a bucket.
class MissingBucketErrorEndpoint : public blink::mojom::IDBFactory {
 public:
  MissingBucketErrorEndpoint() = default;
  ~MissingBucketErrorEndpoint() override = default;

  // blink::mojom::IDBFactory implementation:
  void GetDatabaseInfo(GetDatabaseInfoCallback callback) override {
    std::move(callback).Run(
        {}, blink::mojom::IDBError::New(
                blink::mojom::IDBException::kUnknownError, u"Internal error."));
  }

  void Open(mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
                factory_client,
            mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
                database_callbacks_remote,
            const std::u16string& name,
            int64_t version,
            mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
                transaction_receiver,
            int64_t transaction_id) override {
    IndexedDBFactoryClient(std::move(factory_client))
        .OnError(IndexedDBDatabaseError(
            blink::mojom::IDBException::kUnknownError, u"Internal error."));
  }

  void DeleteDatabase(mojo::PendingAssociatedRemote<
                          blink::mojom::IDBFactoryClient> factory_client,
                      const std::u16string& name,
                      bool force_close) override {
    IndexedDBFactoryClient(std::move(factory_client))
        .OnError(IndexedDBDatabaseError(
            blink::mojom::IDBException::kUnknownError, u"Internal error."));
  }
};

}  // namespace

IndexedDBContextImpl::IndexedDBContextImpl(
    const base::FilePath& base_data_path,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context,
    mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
        file_system_access_context,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> custom_task_runner)
    : idb_task_runner_(
          custom_task_runner
              ? custom_task_runner
              : (base::ThreadPool::CreateSequencedTaskRunner(
                    {base::MayBlock(), base::WithBaseSyncPrimitives(),
                     base::TaskPriority::USER_VISIBLE,
                     // BLOCK_SHUTDOWN to support clearing session-only storage.
                     base::TaskShutdownBehavior::BLOCK_SHUTDOWN}))),
      io_task_runner_(std::move(io_task_runner)),
      base_data_path_(base_data_path.empty() ? base::FilePath()
                                             : base_data_path),
      force_keep_session_state_(false),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      quota_client_(std::make_unique<IndexedDBQuotaClient>(*this)),
      quota_client_wrapper_(
          std::make_unique<storage::QuotaClientCallbackWrapper>(
              quota_client_.get())),
      quota_client_receiver_(quota_client_wrapper_.get()) {
  TRACE_EVENT0("IndexedDB", "init");

  // QuotaManagerProxy::RegisterClient() must be called during construction
  // until crbug.com/1182630 is fixed.
  mojo::PendingRemote<storage::mojom::QuotaClient> quota_client_remote;
  mojo::PendingReceiver<storage::mojom::QuotaClient> quota_client_receiver =
      quota_client_remote.InitWithNewPipeAndPassReceiver();
  quota_manager_proxy_->RegisterClient(
      std::move(quota_client_remote),
      storage::QuotaClientType::kIndexedDatabase,
      {blink::mojom::StorageType::kTemporary});
  IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&IndexedDBContextImpl::BindPipesOnIDBSequence,
                                weak_factory_.GetWeakPtr(),
                                std::move(quota_client_receiver),
                                std::move(blob_storage_context),
                                std::move(file_system_access_context)));
}

void IndexedDBContextImpl::BindPipesOnIDBSequence(
    mojo::PendingReceiver<storage::mojom::QuotaClient>
        pending_quota_client_receiver,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        pending_blob_storage_context,
    mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
        pending_file_system_access_context) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (pending_quota_client_receiver) {
    quota_client_receiver_.Bind(std::move(pending_quota_client_receiver));
  }
  if (pending_blob_storage_context) {
    blob_storage_context_.Bind(std::move(pending_blob_storage_context));
  }
  if (pending_file_system_access_context) {
    file_system_access_context_.Bind(
        std::move(pending_file_system_access_context));
  }
}

void IndexedDBContextImpl::BindControlOnIDBSequence(
    mojo::PendingReceiver<storage::mojom::IndexedDBControl> control) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  // We cannot run this in the constructor it needs to be async, but the async
  // tasks might not finish before the destructor runs.
  InitializeFromFilesIfNeeded(base::DoNothing());
  control_receivers_.Add(this, std::move(control));
}

void IndexedDBContextImpl::BindControl(
    mojo::PendingReceiver<storage::mojom::IndexedDBControl> control) {
  IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IndexedDBContextImpl::BindControlOnIDBSequence,
                     weak_factory_.GetWeakPtr(), std::move(control)));
}

void IndexedDBContextImpl::BindIndexedDB(
    const BucketLocator& bucket_locator,
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote,
    const base::UnguessableToken& client_token,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  auto on_got_bucket = base::BindOnce(&IndexedDBContextImpl::BindIndexedDBImpl,
                                      weak_factory_.GetWeakPtr(),
                                      std::move(client_state_checker_remote),
                                      client_token, std::move(receiver));

  if (bucket_locator.is_default) {
    // If it's for a default bucket, `bucket_locator` will be a placeholder
    // without an ID, meaning the bucket still needs to be created.
    quota_manager_proxy()->UpdateOrCreateBucket(
        storage::BucketInitParams::ForDefaultBucket(bucket_locator.storage_key),
        idb_task_runner_, std::move(on_got_bucket));
  } else {
    // Query the database to make sure the bucket still exists.
    quota_manager_proxy()->GetBucketById(bucket_locator.id, idb_task_runner_,
                                         std::move(on_got_bucket));
  }
}

void IndexedDBContextImpl::BindIndexedDBImpl(
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote,
    const base::UnguessableToken& client_token,
    mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver,
    storage::QuotaErrorOr<storage::BucketInfo> bucket_info) {
  std::optional<storage::BucketInfo> bucket;
  if (bucket_info.has_value()) {
    bucket = bucket_info.value();
  }
  if (bucket) {
    GetOrCreateBucketContext(*bucket, GetDataPath(bucket->ToBucketLocator()))
        .AddReceiver(std::move(client_state_checker_remote), client_token,
                     std::move(pending_receiver));
  } else {
    mojo::MakeSelfOwnedReceiver(std::make_unique<MissingBucketErrorEndpoint>(),
                                std::move(pending_receiver));
  }
}

void IndexedDBContextImpl::DeleteBucketData(
    const BucketLocator& bucket_locator,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  ForceClose(bucket_locator.id,
             storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN,
             base::DoNothing());
  if (is_incognito()) {
    bucket_set_.erase(bucket_locator);
    bucket_size_map_.erase(bucket_locator);
    std::move(callback).Run(true);
    return;
  }

  if (!base::DirectoryExists(GetDataPath(bucket_locator))) {
    std::move(callback).Run(true);
    return;
  }

  bool success = base::ranges::all_of(GetStoragePaths(bucket_locator),
                                      &base::DeletePathRecursively);
  NotifyOfBucketModification(bucket_locator);
  if (success) {
    bucket_set_.erase(bucket_locator);
    bucket_size_map_.erase(bucket_locator);
  }
  std::move(callback).Run(success);
}

void IndexedDBContextImpl::ForceClose(storage::BucketId bucket_id,
                                      storage::mojom::ForceCloseReason reason,
                                      base::OnceClosure closure) {
  auto it = bucket_contexts_.find(bucket_id);
  if (it != bucket_contexts_.end()) {
    it->second->ForceClose(
        /*doom=*/reason ==
        storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN);
  }

  std::move(closure).Run();
}

void IndexedDBContextImpl::DownloadBucketData(
    storage::BucketId bucket_id,
    DownloadBucketDataCallback callback) {
  bool success = false;

  auto bucket_locator = LookUpBucket(bucket_id);
  // Make sure the database hasn't been deleted.
  if (!bucket_locator) {
    std::move(callback).Run(success, base::FilePath(), base::FilePath());
    return;
  }

  ForceClose(bucket_id,
             storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE,
             base::DoNothing());

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    std::move(callback).Run(success, base::FilePath(), base::FilePath());
    return;
  }

  // This will need to get cleaned up after the download has completed.
  base::FilePath temp_path = temp_dir.Take();

  std::string storage_key_id =
      storage::GetIdentifierFromOrigin(bucket_locator->storage_key.origin());
  base::FilePath zip_path = temp_path.AppendASCII(storage_key_id)
                                .AddExtension(FILE_PATH_LITERAL("zip"));

  std::vector<base::FilePath> paths = GetStoragePaths(*bucket_locator);
  zip::ZipWithFilterCallback(GetDataPath(*bucket_locator), zip_path,
                             base::BindRepeating(IsAllowedPath, paths));

  success = true;
  std::move(callback).Run(success, temp_path, zip_path);
}

void IndexedDBContextImpl::GetAllBucketsDetails(
    GetAllBucketsDetailsCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  InitializeFromFilesIfNeeded(base::BindOnce(
      [](base::WeakPtr<IndexedDBContextImpl> handler,
         GetAllBucketsDetailsCallback callback) {
        if (!handler) {
          return;
        }
        std::vector<BucketLocator> bucket_locators = handler->GetAllBuckets();

        auto collect_buckets =
            base::BarrierCallback<storage::QuotaErrorOr<storage::BucketInfo>>(
                bucket_locators.size(),
                base::BindOnce(&IndexedDBContextImpl::OnBucketInfoReady,
                               handler, std::move(callback)));

        for (const BucketLocator& bucket_locator : bucket_locators) {
          handler->quota_manager_proxy_->GetBucketById(
              bucket_locator.id, handler->idb_task_runner_, collect_buckets);
        }
      },
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IndexedDBContextImpl::OnBucketInfoReady(
    GetAllBucketsDetailsCallback callback,
    std::vector<storage::QuotaErrorOr<storage::BucketInfo>> bucket_infos) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  std::map<url::Origin,
           std::map<blink::StorageKey,
                    std::vector<storage::mojom::IdbBucketMetadataPtr>>>
      bucket_map;

  for (const auto& quota_error_or_bucket_info : bucket_infos) {
    if (!quota_error_or_bucket_info.has_value()) {
      continue;
    }
    const storage::BucketInfo& bucket_info = quota_error_or_bucket_info.value();
    const BucketLocator bucket_locator = bucket_info.ToBucketLocator();

    storage::mojom::IdbBucketMetadataPtr info =
        storage::mojom::IdbBucketMetadata::New();
    info->bucket_locator = bucket_locator;
    info->name = bucket_info.name;
    info->size = static_cast<double>(GetBucketDiskUsage(bucket_locator));
    info->last_modified = GetBucketLastModified(bucket_locator);

    if (!is_incognito()) {
      info->paths = GetStoragePaths(bucket_locator);
    }
    // TODO(crbug.com/1474996): This executes synchronously for now, but will
    // need to handle delayed responses.
    FillInBucketMetadata(
        std::move(info),
        base::BindOnce(
            [](std::map<
                   url::Origin,
                   std::map<blink::StorageKey,
                            std::vector<storage::mojom::IdbBucketMetadataPtr>>>&
                   bucket_map,
               storage::mojom::IdbBucketMetadataPtr info) {
              blink::StorageKey storage_key = info->bucket_locator.storage_key;
              bucket_map[storage_key.origin()][storage_key].push_back(
                  std::move(info));
            },
            std::ref(bucket_map)));
  }

  std::vector<storage::mojom::IdbOriginMetadataPtr> origins;
  for (auto& [origin_url, top_level_site_map] : bucket_map) {
    storage::mojom::IdbOriginMetadataPtr origin_metadata =
        storage::mojom::IdbOriginMetadata::New();

    origin_metadata->origin = origin_url;

    for (auto& [storage_key, buckets] : top_level_site_map) {
      storage::mojom::IdbStorageKeyMetadataPtr storage_key_metadata =
          storage::mojom::IdbStorageKeyMetadata::New();

      // Sort by name alphabetically but with the default bucket always first.
      std::sort(
          buckets.begin(), buckets.end(),
          [](const storage::mojom::IdbBucketMetadataPtr& b1,
             const storage::mojom::IdbBucketMetadataPtr& b2) {
            return (b1->bucket_locator.is_default) ||
                   (!b2->bucket_locator.is_default && b1->name < b2->name);
          });

      storage_key_metadata->top_level_site = storage_key.top_level_site();
      storage_key_metadata->serialized_storage_key = storage_key.Serialize();
      storage_key_metadata->buckets = std::move(buckets);

      origin_metadata->storage_keys.push_back(std::move(storage_key_metadata));
    }

    std::sort(origin_metadata->storage_keys.begin(),
              origin_metadata->storage_keys.end());

    origins.push_back(std::move(origin_metadata));
  }

  std::sort(origins.begin(), origins.end());
  std::move(callback).Run(is_incognito(), std::move(origins));
}

void IndexedDBContextImpl::SetForceKeepSessionState() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  force_keep_session_state_ = true;
}

void IndexedDBContextImpl::ApplyPolicyUpdates(
    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  for (const storage::mojom::StoragePolicyUpdatePtr& update : policy_updates) {
    if (!update->purge_on_shutdown) {
      origins_to_purge_on_shutdown_.erase(update->origin);
    } else {
      origins_to_purge_on_shutdown_.insert(update->origin);
    }
  }
}

void IndexedDBContextImpl::BindTestInterface(
    mojo::PendingReceiver<storage::mojom::IndexedDBControlTest> receiver) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  test_receivers_.Add(this, std::move(receiver));
}

void IndexedDBContextImpl::AddObserver(
    mojo::PendingRemote<storage::mojom::IndexedDBObserver> observer) {
  IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<IndexedDBContextImpl> context,
             mojo::PendingRemote<storage::mojom::IndexedDBObserver> observer) {
            if (context)
              context->observers_.Add(std::move(observer));
          },
          weak_factory_.GetWeakPtr(), std::move(observer)));
}

void IndexedDBContextImpl::GetBaseDataPathForTesting(
    GetBaseDataPathForTestingCallback callback) {
  std::move(callback).Run(GetLegacyDataPath());
}

void IndexedDBContextImpl::GetFilePathForTesting(
    const BucketLocator& bucket_locator,
    GetFilePathForTestingCallback callback) {
  std::move(callback).Run(GetLevelDBPath(bucket_locator));
}

void IndexedDBContextImpl::ResetCachesForTesting(base::OnceClosure callback) {
  bucket_set_.clear();
  bucket_size_map_.clear();
  std::move(callback).Run();
}

void IndexedDBContextImpl::WriteToIndexedDBForTesting(
    const BucketLocator& bucket_locator,
    const std::string& key,
    const std::string& value,
    base::OnceClosure callback) {
  bucket_contexts_.find(bucket_locator.id)
      ->second->WriteToIndexedDBForTesting(key, value,  // IN-TEST
                                           std::move(callback));
}

void IndexedDBContextImpl::GetPathForBlobForTesting(
    const BucketLocator& bucket_locator,
    int64_t database_id,
    int64_t blob_number,
    GetPathForBlobForTestingCallback callback) {
  std::move(callback).Run(indexed_db::GetBlobFileNameForKey(
      GetBlobStorePath(bucket_locator), database_id, blob_number));
}

void IndexedDBContextImpl::CompactBackingStoreForTesting(
    const BucketLocator& bucket_locator,
    base::OnceClosure callback) {
  bucket_contexts_.find(bucket_locator.id)
      ->second->CompactBackingStoreForTesting();  // IN-TEST
  std::move(callback).Run();
}

void IndexedDBContextImpl::GetUsageForTesting(
    GetUsageForTestingCallback callback) {
  int64_t total_size = 0;
  for (const BucketLocator& bucket : bucket_set_) {
    total_size += GetBucketDiskUsage(bucket);
  }
  std::move(callback).Run(total_size);
}

void IndexedDBContextImpl::BindMockFailureSingletonForTesting(
    mojo::PendingReceiver<storage::mojom::MockFailureInjector> receiver) {
  pending_failure_injector_ = std::move(receiver);
}

void IndexedDBContextImpl::GetDatabaseKeysForTesting(
    GetDatabaseKeysForTestingCallback callback) {
  std::move(callback).Run(SchemaVersionKey::Encode(), DataVersionKey::Encode());
}

std::vector<BucketLocator> IndexedDBContextImpl::GetAllBuckets() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  return std::vector<BucketLocator>(bucket_set_.begin(), bucket_set_.end());
}

std::optional<BucketLocator> IndexedDBContextImpl::LookUpBucket(
    storage::BucketId bucket_id) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  auto bucket_locator =
      base::ranges::find(bucket_set_, bucket_id, &BucketLocator::id);
  if (bucket_locator == bucket_set_.end())
    return std::nullopt;

  return *bucket_locator;
}

int64_t IndexedDBContextImpl::GetBucketDiskUsage(
    const BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!LookUpBucket(bucket_locator.id))
    return 0;

  bool write_in_progress = false;
  const auto iter = bucket_size_map_.find(bucket_locator);
  if (iter != bucket_size_map_.end()) {
    if (iter->second >= 0) {
      return iter->second;
    }
    write_in_progress = true;
  }

  const int64_t value = ReadUsageFromDisk(bucket_locator, write_in_progress);
  CHECK_GE(value, 0);
  bucket_size_map_[bucket_locator] = value;
  return value;
}

base::Time IndexedDBContextImpl::GetBucketLastModified(
    const BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!LookUpBucket(bucket_locator.id))
    return base::Time();

  // Only used by indexeddb-internals; not worth the complexity to implement.
  if (is_incognito()) {
    return base::Time();
  }

  base::FilePath idb_directory = GetLevelDBPath(bucket_locator);
  base::File::Info info;
  if (base::GetFileInfo(idb_directory, &info)) {
    return info.last_modified;
  }
  return base::Time();
}

std::vector<base::FilePath> IndexedDBContextImpl::GetStoragePaths(
    const BucketLocator& bucket_locator) const {
  std::vector<base::FilePath> paths = {GetLevelDBPath(bucket_locator),
                                       GetBlobStorePath(bucket_locator)};
  return paths;
}

base::FilePath IndexedDBContextImpl::GetDataPath(
    const BucketLocator& bucket_locator) const {
  if (is_incognito()) {
    return base::FilePath();
  }

  if (indexed_db::ShouldUseLegacyFilePath(bucket_locator)) {
    // First-party idb files for the default, for legacy reasons, are stored at:
    // {{storage_partition_path}}/IndexedDB/
    // TODO(crbug.com/1315371): Migrate all first party buckets to the new path.
    return GetLegacyDataPath();
  }

  // Third-party idb files are stored at:
  // {{storage_partition_path}}/WebStorage/{{bucket_id}}/IndexedDB/
  return quota_manager_proxy()->GetClientBucketPath(
      bucket_locator, storage::QuotaClientType::kIndexedDatabase);
}

const base::FilePath IndexedDBContextImpl::GetLegacyDataPath() const {
  return base_data_path_.empty()
             ? base_data_path_
             : base_data_path_.Append(storage::kIndexedDbDirectory);
}

const base::FilePath IndexedDBContextImpl::GetFirstPartyDataPathForTesting()
    const {
  return GetLegacyDataPath();
}

void IndexedDBContextImpl::OnFilesWritten(const BucketLocator& bucket_locator,
                                          bool flushed) {
  bucket_set_.insert(bucket_locator);
  NotifyOfBucketModification(bucket_locator);
  if (!flushed) {
    // A negative value indicates "not cached, and LevelDB file write is
    // potentially in progress". See `bucket_size_map_` docs.
    bucket_size_map_[bucket_locator] = -1;
  }
}

void IndexedDBContextImpl::NotifyIndexedDBContentChanged(
    const BucketLocator& bucket_locator,
    const std::u16string& database_name,
    const std::u16string& object_store_name) {
  for (auto& observer : observers_) {
    observer->OnIndexedDBContentChanged(bucket_locator, database_name,
                                        object_store_name);
  }
}

IndexedDBContextImpl::~IndexedDBContextImpl() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  // Invalidate the weak pointers that bind `on_ready_for_destruction` (among
  // other callbacks) so that `ForceClose()` below doesn't mutate
  // `bucket_contexts_` while it's being iterated.
  weak_factory_.InvalidateWeakPtrs();
  for (const auto& [locator, context] : bucket_contexts_) {
    context->ForceClose(/*doom=*/false);
  }
  bucket_contexts_.clear();
}

void IndexedDBContextImpl::ShutdownOnIDBSequence() {
  // `this` will be destroyed when this method returns.
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  if (force_keep_session_state_)
    return;

  // Clear session-only databases.
  if (origins_to_purge_on_shutdown_.empty()) {
    return;
  }

  for (const BucketLocator& bucket_locator : bucket_set_) {
    // Delete the storage if its origin matches one of the origins to purge, or
    // if it is third-party and the top-level site is same-site with one of
    // those origins.
    bool delete_bucket = base::Contains(origins_to_purge_on_shutdown_,
                                        bucket_locator.storage_key.origin());

    if (!delete_bucket && bucket_locator.storage_key.IsThirdPartyContext()) {
      delete_bucket = base::ranges::any_of(
          origins_to_purge_on_shutdown_, [&](const url::Origin& origin) {
            return net::SchemefulSite(origin) ==
                   bucket_locator.storage_key.top_level_site();
          });
    }

    if (delete_bucket) {
      auto it = bucket_contexts_.find(bucket_locator.id);
      if (it != bucket_contexts_.end()) {
        it->second->ForceClose(false);
      }
      base::ranges::for_each(GetStoragePaths(bucket_locator),
                             &base::DeletePathRecursively);
    }
  }
}

// static
void IndexedDBContextImpl::Shutdown(
    std::unique_ptr<IndexedDBContextImpl> context) {
  IndexedDBContextImpl* context_ptr = context.get();

  // Important: This function is NOT called on the IDB Task Runner. All variable
  // access must be thread-safe.
  if (context->is_incognito()) {
    context_ptr->IDBTaskRunner()->DeleteSoon(FROM_HERE, std::move(context));
    return;
  }

  context_ptr->IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &IndexedDBContextImpl::InitializeFromFilesIfNeeded,
          base::Unretained(context_ptr),
          base::BindOnce(&IndexedDBContextImpl::ShutdownOnIDBSequence,
                         std::move(context))));
}

base::FilePath IndexedDBContextImpl::GetBlobStorePath(
    const BucketLocator& bucket_locator) const {
  DCHECK(!is_incognito());
  return GetDataPath(bucket_locator)
      .Append(indexed_db::GetBlobStoreFileName(bucket_locator));
}

base::FilePath IndexedDBContextImpl::GetLevelDBPath(
    const BucketLocator& bucket_locator) const {
  DCHECK(!is_incognito());
  return GetDataPath(bucket_locator)
      .Append(indexed_db::GetLevelDBFileName(bucket_locator));
}

base::FilePath IndexedDBContextImpl::GetLevelDBPathForTesting(
    const BucketLocator& bucket_locator) const {
  return GetLevelDBPath(bucket_locator);
}

int64_t IndexedDBContextImpl::ReadUsageFromDisk(
    const BucketLocator& bucket_locator,
    bool write_in_progress) const {
  if (is_incognito()) {
    return GetInMemorySize(bucket_locator);
  }

#if BUILDFLAG(IS_WIN)
  // Touch all files in the LevelDB directory to update directory entry
  // metadata. See note for `bucket_size_map_` about why this is necessary.
  if (write_in_progress) {
    const base::FilePath leveldb_dir = GetLevelDBPath(bucket_locator);
    base::FileEnumerator file_iter(leveldb_dir, /*recursive=*/true,
                                   base::FileEnumerator::FILES);
    for (base::FilePath file_path = file_iter.Next(); !file_path.empty();
         file_path = file_iter.Next()) {
      base::File file(
          file_path, base::File::FLAG_OPEN | base::File::FLAG_WIN_SHARE_DELETE);
    }
  }
#endif

  int64_t total_size = 0;
  for (const base::FilePath& path : GetStoragePaths(bucket_locator))
    total_size += base::ComputeDirectorySize(path);
  return total_size;
}

void IndexedDBContextImpl::NotifyOfBucketModification(
    const BucketLocator& bucket_locator) {
  // This method is called very frequently, for example after every transaction
  // commits. Recalculating disk usage is expensive and often unnecessary (e.g.
  // when many transactions commit in a row). Therefore, use a null delta to
  // notify the quota system to invalidate its cache but defer updates to
  // `bucket_size_map_`.
  bucket_size_map_.erase(bucket_locator);
  quota_manager_proxy()->NotifyBucketModified(
      storage::QuotaClientType::kIndexedDatabase, bucket_locator,
      /*delta=*/std::nullopt, base::Time::Now(),
      base::SequencedTaskRunner::GetCurrentDefault(), base::DoNothing());
  for (auto& observer : observers_) {
    observer->OnIndexedDBListChanged(bucket_locator);
  }
}

void IndexedDBContextImpl::InitializeFromFilesIfNeeded(
    base::OnceClosure callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (did_initialize_from_files_) {
    std::move(callback).Run();
    return;
  }
  std::map<blink::StorageKey, base::FilePath> storage_key_to_file_path =
      FindLegacyIndexedDBFiles();
  std::map<storage::BucketId, base::FilePath> bucket_id_to_file_path =
      FindIndexedDBFiles();
  if (storage_key_to_file_path.empty() && bucket_id_to_file_path.empty()) {
    did_initialize_from_files_ = true;
    std::move(callback).Run();
    return;
  }

  const bool running_initialize_from_files =
      on_initialize_from_files_callbacks_.size() > 0;
  on_initialize_from_files_callbacks_.push_back(std::move(callback));
  if (running_initialize_from_files) {
    return;
  }

  using Barrier = base::RepeatingCallback<void(std::optional<BucketLocator>)>;
  Barrier barrier = base::BarrierCallback<std::optional<BucketLocator>>(
      storage_key_to_file_path.size() + bucket_id_to_file_path.size(),
      base::BindOnce(
          [](base::WeakPtr<IndexedDBContextImpl> context,
             const std::vector<std::optional<BucketLocator>>& bucket_locators) {
            DCHECK(context);
            for (const std::optional<BucketLocator>& locator :
                 bucket_locators) {
              if (locator) {
                context->bucket_set_.insert(*locator);
              }
            }
            context->did_initialize_from_files_ = true;
            for (base::OnceClosure& callback :
                 context->on_initialize_from_files_callbacks_) {
              std::move(callback).Run();
              if (!context) {
                return;
              }
            }
            context->on_initialize_from_files_callbacks_.clear();
          },
          weak_factory_.GetWeakPtr()));

  auto on_lookup_done = base::BindRepeating(
      [](Barrier barrier,
         storage::QuotaErrorOr<storage::BucketInfo> bucket_info) {
        barrier.Run(bucket_info.has_value()
                        ? std::make_optional(bucket_info->ToBucketLocator())
                        : std::nullopt);
      },
      barrier);

  for (const auto& [storage_key, file_path] : storage_key_to_file_path) {
    quota_manager_proxy()->UpdateOrCreateBucket(
        storage::BucketInitParams::ForDefaultBucket(storage_key),
        idb_task_runner_, on_lookup_done);
  }

  for (const auto& [bucket_id, file_path] : bucket_id_to_file_path) {
    quota_manager_proxy()->GetBucketById(bucket_id, idb_task_runner_,
                                         on_lookup_done);
  }
}

void IndexedDBContextImpl::ForceInitializeFromFilesForTesting(
    ForceInitializeFromFilesForTestingCallback callback) {
  did_initialize_from_files_ = false;
  InitializeFromFilesIfNeeded(std::move(callback));
}

std::map<blink::StorageKey, base::FilePath>
IndexedDBContextImpl::FindLegacyIndexedDBFiles() const {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath data_path = GetLegacyDataPath();
  if (data_path.empty())
    return {};
  std::map<blink::StorageKey, base::FilePath> storage_key_to_file_path;
  base::FileEnumerator file_enumerator(data_path, /*recursive=*/false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() != indexed_db::kLevelDBExtension ||
        file_path.RemoveExtension().Extension() !=
            indexed_db::kIndexedDBExtension) {
      continue;
    }

    std::string origin_id =
        file_path.BaseName().RemoveExtension().RemoveExtension().MaybeAsASCII();
    url::Origin origin = storage::GetOriginFromIdentifier(origin_id);
    if (origin.opaque()) {
      continue;
    }
    storage_key_to_file_path[blink::StorageKey::CreateFirstParty(origin)] =
        file_path;
  }
  return storage_key_to_file_path;
}

std::map<storage::BucketId, base::FilePath>
IndexedDBContextImpl::FindIndexedDBFiles() const {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  std::map<storage::BucketId, base::FilePath> bucket_id_to_file_path;
  if (base_data_path_.empty())
    return bucket_id_to_file_path;

  base::FilePath third_party_path =
      base_data_path_.Append(storage::kWebStorageDirectory);
  base::FileEnumerator file_enumerator(third_party_path, /*recursive=*/true,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.BaseName().Extension() == indexed_db::kLevelDBExtension &&
        file_path.BaseName().RemoveExtension().value() ==
            indexed_db::kIndexedDBFile &&
        file_path.DirName().BaseName().value() ==
            storage::kIndexedDbDirectory) {
      int64_t raw_bucket_id = 0;
      bool success = base::StringToInt64(
          file_path.DirName().DirName().BaseName().value(), &raw_bucket_id);
      if (success && raw_bucket_id > 0) {
        bucket_id_to_file_path[storage::BucketId::FromUnsafeValue(
            raw_bucket_id)] = file_path;
      }
    }
  }
  return bucket_id_to_file_path;
}

void IndexedDBContextImpl::ForEachBucketContext(
    IndexedDBBucketContext::InstanceClosure callback) {
  for_each_bucket_context_ = callback;
  for (auto& [bucket_id, bucket_context] : bucket_contexts_) {
    bucket_context->RunInstanceClosure(for_each_bucket_context_);
  }
}

int64_t IndexedDBContextImpl::GetInMemorySize(
    const BucketLocator& bucket_locator) const {
  auto it = bucket_contexts_.find(bucket_locator.id);
  if (it == bucket_contexts_.end()) {
    return 0;
  }
  return it->second->GetInMemorySize();
}

std::vector<storage::BucketId>
IndexedDBContextImpl::GetOpenBucketIdsForTesting() const {
  std::vector<storage::BucketId> output;
  output.reserve(bucket_contexts_.size());
  for (const auto& [bucket_id, bucket_context] : bucket_contexts_) {
    output.push_back(bucket_id);
  }
  return output;
}

IndexedDBBucketContext* IndexedDBContextImpl::GetBucketContextForTesting(
    const storage::BucketId& id) const {
  auto it = bucket_contexts_.find(id);
  if (it != bucket_contexts_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void IndexedDBContextImpl::FillInBucketMetadata(
    storage::mojom::IdbBucketMetadataPtr info,
    base::OnceCallback<void(storage::mojom::IdbBucketMetadataPtr)> result) {
  auto it = bucket_contexts_.find(info->bucket_locator.id);
  if (it == bucket_contexts_.end()) {
    std::move(result).Run(std::move(info));
  } else {
    it->second->FillInMetadata(std::move(info), std::move(result));
  }
}

IndexedDBBucketContext& IndexedDBContextImpl::GetOrCreateBucketContext(
    const storage::BucketInfo& bucket,
    const base::FilePath& data_directory) {
  TRACE_EVENT0("IndexedDB", "indexed_db::GetOrCreateBucketContext");
  auto it = bucket_contexts_.find(bucket.id);
  if (it != bucket_contexts_.end()) {
    return *it->second;
  }

  const BucketLocator bucket_locator = bucket.ToBucketLocator();
  IndexedDBBucketContext::Delegate bucket_delegate;
  bucket_delegate.on_ready_for_destruction = base::BindRepeating(
      [](base::WeakPtr<IndexedDBContextImpl> context,
         const BucketLocator& bucket_locator) {
        if (context) {
          context->bucket_contexts_.erase(bucket_locator.id);
        }
      },
      weak_factory_.GetWeakPtr(), bucket_locator);
  bucket_delegate.on_content_changed = base::BindRepeating(
      [](base::WeakPtr<IndexedDBContextImpl> context,
         BucketLocator bucket_locator, const std::u16string& database_name,
         const std::u16string& object_store_name) {
        if (context) {
          context->NotifyIndexedDBContentChanged(bucket_locator, database_name,
                                                 object_store_name);
        }
      },
      weak_factory_.GetWeakPtr(), bucket_locator);
  bucket_delegate.on_files_written = base::BindRepeating(
      [](base::WeakPtr<IndexedDBContextImpl> context,
         BucketLocator bucket_locator, bool did_sync) {
        if (context) {
          context->OnFilesWritten(bucket_locator, did_sync);
        }
      },
      weak_factory_.GetWeakPtr(), bucket_locator);
  bucket_delegate.for_each_bucket_context = base::BindRepeating(
      &IndexedDBContextImpl::ForEachBucketContext, weak_factory_.GetWeakPtr());

  mojo::PendingRemote<storage::mojom::BlobStorageContext>
      cloned_blob_storage_context;
  // May be null in unit tests.
  if (blob_storage_context()) {
    blob_storage_context()->Clone(
        cloned_blob_storage_context.InitWithNewPipeAndPassReceiver());
  }

  mojo::PendingRemote<storage::mojom::FileSystemAccessContext> fsa_context;
  // May be null in unit tests.
  if (file_system_access_context()) {
    file_system_access_context()->Clone(
        fsa_context.InitWithNewPipeAndPassReceiver());
  }

  auto bucket_context = std::make_unique<IndexedDBBucketContext>(
      bucket, data_directory, std::move(bucket_delegate), quota_manager_proxy(),
      IOTaskRunner(), std::move(cloned_blob_storage_context),
      std::move(fsa_context), for_each_bucket_context_);

  it = bucket_contexts_.emplace(bucket_locator.id, std::move(bucket_context))
           .first;
  if (pending_failure_injector_) {
    it->second->BindMockFailureSingletonForTesting(  // IN-TEST
        std::move(pending_failure_injector_));
  }
  return *it->second;
}

}  // namespace content
