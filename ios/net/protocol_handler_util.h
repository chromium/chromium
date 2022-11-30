// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_PROTOCOL_HANDLER_UTIL_H_
#define IOS_NET_PROTOCOL_HANDLER_UTIL_H_

#import <Foundation/Foundation.h>

namespace base {
class Time;
}  // namespace base

namespace net {
class URLRequest;
}  // namespace net

namespace net {

// The error domain for network NSErrors.
extern NSString* const kNSErrorDomain;

// Creates a network NSError. |ns_error_code| is the iOS error code,
// |net_error_code| is the network error from net/base/net_error_list.h.
// |creation_time| is the time when the failing request was started and must be
// valid.
NSError* GetIOSError(NSInteger ns_error_code,
                     int net_error_code,
                     NSString* url,
                     const base::Time& creation_time);

// Builds a NSURLResponse from the response data in |request|.
NSURLResponse* GetNSURLResponseForRequest(URLRequest* request);

// Copy HTTP headers from |in_request| to |out_request|.
void CopyHttpHeaders(NSURLRequest* in_request, URLRequest* out_request);

}  // namespace protocol_handler_util

#endif  // IOS_NET_PROTOCOL_HANDLER_UTIL_H_
