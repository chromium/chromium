// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_DATA_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_DATA_RESPONSE_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#import "ios/web/public/test/http_server/response_provider.h"
#include "net/http/http_response_headers.h"

namespace web {

// An abstract ResponseProvider that returns a test_server::HttpResponse for a
// request. This class encapsulates the logic to convert the response headers
// and body received from `GetResponseHeadersAndBody` into a
// net::test_server::HttpResponse.
class DataResponseProvider : public ResponseProvider {
 public:
  // ResponseProvider implementation.
  std::unique_ptr<net::test_server::HttpResponse> GetEmbeddedTestServerResponse(
      const Request& request) final;

  // Returns the headers and the response body. Will only be called if the
  // provider can handle the request.
  // Note: This should actually be under protected but since this is used by
  // an adapter in order to work with the old MockHttpServer it is under
  // public.
  virtual void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_DATA_RESPONSE_PROVIDER_H_
