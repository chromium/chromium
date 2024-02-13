// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_SCOPED_ERROR_EXPECTER_H_
#define SQL_TEST_SCOPED_ERROR_EXPECTER_H_

#include <set>

#include "sql/database.h"

// This is not strictly necessary for the operation of ScopedErrorExpecter, but
// the class is not useful without the SQLite error codes.
#include "third_party/sqlite/sqlite3.h"

namespace sql {
namespace test {

// sql::Database and sql::Statement treat most SQLite errors as fatal in debug
// mode.  The goal is to catch SQL errors before code is shipped to production.
// That fatal check makes it hard to write tests for error-handling code.  This
// scoper lists errors to expect and treat as non-fatal.  Errors are expected
// globally (on all connections).
//
// Since errors can be very context-dependent, the class is pedantic - specific
// errors must be expected, and every expected error must be seen.
//
// NOTE(shess): There are still fatal error cases this does not address.  If
// your test is handling database errors and you're hitting a case not handled,
// contact me.
class ScopedErrorExpecter {
 public:
  ScopedErrorExpecter();
  ScopedErrorExpecter(const ScopedErrorExpecter&) = delete;
  ScopedErrorExpecter& operator=(const ScopedErrorExpecter&) = delete;
  ~ScopedErrorExpecter();

  // Add an error to expect.  Extended error codes can be specified
  // individually, or the base code can be specified to expect errors for the
  // entire group (SQLITE_IOERR_* versus SQLITE_IOERR).
  void ExpectError(int err);
  void ExpectError(SqliteResultCode err);

  // Return |true| if the all of the expected errors were encountered.  Failure
  // to call this results in an EXPECT failure when the instance is destructed.
  [[nodiscard]] bool SawExpectedErrors();

 private:
  // The target of the callback passed to Database::SetErrorExpecter().  If
  // |err| matches an error passed to ExpectError(), records |err| and returns
  // |true|; this indicates that the enclosing test expected this error and the
  // caller should continue as it would in production.  Otherwise returns
  // |false| and adds a failure to the current test.
  bool ErrorSeen(int err);

  // Callback passed to Database::SetErrorExpecter().
  Database::ScopedErrorExpecterCallback callback_;

  // Record whether SawExpectedErrors() has been called.
  bool checked_;

  // Errors to expect.
  std::set<int> errors_expected_;

  // Expected errors which have been encountered.
  std::set<int> errors_seen_;
};

}  // namespace test
}  // namespace sql

#endif  // SQL_TEST_SCOPED_ERROR_EXPECTER_H_
