/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webdatabase/sqlite/sqlite_transaction.h"

#include "third_party/blink/renderer/modules/webdatabase/sqlite/sqlite_database.h"

namespace blink {

SQLiteTransaction::SQLiteTransaction(SQLiteDatabase& db, bool read_only)
    : db_(db), in_progress_(false), read_only_(read_only) {}

SQLiteTransaction::~SQLiteTransaction() {
  if (in_progress_)
    Rollback();
}

void SQLiteTransaction::begin() {
  if (!in_progress_) {
    DCHECK(!db_->transaction_in_progress_);
    // Call BEGIN IMMEDIATE for a write transaction to acquire
    // a RESERVED lock on the DB file. Otherwise, another write
    // transaction (on another connection) could make changes
    // to the same DB file before this transaction gets to execute
    // any statements. If that happens, this transaction will fail.
    // http://www.sqlite.org/lang_transaction.html
    // http://www.sqlite.org/lockingv3.html#locking
    if (read_only_)
      in_progress_ = db_->ExecuteCommand("BEGIN");
    else
      in_progress_ = db_->ExecuteCommand("BEGIN IMMEDIATE");
    db_->transaction_in_progress_ = in_progress_;
  }
}

void SQLiteTransaction::Commit() {
  if (in_progress_) {
    DCHECK(db_->transaction_in_progress_);
    in_progress_ = !db_->ExecuteCommand("COMMIT");
    db_->transaction_in_progress_ = in_progress_;
  }
}

void SQLiteTransaction::Rollback() {
  // We do not use the 'm_inProgress = m_db.executeCommand("ROLLBACK")'
  // construct here, because m_inProgress should always be set to false after a
  // ROLLBACK, and m_db.executeCommand("ROLLBACK") can sometimes harmlessly
  // fail, thus returning a non-zero/true result
  // (http://www.sqlite.org/lang_transaction.html).
  if (in_progress_) {
    DCHECK(db_->transaction_in_progress_);
    db_->ExecuteCommand("ROLLBACK");
    in_progress_ = false;
    db_->transaction_in_progress_ = false;
  }
}

void SQLiteTransaction::Stop() {
  if (in_progress_) {
    in_progress_ = false;
    db_->transaction_in_progress_ = false;
  }
}

bool SQLiteTransaction::WasRolledBackBySqlite() const {
  // According to http://www.sqlite.org/c3ref/get_autocommit.html,
  // the auto-commit flag should be off in the middle of a transaction
  return in_progress_ && db_->IsAutoCommitOn();
}

}  // namespace blink
