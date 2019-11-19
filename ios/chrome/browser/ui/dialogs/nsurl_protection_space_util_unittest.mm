// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/nsurl_protection_space_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using nsurlprotectionspace_util::CanShow;
using nsurlprotectionspace_util::MessageForHTTPAuth;

namespace {

// Test hostnames and URL origins.
NSString* const kTestHost = @"chromium.org";
NSString* const kTestHttpOrigin = @"http://chromium.org";
NSString* const kTestHttpsOrigin = @"https://chromium.org:80";

// Returns protection space for the given |host|, |protocol| and |port|.
NSURLProtectionSpace* GetProtectionSpaceForHost(NSString* host,
                                                NSString* protocol,
                                                NSInteger port) {
  return [[NSURLProtectionSpace alloc] initWithHost:host
                                               port:port
                                           protocol:protocol
                                              realm:nil
                               authenticationMethod:nil];
}

// Returns protection space for the given |host| and |protocol| and port 80.
NSURLProtectionSpace* GetProtectionSpaceForHost(NSString* host,
                                                NSString* protocol) {
  return GetProtectionSpaceForHost(host, protocol, 80);
}

// Returns protection space for the given proxy |host| and |protocol|.
NSURLProtectionSpace* GetProtectionSpaceForProxyHost(NSString* host,
                                                     NSString* type) {
  return [[NSURLProtectionSpace alloc] initWithProxyHost:host
                                                    port:80
                                                    type:type
                                                   realm:nil
                                    authenticationMethod:nil];
}

}  // namespace

using NSURLProtectionSpaceUtilTest = PlatformTest;

// Tests that dialog can not be shown without valid host.
TEST_F(NSURLProtectionSpaceUtilTest, CantShowWithoutValidHost) {
  NSURLProtectionSpace* protectionSpace =
      GetProtectionSpaceForHost(@"", NSURLProtectionSpaceHTTPS);

  EXPECT_FALSE(CanShow(protectionSpace));
}

// Tests that dialog can not be shown with invalid port.
TEST_F(NSURLProtectionSpaceUtilTest, CantShowWithoutValidPort) {
  NSURLProtectionSpace* protectionSpace =
      GetProtectionSpaceForHost(kTestHost, NSURLProtectionSpaceHTTPS, INT_MAX);

  EXPECT_FALSE(CanShow(protectionSpace));
}

// Tests showing the dialog for SOCKS proxy server.
TEST_F(NSURLProtectionSpaceUtilTest, ShowForSocksProxy) {
  NSURLProtectionSpace* protectionSpace =
      GetProtectionSpaceForProxyHost(kTestHost, NSURLProtectionSpaceSOCKSProxy);

  ASSERT_TRUE(CanShow(protectionSpace));

  // Expecting the following text:
  // The proxy chromium.org requires a username and password.
  // Your connection to this site is not private.
  NSString* expectedText =
      [NSString stringWithFormat:@"%@ %@",
                                 l10n_util::GetNSStringF(
                                     IDS_LOGIN_DIALOG_PROXY_AUTHORITY,
                                     base::SysNSStringToUTF16(kTestHost)),
                                 l10n_util::GetNSString(
                                     IDS_PAGE_INFO_NOT_SECURE_SUMMARY)];

  EXPECT_NSEQ(expectedText, MessageForHTTPAuth(protectionSpace));
}

// Tests showing the dialog for http proxy server.
TEST_F(NSURLProtectionSpaceUtilTest, ShowForHttpProxy) {
  NSURLProtectionSpace* protectionSpace =
      GetProtectionSpaceForProxyHost(kTestHost, NSURLProtectionSpaceHTTPProxy);

  ASSERT_TRUE(CanShow(protectionSpace));

  // Expecting the following text:
  // The proxy http://chromium.org requires a username and password.
  // Your connection to this site is not private.
  NSString* expectedText =
      [NSString stringWithFormat:@"%@ %@",
                                 l10n_util::GetNSStringF(
                                     IDS_LOGIN_DIALOG_PROXY_AUTHORITY,
                                     base::SysNSStringToUTF16(kTestHttpOrigin)),
                                 l10n_util::GetNSString(
                                     IDS_PAGE_INFO_NOT_SECURE_SUMMARY)];
  EXPECT_NSEQ(expectedText, MessageForHTTPAuth(protectionSpace));
}

// Tests showing the dialog for https proxy server.
TEST_F(NSURLProtectionSpaceUtilTest, ShowForHttpsProxy) {
  NSURLProtectionSpace* protectionSpace =
      GetProtectionSpaceForProxyHost(kTestHost, NSURLProtectionSpaceHTTPSProxy);

  ASSERT_TRUE(CanShow(protectionSpace));

  NSString* expectedText = nil;
  // HTTPS Proxy protection space reports itself as unsecure
  // (crbug.com/629570).
  // Expecting the following text:
  // The proxy https://chromium.org requires a username and password.
  // Your connection to this site is not private.
  expectedText = [NSString
      stringWithFormat:@"%@ %@",
                       l10n_util::GetNSStringF(
                           IDS_LOGIN_DIALOG_PROXY_AUTHORITY,
                           base::SysNSStringToUTF16(kTestHttpsOrigin)),
                       l10n_util::GetNSString(
                           IDS_PAGE_INFO_NOT_SECURE_SUMMARY)];
  EXPECT_NSEQ(expectedText, MessageForHTTPAuth(protectionSpace));
}

// Tests showing the dialog for http server.
TEST_F(NSURLProtectionSpaceUtilTest, ShowForHttpServer) {
  NSURLProtectionSpace* protectionSpace =
      GetProtectionSpaceForHost(kTestHost, NSURLProtectionSpaceHTTP);

  ASSERT_TRUE(CanShow(protectionSpace));

  // Expecting the following text:
  // http://chromium.org requires a username and password.
  NSString* expectedText =
      [NSString stringWithFormat:@"%@ %@",
                                 l10n_util::GetNSStringF(
                                     IDS_LOGIN_DIALOG_AUTHORITY,
                                     base::SysNSStringToUTF16(kTestHttpOrigin)),
                                 l10n_util::GetNSString(
                                     IDS_PAGE_INFO_NOT_SECURE_SUMMARY)];
  EXPECT_NSEQ(expectedText, MessageForHTTPAuth(protectionSpace));
}

// Tests showing the dialog for https server.
TEST_F(NSURLProtectionSpaceUtilTest, ShowForHttpsServer) {
  NSURLProtectionSpace* protectionSpace =
      GetProtectionSpaceForHost(kTestHost, NSURLProtectionSpaceHTTPS);

  ASSERT_TRUE(CanShow(protectionSpace));

  // Expecting the following text:
  // https://chromium.org:80 requires a username and password.
  NSString* expectedText = l10n_util::GetNSStringF(
      IDS_LOGIN_DIALOG_AUTHORITY, base::SysNSStringToUTF16(kTestHttpsOrigin));
  EXPECT_NSEQ(expectedText, MessageForHTTPAuth(protectionSpace));
}
