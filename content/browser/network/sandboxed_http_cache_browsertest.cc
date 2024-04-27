// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/network/http_cache_backend_file_operations_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/network_service_test_helper.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/features.h"
#include "sandbox/policy/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace content {
namespace {

using network::mojom::SimpleCache;
using network::mojom::SimpleCacheEntry;
using network::mojom::SimpleCacheEntryEnumerator;
using network::mojom::SimpleCacheOpenEntryResult;
using network::mojom::SimpleCacheOpenEntryResultPtr;

using FileEnumerationEntry =
    disk_cache::BackendFileOperations::FileEnumerationEntry;

// On Mac and Fuchsia, sandboxing is always enabled, so we don't need to test
// the non-sandboxing configuration.
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)

class NonSandboxedNetworkServiceBrowserTest : public ContentBrowserTest {
 public:
  NonSandboxedNetworkServiceBrowserTest() {
    std::vector<base::test::FeatureRef> kDisabledFeatures = {
        sandbox::policy::features::kNetworkServiceSandbox,
    };
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/kDisabledFeatures);
    ForceOutOfProcessNetworkService();
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(IsOutOfProcessNetworkService());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NonSandboxedNetworkServiceBrowserTest,
                       OpeningFileIsAllowed) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::RunLoop run_loop;

  std::optional<bool> result;
  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  const base::FilePath path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("blank.jpg"));
  network_service_test()->OpenFile(path,
                                   base::BindLambdaForTesting([&](bool ok) {
                                     result = ok;
                                     run_loop.Quit();
                                   }));
  run_loop.Run();

  EXPECT_EQ(result, std::make_optional(true));
}

#endif

class SandboxedHttpCacheBrowserTest : public ContentBrowserTest {
 public:
  SandboxedHttpCacheBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
      features::kBrokerFileOperationsOnDiskCacheInNetworkService,
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
      // Network Service Sandboxing is unconditionally enabled on these
      // platforms.
      sandbox::policy::features::kNetworkServiceSandbox,
#endif
    };
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
    ForceOutOfProcessNetworkService();
  }

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    if (!sandbox::policy::features::IsNetworkSandboxSupported()) {
      // On *some* Windows, sandboxing cannot be enabled. We skip all the tests
      // on such platforms.
      GTEST_SKIP();
    }
#endif

#if BUILDFLAG(IS_ANDROID)
    {
      // On older android, we cannot use ftruncate in the network process.
      // See https://crbug.com/1315933 and https://crrev.com/c/3581676.
      // Let's skip the tests.
      const int sdk_version =
          base::android::BuildInfo::GetInstance()->sdk_int();
      if (sdk_version <= base::android::SdkVersion::SDK_VERSION_MARSHMALLOW) {
        DVLOG(0) << "Android is too old: " << sdk_version;
        GTEST_SKIP();
      }
    }
#endif

    // These assertions need to precede ContentBrowserTest::SetUp to prevent the
    // test body from running when one of the assertions fails.
    ASSERT_TRUE(IsOutOfProcessNetworkService());
    ASSERT_TRUE(sandbox::policy::features::IsNetworkSandboxEnabled());

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(IsOutOfProcessNetworkService());
  }

  mojo::Remote<SimpleCache> CreateSimpleCache() {
    base::RunLoop run_loop;

    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    const base::FilePath root_path = GetTempDirPath();
    const base::FilePath path = root_path.AppendASCII("foobar");
    mojo::Remote<SimpleCache> simple_cache;
    mojo::PendingRemote<network::mojom::HttpCacheBackendFileOperationsFactory>
        factory_remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<HttpCacheBackendFileOperationsFactory>(root_path),
        factory_remote.InitWithNewPipeAndPassReceiver());

    network_service_test()->CreateSimpleCache(
        std::move(factory_remote), path, /*reset=*/false,
        base::BindLambdaForTesting([&](mojo::PendingRemote<SimpleCache> cache) {
          if (cache) {
            simple_cache.Bind(std::move(cache));
          }
          run_loop.Quit();
        }));
    run_loop.Run();

    return simple_cache;
  }

  mojo::Remote<SimpleCacheEntry> CreateEntry(SimpleCache* cache,
                                             const std::string& key) {
    mojo::Remote<SimpleCacheEntry> entry;
    base::RunLoop run_loop;
    cache->CreateEntry(
        key, base::BindLambdaForTesting(
                 [&](mojo::PendingRemote<SimpleCacheEntry> pending_entry,
                     int net_error) {
                   if (pending_entry) {
                     entry.Bind(std::move(pending_entry));
                   }
                   run_loop.Quit();
                 }));
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();
    return entry;
  }

  mojo::Remote<SimpleCacheEntry> OpenEntry(SimpleCache* cache,
                                           const std::string& key) {
    mojo::Remote<SimpleCacheEntry> entry;
    base::RunLoop run_loop;
    cache->OpenEntry(
        key, base::BindLambdaForTesting(
                 [&](mojo::PendingRemote<SimpleCacheEntry> pending_entry,
                     int net_error) {
                   if (pending_entry) {
                     entry.Bind(std::move(pending_entry));
                   }
                   run_loop.Quit();
                 }));
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();
    return entry;
  }

  void Close(mojo::Remote<SimpleCacheEntry> entry) {
    base::RunLoop run_loop;
    entry->Close(run_loop.QuitClosure());
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();
  }

  void Detach(mojo::Remote<SimpleCache> cache) {
    base::RunLoop run_loop;
    cache->Detach(run_loop.QuitClosure());
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();
  }

  SimpleCacheOpenEntryResultPtr OpenNextEntry(
      SimpleCacheEntryEnumerator* enumerator) {
    base::RunLoop run_loop;

    SimpleCacheOpenEntryResultPtr result_to_pass;
    std::pair<mojo::Remote<SimpleCacheEntry>, int> result;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    enumerator->GetNext(
        base::BindLambdaForTesting([&](SimpleCacheOpenEntryResultPtr result) {
          DCHECK(result);
          result_to_pass = std::move(result);
          run_loop.Quit();
        }));
    run_loop.Run();

    return result_to_pass;
  }

  const base::FilePath& GetTempDirPath() const { return temp_dir_.GetPath(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, OpeningFileIsProhibited) {
  base::RunLoop run_loop;

  std::optional<bool> result;
  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  const base::FilePath path =
      GetTestDataFilePath().Append(FILE_PATH_LITERAL("blank.jpg"));
  network_service_test()->OpenFile(path,
                                   base::BindLambdaForTesting([&](bool b) {
                                     result = b;
                                     run_loop.Quit();
                                   }));
  run_loop.Run();

  EXPECT_EQ(result, std::make_optional(false));
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
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<HttpCacheBackendFileOperationsFactory>(root),
      factory_remote.InitWithNewPipeAndPassReceiver());

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
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<HttpCacheBackendFileOperationsFactory>(root),
      factory_remote.InitWithNewPipeAndPassReceiver());

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
  mojo::Remote<SimpleCache> simple_cache;
  mojo::PendingRemote<network::mojom::HttpCacheBackendFileOperationsFactory>
      factory_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<HttpCacheBackendFileOperationsFactory>(root_path),
      factory_remote.InitWithNewPipeAndPassReceiver());

  network_service_test()->CreateSimpleCache(
      std::move(factory_remote), root_path, /*reset=*/false,
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
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<HttpCacheBackendFileOperationsFactory>(root_path),
      factory_remote.InitWithNewPipeAndPassReceiver());

  // We expect the network service to crash due to a bad mojo message.
  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  network_service_test()->CreateSimpleCache(
      std::move(factory_remote), path, /*reset=*/false,
      base::BindOnce([](mojo::PendingRemote<SimpleCache> cache) {
        EXPECT_FALSE(cache.is_valid());
      }));
  run_loop.Run();
  IgnoreNetworkServiceCrashes();
}

// TODO(crbug.com/40919503): Flaky on at least Mac11.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CreateSimpleCacheWithParentDirectoryTraversal \
  DISABLED_CreateSimpleCacheWithParentDirectoryTraversal
#else
#define MAYBE_CreateSimpleCacheWithParentDirectoryTraversal \
  CreateSimpleCacheWithParentDirectoryTraversal
#endif
IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest,
                       MAYBE_CreateSimpleCacheWithParentDirectoryTraversal) {
  base::RunLoop run_loop;

  const base::FilePath root_path = GetTempDirPath();
  const base::FilePath path = root_path.AppendASCII("foo")
                                  .Append(base::FilePath::kParentDirectory)
                                  .AppendASCII("bar");
  mojo::PendingRemote<network::mojom::HttpCacheBackendFileOperationsFactory>
      factory_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<HttpCacheBackendFileOperationsFactory>(root_path),
      factory_remote.InitWithNewPipeAndPassReceiver());

  // We expect the network service to crash due to a bad mojo message.
  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  network_service_test()->CreateSimpleCache(
      std::move(factory_remote), path, /*reset=*/false,
      base::BindOnce([](mojo::PendingRemote<SimpleCache> cache) {
        EXPECT_FALSE(cache.is_valid());
      }));
  run_loop.Run();
  IgnoreNetworkServiceCrashes();
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest,
                       CreateSimpleCacheWithReset) {
  base::RunLoop run_loop;

  const base::FilePath root_path = GetTempDirPath();
  const base::FilePath path = root_path.Append(FILE_PATH_LITERAL("cache-dir"));
  const base::FilePath child_path = path.Append(FILE_PATH_LITERAL("child"));
  mojo::Remote<SimpleCache> simple_cache;
  mojo::PendingRemote<network::mojom::HttpCacheBackendFileOperationsFactory>
      factory_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<HttpCacheBackendFileOperationsFactory>(root_path),
      factory_remote.InitWithNewPipeAndPassReceiver());

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::CreateDirectory(path));
    ASSERT_TRUE(base::CreateDirectory(child_path));
    ASSERT_TRUE(base::PathExists(child_path));
  }

  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  network_service_test()->CreateSimpleCache(
      std::move(factory_remote), path, /*reset=*/true,
      base::BindLambdaForTesting([&](mojo::PendingRemote<SimpleCache> cache) {
        if (cache) {
          simple_cache.Bind(std::move(cache));
        }
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_TRUE(simple_cache.is_bound());

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(path));
    EXPECT_FALSE(base::PathExists(child_path));
  }
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, CreateEntry) {
  mojo::Remote<SimpleCache> simple_cache = CreateSimpleCache();

  ASSERT_TRUE(simple_cache.is_bound());
  ASSERT_TRUE(network_service_test().is_connected());

  mojo::Remote<SimpleCacheEntry> entry = CreateEntry(simple_cache.get(), "abc");
  ASSERT_TRUE(entry.is_bound());
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, OpenNonExistingEntry) {
  mojo::Remote<SimpleCache> simple_cache = CreateSimpleCache();

  ASSERT_TRUE(simple_cache.is_bound());
  ASSERT_TRUE(network_service_test().is_connected());

  mojo::Remote<SimpleCacheEntry> entry = OpenEntry(simple_cache.get(), "abc");
  ASSERT_FALSE(entry.is_bound());
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, CreateAndOpenEntry) {
  mojo::Remote<SimpleCache> simple_cache = CreateSimpleCache();

  ASSERT_TRUE(simple_cache.is_bound());
  mojo::Remote<SimpleCacheEntry> entry = CreateEntry(simple_cache.get(), "abc");

  ASSERT_TRUE(entry.is_bound());
  ASSERT_TRUE(network_service_test().is_connected());

  Close(std::move(entry));
  ASSERT_TRUE(network_service_test().is_connected());

  ASSERT_TRUE(simple_cache.is_bound());
  entry = OpenEntry(simple_cache.get(), "abc");

  ASSERT_TRUE(entry.is_bound());
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, WriteAndReadData) {
  mojo::Remote<SimpleCache> simple_cache = CreateSimpleCache();
  const std::vector<uint8_t> kData = {'A', 'B', 'C', 'D', 'E'};
  const std::string kKey = "key";
  constexpr int kOffset = 4;
  constexpr int kIndex = 1;

  ASSERT_TRUE(simple_cache.is_bound());
  mojo::Remote<SimpleCacheEntry> entry = CreateEntry(simple_cache.get(), kKey);
  ASSERT_TRUE(entry.is_bound());
  ASSERT_TRUE(network_service_test().is_connected());

  {
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    entry->WriteData(kIndex, kOffset, kData, /*truncate=*/false,
                     base::BindLambdaForTesting([&](int result) {
                       EXPECT_EQ(result, static_cast<int>(kData.size()));
                       run_loop.Quit();
                     }));
    run_loop.Run();
  }
  ASSERT_TRUE(network_service_test().is_connected());

  Close(std::move(entry));
  ASSERT_TRUE(network_service_test().is_connected());

  Detach(std::move(simple_cache));
  ASSERT_TRUE(network_service_test().is_connected());

  simple_cache = CreateSimpleCache();
  ASSERT_TRUE(simple_cache.is_bound());
  ASSERT_TRUE(network_service_test().is_connected());

  entry = OpenEntry(simple_cache.get(), kKey);
  ASSERT_TRUE(entry);
  ASSERT_TRUE(network_service_test().is_connected());
  {
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    entry->ReadData(kIndex, 0, kOffset + kData.size() + 5,
                    base::BindLambdaForTesting(
                        [&](const std::vector<uint8_t>& data, int result) {
                          std::vector<uint8_t> expected_data(kOffset, 0);
                          expected_data.insert(expected_data.end(),
                                               kData.cbegin(), kData.cend());
                          EXPECT_EQ(result,
                                    static_cast<int>(expected_data.size()));
                          EXPECT_EQ(data, expected_data);
                          run_loop.Quit();
                        }));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest,
                       WriteTruncateAndReadData) {
  mojo::Remote<SimpleCache> simple_cache = CreateSimpleCache();
  const std::vector<uint8_t> kData = {'A', 'B', 'C', 'D', 'E'};
  const std::string kKey = "key";
  constexpr int kOffset = 4;
  constexpr int kIndex = 1;

  ASSERT_TRUE(simple_cache.is_bound());
  mojo::Remote<SimpleCacheEntry> entry = CreateEntry(simple_cache.get(), kKey);
  ASSERT_TRUE(entry.is_bound());
  ASSERT_TRUE(network_service_test().is_connected());

  {
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    entry->WriteData(kIndex, kOffset, kData, /*truncate=*/false,
                     base::BindLambdaForTesting([&](int result) {
                       EXPECT_EQ(result, static_cast<int>(kData.size()));
                       run_loop.Quit();
                     }));
    run_loop.Run();
  }
  ASSERT_TRUE(network_service_test().is_connected());

  {
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    entry->WriteData(kIndex, /*offset=*/0, kData, /*truncate=*/true,
                     base::BindLambdaForTesting([&](int result) {
                       EXPECT_EQ(result, static_cast<int>(kData.size()));
                       run_loop.Quit();
                     }));
    run_loop.Run();
  }
  ASSERT_TRUE(network_service_test().is_connected());

  {
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    entry->Close(run_loop.QuitClosure());
    entry.reset();
  }
  ASSERT_TRUE(network_service_test().is_connected());

  Detach(std::move(simple_cache));
  ASSERT_TRUE(network_service_test().is_connected());

  simple_cache = CreateSimpleCache();
  ASSERT_TRUE(simple_cache.is_bound());
  ASSERT_TRUE(network_service_test().is_connected());

  entry = OpenEntry(simple_cache.get(), kKey);
  ASSERT_TRUE(entry);
  ASSERT_TRUE(network_service_test().is_connected());

  {
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    entry->ReadData(kIndex, 0, kOffset + kData.size() + 5,
                    base::BindLambdaForTesting(
                        [&](const std::vector<uint8_t>& data, int result) {
                          EXPECT_EQ(result, static_cast<int>(kData.size()));
                          EXPECT_EQ(data, kData);
                          run_loop.Quit();
                        }));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, WriteAndReadSparseData) {
  const std::string kKey = "key";
  constexpr int kOffset = 1024;
  std::vector<uint8_t> original_data;
  for (int i = 0; i < 2048; ++i) {
    original_data.push_back(static_cast<uint8_t>(i % 26 + 'A'));
  }

  mojo::Remote<SimpleCache> simple_cache = CreateSimpleCache();
  ASSERT_TRUE(simple_cache.is_bound());
  mojo::Remote<SimpleCacheEntry> entry = CreateEntry(simple_cache.get(), kKey);
  ASSERT_TRUE(entry.is_bound());
  ASSERT_TRUE(network_service_test().is_connected());

  {
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    entry->WriteSparseData(
        kOffset, original_data, base::BindLambdaForTesting([&](int result) {
          EXPECT_EQ(result, static_cast<int>(original_data.size()));
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  ASSERT_TRUE(network_service_test().is_connected());

  {
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    entry->Close(run_loop.QuitClosure());
    entry.reset();
  }
  ASSERT_TRUE(network_service_test().is_connected());

  Detach(std::move(simple_cache));
  ASSERT_TRUE(network_service_test().is_connected());

  simple_cache = CreateSimpleCache();
  ASSERT_TRUE(simple_cache.is_bound());
  ASSERT_TRUE(network_service_test().is_connected());

  entry = OpenEntry(simple_cache.get(), kKey);
  ASSERT_TRUE(entry);
  ASSERT_TRUE(network_service_test().is_connected());

  {
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    entry->ReadSparseData(
        kOffset, original_data.size() + 1024,
        base::BindLambdaForTesting(
            [&](const std::vector<uint8_t>& data, int result) {
              EXPECT_EQ(result, static_cast<int>(original_data.size()));
              EXPECT_EQ(data, original_data);
              run_loop.Quit();
            }));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, DoomEntry) {
  const std::vector<uint8_t> kData = {'A', 'B', 'C'};
  const std::vector<uint8_t> kSparseData(1024, 'A');
  mojo::Remote<SimpleCache> simple_cache = CreateSimpleCache();

  ASSERT_TRUE(simple_cache.is_bound());
  mojo::Remote<SimpleCacheEntry> entry = CreateEntry(simple_cache.get(), "abc");

  ASSERT_TRUE(entry.is_bound());
  ASSERT_TRUE(network_service_test().is_connected());

  {
    // Write something, to open files.
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    entry->WriteData(/*index=*/0, /*offset=*/0, kData, /*truncate=*/false,
                     base::BindLambdaForTesting([&](int result) {
                       EXPECT_EQ(result, static_cast<int>(kData.size()));
                       run_loop.Quit();
                     }));
    run_loop.Run();
  }
  ASSERT_TRUE(network_service_test().is_connected());

  {
    // Write something, to open files.
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    entry->WriteSparseData(
        /*offset=*/0, kSparseData, base::BindLambdaForTesting([&](int result) {
          EXPECT_EQ(result, static_cast<int>(kSparseData.size()));
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  ASSERT_TRUE(network_service_test().is_connected());

  {
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    simple_cache->DoomEntry("abc",
                            base::BindLambdaForTesting([&](int32_t result) {
                              EXPECT_EQ(result, net::OK);
                              run_loop.Quit();
                            }));
    run_loop.Run();
  }
  entry.reset();
  Detach(std::move(simple_cache));
  ASSERT_TRUE(network_service_test().is_connected());

  simple_cache = CreateSimpleCache();
  ASSERT_TRUE(simple_cache.is_bound());
  ASSERT_TRUE(network_service_test().is_connected());

  ASSERT_TRUE(simple_cache.is_bound());
  entry = OpenEntry(simple_cache.get(), "abc");

  ASSERT_FALSE(entry.is_bound());
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, DoomEntryWithoutOpening) {
  mojo::Remote<SimpleCache> simple_cache = CreateSimpleCache();

  ASSERT_TRUE(simple_cache.is_bound());
  {
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    simple_cache->DoomEntry("abc",
                            base::BindLambdaForTesting([&](int32_t result) {
                              EXPECT_EQ(result, net::OK);
                              run_loop.Quit();
                            }));
    run_loop.Run();
  }
  ASSERT_TRUE(network_service_test().is_connected());

  Detach(std::move(simple_cache));
  ASSERT_TRUE(network_service_test().is_connected());
}

// TODO(crbug.com/40881636): Re-enable this test
IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest,
                       DISABLED_EnumerateEntries) {
  const std::string kKey1 = "abc";
  const std::string kKey2 = "def";
  const std::string kKey3 = "ghi";

  mojo::Remote<SimpleCache> simple_cache = CreateSimpleCache();
  ASSERT_TRUE(simple_cache.is_bound());

  {
    mojo::Remote<SimpleCacheEntryEnumerator> enumerator;
    simple_cache->EnumerateEntries(enumerator.BindNewPipeAndPassReceiver());

    auto result = OpenNextEntry(enumerator.get());
    ASSERT_TRUE(network_service_test().is_connected());
    DCHECK(result);

    ASSERT_EQ(result->error, net::ERR_FAILED);
    ASSERT_FALSE(result->entry);
  }
  ASSERT_TRUE(network_service_test().is_connected());

  mojo::Remote<SimpleCacheEntry> entry1 =
      CreateEntry(simple_cache.get(), kKey1);
  ASSERT_TRUE(entry1.is_bound());

  mojo::Remote<SimpleCacheEntry> entry2 =
      CreateEntry(simple_cache.get(), kKey2);
  ASSERT_TRUE(entry2.is_bound());

  mojo::Remote<SimpleCacheEntry> entry3 =
      CreateEntry(simple_cache.get(), kKey3);
  ASSERT_TRUE(entry3.is_bound());

  Close(std::move(entry1));
  ASSERT_TRUE(network_service_test().is_connected());

  Close(std::move(entry2));
  ASSERT_TRUE(network_service_test().is_connected());

  Close(std::move(entry3));
  ASSERT_TRUE(network_service_test().is_connected());

  Detach(std::move(simple_cache));
  ASSERT_TRUE(network_service_test().is_connected());

  simple_cache = CreateSimpleCache();
  ASSERT_TRUE(simple_cache.is_bound());

  {
    mojo::Remote<SimpleCacheEntryEnumerator> enumerator;
    simple_cache->EnumerateEntries(enumerator.BindNewPipeAndPassReceiver());

    std::vector<std::string> keys;
    while (true) {
      auto result = OpenNextEntry(enumerator.get());
      ASSERT_TRUE(network_service_test().is_connected());
      DCHECK(result);

      if (result->error == net::ERR_FAILED) {
        EXPECT_FALSE(result->entry);
        break;
      }
      ASSERT_EQ(result->error, net::OK);
      ASSERT_TRUE(result->entry);
      keys.push_back(result->key);
    }
    std::sort(keys.begin(), keys.end());
    EXPECT_EQ(keys, (std::vector<std::string>{kKey1, kKey2, kKey3}));
  }
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, DoomAllEntries) {
  const std::string kKey1 = "abc";
  const std::string kKey2 = "def";
  const std::string kKey3 = "ghi";

  mojo::Remote<SimpleCache> simple_cache = CreateSimpleCache();
  ASSERT_TRUE(simple_cache.is_bound());

  {
    mojo::Remote<SimpleCacheEntryEnumerator> enumerator;
    simple_cache->EnumerateEntries(enumerator.BindNewPipeAndPassReceiver());

    auto result = OpenNextEntry(enumerator.get());
    ASSERT_TRUE(network_service_test().is_connected());
    ASSERT_TRUE(result);

    ASSERT_EQ(result->error, net::ERR_FAILED);
    ASSERT_FALSE(result->entry);
  }
  ASSERT_TRUE(network_service_test().is_connected());

  mojo::Remote<SimpleCacheEntry> entry1 =
      CreateEntry(simple_cache.get(), kKey1);
  ASSERT_TRUE(entry1.is_bound());

  mojo::Remote<SimpleCacheEntry> entry2 =
      CreateEntry(simple_cache.get(), kKey2);
  ASSERT_TRUE(entry2.is_bound());

  mojo::Remote<SimpleCacheEntry> entry3 =
      CreateEntry(simple_cache.get(), kKey3);
  ASSERT_TRUE(entry3.is_bound());

  {
    base::RunLoop run_loop;
    network_service_test().set_disconnect_handler(run_loop.QuitClosure());
    simple_cache->DoomAllEntries(
        base::BindLambdaForTesting([&](int32_t result) {
          EXPECT_EQ(result, net::OK);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  ASSERT_TRUE(network_service_test().is_connected());

  entry1.reset();
  entry2.reset();
  entry3.reset();

  Detach(std::move(simple_cache));
  ASSERT_TRUE(network_service_test().is_connected());

  simple_cache = CreateSimpleCache();
  ASSERT_TRUE(simple_cache.is_bound());

  {
    mojo::Remote<SimpleCacheEntryEnumerator> enumerator;
    simple_cache->EnumerateEntries(enumerator.BindNewPipeAndPassReceiver());

    std::vector<std::string> keys;
    auto result = OpenNextEntry(enumerator.get());
    EXPECT_EQ(result->error, net::ERR_FAILED);
    EXPECT_FALSE(result->entry);
  }
}

}  // namespace
}  // namespace content
