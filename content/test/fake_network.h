// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_NETWORK_H_
#define CONTENT_TEST_FAKE_NETWORK_H_

#include "content/public/test/url_loader_interceptor.h"

namespace content {

// Mocks network activity. Meant to be used by URLLoaderInterceptor.
//
// 1. By default, it returns 200 OK with a simple body to any request:
//    If request path ends in ".js", the body is
//    "/*this body came from the network*/". Otherwise, the body is
//    "this body came from the network".
// 2. Call SetDefaultResponse() to change the default response.
// 3. Call SetResponse() to set the response for a specific url.
//
// Example:
//   FakeNetwork fake_network;
//   URLLoaderInterceptor interceptor(base::BindRepeating(
//       &FakeNetwork::HandleRequest, base::Unretained(&fake_network_)));
//
//   // |fake_network| will now handle any network request.
//
//   // Customize a response.
//   fake_network_.SetResponse(...);
class FakeNetwork {
 public:
  FakeNetwork();
  ~FakeNetwork();

  // Callback for URLLoaderInterceptor.
  bool HandleRequest(URLLoaderInterceptor::RequestParams* params);

  // Sets the default response.
  void SetDefaultResponse(const std::string& headers,
                          const std::string& body,
                          bool network_accessed,
                          net::Error error_code);

  // Sets the response for a specific url, to use instead of the default.
  void SetResponse(const GURL& url,
                   const std::string& headers,
                   const std::string& body,
                   bool network_accessed,
                   net::Error error_code);

 private:
  struct ResponseInfo;

  // Returns the ResponseInfo for the |url|, it follows the order:
  // 1. Returns the matching entry in |response_info_map_| if exists.
  // 2. Returns |user_defined_default_response_info_| if it's set.
  // 3. Returns default response info (defined inside this method).
  const ResponseInfo& FindResponseInfo(const GURL& url) const;

  // This is user-defined default response info, it overrides the default
  // response info.
  std::unique_ptr<ResponseInfo> user_defined_default_response_info_;

  // User-defined URL => ResponseInfo map.
  base::flat_map<GURL, ResponseInfo> response_info_map_;
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_NETWORK_H_
