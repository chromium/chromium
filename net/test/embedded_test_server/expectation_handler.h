// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_EXPECTATION_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_EXPECTATION_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/synchronization/lock.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net::test_server {

// ExpectationHandler provides a simplified way to handle HTTP requests in
// tests. It allows pre-registering responses for specific URLs and
// automatically sends them when matching requests are received.
//
// Basic usage:
//   EmbeddedTestServer server;
//   ExpectationHandler handler(&server);
//
//   // Start the server
//   ASSERT_TRUE(server.Start());
//
//   // Register a response for a specific URL Path
//   handler.OnRequest("/test.html")
//       .RespondWith("text/html", "<html>Test</html>");
//
//   shell()->web_contents()->GetController().LoadURLWithParams(
//     NavigationController::LoadURLParams(server.GetURL("/test.html")));
//
//   WaitForLoadStop(shell()->web_contents());
//
class ExpectationHandler {
 public:
  // Forward declaration of the ResponseBuilder class
  class ResponseBuilder;

  explicit ExpectationHandler(EmbeddedTestServer* embedded_test_server);
  ~ExpectationHandler();

  // Registers a response for a URL Path and returns a ResponseBuilder
  // for configuring the response.
  ResponseBuilder OnRequest(std::string_view path, bool is_prefix = false);

  // Handles an HTTP request and returns a response.
  // This is called by the EmbeddedTestServer when a request is received.
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

 private:
  // Stores the configuration for a URL response
  struct UrlResponseConfig;
  // Helper method to apply a configuration and create a response
  std::unique_ptr<HttpResponse> ApplyConfig(const HttpRequest& request,
                                            raw_ptr<UrlResponseConfig> config);

  std::map<std::string_view, std::unique_ptr<UrlResponseConfig>> url_responses_
      GUARDED_BY(lock_);
  raw_ref<EmbeddedTestServer> embedded_test_server_;
  base::Lock lock_;
};

// ResponseBuilder provides a fluent interface for configuring responses.
// It is created by ExpectationHandler::OnRequest() and allows chaining
// method calls to configure the response.
class ExpectationHandler::ResponseBuilder {
 public:
  ResponseBuilder(ExpectationHandler& handler,
                  std::string_view path,
                  bool is_prefix);

  ~ResponseBuilder();

  // Sets the content type and content for the HTTP response. The content_type
  // parameter specifies the MIME type (like "text/html") and content contains
  // the response body. Returns a reference to this ResponseBuilder to allow
  // method chaining.
  ResponseBuilder& RespondWith(
      std::string_view content_type = std::string_view("text/html"),
      std::string_view content = "");

  ResponseBuilder& RespondWith(
      const HttpStatusCode status_code,
      std::string_view content_type = std::string_view("text/html"),
      std::string_view content = "");

  // Associates a TestFuture with this response.
  // When a request for this URL is received, the TestFuture will be
  // fulfilled with the HttpRequest object.
  ResponseBuilder& SetValue(base::test::TestFuture<HttpRequest>& future);

 private:
  raw_ref<ExpectationHandler> handler_;
  std::string_view path_;
  raw_ptr<UrlResponseConfig> config_;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_EXPECTATION_HANDLER_H_
