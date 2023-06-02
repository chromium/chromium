// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/chrome_app_startup_parameters.h"

#import <Foundation/Foundation.h>

#import "base/strings/stringprintf.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

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

// Tests that http url remains unchanged.
TEST_F(AppStartupParametersTest, ParseURLWithHttpURL) {
  NSURL* url = [NSURL URLWithString:@"http://www.google.com"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  EXPECT_EQ("http://www.google.com/", [params externalURL]);
}

// Tests that https url remains unchanged.
TEST_F(AppStartupParametersTest, ParseURLWithHttpsURL) {
  NSURL* url = [NSURL URLWithString:@"https://www.google.com"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  EXPECT_EQ("https://www.google.com/", [params externalURL]);
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

// Tests that search widget url is parsed correctly, and the right metric is
// recorded.
TEST_F(AppStartupParametersTest, ParseSearchWidgetKit) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL URLWithString:@"chromewidgetkit://search-widget/search"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  std::string expected_url_string =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(params.externalURL.spec(), expected_url_string);
  EXPECT_EQ(params.postOpeningAction, FOCUS_OMNIBOX);
  EXPECT_NE(params.applicationMode, ApplicationModeForTabOpening::INCOGNITO);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 1, 1);
}

// Tests that quick actions widget search url is parsed correctly, and the right
// metric is recorded.
TEST_F(AppStartupParametersTest, ParseQuickActionsWidgetKitSearch) {
  base::HistogramTester histogram_tester;
  NSURL* url =
      [NSURL URLWithString:@"chromewidgetkit://quick-actions-widget/search"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  std::string expected_url_string =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(params.externalURL.spec(), expected_url_string);
  EXPECT_EQ(params.postOpeningAction, FOCUS_OMNIBOX);
  EXPECT_NE(params.applicationMode, ApplicationModeForTabOpening::INCOGNITO);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 2, 1);
}

// Tests that quick actions widget incognito url is parsed correctly, and the
// right metric is recorded.
TEST_F(AppStartupParametersTest, ParseQuickActionsWidgetKitIncognito) {
  base::HistogramTester histogram_tester;
  NSURL* url =
      [NSURL URLWithString:@"chromewidgetkit://quick-actions-widget/incognito"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  std::string expected_url_string =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(params.externalURL.spec(), expected_url_string);
  EXPECT_EQ(params.postOpeningAction, FOCUS_OMNIBOX);
  EXPECT_EQ(params.applicationMode, ApplicationModeForTabOpening::INCOGNITO);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 3, 1);
}

// Tests that quick actions widget voice search url is parsed correctly, and the
// right metric is recorded.
TEST_F(AppStartupParametersTest, ParseQuickActionsWidgetKitVoiceSearch) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL
      URLWithString:@"chromewidgetkit://quick-actions-widget/voicesearch"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  std::string expected_url_string =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(params.externalURL.spec(), expected_url_string);
  EXPECT_EQ(params.postOpeningAction, START_VOICE_SEARCH);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 4, 1);
}

// Tests that quick actions widget QR reader url is parsed correctly, and the
// right metric is recorded.
TEST_F(AppStartupParametersTest, ParseQuickActionsWidgetKitQRReader) {
  base::HistogramTester histogram_tester;
  NSURL* url =
      [NSURL URLWithString:@"chromewidgetkit://quick-actions-widget/qrreader"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  std::string expected_url_string =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(params.externalURL.spec(), expected_url_string);
  EXPECT_EQ(params.postOpeningAction, START_QR_CODE_SCANNER);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 5, 1);
}

// Tests that quick actions widget Lens url is parsed correctly, and the
// right metric is recorded.
TEST_F(AppStartupParametersTest, ParseQuickActionsWidgetKitLens) {
  base::HistogramTester histogram_tester;
  NSURL* url =
      [NSURL URLWithString:@"chromewidgetkit://quick-actions-widget/lens"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  std::string expected_url_string =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(params.externalURL.spec(), expected_url_string);
  EXPECT_EQ(params.postOpeningAction, START_LENS_FROM_HOME_SCREEN_WIDGET);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 10, 1);
}

// Tests that dino widget game url is parsed correctly, and the right metric is
// recorded.
TEST_F(AppStartupParametersTest, ParseDinoWidgetKit) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL URLWithString:@"chromewidgetkit://dino-game-widget/game"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  GURL expected_url =
      GURL(base::StringPrintf("%s://%s", kChromeUIScheme, kChromeUIDinoHost));

  EXPECT_EQ(params.externalURL, expected_url);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 0, 1);
}

// Tests that the lockscreen launcher widget search url is handled correctly.
TEST_F(AppStartupParametersTest, ParseLockscreenLauncherSearch) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL
      URLWithString:@"chromewidgetkit://lockscreen-launcher-widget/search"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  std::string expected_url_string =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(params.externalURL.spec(), expected_url_string);
  EXPECT_EQ(params.postOpeningAction, FOCUS_OMNIBOX);
  EXPECT_NE(params.applicationMode, ApplicationModeForTabOpening::INCOGNITO);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 6, 1);
}

// Tests that the lockscreen launcher widget incognito url is handled correctly.
TEST_F(AppStartupParametersTest, ParseLockscreenLauncherIncognito) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL
      URLWithString:@"chromewidgetkit://lockscreen-launcher-widget/incognito"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  std::string expected_url_string =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(params.externalURL.spec(), expected_url_string);
  EXPECT_EQ(params.postOpeningAction, FOCUS_OMNIBOX);
  EXPECT_EQ(params.applicationMode, ApplicationModeForTabOpening::INCOGNITO);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 7, 1);
}

// Tests that the lockscreen launcher widget voice search url is
// handled correctly.
TEST_F(AppStartupParametersTest, ParseLockscreenLauncherVoiceSearch) {
  base::HistogramTester histogram_tester;
  NSURL* url =
      [NSURL URLWithString:
                 @"chromewidgetkit://lockscreen-launcher-widget/voicesearch"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  std::string expected_url_string =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(params.externalURL.spec(), expected_url_string);
  EXPECT_EQ(params.postOpeningAction, START_VOICE_SEARCH);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 8, 1);
}

// Tests that the lockscreen launcher widget game url is handled correctly.
TEST_F(AppStartupParametersTest, ParseLockscreenLauncherGame) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL
      URLWithString:@"chromewidgetkit://lockscreen-launcher-widget/game"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil];

  GURL expected_url =
      GURL(base::StringPrintf("%s://%s", kChromeUIScheme, kChromeUIDinoHost));

  EXPECT_EQ(params.externalURL, expected_url);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 9, 1);
}

}  // namespace
