// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_SQLITE_RESULT_CODE_H_
#define SQL_SQLITE_RESULT_CODE_H_

#include <iosfwd>
#include <string>

#include "base/component_export.h"
#include "base/dcheck_is_on.h"

namespace sql {

// Strongly typed enumeration of all known SQLite result codes.
//
// The meaning of the codes is listed at https://www.sqlite.org/rescode.html
//
// Chrome's SQLite expose SqliteResultCode and SqliteErrorCode instead of plain
// ints. This isolates the use of sqlite3.h to the SQLite wrapper code itself.
//
// The forwarding declaration here is sufficient for most usage. The values are
// defined in sqlite_result_code_values.h.
enum class SqliteResultCode : int;

// Strongly typed enumeration of all known SQLite error codes.
//
// Error codes are a subset of all the result codes. Therefore, every
// SqliteErrorCode is a valid SqliteResultCode.
//
// The forwarding declaration here is sufficient for most usage. The values are
// defined in sqlite_result_code_values.h.
enum class SqliteErrorCode : int;

// SQLite result codes, mapped into a more compact form for UMA logging.
//
// SQLite's (extended) result codes cover a wide range of integer values, and
// are not suitable for direct use with our UMA logging infrastructure. This
// enum compresses the range by removing gaps and by mapping multiple SQLite
// result codes to the same value where appropriate.
//
// The forwarding declaration here is sufficient for most headers. The values
// are defined in sqlite_result_code_values.h.
enum class SqliteLoggedResultCode : int;

// Converts an int returned by SQLite into a strongly typed result code.
//
// This method DCHECKs that `sqlite_result_code` is a known SQLite result code.
#if DCHECK_IS_ON()
COMPONENT_EXPORT(SQL)
SqliteResultCode ToSqliteResultCode(int sqlite_result_code);
#else
inline SqliteResultCode ToSqliteResultCode(int sqlite_result_code) {
  return static_cast<SqliteResultCode>(sqlite_result_code);
}
#endif  // DCHECK_IS_ON()

// Converts a SqliteResultCode into a SqliteErrorCode.
//
// Callers should make sure that `sqlite_result_code` is indeed an error code,
// and does not indicate success. IsSqliteSuccessCode() could be used for this
// purpose.
#if DCHECK_IS_ON()
COMPONENT_EXPORT(SQL)
SqliteErrorCode ToSqliteErrorCode(SqliteResultCode sqlite_error_code);
#else
inline SqliteErrorCode ToSqliteErrorCode(SqliteResultCode sqlite_error_code) {
  return static_cast<SqliteErrorCode>(sqlite_error_code);
}
#endif  // DCHECK_IS_ON()

// Returns true if `sqlite_result_code` reports a successful operation.
//
// `sqlite_result_code` should only be passed to ToSqliteErrorCode() if this
// function returns false.
COMPONENT_EXPORT(SQL)
bool IsSqliteSuccessCode(SqliteResultCode sqlite_result_code);

// Helper for logging a SQLite result code to a UMA histogram.
//
// The histogram should be declared as enum="SqliteLoggedResultCode".
//
// Works for all result codes, including success codes and extended error codes.
// DCHECKs if provided result code should not occur in Chrome's usage of SQLite.
COMPONENT_EXPORT(SQL)
void UmaHistogramSqliteResult(const std::string& histogram_name,
                              int sqlite_result_code);

// Converts a SQLite result code into a UMA logging-friendly form.
//
// Works for all result codes, including success codes and extended error codes.
// DCHECKs if provided result code should not occur in Chrome's usage of SQLite.
//
// UmaHistogramSqliteResult() should be preferred for logging results to UMA.
COMPONENT_EXPORT(SQL)
SqliteLoggedResultCode ToSqliteLoggedResultCode(int sqlite_result_code);

// Logging support.
COMPONENT_EXPORT(SQL)
std::ostream& operator<<(std::ostream& os, SqliteResultCode sqlite_result_code);
COMPONENT_EXPORT(SQL)
std::ostream& operator<<(std::ostream& os, SqliteErrorCode sqlite_error_code);

// Called by unit tests.
//
// DCHECKs the representation invariants of the mapping table used to convert
// SQLite result codes to logging-friendly values.
COMPONENT_EXPORT(SQL) void CheckSqliteLoggedResultCodeForTesting();

}  // namespace sql

#endif  // SQL_SQLITE_RESULT_CODE_H_
