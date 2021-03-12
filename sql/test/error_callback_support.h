// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_ERROR_CALLBACK_SUPPORT_H_
#define SQL_TEST_ERROR_CALLBACK_SUPPORT_H_

#include "base/macros.h"
#include "sql/database.h"

namespace sql {

// Helper to capture any errors into a local variable for testing.
// For instance:
//   int error = SQLITE_OK;
//   ScopedErrorCallback sec(db, base::BindRepeating(&CaptureErrorCallback,
//                                                   &error));
//   // Provoke SQLITE_CONSTRAINT on db.
//   EXPECT_EQ(SQLITE_CONSTRAINT, error);
void CaptureErrorCallback(int* error_pointer, int error, sql::Statement* stmt);

// Helper to set db's error callback and then reset it when it goes
// out of scope.
class ScopedErrorCallback {
 public:
  ScopedErrorCallback(sql::Database* db,
                      const sql::Database::ErrorCallback& cb);
  ~ScopedErrorCallback();

 private:
  sql::Database* db_;

  DISALLOW_COPY_AND_ASSIGN(ScopedErrorCallback);
};

}  // namespace sql

#endif  // SQL_TEST_ERROR_CALLBACK_SUPPORT_H_
