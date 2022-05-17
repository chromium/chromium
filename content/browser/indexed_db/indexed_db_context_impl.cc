// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_context_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/histogram_functions.h"
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
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_client_callback_wrapper.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_quota_client.h"
#include "content/browser/indexed_db/indexed_db_storage_key_state.h"
#include "content/browser/indexed_db/indexed_db_storage_key_state_handle.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/mock_browsertest_indexed_db_class_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "third_party/zlib/google/zip.h"
#include "url/origin.h"

using base::DictionaryValue;
using base::ListValue;
using storage::DatabaseUtil;

namespace content {
const base::FilePath::CharType IndexedDBContextImpl::kIndexedDBDirectory[] =
    FILE_PATH_LITERAL("IndexedDB");

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

// This may be called after the IndexedDBContext is destroyed.
void GetAllStorageKeysAndPaths(const base::FilePath& indexeddb_path,
                               std::vector<blink::StorageKey>* storage_keys,
                               std::vector<base::FilePath>* file_paths) {
  // TODO(jsbell): DCHECK that this is running on an IndexedDB sequence,
  // if a global handle to it is ever available.
  if (indexeddb_path.empty())
    return;
  base::FileEnumerator file_enumerator(indexeddb_path, false,
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
      storage_keys->push_back(
          blink::StorageKey(storage::GetOriginFromIdentifier(storage_key_id)));
      if (file_paths)
        file_paths->push_back(file_path);
    }
  }
}

}  // namespace

// static
void IndexedDBContextImpl::ReleaseOnIDBSequence(
    scoped_refptr<IndexedDBContextImpl>&& context) {
  if (!context->idb_task_runner_->RunsTasksInCurrentSequence()) {
    IndexedDBContextImpl* context_ptr = context.get();
    context_ptr->IDBTaskRunner()->ReleaseSoon(FROM_HERE, std::move(context));
  }
}

IndexedDBContextImpl::IndexedDBContextImpl(
    const base::FilePath& data_path,
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
      data_path_(data_path.empty() ? base::FilePath()
                                   : data_path.Append(kIndexedDBDirectory)),
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
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  receivers_.Add(this, std::move(control));
}

void IndexedDBContextImpl::BindIndexedDB(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  // Ensure default bucket exists for storage key on storage access and add
  // bind receiver on retrieval.
  quota_manager_proxy()->GetOrCreateBucket(
      storage_key, storage::kDefaultBucketName, idb_task_runner_,
      base::BindOnce(&IndexedDBContextImpl::BindIndexedDBWithBucket,
                     weak_factory_.GetWeakPtr(), storage_key,
                     std::move(receiver)));
}

void IndexedDBContextImpl::GetUsage(GetUsageCallback usage_callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  std::vector<blink::StorageKey> storage_keys = GetAllStorageKeys();
  std::vector<storage::mojom::StorageUsageInfoPtr> result;
  for (const auto& storage_key : storage_keys) {
    // TODO(https://crbug.com/1199077): Pass the real StorageKey when
    // StorageUsageInfo is converted.
    result.emplace_back(storage::mojom::StorageUsageInfo::New(
        storage_key.origin(), GetStorageKeyDiskUsage(storage_key),
        GetStorageKeyLastModified(storage_key)));
  }
  std::move(usage_callback).Run(std::move(result));
}

// Note - this is being kept async (instead of having a 'sync' version) to allow
// ForceClose to become asynchronous.  This is required for
// https://crbug.com/965142.
void IndexedDBContextImpl::DeleteForStorageKey(
    const blink::StorageKey& storage_key,
    DeleteForStorageKeyCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  ForceCloseSync(storage_key,
                 storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN);
  if (!HasStorageKey(storage_key)) {
    std::move(callback).Run(true);
    return;
  }

  if (is_incognito()) {
    GetStorageKeySet()->erase(storage_key);
    storage_key_size_map_.erase(storage_key);
    std::move(callback).Run(true);
    return;
  }

  base::FilePath idb_directory = GetLevelDBPath(storage_key);
  EnsureDiskUsageCacheInitialized(storage_key);

  leveldb::Status s =
      IndexedDBClassFactory::Get()->leveldb_factory().DestroyLevelDB(
          idb_directory);
  bool success = s.ok();
  if (success)
    success =
        filesystem_proxy_->DeletePathRecursively(GetBlobStorePath(storage_key));
  QueryDiskAndUpdateQuotaUsage(storage_key);
  if (success) {
    GetStorageKeySet()->erase(storage_key);
    storage_key_size_map_.erase(storage_key);
  }
  std::move(callback).Run(success);
}

void IndexedDBContextImpl::ForceClose(const blink::StorageKey& storage_key,
                                      storage::mojom::ForceCloseReason reason,
                                      base::OnceClosure closure) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  base::UmaHistogramEnumeration("WebCore.IndexedDB.Context.ForceCloseReason",
                                reason);
  if (!HasStorageKey(storage_key)) {
    std::move(closure).Run();
    return;
  }

  if (!indexeddb_factory_.get()) {
    std::move(closure).Run();
    return;
  }

  // Make a copy of storage_key, as the ref might go away here during the close.
  auto storage_key_copy = storage_key;
  indexeddb_factory_->ForceClose(
      storage_key_copy,
      reason == storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN);
  DCHECK_EQ(0UL, GetConnectionCountSync(storage_key_copy));
  std::move(closure).Run();
}

void IndexedDBContextImpl::GetConnectionCount(
    const blink::StorageKey& storage_key,
    GetConnectionCountCallback callback) {
  std::move(callback).Run(GetConnectionCountSync(storage_key));
}

void IndexedDBContextImpl::DownloadStorageKeyData(
    const blink::StorageKey& storage_key,
    DownloadStorageKeyDataCallback callback) {
  // All of this must run on the IndexedDB task runner to prevent script from
  // reopening the storage_key while we are zipping.
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  bool success = false;

  // Make sure the database hasn't been deleted.
  if (!HasStorageKey(storage_key)) {
    std::move(callback).Run(success, base::FilePath(), base::FilePath());
    return;
  }

  ForceCloseSync(storage_key,
                 storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE);

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    std::move(callback).Run(success, base::FilePath(), base::FilePath());
    return;
  }

  // This will need to get cleaned up after the download has completed.
  base::FilePath temp_path = temp_dir.Take();

  std::string storage_key_id =
      storage::GetIdentifierFromOrigin(storage_key.origin());
  base::FilePath zip_path = temp_path.AppendASCII(storage_key_id)
                                .AddExtension(FILE_PATH_LITERAL("zip"));

  std::vector<base::FilePath> paths = GetStoragePaths(storage_key);
  zip::ZipWithFilterCallback(data_path(), zip_path,
                             base::BindRepeating(IsAllowedPath, paths));

  success = true;
  std::move(callback).Run(success, temp_path, zip_path);
}

void IndexedDBContextImpl::GetAllStorageKeysDetails(
    GetAllStorageKeysDetailsCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  std::vector<blink::StorageKey> storage_keys = GetAllStorageKeys();

  std::sort(storage_keys.begin(), storage_keys.end());

  base::Value::List list;
  for (const auto& storage_key : storage_keys) {
    base::Value info(base::Value::Type::DICTIONARY);
    // TODO(https://crbug.com/1199077): Serialize storage key directly
    // once supported by OriginDetails.
    info.SetStringKey("url", storage_key.origin().Serialize());
    info.SetDoubleKey("size",
                      static_cast<double>(GetStorageKeyDiskUsage(storage_key)));
    info.SetDoubleKey("last_modified",
                      GetStorageKeyLastModified(storage_key).ToJsTime());

    base::Value paths(base::Value::Type::LIST);
    if (!is_incognito()) {
      for (const base::FilePath& path : GetStoragePaths(storage_key))
        paths.Append(path.AsUTF8Unsafe());
    } else {
      paths.Append("N/A");
    }
    info.SetKey("paths", std::move(paths));
    info.SetDoubleKey("connection_count", GetConnectionCountSync(storage_key));

    // This ends up being O(NlogN), where N = number of open databases. We
    // iterate over all open databases to extract just those in the storage_key,
    // and we're iterating over all storage_keys in the outer loop.

    if (!indexeddb_factory_.get()) {
      list.Append(std::move(info));
      continue;
    }
    std::vector<IndexedDBDatabase*> databases =
        indexeddb_factory_->GetOpenDatabasesForStorageKey(storage_key);
    // TODO(jsbell): Sort by name?
    base::Value database_list(base::Value::Type::LIST);

    for (IndexedDBDatabase* db : databases) {
      base::Value db_info(base::Value::Type::DICTIONARY);

      db_info.SetStringKey("name", db->name());
      db_info.SetDoubleKey("connection_count", db->ConnectionCount());
      db_info.SetDoubleKey("active_open_delete", db->ActiveOpenDeleteCount());
      db_info.SetDoubleKey("pending_open_delete", db->PendingOpenDeleteCount());

      base::Value transaction_list(base::Value::Type::LIST);

      for (IndexedDBConnection* connection : db->connections()) {
        for (const auto& transaction_id_pair : connection->transactions()) {
          const auto* transaction = transaction_id_pair.second.get();
          base::Value transaction_info(base::Value::Type::DICTIONARY);

          switch (transaction->mode()) {
            case blink::mojom::IDBTransactionMode::ReadOnly:
              transaction_info.SetStringKey("mode", "readonly");
              break;
            case blink::mojom::IDBTransactionMode::ReadWrite:
              transaction_info.SetStringKey("mode", "readwrite");
              break;
            case blink::mojom::IDBTransactionMode::VersionChange:
              transaction_info.SetStringKey("mode", "versionchange");
              break;
          }

          switch (transaction->state()) {
            case IndexedDBTransaction::CREATED:
              transaction_info.SetStringKey("status", "blocked");
              break;
            case IndexedDBTransaction::STARTED:
              if (transaction->diagnostics().tasks_scheduled > 0)
                transaction_info.SetStringKey("status", "running");
              else
                transaction_info.SetStringKey("status", "started");
              break;
            case IndexedDBTransaction::COMMITTING:
              transaction_info.SetStringKey("status", "committing");
              break;
            case IndexedDBTransaction::FINISHED:
              transaction_info.SetStringKey("status", "finished");
              break;
          }

          transaction_info.SetDoubleKey("tid", transaction->id());
          transaction_info.SetDoubleKey(
              "age",
              (base::Time::Now() - transaction->diagnostics().creation_time)
                  .InMillisecondsF());
          transaction_info.SetDoubleKey(
              "runtime",
              (base::Time::Now() - transaction->diagnostics().start_time)
                  .InMillisecondsF());
          transaction_info.SetDoubleKey(
              "tasks_scheduled", transaction->diagnostics().tasks_scheduled);
          transaction_info.SetDoubleKey(
              "tasks_completed", transaction->diagnostics().tasks_completed);

          base::Value scope(base::Value::Type::LIST);
          for (const auto& id : transaction->scope()) {
            auto stores_it = db->metadata().object_stores.find(id);
            if (stores_it != db->metadata().object_stores.end())
              scope.Append(stores_it->second.name);
          }

          transaction_info.SetKey("scope", std::move(scope));
          transaction_list.Append(std::move(transaction_info));
        }
      }
      db_info.SetKey("transactions", std::move(transaction_list));

      database_list.Append(std::move(db_info));
    }
    info.SetKey("databases", std::move(database_list));
    list.Append(std::move(info));
  }

  std::move(callback).Run(is_incognito(), std::move(list));
}

void IndexedDBContextImpl::SetForceKeepSessionState() {
  idb_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](IndexedDBContextImpl* context) {
            context->force_keep_session_state_ = true;
          },
          // As |this| is destroyed on the IDBTaskRunner it is safe to post raw.
          base::Unretained(this)));
}

void IndexedDBContextImpl::ApplyPolicyUpdates(
    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates) {
  idb_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](IndexedDBContextImpl* context,
             std::vector<storage::mojom::StoragePolicyUpdatePtr>
                 policy_updates) {
            for (const auto& update : policy_updates) {
              if (!update->purge_on_shutdown) {
                // TODO(https://crbug.com/1199077): Use the real StorageKey when
                // available.
                context->storage_keys_to_purge_on_shutdown_.erase(
                    blink::StorageKey(update->origin));
              } else {
                // TODO(https://crbug.com/1199077): Use the real StorageKey when
                // available.
                context->storage_keys_to_purge_on_shutdown_.insert(
                    blink::StorageKey(update->origin));
              }
            }
          },
          // As |this| is destroyed on the IDBTaskRunner it is safe to post raw.
          base::Unretained(this), std::move(policy_updates)));
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
          [](IndexedDBContextImpl* context,
             mojo::PendingRemote<storage::mojom::IndexedDBObserver> observer) {
            context->observers_.Add(std::move(observer));
          },
          // As |this| is destroyed on the IDBTaskRunner it is safe to post raw.
          base::Unretained(this), std::move(observer)));
}

void IndexedDBContextImpl::GetBaseDataPathForTesting(
    GetBaseDataPathForTestingCallback callback) {
  std::move(callback).Run(data_path());
}

void IndexedDBContextImpl::GetFilePathForTesting(
    const blink::StorageKey& storage_key,
    GetFilePathForTestingCallback callback) {
  std::move(callback).Run(GetLevelDBPath(storage_key));
}

void IndexedDBContextImpl::ResetCachesForTesting(base::OnceClosure callback) {
  storage_key_set_.reset();
  storage_key_size_map_.clear();
  std::move(callback).Run();
}

void IndexedDBContextImpl::ForceSchemaDowngradeForTesting(
    const blink::StorageKey& storage_key,
    ForceSchemaDowngradeForTestingCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  if (is_incognito() || !HasStorageKey(storage_key)) {
    std::move(callback).Run(false);
    return;
  }

  if (indexeddb_factory_.get()) {
    indexeddb_factory_->ForceSchemaDowngrade(storage_key);
    std::move(callback).Run(true);
    return;
  }
  ForceCloseSync(
      storage_key,
      storage::mojom::ForceCloseReason::FORCE_SCHEMA_DOWNGRADE_INTERNALS_PAGE);
  std::move(callback).Run(false);
}

void IndexedDBContextImpl::HasV2SchemaCorruptionForTesting(
    const blink::StorageKey& storage_key,
    HasV2SchemaCorruptionForTestingCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  if (is_incognito() || !HasStorageKey(storage_key)) {
    std::move(callback).Run(
        storage::mojom::V2SchemaCorruptionStatus::CORRUPTION_UNKNOWN);
    return;
  }

  if (indexeddb_factory_.get()) {
    std::move(callback).Run(
        static_cast<storage::mojom::V2SchemaCorruptionStatus>(
            indexeddb_factory_->HasV2SchemaCorruption(storage_key)));
    return;
  }
  return std::move(callback).Run(
      storage::mojom::V2SchemaCorruptionStatus::CORRUPTION_UNKNOWN);
}

void IndexedDBContextImpl::WriteToIndexedDBForTesting(
    const blink::StorageKey& storage_key,
    const std::string& key,
    const std::string& value,
    base::OnceClosure callback) {
  IndexedDBStorageKeyStateHandle handle;
  leveldb::Status s;
  std::tie(handle, s, std::ignore, std::ignore, std::ignore) =
      GetIDBFactory()->GetOrOpenStorageKeyFactory(storage_key, data_path(),
                                                  /*create_if_missing=*/true);
  CHECK(s.ok()) << s.ToString();
  CHECK(handle.IsHeld());

  TransactionalLevelDBDatabase* db =
      handle.storage_key_state()->backing_store()->db();
  std::string value_copy = value;
  s = db->Put(key, &value_copy);
  CHECK(s.ok()) << s.ToString();
  handle.Release();
  GetIDBFactory()->ForceClose(storage_key, true);
  std::move(callback).Run();
}

void IndexedDBContextImpl::GetBlobCountForTesting(
    const blink::StorageKey& storage_key,
    GetBlobCountForTestingCallback callback) {
  std::move(callback).Run(GetStorageKeyBlobFileCount(storage_key));
}

void IndexedDBContextImpl::GetNextBlobNumberForTesting(
    const blink::StorageKey& storage_key,
    int64_t database_id,
    GetNextBlobNumberForTestingCallback callback) {
  IndexedDBStorageKeyStateHandle handle;
  leveldb::Status s;
  std::tie(handle, s, std::ignore, std::ignore, std::ignore) =
      GetIDBFactory()->GetOrOpenStorageKeyFactory(storage_key, data_path(),
                                                  /*create_if_missing=*/true);
  CHECK(s.ok()) << s.ToString();
  CHECK(handle.IsHeld());

  TransactionalLevelDBDatabase* db =
      handle.storage_key_state()->backing_store()->db();

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
    const blink::StorageKey& storage_key,
    int64_t database_id,
    int64_t blob_number,
    GetPathForBlobForTestingCallback callback) {
  IndexedDBStorageKeyStateHandle handle;
  leveldb::Status s;
  std::tie(handle, s, std::ignore, std::ignore, std::ignore) =
      GetIDBFactory()->GetOrOpenStorageKeyFactory(storage_key, data_path(),
                                                  /*create_if_missing=*/true);
  CHECK(s.ok()) << s.ToString();
  CHECK(handle.IsHeld());

  IndexedDBBackingStore* backing_store =
      handle.storage_key_state()->backing_store();
  base::FilePath path =
      backing_store->GetBlobFileName(database_id, blob_number);
  std::move(callback).Run(path);
}

void IndexedDBContextImpl::CompactBackingStoreForTesting(
    const blink::StorageKey& storage_key,
    base::OnceClosure callback) {
  IndexedDBFactoryImpl* factory = GetIDBFactory();

  std::vector<IndexedDBDatabase*> databases =
      factory->GetOpenDatabasesForStorageKey(storage_key);

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

void IndexedDBContextImpl::ForceCloseSync(
    const blink::StorageKey& storage_key,
    storage::mojom::ForceCloseReason reason) {
  ForceClose(storage_key, reason, base::DoNothing());
}

IndexedDBFactoryImpl* IndexedDBContextImpl::GetIDBFactory() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!indexeddb_factory_.get()) {
    // Prime our cache of storage_keys with existing databases so we can
    // detect when dbs are newly created.
    GetStorageKeySet();
    indexeddb_factory_ = std::make_unique<IndexedDBFactoryImpl>(
        this, IndexedDBClassFactory::Get(), clock_);
  }
  return indexeddb_factory_.get();
}

std::vector<blink::StorageKey> IndexedDBContextImpl::GetAllStorageKeys() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  std::set<blink::StorageKey>* storage_keys_set = GetStorageKeySet();
  return std::vector<blink::StorageKey>(storage_keys_set->begin(),
                                        storage_keys_set->end());
}

bool IndexedDBContextImpl::HasStorageKey(const blink::StorageKey& storage_key) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  std::set<blink::StorageKey>* set = GetStorageKeySet();
  return set->find(storage_key) != set->end();
}

int IndexedDBContextImpl::GetStorageKeyBlobFileCount(
    const blink::StorageKey& storage_key) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  int count = 0;
  base::FileEnumerator file_enumerator(GetBlobStorePath(storage_key), true,
                                       base::FileEnumerator::FILES);
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    count++;
  }
  return count;
}

int64_t IndexedDBContextImpl::GetStorageKeyDiskUsage(
    const blink::StorageKey& storage_key) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!HasStorageKey(storage_key))
    return 0;

  EnsureDiskUsageCacheInitialized(storage_key);
  return storage_key_size_map_[storage_key];
}

base::Time IndexedDBContextImpl::GetStorageKeyLastModified(
    const blink::StorageKey& storage_key) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!HasStorageKey(storage_key))
    return base::Time();

  if (is_incognito()) {
    if (!indexeddb_factory_)
      return base::Time();
    return indexeddb_factory_->GetLastModified(storage_key);
  }

  base::FilePath idb_directory = GetLevelDBPath(storage_key);
  absl::optional<base::File::Info> info =
      filesystem_proxy_->GetFileInfo(idb_directory);
  if (!info.has_value())
    return base::Time();
  return info->last_modified;
}

size_t IndexedDBContextImpl::GetConnectionCountSync(
    const blink::StorageKey& storage_key) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!HasStorageKey(storage_key))
    return 0;

  if (!indexeddb_factory_.get())
    return 0;
  return indexeddb_factory_->GetConnectionCount(storage_key);
}

std::vector<base::FilePath> IndexedDBContextImpl::GetStoragePaths(
    const blink::StorageKey& storage_key) const {
  std::vector<base::FilePath> paths = {GetLevelDBPath(storage_key),
                                       GetBlobStorePath(storage_key)};
  return paths;
}

void IndexedDBContextImpl::FactoryOpened(const blink::StorageKey& storage_key) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (GetStorageKeySet()->insert(storage_key).second) {
    // A newly created db, notify the quota system.
    QueryDiskAndUpdateQuotaUsage(storage_key);
  } else {
    EnsureDiskUsageCacheInitialized(storage_key);
  }
}

void IndexedDBContextImpl::ConnectionOpened(
    const blink::StorageKey& storage_key,
    IndexedDBConnection* connection) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  quota_manager_proxy()->NotifyStorageAccessed(
      storage_key, blink::mojom::StorageType::kTemporary, base::Time::Now());
  if (GetStorageKeySet()->insert(storage_key).second) {
    // A newly created db, notify the quota system.
    QueryDiskAndUpdateQuotaUsage(storage_key);
  } else {
    EnsureDiskUsageCacheInitialized(storage_key);
  }
}

void IndexedDBContextImpl::ConnectionClosed(
    const blink::StorageKey& storage_key,
    IndexedDBConnection* connection) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  quota_manager_proxy()->NotifyStorageAccessed(
      storage_key, blink::mojom::StorageType::kTemporary, base::Time::Now());
  if (indexeddb_factory_.get() &&
      indexeddb_factory_->GetConnectionCount(storage_key) == 0)
    QueryDiskAndUpdateQuotaUsage(storage_key);
}

void IndexedDBContextImpl::TransactionComplete(
    const blink::StorageKey& storage_key) {
  DCHECK(!indexeddb_factory_.get() ||
         indexeddb_factory_->GetConnectionCount(storage_key) > 0);
  QueryDiskAndUpdateQuotaUsage(storage_key);
}

void IndexedDBContextImpl::DatabaseDeleted(
    const blink::StorageKey& storage_key) {
  GetStorageKeySet()->insert(storage_key);
  QueryDiskAndUpdateQuotaUsage(storage_key);
}

void IndexedDBContextImpl::BlobFilesCleaned(
    const blink::StorageKey& storage_key) {
  QueryDiskAndUpdateQuotaUsage(storage_key);
}

void IndexedDBContextImpl::NotifyIndexedDBListChanged(
    const blink::StorageKey& storage_key) {
  for (auto& observer : observers_)
    observer->OnIndexedDBListChanged(storage_key);
}

void IndexedDBContextImpl::NotifyIndexedDBContentChanged(
    const blink::StorageKey& storage_key,
    const std::u16string& database_name,
    const std::u16string& object_store_name) {
  for (auto& observer : observers_) {
    observer->OnIndexedDBContentChanged(storage_key, database_name,
                                        object_store_name);
  }
}

IndexedDBContextImpl::~IndexedDBContextImpl() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (indexeddb_factory_.get())
    indexeddb_factory_->ContextDestroyed();
}

void IndexedDBContextImpl::BindIndexedDBWithBucket(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  DCHECK(result.ok());
  dispatcher_host_.AddReceiver(storage_key, std::move(receiver));
}

void IndexedDBContextImpl::ShutdownOnIDBSequence() {
  DCHECK(idb_task_runner_->RunsTasksInCurrentSequence());

  if (force_keep_session_state_)
    return;

  // Clear session-only databases.
  if (storage_keys_to_purge_on_shutdown_.empty())
    return;

  std::vector<blink::StorageKey> storage_keys;
  std::vector<base::FilePath> file_paths;
  IndexedDBFactoryImpl* factory = GetIDBFactory();
  GetAllStorageKeysAndPaths(data_path_, &storage_keys, &file_paths);
  DCHECK_EQ(storage_keys.size(), file_paths.size());

  auto file_path = file_paths.cbegin();
  auto storage_key = storage_keys.cbegin();
  for (; storage_key != storage_keys.cend(); ++storage_key, ++file_path) {
    if (storage_keys_to_purge_on_shutdown_.find(*storage_key) ==
        storage_keys_to_purge_on_shutdown_.end())
      continue;
    factory->ForceClose(*storage_key, false);
    filesystem_proxy_->DeletePathRecursively(*file_path);
  }
}

void IndexedDBContextImpl::Shutdown() {
  // Important: This function is NOT called on the IDB Task Runner. All variable
  // access must be thread-safe.
  if (is_incognito())
    return;

  idb_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&IndexedDBContextImpl::ShutdownOnIDBSequence,
                                base::WrapRefCounted(this)));
}

base::FilePath IndexedDBContextImpl::GetBlobStorePath(
    const blink::StorageKey& storage_key) const {
  DCHECK(!is_incognito());
  return data_path_.Append(indexed_db::GetBlobStoreFileName(storage_key));
}

base::FilePath IndexedDBContextImpl::GetLevelDBPath(
    const blink::StorageKey& storage_key) const {
  DCHECK(!is_incognito());
  return data_path_.Append(indexed_db::GetLevelDBFileName(storage_key));
}

int64_t IndexedDBContextImpl::ReadUsageFromDisk(
    const blink::StorageKey& storage_key) const {
  if (is_incognito()) {
    if (!indexeddb_factory_)
      return 0;
    return indexeddb_factory_->GetInMemoryDBSize(storage_key);
  }

  int64_t total_size = 0;
  for (const base::FilePath& path : GetStoragePaths(storage_key))
    total_size += filesystem_proxy_->ComputeDirectorySize(path);
  return total_size;
}

void IndexedDBContextImpl::EnsureDiskUsageCacheInitialized(
    const blink::StorageKey& storage_key) {
  if (storage_key_size_map_.find(storage_key) == storage_key_size_map_.end())
    storage_key_size_map_[storage_key] = ReadUsageFromDisk(storage_key);
}

void IndexedDBContextImpl::QueryDiskAndUpdateQuotaUsage(
    const blink::StorageKey& storage_key) {
  int64_t former_disk_usage = storage_key_size_map_[storage_key];
  int64_t current_disk_usage = ReadUsageFromDisk(storage_key);
  int64_t difference = current_disk_usage - former_disk_usage;
  if (difference) {
    storage_key_size_map_[storage_key] = current_disk_usage;
    quota_manager_proxy()->NotifyStorageModified(
        storage::QuotaClientType::kIndexedDatabase, storage_key,
        blink::mojom::StorageType::kTemporary, difference, base::Time::Now(),
        base::SequencedTaskRunnerHandle::Get(), base::DoNothing());
    NotifyIndexedDBListChanged(storage_key);
  }
}

std::set<blink::StorageKey>* IndexedDBContextImpl::GetStorageKeySet() {
  if (!storage_key_set_) {
    std::vector<blink::StorageKey> storage_keys;
    GetAllStorageKeysAndPaths(data_path_, &storage_keys, nullptr);
    storage_key_set_ = std::make_unique<std::set<blink::StorageKey>>(
        storage_keys.begin(), storage_keys.end());
  }
  return storage_key_set_.get();
}

}  // namespace content
