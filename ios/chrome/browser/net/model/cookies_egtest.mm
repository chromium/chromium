// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <map>
#import <memory>
#import <string>

#import <XCTest/XCTest.h>

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"

namespace {

NSString* const kNormalCookieName = @"request";
NSString* const kNormalCookieValue = @"pony";
NSString* const kIncognitoCookieName = @"secret";
NSString* const kIncognitoCookieValue = @"rainbow";

std::string CookiePath() {
  return base::StringPrintf(
      "/set-cookie?%s=%s", base::SysNSStringToUTF8(kNormalCookieName).c_str(),
      base::SysNSStringToUTF8(kNormalCookieValue).c_str());
}

std::string IncognitoCookiePath() {
  return base::StringPrintf(
      "/set-cookie?%s=%s",
      base::SysNSStringToUTF8(kIncognitoCookieName).c_str(),
      base::SysNSStringToUTF8(kIncognitoCookieValue).c_str());
}

}  // namespace

@interface CookiesTestCase : ChromeTestCase
@end

@implementation CookiesTestCase

- (void)setUp {
  [super setUp];
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Clear cookies to make sure that tests do not interfere each other.
- (void)tearDown {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  NSString* const clearCookieScript =
      @"var cookies = document.cookie.split(';');"
       "for (var i = 0; i < cookies.length; i++) {"
       "  var cookie = cookies[i];"
       "  var eqPos = cookie.indexOf('=');"
       "  var name = eqPos > -1 ? cookie.substr(0, eqPos) : cookie;"
       "  document.cookie = name + '=;expires=Thu, 01 Jan 1970 00:00:00 GMT';"
       "}";
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:clearCookieScript];
  [super tearDown];
}

#pragma mark - Tests

// Tests toggling between Normal tabs and Incognito tabs. Different cookies
// (request=pony for normal tabs, secret=rainbow for incognito tabs) are set.
// The goal is to verify that cookies set in incognito tabs are available in
// incognito tabs but not available in normal tabs. Cookies set in incognito
// tabs are also deleted when all incognito tabs are closed.
- (void)testClearIncognitoFromMain {
  // Loads a dummy page in normal tab. Sets a normal test cookie. Verifies that
  // the incognito test cookie is not found.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(CookiePath())];
  NSDictionary* cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kNormalCookieValue, cookies[kNormalCookieName],
                         @"Failed to set normal cookie in normal mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in normal mode.");

  // Opens an incognito tab, loads the dummy page, and sets incognito test
  // cookie.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(IncognitoCookiePath())];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kIncognitoCookieValue, cookies[kIncognitoCookieName],
                         @"Failed to set incognito cookie in incognito mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in incognito mode.");

  // Switches back to normal profile by opening up a new tab. Only normal cookie
  // should be found.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kNormalCookieValue, cookies[kNormalCookieName],
                         @"Normal cookie should still exist in normal mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in normal mode.");

  // Work around a TabGrid bug by opening and closing the grid before
  // proceeding.
  // TODO(crbug.com/40856675): Fix the underlying bug and remove this
  // workaround.
  [ChromeEarlGrey showTabSwitcher];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Finally, closes all incognito tabs while still in normal tab.
  // Checks that incognito cookie is gone.
  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqual(1U, cookies.count,
                  @"Incognito cookie should be gone from normal mode.");
  GREYAssertEqualObjects(kNormalCookieValue, cookies[kNormalCookieName],
                         @"Failed to set normal cookie in normal mode.");
}

// Tests that a cookie set in incognito tab is removed after closing all
// incognito tabs and then when new incognito tab is created the cookie will
// not reappear.
- (void)testClearIncognitoFromIncognito {
  // Loads a page in normal tab.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  // Opens an incognito tab, loads a page, and sets an incognito cookie.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(IncognitoCookiePath())];
  NSDictionary* cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kIncognitoCookieValue, cookies[kIncognitoCookieName],
                         @"Failed to set incognito cookie in incognito mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in incognito mode.");

  // Closes all incognito tabs and switch back to a normal tab.
  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  // Opens a new incognito tab and verify that the previously set cookie
  // is no longer there.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqual(0U, cookies.count,
                  @"Incognito cookie should be gone from incognito mode.");

  // Verifies that new incognito cookies can be set.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(IncognitoCookiePath())];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kIncognitoCookieValue, cookies[kIncognitoCookieName],
                         @"Failed to set incognito cookie in incognito mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in incognito mode.");
}

// Tests that a cookie set in normal tab is not available in an incognito tab.
- (void)testSwitchToIncognito {
  // Sets cookie in normal tab.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(CookiePath())];
  NSDictionary* cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kNormalCookieValue, cookies[kNormalCookieName],
                         @"Normal cookie should still exist in normal mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in normal mode.");

  // Switches to a new incognito tab and verifies that cookie is not there.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqual(0U, cookies.count,
                  @"Normal cookie should not be found in incognito mode.");

  // Closes all incognito tabs and then switching back to a normal tab. Verifies
  // that the cookie set earlier is still there.
  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(
      kNormalCookieValue, cookies[kNormalCookieName],
      @"Normal cookie should still be found in normal mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in normal mode.");
}

// Tests that a cookie set in incognito tab is only available in another
// incognito tab. They are not available in a normal tab.
- (void)testSwitchToMain {
  // Loads a page in normal tab and then switches to a new incognito tab. Sets
  // cookie in incognito tab.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(IncognitoCookiePath())];
  NSDictionary* cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kIncognitoCookieValue, cookies[kIncognitoCookieName],
                         @"Failed to set incognito cookie in incognito mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in incognito mode.");

  // Switches back to a normal tab and verifies that cookie set in incognito tab
  // is not available.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqual(0U, cookies.count,
                  @"Incognito cookie should not be found in normal mode.");

  // Returns back to Incognito tab and cookie is still there.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(
      kIncognitoCookieValue, cookies[kIncognitoCookieName],
      @"Incognito cookie should be found in incognito mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in incognito mode.");
}

// Tests that a cookie set in a normal tab can be found in another normal tab.
- (void)testShareCookiesBetweenTabs {
  // Loads page and sets cookie in first normal tab.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(CookiePath())];
  NSDictionary* cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kNormalCookieValue, cookies[kNormalCookieName],
                         @"Failed to set normal cookie in normal mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in normal mode.");

  // Creates another normal tab and verifies that the cookie is also there.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(
      kNormalCookieValue, cookies[kNormalCookieName],
      @"Normal cookie should still be found in normal mode.");
  GREYAssertEqual(1U, cookies.count,
                  @"Only one cookie should be found in normal mode.");
}

@end
