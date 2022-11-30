// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "components/services/storage/public/cpp/constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class CacheStorageBrowserTest : public ContentBrowserTest {
 public:
  CacheStorageBrowserTest() = default;

  base::FilePath profile_path() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetPath();
  }
};

// Test for https://crbug.com/1370035 - when a CacheStorage index file without
// bucket information is present on disk and the QuotaDatabase has't been
// bootstrapped yet, the `CacheStorageManager::GetStorageKeys()` implementation
// must not attempt to use the QuotaManagerProxy to lookup bucket information.
// Doing so creates a deadlock, because `GetStorageKeys()` would wait for the
// bucket information to be returned and the QuotaManager won't respond with
// bucket information until the `GetStorageKeys()` call finishes (as part of the
// bootstrapping process).
IN_PROC_BROWSER_TEST_F(CacheStorageBrowserTest, GetStorageKeysTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  ASSERT_TRUE(embedded_test_server()->Start());

  // Set up the profile directory to have a CacheStorage index file that hasn't
  // been migrated to contain bucket information yet.
  base::FilePath service_worker_dir_path =
      profile_path().Append(storage::kServiceWorkerDirectory);
  base::FilePath cache_storage_dir_path =
      service_worker_dir_path.Append(storage::kCacheStorageDirectory);

  EXPECT_FALSE(base::PathExists(service_worker_dir_path));
  EXPECT_TRUE(base::CreateDirectory(service_worker_dir_path));

  EXPECT_FALSE(base::PathExists(cache_storage_dir_path));
  EXPECT_TRUE(base::CreateDirectory(cache_storage_dir_path));

  base::FilePath test_cache_storage_origin_path =
      GetTestFilePath("cache_storage", "storage_key")
          .AppendASCII("0430f1a484a0ea6d8de562488c5fbeec0111d16f");
  EXPECT_TRUE(base::PathExists(test_cache_storage_origin_path));

  EXPECT_TRUE(base::CopyDirectory(test_cache_storage_origin_path,
                                  cache_storage_dir_path,
                                  /*recursive=*/true));

  // Navigate to any page that we can use for testing.
  GURL empty_url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(NavigateToURL(shell(), empty_url));

  // Assume that the WebStorage directory doesn't exist yet. This indicates that
  // the QuotaDatabase hasn't been bootstrapped, which is a precondition for
  // this test.
  base::FilePath web_storage_dir_path =
      profile_path().Append(storage::kWebStorageDirectory);
  EXPECT_FALSE(base::PathExists(web_storage_dir_path));

  // Use an API that will cause the Quota subsystem to bootstrap itself. We are
  // testing that calling this function doesn't hang.
  EXPECT_EQ(true, EvalJs(shell(), R"(
        navigator.storage.estimate().then(
          ()=>{ return true; },
          ()=>{ return false; });)"));

  // Verify that the WebStorage/QuotaManager directory was created as a result
  // of the Javascript execution.
  EXPECT_TRUE(base::PathExists(web_storage_dir_path.AppendASCII(
      storage::QuotaManagerImpl::kDatabaseName)));
}

}  // namespace content
