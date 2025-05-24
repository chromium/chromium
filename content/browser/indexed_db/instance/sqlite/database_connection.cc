// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/database_connection.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_transaction_impl.h"
#include "content/browser/indexed_db/status.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

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
blink::IndexedDBKeyPath DecodeKeyPath(const std::u16string& key_path) {
  if (key_path.empty()) {
    return blink::IndexedDBKeyPath();
  }
  std::vector<std::u16string> parts = base::SplitString(
      key_path, kKeyPathSeparator, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
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
  // However, "key_path"s are always valid UTF-16 since they contain only
  // identifiers (required to be valid UTF-16) and periods.
  // TODO(crbug.com/40253999): Appropriately handle invalid UTF-16 names.
  //
  // Stores a single row containing the properties of
  // `IndexedDBDatabaseMetadata` for this database.
  TRANSIENT_CHECK(
      db->Execute("CREATE TABLE indexed_db_metadata "
                  "(name TEXT NOT NULL UNIQUE,"
                  " version INTEGER NOT NULL)"));
  TRANSIENT_CHECK(
      db->Execute("CREATE TABLE object_stores "
                  "(id INTEGER PRIMARY KEY,"
                  " name TEXT NOT NULL UNIQUE,"
                  " key_path TEXT,"
                  " auto_increment INTEGER NOT NULL)"));

  // Insert the initial metadata entry.
  sql::Statement statement(
      db->GetUniqueStatement("INSERT INTO indexed_db_metadata "
                             "(name, version) VALUES (?, ?)"));
  statement.BindString16(0, name);
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
    metadata.name = statement.ColumnString16(0);
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
      store_metadata.name = statement.ColumnString16(1);
      store_metadata.key_path = DecodeKeyPath(statement.ColumnString16(2));
      store_metadata.auto_increment = statement.ColumnBool(3);
      max_object_store_id = std::max(max_object_store_id, store_metadata.id);
      metadata.object_stores[store_metadata.id] = std::move(store_metadata);
    }
    TRANSIENT_CHECK(statement.Succeeded());
    metadata.max_object_store_id = max_object_store_id;
  }

  return metadata;
}

}  // namespace

// static
StatusOr<std::unique_ptr<DatabaseConnection>> DatabaseConnection::Open(
    const std::u16string& name,
    const base::FilePath& file_path) {
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

  return base::WrapUnique(new DatabaseConnection(
      std::move(db), std::move(meta_table), std::move(metadata)));
}

DatabaseConnection::DatabaseConnection(
    std::unique_ptr<sql::Database> db,
    std::unique_ptr<sql::MetaTable> meta_table,
    blink::IndexedDBDatabaseMetadata metadata)
    : db_(std::move(db)),
      meta_table_(std::move(meta_table)),
      metadata_(std::move(metadata)) {}

DatabaseConnection::~DatabaseConnection() = default;

base::WeakPtr<DatabaseConnection> DatabaseConnection::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<BackingStoreTransactionImpl>
DatabaseConnection::CreateTransaction(
    base::PassKey<BackingStoreDatabaseImpl>,
    blink::mojom::IDBTransactionDurability durability,
    blink::mojom::IDBTransactionMode mode) {
  // TODO(crbug.com/40253999): Ensure that `DatabaseConnection` outlives active
  // instances of `BackingStoreTransactionImpl`.
  auto transaction = std::make_unique<sql::Transaction>(db_.get());

  // TODO(crbug.com/40253999): Assert preconditions for `mode`.
  return std::make_unique<BackingStoreTransactionImpl>(
      GetWeakPtr(), std::move(transaction), durability, mode);
}

void DatabaseConnection::OnTransactionBegin(
    base::PassKey<BackingStoreTransactionImpl>,
    const BackingStoreTransactionImpl& transaction) {
  // No other transaction can begin while a version change transaction is
  // active.
  CHECK(!HasActiveVersionChangeTransaction());
  if (transaction.mode() == blink::mojom::IDBTransactionMode::VersionChange) {
    metadata_snapshot_.emplace(metadata_);
  }
}

void DatabaseConnection::OnBeforeTransactionCommit(
    base::PassKey<BackingStoreTransactionImpl>,
    const BackingStoreTransactionImpl& transaction) {
  if (transaction.durability() ==
          blink::mojom::IDBTransactionDurability::Strict &&
      transaction.mode() != blink::mojom::IDBTransactionMode::ReadOnly) {
    // TODO(crbug.com/40253999): Execute `PRAGMA synchronous=FULL`
  }
}

void DatabaseConnection::OnTransactionCommit(
    base::PassKey<BackingStoreTransactionImpl>,
    const BackingStoreTransactionImpl& transaction) {
  // TODO(crbug.com/40253999): Reset the `synchronous` setting.
  if (transaction.mode() == blink::mojom::IDBTransactionMode::VersionChange) {
    CHECK(metadata_snapshot_.has_value());
    metadata_snapshot_.reset();
  }
}

void DatabaseConnection::OnTransactionRollback(
    base::PassKey<BackingStoreTransactionImpl>,
    const BackingStoreTransactionImpl& transaction) {
  // TODO(crbug.com/40253999): Reset the `synchronous` setting.
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
      "(id, name, key_path, auto_increment) VALUES (?, ?, ?, ?)"));
  statement.BindInt64(0, metadata.id);
  statement.BindString16(1, metadata.name);
  statement.BindString16(2, EncodeKeyPath(metadata.key_path));
  statement.BindBool(3, metadata.auto_increment);
  TRANSIENT_CHECK(statement.Run());

  metadata_.object_stores[object_store_id] = std::move(metadata);
  metadata_.max_object_store_id = object_store_id;
  return Status::OK();
}

}  // namespace content::indexed_db::sqlite
