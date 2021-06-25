// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TRANSACTION_H_
#define SQL_TRANSACTION_H_

#include "base/component_export.h"
#include "base/macros.h"

namespace sql {

class Database;

class COMPONENT_EXPORT(SQL) Transaction {
 public:
  // Creates the scoped transaction object. You MUST call Begin() to begin the
  // transaction. If you have begun a transaction and not committed it, the
  // constructor will roll back the transaction. If you want to commit, you
  // need to manually call Commit before this goes out of scope.
  //
  // Nested transactions are supported. See sql::Database::BeginTransaction
  // for details.
  explicit Transaction(Database* connection);
  ~Transaction();

  // Returns true when there is a transaction that has been successfully begun.
  bool is_open() const { return is_open_; }

  // Begins the transaction. This uses the default sqlite "deferred" transaction
  // type, which means that the DB lock is lazily acquired the next time the
  // database is accessed, not in the begin transaction command.
  //
  // Returns false on failure. Note that if this fails, you shouldn't do
  // anything you expect to be actually transactional, because it won't be!
  bool Begin();

  // Rolls back the transaction. This will happen automatically if you do
  // nothing when the transaction goes out of scope.
  void Rollback();

  // Commits the transaction, returning true on success. This will return
  // false if sqlite could not commit it, or if another transaction in the
  // same outermost transaction has been rolled back (which necessitates a
  // rollback of all transactions in that outermost one).
  bool Commit();

 private:
  Database* database_;

  // True when the transaction is open, false when it's already been committed
  // or rolled back.
  bool is_open_ = false;

  DISALLOW_COPY_AND_ASSIGN(Transaction);
};

}  // namespace sql

#endif  // SQL_TRANSACTION_H_
