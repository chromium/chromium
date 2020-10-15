// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_SQL_TEST_BASE_H_
#define SQL_TEST_SQL_TEST_BASE_H_

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sql {

// Base class for SQL tests.
//
// WARNING: We want to run the same gtest based unit test code both against
// chromium (which uses this implementation here), and the mojo code (which
// uses a different class named SQLTestBase). These two classes need to have
// the same interface because we compile time switch them based on a
// #define. We need to have two different implementations because the mojo
// version derives from mojo::test::ApplicationTestBase instead of
// testing::Test.
class SQLTestBase : public testing::Test {
 public:
  SQLTestBase();
  explicit SQLTestBase(sql::DatabaseOptions options);
  ~SQLTestBase() override;

  enum WriteJunkType {
    TYPE_OVERWRITE_AND_TRUNCATE,
    TYPE_OVERWRITE
  };

  // Returns the path to the database.
  base::FilePath db_path();

  // Returns a connection to the database at db_path().
  sql::Database& db();

  // Closes the current connection to the database and reopens it.
  bool Reopen();

  // Proxying method around base::PathExists.
  bool GetPathExists(const base::FilePath& path);

  // SQLite stores the database size in the header, and if the actual
  // OS-derived size is smaller, the database is considered corrupt.
  // [This case is actually a common form of corruption in the wild.]
  // This helper sets the in-header size to one page larger than the
  // actual file size.  The resulting file will return SQLITE_CORRUPT
  // for most operations unless PRAGMA writable_schema is turned ON.
  //
  // Returns false if any error occurs accessing the file.
  bool CorruptSizeInHeaderOfDB();

  // Writes junk to the start of the file.
  void WriteJunkToDatabase(WriteJunkType type);

  // Sets the database file size to 0.
  void TruncateDatabase();

  // Overridden from testing::Test:
  void SetUp() override;
  void TearDown() override;

 private:
  base::ScopedTempDir temp_dir_;
  sql::Database db_;

  DISALLOW_COPY_AND_ASSIGN(SQLTestBase);
};

}  // namespace sql

#endif  // SQL_TEST_SQL_TEST_BASE_H_
