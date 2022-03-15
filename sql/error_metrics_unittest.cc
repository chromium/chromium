// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/error_metrics.h"

#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

namespace {

TEST(ErrorMetricsTest, CreateSqliteLoggedResultCode_Success) {
  EXPECT_EQ(SqliteLoggedResultCode::kNoError,
            CreateSqliteLoggedResultCode(SQLITE_OK));
  EXPECT_EQ(SqliteLoggedResultCode::kNoError,
            CreateSqliteLoggedResultCode(SQLITE_DONE));
  EXPECT_EQ(SqliteLoggedResultCode::kNoError,
            CreateSqliteLoggedResultCode(SQLITE_ROW));
}

TEST(ErrorMetricsTest, CreateSqliteLoggedResultCode_PrimaryErrorCodes) {
  EXPECT_EQ(SqliteLoggedResultCode::kIo,
            CreateSqliteLoggedResultCode(SQLITE_IOERR));
  EXPECT_EQ(SqliteLoggedResultCode::kCorrupt,
            CreateSqliteLoggedResultCode(SQLITE_CORRUPT));
  EXPECT_EQ(SqliteLoggedResultCode::kConstraint,
            CreateSqliteLoggedResultCode(SQLITE_CONSTRAINT));
}

TEST(ErrorMetricsTest, CreateSqliteLoggedResultCode_ExtendedErrorCodes) {
  EXPECT_EQ(SqliteLoggedResultCode::kIoRead,
            CreateSqliteLoggedResultCode(SQLITE_IOERR_READ));
  EXPECT_EQ(SqliteLoggedResultCode::kIoWrite,
            CreateSqliteLoggedResultCode(SQLITE_IOERR_WRITE));
  EXPECT_EQ(SqliteLoggedResultCode::kCorruptIndex,
            CreateSqliteLoggedResultCode(SQLITE_CORRUPT_INDEX));
  EXPECT_EQ(SqliteLoggedResultCode::kConstraintUnique,
            CreateSqliteLoggedResultCode(SQLITE_CONSTRAINT_UNIQUE));
}

TEST(ErrorMetricsTest, CreateSqliteLoggedResultCode_MissingLowValue) {
  EXPECT_DCHECK_DEATH_WITH(CreateSqliteLoggedResultCode(-65536),
                           "Unsupported SQLite result code: -65536");
}

TEST(ErrorMetricsTest, CreateSqliteLoggedResultCode_MissingHighValue) {
  EXPECT_DCHECK_DEATH_WITH(CreateSqliteLoggedResultCode(65536),
                           "Unsupported SQLite result code: 65536");
}

TEST(ErrorMetricsTest, CreateSqliteLoggedResultCode_SqliteInternalError) {
#if DCHECK_IS_ON()
  EXPECT_DCHECK_DEATH_WITH(CreateSqliteLoggedResultCode(SQLITE_INTERNAL),
                           "SQLite reported code marked for internal use: 2");
#else
  EXPECT_EQ(SqliteLoggedResultCode::kUnusedSqlite,
            CreateSqliteLoggedResultCode(SQLITE_INTERNAL));
#endif
}

TEST(ErrorMetricsTest, CreateSqliteLoggedResultCode_ChromeBugError) {
#if DCHECK_IS_ON()
  EXPECT_DCHECK_DEATH_WITH(
      CreateSqliteLoggedResultCode(SQLITE_NOTFOUND),
      "SQLite reported code that should never show up in Chrome: 12");
#else
  EXPECT_EQ(SqliteLoggedResultCode::kUnusedChrome,
            CreateSqliteLoggedResultCode(SQLITE_NOTFOUND));
#endif
}

TEST(ErrorMetricsTest, UmaHistogramSqliteResult_Success) {
  base::HistogramTester histogram_tester;
  UmaHistogramSqliteResult("Sql.ResultTest", SQLITE_OK);
  histogram_tester.ExpectTotalCount("Sql.ResultTest", 1);
  histogram_tester.ExpectBucketCount("Sql.ResultTest",
                                     SqliteLoggedResultCode::kNoError, 1);
}

TEST(ErrorMetricsTest, UmaHistogramSqliteResult_PrimaryErrorCode) {
  base::HistogramTester histogram_tester;
  UmaHistogramSqliteResult("Sql.ResultTest", SQLITE_CORRUPT);
  histogram_tester.ExpectTotalCount("Sql.ResultTest", 1);
  histogram_tester.ExpectBucketCount("Sql.ResultTest",
                                     SqliteLoggedResultCode::kCorrupt, 1);
}

TEST(ErrorMetricsTest, UmaHistogramSqliteResult_ExtendedErrorCode) {
  base::HistogramTester histogram_tester;
  UmaHistogramSqliteResult("Sql.ResultTest", SQLITE_CORRUPT_INDEX);
  histogram_tester.ExpectTotalCount("Sql.ResultTest", 1);
  histogram_tester.ExpectBucketCount("Sql.ResultTest",
                                     SqliteLoggedResultCode::kCorruptIndex, 1);
}

TEST(ErrorMetricsTest, CheckMapping) {
  CheckSqliteLoggedResultCodeForTesting();
}

}  // namespace

}  // namespace sql
