// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/database/database_util.h"
#include "base/strings/utf_string_conversions.h"
#include "storage/common/database/database_identifier.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace storage {

namespace {

void TestVfsFilePath(bool expected_result,
                     const char* vfs_file_name,
                     const char* expected_origin_identifier = "",
                     const char* expected_database_name = "",
                     const char* expected_sqlite_suffix = "") {
  std::string origin_identifier;
  std::u16string database_name;
  std::u16string sqlite_suffix;
  EXPECT_EQ(expected_result,
            DatabaseUtil::CrackVfsFileName(ASCIIToUTF16(vfs_file_name),
                                           &origin_identifier, &database_name,
                                           &sqlite_suffix));
  EXPECT_EQ(expected_origin_identifier, origin_identifier);
  EXPECT_EQ(ASCIIToUTF16(expected_database_name), database_name);
  EXPECT_EQ(ASCIIToUTF16(expected_sqlite_suffix), sqlite_suffix);
}

}  // namespace

// Test DatabaseUtil::CrackVfsFilePath on various inputs.
TEST(DatabaseUtilTest, CrackVfsFilePathTest) {
  TestVfsFilePath(true, "http_origin_0/#", "http_origin_0", "", "");
  TestVfsFilePath(true, "http_origin_0/#suffix", "http_origin_0", "", "suffix");
  TestVfsFilePath(true, "http_origin_0/db_name#", "http_origin_0", "db_name",
                  "");
  TestVfsFilePath(true, "http_origin_0/db_name#suffix", "http_origin_0",
                  "db_name", "suffix");
  TestVfsFilePath(false, "http_origin_0db_name#");
  TestVfsFilePath(false, "http_origin_0db_name#suffix");
  TestVfsFilePath(false, "http_origin_0/db_name");
  TestVfsFilePath(false, "http_origin_0#db_name/suffix");
  TestVfsFilePath(false, "/db_name#");
  TestVfsFilePath(false, "/db_name#suffix");
}

}  // namespace storage
