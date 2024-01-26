// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <memory>
#import <string>

#import "base/base_paths.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_timeouts.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/string_response_provider.h"
#import "ios/web/test/web_int_test.h"
#import "net/base/apple/url_conversions.h"
#import "net/http/http_response_headers.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

// Resonse body for requests sent to web::test::HttpServer.
const char kHelloWorld[] = "Hello World";

}  // namespave

using web::test::HttpServer;

// A test fixture for verifying the behavior of web::test::HttpServer.
class HttpServerTest : public web::WebIntTest {
 protected:
  void SetUp() override {
    web::WebIntTest::SetUp();

    std::unique_ptr<web::StringResponseProvider> provider(
        new web::StringResponseProvider(kHelloWorld));

    HttpServer& server = HttpServer::GetSharedInstance();
    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
    server.StartOrDie(test_data_dir.Append("."));
    server.AddResponseProvider(std::move(provider));
  }

  ~HttpServerTest() override {
    HttpServer& server = HttpServer::GetSharedInstance();
    if (server.IsRunning()) {
      server.Stop();
    }
  }
};

// Tests that a web::test::HttpServer can be started and can send and receive
// requests and response from `TestResponseProvider`.
TEST_F(HttpServerTest, StartAndInterfaceWithResponseProvider) {
  __block NSString* page_result;
  id completion_handler =
      ^(NSData* data, NSURLResponse* response, NSError* error) {
        page_result =
            [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
      };
  GURL url = HttpServer::GetSharedInstance().MakeUrl("http://whatever");
  NSURLSessionDataTask* data_task =
      [[NSURLSession sharedSession] dataTaskWithURL:net::NSURLWithGURL(url)
                                  completionHandler:completion_handler];
  [data_task resume];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool() {
        return page_result;
      }));
  EXPECT_NSEQ(page_result, base::SysUTF8ToNSString(kHelloWorld));
}
