// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_line_reader.h"

#include <gtest/gtest.h>

#include "crazy_linker_system_mock.h"

namespace crazy {

static const char kFilePath[] = "/tmp/foo.txt";

static ::testing::AssertionResult AssertMemEqual(const char* expr1,
                                                 const char* expr2,
                                                 const char* expr3,
                                                 const char* expr4,
                                                 const char* expected_str,
                                                 size_t expected_len,
                                                 const char* arg_str,
                                                 size_t arg_len) {
  if (arg_len != expected_len) {
    return ::testing::AssertionFailure()
           << "Invalid value for '" << expr4 << "': " << arg_len
           << " (expecting '" << expr2 << "' which is " << expected_len << ")";
  }
  for (size_t n = 0; n < arg_len; ++n) {
    if (arg_str[n] != expected_str[n]) {
      return ::testing::AssertionFailure()
             << "Invalid value " << (unsigned)arg_str[n] << " at index " << n
             << "\nWhen comparing " << expr3 << " with expected " << expr1;
    }
  }
  return ::testing::AssertionSuccess();
}

#define EXPECT_MEMEQ(expected_str, expected_len, arg_str, arg_len)         \
  EXPECT_PRED_FORMAT4(AssertMemEqual, expected_str, expected_len, arg_str, \
                      arg_len)

TEST(LineReader, EmptyFile) {
  SystemMock sys;
  sys.AddRegularFile(kFilePath, "", 0);

  LineReader reader(kFilePath);
  EXPECT_FALSE(reader.GetNextLine());
}

TEST(LineReader, SingleLineFile) {
  SystemMock sys;
  static const char kFile[] = "foo bar\n";
  static const size_t kFileSize = sizeof(kFile) - 1;
  sys.AddRegularFile(kFilePath, kFile, kFileSize);

  LineReader reader(kFilePath);
  EXPECT_TRUE(reader.GetNextLine());
  EXPECT_EQ(kFileSize, reader.length());
  EXPECT_MEMEQ(kFile, kFileSize, reader.line(), reader.length());
  EXPECT_FALSE(reader.GetNextLine());
}

TEST(LineReader, SingleLineFileUnterminated) {
  SystemMock sys;
  static const char kFile[] = "foo bar";
  static const size_t kFileSize = sizeof(kFile) - 1;
  sys.AddRegularFile(kFilePath, kFile, kFileSize);

  LineReader reader(kFilePath);
  EXPECT_TRUE(reader.GetNextLine());
  // The LineReader will add a newline to the last line.
  EXPECT_EQ(kFileSize + 1, reader.length());
  EXPECT_MEMEQ(kFile, kFileSize, reader.line(), reader.length() - 1);
  EXPECT_EQ('\n', reader.line()[reader.length() - 1]);
  EXPECT_FALSE(reader.GetNextLine());
}

TEST(LineReader, MultiLineFile) {
  SystemMock sys;
  static const char kFile[] =
      "This is a multi\n"
      "line text file that to test the crazy::LineReader class implementation\n"
      "And this is a very long text line to check that the class properly "
      "handles them, through the help of dynamic allocation or something. "
      "Yadda yadda yadda yadda. No newline";
  static const size_t kFileSize = sizeof(kFile) - 1;
  sys.AddRegularFile(kFilePath, kFile, kFileSize);

  LineReader reader(kFilePath);

  EXPECT_TRUE(reader.GetNextLine());
  EXPECT_MEMEQ("This is a multi\n", 16, reader.line(), reader.length());

  EXPECT_TRUE(reader.GetNextLine());
  EXPECT_MEMEQ(
      "line text file that to test the crazy::LineReader class "
      "implementation\n",
      88 - 17,
      reader.line(),
      reader.length());

  EXPECT_TRUE(reader.GetNextLine());
  EXPECT_MEMEQ(
      "And this is a very long text line to check that the class properly "
      "handles them, through the help of dynamic allocation or something. "
      "Yadda yadda yadda yadda. No newline\n",
      187 - 17,
      reader.line(),
      reader.length());

  EXPECT_FALSE(reader.GetNextLine());
}

}  // namespace crazy
