// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ios/web/public/test/http_server/response_provider.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kTestUrlNormalBrowsing[] = "http://choux/normal/browsing";
const char kTestUrlNormalSetCookie[] = "http://choux/normal/set_cookie";
const char kTestUrlIncognitoBrowsing[] = "http://choux/incognito/browsing";
const char kTestUrlIncognitoSetCookie[] = "http://choux/incognito/set_cookie";

const char kTestResponse[] = "fleur";
NSString* const kNormalCookieName = @"request";
NSString* const kNormalCookieValue = @"pony";
NSString* const kIncognitoCookieName = @"secret";
NSString* const kIncognitoCookieValue = @"rainbow";

}  // namespace

@interface CookiesTestCase : ChromeTestCase
@end

@implementation CookiesTestCase

#pragma mark - Overrides superclass

#if defined(CHROME_EARL_GREY_2)
+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [self setUpHelper];
}
#elif defined(CHROME_EARL_GREY_1)
+ (void)setUp {
  [super setUp];
  [self setUpHelper];
}
#else
#error Not an EarlGrey Test
#endif

+ (void)setUpHelper {
  // Creates a map of canned responses and set up the test HTML server.
  // |kTestUrlNormalSetCookie| and |kTestUrlIncognitoSetCookie| always sets
  // cookie in response header while |kTestUrlNormalBrowsing| and
  // |kTestUrlIncognitoBrowsing| doesn't.
  std::map<GURL, std::pair<std::string, std::string>> responses;

  NSString* normalCookie = [NSString
      stringWithFormat:@"%@=%@", kNormalCookieName, kNormalCookieValue];
  NSString* incognitoCookie = [NSString
      stringWithFormat:@"%@=%@", kIncognitoCookieName, kIncognitoCookieValue];

  responses[web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)] =
      std::pair<std::string, std::string>("", kTestResponse);
  responses[web::test::HttpServer::MakeUrl(kTestUrlNormalSetCookie)] =
      std::pair<std::string, std::string>(base::SysNSStringToUTF8(normalCookie),
                                          kTestResponse);
  responses[web::test::HttpServer::MakeUrl(kTestUrlIncognitoBrowsing)] =
      std::pair<std::string, std::string>("", kTestResponse);
  responses[web::test::HttpServer::MakeUrl(kTestUrlIncognitoSetCookie)] =
      std::pair<std::string, std::string>(
          base::SysNSStringToUTF8(incognitoCookie), kTestResponse);

  web::test::SetUpSimpleHttpServerWithSetCookies(responses);
}

// Clear cookies to make sure that tests do not interfere each other.
- (void)tearDown {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];
  NSString* const clearCookieScript =
      @"var cookies = document.cookie.split(';');"
       "for (var i = 0; i < cookies.length; i++) {"
       "  var cookie = cookies[i];"
       "  var eqPos = cookie.indexOf('=');"
       "  var name = eqPos > -1 ? cookie.substr(0, eqPos) : cookie;"
       "  document.cookie = name + '=;expires=Thu, 01 Jan 1970 00:00:00 GMT';"
       "}";
  [ChromeEarlGrey executeJavaScript:clearCookieScript];
  [super tearDown];
}

#pragma mark - Tests

// Tests toggling between Normal tabs and Incognito tabs. Different cookies
// (request=pony for normal tabs, secret=rainbow for incognito tabs) are set.
// The goal is to verify that cookies set in incognito tabs are available in
// incognito tabs but not available in normal tabs. Cookies set in incognito
// tabs are also deleted when all incognito tabs are closed.
// TODO(crbug.com/1019670): This test fails on iOS 12/13 under EG2.
#if defined(CHROME_EARL_GREY_2)
#define MAYBE_testClearIncognitoFromMain DISABLED_testClearIncognitoFromMain
#else
#define MAYBE_testClearIncognitoFromMain testClearIncognitoFromMain
#endif
- (void)MAYBE_testClearIncognitoFromMain {
  // Loads a dummy page in normal tab. Sets a normal test cookie. Verifies that
  // the incognito test cookie is not found.
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalSetCookie)];
  NSDictionary* cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kNormalCookieValue, cookies[kNormalCookieName],
                         @"Failed to set normal cookie in normal mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in normal mode.");

  // Opens an incognito tab, loads the dummy page, and sets incognito test
  // cookie.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoSetCookie)];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kIncognitoCookieValue, cookies[kIncognitoCookieName],
                         @"Failed to set incognito cookie in incognito mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in incognito mode.");

  // Switches back to normal profile by opening up a new tab. Test cookie
  // should not be found.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kNormalCookieValue, cookies[kNormalCookieName],
                         @"Normal cookie should still exist in normal mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in normal mode.");

  // Finally, closes all incognito tabs while still in normal tab.
  // Checks that incognito cookie is gone.
  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoBrowsing)];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqual(0U, cookies.count,
                  @"Incognito cookie should be gone from normal mode.");
}

// Tests that a cookie set in incognito tab is removed after closing all
// incognito tabs and then when new incognito tab is created the cookie will
// not reappear.
// TODO(crbug.com/1019670): This test fails on iOS 12/13 under EG2.
#if defined(CHROME_EARL_GREY_2)
#define MAYBE_testClearIncognitoFromIncognito \
  DISABLED_testClearIncognitoFromIncognito
#else
#define MAYBE_testClearIncognitoFromIncognito testClearIncognitoFromIncognito
#endif
- (void)MAYBE_testClearIncognitoFromIncognito {
  // Loads a page in normal tab.
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];

  // Opens an incognito tab, loads a page, and sets an incognito cookie.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoSetCookie)];
  NSDictionary* cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kIncognitoCookieValue, cookies[kIncognitoCookieName],
                         @"Failed to set incognito cookie in incognito mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in incognito mode.");

  // Closes all incognito tabs and switch back to a normal tab.
  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];

  // Opens a new incognito tab and verify that the previously set cookie
  // is no longer there.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoBrowsing)];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqual(0U, cookies.count,
                  @"Incognito cookie should be gone from incognito mode.");

  // Verifies that new incognito cookies can be set.
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoSetCookie)];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kIncognitoCookieValue, cookies[kIncognitoCookieName],
                         @"Failed to set incognito cookie in incognito mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in incognito mode.");
}

// Tests that a cookie set in normal tab is not available in an incognito tab.
// TODO(crbug.com/1019670): This test fails on iOS 12/13 under EG2.
#if defined(CHROME_EARL_GREY_2)
#define MAYBE_testSwitchToIncognito DISABLED_testSwitchToIncognito
#else
#define MAYBE_testSwitchToIncognito testSwitchToIncognito
#endif
- (void)MAYBE_testSwitchToIncognito {
  // Sets cookie in normal tab.
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalSetCookie)];
  NSDictionary* cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kNormalCookieValue, cookies[kNormalCookieName],
                         @"Normal cookie should still exist in normal mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in normal mode.");

  // Switches to a new incognito tab and verifies that cookie is not there.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoBrowsing)];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqual(0U, cookies.count,
                  @"Normal cookie should not be found in incognito mode.");

  // Closes all incognito tabs and then switching back to a normal tab. Verifies
  // that the cookie set earlier is still there.
  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(
      kNormalCookieValue, cookies[kNormalCookieName],
      @"Normal cookie should still be found in normal mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in normal mode.");
}

// Tests that a cookie set in incognito tab is only available in another
// incognito tab. They are not available in a normal tab.
// TODO(crbug.com/1019670): This test fails on iOS 12/13 under EG2.
#if defined(CHROME_EARL_GREY_2)
#define MAYBE_testSwitchToMain DISABLED_testSwitchToMain
#else
#define MAYBE_testSwitchToMain testSwitchToMain
#endif
- (void)MAYBE_testSwitchToMain {
  // Loads a page in normal tab and then switches to a new incognito tab. Sets
  // cookie in incognito tab.
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoSetCookie)];
  NSDictionary* cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kIncognitoCookieValue, cookies[kIncognitoCookieName],
                         @"Failed to set incognito cookie in incognito mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in incognito mode.");

  // Switches back to a normal tab and verifies that cookie set in incognito tab
  // is not available.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqual(0U, cookies.count,
                  @"Incognito cookie should not be found in normal mode.");

  // Returns back to Incognito tab and cookie is still there.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlIncognitoBrowsing)];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(
      kIncognitoCookieValue, cookies[kIncognitoCookieName],
      @"Incognito cookie should be found in incognito mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in incognito mode.");
}

// Tests that a cookie set in a normal tab can be found in another normal tab.
// TODO(crbug.com/1019670): This test fails on iOS 12/13 under EG2.
#if defined(CHROME_EARL_GREY_2)
#define MAYBE_testShareCookiesBetweenTabs DISABLED_testShareCookiesBetweenTabs
#else
#define MAYBE_testShareCookiesBetweenTabs testShareCookiesBetweenTabs
#endif
- (void)MAYBE_testShareCookiesBetweenTabs {
  // Loads page and sets cookie in first normal tab.
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalSetCookie)];
  NSDictionary* cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kNormalCookieValue, cookies[kNormalCookieName],
                         @"Failed to set normal cookie in normal mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in normal mode.");

  // Creates another normal tab and verifies that the cookie is also there.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kTestUrlNormalBrowsing)];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(
      kNormalCookieValue, cookies[kNormalCookieName],
      @"Normal cookie should still be found in normal mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in normal mode.");
}

@end
