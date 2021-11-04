// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/data_driven_testing/data_driven_test.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace testing {
namespace {

// Reads |file| into |content|, and converts Windows line-endings to Unix ones.
// Returns true on success.
bool ReadFile(const base::FilePath& file, std::string* content) {
  if (!base::ReadFileToString(file, content))
    return false;

  base::ReplaceSubstringsAfterOffset(content, 0, "\r\n", "\n");
  return true;
}

// Write |content| to |file|. Returns true on success.
bool WriteFile(const base::FilePath& file, const std::string& content) {
  int write_size = base::WriteFile(file, content.c_str(),
                                   static_cast<int>(content.length()));
  return write_size == static_cast<int>(content.length());
}

// Removes lines starting with (optional) whitespace and a #.
void StripComments(std::string* content) {
  RE2::GlobalReplace(
      content,
      // Enable multi-line mode, ^ and $ match begin/end line in addition to
      // begin/end text.
      "(?m)"
      // Search for start of lines (^), ignore spaces (\\s*), and then look for
      // '#'.
      "^\\s*#"
      // Consume all characters (.*) until end of line ($).
      ".*$"
      // Consume the line wrapping so that the entire line is gone.
      "[\\r\\n]*",
      // Replace entire line with empty string.
      "");
}

}  // namespace

void DataDrivenTest::RunDataDrivenTest(
    const base::FilePath& input_directory,
    const base::FilePath& output_directory,
    const base::FilePath::StringType& file_name_pattern) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::DirectoryExists(input_directory));
  ASSERT_TRUE(base::DirectoryExists(output_directory));
  base::FileEnumerator input_files(
      input_directory, false, base::FileEnumerator::FILES, file_name_pattern);
  const bool kIsExpectedToPass = true;
  for (base::FilePath input_file = input_files.Next(); !input_file.empty();
       input_file = input_files.Next()) {
    RunOneDataDrivenTest(input_file, output_directory, kIsExpectedToPass);
  }
}

void DataDrivenTest::RunOneDataDrivenTest(
    const base::FilePath& test_file_name,
    const base::FilePath& output_directory,
    bool is_expected_to_pass) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // iOS doesn't get rid of removed test files. TODO(estade): remove this after
  // all iOS bots are clobbered.
  if (test_file_name.BaseName().value() == FILE_PATH_LITERAL("multimerge.in"))
    return;

  ASSERT_TRUE(base::DirectoryExists(output_directory));
  SCOPED_TRACE(test_file_name.BaseName().value());

  std::string input;
  ReadFile(test_file_name, &input);

  std::string output;
  {
    base::ScopedDisallowBlocking disallow_blocking;
    GenerateResults(input, &output);
  }

  base::FilePath output_file = output_directory.Append(
      test_file_name.BaseName().StripTrailingSeparators().ReplaceExtension(
          FILE_PATH_LITERAL(".out")));

  std::string output_file_contents;
  if (!ReadFile(output_file, &output_file_contents)) {
    ASSERT_TRUE(WriteFile(output_file, output));
    return;
  }
  // Remove comment lines (lead by '#' character).
  StripComments(&output_file_contents);

  if (is_expected_to_pass) {
    EXPECT_EQ(output_file_contents, output);
  } else {
    EXPECT_NE(output_file_contents, output);
  }
}

base::FilePath DataDrivenTest::GetInputDirectory() {
  return test_data_directory_.Append(feature_directory_)
      .Append(test_name_)
      .AppendASCII("input");
}

base::FilePath DataDrivenTest::GetOutputDirectory() {
  return test_data_directory_.Append(feature_directory_)
      .Append(test_name_)
      .AppendASCII("output");
}

DataDrivenTest::DataDrivenTest(
    const base::FilePath& test_data_directory,
    const base::FilePath::StringType& feature_directory,
    const base::FilePath::StringType& test_name)
    : test_data_directory_(test_data_directory),
      feature_directory_(feature_directory),
      test_name_(test_name) {}

DataDrivenTest::~DataDrivenTest() {}

}  // namespace testing
