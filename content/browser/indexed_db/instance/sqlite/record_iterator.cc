// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/record_iterator.h"

#include "base/check.h"
#include "base/types/expected.h"
#include "content/browser/indexed_db/instance/record.h"
#include "content/browser/indexed_db/status.h"
#include "sql/statement.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

// TODO(crbug.com/40253999): Remove after handling all error cases.
#define TRANSIENT_CHECK(condition) CHECK(condition)

namespace content::indexed_db::sqlite {

RecordIterator::RecordIterator() = default;
RecordIterator::~RecordIterator() = default;

StatusOr<std::unique_ptr<Record>> RecordIterator::Iterate(
    const blink::IndexedDBKey& key,
    const blink::IndexedDBKey& primary_key) {
  sql::Statement* statement = GetStatement();
  if (!statement) {
    return base::unexpected(Status::IOError("Database connection lost"));
  }

  statement->Reset(/*clear_bound_vars=*/false);
  BindParameters(*statement, key, primary_key, /*offset=*/0);
  if (!statement->Step()) {
    TRANSIENT_CHECK(statement->Succeeded());
    // End of range.
    return nullptr;
  }
  return ReadRow(*statement);
}

StatusOr<std::unique_ptr<Record>> RecordIterator::Iterate(uint32_t count) {
  TRANSIENT_CHECK(count > 0);

  sql::Statement* statement = GetStatement();
  if (!statement) {
    return base::unexpected(Status::IOError("Database connection lost"));
  }

  // TODO(crbug.com/419208481): Implement a fast path where `statement_` is
  // stepped without being reset when no record has changed in the range.
  statement->Reset(/*clear_bound_vars=*/false);

  // Iterate count times => offset by (i.e., skip) [count - 1] rows.
  BindParameters(*statement, /*target_key=*/{},
                 /*target_primary_key=*/{},
                 /*offset=*/count - 1);
  if (!statement->Step()) {
    TRANSIENT_CHECK(statement->Succeeded());
    // End of range.
    return nullptr;
  }

  return ReadRow(*statement);
}

}  // namespace content::indexed_db::sqlite
