// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_INTERCEPTOR_H_
#define NET_URL_REQUEST_URL_REQUEST_INTERCEPTOR_H_

#include "base/macros.h"
#include "net/base/net_export.h"

namespace net {

class URLRequest;
class URLRequestJob;
class NetworkDelegate;

// In tests, URLRequestFilter lets URLRequestInterceptors create URLRequestJobs
// to handle URLRequests before they're handed off to the ProtocolHandler for
// the request's scheme.
//
// TODO(mmenke):  Only include this file in test targets. Also consider using
// callbacks instead, or even removing URLRequestFilter.
class NET_EXPORT URLRequestInterceptor {
 public:
  URLRequestInterceptor();
  virtual ~URLRequestInterceptor();

  // Returns a URLRequestJob to handle |request|, if the interceptor wants to
  // take over the handling the request instead of the default ProtocolHandler.
  // Otherwise, returns NULL.
  virtual URLRequestJob* MaybeInterceptRequest(
      URLRequest* request, NetworkDelegate* network_delegate) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLRequestInterceptor);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_INTERCEPTOR_H_
