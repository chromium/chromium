// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/transaction.h"

#include "base/check.h"
#include "base/sequence_checker.h"
#include "sql/database.h"

namespace sql {

Transaction::Transaction(Database* database) : database_(*database) {
  DCHECK(database);
}

Transaction::~Transaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_active_)
    database_.RollbackTransaction();
}

bool Transaction::Begin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_active_) << "Beginning a transaction twice!";
  is_active_ = database_.BeginTransaction();
  return is_active_;
}

void Transaction::Rollback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_active_) << "Attempting to roll back a nonexistent transaction. "
                     << "Did you call Begin() and check its return?";
  is_active_ = false;
  database_.RollbackTransaction();
}

bool Transaction::Commit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_active_) << "Attempting to commit a nonexistent transaction. "
                     << "Did you call Begin() and check its return?";
  is_active_ = false;
  return database_.CommitTransaction();
}

}  // namespace sql
