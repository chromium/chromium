// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/chrome_app_startup_parameters.h"

#import <Foundation/Foundation.h>

#include "base/strings/stringprintf.h"
#import "ios/chrome/app/app_startup_parameters.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
void CheckLaunchSourceForURL(first_run::ExternalLaunch expectedSource,
                             NSString* urlString) {
  NSURL* url = [NSURL URLWithString:urlString];
  ChromeAppStartupParameters* params = [ChromeAppStartupParameters
      newChromeAppStartupParametersWithURL:url
                     fromSourceApplication:@"com.apple.mobilesafari"];
  EXPECT_EQ(expectedSource, [params launchSource]);
}

typedef PlatformTest AppStartupParametersTest;
TEST_F(PlatformTest, ParseURLWithEmptyURL) {
  NSURL* url = [NSURL URLWithString:@""];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithOneProtocol) {
  NSURL* url = [NSURL URLWithString:@"protocol://www.google.com"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];
  // Here "protocol" opens the app and no protocol is given for the parsed URL,
  // which defaults to be "http".
  EXPECT_EQ("http://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithEmptyParsedURL) {
  // Test chromium://
  NSURL* url = [NSURL URLWithString:@"chromium://"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithParsedURLDefaultToHttp) {
  NSURL* url = [NSURL URLWithString:@"chromium://www.google.com"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  EXPECT_EQ("http://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithInvalidParsedURL) {
  NSURL* url = [NSURL URLWithString:@"http:google.com:foo"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithHttpsParsedURL) {
  NSURL* url = [NSURL URLWithString:@"chromiums://www.google.com"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  EXPECT_EQ("https://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithXCallbackURL) {
  NSURL* url = [NSURL URLWithString:
                          @"chromium-x-callback://x-callback-url/open?"
                           "url=https://www.google.com"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];
  EXPECT_EQ("https://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithXCallbackURLAndExtraParams) {
  NSURL* url = [NSURL URLWithString:
                          @"chromium-x-callback://x-callback-url/open?"
                           "url=https://www.google.com&"
                           "x-success=http://success"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];
  EXPECT_EQ("https://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithMalformedXCallbackURL) {
  NSURL* url =
      [NSURL URLWithString:
                 @"chromium-x-callback://x-callback-url/open?url=foobar&"
                  "x-source=myapp&x-success=http://success"];
  ChromeAppStartupParameters* params = [ChromeAppStartupParameters
      newChromeAppStartupParametersWithURL:url
                     fromSourceApplication:@"com.myapp"];
  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithJavascriptURLInXCallbackURL) {
  NSURL* url = [NSURL
      URLWithString:
          @"chromium-x-callback://x-callback-url/open?url="
           "javascript:window.open()&x-source=myapp&x-success=http://success"];
  ChromeAppStartupParameters* params = [ChromeAppStartupParameters
      newChromeAppStartupParametersWithURL:url
                     fromSourceApplication:@"com.myapp"];
  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithChromeURLInXCallbackURL) {
  NSURL* url = [NSURL URLWithString:
                          @"chromium-x-callback://x-callback-url/open?url="
                           "chrome:passwords"];
  ChromeAppStartupParameters* params = [ChromeAppStartupParameters
      newChromeAppStartupParametersWithURL:url
                     fromSourceApplication:@"com.myapp"];
  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithFileParsedURL) {
  NSURL* url = [NSURL URLWithString:@"file://localhost/path/to/file.pdf"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  std::string expectedUrlString = base::StringPrintf(
      "%s://%s/file.pdf", kChromeUIScheme, kChromeUIExternalFileHost);

  EXPECT_EQ(expectedUrlString, [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupVoiceSearch) {
  ChromeAppStartupParameters* params = [ChromeAppStartupParameters
      newAppStartupParametersForCommand:@"voicesearch"
                       withExternalText:nil
                       withExternalData:nil
                              withIndex:0
                                withURL:nil
                  fromSourceApplication:nil
            fromSecureSourceApplication:nil];

  std::string expectedUrlString =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(expectedUrlString, [params externalURL].spec());
  EXPECT_EQ([params postOpeningAction], START_VOICE_SEARCH);
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupQRCode) {
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newAppStartupParametersForCommand:@"qrscanner"
                                                   withExternalText:nil
                                                   withExternalData:nil
                                                          withIndex:0
                                                            withURL:nil
                                              fromSourceApplication:nil
                                        fromSecureSourceApplication:nil];

  std::string expectedUrlString =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(expectedUrlString, [params externalURL].spec());
  EXPECT_EQ([params postOpeningAction], START_QR_CODE_SCANNER);
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupFocusOmbnibox) {
  ChromeAppStartupParameters* params = [ChromeAppStartupParameters
      newAppStartupParametersForCommand:@"focusomnibox"
                       withExternalText:nil
                       withExternalData:nil
                              withIndex:0
                                withURL:nil
                  fromSourceApplication:nil
            fromSecureSourceApplication:nil];

  std::string expectedUrlString =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(expectedUrlString, [params externalURL].spec());
  EXPECT_EQ([params postOpeningAction], FOCUS_OMNIBOX);
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupNewTab) {
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newAppStartupParametersForCommand:@"newtab"
                                                   withExternalText:nil
                                                   withExternalData:nil
                                                          withIndex:0
                                                            withURL:nil
                                              fromSourceApplication:nil
                                        fromSecureSourceApplication:nil];
  std::string expectedUrlString =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(expectedUrlString, [params externalURL].spec());
  EXPECT_EQ([params postOpeningAction], NO_ACTION);
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupOpenURL) {
  ChromeAppStartupParameters* params = [ChromeAppStartupParameters
      newAppStartupParametersForCommand:@"openurl"
                       withExternalText:@"http://foo/bar"
                       withExternalData:nil
                              withIndex:0
                                withURL:nil
                  fromSourceApplication:nil
            fromSecureSourceApplication:nil];

  EXPECT_EQ("http://foo/bar", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupGarbage) {
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newAppStartupParametersForCommand:@"garbage"
                                                   withExternalText:nil
                                                   withExternalData:nil
                                                          withIndex:0
                                                            withURL:nil
                                              fromSourceApplication:nil
                                        fromSecureSourceApplication:nil];
  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, FirstRunExternalLaunchSource) {
  // Key at the beginning of query string.
  CheckLaunchSourceForURL(
      first_run::LAUNCH_BY_SMARTAPPBANNER,
      @"http://www.google.com/search?safarisab=1&query=pony");
  // Key at the end of query string.
  CheckLaunchSourceForURL(
      first_run::LAUNCH_BY_SMARTAPPBANNER,
      @"http://www.google.com/search?query=pony&safarisab=1");
  // Key in the middle of query string.
  CheckLaunchSourceForURL(
      first_run::LAUNCH_BY_SMARTAPPBANNER,
      @"http://www.google.com/search?query=pony&safarisab=1&hl=en");
  // Key without '=' sign at the beginning, end, and middle of query string.
  CheckLaunchSourceForURL(first_run::LAUNCH_BY_SMARTAPPBANNER,
                          @"http://www.google.com/search?safarisab&query=pony");
  CheckLaunchSourceForURL(first_run::LAUNCH_BY_SMARTAPPBANNER,
                          @"http://www.google.com/search?query=pony&safarisab");
  CheckLaunchSourceForURL(
      first_run::LAUNCH_BY_SMARTAPPBANNER,
      @"http://www.google.com/search?query=pony&safarisab&hl=en");
  // No query string in URL.
  CheckLaunchSourceForURL(first_run::LAUNCH_BY_MOBILESAFARI,
                          @"http://www.google.com/");
  CheckLaunchSourceForURL(first_run::LAUNCH_BY_MOBILESAFARI,
                          @"http://www.google.com/safarisab/foo/bar");
  // Key not present in query string.
  CheckLaunchSourceForURL(first_run::LAUNCH_BY_MOBILESAFARI,
                          @"http://www.google.com/search?query=pony");
  // Key is a substring of some other string.
  CheckLaunchSourceForURL(
      first_run::LAUNCH_BY_MOBILESAFARI,
      @"http://www.google.com/search?query=pony&safarisabcdefg=1");
  CheckLaunchSourceForURL(
      first_run::LAUNCH_BY_MOBILESAFARI,
      @"http://www.google.com/search?query=pony&notsafarisab=1&abc=def");
}

}  // namespace
