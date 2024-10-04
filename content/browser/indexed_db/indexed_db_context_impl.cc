// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_context_impl.h"

#include <algorithm>
#include <compare>
#include <functional>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/numerics/clamped_math.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_features.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/types/expected.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom-shared.h"
#include "components/services/storage/privileged/mojom/indexed_db_internals_types.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_init_params.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/base/schemeful_site.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-shared.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/zlib/google/zip.h"
#include "url/origin.h"

namespace content::indexed_db {

using blink::StorageKey;
using storage::BucketLocator;

namespace {

// Creates a task runner suitable for use either as the main IDB thread or for a
// backing store. See https://crbug.com/329221141 for notes on task priority.
scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::FeatureList::IsEnabled(base::kUseUtilityThreadGroup)
           ? base::TaskPriority::USER_BLOCKING
           : base::TaskPriority::USER_VISIBLE,
       // BLOCK_SHUTDOWN to support clearing session-only storage.
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
}

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
            int64_t transaction_id,
            int scheduling_priority) override {
    mojo::AssociatedRemote<blink::mojom::IDBFactoryClient> remote(
        std::move(factory_client));
    remote->Error(blink::mojom::IDBException::kUnknownError,
                  u"Internal error.");
  }

  void DeleteDatabase(mojo::PendingAssociatedRemote<
                          blink::mojom::IDBFactoryClient> factory_client,
                      const std::u16string& name,
                      bool force_close) override {
    mojo::AssociatedRemote<blink::mojom::IDBFactoryClient> remote(
        std::move(factory_client));
    remote->Error(blink::mojom::IDBException::kUnknownError,
                  u"Internal error.");
  }
};

// Getting all the bucket details requires multiple asynchronous steps.
// `IndexedDBContextImpl::ContinueGetAllBucketsDetails` is invoked after
// asynchronously retrieving buckets from the quota manager, whereas
// `FinishGetAllBucketsDetails` is invoked after retrieving details from
// individual bucket contexts.
void FinishGetAllBucketsDetails(
    base::OnceCallback<void(std::vector<storage::mojom::IdbOriginMetadataPtr>)>
        callback,
    std::vector<storage::mojom::IdbBucketMetadataPtr> infos) {
  std::map<url::Origin,
           std::map<blink::StorageKey,
                    std::vector<storage::mojom::IdbBucketMetadataPtr>>>
      origin_map;
  for (storage::mojom::IdbBucketMetadataPtr& info : infos) {
    if (info) {
      StorageKey storage_key = info->bucket_locator.storage_key;
      origin_map[storage_key.origin()][storage_key].push_back(std::move(info));
    }
  }

  std::vector<storage::mojom::IdbOriginMetadataPtr> origins;
  for (auto& [origin_url, top_level_site_map] : origin_map) {
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
  std::move(callback).Run(std::move(origins));
}

}  // namespace

IndexedDBContextImpl::IndexedDBContextImpl(
    const base::FilePath& base_data_path,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context,
    mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
        file_system_access_context,
    scoped_refptr<base::SequencedTaskRunner> custom_task_runner)
    : idb_task_runner_(custom_task_runner ? custom_task_runner
                                          : CreateTaskRunner()),
      base_data_path_(base_data_path.empty() ? base::FilePath()
                                             : base_data_path),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      quota_client_receiver_(&quota_client_wrapper_),
      force_single_thread_(!!custom_task_runner) {
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
    const storage::BucketClientInfo& client_info,
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  auto on_got_bucket = base::BindOnce(
      &IndexedDBContextImpl::BindIndexedDBImpl, weak_factory_.GetWeakPtr(),
      client_info, std::move(client_state_checker_remote), std::move(receiver));

  if (bucket_locator.is_default) {
    // If it's for a default bucket, `bucket_locator` will be a placeholder
    // without an ID, meaning the bucket still needs to be created.
    quota_manager_proxy_->UpdateOrCreateBucket(
        storage::BucketInitParams::ForDefaultBucket(bucket_locator.storage_key),
        idb_task_runner_, std::move(on_got_bucket));
  } else {
    // Query the database to make sure the bucket still exists.
    quota_manager_proxy_->GetBucketById(bucket_locator.id, idb_task_runner_,
                                        std::move(on_got_bucket));
  }
}

void IndexedDBContextImpl::BindIndexedDBImpl(
    const storage::BucketClientInfo& client_info,
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote,
    mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver,
    storage::QuotaErrorOr<storage::BucketInfo> bucket_info) {
  std::optional<storage::BucketInfo> bucket;
  if (bucket_info.has_value()) {
    bucket = bucket_info.value();
  }
  if (bucket) {
    EnsureBucketContext(*bucket, GetDataPath(bucket->ToBucketLocator()));
    auto iter = bucket_contexts_.find(bucket->id);
    CHECK(iter != bucket_contexts_.end(), base::NotFatalUntil::M130);
    iter->second.AsyncCall(&BucketContext::AddReceiver)
        .WithArgs(client_info, std::move(client_state_checker_remote),
                  std::move(pending_receiver));
  } else {
    mojo::MakeSelfOwnedReceiver(std::make_unique<MissingBucketErrorEndpoint>(),
                                std::move(pending_receiver));
  }
}

void IndexedDBContextImpl::DeleteBucketData(const BucketLocator& bucket_locator,
                                            DeleteBucketDataCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK_EQ(bucket_locator.type, blink::mojom::StorageType::kTemporary);
  DCHECK(!callback.is_null());
  ForceClose(
      bucket_locator.id,
      storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN,
      base::BindOnce(&IndexedDBContextImpl::DidForceCloseForDeleteBucketData,
                     weak_factory_.GetWeakPtr(), bucket_locator,
                     std::move(callback)));
}

void IndexedDBContextImpl::DidForceCloseForDeleteBucketData(
    const storage::BucketLocator& bucket_locator,
    DeleteBucketDataCallback callback) {
  if (in_memory()) {
    bucket_set_.erase(bucket_locator);
    bucket_size_map_.erase(bucket_locator);
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  if (!base::DirectoryExists(GetDataPath(bucket_locator))) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  bool success = base::ranges::all_of(GetStoragePaths(bucket_locator),
                                      &base::DeletePathRecursively);
  NotifyOfBucketModification(bucket_locator);
  if (success) {
    bucket_set_.erase(bucket_locator);
    bucket_size_map_.erase(bucket_locator);
  }
  std::move(callback).Run(success ? blink::mojom::QuotaStatusCode::kOk
                                  : blink::mojom::QuotaStatusCode::kUnknown);
}

void IndexedDBContextImpl::ForceClose(storage::BucketId bucket_id,
                                      storage::mojom::ForceCloseReason reason,
                                      base::OnceClosure closure) {
  const bool doom =
      reason == storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN;
  auto iter = bucket_contexts_.find(bucket_id);
  if (iter != bucket_contexts_.end()) {
    iter->second.AsyncCall(&BucketContext::ForceClose)
        .WithArgs(doom)
        .Then(std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

void IndexedDBContextImpl::StartMetadataRecording(
    storage::BucketId bucket_id,
    StartMetadataRecordingCallback callback) {
  auto iter = bucket_contexts_.find(bucket_id);
  if (iter != bucket_contexts_.end()) {
    iter->second.AsyncCall(&BucketContext::StartMetadataRecording)
        .Then(std::move(callback));
  } else {
    pending_bucket_recording_.insert(bucket_id);
    std::move(callback).Run();
  }
}

void IndexedDBContextImpl::StopMetadataRecording(
    storage::BucketId bucket_id,
    StopMetadataRecordingCallback callback) {
  pending_bucket_recording_.erase(bucket_id);
  auto iter = bucket_contexts_.find(bucket_id);
  if (iter != bucket_contexts_.end()) {
    iter->second.AsyncCall(&BucketContext::StopMetadataRecording)
        .Then(std::move(callback));
  } else {
    std::move(callback).Run({});
  }
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

        auto collect_buckets =
            base::BarrierCallback<storage::QuotaErrorOr<storage::BucketInfo>>(
                handler->bucket_set_.size(),
                base::BindOnce(
                    &IndexedDBContextImpl::ContinueGetAllBucketsDetails,
                    handler, std::move(callback)));

        for (const BucketLocator& bucket_locator : handler->bucket_set_) {
          handler->quota_manager_proxy_->GetBucketById(
              bucket_locator.id, handler->idb_task_runner_, collect_buckets);
        }
      },
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IndexedDBContextImpl::ContinueGetAllBucketsDetails(
    GetAllBucketsDetailsCallback callback,
    std::vector<storage::QuotaErrorOr<storage::BucketInfo>> bucket_infos) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  // This barrier receives the bucket info from individual bucket contexts and
  // invokes the next step in the process, `FinishGetAllBucketsDetails`.
  auto barrier = base::BarrierCallback<storage::mojom::IdbBucketMetadataPtr>(
      bucket_infos.size(),
      base::BindOnce(&FinishGetAllBucketsDetails,
                     base::BindOnce(std::move(callback), in_memory())));

  // Iterate over existing bucket contexts, pre-fill some data into the bucket
  // info struct, and invoke `FillInBucketMetadata`.
  for (const auto& quota_error_or_bucket_info : bucket_infos) {
    if (!quota_error_or_bucket_info.has_value()) {
      barrier.Run({});
      continue;
    }
    const storage::BucketInfo& bucket_info = quota_error_or_bucket_info.value();
    const BucketLocator bucket_locator = bucket_info.ToBucketLocator();

    storage::mojom::IdbBucketMetadataPtr info =
        storage::mojom::IdbBucketMetadata::New();
    info->bucket_locator = bucket_locator;
    info->name = bucket_info.name;
    if (!in_memory()) {
      // Size for in-memory DBs will be filled in
      // `BucketContext::FillInMetadata()`.
      info->size = static_cast<double>(GetBucketDiskUsage(bucket_locator));
    }
    info->last_modified = GetBucketLastModified(bucket_locator);

    if (!in_memory()) {
      info->paths = GetStoragePaths(bucket_locator);
    }
    FillInBucketMetadata(std::move(info), barrier);
  }
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
            if (context) {
              context->observers_.Add(std::move(observer));
            }
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
  DCHECK(BucketContextExists(bucket_locator.id));
  bucket_contexts_.find(bucket_locator.id)
      ->second.AsyncCall(&BucketContext::WriteToIndexedDBForTesting)
      .WithArgs(key, value)
      .Then(std::move(callback));
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
      ->second.AsyncCall(&BucketContext::CompactBackingStoreForTesting)
      .Then(std::move(callback));
}

void IndexedDBContextImpl::GetUsageForTesting(
    GetUsageForTestingCallback callback) {
  if (in_memory()) {
      DCHECK_EQ(1U, bucket_contexts_.size());
      GetInMemorySize(bucket_contexts_.begin()->first, std::move(callback));
      return;
  }

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

std::optional<BucketLocator> IndexedDBContextImpl::LookUpBucket(
    storage::BucketId bucket_id) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  auto bucket_locator =
      base::ranges::find(bucket_set_, bucket_id, &BucketLocator::id);
  if (bucket_locator == bucket_set_.end()) {
    return std::nullopt;
  }

  return *bucket_locator;
}

int64_t IndexedDBContextImpl::GetBucketDiskUsage(
    const BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(!in_memory());
  if (!LookUpBucket(bucket_locator.id)) {
    return 0;
  }

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
  if (!LookUpBucket(bucket_locator.id)) {
    return base::Time();
  }

  // Only used by indexeddb-internals; not worth the complexity to implement.
  if (in_memory()) {
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
  if (in_memory()) {
    return base::FilePath();
  }

  if (indexed_db::ShouldUseLegacyFilePath(bucket_locator)) {
    // First-party idb files for the default, for legacy reasons, are stored at:
    // {{storage_partition_path}}/IndexedDB/
    // TODO(crbug.com/40221733): Migrate all first party buckets to the new
    // path.
    return GetLegacyDataPath();
  }

  // Third-party idb files are stored at:
  // {{storage_partition_path}}/WebStorage/{{bucket_id}}/IndexedDB/
  return quota_manager_proxy_->GetClientBucketPath(
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
  for (auto& [bucket_id, context] : bucket_contexts_) {
    context.AsyncCall(&BucketContext::ForceClose).WithArgs(/*doom=*/false);
  }
  bucket_contexts_.clear();
  task_runner_limiters_.clear();

  // Shutdown won't go through `ShutdownOnIDBSequence()` for in-memory DBs and
  // in some tests.
  if (!shutdown_start_time_.is_null()) {
    base::UmaHistogramTimes("IndexedDB.ContextShutdownDuration",
                            base::TimeTicks::Now() - shutdown_start_time_);
  }
}

void IndexedDBContextImpl::ShutdownOnIDBSequence(base::TimeTicks start_time) {
  // `this` will be destroyed when this method returns.
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  shutdown_start_time_ = start_time;

  if (force_keep_session_state_) {
    return;
  }

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
      ForceClose(bucket_locator.id, {},
                 base::BindOnce(
                     [](std::vector<base::FilePath> paths) {
                       base::ranges::for_each(paths,
                                              &base::DeletePathRecursively);
                     },
                     GetStoragePaths(bucket_locator)));
    }
  }
}

// static
void IndexedDBContextImpl::Shutdown(
    std::unique_ptr<IndexedDBContextImpl> context) {
  IndexedDBContextImpl* context_ptr = context.get();

  // Important: This function is NOT called on the IDB Task Runner. All variable
  // access must be thread-safe.
  if (context->in_memory()) {
    context_ptr->IDBTaskRunner()->DeleteSoon(FROM_HERE, std::move(context));
    return;
  }

  context_ptr->IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &IndexedDBContextImpl::InitializeFromFilesIfNeeded,
          base::Unretained(context_ptr),
          base::BindOnce(&IndexedDBContextImpl::ShutdownOnIDBSequence,
                         std::move(context), base::TimeTicks::Now())));
}

base::FilePath IndexedDBContextImpl::GetBlobStorePath(
    const BucketLocator& bucket_locator) const {
  DCHECK(!in_memory());
  return GetDataPath(bucket_locator)
      .Append(indexed_db::GetBlobStoreFileName(bucket_locator));
}

base::FilePath IndexedDBContextImpl::GetLevelDBPath(
    const BucketLocator& bucket_locator) const {
  DCHECK(!in_memory());
  return GetDataPath(bucket_locator)
      .Append(indexed_db::GetLevelDBFileName(bucket_locator));
}

int64_t IndexedDBContextImpl::ReadUsageFromDisk(
    const BucketLocator& bucket_locator,
    bool write_in_progress) const {
  DCHECK(!in_memory());

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
  quota_manager_proxy_->NotifyBucketModified(
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
  std::map<StorageKey, base::FilePath> storage_key_to_file_path =
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
    quota_manager_proxy_->UpdateOrCreateBucket(
        storage::BucketInitParams::ForDefaultBucket(storage_key),
        idb_task_runner_, on_lookup_done);
  }

  for (const auto& [bucket_id, file_path] : bucket_id_to_file_path) {
    quota_manager_proxy_->GetBucketById(bucket_id, idb_task_runner_,
                                        on_lookup_done);
  }
}

void IndexedDBContextImpl::ForceInitializeFromFilesForTesting(
    ForceInitializeFromFilesForTestingCallback callback) {
  did_initialize_from_files_ = false;
  InitializeFromFilesIfNeeded(std::move(callback));
}

std::map<StorageKey, base::FilePath>
IndexedDBContextImpl::FindLegacyIndexedDBFiles() const {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath data_path = GetLegacyDataPath();
  if (data_path.empty())
    return {};
  std::map<StorageKey, base::FilePath> storage_key_to_file_path;
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
    storage_key_to_file_path[StorageKey::CreateFirstParty(origin)] = file_path;
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
    BucketContext::InstanceClosure callback) {
  for_each_bucket_context_ = callback;
  for (auto& [bucket_id, bucket_context] : bucket_contexts_) {
    bucket_context.AsyncCall(&BucketContext::RunInstanceClosure)
        .WithArgs(for_each_bucket_context_);
  }
}

void IndexedDBContextImpl::GetInMemorySize(
    storage::BucketId bucket_id,
    base::OnceCallback<void(int64_t)> on_got_size) const {
    auto iter = bucket_contexts_.find(bucket_id);
    if (iter == bucket_contexts_.end()) {
      std::move(on_got_size).Run(0);
    } else {
      iter->second.AsyncCall(&BucketContext::GetInMemorySize)
          .Then(std::move(on_got_size));
    }
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

base::SequenceBound<BucketContext>*
IndexedDBContextImpl::GetBucketContextForTesting(const storage::BucketId& id) {
  auto it = bucket_contexts_.find(id);
  if (it != bucket_contexts_.end()) {
    return &it->second;
  }
  return nullptr;
}

void IndexedDBContextImpl::FillInBucketMetadata(
    storage::mojom::IdbBucketMetadataPtr info,
    base::OnceCallback<void(storage::mojom::IdbBucketMetadataPtr)> result) {
  if (!BucketContextExists(info->bucket_locator.id)) {
    std::move(result).Run(std::move(info));
    return;
  }

  bucket_contexts_.find(info->bucket_locator.id)
      ->second.AsyncCall(&BucketContext::FillInMetadata)
      .WithArgs(std::move(info))
      .Then(std::move(result));
}

void IndexedDBContextImpl::DestroyBucketContext(
    storage::BucketLocator bucket_locator) {
  bucket_contexts_.erase(bucket_locator.id);
  task_runner_limiters_[bucket_locator.storage_key.top_level_site()]
      .active_bucket_count--;
}

void IndexedDBContextImpl::EnsureBucketContext(
    const storage::BucketInfo& bucket,
    const base::FilePath& data_directory) {
  TRACE_EVENT0("IndexedDB", "indexed_db::EnsureBucketContext");

  if (BucketContextExists(bucket.id)) {
    return;
  }

  const BucketLocator bucket_locator = bucket.ToBucketLocator();
  BucketContext::Delegate bucket_delegate;
  bucket_delegate.on_ready_for_destruction = base::BindPostTask(
      idb_task_runner_,
      base::BindOnce(&IndexedDBContextImpl::DestroyBucketContext,
                     weak_factory_.GetWeakPtr(), bucket_locator));
  bucket_delegate.on_content_changed = base::BindPostTask(
      idb_task_runner_,
      base::BindRepeating(&IndexedDBContextImpl::NotifyIndexedDBContentChanged,
                          weak_factory_.GetWeakPtr(), bucket_locator));
  bucket_delegate.on_files_written = base::BindPostTask(
      idb_task_runner_,
      base::BindRepeating(&IndexedDBContextImpl::OnFilesWritten,
                          weak_factory_.GetWeakPtr(), bucket_locator));
  bucket_delegate.for_each_bucket_context = base::BindPostTask(
      idb_task_runner_,
      base::BindRepeating(&IndexedDBContextImpl::ForEachBucketContext,
                          weak_factory_.GetWeakPtr()));

  mojo::PendingRemote<storage::mojom::BlobStorageContext>
      cloned_blob_storage_context;
  // May be null in unit tests.
  if (blob_storage_context_) {
    blob_storage_context_->Clone(
        cloned_blob_storage_context.InitWithNewPipeAndPassReceiver());
  }

  mojo::PendingRemote<storage::mojom::FileSystemAccessContext> fsa_context;
  // May be null in unit tests.
  if (file_system_access_context_) {
    file_system_access_context_->Clone(
        fsa_context.InitWithNewPipeAndPassReceiver());
  }

  // See docs above `TaskRunnerLimiter`.
  scoped_refptr<base::SequencedTaskRunner> bucket_task_runner;
  TaskRunnerLimiter& task_runner_limiter =
      task_runner_limiters_[bucket_locator.storage_key.top_level_site()];
  static int kTaskRunnerCountLimit = base::SysInfo::NumberOfProcessors();
  if (++task_runner_limiter.active_bucket_count > kTaskRunnerCountLimit) {
    if (!task_runner_limiter.overflow_task_runner) {
      task_runner_limiter.overflow_task_runner = CreateTaskRunner();
    }
    bucket_task_runner = task_runner_limiter.overflow_task_runner;
  } else {
    bucket_task_runner = CreateTaskRunner();
  }

  const auto& [iter, inserted] = bucket_contexts_.emplace(
      bucket_locator.id,
      base::SequenceBound<BucketContext>(
          force_single_thread_ ? IDBTaskRunner()
                               : std::move(bucket_task_runner),
          bucket, data_directory, std::move(bucket_delegate),
          quota_manager_proxy_, std::move(cloned_blob_storage_context),
          std::move(fsa_context), for_each_bucket_context_));
  DCHECK(inserted);
  if (pending_failure_injector_) {
    iter->second.AsyncCall(&BucketContext::BindMockFailureSingletonForTesting)
        .WithArgs(std::move(pending_failure_injector_));
  }
  // Start metadata recording on the context if it was pending.
  if (pending_bucket_recording_.erase(bucket_locator.id)) {
    iter->second.AsyncCall(&BucketContext::StartMetadataRecording);
  }

  bucket_set_.insert(bucket_locator);
}

void IndexedDBContextImpl::GetBucketUsage(const BucketLocator& bucket,
                                          GetBucketUsageCallback callback) {
  DCHECK_EQ(bucket.type, blink::mojom::StorageType::kTemporary);
  if (in_memory()) {
    GetInMemorySize(bucket.id, std::move(callback));
  } else {
    std::move(callback).Run(GetBucketDiskUsage(bucket));
  }
}

void IndexedDBContextImpl::GetStorageKeysForType(
    blink::mojom::StorageType type,
    GetStorageKeysForTypeCallback callback) {
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);
  std::vector<StorageKey> storage_keys;
  storage_keys.reserve(bucket_set_.size());
  for (const BucketLocator& bucket_locator : bucket_set_) {
    storage_keys.push_back(bucket_locator.storage_key);
  }
  std::move(callback).Run(std::move(storage_keys));
}

void IndexedDBContextImpl::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);
  std::move(callback).Run();
}

bool IndexedDBContextImpl::BucketContextExists(storage::BucketId bucket_id) {
  return bucket_contexts_.find(bucket_id) != bucket_contexts_.end();
}

IndexedDBContextImpl::TaskRunnerLimiter::TaskRunnerLimiter() = default;
IndexedDBContextImpl::TaskRunnerLimiter::~TaskRunnerLimiter() = default;

}  // namespace content::indexed_db
