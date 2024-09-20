// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_STATEMENT_H_
#define SQL_STATEMENT_H_

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "sql/database.h"

namespace sql {

enum class SqliteResultCode : int;

// Possible return values from ColumnType in a statement. These should match
// the values in sqlite3.h.
enum class ColumnType {
  kInteger = 1,
  kFloat = 2,
  kText = 3,
  kBlob = 4,
  kNull = 5,
};

// Compiles and executes SQL statements.
//
// This class is not thread-safe. An instance must be accessed from a single
// sequence. This is enforced in DCHECK-enabled builds.
//
// Normal usage:
//   sql::Statement s(connection_.GetUniqueStatement(...));
//   s.BindInt(0, a);
//   if (s.Step())
//     return s.ColumnString(0);
//
//   If there are errors getting the statement, the statement will be inert; no
//   mutating or database-access methods will work. If you need to check for
//   validity, use:
//   if (!s.is_valid())
//     return false;
//
// Step() and Run() just return true to signal success. If you want to handle
// specific errors such as database corruption, install an error handler in
// in the connection object using set_error_delegate().
class COMPONENT_EXPORT(SQL) Statement {
 public:
  // Utility function that returns what //sql code encodes the 'time' value as
  // in a database when using BindTime
  static int64_t TimeToSqlValue(base::Time time);

  // Creates an uninitialized statement. The statement will be invalid until
  // you initialize it via Assign.
  Statement();

  explicit Statement(scoped_refptr<Database::StatementRef> ref);

  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  Statement(Statement&&) = delete;
  Statement& operator=(Statement&&) = delete;

  ~Statement();

  // Initializes this object with the given statement, which may or may not
  // be valid. Use is_valid() to check if it's OK.
  void Assign(scoped_refptr<Database::StatementRef> ref);

  // Resets the statement to an uninitialized state corresponding to
  // the default constructor, releasing the StatementRef.
  void Clear();

  // Returns true if the statement can be executed. All functions can still
  // be used if the statement is invalid, but they will return failure or some
  // default value. This is because the statement can become invalid in the
  // middle of executing a command if there is a serious error and the database
  // has to be reset.
  bool is_valid() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return ref_->is_valid();
  }

  // Running -------------------------------------------------------------------

  // Executes the statement, returning true on success. This is like Step but
  // for when there is no output, like an INSERT statement.
  bool Run();

  // Executes the statement, returning true if there is a row of data returned.
  // You can keep calling Step() until it returns false to iterate through all
  // the rows in your result set.
  //
  // When Step returns false, the result is either that there is no more data
  // or there is an error. This makes it most convenient for loop usage. If you
  // need to disambiguate these cases, use Succeeded().
  //
  // Typical example:
  //   while (s.Step()) {
  //     ...
  //   }
  //   return s.Succeeded();
  bool Step();

  // Resets the statement to its initial condition. This includes any current
  // result row, and also the bound variables if the |clear_bound_vars| is true.
  void Reset(bool clear_bound_vars);

  // Returns true if the last executed thing in this statement succeeded. If
  // there was no last executed thing or the statement is invalid, this will
  // return false.
  bool Succeeded() const;

  // Binding -------------------------------------------------------------------

  // These all take a 0-based parameter index and return true on success.
  // strings there may be out of memory.
  void BindNull(int param_index);
  void BindBool(int param_index, bool val);
  void BindInt(int param_index, int val);
  void BindInt(int param_index,
               int64_t val) = delete;  // Call BindInt64() instead.
  void BindInt64(int param_index, int64_t val);
  void BindDouble(int param_index, double val);
  void BindCString(int param_index, const char* val);
  void BindString(int param_index, std::string_view val);

  // If you need to store (potentially invalid) UTF-16 strings losslessly,
  // store them as BLOBs instead. `BindBlob()` has an overload for this purpose.
  void BindString16(int param_index, std::u16string_view value);
  void BindBlob(int param_index, base::span<const uint8_t> value);

  // Overload that makes it easy to pass in std::string values.
  void BindBlob(int param_index, base::span<const char> value) {
    BindBlob(param_index, base::as_bytes(base::make_span(value)));
  }

  // Overload that makes it easy to pass in std::u16string values.
  void BindBlob(int param_index, base::span<const char16_t> value) {
    BindBlob(param_index, base::as_bytes(base::make_span(value)));
  }

  // Conforms with base::Time serialization recommendations.
  //
  // This is equivalent to the following snippets, which should be replaced.
  // * BindInt64(col, val.ToInternalValue())
  // * BindInt64(col, val.ToDeltaSinceWindowsEpoch().InMicroseconds())
  //
  // Features that serialize base::Time in other ways, such as ToTimeT() or
  // InMillisecondsSinceUnixEpoch(), will require a database migration to be
  // converted to this (recommended) serialization method.
  //
  // TODO(crbug.com/40176243): Migrate all time serialization to this method,
  // and
  //                          then remove the migration details above.
  void BindTime(int param_index, base::Time time);

  // Conforms with base::TimeDelta serialization recommendations.
  //
  // This is equivalent to the following snippets, which should be replaced.
  // * BindInt64(col, delta.ToInternalValue())
  // * BindInt64(col, delta.InMicroseconds())
  //
  // TODO(crbug.com/40251269): Migrate all TimeDelta serialization to this
  // method
  //                          and remove the migration details above.
  void BindTimeDelta(int param_index, base::TimeDelta delta);

  // Retrieving ----------------------------------------------------------------

  // Returns the number of output columns in the result.
  int ColumnCount() const;

  // Returns the type associated with the given column.
  //
  // Watch out: the type may be undefined if you've done something to cause a
  // "type conversion." This means requesting the value of a column of a type
  // where that type is not the native type. For safety, call ColumnType only
  // on a column before getting the value out in any way.
  ColumnType GetColumnType(int col);

  // These all take a 0-based argument index.
  bool ColumnBool(int column_index);
  int ColumnInt(int column_index);
  int64_t ColumnInt64(int column_index);
  double ColumnDouble(int column_index);
  std::string ColumnString(int column_index);

  // If you need to store and retrieve (potentially invalid) UTF-16 strings
  // losslessly, store them as BLOBs instead. They may be retrieved with
  // `ColumnBlobAsString16()`.
  std::u16string ColumnString16(int column_index);

  // Returns a string view pointing to a buffer containing the string data.
  //
  // This can be used to avoid allocating a temporary string when the value is
  // immediately passed to a function accepting a string view. Otherwise, the
  // string view's contents should be copied to a caller-owned buffer
  // immediately. Any method call on the `Statement` may invalidate the string
  // view.
  //
  // The string view will be empty (and may have a null data) if the underlying
  // string is empty. Code that needs to distinguish between empty strings and
  // NULL should call `GetColumnType()` before calling `ColumnStringView()`.
  std::string_view ColumnStringView(int column_index);

  // Conforms with base::Time serialization recommendations.
  //
  // This is equivalent to the following snippets, which should be replaced.
  // * base::Time::FromInternalValue(ColumnInt64(col))
  // * base::Time::FromDeltaSinceWindowsEpoch(
  //       base::Microseconds(ColumnInt64(col)))
  //
  // TODO(crbug.com/40176243): Migrate all time serialization to this method,
  // and
  //                          then remove the migration details above.
  base::Time ColumnTime(int column_index);

  // Conforms with base::TimeDelta deserialization recommendations.
  //
  // This is equivalent to the following snippets, which should be replaced.
  // * base::TimeDelta::FromInternalValue(ColumnInt64(column_index))
  //
  // TODO(crbug.com/40251269): Migrate all TimeDelta serialization to this
  // method
  //                          and remove the migration details above.
  base::TimeDelta ColumnTimeDelta(int column_index);

  // Returns a span pointing to a buffer containing the blob data.
  //
  // The span's contents should be copied to a caller-owned buffer immediately.
  // Any method call on the Statement may invalidate the span.
  //
  // The span will be empty (and may have a null data) if the underlying blob is
  // empty. Code that needs to distinguish between empty blobs and NULL should
  // call GetColumnType() before calling ColumnBlob().
  base::span<const uint8_t> ColumnBlob(int column_index);

  bool ColumnBlobAsString(int column_index, std::string* result);
  bool ColumnBlobAsString16(int column_index, std::u16string* result);
  bool ColumnBlobAsVector(int column_index, std::vector<char>* result);
  bool ColumnBlobAsVector(int column_index, std::vector<uint8_t>* result);

  // Diagnostics --------------------------------------------------------------

  // Returns the original text of a SQL statement WITHOUT any bound values.
  // Intended for logging in case of failures. Note that DOES NOT return any
  // bound values, because that would cause a privacy / PII issue for logging.
  std::string GetSQLStatement();

 private:
  friend class Database;

  // Checks SQLite result codes and handles any errors.
  //
  // Returns `sqlite_result_code`. This gives callers the convenience of writing
  // "return CheckSqliteResultCode(sqlite_result_code)" and gives the compiler
  // the opportunity of doing tail call optimization (TCO) on the code above.
  //
  // This method reports error codes to the associated Database, and updates
  // internal state to reflect whether the statement succeeded or not.
  SqliteResultCode CheckSqliteResultCode(SqliteResultCode sqlite_result_code);

  // Should be called by all mutating methods to check that the statement is
  // valid. Returns true if the statement is valid. DCHECKS and returns false
  // if it is not.
  // The reason for this is to handle two specific cases in which a Statement
  // may be invalid. The first case is that the programmer made an SQL error.
  // Those cases need to be DCHECKed so that we are guaranteed to find them
  // before release. The second case is that the computer has an error (probably
  // out of disk space) which is prohibiting the correct operation of the
  // database. Our testing apparatus should not exhibit this defect, but release
  // situations may. Therefore, the code is handling disjoint situations in
  // release and test. In test, we're ensuring correct SQL. In release, we're
  // ensuring that contracts are honored in error edge cases.
  bool CheckValid() const;

  // Helper for Run() and Step(), calls sqlite3_step() and returns the checked
  // value from it.
  SqliteResultCode StepInternal();

  // Retrieve and log the count of VM steps required to execute the query.
  void ReportQueryExecutionMetrics() const;

  // The actual sqlite statement. This may be unique to us, or it may be cached
  // by the Database, which is why it's ref-counted. This pointer is
  // guaranteed non-null.
  scoped_refptr<Database::StatementRef> ref_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // See Succeeded() for what this holds.
  bool succeeded_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

#if DCHECK_IS_ON()
  // Used to DCHECK() that Bind*() is called before Step() or Run() are called.
  bool step_called_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool run_called_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
#endif  // DCHECK_IS_ON()

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace sql

#endif  // SQL_STATEMENT_H_
