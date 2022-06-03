// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/http_response_headers_util.h"

#import <Foundation/Foundation.h>

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"
#include "net/http/http_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// String format used to create the http status line from the status code and
// its localized description.
NSString* const kHttpStatusLineFormat = @"HTTP %ld %s";
}

namespace net {

const char kDummyHttpStatusDescription[] = "DummyStatusDescription";

scoped_refptr<HttpResponseHeaders> CreateHeadersFromNSHTTPURLResponse(
    NSHTTPURLResponse* response) {
  DCHECK(response);
  // Create the status line and initialize the headers.
  NSInteger status_code = response.statusCode;
  std::string status_line = base::SysNSStringToUTF8([NSString
      stringWithFormat:kHttpStatusLineFormat, static_cast<long>(status_code),
                       kDummyHttpStatusDescription]);
  scoped_refptr<HttpResponseHeaders> http_headers(
      new HttpResponseHeaders(status_line));
  // Iterate through |response|'s headers and add them to |http_headers|.
  [response.allHeaderFields
      enumerateKeysAndObjectsUsingBlock:^(NSString* name,
                                          NSString* value, BOOL*) {
        std::string header_name = base::SysNSStringToUTF8(name);
        std::string header_value = base::SysNSStringToUTF8(value);
        if (HttpUtil::IsValidHeaderName(header_name) &&
            HttpUtil::IsValidHeaderValue(header_value)) {
          http_headers->AddHeader(header_name, header_value);
        }
      }];
  return http_headers;
}

}  // namespae net
