// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_STATEMENT_H_
#define SQL_STATEMENT_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "sql/database.h"

namespace sql {

// Possible return values from ColumnType in a statement. These should match
// the values in sqlite3.h.
enum class ColumnType {
  kInteger = 1,
  kFloat = 2,
  kText = 3,
  kBlob = 4,
  kNull = 5,
};

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
  // Creates an uninitialized statement. The statement will be invalid until
  // you initialize it via Assign.
  Statement();

  explicit Statement(scoped_refptr<Database::StatementRef> ref);
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
  bool is_valid() const { return ref_->is_valid(); }

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

  // These all take a 0-based argument index and return true on success. You
  // may not always care about the return value (they'll DCHECK if they fail).
  // The main thing you may want to check is when binding large blobs or
  // strings there may be out of memory.
  bool BindNull(int col);
  bool BindBool(int col, bool val);
  bool BindInt(int col, int val);
  bool BindInt64(int col, int64_t val);
  bool BindDouble(int col, double val);
  bool BindCString(int col, const char* val);
  bool BindString(int col, const std::string& val);
  bool BindString16(int col, const base::string16& value);
  bool BindBlob(int col, const void* value, int value_len);

  // Retrieving ----------------------------------------------------------------

  // Returns the number of output columns in the result.
  int ColumnCount() const;

  // Returns the type associated with the given column.
  //
  // Watch out: the type may be undefined if you've done something to cause a
  // "type conversion." This means requesting the value of a column of a type
  // where that type is not the native type. For safety, call ColumnType only
  // on a column before getting the value out in any way.
  ColumnType GetColumnType(int col) const;

  // These all take a 0-based argument index.
  bool ColumnBool(int col) const;
  int ColumnInt(int col) const;
  int64_t ColumnInt64(int col) const;
  double ColumnDouble(int col) const;
  std::string ColumnString(int col) const;
  base::string16 ColumnString16(int col) const;

  // When reading a blob, you can get a raw pointer to the underlying data,
  // along with the length, or you can just ask us to copy the blob into a
  // vector. Danger! ColumnBlob may return nullptr if there is no data!
  int ColumnByteLength(int col) const;
  const void* ColumnBlob(int col) const;
  bool ColumnBlobAsString(int col, std::string* blob) const;
  bool ColumnBlobAsString16(int col, base::string16* val) const;
  bool ColumnBlobAsVector(int col, std::vector<char>* val) const;
  bool ColumnBlobAsVector(int col, std::vector<unsigned char>* val) const;

  // Diagnostics --------------------------------------------------------------

  // Returns the original text of sql statement. Do not keep a pointer to it.
  const char* GetSQLStatement();

 private:
  friend class Database;

  // This is intended to check for serious errors and report them to the
  // Database object. It takes a sqlite error code, and returns the same
  // code. Currently this function just updates the succeeded flag, but will be
  // enhanced in the future to do the notification.
  int CheckError(int err);

  // Contraction for checking an error code against SQLITE_OK. Does not set the
  // succeeded flag.
  bool CheckOk(int err) const;

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
  int StepInternal();

  // The actual sqlite statement. This may be unique to us, or it may be cached
  // by the Database, which is why it's ref-counted. This pointer is
  // guaranteed non-null.
  scoped_refptr<Database::StatementRef> ref_;

  // Set after Step() or Run() are called, reset by Reset().  Used to
  // prevent accidental calls to API functions which would not work
  // correctly after stepping has started.
  bool stepped_;

  // See Succeeded() for what this holds.
  bool succeeded_;

  DISALLOW_COPY_AND_ASSIGN(Statement);
};

}  // namespace sql

#endif  // SQL_STATEMENT_H_
