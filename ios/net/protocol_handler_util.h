// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_PROTOCOL_HANDLER_UTIL_H_
#define IOS_NET_PROTOCOL_HANDLER_UTIL_H_

#import <Foundation/Foundation.h>

#include "base/feature_list.h"

namespace base {
class Time;
}  // namespace base

namespace net {
class URLRequest;
}  // namespace net

namespace net {

// Feature flag to control whether to use NSURLErrorFailingURLErrorKey
// instead of NSURLErrorFailingURLStringErrorKey (deprecated) in NSError
// userInfo.
BASE_DECLARE_FEATURE(kUseNSURLErrorFailingURLErrorKey);

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

// Returns the failing URL string from the error's userInfo, checking both
// the string key (deprecated) and the url key.
// See crbug.com/468893789 for details. This is a short-term workaround to
// fix the unexpected removal of the deprecated error key
// NSURLErrorFailingURLStringErrorKey
NSString* GetFailingURLStringFromError(NSError* error);

// Builds a NSURLResponse from the response data in |request|.
NSURLResponse* GetNSURLResponseForRequest(URLRequest* request);

// Copy HTTP headers from |in_request| to |out_request|.
void CopyHttpHeaders(NSURLRequest* in_request, URLRequest* out_request);

}  // namespace net

#endif  // IOS_NET_PROTOCOL_HANDLER_UTIL_H_
