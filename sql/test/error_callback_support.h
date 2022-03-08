// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_ERROR_CALLBACK_SUPPORT_H_
#define SQL_TEST_ERROR_CALLBACK_SUPPORT_H_

#include "base/memory/raw_ptr.h"
#include "sql/database.h"

namespace sql {

class Statement;

// Helper to capture any errors into a local variable for testing.
// For instance:
//   int error = SQLITE_OK;
//   sql::ScopedErrorCallback scoped_error_callback(
//       db, base::BindRepeating(&sql::CaptureErrorCallback, &error));
//   // Provoke SQLITE_CONSTRAINT on db.
//   EXPECT_EQ(SQLITE_CONSTRAINT, error);
void CaptureErrorCallback(int* error_pointer, int error, Statement* stmt);

// Helper to set db's error callback and then reset it when it goes
// out of scope.
class ScopedErrorCallback {
 public:
  ScopedErrorCallback(Database* db, Database::ErrorCallback callback);
  ScopedErrorCallback(const ScopedErrorCallback&) = delete;
  ScopedErrorCallback& operator=(const ScopedErrorCallback&) = delete;
  ~ScopedErrorCallback();

 private:
  raw_ptr<sql::Database> db_;
};

}  // namespace sql

#endif  // SQL_TEST_ERROR_CALLBACK_SUPPORT_H_
