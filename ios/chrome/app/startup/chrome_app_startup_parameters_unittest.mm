// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/chrome_app_startup_parameters.h"

#import <Foundation/Foundation.h>

#import "base/strings/stringprintf.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {
void CheckLaunchSourceForURL(first_run::ExternalLaunch expectedSource,
                             NSString* urlString) {
  NSURL* url = [NSURL URLWithString:urlString];
  ChromeAppStartupParameters* params = [ChromeAppStartupParameters
      startupParametersWithURL:url
             sourceApplication:@"com.apple.mobilesafari"];
  EXPECT_EQ(expectedSource, [params launchSource]);
}

typedef PlatformTest AppStartupParametersTest;
TEST_F(PlatformTest, ParseURLWithEmptyURL) {
  NSURL* url = [NSURL URLWithString:@""];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithOneProtocol) {
  NSURL* url = [NSURL URLWithString:@"protocol://www.google.com"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];
  // Here "protocol" opens the app and no protocol is given for the parsed URL,
  // which defaults to be "http".
  EXPECT_EQ("http://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithEmptyParsedURL) {
  // Test chromium://
  NSURL* url = [NSURL URLWithString:@"chromium://"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithParsedURLDefaultToHttp) {
  NSURL* url = [NSURL URLWithString:@"chromium://www.google.com"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ("http://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithInvalidParsedURL) {
  NSURL* url = [NSURL URLWithString:@"http:google.com:foo"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithHttpsParsedURL) {
  NSURL* url = [NSURL URLWithString:@"chromiums://www.google.com"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ("https://www.google.com/", [params externalURL].spec());
}

// Tests that http url remains unchanged.
TEST_F(AppStartupParametersTest, ParseURLWithHttpURL) {
  NSURL* url = [NSURL URLWithString:@"http://www.google.com"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ("http://www.google.com/", [params externalURL]);
}

// Tests that https url remains unchanged.
TEST_F(AppStartupParametersTest, ParseURLWithHttpsURL) {
  NSURL* url = [NSURL URLWithString:@"https://www.google.com"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ("https://www.google.com/", [params externalURL]);
}

TEST_F(AppStartupParametersTest, ParseURLWithXCallbackURL) {
  NSURL* url =
      [NSURL URLWithString:@"chromium-x-callback://x-callback-url/open?"
                            "url=https://www.google.com"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];
  EXPECT_EQ("https://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithXCallbackURLAndExtraParams) {
  NSURL* url =
      [NSURL URLWithString:@"chromium-x-callback://x-callback-url/open?"
                            "url=https://www.google.com&"
                            "x-success=http://success"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];
  EXPECT_EQ("https://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithMalformedXCallbackURL) {
  NSURL* url = [NSURL
      URLWithString:@"chromium-x-callback://x-callback-url/open?url=foobar&"
                     "x-source=myapp&x-success=http://success"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:@"com.myapp"];
  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithJavascriptURLInXCallbackURL) {
  NSURL* url = [NSURL
      URLWithString:
          @"chromium-x-callback://x-callback-url/open?url="
           "javascript:window.open()&x-source=myapp&x-success=http://success"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:@"com.myapp"];
  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithChromeURLInXCallbackURL) {
  NSURL* url =
      [NSURL URLWithString:@"chromium-x-callback://x-callback-url/open?url="
                            "chrome:passwords"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:@"com.myapp"];
  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithFileParsedURL) {
  NSURL* url = [NSURL URLWithString:@"file://localhost/path/to/file.pdf"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  std::string expected_url_string = base::StringPrintf(
      "%s://%s/file.pdf", kChromeUIScheme, kChromeUIExternalFileHost);

  EXPECT_EQ(expected_url_string, [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupVoiceSearch) {
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersForCommand:@"voicesearch"
                                             withExternalText:nil
                                                 externalData:nil
                                                        index:0
                                                          URL:nil
                                            sourceApplication:nil
                                      secureSourceApplication:nil];

  std::string expected_url_string =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(expected_url_string, [params externalURL].spec());
  EXPECT_EQ([params postOpeningAction], START_VOICE_SEARCH);
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupQRCode) {
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersForCommand:@"qrscanner"
                                             withExternalText:nil
                                                 externalData:nil
                                                        index:0
                                                          URL:nil
                                            sourceApplication:nil
                                      secureSourceApplication:nil];

  std::string expected_url_string =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(expected_url_string, [params externalURL].spec());
  EXPECT_EQ([params postOpeningAction], START_QR_CODE_SCANNER);
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupFocusOmbnibox) {
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersForCommand:@"focusomnibox"
                                             withExternalText:nil
                                                 externalData:nil
                                                        index:0
                                                          URL:nil
                                            sourceApplication:nil
                                      secureSourceApplication:nil];

  std::string expected_url_string =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(expected_url_string, [params externalURL].spec());
  EXPECT_EQ([params postOpeningAction], FOCUS_OMNIBOX);
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupNewTab) {
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersForCommand:@"newtab"
                                             withExternalText:nil
                                                 externalData:nil
                                                        index:0
                                                          URL:nil
                                            sourceApplication:nil
                                      secureSourceApplication:nil];
  std::string expected_url_string =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(expected_url_string, [params externalURL].spec());
  EXPECT_EQ([params postOpeningAction], NO_ACTION);
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupOpenURL) {
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersForCommand:@"openurl"
                                             withExternalText:@"http://foo/bar"
                                                 externalData:nil
                                                        index:0
                                                          URL:nil
                                            sourceApplication:nil
                                      secureSourceApplication:nil];

  EXPECT_EQ("http://foo/bar", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupGarbage) {
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersForCommand:@"garbage"
                                             withExternalText:nil
                                                 externalData:nil
                                                        index:0
                                                          URL:nil
                                            sourceApplication:nil
                                      secureSourceApplication:nil];
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
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

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
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

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
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

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
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

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
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

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
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_TRUE(params.externalURL.is_empty());
  EXPECT_EQ(params.postOpeningAction, START_LENS_FROM_HOME_SCREEN_WIDGET);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 10, 1);
}

// Tests that shortcuts action search is parsed correctly, and the
// right metric is recorded.
TEST_F(AppStartupParametersTest, ParseShortcutWidgetSearch) {
  base::HistogramTester histogram_tester;
  NSURL* url =
      [NSURL URLWithString:@"chromewidgetkit://shortcuts-widget/search"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ(params.externalURL, kChromeUINewTabURL);
  EXPECT_EQ(params.postOpeningAction, FOCUS_OMNIBOX);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 11, 1);
}

// Tests that shortcuts action open is parsed correctly, and the
// right metric is recorded.
TEST_F(AppStartupParametersTest, ParseShortcutWidgetOpen) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL URLWithString:@"chromewidgetkit://shortcuts-widget/"
                                    @"open?url=https://www.example.org"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ(params.externalURL, "https://www.example.org/");
  EXPECT_EQ(params.postOpeningAction, NO_ACTION);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 12, 1);
}

// Tests that shortcuts action open with invalid URL is parsed correctly, and
// no metric is recorded.
TEST_F(AppStartupParametersTest, ParseShortcutWidgetOpenInvalid) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL
      URLWithString:@"chromewidgetkit://shortcuts-widget/open?url=not_a_url"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ(params, nil);
  histogram_tester.ExpectTotalCount("IOS.WidgetKit.Action", 0);
}

// Tests that dino widget game url is parsed correctly, and the right metric is
// recorded.
TEST_F(AppStartupParametersTest, ParseDinoWidgetKit) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL URLWithString:@"chromewidgetkit://dino-game-widget/game"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

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
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

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
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

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
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

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
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  GURL expected_url =
      GURL(base::StringPrintf("%s://%s", kChromeUIScheme, kChromeUIDinoHost));

  EXPECT_EQ(params.externalURL, expected_url);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 9, 1);
}

// Tests that search passwords widget url is parsed correctly, and the right
// metric is recorded.
TEST_F(AppStartupParametersTest, ParseSearchPasswordsWidgetKit) {
  base::HistogramTester histogram_tester;
  NSURL* url =
      [NSURL URLWithString:
                 @"chromewidgetkit://search-passwords-widget/search-passwords"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_TRUE(params.externalURL.is_empty());
  EXPECT_EQ(params.postOpeningAction, SEARCH_PASSWORDS);
  EXPECT_NE(params.applicationMode, ApplicationModeForTabOpening::INCOGNITO);
  histogram_tester.ExpectUniqueSample("IOS.WidgetKit.Action", 13, 1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kSearchPasswordsWidget, 1);
}

// Tests that the external action scheme is handled correctly with the "OpenNTP"
// action.
TEST_F(AppStartupParametersTest, ExternalActionSchemeOpenNTP) {
  ClearDefaultBrowserPromoData();

  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL
      URLWithString:@"googlechromes://ChromeExternalAction/OpenNTP?test=2"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ(params.externalURL, GURL("chrome://newtab/"));
  histogram_tester.ExpectBucketCount("IOS.LaunchSource",
                                     AppLaunchSource::EXTERNAL_ACTION, 1);
  histogram_tester.ExpectBucketCount("IOS.ExternalAction",
                                     /*ACTION_OPEN_NTP*/ 1, 1);
}

// Tests that the external action scheme is handled correctly with the
// "DefaultBrowserSettings" action.
TEST_F(AppStartupParametersTest, ExternalActionSchemeDefaultBrowserSettings) {
  ClearDefaultBrowserPromoData();

  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL
      URLWithString:
          @"googlechrome://ChromeExternalAction/DefaultBrowserSettings?test=3"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ(params.postOpeningAction, EXTERNAL_ACTION_SHOW_BROWSER_SETTINGS);
  EXPECT_TRUE(params.externalURL.is_empty());
  histogram_tester.ExpectBucketCount("IOS.LaunchSource",
                                     AppLaunchSource::EXTERNAL_ACTION, 1);
  histogram_tester.ExpectBucketCount("IOS.ExternalAction",
                                     /*ACTION_DEFAULT_BROWSER_SETTINGS*/ 2, 1);
}

// Tests that the external action scheme is handled correctly with the
// "DefaultBrowserSettings" action, but Chrome is already default browser.
TEST_F(AppStartupParametersTest,
       ExternalActionSchemeDefaultBrowserSettingsChromeAlreadyDefaultBrowser) {
  LogOpenHTTPURLFromExternalURL();

  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL
      URLWithString:
          @"googlechrome://ChromeExternalAction/DefaultBrowserSettings?test=3"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ(params.externalURL, GURL("chrome://newtab/"));
  histogram_tester.ExpectBucketCount("IOS.LaunchSource",
                                     AppLaunchSource::EXTERNAL_ACTION, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.ExternalAction",
      /*ACTION_SKIPPED_DEFAULT_BROWSER_SETTINGS_FOR_NTP*/ 3, 1);
}

// Tests that the external action scheme is handled with a Chromium-flavored
// URL.
TEST_F(AppStartupParametersTest, ExternalActionSchemeChromiumURLHandled) {
  ClearDefaultBrowserPromoData();

  base::HistogramTester histogram_tester;
  NSURL* url =
      [NSURL URLWithString:@"chromium://ChromeExternalAction/OpenNTP?test=4"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ(params.externalURL, GURL("chrome://newtab/"));
  histogram_tester.ExpectBucketCount("IOS.LaunchSource",
                                     AppLaunchSource::EXTERNAL_ACTION, 1);
  histogram_tester.ExpectBucketCount("IOS.ExternalAction",
                                     /*ACTION_OPEN_NTP*/ 1, 1);
}

// Tests that the external action scheme does nothing when passed an invalid
// action.
TEST_F(AppStartupParametersTest, ExternalActionSchemeInvalidAction) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL
      URLWithString:@"googlechromes://ChromeExternalAction/invalid?test=5"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ(params, nil);
  histogram_tester.ExpectBucketCount("IOS.LaunchSource",
                                     AppLaunchSource::EXTERNAL_ACTION, 1);
  histogram_tester.ExpectBucketCount("IOS.ExternalAction", /*ACTION_INVALID*/ 0,
                                     1);
}

// Tests that the external action scheme does nothing when passed an invalid
// action (long path).
TEST_F(AppStartupParametersTest, ExternalActionSchemeInvalidActionLongPath) {
  base::HistogramTester histogram_tester;
  NSURL* url =
      [NSURL URLWithString:
                 @"googlechromes://ChromeExternalAction/long/path/test?test=5"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ(params, nil);
  histogram_tester.ExpectBucketCount("IOS.LaunchSource",
                                     AppLaunchSource::EXTERNAL_ACTION, 1);
  histogram_tester.ExpectBucketCount("IOS.ExternalAction", /*ACTION_INVALID*/ 0,
                                     1);
}

// Tests that the external action scheme does nothing when passed an invalid
// action (no action passed).
TEST_F(AppStartupParametersTest, ExternalActionSchemeInvalidActionNoAction) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL URLWithString:@"googlechromes://ChromeExternalAction/"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ(params, nil);
  histogram_tester.ExpectBucketCount("IOS.LaunchSource",
                                     AppLaunchSource::EXTERNAL_ACTION, 1);
  histogram_tester.ExpectBucketCount("IOS.ExternalAction", /*ACTION_INVALID*/ 0,
                                     1);
}

// Tests that the external action scheme does nothing when passed an invalid
// action (no path passed).
TEST_F(AppStartupParametersTest, ExternalActionSchemeInvalidActionNoPath) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL URLWithString:@"googlechrome://ChromeExternalAction"];
  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:url
                                         sourceApplication:nil];

  EXPECT_EQ(params, nil);
  histogram_tester.ExpectBucketCount("IOS.LaunchSource",
                                     AppLaunchSource::EXTERNAL_ACTION, 1);
  histogram_tester.ExpectBucketCount("IOS.ExternalAction", /*ACTION_INVALID*/ 0,
                                     1);
}

}  // namespace
