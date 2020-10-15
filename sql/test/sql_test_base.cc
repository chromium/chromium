// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/test/sql_test_base.h"

#include "base/files/file_util.h"
#include "sql/test/test_helpers.h"

namespace sql {

SQLTestBase::SQLTestBase() = default;

SQLTestBase::SQLTestBase(sql::DatabaseOptions options) : db_(options) {}

SQLTestBase::~SQLTestBase() = default;

base::FilePath SQLTestBase::db_path() {
  return temp_dir_.GetPath().AppendASCII("SQLTest.db");
}

sql::Database& SQLTestBase::db() {
  return db_;
}

bool SQLTestBase::Reopen() {
  db_.Close();
  return db_.Open(db_path());
}

bool SQLTestBase::GetPathExists(const base::FilePath& path) {
  return base::PathExists(path);
}

bool SQLTestBase::CorruptSizeInHeaderOfDB() {
  return sql::test::CorruptSizeInHeader(db_path());
}

void SQLTestBase::WriteJunkToDatabase(WriteJunkType type) {
  base::ScopedFILE file(base::OpenFile(
      db_path(),
      type == TYPE_OVERWRITE_AND_TRUNCATE ? "wb" : "rb+"));
  ASSERT_TRUE(file.get());
  ASSERT_EQ(0, fseek(file.get(), 0, SEEK_SET));

  const char* kJunk = "Now is the winter of our discontent.";
  fputs(kJunk, file.get());
}

void SQLTestBase::TruncateDatabase() {
  base::ScopedFILE file(base::OpenFile(db_path(), "rb+"));
  ASSERT_TRUE(file);
  ASSERT_EQ(0, fseek(file.get(), 0, SEEK_SET));
  ASSERT_TRUE(base::TruncateFile(file.get()));
}

void SQLTestBase::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(db_.Open(db_path()));
}

void SQLTestBase::TearDown() {
  db_.Close();
}

}  // namespace sql
