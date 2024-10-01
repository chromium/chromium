// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/backing_store.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/dcheck_is_on.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/trace_event/base_tracing.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "components/services/storage/indexed_db/transactional_leveldb/leveldb_write_batch.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_iterator.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/active_blob_registry.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/status.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_writer_delegate.h"
#include "storage/browser/file_system/local_file_stream_writer.h"
#include "storage/common/database/database_identifier.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBKeyRange;

namespace content::indexed_db {

// An RAII helper to ensure that "DidCommitTransaction" is called
// during this class's destruction.
class AutoDidCommitTransaction {
 public:
  explicit AutoDidCommitTransaction(BackingStore* backing_store)
      : backing_store_(backing_store) {
    DCHECK(backing_store_);
  }

  AutoDidCommitTransaction(const AutoDidCommitTransaction&) = delete;
  AutoDidCommitTransaction operator=(const AutoDidCommitTransaction&) = delete;

  ~AutoDidCommitTransaction() { backing_store_->DidCommitTransaction(); }

 private:
  const raw_ptr<BackingStore> backing_store_;
};

namespace {

std::string ComputeOriginIdentifier(
    const storage::BucketLocator& bucket_locator) {
  return storage::GetIdentifierFromOrigin(bucket_locator.storage_key.origin()) +
         "@1";
}

// Returns some configuration that is shared across leveldb DB instances. The
// configuration is further tweaked in `CreateLevelDBState()`.
leveldb_env::Options GetLevelDBOptions() {
  leveldb_env::Options options;
  options.paranoid_checks = true;
  options.compression = leveldb::kSnappyCompression;
  // For info about the troubles we've run into with this parameter, see:
  // https://crbug.com/227313#c11
  options.max_open_files = 80;

  // Thread-safe: static local construction, and `LDBComparator` contains no
  // state.
  options.comparator = GetDefaultLevelDBComparator();

  // Thread-safe: static local construction, and `leveldb::Cache` implements
  // internal synchronization.
  options.block_cache = leveldb_chrome::GetSharedWebBlockCache();

  // Thread-safe: calls base histogram `FactoryGet()` methods, which are
  // thread-safe.
  options.on_get_error = base::BindRepeating(
      ReportLevelDBError, "WebCore.IndexedDB.LevelDBReadErrors");
  options.on_write_error = base::BindRepeating(
      ReportLevelDBError, "WebCore.IndexedDB.LevelDBWriteErrors");

  // Thread-safe: static local construction, and `BloomFilterPolicy` state is
  // read-only after construction.
  static const leveldb::FilterPolicy* g_filter_policy =
      leveldb::NewBloomFilterPolicy(10);
  options.filter_policy = g_filter_policy;

  // Thread-safe: static local construction, and `ChromiumEnv` implements
  // internal synchronization.
  static base::NoDestructor<leveldb_env::ChromiumEnv> g_leveldb_env{
      /*log_lock_errors=*/true};
  options.env = g_leveldb_env.get();

  return options;
}

std::tuple<scoped_refptr<LevelDBState>,
           Status,
           /* is_disk_full= */ bool>
CreateLevelDBState(const base::FilePath& file_name,
                   bool create_if_missing,
                   const std::string& in_memory_name) {
  leveldb_env::Options options = GetLevelDBOptions();
  if (file_name.empty()) {
    if (!create_if_missing) {
      return {nullptr, Status::NotFound("", ""), false};
    }

    std::unique_ptr<leveldb::Env> in_memory_env =
        leveldb_chrome::NewMemEnv(in_memory_name, options.env);
    leveldb_env::Options in_memory_options = options;
    in_memory_options.env = in_memory_env.get();
    in_memory_options.paranoid_checks = false;
    std::unique_ptr<leveldb::DB> db;
    Status status(leveldb_env::OpenDB(in_memory_options, std::string(), &db));

    if (!status.ok()) [[unlikely]] {
      LOG(ERROR) << "Failed to open in-memory LevelDB database: "
                 << status.ToString();
      return {nullptr, status, false};
    }

    return {LevelDBState::CreateForInMemoryDB(std::move(in_memory_env),
                                              options.comparator, std::move(db),
                                              "in-memory-database"),
            status, false};
  }

  options.write_buffer_size = leveldb_env::WriteBufferSize(
      base::SysInfo::AmountOfTotalDiskSpace(file_name));
  options.create_if_missing = create_if_missing;
  std::unique_ptr<leveldb::DB> db;
  Status status(leveldb_env::OpenDB(options, file_name.AsUTF8Unsafe(), &db));
  if (!status.ok()) [[unlikely]] {
    if (!create_if_missing && status.IsInvalidArgument()) {
      return {nullptr, Status::NotFound("", ""), false};
    }
    constexpr int64_t kBytesInOneKilobyte = 1024;
    int64_t free_disk_space_bytes =
        base::SysInfo::AmountOfFreeDiskSpace(file_name);
    bool below_100kb = free_disk_space_bytes != -1 &&
                       free_disk_space_bytes < 100 * kBytesInOneKilobyte;

    // Disks with <100k of free space almost never succeed in opening a
    // leveldb database.
    bool is_disk_full =
        below_100kb || leveldb_env::IndicatesDiskFull(status.leveldb_status());

    LOG(ERROR) << "Failed to open LevelDB database from "
               << file_name.AsUTF8Unsafe() << "," << status.ToString();
    return {nullptr, status, is_disk_full};
  }

  return {LevelDBState::CreateForDiskDB(options.comparator, std::move(db),
                                        std::move(file_name)),
          status, false};
}

std::tuple<bool, Status> AreSchemasKnown(TransactionalLevelDBDatabase* db) {
  int64_t db_schema_version = 0;
  bool found = false;
  Status s = GetInt(db, SchemaVersionKey::Encode(), &db_schema_version, &found);
  if (!s.ok()) {
    return {false, s};
  }
  if (!found) {
    return {true, s};
  }
  if (db_schema_version < 0) {
    return {false,
            Status::Corruption("Invalid IndexedDB database schema version.")};
  }
  if (db_schema_version > kLatestKnownSchemaVersion ||
      db_schema_version < kEarliestSupportedSchemaVersion) {
    return {false, s};
  }

  int64_t raw_db_data_version = 0;
  s = GetInt(db, DataVersionKey::Encode(), &raw_db_data_version, &found);
  if (!s.ok()) {
    return {false, s};
  }
  if (!found) {
    return {true, s};
  }
  if (raw_db_data_version < 0) {
    return {false, Status::Corruption("Invalid IndexedDB data version.")};
  }

  return {IndexedDBDataFormatVersion::GetCurrent().IsAtLeast(
              IndexedDBDataFormatVersion::Decode(raw_db_data_version)),
          s};
}

Status GetDBSizeFromEnv(leveldb::Env* env,
                        const std::string& path,
                        int64_t* total_size_out) {
  *total_size_out = 0;
  // Root path should be /, but in MemEnv, a path name is not tailed with '/'.
  DCHECK_EQ(path.back(), '/');
  const std::string path_without_slash = path.substr(0, path.length() - 1);

  // This assumes that leveldb will not put a subdirectory into the directory.
  std::vector<std::string> file_names;
  Status s(env->GetChildren(path_without_slash, &file_names));
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

// TODO(ericu): Error recovery. If we persistently can't read the
// blob journal, the safe thing to do is to clear it and leak the blobs,
// though that may be costly. Still, database/directory deletion should always
// clean things up, and we can write an fsck that will do a full correction if
// need be.

// Read and decode the specified blob journal via the supplied transaction.
// The key must be either the recovery journal key or active journal key.
template <typename TransactionType>
Status GetBlobJournal(std::string_view key,
                      TransactionType* transaction,
                      BlobJournalType* journal) {
  TRACE_EVENT0("IndexedDB", "BackingStore::GetBlobJournal");

  std::string data;
  bool found = false;
  Status s(transaction->Get(key, &data, &found));
  if (!s.ok()) {
    INTERNAL_READ_ERROR(READ_BLOB_JOURNAL);
    return s;
  }
  journal->clear();
  if (!found || data.empty()) {
    return Status::OK();
  }
  std::string_view slice(data);
  if (!DecodeBlobJournal(&slice, journal)) {
    INTERNAL_CONSISTENCY_ERROR(DECODE_BLOB_JOURNAL);
    s = InternalInconsistencyStatus();
  }
  return s;
}

template <typename TransactionType>
Status GetRecoveryBlobJournal(TransactionType* transaction,
                              BlobJournalType* journal) {
  return GetBlobJournal(RecoveryBlobJournalKey::Encode(), transaction, journal);
}

template <typename TransactionType>
Status GetActiveBlobJournal(TransactionType* transaction,
                            BlobJournalType* journal) {
  return GetBlobJournal(ActiveBlobJournalKey::Encode(), transaction, journal);
}

// Clear the specified blob journal via the supplied transaction.
// The key must be either the recovery journal key or active journal key.
template <typename TransactionType>
void ClearBlobJournal(TransactionType* transaction, const std::string& key) {
  transaction->Remove(key);
}

// Overwrite the specified blob journal via the supplied transaction.
// The key must be either the recovery journal key or active journal key.
template <typename TransactionType>
Status UpdateBlobJournal(TransactionType* transaction,
                         const std::string& key,
                         const BlobJournalType& journal) {
  std::string data;
  EncodeBlobJournal(journal, &data);
  return Status(transaction->Put(key, &data));
}

template <typename TransactionType>
Status UpdateRecoveryBlobJournal(TransactionType* transaction,
                                 const BlobJournalType& journal) {
  return UpdateBlobJournal(transaction, RecoveryBlobJournalKey::Encode(),
                           journal);
}

template <typename TransactionType>
Status UpdateActiveBlobJournal(TransactionType* transaction,
                               const BlobJournalType& journal) {
  return UpdateBlobJournal(transaction, ActiveBlobJournalKey::Encode(),
                           journal);
}

// Append blobs to the specified blob journal via the supplied transaction.
// The key must be either the recovery journal key or active journal key.
template <typename TransactionType>
Status AppendBlobsToBlobJournal(TransactionType* transaction,
                                const std::string& key,
                                const BlobJournalType& journal) {
  if (journal.empty()) {
    return Status::OK();
  }
  BlobJournalType old_journal;
  Status s = GetBlobJournal(key, transaction, &old_journal);
  if (!s.ok()) {
    return s;
  }
  old_journal.insert(old_journal.end(), journal.begin(), journal.end());
  return UpdateBlobJournal(transaction, key, old_journal);
}

template <typename TransactionType>
Status AppendBlobsToRecoveryBlobJournal(TransactionType* transaction,
                                        const BlobJournalType& journal) {
  return AppendBlobsToBlobJournal(transaction, RecoveryBlobJournalKey::Encode(),
                                  journal);
}

template <typename TransactionType>
Status AppendBlobsToActiveBlobJournal(TransactionType* transaction,
                                      const BlobJournalType& journal) {
  return AppendBlobsToBlobJournal(transaction, ActiveBlobJournalKey::Encode(),
                                  journal);
}

// Append a database to the specified blob journal via the supplied transaction.
// The key must be either the recovery journal key or active journal key.
Status MergeDatabaseIntoBlobJournal(
    TransactionalLevelDBTransaction* transaction,
    const std::string& key,
    int64_t database_id) {
  TRACE_EVENT0("IndexedDB", "BackingStore::MergeDatabaseIntoBlobJournal");

  BlobJournalType journal;
  Status s = GetBlobJournal(key, transaction, &journal);
  if (!s.ok()) {
    return s;
  }
  journal.push_back({database_id, DatabaseMetaDataKey::kAllBlobsNumber});
  UpdateBlobJournal(transaction, key, journal);
  return Status::OK();
}

Status MergeDatabaseIntoRecoveryBlobJournal(
    TransactionalLevelDBTransaction* leveldb_transaction,
    int64_t database_id) {
  return MergeDatabaseIntoBlobJournal(
      leveldb_transaction, RecoveryBlobJournalKey::Encode(), database_id);
}

Status MergeDatabaseIntoActiveBlobJournal(
    TransactionalLevelDBTransaction* leveldb_transaction,
    int64_t database_id) {
  return MergeDatabaseIntoBlobJournal(
      leveldb_transaction, ActiveBlobJournalKey::Encode(), database_id);
}

// Blob Data is encoded as a series of:
//   { object_type [IndexedDBExternalObject::ObjectType as byte],
//     (for Blobs and Files only) blob_number [int64_t as varInt],
//     (for Blobs and Files only) type [string-with-length, may be empty],
//     (for Blobs and Files only) size [int64_t as varInt]
//     (for Files only) fileName [string-with-length]
//     (for Files only) lastModified [int64_t as varInt, in microseconds]
//     (for File System Access Handles only) token [binary-with-length]
//   }
// There is no length field; just read until you run out of data.
std::string EncodeExternalObjects(
    const std::vector<IndexedDBExternalObject>& external_objects) {
  std::string ret;
  for (const auto& info : external_objects) {
    EncodeByte(static_cast<unsigned char>(info.object_type()), &ret);
    switch (info.object_type()) {
      case IndexedDBExternalObject::ObjectType::kBlob:
      case IndexedDBExternalObject::ObjectType::kFile:
        EncodeVarInt(info.blob_number(), &ret);
        EncodeStringWithLength(info.type(), &ret);
        EncodeVarInt(info.size(), &ret);
        if (info.object_type() == IndexedDBExternalObject::ObjectType::kFile) {
          EncodeStringWithLength(info.file_name(), &ret);
          EncodeVarInt(
              info.last_modified().ToDeltaSinceWindowsEpoch().InMicroseconds(),
              &ret);
        }
        break;
      case IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle:
        DCHECK(!info.serialized_file_system_access_handle().empty());
        EncodeBinary(info.serialized_file_system_access_handle(), &ret);
        break;
    }
  }
  return ret;
}

bool DecodeV3ExternalObjects(std::string_view data,
                             std::vector<IndexedDBExternalObject>* output) {
  std::vector<IndexedDBExternalObject> ret;
  output->clear();
  std::string_view slice(data);
  while (!slice.empty()) {
    bool is_file;
    int64_t blob_number;
    std::u16string type;
    int64_t size;
    std::u16string file_name;

    if (!DecodeBool(&slice, &is_file)) {
      return false;
    }
    if (!DecodeVarInt(&slice, &blob_number) ||
        !DatabaseMetaDataKey::IsValidBlobNumber(blob_number)) {
      return false;
    }
    if (!DecodeStringWithLength(&slice, &type)) {
      return false;
    }
    if (is_file) {
      if (!DecodeStringWithLength(&slice, &file_name)) {
        return false;
      }
      ret.emplace_back(blob_number, type, file_name, base::Time(),
                       IndexedDBExternalObject::kUnknownSize);
    } else {
      if (!DecodeVarInt(&slice, &size) || size < 0) {
        return false;
      }
      ret.emplace_back(type, size, blob_number);
    }
  }
  output->swap(ret);

  return true;
}

bool DecodeExternalObjects(const std::string& data,
                           std::vector<IndexedDBExternalObject>* output) {
  std::vector<IndexedDBExternalObject> ret;
  output->clear();
  std::string_view slice(data);
  while (!slice.empty()) {
    unsigned char raw_object_type;
    if (!DecodeByte(&slice, &raw_object_type) ||
        raw_object_type > static_cast<uint8_t>(
                              IndexedDBExternalObject::ObjectType::kMaxValue)) {
      return false;
    }
    IndexedDBExternalObject::ObjectType object_type =
        static_cast<IndexedDBExternalObject::ObjectType>(raw_object_type);
    switch (object_type) {
      case IndexedDBExternalObject::ObjectType::kBlob:
      case IndexedDBExternalObject::ObjectType::kFile: {
        int64_t blob_number;
        std::u16string type;
        int64_t size;
        std::u16string file_name;

        if (!DecodeVarInt(&slice, &blob_number) ||
            !DatabaseMetaDataKey::IsValidBlobNumber(blob_number)) {
          return false;
        }
        if (!DecodeStringWithLength(&slice, &type)) {
          return false;
        }
        if (!DecodeVarInt(&slice, &size) || size < 0) {
          return false;
        }
        if (object_type != IndexedDBExternalObject::ObjectType::kFile) {
          ret.emplace_back(type, size, blob_number);
          break;
        }
        if (!DecodeStringWithLength(&slice, &file_name)) {
          return false;
        }
        int64_t last_modified;
        if (!DecodeVarInt(&slice, &last_modified) || size < 0) {
          return false;
        }
        ret.emplace_back(blob_number, type, file_name,
                         base::Time::FromDeltaSinceWindowsEpoch(
                             base::Microseconds(last_modified)),
                         size);
        break;
      }
      case IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle: {
        base::span<const uint8_t> token;
        if (!DecodeBinary(&slice, &token)) {
          return false;
        }
        ret.emplace_back(std::vector<uint8_t>(token.begin(), token.end()));
        break;
      }
    }
  }
  output->swap(ret);

  return true;
}

// Returns the created iterator and status in a pair for convenience.
std::pair<std::unique_ptr<TransactionalLevelDBIterator>, Status>
CreateIteratorAndGetStatus(TransactionalLevelDBTransaction& transaction) {
  leveldb::Status status_out;
  std::unique_ptr<TransactionalLevelDBIterator> iterator =
      transaction.CreateIterator(status_out);
  return {std::move(iterator), Status(status_out)};
}

Status DeleteBlobsInRange(BackingStore::Transaction* transaction,
                          int64_t database_id,
                          const std::string& start_key,
                          const std::string& end_key,
                          bool upper_open) {
  Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it;
  std::tie(it, s) = CreateIteratorAndGetStatus(*transaction->transaction());
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(CREATE_ITERATOR);
    return s;
  }
  s = it->Seek(start_key);
  for (; s.ok() && it->IsValid() &&
         (upper_open ? CompareKeys(it->Key(), end_key) < 0
                     : CompareKeys(it->Key(), end_key) <= 0);
       s = it->Next()) {
    std::string_view key_piece(it->Key());
    std::string user_key =
        BlobEntryKey::ReencodeToObjectStoreDataKey(&key_piece);
    if (user_key.empty()) {
      INTERNAL_CONSISTENCY_ERROR(GET_IDBDATABASE_METADATA);
      return InternalInconsistencyStatus();
    }
    transaction->PutExternalObjects(database_id, user_key, nullptr);
  }
  return s;
}

Status DeleteBlobsInObjectStore(BackingStore::Transaction* transaction,
                                int64_t database_id,
                                int64_t object_store_id) {
  std::string start_key, stop_key;
  start_key =
      BlobEntryKey::EncodeMinKeyForObjectStore(database_id, object_store_id);
  stop_key =
      BlobEntryKey::EncodeStopKeyForObjectStore(database_id, object_store_id);
  return DeleteBlobsInRange(transaction, database_id, start_key, stop_key,
                            true);
}

bool ObjectStoreCursorOptions(
    TransactionalLevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& range,
    blink::mojom::IDBCursorDirection direction,
    BackingStore::Cursor::CursorOptions* cursor_options,
    Status* status) {
  cursor_options->database_id = database_id;
  cursor_options->object_store_id = object_store_id;

  bool lower_bound = range.lower().IsValid();
  bool upper_bound = range.upper().IsValid();
  cursor_options->forward =
      (direction == blink::mojom::IDBCursorDirection::NextNoDuplicate ||
       direction == blink::mojom::IDBCursorDirection::Next);
  cursor_options->unique =
      (direction == blink::mojom::IDBCursorDirection::NextNoDuplicate ||
       direction == blink::mojom::IDBCursorDirection::PrevNoDuplicate);

  if (!lower_bound) {
    cursor_options->low_key =
        ObjectStoreDataKey::Encode(database_id, object_store_id, MinIDBKey());
    cursor_options->low_open = true;  // Not included.
  } else {
    cursor_options->low_key =
        ObjectStoreDataKey::Encode(database_id, object_store_id, range.lower());
    cursor_options->low_open = range.lower_open();
  }

  if (!upper_bound) {
    cursor_options->high_key =
        ObjectStoreDataKey::Encode(database_id, object_store_id, MaxIDBKey());

    if (cursor_options->forward) {
      cursor_options->high_open = true;  // Not included.
    } else {
      // We need a key that exists.
      if (!FindGreatestKeyLessThanOrEqual(transaction, cursor_options->high_key,
                                          &cursor_options->high_key, status)) {
        return false;
      }
      cursor_options->high_open = false;
    }
  } else {
    cursor_options->high_key =
        ObjectStoreDataKey::Encode(database_id, object_store_id, range.upper());
    cursor_options->high_open = range.upper_open();

    if (!cursor_options->forward) {
      // For reverse cursors, we need a key that exists.
      std::string found_high_key;
      if (!FindGreatestKeyLessThanOrEqual(transaction, cursor_options->high_key,
                                          &found_high_key, status)) {
        return false;
      }

      // If the target key should not be included, but we end up with a smaller
      // key, we should include that.
      if (cursor_options->high_open &&
          CompareIndexKeys(found_high_key, cursor_options->high_key) < 0) {
        cursor_options->high_open = false;
      }

      cursor_options->high_key = found_high_key;
    }
  }

  return true;
}

bool IndexCursorOptions(TransactionalLevelDBTransaction* transaction,
                        int64_t database_id,
                        int64_t object_store_id,
                        int64_t index_id,
                        const IndexedDBKeyRange& range,
                        blink::mojom::IDBCursorDirection direction,
                        BackingStore::Cursor::CursorOptions* cursor_options,
                        Status* status) {
  DCHECK(transaction);
  DCHECK(cursor_options);
  TRACE_EVENT0("IndexedDB", "BackingStore::IndexCursorOptions");

  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id)) {
    return false;
  }

  cursor_options->database_id = database_id;
  cursor_options->object_store_id = object_store_id;
  cursor_options->index_id = index_id;

  bool lower_bound = range.lower().IsValid();
  bool upper_bound = range.upper().IsValid();
  cursor_options->forward =
      (direction == blink::mojom::IDBCursorDirection::NextNoDuplicate ||
       direction == blink::mojom::IDBCursorDirection::Next);
  cursor_options->unique =
      (direction == blink::mojom::IDBCursorDirection::NextNoDuplicate ||
       direction == blink::mojom::IDBCursorDirection::PrevNoDuplicate);

  if (!lower_bound) {
    cursor_options->low_key =
        IndexDataKey::EncodeMinKey(database_id, object_store_id, index_id);
    cursor_options->low_open = false;  // Included.
  } else {
    cursor_options->low_key = IndexDataKey::Encode(database_id, object_store_id,
                                                   index_id, range.lower());
    cursor_options->low_open = range.lower_open();
  }

  if (!upper_bound) {
    cursor_options->high_key =
        IndexDataKey::EncodeMaxKey(database_id, object_store_id, index_id);
    cursor_options->high_open = false;  // Included.

    if (!cursor_options->forward) {
      // We need a key that exists.
      if (!FindGreatestKeyLessThanOrEqual(transaction, cursor_options->high_key,
                                          &cursor_options->high_key, status)) {
        return false;
      }
      cursor_options->high_open = false;
    }
  } else {
    cursor_options->high_key = IndexDataKey::Encode(
        database_id, object_store_id, index_id, range.upper());
    cursor_options->high_open = range.upper_open();

    if (!cursor_options->forward) {
      // For reverse cursors, we need a key that exists.
      std::string found_high_key;
      // Seek to the *last* key in the set of non-unique keys
      if (!FindGreatestKeyLessThanOrEqual(transaction, cursor_options->high_key,
                                          &found_high_key, status)) {
        return false;
      }

      // If the target key should not be included, but we end up with a smaller
      // key, we should include that.
      if (cursor_options->high_open &&
          CompareIndexKeys(found_high_key, cursor_options->high_key) < 0) {
        cursor_options->high_open = false;
      }

      cursor_options->high_key = found_high_key;
    }
  }

  return true;
}

Status ReadIndexes(TransactionalLevelDBDatabase* db,
                   int64_t database_id,
                   int64_t object_store_id,
                   std::map<int64_t, blink::IndexedDBIndexMetadata>* indexes) {
  if (!KeyPrefix::ValidIds(database_id, object_store_id)) {
    return InvalidDBKeyStatus();
  }
  const std::string start_key =
      IndexMetaDataKey::Encode(database_id, object_store_id, 0, 0);
  const std::string stop_key =
      IndexMetaDataKey::Encode(database_id, object_store_id + 1, 0, 0);

  DCHECK(indexes->empty());

  std::unique_ptr<TransactionalLevelDBIterator> it =
      db->CreateIterator(db->DefaultReadOptions());
  Status s(it->Seek(start_key));
  while (s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0) {
    IndexMetaDataKey meta_data_key;
    {
      std::string_view slice(it->Key());
      bool ok = IndexMetaDataKey::Decode(&slice, &meta_data_key);
      DCHECK(ok);
    }
    if (meta_data_key.meta_data_type() != IndexMetaDataKey::NAME) {
      INTERNAL_CONSISTENCY_ERROR(GET_INDEXES);
      // Possible stale metadata due to http://webkit.org/b/85557 but don't fail
      // the load.
      s = it->Next();
      if (!s.ok()) {
        break;
      }
      continue;
    }

    // TODO(jsbell): Do this by direct key lookup rather than iteration, to
    // simplify.
    int64_t index_id = meta_data_key.IndexId();
    std::u16string index_name;
    {
      std::string_view slice(it->Value());
      if (!DecodeString(&slice, &index_name) || !slice.empty()) {
        INTERNAL_CONSISTENCY_ERROR(GET_INDEXES);
      }
    }

    s = it->Next();  // unique flag
    if (!s.ok()) {
      break;
    }
    if (!CheckIndexAndMetaDataKey(it.get(), stop_key, index_id,
                                  IndexMetaDataKey::UNIQUE)) {
      INTERNAL_CONSISTENCY_ERROR(GET_INDEXES);
      break;
    }
    bool index_unique;
    {
      std::string_view slice(it->Value());
      if (!DecodeBool(&slice, &index_unique) || !slice.empty()) {
        INTERNAL_CONSISTENCY_ERROR(GET_INDEXES);
      }
    }

    s = it->Next();  // key_path
    if (!s.ok()) {
      break;
    }
    if (!CheckIndexAndMetaDataKey(it.get(), stop_key, index_id,
                                  IndexMetaDataKey::KEY_PATH)) {
      INTERNAL_CONSISTENCY_ERROR(GET_INDEXES);
      break;
    }
    blink::IndexedDBKeyPath key_path;
    {
      std::string_view slice(it->Value());
      if (!DecodeIDBKeyPath(&slice, &key_path) || !slice.empty()) {
        INTERNAL_CONSISTENCY_ERROR(GET_INDEXES);
      }
    }

    s = it->Next();  // [optional] multi_entry flag
    if (!s.ok()) {
      break;
    }
    bool index_multi_entry = false;
    if (CheckIndexAndMetaDataKey(it.get(), stop_key, index_id,
                                 IndexMetaDataKey::MULTI_ENTRY)) {
      std::string_view slice(it->Value());
      if (!DecodeBool(&slice, &index_multi_entry) || !slice.empty()) {
        INTERNAL_CONSISTENCY_ERROR(GET_INDEXES);
      }

      s = it->Next();
      if (!s.ok()) {
        break;
      }
    }

    (*indexes)[index_id] = blink::IndexedDBIndexMetadata(
        index_name, index_id, key_path, index_unique, index_multi_entry);
  }

  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_INDEXES);
  }

  return s;
}

Status ReadObjectStores(
    TransactionalLevelDBDatabase* db,
    int64_t database_id,
    std::map<int64_t, blink::IndexedDBObjectStoreMetadata>* object_stores) {
  if (!KeyPrefix::IsValidDatabaseId(database_id)) {
    return InvalidDBKeyStatus();
  }
  const std::string start_key =
      ObjectStoreMetaDataKey::Encode(database_id, 1, 0);
  const std::string stop_key =
      ObjectStoreMetaDataKey::EncodeMaxKey(database_id);

  DCHECK(object_stores->empty());

  std::unique_ptr<TransactionalLevelDBIterator> it =
      db->CreateIterator(db->DefaultReadOptions());
  Status s(it->Seek(start_key));
  while (s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0) {
    ObjectStoreMetaDataKey meta_data_key;
    {
      std::string_view slice(it->Key());
      bool ok = ObjectStoreMetaDataKey::Decode(&slice, &meta_data_key) &&
                slice.empty();
      DCHECK(ok);
      if (!ok || meta_data_key.MetaDataType() != ObjectStoreMetaDataKey::NAME) {
        INTERNAL_CONSISTENCY_ERROR(GET_OBJECT_STORES);
        // Possible stale metadata, but don't fail the load.
        s = it->Next();
        if (!s.ok()) {
          break;
        }
        continue;
      }
    }

    int64_t object_store_id = meta_data_key.ObjectStoreId();

    // TODO(jsbell): Do this by direct key lookup rather than iteration, to
    // simplify.
    std::u16string object_store_name;
    {
      std::string_view slice(it->Value());
      if (!DecodeString(&slice, &object_store_name) || !slice.empty()) {
        INTERNAL_CONSISTENCY_ERROR(GET_OBJECT_STORES);
      }
    }

    s = it->Next();
    if (!s.ok()) {
      break;
    }
    if (!CheckObjectStoreAndMetaDataType(it.get(), stop_key, object_store_id,
                                         ObjectStoreMetaDataKey::KEY_PATH)) {
      INTERNAL_CONSISTENCY_ERROR(GET_OBJECT_STORES);
      break;
    }
    blink::IndexedDBKeyPath key_path;
    {
      std::string_view slice(it->Value());
      if (!DecodeIDBKeyPath(&slice, &key_path) || !slice.empty()) {
        INTERNAL_CONSISTENCY_ERROR(GET_OBJECT_STORES);
      }
    }

    s = it->Next();
    if (!s.ok()) {
      break;
    }
    if (!CheckObjectStoreAndMetaDataType(
            it.get(), stop_key, object_store_id,
            ObjectStoreMetaDataKey::AUTO_INCREMENT)) {
      INTERNAL_CONSISTENCY_ERROR(GET_OBJECT_STORES);
      break;
    }
    bool auto_increment;
    {
      std::string_view slice(it->Value());
      if (!DecodeBool(&slice, &auto_increment) || !slice.empty()) {
        INTERNAL_CONSISTENCY_ERROR(GET_OBJECT_STORES);
      }
    }

    s = it->Next();  // Is evictable.
    if (!s.ok()) {
      break;
    }
    if (!CheckObjectStoreAndMetaDataType(it.get(), stop_key, object_store_id,
                                         ObjectStoreMetaDataKey::EVICTABLE)) {
      INTERNAL_CONSISTENCY_ERROR(GET_OBJECT_STORES);
      break;
    }

    s = it->Next();  // Last version.
    if (!s.ok()) {
      break;
    }
    if (!CheckObjectStoreAndMetaDataType(
            it.get(), stop_key, object_store_id,
            ObjectStoreMetaDataKey::LAST_VERSION)) {
      INTERNAL_CONSISTENCY_ERROR(GET_OBJECT_STORES);
      break;
    }

    s = it->Next();  // Maximum index id allocated.
    if (!s.ok()) {
      break;
    }
    if (!CheckObjectStoreAndMetaDataType(
            it.get(), stop_key, object_store_id,
            ObjectStoreMetaDataKey::MAX_INDEX_ID)) {
      INTERNAL_CONSISTENCY_ERROR(GET_OBJECT_STORES);
      break;
    }
    int64_t max_index_id;
    {
      std::string_view slice(it->Value());
      if (!DecodeInt(&slice, &max_index_id) || !slice.empty()) {
        INTERNAL_CONSISTENCY_ERROR(GET_OBJECT_STORES);
      }
    }

    s = it->Next();  // [optional] has key path (is not null)
    if (!s.ok()) {
      break;
    }
    if (CheckObjectStoreAndMetaDataType(it.get(), stop_key, object_store_id,
                                        ObjectStoreMetaDataKey::HAS_KEY_PATH)) {
      bool has_key_path;
      {
        std::string_view slice(it->Value());
        if (!DecodeBool(&slice, &has_key_path)) {
          INTERNAL_CONSISTENCY_ERROR(GET_OBJECT_STORES);
        }
      }
      // This check accounts for two layers of legacy coding:
      // (1) Initially, has_key_path was added to distinguish null vs. string.
      // (2) Later, null vs. string vs. array was stored in the key_path itself.
      // So this check is only relevant for string-type key_paths.
      if (!has_key_path &&
          (key_path.type() == blink::mojom::IDBKeyPathType::String &&
           !key_path.string().empty())) {
        INTERNAL_CONSISTENCY_ERROR(GET_OBJECT_STORES);
        break;
      }
      if (!has_key_path) {
        key_path = blink::IndexedDBKeyPath();
      }
      s = it->Next();
      if (!s.ok()) {
        break;
      }
    }

    int64_t key_generator_current_number = -1;
    if (CheckObjectStoreAndMetaDataType(
            it.get(), stop_key, object_store_id,
            ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER)) {
      std::string_view slice(it->Value());
      if (!DecodeInt(&slice, &key_generator_current_number) || !slice.empty()) {
        INTERNAL_CONSISTENCY_ERROR(GET_OBJECT_STORES);
      }

      // TODO(jsbell): Return key_generator_current_number, cache in
      // object store, and write lazily to backing store.  For now,
      // just assert that if it was written it was valid.
      DCHECK_GE(key_generator_current_number,
                ObjectStoreMetaDataKey::kKeyGeneratorInitialNumber);
      s = it->Next();
      if (!s.ok()) {
        break;
      }
    }

    blink::IndexedDBObjectStoreMetadata metadata(object_store_name,
                                                 object_store_id, key_path,
                                                 auto_increment, max_index_id);
    s = ReadIndexes(db, database_id, object_store_id, &metadata.indexes);
    if (!s.ok()) {
      break;
    }
    (*object_stores)[object_store_id] = metadata;
  }

  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_OBJECT_STORES);
  }

  return s;
}

Status FindDatabaseId(TransactionalLevelDBDatabase* db,
                      const std::string& origin_identifier,
                      const std::u16string& name,
                      int64_t* id,
                      bool* found) {
  const std::string key = DatabaseNameKey::Encode(origin_identifier, name);

  Status s = GetInt(db, key, id, found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_IDBDATABASE_METADATA);
  }

  return s;
}

}  // namespace

BackingStore::BackingStore(
    Mode backing_store_mode,
    const storage::BucketLocator& bucket_locator,
    const base::FilePath& blob_path,
    TransactionalLevelDBFactory& transactional_leveldb_factory,
    std::unique_ptr<TransactionalLevelDBDatabase> db,
    BlobFilesCleanedCallback blob_files_cleaned,
    ReportOutstandingBlobsCallback report_outstanding_blobs,
    scoped_refptr<base::SequencedTaskRunner> idb_task_runner)
    : backing_store_mode_(backing_store_mode),
      bucket_locator_(bucket_locator),
      blob_path_(backing_store_mode == Mode::kInMemory ? base::FilePath()
                                                       : blob_path),
      origin_identifier_(ComputeOriginIdentifier(bucket_locator)),
      idb_task_runner_(std::move(idb_task_runner)),
      transactional_leveldb_factory_(transactional_leveldb_factory),
      db_(std::move(db)),
      blob_files_cleaned_(std::move(blob_files_cleaned)) {
  DCHECK(idb_task_runner_->RunsTasksInCurrentSequence());
  active_blob_registry_ = std::make_unique<ActiveBlobRegistry>(
      std::move(report_outstanding_blobs),
      base::BindRepeating(&BackingStore::ReportBlobUnused,
                          weak_factory_.GetWeakPtr()));
}

BackingStore::~BackingStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

BackingStore::RecordIdentifier::RecordIdentifier(std::string primary_key,
                                                 int64_t version)
    : primary_key_(std::move(primary_key)), version_(version) {
  DCHECK(!primary_key_.empty());
}

BackingStore::RecordIdentifier::RecordIdentifier() = default;

BackingStore::RecordIdentifier::~RecordIdentifier() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BackingStore::RecordIdentifier::Reset(std::string primary_key,
                                           int64_t version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  primary_key_ = std::move(primary_key);
  version_ = version;
}

constexpr const int BackingStore::kMaxJournalCleanRequests;
constexpr const base::TimeDelta BackingStore::kMaxJournalCleaningWindowTime;
constexpr const base::TimeDelta BackingStore::kInitialJournalCleaningWindowTime;

Status BackingStore::Initialize(bool clean_active_journal) {
#if DCHECK_IS_ON()
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialized_);
#endif

  const IndexedDBDataFormatVersion latest_known_data_version =
      IndexedDBDataFormatVersion::GetCurrent();
  const std::string schema_version_key = SchemaVersionKey::Encode();
  const std::string data_version_key = DataVersionKey::Encode();

  std::unique_ptr<LevelDBWriteBatch> write_batch = LevelDBWriteBatch::Create();

  // This must have a default value to handle the case where
  // a not-found entry is reported.
  int64_t db_schema_version = 0;
  IndexedDBDataFormatVersion db_data_version;
  bool found = false;
  Status s = GetInt(db_.get(), schema_version_key, &db_schema_version, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(SET_UP_METADATA);
    return s;
  }
  if (!found) {
    // Initialize new backing store.
    db_schema_version = kLatestKnownSchemaVersion;
    std::ignore =
        PutInt(write_batch.get(), schema_version_key, db_schema_version);
    db_data_version = latest_known_data_version;
    std::ignore =
        PutInt(write_batch.get(), data_version_key, db_data_version.Encode());
    // If a blob directory already exists for this database, blow it away.  It's
    // leftover from a partially-purged previous generation of data.
    if (!in_memory() && !base::DeletePathRecursively(blob_path_)) {
      INTERNAL_WRITE_ERROR(SET_UP_METADATA);
      return IOErrorStatus();
    }
  } else {
    if (db_schema_version > kLatestKnownSchemaVersion ||
        db_schema_version < kEarliestSupportedSchemaVersion) {
      return InternalInconsistencyStatus();
    }

    // Upgrade old backing store.
    if (s.ok() && db_schema_version < 4) {
      s = MigrateToV4(write_batch.get());
    }
    if (s.ok() && db_schema_version < 5) {
      s = MigrateToV5(write_batch.get());
    }
    db_schema_version = kLatestKnownSchemaVersion;
  }

  if (!s.ok()) {
    INTERNAL_READ_ERROR(SET_UP_METADATA);
    return s;
  }

  // All new values will be written using this serialization version.
  found = false;
  if (db_data_version.blink_version() == 0 &&
      db_data_version.v8_version() == 0) {
    // We didn't read `db_data_version` yet.
    int64_t raw_db_data_version = 0;
    s = GetInt(db_.get(), data_version_key, &raw_db_data_version, &found);
    if (!s.ok()) {
      INTERNAL_READ_ERROR(SET_UP_METADATA);
      return s;
    }
    if (!found) {
      INTERNAL_CONSISTENCY_ERROR(SET_UP_METADATA);
      return InternalInconsistencyStatus();
    }
    db_data_version = IndexedDBDataFormatVersion::Decode(raw_db_data_version);
  }
  if (latest_known_data_version == db_data_version) {
    // Up to date. Nothing to do.
  } else if (latest_known_data_version.IsAtLeast(db_data_version)) {
    db_data_version = latest_known_data_version;
    std::ignore =
        PutInt(write_batch.get(), data_version_key, db_data_version.Encode());
  } else {
    // `db_data_version` is in the future according to at least one component.
    INTERNAL_CONSISTENCY_ERROR(SET_UP_METADATA);
    return InternalInconsistencyStatus();
  }

  DCHECK_EQ(db_schema_version, kLatestKnownSchemaVersion);
  DCHECK(db_data_version == latest_known_data_version);

  s = db_->Write(write_batch.get());
  write_batch.reset();
  if (!s.ok()) {
    ReportOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_FAILED_METADATA_SETUP,
                     bucket_locator_);
    INTERNAL_WRITE_ERROR(SET_UP_METADATA);
    return s;
  }

  if (clean_active_journal) {
    s = CleanUpBlobJournal(ActiveBlobJournalKey::Encode());
    if (!s.ok()) {
      ReportOpenStatus(
          INDEXED_DB_BACKING_STORE_OPEN_FAILED_CLEANUP_JOURNAL_ERROR,
          bucket_locator_);
    }
  }
#if DCHECK_IS_ON()
  initialized_ = true;
#endif
  return s;
}

void BackingStore::TearDown(base::WaitableEvent* signal_on_destruction) {
  db()->leveldb_state()->RequestDestruction(signal_on_destruction);
}

Status BackingStore::AnyDatabaseContainsBlobs(bool* blobs_exist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::u16string> names;
  Status status = GetDatabaseNames(&names);
  if (!status.ok()) {
    return status;
  }

  *blobs_exist = false;
  for (const auto& name : names) {
    IndexedDBDatabaseMetadata metadata;
    bool found = false;
    status = ReadMetadataForDatabaseName(name, &metadata, &found);
    if (!found) {
      return Status::NotFound("Metadata not found for \"%s\".",
                              base::UTF16ToUTF8(name));
    }
    for (const auto& store_id_metadata_pair : metadata.object_stores) {
      leveldb::ReadOptions options;
      // Since this is a scan, don't fill up the cache, as it's not likely these
      // blocks will be reloaded.
      options.fill_cache = false;
      options.verify_checksums = true;
      std::unique_ptr<TransactionalLevelDBIterator> iterator =
          db_->CreateIterator(options);
      std::string min_key = BlobEntryKey::EncodeMinKeyForObjectStore(
          metadata.id, store_id_metadata_pair.first);
      std::string max_key = BlobEntryKey::EncodeStopKeyForObjectStore(
          metadata.id, store_id_metadata_pair.first);
      status = iterator->Seek(std::string_view(min_key));
      if (status.IsNotFound()) {
        status = Status::OK();
        continue;
      }
      if (!status.ok()) {
        return status;
      }
      if (iterator->IsValid() &&
          db_->leveldb_state()->comparator()->Compare(
              leveldb_env::MakeSlice(iterator->Key()), max_key) < 0) {
        *blobs_exist = true;
        return Status::OK();
      }
    }

    if (!status.ok()) {
      return status;
    }
  }
  return Status::OK();
}

Status BackingStore::UpgradeBlobEntriesToV4(
    LevelDBWriteBatch* write_batch,
    std::vector<base::FilePath>* empty_blobs_to_delete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::u16string> names;
  Status status = GetDatabaseNames(&names);
  if (!status.ok()) {
    return status;
  }

  for (const auto& name : names) {
    IndexedDBDatabaseMetadata metadata;
    bool found = false;
    status = ReadMetadataForDatabaseName(name, &metadata, &found);
    if (!found) {
      return Status::NotFound("Metadata not found for \"%s\".",
                              base::UTF16ToUTF8(name));
    }
    for (const auto& store_id_metadata_pair : metadata.object_stores) {
      leveldb::ReadOptions options;
      // Since this is a scan, don't fill up the cache, as it's not likely these
      // blocks will be reloaded.
      options.fill_cache = false;
      options.verify_checksums = true;
      std::unique_ptr<TransactionalLevelDBIterator> iterator =
          db_->CreateIterator(options);
      std::string min_key = BlobEntryKey::EncodeMinKeyForObjectStore(
          metadata.id, store_id_metadata_pair.first);
      std::string max_key = BlobEntryKey::EncodeStopKeyForObjectStore(
          metadata.id, store_id_metadata_pair.first);
      status = iterator->Seek(std::string_view(min_key));
      if (status.IsNotFound()) {
        status = Status::OK();
        continue;
      }
      if (!status.ok()) {
        return status;
      }
      // Loop through all blob entries in for the given object store.
      for (; status.ok() && iterator->IsValid() &&
             db_->leveldb_state()->comparator()->Compare(
                 leveldb_env::MakeSlice(iterator->Key()), max_key) < 0;
           status = iterator->Next()) {
        std::vector<IndexedDBExternalObject> temp_external_objects;
        DecodeV3ExternalObjects(iterator->Value(), &temp_external_objects);
        bool needs_rewrite = false;
        // Read the old entries & modify them to add the missing data.
        for (auto& object : temp_external_objects) {
          if (object.object_type() !=
              IndexedDBExternalObject::ObjectType::kFile) {
            continue;
          }
          needs_rewrite = true;
          base::FilePath path =
              GetBlobFileName(metadata.id, object.blob_number());

          base::File::Info info;
          if (!base::GetFileInfo(path, &info)) {
            return Status::Corruption(
                "Unable to upgrade to database version 4.", "");
          }
          object.set_size(info.size);
          object.set_last_modified(info.last_modified);
          if (info.size == 0) {
            empty_blobs_to_delete->push_back(path);
          }
        }
        if (!needs_rewrite) {
          continue;
        }
        std::string data = EncodeExternalObjects(temp_external_objects);
        write_batch->Put(iterator->Key(), data);
        if (!status.ok()) {
          return status;
        }
      }
      if (status.IsNotFound()) {
        status = Status::OK();
      }
      if (!status.ok()) {
        return status;
      }
    }

    if (!status.ok()) {
      return status;
    }
  }
  return Status::OK();
}

Status BackingStore::ValidateBlobFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::u16string> names;
  Status status = GetDatabaseNames(&names);
  if (!status.ok()) {
    return status;
  }

  for (const auto& name : names) {
    IndexedDBDatabaseMetadata metadata;
    bool found = false;
    status = ReadMetadataForDatabaseName(name, &metadata, &found);
    if (!found) {
      return Status::NotFound("Metadata not found for \"%s\".",
                              base::UTF16ToUTF8(name));
    }
    for (const auto& store_id_metadata_pair : metadata.object_stores) {
      leveldb::ReadOptions options;
      // Since this is a scan, don't fill up the cache, as it's not likely these
      // blocks will be reloaded.
      options.fill_cache = false;
      options.verify_checksums = true;
      std::unique_ptr<TransactionalLevelDBIterator> iterator =
          db_->CreateIterator(options);
      std::string min_key = BlobEntryKey::EncodeMinKeyForObjectStore(
          metadata.id, store_id_metadata_pair.first);
      std::string max_key = BlobEntryKey::EncodeStopKeyForObjectStore(
          metadata.id, store_id_metadata_pair.first);
      status = iterator->Seek(std::string_view(min_key));
      if (status.IsNotFound()) {
        status = Status::OK();
        continue;
      }
      if (!status.ok()) {
        return status;
      }
      // Loop through all blob entries in for the given object store.
      for (; status.ok() && iterator->IsValid() &&
             db_->leveldb_state()->comparator()->Compare(
                 leveldb_env::MakeSlice(iterator->Key()), max_key) < 0;
           status = iterator->Next()) {
        std::vector<IndexedDBExternalObject> temp_external_objects;
        DecodeExternalObjects(std::string(iterator->Value()),
                              &temp_external_objects);
        for (auto& object : temp_external_objects) {
          if (object.object_type() !=
              IndexedDBExternalObject::ObjectType::kFile) {
            continue;
          }
          // Empty blobs are not written to disk.
          if (!object.size()) {
            continue;
          }

          base::FilePath path =
              GetBlobFileName(metadata.id, object.blob_number());
          base::File::Info info;
          if (!base::GetFileInfo(path, &info)) {
            return Status::Corruption(
                "Unable to upgrade to database version 5.", "");
          }
        }
      }
      if (status.IsNotFound()) {
        status = Status::OK();
      }
      if (!status.ok()) {
        return status;
      }
    }

    if (!status.ok()) {
      return status;
    }
  }
  return Status::OK();
}

std::unique_ptr<BackingStore::Transaction> BackingStore::CreateTransaction(
    blink::mojom::IDBTransactionDurability durability,
    blink::mojom::IDBTransactionMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<BackingStore::Transaction>(weak_factory_.GetWeakPtr(),
                                                     durability, mode);
}

// static
bool BackingStore::ShouldSyncOnCommit(
    blink::mojom::IDBTransactionDurability durability) {
  switch (durability) {
    case blink::mojom::IDBTransactionDurability::Default:
      NOTREACHED();
    case blink::mojom::IDBTransactionDurability::Strict:
      return true;
    case blink::mojom::IDBTransactionDurability::Relaxed:
      return false;
  }
}

// static
std::tuple<std::unique_ptr<BackingStore>,
           Status,
           IndexedDBDataLossInfo,
           bool /* is_disk_full */>
BackingStore::OpenAndVerify(BucketContext& bucket_context,
                            base::FilePath data_directory,
                            base::FilePath database_path,
                            base::FilePath blob_path,
                            PartitionedLockManager* lock_manager,
                            bool is_first_attempt,
                            bool create_if_missing) {
  CHECK_EQ(database_path.empty(), data_directory.empty());
  CHECK_EQ(blob_path.empty(), data_directory.empty());
  TRACE_EVENT0("IndexedDB", "BackingStore::OpenAndVerify");

  const storage::BucketLocator& bucket_locator =
      bucket_context.bucket_locator();

  bool in_memory = data_directory.empty();
  Status status;
  IndexedDBDataLossInfo data_loss_info;
  if (!in_memory) {
    // Check for previous corruption, and if found then try to delete the
    // database.
    std::string corruption_message =
        ReadCorruptionInfo(data_directory, bucket_locator);
    if (!corruption_message.empty()) [[unlikely]] {
      LOG(ERROR) << "IndexedDB recovering from a corrupted (and deleted) "
                    "database.";
      if (is_first_attempt) {
        ReportOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_FAILED_PRIOR_CORRUPTION,
                         bucket_locator);
      }
      data_loss_info.status = blink::mojom::IDBDataLoss::Total;
      data_loss_info.message = base::StrCat(
          {"IndexedDB (database was corrupt): ", corruption_message});
      // This is a special case where we want to make sure the database is
      // deleted, so we try to delete again.
      status = DestroyDatabase(database_path);
      if (!status.ok()) [[unlikely]] {
        LOG(ERROR) << "Unable to delete backing store: " << status.ToString();
        return {nullptr, status, data_loss_info, /*is_disk_full=*/false};
      }
    }
  }

  // Open the leveldb database.
  scoped_refptr<LevelDBState> database_state;
  {
    TRACE_EVENT0("IndexedDB", "BackingStore::OpenAndVerify.OpenLevelDB");
    base::TimeTicks begin_time = base::TimeTicks::Now();
    bool is_disk_full = false;
    std::tie(database_state, status, is_disk_full) = CreateLevelDBState(
        database_path, create_if_missing,
        base::StringPrintf("indexedDB-bucket-%" PRId64,
                           bucket_context.bucket_info().id.GetUnsafeValue()));
    if (!status.ok()) [[unlikely]] {
      if (!status.IsNotFound()) {
        ReportLevelDBError("WebCore.IndexedDB.LevelDBOpenErrors",
                           status.leveldb_status());
      }
      return {nullptr, status, IndexedDBDataLossInfo(), is_disk_full};
    }
    UMA_HISTOGRAM_MEDIUM_TIMES("WebCore.IndexedDB.LevelDB.OpenTime",
                               base::TimeTicks::Now() - begin_time);
  }

  // Create the LevelDBScopes wrapper.
  std::unique_ptr<LevelDBScopes> scopes;
  {
    TRACE_EVENT0("IndexedDB", "BackingStore::OpenAndVerify.LevelDBScopes");
    scopes = std::make_unique<LevelDBScopes>(
        ScopesPrefix::Encode(),
        /*max_write_batch_size_bytes=*/1024 * 1024, database_state,
        lock_manager,
        base::BindRepeating(
            [](base::RepeatingCallback<void(Status, const std::string&)>
                   on_fatal_error,
               leveldb::Status s) { on_fatal_error.Run(Status(s), {}); },
            base::BindRepeating(&BucketContext::OnDatabaseError,
                                bucket_context.AsWeakPtr())));
    status = scopes->Initialize();
    if (!status.ok()) [[unlikely]] {
      return {nullptr, status, std::move(data_loss_info),
              /*is_disk_full=*/false};
    }
  }

  // Create the TransactionalLevelDBDatabase wrapper.
  std::unique_ptr<TransactionalLevelDBDatabase> database =
      bucket_context.transactional_leveldb_factory()->CreateLevelDBDatabase(
          std::move(database_state), std::move(scopes),
          base::SequencedTaskRunner::GetCurrentDefault(),
          TransactionalLevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase);

  bool are_schemas_known = false;
  std::tie(are_schemas_known, status) = AreSchemasKnown(database.get());
  if (!status.ok()) [[unlikely]] {
    LOG(ERROR) << "IndexedDB had an error checking schema, treating it as "
                  "failure to open: "
               << status.ToString();
    ReportOpenStatus(
        INDEXED_DB_BACKING_STORE_OPEN_FAILED_IO_ERROR_CHECKING_SCHEMA,
        bucket_locator);
    return {nullptr, status, std::move(data_loss_info), /*is_disk_full=*/false};
  }
  if (!are_schemas_known) [[unlikely]] {
    LOG(ERROR) << "IndexedDB backing store had unknown schema, treating it as "
                  "failure to open.";
    ReportOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_SCHEMA,
                     bucket_locator);
    return {nullptr, Status::Corruption("Unknown IndexedDB schema"),
            std::move(data_loss_info), /*is_disk_full=*/false};
  }

  Mode backing_store_mode = in_memory ? Mode::kInMemory : Mode::kOnDisk;
  auto backing_store = std::make_unique<BackingStore>(
      backing_store_mode, bucket_locator, blob_path,
      *bucket_context.transactional_leveldb_factory(), std::move(database),
      base::BindRepeating(bucket_context.delegate().on_files_written,
                          /*flushed=*/true),
      base::BindRepeating(&BucketContext::ReportOutstandingBlobs,
                          bucket_context.AsWeakPtr()),
      base::SequencedTaskRunner::GetCurrentDefault());
  status = backing_store->Initialize(/*clean_active_blob_journal=*/!in_memory);
  if (!status.ok()) [[unlikely]] {
    return {nullptr, status, IndexedDBDataLossInfo(), /*is_disk_full=*/false};
  }

  return {std::move(backing_store), status, std::move(data_loss_info),
          /*is_disk_full=*/false};
}

// static
Status BackingStore::DestroyDatabase(const base::FilePath file_path) {
  return Status(
      leveldb::DestroyDB(file_path.AsUTF8Unsafe(), GetLevelDBOptions()));
}

Status BackingStore::GetCompleteMetadata(
    std::vector<IndexedDBDatabaseMetadata>* output) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif

  std::vector<std::u16string> names;
  Status status = GetDatabaseNames(&names);
  if (!status.ok()) {
    return status;
  }

  output->reserve(names.size());
  for (auto& name : names) {
    output->emplace_back();
    bool found = false;
    status = ReadMetadataForDatabaseName(name, &output->back(), &found);
    output->back().name = std::move(name);
    if (!found) {
      return Status::NotFound("Metadata not found for \"%s\".",
                              base::UTF16ToUTF8(output->back().name));
    }
    if (!status.ok()) {
      return status;
    }
  }

  return status;
}

// static
bool BackingStore::RecordCorruptionInfo(
    const base::FilePath& path_base,
    const storage::BucketLocator& bucket_locator,
    const std::string& message) {
  const base::FilePath info_path =
      path_base.Append(ComputeCorruptionFileName(bucket_locator));
  if (IsPathTooLong(info_path)) {
    return false;
  }

  base::Value::Dict root_dict;
  root_dict.Set("message", message);
  std::string output_js;

  base::JSONWriter::Write(root_dict, &output_js);
  return base::ImportantFileWriter::WriteFileAtomically(info_path,
                                                        std::move(output_js));
}

Status BackingStore::CreateDatabase(
    blink::IndexedDBDatabaseMetadata& metadata) {
  // TODO(jsbell): Don't persist metadata if open fails. http://crbug.com/395472
  std::unique_ptr<LevelDBDirectTransaction> transaction =
      transactional_leveldb_factory().CreateLevelDBDirectTransaction(db_.get());

  int64_t row_id = 0;
  Status s = GetNewDatabaseId(transaction.get(), &row_id);
  if (!s.ok()) {
    return s;
  }
  DCHECK_GE(row_id, 0);

  int64_t version = metadata.version;
  if (version == IndexedDBDatabaseMetadata::NO_VERSION) {
    version = IndexedDBDatabaseMetadata::DEFAULT_VERSION;
  }

  s = PutInt(transaction.get(),
             DatabaseNameKey::Encode(origin_identifier_, metadata.name),
             row_id);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(CREATE_IDBDATABASE_METADATA);
    return s;
  }
  s = PutVarInt(
      transaction.get(),
      DatabaseMetaDataKey::Encode(row_id, DatabaseMetaDataKey::USER_VERSION),
      version);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(CREATE_IDBDATABASE_METADATA);
    return s;
  }
  s = PutVarInt(
      transaction.get(),
      DatabaseMetaDataKey::Encode(
          row_id, DatabaseMetaDataKey::BLOB_KEY_GENERATOR_CURRENT_NUMBER),
      DatabaseMetaDataKey::kBlobNumberGeneratorInitialNumber);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(CREATE_IDBDATABASE_METADATA);
    return s;
  }

  s = transaction->Commit();
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(CREATE_IDBDATABASE_METADATA);
    return s;
  }

  metadata.id = row_id;
  metadata.max_object_store_id = 0;

  return s;
}

Status BackingStore::DeleteDatabase(const std::u16string& name,
                                    std::vector<PartitionedLock> locks,
                                    base::OnceClosure on_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif
  TRACE_EVENT0("IndexedDB", "BackingStore::DeleteDatabase");

  scoped_refptr<TransactionalLevelDBTransaction> transaction =
      transactional_leveldb_factory_->CreateLevelDBTransaction(
          db(), db()->scopes()->CreateScope(std::move(locks)));
  transaction->set_commit_cleanup_complete_callback(std::move(on_complete));

  Status s;
  bool success = false;
  int64_t id = 0;
  s = FindDatabaseId(db_.get(), origin_identifier_, name, &id, &success);
  if (!s.ok()) {
    return s;
  }
  if (!success) {
    return Status::OK();
  }

  // `ORIGIN_NAME` is the first key (0) in the database prefix, so this
  // deletes the whole database.
  const std::string start_key =
      DatabaseMetaDataKey::Encode(id, DatabaseMetaDataKey::ORIGIN_NAME);
  const std::string stop_key =
      DatabaseMetaDataKey::Encode(id + 1, DatabaseMetaDataKey::ORIGIN_NAME);
  {
    TRACE_EVENT0("IndexedDB", "BackingStore::DeleteDatabase.DeleteEntries");
    // It is safe to do deferred deletion here because database ids are never
    // reused, so this range of keys will never be accessed again.
    s = transaction->RemoveRange(
        start_key, stop_key, LevelDBScopeDeletionMode::kDeferredWithCompaction);
  }
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(DELETE_DATABASE);
    return s;
  }

  const std::string key = DatabaseNameKey::Encode(origin_identifier_, name);
  s = transaction->Remove(key);
  if (!s.ok()) {
    return s;
  }

  bool need_cleanup = false;
  bool database_has_blob_references =
      active_blob_registry()->MarkDatabaseDeletedAndCheckIfReferenced(id);
  if (database_has_blob_references) {
    s = MergeDatabaseIntoActiveBlobJournal(transaction.get(), id);
    if (!s.ok()) {
      return s;
    }
  } else {
    s = MergeDatabaseIntoRecoveryBlobJournal(transaction.get(), id);
    if (!s.ok()) {
      return s;
    }
    need_cleanup = true;
  }

  bool sync_on_commit = false;
  s = transaction->Commit(sync_on_commit);
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(DELETE_DATABASE);
    return s;
  }

  // If another transaction is running, this will defer processing
  // the journal until completion.
  if (need_cleanup) {
    CleanRecoveryJournalIgnoreReturn();
  }

  return s;
}

Status BackingStore::SetDatabaseVersion(
    Transaction* transaction,
    int64_t row_id,
    int64_t version,
    blink::IndexedDBDatabaseMetadata* metadata) {
  if (version == IndexedDBDatabaseMetadata::NO_VERSION) {
    version = IndexedDBDatabaseMetadata::DEFAULT_VERSION;
  }
  DCHECK_GE(version, 0) << "version was " << version;
  metadata->version = version;
  return PutVarInt(
      transaction->transaction(),
      DatabaseMetaDataKey::Encode(row_id, DatabaseMetaDataKey::USER_VERSION),
      version);
}

Status BackingStore::CreateObjectStore(
    Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    std::u16string name,
    blink::IndexedDBKeyPath key_path,
    bool auto_increment,
    blink::IndexedDBObjectStoreMetadata* metadata) {
  TransactionalLevelDBTransaction* leveldb_transaction =
      transaction->transaction();
  if (!KeyPrefix::ValidIds(database_id, object_store_id)) {
    return InvalidDBKeyStatus();
  }
  Status s =
      SetMaxObjectStoreId(leveldb_transaction, database_id, object_store_id);
  if (!s.ok()) {
    return s;
  }

  static const constexpr int64_t kInitialLastVersionNumber = 1;
  const std::string name_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::NAME);
  const std::string key_path_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::KEY_PATH);
  const std::string auto_increment_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::AUTO_INCREMENT);
  const std::string evictable_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::EVICTABLE);
  const std::string last_version_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::LAST_VERSION);
  const std::string max_index_id_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::MAX_INDEX_ID);
  const std::string has_key_path_key = ObjectStoreMetaDataKey::Encode(
      database_id, object_store_id, ObjectStoreMetaDataKey::HAS_KEY_PATH);
  const std::string key_generator_current_number_key =
      ObjectStoreMetaDataKey::Encode(
          database_id, object_store_id,
          ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER);
  const std::string names_key = ObjectStoreNamesKey::Encode(database_id, name);

  s = PutString(leveldb_transaction, name_key, name);
  if (!s.ok()) {
    return s;
  }
  s = PutIDBKeyPath(leveldb_transaction, key_path_key, key_path);
  if (!s.ok()) {
    return s;
  }
  s = PutInt(leveldb_transaction, auto_increment_key, auto_increment);
  if (!s.ok()) {
    return s;
  }
  s = PutInt(leveldb_transaction, evictable_key, false);
  if (!s.ok()) {
    return s;
  }
  s = PutInt(leveldb_transaction, last_version_key, kInitialLastVersionNumber);
  if (!s.ok()) {
    return s;
  }
  s = PutInt(leveldb_transaction, max_index_id_key, kMinimumIndexId);
  if (!s.ok()) {
    return s;
  }
  s = PutBool(leveldb_transaction, has_key_path_key, !key_path.IsNull());
  if (!s.ok()) {
    return s;
  }
  s = PutInt(leveldb_transaction, key_generator_current_number_key,
             ObjectStoreMetaDataKey::kKeyGeneratorInitialNumber);
  if (!s.ok()) {
    return s;
  }
  s = PutInt(leveldb_transaction, names_key, object_store_id);
  if (!s.ok()) {
    return s;
  }

  metadata->name = std::move(name);
  metadata->id = object_store_id;
  metadata->key_path = std::move(key_path);
  metadata->auto_increment = auto_increment;
  metadata->max_index_id = blink::IndexedDBObjectStoreMetadata::kMinimumIndexId;
  metadata->indexes.clear();
  return s;
}

Status BackingStore::DeleteObjectStore(
    Transaction* transaction,
    int64_t database_id,
    const blink::IndexedDBObjectStoreMetadata& object_store) {
  if (!KeyPrefix::ValidIds(database_id, object_store.id)) {
    return InvalidDBKeyStatus();
  }

  std::u16string object_store_name;
  bool found = false;
  TransactionalLevelDBTransaction* leveldb_transaction =
      transaction->transaction();
  Status s =
      GetString(leveldb_transaction,
                ObjectStoreMetaDataKey::Encode(database_id, object_store.id,
                                               ObjectStoreMetaDataKey::NAME),
                &object_store_name, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(DELETE_OBJECT_STORE);
    return s;
  }
  if (!found) {
    INTERNAL_CONSISTENCY_ERROR(DELETE_OBJECT_STORE);
    return InternalInconsistencyStatus();
  }

  s = leveldb_transaction->RemoveRange(
      ObjectStoreMetaDataKey::Encode(database_id, object_store.id, 0),
      ObjectStoreMetaDataKey::EncodeMaxKey(database_id, object_store.id),
      LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);

  if (s.ok()) {
    s = leveldb_transaction->Remove(
        ObjectStoreNamesKey::Encode(database_id, object_store_name));
    if (!s.ok()) {
      INTERNAL_WRITE_ERROR(DELETE_OBJECT_STORE);
      return s;
    }

    s = leveldb_transaction->RemoveRange(
        IndexFreeListKey::Encode(database_id, object_store.id, 0),
        IndexFreeListKey::EncodeMaxKey(database_id, object_store.id),
        LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);
  }

  if (s.ok()) {
    s = leveldb_transaction->RemoveRange(
        IndexMetaDataKey::Encode(database_id, object_store.id, 0, 0),
        IndexMetaDataKey::EncodeMaxKey(database_id, object_store.id),
        LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);
  }

  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(DELETE_OBJECT_STORE);
  }
  return s;
}

Status BackingStore::RenameObjectStore(
    Transaction* transaction,
    int64_t database_id,
    std::u16string new_name,
    std::u16string* old_name,
    blink::IndexedDBObjectStoreMetadata* metadata) {
  if (!KeyPrefix::ValidIds(database_id, metadata->id)) {
    return InvalidDBKeyStatus();
  }

  const std::string name_key = ObjectStoreMetaDataKey::Encode(
      database_id, metadata->id, ObjectStoreMetaDataKey::NAME);
  const std::string new_names_key =
      ObjectStoreNamesKey::Encode(database_id, new_name);

  std::u16string old_name_check;
  bool found = false;
  Status s =
      GetString(transaction->transaction(), name_key, &old_name_check, &found);
  // TODO(dmurph): Change DELETE_OBJECT_STORE to RENAME_OBJECT_STORE & fix UMA.
  if (!s.ok()) {
    INTERNAL_READ_ERROR(DELETE_OBJECT_STORE);
    return s;
  }
  if (!found || old_name_check != metadata->name) {
    INTERNAL_CONSISTENCY_ERROR(DELETE_OBJECT_STORE);
    return InternalInconsistencyStatus();
  }
  const std::string old_names_key =
      ObjectStoreNamesKey::Encode(database_id, metadata->name);

  s = PutString(transaction->transaction(), name_key, new_name);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(DELETE_OBJECT_STORE);
    return s;
  }
  s = PutInt(transaction->transaction(), new_names_key, metadata->id);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(DELETE_OBJECT_STORE);
    return s;
  }
  s = transaction->transaction()->Remove(old_names_key);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(DELETE_OBJECT_STORE);
    return s;
  }
  *old_name = std::move(metadata->name);
  metadata->name = std::move(new_name);
  return s;
}

Status BackingStore::CreateIndex(Transaction* transaction,
                                 int64_t database_id,
                                 int64_t object_store_id,
                                 int64_t index_id,
                                 std::u16string name,
                                 blink::IndexedDBKeyPath key_path,
                                 bool is_unique,
                                 bool is_multi_entry,
                                 blink::IndexedDBIndexMetadata* metadata) {
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id)) {
    return InvalidDBKeyStatus();
  }
  TransactionalLevelDBTransaction* leveldb_transaction =
      transaction->transaction();
  Status s = SetMaxIndexId(leveldb_transaction, database_id, object_store_id,
                           index_id);

  if (!s.ok()) {
    return s;
  }

  const std::string name_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, index_id, IndexMetaDataKey::NAME);
  const std::string unique_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, index_id, IndexMetaDataKey::UNIQUE);
  const std::string key_path_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, index_id, IndexMetaDataKey::KEY_PATH);
  const std::string multi_entry_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, index_id, IndexMetaDataKey::MULTI_ENTRY);

  s = PutString(leveldb_transaction, name_key, name);
  if (!s.ok()) {
    return s;
  }
  s = PutBool(leveldb_transaction, unique_key, is_unique);
  if (!s.ok()) {
    return s;
  }
  s = PutIDBKeyPath(leveldb_transaction, key_path_key, key_path);
  if (!s.ok()) {
    return s;
  }
  s = PutBool(leveldb_transaction, multi_entry_key, is_multi_entry);
  if (!s.ok()) {
    return s;
  }

  metadata->name = std::move(name);
  metadata->id = index_id;
  metadata->key_path = std::move(key_path);
  metadata->unique = is_unique;
  metadata->multi_entry = is_multi_entry;

  return s;
}

Status BackingStore::DeleteIndex(
    Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const blink::IndexedDBIndexMetadata& metadata) {
  if (!KeyPrefix::ValidIds(database_id, object_store_id, metadata.id)) {
    return InvalidDBKeyStatus();
  }

  const std::string index_meta_data_start =
      IndexMetaDataKey::Encode(database_id, object_store_id, metadata.id, 0);
  const std::string index_meta_data_end =
      IndexMetaDataKey::EncodeMaxKey(database_id, object_store_id, metadata.id);
  return Status(transaction->transaction()->RemoveRange(
      index_meta_data_start, index_meta_data_end,
      LevelDBScopeDeletionMode::kImmediateWithRangeEndExclusive));
}

Status BackingStore::RenameIndex(Transaction* transaction,
                                 int64_t database_id,
                                 int64_t object_store_id,
                                 std::u16string new_name,
                                 std::u16string* old_name,
                                 blink::IndexedDBIndexMetadata* metadata) {
  if (!KeyPrefix::ValidIds(database_id, object_store_id, metadata->id)) {
    return InvalidDBKeyStatus();
  }

  const std::string name_key = IndexMetaDataKey::Encode(
      database_id, object_store_id, metadata->id, IndexMetaDataKey::NAME);

  // TODO(dmurph): Add consistancy checks & umas for old name.
  Status s = PutString(transaction->transaction(), name_key, new_name);
  if (!s.ok()) {
    return s;
  }
  *old_name = std::move(metadata->name);
  metadata->name = std::move(new_name);
  return Status::OK();
}

void BackingStore::Compact() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif
  db_->CompactAll();
}

Status BackingStore::GetRecord(Transaction* transaction,
                               int64_t database_id,
                               int64_t object_store_id,
                               const IndexedDBKey& key,
                               IndexedDBValue* record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif

  TRACE_EVENT0("IndexedDB", "BackingStore::GetRecord");
  if (!KeyPrefix::ValidIds(database_id, object_store_id)) {
    return InvalidDBKeyStatus();
  }
  TransactionalLevelDBTransaction* leveldb_transaction =
      transaction->transaction();

  const std::string leveldb_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, key);
  std::string data;

  record->clear();

  bool found = false;
  Status s(leveldb_transaction->Get(leveldb_key, &data, &found));
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_RECORD);
    return s;
  }
  if (!found) {
    return s;
  }
  if (data.empty()) {
    INTERNAL_READ_ERROR(GET_RECORD);
    return Status::NotFound("Record contained no data");
  }

  int64_t version;
  std::string_view slice(data);
  if (!DecodeVarInt(&slice, &version)) {
    INTERNAL_READ_ERROR(GET_RECORD);
    return InternalInconsistencyStatus();
  }

  record->bits.assign(slice.begin(), slice.end());
  return transaction->GetExternalObjectsForRecord(database_id, leveldb_key,
                                                  record);
}

int64_t BackingStore::GetInMemorySize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(in_memory());

  int64_t blob_size = 0;
  for (const auto& kvp : in_memory_external_object_map_) {
    for (const IndexedDBExternalObject& object :
         kvp.second->external_objects()) {
      if (object.object_type() == IndexedDBExternalObject::ObjectType::kBlob) {
        blob_size += object.size();
      }
    }
  }

  int64_t level_db_size = 0;
  Status s = GetDBSizeFromEnv(db_->env(), "/", &level_db_size);
  if (!s.ok()) {
    LOG(ERROR) << "Failed to GetDBSizeFromEnv: " << s.ToString();
  }

  return blob_size + level_db_size;
}

Status BackingStore::PutRecord(BackingStore::Transaction* transaction,
                               int64_t database_id,
                               int64_t object_store_id,
                               const IndexedDBKey& key,
                               IndexedDBValue* value,
                               RecordIdentifier* record_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif

  TRACE_EVENT0("IndexedDB", "BackingStore::PutRecord");
  if (!KeyPrefix::ValidIds(database_id, object_store_id)) {
    return InvalidDBKeyStatus();
  }
  DCHECK(key.IsValid());

  TransactionalLevelDBTransaction* leveldb_transaction =
      transaction->transaction();
  int64_t version = -1;
  Status s = GetNewVersionNumber(leveldb_transaction, database_id,
                                 object_store_id, &version);
  if (!s.ok()) {
    return s;
  }
  DCHECK_GE(version, 0);
  const std::string object_store_data_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, key);

  std::string v;
  EncodeVarInt(version, &v);
  v.append(value->bits.begin(), value->bits.end());

  s = leveldb_transaction->Put(object_store_data_key, &v);
  if (!s.ok()) {
    return s;
  }
  s = transaction->PutExternalObjectsIfNeeded(
      database_id, object_store_data_key, &value->external_objects);
  if (!s.ok()) {
    return s;
  }

  const std::string exists_entry_key =
      ExistsEntryKey::Encode(database_id, object_store_id, key);
  std::string version_encoded;
  EncodeInt(version, &version_encoded);
  s = leveldb_transaction->Put(exists_entry_key, &version_encoded);
  if (!s.ok()) {
    return s;
  }

  std::string key_encoded;
  EncodeIDBKey(key, &key_encoded);
  record_identifier->Reset(key_encoded, version);
  return s;
}

Status BackingStore::ClearObjectStore(BackingStore::Transaction* transaction,
                                      int64_t database_id,
                                      int64_t object_store_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif

  TRACE_EVENT0("IndexedDB", "BackingStore::ClearObjectStore");
  if (!KeyPrefix::ValidIds(database_id, object_store_id)) {
    return InvalidDBKeyStatus();
  }

  Status s =
      DeleteBlobsInObjectStore(transaction, database_id, object_store_id);
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(CLEAR_OBJECT_STORE);
    return s;
  }

  // Don't delete the BlobEntryKeys so that they can be read and deleted
  // via CollectBlobFilesToRemove.
  // TODO(enne): This process could be optimized by storing the blob ids
  // in DeleteBlobsInObjectStore rather than re-reading them later.
  const std::string start_key1 =
      KeyPrefix(database_id, object_store_id).Encode();
  const std::string stop_key1 =
      BlobEntryKey::EncodeMinKeyForObjectStore(database_id, object_store_id);
  const std::string start_key2 =
      BlobEntryKey::EncodeStopKeyForObjectStore(database_id, object_store_id);
  const std::string stop_key2 =
      KeyPrefix(database_id, object_store_id + 1).Encode();
  s = transaction->transaction()->RemoveRange(
      start_key1, stop_key1,
      LevelDBScopeDeletionMode::kImmediateWithRangeEndExclusive);
  if (!s.ok()) {
    return s;
  }
  return Status(transaction->transaction()->RemoveRange(
      start_key2, stop_key2,
      LevelDBScopeDeletionMode::kImmediateWithRangeEndExclusive));
}

Status BackingStore::DeleteRecord(BackingStore::Transaction* transaction,
                                  int64_t database_id,
                                  int64_t object_store_id,
                                  const RecordIdentifier& record_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT0("IndexedDB", "BackingStore::DeleteRecord");
  if (!KeyPrefix::ValidIds(database_id, object_store_id)) {
    return InvalidDBKeyStatus();
  }
  TransactionalLevelDBTransaction* leveldb_transaction =
      transaction->transaction();

  const std::string object_store_data_key = ObjectStoreDataKey::Encode(
      database_id, object_store_id, record_identifier.primary_key());
  Status s(leveldb_transaction->Remove(object_store_data_key));
  if (!s.ok()) {
    return s;
  }
  s = transaction->PutExternalObjectsIfNeeded(database_id,
                                              object_store_data_key, nullptr);
  if (!s.ok()) {
    return s;
  }

  const std::string exists_entry_key = ExistsEntryKey::Encode(
      database_id, object_store_id, record_identifier.primary_key());
  return Status(leveldb_transaction->Remove(exists_entry_key));
}

Status BackingStore::DeleteRange(BackingStore::Transaction* transaction,
                                 int64_t database_id,
                                 int64_t object_store_id,
                                 const IndexedDBKeyRange& key_range) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif

  // TODO(dmurph): Remove the need to create these cursors.
  // https://crbug.com/980678
  Status s;
  std::unique_ptr<BackingStore::Cursor> start_cursor = OpenObjectStoreCursor(
      transaction, database_id, object_store_id, key_range,
      blink::mojom::IDBCursorDirection::Next, &s);
  if (!s.ok()) {
    return s;
  }
  if (!start_cursor) {
    return Status::OK();  // Empty range == delete success.
  }
  std::unique_ptr<BackingStore::Cursor> end_cursor = OpenObjectStoreCursor(
      transaction, database_id, object_store_id, key_range,
      blink::mojom::IDBCursorDirection::Prev, &s);

  if (!s.ok()) {
    return s;
  }
  if (!end_cursor) {
    return Status::OK();  // Empty range == delete success.
  }

  BlobEntryKey start_blob_number, end_blob_number;
  std::string start_key = ObjectStoreDataKey::Encode(
      database_id, object_store_id, start_cursor->key());
  std::string_view start_key_piece(start_key);
  if (!BlobEntryKey::FromObjectStoreDataKey(&start_key_piece,
                                            &start_blob_number)) {
    return InternalInconsistencyStatus();
  }
  std::string stop_key = ObjectStoreDataKey::Encode(
      database_id, object_store_id, end_cursor->key());
  std::string_view stop_key_piece(stop_key);
  if (!BlobEntryKey::FromObjectStoreDataKey(&stop_key_piece,
                                            &end_blob_number)) {
    return InternalInconsistencyStatus();
  }

  s = DeleteBlobsInRange(transaction, database_id, start_blob_number.Encode(),
                         end_blob_number.Encode(), false);
  if (!s.ok()) {
    return s;
  }
  s = transaction->transaction()->RemoveRange(
      start_key, stop_key,
      LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);
  if (!s.ok()) {
    return s;
  }
  start_key =
      ExistsEntryKey::Encode(database_id, object_store_id, start_cursor->key());
  stop_key =
      ExistsEntryKey::Encode(database_id, object_store_id, end_cursor->key());

  s = transaction->transaction()->RemoveRange(
      start_key, stop_key,
      LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);
  return s;
}

Status BackingStore::GetKeyGeneratorCurrentNumber(
    BackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t* key_generator_current_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif
  if (!KeyPrefix::ValidIds(database_id, object_store_id)) {
    return InvalidDBKeyStatus();
  }
  TransactionalLevelDBTransaction* leveldb_transaction =
      transaction->transaction();

  const std::string key_generator_current_number_key =
      ObjectStoreMetaDataKey::Encode(
          database_id, object_store_id,
          ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER);

  *key_generator_current_number = -1;
  std::string data;

  bool found = false;
  Status s(leveldb_transaction->Get(key_generator_current_number_key, &data,
                                    &found));
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_KEY_GENERATOR_CURRENT_NUMBER);
    return s;
  }
  if (found && !data.empty()) {
    std::string_view slice(data);
    if (!DecodeInt(&slice, key_generator_current_number) || !slice.empty()) {
      INTERNAL_READ_ERROR(GET_KEY_GENERATOR_CURRENT_NUMBER);
      return InternalInconsistencyStatus();
    }
    return s;
  }

  // Previously, the key generator state was not stored explicitly
  // but derived from the maximum numeric key present in existing
  // data. This violates the spec as the data may be cleared but the
  // key generator state must be preserved.
  // TODO(jsbell): Fix this for all stores on database open?
  const std::string start_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, MinIDBKey());
  const std::string stop_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, MaxIDBKey());

  std::unique_ptr<TransactionalLevelDBIterator> it;
  std::tie(it, s) = CreateIteratorAndGetStatus(*leveldb_transaction);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_KEY_GENERATOR_CURRENT_NUMBER);
    return s;
  }
  int64_t max_numeric_key = 0;

  for (s = it->Seek(start_key);
       s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0;
       s = it->Next()) {
    std::string_view slice(it->Key());
    ObjectStoreDataKey data_key;
    if (!ObjectStoreDataKey::Decode(&slice, &data_key) || !slice.empty()) {
      INTERNAL_READ_ERROR(GET_KEY_GENERATOR_CURRENT_NUMBER);
      return InternalInconsistencyStatus();
    }
    std::unique_ptr<IndexedDBKey> user_key = data_key.user_key();
    if (user_key->type() == blink::mojom::IDBKeyType::Number) {
      int64_t n = static_cast<int64_t>(user_key->number());
      if (n > max_numeric_key) {
        max_numeric_key = n;
      }
    }
  }

  if (s.ok()) {
    *key_generator_current_number = max_numeric_key + 1;
  } else {
    INTERNAL_READ_ERROR(GET_KEY_GENERATOR_CURRENT_NUMBER);
  }

  return s;
}

Status BackingStore::MaybeUpdateKeyGeneratorCurrentNumber(
    BackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t new_number,
    bool check_current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif
  if (!KeyPrefix::ValidIds(database_id, object_store_id)) {
    return InvalidDBKeyStatus();
  }

  if (check_current) {
    int64_t current_number;
    Status s = GetKeyGeneratorCurrentNumber(transaction, database_id,
                                            object_store_id, &current_number);
    if (!s.ok()) {
      return s;
    }
    if (new_number <= current_number) {
      return s;
    }
  }

  const std::string key_generator_current_number_key =
      ObjectStoreMetaDataKey::Encode(
          database_id, object_store_id,
          ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER);
  return PutInt(transaction->transaction(), key_generator_current_number_key,
                new_number);
}

Status BackingStore::KeyExistsInObjectStore(
    BackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKey& key,
    RecordIdentifier* found_record_identifier,
    bool* found) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif
  TRACE_EVENT0("IndexedDB", "BackingStore::KeyExistsInObjectStore");
  if (!KeyPrefix::ValidIds(database_id, object_store_id)) {
    return InvalidDBKeyStatus();
  }
  *found = false;
  const std::string leveldb_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, key);
  std::string data;

  Status s(transaction->transaction()->Get(leveldb_key, &data, found));
  if (!s.ok()) {
    INTERNAL_READ_ERROR(KEY_EXISTS_IN_OBJECT_STORE);
    return s;
  }
  if (!*found) {
    return Status::OK();
  }
  if (data.empty()) {
    INTERNAL_READ_ERROR(KEY_EXISTS_IN_OBJECT_STORE);
    return InternalInconsistencyStatus();
  }

  int64_t version;
  std::string_view slice(data);
  if (!DecodeVarInt(&slice, &version)) {
    return InternalInconsistencyStatus();
  }

  std::string encoded_key;
  EncodeIDBKey(key, &encoded_key);
  found_record_identifier->Reset(encoded_key, version);
  return s;
}

void BackingStore::ReportBlobUnused(int64_t database_id, int64_t blob_number) {
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif
  bool all_blobs = blob_number == DatabaseMetaDataKey::kAllBlobsNumber;
  DCHECK(all_blobs || DatabaseMetaDataKey::IsValidBlobNumber(blob_number));
  std::unique_ptr<LevelDBDirectTransaction> transaction =
      transactional_leveldb_factory().CreateLevelDBDirectTransaction(db_.get());

  BlobJournalType active_blob_journal, recovery_journal;
  if (!GetActiveBlobJournal(transaction.get(), &active_blob_journal).ok()) {
    return;
  }
  DCHECK(!active_blob_journal.empty());
  if (!GetRecoveryBlobJournal(transaction.get(), &recovery_journal).ok()) {
    return;
  }

  // There are several cases to handle.  If blob_number is kAllBlobsNumber, we
  // want to remove all entries with database_id from the active blob journal
  // and add only kAllBlobsNumber to the recovery journal.  Otherwise if
  // IsValidBlobNumber(blob_number) and we hit kAllBlobsNumber for the right
  // database_id in the journal, we leave the kAllBlobsNumber entry in the
  // active blob journal but add the specific blob to the recovery.  Otherwise
  // if IsValidBlobNumber(blob_number) and we find a matching (database_id,
  // blob_number) tuple, we should move it to the recovery journal.
  BlobJournalType new_active_blob_journal;
  for (auto journal_iter = active_blob_journal.begin();
       journal_iter != active_blob_journal.end(); ++journal_iter) {
    int64_t current_database_id = journal_iter->first;
    int64_t current_blob_number = journal_iter->second;
    bool current_all_blobs =
        current_blob_number == DatabaseMetaDataKey::kAllBlobsNumber;
    DCHECK(KeyPrefix::IsValidDatabaseId(current_database_id) ||
           current_all_blobs);
    if (current_database_id == database_id &&
        (all_blobs || current_all_blobs ||
         blob_number == current_blob_number)) {
      if (!all_blobs) {
        recovery_journal.push_back({database_id, current_blob_number});
        if (current_all_blobs) {
          new_active_blob_journal.push_back(*journal_iter);
        }
        new_active_blob_journal.insert(
            new_active_blob_journal.end(), ++journal_iter,
            active_blob_journal.end());  // All the rest.
        break;
      }
    } else {
      new_active_blob_journal.push_back(*journal_iter);
    }
  }
  if (all_blobs) {
    recovery_journal.push_back(
        {database_id, DatabaseMetaDataKey::kAllBlobsNumber});
  }
  UpdateRecoveryBlobJournal(transaction.get(), recovery_journal);
  UpdateActiveBlobJournal(transaction.get(), new_active_blob_journal);
  transaction->Commit();
  // We could just do the deletions/cleaning here, but if there are a lot of
  // blobs about to be garbage collected, it'd be better to wait and do them all
  // at once.
  StartJournalCleaningTimer();
}

void BackingStore::StartJournalCleaningTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif
  ++num_aggregated_journal_cleaning_requests_;

  if (execute_journal_cleaning_on_no_txns_) {
    return;
  }

  if (num_aggregated_journal_cleaning_requests_ >= kMaxJournalCleanRequests) {
    journal_cleaning_timer_.AbandonAndStop();
    CleanRecoveryJournalIgnoreReturn();
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();

  if (journal_cleaning_timer_window_start_ == base::TimeTicks() ||
      !journal_cleaning_timer_.IsRunning()) {
    journal_cleaning_timer_window_start_ = now;
  }

  base::TimeDelta time_until_max = kMaxJournalCleaningWindowTime -
                                   (now - journal_cleaning_timer_window_start_);
  base::TimeDelta delay =
      std::min(kInitialJournalCleaningWindowTime, time_until_max);

  if (delay <= base::Seconds(0)) {
    journal_cleaning_timer_.AbandonAndStop();
    CleanRecoveryJournalIgnoreReturn();
    return;
  }

  journal_cleaning_timer_.Start(
      FROM_HERE, delay, this, &BackingStore::CleanRecoveryJournalIgnoreReturn);
}

// This assumes a file path of dbId/second-to-LSB-of-counter/counter.
base::FilePath BackingStore::GetBlobFileName(int64_t database_id,
                                             int64_t blob_number) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetBlobFileNameForKey(blob_path_, database_id, blob_number);
}

bool BackingStore::RemoveBlobFile(int64_t database_id,
                                  int64_t blob_number) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath path = GetBlobFileName(database_id, blob_number);
#if DCHECK_IS_ON()
  ++num_blob_files_deleted_;
  DVLOG(1) << "Deleting blob " << blob_number << " from IndexedDB database "
           << database_id << " at path " << path.value();
#endif
  return base::DeleteFile(path);
}

bool BackingStore::RemoveBlobDirectory(int64_t database_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath path = GetBlobDirectoryName(blob_path_, database_id);
  return base::DeletePathRecursively(path);
}

Status BackingStore::CleanUpBlobJournal(const std::string& level_db_key) const {
  TRACE_EVENT0("IndexedDB", "BackingStore::CleanUpBlobJournal");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!committing_transaction_count_);
  std::unique_ptr<LevelDBDirectTransaction> journal_transaction =
      transactional_leveldb_factory().CreateLevelDBDirectTransaction(db_.get());
  BlobJournalType journal;

  Status s = GetBlobJournal(level_db_key, journal_transaction.get(), &journal);
  if (!s.ok()) {
    return s;
  }
  if (journal.empty()) {
    return Status::OK();
  }
  s = CleanUpBlobJournalEntries(journal);
  if (!s.ok()) {
    return s;
  }
  ClearBlobJournal(journal_transaction.get(), level_db_key);
  s = journal_transaction->Commit();
  // Notify blob files cleaned even if commit fails, as files could still be
  // deleted.
  if (!in_memory()) {
    blob_files_cleaned_.Run();
  }
  return s;
}

Status BackingStore::CleanUpBlobJournalEntries(
    const BlobJournalType& journal) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IndexedDB", "BackingStore::CleanUpBlobJournalEntries");
  if (journal.empty() || in_memory()) {
    return Status::OK();
  }
  for (const auto& entry : journal) {
    int64_t database_id = entry.first;
    int64_t blob_number = entry.second;
    DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
    if (blob_number == DatabaseMetaDataKey::kAllBlobsNumber) {
      if (!RemoveBlobDirectory(database_id)) {
        return IOErrorStatus();
      }
    } else {
      DCHECK(DatabaseMetaDataKey::IsValidBlobNumber(blob_number));
      if (!RemoveBlobFile(database_id, blob_number)) {
        return IOErrorStatus();
      }
    }
  }
  return Status::OK();
}

void BackingStore::WillCommitTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++committing_transaction_count_;
}

void BackingStore::DidCommitTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(committing_transaction_count_, 0UL);
  --committing_transaction_count_;
  if (committing_transaction_count_ == 0 &&
      execute_journal_cleaning_on_no_txns_) {
    execute_journal_cleaning_on_no_txns_ = false;
    CleanRecoveryJournalIgnoreReturn();
  }
}

Status BackingStore::Transaction::GetExternalObjectsForRecord(
    int64_t database_id,
    const std::string& object_store_data_key,
    IndexedDBValue* value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IndexedDBExternalObjectChangeRecord* change_record = nullptr;
  auto object_iter = external_object_change_map_.find(object_store_data_key);
  if (object_iter != external_object_change_map_.end()) {
    change_record = object_iter->second.get();
  } else {
    object_iter = in_memory_external_object_map_.find(object_store_data_key);
    if (object_iter != in_memory_external_object_map_.end()) {
      change_record = object_iter->second.get();
    }
  }
  if (change_record) {
    // Either we haven't written the blob to disk yet or we're in in_memory
    // mode, so we have to send back the one they sent us.  This change record
    // includes the original UUID.
    value->external_objects = change_record->external_objects();
    return Status::OK();
  }

  BlobEntryKey blob_entry_key;
  std::string_view leveldb_key_piece(object_store_data_key);
  if (!BlobEntryKey::FromObjectStoreDataKey(&leveldb_key_piece,
                                            &blob_entry_key)) {
    NOTREACHED_IN_MIGRATION();
    return InternalInconsistencyStatus();
  }
  std::string encoded_key = blob_entry_key.Encode();
  bool found;
  std::string encoded_value;
  Status s(transaction()->Get(encoded_key, &encoded_value, &found));
  if (!s.ok()) {
    return s;
  }
  if (found) {
    if (!DecodeExternalObjects(encoded_value, &value->external_objects)) {
      INTERNAL_READ_ERROR(GET_BLOB_INFO_FOR_RECORD);
      return InternalInconsistencyStatus();
    }
    for (auto& entry : value->external_objects) {
      switch (entry.object_type()) {
        case IndexedDBExternalObject::ObjectType::kFile:
        case IndexedDBExternalObject::ObjectType::kBlob:
          entry.set_indexed_db_file_path(backing_store_->GetBlobFileName(
              database_id, entry.blob_number()));
          entry.set_mark_used_callback(
              backing_store_->active_blob_registry()->GetMarkBlobActiveCallback(
                  database_id, entry.blob_number()));
          entry.set_release_callback(
              backing_store_->active_blob_registry()->GetFinalReleaseCallback(
                  database_id, entry.blob_number()));
          break;
        case IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle:
          break;
      }
    }
  }
  return Status::OK();
}

base::WeakPtr<BackingStore::Transaction>
BackingStore::Transaction::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void BackingStore::CleanRecoveryJournalIgnoreReturn() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // While a transaction is busy it is not safe to clean the journal.
  if (committing_transaction_count_ > 0) {
    execute_journal_cleaning_on_no_txns_ = true;
    return;
  }
  num_aggregated_journal_cleaning_requests_ = 0;
  CleanUpBlobJournal(RecoveryBlobJournalKey::Encode());
}

Status BackingStore::ClearIndex(BackingStore::Transaction* transaction,
                                int64_t database_id,
                                int64_t object_store_id,
                                int64_t index_id) {
  TRACE_EVENT0("IndexedDB", "BackingStore::ClearIndex");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id)) {
    return InvalidDBKeyStatus();
  }
  TransactionalLevelDBTransaction* leveldb_transaction =
      transaction->transaction();

  const std::string index_data_start =
      IndexDataKey::EncodeMinKey(database_id, object_store_id, index_id);
  const std::string index_data_end =
      IndexDataKey::EncodeMaxKey(database_id, object_store_id, index_id);
  Status s(leveldb_transaction->RemoveRange(
      index_data_start, index_data_end,
      LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive));

  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(DELETE_INDEX);
  }

  return s;
}

Status BackingStore::PutIndexDataForRecord(
    BackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKey& key,
    const RecordIdentifier& record_identifier) {
  TRACE_EVENT0("IndexedDB", "BackingStore::PutIndexDataForRecord");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif
  DCHECK(key.IsValid());
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id)) {
    return InvalidDBKeyStatus();
  }

  std::string encoded_key;
  EncodeIDBKey(key, &encoded_key);

  const std::string index_data_key =
      IndexDataKey::Encode(database_id, object_store_id, index_id, encoded_key,
                           record_identifier.primary_key(), 0);

  std::string data;
  EncodeVarInt(record_identifier.version(), &data);
  data.append(record_identifier.primary_key());

  return Status(transaction->transaction()->Put(index_data_key, &data));
}

Status BackingStore::FindKeyInIndex(BackingStore::Transaction* transaction,
                                    int64_t database_id,
                                    int64_t object_store_id,
                                    int64_t index_id,
                                    const IndexedDBKey& key,
                                    std::string* found_encoded_primary_key,
                                    bool* found) {
  TRACE_EVENT0("IndexedDB", "BackingStore::FindKeyInIndex");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif
  DCHECK(KeyPrefix::ValidIds(database_id, object_store_id, index_id));

  DCHECK(found_encoded_primary_key->empty());
  *found = false;

  TransactionalLevelDBTransaction* leveldb_transaction =
      transaction->transaction();
  const std::string leveldb_key =
      IndexDataKey::Encode(database_id, object_store_id, index_id, key);
  Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it;
  std::tie(it, s) = CreateIteratorAndGetStatus(*leveldb_transaction);
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(CREATE_ITERATOR);
    return s;
  }
  s = it->Seek(leveldb_key);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(FIND_KEY_IN_INDEX);
    return s;
  }

  for (;;) {
    if (!it->IsValid()) {
      return Status::OK();
    }
    if (CompareIndexKeys(it->Key(), leveldb_key) > 0) {
      return Status::OK();
    }

    std::string_view slice(it->Value());

    int64_t version;
    if (!DecodeVarInt(&slice, &version)) {
      INTERNAL_READ_ERROR(FIND_KEY_IN_INDEX);
      return InternalInconsistencyStatus();
    }
    *found_encoded_primary_key = std::string(slice);

    bool exists = false;
    s = VersionExists(leveldb_transaction, database_id, object_store_id,
                      version, *found_encoded_primary_key, &exists);
    if (!s.ok()) {
      return s;
    }
    if (!exists) {
      // Delete stale index data entry and continue.
      s = leveldb_transaction->Remove(it->Key());
      if (!s.ok()) {
        return s;
      }
      s = it->Next();
      continue;
    }
    *found = true;
    return s;
  }
}

Status BackingStore::GetPrimaryKeyViaIndex(
    BackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKey& key,
    std::unique_ptr<IndexedDBKey>* primary_key) {
  TRACE_EVENT0("IndexedDB", "BackingStore::GetPrimaryKeyViaIndex");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id)) {
    return InvalidDBKeyStatus();
  }

  bool found = false;
  std::string found_encoded_primary_key;
  Status s = FindKeyInIndex(transaction, database_id, object_store_id, index_id,
                            key, &found_encoded_primary_key, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_PRIMARY_KEY_VIA_INDEX);
    return s;
  }
  if (!found) {
    return s;
  }
  if (found_encoded_primary_key.empty()) {
    INTERNAL_READ_ERROR(GET_PRIMARY_KEY_VIA_INDEX);
    return InvalidDBKeyStatus();
  }

  std::string_view slice(found_encoded_primary_key);
  if (DecodeIDBKey(&slice, primary_key) && slice.empty()) {
    return s;
  }

  return InvalidDBKeyStatus();
}

Status BackingStore::KeyExistsInIndex(
    BackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKey& index_key,
    std::unique_ptr<IndexedDBKey>* found_primary_key,
    bool* exists) {
  TRACE_EVENT0("IndexedDB", "BackingStore::KeyExistsInIndex");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id)) {
    return InvalidDBKeyStatus();
  }

  *exists = false;
  std::string found_encoded_primary_key;
  Status s = FindKeyInIndex(transaction, database_id, object_store_id, index_id,
                            index_key, &found_encoded_primary_key, exists);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(KEY_EXISTS_IN_INDEX);
    return s;
  }
  if (!*exists) {
    return Status::OK();
  }
  if (found_encoded_primary_key.empty()) {
    INTERNAL_READ_ERROR(KEY_EXISTS_IN_INDEX);
    return InvalidDBKeyStatus();
  }

  std::string_view slice(found_encoded_primary_key);
  if (DecodeIDBKey(&slice, found_primary_key) && slice.empty()) {
    return s;
  }

  return InvalidDBKeyStatus();
}

Status BackingStore::GetDatabaseNames(std::vector<std::u16string>* names) {
  std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions;
  Status s = GetDatabaseNamesAndVersions(&names_and_versions);
  for (const blink::mojom::IDBNameAndVersionPtr& nav : names_and_versions) {
    names->push_back(nav->name);
  }
  return s;
}

Status BackingStore::GetDatabaseNamesAndVersions(
    std::vector<blink::mojom::IDBNameAndVersionPtr>* names_and_versions) {
  // TODO(dmurph): Get rid of on-demand metadata loading, and store metadata
  // in-memory.
  DCHECK(names_and_versions->empty());
  const std::string start_key =
      DatabaseNameKey::EncodeMinKeyForOrigin(origin_identifier_);
  const std::string stop_key =
      DatabaseNameKey::EncodeStopKeyForOrigin(origin_identifier_);

  std::unique_ptr<TransactionalLevelDBIterator> it =
      db_->CreateIterator(db_->DefaultReadOptions());
  Status s;
  for (s = it->Seek(start_key);
       s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0;
       s = it->Next()) {
    // Decode database name (in iterator key).
    std::string_view slice(it->Key());
    DatabaseNameKey database_name_key;
    if (!DatabaseNameKey::Decode(&slice, &database_name_key) ||
        !slice.empty()) {
      INTERNAL_CONSISTENCY_ERROR(GET_DATABASE_NAMES);
      continue;
    }

    // Decode database id (in iterator value).
    int64_t database_id = 0;
    std::string_view value_slice(it->Value());
    if (!DecodeInt(&value_slice, &database_id) || !value_slice.empty()) {
      INTERNAL_CONSISTENCY_ERROR(GET_DATABASE_NAMES);
      continue;
    }

    // Look up version by id.
    bool found = false;
    int64_t database_version = IndexedDBDatabaseMetadata::DEFAULT_VERSION;
    s = GetVarInt(db_.get(),
                  DatabaseMetaDataKey::Encode(
                      database_id, DatabaseMetaDataKey::USER_VERSION),
                  &database_version, &found);
    if (!s.ok() || !found) {
      INTERNAL_READ_ERROR(GET_DATABASE_NAMES);
      continue;
    }

    // Ignore stale metadata from failed initial opens.
    if (database_version != IndexedDBDatabaseMetadata::DEFAULT_VERSION) {
      names_and_versions->push_back(blink::mojom::IDBNameAndVersion::New(
          database_name_key.database_name(), database_version));
    }
  }
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_DATABASE_NAMES);
  }

  return s;
}

Status BackingStore::ReadMetadataForDatabaseName(
    const std::u16string& name,
    blink::IndexedDBDatabaseMetadata* metadata,
    bool* found) {
  TRACE_EVENT0("IndexedDB", "BackingStore::ReadMetadataForDatabaseName");
  const std::string key = DatabaseNameKey::Encode(origin_identifier_, name);
  *found = false;

  Status s = GetInt(db_.get(), key, &metadata->id, found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_IDBDATABASE_METADATA);
    return s;
  }
  if (!*found) {
    return Status::OK();
  }

  s = GetVarInt(db_.get(),
                DatabaseMetaDataKey::Encode(metadata->id,
                                            DatabaseMetaDataKey::USER_VERSION),
                &metadata->version, found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_IDBDATABASE_METADATA);
    return s;
  }
  if (!*found) {
    INTERNAL_CONSISTENCY_ERROR(GET_IDBDATABASE_METADATA);
    return InternalInconsistencyStatus();
  }

  if (metadata->version == IndexedDBDatabaseMetadata::DEFAULT_VERSION) {
    metadata->version = IndexedDBDatabaseMetadata::NO_VERSION;
  }

  s = GetMaxObjectStoreId(db_.get(), metadata->id,
                          &metadata->max_object_store_id);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_IDBDATABASE_METADATA);
  }

  // We don't cache this, we just check it if it's there.
  int64_t blob_number_generator_current_number =
      DatabaseMetaDataKey::kInvalidBlobNumber;

  s = GetVarInt(
      db_.get(),
      DatabaseMetaDataKey::Encode(
          metadata->id, DatabaseMetaDataKey::BLOB_KEY_GENERATOR_CURRENT_NUMBER),
      &blob_number_generator_current_number, found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_IDBDATABASE_METADATA);
    return s;
  }
  if (!*found) {
    // This database predates blob support.
    *found = true;
  } else if (!DatabaseMetaDataKey::IsValidBlobNumber(
                 blob_number_generator_current_number)) {
    INTERNAL_CONSISTENCY_ERROR(GET_IDBDATABASE_METADATA);
    return InternalInconsistencyStatus();
  }

  return ReadObjectStores(db_.get(), metadata->id, &metadata->object_stores);
}

BackingStore::Cursor::Cursor(
    const BackingStore::Cursor* other,
    std::unique_ptr<TransactionalLevelDBIterator> iterator)
    : transaction_(other->transaction_),
      database_id_(other->database_id_),
      cursor_options_(other->cursor_options_),
      iterator_(std::move(iterator)),
      current_key_(std::make_unique<IndexedDBKey>(*other->current_key_)) {
  DCHECK(transaction_);
  DCHECK(iterator_);
}

BackingStore::Cursor::Cursor(base::WeakPtr<Transaction> transaction,
                             int64_t database_id,
                             const CursorOptions& cursor_options)
    : transaction_(std::move(transaction)),
      database_id_(database_id),
      cursor_options_(cursor_options) {
  DCHECK(transaction_);
}

BackingStore::Cursor::~Cursor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<TransactionalLevelDBIterator>
BackingStore::Cursor::CloneIterator(const BackingStore::Cursor* other) {
  if (!other) {
    return nullptr;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(other->sequence_checker_);
  if (!other->iterator_) {
    return nullptr;
  }

  Status s;
  std::unique_ptr<TransactionalLevelDBIterator> it;
  std::tie(it, s) =
      CreateIteratorAndGetStatus(*other->transaction_->transaction());
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(CREATE_ITERATOR);
    return nullptr;
  }

  if (other->iterator_->IsValid()) {
    s = it->Seek(other->iterator_->Key());
    // TODO(cmumford): Handle this error (crbug.com/363397)
    DCHECK(it->IsValid());
  }

  return it;
}

bool BackingStore::Cursor::FirstSeek(Status* s) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(transaction_);
  DCHECK(s);
  std::tie(iterator_, *s) =
      CreateIteratorAndGetStatus(*transaction_->transaction());
  if (!s->ok()) {
    INTERNAL_WRITE_ERROR(CREATE_ITERATOR);
    return false;
  }

  {
    TRACE_EVENT0("IndexedDB", "BackingStore::Cursor::FirstSeek::Seek");
    if (cursor_options_.forward) {
      *s = iterator_->Seek(cursor_options_.low_key);
    } else {
      *s = iterator_->Seek(cursor_options_.high_key);
    }
    if (!s->ok()) {
      return false;
    }
  }
  return Continue(nullptr, READY, s);
}

bool BackingStore::Cursor::Advance(uint32_t count, Status* s) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  *s = Status::OK();
  while (count--) {
    if (!Continue(s)) {
      return false;
    }
  }
  return true;
}

bool BackingStore::Cursor::Continue(const IndexedDBKey* key,
                                    const IndexedDBKey* primary_key,
                                    IteratorState next_state,
                                    Status* s) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IndexedDB", "BackingStore::Cursor::Continue");
  DCHECK(!key || next_state == SEEK);

  if (cursor_options_.forward) {
    return ContinueNext(key, primary_key, next_state, s) ==
           ContinueResult::DONE;
  } else {
    return ContinuePrevious(key, primary_key, next_state, s) ==
           ContinueResult::DONE;
  }
}

BackingStore::Cursor::ContinueResult BackingStore::Cursor::ContinueNext(
    const IndexedDBKey* key,
    const IndexedDBKey* primary_key,
    IteratorState next_state,
    Status* s) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cursor_options_.forward);
  DCHECK(!key || key->IsValid());
  DCHECK(!primary_key || primary_key->IsValid());
  *s = Status::OK();

  // TODO(alecflett): avoid a copy here?
  IndexedDBKey previous_key = current_key_ ? *current_key_ : IndexedDBKey();

  // If seeking to a particular key (or key and primary key), skip the cursor
  // forward rather than iterating it.
  if (next_state == SEEK && key) {
    std::string leveldb_key =
        primary_key ? EncodeKey(*key, *primary_key) : EncodeKey(*key);
    *s = iterator_->Seek(leveldb_key);
    if (!s->ok()) {
      return ContinueResult::LEVELDB_ERROR;
    }
    // Cursor is at the next value already; don't advance it again below.
    next_state = READY;
  }

  for (;;) {
    // Only advance the cursor if it was not set to position already, either
    // because it is newly opened (and positioned at start of range) or
    // skipped forward by continue with a specific key.
    if (next_state == SEEK) {
      *s = iterator_->Next();
      if (!s->ok()) {
        return ContinueResult::LEVELDB_ERROR;
      }
    } else {
      next_state = SEEK;
    }

    // Fail if we've run out of data or gone past the cursor's bounds.
    if (!iterator_->IsValid() || IsPastBounds()) {
      return ContinueResult::OUT_OF_BOUNDS;
    }

    // TODO(jsbell): Document why this might be false. When do we ever not
    // seek into the range before starting cursor iteration?
    if (!HaveEnteredRange()) {
      continue;
    }

    // The row may not load because there's a stale entry in the index. If no
    // error then not fatal.
    if (!LoadCurrentRow(s)) {
      if (!s->ok()) {
        return ContinueResult::LEVELDB_ERROR;
      }
      continue;
    }

    // Cursor is now positioned at a non-stale record in range.

    // "Unique" cursors should continue seeking until a new key value is seen.
    if (cursor_options_.unique && previous_key.IsValid() &&
        current_key_->Equals(previous_key)) {
      continue;
    }

    break;
  }

  return ContinueResult::DONE;
}

BackingStore::Cursor::ContinueResult BackingStore::Cursor::ContinuePrevious(
    const IndexedDBKey* key,
    const IndexedDBKey* primary_key,
    IteratorState next_state,
    Status* s) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!cursor_options_.forward);
  DCHECK(!key || key->IsValid());
  DCHECK(!primary_key || primary_key->IsValid());
  *s = Status::OK();

  // TODO(alecflett): avoid a copy here?
  IndexedDBKey previous_key = current_key_ ? *current_key_ : IndexedDBKey();

  // When iterating with PrevNoDuplicate, spec requires that the value we
  // yield for each key is the *first* duplicate in forwards order. We do this
  // by remembering the duplicate key (implicitly, the first record seen with
  // a new key), keeping track of the earliest duplicate seen, and continuing
  // until yet another new key is seen, at which point the earliest duplicate
  // is the correct cursor position.
  IndexedDBKey duplicate_key;
  std::string earliest_duplicate;

  // TODO(jsbell): Optimize continuing to a specific key (or key and primary
  // key) for reverse cursors as well. See Seek() optimization at the start of
  // ContinueNext() for an example.

  for (;;) {
    if (next_state == SEEK) {
      *s = iterator_->Prev();
      if (!s->ok()) {
        return ContinueResult::LEVELDB_ERROR;
      }
    } else {
      next_state = SEEK;  // for subsequent iterations
    }

    // If we've run out of data or gone past the cursor's bounds.
    if (!iterator_->IsValid() || IsPastBounds()) {
      if (duplicate_key.IsValid()) {
        break;
      }
      return ContinueResult::OUT_OF_BOUNDS;
    }

    // TODO(jsbell): Document why this might be false. When do we ever not
    // seek into the range before starting cursor iteration?
    if (!HaveEnteredRange()) {
      continue;
    }

    // The row may not load because there's a stale entry in the index. If no
    // error then not fatal.
    if (!LoadCurrentRow(s)) {
      if (!s->ok()) {
        return ContinueResult::LEVELDB_ERROR;
      }
      continue;
    }

    // If seeking to a key (or key and primary key), continue until found.
    // TODO(jsbell): If Seek() optimization is added above, remove this.
    if (key) {
      if (primary_key && key->Equals(*current_key_) &&
          primary_key->IsLessThan(this->primary_key())) {
        continue;
      }
      if (key->IsLessThan(*current_key_)) {
        continue;
      }
    }

    // Cursor is now positioned at a non-stale record in range.

    if (cursor_options_.unique) {
      // If entry is a duplicate of the previous, keep going. Although the
      // cursor should be positioned at the first duplicate already, new
      // duplicates may have been inserted since the cursor was last iterated,
      // and should be skipped to maintain "unique" iteration.
      if (previous_key.IsValid() && current_key_->Equals(previous_key)) {
        continue;
      }

      // If we've found a new key, remember it and keep going.
      if (!duplicate_key.IsValid()) {
        duplicate_key = *current_key_;
        earliest_duplicate = std::string(iterator_->Key());
        continue;
      }

      // If we're still seeing duplicates, keep going.
      if (duplicate_key.Equals(*current_key_)) {
        earliest_duplicate = std::string(iterator_->Key());
        continue;
      }
    }

    break;
  }

  if (cursor_options_.unique) {
    DCHECK(duplicate_key.IsValid());
    DCHECK(!earliest_duplicate.empty());

    *s = iterator_->Seek(earliest_duplicate);
    if (!s->ok()) {
      return ContinueResult::LEVELDB_ERROR;
    }
    if (!LoadCurrentRow(s)) {
      DCHECK(!s->ok());
      return ContinueResult::LEVELDB_ERROR;
    }
  }

  return ContinueResult::DONE;
}

bool BackingStore::Cursor::HaveEnteredRange() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (cursor_options_.forward) {
    int compare = CompareIndexKeys(iterator_->Key(), cursor_options_.low_key);
    if (cursor_options_.low_open) {
      return compare > 0;
    }
    return compare >= 0;
  }
  int compare = CompareIndexKeys(iterator_->Key(), cursor_options_.high_key);
  if (cursor_options_.high_open) {
    return compare < 0;
  }
  return compare <= 0;
}

bool BackingStore::Cursor::IsPastBounds() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (cursor_options_.forward) {
    int compare = CompareIndexKeys(iterator_->Key(), cursor_options_.high_key);
    if (cursor_options_.high_open) {
      return compare >= 0;
    }
    return compare > 0;
  }
  int compare = CompareIndexKeys(iterator_->Key(), cursor_options_.low_key);
  if (cursor_options_.low_open) {
    return compare <= 0;
  }
  return compare < 0;
}

const IndexedDBKey& BackingStore::Cursor::primary_key() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *current_key_;
}

class ObjectStoreKeyCursorImpl : public BackingStore::Cursor {
 public:
  ObjectStoreKeyCursorImpl(
      base::WeakPtr<BackingStore::Transaction> transaction,
      int64_t database_id,
      const BackingStore::Cursor::CursorOptions& cursor_options)
      : BackingStore::Cursor(std::move(transaction),
                             database_id,
                             cursor_options) {}

  ObjectStoreKeyCursorImpl(const ObjectStoreKeyCursorImpl&) = delete;
  ObjectStoreKeyCursorImpl& operator=(const ObjectStoreKeyCursorImpl&) = delete;

  std::unique_ptr<Cursor> Clone() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto iter = CloneIterator(this);
    if (!iter) {
      return nullptr;
    }
    return base::WrapUnique(
        new ObjectStoreKeyCursorImpl(this, std::move(iter)));
  }

  // BackingStore::Cursor
  IndexedDBValue* value() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  bool LoadCurrentRow(Status* s) override;

 protected:
  std::string EncodeKey(const IndexedDBKey& key) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ObjectStoreDataKey::Encode(cursor_options_.database_id,
                                      cursor_options_.object_store_id, key);
  }
  std::string EncodeKey(const IndexedDBKey& key,
                        const IndexedDBKey& primary_key) override {
    NOTREACHED_IN_MIGRATION();
    return std::string();
  }

 private:
  explicit ObjectStoreKeyCursorImpl(
      const ObjectStoreKeyCursorImpl* other,
      std::unique_ptr<TransactionalLevelDBIterator> iterator)
      : BackingStore::Cursor(other, std::move(iterator)) {}
};

BackingStore::Cursor::CursorOptions::CursorOptions() = default;

BackingStore::Cursor::CursorOptions::CursorOptions(const CursorOptions& other) =
    default;

BackingStore::Cursor::CursorOptions::~CursorOptions() = default;

const BackingStore::RecordIdentifier& BackingStore::Cursor::record_identifier()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return record_identifier_;
}

bool ObjectStoreKeyCursorImpl::LoadCurrentRow(Status* s) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string_view slice(iterator_->Key());
  ObjectStoreDataKey object_store_data_key;
  if (!ObjectStoreDataKey::Decode(&slice, &object_store_data_key)) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    *s = InvalidDBKeyStatus();
    return false;
  }

  current_key_ = object_store_data_key.user_key();

  int64_t version;
  slice = std::string_view(iterator_->Value());
  if (!DecodeVarInt(&slice, &version)) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    *s = InternalInconsistencyStatus();
    return false;
  }

  // TODO(jsbell): This re-encodes what was just decoded; try and optimize.
  std::string encoded_key;
  EncodeIDBKey(*current_key_, &encoded_key);
  record_identifier_.Reset(encoded_key, version);

  return true;
}

class ObjectStoreCursorImpl : public BackingStore::Cursor {
 public:
  ObjectStoreCursorImpl(
      base::WeakPtr<BackingStore::Transaction> transaction,
      int64_t database_id,
      const BackingStore::Cursor::CursorOptions& cursor_options)
      : BackingStore::Cursor(std::move(transaction),
                             database_id,
                             cursor_options) {}

  ObjectStoreCursorImpl(const ObjectStoreCursorImpl&) = delete;
  ObjectStoreCursorImpl& operator=(const ObjectStoreCursorImpl&) = delete;

  ~ObjectStoreCursorImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // BackingStore::Cursor:

  std::unique_ptr<Cursor> Clone() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto iter = CloneIterator(this);
    if (!iter) {
      return nullptr;
    }
    return base::WrapUnique(new ObjectStoreCursorImpl(this, std::move(iter)));
  }

  IndexedDBValue* value() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return &current_value_;
  }
  bool LoadCurrentRow(Status* s) override;

 protected:
  std::string EncodeKey(const IndexedDBKey& key) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ObjectStoreDataKey::Encode(cursor_options_.database_id,
                                      cursor_options_.object_store_id, key);
  }
  std::string EncodeKey(const IndexedDBKey& key,
                        const IndexedDBKey& primary_key) override {
    NOTREACHED_IN_MIGRATION();
    return std::string();
  }

 private:
  explicit ObjectStoreCursorImpl(
      const ObjectStoreCursorImpl* other,
      std::unique_ptr<TransactionalLevelDBIterator> iterator)
      : BackingStore::Cursor(other, std::move(iterator)) {}

  IndexedDBValue current_value_ GUARDED_BY_CONTEXT(sequence_checker_);
};

bool ObjectStoreCursorImpl::LoadCurrentRow(Status* s) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(transaction_);

  std::string_view key_slice(iterator_->Key());
  ObjectStoreDataKey object_store_data_key;
  if (!ObjectStoreDataKey::Decode(&key_slice, &object_store_data_key)) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    *s = InvalidDBKeyStatus();
    return false;
  }

  current_key_ = object_store_data_key.user_key();

  int64_t version;
  std::string_view value_slice = std::string_view(iterator_->Value());
  if (!DecodeVarInt(&value_slice, &version)) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    *s = InternalInconsistencyStatus();
    return false;
  }

  // TODO(jsbell): This re-encodes what was just decoded; try and optimize.
  std::string encoded_key;
  EncodeIDBKey(*current_key_, &encoded_key);
  record_identifier_.Reset(encoded_key, version);

  *s = transaction_->GetExternalObjectsForRecord(
      database_id_, std::string(iterator_->Key()), &current_value_);
  if (!s->ok()) {
    return false;
  }

  current_value_.bits.assign(value_slice.begin(), value_slice.end());
  return true;
}

class IndexKeyCursorImpl : public BackingStore::Cursor {
 public:
  IndexKeyCursorImpl(base::WeakPtr<BackingStore::Transaction> transaction,
                     int64_t database_id,
                     const BackingStore::Cursor::CursorOptions& cursor_options)
      : BackingStore::Cursor(std::move(transaction),
                             database_id,
                             cursor_options) {}

  IndexKeyCursorImpl(const IndexKeyCursorImpl&) = delete;
  IndexKeyCursorImpl& operator=(const IndexKeyCursorImpl&) = delete;

  ~IndexKeyCursorImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  std::unique_ptr<Cursor> Clone() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto iter = CloneIterator(this);
    if (!iter) {
      return nullptr;
    }
    return base::WrapUnique(new IndexKeyCursorImpl(this, std::move(iter)));
  }

  // BackingStore::Cursor
  IndexedDBValue* value() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  const IndexedDBKey& primary_key() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return *primary_key_;
  }
  const BackingStore::RecordIdentifier& record_identifier() const override {
    NOTREACHED_IN_MIGRATION();
    return record_identifier_;
  }
  bool LoadCurrentRow(Status* s) override;

 protected:
  std::string EncodeKey(const IndexedDBKey& key) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return IndexDataKey::Encode(cursor_options_.database_id,
                                cursor_options_.object_store_id,
                                cursor_options_.index_id, key);
  }
  std::string EncodeKey(const IndexedDBKey& key,
                        const IndexedDBKey& primary_key) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return IndexDataKey::Encode(cursor_options_.database_id,
                                cursor_options_.object_store_id,
                                cursor_options_.index_id, key, primary_key);
  }

 private:
  explicit IndexKeyCursorImpl(
      const IndexKeyCursorImpl* other,
      std::unique_ptr<TransactionalLevelDBIterator> iterator)
      : BackingStore::Cursor(other, std::move(iterator)),
        primary_key_(std::make_unique<IndexedDBKey>(*other->primary_key_)) {}

  std::unique_ptr<IndexedDBKey> primary_key_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

bool IndexKeyCursorImpl::LoadCurrentRow(Status* s) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(transaction_);

  std::string_view slice(iterator_->Key());
  IndexDataKey index_data_key;
  if (!IndexDataKey::Decode(&slice, &index_data_key)) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    *s = InvalidDBKeyStatus();
    return false;
  }

  current_key_ = index_data_key.user_key();
  DCHECK(current_key_);

  slice = std::string_view(iterator_->Value());
  int64_t index_data_version;
  if (!DecodeVarInt(&slice, &index_data_version)) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    *s = InternalInconsistencyStatus();
    return false;
  }

  if (!DecodeIDBKey(&slice, &primary_key_) || !slice.empty()) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    *s = InternalInconsistencyStatus();
    return false;
  }

  std::string primary_leveldb_key =
      ObjectStoreDataKey::Encode(index_data_key.DatabaseId(),
                                 index_data_key.ObjectStoreId(), *primary_key_);

  std::string result;
  bool found = false;
  *s = transaction_->transaction()->Get(primary_leveldb_key, &result, &found);
  if (!s->ok()) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    return false;
  }
  if (!found) {
    // If the version numbers don't match, that means this is an obsolete index
    // entry (a 'tombstone') that can be cleaned up. This removal can only
    // happen in non-read-only transactions.
    if (cursor_options_.mode != blink::mojom::IDBTransactionMode::ReadOnly) {
      *s = transaction_->transaction()->Remove(iterator_->Key());
    }
    return false;
  }
  if (result.empty()) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    return false;
  }

  int64_t object_store_data_version;
  slice = std::string_view(result);
  if (!DecodeVarInt(&slice, &object_store_data_version)) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    *s = InternalInconsistencyStatus();
    return false;
  }

  if (object_store_data_version != index_data_version) {
    *s = transaction_->transaction()->Remove(iterator_->Key());
    return false;
  }

  return true;
}

class IndexCursorImpl : public BackingStore::Cursor {
 public:
  IndexCursorImpl(base::WeakPtr<BackingStore::Transaction> transaction,
                  int64_t database_id,
                  const BackingStore::Cursor::CursorOptions& cursor_options)
      : BackingStore::Cursor(std::move(transaction),
                             database_id,
                             cursor_options) {}

  IndexCursorImpl(const IndexCursorImpl&) = delete;
  IndexCursorImpl& operator=(const IndexCursorImpl&) = delete;

  ~IndexCursorImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  std::unique_ptr<Cursor> Clone() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto iter = CloneIterator(this);
    if (!iter) {
      return nullptr;
    }
    return base::WrapUnique(new IndexCursorImpl(this, std::move(iter)));
  }

  // BackingStore::Cursor
  IndexedDBValue* value() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return &current_value_;
  }
  const IndexedDBKey& primary_key() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return *primary_key_;
  }
  const BackingStore::RecordIdentifier& record_identifier() const override {
    NOTREACHED_IN_MIGRATION();
    return record_identifier_;
  }
  bool LoadCurrentRow(Status* s) override;

 protected:
  std::string EncodeKey(const IndexedDBKey& key) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return IndexDataKey::Encode(cursor_options_.database_id,
                                cursor_options_.object_store_id,
                                cursor_options_.index_id, key);
  }
  std::string EncodeKey(const IndexedDBKey& key,
                        const IndexedDBKey& primary_key) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return IndexDataKey::Encode(cursor_options_.database_id,
                                cursor_options_.object_store_id,
                                cursor_options_.index_id, key, primary_key);
  }

 private:
  explicit IndexCursorImpl(
      const IndexCursorImpl* other,
      std::unique_ptr<TransactionalLevelDBIterator> iterator)
      : BackingStore::Cursor(other, std::move(iterator)),
        primary_key_(std::make_unique<IndexedDBKey>(*other->primary_key_)),
        current_value_(other->current_value_),
        primary_leveldb_key_(other->primary_leveldb_key_) {}

  std::unique_ptr<IndexedDBKey> primary_key_
      GUARDED_BY_CONTEXT(sequence_checker_);
  IndexedDBValue current_value_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string primary_leveldb_key_ GUARDED_BY_CONTEXT(sequence_checker_);
};

bool IndexCursorImpl::LoadCurrentRow(Status* s) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(transaction_);

  std::string_view slice(iterator_->Key());
  IndexDataKey index_data_key;
  if (!IndexDataKey::Decode(&slice, &index_data_key)) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    *s = InvalidDBKeyStatus();
    return false;
  }

  current_key_ = index_data_key.user_key();
  DCHECK(current_key_);

  slice = std::string_view(iterator_->Value());
  int64_t index_data_version;
  if (!DecodeVarInt(&slice, &index_data_version)) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    *s = InternalInconsistencyStatus();
    return false;
  }
  if (!DecodeIDBKey(&slice, &primary_key_)) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    *s = InvalidDBKeyStatus();
    return false;
  }

  DCHECK_EQ(index_data_key.DatabaseId(), database_id_);
  primary_leveldb_key_ =
      ObjectStoreDataKey::Encode(index_data_key.DatabaseId(),
                                 index_data_key.ObjectStoreId(), *primary_key_);

  std::string result;
  bool found = false;
  *s = transaction_->transaction()->Get(primary_leveldb_key_, &result, &found);
  if (!s->ok()) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    return false;
  }
  if (!found) {
    // If the version numbers don't match, that means this is an obsolete index
    // entry (a 'tombstone') that can be cleaned up. This removal can only
    // happen in non-read-only transactions.
    if (cursor_options_.mode != blink::mojom::IDBTransactionMode::ReadOnly) {
      *s = transaction_->transaction()->Remove(iterator_->Key());
    }
    return false;
  }
  if (result.empty()) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    return false;
  }

  int64_t object_store_data_version;
  slice = std::string_view(result);
  if (!DecodeVarInt(&slice, &object_store_data_version)) {
    INTERNAL_READ_ERROR(LOAD_CURRENT_ROW);
    *s = InternalInconsistencyStatus();
    return false;
  }

  if (object_store_data_version != index_data_version) {
    // If the version numbers don't match, that means this is an obsolete index
    // entry (a 'tombstone') that can be cleaned up. This removal can only
    // happen in non-read-only transactions.
    if (cursor_options_.mode != blink::mojom::IDBTransactionMode::ReadOnly) {
      *s = transaction_->transaction()->Remove(iterator_->Key());
    }
    return false;
  }

  current_value_.bits.assign(slice.begin(), slice.end());
  *s = transaction_->GetExternalObjectsForRecord(
      database_id_, primary_leveldb_key_, &current_value_);
  return s->ok();
}

std::unique_ptr<BackingStore::Cursor> BackingStore::OpenObjectStoreCursor(
    BackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& range,
    blink::mojom::IDBCursorDirection direction,
    Status* s) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IndexedDB", "BackingStore::OpenObjectStoreCursor");

  TransactionalLevelDBTransaction* leveldb_transaction =
      transaction->transaction();
  BackingStore::Cursor::CursorOptions cursor_options;
  cursor_options.mode = transaction->mode();
  // TODO(cmumford): Handle this error (crbug.com/363397)
  if (!ObjectStoreCursorOptions(leveldb_transaction, database_id,
                                object_store_id, range, direction,
                                &cursor_options, s)) {
    return nullptr;
  }
  std::unique_ptr<ObjectStoreCursorImpl> cursor(
      std::make_unique<ObjectStoreCursorImpl>(transaction->AsWeakPtr(),
                                              database_id, cursor_options));
  if (!cursor->FirstSeek(s)) {
    return nullptr;
  }

  return std::move(cursor);
}

std::unique_ptr<BackingStore::Cursor> BackingStore::OpenObjectStoreKeyCursor(
    BackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& range,
    blink::mojom::IDBCursorDirection direction,
    Status* s) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IndexedDB", "BackingStore::OpenObjectStoreKeyCursor");

  TransactionalLevelDBTransaction* leveldb_transaction =
      transaction->transaction();
  BackingStore::Cursor::CursorOptions cursor_options;
  cursor_options.mode = transaction->mode();
  // TODO(cmumford): Handle this error (crbug.com/363397)
  if (!ObjectStoreCursorOptions(leveldb_transaction, database_id,
                                object_store_id, range, direction,
                                &cursor_options, s)) {
    return nullptr;
  }
  std::unique_ptr<ObjectStoreKeyCursorImpl> cursor(
      std::make_unique<ObjectStoreKeyCursorImpl>(transaction->AsWeakPtr(),
                                                 database_id, cursor_options));
  if (!cursor->FirstSeek(s)) {
    return nullptr;
  }

  return std::move(cursor);
}

std::unique_ptr<BackingStore::Cursor> BackingStore::OpenIndexKeyCursor(
    BackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& range,
    blink::mojom::IDBCursorDirection direction,
    Status* s) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IndexedDB", "BackingStore::OpenIndexKeyCursor");
  *s = Status::OK();
  TransactionalLevelDBTransaction* leveldb_transaction =
      transaction->transaction();
  BackingStore::Cursor::CursorOptions cursor_options;
  cursor_options.mode = transaction->mode();
  if (!IndexCursorOptions(leveldb_transaction, database_id, object_store_id,
                          index_id, range, direction, &cursor_options, s)) {
    return nullptr;
  }
  std::unique_ptr<IndexKeyCursorImpl> cursor(
      std::make_unique<IndexKeyCursorImpl>(transaction->AsWeakPtr(),
                                           database_id, cursor_options));
  if (!cursor->FirstSeek(s)) {
    return nullptr;
  }

  return std::move(cursor);
}

std::unique_ptr<BackingStore::Cursor> BackingStore::OpenIndexCursor(
    BackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& range,
    blink::mojom::IDBCursorDirection direction,
    Status* s) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IndexedDB", "BackingStore::OpenIndexCursor");
  TransactionalLevelDBTransaction* leveldb_transaction =
      transaction->transaction();
  BackingStore::Cursor::CursorOptions cursor_options;
  cursor_options.mode = transaction->mode();
  if (!IndexCursorOptions(leveldb_transaction, database_id, object_store_id,
                          index_id, range, direction, &cursor_options, s)) {
    return nullptr;
  }
  auto cursor = std::make_unique<IndexCursorImpl>(transaction->AsWeakPtr(),
                                                  database_id, cursor_options);
  if (!cursor->FirstSeek(s)) {
    return nullptr;
  }

  return cursor;
}

void BackingStore::WriteToIndexedDBForTesting(const std::string& key,
                                              const std::string& value) {
  std::string value_copy = value;
  Status s(db_->Put(key, &value_copy));
  CHECK(s.ok()) << s.ToString();
}

bool BackingStore::IsBlobCleanupPending() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return journal_cleaning_timer_.IsRunning();
}

void BackingStore::ForceRunBlobCleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  journal_cleaning_timer_.FireNow();
}

BackingStore::Transaction::BlobWriteState::BlobWriteState() = default;

BackingStore::Transaction::BlobWriteState::BlobWriteState(
    int calls_left,
    BlobWriteCallback on_complete)
    : calls_left(calls_left), on_complete(std::move(on_complete)) {}

BackingStore::Transaction::BlobWriteState::~BlobWriteState() = default;

// `backing_store` can be null in unittests (see FakeTransaction).
BackingStore::Transaction::Transaction(
    base::WeakPtr<BackingStore> backing_store,
    blink::mojom::IDBTransactionDurability durability,
    blink::mojom::IDBTransactionMode mode)
    : backing_store_(std::move(backing_store)),
      durability_(durability),
      mode_(mode) {
  // `Default` should have already been converted to the bucket's setting.
  DCHECK(durability_ != blink::mojom::IDBTransactionDurability::Default);
  DCHECK(!backing_store_ ||
         backing_store_->idb_task_runner()->RunsTasksInCurrentSequence());
}

BackingStore::Transaction::~Transaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!committing_);
}

void BackingStore::Transaction::Begin(std::vector<PartitionedLock> locks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backing_store_->sequence_checker_);
  DCHECK(backing_store_);
  DCHECK(!transaction_.get());
  TRACE_EVENT0("IndexedDB", "BackingStore::Transaction::Begin");

  transaction_ =
      backing_store_->transactional_leveldb_factory().CreateLevelDBTransaction(
          backing_store_->db_.get(),
          backing_store_->db_->scopes()->CreateScope(std::move(locks)));

  // If in_memory, this snapshots blobs just as the above transaction_
  // constructor snapshots the leveldb.
  for (const auto& iter : backing_store_->in_memory_external_object_map_) {
    in_memory_external_object_map_[iter.first] = iter.second->Clone();
  }
}

Status BackingStore::MigrateToV4(LevelDBWriteBatch* write_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int64_t db_schema_version = 4;
  const std::string schema_version_key = SchemaVersionKey::Encode();
  Status s;

  std::vector<base::FilePath> empty_blobs_to_delete;
  s = UpgradeBlobEntriesToV4(write_batch, &empty_blobs_to_delete);
  if (!s.ok()) {
    INTERNAL_CONSISTENCY_ERROR(SET_UP_METADATA);
    return InternalInconsistencyStatus();
  }
  std::ignore = PutInt(write_batch, schema_version_key, db_schema_version);

  // Delete all empty files that resulted from the migration to v4. If this
  // fails it's not a big deal.
  for (const auto& path : empty_blobs_to_delete) {
    base::DeleteFile(path);
  }

  return s;
}

Status BackingStore::MigrateToV5(LevelDBWriteBatch* write_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Some blob files were not written to disk due to a bug.
  // Validate that all blob files in the db exist on disk,
  // and return InternalInconsistencyStatus if any do not.
  // See http://crbug.com/1131151 for more details.
  const int64_t db_schema_version = 5;
  const std::string schema_version_key = SchemaVersionKey::Encode();
  Status s;

  if (bucket_locator_.storage_key.origin().host() != "docs.google.com") {
    s = ValidateBlobFiles();
    if (!s.ok()) {
      INTERNAL_CONSISTENCY_ERROR(SET_UP_METADATA);
      return InternalInconsistencyStatus();
    }
  }
  std::ignore = PutInt(write_batch, schema_version_key, db_schema_version);

  return s;
}

Status BackingStore::Transaction::HandleBlobPreTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backing_store_);
  DCHECK(blobs_to_write_.empty());

  if (backing_store_->in_memory()) {
    return Status::OK();
  }

  if (external_object_change_map_.empty()) {
    return Status::OK();
  }

  std::unique_ptr<LevelDBDirectTransaction> direct_txn =
      backing_store_->transactional_leveldb_factory()
          .CreateLevelDBDirectTransaction(backing_store_->db_.get());

  int64_t next_blob_number = -1;
  bool result = GetBlobNumberGeneratorCurrentNumber(
      direct_txn.get(), database_id_, &next_blob_number);
  if (!result || next_blob_number < 0) {
    return InternalInconsistencyStatus();
  }

  // Because blob numbers were not incremented on the correct transaction for
  // m78 and m79, they need to be checked. See https://crbug.com/1039446
  base::FilePath blob_path =
      backing_store_->GetBlobFileName(database_id_, next_blob_number);
  while (base::PathExists(blob_path)) {
    ++next_blob_number;
    blob_path = backing_store_->GetBlobFileName(database_id_, next_blob_number);
  }

  for (auto& iter : external_object_change_map_) {
    for (auto& entry : iter.second->mutable_external_objects()) {
      switch (entry.object_type()) {
        case IndexedDBExternalObject::ObjectType::kFile:
        case IndexedDBExternalObject::ObjectType::kBlob:
          blobs_to_write_.push_back({database_id_, next_blob_number});
          DCHECK(entry.is_remote_valid());
          entry.set_blob_number(next_blob_number);
          ++next_blob_number;
          result = UpdateBlobNumberGeneratorCurrentNumber(
              direct_txn.get(), database_id_, next_blob_number);
          if (!result) {
            return InternalInconsistencyStatus();
          }
          break;
        case IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle:
          break;
      }
    }
  }

  AppendBlobsToRecoveryBlobJournal(direct_txn.get(), blobs_to_write_);

  return Status(direct_txn->Commit());
}

bool BackingStore::Transaction::CollectBlobFilesToRemove() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backing_store_);

  if (backing_store_->in_memory()) {
    return true;
  }

  // Look up all old files to remove as part of the transaction, store their
  // names in blobs_to_remove_, and remove their old blob data entries.
  for (const auto& iter : external_object_change_map_) {
    BlobEntryKey blob_entry_key;
    std::string_view key_piece(iter.second->object_store_data_key());
    if (!BlobEntryKey::FromObjectStoreDataKey(&key_piece, &blob_entry_key)) {
      NOTREACHED_IN_MIGRATION();
      INTERNAL_WRITE_ERROR(TRANSACTION_COMMIT_METHOD);
      transaction_ = nullptr;
      return false;
    }
    if (database_id_ < 0) {
      database_id_ = blob_entry_key.database_id();
    } else {
      DCHECK_EQ(database_id_, blob_entry_key.database_id());
    }
    std::string blob_entry_key_bytes = blob_entry_key.Encode();
    bool found;
    std::string blob_entry_value_bytes;
    Status s(transaction_->Get(blob_entry_key_bytes, &blob_entry_value_bytes,
                               &found));
    if (s.ok() && found) {
      std::vector<IndexedDBExternalObject> external_objects;
      if (!DecodeExternalObjects(blob_entry_value_bytes, &external_objects)) {
        INTERNAL_READ_ERROR(TRANSACTION_COMMIT_METHOD);
        transaction_ = nullptr;
        return false;
      }
      for (const auto& blob : external_objects) {
        if (blob.object_type() != IndexedDBExternalObject::ObjectType::kBlob &&
            blob.object_type() != IndexedDBExternalObject::ObjectType::kFile) {
          continue;
        }
        blobs_to_remove_.push_back({database_id_, blob.blob_number()});
        s = transaction_->Remove(blob_entry_key_bytes);
        if (!s.ok()) {
          transaction_ = nullptr;
          return false;
        }
      }
    }
  }
  return true;
}

void BackingStore::Transaction::PartitionBlobsToRemove(
    BlobJournalType* inactive_blobs,
    BlobJournalType* active_blobs) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backing_store_);

  ActiveBlobRegistry* registry = backing_store_->active_blob_registry();
  for (const auto& iter : blobs_to_remove_) {
    bool is_blob_referenced = registry->MarkBlobInfoDeletedAndCheckIfReferenced(
        iter.first, iter.second);
    if (is_blob_referenced) {
      active_blobs->push_back(iter);
    } else {
      inactive_blobs->push_back(iter);
    }
  }
}

Status BackingStore::Transaction::CommitPhaseOne(BlobWriteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(transaction_.get());
  DCHECK(backing_store_);
  DCHECK(backing_store_->idb_task_runner()->RunsTasksInCurrentSequence());
  TRACE_EVENT0("IndexedDB", "BackingStore::Transaction::CommitPhaseOne");

  Status s;

  s = HandleBlobPreTransaction();
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(TRANSACTION_COMMIT_METHOD);
    transaction_ = nullptr;
    return s;
  }

  DCHECK(external_object_change_map_.empty() ||
         KeyPrefix::IsValidDatabaseId(database_id_));
  if (!CollectBlobFilesToRemove()) {
    INTERNAL_WRITE_ERROR(TRANSACTION_COMMIT_METHOD);
    transaction_ = nullptr;
    return InternalInconsistencyStatus();
  }

  committing_ = true;
  backing_store_->WillCommitTransaction();

  if (!external_object_change_map_.empty() && !backing_store_->in_memory()) {
    // This kicks off the writes of the new blobs, if any.
    return WriteNewBlobs(std::move(callback));
  } else {
    return std::move(callback).Run(
        BlobWriteResult::kRunPhaseTwoAndReturnResult,
        storage::mojom::WriteBlobToFileResult::kSuccess);
  }
}

Status BackingStore::Transaction::CommitPhaseTwo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backing_store_->sequence_checker_);
  DCHECK(backing_store_);
  TRACE_EVENT0("IndexedDB", "BackingStore::Transaction::CommitPhaseTwo");

  DCHECK(committing_);
  committing_ = false;

  Status s;

  // DidCommitTransaction must be called during CommitPhaseTwo,
  // as it decrements the number of active transactions that were
  // incremented from CommitPhaseOne.  However, it also potentially cleans up
  // the recovery blob journal, and so needs to be done after the newly
  // written blobs have been removed from the recovery journal further below.
  // As there are early outs in this function, use an RAII helper here.
  AutoDidCommitTransaction run_did_commit_transaction_on_return(
      backing_store_.get());

  BlobJournalType recovery_journal, active_journal, saved_recovery_journal,
      inactive_blobs;
  if (!external_object_change_map_.empty()) {
    if (!backing_store_->in_memory()) {
      for (auto& iter : external_object_change_map_) {
        BlobEntryKey blob_entry_key;
        std::string_view key_piece(iter.second->object_store_data_key());
        if (!BlobEntryKey::FromObjectStoreDataKey(&key_piece,
                                                  &blob_entry_key)) {
          NOTREACHED_IN_MIGRATION();
          return InternalInconsistencyStatus();
        }
        // Add the new blob-table entry for each blob to the main transaction,
        // or remove any entry that may exist if there's no new one.
        if (iter.second->external_objects().empty()) {
          s = transaction_->Remove(blob_entry_key.Encode());
        } else {
          std::string tmp =
              EncodeExternalObjects(iter.second->external_objects());
          s = transaction_->Put(blob_entry_key.Encode(), &tmp);
        }
        if (!s.ok()) {
          return s;
        }
      }
    }

    TRACE_EVENT0("IndexedDB", "BackingStore::Transaction.BlobJournal");
    // Read the persisted states of the recovery/live blob journals,
    // so that they can be updated correctly by the transaction.
    std::unique_ptr<LevelDBDirectTransaction> journal_transaction =
        backing_store_->transactional_leveldb_factory()
            .CreateLevelDBDirectTransaction(backing_store_->db_.get());
    s = GetRecoveryBlobJournal(journal_transaction.get(), &recovery_journal);
    if (!s.ok()) {
      return s;
    }
    s = GetActiveBlobJournal(journal_transaction.get(), &active_journal);
    if (!s.ok()) {
      return s;
    }

    // Remove newly added blobs from the journal - they will be accounted
    // for in blob entry tables in the transaction.
    std::sort(recovery_journal.begin(), recovery_journal.end());
    std::sort(blobs_to_write_.begin(), blobs_to_write_.end());
    BlobJournalType new_journal = base::STLSetDifference<BlobJournalType>(
        recovery_journal, blobs_to_write_);
    recovery_journal.swap(new_journal);

    // Append newly deleted blobs to appropriate recovery/active journals.
    saved_recovery_journal = recovery_journal;
    BlobJournalType active_blobs;
    if (!blobs_to_remove_.empty()) {
      DCHECK(!backing_store_->in_memory());
      PartitionBlobsToRemove(&inactive_blobs, &active_blobs);
    }
    recovery_journal.insert(recovery_journal.end(), inactive_blobs.begin(),
                            inactive_blobs.end());
    active_journal.insert(active_journal.end(), active_blobs.begin(),
                          active_blobs.end());
    s = UpdateRecoveryBlobJournal(transaction_.get(), recovery_journal);
    if (!s.ok()) {
      return s;
    }
    s = UpdateActiveBlobJournal(transaction_.get(), active_journal);
    if (!s.ok()) {
      return s;
    }
  }

  // Actually commit. If this succeeds, the journals will appropriately
  // reflect pending blob work - dead files that should be deleted
  // immediately, and live files to monitor.
  s = transaction_->Commit(BackingStore::ShouldSyncOnCommit(durability_));
  transaction_ = nullptr;

  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(TRANSACTION_COMMIT_METHOD);
    return s;
  }

  if (backing_store_->in_memory()) {
    if (!external_object_change_map_.empty()) {
      auto& target_map = backing_store_->in_memory_external_object_map_;
      for (auto& iter : external_object_change_map_) {
        auto target_record = target_map.find(iter.first);
        if (target_record != target_map.end()) {
          target_map.erase(target_record);
        }
        if (iter.second) {
          target_map[iter.first] = std::move(iter.second);
        }
      }
    }
    return Status::OK();
  }

  // Actually delete dead blob files, then remove those entries
  // from the persisted recovery journal.
  if (inactive_blobs.empty()) {
    return Status::OK();
  }

  DCHECK(!external_object_change_map_.empty());

  s = backing_store_->CleanUpBlobJournalEntries(inactive_blobs);
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(TRANSACTION_COMMIT_METHOD);
    return s;
  }

  std::unique_ptr<LevelDBDirectTransaction> update_journal_transaction =
      backing_store_->transactional_leveldb_factory()
          .CreateLevelDBDirectTransaction(backing_store_->db_.get());
  UpdateRecoveryBlobJournal(update_journal_transaction.get(),
                            saved_recovery_journal);
  s = update_journal_transaction->Commit();
  return s;
}

Status BackingStore::Transaction::WriteNewBlobs(BlobWriteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backing_store_);
  DCHECK(!backing_store_->in_memory());
  DCHECK(!external_object_change_map_.empty());
  DCHECK_GT(database_id_, 0);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "IndexedDB", "BackingStore::Transaction::WriteNewBlobs", this);

  // Count how many objects we need to write by excluding all empty files and
  // blobs.
  int num_objects_to_write = 0;
  for (const auto& iter : external_object_change_map_) {
    for (const auto& entry : iter.second->external_objects()) {
      switch (entry.object_type()) {
        case IndexedDBExternalObject::ObjectType::kFile:
        case IndexedDBExternalObject::ObjectType::kBlob:
          if (entry.size() != 0) {
            ++num_objects_to_write;
          }
          break;
        case IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle:
          if (entry.serialized_file_system_access_handle().empty()) {
            ++num_objects_to_write;
          }
          break;
      }
    }
  }
  if (num_objects_to_write == 0) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "IndexedDB", "BackingStore::Transaction::WriteNewBlobs", this);
    return std::move(callback).Run(
        BlobWriteResult::kRunPhaseTwoAndReturnResult,
        storage::mojom::WriteBlobToFileResult::kSuccess);
  }

  write_state_.emplace(num_objects_to_write, std::move(callback));

  auto write_result_callback = base::BindRepeating(
      [](base::WeakPtr<Transaction> transaction,
         storage::mojom::WriteBlobToFileResult result) {
        if (!transaction) {
          return;
        }
        DCHECK_CALLED_ON_VALID_SEQUENCE(transaction->sequence_checker_);

        // This can be null if Rollback() is called.
        if (!transaction->write_state_) {
          return;
        }
        auto& write_state = transaction->write_state_.value();
        DCHECK(!write_state.on_complete.is_null());
        if (result != storage::mojom::WriteBlobToFileResult::kSuccess) {
          auto on_complete = std::move(write_state.on_complete);
          transaction->write_state_.reset();
          TRACE_EVENT_NESTABLE_ASYNC_END0(
              "IndexedDB", "BackingStore::Transaction::WriteNewBlobs",
              transaction.get());
          std::move(on_complete).Run(BlobWriteResult::kFailure, result);
          return;
        }
        --(write_state.calls_left);
        if (write_state.calls_left == 0) {
          auto on_complete = std::move(write_state.on_complete);
          transaction->write_state_.reset();
          TRACE_EVENT_NESTABLE_ASYNC_END0(
              "IndexedDB", "BackingStore::Transaction::WriteNewBlobs",
              transaction.get());
          std::move(on_complete)
              .Run(BlobWriteResult::kRunPhaseTwoAsync, result);
        }
      },
      weak_ptr_factory_.GetWeakPtr());

  for (auto& iter : external_object_change_map_) {
    for (auto& entry : iter.second->mutable_external_objects()) {
      switch (entry.object_type()) {
        case IndexedDBExternalObject::ObjectType::kFile:
        case IndexedDBExternalObject::ObjectType::kBlob: {
          if (entry.size() == 0) {
            continue;
          }
          // If this directory creation fails then the WriteBlobToFile call
          // will fail. So there is no need to special-case handle it here.
          base::FilePath path = GetBlobDirectoryNameForKey(
              backing_store_->blob_path_, database_id_, entry.blob_number());
          base::CreateDirectory(path);
          // TODO(dmurph): Refactor IndexedDBExternalObject to not use a
          // SharedRemote, so this code can just move the remote, instead of
          // cloning.
          mojo::PendingRemote<blink::mojom::Blob> pending_blob;
          entry.remote()->Clone(pending_blob.InitWithNewPipeAndPassReceiver());

          // Android doesn't seem to consistently be able to set file
          // modification times. The timestamp is not checked during reading
          // on Android either. https://crbug.com/1045488
          std::optional<base::Time> last_modified;
#if !BUILDFLAG(IS_ANDROID)
          last_modified = entry.last_modified().is_null()
                              ? std::nullopt
                              : std::make_optional(entry.last_modified());
#endif
          backing_store_->bucket_context_->blob_storage_context()
              ->WriteBlobToFile(std::move(pending_blob),
                                backing_store_->GetBlobFileName(
                                    database_id_, entry.blob_number()),
                                BackingStore::ShouldSyncOnCommit(durability_),
                                last_modified, write_result_callback);
          break;
        }
        case IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle: {
          if (!entry.serialized_file_system_access_handle().empty()) {
            continue;
          }
          // TODO(dmurph): Refactor IndexedDBExternalObject to not use a
          // SharedRemote, so this code can just move the remote, instead of
          // cloning.
          mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
              token_clone;
          entry.file_system_access_token_remote()->Clone(
              token_clone.InitWithNewPipeAndPassReceiver());

          backing_store_->bucket_context_->file_system_access_context()
              ->SerializeHandle(
                  std::move(token_clone),
                  base::BindOnce(
                      [](base::WeakPtr<Transaction> transaction,
                         IndexedDBExternalObject* object,
                         base::OnceCallback<void(
                             storage::mojom::WriteBlobToFileResult)> callback,
                         const std::vector<uint8_t>& serialized_token) {
                        // `object` is owned by `transaction`, so make sure
                        // `transaction` is still valid before doing anything
                        // else.
                        if (!transaction) {
                          return;
                        }
                        if (serialized_token.empty()) {
                          std::move(callback).Run(
                              storage::mojom::WriteBlobToFileResult::kError);
                          return;
                        }
                        object->set_serialized_file_system_access_handle(
                            serialized_token);
                        std::move(callback).Run(
                            storage::mojom::WriteBlobToFileResult::kSuccess);
                      },
                      weak_ptr_factory_.GetWeakPtr(),
                      base::UnsafeDanglingUntriaged(&entry),
                      write_result_callback));
          break;
        }
      }
    }
  }
  return Status::OK();
}

void BackingStore::Transaction::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  backing_store_.reset();
  transaction_ = nullptr;
}

void BackingStore::Transaction::Rollback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backing_store_);
  TRACE_EVENT0("IndexedDB", "BackingStore::Transaction::Rollback");

  if (committing_) {
    committing_ = false;
    backing_store_->DidCommitTransaction();
  }

  write_state_.reset();

  if (transaction_) {
    // The RollbackAndMaybeTearDown method could tear down the BucketContext,
    // which would destroy `this`.
    scoped_refptr<TransactionalLevelDBTransaction> transaction =
        std::move(transaction_);
    transaction->Rollback();
  }
}

uint64_t BackingStore::Transaction::GetTransactionSize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(transaction_);
  return transaction_->GetTransactionSize();
}

Status BackingStore::Transaction::PutExternalObjectsIfNeeded(
    int64_t database_id,
    const std::string& object_store_data_key,
    std::vector<IndexedDBExternalObject>* external_objects) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!external_objects || external_objects->empty()) {
    external_object_change_map_.erase(object_store_data_key);
    in_memory_external_object_map_.erase(object_store_data_key);

    BlobEntryKey blob_entry_key;
    std::string_view leveldb_key_piece(object_store_data_key);
    if (!BlobEntryKey::FromObjectStoreDataKey(&leveldb_key_piece,
                                              &blob_entry_key)) {
      NOTREACHED_IN_MIGRATION();
      return InternalInconsistencyStatus();
    }
    std::string value;
    bool found = false;
    Status s(transaction()->Get(blob_entry_key.Encode(), &value, &found));
    if (!s.ok()) {
      return s;
    }
    if (!found) {
      return Status::OK();
    }
  }
  PutExternalObjects(database_id, object_store_data_key, external_objects);
  return Status::OK();
}

// This is storing an info, even if empty, even if the previous key had no blob
// info that we know of.  It duplicates a bunch of information stored in the
// leveldb transaction, but only w.r.t. the user keys altered--we don't keep the
// changes to exists or index keys here.
void BackingStore::Transaction::PutExternalObjects(
    int64_t database_id,
    const std::string& object_store_data_key,
    std::vector<IndexedDBExternalObject>* external_objects) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!object_store_data_key.empty());
  if (database_id_ < 0) {
    database_id_ = database_id;
  }
  DCHECK_EQ(database_id_, database_id);

  auto it = external_object_change_map_.find(object_store_data_key);
  IndexedDBExternalObjectChangeRecord* record = nullptr;
  if (it == external_object_change_map_.end()) {
    std::unique_ptr<IndexedDBExternalObjectChangeRecord> new_record =
        std::make_unique<IndexedDBExternalObjectChangeRecord>(
            object_store_data_key);
    record = new_record.get();
    external_object_change_map_[object_store_data_key] = std::move(new_record);
  } else {
    record = it->second.get();
  }
  record->SetExternalObjects(external_objects);
}

}  // namespace content::indexed_db
