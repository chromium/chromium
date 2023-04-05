// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_context_impl.h"

#include <memory>
#include <string>
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
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/values.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_factory.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/privileged/mojom/indexed_db_bucket_types.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/cpp/quota_client_callback_wrapper.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/indexed_db/indexed_db_bucket_state.h"
#include "content/browser/indexed_db/indexed_db_bucket_state_handle.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_quota_client.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/mock_browsertest_indexed_db_class_factory.h"
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

using storage::DatabaseUtil;

namespace content {

namespace {

static MockBrowserTestIndexedDBClassFactory* GetTestClassFactory() {
  static ::base::LazyInstance<MockBrowserTestIndexedDBClassFactory>::Leaky
      s_factory = LAZY_INSTANCE_INITIALIZER;
  return s_factory.Pointer();
}

static IndexedDBClassFactory* GetTestIDBClassFactory() {
  return GetTestClassFactory();
}

bool IsAllowedPath(const std::vector<base::FilePath>& allowed_paths,
                   const base::FilePath& candidate_path) {
  for (const base::FilePath& allowed_path : allowed_paths) {
    if (candidate_path == allowed_path || allowed_path.IsParent(candidate_path))
      return true;
  }
  return false;
}

}  // namespace

// static
void IndexedDBContextImpl::ReleaseOnIDBSequence(
    scoped_refptr<IndexedDBContextImpl>&& context) {
  if (!context->IDBTaskRunner()->RunsTasksInCurrentSequence()) {
    IndexedDBContextImpl* context_ptr = context.get();
    context_ptr->IDBTaskRunner()->ReleaseSoon(FROM_HERE, std::move(context));
  }
}

IndexedDBContextImpl::IndexedDBContextImpl(
    const base::FilePath& base_data_path,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::Clock* clock,
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
      dispatcher_host_(this, std::move(io_task_runner)),
      base_data_path_(base_data_path.empty() ? base::FilePath()
                                             : base_data_path),
      force_keep_session_state_(false),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      clock_(clock),
      quota_client_(std::make_unique<IndexedDBQuotaClient>(*this)),
      quota_client_wrapper_(
          std::make_unique<storage::QuotaClientCallbackWrapper>(
              quota_client_.get())),
      quota_client_receiver_(quota_client_wrapper_.get()),
      filesystem_proxy_(storage::CreateFilesystemProxy()) {
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

void IndexedDBContextImpl::Bind(
    mojo::PendingReceiver<storage::mojom::IndexedDBControl> control) {
  // We cannot run this in the constructor it needs to be async, but the async
  // tasks might not finish before the destructor runs.
  InitializeFromFilesIfNeeded(base::DoNothing());
  receivers_.Add(this, std::move(control));
}

void IndexedDBContextImpl::BindIndexedDB(
    const blink::StorageKey& storage_key,
    mojo::PendingAssociatedRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  quota_manager_proxy()->UpdateOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key),
      idb_task_runner_,
      base::BindOnce(
          &IndexedDBContextImpl::BindIndexedDBImpl, weak_factory_.GetWeakPtr(),
          std::move(client_state_checker_remote), std::move(receiver)));
}

void IndexedDBContextImpl::BindIndexedDBForBucket(
    const storage::BucketLocator& bucket_locator,
    mojo::PendingAssociatedRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  // Query the database to make sure the bucket still exists.

  quota_manager_proxy()->GetBucketById(
      bucket_locator.id, idb_task_runner_,
      base::BindOnce(
          &IndexedDBContextImpl::BindIndexedDBImpl, weak_factory_.GetWeakPtr(),
          std::move(client_state_checker_remote), std::move(receiver)));
}

void IndexedDBContextImpl::BindIndexedDBImpl(
    mojo::PendingAssociatedRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
    storage::QuotaErrorOr<storage::BucketInfo> bucket_info) {
  absl::optional<storage::BucketInfo> bucket;
  if (bucket_info.has_value()) {
    bucket = bucket_info.value();
  }
  dispatcher_host_.AddReceiver(
      IndexedDBDispatcherHost::ReceiverContext(
          bucket, std::move(client_state_checker_remote)),
      std::move(receiver));
}

void IndexedDBContextImpl::GetUsage(GetUsageCallback usage_callback) {
  InitializeFromFilesIfNeeded(
      base::BindOnce(&IndexedDBContextImpl::GetUsageImpl,
                     weak_factory_.GetWeakPtr(), std::move(usage_callback)));
}

void IndexedDBContextImpl::GetUsageImpl(GetUsageCallback usage_callback) {
  std::map<blink::StorageKey, storage::mojom::StorageUsageInfoPtr> usage_map;
  for (const auto& bucket_locator : GetAllBuckets()) {
    const auto& it = usage_map.find(bucket_locator.storage_key);
    if (it != usage_map.end()) {
      it->second->total_size_bytes += GetBucketDiskUsage(bucket_locator);
      const auto& last_modified = GetBucketLastModified(bucket_locator);
      if (it->second->last_modified < last_modified) {
        it->second->last_modified = last_modified;
      }
    } else {
      usage_map[bucket_locator.storage_key] =
          storage::mojom::StorageUsageInfo::New(
              bucket_locator.storage_key, GetBucketDiskUsage(bucket_locator),
              GetBucketLastModified(bucket_locator));
    }
  }
  std::vector<storage::mojom::StorageUsageInfoPtr> result;
  for (const auto& it : usage_map) {
    result.emplace_back(it.second->Clone());
  }
  std::move(usage_callback).Run(std::move(result));
}

// Note - this is being kept async (instead of having a 'sync' version) to allow
// ForceClose to become asynchronous.  This is required for
// https://crbug.com/965142.
void IndexedDBContextImpl::DeleteForStorageKey(
    const blink::StorageKey& storage_key,
    DeleteForStorageKeyCallback callback) {
  quota_manager_proxy_->GetBucketsForStorageKey(
      storage_key, blink::mojom::StorageType::kTemporary,
      /*delete_expired=*/false, IDBTaskRunner(),
      base::BindOnce(&IndexedDBContextImpl::OnGotBucketsForDeletion,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IndexedDBContextImpl::OnGotBucketsForDeletion(
    base::OnceCallback<void(bool)> callback,
    storage::QuotaErrorOr<std::set<storage::BucketInfo>> buckets) {
  if (!buckets.has_value() || buckets.value().empty()) {
    std::move(callback).Run(buckets.has_value());
    return;
  }

  auto barrier = base::BarrierCallback<bool>(
      buckets->size(),
      base::BindOnce(
          [](base::OnceCallback<void(bool)> final_callback,
             const std::vector<bool>& successes) {
            std::move(final_callback)
                .Run(base::ranges::all_of(
                    successes, [](bool success) { return success; }));
          },
          std::move(callback)));

  for (const auto& bucket : buckets.value()) {
    DoDeleteBucketData(bucket.ToBucketLocator(), barrier);
  }
}

void IndexedDBContextImpl::DeleteBucketData(
    const storage::BucketLocator& bucket_locator,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  DoDeleteBucketData(bucket_locator, std::move(callback));
}

void IndexedDBContextImpl::DoDeleteBucketData(
    const storage::BucketLocator& bucket_locator,
    base::OnceCallback<void(bool)> callback) {
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

  base::FilePath idb_file_path = GetLevelDBPath(bucket_locator);
  EnsureDiskUsageCacheInitialized(bucket_locator);

  leveldb::Status s =
      IndexedDBClassFactory::Get()->leveldb_factory().DestroyLevelDB(
          idb_file_path);
  bool success = s.ok();
  if (success) {
    success = filesystem_proxy_->DeletePathRecursively(
        GetBlobStorePath(bucket_locator));
  }

  QueryDiskAndUpdateQuotaUsage(bucket_locator);
  if (success) {
    bucket_set_.erase(bucket_locator);
    bucket_size_map_.erase(bucket_locator);
  }
  std::move(callback).Run(success);
}

void IndexedDBContextImpl::ForceClose(storage::BucketId bucket_id,
                                      storage::mojom::ForceCloseReason reason,
                                      base::OnceClosure closure) {
  base::UmaHistogramEnumeration("WebCore.IndexedDB.Context.ForceCloseReason",
                                reason);
  if (!LookUpBucket(bucket_id)) {
    std::move(closure).Run();
    return;
  }

  if (!indexeddb_factory_.get()) {
    std::move(closure).Run();
    return;
  }

  // Make a copy of storage_key, as the ref might go away here during the close.
  indexeddb_factory_->ForceClose(
      bucket_id,
      reason == storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN);
  DCHECK_EQ(0UL, GetConnectionCountSync(bucket_id));
  std::move(closure).Run();
}

void IndexedDBContextImpl::GetConnectionCount(
    storage::BucketId bucket_id,
    GetConnectionCountCallback callback) {
  GetConnectionCountImpl(std::move(callback), bucket_id);
}

void IndexedDBContextImpl::GetConnectionCountImpl(
    GetConnectionCountCallback callback,
    storage::BucketId bucket_id) {
  std::move(callback).Run(GetConnectionCountSync(bucket_id));
}

size_t IndexedDBContextImpl::GetConnectionCountSync(
    storage::BucketId bucket_id) {
  size_t count = 0;
  if (LookUpBucket(bucket_id) && indexeddb_factory_.get()) {
    count = indexeddb_factory_->GetConnectionCount(bucket_id);
  }
  return count;
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
        std::vector<storage::BucketLocator> bucket_locators =
            handler->GetAllBuckets();

        auto collect_buckets =
            base::BarrierCallback<storage::QuotaErrorOr<storage::BucketInfo>>(
                bucket_locators.size(),
                base::BindOnce(&IndexedDBContextImpl::OnBucketInfoReady,
                               handler, std::move(callback)));

        for (const auto& bucket_locator : bucket_locators) {
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
    const storage::BucketLocator bucket_locator = bucket_info.ToBucketLocator();

    storage::mojom::IdbBucketMetadataPtr info =
        storage::mojom::IdbBucketMetadata::New();
    info->bucket_locator = bucket_locator;
    info->name = bucket_info.name;
    info->size = static_cast<double>(GetBucketDiskUsage(bucket_locator));
    info->last_modified = GetBucketLastModified(bucket_locator);

    if (!is_incognito()) {
      info->paths = GetStoragePaths(bucket_locator);
    }
    info->connection_count = GetConnectionCountSync(bucket_info.id);

    // This ends up being O(NlogN), where N = number of open databases. We
    // iterate over all open databases to extract just those in the
    // bucket_locator, and we're iterating over all bucket_locators in the outer
    // loop.

    if (!indexeddb_factory_.get()) {
      bucket_map[bucket_info.storage_key.origin()][bucket_info.storage_key]
          .push_back(std::move(info));
      continue;
    }
    std::vector<IndexedDBDatabase*> databases =
        indexeddb_factory_->GetOpenDatabasesForBucket(bucket_locator);
    // TODO(jsbell): Sort by name?
    std::vector<storage::mojom::IdbDatabaseMetadataPtr> database_list;

    for (IndexedDBDatabase* db : databases) {
      storage::mojom::IdbDatabaseMetadataPtr db_info =
          storage::mojom::IdbDatabaseMetadata::New();

      db_info->name = db->name();
      db_info->connection_count = db->ConnectionCount();
      db_info->active_open_delete = db->ActiveOpenDeleteCount();
      db_info->pending_open_delete = db->PendingOpenDeleteCount();

      std::vector<storage::mojom::IdbTransactionMetadataPtr> transaction_list;

      for (IndexedDBConnection* connection : db->connections()) {
        for (const auto& transaction_id_pair : connection->transactions()) {
          const content::IndexedDBTransaction* transaction =
              transaction_id_pair.second.get();
          storage::mojom::IdbTransactionMetadataPtr transaction_info =
              storage::mojom::IdbTransactionMetadata::New();

          transaction_info->mode =
              static_cast<storage::mojom::IdbTransactionMode>(
                  transaction->mode());

          switch (transaction->state()) {
            case IndexedDBTransaction::CREATED:
              transaction_info->status =
                  storage::mojom::IdbTransactionState::kBlocked;
              break;
            case IndexedDBTransaction::STARTED:
              if (transaction->diagnostics().tasks_scheduled > 0) {
                transaction_info->status =
                    storage::mojom::IdbTransactionState::kRunning;
              } else {
                transaction_info->status =
                    storage::mojom::IdbTransactionState::kStarted;
              }
              break;
            case IndexedDBTransaction::COMMITTING:
              transaction_info->status =
                  storage::mojom::IdbTransactionState::kCommitting;
              break;
            case IndexedDBTransaction::FINISHED:
              transaction_info->status =
                  storage::mojom::IdbTransactionState::kFinished;
              break;
          }

          transaction_info->tid = transaction->id();
          transaction_info->age =
              (base::Time::Now() - transaction->diagnostics().creation_time)
                  .InMillisecondsF();
          transaction_info->runtime =
              (base::Time::Now() - transaction->diagnostics().start_time)
                  .InMillisecondsF();
          transaction_info->tasks_scheduled =
              transaction->diagnostics().tasks_scheduled;
          transaction_info->tasks_completed =
              transaction->diagnostics().tasks_completed;

          for (const int64_t& id : transaction->scope()) {
            auto stores_it = db->metadata().object_stores.find(id);
            if (stores_it != db->metadata().object_stores.end()) {
              transaction_info->scope.emplace_back(stores_it->second.name);
            }
          }

          transaction_list.push_back(std::move(transaction_info));
        }
      }
      db_info->transactions = std::move(transaction_list);

      database_list.push_back(std::move(db_info));
    }
    info->databases = std::move(database_list);
    bucket_map[bucket_info.storage_key.origin()][bucket_info.storage_key]
        .push_back(std::move(info));
  }

  std::vector<storage::mojom::IdbOriginMetadataPtr> origins;
  for (auto& [origin_url, top_level_site_map] : bucket_map) {
    storage::mojom::IdbOriginMetadataPtr origin_metadata =
        storage::mojom::IdbOriginMetadata::New();

    origin_metadata->origin = std::move(origin_url);

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
  IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<IndexedDBContextImpl> context) {
                       if (context)
                         context->force_keep_session_state_ = true;
                     },
                     weak_factory_.GetWeakPtr()));
}

void IndexedDBContextImpl::ApplyPolicyUpdates(
    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  for (const auto& update : policy_updates) {
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
    const storage::BucketLocator& bucket_locator,
    GetFilePathForTestingCallback callback) {
  std::move(callback).Run(GetLevelDBPath(bucket_locator));
}

void IndexedDBContextImpl::ResetCachesForTesting(base::OnceClosure callback) {
  bucket_set_.clear();
  bucket_size_map_.clear();
  std::move(callback).Run();
}

void IndexedDBContextImpl::ForceSchemaDowngradeForTesting(
    const storage::BucketLocator& bucket_locator,
    ForceSchemaDowngradeForTestingCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  if (is_incognito() || !LookUpBucket(bucket_locator.id)) {
    std::move(callback).Run(false);
    return;
  }

  if (indexeddb_factory_.get()) {
    indexeddb_factory_->ForceSchemaDowngrade(bucket_locator);
    std::move(callback).Run(true);
    return;
  }
  ForceClose(
      bucket_locator.id,
      storage::mojom::ForceCloseReason::FORCE_SCHEMA_DOWNGRADE_INTERNALS_PAGE,
      base::DoNothing());
  std::move(callback).Run(false);
}

void IndexedDBContextImpl::HasV2SchemaCorruptionForTesting(
    const storage::BucketLocator& bucket_locator,
    HasV2SchemaCorruptionForTestingCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  if (is_incognito() || !LookUpBucket(bucket_locator.id)) {
    std::move(callback).Run(
        storage::mojom::V2SchemaCorruptionStatus::CORRUPTION_UNKNOWN);
    return;
  }

  if (indexeddb_factory_.get()) {
    std::move(callback).Run(
        static_cast<storage::mojom::V2SchemaCorruptionStatus>(
            indexeddb_factory_->HasV2SchemaCorruption(bucket_locator)));
    return;
  }
  return std::move(callback).Run(
      storage::mojom::V2SchemaCorruptionStatus::CORRUPTION_UNKNOWN);
}

void IndexedDBContextImpl::WriteToIndexedDBForTesting(
    const storage::BucketLocator& bucket_locator,
    const std::string& key,
    const std::string& value,
    base::OnceClosure callback) {
  IndexedDBBucketStateHandle handle;
  leveldb::Status s;
  std::tie(handle, s, std::ignore, std::ignore, std::ignore) =
      GetIDBFactory()->GetOrOpenBucketFactory(bucket_locator,
                                              GetDataPath(bucket_locator),
                                              /*create_if_missing=*/true);
  CHECK(s.ok()) << s.ToString();
  CHECK(handle.IsHeld());

  TransactionalLevelDBDatabase* db =
      handle.bucket_state()->backing_store()->db();
  std::string value_copy = value;
  s = db->Put(key, &value_copy);
  CHECK(s.ok()) << s.ToString();
  handle.Release();
  GetIDBFactory()->ForceClose(bucket_locator.id, true);
  std::move(callback).Run();
}

void IndexedDBContextImpl::GetBlobCountForTesting(
    const storage::BucketLocator& bucket_locator,
    GetBlobCountForTestingCallback callback) {
  std::move(callback).Run(GetBucketBlobFileCount(bucket_locator));
}

void IndexedDBContextImpl::GetNextBlobNumberForTesting(
    const storage::BucketLocator& bucket_locator,
    int64_t database_id,
    GetNextBlobNumberForTestingCallback callback) {
  IndexedDBBucketStateHandle handle;
  leveldb::Status s;
  std::tie(handle, s, std::ignore, std::ignore, std::ignore) =
      GetIDBFactory()->GetOrOpenBucketFactory(bucket_locator,
                                              GetDataPath(bucket_locator),
                                              /*create_if_missing=*/true);
  CHECK(s.ok()) << s.ToString();
  CHECK(handle.IsHeld());

  TransactionalLevelDBDatabase* db =
      handle.bucket_state()->backing_store()->db();

  const std::string key_gen_key = DatabaseMetaDataKey::Encode(
      database_id, DatabaseMetaDataKey::BLOB_KEY_GENERATOR_CURRENT_NUMBER);
  std::string data;
  bool found = false;
  bool ok = db->Get(key_gen_key, &data, &found).ok();
  CHECK(found);
  CHECK(ok);
  base::StringPiece slice(data);
  int64_t number;
  CHECK(DecodeVarInt(&slice, &number));
  CHECK(DatabaseMetaDataKey::IsValidBlobNumber(number));

  std::move(callback).Run(number);
}

void IndexedDBContextImpl::GetPathForBlobForTesting(
    const storage::BucketLocator& bucket_locator,
    int64_t database_id,
    int64_t blob_number,
    GetPathForBlobForTestingCallback callback) {
  IndexedDBBucketStateHandle handle;
  leveldb::Status s;
  std::tie(handle, s, std::ignore, std::ignore, std::ignore) =
      GetIDBFactory()->GetOrOpenBucketFactory(bucket_locator,
                                              GetDataPath(bucket_locator),
                                              /*create_if_missing=*/true);
  CHECK(s.ok()) << s.ToString();
  CHECK(handle.IsHeld());

  IndexedDBBackingStore* backing_store = handle.bucket_state()->backing_store();
  base::FilePath path =
      backing_store->GetBlobFileName(database_id, blob_number);
  std::move(callback).Run(path);
}

void IndexedDBContextImpl::CompactBackingStoreForTesting(
    const storage::BucketLocator& bucket_locator,
    base::OnceClosure callback) {
  IndexedDBFactory* factory = GetIDBFactory();

  std::vector<IndexedDBDatabase*> databases =
      factory->GetOpenDatabasesForBucket(bucket_locator);

  if (!databases.empty()) {
    // Compact the first db's backing store since all the db's are in the same
    // backing store.
    IndexedDBDatabase* db = databases[0];
    IndexedDBBackingStore* backing_store = db->backing_store();
    backing_store->Compact();
  }
  std::move(callback).Run();
}

void IndexedDBContextImpl::BindMockFailureSingletonForTesting(
    mojo::PendingReceiver<storage::mojom::MockFailureInjector> receiver) {
  // Lazily instantiate the GetTestClassFactory.
  if (!mock_failure_injector_.has_value())
    mock_failure_injector_.emplace(GetTestClassFactory());

  // TODO(enne): this should really not be a static setter.
  CHECK(!mock_failure_injector_->is_bound());
  GetTestClassFactory()->Reset();
  IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(GetTestIDBClassFactory);

  mock_failure_injector_->Bind(std::move(receiver));
  mock_failure_injector_->set_disconnect_handler(base::BindOnce([]() {
    IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(nullptr);
  }));
}

void IndexedDBContextImpl::GetDatabaseKeysForTesting(
    GetDatabaseKeysForTestingCallback callback) {
  std::move(callback).Run(SchemaVersionKey::Encode(), DataVersionKey::Encode());
}

IndexedDBFactory* IndexedDBContextImpl::GetIDBFactory() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!indexeddb_factory_.get()) {
    indexeddb_factory_ = std::make_unique<IndexedDBFactory>(
        this, IndexedDBClassFactory::Get(), clock_);
  }
  return indexeddb_factory_.get();
}

std::vector<storage::BucketLocator> IndexedDBContextImpl::GetAllBuckets() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  return std::vector<storage::BucketLocator>(bucket_set_.begin(),
                                             bucket_set_.end());
}

absl::optional<storage::BucketLocator> IndexedDBContextImpl::LookUpBucket(
    storage::BucketId bucket_id) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  auto bucket_locator =
      base::ranges::find(bucket_set_, bucket_id, &storage::BucketLocator::id);
  if (bucket_locator == bucket_set_.end())
    return absl::nullopt;

  return *bucket_locator;
}

int IndexedDBContextImpl::GetBucketBlobFileCount(
    const storage::BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  int count = 0;
  base::FileEnumerator file_enumerator(GetBlobStorePath(bucket_locator), true,
                                       base::FileEnumerator::FILES);
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    count++;
  }
  return count;
}

int64_t IndexedDBContextImpl::GetBucketDiskUsage(
    const storage::BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!LookUpBucket(bucket_locator.id))
    return 0;
  EnsureDiskUsageCacheInitialized(bucket_locator);
  return bucket_size_map_[bucket_locator];
}

base::Time IndexedDBContextImpl::GetBucketLastModified(
    const storage::BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!LookUpBucket(bucket_locator.id))
    return base::Time();

  if (is_incognito()) {
    if (!indexeddb_factory_)
      return base::Time();
    return indexeddb_factory_->GetLastModified(bucket_locator);
  }

  base::FilePath idb_directory = GetLevelDBPath(bucket_locator);
  absl::optional<base::File::Info> info =
      filesystem_proxy_->GetFileInfo(idb_directory);
  if (!info.has_value())
    return base::Time();
  return info->last_modified;
}

std::vector<base::FilePath> IndexedDBContextImpl::GetStoragePaths(
    const storage::BucketLocator& bucket_locator) const {
  std::vector<base::FilePath> paths = {GetLevelDBPath(bucket_locator),
                                       GetBlobStorePath(bucket_locator)};
  return paths;
}

const base::FilePath IndexedDBContextImpl::GetDataPath(
    const storage::BucketLocator& bucket_locator) const {
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

void IndexedDBContextImpl::FactoryOpened(
    const storage::BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (bucket_set_.insert(bucket_locator).second) {
    // A newly created db, notify the quota system.
    QueryDiskAndUpdateQuotaUsage(bucket_locator);
  } else {
    EnsureDiskUsageCacheInitialized(bucket_locator);
  }
}

void IndexedDBContextImpl::ConnectionOpened(
    const storage::BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  quota_manager_proxy()->NotifyBucketAccessed(bucket_locator,
                                              base::Time::Now());
  if (bucket_set_.insert(bucket_locator).second) {
    // A newly created db, notify the quota system.
    QueryDiskAndUpdateQuotaUsage(bucket_locator);
  } else {
    EnsureDiskUsageCacheInitialized(bucket_locator);
  }
}

void IndexedDBContextImpl::ConnectionClosed(
    const storage::BucketLocator& bucket_locator) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  quota_manager_proxy()->NotifyBucketAccessed(bucket_locator,
                                              base::Time::Now());
  if (indexeddb_factory_.get() &&
      indexeddb_factory_->GetConnectionCount(bucket_locator.id) == 0)
    QueryDiskAndUpdateQuotaUsage(bucket_locator);
}

void IndexedDBContextImpl::TransactionComplete(
    const storage::BucketLocator& bucket_locator) {
  DCHECK(!indexeddb_factory_.get() ||
         indexeddb_factory_->GetConnectionCount(bucket_locator.id) > 0);
  QueryDiskAndUpdateQuotaUsage(bucket_locator);
}

void IndexedDBContextImpl::DatabaseDeleted(
    const storage::BucketLocator& bucket_locator) {
  bucket_set_.insert(bucket_locator);
  QueryDiskAndUpdateQuotaUsage(bucket_locator);
}

void IndexedDBContextImpl::BlobFilesCleaned(
    const storage::BucketLocator& bucket_locator) {
  QueryDiskAndUpdateQuotaUsage(bucket_locator);
}

void IndexedDBContextImpl::NotifyIndexedDBListChanged(
    const storage::BucketLocator& bucket_locator) {
  for (auto& observer : observers_) {
    observer->OnIndexedDBListChanged(bucket_locator);
  }
}

void IndexedDBContextImpl::NotifyIndexedDBContentChanged(
    const storage::BucketLocator& bucket_locator,
    const std::u16string& database_name,
    const std::u16string& object_store_name) {
  for (auto& observer : observers_) {
    observer->OnIndexedDBContentChanged(bucket_locator, database_name,
                                        object_store_name);
  }
}

IndexedDBContextImpl::~IndexedDBContextImpl() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (indexeddb_factory_.get())
    indexeddb_factory_->ContextDestroyed();
}

void IndexedDBContextImpl::ShutdownOnIDBSequence() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  if (force_keep_session_state_)
    return;

  // Clear session-only databases.
  if (origins_to_purge_on_shutdown_.empty()) {
    return;
  }

  IndexedDBFactory* factory = GetIDBFactory();
  const auto& storage_key_to_file_path = FindLegacyIndexedDBFiles();
  const auto& bucket_id_to_file_path = FindIndexedDBFiles();
  for (const auto& bucket_locator : bucket_set_) {
    // Delete the storage if its origin matches one of the origins to purge, or
    // if it is third-party and the top-level site is same-site with one of
    // those origins.
    auto delete_bucket = origins_to_purge_on_shutdown_.find(
                             bucket_locator.storage_key.origin()) !=
                         origins_to_purge_on_shutdown_.end();
    if (!delete_bucket) {
      auto& bucket_site = bucket_locator.storage_key.top_level_site();
      for (const auto& origin_to_purge : origins_to_purge_on_shutdown_) {
        if (net::SchemefulSite(origin_to_purge) == bucket_site) {
          delete_bucket = true;
          break;
        }
      }
    }
    if (!delete_bucket) {
      continue;
    }

    base::FilePath path;
    const auto& legacy_it =
        storage_key_to_file_path.find(bucket_locator.storage_key);
    const auto& bucket_path_it = bucket_id_to_file_path.find(bucket_locator.id);
    // If the bucket exists on the file system, it is in one of two possible
    // locations.
    if (legacy_it != storage_key_to_file_path.end()) {
      DCHECK(bucket_path_it == bucket_id_to_file_path.end());
      DCHECK(bucket_locator.storage_key.IsFirstPartyContext());
      DCHECK(bucket_locator.is_default);
      path = legacy_it->second;
    } else if (bucket_path_it != bucket_id_to_file_path.end()) {
      DCHECK(legacy_it == storage_key_to_file_path.end());
      path = bucket_path_it->second;
    }
    if (!path.empty()) {
      factory->ForceClose(bucket_locator.id, false);
      filesystem_proxy_->DeletePathRecursively(path);
    }
  }
}

void IndexedDBContextImpl::Shutdown() {
  // Important: This function is NOT called on the IDB Task Runner. All variable
  // access must be thread-safe.
  if (is_incognito())
    return;

  IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &IndexedDBContextImpl::InitializeFromFilesIfNeeded,
          weak_factory_.GetWeakPtr(),
          base::BindOnce(&IndexedDBContextImpl::ShutdownOnIDBSequence,
                         base::WrapRefCounted(this))));
}

base::FilePath IndexedDBContextImpl::GetBlobStorePath(
    const storage::BucketLocator& bucket_locator) const {
  DCHECK(!is_incognito());
  return GetDataPath(bucket_locator)
      .Append(indexed_db::GetBlobStoreFileName(bucket_locator));
}

base::FilePath IndexedDBContextImpl::GetLevelDBPath(
    const storage::BucketLocator& bucket_locator) const {
  DCHECK(!is_incognito());
  return GetDataPath(bucket_locator)
      .Append(indexed_db::GetLevelDBFileName(bucket_locator));
}

base::FilePath IndexedDBContextImpl::GetLevelDBPathForTesting(
    const storage::BucketLocator& bucket_locator) const {
  return GetLevelDBPath(bucket_locator);
}

int64_t IndexedDBContextImpl::ReadUsageFromDisk(
    const storage::BucketLocator& bucket_locator) const {
  if (is_incognito()) {
    if (!indexeddb_factory_)
      return 0;
    return indexeddb_factory_->GetInMemoryDBSize(bucket_locator);
  }

  int64_t total_size = 0;
  for (const base::FilePath& path : GetStoragePaths(bucket_locator))
    total_size += filesystem_proxy_->ComputeDirectorySize(path);
  return total_size;
}

void IndexedDBContextImpl::EnsureDiskUsageCacheInitialized(
    const storage::BucketLocator& bucket_locator) {
  if (bucket_size_map_.find(bucket_locator) == bucket_size_map_.end())
    bucket_size_map_[bucket_locator] = ReadUsageFromDisk(bucket_locator);
}

void IndexedDBContextImpl::QueryDiskAndUpdateQuotaUsage(
    const storage::BucketLocator& bucket_locator) {
  int64_t former_disk_usage = bucket_size_map_[bucket_locator];
  int64_t current_disk_usage = ReadUsageFromDisk(bucket_locator);
  int64_t difference = current_disk_usage - former_disk_usage;
  if (difference) {
    bucket_size_map_[bucket_locator] = current_disk_usage;
    quota_manager_proxy()->NotifyBucketModified(
        storage::QuotaClientType::kIndexedDatabase, bucket_locator, difference,
        base::Time::Now(), base::SequencedTaskRunner::GetCurrentDefault(),
        base::DoNothing());
    NotifyIndexedDBListChanged(bucket_locator);
  }
}

void IndexedDBContextImpl::InitializeFromFilesIfNeeded(
    base::OnceClosure callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (did_initialize_from_files_) {
    std::move(callback).Run();
    return;
  }
  const auto& storage_key_to_file_path = FindLegacyIndexedDBFiles();
  const auto& bucket_id_to_file_path = FindIndexedDBFiles();
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

  using Barrier =
      base::RepeatingCallback<void(absl::optional<storage::BucketLocator>)>;
  Barrier barrier =
      base::BarrierCallback<absl::optional<storage::BucketLocator>>(
          storage_key_to_file_path.size() + bucket_id_to_file_path.size(),
          base::BindOnce(
              [](base::WeakPtr<IndexedDBContextImpl> context,
                 const std::vector<absl::optional<storage::BucketLocator>>&
                     bucket_locators) {
                DCHECK(context);
                for (auto& locator : bucket_locators) {
                  if (locator)
                    context->bucket_set_.insert(*locator);
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
        if (bucket_info.has_value()) {
          barrier.Run(bucket_info->ToBucketLocator());
        } else {
          barrier.Run(absl::nullopt);
        }
      },
      barrier);

  for (auto& iter : storage_key_to_file_path) {
    quota_manager_proxy()->UpdateOrCreateBucket(
        storage::BucketInitParams::ForDefaultBucket(iter.first),
        idb_task_runner_, on_lookup_done);
  }

  for (auto& iter : bucket_id_to_file_path) {
    quota_manager_proxy()->GetBucketById(iter.first, idb_task_runner_,
                                         on_lookup_done);
  }
}

void IndexedDBContextImpl::ForceInitializeFromFilesForTesting(
    ForceInitializeFromFilesForTestingCallback callback) {
  did_initialize_from_files_ = false;
  InitializeFromFilesIfNeeded(std::move(callback));
}

const std::map<blink::StorageKey, base::FilePath>
IndexedDBContextImpl::FindLegacyIndexedDBFiles() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  base::FilePath data_path = GetLegacyDataPath();
  if (data_path.empty())
    return {};
  std::map<blink::StorageKey, base::FilePath> storage_key_to_file_path;
  base::FileEnumerator file_enumerator(data_path, /*recursive=*/false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() == indexed_db::kLevelDBExtension &&
        file_path.RemoveExtension().Extension() ==
            indexed_db::kIndexedDBExtension) {
      std::string storage_key_id = file_path.BaseName()
                                       .RemoveExtension()
                                       .RemoveExtension()
                                       .MaybeAsASCII();
      storage_key_to_file_path[blink::StorageKey::CreateFirstParty(
          storage::GetOriginFromIdentifier(storage_key_id))] = file_path;
    }
  }
  return storage_key_to_file_path;
}

const std::map<storage::BucketId, base::FilePath>
IndexedDBContextImpl::FindIndexedDBFiles() {
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

}  // namespace content
