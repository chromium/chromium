/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webdatabase/sqlite/sqlite_statement.h"

#include <memory>
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sql_log.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sql_value.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/sqlite/sqlite3.h"

// SQLite 3.6.16 makes sqlite3_prepare_v2 automatically retry preparing the
// statement once if the database scheme has changed. We rely on this behavior.
#if SQLITE_VERSION_NUMBER < 3006016
#error SQLite version 3.6.16 or newer is required
#endif

namespace {

// Only return error codes consistent with 3.7.6.3.
int restrictError(int error) {
  switch (error) {
    case SQLITE_IOERR_READ:
    case SQLITE_IOERR_SHORT_READ:
    case SQLITE_IOERR_WRITE:
    case SQLITE_IOERR_FSYNC:
    case SQLITE_IOERR_DIR_FSYNC:
    case SQLITE_IOERR_TRUNCATE:
    case SQLITE_IOERR_FSTAT:
    case SQLITE_IOERR_UNLOCK:
    case SQLITE_IOERR_RDLOCK:
    case SQLITE_IOERR_DELETE:
    case SQLITE_IOERR_BLOCKED:
    case SQLITE_IOERR_NOMEM:
    case SQLITE_IOERR_ACCESS:
    case SQLITE_IOERR_CHECKRESERVEDLOCK:
    case SQLITE_IOERR_LOCK:
    case SQLITE_IOERR_CLOSE:
    case SQLITE_IOERR_DIR_CLOSE:
    case SQLITE_IOERR_SHMOPEN:
    case SQLITE_IOERR_SHMSIZE:
    case SQLITE_IOERR_SHMLOCK:
    case SQLITE_LOCKED_SHAREDCACHE:
    case SQLITE_BUSY_RECOVERY:
    case SQLITE_CANTOPEN_NOTEMPDIR:
      return error;
    default:
      return (error & 0xff);
  }
}

}  // namespace

namespace blink {

SQLiteStatement::SQLiteStatement(SQLiteDatabase& db, const String& sql)
    : database_(db), query_(sql), statement_(nullptr) {}

SQLiteStatement::~SQLiteStatement() {
  Finalize();
}

int SQLiteStatement::Prepare() {
#if DCHECK_IS_ON()
  DCHECK(!is_prepared_);
#endif

  std::string query = query_.StripWhiteSpace().Utf8();

  // Need to pass non-stack |const char*| and |sqlite3_stmt*| to avoid race
  // with Oilpan stack scanning.
  std::unique_ptr<const char*> tail = std::make_unique<const char*>();
  std::unique_ptr<sqlite3_stmt*> statement = std::make_unique<sqlite3_stmt*>();
  *tail = nullptr;
  *statement = nullptr;
  int error;
  {
    SQL_DVLOG(1) << "SQL - prepare - " << query;

    // Pass the length of the string including the null character to
    // sqlite3_prepare_v3(); this lets SQLite avoid an extra string copy.
    wtf_size_t length_including_null_character =
        static_cast<wtf_size_t>(query.length()) + 1;

    error = sqlite3_prepare_v3(database_.Sqlite3Handle(), query.c_str(),
                               length_including_null_character,
                               /* prepFlags= */ 0, statement.get(), tail.get());
  }
  statement_ = *statement;

  if (error != SQLITE_OK) {
    SQL_DVLOG(1) << "sqlite3_prepare_v3 failed (" << error << ")\n"
                 << query << "\n"
                 << sqlite3_errmsg(database_.Sqlite3Handle());
  } else if (*tail && **tail) {
    error = SQLITE_ERROR;
  }

#if DCHECK_IS_ON()
  is_prepared_ = error == SQLITE_OK;
#endif
  return restrictError(error);
}

int SQLiteStatement::Step() {
  if (!statement_)
    return SQLITE_OK;

  // The database needs to update its last changes count before each statement
  // in order to compute properly the lastChanges() return value.
  database_.UpdateLastChangesCount();

  SQL_DVLOG(1) << "SQL - step - " << query_;
  int error = sqlite3_step(statement_);
  if (error != SQLITE_DONE && error != SQLITE_ROW) {
    SQL_DVLOG(1) << "sqlite3_step failed (" << error << " )\nQuery - " << query_
                 << "\nError - " << sqlite3_errmsg(database_.Sqlite3Handle());
  }

  return restrictError(error);
}

int SQLiteStatement::Finalize() {
#if DCHECK_IS_ON()
  is_prepared_ = false;
#endif
  if (!statement_)
    return SQLITE_OK;
  SQL_DVLOG(1) << "SQL - finalize - " << query_;
  int result = sqlite3_finalize(statement_);
  statement_ = nullptr;
  return restrictError(result);
}

bool SQLiteStatement::ExecuteCommand() {
  if (!statement_ && Prepare() != SQLITE_OK)
    return false;
#if DCHECK_IS_ON()
  DCHECK(is_prepared_);
#endif
  if (Step() != SQLITE_DONE) {
    Finalize();
    return false;
  }
  Finalize();
  return true;
}

int SQLiteStatement::BindText(int index, const String& text) {
#if DCHECK_IS_ON()
  DCHECK(is_prepared_);
#endif
  DCHECK_GT(index, 0);
  DCHECK_LE(static_cast<unsigned>(index), BindParameterCount());

  String text16(text);
  text16.Ensure16Bit();
  return restrictError(
      sqlite3_bind_text16(statement_, index, text16.Characters16(),
                          sizeof(UChar) * text16.length(), SQLITE_TRANSIENT));
}

int SQLiteStatement::BindDouble(int index, double number) {
#if DCHECK_IS_ON()
  DCHECK(is_prepared_);
#endif
  DCHECK_GT(index, 0);
  DCHECK_LE(static_cast<unsigned>(index), BindParameterCount());

  return restrictError(sqlite3_bind_double(statement_, index, number));
}

int SQLiteStatement::BindNull(int index) {
#if DCHECK_IS_ON()
  DCHECK(is_prepared_);
#endif
  DCHECK_GT(index, 0);
  DCHECK_LE(static_cast<unsigned>(index), BindParameterCount());

  return restrictError(sqlite3_bind_null(statement_, index));
}

int SQLiteStatement::BindValue(int index, const SQLValue& value) {
  switch (value.GetType()) {
    case SQLValue::kStringValue:
      return BindText(index, value.GetString());
    case SQLValue::kNumberValue:
      return BindDouble(index, value.Number());
    case SQLValue::kNullValue:
      return BindNull(index);
  }

  NOTREACHED();
  return SQLITE_ERROR;
}

unsigned SQLiteStatement::BindParameterCount() const {
#if DCHECK_IS_ON()
  DCHECK(is_prepared_);
#endif
  if (!statement_)
    return 0;
  return sqlite3_bind_parameter_count(statement_);
}

int SQLiteStatement::ColumnCount() {
#if DCHECK_IS_ON()
  DCHECK(is_prepared_);
#endif
  if (!statement_)
    return 0;
  return sqlite3_data_count(statement_);
}

String SQLiteStatement::GetColumnName(int col) {
  DCHECK_GE(col, 0);
  if (!statement_)
    if (PrepareAndStep() != SQLITE_ROW)
      return String();
  if (ColumnCount() <= col)
    return String();
  return String(
      reinterpret_cast<const UChar*>(sqlite3_column_name16(statement_, col)));
}

SQLValue SQLiteStatement::GetColumnValue(int col) {
  DCHECK_GE(col, 0);
  if (!statement_)
    if (PrepareAndStep() != SQLITE_ROW)
      return SQLValue();
  if (ColumnCount() <= col)
    return SQLValue();

  // SQLite is typed per value. optional column types are
  // "(mostly) ignored"
  switch (sqlite3_column_type(statement_, col)) {
    case SQLITE_INTEGER:  // SQLValue and JS don't represent integers, so use
                          // FLOAT -case
    case SQLITE_FLOAT:
      return SQLValue(sqlite3_column_double(statement_, col));
    case SQLITE_BLOB:  // SQLValue and JS don't represent blobs, so use TEXT
                       // -case
    case SQLITE_TEXT: {
      const UChar* string = reinterpret_cast<const UChar*>(
          sqlite3_column_text16(statement_, col));
      unsigned length = sqlite3_column_bytes16(statement_, col) / sizeof(UChar);
      return SQLValue(StringImpl::Create8BitIfPossible(string, length));
    }
    case SQLITE_NULL:
      return SQLValue();
  }
  NOTREACHED();
  return SQLValue();
}

String SQLiteStatement::GetColumnText(int col) {
  DCHECK_GE(col, 0);
  if (!statement_)
    if (PrepareAndStep() != SQLITE_ROW)
      return String();
  if (ColumnCount() <= col)
    return String();
  const UChar* string =
      reinterpret_cast<const UChar*>(sqlite3_column_text16(statement_, col));
  return StringImpl::Create8BitIfPossible(
      string, sqlite3_column_bytes16(statement_, col) / sizeof(UChar));
}

int SQLiteStatement::GetColumnInt(int col) {
  DCHECK_GE(col, 0);
  if (!statement_)
    if (PrepareAndStep() != SQLITE_ROW)
      return 0;
  if (ColumnCount() <= col)
    return 0;
  return sqlite3_column_int(statement_, col);
}

int64_t SQLiteStatement::GetColumnInt64(int col) {
  DCHECK_GE(col, 0);
  if (!statement_)
    if (PrepareAndStep() != SQLITE_ROW)
      return 0;
  if (ColumnCount() <= col)
    return 0;
  return sqlite3_column_int64(statement_, col);
}

}  // namespace blink
