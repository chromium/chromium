// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/content_verifier_utils.h"

#include "base/files/file_path.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

struct UnaryTestData {
  base::FilePath::StringViewType input;
  base::FilePath::StringViewType expected;
};

}  // namespace

using ContentVerifierUtilsTest = testing::Test;

// This macro helps avoid wrapped lines in the test structs.
#define FPL(x) FILE_PATH_LITERAL(x)

TEST_F(ContentVerifierUtilsTest, NormalizePathComponents) {
  const UnaryTestData cases[] = {
      // Based on Python's os.path.normpath test cases:
      // https://github.com/python/cpython/blob/bf4c1bf344ed1f80c4e8f4fd5b1a8f0e0858777e/Lib/test/test_posixpath.py#L376
      {FPL(""), FPL(".")},
      {FPL("."), FPL(".")},
      {FPL("./"), FPL("./")},
      {FPL(".//."), FPL(".")},
      {FPL("./.."), FPL("..")},
      {FPL("./../"), FPL("../")},
      {FPL("./foo/bar"), FPL("foo/bar")},
      {FPL(".."), FPL("..")},
      {FPL("../"), FPL("../")},
      {FPL("../foo"), FPL("../foo")},
      {FPL("../../foo"), FPL("../../foo")},
      {FPL("../foo/../bar"), FPL("../bar")},
      {FPL("../../foo/../bar/./baz/boom/.."), FPL("../../bar/baz")},
      {FPL("foo/../bar/baz"), FPL("bar/baz")},
      {FPL("foo/../../bar/baz"), FPL("../bar/baz")},
      {FPL("foo/../../../bar/baz"), FPL("../../bar/baz")},
      {FPL("foo///../bar/.././../baz/boom"), FPL("../baz/boom")},
      {FPL("foo/bar/../..///../../baz/boom"), FPL("../../baz/boom")},
  };

  for (const auto& i : cases) {
    base::FilePath input = base::FilePath(i.input).NormalizePathSeparators();
    base::FilePath expected =
        base::FilePath(i.expected).NormalizePathSeparators();
    EXPECT_EQ(expected.value(),
              content_verifier_utils::NormalizePathComponents(input).value());
  }
}

#if defined(FILE_PATH_USES_WIN_SEPARATORS)
TEST_F(ContentVerifierUtilsTest,
       NormalizePathComponentsWithUnnormalizedSeparators) {
  const struct UnaryTestData cases[] = {
      {FPL("foo/bar"), FPL("foo\\bar")},
      {FPL("foo/bar\\betz"), FPL("foo\\bar\\betz")},
      {FPL("foo\\bar"), FPL("foo\\bar")},
      {FPL("foo\\bar/betz"), FPL("foo\\bar\\betz")},
      {FPL("foo"), FPL("foo")},
      // Trailing slashes don't automatically get stripped.  That's what
      // StripTrailingSeparators() is for.
      {FPL("foo\\"), FPL("foo\\")},
      {FPL("foo/"), FPL("foo\\")},
      {FPL("foo/bar\\"), FPL("foo\\bar\\")},
      {FPL("foo\\bar/"), FPL("foo\\bar\\")},
      {FPL("foo/bar/"), FPL("foo\\bar\\")},
      {FPL("foo\\bar\\"), FPL("foo\\bar\\")},
      // This method normalizes the number of path separators.
      {FPL("foo\\\\bar"), FPL("foo\\bar")},
      {FPL("foo//bar"), FPL("foo\\bar")},
      {FPL("foo/\\bar"), FPL("foo\\bar")},
      {FPL("foo\\/bar"), FPL("foo\\bar")},
      {FPL("foo//bar///"), FPL("foo\\bar\\")},
      {FPL("foo/\\bar/\\"), FPL("foo\\bar\\")},
  };

  for (const auto& i : cases) {
    base::FilePath observed = content_verifier_utils::NormalizePathComponents(
        base::FilePath(i.input));
    EXPECT_EQ(base::FilePath::StringType(i.expected), observed.value());
  }
}
#endif

#undef FPL

}  // namespace extensions
