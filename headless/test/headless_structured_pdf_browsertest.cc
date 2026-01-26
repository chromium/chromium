// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "content/public/test/browser_test.h"
#include "headless/test/headless_pdf_browsertest.h"
#include "pdf/pdf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {

namespace {
static const base::FilePath kTestDataDir(
    FILE_PATH_LITERAL("headless/test/data"));

static const base::FilePath kTestDataSubDir(
    FILE_PATH_LITERAL("structured_doc"));

// Returns files matching 'headless/test/data/structured_doc/*.html'.
std::vector<std::string> GetTestFiles() {
  base::FilePath root_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path));
  root_path = root_path.Append(kTestDataDir).Append(kTestDataSubDir);

  base::FileEnumerator enumerator(root_path, /*recursive=*/false,
                                  /*file_type=*/base::FileEnumerator::FILES,
                                  /*pattern=*/FILE_PATH_LITERAL("*.html"));

  std::vector<std::string> test_files;
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    test_files.push_back(name.BaseName().AsUTF8Unsafe());
  }

  std::sort(test_files.begin(), test_files.end());

  return test_files;
}

// Generate pretty test name given the test file name.
std::string GetTestName(std::string_view test_filename) {
  std::string name;
  name.reserve(test_filename.size());
  bool upper_case_next_char = true;
  for (char ch : test_filename) {
    if (ch == '.') {
      break;
    }
    if (ch == '_') {
      upper_case_next_char = true;
      continue;
    }
    if (!base::IsAsciiAlphaNumeric(ch)) {
      name += '_';
      continue;
    }
    if (upper_case_next_char) {
      upper_case_next_char = false;
      name += base::ToUpperASCII(ch);
    } else {
      name += ch;
    }
  }

  return name;
}

}  // namespace

class HeadlessStructuredPDFBrowserTest
    : public HeadlessPDFBrowserTestBase,
      public ::testing::WithParamInterface<std::string> {
 public:
  std::string GetTestPath() override { return test_path_; }

  std::string test_filename() const { return GetParam(); }

  void SetUp() override {
    base::FilePath path(kTestDataSubDir);
    path = path.AppendASCII(test_filename());
    test_path_ = path.AsUTF8Unsafe();

    HeadlessPDFBrowserTestBase::SetUp();
  }

  const base::FilePath GetExpectationsFilePath() {
    base::FilePath filepath;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &filepath));

    filepath = filepath.Append(kTestDataDir).AppendASCII(test_path_);
    filepath = filepath.InsertBeforeExtension(FILE_PATH_LITERAL("_expected"));
    filepath = filepath.ReplaceExtension(FILE_PATH_LITERAL(".txt"));

    return filepath;
  }

  void OnPDFReady(base::span<const uint8_t> pdf_span, int num_pages) override {
    EXPECT_THAT(num_pages, testing::Eq(1));

    std::optional<bool> tagged = chrome_pdf::IsPDFDocTagged(pdf_span);
    ASSERT_THAT(tagged, testing::Optional(true));

    constexpr int kFirstPage = 0;
    base::Value struct_tree =
        chrome_pdf::GetPDFStructTreeForPage(pdf_span, kFirstPage);
    std::string json;
    base::JSONWriter::WriteWithOptions(
        struct_tree, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);

    // Map Windows line endings to Unix by removing '\r'.
    base::RemoveChars(json, "\r", &json);

    base::FilePath expectations_filepath = GetExpectationsFilePath();
    base::ScopedAllowBlockingForTesting allow_blocking;

    if (ShouldUpdateExpectations()) {
      LOG(INFO) << "Updating expectations in " << expectations_filepath;
      CHECK(base::WriteFile(expectations_filepath, json));
    }

    std::string expected_json;
    if (!base::ReadFileToString(expectations_filepath, &expected_json)) {
      ADD_FAILURE() << "Unable to read expectations in "
                    << expectations_filepath;
    }
    EXPECT_EQ(expected_json, json)
        << "To update test expectations run the tests with --reset-results "
           "command line switch.\n";
  }

 private:
  std::string test_path_;
};

HEADLESS_DEVTOOLED_TEST_P(HeadlessStructuredPDFBrowserTest);

INSTANTIATE_TEST_SUITE_P(
    /* no prefix*/,
    HeadlessStructuredPDFBrowserTest,
    ::testing::ValuesIn(GetTestFiles()),
    [](const testing::TestParamInfo<
        HeadlessStructuredPDFBrowserTest::ParamType>& info) {
      return GetTestName(info.param);
    }

);

}  // namespace headless
