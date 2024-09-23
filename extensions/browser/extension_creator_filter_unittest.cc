// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/extension_creator_filter.h"

#include <stddef.h>

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace {

class ExtensionCreatorFilterTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    extension_dir_ = temp_dir_.GetPath();

    filter_ = base::MakeRefCounted<extensions::ExtensionCreatorFilter>(
        extension_dir_);
  }

  base::FilePath CreateTestFile(const base::FilePath& file_path) {
    return CreateRelativeFilePath(file_path);
  }

  // Creates an empty file with the given relative path. Creates parent
  // directories if needed.
  base::FilePath CreateRelativeFilePath(
      const base::FilePath& relative_file_path) {
    base::FilePath path = extension_dir_.Append(relative_file_path);
    EXPECT_TRUE(base::CreateDirectory(path.DirName()));

    std::string contents = "test";
    EXPECT_TRUE(base::WriteFile(path, contents));
    return path;
  }

  base::FilePath CreateTestFileInDir(
      const base::FilePath::StringType& file_name,
      const base::FilePath::StringType& dir) {
    return CreateRelativeFilePath(base::FilePath(dir).Append(file_name));
  }

  scoped_refptr<extensions::ExtensionCreatorFilter> filter_;

  base::ScopedTempDir temp_dir_;

  base::FilePath extension_dir_;
};

struct UnaryBooleanTestData {
  const base::FilePath::CharType* input;
  bool expected;
};

TEST_F(ExtensionCreatorFilterTest, NormalCases) {
  const struct UnaryBooleanTestData cases[] = {
      {FILE_PATH_LITERAL("foo"), true},
      {FILE_PATH_LITERAL(".foo"), false},
      {FILE_PATH_LITERAL("~foo"), true},
      {FILE_PATH_LITERAL("foo~"), false},
      {FILE_PATH_LITERAL("#foo"), true},
      {FILE_PATH_LITERAL("foo#"), true},
      {FILE_PATH_LITERAL("#foo#"), false},
      {FILE_PATH_LITERAL(".svn"), false},
      {FILE_PATH_LITERAL("__MACOSX"), false},
      {FILE_PATH_LITERAL(".DS_Store"), false},
      {FILE_PATH_LITERAL("desktop.ini"), false},
      {FILE_PATH_LITERAL("Thumbs.db"), false},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    base::FilePath input(cases[i].input);
    base::FilePath test_file(CreateTestFile(input));
    bool observed = filter_->ShouldPackageFile(test_file);

    EXPECT_EQ(cases[i].expected, observed)
        << "i: " << i << ", input: " << test_file.value();
  }
}

TEST_F(ExtensionCreatorFilterTest, MetadataFolderExcluded) {
  const struct UnaryBooleanTestData cases[] = {
      {FILE_PATH_LITERAL("_metadata/foo"), false},
      {FILE_PATH_LITERAL("_metadata/abc/foo"), false},
      {FILE_PATH_LITERAL("_metadata/abc/xyz/foo"), false},
      {FILE_PATH_LITERAL("abc/_metadata/xyz"), true},
      {FILE_PATH_LITERAL("xyz/_metadata"), true},
  };

  // Create and test the filepaths.
  for (size_t i = 0; i < std::size(cases); ++i) {
    base::FilePath test_file =
        CreateRelativeFilePath(base::FilePath(cases[i].input));
    bool observed = filter_->ShouldPackageFile(test_file);

    EXPECT_EQ(cases[i].expected, observed)
        << "i: " << i << ", input: " << test_file.value();
  }

  // Also test directories.
  const struct UnaryBooleanTestData directory_cases[] = {
      {FILE_PATH_LITERAL("_metadata"), false},
      {FILE_PATH_LITERAL("_metadata/abc"), false},
      {FILE_PATH_LITERAL("_metadata/abc/xyz"), false},
      {FILE_PATH_LITERAL("abc"), true},
      {FILE_PATH_LITERAL("abc/_metadata"), true},
      {FILE_PATH_LITERAL("xyz"), true},
  };
  for (size_t i = 0; i < std::size(directory_cases); ++i) {
    base::FilePath directory = extension_dir_.Append(directory_cases[i].input);
    bool observed = filter_->ShouldPackageFile(directory);

    EXPECT_EQ(directory_cases[i].expected, observed)
        << "i: " << i << ", input: " << directory.value();
  }
}

struct StringStringWithBooleanTestData {
  const base::FilePath::StringType file_name;
  const base::FilePath::StringType dir;
  bool expected;
};

// Ignore the files in special directories, including ".git", ".svn",
// "__MACOSX".
TEST_F(ExtensionCreatorFilterTest, IgnoreFilesInSpecialDir) {
  const struct StringStringWithBooleanTestData cases[] = {
      {FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL(".git"), false},
      {FILE_PATH_LITERAL("goo"), FILE_PATH_LITERAL(".svn"), false},
      {FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL("__MACOSX"), false},
      {FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL("foo"), true},
      {FILE_PATH_LITERAL("index.js"), FILE_PATH_LITERAL("scripts"), true},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    base::FilePath test_file(
        CreateTestFileInDir(cases[i].file_name, cases[i].dir));
    bool observed = filter_->ShouldPackageFile(test_file);
    EXPECT_EQ(cases[i].expected, observed)
        << "i: " << i << ", input: " << test_file.value();
  }
}

#if BUILDFLAG(IS_WIN)
struct StringBooleanWithBooleanTestData {
  const base::FilePath::CharType* input_char;
  bool input_bool;
  bool expected;
};

TEST_F(ExtensionCreatorFilterTest, WindowsHiddenFiles) {
  const struct StringBooleanWithBooleanTestData cases[] = {
      {FILE_PATH_LITERAL("a-normal-file"), false, true},
      {FILE_PATH_LITERAL(".a-dot-file"), false, false},
      {FILE_PATH_LITERAL(".a-dot-file-that-we-have-set-to-hidden"), true,
       false},
      {FILE_PATH_LITERAL("a-file-that-we-have-set-to-hidden"), true, false},
      {FILE_PATH_LITERAL("a-file-that-we-have-not-set-to-hidden"), false, true},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    base::FilePath input(cases[i].input_char);
    bool should_hide = cases[i].input_bool;
    base::FilePath test_file(CreateTestFile(input));

    if (should_hide) {
      SetFileAttributes(test_file.value().c_str(), FILE_ATTRIBUTE_HIDDEN);
    }
    bool observed = filter_->ShouldPackageFile(test_file);
    EXPECT_EQ(cases[i].expected, observed)
        << "i: " << i << ", input: " << test_file.value();
  }
}
#endif

}  // namespace
