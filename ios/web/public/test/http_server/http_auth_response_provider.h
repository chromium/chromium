// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTTP_AUTH_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTTP_AUTH_RESPONSE_PROVIDER_H_

#include <string>

#import "ios/web/public/test/http_server/html_response_provider.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
class HttpRequestHeaders;
}  // namespace net

namespace web {

// Serves a page which requires Basic HTTP Authentication.
class HttpAuthResponseProvider : public HtmlResponseProvider {
 public:
  // Constructs provider which will respond to the given `url` and will use the
  // given authenticaion `realm`. `username` and `password` are credentials
  // required for successful authentication. Use different realms and
  // username/password combination for different tests to prevent credentials
  // caching.
  HttpAuthResponseProvider(const GURL& url,
                           const std::string& realm,
                           const std::string& username,
                           const std::string& password);
  ~HttpAuthResponseProvider() override;

  // HtmlResponseProvider overrides:
  bool CanHandleRequest(const Request& request) override;
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override;

  // Text returned in response if authentication was successful.
  static std::string page_text() { return "authenticated"; }

 private:
  // Checks if authorization header has valid credintials:
  // https://tools.ietf.org/html/rfc1945#section-10.2
  bool HeadersHaveValidCredentials(const net::HttpRequestHeaders& headers);

  // URL this provider responds to.
  GURL url_;
  // HTTP Authentication realm.
  std::string realm_;
  // Correct username to pass authentication
  std::string username_;
  // Correct password to pass authentication
  std::string password_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTTP_AUTH_RESPONSE_PROVIDER_H_
