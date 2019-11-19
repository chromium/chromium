// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/application_delegate/url_opener.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "ios/chrome/app/application_delegate/app_state.h"
#include "ios/chrome/app/application_delegate/mock_tab_opener.h"
#include "ios/chrome/app/application_delegate/startup_information.h"
#include "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface_provider.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/test/web_task_environment.h"
#include "net/base/mac/url_conversions.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// URLOpenerTest is parameterized on this enum to test with
// enabled and disabled kExternalFilesLoadedInWebState feature flag.
enum class ExternalFilesLoadedInWebStateFeature {
  Disabled = 0,
  Enabled,
};

@interface StubStartupInformation : NSObject <StartupInformation>
@end
@implementation StubStartupInformation
@synthesize isPresentingFirstRunUI = _isPresentingFirstRunUI;
@synthesize isColdStart = _isColdStart;
@synthesize startupParameters = _startupParameters;
@synthesize appLaunchTime = _appLaunchTime;
- (FirstUserActionRecorder*)firstUserActionRecorder {
  return nil;
}

- (void)resetFirstUserActionRecorder {
}

- (void)expireFirstUserActionRecorder {
}

- (void)expireFirstUserActionRecorderAfterDelay:(NSTimeInterval)delay {
}

- (void)activateFirstUserActionRecorderWithBackgroundTime:
    (NSTimeInterval)backgroundTime {
}

- (void)stopChromeMain {
}

@end

class URLOpenerTest : public PlatformTest {
 private:
  web::WebTaskEnvironment task_environment_;
};

TEST_F(URLOpenerTest, HandleOpenURL) {
  // A set of tests for robustness of
  // application:openURL:options:tabOpener:startupInformation:
  // It verifies that the function handles correctly different URLs parsed by
  // ChromeAppStartupParameters.

  id<StartupInformation> startupInformation =
      [[StubStartupInformation alloc] init];

  // The array with the different states to tests (active, not active).
  NSArray* applicationStatesToTest = @[ @YES, @NO ];

  // Mock of TabOpening, preventing the creation of a new tab.
  MockTabOpener* tabOpener = [[MockTabOpener alloc] init];

  // The keys for this dictionary is the URL to call openURL:. The value
  // from the key is either YES or NO to indicate if this is a valid URL
  // or not.

  NSDictionary* urlsToTest = @{
    [NSNull null] : @NO,
    @"" : @NO,
    // Tests for http, googlechrome, and chromium scheme URLs.
    @"http://www.google.com/" : @YES,
    @"https://www.google.com/settings/account/" : @YES,
    @"googlechrome://www.google.com/" : @YES,
    @"googlechromes://www.google.com/settings/account/" : @YES,
    @"chromium://www.google.com/" : @YES,
    @"chromiums://www.google.com/settings/account/" : @YES,

    // Google search results page URLs.
    @"https://www.google.com/search?q=pony&"
     "sugexp=chrome,mod=7&sourceid=chrome&ie=UTF-8" : @YES,
    @"googlechromes://www.google.com/search?q=pony&"
     "sugexp=chrome,mod=7&sourceid=chrome&ie=UTF-8" : @YES,

    // Other protocols.
    @"chromium-x-callback://x-callback-url/open?url=https://"
     "www.google.com&x-success=http://success" : @YES,
    @"file://localhost/path/to/file.pdf" : @YES,

    // Invalid format input URL will be ignored.
    @"this.is.not.a.valid.url" : @NO,

    // Valid format but invalid data.
    @"this://is/garbage/but/valid" : @YES
  };

  NSArray* sourcesToTest = @[
    [NSNull null], @"", @"com.google.GoogleMobile", @"com.google.GooglePlus",
    @"com.google.SomeOtherProduct", @"com.apple.mobilesafari",
    @"com.othercompany.otherproduct"
  ];
  // See documentation for |annotation| property in
  // UIDocumentInteractionstartupInformation Class Reference.  The following
  // values are mostly to detect garbage-in situations and ensure that the app
  // won't crash or garbage out.
  NSArray* annotationsToTest = @[
    [NSNull null], [NSArray arrayWithObjects:@"foo", @"bar", nil],
    [NSDictionary dictionaryWithObject:@"bar" forKey:@"foo"],
    @"a string annotation object"
  ];
  for (id urlString in [urlsToTest allKeys]) {
    for (id source in sourcesToTest) {
      for (id annotation in annotationsToTest) {
        for (NSNumber* applicationActive in applicationStatesToTest) {
          BOOL applicationIsActive = [applicationActive boolValue];

          startupInformation.startupParameters = nil;
          [tabOpener resetURL];
          NSURL* testUrl = urlString == [NSNull null]
                               ? nil
                               : [NSURL URLWithString:urlString];
          BOOL isValid = [[urlsToTest objectForKey:urlString] boolValue];
          NSMutableDictionary* options = [[NSMutableDictionary alloc] init];
          if (source != [NSNull null]) {
            [options setObject:source
                        forKey:UIApplicationOpenURLOptionsSourceApplicationKey];
          }
          if (annotation != [NSNull null]) {
            [options setObject:annotation
                        forKey:UIApplicationOpenURLOptionsAnnotationKey];
          }
          ChromeAppStartupParameters* params = [ChromeAppStartupParameters
              newChromeAppStartupParametersWithURL:testUrl
                             fromSourceApplication:nil];

          // Action.
          BOOL result = [URLOpener openURL:testUrl
                         applicationActive:applicationIsActive
                                   options:options
                                 tabOpener:tabOpener
                        startupInformation:startupInformation];

          // Tests.
          EXPECT_EQ(isValid, result);
          if (!applicationIsActive) {
            if (result)
              EXPECT_EQ([params externalURL],
                        startupInformation.startupParameters.externalURL);
            else
              EXPECT_EQ(nil, startupInformation.startupParameters);
          } else if (result) {
            if ([params completeURL].SchemeIsFile()) {
              // External file:// URL will be loaded by WebState, which expects
              // complete // file:// URL. chrome:// URL is expected to be
              // displayed in the omnibox, and omnibox shows virtual URL.
              EXPECT_EQ([params completeURL],
                        tabOpener.urlLoadParams.web_params.url);
              EXPECT_EQ([params externalURL],
                        tabOpener.urlLoadParams.web_params.virtual_url);
            } else {
              // External chromium-x-callback:// URL will be loaded by
              // WebState, which expects externalURL URL.
              EXPECT_EQ([params externalURL],
                        tabOpener.urlLoadParams.web_params.url);
            }
            tabOpener.completionBlock();
            EXPECT_EQ(nil, startupInformation.startupParameters);
          }
        }
      }
    }
  }
}

// Tests that -handleApplication set startup parameters as expected.
TEST_F(URLOpenerTest, VerifyLaunchOptions) {
  // Setup.
  NSURL* url = [NSURL URLWithString:@"chromium://www.google.com"];
  NSDictionary* launchOptions = @{
    UIApplicationLaunchOptionsURLKey : url,
    UIApplicationLaunchOptionsSourceApplicationKey : @"com.apple.mobilesafari"
  };

  id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[[startupInformationMock expect] andReturnValue:@NO] isPresentingFirstRunUI];
  [[startupInformationMock expect] resetFirstUserActionRecorder];
  __block ChromeAppStartupParameters* params = nil;
  [[startupInformationMock expect]
      setStartupParameters:[OCMArg checkWithBlock:^(
                                       ChromeAppStartupParameters* p) {
        params = p;
        EXPECT_NSEQ(net::NSURLWithGURL(p.completeURL), url);
        EXPECT_EQ(p.callerApp, CALLER_APP_APPLE_MOBILESAFARI);
        return YES;
      }]];
  [[[startupInformationMock expect] andReturn:params] startupParameters];

  id appStateMock = [OCMockObject mockForClass:[AppState class]];
  [[appStateMock expect] launchFromURLHandled:NO];

  // Action.
  [URLOpener handleLaunchOptions:launchOptions
               applicationActive:NO
                       tabOpener:tabOpenerMock
              startupInformation:startupInformationMock
                        appState:appStateMock];

  // Test.
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
}

// Tests that -handleApplication set startup parameters as expected with options
// as nil.
TEST_F(URLOpenerTest, VerifyLaunchOptionsNil) {
  // Creates a mock with no stub. This test will pass only if we don't use these
  // objects.
  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  id appStateMock = [OCMockObject mockForClass:[AppState class]];

  // Action.
  [URLOpener handleLaunchOptions:nil
               applicationActive:YES
                       tabOpener:nil
              startupInformation:startupInformationMock
                        appState:appStateMock];
}

// Tests that -handleApplication set startup parameters as expected with no
// source application.
TEST_F(URLOpenerTest, VerifyLaunchOptionsWithNoSourceApplication) {
  // Setup.
  NSURL* url = [NSURL URLWithString:@"chromium://www.google.com"];
  NSDictionary* launchOptions = @{
    UIApplicationLaunchOptionsURLKey : url,
  };

  MockTabOpener* tabOpenerMock = [[MockTabOpener alloc] init];

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[[startupInformationMock expect] andReturnValue:@NO] isPresentingFirstRunUI];
  __block ChromeAppStartupParameters* params = nil;
  [[startupInformationMock expect]
      setStartupParameters:[OCMArg checkWithBlock:^(
                                       ChromeAppStartupParameters* p) {
        params = p;
        EXPECT_NSEQ(net::NSURLWithGURL(p.completeURL), url);
        EXPECT_EQ(p.callerApp, CALLER_APP_NOT_AVAILABLE);
        return YES;
      }]];
#if DCHECK_IS_ON()
  // This function is called in a DCHECK.
  [[[startupInformationMock expect] andReturn:params] startupParameters];
#endif

  id appStateMock = [OCMockObject mockForClass:[AppState class]];
  [[appStateMock expect] launchFromURLHandled:YES];

  // Action.
  [URLOpener handleLaunchOptions:launchOptions
               applicationActive:YES
                       tabOpener:tabOpenerMock
              startupInformation:startupInformationMock
                        appState:appStateMock];

  // Test.
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
}

// Tests that -handleApplication set startup parameters as expected with no url.
TEST_F(URLOpenerTest, VerifyLaunchOptionsWithNoURL) {
  // Setup.
  NSDictionary* launchOptions = @{
    UIApplicationLaunchOptionsSourceApplicationKey : @"com.apple.mobilesafari"
  };

  // Creates a mock with no stub. This test will pass only if we don't use these
  // objects.
  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  id appStateMock = [OCMockObject mockForClass:[AppState class]];

  // Action.
  [URLOpener handleLaunchOptions:launchOptions
               applicationActive:YES
                       tabOpener:nil
              startupInformation:startupInformationMock
                        appState:appStateMock];
}

// Tests that -handleApplication set startup parameters as expected with a bad
// url.
TEST_F(URLOpenerTest, VerifyLaunchOptionsWithBadURL) {
  // Setup.
  NSURL* url = [NSURL URLWithString:@"chromium.www.google.com"];
  NSDictionary* launchOptions = @{
    UIApplicationLaunchOptionsURLKey : url,
    UIApplicationLaunchOptionsSourceApplicationKey : @"com.apple.mobilesafari"
  };

  id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[startupInformationMock expect] resetFirstUserActionRecorder];
  [[[startupInformationMock expect] andReturnValue:@NO] isPresentingFirstRunUI];
  [[startupInformationMock expect] setStartupParameters:[OCMArg isNil]];
  [[[startupInformationMock expect] andReturn:nil] startupParameters];

  id appStateMock = [OCMockObject mockForClass:[AppState class]];
  [[appStateMock expect] launchFromURLHandled:NO];

  // Action.
  [URLOpener handleLaunchOptions:launchOptions
               applicationActive:NO
                       tabOpener:tabOpenerMock
              startupInformation:startupInformationMock
                        appState:appStateMock];

  // Test.
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
}
