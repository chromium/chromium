// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_factory.h"

#include <inttypes.h>
#include <stdint.h>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "base/trace_event/base_tracing.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_factory.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "content/browser/indexed_db/indexed_db_bucket_context.h"
#include "content/browser/indexed_db/indexed_db_bucket_context_handle.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_factory_client.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/browser/indexed_db/indexed_db_tombstone_sweeper.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace content {

namespace {
constexpr static const int kNumOpenTries = 2;

leveldb::Status GetDBSizeFromEnv(leveldb::Env* env,
                                 const std::string& path,
                                 int64_t* total_size_out) {
  *total_size_out = 0;
  // Root path should be /, but in MemEnv, a path name is not tailed with '/'
  DCHECK_EQ(path.back(), '/');
  const std::string path_without_slash = path.substr(0, path.length() - 1);

  // This assumes that leveldb will not put a subdirectory into the directory
  std::vector<std::string> file_names;
  leveldb::Status s = env->GetChildren(path_without_slash, &file_names);
  if (!s.ok()) {
    return s;
  }

  for (std::string& file_name : file_names) {
    file_name.insert(0, path);
    uint64_t file_size;
    s = env->GetFileSize(file_name, &file_size);
    if (!s.ok()) {
      return s;
    } else {
      *total_size_out += static_cast<int64_t>(file_size);
    }
  }
  return s;
}

IndexedDBDatabaseError CreateDefaultError() {
  return IndexedDBDatabaseError(
      blink::mojom::IDBException::kUnknownError,
      u"Internal error opening backing store for indexedDB.open.");
}

// Creates the leveldb and blob storage directories for IndexedDB.
std::tuple<base::FilePath /*leveldb_path*/,
           base::FilePath /*blob_path*/,
           leveldb::Status>
CreateDatabaseDirectories(const base::FilePath& path_base,
                          const storage::BucketLocator& bucket_locator) {
  leveldb::Status status;
  if (!base::CreateDirectory(path_base)) {
    status =
        leveldb::Status::IOError("Unable to create IndexedDB database path");
    LOG(ERROR) << status.ToString() << ": \"" << path_base.AsUTF8Unsafe()
               << "\"";
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_DIRECTORY,
                     bucket_locator);
    return {base::FilePath(), base::FilePath(), status};
  }

  base::FilePath leveldb_path =
      path_base.Append(indexed_db::GetLevelDBFileName(bucket_locator));
  base::FilePath blob_path =
      path_base.Append(indexed_db::GetBlobStoreFileName(bucket_locator));
  if (indexed_db::IsPathTooLong(leveldb_path)) {
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_ORIGIN_TOO_LONG,
                     bucket_locator);
    status = leveldb::Status::IOError("File path too long");
    return {base::FilePath(), base::FilePath(), status};
  }
  return {leveldb_path, blob_path, status};
}

std::tuple<bool, leveldb::Status> AreSchemasKnown(
    TransactionalLevelDBDatabase* db) {
  int64_t db_schema_version = 0;
  bool found = false;
  leveldb::Status s = indexed_db::GetInt(db, SchemaVersionKey::Encode(),
                                         &db_schema_version, &found);
  if (!s.ok()) {
    return {false, s};
  }
  if (!found) {
    return {true, s};
  }
  if (db_schema_version < 0) {
    return {false, leveldb::Status::Corruption(
                       "Invalid IndexedDB database schema version.")};
  }
  if (db_schema_version > indexed_db::kLatestKnownSchemaVersion) {
    return {false, s};
  }

  int64_t raw_db_data_version = 0;
  s = indexed_db::GetInt(db, DataVersionKey::Encode(), &raw_db_data_version,
                         &found);
  if (!s.ok()) {
    return {false, s};
  }
  if (!found) {
    return {true, s};
  }
  if (raw_db_data_version < 0) {
    return {false,
            leveldb::Status::Corruption("Invalid IndexedDB data version.")};
  }

  return {IndexedDBDataFormatVersion::GetCurrent().IsAtLeast(
              IndexedDBDataFormatVersion::Decode(raw_db_data_version)),
          s};
}

}  // namespace

IndexedDBFactory::IndexedDBFactory(IndexedDBContextImpl* context)
    : context_(context) {
  DCHECK(context);
}

IndexedDBFactory::~IndexedDBFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBFactory::AddReceiver(
    std::optional<storage::BucketInfo> bucket,
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote,
    mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(
      this, std::move(pending_receiver),
      ReceiverContext(bucket, std::move(client_state_checker_remote)));
}

void IndexedDBFactory::GetDatabaseInfo(GetDatabaseInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IndexedDB", "IndexedDBFactory::GetDatabaseInfo");

  const std::optional<storage::BucketInfo>& bucket =
      receivers_.current_context().bucket;

  // Return error if failed to retrieve bucket from the QuotaManager.
  if (!bucket) {
    std::move(callback).Run(
        {}, blink::mojom::IDBError::New(
                blink::mojom::IDBException::kUnknownError, u"Internal error."));
    return;
  }

  const storage::BucketLocator bucket_locator = bucket->ToBucketLocator();
  const base::FilePath data_directory = context_->GetDataPath(bucket_locator);

  IndexedDBBucketContextHandle bucket_context_handle;
  leveldb::Status s;
  IndexedDBDatabaseError error;
  std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions;
  // Note: Any data loss information here is not piped up to the renderer, and
  // will be lost.
  std::tie(bucket_context_handle, s, error, std::ignore, std::ignore) =
      GetOrCreateBucketContext(*bucket, data_directory,
                               /*create_if_missing=*/false);
  if (!bucket_context_handle.IsHeld() ||
      !bucket_context_handle.bucket_context()) {
    if (s.IsNotFound()) {
      std::move(callback).Run(std::move(names_and_versions), nullptr);
    } else {
      std::move(callback).Run(
          {}, blink::mojom::IDBError::New(error.code(), error.message()));
    }
    if (s.IsCorruption()) {
      HandleBackingStoreCorruption(bucket_locator, error);
    }
    return;
  }
  IndexedDBBucketContext* factory = bucket_context_handle.bucket_context();
  s = factory->backing_store()->GetDatabaseNamesAndVersions(
      &names_and_versions);
  if (!s.ok()) {
    error = IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError,
                                   "Internal error opening backing store for "
                                   "indexedDB.databases().");
    std::move(callback).Run(
        {}, blink::mojom::IDBError::New(error.code(), error.message()));
    if (s.IsCorruption()) {
      HandleBackingStoreCorruption(bucket_locator, error);
    }
    return;
  }

  std::move(callback).Run(std::move(names_and_versions), nullptr);
}

void IndexedDBFactory::Open(
    mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
        pending_factory_client,
    mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
        database_callbacks_remote,
    const std::u16string& name,
    int64_t version,
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        transaction_receiver,
    int64_t transaction_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IndexedDB", "IndexedDBFactory::Open");

  const std::optional<storage::BucketInfo>& bucket =
      receivers_.current_context().bucket;

  // Return error if failed to retrieve bucket from the QuotaManager.
  if (!bucket) {
    IndexedDBFactoryClient(std::move(pending_factory_client))
        .OnError(IndexedDBDatabaseError(
            blink::mojom::IDBException::kUnknownError, u"Internal error."));
    return;
  }

  // TODO(dgrogan): Don't let a non-existing database be opened (and therefore
  // created) if this origin is already over quota.

  const storage::BucketLocator bucket_locator = bucket->ToBucketLocator();
  const base::FilePath data_directory = context_->GetDataPath(bucket_locator);

  IndexedDBBucketContextHandle bucket_context_handle;
  leveldb::Status s;
  IndexedDBDatabaseError error;
  IndexedDBDataLossInfo data_loss_info;
  bool was_cold_open;
  std::tie(bucket_context_handle, s, error, data_loss_info, was_cold_open) =
      GetOrCreateBucketContext(*bucket, data_directory,
                               /*create_if_missing=*/true);
  if (!bucket_context_handle.IsHeld() ||
      !bucket_context_handle.bucket_context()) {
    IndexedDBFactoryClient(std::move(pending_factory_client)).OnError(error);
    if (s.IsCorruption()) {
      HandleBackingStoreCorruption(bucket_locator, error);
    }
    return;
  }

  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      state_checker;
  // May be null in unit tests.
  if (receivers_.current_context().client_state_checker_remote) {
    receivers_.current_context().client_state_checker_remote->MakeClone(
        state_checker.InitWithNewPipeAndPassReceiver());
  }

  bucket_context_handle->OpenDatabase(
      name, version, std::move(pending_factory_client),
      std::move(database_callbacks_remote), transaction_id,
      std::move(transaction_receiver), was_cold_open, data_loss_info,
      std::move(state_checker));
}

void IndexedDBFactory::DeleteDatabase(
    mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
        pending_factory_client,
    const std::u16string& name,
    bool force_close) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IndexedDB", "IndexedDBFactory::DeleteDatabase");

  const std::optional<storage::BucketInfo>& bucket =
      receivers_.current_context().bucket;

  // Return error if failed to retrieve bucket from the QuotaManager.
  if (!bucket) {
    IndexedDBFactoryClient(std::move(pending_factory_client))
        .OnError(IndexedDBDatabaseError(
            blink::mojom::IDBException::kUnknownError, u"Internal error."));
    return;
  }

  const storage::BucketLocator bucket_locator = bucket->ToBucketLocator();
  IndexedDBBucketContextHandle bucket_context_handle;
  leveldb::Status s;
  IndexedDBDatabaseError error;
  // Note: Any data loss information here is not piped up to the renderer, and
  // will be lost.
  std::tie(bucket_context_handle, s, error, std::ignore, std::ignore) =
      GetOrCreateBucketContext(*bucket, context_->GetDataPath(bucket_locator),
                               /*create_if_missing=*/false);
  if (!bucket_context_handle.IsHeld() ||
      !bucket_context_handle.bucket_context()) {
    if (s.IsNotFound()) {
      IndexedDBFactoryClient(std::move(pending_factory_client))
          .OnDeleteSuccess(/*version=*/0);
      return;
    }

    IndexedDBFactoryClient(std::move(pending_factory_client)).OnError(error);
    if (s.IsCorruption()) {
      HandleBackingStoreCorruption(bucket_locator, error);
    }
    return;
  }

  bucket_context_handle->DeleteDatabase(
      std::move(pending_factory_client), name, force_close,
      base::BindOnce(&IndexedDBFactory::OnDatabaseDeleted,
                     idb_context_destruction_weak_factory_.GetWeakPtr(),
                     bucket_locator));
}

void IndexedDBFactory::HandleBackingStoreFailure(
    const storage::BucketLocator& bucket_locator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // nullptr after ContextDestroyed() called, and in some unit tests.
  if (!context_) {
    return;
  }
  context_->ForceClose(
      bucket_locator.id,
      storage::mojom::ForceCloseReason::FORCE_CLOSE_BACKING_STORE_FAILURE,
      base::DoNothing());
}

void IndexedDBFactory::HandleBackingStoreCorruption(
    storage::BucketLocator bucket_locator,
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(context_);
  const base::FilePath path_base = context_->GetDataPath(bucket_locator);

  // The message may contain the database path, which may be considered
  // sensitive data, and those strings are passed to the extension, so strip it.
  std::string sanitized_message = base::UTF16ToUTF8(error.message());
  base::ReplaceSubstringsAfterOffset(&sanitized_message, 0u,
                                     path_base.AsUTF8Unsafe(), "...");
  IndexedDBBackingStore::RecordCorruptionInfo(path_base, bucket_locator,
                                              sanitized_message);
  HandleBackingStoreFailure(bucket_locator);
  // Note: DestroyLevelDB only deletes LevelDB files, leaving all others,
  //       so our corruption info file will remain.
  //       The blob directory will be deleted when the database is recreated
  //       the next time it is opened.
  const base::FilePath file_path =
      path_base.Append(indexed_db::GetLevelDBFileName(bucket_locator));
  leveldb::Status s =
      IndexedDBClassFactory::Get()->leveldb_factory().DestroyLevelDB(file_path);
  DLOG_IF(ERROR, !s.ok()) << "Unable to delete backing store: " << s.ToString();
}

void IndexedDBFactory::ForceClose(storage::BucketId bucket_id,
                                  bool will_be_deleted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = bucket_contexts_.find(bucket_id);
  if (it == bucket_contexts_.end()) {
    return;
  }

  it->second->ForceClose(/*doom=*/will_be_deleted);
}

void IndexedDBFactory::ForceSchemaDowngrade(
    const storage::BucketLocator& bucket_locator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = bucket_contexts_.find(bucket_locator.id);
  if (it == bucket_contexts_.end()) {
    return;
  }

  IndexedDBBackingStore* backing_store = it->second->backing_store();
  leveldb::Status s = backing_store->RevertSchemaToV2();
  DLOG_IF(ERROR, !s.ok()) << "Unable to force downgrade: " << s.ToString();
}

V2SchemaCorruptionStatus IndexedDBFactory::HasV2SchemaCorruption(
    const storage::BucketLocator& bucket_locator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = bucket_contexts_.find(bucket_locator.id);
  if (it == bucket_contexts_.end()) {
    return V2SchemaCorruptionStatus::kUnknown;
  }

  IndexedDBBackingStore* backing_store = it->second->backing_store();
  return backing_store->HasV2SchemaCorruption();
}

void IndexedDBFactory::ContextDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set `context_` to nullptr first to ensure no re-entry into the `context_`
  // object during shutdown. This can happen in methods like BlobFilesCleaned.
  context_ = nullptr;
  // Invalidate the weak pointers that bind `on_ready_for_destruction` (among
  // other callbacks) so that `ForceClose()` below doesn't mutate
  // `bucket_contexts_` while it's being iterated.
  idb_context_destruction_weak_factory_.InvalidateWeakPtrs();
  for (const auto& pair : bucket_contexts_) {
    pair.second->ForceClose(/*doom=*/false);
  }
  bucket_contexts_.clear();
}

void IndexedDBFactory::ReportOutstandingBlobs(
    const storage::BucketLocator& bucket_locator,
    bool blobs_outstanding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!context_) {
    return;
  }
  auto it = bucket_contexts_.find(bucket_locator.id);
  CHECK(it != bucket_contexts_.end());

  it->second->ReportOutstandingBlobs(blobs_outstanding);
}

void IndexedDBFactory::BlobFilesCleaned(
    const storage::BucketLocator& bucket_locator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // nullptr after ContextDestroyed() called, and in some unit tests.
  if (!context_) {
    return;
  }
  context_->BlobFilesCleaned(bucket_locator);
}

void IndexedDBFactory::ForEachBucketContext(
    IndexedDBBucketContext::InstanceClosure callback) {
  for_each_bucket_context_ = callback;
  for (auto& [bucket_id, bucket_context] : bucket_contexts_) {
    bucket_context->RunInstanceClosure(for_each_bucket_context_);
  }
}

int64_t IndexedDBFactory::GetInMemoryDBSize(
    const storage::BucketLocator& bucket_locator) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = bucket_contexts_.find(bucket_locator.id);
  if (it == bucket_contexts_.end()) {
    return 0;
  }
  IndexedDBBackingStore* backing_store = it->second->backing_store();
  int64_t level_db_size = 0;
  leveldb::Status s =
      GetDBSizeFromEnv(backing_store->db()->env(), "/", &level_db_size);
  if (!s.ok()) {
    LOG(ERROR) << "Failed to GetDBSizeFromEnv: " << s.ToString();
  }

  return backing_store->GetInMemoryBlobSize() + level_db_size;
}

std::vector<storage::BucketId> IndexedDBFactory::GetOpenBucketIdsForTesting()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<storage::BucketId> output;
  output.reserve(bucket_contexts_.size());
  for (const auto& pair : bucket_contexts_) {
    output.push_back(pair.first);
  }
  return output;
}

IndexedDBBucketContext* IndexedDBFactory::GetBucketContextForTesting(
    const storage::BucketId& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = bucket_contexts_.find(id);
  if (it != bucket_contexts_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void IndexedDBFactory::FillInBucketMetadata(
    storage::mojom::IdbBucketMetadataPtr info,
    base::OnceCallback<void(storage::mojom::IdbBucketMetadataPtr)> result) {
  auto it = bucket_contexts_.find(info->bucket_locator.id);
  if (it == bucket_contexts_.end()) {
    std::move(result).Run(std::move(info));
  } else {
    it->second->FillInMetadata(std::move(info), std::move(result));
  }
}

void IndexedDBFactory::CompactBackingStoreForTesting(
    const storage::BucketLocator& bucket_locator) {
  auto it = bucket_contexts_.find(bucket_locator.id);
  if (it != bucket_contexts_.end()) {
    it->second->CompactBackingStoreForTesting();  // IN-TEST
  }
}

std::tuple<IndexedDBBucketContextHandle,
           leveldb::Status,
           IndexedDBDatabaseError,
           IndexedDBDataLossInfo,
           /*is_cold_open=*/bool>
IndexedDBFactory::GetOrCreateBucketContext(const storage::BucketInfo& bucket,
                                           const base::FilePath& data_directory,
                                           bool create_if_missing) {
  TRACE_EVENT0("IndexedDB", "indexed_db::GetOrCreateBucketContext");
  // Please see docs/open_and_verify_leveldb_database.code2flow, and the
  // generated pdf (from https://code2flow.com).
  // The intended strategy here is to have this function match that flowchart,
  // where the flowchart should be seen as the 'master' logic template. Please
  // check the git history of both to make sure they are in sync.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = bucket_contexts_.find(bucket.id);
  if (it != bucket_contexts_.end()) {
    return {IndexedDBBucketContextHandle(*it->second), leveldb::Status::OK(),
            IndexedDBDatabaseError(), IndexedDBDataLossInfo(),
            /*was_cold_open=*/false};
  }
  UMA_HISTOGRAM_ENUMERATION(
      indexed_db::kBackingStoreActionUmaName,
      indexed_db::IndexedDBAction::kBackingStoreOpenAttempt);

  bool in_memory = data_directory.empty();
  base::FilePath blob_path;
  base::FilePath database_path;
  leveldb::Status s = leveldb::Status::OK();
  const storage::BucketLocator bucket_locator = bucket.ToBucketLocator();
  if (!in_memory) {
    std::tie(database_path, blob_path, s) =
        CreateDatabaseDirectories(data_directory, bucket_locator);
    if (!s.ok()) {
      return {IndexedDBBucketContextHandle(), s, CreateDefaultError(),
              IndexedDBDataLossInfo(), /*was_cold_open=*/true};
    }
  }

  auto lock_manager = std::make_unique<PartitionedLockManager>();
  IndexedDBDataLossInfo data_loss_info;
  std::unique_ptr<IndexedDBBackingStore> backing_store;
  bool disk_full = false;
  base::ElapsedTimer open_timer;
  leveldb::Status first_try_status;
  for (int i = 0; i < kNumOpenTries; ++i) {
    const bool is_first_attempt = i == 0;
    std::tie(backing_store, s, data_loss_info, disk_full) =
        OpenAndVerifyIndexedDBBackingStore(
            bucket_locator, data_directory, database_path, blob_path,
            lock_manager.get(), is_first_attempt, create_if_missing);
    if (LIKELY(is_first_attempt)) {
      first_try_status = s;
    }
    if (LIKELY(s.ok())) {
      break;
    }
    if (!create_if_missing && s.IsNotFound()) {
      return {IndexedDBBucketContextHandle(), s, IndexedDBDatabaseError(),
              data_loss_info, /*was_cold_open=*/true};
    }
    DCHECK(!backing_store);
    // If the disk is full, always exit immediately.
    if (disk_full) {
      break;
    }
    if (s.IsCorruption()) {
      std::string sanitized_message = leveldb_env::GetCorruptionMessage(s);
      base::ReplaceSubstringsAfterOffset(&sanitized_message, 0u,
                                         data_directory.AsUTF8Unsafe(), "...");
      LOG(ERROR) << "Got corruption for "
                 << bucket_locator.storage_key.GetDebugString() << ", "
                 << sanitized_message;
      IndexedDBBackingStore::RecordCorruptionInfo(
          data_directory, bucket_locator, sanitized_message);
    }
  }

  UMA_HISTOGRAM_ENUMERATION(
      "WebCore.IndexedDB.BackingStore.OpenFirstTryResult",
      leveldb_env::GetLevelDBStatusUMAValue(first_try_status),
      leveldb_env::LEVELDB_STATUS_MAX);

  if (LIKELY(first_try_status.ok())) {
    UMA_HISTOGRAM_TIMES(
        "WebCore.IndexedDB.BackingStore.OpenFirstTrySuccessTime",
        open_timer.Elapsed());
  }

  if (LIKELY(s.ok())) {
    base::UmaHistogramTimes("WebCore.IndexedDB.BackingStore.OpenSuccessTime",
                            open_timer.Elapsed());
  } else {
    base::UmaHistogramTimes("WebCore.IndexedDB.BackingStore.OpenFailureTime",
                            open_timer.Elapsed());
    if (disk_full) {
      ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_DISK_FULL,
                       bucket_locator);
      context_->quota_manager_proxy()->OnClientWriteFailed(
          bucket_locator.storage_key);
      return {IndexedDBBucketContextHandle(), s,
              IndexedDBDatabaseError(blink::mojom::IDBException::kQuotaError,
                                     u"Encountered full disk while opening "
                                     "backing store for indexedDB.open."),
              data_loss_info, /*was_cold_open=*/true};
    }
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_NO_RECOVERY,
                     bucket_locator);
    return {IndexedDBBucketContextHandle(), s, CreateDefaultError(),
            data_loss_info, /*was_cold_open=*/true};
  }
  backing_store->db()->scopes()->StartRecoveryAndCleanupTasks();

  if (!in_memory) {
    ReportOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_SUCCESS,
                     bucket_locator);
  }

  IndexedDBBucketContext::Delegate bucket_delegate;
  bucket_delegate.on_fatal_error = base::BindRepeating(
      [](const storage::BucketLocator& bucket_locator,
         base::WeakPtr<IndexedDBFactory> factory, leveldb::Status s,
         const std::string& error_message) {
        if (factory) {
          factory->OnDatabaseError(bucket_locator, s, error_message);
        }
      },
      bucket_locator, weak_factory_.GetWeakPtr());
  bucket_delegate.on_corruption = base::BindRepeating(
      [](const storage::BucketLocator& bucket_locator,
         base::WeakPtr<IndexedDBFactory> factory,
         const IndexedDBDatabaseError& error) {
        if (factory) {
          factory->HandleBackingStoreCorruption(bucket_locator, error);
        }
      },
      bucket_locator, weak_factory_.GetWeakPtr());
  bucket_delegate.on_ready_for_destruction = base::BindRepeating(
      [](const storage::BucketLocator& bucket_locator,
         base::WeakPtr<IndexedDBFactory> factory) {
        if (factory) {
          factory->bucket_contexts_.erase(bucket_locator.id);
        }
      },
      bucket_locator, idb_context_destruction_weak_factory_.GetWeakPtr());
  bucket_delegate.on_content_changed = base::BindRepeating(
      [](base::WeakPtr<IndexedDBFactory> factory,
         storage::BucketLocator bucket_locator,
         const std::u16string& database_name,
         const std::u16string& object_store_name) {
        if (factory) {
          factory->context_->NotifyIndexedDBContentChanged(
              bucket_locator, database_name, object_store_name);
        }
      },
      idb_context_destruction_weak_factory_.GetWeakPtr(), bucket_locator);
  bucket_delegate.on_writing_transaction_complete = base::BindRepeating(
      [](base::WeakPtr<IndexedDBFactory> factory,
         storage::BucketLocator bucket_locator, bool did_sync) {
        if (factory) {
          factory->context_->WritingTransactionComplete(bucket_locator,
                                                        did_sync);
        }
      },
      idb_context_destruction_weak_factory_.GetWeakPtr(), bucket_locator);
  bucket_delegate.for_each_bucket_context = base::BindRepeating(
      &IndexedDBFactory::ForEachBucketContext, weak_factory_.GetWeakPtr());

  mojo::PendingRemote<storage::mojom::BlobStorageContext> blob_storage_context;
  // May be null in unit tests.
  if (context_->blob_storage_context()) {
    context_->blob_storage_context()->Clone(
        blob_storage_context.InitWithNewPipeAndPassReceiver());
  }

  mojo::PendingRemote<storage::mojom::FileSystemAccessContext> fsa_context;
  // May be null in unit tests.
  if (context_->file_system_access_context()) {
    context_->file_system_access_context()->Clone(
        fsa_context.InitWithNewPipeAndPassReceiver());
  }

  auto bucket_context = std::make_unique<IndexedDBBucketContext>(
      bucket, std::move(lock_manager), std::move(bucket_delegate),
      std::move(backing_store), context_->quota_manager_proxy(),
      context_->IOTaskRunner(), std::move(blob_storage_context),
      std::move(fsa_context), for_each_bucket_context_);

  it = bucket_contexts_.emplace(bucket_locator.id, std::move(bucket_context))
           .first;
  context_->FactoryOpened(bucket_locator);
  return {IndexedDBBucketContextHandle(*it->second), s,
          IndexedDBDatabaseError(), data_loss_info, /*was_cold_open=*/true};
}

std::unique_ptr<IndexedDBBackingStore> IndexedDBFactory::CreateBackingStore(
    IndexedDBBackingStore::Mode backing_store_mode,
    const storage::BucketLocator& bucket_locator,
    const base::FilePath& blob_path,
    std::unique_ptr<TransactionalLevelDBDatabase> db,
    IndexedDBBackingStore::BlobFilesCleanedCallback blob_files_cleaned,
    IndexedDBBackingStore::ReportOutstandingBlobsCallback
        report_outstanding_blobs,
    scoped_refptr<base::SequencedTaskRunner> idb_task_runner) {
  return std::make_unique<IndexedDBBackingStore>(
      backing_store_mode, bucket_locator, blob_path, std::move(db),
      std::move(blob_files_cleaned), std::move(report_outstanding_blobs),
      std::move(idb_task_runner));
}
std::tuple<std::unique_ptr<IndexedDBBackingStore>,
           leveldb::Status,
           IndexedDBDataLossInfo,
           bool /* is_disk_full */>
IndexedDBFactory::OpenAndVerifyIndexedDBBackingStore(
    const storage::BucketLocator& bucket_locator,
    base::FilePath data_directory,
    base::FilePath database_path,
    base::FilePath blob_path,
    PartitionedLockManager* lock_manager,
    bool is_first_attempt,
    bool create_if_missing) {
  // Please see docs/open_and_verify_leveldb_database.code2flow, and the
  // generated pdf (from https://code2flow.com).
  // The intended strategy here is to have this function match that flowchart,
  // where the flowchart should be seen as the 'master' logic template. Please
  // check the git history of both to make sure they are in sync.
  DCHECK_EQ(database_path.empty(), data_directory.empty());
  DCHECK_EQ(blob_path.empty(), data_directory.empty());
  TRACE_EVENT0("IndexedDB", "indexed_db::OpenAndVerifyLevelDBDatabase");

  bool in_memory = data_directory.empty();
  leveldb::Status status;
  IndexedDBDataLossInfo data_loss_info;
  data_loss_info.status = blink::mojom::IDBDataLoss::None;
  if (!in_memory) {
    // Check for previous corruption, and if found then try to delete the
    // database.
    std::string corruption_message =
        indexed_db::ReadCorruptionInfo(data_directory, bucket_locator);
    if (UNLIKELY(!corruption_message.empty())) {
      LOG(ERROR) << "IndexedDB recovering from a corrupted (and deleted) "
                    "database.";
      if (is_first_attempt) {
        ReportOpenStatus(
            indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_PRIOR_CORRUPTION,
            bucket_locator);
      }
      data_loss_info.status = blink::mojom::IDBDataLoss::Total;
      data_loss_info.message = base::StrCat(
          {"IndexedDB (database was corrupt): ", corruption_message});
      // This is a special case where we want to make sure the database is
      // deleted, so we try to delete again.
      status = IndexedDBClassFactory::Get()->leveldb_factory().DestroyLevelDB(
          database_path);

      if (UNLIKELY(!status.ok())) {
        LOG(ERROR) << "Unable to delete backing store: " << status.ToString();
        return {nullptr, status, data_loss_info, /*is_disk_full=*/false};
      }
    }
  }

  // Open the leveldb database.
  scoped_refptr<LevelDBState> database_state;
  bool is_disk_full;
  {
    TRACE_EVENT0("IndexedDB", "IndexedDBFactory::OpenLevelDB");
    base::TimeTicks begin_time = base::TimeTicks::Now();
    size_t write_buffer_size = leveldb_env::WriteBufferSize(
        base::SysInfo::AmountOfTotalDiskSpace(database_path));
    std::tie(database_state, status, is_disk_full) =
        IndexedDBClassFactory::Get()->leveldb_factory().OpenLevelDBState(
            database_path, create_if_missing, write_buffer_size);
    if (UNLIKELY(!status.ok())) {
      if (!status.IsNotFound()) {
        indexed_db::ReportLevelDBError("WebCore.IndexedDB.LevelDBOpenErrors",
                                       status);
      }
      return {nullptr, status, IndexedDBDataLossInfo(), is_disk_full};
    }
    UMA_HISTOGRAM_MEDIUM_TIMES("WebCore.IndexedDB.LevelDB.OpenTime",
                               base::TimeTicks::Now() - begin_time);
  }

  // Create the LevelDBScopes wrapper.
  std::unique_ptr<LevelDBScopes> scopes;
  {
    TRACE_EVENT0("IndexedDB", "IndexedDBFactory::OpenLevelDBScopes");
    scopes = std::make_unique<LevelDBScopes>(
        ScopesPrefix::Encode(),
        /*max_write_batch_size_bytes=*/1024 * 1024, database_state,
        lock_manager,
        base::BindRepeating(
            [](const storage::BucketLocator& bucket_locator,
               base::WeakPtr<IndexedDBFactory> factory, leveldb::Status s) {
              if (!factory) {
                return;
              }
              factory->OnDatabaseError(bucket_locator, s, {});
            },
            bucket_locator, weak_factory_.GetWeakPtr()));
    status = scopes->Initialize();

    if (UNLIKELY(!status.ok())) {
      return {nullptr, status, std::move(data_loss_info),
              /*is_disk_full=*/false};
    }
  }

  // Create the TransactionalLevelDBDatabase wrapper.
  std::unique_ptr<TransactionalLevelDBDatabase> database =
      IndexedDBClassFactory::Get()
          ->transactional_leveldb_factory()
          .CreateLevelDBDatabase(std::move(database_state), std::move(scopes),
                                 context_->IDBTaskRunner(),
                                 TransactionalLevelDBDatabase::
                                     kDefaultMaxOpenIteratorsPerDatabase);

  bool are_schemas_known = false;
  std::tie(are_schemas_known, status) = AreSchemasKnown(database.get());
  if (UNLIKELY(!status.ok())) {
    LOG(ERROR) << "IndexedDB had an error checking schema, treating it as "
                  "failure to open: "
               << status.ToString();
    ReportOpenStatus(
        indexed_db::
            INDEXED_DB_BACKING_STORE_OPEN_FAILED_IO_ERROR_CHECKING_SCHEMA,
        bucket_locator);
    return {nullptr, status, std::move(data_loss_info), /*is_disk_full=*/false};
  } else if (UNLIKELY(!are_schemas_known)) {
    LOG(ERROR) << "IndexedDB backing store had unknown schema, treating it as "
                  "failure to open.";
    ReportOpenStatus(
        indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_SCHEMA,
        bucket_locator);
    return {nullptr, leveldb::Status::Corruption("Unknown IndexedDB schema"),
            std::move(data_loss_info), /*is_disk_full=*/false};
  }

  bool first_open_since_startup =
      backends_opened_since_startup_.insert(bucket_locator).second;
  IndexedDBBackingStore::Mode backing_store_mode =
      in_memory ? IndexedDBBackingStore::Mode::kInMemory
                : IndexedDBBackingStore::Mode::kOnDisk;
  std::unique_ptr<IndexedDBBackingStore> backing_store = CreateBackingStore(
      backing_store_mode, bucket_locator, blob_path, std::move(database),
      base::BindRepeating(&IndexedDBFactory::BlobFilesCleaned,
                          weak_factory_.GetWeakPtr(), bucket_locator),
      base::BindRepeating(&IndexedDBFactory::ReportOutstandingBlobs,
                          weak_factory_.GetWeakPtr(), bucket_locator),
      context_->IDBTaskRunner());
  status = backing_store->Initialize(
      /*clean_active_blob_journal=*/(!in_memory && first_open_since_startup));

  if (UNLIKELY(!status.ok())) {
    return {nullptr, status, IndexedDBDataLossInfo(), /*is_disk_full=*/false};
  }

  return {std::move(backing_store), status, std::move(data_loss_info),
          /*is_disk_full=*/false};
}

void IndexedDBFactory::OnDatabaseError(
    const storage::BucketLocator& bucket_locator,
    leveldb::Status status,
    const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!status.ok());
  if (status.IsCorruption()) {
    IndexedDBDatabaseError error(
        blink::mojom::IDBException::kUnknownError,
        base::ASCIIToUTF16(message.empty() ? status.ToString() : message));
    HandleBackingStoreCorruption(bucket_locator, error);
    return;
  }
  if (status.IsIOError()) {
    context_->quota_manager_proxy()->OnClientWriteFailed(
        bucket_locator.storage_key);
  }
  HandleBackingStoreFailure(bucket_locator);
}

void IndexedDBFactory::OnDatabaseDeleted(
    const storage::BucketLocator& bucket_locator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!context_) {
    return;
  }
  context_->DatabaseDeleted(bucket_locator);
}

IndexedDBFactory::ReceiverContext::ReceiverContext(
    std::optional<storage::BucketInfo> bucket,
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker)
    : bucket(bucket),
      client_state_checker_remote(std::move(client_state_checker)) {}

IndexedDBFactory::ReceiverContext::ReceiverContext(
    IndexedDBFactory::ReceiverContext&&) noexcept = default;
IndexedDBFactory::ReceiverContext::~ReceiverContext() = default;

}  // namespace content
