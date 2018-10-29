// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_backing_store.h"

#include <algorithm>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/indexed_db/indexed_db_blob_info.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_metadata_coding.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/leveldb/leveldb_comparator.h"
#include "content/browser/indexed_db/leveldb/leveldb_database.h"
#include "content/browser/indexed_db/leveldb/leveldb_factory.h"
#include "content/browser/indexed_db/leveldb/leveldb_iterator.h"
#include "content/browser/indexed_db/leveldb/leveldb_transaction.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/fileapi/file_stream_writer.h"
#include "storage/browser/fileapi/file_writer_delegate.h"
#include "storage/browser/fileapi/local_file_stream_writer.h"
#include "storage/common/database/database_identifier.h"
#include "storage/common/fileapi/file_system_mount_option.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/leveldatabase/env_chromium.h"

using base::FilePath;
using base::StringPiece;
using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBKeyRange;
using leveldb::Status;
using storage::FileWriterDelegate;
using url::Origin;

namespace content {
using indexed_db::CheckObjectStoreAndMetaDataType;
using indexed_db::CheckIndexAndMetaDataKey;
using indexed_db::FindGreatestKeyLessThanOrEqual;
using indexed_db::GetInt;
using indexed_db::GetString;
using indexed_db::GetVarInt;
using indexed_db::HistogramOpenStatus;
using indexed_db::IOErrorStatus;
using indexed_db::InternalInconsistencyStatus;
using indexed_db::InvalidDBKeyStatus;
using indexed_db::PutBool;
using indexed_db::PutInt;
using indexed_db::PutVarInt;
using indexed_db::PutString;
using indexed_db::PutIDBKeyPath;

namespace {

FilePath GetBlobDirectoryName(const FilePath& path_base, int64_t database_id) {
  return path_base.AppendASCII(base::StringPrintf("%" PRIx64, database_id));
}

FilePath GetBlobDirectoryNameForKey(const FilePath& path_base,
                                    int64_t database_id,
                                    int64_t key) {
  FilePath path = GetBlobDirectoryName(path_base, database_id);
  path = path.AppendASCII(base::StringPrintf(
      "%02x", static_cast<int>(key & 0x000000000000ff00) >> 8));
  return path;
}

FilePath GetBlobFileNameForKey(const FilePath& path_base,
                               int64_t database_id,
                               int64_t key) {
  FilePath path = GetBlobDirectoryNameForKey(path_base, database_id, key);
  path = path.AppendASCII(base::StringPrintf("%" PRIx64, key));
  return path;
}

bool MakeIDBBlobDirectory(const FilePath& path_base,
                          int64_t database_id,
                          int64_t key) {
  FilePath path = GetBlobDirectoryNameForKey(path_base, database_id, key);
  return base::CreateDirectory(path);
}

std::string ComputeOriginIdentifier(const Origin& origin) {
  return storage::GetIdentifierFromOrigin(origin) + "@1";
}

FilePath ComputeCorruptionFileName(const Origin& origin) {
  return IndexedDBContextImpl::GetLevelDBFileName(origin).Append(
      FILE_PATH_LITERAL("corruption_info.json"));
}

WARN_UNUSED_RESULT bool IsSchemaKnown(LevelDBDatabase* db, bool* known) {
  int64_t db_schema_version = 0;
  bool found = false;
  Status s = GetInt(db, SchemaVersionKey::Encode(), &db_schema_version, &found);
  if (!s.ok())
    return false;
  if (!found) {
    *known = true;
    return true;
  }
  if (db_schema_version < 0)
    return false;  // Only corruption should cause this.
  if (db_schema_version > indexed_db::kLatestKnownSchemaVersion) {
    *known = false;
    return true;
  }

  int64_t raw_db_data_version = 0;
  s = GetInt(db, DataVersionKey::Encode(), &raw_db_data_version, &found);
  if (!s.ok())
    return false;
  if (!found) {
    *known = true;
    return true;
  }
  if (raw_db_data_version < 0)
    return false;  // Only corruption should cause this.

  *known = IndexedDBDataFormatVersion::GetCurrent().IsAtLeast(
      IndexedDBDataFormatVersion::Decode(raw_db_data_version));
  return true;
}

// TODO(ericu): Error recovery. If we persistently can't read the
// blob journal, the safe thing to do is to clear it and leak the blobs,
// though that may be costly. Still, database/directory deletion should always
// clean things up, and we can write an fsck that will do a full correction if
// need be.

// Read and decode the specified blob journal via the supplied transaction.
// The key must be either the primary journal key or live journal key.
template <typename TransactionType>
Status GetBlobJournal(const StringPiece& key,
                      TransactionType* transaction,
                      BlobJournalType* journal) {
  IDB_TRACE("IndexedDBBackingStore::GetBlobJournal");
  std::string data;
  bool found = false;
  Status s = transaction->Get(key, &data, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(READ_BLOB_JOURNAL);
    return s;
  }
  journal->clear();
  if (!found || data.empty())
    return Status::OK();
  StringPiece slice(data);
  if (!DecodeBlobJournal(&slice, journal)) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(DECODE_BLOB_JOURNAL);
    s = InternalInconsistencyStatus();
  }
  return s;
}

template <typename TransactionType>
Status GetPrimaryBlobJournal(TransactionType* transaction,
                             BlobJournalType* journal) {
  return GetBlobJournal(BlobJournalKey::Encode(), transaction, journal);
}

template <typename TransactionType>
Status GetLiveBlobJournal(TransactionType* transaction,
                          BlobJournalType* journal) {
  return GetBlobJournal(LiveBlobJournalKey::Encode(), transaction, journal);
}

// Clear the specified blob journal via the supplied transaction.
// The key must be either the primary journal key or live journal key.
template <typename TransactionType>
void ClearBlobJournal(TransactionType* transaction, const std::string& key) {
  transaction->Remove(key);
}

// Overwrite the specified blob journal via the supplied transaction.
// The key must be either the primary journal key or live journal key.
template <typename TransactionType>
void UpdateBlobJournal(TransactionType* transaction,
                       const std::string& key,
                       const BlobJournalType& journal) {
  std::string data;
  EncodeBlobJournal(journal, &data);
  transaction->Put(key, &data);
}

template <typename TransactionType>
void UpdatePrimaryBlobJournal(TransactionType* transaction,
                              const BlobJournalType& journal) {
  UpdateBlobJournal(transaction, BlobJournalKey::Encode(), journal);
}

template <typename TransactionType>
void UpdateLiveBlobJournal(TransactionType* transaction,
                           const BlobJournalType& journal) {
  UpdateBlobJournal(transaction, LiveBlobJournalKey::Encode(), journal);
}

// Append blobs to the specified blob journal via the supplied transaction.
// The key must be either the primary journal key or live journal key.
template <typename TransactionType>
Status AppendBlobsToBlobJournal(TransactionType* transaction,
                                const std::string& key,
                                const BlobJournalType& journal) {
  if (journal.empty())
    return Status::OK();
  BlobJournalType old_journal;
  Status s = GetBlobJournal(key, transaction, &old_journal);
  if (!s.ok())
    return s;
  old_journal.insert(old_journal.end(), journal.begin(), journal.end());
  UpdateBlobJournal(transaction, key, old_journal);
  return Status::OK();
}

template <typename TransactionType>
Status AppendBlobsToPrimaryBlobJournal(TransactionType* transaction,
                                       const BlobJournalType& journal) {
  return AppendBlobsToBlobJournal(transaction, BlobJournalKey::Encode(),
                                  journal);
}

template <typename TransactionType>
Status AppendBlobsToLiveBlobJournal(TransactionType* transaction,
                                    const BlobJournalType& journal) {
  return AppendBlobsToBlobJournal(transaction, LiveBlobJournalKey::Encode(),
                                  journal);
}

// Append a database to the specified blob journal via the supplied transaction.
// The key must be either the primary journal key or live journal key.
Status MergeDatabaseIntoBlobJournal(LevelDBDirectTransaction* transaction,
                                    const std::string& key,
                                    int64_t database_id) {
  IDB_TRACE("IndexedDBBackingStore::MergeDatabaseIntoBlobJournal");
  BlobJournalType journal;
  Status s = GetBlobJournal(key, transaction, &journal);
  if (!s.ok())
    return s;
  journal.push_back(
      std::make_pair(database_id, DatabaseMetaDataKey::kAllBlobsKey));
  UpdateBlobJournal(transaction, key, journal);
  return Status::OK();
}

Status MergeDatabaseIntoPrimaryBlobJournal(
    LevelDBDirectTransaction* leveldb_transaction,
    int64_t database_id) {
  return MergeDatabaseIntoBlobJournal(leveldb_transaction,
                                      BlobJournalKey::Encode(), database_id);
}

Status MergeDatabaseIntoLiveBlobJournal(
    LevelDBDirectTransaction* leveldb_transaction,
    int64_t database_id) {
  return MergeDatabaseIntoBlobJournal(
      leveldb_transaction, LiveBlobJournalKey::Encode(), database_id);
}

// Blob Data is encoded as a series of:
//   { is_file [bool], key [int64_t as varInt],
//     type [string-with-length, may be empty],
//     (for Blobs only) size [int64_t as varInt]
//     (for Files only) fileName [string-with-length]
//   }
// There is no length field; just read until you run out of data.
std::string EncodeBlobData(const std::vector<IndexedDBBlobInfo*>& blob_info) {
  std::string ret;
  for (const auto* info : blob_info) {
    EncodeBool(info->is_file(), &ret);
    EncodeVarInt(info->key(), &ret);
    EncodeStringWithLength(info->type(), &ret);
    if (info->is_file())
      EncodeStringWithLength(info->file_name(), &ret);
    else
      EncodeVarInt(info->size(), &ret);
  }
  return ret;
}

bool DecodeBlobData(const std::string& data,
                    std::vector<IndexedDBBlobInfo>* output) {
  std::vector<IndexedDBBlobInfo> ret;
  output->clear();
  StringPiece slice(data);
  while (!slice.empty()) {
    bool is_file;
    int64_t key;
    base::string16 type;
    int64_t size;
    base::string16 file_name;

    if (!DecodeBool(&slice, &is_file))
      return false;
    if (!DecodeVarInt(&slice, &key) ||
        !DatabaseMetaDataKey::IsValidBlobKey(key))
      return false;
    if (!DecodeStringWithLength(&slice, &type))
      return false;
    if (is_file) {
      if (!DecodeStringWithLength(&slice, &file_name))
        return false;
      ret.push_back(IndexedDBBlobInfo(key, type, file_name));
    } else {
      if (!DecodeVarInt(&slice, &size) || size < 0)
        return false;
      ret.push_back(IndexedDBBlobInfo(type, size, key));
    }
  }
  output->swap(ret);

  return true;
}

bool IsPathTooLong(const FilePath& leveldb_dir) {
  int limit = base::GetMaximumPathComponentLength(leveldb_dir.DirName());
  if (limit == -1) {
    DLOG(WARNING) << "GetMaximumPathComponentLength returned -1";
// In limited testing, ChromeOS returns 143, other OSes 255.
#if defined(OS_CHROMEOS)
    limit = 143;
#else
    limit = 255;
#endif
  }
  size_t component_length = leveldb_dir.BaseName().value().length();
  if (component_length > static_cast<uint32_t>(limit)) {
    DLOG(WARNING) << "Path component length (" << component_length
                  << ") exceeds maximum (" << limit
                  << ") allowed by this filesystem.";
    const int min = 140;
    const int max = 300;
    const int num_buckets = 12;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "WebCore.IndexedDB.BackingStore.OverlyLargeOriginLength",
        component_length, min, max, num_buckets);
    return true;
  }
  return false;
}

Status DeleteBlobsInRange(IndexedDBBackingStore::Transaction* transaction,
                          int64_t database_id,
                          int64_t object_store_id,
                          const std::string& start_key,
                          const std::string& end_key,
                          bool upper_open) {
  std::unique_ptr<LevelDBIterator> it =
      transaction->transaction()->CreateIterator();
  Status s = it->Seek(start_key);
  for (; s.ok() && it->IsValid() &&
         (upper_open ? CompareKeys(it->Key(), end_key) < 0
                     : CompareKeys(it->Key(), end_key) <= 0);
       s = it->Next()) {
    StringPiece key_piece(it->Key());
    std::string user_key =
        BlobEntryKey::ReencodeToObjectStoreDataKey(&key_piece);
    if (user_key.empty()) {
      INTERNAL_CONSISTENCY_ERROR_UNTESTED(GET_IDBDATABASE_METADATA);
      return InternalInconsistencyStatus();
    }
    transaction->PutBlobInfo(database_id, object_store_id, user_key, nullptr);
  }
  return s;
}

Status DeleteBlobsInObjectStore(IndexedDBBackingStore::Transaction* transaction,
                                int64_t database_id,
                                int64_t object_store_id) {
  std::string start_key, stop_key;
  start_key =
      BlobEntryKey::EncodeMinKeyForObjectStore(database_id, object_store_id);
  stop_key =
      BlobEntryKey::EncodeStopKeyForObjectStore(database_id, object_store_id);
  return DeleteBlobsInRange(transaction, database_id, object_store_id,
                            start_key, stop_key, true);
}

bool ObjectStoreCursorOptions(
    LevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& range,
    blink::WebIDBCursorDirection direction,
    IndexedDBBackingStore::Cursor::CursorOptions* cursor_options,
    Status* status) {
  cursor_options->database_id = database_id;
  cursor_options->object_store_id = object_store_id;

  bool lower_bound = range.lower().IsValid();
  bool upper_bound = range.upper().IsValid();
  cursor_options->forward =
      (direction == blink::kWebIDBCursorDirectionNextNoDuplicate ||
       direction == blink::kWebIDBCursorDirectionNext);
  cursor_options->unique =
      (direction == blink::kWebIDBCursorDirectionNextNoDuplicate ||
       direction == blink::kWebIDBCursorDirectionPrevNoDuplicate);

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
                                          &cursor_options->high_key, status))
        return false;
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
                                          &found_high_key, status))
        return false;

      // If the target key should not be included, but we end up with a smaller
      // key, we should include that.
      if (cursor_options->high_open &&
          CompareIndexKeys(found_high_key, cursor_options->high_key) < 0)
        cursor_options->high_open = false;

      cursor_options->high_key = found_high_key;
    }
  }

  return true;
}

bool IndexCursorOptions(
    LevelDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& range,
    blink::WebIDBCursorDirection direction,
    IndexedDBBackingStore::Cursor::CursorOptions* cursor_options,
    Status* status) {
  IDB_TRACE("IndexedDBBackingStore::IndexCursorOptions");
  DCHECK(transaction);
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id))
    return false;

  cursor_options->database_id = database_id;
  cursor_options->object_store_id = object_store_id;
  cursor_options->index_id = index_id;

  bool lower_bound = range.lower().IsValid();
  bool upper_bound = range.upper().IsValid();
  cursor_options->forward =
      (direction == blink::kWebIDBCursorDirectionNextNoDuplicate ||
       direction == blink::kWebIDBCursorDirectionNext);
  cursor_options->unique =
      (direction == blink::kWebIDBCursorDirectionNextNoDuplicate ||
       direction == blink::kWebIDBCursorDirectionPrevNoDuplicate);

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
                                          &cursor_options->high_key, status))
        return false;
      cursor_options->high_open = false;
    }
  } else {
    cursor_options->high_key = IndexDataKey::Encode(
        database_id, object_store_id, index_id, range.upper());
    cursor_options->high_open = range.upper_open();

    std::string found_high_key;
    // Seek to the *last* key in the set of non-unique keys
    if (!FindGreatestKeyLessThanOrEqual(transaction, cursor_options->high_key,
                                        &found_high_key, status))
      return false;

    // If the target key should not be included, but we end up with a smaller
    // key, we should include that.
    if (cursor_options->high_open &&
        CompareIndexKeys(found_high_key, cursor_options->high_key) < 0)
      cursor_options->high_open = false;

    cursor_options->high_key = found_high_key;
  }

  return true;
}

}  // namespace

class DefaultLevelDBFactory : public LevelDBFactory {
 public:
  DefaultLevelDBFactory() {}
  Status OpenLevelDB(const FilePath& file_name,
                     const LevelDBComparator* comparator,
                     std::unique_ptr<LevelDBDatabase>* db,
                     bool* is_disk_full) override {
    return LevelDBDatabase::Open(
        file_name, comparator,
        LevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase, db, is_disk_full);
  }
  Status DestroyLevelDB(const FilePath& file_name) override {
    return LevelDBDatabase::Destroy(file_name);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultLevelDBFactory);
};

IndexedDBBackingStore::IndexedDBBackingStore(
    IndexedDBFactory* indexed_db_factory,
    const Origin& origin,
    const FilePath& blob_path,
    std::unique_ptr<LevelDBDatabase> db,
    std::unique_ptr<LevelDBComparator> comparator,
    base::SequencedTaskRunner* task_runner)
    : indexed_db_factory_(indexed_db_factory),
      origin_(origin),
      blob_path_(blob_path),
      origin_identifier_(ComputeOriginIdentifier(origin)),
      task_runner_(task_runner),
      db_(std::move(db)),
      comparator_(std::move(comparator)),
      active_blob_registry_(this),
      committing_transaction_count_(0) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

IndexedDBBackingStore::~IndexedDBBackingStore() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!blob_path_.empty() && !child_process_ids_granted_.empty()) {
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    for (const auto& pid : child_process_ids_granted_)
      policy->RevokeAllPermissionsForFile(pid, blob_path_);
  }
  // db_'s destructor uses comparator_. The order of destruction is important.
  db_.reset();
  comparator_.reset();
}

IndexedDBBackingStore::RecordIdentifier::RecordIdentifier(
    const std::string& primary_key,
    int64_t version)
    : primary_key_(primary_key), version_(version) {
  DCHECK(!primary_key.empty());
}
IndexedDBBackingStore::RecordIdentifier::RecordIdentifier()
    : primary_key_(), version_(-1) {}
IndexedDBBackingStore::RecordIdentifier::~RecordIdentifier() {}

constexpr const int IndexedDBBackingStore::kMaxJournalCleanRequests;
constexpr const base::TimeDelta
    IndexedDBBackingStore::kMaxJournalCleaningWindowTime;
constexpr const base::TimeDelta
    IndexedDBBackingStore::kInitialJournalCleaningWindowTime;

// static
scoped_refptr<IndexedDBBackingStore> IndexedDBBackingStore::Open(
    IndexedDBFactory* indexed_db_factory,
    const Origin& origin,
    const FilePath& path_base,
    IndexedDBDataLossInfo* data_loss_info,
    bool* disk_full,
    base::SequencedTaskRunner* task_runner,
    bool clean_journal,
    Status* status) {
  DefaultLevelDBFactory leveldb_factory;
  return IndexedDBBackingStore::Open(
      indexed_db_factory, origin, path_base, data_loss_info, disk_full,
      &leveldb_factory, task_runner, clean_journal, status);
}

Status IndexedDBBackingStore::DestroyBackingStore(const FilePath& path_base,
                                                  const Origin& origin) {
  const FilePath file_path =
      path_base.Append(IndexedDBContextImpl::GetLevelDBFileName(origin));
  DefaultLevelDBFactory leveldb_factory;
  return leveldb_factory.DestroyLevelDB(file_path);
}

Status IndexedDBBackingStore::AnyDatabaseContainsBlobs(
    LevelDBTransaction* transaction,
    bool* blobs_exist) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  Status status = leveldb::Status::OK();
  std::vector<base::string16> names;
  IndexedDBMetadataCoding metadata_coding;
  status = metadata_coding.ReadDatabaseNames(transaction, origin_identifier_,
                                             &names);
  if (!status.ok())
    return status;

  *blobs_exist = false;
  for (const auto& name : names) {
    IndexedDBDatabaseMetadata metadata;
    bool found = false;
    status = metadata_coding.ReadMetadataForDatabaseName(
        transaction, origin_identifier_, name, &metadata, &found);
    if (!found)
      return Status::NotFound("Metadata not found for \"%s\".",
                              base::UTF16ToUTF8(name));
    for (const auto& store_id_metadata_pair : metadata.object_stores) {
      std::unique_ptr<LevelDBIterator> iterator = transaction->CreateIterator();
      std::string min_key = BlobEntryKey::EncodeMinKeyForObjectStore(
          metadata.id, store_id_metadata_pair.first);
      std::string max_key = BlobEntryKey::EncodeStopKeyForObjectStore(
          metadata.id, store_id_metadata_pair.first);
      status = iterator->Seek(base::StringPiece(min_key));
      if (status.IsNotFound()) {
        status = Status::OK();
        continue;
      }
      if (!status.ok())
        return status;
      if (iterator->IsValid() &&
          comparator_->Compare(iterator->Key(), base::StringPiece(max_key)) <
              0) {
        *blobs_exist = true;
        return Status::OK();
      }
    }

    if (!status.ok())
      return status;
  }
  return Status::OK();
}

Status IndexedDBBackingStore::RevertSchemaToV2() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  const std::string schema_version_key = SchemaVersionKey::Encode();
  scoped_refptr<LevelDBTransaction> transaction =
      IndexedDBClassFactory::Get()->CreateLevelDBTransaction(db_.get());

  PutInt(transaction.get(), schema_version_key, 2);
  Status s = transaction->Commit();
  if (!s.ok())
    INTERNAL_WRITE_ERROR_UNTESTED(REVERT_SCHEMA_TO_V2);
  return s;
}

V2SchemaCorruptionStatus IndexedDBBackingStore::HasV2SchemaCorruption() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  const std::string schema_version_key = SchemaVersionKey::Encode();
  scoped_refptr<LevelDBTransaction> transaction =
      IndexedDBClassFactory::Get()->CreateLevelDBTransaction(db_.get());

  int64_t db_schema_version = 0;
  bool found = false;
  Status s =
      GetInt(transaction.get(), schema_version_key, &db_schema_version, &found);
  if (!s.ok())
    return V2SchemaCorruptionStatus::kUnknown;
  if (db_schema_version != 2)
    return V2SchemaCorruptionStatus::kNo;

  bool has_blobs = false;
  s = AnyDatabaseContainsBlobs(transaction.get(), &has_blobs);
  if (!s.ok())
    return V2SchemaCorruptionStatus::kUnknown;
  if (!has_blobs)
    return V2SchemaCorruptionStatus::kNo;

  s = transaction->Commit();
  if (!s.ok())
    return V2SchemaCorruptionStatus::kUnknown;
  return V2SchemaCorruptionStatus::kYes;
}

WARN_UNUSED_RESULT Status IndexedDBBackingStore::SetUpMetadata() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const IndexedDBDataFormatVersion latest_known_data_version =
      IndexedDBDataFormatVersion::GetCurrent();
  const std::string schema_version_key = SchemaVersionKey::Encode();
  const std::string data_version_key = DataVersionKey::Encode();

  scoped_refptr<LevelDBTransaction> transaction =
      IndexedDBClassFactory::Get()->CreateLevelDBTransaction(db_.get());

  int64_t db_schema_version = 0;
  IndexedDBDataFormatVersion db_data_version;
  bool found = false;
  Status s =
      GetInt(transaction.get(), schema_version_key, &db_schema_version, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(SET_UP_METADATA);
    return s;
  }
  indexed_db::ReportSchemaVersion(db_schema_version, origin_);
  if (!found) {
    // Initialize new backing store.
    db_schema_version = indexed_db::kLatestKnownSchemaVersion;
    PutInt(transaction.get(), schema_version_key, db_schema_version);
    db_data_version = latest_known_data_version;
    PutInt(transaction.get(), data_version_key, db_data_version.Encode());
    // If a blob directory already exists for this database, blow it away.  It's
    // leftover from a partially-purged previous generation of data.
    if (!base::DeleteFile(blob_path_, true)) {
      INTERNAL_WRITE_ERROR_UNTESTED(SET_UP_METADATA);
      return IOErrorStatus();
    }
  } else {
    // Upgrade old backing store.
    DCHECK_LE(db_schema_version, indexed_db::kLatestKnownSchemaVersion);
    if (db_schema_version < 1) {
      db_schema_version = 1;
      PutInt(transaction.get(), schema_version_key, db_schema_version);
      const std::string start_key =
          DatabaseNameKey::EncodeMinKeyForOrigin(origin_identifier_);
      const std::string stop_key =
          DatabaseNameKey::EncodeStopKeyForOrigin(origin_identifier_);
      std::unique_ptr<LevelDBIterator> it =
          db_->CreateIterator(db_->DefaultReadOptions());
      for (s = it->Seek(start_key);
           s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0;
           s = it->Next()) {
        int64_t database_id = 0;
        found = false;
        s = GetInt(transaction.get(), it->Key(), &database_id, &found);
        if (!s.ok()) {
          INTERNAL_READ_ERROR_UNTESTED(SET_UP_METADATA);
          return s;
        }
        if (!found) {
          INTERNAL_CONSISTENCY_ERROR_UNTESTED(SET_UP_METADATA);
          return InternalInconsistencyStatus();
        }
        std::string version_key = DatabaseMetaDataKey::Encode(
            database_id, DatabaseMetaDataKey::USER_VERSION);
        PutVarInt(transaction.get(), version_key,
                  IndexedDBDatabaseMetadata::DEFAULT_VERSION);
      }
    }
    if (s.ok() && db_schema_version < 2) {
      db_schema_version = 2;
      PutInt(transaction.get(), schema_version_key, db_schema_version);
      db_data_version = latest_known_data_version;
      PutInt(transaction.get(), data_version_key, db_data_version.Encode());
    }
    if (db_schema_version < 3) {
      // Up until http://crrev.com/3c0d175b, this migration path did not write
      // the updated schema version to disk. In consequence, any database that
      // started out as schema version <= 2 will remain at schema version 2
      // indefinitely. Furthermore, this migration path used to call
      // "base::DeleteFile(blob_path_, true)", so databases stuck at version 2
      // would lose their stored Blobs on every open call.
      //
      // In order to prevent corrupt databases, when upgrading from 2 to 3 this
      // will consider any v2 databases with BlobEntryKey entries as corrupt.
      // https://crbug.com/756447, https://crbug.com/829125,
      // https://crbug.com/829141
      db_schema_version = 3;
      bool has_blobs = false;
      s = AnyDatabaseContainsBlobs(transaction.get(), &has_blobs);
      if (!s.ok()) {
        INTERNAL_CONSISTENCY_ERROR_UNTESTED(SET_UP_METADATA);
        return InternalInconsistencyStatus();
      }
      indexed_db::ReportV2Schema(has_blobs, origin_);
      if (has_blobs) {
        INTERNAL_CONSISTENCY_ERROR(UPGRADING_SCHEMA_CORRUPTED_BLOBS);
        // Put database wiping behind a flag so we can use finch to stop this
        // behavior if first-party customers have problems.
        if (base::FeatureList::IsEnabled(
                features::kWipeCorruptV2IDBDatabases)) {
          return InternalInconsistencyStatus();
        }
      } else {
        PutInt(transaction.get(), schema_version_key, db_schema_version);
      }
    }
  }

  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(SET_UP_METADATA);
    return s;
  }

  // All new values will be written using this serialization version.
  found = false;
  int64_t raw_db_data_version = 0;
  s = GetInt(transaction.get(), data_version_key, &raw_db_data_version, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(SET_UP_METADATA);
    return s;
  }
  if (!found) {
    INTERNAL_CONSISTENCY_ERROR_UNTESTED(SET_UP_METADATA);
    return InternalInconsistencyStatus();
  }
  db_data_version = IndexedDBDataFormatVersion::Decode(raw_db_data_version);
  if (latest_known_data_version == db_data_version) {
    // Up to date. Nothing to do.
  } else if (latest_known_data_version.IsAtLeast(db_data_version)) {
    db_data_version = latest_known_data_version;
    PutInt(transaction.get(), data_version_key, db_data_version.Encode());
  } else {
    // |db_data_version| is in the future according to at least one component.
    INTERNAL_CONSISTENCY_ERROR(SET_UP_METADATA);
    return InternalInconsistencyStatus();
  }

  DCHECK_EQ(db_schema_version, indexed_db::kLatestKnownSchemaVersion);
  DCHECK(db_data_version == latest_known_data_version);

  s = transaction->Commit();
  if (!s.ok())
    INTERNAL_WRITE_ERROR_UNTESTED(SET_UP_METADATA);
  return s;
}

leveldb::Status IndexedDBBackingStore::GetCompleteMetadata(
    std::vector<IndexedDBDatabaseMetadata>* output) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  IndexedDBMetadataCoding metadata_coding;
  leveldb::Status status = leveldb::Status::OK();
  std::vector<base::string16> names;
  status =
      metadata_coding.ReadDatabaseNames(db_.get(), origin_identifier_, &names);
  if (!status.ok())
    return status;

  output->reserve(names.size());
  for (auto& name : names) {
    output->emplace_back();
    bool found = false;
    status = metadata_coding.ReadMetadataForDatabaseName(
        db_.get(), origin_identifier_, name, &output->back(), &found);
    output->back().name = std::move(name);
    if (!found)
      return Status::NotFound("Metadata not found for \"%s\".",
                              base::UTF16ToUTF8(output->back().name));
    if (!status.ok())
      return status;
  }

  return status;
}

// static
bool IndexedDBBackingStore::ReadCorruptionInfo(const FilePath& path_base,
                                               const Origin& origin,
                                               std::string* message) {
  const FilePath info_path =
      path_base.Append(ComputeCorruptionFileName(origin));

  if (IsPathTooLong(info_path))
    return false;

  const int64_t kMaxJsonLength = 4096;
  int64_t file_size = 0;
  if (!GetFileSize(info_path, &file_size))
    return false;
  if (!file_size || file_size > kMaxJsonLength) {
    base::DeleteFile(info_path, false);
    return false;
  }

  base::File file(info_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  bool success = false;
  if (file.IsValid()) {
    std::string input_js(file_size, '\0');
    if (file_size == file.Read(0, base::data(input_js), file_size)) {
      base::JSONReader reader;
      std::unique_ptr<base::DictionaryValue> val(
          base::DictionaryValue::From(reader.ReadToValue(input_js)));
      if (val)
        success = val->GetString("message", message);
    }
    file.Close();
  }

  base::DeleteFile(info_path, false);

  return success;
}

// static
bool IndexedDBBackingStore::RecordCorruptionInfo(const FilePath& path_base,
                                                 const Origin& origin,
                                                 const std::string& message) {
  const FilePath info_path =
      path_base.Append(ComputeCorruptionFileName(origin));
  if (IsPathTooLong(info_path))
    return false;

  base::DictionaryValue root_dict;
  root_dict.SetString("message", message);
  std::string output_js;
  base::JSONWriter::Write(root_dict, &output_js);

  base::File file(info_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid())
    return false;
  int written = file.Write(0, output_js.c_str(), output_js.length());
  return size_t(written) == output_js.length();
}

// static
scoped_refptr<IndexedDBBackingStore> IndexedDBBackingStore::Open(
    IndexedDBFactory* indexed_db_factory,
    const Origin& origin,
    const FilePath& path_base,
    IndexedDBDataLossInfo* data_loss_info,
    bool* is_disk_full,
    LevelDBFactory* leveldb_factory,
    base::SequencedTaskRunner* task_runner,
    bool clean_journal,
    Status* status) {
  IDB_TRACE("IndexedDBBackingStore::Open");
  DCHECK(!path_base.empty());
  *is_disk_full = false;

  data_loss_info->status = blink::kWebIDBDataLossNone;
  *status = Status::OK();

  std::unique_ptr<LevelDBComparator> comparator(std::make_unique<Comparator>());

  if (!base::IsStringASCII(path_base.AsUTF8Unsafe())) {
    HistogramOpenStatus(
        indexed_db::INDEXED_DB_BACKING_STORE_OPEN_ATTEMPT_NON_ASCII, origin);
  }
  if (!base::CreateDirectory(path_base)) {
    *status = Status::IOError("Unable to create IndexedDB database path");
    LOG(ERROR) << status->ToString() << ": \"" << path_base.AsUTF8Unsafe()
               << "\"";
    HistogramOpenStatus(
        indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_DIRECTORY, origin);
    return scoped_refptr<IndexedDBBackingStore>();
  }

  const FilePath file_path =
      path_base.Append(IndexedDBContextImpl::GetLevelDBFileName(origin));
  const FilePath blob_path =
      path_base.Append(IndexedDBContextImpl::GetBlobStoreFileName(origin));

  if (IsPathTooLong(file_path)) {
    *status = Status::IOError("File path too long");
    HistogramOpenStatus(
        indexed_db::INDEXED_DB_BACKING_STORE_OPEN_ORIGIN_TOO_LONG, origin);
    return scoped_refptr<IndexedDBBackingStore>();
  }

  std::unique_ptr<LevelDBDatabase> db;
  *status = leveldb_factory->OpenLevelDB(
      file_path, comparator.get(), &db, is_disk_full);

  DCHECK(!db == !status->ok());
  if (!status->ok()) {
    if (leveldb_env::IndicatesDiskFull(*status)) {
      *is_disk_full = true;
    } else if (status->IsCorruption()) {
      data_loss_info->status = blink::kWebIDBDataLossTotal;
      data_loss_info->message = leveldb_env::GetCorruptionMessage(*status);
    }
  }

  bool is_schema_known = false;
  if (db) {
    std::string corruption_message;
    if (ReadCorruptionInfo(path_base, origin, &corruption_message)) {
      LOG(ERROR) << "IndexedDB recovering from a corrupted (and deleted) "
                    "database.";
      HistogramOpenStatus(
          indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_PRIOR_CORRUPTION,
          origin);
      db.reset();
      data_loss_info->status = blink::kWebIDBDataLossTotal;
      data_loss_info->message =
          "IndexedDB (database was corrupt): " + corruption_message;
    } else if (!IsSchemaKnown(db.get(), &is_schema_known)) {
      LOG(ERROR) << "IndexedDB had IO error checking schema, treating it as "
                    "failure to open";
      HistogramOpenStatus(
          indexed_db::
              INDEXED_DB_BACKING_STORE_OPEN_FAILED_IO_ERROR_CHECKING_SCHEMA,
          origin);
      db.reset();
      data_loss_info->status = blink::kWebIDBDataLossTotal;
      data_loss_info->message = "I/O error checking schema";
    } else if (!is_schema_known) {
      LOG(ERROR) << "IndexedDB backing store had unknown schema, treating it "
                    "as failure to open";
      HistogramOpenStatus(
          indexed_db::INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_SCHEMA,
          origin);
      db.reset();
      data_loss_info->status = blink::kWebIDBDataLossTotal;
      data_loss_info->message = "Unknown schema";
    }
  }

  DCHECK(status->ok() || !is_schema_known || status->IsIOError() ||
         status->IsCorruption());

  if (db) {
    HistogramOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_SUCCESS,
                        origin);
  } else if (status->IsIOError()) {
    LOG(ERROR) << "Unable to open backing store, not trying to recover - "
               << status->ToString();
    HistogramOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_NO_RECOVERY,
                        origin);
    return scoped_refptr<IndexedDBBackingStore>();
  } else {
    DCHECK(!is_schema_known || status->IsCorruption());
    LOG(ERROR) << "IndexedDB backing store open failed, attempting cleanup";
    *status = leveldb_factory->DestroyLevelDB(file_path);
    if (!status->ok()) {
      LOG(ERROR) << "IndexedDB backing store cleanup failed";
      HistogramOpenStatus(
          indexed_db::INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_DESTROY_FAILED,
          origin);
      return scoped_refptr<IndexedDBBackingStore>();
    }

    LOG(ERROR) << "IndexedDB backing store cleanup succeeded, reopening";
    *status =
        leveldb_factory->OpenLevelDB(file_path, comparator.get(), &db, nullptr);
    if (!status->ok()) {
      DCHECK(!db);
      LOG(ERROR) << "IndexedDB backing store reopen after recovery failed";
      HistogramOpenStatus(
          indexed_db::INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_REOPEN_FAILED,
          origin);
      return scoped_refptr<IndexedDBBackingStore>();
    }
    HistogramOpenStatus(
        indexed_db::INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_REOPEN_SUCCESS,
        origin);
  }

  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          db.get(), "IndexedDBBackingStore", task_runner,
          base::trace_event::MemoryDumpProvider::Options());

  scoped_refptr<IndexedDBBackingStore> backing_store =
      Create(indexed_db_factory, origin, blob_path, std::move(db),
             std::move(comparator), task_runner, status);

  if (clean_journal && backing_store.get()) {
    *status = backing_store->CleanUpBlobJournal(LiveBlobJournalKey::Encode());
    if (!status->ok()) {
      HistogramOpenStatus(
          indexed_db::
              INDEXED_DB_BACKING_STORE_OPEN_FAILED_CLEANUP_JOURNAL_ERROR,
          origin);
      return scoped_refptr<IndexedDBBackingStore>();
    }
  }
  return backing_store;
}

// static
scoped_refptr<IndexedDBBackingStore> IndexedDBBackingStore::OpenInMemory(
    const Origin& origin,
    base::SequencedTaskRunner* task_runner,
    Status* status) {
  DefaultLevelDBFactory leveldb_factory;
  return IndexedDBBackingStore::OpenInMemory(origin, &leveldb_factory,
                                             task_runner, status);
}

// static
scoped_refptr<IndexedDBBackingStore> IndexedDBBackingStore::OpenInMemory(
    const Origin& origin,
    LevelDBFactory* leveldb_factory,
    base::SequencedTaskRunner* task_runner,
    Status* status) {
  IDB_TRACE("IndexedDBBackingStore::OpenInMemory");

  std::unique_ptr<LevelDBComparator> comparator(std::make_unique<Comparator>());
  std::unique_ptr<LevelDBDatabase> db =
      LevelDBDatabase::OpenInMemory(comparator.get());
  if (!db) {
    LOG(ERROR) << "LevelDBDatabase::OpenInMemory failed.";
    HistogramOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_MEMORY_FAILED,
                        origin);
    return scoped_refptr<IndexedDBBackingStore>();
  }
  HistogramOpenStatus(indexed_db::INDEXED_DB_BACKING_STORE_OPEN_MEMORY_SUCCESS,
                      origin);
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          db.get(), "IndexedDBBackingStore", task_runner,
          base::trace_event::MemoryDumpProvider::Options());

  return Create(nullptr /* indexed_db_factory */, origin, FilePath(),
                std::move(db), std::move(comparator), task_runner, status);
}

// static
scoped_refptr<IndexedDBBackingStore> IndexedDBBackingStore::Create(
    IndexedDBFactory* indexed_db_factory,
    const Origin& origin,
    const FilePath& blob_path,
    std::unique_ptr<LevelDBDatabase> db,
    std::unique_ptr<LevelDBComparator> comparator,
    base::SequencedTaskRunner* task_runner,
    Status* status) {
  // TODO(jsbell): Handle comparator name changes.
  scoped_refptr<IndexedDBBackingStore> backing_store(new IndexedDBBackingStore(
      indexed_db_factory, origin, blob_path, std::move(db),
      std::move(comparator), task_runner));
  *status = backing_store->SetUpMetadata();
  if (!status->ok())
    return scoped_refptr<IndexedDBBackingStore>();

  return backing_store;
}

void IndexedDBBackingStore::GrantChildProcessPermissions(int child_process_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!child_process_ids_granted_.count(child_process_id)) {
    child_process_ids_granted_.insert(child_process_id);
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        child_process_id, blob_path_);
  }
}

Status IndexedDBBackingStore::DeleteDatabase(const base::string16& name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  IDB_TRACE("IndexedDBBackingStore::DeleteDatabase");
  std::unique_ptr<LevelDBDirectTransaction> transaction =
      LevelDBDirectTransaction::Create(db_.get());

  Status s;

  bool success = false;
  int64_t id = 0;
  s = IndexedDBMetadataCoding().FindDatabaseId(db_.get(), origin_identifier_,
                                               name, &id, &success);
  if (!s.ok())
    return s;
  if (!success)
    return Status::OK();

  const std::string start_key =
      DatabaseMetaDataKey::Encode(id, DatabaseMetaDataKey::ORIGIN_NAME);
  const std::string stop_key =
      DatabaseMetaDataKey::Encode(id + 1, DatabaseMetaDataKey::ORIGIN_NAME);
  {
    IDB_TRACE("IndexedDBBackingStore::DeleteDatabase.DeleteEntries");
    leveldb::ReadOptions options = db_->DefaultReadOptions();
    options.fill_cache = false;
    std::unique_ptr<LevelDBIterator> it = db_->CreateIterator(options);
    for (s = it->Seek(start_key);
         s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0;
         s = it->Next())
      transaction->Remove(it->Key());
  }
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR_UNTESTED(DELETE_DATABASE);
    return s;
  }

  const std::string key = DatabaseNameKey::Encode(origin_identifier_, name);
  transaction->Remove(key);

  bool need_cleanup = false;
  if (active_blob_registry()->MarkDeletedCheckIfUsed(
          id, DatabaseMetaDataKey::kAllBlobsKey)) {
    s = MergeDatabaseIntoLiveBlobJournal(transaction.get(), id);
    if (!s.ok())
      return s;
  } else {
    s = MergeDatabaseIntoPrimaryBlobJournal(transaction.get(), id);
    if (!s.ok())
      return s;
    need_cleanup = true;
  }

  s = transaction->Commit();
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR_UNTESTED(DELETE_DATABASE);
    return s;
  }

  // If another transaction is running, this will defer processing
  // the journal until completion.
  if (need_cleanup)
    CleanPrimaryJournalIgnoreReturn();

  db_->Compact(start_key, stop_key);
  return s;
}

void IndexedDBBackingStore::Compact() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  db_->CompactAll();
}

Status IndexedDBBackingStore::GetRecord(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKey& key,
    IndexedDBValue* record) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  IDB_TRACE("IndexedDBBackingStore::GetRecord");
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();

  const std::string leveldb_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, key);
  std::string data;

  record->clear();

  bool found = false;
  Status s = leveldb_transaction->Get(leveldb_key, &data, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR(GET_RECORD);
    return s;
  }
  if (!found)
    return s;
  if (data.empty()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_RECORD);
    return Status::NotFound("Record contained no data");
  }

  int64_t version;
  StringPiece slice(data);
  if (!DecodeVarInt(&slice, &version)) {
    INTERNAL_READ_ERROR_UNTESTED(GET_RECORD);
    return InternalInconsistencyStatus();
  }

  record->bits = slice.as_string();
  return transaction->GetBlobInfoForRecord(database_id, leveldb_key, record);
}

int64_t IndexedDBBackingStore::GetInMemoryBlobSize() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  int64_t total_size = 0;
  for (const auto& kvp : incognito_blob_map_) {
    for (const IndexedDBBlobInfo& blob_info : kvp.second->blob_info()) {
      if (!blob_info.is_file()) {
        total_size += blob_info.size();
      }
    }
  }
  return total_size;
}

Status IndexedDBBackingStore::PutRecord(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKey& key,
    IndexedDBValue* value,
    RecordIdentifier* record_identifier) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  IDB_TRACE("IndexedDBBackingStore::PutRecord");
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  DCHECK(key.IsValid());

  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  int64_t version = -1;
  Status s = indexed_db::GetNewVersionNumber(leveldb_transaction, database_id,
                                             object_store_id, &version);
  if (!s.ok())
    return s;
  DCHECK_GE(version, 0);
  const std::string object_store_data_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, key);

  std::string v;
  EncodeVarInt(version, &v);
  v.append(value->bits);

  leveldb_transaction->Put(object_store_data_key, &v);
  s = transaction->PutBlobInfoIfNeeded(
      database_id, object_store_id, object_store_data_key, &value->blob_info);
  if (!s.ok())
    return s;

  const std::string exists_entry_key =
      ExistsEntryKey::Encode(database_id, object_store_id, key);
  std::string version_encoded;
  EncodeInt(version, &version_encoded);
  leveldb_transaction->Put(exists_entry_key, &version_encoded);

  std::string key_encoded;
  EncodeIDBKey(key, &key_encoded);
  record_identifier->Reset(key_encoded, version);
  return s;
}

Status IndexedDBBackingStore::ClearObjectStore(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  IDB_TRACE("IndexedDBBackingStore::ClearObjectStore");
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  const std::string start_key =
      KeyPrefix(database_id, object_store_id).Encode();
  const std::string stop_key =
      KeyPrefix(database_id, object_store_id + 1).Encode();
  Status s = transaction->transaction()->RemoveRange(start_key, stop_key, true);
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(CLEAR_OBJECT_STORE);
    return s;
  }
  return DeleteBlobsInObjectStore(transaction, database_id, object_store_id);
}

Status IndexedDBBackingStore::DeleteRecord(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const RecordIdentifier& record_identifier) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  IDB_TRACE("IndexedDBBackingStore::DeleteRecord");
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();

  const std::string object_store_data_key = ObjectStoreDataKey::Encode(
      database_id, object_store_id, record_identifier.primary_key());
  leveldb_transaction->Remove(object_store_data_key);
  Status s = transaction->PutBlobInfoIfNeeded(database_id, object_store_id,
                                              object_store_data_key, nullptr);
  if (!s.ok())
    return s;

  const std::string exists_entry_key = ExistsEntryKey::Encode(
      database_id, object_store_id, record_identifier.primary_key());
  leveldb_transaction->Remove(exists_entry_key);
  return Status::OK();
}

Status IndexedDBBackingStore::DeleteRange(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& key_range) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  Status s;
  std::unique_ptr<IndexedDBBackingStore::Cursor> start_cursor =
      OpenObjectStoreCursor(transaction, database_id, object_store_id,
                            key_range, blink::kWebIDBCursorDirectionNext, &s);
  if (!s.ok())
    return s;
  if (!start_cursor)
    return Status::OK();  // Empty range == delete success.
  std::unique_ptr<IndexedDBBackingStore::Cursor> end_cursor =
      OpenObjectStoreCursor(transaction, database_id, object_store_id,
                            key_range, blink::kWebIDBCursorDirectionPrev, &s);

  if (!s.ok())
    return s;
  if (!end_cursor)
    return Status::OK();  // Empty range == delete success.

  BlobEntryKey start_blob_key, end_blob_key;
  std::string start_key = ObjectStoreDataKey::Encode(
      database_id, object_store_id, start_cursor->key());
  StringPiece start_key_piece(start_key);
  if (!BlobEntryKey::FromObjectStoreDataKey(&start_key_piece, &start_blob_key))
    return InternalInconsistencyStatus();
  std::string stop_key = ObjectStoreDataKey::Encode(
      database_id, object_store_id, end_cursor->key());
  StringPiece stop_key_piece(stop_key);
  if (!BlobEntryKey::FromObjectStoreDataKey(&stop_key_piece, &end_blob_key))
    return InternalInconsistencyStatus();

  s = DeleteBlobsInRange(transaction, database_id, object_store_id,
                         start_blob_key.Encode(), end_blob_key.Encode(), false);
  if (!s.ok())
    return s;
  s = transaction->transaction()->RemoveRange(start_key, stop_key, false);
  if (!s.ok())
    return s;
  start_key =
      ExistsEntryKey::Encode(database_id, object_store_id, start_cursor->key());
  stop_key =
      ExistsEntryKey::Encode(database_id, object_store_id, end_cursor->key());

  s = transaction->transaction()->RemoveRange(start_key, stop_key, false);
  return s;
}

Status IndexedDBBackingStore::GetKeyGeneratorCurrentNumber(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t* key_generator_current_number) {
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();

  const std::string key_generator_current_number_key =
      ObjectStoreMetaDataKey::Encode(
          database_id, object_store_id,
          ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER);

  *key_generator_current_number = -1;
  std::string data;

  bool found = false;
  Status s =
      leveldb_transaction->Get(key_generator_current_number_key, &data, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_KEY_GENERATOR_CURRENT_NUMBER);
    return s;
  }
  if (found && !data.empty()) {
    StringPiece slice(data);
    if (!DecodeInt(&slice, key_generator_current_number) || !slice.empty()) {
      INTERNAL_READ_ERROR_UNTESTED(GET_KEY_GENERATOR_CURRENT_NUMBER);
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

  std::unique_ptr<LevelDBIterator> it = leveldb_transaction->CreateIterator();
  int64_t max_numeric_key = 0;

  for (s = it->Seek(start_key);
       s.ok() && it->IsValid() && CompareKeys(it->Key(), stop_key) < 0;
       s = it->Next()) {
    StringPiece slice(it->Key());
    ObjectStoreDataKey data_key;
    if (!ObjectStoreDataKey::Decode(&slice, &data_key) || !slice.empty()) {
      INTERNAL_READ_ERROR_UNTESTED(GET_KEY_GENERATOR_CURRENT_NUMBER);
      return InternalInconsistencyStatus();
    }
    std::unique_ptr<IndexedDBKey> user_key = data_key.user_key();
    if (user_key->type() == blink::kWebIDBKeyTypeNumber) {
      int64_t n = static_cast<int64_t>(user_key->number());
      if (n > max_numeric_key)
        max_numeric_key = n;
    }
  }

  if (s.ok())
    *key_generator_current_number = max_numeric_key + 1;
  else
    INTERNAL_READ_ERROR_UNTESTED(GET_KEY_GENERATOR_CURRENT_NUMBER);

  return s;
}

Status IndexedDBBackingStore::MaybeUpdateKeyGeneratorCurrentNumber(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t new_number,
    bool check_current) {
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();

  if (check_current) {
    int64_t current_number;
    Status s = GetKeyGeneratorCurrentNumber(transaction, database_id,
                                            object_store_id, &current_number);
    if (!s.ok())
      return s;
    if (new_number <= current_number)
      return s;
  }

  const std::string key_generator_current_number_key =
      ObjectStoreMetaDataKey::Encode(
          database_id, object_store_id,
          ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER);
  PutInt(transaction->transaction(), key_generator_current_number_key,
         new_number);
  return Status::OK();
}

Status IndexedDBBackingStore::KeyExistsInObjectStore(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKey& key,
    RecordIdentifier* found_record_identifier,
    bool* found) {
  IDB_TRACE("IndexedDBBackingStore::KeyExistsInObjectStore");
  if (!KeyPrefix::ValidIds(database_id, object_store_id))
    return InvalidDBKeyStatus();
  *found = false;
  const std::string leveldb_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, key);
  std::string data;

  Status s = transaction->transaction()->Get(leveldb_key, &data, found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(KEY_EXISTS_IN_OBJECT_STORE);
    return s;
  }
  if (!*found)
    return Status::OK();
  if (data.empty()) {
    INTERNAL_READ_ERROR_UNTESTED(KEY_EXISTS_IN_OBJECT_STORE);
    return InternalInconsistencyStatus();
  }

  int64_t version;
  StringPiece slice(data);
  if (!DecodeVarInt(&slice, &version))
    return InternalInconsistencyStatus();

  std::string encoded_key;
  EncodeIDBKey(key, &encoded_key);
  found_record_identifier->Reset(encoded_key, version);
  return s;
}

class IndexedDBBackingStore::Transaction::ChainedBlobWriterImpl
    : public IndexedDBBackingStore::Transaction::ChainedBlobWriter {
 public:
  typedef IndexedDBBackingStore::Transaction::WriteDescriptorVec
      WriteDescriptorVec;
  static scoped_refptr<ChainedBlobWriterImpl> Create(
      int64_t database_id,
      IndexedDBBackingStore* backing_store,
      WriteDescriptorVec* blobs,
      scoped_refptr<IndexedDBBackingStore::BlobWriteCallback> callback) {
    auto writer = base::WrapRefCounted(new ChainedBlobWriterImpl(
        database_id, backing_store, std::move(callback)));
    writer->blobs_.swap(*blobs);
    writer->iter_ = writer->blobs_.begin();
    backing_store->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ChainedBlobWriterImpl::WriteNextFile, writer));
    return writer;
  }

  void set_delegate(std::unique_ptr<FileWriterDelegate> delegate) override {
    delegate_.reset(delegate.release());
  }

  void ReportWriteCompletion(bool succeeded, int64_t bytes_written) override {
    DCHECK(waiting_for_callback_);
    DCHECK(!succeeded || bytes_written >= 0);
    waiting_for_callback_ = false;
    if (delegate_.get())  // Only present for Blob, not File.
      content::BrowserThread::DeleteSoon(
          content::BrowserThread::IO, FROM_HERE, delegate_.release());
    if (aborted_) {
      self_ref_ = nullptr;
      return;
    }
    if (iter_->size() != -1 && iter_->size() != bytes_written)
      succeeded = false;
    if (succeeded) {
      ++iter_;
      WriteNextFile();
    } else {
      callback_->Run(BlobWriteResult::FAILURE_ASYNC);
    }
  }

  void Abort() override {
    aborted_ = true;
    if (!waiting_for_callback_)
      return;
    self_ref_ = this;
  }

 private:
  ChainedBlobWriterImpl(
      int64_t database_id,
      IndexedDBBackingStore* backing_store,
      scoped_refptr<IndexedDBBackingStore::BlobWriteCallback> callback)
      : waiting_for_callback_(false),
        database_id_(database_id),
        backing_store_(backing_store),
        callback_(callback),
        aborted_(false) {}
  ~ChainedBlobWriterImpl() override {}

  void WriteNextFile() {
    DCHECK(!waiting_for_callback_);
    if (aborted_) {
      self_ref_ = nullptr;
      return;
    }
    if (iter_ == blobs_.end()) {
      DCHECK(!self_ref_.get());
      callback_->Run(BlobWriteResult::SUCCESS_ASYNC);
      return;
    } else {
      if (!backing_store_->WriteBlobFile(database_id_, *iter_, this)) {
        callback_->Run(BlobWriteResult::FAILURE_ASYNC);
        return;
      }
      waiting_for_callback_ = true;
    }
  }

  bool waiting_for_callback_;
  scoped_refptr<ChainedBlobWriterImpl> self_ref_;
  WriteDescriptorVec blobs_;
  WriteDescriptorVec::const_iterator iter_;
  int64_t database_id_;
  IndexedDBBackingStore* backing_store_;
  // Callback result is useless as call stack is no longer transaction's
  // operations queue. Errors are instead handled in
  // IndexedDBTransaction::BlobWriteComplete.
  scoped_refptr<IndexedDBBackingStore::BlobWriteCallback> callback_;
  std::unique_ptr<FileWriterDelegate> delegate_;
  bool aborted_;

  DISALLOW_COPY_AND_ASSIGN(ChainedBlobWriterImpl);
};

class LocalWriteClosure : public FileWriterDelegate::DelegateWriteCallback,
                          public base::RefCountedThreadSafe<LocalWriteClosure> {
 public:
  LocalWriteClosure(IndexedDBBackingStore::Transaction::ChainedBlobWriter*
                        chained_blob_writer,
                    base::SequencedTaskRunner* task_runner)
      : chained_blob_writer_(chained_blob_writer),
        task_runner_(task_runner),
        bytes_written_(0) {}

  void Run(base::File::Error rv,
           int64_t bytes,
           FileWriterDelegate::WriteProgressStatus write_status) {
    DCHECK_GE(bytes, 0);
    bytes_written_ += bytes;
    if (write_status == FileWriterDelegate::SUCCESS_IO_PENDING)
      return;  // We don't care about progress events.
    if (rv == base::File::FILE_OK) {
      DCHECK_EQ(write_status, FileWriterDelegate::SUCCESS_COMPLETED);
    } else {
      DCHECK(write_status == FileWriterDelegate::ERROR_WRITE_STARTED ||
             write_status == FileWriterDelegate::ERROR_WRITE_NOT_STARTED);
    }

    bool success = write_status == FileWriterDelegate::SUCCESS_COMPLETED;
    if (success && !bytes_written_) {
      // LocalFileStreamWriter only creates a file if data is actually written.
      // If none was then create one now.
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&LocalWriteClosure::CreateEmptyFile, this));
    } else if (success && !last_modified_.is_null()) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&LocalWriteClosure::UpdateTimeStamp, this));
    } else {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&IndexedDBBackingStore::Transaction::
                             ChainedBlobWriter::ReportWriteCompletion,
                         chained_blob_writer_, success, bytes_written_));
    }
  }

  void WriteBlobToFileOnIOThread(const FilePath& file_path,
                                 std::unique_ptr<storage::BlobDataHandle> blob,
                                 const base::Time& last_modified) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    std::unique_ptr<storage::FileStreamWriter> writer(
        storage::FileStreamWriter::CreateForLocalFile(
            task_runner_.get(), file_path, 0,
            storage::FileStreamWriter::CREATE_NEW_FILE));
    std::unique_ptr<FileWriterDelegate> delegate(
        std::make_unique<FileWriterDelegate>(
            std::move(writer), storage::FlushPolicy::FLUSH_ON_COMPLETION));

    DCHECK(blob);
    this->file_path_ = file_path;
    this->last_modified_ = last_modified;

    delegate->Start(blob->CreateReader(),
                    base::Bind(&LocalWriteClosure::Run, this));
    chained_blob_writer_->set_delegate(std::move(delegate));
  }

 private:
  virtual ~LocalWriteClosure() {
    // Make sure the last reference to a ChainedBlobWriter is released (and
    // deleted) on the IDB sequence since it owns a transaction which has
    // sequence affinity.
    IndexedDBBackingStore::Transaction::ChainedBlobWriter* raw_tmp =
        chained_blob_writer_.get();
    raw_tmp->AddRef();
    chained_blob_writer_ = nullptr;
    task_runner_->ReleaseSoon(FROM_HERE, raw_tmp);
  }
  friend class base::RefCountedThreadSafe<LocalWriteClosure>;

  // If necessary, update the timestamps on the file as a final
  // step before reporting success.
  void UpdateTimeStamp() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (!base::TouchFile(file_path_, last_modified_, last_modified_)) {
      // TODO(ericu): Complain quietly; timestamp's probably not vital.
    }
    chained_blob_writer_->ReportWriteCompletion(true, bytes_written_);
  }

  // Create an empty file.
  void CreateEmptyFile() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    base::File file(file_path_,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    bool success = file.created();
    if (success && !last_modified_.is_null() &&
        !file.SetTimes(last_modified_, last_modified_)) {
      // TODO(cmumford): Complain quietly; timestamp's probably not vital.
    }
    file.Close();
    chained_blob_writer_->ReportWriteCompletion(success, bytes_written_);
  }

  scoped_refptr<IndexedDBBackingStore::Transaction::ChainedBlobWriter>
      chained_blob_writer_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  int64_t bytes_written_;

  FilePath file_path_;
  base::Time last_modified_;

  DISALLOW_COPY_AND_ASSIGN(LocalWriteClosure);
};

bool IndexedDBBackingStore::WriteBlobFile(
    int64_t database_id,
    const Transaction::WriteDescriptor& descriptor,
    Transaction::ChainedBlobWriter* chained_blob_writer) {
  if (!MakeIDBBlobDirectory(blob_path_, database_id, descriptor.key()))
    return false;

  FilePath path = GetBlobFileName(database_id, descriptor.key());

  if (descriptor.is_file() && !descriptor.file_path().empty()) {
    if (!base::CopyFile(descriptor.file_path(), path))
      return false;

    base::File::Info info;
    if (base::GetFileInfo(descriptor.file_path(), &info)) {
      if (descriptor.size() != -1) {
        if (descriptor.size() != info.size)
          return false;
        // The round-trip can be lossy; round to nearest millisecond.
        int64_t delta =
            (descriptor.last_modified() - info.last_modified).InMilliseconds();
        if (std::abs(delta) > 1)
          return false;
      }
      if (!base::TouchFile(path, info.last_accessed, info.last_modified)) {
        // TODO(ericu): Complain quietly; timestamp's probably not vital.
      }
    } else {
      // TODO(ericu): Complain quietly; timestamp's probably not vital.
    }

    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Transaction::ChainedBlobWriter::ReportWriteCompletion,
                       chained_blob_writer, true, info.size));
  } else {
    DCHECK(descriptor.blob());
    scoped_refptr<LocalWriteClosure> write_closure(
        new LocalWriteClosure(chained_blob_writer, task_runner_.get()));
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            &LocalWriteClosure::WriteBlobToFileOnIOThread, write_closure, path,
            std::make_unique<storage::BlobDataHandle>(*descriptor.blob()),
            descriptor.last_modified()));
  }
  return true;
}

void IndexedDBBackingStore::ReportBlobUnused(int64_t database_id,
                                             int64_t blob_key) {
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  bool all_blobs = blob_key == DatabaseMetaDataKey::kAllBlobsKey;
  DCHECK(all_blobs || DatabaseMetaDataKey::IsValidBlobKey(blob_key));
  scoped_refptr<LevelDBTransaction> transaction =
      IndexedDBClassFactory::Get()->CreateLevelDBTransaction(db_.get());

  BlobJournalType live_blob_journal, primary_journal;
  if (!GetLiveBlobJournal(transaction.get(), &live_blob_journal).ok())
    return;
  DCHECK(!live_blob_journal.empty());
  if (!GetPrimaryBlobJournal(transaction.get(), &primary_journal).ok())
    return;

  // There are several cases to handle.  If blob_key is kAllBlobsKey, we want to
  // remove all entries with database_id from the live_blob journal and add only
  // kAllBlobsKey to the primary journal.  Otherwise if IsValidBlobKey(blob_key)
  // and we hit kAllBlobsKey for the right database_id in the journal, we leave
  // the kAllBlobsKey entry in the live_blob journal but add the specific blob
  // to the primary.  Otherwise if IsValidBlobKey(blob_key) and we find a
  // matching (database_id, blob_key) tuple, we should move it to the primary
  // journal.
  BlobJournalType new_live_blob_journal;
  for (auto journal_iter = live_blob_journal.begin();
       journal_iter != live_blob_journal.end(); ++journal_iter) {
    int64_t current_database_id = journal_iter->first;
    int64_t current_blob_key = journal_iter->second;
    bool current_all_blobs =
        current_blob_key == DatabaseMetaDataKey::kAllBlobsKey;
    DCHECK(KeyPrefix::IsValidDatabaseId(current_database_id) ||
           current_all_blobs);
    if (current_database_id == database_id &&
        (all_blobs || current_all_blobs || blob_key == current_blob_key)) {
      if (!all_blobs) {
        primary_journal.push_back(
            std::make_pair(database_id, current_blob_key));
        if (current_all_blobs)
          new_live_blob_journal.push_back(*journal_iter);
        new_live_blob_journal.insert(new_live_blob_journal.end(),
                                     ++journal_iter,
                                     live_blob_journal.end());  // All the rest.
        break;
      }
    } else {
      new_live_blob_journal.push_back(*journal_iter);
    }
  }
  if (all_blobs) {
    primary_journal.push_back(
        std::make_pair(database_id, DatabaseMetaDataKey::kAllBlobsKey));
  }
  UpdatePrimaryBlobJournal(transaction.get(), primary_journal);
  UpdateLiveBlobJournal(transaction.get(), new_live_blob_journal);
  transaction->Commit();
  // We could just do the deletions/cleaning here, but if there are a lot of
  // blobs about to be garbage collected, it'd be better to wait and do them all
  // at once.
  StartJournalCleaningTimer();
}

// The this reference is a raw pointer that's declared Unretained inside the
// timer code, so this won't confuse IndexedDBFactory's check for
// HasLastBackingStoreReference.  It's safe because if the backing store is
// deleted, the timer will automatically be canceled on destruction.
void IndexedDBBackingStore::StartJournalCleaningTimer() {
  ++num_aggregated_journal_cleaning_requests_;

  if (execute_journal_cleaning_on_no_txns_)
    return;

  if (num_aggregated_journal_cleaning_requests_ >= kMaxJournalCleanRequests) {
    journal_cleaning_timer_.AbandonAndStop();
    CleanPrimaryJournalIgnoreReturn();
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

  if (delay <= base::TimeDelta::FromSeconds(0)) {
    journal_cleaning_timer_.AbandonAndStop();
    CleanPrimaryJournalIgnoreReturn();
    return;
  }

  journal_cleaning_timer_.Start(
      FROM_HERE, delay, this,
      &IndexedDBBackingStore::CleanPrimaryJournalIgnoreReturn);
}

// This assumes a file path of dbId/second-to-LSB-of-counter/counter.
FilePath IndexedDBBackingStore::GetBlobFileName(int64_t database_id,
                                                int64_t key) const {
  return GetBlobFileNameForKey(blob_path_, database_id, key);
}

bool IndexedDBBackingStore::RemoveBlobFile(int64_t database_id,
                                           int64_t key) const {
  FilePath path = GetBlobFileName(database_id, key);
#if DCHECK_IS_ON()
  ++num_blob_files_deleted_;
  DVLOG(1) << "Deleting blob " << key << " from IndexedDB database "
           << database_id << " at path " << path.value();
#endif
  return base::DeleteFile(path, false);
}

bool IndexedDBBackingStore::RemoveBlobDirectory(int64_t database_id) const {
  FilePath path = GetBlobDirectoryName(blob_path_, database_id);
  return base::DeleteFile(path, true);
}

Status IndexedDBBackingStore::CleanUpBlobJournal(
    const std::string& level_db_key) const {
  IDB_TRACE("IndexedDBBackingStore::CleanUpBlobJournal");
  DCHECK(!committing_transaction_count_);
  scoped_refptr<LevelDBTransaction> journal_transaction =
      IndexedDBClassFactory::Get()->CreateLevelDBTransaction(db_.get());
  BlobJournalType journal;

  Status s = GetBlobJournal(level_db_key, journal_transaction.get(), &journal);
  if (!s.ok())
    return s;
  if (journal.empty())
    return Status::OK();
  s = CleanUpBlobJournalEntries(journal);
  if (!s.ok())
    return s;
  ClearBlobJournal(journal_transaction.get(), level_db_key);
  s = journal_transaction->Commit();
  // Notify blob files cleaned even if commit fails, as files could still be
  // deleted. |indexed_db_factory_| is null for in-memory backing stores.
  if (indexed_db_factory_)
    indexed_db_factory_->BlobFilesCleaned(origin_);
  return s;
}

Status IndexedDBBackingStore::CleanUpBlobJournalEntries(
    const BlobJournalType& journal) const {
  IDB_TRACE("IndexedDBBackingStore::CleanUpBlobJournalEntries");
  if (journal.empty())
    return Status::OK();
  for (const auto& entry : journal) {
    int64_t database_id = entry.first;
    int64_t blob_key = entry.second;
    DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
    if (blob_key == DatabaseMetaDataKey::kAllBlobsKey) {
      if (!RemoveBlobDirectory(database_id))
        return IOErrorStatus();
    } else {
      DCHECK(DatabaseMetaDataKey::IsValidBlobKey(blob_key));
      if (!RemoveBlobFile(database_id, blob_key))
        return IOErrorStatus();
    }
  }
  return Status::OK();
}

void IndexedDBBackingStore::WillCommitTransaction() {
  ++committing_transaction_count_;
}

void IndexedDBBackingStore::DidCommitTransaction() {
  DCHECK_GT(committing_transaction_count_, 0UL);
  --committing_transaction_count_;
  if (committing_transaction_count_ == 0 &&
      execute_journal_cleaning_on_no_txns_) {
    execute_journal_cleaning_on_no_txns_ = false;
    CleanPrimaryJournalIgnoreReturn();
  }
}

Status IndexedDBBackingStore::Transaction::GetBlobInfoForRecord(
    int64_t database_id,
    const std::string& object_store_data_key,
    IndexedDBValue* value) {
  BlobChangeRecord* change_record = nullptr;
  auto blob_iter = blob_change_map_.find(object_store_data_key);
  if (blob_iter != blob_change_map_.end()) {
    change_record = blob_iter->second.get();
  } else {
    blob_iter = incognito_blob_map_.find(object_store_data_key);
    if (blob_iter != incognito_blob_map_.end())
      change_record = blob_iter->second.get();
  }
  if (change_record) {
    // Either we haven't written the blob to disk yet or we're in incognito
    // mode, so we have to send back the one they sent us.  This change record
    // includes the original UUID.
    value->blob_info = change_record->blob_info();
    return Status::OK();
  }

  BlobEntryKey blob_entry_key;
  StringPiece leveldb_key_piece(object_store_data_key);
  if (!BlobEntryKey::FromObjectStoreDataKey(&leveldb_key_piece,
                                            &blob_entry_key)) {
    NOTREACHED();
    return InternalInconsistencyStatus();
  }
  std::string encoded_key = blob_entry_key.Encode();
  bool found;
  std::string encoded_value;
  Status s = transaction()->Get(encoded_key, &encoded_value, &found);
  if (!s.ok())
    return s;
  if (found) {
    if (!DecodeBlobData(encoded_value, &value->blob_info)) {
      INTERNAL_READ_ERROR(GET_BLOB_INFO_FOR_RECORD);
      return InternalInconsistencyStatus();
    }
    for (auto& entry : value->blob_info) {
      entry.set_file_path(
          backing_store_->GetBlobFileName(database_id, entry.key()));
      entry.set_mark_used_callback(
          backing_store_->active_blob_registry()->GetAddBlobRefCallback(
              database_id, entry.key()));
      entry.set_release_callback(
          backing_store_->active_blob_registry()->GetFinalReleaseCallback(
              database_id, entry.key()));
      if (entry.is_file() && !entry.file_path().empty()) {
        base::File::Info info;
        if (base::GetFileInfo(entry.file_path(), &info)) {
          // This should always work, but it isn't fatal if it doesn't; it just
          // means a potential slow synchronous call from the renderer later.
          entry.set_last_modified(info.last_modified);
          entry.set_size(info.size);
        }
      }
    }
  }
  return Status::OK();
}

void IndexedDBBackingStore::CleanPrimaryJournalIgnoreReturn() {
  // While a transaction is busy it is not safe to clean the journal.
  if (committing_transaction_count_ > 0) {
    execute_journal_cleaning_on_no_txns_ = true;
    return;
  }
  num_aggregated_journal_cleaning_requests_ = 0;
  CleanUpBlobJournal(BlobJournalKey::Encode());
}

Status IndexedDBBackingStore::ClearIndex(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id) {
  IDB_TRACE("IndexedDBBackingStore::ClearIndex");
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id))
    return InvalidDBKeyStatus();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();

  const std::string index_data_start =
      IndexDataKey::EncodeMinKey(database_id, object_store_id, index_id);
  const std::string index_data_end =
      IndexDataKey::EncodeMaxKey(database_id, object_store_id, index_id);
  Status s =
      leveldb_transaction->RemoveRange(index_data_start, index_data_end, true);

  if (!s.ok())
    INTERNAL_WRITE_ERROR_UNTESTED(DELETE_INDEX);

  return s;
}

Status IndexedDBBackingStore::PutIndexDataForRecord(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKey& key,
    const RecordIdentifier& record_identifier) {
  IDB_TRACE("IndexedDBBackingStore::PutIndexDataForRecord");
  DCHECK(key.IsValid());
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id))
    return InvalidDBKeyStatus();

  std::string encoded_key;
  EncodeIDBKey(key, &encoded_key);

  const std::string index_data_key =
      IndexDataKey::Encode(database_id,
                           object_store_id,
                           index_id,
                           encoded_key,
                           record_identifier.primary_key(),
                           0);

  std::string data;
  EncodeVarInt(record_identifier.version(), &data);
  data.append(record_identifier.primary_key());

  transaction->transaction()->Put(index_data_key, &data);
  return Status::OK();
}

Status IndexedDBBackingStore::FindKeyInIndex(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKey& key,
    std::string* found_encoded_primary_key,
    bool* found) {
  IDB_TRACE("IndexedDBBackingStore::FindKeyInIndex");
  DCHECK(KeyPrefix::ValidIds(database_id, object_store_id, index_id));

  DCHECK(found_encoded_primary_key->empty());
  *found = false;

  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  const std::string leveldb_key =
      IndexDataKey::Encode(database_id, object_store_id, index_id, key);
  std::unique_ptr<LevelDBIterator> it = leveldb_transaction->CreateIterator();
  Status s = it->Seek(leveldb_key);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(FIND_KEY_IN_INDEX);
    return s;
  }

  for (;;) {
    if (!it->IsValid())
      return Status::OK();
    if (CompareIndexKeys(it->Key(), leveldb_key) > 0)
      return Status::OK();

    StringPiece slice(it->Value());

    int64_t version;
    if (!DecodeVarInt(&slice, &version)) {
      INTERNAL_READ_ERROR_UNTESTED(FIND_KEY_IN_INDEX);
      return InternalInconsistencyStatus();
    }
    *found_encoded_primary_key = slice.as_string();

    bool exists = false;
    s = indexed_db::VersionExists(leveldb_transaction, database_id,
                                  object_store_id, version,
                                  *found_encoded_primary_key, &exists);
    if (!s.ok())
      return s;
    if (!exists) {
      // Delete stale index data entry and continue.
      leveldb_transaction->Remove(it->Key());
      s = it->Next();
      continue;
    }
    *found = true;
    return s;
  }
}

Status IndexedDBBackingStore::GetPrimaryKeyViaIndex(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKey& key,
    std::unique_ptr<IndexedDBKey>* primary_key) {
  IDB_TRACE("IndexedDBBackingStore::GetPrimaryKeyViaIndex");
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id))
    return InvalidDBKeyStatus();

  bool found = false;
  std::string found_encoded_primary_key;
  Status s = FindKeyInIndex(transaction, database_id, object_store_id, index_id,
                            key, &found_encoded_primary_key, &found);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_PRIMARY_KEY_VIA_INDEX);
    return s;
  }
  if (!found)
    return s;
  if (found_encoded_primary_key.empty()) {
    INTERNAL_READ_ERROR_UNTESTED(GET_PRIMARY_KEY_VIA_INDEX);
    return InvalidDBKeyStatus();
  }

  StringPiece slice(found_encoded_primary_key);
  if (DecodeIDBKey(&slice, primary_key) && slice.empty())
    return s;
  else
    return InvalidDBKeyStatus();
}

Status IndexedDBBackingStore::KeyExistsInIndex(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKey& index_key,
    std::unique_ptr<IndexedDBKey>* found_primary_key,
    bool* exists) {
  IDB_TRACE("IndexedDBBackingStore::KeyExistsInIndex");
  if (!KeyPrefix::ValidIds(database_id, object_store_id, index_id))
    return InvalidDBKeyStatus();

  *exists = false;
  std::string found_encoded_primary_key;
  Status s = FindKeyInIndex(transaction, database_id, object_store_id, index_id,
                            index_key, &found_encoded_primary_key, exists);
  if (!s.ok()) {
    INTERNAL_READ_ERROR_UNTESTED(KEY_EXISTS_IN_INDEX);
    return s;
  }
  if (!*exists)
    return Status::OK();
  if (found_encoded_primary_key.empty()) {
    INTERNAL_READ_ERROR_UNTESTED(KEY_EXISTS_IN_INDEX);
    return InvalidDBKeyStatus();
  }

  StringPiece slice(found_encoded_primary_key);
  if (DecodeIDBKey(&slice, found_primary_key) && slice.empty())
    return s;
  else
    return InvalidDBKeyStatus();
}

IndexedDBBackingStore::Cursor::Cursor(
    const IndexedDBBackingStore::Cursor* other)
    : backing_store_(other->backing_store_),
      transaction_(other->transaction_),
      database_id_(other->database_id_),
      cursor_options_(other->cursor_options_),
      current_key_(std::make_unique<IndexedDBKey>(*other->current_key_)) {
  if (other->iterator_) {
    iterator_ = transaction_->transaction()->CreateIterator();

    if (other->iterator_->IsValid()) {
      Status s = iterator_->Seek(other->iterator_->Key());
      // TODO(cmumford): Handle this error (crbug.com/363397)
      DCHECK(iterator_->IsValid());
    }
  }
}

IndexedDBBackingStore::Cursor::Cursor(
    scoped_refptr<IndexedDBBackingStore> backing_store,
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    const CursorOptions& cursor_options)
    : backing_store_(backing_store.get()),
      transaction_(transaction),
      database_id_(database_id),
      cursor_options_(cursor_options) {}
IndexedDBBackingStore::Cursor::~Cursor() {}

bool IndexedDBBackingStore::Cursor::FirstSeek(Status* s) {
  iterator_ = transaction_->transaction()->CreateIterator();
  {
    IDB_TRACE("IndexedDBBackingStore::Cursor::FirstSeek::Seek");
    if (cursor_options_.forward)
      *s = iterator_->Seek(cursor_options_.low_key);
    else
      *s = iterator_->Seek(cursor_options_.high_key);
    if (!s->ok())
      return false;
  }
  return Continue(nullptr, READY, s);
}

bool IndexedDBBackingStore::Cursor::Advance(uint32_t count, Status* s) {
  *s = Status::OK();
  while (count--) {
    if (!Continue(s))
      return false;
  }
  return true;
}

bool IndexedDBBackingStore::Cursor::Continue(const IndexedDBKey* key,
                                             const IndexedDBKey* primary_key,
                                             IteratorState next_state,
                                             Status* s) {
  IDB_TRACE("IndexedDBBackingStore::Cursor::Continue");
  DCHECK(!key || next_state == SEEK);

  if (cursor_options_.forward)
    return ContinueNext(key, primary_key, next_state, s) ==
           ContinueResult::DONE;
  else
    return ContinuePrevious(key, primary_key, next_state, s) ==
           ContinueResult::DONE;
}

IndexedDBBackingStore::Cursor::ContinueResult
IndexedDBBackingStore::Cursor::ContinueNext(const IndexedDBKey* key,
                                            const IndexedDBKey* primary_key,
                                            IteratorState next_state,
                                            Status* s) {
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
    if (!s->ok())
      return ContinueResult::LEVELDB_ERROR;
    // Cursor is at the next value already; don't advance it again below.
    next_state = READY;
  }

  for (;;) {
    // Only advance the cursor if it was not set to position already, either
    // because it is newly opened (and positioned at start of range) or
    // skipped forward by continue with a specific key.
    if (next_state == SEEK) {
      *s = iterator_->Next();
      if (!s->ok())
        return ContinueResult::LEVELDB_ERROR;
    } else {
      next_state = SEEK;
    }

    // Fail if we've run out of data or gone past the cursor's bounds.
    if (!iterator_->IsValid() || IsPastBounds())
      return ContinueResult::OUT_OF_BOUNDS;

    // TODO(jsbell): Document why this might be false. When do we ever not
    // seek into the range before starting cursor iteration?
    if (!HaveEnteredRange())
      continue;

    // The row may not load because there's a stale entry in the index. If no
    // error then not fatal.
    if (!LoadCurrentRow(s)) {
      if (!s->ok())
        return ContinueResult::LEVELDB_ERROR;
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

IndexedDBBackingStore::Cursor::ContinueResult
IndexedDBBackingStore::Cursor::ContinuePrevious(const IndexedDBKey* key,
                                                const IndexedDBKey* primary_key,
                                                IteratorState next_state,
                                                Status* s) {
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
      if (!s->ok())
        return ContinueResult::LEVELDB_ERROR;
    } else {
      next_state = SEEK;  // for subsequent iterations
    }

    // If we've run out of data or gone past the cursor's bounds.
    if (!iterator_->IsValid() || IsPastBounds()) {
      if (duplicate_key.IsValid())
        break;
      return ContinueResult::OUT_OF_BOUNDS;
    }

    // TODO(jsbell): Document why this might be false. When do we ever not
    // seek into the range before starting cursor iteration?
    if (!HaveEnteredRange())
      continue;

    // The row may not load because there's a stale entry in the index. If no
    // error then not fatal.
    if (!LoadCurrentRow(s)) {
      if (!s->ok())
        return ContinueResult::LEVELDB_ERROR;
      continue;
    }

    // If seeking to a key (or key and primary key), continue until found.
    // TODO(jsbell): If Seek() optimization is added above, remove this.
    if (key) {
      if (primary_key && key->Equals(*current_key_) &&
          primary_key->IsLessThan(this->primary_key()))
        continue;
      if (key->IsLessThan(*current_key_))
        continue;
    }

    // Cursor is now positioned at a non-stale record in range.

    if (cursor_options_.unique) {
      // If entry is a duplicate of the previous, keep going. Although the
      // cursor should be positioned at the first duplicate already, new
      // duplicates may have been inserted since the cursor was last iterated,
      // and should be skipped to maintain "unique" iteration.
      if (previous_key.IsValid() && current_key_->Equals(previous_key))
        continue;

      // If we've found a new key, remember it and keep going.
      if (!duplicate_key.IsValid()) {
        duplicate_key = *current_key_;
        earliest_duplicate = iterator_->Key().as_string();
        continue;
      }

      // If we're still seeing duplicates, keep going.
      if (duplicate_key.Equals(*current_key_)) {
        earliest_duplicate = iterator_->Key().as_string();
        continue;
      }
    }

    break;
  }

  if (cursor_options_.unique) {
    DCHECK(duplicate_key.IsValid());
    DCHECK(!earliest_duplicate.empty());

    *s = iterator_->Seek(earliest_duplicate);
    if (!s->ok())
      return ContinueResult::LEVELDB_ERROR;
    if (!LoadCurrentRow(s)) {
      DCHECK(!s->ok());
      return ContinueResult::LEVELDB_ERROR;
    }
  }

  return ContinueResult::DONE;
}

bool IndexedDBBackingStore::Cursor::HaveEnteredRange() const {
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

bool IndexedDBBackingStore::Cursor::IsPastBounds() const {
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

const IndexedDBKey& IndexedDBBackingStore::Cursor::primary_key() const {
  return *current_key_;
}

class ObjectStoreKeyCursorImpl : public IndexedDBBackingStore::Cursor {
 public:
  ObjectStoreKeyCursorImpl(
      scoped_refptr<IndexedDBBackingStore> backing_store,
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      const IndexedDBBackingStore::Cursor::CursorOptions& cursor_options)
      : IndexedDBBackingStore::Cursor(backing_store,
                                      transaction,
                                      database_id,
                                      cursor_options) {}

  std::unique_ptr<Cursor> Clone() const override {
    return base::WrapUnique(new ObjectStoreKeyCursorImpl(this));
  }

  // IndexedDBBackingStore::Cursor
  IndexedDBValue* value() override {
    NOTREACHED();
    return nullptr;
  }
  bool LoadCurrentRow(Status* s) override;

 protected:
  std::string EncodeKey(const IndexedDBKey& key) override {
    return ObjectStoreDataKey::Encode(
        cursor_options_.database_id, cursor_options_.object_store_id, key);
  }
  std::string EncodeKey(const IndexedDBKey& key,
                        const IndexedDBKey& primary_key) override {
    NOTREACHED();
    return std::string();
  }

 private:
  explicit ObjectStoreKeyCursorImpl(const ObjectStoreKeyCursorImpl* other)
      : IndexedDBBackingStore::Cursor(other) {}

  DISALLOW_COPY_AND_ASSIGN(ObjectStoreKeyCursorImpl);
};

IndexedDBBackingStore::Cursor::CursorOptions::CursorOptions() {}
IndexedDBBackingStore::Cursor::CursorOptions::CursorOptions(
    const CursorOptions& other) = default;
IndexedDBBackingStore::Cursor::CursorOptions::~CursorOptions() {}
const IndexedDBBackingStore::RecordIdentifier&
IndexedDBBackingStore::Cursor::record_identifier() const {
  return record_identifier_;
}

bool ObjectStoreKeyCursorImpl::LoadCurrentRow(Status* s) {
  StringPiece slice(iterator_->Key());
  ObjectStoreDataKey object_store_data_key;
  if (!ObjectStoreDataKey::Decode(&slice, &object_store_data_key)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    *s = InvalidDBKeyStatus();
    return false;
  }

  current_key_ = object_store_data_key.user_key();

  int64_t version;
  slice = StringPiece(iterator_->Value());
  if (!DecodeVarInt(&slice, &version)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    *s = InternalInconsistencyStatus();
    return false;
  }

  // TODO(jsbell): This re-encodes what was just decoded; try and optimize.
  std::string encoded_key;
  EncodeIDBKey(*current_key_, &encoded_key);
  record_identifier_.Reset(encoded_key, version);

  return true;
}

class ObjectStoreCursorImpl : public IndexedDBBackingStore::Cursor {
 public:
  ObjectStoreCursorImpl(
      scoped_refptr<IndexedDBBackingStore> backing_store,
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      const IndexedDBBackingStore::Cursor::CursorOptions& cursor_options)
      : IndexedDBBackingStore::Cursor(backing_store,
                                      transaction,
                                      database_id,
                                      cursor_options) {}

  std::unique_ptr<Cursor> Clone() const override {
    return base::WrapUnique(new ObjectStoreCursorImpl(this));
  }

  // IndexedDBBackingStore::Cursor
  IndexedDBValue* value() override { return &current_value_; }
  bool LoadCurrentRow(Status* s) override;

 protected:
  std::string EncodeKey(const IndexedDBKey& key) override {
    return ObjectStoreDataKey::Encode(
        cursor_options_.database_id, cursor_options_.object_store_id, key);
  }
  std::string EncodeKey(const IndexedDBKey& key,
                        const IndexedDBKey& primary_key) override {
    NOTREACHED();
    return std::string();
  }

 private:
  explicit ObjectStoreCursorImpl(const ObjectStoreCursorImpl* other)
      : IndexedDBBackingStore::Cursor(other),
        current_value_(other->current_value_) {}

  IndexedDBValue current_value_;

  DISALLOW_COPY_AND_ASSIGN(ObjectStoreCursorImpl);
};

bool ObjectStoreCursorImpl::LoadCurrentRow(Status* s) {
  StringPiece key_slice(iterator_->Key());
  ObjectStoreDataKey object_store_data_key;
  if (!ObjectStoreDataKey::Decode(&key_slice, &object_store_data_key)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    *s = InvalidDBKeyStatus();
    return false;
  }

  current_key_ = object_store_data_key.user_key();

  int64_t version;
  StringPiece value_slice = StringPiece(iterator_->Value());
  if (!DecodeVarInt(&value_slice, &version)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    *s = InternalInconsistencyStatus();
    return false;
  }

  // TODO(jsbell): This re-encodes what was just decoded; try and optimize.
  std::string encoded_key;
  EncodeIDBKey(*current_key_, &encoded_key);
  record_identifier_.Reset(encoded_key, version);

  *s = transaction_->GetBlobInfoForRecord(
      database_id_, iterator_->Key().as_string(), &current_value_);
  if (!s->ok())
    return false;

  current_value_.bits = value_slice.as_string();
  return true;
}

class IndexKeyCursorImpl : public IndexedDBBackingStore::Cursor {
 public:
  IndexKeyCursorImpl(
      scoped_refptr<IndexedDBBackingStore> backing_store,
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      const IndexedDBBackingStore::Cursor::CursorOptions& cursor_options)
      : IndexedDBBackingStore::Cursor(backing_store,
                                      transaction,
                                      database_id,
                                      cursor_options) {}

  std::unique_ptr<Cursor> Clone() const override {
    return base::WrapUnique(new IndexKeyCursorImpl(this));
  }

  // IndexedDBBackingStore::Cursor
  IndexedDBValue* value() override {
    NOTREACHED();
    return nullptr;
  }
  const IndexedDBKey& primary_key() const override { return *primary_key_; }
  const IndexedDBBackingStore::RecordIdentifier& record_identifier()
      const override {
    NOTREACHED();
    return record_identifier_;
  }
  bool LoadCurrentRow(Status* s) override;

 protected:
  std::string EncodeKey(const IndexedDBKey& key) override {
    return IndexDataKey::Encode(cursor_options_.database_id,
                                cursor_options_.object_store_id,
                                cursor_options_.index_id,
                                key);
  }
  std::string EncodeKey(const IndexedDBKey& key,
                        const IndexedDBKey& primary_key) override {
    return IndexDataKey::Encode(cursor_options_.database_id,
                                cursor_options_.object_store_id,
                                cursor_options_.index_id,
                                key,
                                primary_key);
  }

 private:
  explicit IndexKeyCursorImpl(const IndexKeyCursorImpl* other)
      : IndexedDBBackingStore::Cursor(other),
        primary_key_(std::make_unique<IndexedDBKey>(*other->primary_key_)) {}

  std::unique_ptr<IndexedDBKey> primary_key_;

  DISALLOW_COPY_AND_ASSIGN(IndexKeyCursorImpl);
};

bool IndexKeyCursorImpl::LoadCurrentRow(Status* s) {
  StringPiece slice(iterator_->Key());
  IndexDataKey index_data_key;
  if (!IndexDataKey::Decode(&slice, &index_data_key)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    *s = InvalidDBKeyStatus();
    return false;
  }

  current_key_ = index_data_key.user_key();
  DCHECK(current_key_);

  slice = StringPiece(iterator_->Value());
  int64_t index_data_version;
  if (!DecodeVarInt(&slice, &index_data_version)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    *s = InternalInconsistencyStatus();
    return false;
  }

  if (!DecodeIDBKey(&slice, &primary_key_) || !slice.empty()) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
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
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }
  if (!found) {
    transaction_->transaction()->Remove(iterator_->Key());
    return false;
  }
  if (result.empty()) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  int64_t object_store_data_version;
  slice = StringPiece(result);
  if (!DecodeVarInt(&slice, &object_store_data_version)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    *s = InternalInconsistencyStatus();
    return false;
  }

  if (object_store_data_version != index_data_version) {
    transaction_->transaction()->Remove(iterator_->Key());
    return false;
  }

  return true;
}

class IndexCursorImpl : public IndexedDBBackingStore::Cursor {
 public:
  IndexCursorImpl(
      scoped_refptr<IndexedDBBackingStore> backing_store,
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      const IndexedDBBackingStore::Cursor::CursorOptions& cursor_options)
      : IndexedDBBackingStore::Cursor(backing_store,
                                      transaction,
                                      database_id,
                                      cursor_options) {}

  std::unique_ptr<Cursor> Clone() const override {
    return base::WrapUnique(new IndexCursorImpl(this));
  }

  // IndexedDBBackingStore::Cursor
  IndexedDBValue* value() override { return &current_value_; }
  const IndexedDBKey& primary_key() const override { return *primary_key_; }
  const IndexedDBBackingStore::RecordIdentifier& record_identifier()
      const override {
    NOTREACHED();
    return record_identifier_;
  }
  bool LoadCurrentRow(Status* s) override;

 protected:
  std::string EncodeKey(const IndexedDBKey& key) override {
    return IndexDataKey::Encode(cursor_options_.database_id,
                                cursor_options_.object_store_id,
                                cursor_options_.index_id,
                                key);
  }
  std::string EncodeKey(const IndexedDBKey& key,
                        const IndexedDBKey& primary_key) override {
    return IndexDataKey::Encode(cursor_options_.database_id,
                                cursor_options_.object_store_id,
                                cursor_options_.index_id,
                                key,
                                primary_key);
  }

 private:
  explicit IndexCursorImpl(const IndexCursorImpl* other)
      : IndexedDBBackingStore::Cursor(other),
        primary_key_(std::make_unique<IndexedDBKey>(*other->primary_key_)),
        current_value_(other->current_value_),
        primary_leveldb_key_(other->primary_leveldb_key_) {}

  std::unique_ptr<IndexedDBKey> primary_key_;
  IndexedDBValue current_value_;
  std::string primary_leveldb_key_;

  DISALLOW_COPY_AND_ASSIGN(IndexCursorImpl);
};

bool IndexCursorImpl::LoadCurrentRow(Status* s) {
  StringPiece slice(iterator_->Key());
  IndexDataKey index_data_key;
  if (!IndexDataKey::Decode(&slice, &index_data_key)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    *s = InvalidDBKeyStatus();
    return false;
  }

  current_key_ = index_data_key.user_key();
  DCHECK(current_key_);

  slice = StringPiece(iterator_->Value());
  int64_t index_data_version;
  if (!DecodeVarInt(&slice, &index_data_version)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    *s = InternalInconsistencyStatus();
    return false;
  }
  if (!DecodeIDBKey(&slice, &primary_key_)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    *s = InvalidDBKeyStatus();
    return false;
  }

  DCHECK_EQ(index_data_key.DatabaseId(), database_id_);
  primary_leveldb_key_ =
      ObjectStoreDataKey::Encode(index_data_key.DatabaseId(),
                                 index_data_key.ObjectStoreId(),
                                 *primary_key_);

  std::string result;
  bool found = false;
  *s = transaction_->transaction()->Get(primary_leveldb_key_, &result, &found);
  if (!s->ok()) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }
  if (!found) {
    transaction_->transaction()->Remove(iterator_->Key());
    return false;
  }
  if (result.empty()) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    return false;
  }

  int64_t object_store_data_version;
  slice = StringPiece(result);
  if (!DecodeVarInt(&slice, &object_store_data_version)) {
    INTERNAL_READ_ERROR_UNTESTED(LOAD_CURRENT_ROW);
    *s = InternalInconsistencyStatus();
    return false;
  }

  if (object_store_data_version != index_data_version) {
    transaction_->transaction()->Remove(iterator_->Key());
    return false;
  }

  current_value_.bits = slice.as_string();
  *s = transaction_->GetBlobInfoForRecord(database_id_, primary_leveldb_key_,
                                          &current_value_);
  return s->ok();
}

int IndexedDBBackingStore::Comparator::Compare(const StringPiece& a,
                                               const StringPiece& b) const {
  return content::Compare(a, b, false /*index_keys*/);
}

const char* IndexedDBBackingStore::Comparator::Name() const {
  return "idb_cmp1";
}

std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBBackingStore::OpenObjectStoreCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& range,
    blink::WebIDBCursorDirection direction,
    Status* s) {
  IDB_TRACE("IndexedDBBackingStore::OpenObjectStoreCursor");
  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  IndexedDBBackingStore::Cursor::CursorOptions cursor_options;
  // TODO(cmumford): Handle this error (crbug.com/363397)
  if (!ObjectStoreCursorOptions(leveldb_transaction, database_id,
                                object_store_id, range, direction,
                                &cursor_options, s)) {
    return std::unique_ptr<IndexedDBBackingStore::Cursor>();
  }
  std::unique_ptr<ObjectStoreCursorImpl> cursor(
      std::make_unique<ObjectStoreCursorImpl>(this, transaction, database_id,
                                              cursor_options));
  if (!cursor->FirstSeek(s))
    return std::unique_ptr<IndexedDBBackingStore::Cursor>();

  return std::move(cursor);
}

std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBBackingStore::OpenObjectStoreKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& range,
    blink::WebIDBCursorDirection direction,
    Status* s) {
  IDB_TRACE("IndexedDBBackingStore::OpenObjectStoreKeyCursor");
  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  IndexedDBBackingStore::Cursor::CursorOptions cursor_options;
  // TODO(cmumford): Handle this error (crbug.com/363397)
  if (!ObjectStoreCursorOptions(leveldb_transaction, database_id,
                                object_store_id, range, direction,
                                &cursor_options, s)) {
    return std::unique_ptr<IndexedDBBackingStore::Cursor>();
  }
  std::unique_ptr<ObjectStoreKeyCursorImpl> cursor(
      std::make_unique<ObjectStoreKeyCursorImpl>(this, transaction, database_id,
                                                 cursor_options));
  if (!cursor->FirstSeek(s))
    return std::unique_ptr<IndexedDBBackingStore::Cursor>();

  return std::move(cursor);
}

std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBBackingStore::OpenIndexKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& range,
    blink::WebIDBCursorDirection direction,
    Status* s) {
  IDB_TRACE("IndexedDBBackingStore::OpenIndexKeyCursor");
  *s = Status::OK();
  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  IndexedDBBackingStore::Cursor::CursorOptions cursor_options;
  if (!IndexCursorOptions(leveldb_transaction, database_id, object_store_id,
                          index_id, range, direction, &cursor_options, s))
    return std::unique_ptr<IndexedDBBackingStore::Cursor>();
  std::unique_ptr<IndexKeyCursorImpl> cursor(
      std::make_unique<IndexKeyCursorImpl>(this, transaction, database_id,
                                           cursor_options));
  if (!cursor->FirstSeek(s))
    return std::unique_ptr<IndexedDBBackingStore::Cursor>();

  return std::move(cursor);
}

std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBBackingStore::OpenIndexCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& range,
    blink::WebIDBCursorDirection direction,
    Status* s) {
  IDB_TRACE("IndexedDBBackingStore::OpenIndexCursor");
  LevelDBTransaction* leveldb_transaction = transaction->transaction();
  IndexedDBBackingStore::Cursor::CursorOptions cursor_options;
  if (!IndexCursorOptions(leveldb_transaction, database_id, object_store_id,
                          index_id, range, direction, &cursor_options, s))
    return std::unique_ptr<IndexedDBBackingStore::Cursor>();
  std::unique_ptr<IndexCursorImpl> cursor(
      new IndexCursorImpl(this, transaction, database_id, cursor_options));
  if (!cursor->FirstSeek(s))
    return std::unique_ptr<IndexedDBBackingStore::Cursor>();

  return std::move(cursor);
}

void IndexedDBBackingStore::StartPreCloseTasks() {
  DCHECK(pre_close_task_queue_);
  pre_close_task_queue_->Start(base::BindOnce(
      &IndexedDBBackingStore::GetCompleteMetadata, base::Unretained(this)));
}

bool IndexedDBBackingStore::IsBlobCleanupPending() {
  return journal_cleaning_timer_.IsRunning();
}

void IndexedDBBackingStore::ForceRunBlobCleanup() {
  journal_cleaning_timer_.FireNow();
}

IndexedDBBackingStore::Transaction::Transaction(
    IndexedDBBackingStore* backing_store)
    : backing_store_(backing_store),
      database_id_(-1),
      committing_(false),
      ptr_factory_(this) {}

IndexedDBBackingStore::Transaction::~Transaction() {
  DCHECK(!committing_);
}

void IndexedDBBackingStore::Transaction::Begin() {
  IDB_TRACE("IndexedDBBackingStore::Transaction::Begin");
  DCHECK(!transaction_.get());
  transaction_ = IndexedDBClassFactory::Get()->CreateLevelDBTransaction(
      backing_store_->db_.get());

  // If incognito, this snapshots blobs just as the above transaction_
  // constructor snapshots the leveldb.
  for (const auto& iter : backing_store_->incognito_blob_map_)
    incognito_blob_map_[iter.first] = iter.second->Clone();
}

Status IndexedDBBackingStore::Transaction::HandleBlobPreTransaction(
    BlobEntryKeyValuePairVec* new_blob_entries,
    WriteDescriptorVec* new_files_to_write) {
  if (backing_store_->is_incognito())
    return Status::OK();

  DCHECK(new_blob_entries->empty());
  DCHECK(new_files_to_write->empty());
  DCHECK(blobs_to_write_.empty());

  if (blob_change_map_.empty())
    return Status::OK();

  // Create LevelDBTransaction for the name generator seed and add-journal.
  scoped_refptr<LevelDBTransaction> pre_transaction =
      IndexedDBClassFactory::Get()->CreateLevelDBTransaction(
          backing_store_->db_.get());

  for (auto& iter : blob_change_map_) {
    std::vector<IndexedDBBlobInfo*> new_blob_keys;
    for (auto& entry : iter.second->mutable_blob_info()) {
      int64_t next_blob_key = -1;
      bool result = indexed_db::GetBlobKeyGeneratorCurrentNumber(
          pre_transaction.get(), database_id_, &next_blob_key);
      if (!result || next_blob_key < 0)
        return InternalInconsistencyStatus();
      blobs_to_write_.push_back(std::make_pair(database_id_, next_blob_key));
      if (entry.is_file() && !entry.file_path().empty()) {
        new_files_to_write->push_back(
            WriteDescriptor(entry.file_path(), next_blob_key, entry.size(),
                            entry.last_modified()));
      } else {
        new_files_to_write->push_back(
            WriteDescriptor(entry.blob_handle(), next_blob_key, entry.size(),
                            entry.last_modified()));
      }
      entry.set_key(next_blob_key);
      new_blob_keys.push_back(&entry);
      result = indexed_db::UpdateBlobKeyGeneratorCurrentNumber(
          pre_transaction.get(), database_id_, next_blob_key + 1);
      if (!result)
        return InternalInconsistencyStatus();
    }
    BlobEntryKey blob_entry_key;
    StringPiece key_piece(iter.second->key());
    if (!BlobEntryKey::FromObjectStoreDataKey(&key_piece, &blob_entry_key)) {
      NOTREACHED();
      return InternalInconsistencyStatus();
    }
    new_blob_entries->push_back(
        std::make_pair(blob_entry_key, EncodeBlobData(new_blob_keys)));
  }

  AppendBlobsToPrimaryBlobJournal(pre_transaction.get(), blobs_to_write_);
  Status s = pre_transaction->Commit();
  if (!s.ok())
    return InternalInconsistencyStatus();

  return Status::OK();
}

bool IndexedDBBackingStore::Transaction::CollectBlobFilesToRemove() {
  if (backing_store_->is_incognito())
    return true;

  // Look up all old files to remove as part of the transaction, store their
  // names in blobs_to_remove_, and remove their old blob data entries.
  for (const auto& iter : blob_change_map_) {
    BlobEntryKey blob_entry_key;
    StringPiece key_piece(iter.second->key());
    if (!BlobEntryKey::FromObjectStoreDataKey(&key_piece, &blob_entry_key)) {
      NOTREACHED();
      INTERNAL_WRITE_ERROR_UNTESTED(TRANSACTION_COMMIT_METHOD);
      transaction_ = nullptr;
      return false;
    }
    if (database_id_ < 0)
      database_id_ = blob_entry_key.database_id();
    else
      DCHECK_EQ(database_id_, blob_entry_key.database_id());
    std::string blob_entry_key_bytes = blob_entry_key.Encode();
    bool found;
    std::string blob_entry_value_bytes;
    Status s = transaction_->Get(blob_entry_key_bytes, &blob_entry_value_bytes,
                                 &found);
    if (s.ok() && found) {
      std::vector<IndexedDBBlobInfo> blob_info;
      if (!DecodeBlobData(blob_entry_value_bytes, &blob_info)) {
        INTERNAL_READ_ERROR_UNTESTED(TRANSACTION_COMMIT_METHOD);
        transaction_ = nullptr;
        return false;
      }
      for (const auto& blob : blob_info) {
        blobs_to_remove_.push_back(std::make_pair(database_id_, blob.key()));
        transaction_->Remove(blob_entry_key_bytes);
      }
    }
  }
  return true;
}

void IndexedDBBackingStore::Transaction::PartitionBlobsToRemove(
    BlobJournalType* dead_blobs,
    BlobJournalType* live_blobs) const {
  IndexedDBActiveBlobRegistry* registry =
      backing_store_->active_blob_registry();
  for (const auto& iter : blobs_to_remove_) {
    if (registry->MarkDeletedCheckIfUsed(iter.first, iter.second))
      live_blobs->push_back(iter);
    else
      dead_blobs->push_back(iter);
  }
}

Status IndexedDBBackingStore::Transaction::CommitPhaseOne(
    scoped_refptr<BlobWriteCallback> callback) {
  IDB_TRACE("IndexedDBBackingStore::Transaction::CommitPhaseOne");
  DCHECK(transaction_.get());
  DCHECK(backing_store_->task_runner()->RunsTasksInCurrentSequence());

  Status s;

  BlobEntryKeyValuePairVec new_blob_entries;
  WriteDescriptorVec new_files_to_write;
  s = HandleBlobPreTransaction(&new_blob_entries, &new_files_to_write);
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR_UNTESTED(TRANSACTION_COMMIT_METHOD);
    transaction_ = nullptr;
    return s;
  }

  DCHECK(new_files_to_write.empty() ||
         KeyPrefix::IsValidDatabaseId(database_id_));
  if (!CollectBlobFilesToRemove()) {
    INTERNAL_WRITE_ERROR_UNTESTED(TRANSACTION_COMMIT_METHOD);
    transaction_ = nullptr;
    return InternalInconsistencyStatus();
  }

  committing_ = true;
  backing_store_->WillCommitTransaction();

  if (!new_files_to_write.empty()) {
    // This kicks off the writes of the new blobs, if any.
    // This call will zero out new_blob_entries and new_files_to_write.
    WriteNewBlobs(&new_blob_entries, &new_files_to_write, callback);
  } else {
    return callback->Run(BlobWriteResult::SUCCESS_SYNC);
  }

  return Status::OK();
}

Status IndexedDBBackingStore::Transaction::CommitPhaseTwo() {
  IDB_TRACE("IndexedDBBackingStore::Transaction::CommitPhaseTwo");
  Status s;

  DCHECK(committing_);
  committing_ = false;

  backing_store_->DidCommitTransaction();

  BlobJournalType primary_journal, live_journal, saved_primary_journal,
      dead_blobs;
  if (!blob_change_map_.empty()) {
    IDB_TRACE("IndexedDBBackingStore::Transaction.BlobJournal");
    // Read the persisted states of the primary/live blob journals,
    // so that they can be updated correctly by the transaction.
    scoped_refptr<LevelDBTransaction> journal_transaction =
        IndexedDBClassFactory::Get()->CreateLevelDBTransaction(
            backing_store_->db_.get());
    s = GetPrimaryBlobJournal(journal_transaction.get(), &primary_journal);
    if (!s.ok())
      return s;
    s = GetLiveBlobJournal(journal_transaction.get(), &live_journal);
    if (!s.ok())
      return s;

    // Remove newly added blobs from the journal - they will be accounted
    // for in blob entry tables in the transaction.
    std::sort(primary_journal.begin(), primary_journal.end());
    std::sort(blobs_to_write_.begin(), blobs_to_write_.end());
    BlobJournalType new_journal = base::STLSetDifference<BlobJournalType>(
        primary_journal, blobs_to_write_);
    primary_journal.swap(new_journal);

    // Append newly deleted blobs to appropriate primary/live journals.
    saved_primary_journal = primary_journal;
    BlobJournalType live_blobs;
    if (!blobs_to_remove_.empty()) {
      DCHECK(!backing_store_->is_incognito());
      PartitionBlobsToRemove(&dead_blobs, &live_blobs);
    }
    primary_journal.insert(primary_journal.end(), dead_blobs.begin(),
                           dead_blobs.end());
    live_journal.insert(live_journal.end(), live_blobs.begin(),
                        live_blobs.end());
    UpdatePrimaryBlobJournal(transaction_.get(), primary_journal);
    UpdateLiveBlobJournal(transaction_.get(), live_journal);
  }

  // Actually commit. If this succeeds, the journals will appropriately
  // reflect pending blob work - dead files that should be deleted
  // immediately, and live files to monitor.
  s = transaction_->Commit();
  transaction_ = nullptr;

  if (!s.ok()) {
    INTERNAL_WRITE_ERROR(TRANSACTION_COMMIT_METHOD);
    return s;
  }

  if (backing_store_->is_incognito()) {
    if (!blob_change_map_.empty()) {
      auto& target_map = backing_store_->incognito_blob_map_;
      for (auto& iter : blob_change_map_) {
        auto target_record = target_map.find(iter.first);
        if (target_record != target_map.end())
          target_map.erase(target_record);
        if (iter.second)
          target_map[iter.first] = std::move(iter.second);
      }
    }
    return Status::OK();
  }

  // Actually delete dead blob files, then remove those entries
  // from the persisted primary journal.
  if (dead_blobs.empty())
    return Status::OK();

  DCHECK(!blob_change_map_.empty());

  s = backing_store_->CleanUpBlobJournalEntries(dead_blobs);
  if (!s.ok()) {
    INTERNAL_WRITE_ERROR_UNTESTED(TRANSACTION_COMMIT_METHOD);
    return s;
  }

  scoped_refptr<LevelDBTransaction> update_journal_transaction =
      IndexedDBClassFactory::Get()->CreateLevelDBTransaction(
          backing_store_->db_.get());
  UpdatePrimaryBlobJournal(update_journal_transaction.get(),
                           saved_primary_journal);
  s = update_journal_transaction->Commit();
  return s;
}


class IndexedDBBackingStore::Transaction::BlobWriteCallbackWrapper
    : public IndexedDBBackingStore::BlobWriteCallback {
 public:
  BlobWriteCallbackWrapper(
      base::WeakPtr<IndexedDBBackingStore::Transaction> transaction,
      void* tracing_end_ptr,
      scoped_refptr<BlobWriteCallback> callback)
      : transaction_(std::move(transaction)),
        tracing_end_ptr_(tracing_end_ptr),
        callback_(callback) {}
  Status Run(BlobWriteResult result) override {
    DCHECK_NE(result, BlobWriteResult::SUCCESS_SYNC);
    IDB_ASYNC_TRACE_END("IndexedDBBackingStore::Transaction::WriteNewBlobs",
                        tracing_end_ptr_);
    Status leveldb_result = callback_->Run(result);
    switch (result) {
      case BlobWriteResult::FAILURE_ASYNC:
        break;
      case BlobWriteResult::SUCCESS_ASYNC:
      case BlobWriteResult::SUCCESS_SYNC:
        if (transaction_)
          transaction_->chained_blob_writer_ = nullptr;
        break;
    }
    return leveldb_result;
  }

 private:
  ~BlobWriteCallbackWrapper() override {}
  friend class base::RefCounted<IndexedDBBackingStore::BlobWriteCallback>;

  base::WeakPtr<IndexedDBBackingStore::Transaction> transaction_;
  const void* const tracing_end_ptr_;
  scoped_refptr<BlobWriteCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(BlobWriteCallbackWrapper);
};

void IndexedDBBackingStore::Transaction::WriteNewBlobs(
    BlobEntryKeyValuePairVec* new_blob_entries,
    WriteDescriptorVec* new_files_to_write,
    scoped_refptr<BlobWriteCallback> callback) {
  IDB_ASYNC_TRACE_BEGIN("IndexedDBBackingStore::Transaction::WriteNewBlobs",
                        this);
  DCHECK(!new_files_to_write->empty());
  DCHECK_GT(database_id_, 0);
  for (auto& blob_entry_iter : *new_blob_entries) {
    // Add the new blob-table entry for each blob to the main transaction, or
    // remove any entry that may exist if there's no new one.
    if (blob_entry_iter.second.empty())
      transaction_->Remove(blob_entry_iter.first.Encode());
    else
      transaction_->Put(blob_entry_iter.first.Encode(),
                        &blob_entry_iter.second);
  }
  // Creating the writer will start it going asynchronously. The transaction
  // can be destructed before the callback is triggered.
  chained_blob_writer_ = ChainedBlobWriterImpl::Create(
      database_id_, backing_store_, new_files_to_write,
      new BlobWriteCallbackWrapper(ptr_factory_.GetWeakPtr(), this, callback));
}

void IndexedDBBackingStore::Transaction::Rollback() {
  IDB_TRACE("IndexedDBBackingStore::Transaction::Rollback");
  if (committing_) {
    committing_ = false;
    backing_store_->DidCommitTransaction();
  }

  if (chained_blob_writer_.get()) {
    chained_blob_writer_->Abort();
    chained_blob_writer_ = nullptr;
  }
  if (!transaction_)
    return;
  transaction_->Rollback();
  transaction_ = nullptr;
}

uint64_t IndexedDBBackingStore::Transaction::GetTransactionSize() {
  DCHECK(transaction_);
  return transaction_->GetTransactionSize();
}

IndexedDBBackingStore::BlobChangeRecord::BlobChangeRecord(
    const std::string& key,
    int64_t object_store_id)
    : key_(key), object_store_id_(object_store_id) {}

IndexedDBBackingStore::BlobChangeRecord::~BlobChangeRecord() {
}

void IndexedDBBackingStore::BlobChangeRecord::SetBlobInfo(
    std::vector<IndexedDBBlobInfo>* blob_info) {
  blob_info_.clear();
  if (blob_info)
    blob_info_.swap(*blob_info);
}

std::unique_ptr<IndexedDBBackingStore::BlobChangeRecord>
IndexedDBBackingStore::BlobChangeRecord::Clone() const {
  std::unique_ptr<IndexedDBBackingStore::BlobChangeRecord> record(
      new BlobChangeRecord(key_, object_store_id_));
  record->blob_info_ = blob_info_;

  return record;
}

Status IndexedDBBackingStore::Transaction::PutBlobInfoIfNeeded(
    int64_t database_id,
    int64_t object_store_id,
    const std::string& object_store_data_key,
    std::vector<IndexedDBBlobInfo>* blob_info) {
  if (!blob_info || blob_info->empty()) {
    blob_change_map_.erase(object_store_data_key);
    incognito_blob_map_.erase(object_store_data_key);

    BlobEntryKey blob_entry_key;
    StringPiece leveldb_key_piece(object_store_data_key);
    if (!BlobEntryKey::FromObjectStoreDataKey(&leveldb_key_piece,
                                              &blob_entry_key)) {
      NOTREACHED();
      return InternalInconsistencyStatus();
    }
    std::string value;
    bool found = false;
    Status s = transaction()->Get(blob_entry_key.Encode(), &value, &found);
    if (!s.ok())
      return s;
    if (!found)
      return Status::OK();
  }
  PutBlobInfo(database_id, object_store_id, object_store_data_key, blob_info);
  return Status::OK();
}

// This is storing an info, even if empty, even if the previous key had no blob
// info that we know of.  It duplicates a bunch of information stored in the
// leveldb transaction, but only w.r.t. the user keys altered--we don't keep the
// changes to exists or index keys here.
void IndexedDBBackingStore::Transaction::PutBlobInfo(
    int64_t database_id,
    int64_t object_store_id,
    const std::string& object_store_data_key,
    std::vector<IndexedDBBlobInfo>* blob_info) {
  DCHECK(!object_store_data_key.empty());
  if (database_id_ < 0)
    database_id_ = database_id;
  DCHECK_EQ(database_id_, database_id);

  const auto& it = blob_change_map_.find(object_store_data_key);
  BlobChangeRecord* record = nullptr;
  if (it == blob_change_map_.end()) {
    std::unique_ptr<BlobChangeRecord> new_record =
        std::make_unique<BlobChangeRecord>(object_store_data_key,
                                           object_store_id);
    record = new_record.get();
    blob_change_map_[object_store_data_key] = std::move(new_record);
  } else {
    record = it->second.get();
  }
  DCHECK_EQ(record->object_store_id(), object_store_id);
  record->SetBlobInfo(blob_info);
}

IndexedDBBackingStore::Transaction::WriteDescriptor::WriteDescriptor(
    const storage::BlobDataHandle* blob,
    int64_t key,
    int64_t size,
    base::Time last_modified)
    : is_file_(false),
      blob_(*blob),
      key_(key),
      size_(size),
      last_modified_(last_modified) {}

IndexedDBBackingStore::Transaction::WriteDescriptor::WriteDescriptor(
    const FilePath& file_path,
    int64_t key,
    int64_t size,
    base::Time last_modified)
    : is_file_(true),
      file_path_(file_path),
      key_(key),
      size_(size),
      last_modified_(last_modified) {
}

IndexedDBBackingStore::Transaction::WriteDescriptor::WriteDescriptor(
    const WriteDescriptor& other) = default;
IndexedDBBackingStore::Transaction::WriteDescriptor::~WriteDescriptor() =
    default;
IndexedDBBackingStore::Transaction::WriteDescriptor&
IndexedDBBackingStore::Transaction::WriteDescriptor::operator=(
    const WriteDescriptor& other) = default;

}  // namespace content
