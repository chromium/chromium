// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_view/error_translation_util.h"

#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#import "ios/net/protocol_handler_util.h"
#include "ios/web/test/test_url_constants.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Test fixture for error translation testing.
typedef PlatformTest ErrorTranslationUtilTest;

// Tests translation of CFNetwork error code to net error code.
TEST_F(ErrorTranslationUtilTest, ErrorCodeTranslation) {
  // kCFURLErrorUnknown -> net::ERR_FAILED
  int net_error_code = 0;
  EXPECT_TRUE(GetNetErrorFromIOSErrorCode(kCFURLErrorUnknown, &net_error_code,
                                          /*url=*/nil));
  EXPECT_EQ(net::ERR_FAILED, net_error_code);

  // kCFURLErrorUnsupportedURL -> net::ERR_INVALID_URL for app specific URLs.
  GURL web_ui_url(url::SchemeHostPort(kTestWebUIScheme, "foo", 0).Serialize());
  EXPECT_TRUE(GetNetErrorFromIOSErrorCode(kCFURLErrorUnsupportedURL,
                                          &net_error_code,
                                          net::NSURLWithGURL(web_ui_url)));
  EXPECT_EQ(net::ERR_INVALID_URL, net_error_code);

  // kCFURLErrorUnsupportedURL -> net::ERR_UNKNOWN_URL_SCHEME for app with
  // scheme that is neither supported by WebState nor app-specific scheme.
  NSURL* unsupported_url = [NSURL URLWithString:@"fooooo:baaar"];
  EXPECT_TRUE(GetNetErrorFromIOSErrorCode(kCFURLErrorUnsupportedURL,
                                          &net_error_code, unsupported_url));
  EXPECT_EQ(net::ERR_UNKNOWN_URL_SCHEME, net_error_code);

  // kCFSOCKSErrorUnknownClientVersion -> ?
  EXPECT_FALSE(GetNetErrorFromIOSErrorCode(kCFSOCKSErrorUnknownClientVersion,
                                           &net_error_code, /*url=*/nil));
}

// Tests translation of an error with empty domain and no underlying error.
TEST_F(ErrorTranslationUtilTest, MalformedError) {
  NSError* error = [[NSError alloc] initWithDomain:@"" code:0 userInfo:nil];
  NSError* net_error = NetErrorFromError(error);

  // Top level error should be the same as the original error.
  EXPECT_TRUE(net_error);
  EXPECT_NSEQ([error domain], [net_error domain]);
  EXPECT_EQ([error code], [net_error code]);

  // Underlying error should have net error doamin and code.
  NSError* net_underlying_error = [net_error userInfo][NSUnderlyingErrorKey];
  EXPECT_TRUE(net_underlying_error);
  EXPECT_NSEQ(net::kNSErrorDomain, [net_underlying_error domain]);
  EXPECT_EQ(net::ERR_FAILED, [net_underlying_error code]);
}

// Tests translation of unknown CFNetwork error, which does not have an
// underlying error.
TEST_F(ErrorTranslationUtilTest, UnknownCFNetworkError) {
  NSError* error = [[NSError alloc]
      initWithDomain:base::mac::CFToNSCast(kCFErrorDomainCFNetwork)
                code:kCFURLErrorUnknown
            userInfo:nil];
  NSError* net_error = NetErrorFromError(error);

  // Top level error should be the same as the original error.
  EXPECT_TRUE(net_error);
  EXPECT_NSEQ([error domain], [net_error domain]);
  EXPECT_EQ([error code], [net_error code]);

  // Underlying error should have net error domain and code.
  NSError* net_underlying_error = [net_error userInfo][NSUnderlyingErrorKey];
  EXPECT_TRUE(net_underlying_error);
  EXPECT_NSEQ(net::kNSErrorDomain, [net_underlying_error domain]);
  EXPECT_EQ(net::ERR_FAILED, [net_underlying_error code]);
}

// Tests translation of kCFURLErrorCannotFindHost CFNetwork error, which has an
// underlying error with NSURLError domain.
TEST_F(ErrorTranslationUtilTest, CanNotFindHostError) {
  NSError* underlying_error =
      [[NSError alloc] initWithDomain:NSURLErrorDomain
                                 code:kCFURLErrorCannotFindHost
                             userInfo:nil];

  NSError* error =
      [[NSError alloc] initWithDomain:NSURLErrorDomain
                                 code:NSURLErrorCannotFindHost
                             userInfo:@{
                               NSUnderlyingErrorKey : underlying_error,
                             }];
  NSError* net_error = NetErrorFromError(error);

  // Top level error should be the same as the original error.
  EXPECT_TRUE(net_error);
  EXPECT_NSEQ([error domain], [net_error domain]);
  EXPECT_EQ([error code], [net_error code]);

  // First underlying error should be the same as the original underlying error.
  NSError* net_underlying_error = [net_error userInfo][NSUnderlyingErrorKey];
  EXPECT_TRUE(underlying_error);
  EXPECT_NSEQ([underlying_error domain], [net_underlying_error domain]);
  EXPECT_EQ([underlying_error code], [net_underlying_error code]);

  // Final underlying error should have net error domain and code.
  NSError* final_net_underlying_error =
      [net_underlying_error userInfo][NSUnderlyingErrorKey];
  EXPECT_TRUE(final_net_underlying_error);
  EXPECT_NSEQ(net::kNSErrorDomain, [final_net_underlying_error domain]);
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, [final_net_underlying_error code]);
}

// Tests translation of kCFURLErrorSecureConnectionFailed CFNetwork error, by
// specifying different net error code.
TEST_F(ErrorTranslationUtilTest, CertError) {
  NSError* underlying_error =
      [[NSError alloc] initWithDomain:NSURLErrorDomain
                                 code:kCFURLErrorSecureConnectionFailed
                             userInfo:nil];

  NSError* error =
      [[NSError alloc] initWithDomain:NSURLErrorDomain
                                 code:kCFURLErrorSecureConnectionFailed
                             userInfo:@{
                               NSUnderlyingErrorKey : underlying_error,
                             }];
  NSError* net_error = NetErrorFromError(error, net::ERR_CONNECTION_RESET);

  // Top level error should be the same as the original error.
  EXPECT_TRUE(net_error);
  EXPECT_NSEQ([error domain], [net_error domain]);
  EXPECT_EQ([error code], [net_error code]);

  // First underlying error should be the same as the original underlying error.
  NSError* net_underlying_error = [net_error userInfo][NSUnderlyingErrorKey];
  EXPECT_TRUE(underlying_error);
  EXPECT_NSEQ([underlying_error domain], [net_underlying_error domain]);
  EXPECT_EQ([underlying_error code], [net_underlying_error code]);

  // Final underlying error should have net error domain and specified code.
  NSError* final_net_underlying_error =
      [net_underlying_error userInfo][NSUnderlyingErrorKey];
  EXPECT_TRUE(final_net_underlying_error);
  EXPECT_NSEQ(net::kNSErrorDomain, [final_net_underlying_error domain]);
  EXPECT_EQ(net::ERR_CONNECTION_RESET, [final_net_underlying_error code]);
}

}  // namespace web
