// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/dump_accessibility_test_helper.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/accessibility_tree_formatter.h"
#include "content/public/common/content_switches.h"

namespace content {

namespace {
const char kCommentToken = '#';
const char kMarkSkipFile[] = "#<skip";
const char kMarkEndOfFile[] = "<-- End-of-file -->";
const char kSignalDiff[] = "*";
}  // namespace

DumpAccessibilityTestHelper::DumpAccessibilityTestHelper(
    AccessibilityTestExpectationsLocator* test_locator)
    : test_locator_(test_locator) {}

base::FilePath DumpAccessibilityTestHelper::GetExpectationFilePath(
    const base::FilePath& test_file_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath expected_file_path;

  // Try to get version specific expected file.
  base::FilePath::StringType expected_file_suffix =
      test_locator_->GetVersionSpecificExpectedFileSuffix();
  if (expected_file_suffix != FILE_PATH_LITERAL("")) {
    expected_file_path = base::FilePath(
        test_file_path.RemoveExtension().value() + expected_file_suffix);
    if (base::PathExists(expected_file_path))
      return expected_file_path;
  }

  // If a version specific file does not exist, get the generic one.
  expected_file_suffix = test_locator_->GetExpectedFileSuffix();
  expected_file_path = base::FilePath(test_file_path.RemoveExtension().value() +
                                      expected_file_suffix);
  if (base::PathExists(expected_file_path))
    return expected_file_path;

  // If no expected file could be found, display error.
  LOG(INFO) << "File not found: " << expected_file_path.LossyDisplayName();
  LOG(INFO) << "To run this test, create "
            << expected_file_path.LossyDisplayName()
            << " (it can be empty) and then run this test "
            << "with the switch: --"
            << switches::kGenerateAccessibilityTestExpectations;
  return base::FilePath();
}

base::Optional<std::vector<std::string>>
DumpAccessibilityTestHelper::LoadExpectationFile(
    const base::FilePath& expected_file) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::string expected_contents_raw;
  base::ReadFileToString(expected_file, &expected_contents_raw);

  // Tolerate Windows-style line endings (\r\n) in the expected file:
  // normalize by deleting all \r from the file (if any) to leave only \n.
  std::string expected_contents;
  base::RemoveChars(expected_contents_raw, "\r", &expected_contents);

  if (!expected_contents.compare(0, strlen(kMarkSkipFile), kMarkSkipFile)) {
    return base::nullopt;
  }

  std::vector<std::string> expected_lines =
      base::SplitString(expected_contents, "\n", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  // Marking the end of the file with a line of text ensures that
  // file length differences are found.
  expected_lines.push_back(kMarkEndOfFile);

  return expected_lines;
}

bool DumpAccessibilityTestHelper::ValidateAgainstExpectation(
    const base::FilePath& test_file_path,
    const base::FilePath& expected_file,
    const std::vector<std::string>& actual_lines,
    const std::vector<std::string>& expected_lines) {
  // Output the test path to help anyone who encounters a failure and needs
  // to know where to look.
  LOG(INFO) << "Testing: "
            << test_file_path.NormalizePathSeparatorsTo('/').LossyDisplayName();
  LOG(INFO) << "Expected output: "
            << expected_file.NormalizePathSeparatorsTo('/').LossyDisplayName();

  // Perform a diff (or write the initial baseline).
  std::vector<int> diff_lines = DiffLines(expected_lines, actual_lines);
  bool is_different = diff_lines.size() > 0;
  if (is_different) {
    std::string diff;

    // Mark the expected lines which did not match actual output with a *.
    diff += "* Line Expected\n";
    diff += "- ---- --------\n";
    for (int line = 0, diff_index = 0;
         line < static_cast<int>(expected_lines.size()); ++line) {
      bool is_diff = false;
      if (diff_index < static_cast<int>(diff_lines.size()) &&
          diff_lines[diff_index] == line) {
        is_diff = true;
        ++diff_index;
      }
      diff += base::StringPrintf("%1s %4d %s\n", is_diff ? kSignalDiff : "",
                                 line + 1, expected_lines[line].c_str());
    }
    diff += "\nActual\n";
    diff += "------\n";
    diff += base::JoinString(actual_lines, "\n");
    diff += "\n";
    diff += kMarkEndOfFile;
    LOG(ERROR) << "Diff:\n" << diff;
  } else {
    LOG(INFO) << "Test output matches expectations.";
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGenerateAccessibilityTestExpectations)) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string actual_contents_for_output =
        base::JoinString(actual_lines, "\n") + "\n";
    CHECK(base::WriteFile(expected_file, actual_contents_for_output.c_str(),
                          actual_contents_for_output.size()) ==
          static_cast<int>(actual_contents_for_output.size()));
    LOG(INFO) << "Wrote expectations to: " << expected_file.LossyDisplayName();
  }

  return !is_different;
}

std::vector<int> DumpAccessibilityTestHelper::DiffLines(
    const std::vector<std::string>& expected_lines,
    const std::vector<std::string>& actual_lines) {
  int actual_lines_count = actual_lines.size();
  int expected_lines_count = expected_lines.size();
  std::vector<int> diff_lines;
  int i = 0, j = 0;
  while (i < actual_lines_count && j < expected_lines_count) {
    if (expected_lines[j].size() == 0 ||
        expected_lines[j][0] == kCommentToken) {
      // Skip comment lines and blank lines in expected output.
      ++j;
      continue;
    }

    if (actual_lines[i] != expected_lines[j])
      diff_lines.push_back(j);
    ++i;
    ++j;
  }

  // Actual file has been fully checked.
  return diff_lines;
}

}  // namespace content
