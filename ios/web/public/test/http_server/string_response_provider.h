// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_STRING_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_STRING_RESPONSE_PROVIDER_H_

#include <string>

#import "ios/web/public/test/http_server/data_response_provider.h"

namespace web {

// A response provider that returns a  string for all requests. Used for testing
// purposes.
class StringResponseProvider : public web::DataResponseProvider {
 public:
  explicit StringResponseProvider(const std::string& response_body);

  StringResponseProvider(const StringResponseProvider&) = delete;
  StringResponseProvider& operator=(const StringResponseProvider&) = delete;

  // web::DataResponseProvider methods.
  bool CanHandleRequest(const Request& request) override;
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override;

 private:
  // The string that is returned in the response body.
  std::string response_body_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_STRING_RESPONSE_PROVIDER_H_
