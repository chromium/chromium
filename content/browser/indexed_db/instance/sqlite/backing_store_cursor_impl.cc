// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/backing_store_cursor_impl.h"

#include <memory>
#include <utility>

#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "content/browser/indexed_db/instance/record.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "content/browser/indexed_db/status.h"
#include "sql/statement.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

namespace content::indexed_db::sqlite {

BackingStoreCursorImpl::BackingStoreCursorImpl(
    base::WeakPtr<DatabaseConnection> db,
    int64_t statement_id)
    : db_(std::move(db)), statement_id_(statement_id) {}

BackingStoreCursorImpl::~BackingStoreCursorImpl() {
  if (db_) {
    db_->ReleaseCursorStatement(PassKey(), statement_id_);
  }
}

// static
void BackingStoreCursorImpl::InvalidateStatement(sql::Statement& statement) {
  if (statement.Succeeded()) {
    statement.Reset(/*clear_bound_vars=*/false);
  }
}

const blink::IndexedDBKey& BackingStoreCursorImpl::GetKey() const {
  return current_record_->key();
}

blink::IndexedDBKey BackingStoreCursorImpl::TakeKey() && {
  return std::move(current_record_->key());
}

const blink::IndexedDBKey& BackingStoreCursorImpl::GetPrimaryKey() const {
  return current_record_->primary_key();
}

IndexedDBValue& BackingStoreCursorImpl::GetValue() {
  return current_record_->value();
}

StatusOr<bool> BackingStoreCursorImpl::Continue() {
  return Advance(1);
}

StatusOr<bool> BackingStoreCursorImpl::Continue(
    const blink::IndexedDBKey& key,
    const blink::IndexedDBKey& primary_key) {
  if (!key.IsValid()) {
    // It is faster to `Advance()` when no target key is specified.
    return Advance(1);
  }

  sql::Statement* statement = GetStatement();
  if (!statement) {
    return base::unexpected(Status::IOError("Database connection lost"));
  }

  // Necessarily invalidate the statement since a target key has been specified.
  InvalidateStatement(*statement);
  BindParameters(*statement, key, primary_key);

  if (!statement->Step()) {
    if (!statement->Succeeded()) {
      return base::unexpected(db_->GetStatusOfLastOperation(PassKey()));
    }
    // End of range.
    InvalidateStatement(*statement);
    current_record_.reset();
    return false;
  }
  Status s = UpdateRecord(*statement);
  if (!s.ok()) {
    return base::unexpected(s);
  }
  return true;
}

StatusOr<bool> BackingStoreCursorImpl::Advance(uint32_t count) {
  if (count == 0) {
    return base::unexpected(
        Status::InvalidArgument("Advance-by count must be non-zero"));
  }

  sql::Statement* statement = GetStatement();
  if (!statement) {
    return base::unexpected(Status::IOError("Database connection lost"));
  }

  if (!statement->Succeeded()) {
    // The statement was invalidated.
    BindParameters(*statement, /*target_key=*/{}, /*target_primary_key=*/{});
  }

  while (count--) {
    if (!statement->Step()) {
      if (!statement->Succeeded()) {
        return base::unexpected(db_->GetStatusOfLastOperation(PassKey()));
      }
      // End of range.
      InvalidateStatement(*statement);
      current_record_.reset();
      return false;
    }
  }
  Status s = UpdateRecord(*statement);
  if (!s.ok()) {
    return base::unexpected(s);
  }
  return true;
}

Status BackingStoreCursorImpl::TryResetToLastSavedPosition() {
  sql::Statement* statement = GetStatement();
  if (!statement) {
    return Status::IOError("Database connection lost");
  }
  InvalidateStatement(*statement);
  return Status::OK();
}

Status BackingStoreCursorImpl::UpdateRecord(sql::Statement& statement) {
  ASSIGN_OR_RETURN(current_record_, ReadRow(statement));
  return Status::OK();
}

sql::Statement* BackingStoreCursorImpl::GetStatement() {
  if (!db_) {
    return nullptr;
  }
  return db_->GetCursorStatement(PassKey(), statement_id_);
}

}  // namespace content::indexed_db::sqlite
