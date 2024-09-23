// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TRANSACTION_H_
#define SQL_TRANSACTION_H_

#include "base/component_export.h"
#include "base/dcheck_is_on.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"

namespace sql {

class Database;

// Automatically rolls back uncommitted transactions when going out of scope.
//
// This class is not thread-safe. Each instance must be used from a single
// sequence.
class COMPONENT_EXPORT(SQL) Transaction {
 public:
  // Creates an inactive instance.
  //
  // `database` must be non-null and must outlive the newly created instance.
  //
  // The instance must be activated by calling Begin().
  //
  // sql::Database implements "virtual" nested transactions, as documented in
  // sql::Database::BeginTransaction(). This is a mis-feature, and should not be
  // used in new code. The sql::Database implementation does not match the
  // approach recommended at https://www.sqlite.org/lang_transaction.html.
  explicit Transaction(Database* database);
  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;
  Transaction(Transaction&&) = delete;
  Transaction& operator=(Transaction&&) = delete;
  ~Transaction();

  // Activates an inactive transaction. Must be called after construction.
  //
  // Returns false in case of failure. If this method fails, the database
  // connection will still execute SQL statements, but they will not be enclosed
  // in a transaction scope. In most cases, Begin() callers should handle
  // failures by abandoning the high-level operation that was meant to be
  // carried out in the transaction.
  //
  // In most cases (no nested transactions), this method issues a BEGIN
  // statement, which invokes SQLite's deferred transaction startup documented
  // in https://www.sqlite.org/lang_transaction.html. This means the database
  // lock is not acquired by the time Begin() completes. Instead, the first
  // statement after Begin() will attempt to acquire a read or write lock.
  //
  // This method is not idempotent. Calling Begin() twice on a Transaction will
  // cause a DCHECK crash.
  [[nodiscard]] bool Begin();

  // Explicitly rolls back the transaction. All changes will be forgotten.
  //
  // Most features can avoid calling this method, because Transactions that do
  // not get Commit()ed are automatically rolled back when they go out of scope.
  //
  // This method is not idempotent. Calling Rollback() twice on a Transaction
  // will cause a DCHECK crash.
  //
  // Must be called after a successful call to Begin(). Must not be called after
  // Commit().
  void Rollback();

  // Commits the transaction. All changes will be persisted in the database.
  //
  // Returns false in case of failure. The most common failure case is a SQLite
  // failure in committing the transaction. If sql::Database's support for
  // nested transactions is in use, this method will also fail if any nested
  // transaction has been rolled back.
  //
  // This method is not idempotent. Calling Commit() twice on a Transaction will
  // cause a DCHECK crash.
  //
  // Must be called after a successful call to Begin(). Must not be called after
  // Rollback().
  bool Commit();

  // True if Begin() succeeded, and neither Commit() nor Rollback() were called.
  bool IsActiveForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_active_;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<Database> database_ GUARDED_BY_CONTEXT(sequence_checker_);

#if DCHECK_IS_ON()
  bool begin_called_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool commit_called_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool rollback_called_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
#endif  // DCHECK_IS_ON()

  // True between a successful Begin() and a Commit() / Rollback() call.
  bool is_active_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
};

}  // namespace sql

#endif  // SQL_TRANSACTION_H_
