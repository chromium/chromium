// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTML_RESPONSE_PROVIDER_IMPL_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTML_RESPONSE_PROVIDER_IMPL_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#import "ios/web/public/test/http_server/data_response_provider.h"
#import "ios/web/public/test/http_server/response_provider.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

// This class encapsulates the logic needed to map a request URL to a response.
// The mapping -- between URL to response -- is maintained internally, use
// `CanHandleRequest` to check if a request can be handled and use
// `GetResponseHeadersAndBody` to actually handle the request.
class HtmlResponseProviderImpl {
 public:
  // Encapsulates the body and headers that make up a response.
  struct Response {
    Response(const std::string& body,
             const scoped_refptr<net::HttpResponseHeaders>& headers);
    Response(const Response&);
    Response();
    ~Response();

    std::string body;
    scoped_refptr<net::HttpResponseHeaders> headers;
  };
  // Constructs an HtmlResponseProviderImpl that does not respond to any
  // request.
  HtmlResponseProviderImpl();
  // Constructs an HtmlResponseProviderImpl that generates a simple string
  // response to a URL based on the mapping present in `responses`.
  explicit HtmlResponseProviderImpl(
      const std::map<GURL, std::string>& responses);
  // Constructs an HtmlResponseProvider that generates a simple string response
  // to a URL with set cookie in the headers based on the mapping present in
  // `responses`.
  explicit HtmlResponseProviderImpl(
      const std::map<GURL, std::pair<std::string, std::string>>& responses);
  // Constructs an HtmlResponseProviderImpl that generates a response to a URL
  // based on the mapping present in `responses`.
  explicit HtmlResponseProviderImpl(const std::map<GURL, Response>& responses);

  virtual ~HtmlResponseProviderImpl();

  // Creates a response based on a redirect to `destination_url`.
  static HtmlResponseProviderImpl::Response GetRedirectResponse(
      const GURL& destination_url,
      const net::HttpStatusCode& http_status);

  // Creates a response with `net::HTTP_OK`.
  static HtmlResponseProviderImpl::Response GetSimpleResponse(
      const std::string& body);

  // Returns true if the request can be handled.
  bool CanHandleRequest(const web::ResponseProvider::Request& request);
  // Returns the headers and the response body.
  void GetResponseHeadersAndBody(
      const web::ResponseProvider::Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body);

 private:
  const std::map<GURL, Response> responses_;
};

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTML_RESPONSE_PROVIDER_IMPL_H_
