// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_watch_scope.h"

#include <list>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/safe_base_name.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

#if BUILDFLAG(IS_WIN)
#include "base/files/file_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

class FileSystemAccessWatchScopeTest : public testing::Test {
 public:
  FileSystemAccessWatchScopeTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
#if BUILDFLAG(IS_WIN)
    // Convert path to long format to avoid mixing long and 8.3 formats in test.
    ASSERT_TRUE(dir_.Set(base::MakeLongFilePath(dir_.Take())));
#endif  // BUILDFLAG(IS_WIN)

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());
  }

  void TearDown() override { EXPECT_TRUE(dir_.Delete()); }

  storage::FileSystemURL CreateFileSystemURLFromPath(
      const base::FilePath& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey(), storage::kFileSystemTypeLocal, path);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir dir_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;
};

TEST_F(FileSystemAccessWatchScopeTest, FileScope) {
  auto file_path = dir_.GetPath().AppendASCII("file");
  auto file_url = CreateFileSystemURLFromPath(file_path);

  auto scope = FileSystemAccessWatchScope::GetScopeForFileWatch(file_url);

  EXPECT_TRUE(scope.Contains(scope));
  EXPECT_TRUE(scope.Contains(file_url));
  EXPECT_FALSE(scope.Contains(
      FileSystemAccessWatchScope::GetScopeForAllBucketFileSystems()));

  std::optional<base::SafeBaseName> sibling_name =
      base::SafeBaseName::Create(FILE_PATH_LITERAL("sibling"));
  auto sibling_url = file_url.CreateSibling(*sibling_name);
  EXPECT_FALSE(scope.Contains(sibling_url));

  auto sibling_scope =
      FileSystemAccessWatchScope::GetScopeForFileWatch(sibling_url);
  EXPECT_FALSE(scope.Contains(sibling_scope));
  EXPECT_FALSE(sibling_scope.Contains(scope));

  auto parent_url = CreateFileSystemURLFromPath(file_path.DirName());
  EXPECT_FALSE(scope.Contains(parent_url));

  auto parent_non_recursive_scope =
      FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
          parent_url, /*is_recursive=*/false);
  EXPECT_FALSE(scope.Contains(parent_non_recursive_scope));
  EXPECT_TRUE(parent_non_recursive_scope.Contains(scope));

  auto parent_recursive_scope =
      FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
          parent_url, /*is_recursive=*/true);
  EXPECT_FALSE(scope.Contains(parent_recursive_scope));
  EXPECT_TRUE(parent_recursive_scope.Contains(scope));

  // A file shouldn't have a child, but... test it just in case.
  auto child_url = CreateFileSystemURLFromPath(file_path.AppendASCII("child"));
  EXPECT_FALSE(scope.Contains(child_url));

  auto child_scope =
      FileSystemAccessWatchScope::GetScopeForFileWatch(child_url);
  EXPECT_FALSE(scope.Contains(child_scope));
  EXPECT_FALSE(child_scope.Contains(scope));

  // TODO(crbug.com/321980129): Test that URLs from different file systems
  // return are out of scope.
}

TEST_F(FileSystemAccessWatchScopeTest, DirectoryScope) {
  auto dir_path = dir_.GetPath().AppendASCII("dir");
  auto dir_url = CreateFileSystemURLFromPath(dir_path);

  auto scope = FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
      dir_url, /*is_recursive=*/false);

  EXPECT_TRUE(scope.Contains(scope));
  EXPECT_TRUE(scope.Contains(dir_url));
  EXPECT_FALSE(scope.Contains(
      FileSystemAccessWatchScope::GetScopeForAllBucketFileSystems()));

  std::optional<base::SafeBaseName> sibling_name =
      base::SafeBaseName::Create(FILE_PATH_LITERAL("sibling"));
  auto sibling_url = dir_url.CreateSibling(*sibling_name);
  EXPECT_FALSE(scope.Contains(sibling_url));

  auto sibling_scope =
      FileSystemAccessWatchScope::GetScopeForFileWatch(sibling_url);
  EXPECT_FALSE(scope.Contains(sibling_scope));
  EXPECT_FALSE(sibling_scope.Contains(scope));

  auto parent_url = CreateFileSystemURLFromPath(dir_path.DirName());
  EXPECT_FALSE(scope.Contains(parent_url));

  auto parent_non_recursive_scope =
      FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
          parent_url, /*is_recursive=*/false);
  EXPECT_FALSE(scope.Contains(parent_non_recursive_scope));
  // TODO(crbug.com/321980367): This is unfortunate. See what can be done
  // here.
  EXPECT_FALSE(parent_non_recursive_scope.Contains(scope));

  auto parent_recursive_scope =
      FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
          parent_url, /*is_recursive=*/true);
  EXPECT_FALSE(scope.Contains(parent_recursive_scope));
  EXPECT_TRUE(parent_recursive_scope.Contains(scope));

  auto child_path = dir_path.AppendASCII("child");
  auto child_url = CreateFileSystemURLFromPath(child_path);
  ASSERT_TRUE(dir_url.IsParent(child_url));
  EXPECT_TRUE(scope.Contains(child_url));

  auto child_scope =
      FileSystemAccessWatchScope::GetScopeForFileWatch(child_url);
  EXPECT_TRUE(scope.Contains(child_scope));
  EXPECT_FALSE(child_scope.Contains(scope));

  auto grandchild_url =
      CreateFileSystemURLFromPath(child_path.AppendASCII("grand"));
  ASSERT_TRUE(child_url.IsParent(grandchild_url));
  EXPECT_FALSE(scope.Contains(grandchild_url));

  auto grandchild_scope =
      FileSystemAccessWatchScope::GetScopeForFileWatch(grandchild_url);
  EXPECT_FALSE(scope.Contains(grandchild_scope));
  EXPECT_FALSE(grandchild_scope.Contains(scope));

  // TODO(crbug.com/321980129): Test that URLs from different file systems
  // return are out of scope.
}

TEST_F(FileSystemAccessWatchScopeTest, RecursiveDirectoryScope) {
  auto dir_path = dir_.GetPath().AppendASCII("dir");
  auto dir_url = CreateFileSystemURLFromPath(dir_path);

  auto scope = FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
      dir_url, /*is_recursive=*/true);

  EXPECT_TRUE(scope.Contains(scope));
  EXPECT_TRUE(scope.Contains(dir_url));
  EXPECT_FALSE(scope.Contains(
      FileSystemAccessWatchScope::GetScopeForAllBucketFileSystems()));

  std::optional<base::SafeBaseName> sibling_name =
      base::SafeBaseName::Create(FILE_PATH_LITERAL("sibling"));
  auto sibling_url = dir_url.CreateSibling(*sibling_name);
  EXPECT_FALSE(scope.Contains(sibling_url));

  auto sibling_scope =
      FileSystemAccessWatchScope::GetScopeForFileWatch(sibling_url);
  EXPECT_FALSE(scope.Contains(sibling_scope));
  EXPECT_FALSE(sibling_scope.Contains(scope));

  auto parent_url = CreateFileSystemURLFromPath(dir_path.DirName());
  EXPECT_FALSE(scope.Contains(parent_url));

  auto parent_non_recursive_scope =
      FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
          parent_url, /*is_recursive=*/false);
  EXPECT_FALSE(scope.Contains(parent_non_recursive_scope));
  EXPECT_FALSE(parent_non_recursive_scope.Contains(scope));

  auto parent_recursive_scope =
      FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
          parent_url, /*is_recursive=*/true);
  EXPECT_FALSE(scope.Contains(parent_recursive_scope));
  EXPECT_TRUE(parent_recursive_scope.Contains(scope));

  auto child_path = dir_path.AppendASCII("child");
  auto child_url = CreateFileSystemURLFromPath(child_path);
  ASSERT_TRUE(dir_url.IsParent(child_url));
  EXPECT_TRUE(scope.Contains(child_url));

  auto child_scope =
      FileSystemAccessWatchScope::GetScopeForFileWatch(child_url);
  EXPECT_TRUE(scope.Contains(child_scope));
  EXPECT_FALSE(child_scope.Contains(scope));

  auto grandchild_url =
      CreateFileSystemURLFromPath(child_path.AppendASCII("grand"));
  ASSERT_TRUE(child_url.IsParent(grandchild_url));
  EXPECT_TRUE(scope.Contains(grandchild_url));

  auto grandchild_scope =
      FileSystemAccessWatchScope::GetScopeForFileWatch(grandchild_url);
  EXPECT_TRUE(scope.Contains(grandchild_scope));
  EXPECT_FALSE(grandchild_scope.Contains(scope));

  // TODO(crbug.com/321980129): Test that URLs from different file systems
  // return are out of scope.
}

TEST_F(FileSystemAccessWatchScopeTest, AllBucketFileSystemsScope) {
  auto scope = FileSystemAccessWatchScope::GetScopeForAllBucketFileSystems();

  EXPECT_TRUE(scope.Contains(scope));

  auto dir_url = CreateFileSystemURLFromPath(dir_.GetPath());
  ASSERT_EQ(dir_url.type(), storage::FileSystemType::kFileSystemTypeLocal);
  EXPECT_FALSE(scope.Contains(dir_url));

  auto bucket_url = storage::FileSystemURL::CreateForTest(
      blink::StorageKey(), storage::kFileSystemTypeTemporary,
      base::FilePath::FromASCII("testing"));
  ASSERT_EQ(bucket_url.type(),
            storage::FileSystemType::kFileSystemTypeTemporary);
  EXPECT_TRUE(scope.Contains(bucket_url));
}

}  // namespace content
