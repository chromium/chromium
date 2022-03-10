// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/net/http_cache_backend_file_operations_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
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

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    content::GetNetworkService()->BindTestInterface(
        network_service_test_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<network::mojom::NetworkServiceTest>& network_service_test() {
    return network_service_test_;
  }

  const base::FilePath& GetTempDirPath() const { return temp_dir_.GetPath(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test_;
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
  IgnoreNetworkServiceCrashes();
  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  network_service_test()->CreateSimpleCache(
      std::move(factory_remote), path,
      base::BindOnce([](mojo::PendingRemote<SimpleCache> cache) {
        ADD_FAILURE() << "NOTREACHED";
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest,
                       CreateSimpleCacheWithParentDirectory) {
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
  IgnoreNetworkServiceCrashes();
  network_service_test().set_disconnect_handler(run_loop.QuitClosure());
  network_service_test()->CreateSimpleCache(
      std::move(factory_remote), path,
      base::BindOnce([](mojo::PendingRemote<SimpleCache> cache) {
        ADD_FAILURE() << "NOTREACHED";
      }));
  run_loop.Run();
}

}  // namespace
}  // namespace content
