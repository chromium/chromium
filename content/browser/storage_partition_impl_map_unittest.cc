// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl_map.h"

#include <unordered_set>
#include <utility>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/database/database_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(StoragePartitionImplMapTest, GarbageCollect) {
  BrowserTaskEnvironment task_environment;
  TestBrowserContext browser_context;
  StoragePartitionImplMap storage_partition_impl_map(&browser_context);

  std::unordered_set<base::FilePath> active_paths;

  base::FilePath active_path = browser_context.GetPath().Append(
      StoragePartitionImplMap::GetStoragePartitionPath(
          "active", std::string()));
  ASSERT_TRUE(base::CreateDirectory(active_path));
  active_paths.insert(active_path);

  base::FilePath inactive_path = browser_context.GetPath().Append(
      StoragePartitionImplMap::GetStoragePartitionPath(
          "inactive", std::string()));
  ASSERT_TRUE(base::CreateDirectory(inactive_path));

  base::RunLoop run_loop;
  storage_partition_impl_map.GarbageCollect(std::move(active_paths),
                                            run_loop.QuitClosure());
  run_loop.Run();
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);

  EXPECT_TRUE(base::PathExists(active_path));
  EXPECT_FALSE(base::PathExists(inactive_path));
}

// TODO(crbug/333756088): Enable for Android when WebSQL has been removed from
// Android WebView.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WebSQLCleanup DISABLED_WebSQLCleanup
#else
#define MAYBE_WebSQLCleanup WebSQLCleanup
#endif
TEST(StoragePartitionImplMapTest, MAYBE_WebSQLCleanup) {
  BrowserTaskEnvironment task_environment;
  TestBrowserContext browser_context;
  base::FilePath websql_path;

  const auto kOnDiskConfig = content::StoragePartitionConfig::Create(
      &browser_context, "foo", /*partition_name=*/"", /*in_memory=*/false);

  {
    // Creating the partition in the map also does the deletion, so
    // create it once, so we can find out what path the partition
    // with this name is.
    StoragePartitionImplMap map(&browser_context);

    auto* partition = map.Get(kOnDiskConfig, true);
    websql_path = partition->GetPath().Append(storage::kDatabaseDirectoryName);

    task_environment.RunUntilIdle();

    partition->OnBrowserContextWillBeDestroyed();
  }

  // Create an WebSQL directory that would have existed.
  EXPECT_FALSE(base::PathExists(websql_path));
  EXPECT_TRUE(base::CreateDirectory(websql_path));

  {
    StoragePartitionImplMap map(&browser_context);
    auto* partition = map.Get(kOnDiskConfig, true);

    ASSERT_EQ(websql_path,
              partition->GetPath().Append(storage::kDatabaseDirectoryName));

    task_environment.RunUntilIdle();

    // Verify that creating this partition deletes any WebSQL directory it may
    // have had.
    EXPECT_FALSE(base::PathExists(websql_path));

    partition->OnBrowserContextWillBeDestroyed();
  }
}

}  // namespace content
