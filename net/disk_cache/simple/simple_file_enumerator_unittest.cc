// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_file_enumerator.h"

#include "base/path_service.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {
namespace {

base::FilePath GetRoot() {
  base::FilePath root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root);
  return root.AppendASCII("net")
      .AppendASCII("data")
      .AppendASCII("cache_tests")
      .AppendASCII("simple_file_enumerator");
}

TEST(SimpleFileEnumeratorTest, Root) {
  const base::FilePath kRoot = GetRoot();
  SimpleFileEnumerator enumerator(kRoot);

  auto entry = enumerator.Next();
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(entry->path, kRoot.AppendASCII("test.txt"));
  EXPECT_EQ(entry->size, 13);
  EXPECT_FALSE(enumerator.HasError());

  // No directories should be listed, no indirect descendants should be listed.
  EXPECT_EQ(std::nullopt, enumerator.Next());
  EXPECT_FALSE(enumerator.HasError());

  // We can call enumerator.Next() after the iteration is done.
  EXPECT_EQ(std::nullopt, enumerator.Next());
  EXPECT_FALSE(enumerator.HasError());
}

TEST(SimpleFileEnumeratorTest, NotFound) {
  const base::FilePath kRoot = GetRoot().AppendASCII("not-found");
  SimpleFileEnumerator enumerator(kRoot);

  auto entry = enumerator.Next();
  EXPECT_EQ(std::nullopt, enumerator.Next());
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  EXPECT_TRUE(enumerator.HasError());
#endif
}

}  // namespace
}  // namespace disk_cache
