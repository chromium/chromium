// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_HTTP_PROTOCOL_LOGGING_H_
#define IOS_NET_HTTP_PROTOCOL_LOGGING_H_

@class NSURLRequest;
@class NSURLResponse;

namespace net {

// Logs the request in DVLOG(2).
void LogNSURLRequest(NSURLRequest* request);

// Logs the response headers in DVLOG(2).
void LogNSURLResponse(NSURLResponse* response);

}  // namespace http_protocol_logging

#endif  // IOS_NET_HTTP_PROTOCOL_LOGGING_H_
