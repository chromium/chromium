// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/u2f/u2f_controller.h"

#import "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/chrome_url_util.h"
#include "ios/web/public/deprecated/url_verification_constants.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Mocks ExecuteJavaScript method.
class WebStateMock : public web::TestWebState {
 public:
  MOCK_METHOD1(ExecuteJavaScript, void(const base::string16&));
};

}  // namespace

class U2FControllerTest : public PlatformTest {
 protected:
  U2FControllerTest() : _U2FController([[U2FController alloc] init]) {
    url::AddStandardScheme("chromium", url::SCHEME_WITH_HOST);
    [[ChromeAppConstants sharedInstance]
        setCallbackSchemeForTesting:@"chromium"];
  }

  // Get the requestUUID NSString from a properly formatted U2F XCallback GURL.
  NSString* requestUUIDFromXCallbackURL(const GURL& XCallbackURL) {
    NSRegularExpression* regex =
        [NSRegularExpression regularExpressionWithPattern:
                                 @".+success.+requestUUID%3D(.+)%26.+error.+"
                                                  options:0
                                                    error:nil];
    NSArray* matches =
        [regex matchesInString:base::SysUTF8ToNSString(XCallbackURL.spec())
                       options:0
                         range:NSMakeRange(0, XCallbackURL.spec().length())];
    EXPECT_EQ(1u, [matches count]);
    NSString* requestUUIDString = [regex
        stringByReplacingMatchesInString:base::SysUTF8ToNSString(
                                             XCallbackURL.spec())
                                 options:0
                                   range:NSMakeRange(
                                             0, XCallbackURL.spec().length())
                            withTemplate:@"$1"];
    DCHECK([requestUUIDString length]);
    return requestUUIDString;
  }

  U2FController* _U2FController;
};

TEST_F(U2FControllerTest, XCallbackFromRequestURLWithCorrectFlowTest) {
  // Test when request is legal and properly formatted.
  GURL requestURL("u2f://accounts.google.com?data=abc&def%26ghi");
  GURL originURL("https://accounts.google.com");
  GURL tabURL("https://accounts.google.com");
  NSString* tabID = @"B05B1860-18BA-43EA-B7DC-470D9F918FF5";
  GURL XCallbackURL = [_U2FController XCallbackFromRequestURL:requestURL
                                                    originURL:originURL
                                                       tabURL:tabURL
                                                        tabID:tabID];

  NSRegularExpression* regex = [NSRegularExpression
      regularExpressionWithPattern:
          @"u2f-x-callback://x-callback-url/auth\\?x-success=.+"
           "u2f-callback%2F%3FtabID%3DB05B1860-18BA-43EA-B7DC-470D9F918FF5%"
           "26requestUUID%3.+%26isU2F%3D1&x-error=.+u2f-callback%2F%3FtabID%"
           "3DB05B1860-18BA-43EA-B7DC-470D9F918FF5%26requestUUID%3.+%26isU2F%"
           "3D1&data=data%3Dabc%26def%2526ghi&origin=https%3A%2F%2Faccounts."
           "google.com%2F"
                           options:0
                             error:nil];

  NSArray* matches =
      [regex matchesInString:base::SysUTF8ToNSString(XCallbackURL.spec())
                     options:0
                       range:NSMakeRange(0, XCallbackURL.spec().length())];
  EXPECT_EQ(1u, [matches count]);
}

TEST_F(U2FControllerTest, XCallbackFromRequestURLWithDuplicatedParamsTest) {
  // Test when request is legal but contains duplicated parameters.
  GURL duplicatedParamsRequestURL(
      "chromium://u2f-callback?isU2F=0&tabID=1&requestUUID=2"
      "&data=abc&def%26ghi");
  GURL originURL("https://accounts.google.com");
  GURL tabURL("https://accounts.google.com");
  NSString* tabID = @"B05B1860-18BA-43EA-B7DC-470D9F918FF5";
  GURL duplicatedParamsXCallbackURL =
      [_U2FController XCallbackFromRequestURL:duplicatedParamsRequestURL
                                    originURL:originURL
                                       tabURL:tabURL
                                        tabID:tabID];

  NSRegularExpression* duplicatedParamsRegex = [NSRegularExpression
      regularExpressionWithPattern:
          @"u2f-x-callback://x-callback-url/auth\\?x-success=.+"
           "u2f-callback%2F%3FtabID%3DB05B1860-18BA-43EA-B7DC-470D9F918FF5%"
           "26requestUUID%3.+%26isU2F%3D1&x-error=.+u2f-callback%2F%3FtabID%"
           "3DB05B1860-18BA-43EA-B7DC-470D9F918FF5%26requestUUID%3.+%26isU2F%"
           "3D1&data=isU2F%3D0%26tabID%3D1%26requestUUID%3D2%26data%3Dabc%"
           "26def%2526ghi&origin=https%3A%2F%2Faccounts.google.com%2F"
                           options:0
                             error:nil];

  NSArray* duplicatedParamsMatches = [duplicatedParamsRegex
      matchesInString:base::SysUTF8ToNSString(
                          duplicatedParamsXCallbackURL.spec())
              options:0
                range:NSMakeRange(
                          0, duplicatedParamsXCallbackURL.spec().length())];
  EXPECT_EQ(1u, [duplicatedParamsMatches count]);
}

TEST_F(U2FControllerTest, XCallbackFromRequestURLWithNonWhitelistedURLTest) {
  // Test when request site is not whitelisted.
  GURL requestURL("u2f://accounts.google.com?data=abc&def%26ghi");
  GURL evilOriginURL("https://evil.appspot.com");
  GURL tabURL("https://accounts.google.com");
  NSString* tabID = @"B05B1860-18BA-43EA-B7DC-470D9F918FF5";
  GURL evilXCallbackURL = [_U2FController XCallbackFromRequestURL:requestURL
                                                        originURL:evilOriginURL
                                                           tabURL:tabURL
                                                            tabID:tabID];
  EXPECT_EQ(GURL(), evilXCallbackURL);
}

TEST_F(U2FControllerTest, XCallbackFromRequestURLWithInsecureConnectionTest) {
  // Test when request site does not have secure connection.
  GURL requestURL("u2f://accounts.google.com?data=abc&def%26ghi");
  GURL insecureOriginURL("http://accounts.google.com");
  GURL tabURL("https://accounts.google.com");
  NSString* tabID = @"B05B1860-18BA-43EA-B7DC-470D9F918FF5";
  GURL insecureXCallbackURL =
      [_U2FController XCallbackFromRequestURL:requestURL
                                    originURL:insecureOriginURL
                                       tabURL:tabURL
                                        tabID:tabID];
  EXPECT_EQ(GURL(), insecureXCallbackURL);
}

TEST_F(U2FControllerTest, EvaluateU2FResultFromU2FURLWithCorrectFlowTest) {
  GURL requestURL("u2f://accounts.google.com?data=abc");
  GURL originURL("https://accounts.google.com");
  GURL tabURL("https://accounts.google.com");
  NSString* tabID = @"B05B1860-18BA-43EA-B7DC-470D9F918FF5";
  GURL XCallbackURL = [_U2FController XCallbackFromRequestURL:requestURL
                                                    originURL:originURL
                                                       tabURL:tabURL
                                                        tabID:tabID];
  NSString* requestUUIDString = this->requestUUIDFromXCallbackURL(XCallbackURL);

  WebStateMock webState;
  webState.SetTrustLevel(web::URLVerificationTrustLevel::kAbsolute);
  webState.SetCurrentURL(tabURL);

  // Test when U2F callback has correct information, Tab URL has not changed and
  // is trusted.
  GURL correctRequestUUIDURL("chromium://u2f-callback?requestUUID=" +
                             base::SysNSStringToUTF8(requestUUIDString) +
                             "&tabID=" + base::SysNSStringToUTF8(tabID));
  EXPECT_CALL(webState, ExecuteJavaScript(testing::_)).Times(1);
  [_U2FController evaluateU2FResultFromU2FURL:correctRequestUUIDURL
                                     webState:&webState];
}

TEST_F(U2FControllerTest, EvaluateU2FResultFromU2FURLWithReplayAttackTest) {
  GURL requestURL("u2f://accounts.google.com?data=abc");
  GURL originURL("https://accounts.google.com");
  GURL tabURL("https://accounts.google.com");
  NSString* tabID = @"B05B1860-18BA-43EA-B7DC-470D9F918FF5";
  GURL XCallbackURL = [_U2FController XCallbackFromRequestURL:requestURL
                                                    originURL:originURL
                                                       tabURL:tabURL
                                                        tabID:tabID];
  NSString* requestUUIDString = this->requestUUIDFromXCallbackURL(XCallbackURL);

  WebStateMock webState;
  webState.SetTrustLevel(web::URLVerificationTrustLevel::kAbsolute);
  webState.SetCurrentURL(tabURL);

  // Test when U2F callback has correct information, Tab URL has not changed and
  // is trusted.
  GURL correctRequestUUIDURL("chromium://u2f-callback?requestUUID=" +
                             base::SysNSStringToUTF8(requestUUIDString) +
                             "&tabID=" + base::SysNSStringToUTF8(tabID));
  EXPECT_CALL(webState, ExecuteJavaScript(testing::_)).Times(1);
  [_U2FController evaluateU2FResultFromU2FURL:correctRequestUUIDURL
                                     webState:&webState];

  // Test when requestUUID is used for multiple times. Subsequent calls with the
  // same requestUUID should not do anything.
  EXPECT_CALL(webState, ExecuteJavaScript(testing::_)).Times(0);
  [_U2FController evaluateU2FResultFromU2FURL:correctRequestUUIDURL
                                     webState:&webState];
}

TEST_F(U2FControllerTest, EvaluateU2FResultFromU2FURLWithBadURLFormatTest) {
  GURL requestURL("u2f://accounts.google.com?data=abc");
  GURL originURL("https://accounts.google.com");
  GURL tabURL("https://accounts.google.com");
  NSString* tabID = @"B05B1860-18BA-43EA-B7DC-470D9F918FF5";
  GURL XCallbackURL = [_U2FController XCallbackFromRequestURL:requestURL
                                                    originURL:originURL
                                                       tabURL:tabURL
                                                        tabID:tabID];
  NSString* requestUUIDString = this->requestUUIDFromXCallbackURL(XCallbackURL);

  WebStateMock webState;
  webState.SetTrustLevel(web::URLVerificationTrustLevel::kAbsolute);
  webState.SetCurrentURL(tabURL);

  // Test when U2F callback has no requestUUID info.
  GURL noRequestUUIDURL("chromium://u2f-callback?tabID=" +
                        base::SysNSStringToUTF8(tabID));
  EXPECT_CALL(webState, ExecuteJavaScript(testing::_)).Times(0);
  [_U2FController evaluateU2FResultFromU2FURL:noRequestUUIDURL
                                     webState:&webState];

  // Test when U2F callback has wrong requestUUID value.
  GURL wrongRequestUUIDURL("chromium://u2f-callback?requestUUID=123&tabID=" +
                           base::SysNSStringToUTF8(tabID));
  EXPECT_CALL(webState, ExecuteJavaScript(testing::_)).Times(0);
  [_U2FController evaluateU2FResultFromU2FURL:wrongRequestUUIDURL
                                     webState:&webState];

  // Test when U2F callback hostname is unexpected.
  GURL wrongHostnameURL("chromium://evil-callback?requestUUID=" +
                        base::SysNSStringToUTF8(requestUUIDString) + "&tabID=" +
                        base::SysNSStringToUTF8(tabID));
  EXPECT_CALL(webState, ExecuteJavaScript(testing::_)).Times(0);
  [_U2FController evaluateU2FResultFromU2FURL:wrongHostnameURL
                                     webState:&webState];
}

TEST_F(U2FControllerTest, EvaluateU2FResultFromU2FURLWithBadTabStateTest) {
  GURL requestURL("u2f://accounts.google.com?data=abc");
  GURL originURL("https://accounts.google.com");
  GURL tabURL("https://accounts.google.com");
  NSString* tabID = @"B05B1860-18BA-43EA-B7DC-470D9F918FF5";
  GURL XCallbackURL = [_U2FController XCallbackFromRequestURL:requestURL
                                                    originURL:originURL
                                                       tabURL:tabURL
                                                        tabID:tabID];
  NSString* requestUUIDString = this->requestUUIDFromXCallbackURL(XCallbackURL);

  WebStateMock webState;

  // Test when U2F callback has correct information but Tab URL changed.
  webState.SetTrustLevel(web::URLVerificationTrustLevel::kAbsolute);
  webState.SetCurrentURL(GURL("http://www.dummy.com"));
  GURL correctRequestUUIDURL("chromium://u2f-callback?requestUUID=" +
                             base::SysNSStringToUTF8(requestUUIDString) +
                             "&tabID=" + base::SysNSStringToUTF8(tabID));
  [_U2FController evaluateU2FResultFromU2FURL:correctRequestUUIDURL
                                     webState:&webState];
  EXPECT_CALL(webState, ExecuteJavaScript(testing::_)).Times(0);

  // Test when U2F callback has correct information but Tab URL not trusted.
  webState.SetTrustLevel(web::URLVerificationTrustLevel::kNone);
  webState.SetCurrentURL(tabURL);
  XCallbackURL = [_U2FController XCallbackFromRequestURL:requestURL
                                               originURL:originURL
                                                  tabURL:tabURL
                                                   tabID:tabID];
  requestUUIDString = this->requestUUIDFromXCallbackURL(XCallbackURL);
  correctRequestUUIDURL = GURL("chromium://u2f-callback?requestUUID=" +
                               base::SysNSStringToUTF8(requestUUIDString) +
                               "&tabID=" + base::SysNSStringToUTF8(tabID));
  EXPECT_CALL(webState, ExecuteJavaScript(testing::_)).Times(0);
  [_U2FController evaluateU2FResultFromU2FURL:correctRequestUUIDURL
                                     webState:&webState];
}
