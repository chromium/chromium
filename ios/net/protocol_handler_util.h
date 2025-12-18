// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_PROTOCOL_HANDLER_UTIL_H_
#define IOS_NET_PROTOCOL_HANDLER_UTIL_H_

#import <Foundation/Foundation.h>

namespace net {
class URLRequest;
}  // namespace net

namespace net {

// The error domain for network NSErrors.
extern NSString* const kNSErrorDomain;

// Builds a NSURLResponse from the response data in |request|.
NSURLResponse* GetNSURLResponseForRequest(URLRequest* request);

// Copy HTTP headers from |in_request| to |out_request|.
void CopyHttpHeaders(NSURLRequest* in_request, URLRequest* out_request);

}  // namespace net

#endif  // IOS_NET_PROTOCOL_HANDLER_UTIL_H_
