// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/database_connection.h"

#include <memory>
#include <string>
#include <utility>

#include "base/byte_count.h"
#include "base/byte_size.h"
#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "content/browser/indexed_db/indexed_db_data_format_version.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/record.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_cursor_impl.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_transaction_impl.h"
#include "content/browser/indexed_db/status.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/snappy/src/snappy.h"
#include "third_party/zstd/src/lib/zstd.h"

// TODO(crbug.com/40253999): Rename the file to indicate that it contains
// backend-agnostic utils to encode/decode IDB types, and potentially move the
// (Encode/Decode)KeyPath methods below to this file.
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"

// Returns an error if the given SQL statement has not succeeded/is no longer
// valid (`Succeeded()` returns false; `db_` has an error).
//
// This should be used after `Statement::Step()` returns false.
#define RETURN_IF_STATEMENT_ERRORED(statement) \
  if (!statement.Succeeded()) {                \
    return base::unexpected(Status(*db_));     \
  }

// Runs the statement and returns if there was an error. For use with functions
// that return StatusOr<T>.
#define RUN_STATEMENT_RETURN_ON_ERROR(statement) \
  if (!statement.Run()) {                        \
    return base::unexpected(Status(*db_));       \
  }

// Returns a `Status` if the passed expression evaluates to false.
#define RETURN_STATUS_ON_ERROR(expr) \
  if (!expr) {                       \
    return Status(*db_);             \
  }

// Executes the given SQL on `db` and returns a Status if there was an error.
#define EXECUTE_AND_RETURN_STATUS_ON_ERROR(db, sql) \
  if (!db->Execute(sql)) {                          \
    return Status(*db);                             \
  }

namespace content::indexed_db::sqlite {
namespace {

// Persisted to disk; do not reuse or change values.
enum class CompressionType : uint8_t {
  kUncompressed = 0,  // Not compressed.
  kZstd = 1,          // Standalone ZSTD with no dictionary.
  kSnappy = 2,        // Snappy.
};

// Used for tests.
std::optional<base::ByteCount> g_max_blob_size_override;

// The maximum number of bytes that will be stored in a single SQLite BLOB
// column. If a blob is larger than this, it will be chunked into multiple rows
// in the `overflow_blob_chunks` table.
//
// This value is much smaller than the maximum size SQLite is able to handle
// because:
// 1. some operations such as VACUUM will read an entire row into memory, so
// this cuts down on concurrent memory usage.
// https://sqlite.org/forum/forumpost/756c1a1e4807217e?t=h
// 2. SQLite docs assert without much explanation that applications "might do
// well to lower the maximum string and blob length to something more in the
// range of a few million if that is possible".
// https://www.sqlite.org/limits.html
base::ByteCount GetMaxBlobSize() {
  return g_max_blob_size_override.value_or(base::MiB(5));
}

// The separator used to join the strings when encoding an `IndexedDBKeyPath` of
// type array. Spaces are not allowed in the individual strings, which makes
// this a convenient choice.
constexpr char16_t kKeyPathSeparator[] = u" ";
void BindKeyPath(sql::Statement& statement,
                 int param_index,
                 const blink::IndexedDBKeyPath& key_path) {
  switch (key_path.type()) {
    case blink::mojom::IDBKeyPathType::Null:
      statement.BindNull(param_index);
      break;
    case blink::mojom::IDBKeyPathType::String:
      statement.BindBlob(param_index, key_path.string());
      break;
    case blink::mojom::IDBKeyPathType::Array:
      statement.BindBlob(param_index,
                         base::JoinString(key_path.array(), kKeyPathSeparator));
      break;
    default:
      NOTREACHED();
  }
}

StatusOr<blink::IndexedDBKeyPath> ColumnKeyPath(sql::Statement& statement,
                                                int column_index) {
  if (statement.GetColumnType(column_index) == sql::ColumnType::kNull) {
    // `Null` key path.
    return blink::IndexedDBKeyPath();
  }
  ASSIGN_OR_RETURN(
      std::u16string encoded, statement.ColumnBlobAsString16(column_index),
      []() { return Status::Corruption("Key path unexpected size"); });
  std::vector<std::u16string> parts = base::SplitString(
      encoded, kKeyPathSeparator, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.empty()) {
    // Empty `String` key path.
    return blink::IndexedDBKeyPath(std::u16string());
  }
  if (parts.size() == 1) {
    // Non-empty `String` key path.
    return blink::IndexedDBKeyPath(std::move(parts.front()));
  }
  // `Array` key path.
  return blink::IndexedDBKeyPath(std::move(parts));
}

StatusOr<mojo_base::BigBuffer> DoDecompress(
    base::span<const uint8_t> compressed,
    int compression_type) {
  if (compression_type == static_cast<int>(CompressionType::kUncompressed)) {
    return mojo_base::BigBuffer(compressed);
  }

  if (compression_type == static_cast<int>(CompressionType::kZstd)) {
    uint64_t decompressed_size =
        ZSTD_getFrameContentSize(compressed.data(), compressed.size());
    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN ||
        decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
      return base::unexpected(Status::Corruption("ZSTD decompression failed"));
    }

    mojo_base::BigBuffer decompressed(decompressed_size);
    if (ZSTD_isError(ZSTD_decompress(decompressed.data(), decompressed.size(),
                                     compressed.data(), compressed.size()))) {
      return base::unexpected(Status::Corruption("ZSTD decompression failed"));
    }

    return std::move(decompressed);
  }

  if (compression_type == static_cast<int>(CompressionType::kSnappy)) {
    size_t decompressed_length;
    base::span<const char> src = base::as_chars(compressed);
    if (!snappy::GetUncompressedLength(src.data(), src.size(),
                                       &decompressed_length)) {
      return base::unexpected(
          Status::Corruption("Snappy decompression failed"));
    }

    mojo_base::BigBuffer decompressed(decompressed_length);
    base::span<char> dest = base::as_writable_chars(base::span(decompressed));
    if (!snappy::RawUncompress(src.data(), src.size(), dest.data())) {
      return base::unexpected(
          Status::Corruption("Snappy decompression failed"));
    }

    return std::move(decompressed);
  }

  return base::unexpected(Status::Corruption("unknown compression type"));
}

// Key used in MetaTable to track the data encoding version used by Blink/V8.
constexpr std::string_view kV8DataVersionKey = "v8_data_version";

// These are schema versions of our implementation of `sql::Database`; not the
// version supplied by the application for the IndexedDB database.
//
// The version used to initialize the meta table for the first time.
constexpr int kCurrentSchemaVersion = 1;
constexpr int kCompatibleSchemaVersion = kCurrentSchemaVersion;

// Creates the current schema for a new `db` and inserts the initial IndexedDB
// metadata entry with `name`.
Status CreateSchema(sql::Database* db, std::u16string_view name) {
  // Create the tables.
  //
  // Note on the schema: The IDB spec defines the "name"
  // (https://www.w3.org/TR/IndexedDB/#name) of the database, object stores and
  // indexes as an arbitrary sequence of 16-bit code units, which implies that
  // the application-supplied name strings need not be valid UTF-16.
  // "key_path"s are always valid UTF-16 since they contain only identifiers
  // (required to be valid UTF-16) and periods.
  // However, to avoid unnecessary conversion from UTF-16 to UTF-8 and back,
  // all 16-bit strings are stored as BLOBs.
  //
  // Though object store names and index names (within an object store) are
  // unique, this is not enforced in the schema itself since this constraint can
  // be transiently violated at the backing store level (the IDs are always
  // guaranteed to be unique, however). This is because creation of object
  // stores and indexes happens on the preemptive task queue while deletion
  // happens on the regular queue.
  //
  // Stores a single row containing the properties of
  // `IndexedDBDatabaseMetadata` for this database.
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(db,
                                     "CREATE TABLE indexed_db_metadata "
                                     "(name BLOB NOT NULL,"
                                     " version INTEGER NOT NULL)");
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(
      db,
      "CREATE TABLE object_stores "
      "(id INTEGER PRIMARY KEY,"
      " name BLOB NOT NULL,"
      " key_path BLOB,"
      " auto_increment INTEGER NOT NULL,"
      " key_generator_current_number INTEGER NOT NULL)");
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(db,
                                     "CREATE TABLE indexes "
                                     "(object_store_id INTEGER NOT NULL,"
                                     " id INTEGER NOT NULL,"
                                     " name BLOB NOT NULL,"
                                     " key_path BLOB,"
                                     " is_unique INTEGER NOT NULL,"
                                     " multi_entry INTEGER NOT NULL,"
                                     " PRIMARY KEY (object_store_id, id)"
                                     ") WITHOUT ROWID");
  // Stores object store records. The rows are immutable - updating the value
  // for a combination of object_store_id and key is accomplished by deleting
  // the previous row and inserting a new one (see `PutRecord()`).
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(db,
                                     "CREATE TABLE records "
                                     "(row_id INTEGER PRIMARY KEY,"
                                     " object_store_id INTEGER NOT NULL,"
                                     " compression_type INTEGER NOT NULL,"
                                     " key BLOB NOT NULL,"
                                     " value BLOB NOT NULL)");
  // Create the index separately so it can be given a name (which is referenced
  // by tests).
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(
      db,
      "CREATE UNIQUE INDEX records_by_key ON records(object_store_id, key)");
  // Stores the mapping of object store records to the index keys that reference
  // them: record_row_id -> [index_id, key]. In general, a record may be
  // referenced by multiple keys across multiple indexes (on the same object
  // store). Since the row_id of a record uniquely identifies the record across
  // object stores, object_store_id is not part of the primary key of this
  // table. Deleting a record leads to the deletion of all index references to
  // it (through the on_record_deleted trigger).
  // The object store ID and record key are redundant information, but including
  // them here and creating an index over them expedites cursor iteration
  // because it removes the need to JOIN against the records table. See
  // https://crbug.com/433318798.
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(
      db,
      "CREATE TABLE index_references "
      "(record_row_id INTEGER NOT NULL,"
      " index_id INTEGER NOT NULL,"
      " key BLOB NOT NULL,"
      " object_store_id INTEGER NOT NULL,"
      " record_key BLOB NOT NULL,"
      " PRIMARY KEY (record_row_id, index_id, key)"
      ") WITHOUT ROWID");
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(
      db,
      "CREATE INDEX index_references_by_key "
      "ON index_references (object_store_id, index_id, key, record_key)");

  // This table stores blob metadata and its actual bytes. A blob should only
  // appear once, regardless of how many records point to it. The columns in
  // this table should be effectively const, as SQLite blob handles will be used
  // to stream out of the table, and the associated row must never change while
  // blob handles are active. Blobs will be removed from this table when no
  // references remain (see `blob_references`).
  //
  // TODO(crbug.com/419208485): consider taking into account the blob's UUID to
  // further avoid duplication.
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(
      db,
      "CREATE TABLE blobs "
      // This row id will be used as the IndexedDBExternalObject::blob_number_.
      // AUTOINCREMENT prevents reuse of an ID if a blob is deleted, to avoid
      // confusion in `blobs_staged_for_commit_` which uses this ID.
      "(row_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      // Corresponds to `IndexedDBExternalObject::ObjectType`.
      " object_type INTEGER NOT NULL,"
      // Null for FSA handles.
      " mime_type TEXT,"
      // Null for FSA handles. For blobs, this is the total size of the blob,
      // including overflow bytes.
      " size_bytes INTEGER,"
      " file_name BLOB,"          // only for files
      " last_modified INTEGER, "  // only for files
      // NB: a large BLOB should be the last column when possible. See
      // https://sqlite.org/forum/forumpost/756c1a1e4807217e?t=h
      //
      // This column is null if the blob is stored on disk, which will be the
      // case for legacy blobs. It's also temporarily null while FSA handles are
      // being serialized into a token (after which point, this holds the
      // token). If there are more bytes than fit into a single SQLite BLOB
      // (GetMaxBlobSize()), additional bytes will be stored in
      // `overflow_blob_chunks` table.
      " bytes BLOB)");

  // IndexedDB aims to support multi-GB blobs. SQLite does not support blobs
  // larger than a certain size, which is at most 2^31 - 1, and is by default
  // 1,000,000,000 (1 billion) bytes. See https://www.sqlite.org/limits.html
  //
  // As such, large blobs must be split across multiple rows. Each piece of a
  // blob is called a "chunk" in this context. The 0th chunk (which for most
  // blobs is the only chunk) is stored in the `blobs` table, along with
  // metadata. Subsequent chunks are stored here, and identified by an index (1
  // or higher). When reading or writing a blob, helper classes
  // (ActiveBlobStreamer and BlobWriter) will translate offsets into the entire
  // blob into offsets into the appropriate chunk.
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(
      db,
      "CREATE TABLE overflow_blob_chunks "
      "(row_id INTEGER PRIMARY KEY,"
      // The ID in the `blobs` table for which this row holds some overflow
      // bytes.
      " blob_row_id INTEGER NOT NULL,"
      // 1-based index into overflow chunks for a given blob. 1 refers to the
      // first *overflow chunk*. (0 theoretically refers to the non-overflow
      // bytes in the `blobs` table.)
      " chunk_index INTEGER NOT NULL,"
      // NB: a large BLOB should be the last column when possible. See
      // https://sqlite.org/forum/forumpost/756c1a1e4807217e?t=h
      " bytes BLOB NOT NULL)");

  // Blobs may be referenced by rows in `records` or by active connections to
  // clients.
  // TODO(crbug.com/419208485): Consider making this a WITHOUT ROWID table.
  // Since NULL values are not allowed in the primary key of such a table, a
  // specific value of record_row_id will be needed to represent active blobs.
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(
      db,
      "CREATE TABLE blob_references "
      "(row_id INTEGER PRIMARY KEY,"
      " blob_row_id INTEGER NOT NULL,"
      // record_row_id will be null when the reference corresponds
      // to an active blob reference (represented in the browser by
      // ActiveBlobStreamer). Otherwise it will be the id of the
      // record row that holds the reference.
      " record_row_id INTEGER)");
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(db,
                                     "CREATE INDEX blob_references_by_blob "
                                     "ON blob_references (blob_row_id)");
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(db,
                                     "CREATE INDEX blob_references_by_record "
                                     "ON blob_references (record_row_id)");

  // Create deletion triggers. Deletion triggers are not used for the
  // object_stores and indexes tables since their deletion occurs only through
  // dedicated functions intended specifically for this purpose.
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(
      db,
      "CREATE TRIGGER on_record_deleted AFTER DELETE ON records "
      "BEGIN"
      "  DELETE FROM index_references WHERE record_row_id = OLD.row_id;"
      "  DELETE FROM blob_references WHERE record_row_id = OLD.row_id;"
      "END");
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(
      db,
      "CREATE TRIGGER on_blob_reference_deleted"
      "  AFTER DELETE ON blob_references "
      "WHEN NOT EXISTS"
      "  (SELECT 1 FROM blob_references WHERE blob_row_id = OLD.blob_row_id) "
      "BEGIN"
      "  DELETE FROM blobs WHERE row_id = OLD.blob_row_id;"
      "END");
  EXECUTE_AND_RETURN_STATUS_ON_ERROR(
      db,
      "CREATE TRIGGER on_blob_deleted"
      "  AFTER DELETE ON blobs "
      "BEGIN"
      "  DELETE FROM overflow_blob_chunks WHERE blob_row_id = OLD.row_id;"
      "END");

  // Insert the initial metadata entry.
  sql::Statement statement(
      db->GetUniqueStatement("INSERT INTO indexed_db_metadata "
                             "(name, version) VALUES (?, ?)"));
  statement.BindBlob(0, std::u16string(name));
  statement.BindInt64(1, blink::IndexedDBDatabaseMetadata::NO_VERSION);
  if (!statement.Run()) {
    return Status(*db);
  }

  return Status::OK();
}

std::vector<std::string_view> StartRecordRangeQuery(
    std::string_view command,
    const blink::IndexedDBKeyRange& key_range) {
  std::vector<std::string_view> query_pieces{command};
  query_pieces.push_back(
      " FROM records"
      " WHERE object_store_id = ?");
  if (key_range.lower().IsValid()) {
    query_pieces.push_back(key_range.lower_open() ? " AND key > ?"
                                                  : " AND key >= ?");
  }
  if (key_range.upper().IsValid()) {
    query_pieces.push_back(key_range.upper_open() ? " AND key < ?"
                                                  : " AND key <= ?");
  }
  return query_pieces;
}
// Returns the next index for binding subsequent parameters.
int BindRecordRangeQueryParams(sql::Statement& statement,
                               int64_t object_store_id,
                               const blink::IndexedDBKeyRange& key_range) {
  int param_index = 0;
  statement.BindInt64(param_index++, object_store_id);
  if (key_range.lower().IsValid()) {
    statement.BindBlob(param_index++, EncodeSortableIDBKey(key_range.lower()));
  }
  if (key_range.upper().IsValid()) {
    statement.BindBlob(param_index++, EncodeSortableIDBKey(key_range.upper()));
  }
  return param_index;
}

class ObjectStoreCursorImpl : public BackingStoreCursorImpl {
 public:
  // Returns null if no record exists in the supplied range.
  static StatusOr<std::unique_ptr<ObjectStoreCursorImpl>> Create(
      base::WeakPtr<DatabaseConnection> db,
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      bool key_only,
      bool ascending_order) {
    std::vector<std::string_view> query_pieces = StartRecordRangeQuery(
        key_only ? "SELECT key" : "SELECT key, value, compression_type, row_id",
        key_range);
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

    auto [statement_id, statement] = db->CreateCursorStatement(
        PassKey(), base::StrCat(query_pieces), object_store_id);
    auto cursor =
        base::WrapUnique(new ObjectStoreCursorImpl(db, statement_id, key_only));

    // Bind the fixed parameters.
    int param_index =
        BindRecordRangeQueryParams(*statement, object_store_id, key_range);

    // Store the variable parameter indexes and attempt to update the record to
    // the initial one in the range.
    statement->BindBool(cursor->is_first_seek_index_ = param_index++, true);
    statement->BindNull(cursor->position_index_ = param_index++);
    statement->BindNull(cursor->target_key_index_ = param_index++);
    if (!statement->Step()) {
      if (!statement->Succeeded()) {
        return base::unexpected(db->GetStatusOfLastOperation(PassKey()));
      }
      // Empty range.
      return nullptr;
    }
    Status s = cursor->UpdateRecord(*statement);
    if (!s.ok()) {
      return base::unexpected(s);
    }
    return std::move(cursor);
  }

  void SavePosition() override { saved_position_ = position_; }

  Status TryResetToLastSavedPosition() override {
    if (!saved_position_) {
      return Status::InvalidArgument("Position not saved");
    }
    position_ = *std::move(saved_position_);
    saved_position_.reset();
    return BackingStoreCursorImpl::TryResetToLastSavedPosition();
  }

 protected:
  void BindParameters(sql::Statement& statement,
                      const blink::IndexedDBKey& target_key,
                      const blink::IndexedDBKey& target_primary_key) override {
    // `target_primary_key` is not expected when iterating over object store
    // records.
    CHECK(!target_primary_key.IsValid());
    statement.BindBool(is_first_seek_index_, false);
    statement.BindBlob(position_index_, position_);
    if (target_key.IsValid()) {
      statement.BindBlob(target_key_index_, EncodeSortableIDBKey(target_key));
    } else {
      statement.BindNull(target_key_index_);
    }
  }

  StatusOr<std::unique_ptr<Record>> ReadRow(
      sql::Statement& statement) override {
    CHECK(statement.Succeeded());
    position_ = statement.ColumnBlobAsString(0);
    blink::IndexedDBKey key = DecodeSortableIDBKey(position_);
    if (key_only_) {
      return std::make_unique<ObjectStoreKeyOnlyRecord>(std::move(key));
    }
    base::span<const uint8_t> bits = statement.ColumnBlob(1);
    int compression_type = statement.ColumnInt(2);
    IndexedDBValue value;
    ASSIGN_OR_RETURN(value.bits, db()->Decompress(bits, compression_type));
    int64_t record_row_id = statement.ColumnInt64(3);
    return db()
        ->AddExternalObjectMetadataToValue(std::move(value), record_row_id)
        .transform([&](IndexedDBValue value_with_metadata) {
          return std::make_unique<ObjectStoreRecord>(
              std::move(key), std::move(value_with_metadata));
        });
  }

 private:
  ObjectStoreCursorImpl(base::WeakPtr<DatabaseConnection> db,
                        uint64_t statement_id,
                        bool key_only)
      : BackingStoreCursorImpl(std::move(db), statement_id),
        key_only_(key_only) {}

  bool key_only_;

  int is_first_seek_index_ = 0;
  int position_index_ = 0;
  int target_key_index_ = 0;

  // Encoded key from the current record, tracking the position in the range.
  std::string position_;

  std::optional<std::string> saved_position_;
};

class IndexCursorImpl : public BackingStoreCursorImpl {
 public:
  // If `first_primary_keys_only` is true, `this` will iterate over only the
  // first (i.e., smallest) primary key for each index key in `key_range` (this
  // correlates to the nextunique/prevunique IndexedDB cursor direction). Else,
  // all the primary keys are iterated over for each index key in the range.
  //
  // Returns null if no record exists in the supplied range.
  static StatusOr<std::unique_ptr<IndexCursorImpl>> Create(
      base::WeakPtr<DatabaseConnection> db,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      bool key_only,
      bool first_primary_keys_only,
      bool ascending_order) {
    std::vector<std::string_view> query_pieces{
        "SELECT index_references.key AS index_key"};
    if (first_primary_keys_only) {
      query_pieces.push_back(", MIN(record_key)");
    } else {
      query_pieces.push_back(", record_key");
    }
    if (key_only) {
      query_pieces.push_back(" FROM index_references");
    } else {
      query_pieces.push_back(
          ", records.value"
          ", records.compression_type"
          ", records.row_id"
          " FROM index_references INNER JOIN records"
          "  ON index_references.record_row_id = records.row_id");
    }
    query_pieces.push_back(
        " WHERE index_references.object_store_id = @object_store_id"
        "   AND index_references.index_id = @index_id");
    if (key_range.lower().IsValid()) {
      query_pieces.push_back(key_range.lower_open()
                                 ? " AND index_key > @lower"
                                 : " AND index_key >= @lower");
    }
    if (key_range.upper().IsValid()) {
      query_pieces.push_back(key_range.upper_open()
                                 ? " AND index_key < @upper"
                                 : " AND index_key <= @upper");
    }
    if (ascending_order) {
      if (first_primary_keys_only) {
        query_pieces.push_back(
            " AND (@is_first_seek = 1 OR index_key > @position)"
            " AND (@target_key IS NULL OR index_key >= @target_key)"
            " GROUP BY index_key"
            " ORDER BY index_key ASC");
      } else {
        query_pieces.push_back(
            " AND "
            " ("
            "  @is_first_seek = 1"
            "  OR (index_key = @position AND record_key >"
            "      @object_store_position)"
            "  OR index_key > @position"
            " )"
            " AND (@target_key IS NULL OR index_key >= @target_key)"
            " AND "
            " ("
            "  @target_primary_key IS NULL"
            "  OR (index_key = @target_key AND record_key >="
            "      @target_primary_key)"
            "  OR index_key > @target_key"
            " )"
            " ORDER BY index_key ASC, record_key ASC");
      }
    } else {
      if (first_primary_keys_only) {
        query_pieces.push_back(
            " AND (@is_first_seek = 1 OR index_key < @position)"
            " AND (@target_key IS NULL OR index_key <= @target_key)"
            " GROUP BY index_key"
            " ORDER BY index_key DESC");
      } else {
        query_pieces.push_back(
            " AND "
            " ("
            "  @is_first_seek = 1"
            "  OR (index_key = @position AND record_key < "
            "      @object_store_position)"
            "  OR index_key < @position"
            " )"
            " AND (@target_key IS NULL OR index_key <= @target_key)"
            " AND "
            " ("
            "  @target_primary_key IS NULL"
            "  OR (index_key = @target_key AND record_key <= "
            "      @target_primary_key)"
            "  OR index_key < @target_key"
            " )"
            " ORDER BY index_key DESC, record_key DESC");
      }
    }

    auto [statement_id, statement] = db->CreateCursorStatement(
        PassKey(), base::StrCat(query_pieces), object_store_id);
    auto cursor = base::WrapUnique(new IndexCursorImpl(
        db, statement_id, key_only, first_primary_keys_only));

    // Bind the fixed parameters.
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

    // Store the variable parameter indexes and attempt to update the record to
    // the initial one in the range.
    statement->BindBool(cursor->is_first_seek_index_ = param_index++, true);
    statement->BindNull(cursor->position_index_ = param_index++);
    if (!first_primary_keys_only) {
      cursor->object_store_position_index_ = param_index++;
      statement->BindNull(cursor->object_store_position_index_.value());
    }
    statement->BindNull(cursor->target_key_index_ = param_index++);
    if (!first_primary_keys_only) {
      cursor->target_primary_key_index_ = param_index++;
      statement->BindNull(cursor->target_primary_key_index_.value());
    }
    if (!statement->Step()) {
      if (!statement->Succeeded()) {
        return base::unexpected(db->GetStatusOfLastOperation(PassKey()));
      }
      // Empty range.
      return nullptr;
    }
    Status s = cursor->UpdateRecord(*statement);
    if (!s.ok()) {
      return base::unexpected(s);
    }
    return std::move(cursor);
  }

  void SavePosition() override {
    saved_position_ = {position_, object_store_position_};
  }

  Status TryResetToLastSavedPosition() override {
    if (!saved_position_) {
      return Status::InvalidArgument("Position not saved");
    }
    std::tie(position_, object_store_position_) = *std::move(saved_position_);
    saved_position_.reset();
    return BackingStoreCursorImpl::TryResetToLastSavedPosition();
  }

 protected:
  void BindParameters(sql::Statement& statement,
                      const blink::IndexedDBKey& target_key,
                      const blink::IndexedDBKey& target_primary_key) override {
    // `target_primary_key` is not expected when iterating over only the
    // first primary keys.
    CHECK(!first_primary_keys_only_ || !target_primary_key.IsValid());
    statement.BindBool(is_first_seek_index_, false);
    statement.BindBlob(position_index_, position_);
    if (target_key.IsValid()) {
      statement.BindBlob(target_key_index_, EncodeSortableIDBKey(target_key));
    } else {
      statement.BindNull(target_key_index_);
    }
    if (!first_primary_keys_only_) {
      statement.BindBlob(object_store_position_index_.value(),
                         object_store_position_);
      if (target_primary_key.IsValid()) {
        statement.BindBlob(target_primary_key_index_.value(),
                           EncodeSortableIDBKey(target_primary_key));
      } else {
        statement.BindNull(target_primary_key_index_.value());
      }
    }
  }

  StatusOr<std::unique_ptr<Record>> ReadRow(
      sql::Statement& statement) override {
    CHECK(statement.Succeeded());
    position_ = statement.ColumnBlobAsString(0);
    blink::IndexedDBKey key = DecodeSortableIDBKey(position_);
    object_store_position_ = statement.ColumnBlobAsString(1);
    blink::IndexedDBKey primary_key =
        DecodeSortableIDBKey(object_store_position_);
    if (key_only_) {
      return std::make_unique<IndexKeyOnlyRecord>(std::move(key),
                                                  std::move(primary_key));
    }

    base::span<const uint8_t> bits = statement.ColumnBlob(2);
    int compression_type = statement.ColumnInt(3);
    IndexedDBValue value;
    ASSIGN_OR_RETURN(value.bits, db()->Decompress(bits, compression_type));

    int64_t record_row_id = statement.ColumnInt64(4);

    return db()
        ->AddExternalObjectMetadataToValue(std::move(value), record_row_id)
        .transform([&](IndexedDBValue value_with_metadata) {
          return std::make_unique<IndexRecord>(std::move(key),
                                               std::move(primary_key),
                                               std::move(value_with_metadata));
        });
  }

 private:
  IndexCursorImpl(base::WeakPtr<DatabaseConnection> db,
                  uint64_t statement_id,
                  bool key_only,
                  bool first_primary_keys_only)
      : BackingStoreCursorImpl(std::move(db), statement_id),
        key_only_(key_only),
        first_primary_keys_only_(first_primary_keys_only) {}

  bool key_only_;
  bool first_primary_keys_only_;

  int is_first_seek_index_ = 0;
  int position_index_ = 0;
  int target_key_index_ = 0;
  // Set iff `first_primary_keys_only_` is false.
  std::optional<int> object_store_position_index_;
  std::optional<int> target_primary_key_index_;

  // Encoded key from the current record.
  std::string position_;
  // Encoded primary key from the current record.
  std::string object_store_position_;

  std::optional<std::tuple<std::string, std::string>> saved_position_;
};

}  // namespace

// static
StatusOr<std::unique_ptr<DatabaseConnection>> DatabaseConnection::Open(
    std::optional<std::u16string_view> name,
    base::FilePath path,
    BackingStoreImpl& backing_store) {
  auto connection =
      base::WrapUnique(new DatabaseConnection(path, backing_store));
  Status s = connection->Init(name);
  if (!path.empty() && !s.ok()) {
    IndexedDBDataLossInfo loss;
    if (connection->marked_for_permanent_deletion_) {
      loss.status = blink::mojom::IDBDataLoss::Total;
      loss.message = s.ToString();
    }
    // If opening fails, recover or destroy the DB and try once more. This is
    // accomplished by destroying `connection`, since the destructor handles
    // errors.
    connection = base::WrapUnique(new DatabaseConnection(path, backing_store));
    s = connection->Init(name);
    connection->data_loss_info_ = std::move(loss);
    s.Log("IndexedDB.SQLite.OpenRetryResult");
  }
  if (!s.ok()) {
    return base::unexpected(s);
  }
  return connection;
}

// static
void DatabaseConnection::Release(base::WeakPtr<DatabaseConnection> db) {
  if (!db) {
    return;
  }

  // TODO(crbug.com/419203257):  Consider delaying destruction by a short period
  // in case the page reopens the same database soon.
  DatabaseConnection* db_ptr = db.get();
  db.reset();
  if (db_ptr->CanSelfDestruct()) {
    db_ptr->backing_store_->DestroyConnection(db_ptr->metadata_.name);
  }
}

DatabaseConnection::DatabaseConnection(base::FilePath path,
                                       BackingStoreImpl& backing_store)
    : path_(path), backing_store_(backing_store) {}

DatabaseConnection::~DatabaseConnection() {
  // Although generally active blobs will keep `this` alive, in some cases such
  // as when the backing store is being force-closed, blobs may still be active.
  active_blobs_.clear();

  if (!db_ || in_memory()) {
    return;
  }

  bool had_sql_error =
      !sql::IsSqliteSuccessCode(sql::ToSqliteResultCode(db_->GetErrorCode()));

  // When the database never finished initializing, it will be zygotic. This
  // could happen if version change transaction was aborted/rolled back. In this
  // case the newly created database should be deleted.
  if (marked_for_permanent_deletion_ || (IsZygotic() && !had_sql_error)) {
    db_.reset();
    sql::Database::Delete(path_);
  } else if (had_sql_error) {
    LogEvent(SpecificEvent::kDatabaseHadSqlError);

    // Note that `DatabaseConnection` does not set an error callback on
    // sql::Database. Instead, errors are returned for individual operations,
    // which will trickle up through backing store agnostic code and close all
    // `Transaction`s, `Connection`s and `Database`s. When the last
    // `BackingStore::Database` is deleted, `this` will be deleted, at which
    // point recovery will be attempted if appropriate.
#if BUILDFLAG(IS_FUCHSIA)
    // Recovery is not supported with WAL mode DBs in Fuchsia.
    if (db_->is_open() && sql::IsErrorCatastrophic(db_->GetErrorCode())) {
      db_.reset();
      sql::Database::Delete(path_);
    }
#else
    // `RecoverIfPossible` will no-op for several reasons including if the error
    // is thought to be transient.
    std::ignore = sql::Recovery::RecoverIfPossible(
        db_.get(), db_->GetErrorCode(),
        sql::Recovery::Strategy::kRecoverWithMetaVersionOrRaze);
#endif
  }
}

Status DatabaseConnection::Init(std::optional<std::u16string_view> name) {
  LogEvent(SpecificEvent::kDatabaseOpenAttempt);

  constexpr sql::Database::Tag kSqlTag = "IndexedDB";
  constexpr sql::Database::Tag kSqlTagInMemory = "IndexedDBEphemeral";
  db_ =
      std::make_unique<sql::Database>(sql::DatabaseOptions()
                                          .set_exclusive_locking(true)
                                          .set_wal_mode(true)
                                          .set_enable_triggers(true),
                                      in_memory() ? kSqlTagInMemory : kSqlTag);

  if (in_memory()) {
    RETURN_STATUS_ON_ERROR(db_->OpenInMemory());
  } else {
    RETURN_STATUS_ON_ERROR(db_->Open(path_));
  }

  // What SQLite calls "recursive" triggers are required for SQLite to execute
  // a DELETE ON trigger after `INSERT OR REPLACE` replaces a row.
  RETURN_STATUS_ON_ERROR(db_->Execute("PRAGMA recursive_triggers=ON"));

  sql::Transaction transaction(db_.get());
  RETURN_STATUS_ON_ERROR(transaction.Begin());

  const bool is_new_db = !sql::MetaTable::DoesTableExist(db_.get());
  if (is_new_db) {
    IDB_RETURN_IF_ERROR(CreateSchema(db_.get(), *name));
  }

  meta_table_ = std::make_unique<sql::MetaTable>();
  RETURN_STATUS_ON_ERROR(meta_table_->Init(db_.get(), kCurrentSchemaVersion,
                                           kCompatibleSchemaVersion));

  if (meta_table_->GetCompatibleVersionNumber() > kCurrentSchemaVersion) {
    return Fatal(Status::NotFound("Database too new"),
                 SpecificEvent::kDatabaseTooNew);
  }

  // The "data format version" refers to the encoding routine Blink/V8 uses to
  // serialize values. It will always be backwards compatible, but we may open a
  // database that is too new (written by a future version of the browser),
  // which will be a fatal error.
  const auto current_data_format = IndexedDBDataFormatVersion::GetCurrent();
  if (!is_new_db) {
    int64_t data_format_version;
    if (!meta_table_->GetValue(kV8DataVersionKey, &data_format_version)) {
      return Fatal(Status::Corruption("Missing data format version"),
                   SpecificEvent::kV8FormatTooNewOrMissing);
    }
    if (!current_data_format.IsAtLeast(
            IndexedDBDataFormatVersion::Decode(data_format_version))) {
      return Fatal(
          Status::NotFound("Unintelligible data format version: too new"),
          SpecificEvent::kV8FormatTooNewOrMissing);
    }
  }
  meta_table_->SetValue(kV8DataVersionKey, current_data_format.Encode());

  switch (meta_table_->GetVersionNumber()) {
    // ...
    // Schema upgrades go here.
    // ...
    case kCurrentSchemaVersion:
      // Already current.
      break;
    default:
      return Fatal(Status::NotFound(
                       "Unknown database schema version (database too new?)"),
                   SpecificEvent::kDatabaseSchemaUnknown);
  }

  StatusOr<blink::IndexedDBDatabaseMetadata> metadata =
      GenerateIndexedDbMetadata();
  if (!metadata.has_value()) {
    return metadata.error();
  }

  metadata_ = *std::move(metadata);
  // Database corruption can cause a mismatch.
  if (name && (metadata_.name != *name)) {
    return Fatal(Status::Corruption("Database name mismatch"),
                 SpecificEvent::kDatabaseNameMismatch);
  }

  // There should be no active blobs in this database at this point, so we can
  // remove blob references that were associated with active blobs. These may
  // have been left behind if Chromium crashed. Deleting the blob references
  // should also delete the blob if appropriate.
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM blob_references WHERE record_row_id IS NULL"));
  RETURN_STATUS_ON_ERROR(statement.Run());

  RETURN_STATUS_ON_ERROR(transaction.Commit());
  return Status::OK();
}

bool DatabaseConnection::IsZygotic() const {
  return metadata().version == blink::IndexedDBDatabaseMetadata::NO_VERSION;
}

int64_t DatabaseConnection::GetCommittedVersion() const {
  return metadata_snapshot_ ? metadata_snapshot_->version : metadata_.version;
}

uint64_t DatabaseConnection::GetInMemorySize() const {
  CHECK(in_memory());
  // TODO(crbug.com/419203257): For consistency, consider using this logic while
  // reporting usage of on-disk databases too.
  //
  // The maximum page count is ~2^32: https://www.sqlite.org/limits.html.
  uint32_t page_count = 0;
  // The maximum page size is 65536 bytes.
  uint16_t page_size = 0;
  {
    sql::Statement statement(db_->GetReadonlyStatement("PRAGMA page_count"));
    if (!statement.Step()) {
      LogEvent(SpecificEvent::kPragmaPageCountFailed);
      return 0;
    }
    page_count = static_cast<uint32_t>(statement.ColumnInt(0));
  }
  {
    sql::Statement statement(db_->GetReadonlyStatement("PRAGMA page_size"));
    if (!statement.Step()) {
      LogEvent(SpecificEvent::kPragmaPageSizeFailed);
      return 0;
    }
    page_size = static_cast<uint16_t>(statement.ColumnInt(0));
  }
  return static_cast<uint64_t>(page_count) * page_size;
}

std::unique_ptr<BackingStoreDatabaseImpl>
DatabaseConnection::CreateDatabaseWrapper() {
  return std::make_unique<BackingStoreDatabaseImpl>(
      interface_wrapper_weak_factory_.GetWeakPtr());
}

std::unique_ptr<BackingStoreTransactionImpl>
DatabaseConnection::CreateTransactionWrapper(
    base::PassKey<BackingStoreDatabaseImpl>,
    blink::mojom::IDBTransactionDurability durability,
    blink::mojom::IDBTransactionMode mode) {
  return std::make_unique<BackingStoreTransactionImpl>(
      interface_wrapper_weak_factory_.GetWeakPtr(), durability, mode);
}

Status DatabaseConnection::BeginTransaction(
    base::PassKey<BackingStoreTransactionImpl>,
    const BackingStoreTransactionImpl& transaction) {
  // No other transaction can begin while a version change transaction is
  // active.
  CHECK(!HasActiveVersionChangeTransaction());
  if (transaction.mode() == blink::mojom::IDBTransactionMode::ReadOnly) {
    // Nothing to do.
    return Status::OK();
  }
  CHECK(!active_rw_transaction_);
  active_rw_transaction_ = std::make_unique<sql::Transaction>(db_.get());
  if (transaction.durability() ==
      blink::mojom::IDBTransactionDurability::Strict) {
    RETURN_STATUS_ON_ERROR(db_->Execute("PRAGMA synchronous=FULL"));
  } else {
    // WAL mode is guaranteed to be consistent only with synchronous=NORMAL or
    // higher: https://www.sqlite.org/pragma.html#pragma_synchronous.
    RETURN_STATUS_ON_ERROR(db_->Execute("PRAGMA synchronous=NORMAL"));
  }
  RETURN_STATUS_ON_ERROR(active_rw_transaction_->Begin());
  if (transaction.mode() == blink::mojom::IDBTransactionMode::VersionChange) {
    metadata_snapshot_.emplace(metadata_);
  }
  return Status::OK();
}

Status DatabaseConnection::CommitTransactionPhaseOne(
    base::PassKey<BackingStoreTransactionImpl>,
    const BackingStoreTransactionImpl& transaction,
    BlobWriteCallback callback,
    SerializeFsaCallback serialize_fsa_handle) {
  CHECK(blob_write_callback_.is_null());
  CHECK(blob_writers_.empty());
  CHECK_EQ(outstanding_external_object_writes_, 0U);

  std::map<int64_t, IndexedDBExternalObject> blobs_to_commit =
      std::move(blobs_staged_for_commit_);
  for (auto& [blob_row_id, external_object] : blobs_to_commit) {
    {
      // The blob may have been added and deleted in the same txn.
      sql::Statement statement(db_->GetCachedStatement(
          SQL_FROM_HERE, "SELECT 1 FROM blobs WHERE row_id = ?"));
      statement.BindInt64(0, blob_row_id);
      if (!statement.Step()) {
        continue;
      }
    }

    ++outstanding_external_object_writes_;
    if (external_object.object_type() ==
        IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle) {
      serialize_fsa_handle.Run(
          *external_object.file_system_access_token_remote(),
          base::BindOnce(&DatabaseConnection::OnFsaHandleSerialized,
                         blob_writers_weak_factory_.GetWeakPtr(), blob_row_id));
      continue;
    }
    std::unique_ptr<BlobWriter> writer = BlobWriter::WriteBlobIntoDatabase(
        external_object,
        // Unretained is safe because `this` owns `writer`. (And WeakPtr doesn't
        // work with functions that return values.)
        base::BindRepeating(&DatabaseConnection::OpenBlobChunkForStreaming,
                            base::Unretained(this), blob_row_id,
                            /*readonly=*/false),
        // Uses a WeakPtr because the completion callback can be Posted i.e. can
        // be in flight when `writer` is destroyed.
        base::BindOnce(&DatabaseConnection::OnBlobWriteComplete,
                       blob_writers_weak_factory_.GetWeakPtr(), blob_row_id));
    blob_writers_[blob_row_id] = std::move(writer);
  }

  if (outstanding_external_object_writes_ == 0) {
    return std::move(callback).Run(
        BlobWriteResult::kRunPhaseTwoAndReturnResult);
  }

  CHECK_NE(transaction.mode(), blink::mojom::IDBTransactionMode::ReadOnly);
  blob_write_callback_ = std::move(callback);
  return Status::OK();
}

std::optional<sql::StreamingBlobHandle>
DatabaseConnection::OpenBlobChunkForStreaming(int64_t blob_row_id,
                                              bool readonly,
                                              size_t chunk_index) {
  if (chunk_index == 0) {
    return db_->GetStreamingBlob("blobs", "bytes", blob_row_id, readonly);
  }

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT row_id "
                              "FROM overflow_blob_chunks "
                              "WHERE blob_row_id = ? AND chunk_index = ?"));
  statement.BindInt64(0, blob_row_id);
  statement.BindInt64(1, chunk_index);
  if (!statement.Step()) {
    // If the statement succeeded (no SQLite error), there's a programming
    // error.
    if (statement.Succeeded()) {
      Fatal(Status::NotFound("Blob chunk missing"),
            SpecificEvent::kBlobChunkMissing);
    } else {
      LogEvent(SpecificEvent::kOpenBlobForStreamingFailed);
    }
    return std::nullopt;
  }
  int64_t chunk_row_id = statement.ColumnInt64(0);
  return db_->GetStreamingBlob("overflow_blob_chunks", "bytes", chunk_row_id,
                               readonly);
}

void DatabaseConnection::OnBlobWriteComplete(int64_t blob_row_id,
                                             bool success) {
  blob_writers_.erase(blob_row_id);

  if (!success) {
    CancelBlobWriting();
    return;
  }

  if (--outstanding_external_object_writes_ == 0) {
    std::move(blob_write_callback_).Run(BlobWriteResult::kRunPhaseTwoAsync);
  }
}

void DatabaseConnection::OnFsaHandleSerialized(
    int64_t blob_row_id,
    const std::vector<uint8_t>& data) {
  bool success = false;
  if (!data.empty()) {
    sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE,
                                                     "UPDATE blobs "
                                                     "SET bytes = ? "
                                                     "WHERE row_id = ?"));
    statement.BindBlob(0, data);
    statement.BindInt64(1, blob_row_id);
    success = statement.Run();
  }

  OnBlobWriteComplete(blob_row_id, success);
}

void DatabaseConnection::CancelBlobWriting() {
  blob_writers_weak_factory_.InvalidateWeakPtrs();
  blob_writers_.clear();
  outstanding_external_object_writes_ = 0;
  if (blob_write_callback_) {
    std::move(blob_write_callback_)
        .Run(base::unexpected(Status::IOError("Error")));
  }
}

Status DatabaseConnection::CommitTransactionPhaseTwo(
    base::PassKey<BackingStoreTransactionImpl>,
    const BackingStoreTransactionImpl& transaction) {
  if (transaction.mode() == blink::mojom::IDBTransactionMode::ReadOnly) {
    // Nothing to do.
    return Status::OK();
  }
  // No need to sync active blobs when the transaction successfully commits.
  sync_active_blobs_after_transaction_ = false;
  RETURN_STATUS_ON_ERROR(active_rw_transaction_->Commit());
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

  // Abort ongoing or future blob writes, if any.
  blobs_staged_for_commit_.clear();
  blob_write_callback_.Reset();
  CancelBlobWriting();

  active_rw_transaction_->Rollback();

  if (transaction.mode() == blink::mojom::IDBTransactionMode::VersionChange) {
    CHECK(metadata_snapshot_.has_value());
    metadata_ = *std::move(metadata_snapshot_);
    metadata_snapshot_.reset();
  }
}

void DatabaseConnection::EndTransaction(
    base::PassKey<BackingStoreTransactionImpl>,
    const BackingStoreTransactionImpl& transaction) {
  if (transaction.mode() == blink::mojom::IDBTransactionMode::ReadOnly) {
    return;
  }

  // The transaction may have been committed, rolled back, or neither. If
  // neither, this will cause a rollback, although this should only occur if
  // there were no statements executed anyway.
  CHECK(active_rw_transaction_);
  active_rw_transaction_.reset();

  // If the transaction is rolled back, recent changes to the blob_references
  // table may be lost. Make sure that table is up to date with memory state.
  if (sync_active_blobs_after_transaction_) {
    sql::Transaction sql_transaction(db_.get());
    if (!sql_transaction.Begin()) {
      LogEvent(SpecificEvent::kSyncActiveBlobsFailed);
      return;
    }

    // Step 1, mark existing active references with an invalid (but not null)
    // row id. This can't immediately remove them as that could trigger cleanup
    // of the underlying blob.
    {
      sql::Statement statement(
          db_->GetCachedStatement(SQL_FROM_HERE,
                                  "UPDATE blob_references SET record_row_id = 0"
                                  "   WHERE record_row_id IS NULL"));
      if (!statement.Run()) {
        LogEvent(SpecificEvent::kSyncActiveBlobsFailed);
        // `sql_transaction` will attempt rollback.
        return;
      }
    }
    // Step 2, add all the active references.
    for (auto& [blob_number, _] : active_blobs_) {
      if (!AddActiveBlobReference(blob_number)) {
        LogEvent(SpecificEvent::kSyncActiveBlobsFailed);
        // `sql_transaction` will attempt rollback.
        return;
      }
    }
    // Step 3, remove the old references.
    {
      sql::Statement statement(db_->GetCachedStatement(
          SQL_FROM_HERE,
          "DELETE FROM blob_references WHERE record_row_id = 0"));
      if (!statement.Run()) {
        LogEvent(SpecificEvent::kSyncActiveBlobsFailed);
        // `sql_transaction` will attempt rollback.
        return;
      }
    }

    if (!sql_transaction.Commit()) {
      LogEvent(SpecificEvent::kSyncActiveBlobsFailed);
    }
    sync_active_blobs_after_transaction_ = false;
  }
}

Status DatabaseConnection::SetDatabaseVersion(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t version) {
  CHECK(HasActiveVersionChangeTransaction());
  sql::Statement statement(
      db_->GetUniqueStatement("UPDATE indexed_db_metadata SET version = ?"));
  statement.BindInt64(0, version);
  RETURN_STATUS_ON_ERROR(statement.Run());
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
  CHECK(!metadata_.object_stores.contains(object_store_id));
  CHECK_GT(object_store_id, metadata_.max_object_store_id);

  blink::IndexedDBObjectStoreMetadata metadata(
      std::move(name), object_store_id, std::move(key_path), auto_increment);
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO object_stores "
      "(id, name, key_path, auto_increment, key_generator_current_number) "
      "VALUES (?, ?, ?, ?, ?)"));
  statement.BindInt64(0, metadata.id);
  statement.BindBlob(1, metadata.name);
  BindKeyPath(statement, 2, metadata.key_path);
  statement.BindBool(3, metadata.auto_increment);
  statement.BindInt64(4, ObjectStoreMetaDataKey::kKeyGeneratorInitialNumber);
  RETURN_STATUS_ON_ERROR(statement.Run());

  metadata_.object_stores[object_store_id] = std::move(metadata);
  metadata_.max_object_store_id = object_store_id;
  return Status::OK();
}

Status DatabaseConnection::DeleteObjectStore(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id) {
  CHECK(HasActiveVersionChangeTransaction());
  CHECK(metadata_.object_stores.contains(object_store_id));
  {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "DELETE FROM index_references WHERE object_store_id = ?"));
    statement.BindInt64(0, object_store_id);
    RETURN_STATUS_ON_ERROR(statement.Run());
  }
  {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE, "DELETE FROM indexes WHERE object_store_id = ?"));
    statement.BindInt64(0, object_store_id);
    RETURN_STATUS_ON_ERROR(statement.Run());
  }
  {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE, "DELETE FROM records WHERE object_store_id = ?"));
    statement.BindInt64(0, object_store_id);
    RETURN_STATUS_ON_ERROR(statement.Run());
  }
  {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE, "DELETE FROM object_stores WHERE id = ?"));
    statement.BindInt64(0, object_store_id);
    RETURN_STATUS_ON_ERROR(statement.Run());
  }
  CHECK(metadata_.object_stores.erase(object_store_id) == 1);
  return Status::OK();
}

Status DatabaseConnection::RenameObjectStore(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    const std::u16string& new_name) {
  CHECK(HasActiveVersionChangeTransaction());
  CHECK(metadata_.object_stores.contains(object_store_id));

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "UPDATE object_stores SET name = ? WHERE id = ?"));
  statement.BindBlob(0, new_name);
  statement.BindInt64(1, object_store_id);
  RETURN_STATUS_ON_ERROR(statement.Run());
  metadata_.object_stores.at(object_store_id).name = new_name;
  return Status::OK();
}

Status DatabaseConnection::CreateIndex(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    blink::IndexedDBIndexMetadata index) {
  CHECK(HasActiveVersionChangeTransaction());
  CHECK(metadata_.object_stores.contains(object_store_id));

  blink::IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores.at(object_store_id);
  int64_t index_id = index.id;
  CHECK(!object_store.indexes.contains(index_id));
  CHECK_GT(index_id, object_store.max_index_id);

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO indexes "
      "(object_store_id, id, name, key_path, is_unique, multi_entry) "
      "VALUES (?, ?, ?, ?, ?, ?)"));
  statement.BindInt64(0, object_store_id);
  statement.BindInt64(1, index_id);
  statement.BindBlob(2, index.name);
  BindKeyPath(statement, 3, index.key_path);
  statement.BindBool(4, index.unique);
  statement.BindBool(5, index.multi_entry);
  RETURN_STATUS_ON_ERROR(statement.Run());

  object_store.indexes[index_id] = std::move(index);
  object_store.max_index_id = index_id;
  return Status::OK();
}

Status DatabaseConnection::DeleteIndex(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    int64_t index_id) {
  CHECK(HasActiveVersionChangeTransaction());
  ValidateInputs(object_store_id, index_id);

  {
    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "DELETE FROM index_references "
                                "WHERE object_store_id = ? AND index_id = ?"));
    statement.BindInt64(0, object_store_id);
    statement.BindInt64(1, index_id);
    RETURN_STATUS_ON_ERROR(statement.Run());
  }
  {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "DELETE FROM indexes WHERE object_store_id = ? AND id = ?"));
    statement.BindInt64(0, object_store_id);
    statement.BindInt64(1, index_id);
    RETURN_STATUS_ON_ERROR(statement.Run());
  }
  CHECK(metadata_.object_stores.at(object_store_id).indexes.erase(index_id) ==
        1);
  return Status::OK();
}

Status DatabaseConnection::RenameIndex(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    int64_t index_id,
    const std::u16string& new_name) {
  CHECK(HasActiveVersionChangeTransaction());
  ValidateInputs(object_store_id, index_id);

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE indexes SET name = ? WHERE object_store_id = ? AND id = ?"));
  statement.BindBlob(0, new_name);
  statement.BindInt64(1, object_store_id);
  statement.BindInt64(2, index_id);
  RETURN_STATUS_ON_ERROR(statement.Run());
  metadata_.object_stores.at(object_store_id).indexes.at(index_id).name =
      new_name;
  return Status::OK();
}

StatusOr<int64_t> DatabaseConnection::GetKeyGeneratorCurrentNumber(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id) {
  CHECK(metadata_.object_stores.contains(object_store_id));
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT key_generator_current_number "
                              "FROM object_stores WHERE id = ?"));
  statement.BindInt64(0, object_store_id);
  if (!statement.Step()) {
    RETURN_IF_STATEMENT_ERRORED(statement);
    return base::unexpected(
        Fatal(Status::NotFound("Object store not found in database"),
              SpecificEvent::kObjectStoreNotFound));
  }
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
  RETURN_STATUS_ON_ERROR(statement.Run());
  return Status::OK();
}

StatusOr<std::optional<BackingStore::RecordIdentifier>>
DatabaseConnection::GetRecordIdentifierIfExists(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    const blink::IndexedDBKey& key) {
  std::string encoded_key = EncodeSortableIDBKey(key);
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT row_id FROM records "
                              "WHERE object_store_id = ? AND key = ?"));
  statement.BindInt64(0, object_store_id);
  statement.BindBlob(1, encoded_key);
  if (statement.Step()) {
    return BackingStore::RecordIdentifier{statement.ColumnInt64(0),
                                          std::move(encoded_key)};
  }
  RETURN_IF_STATEMENT_ERRORED(statement);
  return std::nullopt;
}

StatusOr<IndexedDBValue> DatabaseConnection::GetValue(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    const blink::IndexedDBKey& key) {
  IndexedDBValue value;
  int64_t record_row_id;

  {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT row_id, value, compression_type FROM records "
        "WHERE object_store_id = ? AND key = ?"));
    statement.BindInt64(0, object_store_id);
    statement.BindBlob(1, EncodeSortableIDBKey(key));
    if (!statement.Step()) {
      RETURN_IF_STATEMENT_ERRORED(statement);
      return IndexedDBValue();
    }
    record_row_id = statement.ColumnInt64(0);

    base::span<const uint8_t> bits = statement.ColumnBlob(1);
    int compression_type = statement.ColumnInt(2);
    ASSIGN_OR_RETURN(value.bits, Decompress(bits, compression_type));
  }

  return AddExternalObjectMetadataToValue(std::move(value), record_row_id);
}

StatusOr<IndexedDBValue> DatabaseConnection::AddExternalObjectMetadataToValue(
    IndexedDBValue value,
    int64_t record_row_id) {
  // First add Blob and File objects' metadata (not FSA handles).
  {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT "
        "  blobs.row_id, object_type, mime_type, size_bytes, file_name, "
        "  last_modified "
        "FROM blobs INNER JOIN blob_references"
        "  ON blob_references.blob_row_id = blobs.row_id "
        "WHERE"
        "  blob_references.record_row_id = ? AND object_type != ? "
        // The order is important because the serialized data uses indexes to
        // refer to embedded external objects.
        "ORDER BY blobs.row_id"));
    statement.BindInt64(0, record_row_id);
    statement.BindInt64(
        1, static_cast<int>(
               IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle));
    while (statement.Step()) {
      const int64_t blob_row_id = statement.ColumnInt64(0);
      if (auto it = blobs_staged_for_commit_.find(blob_row_id);
          it != blobs_staged_for_commit_.end()) {
        // If the blob is being written in this transaction, copy the external
        // object (and later the Blob mojo endpoint) from
        // `blobs_staged_for_commit_`.
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
          return base::unexpected(
              Fatal(Status::Corruption("Unknown object type in `blobs`"),
                    SpecificEvent::kBlobTypeUnknown));
        }
      }
    }
    RETURN_IF_STATEMENT_ERRORED(statement);
  }
  // Then add FileSystemAccessHandle objects' metadata.
  {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT "
        "  blobs.row_id, bytes "
        "FROM blobs INNER JOIN blob_references"
        "  ON blob_references.blob_row_id = blobs.row_id "
        "WHERE"
        "  blob_references.record_row_id = ? AND object_type = ? "
        "ORDER BY blobs.row_id"));
    statement.BindInt64(0, record_row_id);
    statement.BindInt64(
        1, static_cast<int>(
               IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle));
    while (statement.Step()) {
      const int64_t blob_row_id = statement.ColumnInt64(0);
      if (auto it = blobs_staged_for_commit_.find(blob_row_id);
          it != blobs_staged_for_commit_.end()) {
        mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
            token_clone;
        it->second.file_system_access_token_remote()->Clone(
            token_clone.InitWithNewPipeAndPassReceiver());
        value.external_objects.emplace_back(std::move(token_clone));
      } else {
        base::span<const uint8_t> serialized_handle = statement.ColumnBlob(1);
        value.external_objects.emplace_back(std::vector<uint8_t>(
            serialized_handle.begin(), serialized_handle.end()));
      }
    }
    RETURN_IF_STATEMENT_ERRORED(statement);
  }

  return value;
}

StatusOr<BackingStore::RecordIdentifier> DatabaseConnection::PutRecord(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    const blink::IndexedDBKey& key,
    IndexedDBValue value) {
  // Insert record, including inline data.
  const std::string encoded_key = EncodeSortableIDBKey(key);
  {
    // `bits_copy` *may* be used to briefly own the data that is copied into the
    // SQL db. NB: this is declared here to ensure its lifetime exceeds that of
    // `statement`.
    std::vector<uint8_t> bits_copy;
    // `bits_span` *will* refer to the data that should be copied into the SQL
    // db.
    base::span<const uint8_t> bits_span;

    // "INSERT OR REPLACE" deletes the row corresponding to
    // [object_store_id, key] if it exists and inserts a new row with `value`.
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT OR REPLACE INTO records "
        "(object_store_id, key, compression_type, value) VALUES (?, ?, ?, ?)"));
    statement.BindInt64(0, object_store_id);
    statement.BindBlob(1, encoded_key);

    CompressionType compression_type = CompressionType::kUncompressed;

    static constexpr base::ByteSize kMinimumCompressionSize(64);
    static constexpr float kMinimumCompressionRatio = 0.8f;
    if (value.bits.storage_type() ==
        mojo_base::BigBuffer::StorageType::kSharedMemory) {
      // Make a copy of the bits if they are in shared memory before attempting
      // to compress. See BigBuffer docs re: TOCTOU bugs.
      bits_copy = base::ToVector(std::move(value.bits));
      bits_span = base::span(bits_copy);
    } else {
      bits_span = base::span(value.bits);
    }

    // Maybe compress, updating `bits_span` and `bits_copy` as appropriate.
    if (bits_span.size() >= kMinimumCompressionSize.InBytes()) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
      size_t max_compressed_size = ZSTD_compressBound(bits_span.size());
      std::vector<uint8_t> compressed_bits(max_compressed_size);

      // Compression level of -4 yields compression output similar to Snappy.
      size_t compressed_length = ZSTD_compress(
          compressed_bits.data(), compressed_bits.size(), bits_span.data(),
          bits_span.size(), /*compressionLevel=*/-4);
      compression_type = CompressionType::kZstd;
#else
      size_t max_compressed_size =
          snappy::MaxCompressedLength(bits_span.size());
      std::vector<uint8_t> compressed_bits(max_compressed_size);
      size_t compressed_length = 0;
      base::span<const char> src = base::as_chars(bits_span);
      base::span<char> dest =
          base::as_writable_chars(base::span(compressed_bits));
      snappy::RawCompress(src.data(), src.size(), dest.data(),
                          &compressed_length);
      compression_type = CompressionType::kSnappy;
#endif
      if (compressed_length <= bits_span.size() * kMinimumCompressionRatio) {
        compressed_bits.resize(compressed_length);
        bits_copy = std::move(compressed_bits);
        bits_span = base::span(bits_copy);
      } else {
        compression_type = CompressionType::kUncompressed;
      }
    }

    statement.BindInt(2, static_cast<int>(compression_type));
    // Passing `bits_span` directly would make an unnecessary copy via
    // `RefCountedBytes`, so construct a `RefCountedStaticMemory`, which doesn't
    // copy. This is safe as long as `statement` is destroyed and clears the
    // `RefCountedStaticMemory` binding *before* the object that owns that
    // memory (`bits_copy` or `value.bits`) is destroyed.
    statement.BindBlob(
        3, base::MakeRefCounted<base::RefCountedStaticMemory>(bits_span));
    RUN_STATEMENT_RETURN_ON_ERROR(statement);
  }
  const int64_t record_row_id = db_->GetLastInsertRowId();

  // Insert external objects into relevant tables.
  for (auto& external_object : value.external_objects) {
    int64_t blob_row_id = -1;
    if (external_object.object_type() ==
        IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle) {
      // Write metadata. Blob bytes will be written later in one go, after
      // serializing the handle.
      sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE,
                                                       "INSERT INTO blobs "
                                                       "(object_type) "
                                                       "VALUES (?)"));
      statement.BindInt(0, static_cast<int>(external_object.object_type()));
      RUN_STATEMENT_RETURN_ON_ERROR(statement);
      blob_row_id = db_->GetLastInsertRowId();
    } else {
      // Write metadata and reserve space for the `bytes` column. Blob bytes are
      // not actually written yet though.
      int main_chunk_size =
          std::min(external_object.size(), GetMaxBlobSize().InBytes());
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
        statement.BindBlobForStreaming(3, main_chunk_size);
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
        RUN_STATEMENT_RETURN_ON_ERROR(statement);
      }

      blob_row_id = db_->GetLastInsertRowId();

      // Reserve space for overflow chunks, if any.
      int chunk_index = 1;
      for (int64_t bytes_written = main_chunk_size;
           bytes_written < external_object.size();) {
        const int64_t chunk_size = std::min(
            external_object.size() - bytes_written, GetMaxBlobSize().InBytes());
        sql::Statement statement(
            db_->GetCachedStatement(SQL_FROM_HERE,
                                    "INSERT INTO overflow_blob_chunks "
                                    "(blob_row_id, chunk_index, bytes)"
                                    "VALUES (?, ?, ?)"));
        statement.BindInt64(0, blob_row_id);
        statement.BindInt(1, chunk_index++);
        statement.BindBlobForStreaming(2, chunk_size);
        RUN_STATEMENT_RETURN_ON_ERROR(statement);
        bytes_written += chunk_size;
      }
    }

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
      RUN_STATEMENT_RETURN_ON_ERROR(statement);
    }

    auto rv =
        blobs_staged_for_commit_.emplace(blob_row_id,
                                         // TODO(crbug.com/419208485): this type
                                         // is copy only at the moment.
                                         std::move(external_object));
    CHECK(rv.second);
  }
  OnRecordsModified(object_store_id);
  return BackingStore::RecordIdentifier{record_row_id, std::move(encoded_key)};
}

Status DatabaseConnection::DeleteRange(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    const blink::IndexedDBKeyRange& key_range) {
  std::vector<std::string_view> query_pieces =
      StartRecordRangeQuery("DELETE", key_range);
  sql::Statement statement(db_->GetUniqueStatement(base::StrCat(query_pieces)));
  BindRecordRangeQueryParams(statement, object_store_id, key_range);
  RETURN_STATUS_ON_ERROR(statement.Run());
  OnRecordsModified(object_store_id);
  return Status::OK();
}

Status DatabaseConnection::ClearObjectStore(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM records WHERE object_store_id = ?"));
  statement.BindInt64(0, object_store_id);
  RETURN_STATUS_ON_ERROR(statement.Run());
  OnRecordsModified(object_store_id);
  return Status::OK();
}

StatusOr<uint32_t> DatabaseConnection::GetObjectStoreKeyCount(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    blink::IndexedDBKeyRange key_range) {
  std::vector<std::string_view> query_pieces =
      StartRecordRangeQuery("SELECT COUNT()", key_range);
  // TODO(crbug.com/40253999): Evaluate performance benefit of using
  // `GetCachedStatement()` instead.
  sql::Statement statement(
      db_->GetReadonlyStatement(base::StrCat(query_pieces)));
  BindRecordRangeQueryParams(statement, object_store_id, key_range);
  if (!statement.Step()) {
    RETURN_IF_STATEMENT_ERRORED(statement);
    // COUNT() can't fail to return a value.
    NOTREACHED();
  }
  return statement.ColumnInt(0);
}

Status DatabaseConnection::PutIndexDataForRecord(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key,
    const BackingStore::RecordIdentifier& record) {
  ValidateInputs(object_store_id, index_id);
  // `PutIndexDataForRecord()` can be called more than once with the same `key`
  // and `record` - in the case of multi-entry indexes, for example.
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR IGNORE INTO index_references "
      "(record_row_id, index_id, key, object_store_id, record_key) "
      "VALUES (?, ?, ?, ?, ?)"));
  statement.BindInt64(0, record.number);
  statement.BindInt64(1, index_id);
  statement.BindBlob(2, EncodeSortableIDBKey(key));
  statement.BindInt64(3, object_store_id);
  statement.BindBlob(4, record.data);
  RETURN_STATUS_ON_ERROR(statement.Run());
  return Status::OK();
}

StatusOr<blink::IndexedDBKey> DatabaseConnection::GetFirstPrimaryKeyForIndexKey(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    int64_t index_id,
    const blink::IndexedDBKey& key) {
  ValidateInputs(object_store_id, index_id);
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT MIN(record_key) FROM index_references "
      "WHERE object_store_id = ? AND index_id = ? AND key = ?"));
  statement.BindInt64(0, object_store_id);
  statement.BindInt64(1, index_id);
  statement.BindBlob(2, EncodeSortableIDBKey(key));
  if (statement.Step()) {
    return DecodeSortableIDBKey(statement.ColumnBlobAsString(0));
  }
  RETURN_IF_STATEMENT_ERRORED(statement);
  // Not found.
  return blink::IndexedDBKey();
}

StatusOr<uint32_t> DatabaseConnection::GetIndexKeyCount(
    base::PassKey<BackingStoreTransactionImpl>,
    int64_t object_store_id,
    int64_t index_id,
    blink::IndexedDBKeyRange key_range) {
  ValidateInputs(object_store_id, index_id);
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
  if (!statement.Step()) {
    RETURN_IF_STATEMENT_ERRORED(statement);
    // COUNT() can't fail to return a value.
    NOTREACHED();
  }
  return statement.ColumnInt(0);
}

std::vector<blink::mojom::IDBExternalObjectPtr>
DatabaseConnection::CreateAllExternalObjects(
    base::PassKey<BackingStoreTransactionImpl>,
    const std::vector<IndexedDBExternalObject>& objects,
    DeserializeFsaCallback deserialize_fsa_handle) {
  std::vector<blink::mojom::IDBExternalObjectPtr> mojo_objects;
  IndexedDBExternalObject::ConvertToMojo(objects, &mojo_objects);

  for (size_t i = 0; i < objects.size(); ++i) {
    const IndexedDBExternalObject& object = objects[i];
    blink::mojom::IDBExternalObjectPtr& mojo_object = mojo_objects[i];
    if (object.object_type() ==
        IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle) {
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
          mojo_token;
      if (object.is_file_system_access_remote_valid()) {
        // The remote will be valid if this is a pending FSA handle i.e. came
        // from `blobs_staged_for_commit_`.
        object.file_system_access_token_remote()->Clone(
            mojo_token.InitWithNewPipeAndPassReceiver());
      } else {
        CHECK(!object.serialized_file_system_access_handle().empty());
        deserialize_fsa_handle.Run(
            object.serialized_file_system_access_handle(),
            mojo_token.InitWithNewPipeAndPassReceiver());
      }
      mojo_object->set_file_system_access_token(std::move(mojo_token));
      continue;
    }
    mojo::PendingReceiver<blink::mojom::Blob> receiver =
        mojo_object->get_blob_or_file()->blob.InitWithNewPipeAndPassReceiver();
    // The remote will be valid if this is a pending blob i.e. came from
    // `blobs_staged_for_commit_`.
    if (object.is_remote_valid()) {
      object.Clone(std::move(receiver));
      continue;
    }

    // Otherwise the blob is in the database already. Look up or create the
    // object that manages the active blob.
    auto it = active_blobs_.find(object.blob_number());
    if (it == active_blobs_.end()) {
      auto streamer = std::make_unique<ActiveBlobStreamer>(
          object,
          // Unretained is safe because `this` owns `streamer`.
          base::BindRepeating(&DatabaseConnection::OpenBlobChunkForStreaming,
                              base::Unretained(this), object.blob_number(),
                              /*readonly=*/true),
          GetMaxBlobSize().InBytes(),
          base::BindOnce(&DatabaseConnection::OnBlobBecameInactive,
                         base::Unretained(this), object.blob_number()),
          base::BindRepeating(&LogNetError, "IndexedDB.BackingStore.ReadBlob",
                              in_memory()));
      it = active_blobs_.insert({object.blob_number(), std::move(streamer)})
               .first;
      if (!AddActiveBlobReference(object.blob_number())) {
        LogEvent(SpecificEvent::kAddActiveBlobReferenceFailed);
      }
    }
    it->second->AddReceiver(std::move(receiver),
                            backing_store_->blob_storage_context());
  }
  return mojo_objects;
}

void DatabaseConnection::DeleteIdbDatabase(
    base::PassKey<BackingStoreDatabaseImpl>) {
  marked_for_permanent_deletion_ = true;
  metadata_ = blink::IndexedDBDatabaseMetadata(metadata_.name);
  interface_wrapper_weak_factory_.InvalidateWeakPtrs();
  CHECK(!blob_writers_weak_factory_.HasWeakPtrs());

  if (CanSelfDestruct()) {
    // Fast path: skip explicitly deleting data as the whole database will be
    // dropped.
    backing_store_->DestroyConnection(metadata_.name);
    // `this` is deleted.
    return;
  }

  cursor_weak_factory_.InvalidateWeakPtrs();
  cursor_statements_.clear();

  // Since blobs are still active, reset to zygotic state instead of destroying.
  bool success =
      db_->Execute(
          "DELETE FROM blob_references WHERE record_row_id IS NOT NULL") &&
      db_->Execute("DELETE FROM index_references") &&
      db_->Execute("DELETE FROM indexes") &&
      db_->Execute("DELETE FROM records") &&
      db_->Execute("DELETE FROM object_stores") && [&]() {
        sql::Statement statement(db_->GetUniqueStatement(
            "UPDATE indexed_db_metadata SET version = ?"));
        statement.BindInt64(0, blink::IndexedDBDatabaseMetadata::NO_VERSION);
        return statement.Run();
      }();

  // If there are any errors in the above, then blobs will probably error out
  // too, so go ahead and destroy `this`.
  if (!success) {
    backing_store_->DestroyConnection(metadata_.name);
    // `this` is deleted.
  }
}

void DatabaseConnection::OnBlobBecameInactive(int64_t blob_number) {
  CHECK_EQ(active_blobs_.erase(blob_number), 1U);

  if (active_rw_transaction_) {
    sync_active_blobs_after_transaction_ = true;
  }

  {
    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "DELETE FROM blob_references "
                                "WHERE blob_row_id = ? "
                                "AND record_row_id IS NULL"));
    statement.BindInt64(0, blob_number);
    if (!statement.Run()) {
      LogEvent(SpecificEvent::kRemoveActiveBlobReferenceFailed);
    }
  }

  if (CanSelfDestruct()) {
    backing_store_->DestroyConnection(metadata_.name);
    // `this` is deleted.
    return;
  }
}

bool DatabaseConnection::AddActiveBlobReference(int64_t blob_number) {
  if (active_rw_transaction_) {
    sync_active_blobs_after_transaction_ = true;
  }

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "INSERT INTO blob_references (blob_row_id) VALUES (?)"));
  statement.BindInt64(0, blob_number);
  return statement.Run();
}

bool DatabaseConnection::CanSelfDestruct() const {
  // In-memory databases must remain alive until the BrowserContext is destroyed
  // (which destroys the BackingStore).
  if (in_memory() && !marked_for_permanent_deletion_) {
    return false;
  }

  return active_blobs_.empty() &&
         !interface_wrapper_weak_factory_.HasWeakPtrs();
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
  return ObjectStoreCursorImpl::Create(cursor_weak_factory_.GetWeakPtr(),
                                       object_store_id, key_range, key_only,
                                       ascending_order);
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
  return IndexCursorImpl::Create(cursor_weak_factory_.GetWeakPtr(),
                                 object_store_id, index_id, key_range, key_only,
                                 first_primary_keys_only, ascending_order);
}

std::tuple<uint64_t, sql::Statement*> DatabaseConnection::CreateCursorStatement(
    base::PassKey<BackingStoreCursorImpl>,
    std::string query,
    int64_t object_store_id) {
  auto [it, inserted] = cursor_statements_.emplace(
      ++next_statement_id_, std::make_tuple(std::make_unique<sql::Statement>(
                                                db_->GetUniqueStatement(query)),
                                            object_store_id));
  CHECK(inserted);
  return {next_statement_id_, std::get<0>(it->second).get()};
}

void DatabaseConnection::ReleaseCursorStatement(
    base::PassKey<BackingStoreCursorImpl>,
    uint64_t id) {
  CHECK_EQ(1U, cursor_statements_.erase(id));
}

sql::Statement* DatabaseConnection::GetCursorStatement(
    base::PassKey<BackingStoreCursorImpl>,
    uint64_t id) {
  auto it = cursor_statements_.find(id);
  if (it == cursor_statements_.end()) {
    return nullptr;
  }
  return std::get<0>(it->second).get();
}

void DatabaseConnection::OnRecordsModified(int64_t object_store_id) {
  for (const auto& [_, statement_holder] : cursor_statements_) {
    const auto& [statement, cursor_object_store_id] = statement_holder;
    if (cursor_object_store_id == object_store_id) {
      BackingStoreCursorImpl::InvalidateStatement(*statement);
    }
  }
}

Status DatabaseConnection::GetStatusOfLastOperation(
    base::PassKey<BackingStoreCursorImpl>) {
  return Status(*db_);
}

StatusOr<blink::IndexedDBDatabaseMetadata>
DatabaseConnection::GenerateIndexedDbMetadata() {
  blink::IndexedDBDatabaseMetadata metadata;

  // Set the database name and version.
  {
    sql::Statement statement(db_->GetReadonlyStatement(
        "SELECT name, version FROM indexed_db_metadata"));
    if (!statement.Step()) {
      RETURN_IF_STATEMENT_ERRORED(statement);
      return base::unexpected(
          Fatal(Status::Corruption("Missing table `indexed_db_metadata`"),
                SpecificEvent::kMissingMetadataTable));
    }
    ASSIGN_OR_RETURN(metadata.name, statement.ColumnBlobAsString16(0), [&]() {
      return Fatal(Status::Corruption("Database name is unexpected size"),
                   SpecificEvent::kUtf16StringUnreadable);
    });
    metadata.version = statement.ColumnInt64(1);
  }

  // Populate object store metadata.
  {
    sql::Statement statement(db_->GetReadonlyStatement(
        "SELECT id, name, key_path, auto_increment FROM object_stores"));
    int64_t max_object_store_id = 0;
    while (statement.Step()) {
      blink::IndexedDBObjectStoreMetadata store_metadata;
      store_metadata.id = statement.ColumnInt64(0);
      ASSIGN_OR_RETURN(
          store_metadata.name, statement.ColumnBlobAsString16(1), [&]() {
            return Fatal(
                Status::Corruption("Object store name is unexpected size"),
                SpecificEvent::kUtf16StringUnreadable);
          });
      ASSIGN_OR_RETURN(store_metadata.key_path, ColumnKeyPath(statement, 2),
                       [&](Status error) {
                         return Fatal(error,
                                      SpecificEvent::kUtf16StringUnreadable);
                       });
      store_metadata.auto_increment = statement.ColumnBool(3);
      max_object_store_id = std::max(max_object_store_id, store_metadata.id);
      metadata.object_stores[store_metadata.id] = std::move(store_metadata);
    }
    RETURN_IF_STATEMENT_ERRORED(statement);
    metadata.max_object_store_id = max_object_store_id;
  }

  // Populate index metadata.
  {
    sql::Statement statement(db_->GetReadonlyStatement(
        "SELECT object_store_id, id, name, key_path, is_unique, multi_entry "
        "FROM indexes"));
    while (statement.Step()) {
      blink::IndexedDBIndexMetadata index_metadata;
      int64_t object_store_id = statement.ColumnInt64(0);
      index_metadata.id = statement.ColumnInt64(1);
      ASSIGN_OR_RETURN(
          index_metadata.name, statement.ColumnBlobAsString16(2), [&]() {
            return Fatal(Status::Corruption("Index name is unexpected size"),
                         SpecificEvent::kUtf16StringUnreadable);
          });
      ASSIGN_OR_RETURN(index_metadata.key_path, ColumnKeyPath(statement, 3),
                       [&](Status error) {
                         return Fatal(error,
                                      SpecificEvent::kUtf16StringUnreadable);
                       });
      index_metadata.unique = statement.ColumnBool(4);
      index_metadata.multi_entry = statement.ColumnBool(5);
      blink::IndexedDBObjectStoreMetadata& store_metadata =
          metadata.object_stores[object_store_id];
      store_metadata.max_index_id =
          std::max(store_metadata.max_index_id, index_metadata.id);
      store_metadata.indexes[index_metadata.id] = std::move(index_metadata);
    }
    RETURN_IF_STATEMENT_ERRORED(statement);
  }

  return metadata;
}

void DatabaseConnection::LogEvent(SpecificEvent event) const {
  if (in_memory()) {
    base::UmaHistogramEnumeration("IndexedDB.SQLite.SpecificEvent.InMemory",
                                  event);
  } else {
    base::UmaHistogramEnumeration("IndexedDB.SQLite.SpecificEvent.OnDisk",
                                  event);
  }
}

Status DatabaseConnection::Fatal(Status s, SpecificEvent event) {
  LogEvent(event);
  marked_for_permanent_deletion_ = true;
  return s;
}

void DatabaseConnection::ValidateInputs(int64_t object_store_id,
                                        int64_t index_id) {
  auto iter = metadata_.object_stores.find(object_store_id);
  CHECK(iter != metadata_.object_stores.end());
  CHECK(iter->second.indexes.contains(index_id));
}

StatusOr<mojo_base::BigBuffer> DatabaseConnection::Decompress(
    base::span<const uint8_t> compressed,
    int compression_type) {
  return DoDecompress(compressed, compression_type)
      .transform_error([&](Status status) {
        return Fatal(status, SpecificEvent::kDecompressionFailure);
      });
}

// static
void DatabaseConnection::OverrideMaxBlobSizeForTesting(base::ByteCount size) {
  g_max_blob_size_override = size;
}

}  // namespace content::indexed_db::sqlite
