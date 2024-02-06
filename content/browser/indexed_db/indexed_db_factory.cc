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
    base::UnguessableToken client_token,
    mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (bucket) {
    GetOrCreateBucketContext(*bucket,
                             context_->GetDataPath(bucket->ToBucketLocator()))
        .AddReceiver(std::move(client_state_checker_remote), client_token,
                     std::move(pending_receiver));
  } else {
    receivers_.Add(this, std::move(pending_receiver));
  }
}

void IndexedDBFactory::GetDatabaseInfo(GetDatabaseInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IndexedDB", "IndexedDBFactory::GetDatabaseInfo");

  std::move(callback).Run(
      {}, blink::mojom::IDBError::New(blink::mojom::IDBException::kUnknownError,
                                      u"Internal error."));
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

  IndexedDBFactoryClient(std::move(pending_factory_client))
      .OnError(IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError,
                                      u"Internal error."));
}

void IndexedDBFactory::DeleteDatabase(
    mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
        pending_factory_client,
    const std::u16string& name,
    bool force_close) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IndexedDB", "IndexedDBFactory::DeleteDatabase");

  IndexedDBFactoryClient(std::move(pending_factory_client))
      .OnError(IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError,
                                      u"Internal error."));
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

IndexedDBBucketContext& IndexedDBFactory::GetOrCreateBucketContext(
    const storage::BucketInfo& bucket,
    const base::FilePath& data_directory) {
  TRACE_EVENT0("IndexedDB", "indexed_db::GetOrCreateBucketContext");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = bucket_contexts_.find(bucket.id);
  if (it != bucket_contexts_.end()) {
    return *it->second;
  }
  UMA_HISTOGRAM_ENUMERATION(
      indexed_db::kBackingStoreActionUmaName,
      indexed_db::IndexedDBAction::kBackingStoreOpenAttempt);

  const storage::BucketLocator bucket_locator = bucket.ToBucketLocator();
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
      bucket, data_directory, std::move(bucket_delegate),
      context_->quota_manager_proxy(), context_->IOTaskRunner(),
      std::move(blob_storage_context), std::move(fsa_context),
      for_each_bucket_context_);

  it = bucket_contexts_.emplace(bucket_locator.id, std::move(bucket_context))
           .first;
  return *it->second;
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

}  // namespace content
