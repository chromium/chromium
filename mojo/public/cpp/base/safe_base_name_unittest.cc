// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/safe_base_name_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/safe_base_name.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {

namespace {
std::optional<base::SafeBaseName> CreateSafeBaseName() {
  return base::SafeBaseName::Create(base::FilePath());
}
}  // namespace

TEST(SafeBaseNameTest, PathEmpty) {
  std::optional<base::SafeBaseName> basename = CreateSafeBaseName();
  std::optional<base::SafeBaseName> basename_out = CreateSafeBaseName();

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SafeBaseName>(
      *basename, *basename_out));
  EXPECT_EQ(basename->path(), basename_out->path());
}

TEST(SafeBaseNameTest, PathContainsNoSeparators) {
  std::optional<base::SafeBaseName> basename(
      base::SafeBaseName::Create(FILE_PATH_LITERAL("hello")));
  std::optional<base::SafeBaseName> basename_out = CreateSafeBaseName();

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SafeBaseName>(
      *basename, *basename_out));
  EXPECT_EQ(basename->path(), basename_out->path());
}

TEST(SafeBaseNameTest, PathContainsSeparators) {
  base::FilePath file = base::FilePath(FILE_PATH_LITERAL("hello"))
                            .Append(FILE_PATH_LITERAL("world"));
  std::optional<base::SafeBaseName> basename(base::SafeBaseName::Create(file));
  std::optional<base::SafeBaseName> basename_out = CreateSafeBaseName();

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SafeBaseName>(
      *basename, *basename_out));
  EXPECT_EQ(basename->path(), basename_out->path());
}

TEST(SafeBaseNameTest, PathEndsWithSeparator) {
  base::FilePath file = base::FilePath(FILE_PATH_LITERAL("hello"))
                            .Append(FILE_PATH_LITERAL("world"))
                            .AsEndingWithSeparator();
  std::optional<base::SafeBaseName> basename(base::SafeBaseName::Create(file));
  std::optional<base::SafeBaseName> basename_out = CreateSafeBaseName();

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SafeBaseName>(
      *basename, *basename_out));
  EXPECT_EQ(basename->path(), basename_out->path());
}

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
TEST(SafeBaseNameTest, PathIsRootWin) {
  mojo_base::mojom::SafeBaseNamePtr mojom_basename =
      mojo_base::mojom::SafeBaseName::New();
  mojom_basename->path = base::FilePath(FILE_PATH_LITERAL("C:\\"));
  std::optional<base::SafeBaseName> basename_out = CreateSafeBaseName();

  // Expect deserialization to fail because "C:\\ is an absolute path. See
  // safe_base_name.h
  ASSERT_FALSE(mojo::test::SerializeAndDeserialize<mojom::SafeBaseName>(
      mojom_basename, *basename_out));
}
#else
TEST(SafeBaseNameTest, PathIsRoot) {
  mojo_base::mojom::SafeBaseNamePtr mojom_basename =
      mojo_base::mojom::SafeBaseName::New();
  mojom_basename->path = base::FilePath(FILE_PATH_LITERAL("/"));
  std::optional<base::SafeBaseName> basename_out = CreateSafeBaseName();

  // Expect deserialization to fail because "/" is an absolute path. See
  // safe_base_name.h
  ASSERT_FALSE(mojo::test::SerializeAndDeserialize<mojom::SafeBaseName>(
      mojom_basename, *basename_out));
}
#endif  // FILE_PATH_USES_DRIVE_LETTERS

#if defined(FILE_PATH_USES_WIN_SEPARATORS)
TEST(SafeBaseNameTest, PathIsFileInRootWin) {
  std::optional<base::SafeBaseName> basename(
      base::SafeBaseName::Create(FILE_PATH_LITERAL("C:\\foo.txt")));
  std::optional<base::SafeBaseName> basename_out = CreateSafeBaseName();

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SafeBaseName>(
      *basename, *basename_out));
  EXPECT_EQ(basename->path(), basename_out->path());
}
#endif  // FILE_PATH_USES_WIN_SEPARATORS

}  // namespace mojo_base