// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/test/scoped_error_expecter.h"

#include "base/functional/bind.h"
#include "base/types/pass_key.h"
#include "sql/database.h"
#include "sql/sqlite_result_code_values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sql {
namespace test {

ScopedErrorExpecter::ScopedErrorExpecter()
    : checked_(false) {
  callback_ = base::BindRepeating(&ScopedErrorExpecter::ErrorSeen,
                                  base::Unretained(this));
  Database::SetScopedErrorExpecter(&callback_,
                                   base::PassKey<ScopedErrorExpecter>());
}

ScopedErrorExpecter::~ScopedErrorExpecter() {
  EXPECT_TRUE(checked_) << " Test must call SawExpectedErrors()";
  Database::ResetScopedErrorExpecter(base::PassKey<ScopedErrorExpecter>());
}

void ScopedErrorExpecter::ExpectError(int err) {
  EXPECT_EQ(0u, errors_expected_.count(err))
      << " Error " << err << " is already expected";
  errors_expected_.insert(err);
}

void ScopedErrorExpecter::ExpectError(SqliteResultCode err) {
  ExpectError(static_cast<int>(err));
}

bool ScopedErrorExpecter::SawExpectedErrors() {
  checked_ = true;
  return errors_expected_ == errors_seen_;
}

bool ScopedErrorExpecter::ErrorSeen(int err) {
  // Look for extended code.
  if (errors_expected_.count(err) > 0) {
    // Record that the error was seen.
    errors_seen_.insert(err);
    return true;
  }

  // Trim extended codes and check again.
  int base_err = err & 0xff;
  if (errors_expected_.count(base_err) > 0) {
    // Record that the error was seen.
    errors_seen_.insert(base_err);
    return true;
  }

  // Unexpected error.
  ADD_FAILURE() << " Unexpected SQLite error " << err;
  return false;
}

}  // namespace test
}  // namespace sql
