// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_DUMP_ACCESSIBILITY_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_DUMP_ACCESSIBILITY_TEST_HELPER_H_

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/optional.h"

namespace base {
class FilePath;
}

namespace content {

// A helper class for writing accessibility tree dump tests.
class DumpAccessibilityTestHelper {
 public:
  explicit DumpAccessibilityTestHelper(const char* expectation_type);
  ~DumpAccessibilityTestHelper() = default;

  // Returns a path to an expectation file for the current platform. If no
  // suitable expectation file can be found, logs an error message and returns
  // an empty path.
  base::FilePath GetExpectationFilePath(const base::FilePath& test_file_path);

  // Loads the given expectation file and returns the contents. An expectation
  // file may be empty, in which case an empty vector is returned.
  // Returns nullopt if the file contains a skip marker.
  static base::Optional<std::vector<std::string>> LoadExpectationFile(
      const base::FilePath& expected_file);

  // Compares the given actual dump against the given expectation and generates
  // a new expectation file if switches::kGenerateAccessibilityTestExpectations
  // has been set. Returns true if the result matches the expectation.
  static bool ValidateAgainstExpectation(
      const base::FilePath& test_file_path,
      const base::FilePath& expected_file,
      const std::vector<std::string>& actual_lines,
      const std::vector<std::string>& expected_lines);

 private:
  // Suffix of the expectation file corresponding to html file.
  // Overridden by each platform subclass.
  // Example:
  // HTML test:      test-file.html
  // Expected:       test-file-expected-mac.txt.
  base::FilePath::StringType GetExpectedFileSuffix();

  // Some Platforms expect different outputs depending on the version.
  // Most test outputs are identical but this allows a version specific
  // expected file to be used.
  base::FilePath::StringType GetVersionSpecificExpectedFileSuffix();

  FRIEND_TEST_ALL_PREFIXES(DumpAccessibilityTestHelperTest, TestDiffLines);

  // Utility helper that does a comment-aware equality check.
  // Returns array of lines from expected file which are different.
  static std::vector<int> DiffLines(
      const std::vector<std::string>& expected_lines,
      const std::vector<std::string>& actual_lines);

  std::string expectation_type_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_DUMP_ACCESSIBILITY_TEST_HELPER_H_
