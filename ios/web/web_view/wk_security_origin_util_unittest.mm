// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_view/wk_security_origin_util.h"

#import <WebKit/WebKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

// WKSecurityOrigin can not be manually created, hence stub is needed.
@interface WKSecurityOriginStub : NSObject
// Methods from WKSecurityOrigin class.
@property(nonatomic, copy) NSString* protocol;
@property(nonatomic, copy) NSString* host;
@property(nonatomic) NSInteger port;
@end

@implementation WKSecurityOriginStub
@synthesize protocol = _protocol;
@synthesize host = _host;
@synthesize port = _port;
@end

namespace web {

using WKSecurityOriginUtilTest = PlatformTest;

// Tests calling GURLOriginWithWKSecurityOrigin with nil.
TEST_F(WKSecurityOriginUtilTest, GURLOriginWithNilWKSecurityOrigin) {
  GURL url(GURLOriginWithWKSecurityOrigin(nil));

  EXPECT_FALSE(url.is_valid());
  EXPECT_TRUE(url.spec().empty());
}

// Tests calling GURLOriginWithWKSecurityOrigin with valid origin.
TEST_F(WKSecurityOriginUtilTest, GURLOriginWithValidWKSecurityOrigin) {
  WKSecurityOriginStub* origin = [[WKSecurityOriginStub alloc] init];
  [origin setProtocol:@"http"];
  [origin setHost:@"chromium.org"];
  [origin setPort:80];

  GURL url(
      GURLOriginWithWKSecurityOrigin(static_cast<WKSecurityOrigin*>(origin)));
  EXPECT_EQ("http://chromium.org/", url.spec());
  EXPECT_TRUE(url.port().empty());
}

// Tests calling GURLOriginWithWKSecurityOrigin with default port.
TEST_F(WKSecurityOriginUtilTest, GURLOriginWithDefaultPort) {
  WKSecurityOriginStub* origin = [[WKSecurityOriginStub alloc] init];
  [origin setProtocol:@"http"];
  [origin setHost:@"chromium.org"];
  [origin setPort:0];

  GURL url(
      GURLOriginWithWKSecurityOrigin(static_cast<WKSecurityOrigin*>(origin)));
  EXPECT_EQ("http://chromium.org/", url.spec());
  EXPECT_TRUE(url.port().empty());
}

// Tests calling GURLOriginWithWKSecurityOrigin with valid origin.
TEST_F(WKSecurityOriginUtilTest, GURLOriginWithNonDefaultPort) {
  WKSecurityOriginStub* origin = [[WKSecurityOriginStub alloc] init];
  [origin setProtocol:@"http"];
  [origin setHost:@"chromium.org"];
  [origin setPort:123];

  GURL url(
      GURLOriginWithWKSecurityOrigin(static_cast<WKSecurityOrigin*>(origin)));
  EXPECT_EQ("http://chromium.org:123/", url.spec());
  EXPECT_EQ("123", url.port());
}

// Tests calling GURLOriginWithWKSecurityOrigin with valid origin.
TEST_F(WKSecurityOriginUtilTest, GURLOriginWithChromeProtocol) {
  WKSecurityOriginStub* origin = [[WKSecurityOriginStub alloc] init];
  [origin setProtocol:@"testwebui"];
  [origin setHost:@"version"];
  [origin setPort:0];

  GURL url(
      GURLOriginWithWKSecurityOrigin(static_cast<WKSecurityOrigin*>(origin)));
  EXPECT_EQ("testwebui://version/", url.spec());
  EXPECT_TRUE(url.port().empty());
}

}  // namespace web
