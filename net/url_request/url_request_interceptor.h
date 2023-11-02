// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_INTERCEPTOR_H_
#define NET_URL_REQUEST_URL_REQUEST_INTERCEPTOR_H_

#include <memory>

#include "net/base/net_export.h"

namespace net {

class URLRequest;
class URLRequestJob;

// In tests, URLRequestFilter lets URLRequestInterceptors create URLRequestJobs
// to handle URLRequests before they're handed off to the ProtocolHandler for
// the request's scheme.
//
// TODO(mmenke):  Only include this file in test targets. Also consider using
// callbacks instead, or even removing URLRequestFilter.
class NET_EXPORT URLRequestInterceptor {
 public:
  URLRequestInterceptor();

  URLRequestInterceptor(const URLRequestInterceptor&) = delete;
  URLRequestInterceptor& operator=(const URLRequestInterceptor&) = delete;

  virtual ~URLRequestInterceptor();

  // Returns a URLRequestJob to handle |request|, if the interceptor wants to
  // take over the handling the request instead of the default ProtocolHandler.
  // Otherwise, returns nullptr.
  virtual std::unique_ptr<URLRequestJob> MaybeInterceptRequest(
      URLRequest* request) const = 0;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_INTERCEPTOR_H_
