// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/apple/http_response_headers_util.h"

#import <Foundation/Foundation.h>

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

// Returns true if all the information in |http_response| is present in
// |http_response_headers|.
bool AreHeadersEqual(NSHTTPURLResponse* http_response,
                     HttpResponseHeaders* http_response_headers) {
  if (!http_response || !http_response_headers)
    return false;
  if (http_response.statusCode != http_response_headers->response_code())
    return false;
  __block bool all_headers_present = true;
  [http_response.allHeaderFields
      enumerateKeysAndObjectsUsingBlock:^(NSString* header_name,
                                          NSString* header_value, BOOL* stop) {
        std::string value;
        http_response_headers->GetNormalizedHeader(
            base::SysNSStringToUTF8(header_name), &value);
        all_headers_present = (value == base::SysNSStringToUTF8(header_value));
        *stop = !all_headers_present;
      }];
  return all_headers_present;
}

using HttpResponseHeadersUtilTest = PlatformTest;

// Tests that HttpResponseHeaders created from NSHTTPURLResponses successfully
// copy over the status code and the header names and values.
TEST_F(HttpResponseHeadersUtilTest, CreateHeadersFromNSHTTPURLResponse) {
  NSHTTPURLResponse* http_response =
      [[NSHTTPURLResponse alloc] initWithURL:[NSURL URLWithString:@"test.com"]
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:@{
                                  @"headerName1" : @"headerValue1",
                                  @"headerName2" : @"headerValue2",
                                  @"headerName3" : @"headerValue3",
                                }];
  scoped_refptr<HttpResponseHeaders> http_response_headers =
      CreateHeadersFromNSHTTPURLResponse(http_response);
  EXPECT_TRUE(AreHeadersEqual(http_response, http_response_headers.get()));
}

}  // namespace net.
