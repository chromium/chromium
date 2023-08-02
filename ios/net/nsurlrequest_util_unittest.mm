// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/nsurlrequest_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

using NSURLRequestUtilTest = PlatformTest;

// Tests that FormatUrlRequestForLogging outputs the string in the form:
// request:<url> request.mainDocURL:<mainDocumentURL>.
TEST_F(NSURLRequestUtilTest, FormatUrlRequestForLogging) {
  NSMutableURLRequest* request = [[NSMutableURLRequest alloc] init];
  request.URL = [NSURL URLWithString:@"http://www.google.com"];
  request.mainDocumentURL = [NSURL URLWithString:@"http://www.google1.com"];
  std::string actualString, expectedString;

  actualString = net::FormatUrlRequestForLogging(request);
  expectedString = "request: http://www.google.com"
                   " request.mainDocURL: http://www.google1.com";
  EXPECT_EQ(expectedString, actualString);

  request.URL = nil;
  request.mainDocumentURL = [NSURL URLWithString:@"http://www.google1.com"];
  actualString = net::FormatUrlRequestForLogging(request);
  expectedString = "request: [nil] request.mainDocURL: http://www.google1.com";
  EXPECT_EQ(expectedString, actualString);

  request.URL = [NSURL URLWithString:@"http://www.google.com"];
  request.mainDocumentURL = nil;
  actualString = net::FormatUrlRequestForLogging(request);
  expectedString = "request: http://www.google.com request.mainDocURL: [nil]";
  EXPECT_EQ(expectedString, actualString);

  request.URL = nil;
  request.mainDocumentURL = nil;
  actualString = net::FormatUrlRequestForLogging(request);
  expectedString = "request: [nil] request.mainDocURL: [nil]";
  EXPECT_EQ(expectedString, actualString);
}

}  // namespace
