// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_INTERNAL_API_TOKEN_H_
#define SQL_INTERNAL_API_TOKEN_H_

#include "base/gtest_prod_util.h"
#include "base/types/expected.h"

namespace base {
class FilePath;
}  // namespace base

namespace sql {

class Database;

namespace test {
struct ColumnInfo;
bool CorruptSizeInHeader(const base::FilePath&);
base::expected<int, int> GetUncheckpointedFrameCount(const Database& db);
}  // namespace test

// Restricts access to APIs internal to the //sql package.
//
// This implements Java's package-private via the passkey idiom.
class InternalApiToken {
 private:
  // Must NOT be =default to disallow creation by uniform initialization.
  InternalApiToken() {}
  InternalApiToken(const InternalApiToken&) = default;

  friend class Database;
  friend class Recovery;
  friend class Transaction;
  friend struct test::ColumnInfo;
  friend bool test::CorruptSizeInHeader(const base::FilePath&);
  friend base::expected<int, int> test::GetUncheckpointedFrameCount(
      const Database& db);

  FRIEND_TEST_ALL_PREFIXES(DatabaseDiskFullTest, SqliteFullAbortsTransactions);
  FRIEND_TEST_ALL_PREFIXES(DatabaseDiskFullTest,
                           CommitInTransactionAbortedByRawSqliteCalls);
  FRIEND_TEST_ALL_PREFIXES(DatabaseDiskFullTest,
                           RollbackInTransactionAbortedByRawSqliteCalls);
  FRIEND_TEST_ALL_PREFIXES(SQLiteFeaturesTest, WALNoClose);
};

}  // namespace sql

#endif  // SQL_INTERNAL_API_TOKEN_H_
