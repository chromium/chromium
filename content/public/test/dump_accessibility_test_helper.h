// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_DUMP_ACCESSIBILITY_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_DUMP_ACCESSIBILITY_TEST_HELPER_H_

#include "base/optional.h"

namespace base {
class FilePath;
}

namespace content {

class AccessibilityTestExpectationsLocator;

// A helper class for writing accessibility tree dump tests.
class DumpAccessibilityTestHelper {
 public:
  explicit DumpAccessibilityTestHelper(
      AccessibilityTestExpectationsLocator* test_locator);
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
  // Utility helper that does a comment-aware equality check.
  // Returns array of lines from expected file which are different.
  static std::vector<int> DiffLines(
      const std::vector<std::string>& expected_lines,
      const std::vector<std::string>& actual_lines);

  AccessibilityTestExpectationsLocator* const test_locator_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_DUMP_ACCESSIBILITY_TEST_HELPER_H_
