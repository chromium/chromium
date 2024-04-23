// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
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

class QuotaBrowserTest : public ContentBrowserTest {
 public:
  QuotaBrowserTest() = default;

  base::FilePath profile_path() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetPath();
  }
};

// TODO(crbug.com/40488499): Android does not support PRE_ tests.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(QuotaBrowserTest, PRE_QuotaDatabaseBootstrapTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL empty_url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(NavigateToURL(shell(), empty_url));

  // Call storage APIs to populate data on-disk in their legacy file paths.
  //
  // Cache Storage
  EXPECT_EQ(true, EvalJs(shell(), R"(
    new Promise((resolve) => {
      self.caches.open("attachements")
        .then((cache) => {
            return cache.put("notes.txt", new Response("foo"));
          })
        .then(() => { resolve(true); })
        .catch(() => { resolve(false); });
    });)"));

  // IndexedDB
  EXPECT_EQ(true, EvalJs(shell(), R"(
    new Promise((resolve) => {
      const request = window.indexedDB.open('notes');
      request.onupgradeneeded = (event) => {
        event.target.result.createObjectStore('primary', {keyPath: 'id'});
        event.target.transaction.commit();
      };
      request.onsuccess = () => resolve(true);
      request.onerror = () => resolve(false);
    });)"));

  // FileSystem
  EXPECT_EQ(true, EvalJs(shell(), R"(
    new Promise((resolve, reject) => {
      window.webkitRequestFileSystem(
        window.TEMPORARY,
        1024 * 1024,
        (fs) => {
          fs.root.getFile('file.txt', {create: true}, (entry) => {
            entry.createWriter((writer) => {
              writer.onwriteend = () => {
                resolve(true);
              };
              writer.onerror = reject;
              var blob = new Blob(['foo'], {type: 'text/plain'});
              writer.write(blob);
            }, reject);
          }, reject);
        }, reject);
      });)"));

  // ServiceWorker
  ASSERT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('empty.js');"));

  // Verify that the WebStorage directory and QuotaDatabase exists as a result
  // of accessing Storage APIs.
  base::FilePath web_storage_dir_path =
      profile_path().Append(storage::kWebStorageDirectory);
  EXPECT_TRUE(base::PathExists(web_storage_dir_path));
  base::FilePath db_path = web_storage_dir_path.AppendASCII(
      storage::QuotaManagerImpl::kDatabaseName);
  EXPECT_TRUE(base::PathExists(db_path));
}

// Continue testing after a restart to ensure that processes to the
// QuotaDatabase are disconnected.
IN_PROC_BROWSER_TEST_F(QuotaBrowserTest, QuotaDatabaseBootstrapTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Verify that calling the APIs have created files on-disk in their legacy
  // file paths. This is done after shutdown to ensure data has been flushed to
  // disk after javascript execution.
  //
  // CacheStorage
  base::FilePath service_worker_dir =
      profile_path().Append(storage::kServiceWorkerDirectory);
  EXPECT_TRUE(base::PathExists(service_worker_dir));
  base::FilePath cache_dir =
      service_worker_dir.Append(storage::kCacheStorageDirectory);
  EXPECT_TRUE(base::PathExists(cache_dir));
  EXPECT_FALSE(base::IsDirectoryEmpty(cache_dir));

  // IndexedDB
  base::FilePath idb_dir = profile_path().Append(storage::kIndexedDbDirectory);
  EXPECT_TRUE(base::PathExists(idb_dir));
  EXPECT_FALSE(base::IsDirectoryEmpty(idb_dir));

  // FileSystem
  base::FilePath fs_dir =
      profile_path().Append(FILE_PATH_LITERAL("File System"));
  EXPECT_TRUE(base::PathExists(fs_dir));
  EXPECT_FALSE(base::IsDirectoryEmpty(fs_dir));

  // ServiceWorker
  base::FilePath script_dir =
      service_worker_dir.Append(storage::kScriptCacheDirectory);
  EXPECT_TRUE(base::PathExists(script_dir));
  EXPECT_FALSE(base::IsDirectoryEmpty(script_dir));

  // Delete WebStorage directory to force a new database creation if one exists
  // so it triggers the bootstrap task. This is done after shutdown to ensure
  // files aren't accessed.
  base::FilePath web_storage_dir_path =
      profile_path().Append(storage::kWebStorageDirectory);
  EXPECT_TRUE(base::DeletePathRecursively(web_storage_dir_path));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL empty_url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(NavigateToURL(shell(), empty_url));

  // Use an API that will cause the Quota subsystem to bootstrap itself. We are
  // testing that calling this function doesn't hang.
  EXPECT_EQ(true, EvalJs(shell(), R"(
    navigator.storage.estimate().then(
      ()=>{ return true; },
      ()=>{ return false; });)"));

  // Verify that the WebStorage/QuotaManager directory was created as a result
  // of the Javascript execution.
  EXPECT_TRUE(base::PathExists(web_storage_dir_path));
  base::FilePath db_path = web_storage_dir_path.AppendASCII(
      storage::QuotaManagerImpl::kDatabaseName);
  EXPECT_TRUE(base::PathExists(db_path));
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Test for https://crbug.com/1370035 - when a CacheStorage index file without
// bucket information is present on disk and the QuotaDatabase has't been
// bootstrapped yet, the `CacheStorageManager::GetStorageKeys()` implementation
// must not attempt to use the QuotaManagerProxy to lookup bucket information.
// Doing so creates a deadlock, because `GetStorageKeys()` would wait for the
// bucket information to be returned and the QuotaManager won't respond with
// bucket information until the `GetStorageKeys()` call finishes (as part of the
// bootstrapping process).
IN_PROC_BROWSER_TEST_F(QuotaBrowserTest,
                       QuotaDatabaseBootstrapUnmigratedCacheStorageTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
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

  ASSERT_TRUE(embedded_test_server()->Start());
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
