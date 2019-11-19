// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/public/web_client.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/simple_url_loader.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

class URLLoaderTest : public WebTest {
 protected:
  URLLoaderTest() : WebTest(WebTaskEnvironment::Options::IO_MAINLOOP) {}

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
  // allowed by TestBrowserState. If BrowserState implementation does not
  // permit to use this header in |cors_exempt_headers| explicitly, the request
  // fails with net::ERR_INVALID_ARGUMENT.
  request->cors_exempt_headers.SetHeader(
      TestBrowserState::kCorsExemptTestHeaderName, "Test");
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
