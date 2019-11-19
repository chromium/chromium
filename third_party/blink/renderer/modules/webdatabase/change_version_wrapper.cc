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
#include "third_party/blink/renderer/modules/webdatabase/change_version_wrapper.h"

#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_error.h"

namespace blink {

ChangeVersionWrapper::ChangeVersionWrapper(const String& old_version,
                                           const String& new_version)
    : old_version_(old_version.IsolatedCopy()),
      new_version_(new_version.IsolatedCopy()) {}

bool ChangeVersionWrapper::PerformPreflight(
    SQLTransactionBackend* transaction) {
  DCHECK(transaction);
  DCHECK(transaction->GetDatabase());

  Database* database = transaction->GetDatabase();

  String actual_version;
  if (!database->GetVersionFromDatabase(actual_version)) {
    int sqlite_error = database->SqliteDatabase().LastError();
    database->ReportSqliteError(sqlite_error);
    sql_error_ = SQLErrorData::Create(
        SQLError::kUnknownErr, "unable to read the current version",
        sqlite_error, database->SqliteDatabase().LastErrorMsg());
    return false;
  }

  if (actual_version != old_version_) {
    sql_error_ =
        std::make_unique<SQLErrorData>(SQLError::kVersionErr,
                                       "current version of the database and "
                                       "`oldVersion` argument do not match");
    return false;
  }

  return true;
}

bool ChangeVersionWrapper::PerformPostflight(
    SQLTransactionBackend* transaction) {
  DCHECK(transaction);
  DCHECK(transaction->GetDatabase());

  Database* database = transaction->GetDatabase();

  if (!database->SetVersionInDatabase(new_version_)) {
    int sqlite_error = database->SqliteDatabase().LastError();
    database->ReportSqliteError(sqlite_error);
    sql_error_ = SQLErrorData::Create(
        SQLError::kUnknownErr, "unable to set new version in database",
        sqlite_error, database->SqliteDatabase().LastErrorMsg());
    return false;
  }

  database->SetExpectedVersion(new_version_);

  return true;
}

void ChangeVersionWrapper::HandleCommitFailedAfterPostflight(
    SQLTransactionBackend* transaction) {
  transaction->GetDatabase()->SetCachedVersion(old_version_);
}

}  // namespace blink
