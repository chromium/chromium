// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/url_opener.h"

#import <Foundation/Foundation.h>

#import "base/check_op.h"
#import "base/test/with_feature_override.h"
#import "ios/chrome/app/application_delegate/mock_tab_opener.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/url_opener_params.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_connection_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/testing/open_url_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// URLOpenerTest is parameterized on this enum to test with
// enabled and disabled kExternalFilesLoadedInWebState feature flag.
enum class ExternalFilesLoadedInWebStateFeature {
  Disabled = 0,
  Enabled,
};

#pragma mark - stubs and test fakes

@interface StubStartupInformation : NSObject <StartupInformation>
@end
@implementation StubStartupInformation
@synthesize isFirstRun = _isFirstRun;
@synthesize isColdStart = _isColdStart;
@synthesize appLaunchTime = _appLaunchTime;
@synthesize didFinishLaunchingTime = _didFinishLaunchingTime;
@synthesize firstSceneConnectionTime = _firstSceneConnectionTime;

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

- (NSDictionary*)launchOptions {
  return @{};
}

@end

#pragma mark -

class URLOpenerTest : public PlatformTest {
 protected:
  URLOpenerTest() {}

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
  id<ConnectionInformation> connectionInformation =
      [[FakeConnectionInformation alloc] init];

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
    @"", @"com.google.GoogleMobile", @"com.google.GooglePlus",
    @"com.google.SomeOtherProduct", @"com.apple.mobilesafari",
    @"com.othercompany.otherproduct"
  ];
  // See documentation for `annotation` property in
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
          connectionInformation.startupParameters = nil;
          [tabOpener resetURL];
          NSURL* testUrl = urlString == [NSNull null]
                               ? nil
                               : [NSURL URLWithString:urlString];
          BOOL isValid = [[urlsToTest objectForKey:urlString] boolValue];

          TestSceneOpenURLOptions* options =
              [[TestSceneOpenURLOptions alloc] init];
          options.sourceApplication = source;
          options.annotation = annotation;

          TestOpenURLContext* context = [[TestOpenURLContext alloc] init];
          context.URL = testUrl;
          context.options = (id)options;  //< Unsafe cast intended.

          URLOpenerParams* urlOpenerParams = [[URLOpenerParams alloc]
              initWithUIOpenURLContext:(id)context];  //< Unsafe cast intended.

          ChromeAppStartupParameters* params =
              [ChromeAppStartupParameters startupParametersWithURL:testUrl
                                                 sourceApplication:nil];

          // Action.
          BOOL result = [URLOpener openURL:urlOpenerParams
                         applicationActive:applicationIsActive
                                 tabOpener:tabOpener
                     connectionInformation:connectionInformation
                        startupInformation:startupInformation
                               prefService:nil
                                 initStage:ProfileInitStage::kFinal];

          // Tests.
          EXPECT_EQ(isValid, result);
          if (!applicationIsActive) {
            if (result)
              EXPECT_EQ([params externalURL],
                        connectionInformation.startupParameters.externalURL);
            else
              EXPECT_EQ(nil, connectionInformation.startupParameters);
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
            EXPECT_EQ(nil, connectionInformation.startupParameters);
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
  URLOpenerParams* urlOpenerParams =
      [[URLOpenerParams alloc] initWithURL:url
                         sourceApplication:@"com.apple.mobilesafari"];

  id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[startupInformationMock expect] resetFirstUserActionRecorder];
  id connectionInformationMock =
      [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];
  __block ChromeAppStartupParameters* params = nil;
  [[connectionInformationMock expect]
      setStartupParameters:[OCMArg checkWithBlock:^(
                                       ChromeAppStartupParameters* p) {
        params = p;
        EXPECT_NSEQ(net::NSURLWithGURL(p.completeURL), url);
        EXPECT_EQ(p.callerApp, CALLER_APP_APPLE_MOBILESAFARI);
        return YES;
      }]];
  [[[connectionInformationMock expect] andReturn:params] startupParameters];

  // Action.
  [URLOpener handleLaunchOptions:urlOpenerParams
                       tabOpener:tabOpenerMock
           connectionInformation:connectionInformationMock
              startupInformation:startupInformationMock
                     prefService:nil
                       initStage:ProfileInitStage::kFinal];

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
  id connectionInformationMock =
      [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];

  // Action.
  [URLOpener handleLaunchOptions:nil
                       tabOpener:nil
           connectionInformation:connectionInformationMock
              startupInformation:startupInformationMock
                     prefService:nil
                       initStage:ProfileInitStage::kStart];
}

// Tests that -handleApplication set startup parameters as expected with no
// source application.
TEST_F(URLOpenerTest, VerifyLaunchOptionsWithNoSourceApplication) {
  // Setup.
  NSURL* url = [NSURL URLWithString:@"chromium://www.google.com"];
  URLOpenerParams* urlOpenerParams = [[URLOpenerParams alloc] initWithURL:url
                                                        sourceApplication:nil];

  MockTabOpener* tabOpenerMock = [[MockTabOpener alloc] init];

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[startupInformationMock expect] resetFirstUserActionRecorder];
  id connectionInformationMock =
      [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];
  __block ChromeAppStartupParameters* params = nil;
  [[connectionInformationMock expect]
      setStartupParameters:[OCMArg checkWithBlock:^(
                                       ChromeAppStartupParameters* p) {
        params = p;
        EXPECT_NSEQ(net::NSURLWithGURL(p.completeURL), url);
        EXPECT_EQ(p.callerApp, CALLER_APP_NOT_AVAILABLE);
        return YES;
      }]];
  [[[connectionInformationMock expect] andReturn:params] startupParameters];

  // Action.
  [URLOpener handleLaunchOptions:urlOpenerParams
                       tabOpener:tabOpenerMock
           connectionInformation:connectionInformationMock
              startupInformation:startupInformationMock
                     prefService:nil
                       initStage:ProfileInitStage::kFinal];

  // Test.
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
}

// Tests that -handleApplication set startup parameters as expected with no url.
TEST_F(URLOpenerTest, VerifyLaunchOptionsWithNoURL) {
  // Setup.
  URLOpenerParams* urlOpenerParams =
      [[URLOpenerParams alloc] initWithURL:nil
                         sourceApplication:@"com.apple.mobilesafari"];

  // Creates a mock with no stub. This test will pass only if we don't use these
  // objects.
  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  id connectionInformationMock =
      [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];

  // Action.
  [URLOpener handleLaunchOptions:urlOpenerParams
                       tabOpener:nil
           connectionInformation:connectionInformationMock
              startupInformation:startupInformationMock
                     prefService:nil
                       initStage:ProfileInitStage::kStart];
}

// Tests that -handleApplication set startup parameters as expected with a bad
// url.
TEST_F(URLOpenerTest, VerifyLaunchOptionsWithBadURL) {
  // Setup.
  NSURL* url = [NSURL URLWithString:@"chromium.www.google.com"];
  URLOpenerParams* urlOpenerParams =
      [[URLOpenerParams alloc] initWithURL:url
                         sourceApplication:@"com.apple.mobilesafari"];

  id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[startupInformationMock expect] resetFirstUserActionRecorder];

  id connectionInformationMock =
      [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];
  [[connectionInformationMock expect] setStartupParameters:[OCMArg isNil]];
  [[[connectionInformationMock expect] andReturn:nil] startupParameters];

  // Action.
  [URLOpener handleLaunchOptions:urlOpenerParams
                       tabOpener:tabOpenerMock
           connectionInformation:connectionInformationMock
              startupInformation:startupInformationMock
                     prefService:nil
                       initStage:ProfileInitStage::kFinal];

  // Test.
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
}

// Tests URL is not opened if the FRE is presented.
TEST_F(URLOpenerTest, PresentingFirstRunUI) {
  // Setup.
  NSURL* url = [NSURL URLWithString:@"chromium://www.google.com"];
  URLOpenerParams* urlOpenerParams =
      [[URLOpenerParams alloc] initWithURL:url
                         sourceApplication:@"com.apple.mobilesafari"];
  id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];
  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  id connectionInformationMock =
      [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];
  __block ChromeAppStartupParameters* params = nil;
  [[connectionInformationMock expect]
      setStartupParameters:[OCMArg checkWithBlock:^(
                                       ChromeAppStartupParameters* p) {
        params = p;
        EXPECT_NSEQ(net::NSURLWithGURL(p.completeURL), url);
        EXPECT_EQ(p.callerApp, CALLER_APP_APPLE_MOBILESAFARI);
        return YES;
      }]];
  [[[connectionInformationMock expect] andReturn:params] startupParameters];

  // Action.
  [URLOpener handleLaunchOptions:urlOpenerParams
                       tabOpener:tabOpenerMock
           connectionInformation:connectionInformationMock
              startupInformation:startupInformationMock
                     prefService:nil
                       initStage:ProfileInitStage::kFirstRun];

  // Test.
  EXPECT_OCMOCK_VERIFY(tabOpenerMock);
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
}
