// Copyright 2011 The Chromium Authors
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
#if DCHECK_IS_ON()
  DCHECK(begin_called_)
      << "Begin() not called immediately after Transaction creation";
#endif  // DCHECK_IS_ON()

  if (is_active_)
    database_->RollbackTransaction();
}

bool Transaction::Begin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(!begin_called_) << __func__ << " already called";
  begin_called_ = true;
#endif  // DCHECK_IS_ON()

  DCHECK(!is_active_);
  is_active_ = database_->BeginTransaction();
  return is_active_;
}

void Transaction::Rollback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(begin_called_) << __func__ << " called before Begin()";
  DCHECK(!commit_called_) << __func__ << " called after Commit()";
  DCHECK(!rollback_called_) << __func__ << " called twice";
  rollback_called_ = true;
#endif  // DCHECK_IS_ON()

  DCHECK(is_active_) << __func__ << " called after Begin() failed";
  is_active_ = false;

  database_->RollbackTransaction();
}

bool Transaction::Commit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(begin_called_) << __func__ << " called before Begin()";
  DCHECK(!rollback_called_) << __func__ << " called after Rollback()";
  DCHECK(!commit_called_) << __func__ << " called after Commit()";
  commit_called_ = true;
#endif  // DCHECK_IS_ON()

  DCHECK(is_active_) << __func__ << " called after Begin() failed";
  is_active_ = false;
  return database_->CommitTransaction();
}

}  // namespace sql
