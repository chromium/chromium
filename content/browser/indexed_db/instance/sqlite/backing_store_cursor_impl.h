// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_CURSOR_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_CURSOR_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/status.h"

namespace sql {
class Statement;
}

namespace content::indexed_db {
class Record;

namespace sqlite {
class DatabaseConnection;

// Concrete subclasses are defined alongside `DatabaseConnection`, which owns
// the actual SQLite resources (for statements) and the `sql::Database`.
class BackingStoreCursorImpl : public BackingStore::Cursor {
 public:
  ~BackingStoreCursorImpl() override;

  // Resets the statement if it is live (being stepped).
  static void InvalidateStatement(sql::Statement& statement);

  // BackingStore::Cursor:
  const blink::IndexedDBKey& GetKey() const override;
  blink::IndexedDBKey TakeKey() && override;
  const blink::IndexedDBKey& GetPrimaryKey() const override;
  IndexedDBValue& GetValue() override;
  StatusOr<bool> Continue() override;
  StatusOr<bool> Continue(const blink::IndexedDBKey& key,
                          const blink::IndexedDBKey& primary_key) override;
  StatusOr<bool> Advance(uint32_t count) override;
  Status TryResetToLastSavedPosition() override;

 protected:
  BackingStoreCursorImpl(base::WeakPtr<DatabaseConnection> db,
                         int64_t statement_id);

  static constexpr base::PassKey<BackingStoreCursorImpl> PassKey() {
    return {};
  }

  DatabaseConnection* db() const { return db_.get(); }

  // Updates the record that `this` is pointing to by reading the current row
  // from `statement`.
  Status UpdateRecord(sql::Statement& statement);

  // Binds variable parameters to `statement`, these being, abstractly:
  // - the cursor position, which is the encoded key of the last record returned
  //   by `ReadRow()`.
  // - the key and primary key (if valid and applicable to the cursor type) to
  //   seek to.
  virtual void BindParameters(
      sql::Statement& statement,
      const blink::IndexedDBKey& target_key,
      const blink::IndexedDBKey& target_primary_key) = 0;

  // Updates the current position and returns the record from the current row.
  virtual StatusOr<std::unique_ptr<Record>> ReadRow(
      sql::Statement& statement) = 0;

 private:
  // Returns the parsed and bound statement that embeds the SQL query for this
  // iterator. The query itself is immutable for the duration of `this`, and
  // contains a mix of fixed and variable bound parameters. To avoid re-parsing
  // the query and rebinding the fixed parameters every time, the statement is
  // created once and registered with the `DatabaseConnection`, and released
  // when `this` is destroyed.
  //
  // The records contained in the range can change between `Iterate()` calls;
  // the `sql::Statement` is invalidated (reset) when it can no longer be
  // stepped with a guarantee of correctness. If `sql::Statement::Succeeded()`
  // is true, the statement is live and can be stepped without rebinding, else
  // the variable parameters need to be bound again using `BindParameters()`
  // below before the statement is stepped.
  //
  // May return null if the SQLite database was closed e.g. due to an error.
  sql::Statement* GetStatement();

  base::WeakPtr<DatabaseConnection> db_;
  uint64_t statement_id_;

  std::unique_ptr<Record> current_record_;
};

}  // namespace sqlite
}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_CURSOR_IMPL_H_
