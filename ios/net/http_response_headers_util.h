// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_HTTP_RESPONSE_HEADERS_UTIL_H_
#define IOS_NET_HTTP_RESPONSE_HEADERS_UTIL_H_

#include "net/http/http_response_headers.h"

@class NSHTTPURLResponse;

namespace net {

// Placeholder status description since the actual text from the headers is not
// available.
extern const char kDummyHttpStatusDescription[];

// Constructs a net::HttpResponseHeaders from |response|.
// Note: The HTTP version and the status code description are not accessible
// from NSHTTPURLResponse, so HTTP/1.0 and kDummyHttpStatusDescription will
// be used in the status line instead.
scoped_refptr<HttpResponseHeaders> CreateHeadersFromNSHTTPURLResponse(
    NSHTTPURLResponse* response);

}  // namespace net

#endif  // IOS_NET_HTTP_RESPONSE_HEADERS_UTIL_H_
