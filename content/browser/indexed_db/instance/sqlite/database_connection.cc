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
                  " key_path BLOB NOT NULL,"
                  " auto_increment INTEGER NOT NULL,"
                  " key_generator_current_number INTEGER NOT NULL)"));
  // TODO(crbug.com/419203258): Can this be a NO ROWID table?
  TRANSIENT_CHECK(
      db->Execute("CREATE TABLE indexes "
                  "(row_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                  " object_store_id INTEGER NOT NULL,"
                  " id INTEGER NOT NULL,"
                  " name BLOB NOT NULL,"
                  " key_path BLOB NOT NULL,"
                  " is_unique INTEGER NOT NULL,"
                  " multi_entry INTEGER NOT NULL,"
                  " UNIQUE (object_store_id, id),"
                  " UNIQUE (object_store_id, name))"));
  // Stores object store records. The rows are immutable - updating the value
  // for a combination of object_store_id and key is accomplished by deleting
  // the previous row and inserting a new one (see `PutRecord()`).
  TRANSIENT_CHECK(
      db->Execute("CREATE TABLE records "
                  "(row_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                  " object_store_id INTEGER NOT NULL,"
                  " key BLOB NOT NULL,"
                  " value BLOB NOT NULL,"
                  " UNIQUE (object_store_id, key))"));
  // Stores references from index keys to object store records:
  // [object_store_id, index_id, key] -> record_row_id. There should always be
  // one (and only one) row in the records table with row_id = record_row_id.
  TRANSIENT_CHECK(
      db->Execute("CREATE TABLE index_references "
                  "(row_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                  " object_store_id INTEGER NOT NULL,"
                  " index_id INTEGER NOT NULL,"
                  " key BLOB NOT NULL,"
                  " record_row_id INTEGER NOT NULL)"));
  TRANSIENT_CHECK(db->Execute(
      "CREATE TRIGGER delete_index_references AFTER DELETE ON records "
      "BEGIN"
      "  DELETE FROM index_references WHERE record_row_id = OLD.row_id; "
      "END"));

  // This table stores blob metadata and its actual bytes. A blob should only
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
      "  DELETE FROM blob_references WHERE record_row_id = OLD.row_id; "
      "END"));
  TRANSIENT_CHECK(db->Execute(
      "CREATE TRIGGER delete_unreferenced_blobs"
      "  AFTER DELETE ON blob_references "
      "WHEN NOT EXISTS "
      "  (SELECT 1 FROM blob_references WHERE blob_row_id = OLD.blob_row_id) "
      "BEGIN"
      "  DELETE FROM blobs WHERE row_id = OLD.blob_row_id; "
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
      store_metadata.max_index_id = 0;
      max_object_store_id = std::max(max_object_store_id, store_metadata.id);
      metadata.object_stores[store_metadata.id] = std::move(store_metadata);
    }
    TRANSIENT_CHECK(statement.Succeeded());
    metadata.max_object_store_id = max_object_store_id;
  }

  // Populate index metadata.
  {
    sql::Statement statement(db->GetReadonlyStatement(
        "SELECT object_store_id, id, name, key_path, is_unique, multi_entry "
        "FROM indexes"));
    while (statement.Step()) {
      blink::IndexedDBIndexMetadata index_metadata;
      int64_t object_store_id = statement.ColumnInt64(0);
      index_metadata.id = statement.ColumnInt64(1);
      TRANSIENT_CHECK(statement.ColumnBlobAsString16(2, &index_metadata.name));
      std::u16string encoded_key_path;
      TRANSIENT_CHECK(statement.ColumnBlobAsString16(3, &encoded_key_path));
      index_metadata.key_path = DecodeKeyPath(encoded_key_path);
      index_metadata.unique = statement.ColumnBool(4);
      index_metadata.multi_entry = statement.ColumnBool(5);
      blink::IndexedDBObjectStoreMetadata& store_metadata =
          metadata.object_stores[object_store_id];
      store_metadata.max_index_id =
          std::max(store_metadata.max_index_id, index_metadata.id);
      store_metadata.indexes[index_metadata.id] = std::move(index_metadata);
    }
    TRANSIENT_CHECK(statement.Succeeded());
  }

  return metadata;
}

class ObjectStoreRecordIterator : public RecordIterator {
 public:
  ObjectStoreRecordIterator(base::WeakPtr<DatabaseConnection> db, bool key_only)
      : db_(db), key_only_(key_only) {}

  ~ObjectStoreRecordIterator() override {
    if (db_) {
      db_->ReleaseLongLivedStatement(statement_id_);
    }
  }

  // If Initialize() returns an error or nullptr, `this` should be discarded.
  StatusOr<std::unique_ptr<Record>> Initialize(
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      bool ascending_order) {
    std::vector<std::string_view> query_pieces{
        "SELECT ", key_only_ ? "key" : "key, value, row_id",
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

    sql::Statement* statement;
    std::tie(statement_id_, statement) =
        db_->CreateLongLivedStatement(base::StrCat(query_pieces));
    int param_index = 0;
    statement->BindInt64(param_index++, object_store_id);
    if (key_range.lower().IsValid()) {
      statement->BindBlob(param_index++,
                          EncodeSortableIDBKey(key_range.lower()));
    }
    if (key_range.upper().IsValid()) {
      statement->BindBlob(param_index++,
                          EncodeSortableIDBKey(key_range.upper()));
    }

    // Store the variable parameter indexes and attempt to find the initial
    // record in the range.
    statement->BindBool(is_first_seek_index_ = param_index++, true);
    statement->BindNull(position_index_ = param_index++);
    statement->BindNull(target_key_index_ = param_index++);
    statement->BindInt64(offset_index_ = param_index++, 0);
    if (!statement->Step()) {
      TRANSIENT_CHECK(statement->Succeeded());
      // Empty range.
      return nullptr;
    }
    return ReadRow(*statement);
  }

 protected:
  // RecordIterator:
  void BindParameters(sql::Statement& statement,
                      const blink::IndexedDBKey& target_key,
                      const blink::IndexedDBKey& target_primary_key,
                      uint32_t offset) override {
    statement.BindBool(is_first_seek_index_, false);
    statement.BindBlob(position_index_, position_);
    if (target_key.IsValid()) {
      statement.BindBlob(target_key_index_, EncodeSortableIDBKey(target_key));
    } else {
      statement.BindNull(target_key_index_);
    }
    statement.BindInt64(offset_index_, offset);
  }

  StatusOr<std::unique_ptr<Record>> ReadRow(
      sql::Statement& statement) override {
    TRANSIENT_CHECK(statement.ColumnBlobAsString(0, &position_));
    blink::IndexedDBKey key = DecodeSortableIDBKey(position_);
    if (key_only_) {
      return std::make_unique<ObjectStoreKeyOnlyRecord>(std::move(key));
    }
    IndexedDBValue value;
    TRANSIENT_CHECK(statement.ColumnBlobAsVector(1, &value.bits));
    int64_t record_row_id = statement.ColumnInt64(2);
    return std::make_unique<ObjectStoreRecord>(
        std::move(key),
        db_->AddExternalObjectMetadataToValue(std::move(value), record_row_id));
  }

  sql::Statement* GetStatement() override {
    if (!db_) {
      return nullptr;
    }
    return db_->GetLongLivedStatement(statement_id_);
  }

 private:
  base::WeakPtr<DatabaseConnection> db_;

  uint64_t statement_id_ = 0;

  bool key_only_;

  int is_first_seek_index_ = 0;
  int position_index_ = 0;
  int target_key_index_ = 0;
  int offset_index_ = 0;

  // Encoded key from the current record, tracking the position in the range.
  std::string position_;
};

class IndexRecordIterator : public RecordIterator {
 public:
  IndexRecordIterator(base::WeakPtr<DatabaseConnection> db, bool key_only)
      : db_(db), key_only_(key_only) {}

  ~IndexRecordIterator() override {
    if (db_) {
      db_->ReleaseLongLivedStatement(statement_id_);
    }
  }

  // If Initialize() returns an error or nullptr, `this` should be discarded.
  // If `first_primary_keys_only` is true, `this` will iterate over only the
  // first (i.e., smallest) primary key for each index key in `key_range`. Else,
  // all the primary keys are iterated over for each index key in the range.
  StatusOr<std::unique_ptr<Record>> Initialize(
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      bool ascending_order,
      bool first_primary_keys_only) {
    std::vector<std::string_view> query_pieces{
        "WITH record_range AS (SELECT index_references.key AS index_key"};
    if (first_primary_keys_only) {
      query_pieces.push_back(", MIN(records.key) AS primary_key");
    } else {
      query_pieces.push_back(", records.key AS primary_key");
    }
    if (!key_only_) {
      query_pieces.push_back(
          ", records.value AS value"
          ", records.row_id AS record_row_id");
    }
    query_pieces.push_back(
        " FROM index_references INNER JOIN records"
        "  ON index_references.record_row_id = records.row_id"
        " WHERE"
        "  index_references.object_store_id = @object_store_id"
        "  AND index_references.index_id = @index_id");
    if (key_range.lower().IsValid()) {
      query_pieces.push_back(key_range.lower_open()
                                 ? " AND index_references.key > @lower"
                                 : " AND index_references.key >= @lower");
    }
    if (key_range.upper().IsValid()) {
      query_pieces.push_back(key_range.upper_open()
                                 ? " AND index_references.key < @upper"
                                 : " AND index_references.key <= @upper");
    }
    if (first_primary_keys_only) {
      query_pieces.push_back(" GROUP BY index_references.key");
    }
    if (ascending_order) {
      query_pieces.push_back(" ORDER BY index_key ASC, primary_key ASC)");
    } else {
      query_pieces.push_back(" ORDER BY index_key DESC, primary_key DESC)");
    }
    // The "WITH" clause ends here.
    if (key_only_) {
      query_pieces.push_back(
          " SELECT index_key, primary_key"
          " FROM record_range WHERE");
    } else {
      query_pieces.push_back(
          " SELECT index_key, primary_key, value, record_row_id"
          " FROM record_range WHERE");
    }
    if (ascending_order) {
      query_pieces.push_back(
          "("
          " @is_first_seek = 1"
          " OR (index_key = @position AND primary_key > @object_store_position)"
          " OR index_key > @position"
          ")"
          " AND (@target_key IS NULL OR index_key >= @target_key)"
          " AND (@target_primary_key IS NULL OR primary_key >= "
          "@target_primary_key)");
    } else {
      query_pieces.push_back(
          "("
          " @is_first_seek = 1"
          " OR (index_key = @position AND primary_key < @object_store_position)"
          " OR index_key < @position"
          ")"
          " AND (@target_key IS NULL OR index_key <= @target_key)"
          " AND (@target_primary_key IS NULL OR primary_key <= "
          "@target_primary_key)");
    }
    // LIMIT is needed to use OFFSET. A negative LIMIT implies no limit on the
    // number of rows returned:
    // https://www.sqlite.org/lang_select.html#the_limit_clause.
    query_pieces.push_back(" LIMIT -1 OFFSET @offset");

    sql::Statement* statement;
    std::tie(statement_id_, statement) =
        db_->CreateLongLivedStatement(base::StrCat(query_pieces));
    int param_index = 0;
    statement->BindInt64(param_index++, object_store_id);
    statement->BindInt64(param_index++, index_id);
    if (key_range.lower().IsValid()) {
      statement->BindBlob(param_index++,
                          EncodeSortableIDBKey(key_range.lower()));
    }
    if (key_range.upper().IsValid()) {
      statement->BindBlob(param_index++,
                          EncodeSortableIDBKey(key_range.upper()));
    }

    // Store the variable parameter indexes and attempt to find the initial
    // record in the range.
    statement->BindBool(is_first_seek_index_ = param_index++, true);
    statement->BindNull(position_index_ = param_index++);
    statement->BindNull(object_store_position_index_ = param_index++);
    statement->BindNull(target_key_index_ = param_index++);
    statement->BindNull(target_primary_key_index_ = param_index++);
    statement->BindInt64(offset_index_ = param_index++, 0);
    if (!statement->Step()) {
      TRANSIENT_CHECK(statement->Succeeded());
      // Empty range.
      return nullptr;
    }
    return ReadRow(*statement);
  }

 protected:
  // RecordIterator:
  void BindParameters(sql::Statement& statement,
                      const blink::IndexedDBKey& target_key,
                      const blink::IndexedDBKey& target_primary_key,
                      uint32_t offset) override {
    statement.BindBool(is_first_seek_index_, false);
    statement.BindBlob(position_index_, position_);
    statement.BindBlob(object_store_position_index_, object_store_position_);
    if (target_key.IsValid()) {
      statement.BindBlob(target_key_index_, EncodeSortableIDBKey(target_key));
    } else {
      statement.BindNull(target_key_index_);
    }
    if (target_primary_key.IsValid()) {
      statement.BindBlob(target_primary_key_index_,
                         EncodeSortableIDBKey(target_primary_key));
    } else {
      statement.BindNull(target_primary_key_index_);
    }
    statement.BindInt64(offset_index_, offset);
  }

  StatusOr<std::unique_ptr<Record>> ReadRow(
      sql::Statement& statement) override {
    TRANSIENT_CHECK(statement.ColumnBlobAsString(0, &position_));
    blink::IndexedDBKey key = DecodeSortableIDBKey(position_);
    TRANSIENT_CHECK(statement.ColumnBlobAsString(1, &object_store_position_));
    blink::IndexedDBKey primary_key =
        DecodeSortableIDBKey(object_store_position_);
    if (key_only_) {
      return std::make_unique<IndexKeyOnlyRecord>(std::move(key),
                                                  std::move(primary_key));
    }
    IndexedDBValue value;
    TRANSIENT_CHECK(statement.ColumnBlobAsVector(2, &value.bits));
    int64_t record_row_id = statement.ColumnInt64(3);
    return std::make_unique<IndexRecord>(
        std::move(key), std::move(primary_key),
        db_->AddExternalObjectMetadataToValue(std::move(value), record_row_id));
  }

  sql::Statement* GetStatement() override {
    if (!db_) {
      return nullptr;
    }
    return db_->GetLongLivedStatement(statement_id_);
  }

 private:
  base::WeakPtr<DatabaseConnection> db_;

  uint64_t statement_id_ = 0;

  bool key_only_;

  int is_first_seek_index_ = 0;
  int position_index_ = 0;
  int object_store_position_index_ = 0;
  int target_key_index_ = 0;
  int target_primary_key_index_ = 0;
  int offset_index_ = 0;

  // Encoded key from the current record.
  std::string position_;
  // Encoded primary key from the current record.
  std::string object_store_position_;
};

}  // namespace

// static
StatusOr<std::unique_ptr<DatabaseConnection>> DatabaseConnection::Open(
    const std::u16string& name,
    const base::FilePath& file_path,
    BackingStoreImpl& backing_store) {
  // TODO(crbug.com/40253999): Create new tag(s) for metrics.
  constexpr sql::Database::Tag kSqlTag = "Test";
  auto db = std::make_unique<sql::Database>(sql::DatabaseOptions()
                                                .set_exclusive_locking(true)
                                                .set_wal_mode(true)
                                                .set_enable_triggers(true),
                                            kSqlTag);

  // TODO(crbug.com/40253999): Support on-disk databases.
  TRANSIENT_CHECK(db->OpenInMemory());

  // What SQLite calls "recursive" triggers are required for SQLite to execute
  // a DELETE ON trigger after `INSERT OR REPLACE` replaces a row.
  TRANSIENT_CHECK(db->Execute("PRAGMA recursive_triggers=ON"));

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

DatabaseConnection::~DatabaseConnection() {
  // If in a zygotic state, the database should be deleted. For now, the
  // database is only in memory, so no-op is fine.
  // TODO(crbug.com/419203257): handle the on-disk case.
}

base::WeakPtr<DatabaseConnection> DatabaseConnection::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool DatabaseConnection::IsZygotic() const {
  return metadata().version == blink::IndexedDBDatabaseMetadata::NO_VERSION;
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
  if (metadata_.object_stores.contains(object_store_id) ||
      !KeyPrefix::IsValidObjectStoreId(object_store_id) ||
      object_store_id <= metadata_.max_object_store_id) {
    return Status::InvalidArgument("Invalid object_store_id");
  }

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

Status DatabaseConnection::DeleteObjectStore(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id) {
  CHECK(HasActiveVersionChangeTransaction());

  {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE, "DELETE FROM records WHERE object_store_id = ?"));
    statement.BindInt64(0, object_store_id);
    TRANSIENT_CHECK(statement.Run());
  }

  {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE, "DELETE FROM object_stores WHERE id = ?"));
    statement.BindInt64(0, object_store_id);
    TRANSIENT_CHECK(statement.Run());
  }

  CHECK(metadata_.object_stores.erase(object_store_id) == 1);
  return Status::OK();
}

Status DatabaseConnection::CreateIndex(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    blink::IndexedDBIndexMetadata index) {
  CHECK(HasActiveVersionChangeTransaction());
  if (!metadata_.object_stores.contains(object_store_id)) {
    return Status::InvalidArgument("Invalid object_store_id.");
  }
  blink::IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores.at(object_store_id);
  int64_t index_id = index.id;
  if (object_store.indexes.contains(index_id) ||
      !KeyPrefix::IsValidIndexId(index_id) ||
      index_id <= object_store.max_index_id) {
    return Status::InvalidArgument("Invalid index_id.");
  }

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO indexes "
      "(object_store_id, id, name, key_path, is_unique, multi_entry) "
      "VALUES (?, ?, ?, ?, ?, ?)"));
  statement.BindInt64(0, object_store_id);
  statement.BindInt64(1, index_id);
  statement.BindBlob(2, index.name);
  statement.BindBlob(3, EncodeKeyPath(index.key_path));
  statement.BindBool(4, index.unique);
  statement.BindBool(5, index.multi_entry);
  TRANSIENT_CHECK(statement.Run());

  object_store.indexes[index_id] = std::move(index);
  object_store.max_index_id = index_id;
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

  return AddExternalObjectMetadataToValue(std::move(value), record_row_id);
}

IndexedDBValue DatabaseConnection::AddExternalObjectMetadataToValue(
    IndexedDBValue value,
    int64_t record_row_id) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT "
      "  blobs.row_id, object_type, mime_type, size_bytes, file_name, "
      "  last_modified "
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
      auto object_type = static_cast<IndexedDBExternalObject::ObjectType>(
          statement.ColumnInt(1));
      if (object_type == IndexedDBExternalObject::ObjectType::kBlob) {
        // Otherwise, create a new `IndexedDBExternalObject` from the
        // database.
        value.external_objects.emplace_back(
            /*type=*/statement.ColumnString16(2),
            /*size=*/statement.ColumnInt64(3), blob_row_id);
      } else if (object_type == IndexedDBExternalObject::ObjectType::kFile) {
        value.external_objects.emplace_back(
            blob_row_id, /*type=*/statement.ColumnString16(2),
            /*file_name=*/statement.ColumnString16(4),
            /*last_modified=*/statement.ColumnTime(5),
            /*size=*/statement.ColumnInt64(3));
      } else {
        NOTREACHED();
      }
    }
  }
  return value;
}

StatusOr<BackingStore::RecordIdentifier> DatabaseConnection::PutRecord(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    const blink::IndexedDBKey& key,
    IndexedDBValue value) {
  // Insert record, including inline data.
  {
    // "INSERT OR REPLACE" deletes the row corresponding to
    // [object_store_id, key] if it exists and inserts a new row with `value`.
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
    // TODO(crbug.com/419208485): Support FSA handles.
    TRANSIENT_CHECK(
        external_object.object_type() !=
        IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle);
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
      if (external_object.object_type() ==
          IndexedDBExternalObject::ObjectType::kBlob) {
        statement.BindNull(4);
        statement.BindNull(5);
      } else {
        CHECK_EQ(external_object.object_type(),
                 IndexedDBExternalObject::ObjectType::kFile);
        statement.BindString16(4, external_object.file_name());
        statement.BindTime(5, external_object.last_modified());
      }
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

Status DatabaseConnection::DeleteRange(
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range) {
  // TODO(crbug.com/40253999): share code with GetObjectStoreKeyCount() and
  // others.
  std::vector<std::string_view> query_pieces{
      "DELETE FROM records WHERE object_store_id = ?"};
  if (key_range.lower().IsValid()) {
    query_pieces.insert(
        query_pieces.end(),
        {" AND key ", key_range.lower_open() ? ">" : ">=", " ?"});
  }
  if (key_range.upper().IsValid()) {
    query_pieces.insert(
        query_pieces.end(),
        {" AND key ", key_range.upper_open() ? "<" : "<=", " ?"});
  }

  sql::Statement statement(db_->GetUniqueStatement(base::StrCat(query_pieces)));
  int param_index = 0;
  statement.BindInt64(param_index++, object_store_id);
  if (key_range.lower().IsValid()) {
    statement.BindBlob(param_index++, EncodeSortableIDBKey(key_range.lower()));
  }
  if (key_range.upper().IsValid()) {
    statement.BindBlob(param_index++, EncodeSortableIDBKey(key_range.upper()));
  }
  TRANSIENT_CHECK(statement.Run());
  return Status::OK();
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

Status DatabaseConnection::PutIndexDataForRecord(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key,
    const BackingStore::RecordIdentifier& record) {
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "INSERT INTO index_references "
                              "(object_store_id, index_id, key, record_row_id) "
                              "VALUES (?, ?, ?, ?)"));
  statement.BindInt64(0, object_store_id);
  statement.BindInt64(1, index_id);
  statement.BindBlob(2, EncodeSortableIDBKey(key));
  statement.BindInt64(3, record.number);
  TRANSIENT_CHECK(statement.Run());
  return Status::OK();
}

StatusOr<blink::IndexedDBKey> DatabaseConnection::GetFirstPrimaryKeyForIndexKey(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT records.key "
      "FROM index_references INNER JOIN records"
      " ON index_references.record_row_id = records.row_id "
      "WHERE index_references.object_store_id = ?"
      " AND index_references.index_id = ?"
      " AND index_references.key = ? "
      "ORDER BY records.key ASC"));
  statement.BindInt64(0, object_store_id);
  statement.BindInt64(1, index_id);
  statement.BindBlob(2, EncodeSortableIDBKey(key));
  if (statement.Step()) {
    std::string primary_key;
    TRANSIENT_CHECK(statement.ColumnBlobAsString(0, &primary_key));
    return DecodeSortableIDBKey(primary_key);
  }
  TRANSIENT_CHECK(statement.Succeeded());
  // Not found.
  return blink::IndexedDBKey();
}

StatusOr<uint32_t> DatabaseConnection::GetIndexKeyCount(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    int64_t index_id,
    blink::IndexedDBKeyRange key_range) {
  std::vector<std::string_view> query_pieces{
      "SELECT COUNT() FROM index_references WHERE object_store_id = ?"
      " AND index_id = ?"};
  if (key_range.lower().IsValid()) {
    query_pieces.push_back(key_range.lower_open() ? " AND key > ?"
                                                  : " AND key >= ?");
  }
  if (key_range.upper().IsValid()) {
    query_pieces.push_back(key_range.upper_open() ? " AND key < ?"
                                                  : " AND key <= ?");
  }
  sql::Statement statement(
      db_->GetReadonlyStatement(base::StrCat(query_pieces)));
  int param_index = 0;
  statement.BindInt64(param_index++, object_store_id);
  statement.BindInt64(param_index++, index_id);
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
    if (object.object_type() ==
        IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle) {
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

void DatabaseConnection::DeleteIdbDatabase(
    base::PassKey<BackingStoreDatabaseImpl>) {
  metadata_ = blink::IndexedDBDatabaseMetadata(metadata_.name);
  weak_factory_.InvalidateWeakPtrs();
  CHECK(!blob_writers_weak_factory_.HasWeakPtrs());

  if (active_blobs_.empty()) {
    // Fast path: skip explicitly deleting data as the whole database will be
    // dropped.
    backing_store_->DestroyConnection(metadata_.name);
    // `this` is deleted.
    return;
  }

  record_iterator_weak_factory_.InvalidateWeakPtrs();
  statements_.clear();

  // Since blobs are still active, reset to zygotic state instead of destroying.
  TRANSIENT_CHECK(db_->Execute(
      "DELETE FROM blob_references WHERE record_row_id IS NOT NULL"));
  TRANSIENT_CHECK(db_->Execute("DELETE FROM index_references"));
  TRANSIENT_CHECK(db_->Execute("DELETE FROM indexes"));
  TRANSIENT_CHECK(db_->Execute("DELETE FROM records"));
  TRANSIENT_CHECK(db_->Execute("DELETE FROM object_stores"));

  {
    sql::Statement statement(
        db_->GetUniqueStatement("UPDATE indexed_db_metadata SET version = ?"));
    statement.BindInt64(0, blink::IndexedDBDatabaseMetadata::NO_VERSION);
    TRANSIENT_CHECK(statement.Run());
  }
}

void DatabaseConnection::OnBlobBecameInactive(int64_t blob_number) {
  CHECK_EQ(active_blobs_.erase(blob_number), 1U);
  if (active_blobs_.empty() && IsZygotic()) {
    backing_store_->DestroyConnection(metadata_.name);
    // `this` is deleted.
    return;
  }

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
  auto record_iterator = std::make_unique<ObjectStoreRecordIterator>(
      record_iterator_weak_factory_.GetWeakPtr(), key_only);
  return record_iterator
      ->Initialize(object_store_id, key_range, ascending_order)
      .transform([&](std::unique_ptr<Record> first_record)
                     -> std::unique_ptr<BackingStore::Cursor> {
        if (!first_record) {
          return nullptr;
        }
        return std::make_unique<BackingStoreCursorImpl>(
            std::move(record_iterator), std::move(first_record));
      });
}

StatusOr<std::unique_ptr<BackingStore::Cursor>>
DatabaseConnection::OpenIndexCursor(base::PassKey<BackingStoreTransactionImpl>,
                                    int64_t object_store_id,
                                    int64_t index_id,
                                    const blink::IndexedDBKeyRange& key_range,
                                    blink::mojom::IDBCursorDirection direction,
                                    bool key_only) {
  bool ascending_order =
      (direction == blink::mojom::IDBCursorDirection::Next ||
       direction == blink::mojom::IDBCursorDirection::NextNoDuplicate);
  // NoDuplicate => iterate over the first primary keys only.
  bool first_primary_keys_only =
      (direction == blink::mojom::IDBCursorDirection::NextNoDuplicate ||
       direction == blink::mojom::IDBCursorDirection::PrevNoDuplicate);
  auto record_iterator = std::make_unique<IndexRecordIterator>(
      record_iterator_weak_factory_.GetWeakPtr(), key_only);
  return record_iterator
      ->Initialize(object_store_id, index_id, key_range, ascending_order,
                   first_primary_keys_only)
      .transform([&](std::unique_ptr<Record> first_record)
                     -> std::unique_ptr<BackingStore::Cursor> {
        if (!first_record) {
          return nullptr;
        }
        return std::make_unique<BackingStoreCursorImpl>(
            std::move(record_iterator), std::move(first_record));
      });
}

std::tuple<uint64_t, sql::Statement*>
DatabaseConnection::CreateLongLivedStatement(std::string query) {
  auto it = statements_.emplace(
      ++next_statement_id_,
      std::make_unique<sql::Statement>(db_->GetUniqueStatement(query)));
  CHECK(it.second);
  return {next_statement_id_, it.first->second.get()};
}

void DatabaseConnection::ReleaseLongLivedStatement(uint64_t id) {
  CHECK_EQ(1U, statements_.erase(id));
}

sql::Statement* DatabaseConnection::GetLongLivedStatement(uint64_t id) {
  auto it = statements_.find(id);
  if (it == statements_.end()) {
    return nullptr;
  }
  return it->second.get();
}

}  // namespace content::indexed_db::sqlite
