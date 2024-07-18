// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/base/file_path_mojom_traits.h"

#include "base/test/gtest_util.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/file_path.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL(literal) FILE_PATH_LITERAL(literal)

namespace mojo_base {
namespace {

// Helper to construct RelativeFilePath structs that do not actually conform to
// the expected preconditions.
mojom::RelativeFilePathPtr CreateArbitraryRelativeFilePath(
    const base::FilePath& file_path) {
  auto mojo_file_path = mojom::RelativeFilePath::New();
#if BUILDFLAG(IS_WIN)
  const auto* data_ptr =
      reinterpret_cast<const uint16_t*>(file_path.value().data());
  mojo_file_path->path =
      std::vector(data_ptr, data_ptr + file_path.value().size());
#else
  mojo_file_path->path = file_path.value();
#endif
  return mojo_file_path;
}

TEST(FilePathTest, File) {
  base::FilePath dir(FILE_PATH_LITERAL("hello"));
  base::FilePath file = dir.Append(FILE_PATH_LITERAL("world"));
  base::FilePath file_out;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::FilePath>(file, file_out));
  ASSERT_EQ(file, file_out);
}

TEST(FilePathTest, RelativeFilePath) {
  base::FilePath dir(FILE_PATH_LITERAL("hello"));

  base::FilePath file = dir.Append(FILE_PATH_LITERAL("world"));
  base::FilePath file_out;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::RelativeFilePath>(
      file, file_out));
  ASSERT_EQ(file, file_out);

  base::FilePath ignored_out;
  {
#if BUILDFLAG(IS_WIN)
    const base::FilePath in_path(FPL("C:\\\\Windows\\system32\\kernel32.dll"));
#else
    const base::FilePath in_path(FPL("/vmlinuz"));
#endif
    ASSERT_TRUE(in_path.IsAbsolute());

    EXPECT_CHECK_DEATH(
        mojo::test::SerializeAndDeserialize<mojom::RelativeFilePath>(
            in_path, ignored_out));

    auto in_struct = CreateArbitraryRelativeFilePath(in_path);
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::RelativeFilePath>(
        in_struct, ignored_out));
  }

  {
#if BUILDFLAG(IS_WIN)
    const base::FilePath in_path(FPL("relative\\path\\..\\with\\traversals"));
#else
    const base::FilePath in_path(FPL("relative/path/../with/traversals"));
#endif
    ASSERT_TRUE(!in_path.IsAbsolute());
    ASSERT_TRUE(in_path.ReferencesParent());

    EXPECT_CHECK_DEATH(
        mojo::test::SerializeAndDeserialize<mojom::RelativeFilePath>(
            in_path, ignored_out));

    auto in_struct = CreateArbitraryRelativeFilePath(in_path);
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::RelativeFilePath>(
        in_struct, ignored_out));
  }
}

}  // namespace
}  // namespace mojo_base
