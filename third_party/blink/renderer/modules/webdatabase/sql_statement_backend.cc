/*
 * Copyright (C) 2007, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webdatabase/sql_statement_backend.h"

#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_error.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_statement.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sqlite_database.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sqlite_statement.h"
#include "third_party/blink/renderer/modules/webdatabase/storage_log.h"

// The Life-Cycle of a SQLStatement i.e. Who's keeping the SQLStatement alive?
// ==========================================================================
// The RefPtr chain goes something like this:
//
//     At birth (in SQLTransactionBackend::executeSQL()):
//     =================================================
//     SQLTransactionBackend
//         // HeapDeque<Member<SQLStatementBackend>> m_statementQueue
//         // points to ...
//     --> SQLStatementBackend
//         // Member<SQLStatement> m_frontend points to ...
//     --> SQLStatement
//
//     After grabbing the statement for execution (in
//     SQLTransactionBackend::getNextStatement()):
//     ======================================================================
//     SQLTransactionBackend
//         // Member<SQLStatementBackend> m_currentStatementBackend
//         // points to ...
//     --> SQLStatementBackend
//         // Member<SQLStatement> m_frontend points to ...
//     --> SQLStatement
//
//     Then we execute the statement in
//     SQLTransactionBackend::runCurrentStatementAndGetNextState().
//     And we callback to the script in
//     SQLTransaction::deliverStatementCallback() if necessary.
//     - Inside SQLTransaction::deliverStatementCallback(), we operate on a raw
//       SQLStatement*.  This pointer is valid because it is owned by
//       SQLTransactionBackend's
//       SQLTransactionBackend::m_currentStatementBackend.
//
//     After we're done executing the statement (in
//     SQLTransactionBackend::getNextStatement()):
//     ======================================================================
//     When we're done executing, we'll grab the next statement. But before we
//     do that, getNextStatement() nullify
//     SQLTransactionBackend::m_currentStatementBackend.
//     This will trigger the deletion of the SQLStatementBackend and
//     SQLStatement.
//
//     Note: unlike with SQLTransaction, there is no JS representation of
//     SQLStatement.  Hence, there is no GC dependency at play here.

namespace blink {

SQLStatementBackend::SQLStatementBackend(SQLStatement* frontend,
                                         const String& statement,
                                         const Vector<SQLValue>& arguments,
                                         int permissions)
    : frontend_(frontend),
      statement_(statement.IsolatedCopy()),
      arguments_(arguments),
      has_callback_(frontend_->HasCallback()),
      has_error_callback_(frontend_->HasErrorCallback()),
      result_set_(MakeGarbageCollected<SQLResultSet>()),
      permissions_(permissions) {
  DCHECK(IsMainThread());

  frontend_->SetBackend(this);
}

void SQLStatementBackend::Trace(blink::Visitor* visitor) {
  visitor->Trace(frontend_);
  visitor->Trace(result_set_);
}

SQLStatement* SQLStatementBackend::GetFrontend() {
  return frontend_.Get();
}

SQLErrorData* SQLStatementBackend::SqlError() const {
  return error_.get();
}

SQLResultSet* SQLStatementBackend::SqlResultSet() const {
  return result_set_->IsValid() ? result_set_.Get() : nullptr;
}

bool SQLStatementBackend::Execute(Database* db) {
  DCHECK(!result_set_->IsValid());

  // If we're re-running this statement after a quota violation, we need to
  // clear that error now
  ClearFailureDueToQuota();

  // This transaction might have been marked bad while it was being set up on
  // the main thread, so if there is still an error, return false.
  if (error_)
    return false;

  db->SetAuthorizerPermissions(permissions_);

  SQLiteDatabase* database = &db->SqliteDatabase();

  SQLiteStatement statement(*database, statement_);
  int result = statement.Prepare();

  if (result != kSQLResultOk) {
    STORAGE_DVLOG(1) << "Unable to verify correctness of statement "
                     << statement_ << " - error " << result << " ("
                     << database->LastErrorMsg() << ")";
    if (result == kSQLResultInterrupt) {
      error_ = SQLErrorData::Create(SQLError::kDatabaseErr,
                                    "could not prepare statement", result,
                                    "interrupted");
    } else {
      error_ = SQLErrorData::Create(SQLError::kSyntaxErr,
                                    "could not prepare statement", result,
                                    database->LastErrorMsg());
    }
    db->ReportSqliteError(result);
    return false;
  }

  // FIXME: If the statement uses the ?### syntax supported by sqlite, the bind
  // parameter count is very likely off from the number of question marks.  If
  // this is the case, they might be trying to do something fishy or malicious
  if (statement.BindParameterCount() != arguments_.size()) {
    STORAGE_DVLOG(1)
        << "Bind parameter count doesn't match number of question marks";
    error_ = std::make_unique<SQLErrorData>(
        SQLError::kSyntaxErr,
        "number of '?'s in statement string does not match argument count");
    return false;
  }

  for (unsigned i = 0; i < arguments_.size(); ++i) {
    result = statement.BindValue(i + 1, arguments_[i]);
    if (result == kSQLResultFull) {
      SetFailureDueToQuota(db);
      return false;
    }

    if (result != kSQLResultOk) {
      STORAGE_DVLOG(1) << "Failed to bind value index " << (i + 1)
                       << " to statement for query " << statement_;
      db->ReportSqliteError(result);
      error_ =
          SQLErrorData::Create(SQLError::kDatabaseErr, "could not bind value",
                               result, database->LastErrorMsg());
      return false;
    }
  }

  // Step so we can fetch the column names.
  result = statement.Step();
  if (result == kSQLResultRow) {
    int column_count = statement.ColumnCount();
    SQLResultSetRowList* rows = result_set_->rows();

    for (int i = 0; i < column_count; i++)
      rows->AddColumn(statement.GetColumnName(i));

    do {
      for (int i = 0; i < column_count; i++)
        rows->AddResult(statement.GetColumnValue(i));

      result = statement.Step();
    } while (result == kSQLResultRow);

    if (result != kSQLResultDone) {
      db->ReportSqliteError(result);
      error_ = SQLErrorData::Create(SQLError::kDatabaseErr,
                                    "could not iterate results", result,
                                    database->LastErrorMsg());
      return false;
    }
  } else if (result == kSQLResultDone) {
    // Didn't find anything, or was an insert
    if (db->LastActionWasInsert())
      result_set_->SetInsertId(database->LastInsertRowID());
  } else if (result == kSQLResultFull) {
    // Return the Quota error - the delegate will be asked for more space and
    // this statement might be re-run.
    SetFailureDueToQuota(db);
    return false;
  } else if (result == kSQLResultConstraint) {
    db->ReportSqliteError(result);
    error_ = SQLErrorData::Create(
        SQLError::kConstraintErr,
        "could not execute statement due to a constraint failure", result,
        database->LastErrorMsg());
    return false;
  } else {
    db->ReportSqliteError(result);
    error_ = SQLErrorData::Create(SQLError::kDatabaseErr,
                                  "could not execute statement", result,
                                  database->LastErrorMsg());
    return false;
  }

  // FIXME: If the spec allows triggers, and we want to be "accurate" in a
  // different way, we'd use sqlite3_total_changes() here instead of
  // sqlite3_changed, because that includes rows modified from within a trigger.
  // For now, this seems sufficient.
  result_set_->SetRowsAffected(database->LastChanges());

  return true;
}

void SQLStatementBackend::SetVersionMismatchedError(Database* database) {
  DCHECK(!error_);
  DCHECK(!result_set_->IsValid());
  error_ = std::make_unique<SQLErrorData>(
      SQLError::kVersionErr,
      "current version of the database and `oldVersion` argument do not match");
}

void SQLStatementBackend::SetFailureDueToQuota(Database* database) {
  DCHECK(!error_);
  DCHECK(!result_set_->IsValid());
  error_ = std::make_unique<SQLErrorData>(
      SQLError::kQuotaErr,
      "there was not enough remaining storage "
      "space, or the storage quota was reached and "
      "the user declined to allow more space");
}

void SQLStatementBackend::ClearFailureDueToQuota() {
  if (LastExecutionFailedDueToQuota())
    error_ = nullptr;
}

bool SQLStatementBackend::LastExecutionFailedDueToQuota() const {
  return error_ && error_->Code() == SQLError::kQuotaErr;
}

}  // namespace blink
