// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/path_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::FilePath;

namespace extensions {

// Basic unittest for path_util::PrettifyPath.
// For legacy reasons, it's tested more in
// FileSystemApiTest.FileSystemApiGetDisplayPathPrettify.
TEST(ExtensionPathUtilTest, BasicPrettifyPathTest) {
  const FilePath::CharType kHomeShortcut[] = FILE_PATH_LITERAL("~");

  // Test prettifying empty path.
  FilePath unprettified;
  FilePath prettified = path_util::PrettifyPath(unprettified);
  EXPECT_EQ(unprettified, prettified);

  // Test home directory ("~").
  unprettified = base::GetHomeDir();
  prettified = path_util::PrettifyPath(unprettified);
  EXPECT_NE(unprettified, prettified);
  EXPECT_EQ(FilePath(kHomeShortcut), prettified);

  // Test with one layer ("~/foo").
  unprettified = unprettified.AppendASCII("foo");
  prettified = path_util::PrettifyPath(unprettified);
  EXPECT_NE(unprettified, prettified);
  EXPECT_EQ(FilePath(kHomeShortcut).AppendASCII("foo"), prettified);

  // Test with two layers ("~/foo/bar").
  unprettified = unprettified.AppendASCII("bar");
  prettified = path_util::PrettifyPath(unprettified);
  EXPECT_NE(unprettified, prettified);
  EXPECT_EQ(
      FilePath(kHomeShortcut).AppendASCII("foo").AppendASCII("bar"),
      prettified);
}

TEST(ExtensionPathUtilTest, ResolveHomeDirTest) {
  FilePath home_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_HOME, &home_dir));
  const FilePath abs_path(FILE_PATH_LITERAL("/foo/bar/baz"));
  const FilePath rel_path(FILE_PATH_LITERAL("foo/bar/baz"));
  const FilePath rel_path_with_tilde(FILE_PATH_LITERAL("~/foo/bar"));
  const FilePath rel_path_with_tilde_no_separator(FILE_PATH_LITERAL("~foobar"));

// This function is a no-op on Windows.
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(rel_path_with_tilde,
            path_util::ResolveHomeDirectory(rel_path_with_tilde));
#else
  EXPECT_EQ(home_dir.Append("foo/bar"),
            path_util::ResolveHomeDirectory(rel_path_with_tilde));
  // Make sure tilde without any relative path works as expected.
  EXPECT_EQ(home_dir,
            path_util::ResolveHomeDirectory(FilePath(FILE_PATH_LITERAL("~"))));
  EXPECT_EQ(home_dir,
            path_util::ResolveHomeDirectory(FilePath(FILE_PATH_LITERAL("~/"))));
#endif

  // An absolute path without a ~ should be untouched.
  EXPECT_EQ(abs_path, path_util::ResolveHomeDirectory(abs_path));
  // A relative path without a ~ should be untouched.
  EXPECT_EQ(rel_path, path_util::ResolveHomeDirectory(rel_path));
  // A tilde, followed by a non-separator character should not
  // expand.
  EXPECT_EQ(rel_path_with_tilde_no_separator,
            path_util::ResolveHomeDirectory(rel_path_with_tilde_no_separator));
}

}  // namespace extensions
