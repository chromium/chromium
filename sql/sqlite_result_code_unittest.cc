// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sqlite_result_code.h"

#include "base/dcheck_is_on.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "sql/sqlite_result_code_values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

namespace {

TEST(SqliteResultCodeTest, ToSqliteResultCode_Success) {
  EXPECT_EQ(SqliteResultCode::kOk, ToSqliteResultCode(SQLITE_OK));
  EXPECT_EQ(SqliteResultCode::kDone, ToSqliteResultCode(SQLITE_DONE));
  EXPECT_EQ(SqliteResultCode::kRow, ToSqliteResultCode(SQLITE_ROW));
}

TEST(SqliteResultCodeTest, ToSqliteResultCode_PrimaryErrorCodes) {
  EXPECT_EQ(SqliteResultCode::kIo, ToSqliteResultCode(SQLITE_IOERR));
  EXPECT_EQ(SqliteResultCode::kCorrupt, ToSqliteResultCode(SQLITE_CORRUPT));
  EXPECT_EQ(SqliteResultCode::kConstraint,
            ToSqliteResultCode(SQLITE_CONSTRAINT));
}

TEST(SqliteResultCodeTest, ToSqliteResultCode_ExtendedErrorCodes) {
  EXPECT_EQ(SqliteResultCode::kIoRead, ToSqliteResultCode(SQLITE_IOERR_READ));
  EXPECT_EQ(SqliteResultCode::kIoWrite, ToSqliteResultCode(SQLITE_IOERR_WRITE));
  EXPECT_EQ(SqliteResultCode::kCorruptIndex,
            ToSqliteResultCode(SQLITE_CORRUPT_INDEX));
  EXPECT_EQ(SqliteResultCode::kConstraintUnique,
            ToSqliteResultCode(SQLITE_CONSTRAINT_UNIQUE));
}

TEST(SqliteResultCodeTest, ToSqliteResultCode_MissingLowValue) {
  EXPECT_DCHECK_DEATH_WITH(ToSqliteResultCode(-65536),
                           "Unsupported SQLite result code: -65536");
}

TEST(SqliteResultCodeTest, ToSqliteResultCode_MissingHighValue) {
  EXPECT_DCHECK_DEATH_WITH(ToSqliteResultCode(65536),
                           "Unsupported SQLite result code: 65536");
}

TEST(SqliteResultCodeTest, ToSqliteResultCode_SqliteInternalError) {
#if DCHECK_IS_ON()
  EXPECT_DCHECK_DEATH_WITH(ToSqliteResultCode(SQLITE_INTERNAL),
                           "SQLite reported code marked for internal use: 2");
#else
  EXPECT_EQ(SqliteResultCode::kInternal, ToSqliteResultCode(SQLITE_INTERNAL));
#endif
}

TEST(SqliteResultCodeTest, ToSqliteErrorCode_Success_Ok) {
  EXPECT_DCHECK_DEATH_WITH(
      ToSqliteErrorCode(SqliteResultCode::kOk),
      "ToSqliteErrorCode called with non-error result code: 0");
}

TEST(SqliteResultCodeTest, ToSqliteErrorCode_Success_Done) {
  EXPECT_DCHECK_DEATH_WITH(
      ToSqliteErrorCode(SqliteResultCode::kDone),
      "ToSqliteErrorCode called with non-error result code: 101");
}

TEST(SqliteResultCodeTest, ToSqliteErrorCode_Success_Row) {
  EXPECT_DCHECK_DEATH_WITH(
      ToSqliteErrorCode(SqliteResultCode::kRow),
      "ToSqliteErrorCode called with non-error result code: 100");
}

TEST(SqliteResultCodeTest, ToSqliteErrorCode_PrimaryErrorCodes) {
  EXPECT_EQ(SqliteErrorCode::kIo, ToSqliteErrorCode(SqliteResultCode::kIo));
  EXPECT_EQ(SqliteErrorCode::kCorrupt,
            ToSqliteErrorCode(SqliteResultCode::kCorrupt));
  EXPECT_EQ(SqliteErrorCode::kConstraint,
            ToSqliteErrorCode(SqliteResultCode::kConstraint));
}

TEST(SqliteResultCodeTest, ToSqliteErrorCode_ExtendedErrorCodes) {
  EXPECT_EQ(SqliteErrorCode::kIoRead,
            ToSqliteErrorCode(SqliteResultCode::kIoRead));
  EXPECT_EQ(SqliteErrorCode::kIoWrite,
            ToSqliteErrorCode(SqliteResultCode::kIoWrite));
  EXPECT_EQ(SqliteErrorCode::kCorruptIndex,
            ToSqliteErrorCode(SqliteResultCode::kCorruptIndex));
  EXPECT_EQ(SqliteErrorCode::kConstraintUnique,
            ToSqliteErrorCode(SqliteResultCode::kConstraintUnique));
}

TEST(SqliteResultCodeTest, ToSqliteErrorCode_SqliteInternalError) {
#if DCHECK_IS_ON()
  EXPECT_DCHECK_DEATH_WITH(ToSqliteErrorCode(SqliteResultCode::kInternal),
                           "SQLite reported code marked for internal use: 2");
#else
  EXPECT_EQ(SqliteErrorCode::kInternal,
            ToSqliteErrorCode(SqliteResultCode::kInternal));
#endif
}

TEST(SqliteResultCodeTest, ToSqliteErrorCode_Success) {
  EXPECT_TRUE(IsSqliteSuccessCode(SqliteResultCode::kOk));
  EXPECT_TRUE(IsSqliteSuccessCode(SqliteResultCode::kDone));
  EXPECT_TRUE(IsSqliteSuccessCode(SqliteResultCode::kRow));
}

TEST(SqliteResultCodeTest, IsSqliteSuccessCode_PrimaryErrorCodes) {
  EXPECT_FALSE(IsSqliteSuccessCode(SqliteResultCode::kIo));
  EXPECT_FALSE(IsSqliteSuccessCode(SqliteResultCode::kCorrupt));
  EXPECT_FALSE(IsSqliteSuccessCode(SqliteResultCode::kConstraint));
}

TEST(SqliteResultCodeTest, IsSqliteSuccessCode_ExtendedErrorCodes) {
  EXPECT_FALSE(IsSqliteSuccessCode(SqliteResultCode::kIoRead));
  EXPECT_FALSE(IsSqliteSuccessCode(SqliteResultCode::kIoWrite));
  EXPECT_FALSE(IsSqliteSuccessCode(SqliteResultCode::kCorruptIndex));
  EXPECT_FALSE(IsSqliteSuccessCode(SqliteResultCode::kConstraintUnique));
}

TEST(SqliteResultCodeTest, IsSqliteSuccessCode_SqliteInternalError) {
#if DCHECK_IS_ON()
  EXPECT_DCHECK_DEATH_WITH(IsSqliteSuccessCode(SqliteResultCode::kInternal),
                           "SQLite reported code marked for internal use: 2");
#else
  EXPECT_FALSE(IsSqliteSuccessCode(SqliteResultCode::kInternal));
#endif
}

TEST(SqliteResultCodeTest, IsSqliteSuccessCode_ChromeBugError) {
#if DCHECK_IS_ON()
  EXPECT_DCHECK_DEATH_WITH(
      IsSqliteSuccessCode(SqliteResultCode::kNotFound),
      "SQLite reported code that should never show up in Chrome: 12");
#else
  EXPECT_FALSE(IsSqliteSuccessCode(SqliteResultCode::kNotFound));
#endif
}

TEST(SqliteResultCodeTest, ToSqliteLoggedResultCode_Success) {
  EXPECT_EQ(SqliteLoggedResultCode::kNoError,
            ToSqliteLoggedResultCode(SQLITE_OK));
  EXPECT_EQ(SqliteLoggedResultCode::kNoError,
            ToSqliteLoggedResultCode(SQLITE_DONE));
  EXPECT_EQ(SqliteLoggedResultCode::kNoError,
            ToSqliteLoggedResultCode(SQLITE_ROW));
}

TEST(SqliteResultCodeTest, ToSqliteLoggedResultCode_PrimaryErrorCodes) {
  EXPECT_EQ(SqliteLoggedResultCode::kIo,
            ToSqliteLoggedResultCode(SQLITE_IOERR));
  EXPECT_EQ(SqliteLoggedResultCode::kCorrupt,
            ToSqliteLoggedResultCode(SQLITE_CORRUPT));
  EXPECT_EQ(SqliteLoggedResultCode::kConstraint,
            ToSqliteLoggedResultCode(SQLITE_CONSTRAINT));
}

TEST(SqliteResultCodeTest, ToSqliteLoggedResultCode_ExtendedErrorCodes) {
  EXPECT_EQ(SqliteLoggedResultCode::kIoRead,
            ToSqliteLoggedResultCode(SQLITE_IOERR_READ));
  EXPECT_EQ(SqliteLoggedResultCode::kIoWrite,
            ToSqliteLoggedResultCode(SQLITE_IOERR_WRITE));
  EXPECT_EQ(SqliteLoggedResultCode::kCorruptIndex,
            ToSqliteLoggedResultCode(SQLITE_CORRUPT_INDEX));
  EXPECT_EQ(SqliteLoggedResultCode::kConstraintUnique,
            ToSqliteLoggedResultCode(SQLITE_CONSTRAINT_UNIQUE));
}

TEST(SqliteResultCodeTest, ToSqliteLoggedResultCode_MissingLowValue) {
  EXPECT_CHECK_DEATH_WITH(ToSqliteLoggedResultCode(-65536),
                          "Unsupported SQLite result code: -65536");
}

TEST(SqliteResultCodeTest, ToSqliteLoggedResultCode_MissingHighValue) {
  EXPECT_CHECK_DEATH_WITH(ToSqliteLoggedResultCode(65536),
                          "Unsupported SQLite result code: 65536");
}

TEST(SqliteResultCodeTest, ToSqliteLoggedResultCode_SqliteInternalError) {
#if DCHECK_IS_ON()
  EXPECT_DCHECK_DEATH_WITH(ToSqliteLoggedResultCode(SQLITE_INTERNAL),
                           "SQLite reported code marked for internal use: 2");
#else
  EXPECT_EQ(SqliteLoggedResultCode::kUnusedSqlite,
            ToSqliteLoggedResultCode(SQLITE_INTERNAL));
#endif
}

TEST(SqliteResultCodeTest, ToSqliteLoggedResultCode_ChromeBugError) {
#if DCHECK_IS_ON()
  EXPECT_DCHECK_DEATH_WITH(
      ToSqliteLoggedResultCode(SQLITE_NOTFOUND),
      "SQLite reported code that should never show up in Chrome: 12");
#else
  EXPECT_EQ(SqliteLoggedResultCode::kUnusedChrome,
            ToSqliteLoggedResultCode(SQLITE_NOTFOUND));
#endif
}

TEST(SqliteResultCodeTest, UmaHistogramSqliteResult_Success) {
  base::HistogramTester histogram_tester;
  UmaHistogramSqliteResult("Sql.ResultTest", SQLITE_OK);
  histogram_tester.ExpectTotalCount("Sql.ResultTest", 1);
  histogram_tester.ExpectBucketCount("Sql.ResultTest",
                                     SqliteLoggedResultCode::kNoError, 1);
}

TEST(SqliteResultCodeTest, UmaHistogramSqliteResult_PrimaryErrorCode) {
  base::HistogramTester histogram_tester;
  UmaHistogramSqliteResult("Sql.ResultTest", SQLITE_CORRUPT);
  histogram_tester.ExpectTotalCount("Sql.ResultTest", 1);
  histogram_tester.ExpectBucketCount("Sql.ResultTest",
                                     SqliteLoggedResultCode::kCorrupt, 1);
}

TEST(SqliteResultCodeTest, UmaHistogramSqliteResult_ExtendedErrorCode) {
  base::HistogramTester histogram_tester;
  UmaHistogramSqliteResult("Sql.ResultTest", SQLITE_CORRUPT_INDEX);
  histogram_tester.ExpectTotalCount("Sql.ResultTest", 1);
  histogram_tester.ExpectBucketCount("Sql.ResultTest",
                                     SqliteLoggedResultCode::kCorruptIndex, 1);
}

TEST(SqliteResultCodeTest, CheckMapping) {
  CheckSqliteLoggedResultCodeForTesting();
}

}  // namespace

}  // namespace sql
