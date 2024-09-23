// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/run_loop.h"
#import "base/test/bind.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/public/web_client.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/simple_url_loader.h"
#import "services/network/public/mojom/url_response_head.mojom.h"

namespace web {

class URLLoaderTest : public WebTest {
 protected:
  URLLoaderTest() : WebTest(WebTaskEnvironment::MainThreadType::IO) {}

 protected:
  net::EmbeddedTestServer server_;
};

// Tests that basic URLLoader wrapper works.
TEST_F(URLLoaderTest, Basic) {
  server_.AddDefaultHandlers(FILE_PATH_LITERAL(base::FilePath()));
  ASSERT_TRUE(server_.Start());

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = server_.GetURL("/echo");
  // Adds kCorsExemptHeaderName into the cors_exempt_headers, that use should be
  // allowed by FakeBrowserState. If BrowserState implementation does not
  // permit to use this header in `cors_exempt_headers` explicitly, the request
  // fails with net::ERR_INVALID_ARGUMENT.
  request->cors_exempt_headers.SetHeader(
      FakeBrowserState::kCorsExemptTestHeaderName, "Test");
  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);
  std::string result;
  base::RunLoop run_loop;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      GetBrowserState()->GetURLLoaderFactory(),
      base::BindLambdaForTesting(
          [&](std::unique_ptr<std::string> response_body) {
            if (response_body)
              result = *response_body;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_EQ(0, loader->NetError());
  EXPECT_EQ(result, "Echo");
  auto* response_info = loader->ResponseInfo();
  ASSERT_TRUE(!!response_info);
  ASSERT_TRUE(!!response_info->headers);
  EXPECT_EQ(200, response_info->headers->response_code());
}

}  // namespace web
