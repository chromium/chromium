// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/http_protocol_logging.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/net/url_scheme_util.h"

namespace {
const unsigned int kMaxUrlLength = 100;
}

namespace net {

void LogNSURLRequest(NSURLRequest* request) {
  DVLOG_IF(2, UrlHasDataScheme([request URL]) &&
              [[[request URL] absoluteString] length] > kMaxUrlLength)
      << "Request (data scheme) "
      << base::SysNSStringToUTF8(
             [[[request URL] absoluteString] substringToIndex:kMaxUrlLength])
      << " ...";

  DVLOG_IF(2, ![[[request URL] scheme] isEqualToString:@"data"] ||
              [[[request URL] absoluteString] length] <= kMaxUrlLength)
      << "Request "
      << base::SysNSStringToUTF8([[request URL] description]);

  DVLOG_IF(2, ![[request HTTPMethod] isEqualToString:@"GET"])
      << base::SysNSStringToUTF8([request HTTPMethod]);

  DVLOG_IF(2, [request allHTTPHeaderFields])
      << base::SysNSStringToUTF8([[request allHTTPHeaderFields] description]);

  DVLOG_IF(2, [request networkServiceType])
      << "Service type: " << [request networkServiceType];

  DVLOG_IF(2, ![request HTTPShouldHandleCookies]) << "No cookies";

  DVLOG_IF(2, [request HTTPShouldUsePipelining]) << "Pipelining allowed";
}

void LogNSURLResponse(NSURLResponse* response) {
  DVLOG_IF(2, UrlHasDataScheme([response URL]) &&
              [[[response URL] absoluteString] length] > kMaxUrlLength)
      << "Response (data scheme) "
      << base::SysNSStringToUTF8(
             [[[response URL] absoluteString] substringToIndex:kMaxUrlLength]);

  DVLOG_IF(2, !UrlHasDataScheme([response URL]) ||
              [[[response URL] absoluteString] length] <= kMaxUrlLength)
      << "Response "
      << base::SysNSStringToUTF8([[response URL] description]);

  DVLOG_IF(2, [response isKindOfClass:[NSHTTPURLResponse class]] &&
              [(NSHTTPURLResponse*)response allHeaderFields])
      << base::SysNSStringToUTF8(
          [[(NSHTTPURLResponse*)response allHeaderFields] description]);

  DVLOG_IF(2, [response expectedContentLength])
      << "Length: " << [response expectedContentLength];

  DVLOG_IF(2, [response MIMEType])
      << "MIMEType: " << base::SysNSStringToUTF8([response MIMEType]);

  DVLOG_IF(2, [response isKindOfClass:[NSHTTPURLResponse class]])
      << "Response code: " << [(NSHTTPURLResponse*)response statusCode];

  DVLOG_IF(2, [response textEncodingName])
      << "Text encoding: "
      << base::SysNSStringToUTF8([response textEncodingName]);
}

}  // namespace http_protocol_logging
