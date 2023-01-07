// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_RESPONSE_PROVIDER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace web {

// An abstract class for a provider that services a request and returns a
// EmbeddedTestServerResponse.
// Note: The ResponseProviders can be called from any arbitrary GCD thread.
class ResponseProvider {
 public:
  // A data structure that encapsulated all the fields of a request.
  struct Request {
    Request(const GURL& url,
            const std::string& method,
            const std::string& body,
            const net::HttpRequestHeaders& headers);
    Request(const Request& other);
    virtual ~Request();

    // The URL for the request.
    GURL url;
    // The HTTP method for the request such as "GET" or "POST".
    std::string method;
    // The body of the request.
    std::string body;
    // The HTTP headers for the request.
    net::HttpRequestHeaders headers;
  };

  // Returns true if the request is handled by the provider.
  virtual bool CanHandleRequest(const Request& request) = 0;

  // Returns the test_server::HttpResponse as a reply to the request. Will only
  // be called if the provider can handle the request.
  virtual std::unique_ptr<net::test_server::HttpResponse>
  GetEmbeddedTestServerResponse(const Request& request) = 0;

  // Gets default response headers with a text/html content type and a 200
  // response code.
  static scoped_refptr<net::HttpResponseHeaders> GetDefaultResponseHeaders();
  // Gets a map of response headers with a text/html content type, a 200
  // response code and Set-Cookie in headers.
  static std::map<GURL, scoped_refptr<net::HttpResponseHeaders>>
  GetDefaultResponseHeaders(
      const std::map<GURL, std::pair<std::string, std::string>>& responses);
  // Gets configurable response headers with a provided content type and a
  // 200 response code.
  static scoped_refptr<net::HttpResponseHeaders> GetResponseHeaders(
      const std::string& content_type);
  // Gets configurable response headers with a provided content type and
  // response code.
  static scoped_refptr<net::HttpResponseHeaders> GetResponseHeaders(
      const std::string& content_type,
      net::HttpStatusCode response_code);
  // Gets configurable response based on `http_status` headers for redirecting
  // to `destination`.
  static scoped_refptr<net::HttpResponseHeaders> GetRedirectResponseHeaders(
      const std::string& destination,
      const net::HttpStatusCode& http_status);

  ResponseProvider();

  ResponseProvider(const ResponseProvider&) = delete;
  ResponseProvider& operator=(const ResponseProvider&) = delete;

  virtual ~ResponseProvider() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_RESPONSE_PROVIDER_H_
