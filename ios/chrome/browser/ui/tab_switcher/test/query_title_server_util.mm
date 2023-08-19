// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/test/query_title_server_util.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"

namespace {

// net::EmbeddedTestServer handler that responds with the request's query as the
// title and body.
std::unique_ptr<net::test_server::HttpResponse> HandleQueryTitle(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->set_content("<html><head><title>" + request.GetURL().query() +
                             "</title></head><body>" +
                             request.GetURL().query() + "</body></html>");
  return std::move(http_response);
}

}  // namespace

void RegisterQueryTitleHandler(
    net::test_server::EmbeddedTestServer* test_server) {
  test_server->RegisterDefaultHandler(base::BindRepeating(
      net::test_server::HandlePrefixedRequest, "/querytitle",
      base::BindRepeating(&HandleQueryTitle)));
}

GURL GetQueryTitleURL(net::test_server::EmbeddedTestServer* test_server,
                      NSString* title) {
  return test_server->GetURL("/querytitle?" + base::SysNSStringToUTF8(title));
}
