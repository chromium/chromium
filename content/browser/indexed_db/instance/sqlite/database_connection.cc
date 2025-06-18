// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/database_connection.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/record.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_cursor_impl.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_transaction_impl.h"
#include "content/browser/indexed_db/instance/sqlite/record_iterator.h"
#include "content/browser/indexed_db/status.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

// TODO(crbug.com/40253999): Rename the file to indicate that it contains
// backend-agnostic utils to encode/decode IDB types, and potentially move the
// (Encode/Decode)KeyPath methods below to this file.
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"

// TODO(crbug.com/40253999): Remove after handling all error cases.
#define TRANSIENT_CHECK(condition) CHECK(condition)

namespace content::indexed_db::sqlite {
namespace {

// The separator used to join the strings when encoding an `IndexedDBKeyPath` of
// type array. Spaces are not allowed in the individual strings, which makes
// this a convenient choice.
constexpr char16_t kKeyPathSeparator[] = u" ";

// Encodes `key_path` into a string. The key path can be either a string or an
// array of strings. If it is an array, the contents are joined with
// `kKeyPathSeparator`.
std::u16string EncodeKeyPath(const blink::IndexedDBKeyPath& key_path) {
  switch (key_path.type()) {
    case blink::mojom::IDBKeyPathType::Null:
      return std::u16string();
    case blink::mojom::IDBKeyPathType::String:
      return key_path.string();
    case blink::mojom::IDBKeyPathType::Array:
      return base::JoinString(key_path.array(), kKeyPathSeparator);
    default:
      NOTREACHED();
  }
}
blink::IndexedDBKeyPath DecodeKeyPath(const std::u16string& encoded) {
  if (encoded.empty()) {
    return blink::IndexedDBKeyPath();
  }
  std::vector<std::u16string> parts = base::SplitString(
      encoded, kKeyPathSeparator, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() > 1) {
    return blink::IndexedDBKeyPath(std::move(parts));
  }
  return blink::IndexedDBKeyPath(std::move(parts.front()));
}

// These are schema versions of our implementation of `sql::Database`; not the
// version supplied by the application for the IndexedDB database.
//
// The version used to initialize the meta table for the first time.
constexpr int kEmptySchemaVersion = 1;
constexpr int kCurrentSchemaVersion = 10;
constexpr int kCompatibleSchemaVersion = kCurrentSchemaVersion;

// Atomically creates the current schema for a new `db`, inserts the initial
// IndexedDB metadata entry with `name`, and sets the current version in
// `meta_table`.
void InitializeNewDatabase(sql::Database* db,
                           const std::u16string& name,
                           sql::MetaTable* meta_table) {
  sql::Transaction transaction(db);
  TRANSIENT_CHECK(transaction.Begin());

  // Create the tables.
  //
  // Note on the schema: The IDB spec defines the "name"
  // (https://www.w3.org/TR/IndexedDB/#name) of the database, object stores and
  // indexes as an arbitrary sequence of 16-bit code units, which implies that
  // the application-supplied name strings need not be valid UTF-16.
  // "key_path"s are always valid UTF-16 since they contain only identifiers
  // (required to be valid UTF-16) and periods.
  // However, to avoid unnecessary conversion from UTF-16 to UTF-8 and back, we
  // store all application-supplied strings as BLOBs.
  //
  // Stores a single row containing the properties of
  // `IndexedDBDatabaseMetadata` for this database.
  TRANSIENT_CHECK(
      db->Execute("CREATE TABLE indexed_db_metadata "
                  "(name BLOB NOT NULL,"
                  " version INTEGER NOT NULL)"));
  TRANSIENT_CHECK(
      db->Execute("CREATE TABLE object_stores "
                  "(id INTEGER PRIMARY KEY,"
                  " name BLOB NOT NULL UNIQUE,"
                  " key_path BLOB,"
                  " auto_increment INTEGER NOT NULL,"
                  " key_generator_current_number INTEGER NOT NULL)"));
  TRANSIENT_CHECK(
      db->Execute("CREATE TABLE records "
                  "(row_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                  " object_store_id INTEGER NOT NULL,"
                  " key BLOB NOT NULL,"
                  " value BLOB NOT NULL,"
                  " UNIQUE (object_store_id, key))"));

  // This table store blob metadata and its actual bytes. A blob should only
  // appear once, regardless of how many records point to it. The columns in
  // this table should be effectively const, as SQLite blob handles will be used
  // to stream out of the table, and the associated row must never change while
  // blob handles are active. Blobs will be removed from this table when no
  // references remain (see `blob_references`).
  //
  // TODO(crbug.com/419208485): consider taking into account the blob's UUID to
  // further avoid duplication.
  TRANSIENT_CHECK(db->Execute(
      "CREATE TABLE blobs "
      // This row id will be used as the IndexedDBExternalObject::blob_number_.
      "(row_id INTEGER PRIMARY KEY AUTOINCREMENT, "
      // Corresponds to `IndexedDBExternalObject::ObjectType`.
      " object_type INTEGER NOT NULL,"
      " mime_type TEXT NOT NULL,"
      " size_bytes INTEGER NOT NULL,"
      // This can be null if the blob is stored on disk, which will be the
      // case for legacy blobs.
      " bytes BLOB,"
      " file_name BLOB,"         // only for files
      " last_modified INTEGER)"  // only for files
      ));

  // Blobs may be referenced by rows in `records` or by active connections to
  // clients.
  TRANSIENT_CHECK(
      db->Execute("CREATE TABLE blob_references "
                  "(row_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                  " blob_row_id INTEGER NOT NULL,"
                  // record_row_id will be null when the reference corresponds
                  // to an active blob reference (represented in the browser by
                  // ActiveBlobStreamer). Otherwise it will be the id of the
                  // record row that holds the reference.
                  " record_row_id INTEGER)"));

  TRANSIENT_CHECK(db->Execute(
      "CREATE TRIGGER delete_blob_references AFTER DELETE ON records "
      "BEGIN"
      "  DELETE FROM blob_references WHERE record_row_id = old.row_id; "
      "END"));
  // TODO(crbug.com/419208485): enable recursive triggers.
  TRANSIENT_CHECK(db->Execute(
      "CREATE TRIGGER delete_unreferenced_blobs"
      "  AFTER DELETE ON blob_references "
      "WHEN NOT EXISTS "
      "  (SELECT 1 FROM blob_references WHERE blob_row_id = old.blob_row_id) "
      "BEGIN"
      "  DELETE FROM blobs WHERE row_id = old.blob_row_id; "
      "END"));

  // Insert the initial metadata entry.
  sql::Statement statement(
      db->GetUniqueStatement("INSERT INTO indexed_db_metadata "
                             "(name, version) VALUES (?, ?)"));
  statement.BindBlob(0, name);
  statement.BindInt64(1, blink::IndexedDBDatabaseMetadata::NO_VERSION);
  TRANSIENT_CHECK(statement.Run());

  // Set the current version in the meta table.
  TRANSIENT_CHECK(meta_table->SetVersionNumber(kCurrentSchemaVersion));

  TRANSIENT_CHECK(transaction.Commit());
}

blink::IndexedDBDatabaseMetadata GenerateIndexedDbMetadata(sql::Database* db) {
  blink::IndexedDBDatabaseMetadata metadata;

  // Set the database name and version.
  {
    sql::Statement statement(db->GetReadonlyStatement(
        "SELECT name, version FROM indexed_db_metadata"));
    TRANSIENT_CHECK(statement.Step());
    TRANSIENT_CHECK(statement.ColumnBlobAsString16(0, &metadata.name));
    metadata.version = statement.ColumnInt64(1);
  }

  // Populate object store metadata.
  {
    sql::Statement statement(db->GetReadonlyStatement(
        "SELECT id, name, key_path, auto_increment FROM object_stores"));
    int64_t max_object_store_id = 0;
    while (statement.Step()) {
      blink::IndexedDBObjectStoreMetadata store_metadata;
      store_metadata.id = statement.ColumnInt64(0);
      TRANSIENT_CHECK(statement.ColumnBlobAsString16(1, &store_metadata.name));
      std::u16string encoded_key_path;
      TRANSIENT_CHECK(statement.ColumnBlobAsString16(2, &encoded_key_path));
      store_metadata.key_path = DecodeKeyPath(encoded_key_path);
      store_metadata.auto_increment = statement.ColumnBool(3);
      max_object_store_id = std::max(max_object_store_id, store_metadata.id);
      metadata.object_stores[store_metadata.id] = std::move(store_metadata);
    }
    TRANSIENT_CHECK(statement.Succeeded());
    metadata.max_object_store_id = max_object_store_id;
  }

  return metadata;
}

// Returns a `RecordIterator` and the initial `Record` for the range of object
// store records determined by the parameters. Returns nullptrs if the range is
// empty or an error if one occurs.
StatusOr<std::pair<std::unique_ptr<RecordIterator>, std::unique_ptr<Record>>>
CreateObjectStoreRecordIterator(sql::Database* db,
                                int64_t object_store_id,
                                const blink::IndexedDBKeyRange& key_range,
                                bool ascending_order,
                                bool key_only) {
  std::vector<std::string_view> query_pieces{
      "SELECT ", key_only ? "key" : "key, value",
      " FROM records WHERE object_store_id = @object_store_id"};
  if (key_range.lower().IsValid()) {
    query_pieces.push_back(key_range.lower_open() ? " AND key > @lower"
                                                  : " AND key >= @lower");
  }
  if (key_range.upper().IsValid()) {
    query_pieces.push_back(key_range.upper_open() ? " AND key < @upper"
                                                  : " AND key <= @upper");
  }
  if (ascending_order) {
    query_pieces.push_back(
        " AND (@is_first_seek = 1 OR key > @position)"
        " AND (@target_key IS NULL OR key >= @target_key)"
        " ORDER BY key ASC");
  } else {
    query_pieces.push_back(
        " AND (@is_first_seek = 1 OR key < @position)"
        " AND (@target_key IS NULL OR key <= @target_key)"
        " ORDER BY key DESC");
  }
  // LIMIT is needed to use OFFSET. A negative LIMIT implies no limit on the
  // number of rows returned:
  // https://www.sqlite.org/lang_select.html#the_limit_clause.
  query_pieces.push_back(" LIMIT -1 OFFSET @offset");

  auto statement = std::make_unique<sql::Statement>(
      db->GetReadonlyStatement(base::StrCat(query_pieces)));
  int param_index = 0;
  statement->BindInt64(param_index++, object_store_id);
  if (key_range.lower().IsValid()) {
    statement->BindBlob(param_index++, EncodeSortableIDBKey(key_range.lower()));
  }
  if (key_range.upper().IsValid()) {
    statement->BindBlob(param_index++, EncodeSortableIDBKey(key_range.upper()));
  }
  int is_first_seek_index = param_index++;
  int position_index = param_index++;
  int target_key_index = param_index++;
  int offset_index = param_index++;

  RecordIterator::BindCallback bind_parameters = base::BindRepeating(
      [](int is_first_seek_index, int position_index, int target_key_index,
         int offset_index, sql::Statement& statement,
         const std::string& position, const blink::IndexedDBKey& target_key,
         const blink::IndexedDBKey& _, uint32_t offset) {
        statement.BindBool(is_first_seek_index, false);
        statement.BindBlob(position_index, position);
        if (target_key.IsValid()) {
          statement.BindBlob(target_key_index,
                             EncodeSortableIDBKey(target_key));
        } else {
          statement.BindNull(target_key_index);
        }
        statement.BindInt64(offset_index, offset);
      },
      is_first_seek_index, position_index, target_key_index, offset_index);

  RecordIterator::ReadCallback read_row = base::BindRepeating(
      [](bool key_only, sql::Statement& statement)
          -> StatusOr<RecordIterator::PositionAndRecord> {
        std::string position;
        TRANSIENT_CHECK(statement.ColumnBlobAsString(0, &position));
        blink::IndexedDBKey key = DecodeSortableIDBKey(position);
        if (key_only) {
          return std::make_pair(
              std::move(position),
              std::make_unique<ObjectStoreKeyOnlyRecord>(std::move(key)));
        }
        IndexedDBValue value;
        TRANSIENT_CHECK(statement.ColumnBlobAsVector(1, &value.bits));
        return std::make_pair(std::move(position),
                              std::make_unique<ObjectStoreRecord>(
                                  std::move(key), std::move(value)));
      },
      key_only);

  // Attempt to find the initial record in the range.
  statement->BindBool(is_first_seek_index, true);
  statement->BindNull(position_index);
  statement->BindNull(target_key_index);
  statement->BindInt(offset_index, 0);
  if (!statement->Step()) {
    TRANSIENT_CHECK(statement->Succeeded());
    // Empty range.
    return std::make_pair(nullptr, nullptr);
  }
  return read_row.Run(*statement)
      .transform([&](RecordIterator::PositionAndRecord result) {
        return std::make_pair(
            std::make_unique<RecordIterator>(
                std::move(statement), std::move(bind_parameters),
                std::move(read_row), std::move(result.first)),
            std::move(result.second));
      });
}

}  // namespace

// static
StatusOr<std::unique_ptr<DatabaseConnection>> DatabaseConnection::Open(
    const std::u16string& name,
    const base::FilePath& file_path,
    BackingStoreImpl& backing_store) {
  // TODO(crbug.com/40253999): Create new tag(s) for metrics.
  constexpr sql::Database::Tag kSqlTag = "Test";
  auto db = std::make_unique<sql::Database>(
      sql::DatabaseOptions().set_exclusive_locking(true).set_wal_mode(true),
      kSqlTag);

  // TODO(crbug.com/40253999): Support on-disk databases.
  TRANSIENT_CHECK(db->OpenInMemory());

  auto meta_table = std::make_unique<sql::MetaTable>();
  TRANSIENT_CHECK(meta_table->Init(db.get(), kEmptySchemaVersion,
                                   kCompatibleSchemaVersion));

  switch (meta_table->GetVersionNumber()) {
    case kEmptySchemaVersion:
      InitializeNewDatabase(db.get(), name, meta_table.get());
      break;
    // ...
    // Schema upgrades go here.
    // ...
    case kCurrentSchemaVersion:
      // Already current.
      break;
    default:
      NOTREACHED();
  }

  blink::IndexedDBDatabaseMetadata metadata =
      GenerateIndexedDbMetadata(db.get());
  // Database corruption can cause a mismatch.
  TRANSIENT_CHECK(metadata.name == name);

  return base::WrapUnique(
      new DatabaseConnection(std::move(db), std::move(meta_table),
                             std::move(metadata), backing_store));
}

DatabaseConnection::DatabaseConnection(
    std::unique_ptr<sql::Database> db,
    std::unique_ptr<sql::MetaTable> meta_table,
    blink::IndexedDBDatabaseMetadata metadata,
    BackingStoreImpl& backing_store)
    : db_(std::move(db)),
      meta_table_(std::move(meta_table)),
      metadata_(std::move(metadata)),
      backing_store_(backing_store) {
  // There should be no active blobs in this database at this point, so we can
  // remove blob references that were associated with active blobs. These may
  // have been left behind if Chromium crashed. Deleting the blob references
  // should also delete the blob if appropriate.
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM blob_references WHERE record_row_id IS NULL"));
  TRANSIENT_CHECK(statement.Run());
}

DatabaseConnection::~DatabaseConnection() = default;

base::WeakPtr<DatabaseConnection> DatabaseConnection::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<BackingStoreTransactionImpl>
DatabaseConnection::CreateTransaction(
    base::PassKey<BackingStoreDatabaseImpl>,
    blink::mojom::IDBTransactionDurability durability,
    blink::mojom::IDBTransactionMode mode) {
  return std::make_unique<BackingStoreTransactionImpl>(GetWeakPtr(), durability,
                                                       mode);
}

void DatabaseConnection::BeginTransaction(
    base::PassKey<BackingStoreTransactionImpl>,
    const BackingStoreTransactionImpl& transaction) {
  // No other transaction can begin while a version change transaction is
  // active.
  CHECK(!HasActiveVersionChangeTransaction());
  if (transaction.mode() == blink::mojom::IDBTransactionMode::ReadOnly) {
    // Nothing to do.
    return;
  }
  CHECK(!active_rw_transaction_);
  active_rw_transaction_ = std::make_unique<sql::Transaction>(db_.get());
  // TODO(crbug.com/40253999): Set the appropriate value for `PRAGMA
  // synchronous` based on `transaction.durability()`.
  // TODO(crbug.com/40253999): How do we surface the error if this call fails?
  TRANSIENT_CHECK(active_rw_transaction_->Begin());
  if (transaction.mode() == blink::mojom::IDBTransactionMode::VersionChange) {
    metadata_snapshot_.emplace(metadata_);
  }
}

Status DatabaseConnection::CommitTransactionPhaseOne(
    base::PassKey<BackingStoreTransactionImpl>,
    const BackingStoreTransactionImpl& transaction,
    BlobWriteCallback callback) {
  if (transaction.mode() == blink::mojom::IDBTransactionMode::ReadOnly ||
      blobs_to_write_.empty()) {
    return std::move(callback).Run(
        BlobWriteResult::kRunPhaseTwoAndReturnResult,
        storage::mojom::WriteBlobToFileResult::kSuccess);
  }

  CHECK(blob_write_callback_.is_null());
  CHECK(blob_writers_.empty());

  blob_write_callback_ = std::move(callback);

  auto blobs_to_write = std::move(blobs_to_write_);
  for (auto& [blob_row_id, external_object] : blobs_to_write) {
    std::optional<sql::StreamingBlobHandle> blob_for_writing =
        db_->GetStreamingBlob("blobs", "bytes", blob_row_id,
                              /*readonly=*/false);
    TRANSIENT_CHECK(blob_for_writing);
    std::unique_ptr<BlobWriter> writer = BlobWriter::WriteBlobIntoDatabase(
        external_object, *std::move(blob_for_writing),
        base::BindOnce(&DatabaseConnection::OnBlobWriteComplete,
                       blob_writers_weak_factory_.GetWeakPtr(), blob_row_id));
    if (!writer) {
      blob_writers_.clear();
      return std::move(blob_write_callback_)
          .Run(BlobWriteResult::kRunPhaseTwoAndReturnResult,
               storage::mojom::WriteBlobToFileResult::kError);
    }

    blob_writers_[blob_row_id] = std::move(writer);
  }

  return Status::OK();
}

void DatabaseConnection::OnBlobWriteComplete(int64_t blob_row_id,
                                             bool success) {
  CHECK_EQ(blob_writers_.erase(blob_row_id), 1U);

  if (!success) {
    blob_writers_weak_factory_.InvalidateWeakPtrs();
    blob_writers_.clear();
    std::move(blob_write_callback_)
        .Run(BlobWriteResult::kRunPhaseTwoAsync,
             storage::mojom::WriteBlobToFileResult::kError);
    return;
  }

  if (blob_writers_.empty()) {
    std::move(blob_write_callback_)
        .Run(BlobWriteResult::kRunPhaseTwoAsync,
             storage::mojom::WriteBlobToFileResult::kSuccess);
  }
}

Status DatabaseConnection::CommitTransactionPhaseTwo(
    base::PassKey<BackingStoreTransactionImpl>,
    const BackingStoreTransactionImpl& transaction) {
  if (transaction.mode() == blink::mojom::IDBTransactionMode::ReadOnly) {
    // Nothing to do.
    return Status::OK();
  }
  TRANSIENT_CHECK(active_rw_transaction_->Commit());
  active_rw_transaction_.reset();
  if (transaction.mode() == blink::mojom::IDBTransactionMode::VersionChange) {
    CHECK(metadata_snapshot_.has_value());
    metadata_snapshot_.reset();
  }
  return Status::OK();
}

void DatabaseConnection::RollBackTransaction(
    base::PassKey<BackingStoreTransactionImpl>,
    const BackingStoreTransactionImpl& transaction) {
  if (transaction.mode() == blink::mojom::IDBTransactionMode::ReadOnly) {
    // Nothing to do.
    return;
  }

  // Abort ongoing blob writes, if any.
  // TODO(crbug.com/419208485): Be sure to test this case.
  blob_writers_.clear();
  blob_write_callback_ = BlobWriteCallback();

  active_rw_transaction_->Rollback();
  active_rw_transaction_.reset();

  if (transaction.mode() == blink::mojom::IDBTransactionMode::VersionChange) {
    CHECK(metadata_snapshot_.has_value());
    metadata_ = std::move(*metadata_snapshot_);
    metadata_snapshot_.reset();
  }
}

Status DatabaseConnection::SetDatabaseVersion(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t version) {
  CHECK(HasActiveVersionChangeTransaction());
  sql::Statement statement(
      db_->GetUniqueStatement("UPDATE indexed_db_metadata SET version = ?"));
  statement.BindInt64(0, version);
  TRANSIENT_CHECK(statement.Run());
  metadata_.version = version;
  return Status::OK();
}

Status DatabaseConnection::CreateObjectStore(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    std::u16string name,
    blink::IndexedDBKeyPath key_path,
    bool auto_increment) {
  CHECK(HasActiveVersionChangeTransaction());
  if (metadata_.object_stores.contains(object_store_id)) {
    return Status::InvalidArgument("Invalid object_store_id");
  }
  TRANSIENT_CHECK(object_store_id > metadata_.max_object_store_id);

  blink::IndexedDBObjectStoreMetadata metadata(
      std::move(name), object_store_id, std::move(key_path), auto_increment,
      /*max_index_id=*/0);
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO object_stores "
      "(id, name, key_path, auto_increment, key_generator_current_number) "
      "VALUES (?, ?, ?, ?, ?)"));
  statement.BindInt64(0, metadata.id);
  statement.BindBlob(1, metadata.name);
  statement.BindBlob(2, EncodeKeyPath(metadata.key_path));
  statement.BindBool(3, metadata.auto_increment);
  statement.BindInt64(4, ObjectStoreMetaDataKey::kKeyGeneratorInitialNumber);
  TRANSIENT_CHECK(statement.Run());

  metadata_.object_stores[object_store_id] = std::move(metadata);
  metadata_.max_object_store_id = object_store_id;
  return Status::OK();
}

StatusOr<int64_t> DatabaseConnection::GetKeyGeneratorCurrentNumber(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id) {
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT key_generator_current_number "
                              "FROM object_stores WHERE id = ?"));
  statement.BindInt64(0, object_store_id);
  TRANSIENT_CHECK(statement.Step());
  return statement.ColumnInt64(0);
}

Status DatabaseConnection::MaybeUpdateKeyGeneratorCurrentNumber(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    int64_t new_number) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE object_stores SET key_generator_current_number = ? "
      "WHERE id = ? AND key_generator_current_number < ?"));
  statement.BindInt64(0, new_number);
  statement.BindInt64(1, object_store_id);
  statement.BindInt64(2, new_number);
  TRANSIENT_CHECK(statement.Run());
  return Status::OK();
}

StatusOr<std::optional<BackingStore::RecordIdentifier>>
DatabaseConnection::GetRecordIdentifierIfExists(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    const blink::IndexedDBKey& key) {
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT row_id FROM records "
                              "WHERE object_store_id = ? AND key = ?"));
  statement.BindInt64(0, object_store_id);
  statement.BindBlob(1, EncodeSortableIDBKey(key));
  if (statement.Step()) {
    return BackingStore::RecordIdentifier{statement.ColumnInt64(0)};
  }
  TRANSIENT_CHECK(statement.Succeeded());
  return std::nullopt;
}

StatusOr<IndexedDBValue> DatabaseConnection::GetValue(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    const blink::IndexedDBKey& key) {
  IndexedDBValue value;
  int64_t record_row_id;

  {
    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "SELECT row_id, value FROM records "
                                "WHERE object_store_id = ? AND key = ?"));
    statement.BindInt64(0, object_store_id);
    statement.BindBlob(1, EncodeSortableIDBKey(key));
    if (!statement.Step()) {
      TRANSIENT_CHECK(statement.Succeeded());
      return IndexedDBValue();
    }
    record_row_id = statement.ColumnInt64(0);
    TRANSIENT_CHECK(statement.ColumnBlobAsVector(1, &value.bits));
  }

  {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT "
        "  blobs.row_id, mime_type, size_bytes "
        "FROM blobs INNER JOIN blob_references"
        "  ON blob_references.blob_row_id = blobs.row_id "
        "WHERE"
        "  blob_references.record_row_id = ?"));
    statement.BindInt64(0, record_row_id);
    while (statement.Step()) {
      const int64_t blob_row_id = statement.ColumnInt64(0);
      if (auto it = blobs_to_write_.find(blob_row_id);
          it != blobs_to_write_.end()) {
        // If the blob is being written in this transaction, copy the external
        // object (and later the Blob mojo endpoint) from `blobs_to_write_`.
        value.external_objects.emplace_back(it->second);
      } else {
        // Otherwise, create a new `IndexedDBExternalObject` from the
        // database.
        value.external_objects.emplace_back(
            statement.ColumnString16(1), statement.ColumnInt64(2), blob_row_id);
      }
    }
  }

  return std::move(value);
}

StatusOr<BackingStore::RecordIdentifier> DatabaseConnection::PutRecord(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    const blink::IndexedDBKey& key,
    IndexedDBValue value) {
  // Insert record, including inline data.
  {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT OR REPLACE INTO records "
        "(object_store_id, key, value) VALUES (?, ?, ?)"));
    statement.BindInt64(0, object_store_id);
    statement.BindBlob(1, EncodeSortableIDBKey(key));
    statement.BindBlob(2, std::move(value.bits));
    TRANSIENT_CHECK(statement.Run());
  }
  const int64_t record_row_id = db_->GetLastInsertRowId();

  // Insert external objects into relevant tables.
  for (auto& external_object : value.external_objects) {
    // TODO(crbug.com/419208485): Support other types of external objects.
    TRANSIENT_CHECK(external_object.object_type() ==
                    IndexedDBExternalObject::ObjectType::kBlob);
    // Reserve space in the blob table. It's not actually written yet though.
    {
      sql::Statement statement(
          db_->GetCachedStatement(SQL_FROM_HERE,
                                  "INSERT INTO blobs "
                                  "(object_type, mime_type, size_bytes, "
                                  "bytes, file_name, last_modified) "
                                  "VALUES (?, ?, ?, ?, ?, ?)"));
      statement.BindInt(0, static_cast<int>(external_object.object_type()));
      statement.BindString16(1, external_object.type());
      statement.BindInt64(2, external_object.size());
      statement.BindBlobForStreaming(3, external_object.size());
      statement.BindNull(4);
      statement.BindNull(5);
      TRANSIENT_CHECK(statement.Run());
    }

    const int64_t blob_row_id = db_->GetLastInsertRowId();
    external_object.set_blob_number(blob_row_id);

    // Store the reference.
    {
      sql::Statement statement(
          db_->GetCachedStatement(SQL_FROM_HERE,
                                  "INSERT INTO blob_references "
                                  "(blob_row_id, record_row_id) "
                                  "VALUES (?, ?)"));
      statement.BindInt64(0, blob_row_id);
      statement.BindInt64(1, record_row_id);
      TRANSIENT_CHECK(statement.Run());
    }

    // TODO(crbug.com/419208485): Consider writing the blobs eagerly (but still
    // asynchronously) so that transaction commit is expedited.
    auto rv = blobs_to_write_.emplace(blob_row_id,
                                      // TODO(crbug.com/419208485): this type is
                                      // copy only at the moment.
                                      std::move(external_object));
    CHECK(rv.second);
  }
  return BackingStore::RecordIdentifier{record_row_id};
}

StatusOr<uint32_t> DatabaseConnection::GetObjectStoreKeyCount(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    blink::IndexedDBKeyRange key_range) {
  std::vector<std::string_view> query_pieces{
      "SELECT COUNT() FROM records WHERE object_store_id = ?"};
  if (key_range.lower().IsValid()) {
    query_pieces.push_back(key_range.lower_open() ? " AND key > ?"
                                                  : " AND key >= ?");
  }
  if (key_range.upper().IsValid()) {
    query_pieces.push_back(key_range.upper_open() ? " AND key < ?"
                                                  : " AND key <= ?");
  }

  // TODO(crbug.com/40253999): Evaluate performance benefit of using
  // `GetCachedStatement()` instead.
  sql::Statement statement(
      db_->GetReadonlyStatement(base::StrCat(query_pieces)));
  int param_index = 0;
  statement.BindInt64(param_index++, object_store_id);
  if (key_range.lower().IsValid()) {
    statement.BindBlob(param_index++, EncodeSortableIDBKey(key_range.lower()));
  }
  if (key_range.upper().IsValid()) {
    statement.BindBlob(param_index++, EncodeSortableIDBKey(key_range.upper()));
  }
  TRANSIENT_CHECK(statement.Step());
  return statement.ColumnInt(0);
}

std::vector<blink::mojom::IDBExternalObjectPtr>
DatabaseConnection::CreateAllExternalObjects(
    base::PassKey<BackingStoreTransactionImpl>,
    const std::vector<IndexedDBExternalObject>& objects) {
  std::vector<blink::mojom::IDBExternalObjectPtr> mojo_objects;
  IndexedDBExternalObject::ConvertToMojo(objects, &mojo_objects);

  for (size_t i = 0; i < objects.size(); ++i) {
    const IndexedDBExternalObject& object = objects[i];
    blink::mojom::IDBExternalObjectPtr& mojo_object = mojo_objects[i];
    if (object.object_type() != IndexedDBExternalObject::ObjectType::kBlob) {
      NOTIMPLEMENTED();
      continue;
    }
    mojo::PendingReceiver<blink::mojom::Blob> receiver =
        mojo_object->get_blob_or_file()->blob.InitWithNewPipeAndPassReceiver();
    // The remote will be valid if this is a pending blob i.e. came from
    // `blobs_to_write_`.
    if (object.is_remote_valid()) {
      object.Clone(std::move(receiver));
      continue;
    }

    // Otherwise the blob is in the database already. Look up or create the
    // object that manages the active blob.
    auto it = active_blobs_.find(object.blob_number());
    if (it == active_blobs_.end()) {
      std::optional<sql::StreamingBlobHandle> blob_for_reading =
          db_->GetStreamingBlob("blobs", "bytes", object.blob_number(),
                                /*readonly=*/true);
      TRANSIENT_CHECK(blob_for_reading);
      auto streamer = std::make_unique<ActiveBlobStreamer>(
          object, *std::move(blob_for_reading),
          base::BindOnce(&DatabaseConnection::OnBlobBecameInactive,
                         base::Unretained(this), object.blob_number()));
      it = active_blobs_.insert({object.blob_number(), std::move(streamer)})
               .first;

      {
        sql::Statement statement(db_->GetCachedStatement(
            SQL_FROM_HERE,
            "INSERT INTO blob_references (blob_row_id) VALUES (?)"));
        statement.BindInt64(0, object.blob_number());
        TRANSIENT_CHECK(statement.Run());
      }
    }
    it->second->AddReceiver(std::move(receiver),
                            backing_store_->blob_storage_context());
  }
  return mojo_objects;
}

void DatabaseConnection::OnBlobBecameInactive(int64_t blob_number) {
  CHECK_EQ(active_blobs_.erase(blob_number), 1U);
  {
    // TODO(crbug.com/419208485): If this operation happens in the middle of a
    // r/w txn that is not committed (Chromium crashes or txn gets rolled back),
    // the blob will come back from the dead! `this` should run this statement
    // after any active r/w txn.
    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "DELETE FROM blob_references "
                                "WHERE blob_row_id = ? "
                                "AND record_row_id IS NULL"));
    statement.BindInt64(0, blob_number);
    TRANSIENT_CHECK(statement.Run());
  }
}

StatusOr<std::unique_ptr<BackingStore::Cursor>>
DatabaseConnection::OpenObjectStoreCursor(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction,
    bool key_only) {
  bool ascending_order =
      (direction == blink::mojom::IDBCursorDirection::Next ||
       direction == blink::mojom::IDBCursorDirection::NextNoDuplicate);
  return CreateObjectStoreRecordIterator(db_.get(), object_store_id, key_range,
                                         ascending_order, key_only)
      .transform([](std::pair<std::unique_ptr<RecordIterator>,
                              std::unique_ptr<Record>> result) {
        return result.first
                   ? std::make_unique<BackingStoreCursorImpl>(
                         std::move(result.first), std::move(result.second))
                   // Empty range.
                   : nullptr;
      });
}

}  // namespace content::indexed_db::sqlite
