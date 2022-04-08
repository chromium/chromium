// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/net/http_cache_backend_file_operations_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/network_service_test_helper.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "sandbox/features.h"
#include "sandbox/policy/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

namespace content {
namespace {

using network::mojom::SimpleCache;
using network::mojom::SimpleCacheEntry;
using FileEnumerationEntry =
    disk_cache::BackendFileOperations::FileEnumerationEntry;

class SandboxedHttpCacheBrowserTest : public ContentBrowserTest {
 public:
  SandboxedHttpCacheBrowserTest() {
    std::vector<base::Feature> enabled_features = {
      net::features::kSandboxHttpCache,
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
      // Network Service Sandboxing is unconditionally enabled on these platforms.
      sandbox::policy::features::kNetworkServiceSandbox,
#endif
    };
    scoped_feature_list_.InitWithFeatures(
        enabled_features,
        /*disabled_features=*/{features::kNetworkServiceInProcess});
  }

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    if (!sandbox::features::IsAppContainerSandboxSupported()) {
      // On *some* Windows, sandboxing cannot be enabled. We skip all the tests
      // on such platforms.
      GTEST_SKIP();
    }
#endif

    // These assertions need to precede ContentBrowserTest::SetUp to prevent the
    // test body from running when one of the assertions fails.
    ASSERT_TRUE(IsOutOfProcessNetworkService());
    ASSERT_TRUE(sandbox::policy::features::IsNetworkSandboxEnabled());

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ContentBrowserTest::SetUp();
  }

  mojo::Remote<SimpleCache> CreateSimpleCache() {
    base::RunLoop run_loop;

    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    const base::FilePath root_path = GetTempDirPath();
    const base::FilePath path = root_path.AppendASCII("foobar");
    mojo::Remote<SimpleCache> simple_cache;
    mojo::PendingRemote<network::mojom::HttpCacheBackendFileOperationsFactory>
        factory_remote;
    HttpCacheBackendFileOperationsFactory factory(
        factory_remote.InitWithNewPipeAndPassReceiver(), root_path);

    network_service_test()->CreateSimpleCache(
        std::move(factory_remote), path,
        base::BindLambdaForTesting([&](mojo::PendingRemote<SimpleCache> cache) {
          if (cache) {
            simple_cache.Bind(std::move(cache));
          }
          run_loop.Quit();
        }));
    run_loop.Run();

    return simple_cache;
  }

  const base::FilePath& GetTempDirPath() const { return temp_dir_.GetPath(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, OpeningFileIsProhibited) {
  base::RunLoop run_loop;

  absl::optional<bool> result;
  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  const base::FilePath path =
      GetTestDataFilePath().Append(FILE_PATH_LITERAL("blank.jpg"));
  network_service_test()->OpenFile(path,
                                   base::BindLambdaForTesting([&](bool b) {
                                     result = b;
                                     run_loop.Quit();
                                   }));
  run_loop.Run();

  EXPECT_EQ(result, absl::make_optional(false));
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest,
                       EnumerateFilesOnNonExistingDirectory) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::RunLoop run_loop;

  base::FilePath root;
  base::PathService::Get(content::DIR_TEST_DATA, &root);
  root =
      root.Append(FILE_PATH_LITERAL("net")).Append(FILE_PATH_LITERAL("cache"));
  mojo::PendingRemote<network::mojom::HttpCacheBackendFileOperationsFactory>
      factory_remote;
  HttpCacheBackendFileOperationsFactory factory(
      factory_remote.InitWithNewPipeAndPassReceiver(), root);

  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  const base::FilePath path = root.Append(FILE_PATH_LITERAL("not-found"));
  std::vector<FileEnumerationEntry> entries;
  bool has_error = false;

  network_service_test()->EnumerateFiles(
      path, std::move(factory_remote),
      base::BindLambdaForTesting(
          [&](const std::vector<FileEnumerationEntry>& in_entries,
              bool in_has_error) {
            entries = in_entries;
            has_error = in_has_error;
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(entries.empty());
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, EnumerateFiles) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::RunLoop run_loop;

  base::FilePath root;
  base::PathService::Get(content::DIR_TEST_DATA, &root);
  root =
      root.Append(FILE_PATH_LITERAL("net")).Append(FILE_PATH_LITERAL("cache"));
  mojo::PendingRemote<network::mojom::HttpCacheBackendFileOperationsFactory>
      factory_remote;
  HttpCacheBackendFileOperationsFactory factory(
      factory_remote.InitWithNewPipeAndPassReceiver(), root);

  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  const base::FilePath path = root.Append(FILE_PATH_LITERAL("file_enumerator"));
  std::vector<FileEnumerationEntry> entries;
  bool has_error = false;

  network_service_test()->EnumerateFiles(
      path, std::move(factory_remote),
      base::BindLambdaForTesting(
          [&](const std::vector<FileEnumerationEntry>& in_entries,
              bool in_has_error) {
            entries = in_entries;
            has_error = in_has_error;
            run_loop.Quit();
          }));
  run_loop.Run();
  ASSERT_EQ(1u, entries.size());
  EXPECT_FALSE(has_error);
  const FileEnumerationEntry entry = entries[0];
  EXPECT_EQ(entry.path, path.Append(FILE_PATH_LITERAL("test.txt")));
  EXPECT_EQ(entry.size, 13);
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, CreateSimpleCache) {
  base::RunLoop run_loop;

  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  const base::FilePath root_path = GetTempDirPath();
  const base::FilePath path = root_path.AppendASCII("foobar");
  mojo::Remote<SimpleCache> simple_cache;
  mojo::PendingRemote<network::mojom::HttpCacheBackendFileOperationsFactory>
      factory_remote;
  HttpCacheBackendFileOperationsFactory factory(
      factory_remote.InitWithNewPipeAndPassReceiver(), root_path);

  network_service_test()->CreateSimpleCache(
      std::move(factory_remote), path,
      base::BindLambdaForTesting([&](mojo::PendingRemote<SimpleCache> cache) {
        if (cache) {
          simple_cache.Bind(std::move(cache));
        }
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(simple_cache.is_bound());
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest,
                       CreateSimpleCacheOnParentDirectory) {
  base::RunLoop run_loop;

  const base::FilePath path = GetTempDirPath();
  const base::FilePath root_path = path.AppendASCII("foobar");
  mojo::PendingRemote<network::mojom::HttpCacheBackendFileOperationsFactory>
      factory_remote;
  HttpCacheBackendFileOperationsFactory factory(
      factory_remote.InitWithNewPipeAndPassReceiver(), root_path);

  // We expect the network service to crash due to a bad mojo message.
  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  network_service_test()->CreateSimpleCache(
      std::move(factory_remote), path,
      base::BindOnce([](mojo::PendingRemote<SimpleCache> cache) {
        EXPECT_FALSE(cache.is_valid());
      }));
  run_loop.Run();
  IgnoreNetworkServiceCrashes();
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest,
                       CreateSimpleCacheWithParentDirectoryTraversal) {
  base::RunLoop run_loop;

  const base::FilePath root_path = GetTempDirPath();
  const base::FilePath path = root_path.AppendASCII("foo")
                                  .Append(base::FilePath::kParentDirectory)
                                  .AppendASCII("bar");
  mojo::PendingRemote<network::mojom::HttpCacheBackendFileOperationsFactory>
      factory_remote;
  HttpCacheBackendFileOperationsFactory factory(
      factory_remote.InitWithNewPipeAndPassReceiver(), root_path);

  // We expect the network service to crash due to a bad mojo message.
  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  network_service_test()->CreateSimpleCache(
      std::move(factory_remote), path,
      base::BindOnce([](mojo::PendingRemote<SimpleCache> cache) {
        EXPECT_FALSE(cache.is_valid());
      }));
  run_loop.Run();
  IgnoreNetworkServiceCrashes();
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, CreateEntry) {
  mojo::Remote<SimpleCache> simple_cache = CreateSimpleCache();

  ASSERT_TRUE(simple_cache.is_bound());
  mojo::Remote<SimpleCacheEntry> entry;
  base::RunLoop run_loop;
  simple_cache->CreateEntry(
      "abc", base::BindLambdaForTesting(
                 [&](mojo::PendingRemote<SimpleCacheEntry> pending_entry) {
                   if (pending_entry) {
                     entry.Bind(std::move(pending_entry));
                   }
                   run_loop.Quit();
                 }));
  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();
  ASSERT_TRUE(entry.is_bound());
}

}  // namespace
}  // namespace content
