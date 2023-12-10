// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <map>
#import <string>

#import "ios/chrome/browser/https_upgrades/model/https_upgrade_test_helper.h"

#import "base/functional/bind.h"
#import "base/strings/escape.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_app_interface.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/common/features.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const long kVeryLongTimeout = 100 * 3600 * 1000;

// net::EmbeddedTestServer handler that responds with simple text.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    int* counter,
    const net::test_server::HttpRequest& request) {
  // Ignore favicon requests.
  if (request.relative_url == "/favicon.ico") {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse);
  response->set_content_type("text/html");
  response->set_content("HTTP_RESPONSE");
  if (counter) {
    (*counter)++;
  }
  return std::move(response);
}

std::unique_ptr<net::test_server::HttpResponse> FakeHTTPSResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse);

  const GURL request_url = request.GetURL();
  const std::string destValue =
      base::UnescapeBinaryURLComponent(request_url.query_piece());
  // If the URL is in the form http://example.com/?redirect=url,
  // redirect the response to `url`.
  if (base::StartsWith(destValue, "redirect=")) {
    const std::string dest = destValue.substr(strlen("redirect="));
    response->set_code(net::HttpStatusCode::HTTP_MOVED_PERMANENTLY);
    response->AddCustomHeader("Location", dest);
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    response->set_content_type("text/html");
    response->set_content(base::StringPrintf(
        "<html><head></head><body>Redirecting to %s</body></html>",
        dest.c_str()));
    return response;
  }

  response->set_content_type("text/html");
  response->set_content("HTTPS_RESPONSE");
  return std::move(response);
}

std::unique_ptr<net::test_server::HttpResponse> FakeHungResponse(
    const net::test_server::HttpRequest& request) {
  return std::make_unique<net::test_server::HungResponse>();
}

}  // namespace

@implementation HttpsUpgradeTestCaseBase

- (net::EmbeddedTestServer*)goodHTTPSServer {
  if (!_goodHTTPSServer) {
    _goodHTTPSServer = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTP);
    _goodHTTPSServer->RegisterRequestHandler(
        base::BindRepeating(&FakeHTTPSResponse));
  }
  return _goodHTTPSServer.get();
}

- (net::EmbeddedTestServer*)badHTTPSServer {
  if (!_badHTTPSServer) {
    _badHTTPSServer = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    _badHTTPSServer->RegisterRequestHandler(
        base::BindRepeating(&StandardResponse, nullptr));
  }
  return _badHTTPSServer.get();
}

- (net::EmbeddedTestServer*)slowServer {
  if (!_slowServer) {
    _slowServer = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTP);
    _slowServer->RegisterRequestHandler(base::BindRepeating(&FakeHungResponse));
  }
  return _slowServer.get();
}

- (void)setUp {
  [super setUp];
  // Start test servers.
  _HTTPResponseCounter = 0;
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      base::BindRepeating(&StandardResponse, &_HTTPResponseCounter)));

  GREYAssertTrue(self.testServer->Start(), @"Test HTTP server failed to start");
  GREYAssertTrue(self.goodHTTPSServer->Start(),
                 @"Test good faux-HTTPS server failed to start.");
  GREYAssertTrue(self.badHTTPSServer->Start(),
                 @"Test bad HTTPS server failed to start.");
  GREYAssertTrue(self.slowServer->Start(),
                 @"Test slow server failed to start.");

  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");

  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:false];
  [HttpsUpgradeAppInterface setFallbackDelayForTesting:kVeryLongTimeout];
}

- (void)tearDown {
  // Release the histogram tester.
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");

  [super tearDown];
}

@end
