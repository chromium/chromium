// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_context_impl.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_factory.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_origin_state.h"
#include "content/browser/indexed_db/indexed_db_origin_state_handle.h"
#include "content/browser/indexed_db/indexed_db_quota_client.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/mock_browsertest_indexed_db_class_factory.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "third_party/zlib/google/zip.h"
#include "url/origin.h"

using base::DictionaryValue;
using base::ListValue;
using storage::DatabaseUtil;
using url::Origin;

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
void GetAllOriginsAndPaths(const base::FilePath& indexeddb_path,
                           std::vector<Origin>* origins,
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
      std::string origin_id = file_path.BaseName()
                                  .RemoveExtension()
                                  .RemoveExtension()
                                  .MaybeAsASCII();
      origins->push_back(storage::GetOriginFromIdentifier(origin_id));
      if (file_paths)
        file_paths->push_back(file_path);
    }
  }
}

}  // namespace

IndexedDBContextImpl::IndexedDBContextImpl(
    const base::FilePath& data_path,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::Clock* clock,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context,
    mojo::PendingRemote<storage::mojom::NativeFileSystemContext>
        native_file_system_context,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> custom_task_runner)
    : base::RefCountedDeleteOnSequence<IndexedDBContextImpl>(
          custom_task_runner
              ? custom_task_runner
              : (base::ThreadPool::CreateSequencedTaskRunner(
                    {base::MayBlock(), base::WithBaseSyncPrimitives(),
                     base::TaskPriority::USER_VISIBLE,
                     // BLOCK_SHUTDOWN to support clearing session-only storage.
                     base::TaskShutdownBehavior::BLOCK_SHUTDOWN}))),
      indexed_db_factory_(this),
      force_keep_session_state_(false),
      quota_manager_proxy_(quota_manager_proxy),
      io_task_runner_(io_task_runner),
      clock_(clock),
      filesystem_proxy_(storage::CreateFilesystemProxy()) {
  IDB_TRACE("init");
  if (!data_path.empty())
    data_path_ = data_path.Append(kIndexedDBDirectory);
  quota_manager_proxy->RegisterClient(
      base::MakeRefCounted<IndexedDBQuotaClient>(this),
      storage::QuotaClientType::kIndexedDatabase,
      {blink::mojom::StorageType::kTemporary});

  // This is safe because the IndexedDBContextImpl must be destructed on the
  // IDBTaskRunner, and this task will always happen before that.
  if (blob_storage_context || native_file_system_context) {
    IDBTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](mojo::Remote<storage::mojom::BlobStorageContext>*
                   blob_storage_context,
               mojo::Remote<storage::mojom::NativeFileSystemContext>*
                   native_file_system_context,
               mojo::PendingRemote<storage::mojom::BlobStorageContext>
                   pending_blob_storage_context,
               mojo::PendingRemote<storage::mojom::NativeFileSystemContext>
                   pending_native_file_system_context) {
              if (pending_blob_storage_context) {
                blob_storage_context->Bind(
                    std::move(pending_blob_storage_context));
              }
              if (pending_native_file_system_context) {
                native_file_system_context->Bind(
                    std::move(pending_native_file_system_context));
              }
            },
            &blob_storage_context_, &native_file_system_context_,
            std::move(blob_storage_context),
            std::move(native_file_system_context)));
  }
}

void IndexedDBContextImpl::Bind(
    mojo::PendingReceiver<storage::mojom::IndexedDBControl> control) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  receivers_.Add(this, std::move(control));
}

void IndexedDBContextImpl::BindIndexedDB(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  indexed_db_factory_.AddReceiver(origin, std::move(receiver));
}

void IndexedDBContextImpl::GetUsage(GetUsageCallback usage_callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  std::vector<Origin> origins = GetAllOrigins();
  std::vector<storage::mojom::IndexedDBStorageUsageInfoPtr> result;
  for (const auto& origin : origins) {
    storage::mojom::IndexedDBStorageUsageInfoPtr usage_info =
        storage::mojom::IndexedDBStorageUsageInfo::New(
            origin, GetOriginDiskUsage(origin), GetOriginLastModified(origin));
    result.push_back(std::move(usage_info));
  }
  std::move(usage_callback).Run(std::move(result));
}

// Note - this is being kept async (instead of having a 'sync' version) to allow
// ForceClose to become asynchronous.  This is required for
// https://crbug.com/965142.
void IndexedDBContextImpl::DeleteForOrigin(const Origin& origin,
                                           DeleteForOriginCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  ForceCloseSync(origin,
                 storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN);
  if (!HasOrigin(origin)) {
    std::move(callback).Run(true);
    return;
  }

  if (is_incognito()) {
    GetOriginSet()->erase(origin);
    origin_size_map_.erase(origin);
    std::move(callback).Run(true);
    return;
  }

  base::FilePath idb_directory = GetLevelDBPath(origin);
  EnsureDiskUsageCacheInitialized(origin);

  leveldb::Status s =
      IndexedDBClassFactory::Get()->leveldb_factory().DestroyLevelDB(
          idb_directory);
  bool success = s.ok();
  if (success)
    success =
        filesystem_proxy_->DeletePathRecursively(GetBlobStorePath(origin));
  QueryDiskAndUpdateQuotaUsage(origin);
  if (success) {
    GetOriginSet()->erase(origin);
    origin_size_map_.erase(origin);
  }
  std::move(callback).Run(success);
}

void IndexedDBContextImpl::ForceClose(const Origin& origin,
                                      storage::mojom::ForceCloseReason reason,
                                      base::OnceClosure closure) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  base::UmaHistogramEnumeration("WebCore.IndexedDB.Context.ForceCloseReason",
                                reason);
  if (!HasOrigin(origin)) {
    std::move(closure).Run();
    return;
  }

  if (!indexeddb_factory_.get()) {
    std::move(closure).Run();
    return;
  }

  // Make a copy of origin, as the ref might go away here during the close.
  auto origin_copy = origin;
  indexeddb_factory_->ForceClose(
      origin_copy,
      reason == storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN);
  DCHECK_EQ(0UL, GetConnectionCountSync(origin_copy));
  std::move(closure).Run();
}

void IndexedDBContextImpl::GetConnectionCount(
    const Origin& origin,
    GetConnectionCountCallback callback) {
  std::move(callback).Run(GetConnectionCountSync(origin));
}

void IndexedDBContextImpl::DownloadOriginData(
    const url::Origin& origin,
    DownloadOriginDataCallback callback) {
  // All of this must run on the IndexedDB task runner to prevent script from
  // reopening the origin while we are zipping.
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  bool success = false;

  // Make sure the database hasn't been deleted.
  if (!HasOrigin(origin)) {
    std::move(callback).Run(success, base::FilePath(), base::FilePath());
    return;
  }

  ForceCloseSync(origin,
                 storage::mojom::ForceCloseReason::FORCE_CLOSE_INTERNALS_PAGE);

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    std::move(callback).Run(success, base::FilePath(), base::FilePath());
    return;
  }

  // This will need to get cleaned up after the download has completed.
  base::FilePath temp_path = temp_dir.Take();

  std::string origin_id = storage::GetIdentifierFromOrigin(origin);
  base::FilePath zip_path =
      temp_path.AppendASCII(origin_id).AddExtension(FILE_PATH_LITERAL("zip"));

  std::vector<base::FilePath> paths = GetStoragePaths(origin);
  zip::ZipWithFilterCallback(data_path(), zip_path,
                             base::BindRepeating(IsAllowedPath, paths));

  success = true;
  std::move(callback).Run(success, temp_path, zip_path);
}

void IndexedDBContextImpl::GetAllOriginsDetails(
    GetAllOriginsDetailsCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  std::vector<Origin> origins = GetAllOrigins();

  std::sort(origins.begin(), origins.end());

  base::ListValue list;
  for (const auto& origin : origins) {
    std::unique_ptr<base::DictionaryValue> info(
        std::make_unique<base::DictionaryValue>());
    info->SetString("url", origin.Serialize());
    info->SetDouble("size", static_cast<double>(GetOriginDiskUsage(origin)));
    info->SetDouble("last_modified", GetOriginLastModified(origin).ToJsTime());

    auto paths = std::make_unique<base::ListValue>();
    if (!is_incognito()) {
      for (const base::FilePath& path : GetStoragePaths(origin))
        paths->AppendString(path.value());
    } else {
      paths->AppendString("N/A");
    }
    info->Set("paths", std::move(paths));
    info->SetDouble("connection_count", GetConnectionCountSync(origin));

    // This ends up being O(NlogN), where N = number of open databases. We
    // iterate over all open databases to extract just those in the origin, and
    // we're iterating over all origins in the outer loop.

    if (!indexeddb_factory_.get()) {
      list.Append(std::move(info));
      continue;
    }
    std::vector<IndexedDBDatabase*> databases =
        indexeddb_factory_->GetOpenDatabasesForOrigin(origin);
    // TODO(jsbell): Sort by name?
    std::unique_ptr<base::ListValue> database_list(
        std::make_unique<base::ListValue>());

    for (IndexedDBDatabase* db : databases) {
      std::unique_ptr<base::DictionaryValue> db_info(
          std::make_unique<base::DictionaryValue>());

      db_info->SetString("name", db->name());
      db_info->SetDouble("connection_count", db->ConnectionCount());
      db_info->SetDouble("active_open_delete", db->ActiveOpenDeleteCount());
      db_info->SetDouble("pending_open_delete", db->PendingOpenDeleteCount());

      std::unique_ptr<base::ListValue> transaction_list(
          std::make_unique<base::ListValue>());

      for (IndexedDBConnection* connection : db->connections()) {
        for (const auto& transaction_id_pair : connection->transactions()) {
          const auto* transaction = transaction_id_pair.second.get();
          std::unique_ptr<base::DictionaryValue> transaction_info(
              std::make_unique<base::DictionaryValue>());

          switch (transaction->mode()) {
            case blink::mojom::IDBTransactionMode::ReadOnly:
              transaction_info->SetString("mode", "readonly");
              break;
            case blink::mojom::IDBTransactionMode::ReadWrite:
              transaction_info->SetString("mode", "readwrite");
              break;
            case blink::mojom::IDBTransactionMode::VersionChange:
              transaction_info->SetString("mode", "versionchange");
              break;
          }

          switch (transaction->state()) {
            case IndexedDBTransaction::CREATED:
              transaction_info->SetString("status", "blocked");
              break;
            case IndexedDBTransaction::STARTED:
              if (transaction->diagnostics().tasks_scheduled > 0)
                transaction_info->SetString("status", "running");
              else
                transaction_info->SetString("status", "started");
              break;
            case IndexedDBTransaction::COMMITTING:
              transaction_info->SetString("status", "committing");
              break;
            case IndexedDBTransaction::FINISHED:
              transaction_info->SetString("status", "finished");
              break;
          }

          transaction_info->SetDouble("tid", transaction->id());
          transaction_info->SetDouble(
              "age",
              (base::Time::Now() - transaction->diagnostics().creation_time)
                  .InMillisecondsF());
          transaction_info->SetDouble(
              "runtime",
              (base::Time::Now() - transaction->diagnostics().start_time)
                  .InMillisecondsF());
          transaction_info->SetDouble(
              "tasks_scheduled", transaction->diagnostics().tasks_scheduled);
          transaction_info->SetDouble(
              "tasks_completed", transaction->diagnostics().tasks_completed);

          std::unique_ptr<base::ListValue> scope(
              std::make_unique<base::ListValue>());
          for (const auto& id : transaction->scope()) {
            const auto& stores_it = db->metadata().object_stores.find(id);
            if (stores_it != db->metadata().object_stores.end())
              scope->AppendString(stores_it->second.name);
          }

          transaction_info->Set("scope", std::move(scope));
          transaction_list->Append(std::move(transaction_info));
        }
      }
      db_info->Set("transactions", std::move(transaction_list));

      database_list->Append(std::move(db_info));
    }
    info->Set("databases", std::move(database_list));
    list.Append(std::move(info));
  }

  std::move(callback).Run(is_incognito(), std::move(list));
}

void IndexedDBContextImpl::SetForceKeepSessionState() {
  force_keep_session_state_ = true;
}

void IndexedDBContextImpl::ApplyPolicyUpdates(
    std::vector<storage::mojom::IndexedDBStoragePolicyUpdatePtr>
        policy_updates) {
  for (const auto& update : policy_updates) {
    if (!update->purge_on_shutdown)
      origins_to_purge_on_shutdown_.erase(update->origin);
    else
      origins_to_purge_on_shutdown_.insert(std::move(update->origin));
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
    const Origin& origin,
    GetFilePathForTestingCallback callback) {
  std::move(callback).Run(GetLevelDBPath(origin));
}

void IndexedDBContextImpl::ResetCachesForTesting(base::OnceClosure callback) {
  origin_set_.reset();
  origin_size_map_.clear();
  std::move(callback).Run();
}

void IndexedDBContextImpl::ForceSchemaDowngradeForTesting(
    const url::Origin& origin,
    ForceSchemaDowngradeForTestingCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  if (is_incognito() || !HasOrigin(origin)) {
    std::move(callback).Run(false);
    return;
  }

  if (indexeddb_factory_.get()) {
    indexeddb_factory_->ForceSchemaDowngrade(origin);
    std::move(callback).Run(true);
    return;
  }
  ForceCloseSync(
      origin,
      storage::mojom::ForceCloseReason::FORCE_SCHEMA_DOWNGRADE_INTERNALS_PAGE);
  std::move(callback).Run(false);
}

void IndexedDBContextImpl::HasV2SchemaCorruptionForTesting(
    const url::Origin& origin,
    HasV2SchemaCorruptionForTestingCallback callback) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());

  if (is_incognito() || !HasOrigin(origin)) {
    std::move(callback).Run(
        storage::mojom::V2SchemaCorruptionStatus::CORRUPTION_UNKNOWN);
    return;
  }

  if (indexeddb_factory_.get()) {
    std::move(callback).Run(
        static_cast<storage::mojom::V2SchemaCorruptionStatus>(
            indexeddb_factory_->HasV2SchemaCorruption(origin)));
    return;
  }
  return std::move(callback).Run(
      storage::mojom::V2SchemaCorruptionStatus::CORRUPTION_UNKNOWN);
}

void IndexedDBContextImpl::WriteToIndexedDBForTesting(
    const url::Origin& origin,
    const std::string& key,
    const std::string& value,
    base::OnceClosure callback) {
  IndexedDBOriginStateHandle handle;
  leveldb::Status s;
  std::tie(handle, s, std::ignore, std::ignore, std::ignore) =
      GetIDBFactory()->GetOrOpenOriginFactory(origin, data_path(),
                                              /*create_if_missing=*/true);
  CHECK(s.ok()) << s.ToString();
  CHECK(handle.IsHeld());

  TransactionalLevelDBDatabase* db =
      handle.origin_state()->backing_store()->db();
  std::string value_copy = value;
  s = db->Put(key, &value_copy);
  CHECK(s.ok()) << s.ToString();
  handle.Release();

  GetIDBFactory()->ForceClose(origin, true);
  std::move(callback).Run();
}

void IndexedDBContextImpl::GetBlobCountForTesting(
    const Origin& origin,
    GetBlobCountForTestingCallback callback) {
  std::move(callback).Run(GetOriginBlobFileCount(origin));
}

void IndexedDBContextImpl::GetNextBlobNumberForTesting(
    const Origin& origin,
    int64_t database_id,
    GetNextBlobNumberForTestingCallback callback) {
  IndexedDBOriginStateHandle handle;
  leveldb::Status s;
  std::tie(handle, s, std::ignore, std::ignore, std::ignore) =
      GetIDBFactory()->GetOrOpenOriginFactory(origin, data_path(),
                                              /*create_if_missing=*/true);
  CHECK(s.ok()) << s.ToString();
  CHECK(handle.IsHeld());

  TransactionalLevelDBDatabase* db =
      handle.origin_state()->backing_store()->db();

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
    const url::Origin& origin,
    int64_t database_id,
    int64_t blob_number,
    GetPathForBlobForTestingCallback callback) {
  IndexedDBOriginStateHandle handle;
  leveldb::Status s;
  std::tie(handle, s, std::ignore, std::ignore, std::ignore) =
      GetIDBFactory()->GetOrOpenOriginFactory(origin, data_path(),
                                              /*create_if_missing=*/true);
  CHECK(s.ok()) << s.ToString();
  CHECK(handle.IsHeld());

  IndexedDBBackingStore* backing_store = handle.origin_state()->backing_store();
  base::FilePath path =
      backing_store->GetBlobFileName(database_id, blob_number);
  std::move(callback).Run(path);
}

void IndexedDBContextImpl::CompactBackingStoreForTesting(
    const url::Origin& origin,
    base::OnceClosure callback) {
  IndexedDBFactoryImpl* factory = GetIDBFactory();

  std::vector<IndexedDBDatabase*> databases =
      factory->GetOpenDatabasesForOrigin(origin);

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
    const Origin& origin,
    storage::mojom::ForceCloseReason reason) {
  ForceClose(origin, reason, base::DoNothing());
}

IndexedDBFactoryImpl* IndexedDBContextImpl::GetIDBFactory() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!indexeddb_factory_.get()) {
    // Prime our cache of origins with existing databases so we can
    // detect when dbs are newly created.
    GetOriginSet();
    indexeddb_factory_ = std::make_unique<IndexedDBFactoryImpl>(
        this, IndexedDBClassFactory::Get(), clock_);
  }
  return indexeddb_factory_.get();
}

base::SequencedTaskRunner* IndexedDBContextImpl::IOTaskRunner() {
  DCHECK(io_task_runner_.get());
  return io_task_runner_.get();
}

std::vector<Origin> IndexedDBContextImpl::GetAllOrigins() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  std::set<Origin>* origins_set = GetOriginSet();
  return std::vector<Origin>(origins_set->begin(), origins_set->end());
}

bool IndexedDBContextImpl::HasOrigin(const Origin& origin) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  std::set<Origin>* set = GetOriginSet();
  return set->find(origin) != set->end();
}

int IndexedDBContextImpl::GetOriginBlobFileCount(const Origin& origin) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  int count = 0;
  base::FileEnumerator file_enumerator(GetBlobStorePath(origin), true,
                                       base::FileEnumerator::FILES);
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    count++;
  }
  return count;
}

int64_t IndexedDBContextImpl::GetOriginDiskUsage(const Origin& origin) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!HasOrigin(origin))
    return 0;

  EnsureDiskUsageCacheInitialized(origin);
  return origin_size_map_[origin];
}

base::Time IndexedDBContextImpl::GetOriginLastModified(const Origin& origin) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!HasOrigin(origin))
    return base::Time();

  if (is_incognito()) {
    if (!indexeddb_factory_)
      return base::Time();
    return indexeddb_factory_->GetLastModified(origin);
  }

  base::FilePath idb_directory = GetLevelDBPath(origin);
  base::Optional<base::File::Info> info =
      filesystem_proxy_->GetFileInfo(idb_directory);
  if (!info.has_value())
    return base::Time();
  return info->last_modified;
}

size_t IndexedDBContextImpl::GetConnectionCountSync(const Origin& origin) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (!HasOrigin(origin))
    return 0;

  if (!indexeddb_factory_.get())
    return 0;

  return indexeddb_factory_->GetConnectionCount(origin);
}

std::vector<base::FilePath> IndexedDBContextImpl::GetStoragePaths(
    const Origin& origin) const {
  std::vector<base::FilePath> paths = {GetLevelDBPath(origin),
                                       GetBlobStorePath(origin)};
  return paths;
}

void IndexedDBContextImpl::FactoryOpened(const Origin& origin) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (GetOriginSet()->insert(origin).second) {
    // A newly created db, notify the quota system.
    QueryDiskAndUpdateQuotaUsage(origin);
  } else {
    EnsureDiskUsageCacheInitialized(origin);
  }
}

void IndexedDBContextImpl::ConnectionOpened(const Origin& origin,
                                            IndexedDBConnection* connection) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  quota_manager_proxy()->NotifyStorageAccessed(
      origin, blink::mojom::StorageType::kTemporary);
  if (GetOriginSet()->insert(origin).second) {
    // A newly created db, notify the quota system.
    QueryDiskAndUpdateQuotaUsage(origin);
  } else {
    EnsureDiskUsageCacheInitialized(origin);
  }
}

void IndexedDBContextImpl::ConnectionClosed(const Origin& origin,
                                            IndexedDBConnection* connection) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  quota_manager_proxy()->NotifyStorageAccessed(
      origin, blink::mojom::StorageType::kTemporary);
  if (indexeddb_factory_.get() &&
      indexeddb_factory_->GetConnectionCount(origin) == 0)
    QueryDiskAndUpdateQuotaUsage(origin);
}

void IndexedDBContextImpl::TransactionComplete(const Origin& origin) {
  DCHECK(!indexeddb_factory_.get() ||
         indexeddb_factory_->GetConnectionCount(origin) > 0);
  QueryDiskAndUpdateQuotaUsage(origin);
}

void IndexedDBContextImpl::DatabaseDeleted(const Origin& origin) {
  GetOriginSet()->insert(origin);
  QueryDiskAndUpdateQuotaUsage(origin);
}

void IndexedDBContextImpl::BlobFilesCleaned(const url::Origin& origin) {
  QueryDiskAndUpdateQuotaUsage(origin);
}

void IndexedDBContextImpl::NotifyIndexedDBListChanged(const Origin& origin) {
  for (auto& observer : observers_)
    observer->OnIndexedDBListChanged(origin);
}

void IndexedDBContextImpl::NotifyIndexedDBContentChanged(
    const Origin& origin,
    const base::string16& database_name,
    const base::string16& object_store_name) {
  for (auto& observer : observers_) {
    observer->OnIndexedDBContentChanged(origin, database_name,
                                        object_store_name);
  }
}

IndexedDBContextImpl::~IndexedDBContextImpl() {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  if (indexeddb_factory_.get())
    indexeddb_factory_->ContextDestroyed();
}

void IndexedDBContextImpl::Shutdown() {
  // Important: This function is NOT called on the IDB Task Runner. All variable
  // access must be thread-safe.
  if (is_incognito())
    return;

  // TODO(dmurph): Make this variable atomic.
  if (force_keep_session_state_)
    return;

  // Clear session-only databases.
  if (origins_to_purge_on_shutdown_.empty())
    return;

  IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<IndexedDBContextImpl> context) {
            std::vector<Origin> origins;
            std::vector<base::FilePath> file_paths;
            // This function only needs the factory, and not the context, but
            // the context is used because passing that is thread-safe.
            IndexedDBFactoryImpl* factory = context->GetIDBFactory();
            GetAllOriginsAndPaths(context->data_path_, &origins, &file_paths);
            DCHECK_EQ(origins.size(), file_paths.size());

            auto file_path = file_paths.cbegin();
            auto origin = origins.cbegin();
            for (; origin != origins.cend(); ++origin, ++file_path) {
              if (context->origins_to_purge_on_shutdown_.find(*origin) ==
                  context->origins_to_purge_on_shutdown_.end())
                continue;
              factory->ForceClose(*origin, false);
              context->filesystem_proxy_->DeletePathRecursively(*file_path);
            }
          },
          base::WrapRefCounted(this)));
}

base::FilePath IndexedDBContextImpl::GetBlobStorePath(
    const Origin& origin) const {
  DCHECK(!is_incognito());
  return data_path_.Append(indexed_db::GetBlobStoreFileName(origin));
}

base::FilePath IndexedDBContextImpl::GetLevelDBPath(
    const Origin& origin) const {
  DCHECK(!is_incognito());
  return data_path_.Append(indexed_db::GetLevelDBFileName(origin));
}

int64_t IndexedDBContextImpl::ReadUsageFromDisk(const Origin& origin) const {
  if (is_incognito()) {
    if (!indexeddb_factory_)
      return 0;
    return indexeddb_factory_->GetInMemoryDBSize(origin);
  }

  int64_t total_size = 0;
  for (const base::FilePath& path : GetStoragePaths(origin))
    total_size += filesystem_proxy_->ComputeDirectorySize(path);
  return total_size;
}

void IndexedDBContextImpl::EnsureDiskUsageCacheInitialized(
    const Origin& origin) {
  if (origin_size_map_.find(origin) == origin_size_map_.end())
    origin_size_map_[origin] = ReadUsageFromDisk(origin);
}

void IndexedDBContextImpl::QueryDiskAndUpdateQuotaUsage(const Origin& origin) {
  int64_t former_disk_usage = origin_size_map_[origin];
  int64_t current_disk_usage = ReadUsageFromDisk(origin);
  int64_t difference = current_disk_usage - former_disk_usage;
  if (difference) {
    origin_size_map_[origin] = current_disk_usage;
    quota_manager_proxy()->NotifyStorageModified(
        storage::QuotaClientType::kIndexedDatabase, origin,
        blink::mojom::StorageType::kTemporary, difference);
    NotifyIndexedDBListChanged(origin);
  }
}

std::set<Origin>* IndexedDBContextImpl::GetOriginSet() {
  if (!origin_set_) {
    std::vector<Origin> origins;
    GetAllOriginsAndPaths(data_path_, &origins, nullptr);
    origin_set_ =
        std::make_unique<std::set<Origin>>(origins.begin(), origins.end());
  }
  return origin_set_.get();
}

base::SequencedTaskRunner* IndexedDBContextImpl::IDBTaskRunner() {
  DCHECK(owning_task_runner());
  return owning_task_runner();
}

}  // namespace content
