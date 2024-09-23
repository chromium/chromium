// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/lock_screen/lock_screen_service_impl.h"
#include "content/browser/lock_screen/lock_screen_storage_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/lock_screen/lock_screen.mojom.h"

namespace content {

class LockScreenServiceImplBrowserTest : public ContentBrowserTest {
 public:
  LockScreenServiceImplBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kWebLockScreenApi);
  }

  LockScreenServiceImplBrowserTest(const LockScreenServiceImplBrowserTest&) =
      delete;
  LockScreenServiceImplBrowserTest& operator=(
      const LockScreenServiceImplBrowserTest&) = delete;

  ~LockScreenServiceImplBrowserTest() override = default;

  base::FilePath GetStoragePath() {
    base::FilePath path;
    EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &path));
    path = path.AppendASCII("web_lock_screen_api_data");
    path = path.AppendASCII("test-user");
    return path;
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    EXPECT_TRUE(base::DeletePathRecursively(GetStoragePath()));
    LockScreenStorageImpl::GetInstance()->InitForTesting(
        shell()->web_contents()->GetBrowserContext(), GetStoragePath());
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL url = embedded_test_server()->GetURL("/lock_screen/simple.html");
    lock_screen_service_ = NavigateAndCreateService(url);
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
  }

  mojo::Remote<blink::mojom::LockScreenService> NavigateAndCreateService(
      const GURL& url) {
    Shell* shell = CreateBrowser();
    EXPECT_TRUE(NavigateToURL(shell, url));
    RenderFrameHost* rfh = shell->web_contents()->GetPrimaryMainFrame();
    mojo::Remote<blink::mojom::LockScreenService> service;
    LockScreenServiceImpl::Create(rfh, service.BindNewPipeAndPassReceiver());
    return service;
  }

  blink::mojom::LockScreenService* service() {
    return lock_screen_service_.get();
  }

  blink::mojom::LockScreenServiceStatus AwaitSetData(
      blink::mojom::LockScreenService* service,
      const std::string& key,
      const std::string& data) {
    base::RunLoop run_loop;
    blink::mojom::LockScreenServiceStatus result;
    service->SetData(key, data,
                     base::BindLambdaForTesting(
                         [&](blink::mojom::LockScreenServiceStatus status) {
                           result = status;
                           run_loop.Quit();
                         }));
    run_loop.Run();
    return result;
  }

  std::vector<std::string> AwaitGetKeys(
      blink::mojom::LockScreenService* service) {
    base::RunLoop run_loop;
    std::vector<std::string> result;
    service->GetKeys(
        base::BindLambdaForTesting([&](const std::vector<std::string>& keys) {
          result = keys;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<blink::mojom::LockScreenService> lock_screen_service_;
};

IN_PROC_BROWSER_TEST_F(LockScreenServiceImplBrowserTest,
                       CorrectDirectoryCreated) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath expected_dir = GetStoragePath();
  EXPECT_FALSE(base::PathExists(expected_dir));
  // TODO(crbug.com/40204655): Consider testing write failure case.
  ASSERT_EQ(blink::mojom::LockScreenServiceStatus::kSuccess,
            AwaitSetData(service(), "key1", "data1"));
  ASSERT_TRUE(base::PathExists(expected_dir));

  base::FileEnumerator e(expected_dir, false,
                         base::FileEnumerator::DIRECTORIES);
  std::vector<base::FilePath> directories;
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    directories.push_back(name);
  }
  ASSERT_EQ(1u, directories.size());
  EXPECT_EQ(64u, directories[0].BaseName().MaybeAsASCII().size());
}

IN_PROC_BROWSER_TEST_F(LockScreenServiceImplBrowserTest, SetDataOpaqueOrigin) {
  auto service = NavigateAndCreateService(GURL("about:blank"));
  ASSERT_EQ(blink::mojom::LockScreenServiceStatus::kNotAllowedFromContext,
            AwaitSetData(service.get(), "key1", "data1"));
  ASSERT_EQ(blink::mojom::LockScreenServiceStatus::kNotAllowedFromContext,
            AwaitSetData(service.get(), "key2", "data2"));

  std::vector<std::string> result = AwaitGetKeys(service.get());
  ASSERT_EQ(0u, result.size());
}

IN_PROC_BROWSER_TEST_F(LockScreenServiceImplBrowserTest, GetKeys) {
  std::vector<std::string> result = AwaitGetKeys(service());
  ASSERT_EQ(0u, result.size());

  AwaitSetData(service(), "key1", "data1");
  AwaitSetData(service(), "key2", "data2");
  result = AwaitGetKeys(service());
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("key1", result[0]);
  EXPECT_EQ("key2", result[1]);

  AwaitSetData(service(), "key2", "data3");
  result = AwaitGetKeys(service());
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("key1", result[0]);
  EXPECT_EQ("key2", result[1]);
}

IN_PROC_BROWSER_TEST_F(LockScreenServiceImplBrowserTest,
                       DataNotSharedBetweenDifferentOrigins) {
  GURL url_a =
      embedded_test_server()->GetURL("a.com", "/lock_screen/simple.html");
  GURL url_b =
      embedded_test_server()->GetURL("b.com", "/lock_screen/simple.html");
  auto service_a = NavigateAndCreateService(url_a);
  auto service_b = NavigateAndCreateService(url_b);

  AwaitSetData(service_a.get(), "key1", "a");
  AwaitSetData(service_a.get(), "key2", "a");
  std::vector<std::string> result = AwaitGetKeys(service_a.get());
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("key1", result[0]);
  EXPECT_EQ("key2", result[1]);

  AwaitSetData(service_b.get(), "key1", "b");
  AwaitSetData(service_b.get(), "key3", "b");
  result = AwaitGetKeys(service_b.get());
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("key1", result[0]);
  EXPECT_EQ("key3", result[1]);
}

IN_PROC_BROWSER_TEST_F(LockScreenServiceImplBrowserTest,
                       DataSharedBetweenSameOrigins) {
  GURL url_a = embedded_test_server()->GetURL("/lock_screen/simple.html");
  GURL url_b = embedded_test_server()->GetURL("/lock_screen/simple.html?abcd");
  auto service_a = NavigateAndCreateService(url_a);
  auto service_b = NavigateAndCreateService(url_b);

  AwaitSetData(service_a.get(), "key1", "a");
  AwaitSetData(service_a.get(), "key2", "a");
  std::vector<std::string> result = AwaitGetKeys(service_a.get());
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("key1", result[0]);
  EXPECT_EQ("key2", result[1]);

  result = AwaitGetKeys(service_b.get());
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("key1", result[0]);
  EXPECT_EQ("key2", result[1]);

  AwaitSetData(service_b.get(), "key1", "b");
  AwaitSetData(service_b.get(), "key3", "b");
  result = AwaitGetKeys(service_a.get());
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("key1", result[0]);
  EXPECT_EQ("key2", result[1]);
  EXPECT_EQ("key3", result[2]);
}

}  // namespace content