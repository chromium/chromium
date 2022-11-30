// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTML_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTML_RESPONSE_PROVIDER_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#import "ios/web/public/test/http_server/data_response_provider.h"
#import "ios/web/public/test/http_server/html_response_provider_impl.h"
#import "ios/web/public/test/http_server/response_provider.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}

// A web::DataResponseProvider that maps URLs to requests.
class HtmlResponseProvider : public web::DataResponseProvider {
 public:
  // Constructs an HtmlResponseProvider that does not respond to any request.
  HtmlResponseProvider();
  // Constructs an HtmlResponseProvider that generates a simple string response
  // to a URL based on the mapping present in `responses`.
  explicit HtmlResponseProvider(const std::map<GURL, std::string>& responses);
  // Constructs an HtmlResponseProvider that generates a simple string response
  // to a URL with a Set-Cookie entry in the headers based on the mapping
  // present in `responses`.
  explicit HtmlResponseProvider(
      const std::map<GURL, std::pair<std::string, std::string>>& responses);
  // Constructs an HtmlResponseProvider that generates a response to a URL based
  // on the mapping present in `responses`.
  explicit HtmlResponseProvider(
      const std::map<GURL, HtmlResponseProviderImpl::Response>& responses);

  ~HtmlResponseProvider() override;

  // web::ResponseProvider implementation.
  bool CanHandleRequest(const Request& request) override;
  // web::DataResponseProvider implementation.
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override;

 private:
  std::unique_ptr<HtmlResponseProviderImpl> response_provider_impl_;
};

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTML_RESPONSE_PROVIDER_H_
