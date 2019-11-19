// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/url_fetcher_block_adapter.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Test fixture for URLFetcherBlockAdapter.
class URLFetcherBlockAdapterTest : public PlatformTest {
 protected:
  URLFetcherBlockAdapterTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  // Required for base::MessageLoopCurrent::Get().
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests that URLFetcherBlockAdapter calls its completion handler with the
// appropriate data for a text resource.
TEST_F(URLFetcherBlockAdapterTest, FetchTextResource) {
  GURL test_url("http://test");
  std::string response("<html><body>Hello World!</body></html>");
  NSData* expected_data =
      [NSData dataWithBytes:response.c_str() length:response.size()];
  web::URLFetcherBlockAdapterCompletion completion_handler =
      ^(NSData* data, web::URLFetcherBlockAdapter* fetcher) {
        EXPECT_NSEQ(expected_data, data);
      };

  network::TestURLLoaderFactory test_url_loader_factory;
  auto test_shared_url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);

  web::URLFetcherBlockAdapter web_ui_fetcher(
      test_url, test_shared_url_loader_factory, completion_handler);
  web_ui_fetcher.Start();
  test_url_loader_factory.AddResponse(test_url.spec(), response);
  base::RunLoop().RunUntilIdle();
}

// Tests that URLFetcherBlockAdapter calls its completion handler with the
// appropriate data for a png resource.
TEST_F(URLFetcherBlockAdapterTest, FetchPNGResource) {
  GURL test_url("http://test");
  base::FilePath favicon_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &favicon_path));
  favicon_path = favicon_path.AppendASCII("ios/web/test/data/testfavicon.png");
  NSData* expected_data = [NSData
      dataWithContentsOfFile:base::SysUTF8ToNSString(favicon_path.value())];
  web::URLFetcherBlockAdapterCompletion completion_handler =
      ^(NSData* data, URLFetcherBlockAdapter* fetcher) {
        EXPECT_NSEQ(expected_data, data);
      };

  network::TestURLLoaderFactory test_url_loader_factory;
  auto test_shared_url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);

  web::URLFetcherBlockAdapter web_ui_fetcher(
      test_url, test_shared_url_loader_factory, completion_handler);
  std::string response;
  EXPECT_TRUE(ReadFileToString(favicon_path, &response));
  web_ui_fetcher.Start();
  test_url_loader_factory.AddResponse(test_url.spec(), response);
  base::RunLoop().RunUntilIdle();
}

}  // namespace web
