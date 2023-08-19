// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/user_activity_handler.h"

#import <memory>

#import <CoreSpotlight/CoreSpotlight.h>

#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_command_line.h"
#import "base/test/task_environment.h"
#import "base/test/with_feature_override.h"
#import "components/handoff/handoff_utility.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/app/application_delegate/mock_tab_opener.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/app/spotlight/actions_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_connection_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/common/intents/OpenInChromeIncognitoIntent.h"
#import "ios/chrome/common/intents/OpenInChromeIntent.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/base/mac/url_conversions.h"
#import "net/test/gtest_util.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

#pragma mark - Test class.

// A block that takes as arguments the caller and the arguments from
// UserActivityHandler +handleStartupParameters and returns nothing.
typedef void (^startupParameterBlock)(id,
                                      id<TabOpening>,
                                      id<StartupInformation>,
                                      id<BrowserProviderInterface>);

// A block that takes a BOOL argument and returns nothing.
typedef void (^conditionBlock)(BOOL);

class UserActivityHandlerTest : public PlatformTest {
 public:
  UserActivityHandlerTest() {
    browserProviderInterface_ = [[StubBrowserProviderInterface alloc] init];
  }

 protected:
  void swizzleHandleStartupParameters() {
    handle_startup_parameters_has_been_called_ = NO;
    swizzle_block_ = [^(id self) {
      handle_startup_parameters_has_been_called_ = YES;
    } copy];
    user_activity_handler_swizzler_.reset(new ScopedBlockSwizzler(
        [UserActivityHandler class],
        @selector(handleStartupParametersWithTabOpener:
                                 connectionInformation:startupInformation
                                                      :browserState:initStage:),
        swizzle_block_));
  }

  BOOL getHandleStartupParametersHasBeenCalled() {
    return handle_startup_parameters_has_been_called_;
  }

  void resetHandleStartupParametersHasBeenCalled() {
    handle_startup_parameters_has_been_called_ = NO;
  }

  // Mock & stub a NSUserActivity object with an arbitrary `interaction`
  // property. The object is mock'ed otherwise the same as the `base` parameter
  id CreateMockNSUserActivity(NSUserActivity* base,
                              INInteraction* interaction) {
    id mock_user_activity = OCMClassMock([NSUserActivity class]);
    OCMStub([(NSUserActivity*)mock_user_activity interaction])
        .andReturn(interaction);
    OCMStub([(NSUserActivity*)mock_user_activity webpageURL])
        .andReturn(base.webpageURL);
    OCMStub([(NSUserActivity*)mock_user_activity activityType])
        .andReturn(base.activityType);
    OCMStub([(NSUserActivity*)mock_user_activity userInfo])
        .andReturn(base.userInfo);

    return mock_user_activity;
  }

  conditionBlock getCompletionHandler() {
    if (!completion_block_) {
      block_executed_ = NO;
      completion_block_ = [^(BOOL arg) {
        block_executed_ = YES;
        block_argument_ = arg;
      } copy];
    }
    return completion_block_;
  }

  BOOL completionHandlerExecuted() { return block_executed_; }

  BOOL completionHandlerArgument() { return block_argument_; }

  StubBrowserProviderInterface* GetInterfaceProvider() {
    return browserProviderInterface_;
  }

 private:
  __block BOOL block_executed_;
  __block BOOL block_argument_;
  std::unique_ptr<ScopedBlockSwizzler> user_activity_handler_swizzler_;
  startupParameterBlock swizzle_block_;
  conditionBlock completion_block_;
  __block BOOL handle_startup_parameters_has_been_called_;
  StubBrowserProviderInterface* browserProviderInterface_;
};

#pragma mark - Tests.

// Tests that Chrome notifies the user if we are passing a correct
// userActivityType.
TEST_F(UserActivityHandlerTest, WillContinueUserActivityCorrectActivity) {
  EXPECT_TRUE([UserActivityHandler
      willContinueUserActivityWithType:handoff::kChromeHandoffActivityType]);

  if (spotlight::IsSpotlightAvailable()) {
    EXPECT_TRUE([UserActivityHandler
        willContinueUserActivityWithType:CSSearchableItemActionType]);
  }
}

// Tests that Chrome does not notifies the user if we are passing an incorrect
// userActivityType.
TEST_F(UserActivityHandlerTest, WillContinueUserActivityIncorrectActivity) {
  EXPECT_FALSE([UserActivityHandler
      willContinueUserActivityWithType:[handoff::kChromeHandoffActivityType
                                           stringByAppendingString:@"test"]]);

  EXPECT_FALSE([UserActivityHandler
      willContinueUserActivityWithType:@"it.does.not.work"]);

  EXPECT_FALSE([UserActivityHandler willContinueUserActivityWithType:@""]);

  EXPECT_FALSE([UserActivityHandler willContinueUserActivityWithType:nil]);
}

// Tests that Chrome does not continue the activity is the activity type is
// random.
TEST_F(UserActivityHandlerTest, ContinueUserActivityFromGarbage) {
  // Setup.
  NSString* handoffWithSuffix =
      [handoff::kChromeHandoffActivityType stringByAppendingString:@"test"];
  NSString* handoffWithPrefix =
      [@"test" stringByAppendingString:handoff::kChromeHandoffActivityType];
  NSArray* userActivityTypes = @[
    @"thisIsGarbage", @"it.does.not.work", handoffWithSuffix, handoffWithPrefix
  ];
  for (NSString* userActivityType in userActivityTypes) {
    NSUserActivity* userActivity =
        [[NSUserActivity alloc] initWithActivityType:userActivityType];
    [userActivity setWebpageURL:[NSURL URLWithString:@"http://www.google.com"]];

    // The test will fail is a method of those objects is called.
    id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];
    id startupInformationMock =
        [OCMockObject mockForProtocol:@protocol(StartupInformation)];
    id connectionInformation =
        [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];

    // Action.
    BOOL result =
        [UserActivityHandler continueUserActivity:userActivity
                              applicationIsActive:NO
                                        tabOpener:tabOpenerMock
                            connectionInformation:connectionInformation
                               startupInformation:startupInformationMock
                                     browserState:nullptr
                                        initStage:InitStageFinal];

    // Tests.
    EXPECT_FALSE(result);
  }
}

// Tests that Chrome does not continue the activity if the webpage url is not
// set.
TEST_F(UserActivityHandlerTest, ContinueUserActivityNoWebpage) {
  // Setup.
  NSUserActivity* userActivity = [[NSUserActivity alloc]
      initWithActivityType:handoff::kChromeHandoffActivityType];

  // The test will fail is a method of those objects is called.
  id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];
  id connectionInformationMock =
      [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];
  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];

  // Action.
  BOOL result =
      [UserActivityHandler continueUserActivity:userActivity
                            applicationIsActive:NO
                                      tabOpener:tabOpenerMock
                          connectionInformation:connectionInformationMock
                             startupInformation:startupInformationMock
                                   browserState:nullptr
                                      initStage:InitStageFinal];

  // Tests.
  EXPECT_FALSE(result);
}

// Tests that Chrome does not continue the activity if the activity is a
// Spotlight action of an unknown type.
TEST_F(UserActivityHandlerTest,
       ContinueUserActivitySpotlightActionFromGarbage) {
  // Only test Spotlight if it is enabled and available on the device.
  if (!spotlight::IsSpotlightAvailable()) {
    return;
  }
  // Setup.
  NSUserActivity* userActivity =
      [[NSUserActivity alloc] initWithActivityType:CSSearchableItemActionType];
  NSString* invalidAction =
      [NSString stringWithFormat:@"%@.invalidAction",
                                 spotlight::StringFromSpotlightDomain(
                                     spotlight::DOMAIN_ACTIONS)];
  NSDictionary* userInfo =
      @{CSSearchableItemActivityIdentifier : invalidAction};
  [userActivity addUserInfoEntriesFromDictionary:userInfo];

  // Enable the SpotlightActions experiment.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableSpotlightActions);

  id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];
  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  id connectionInformationMock =
      [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];
  // Action.
  BOOL result =
      [UserActivityHandler continueUserActivity:userActivity
                            applicationIsActive:NO
                                      tabOpener:tabOpenerMock
                          connectionInformation:connectionInformationMock
                             startupInformation:startupInformationMock
                                   browserState:nullptr
                                      initStage:InitStageFinal];

  // Tests.
  EXPECT_FALSE(result);
}

// Tests that Chrome continues the activity if the application is in background
// by saving the url to startupParameters.
TEST_F(UserActivityHandlerTest, ContinueUserActivityBackground) {
  // Setup.
  NSUserActivity* userActivity = [[NSUserActivity alloc]
      initWithActivityType:handoff::kChromeHandoffActivityType];
  NSURL* nsurl = [NSURL URLWithString:@"http://www.google.com"];
  [userActivity setWebpageURL:nsurl];

  id startupInformationMock =
      [OCMockObject niceMockForProtocol:@protocol(StartupInformation)];
  id connectionInformationMock =
      [OCMockObject niceMockForProtocol:@protocol(ConnectionInformation)];
  [[connectionInformationMock expect]
      setStartupParameters:[OCMArg checkWithBlock:^BOOL(id value) {
        EXPECT_TRUE([value isKindOfClass:[AppStartupParameters class]]);

        AppStartupParameters* startupParameters = (AppStartupParameters*)value;
        const GURL calledURL = startupParameters.externalURL;
        return calledURL == net::GURLWithNSURL(nsurl);
      }]];

  // The test will fail is a method of this object is called.
  id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];

  // Action.
  BOOL result =
      [UserActivityHandler continueUserActivity:userActivity
                            applicationIsActive:NO
                                      tabOpener:tabOpenerMock
                          connectionInformation:connectionInformationMock
                             startupInformation:startupInformationMock
                                   browserState:nullptr
                                      initStage:InitStageFinal];

  // Test.
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
  EXPECT_TRUE(result);
}

// Tests that Chrome continues the activity if the application is in foreground
// by opening a new tab.
TEST_F(UserActivityHandlerTest, ContinueUserActivityForeground) {
  // Setup.
  NSUserActivity* userActivity = [[NSUserActivity alloc]
      initWithActivityType:handoff::kChromeHandoffActivityType];
  GURL gurl("http://www.google.com");
  [userActivity setWebpageURL:net::NSURLWithGURL(gurl)];

  MockTabOpener* tabOpener = [[MockTabOpener alloc] init];

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];

  id connectionInformationMock =
      [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];
  AppStartupParameters* startupParams = [[AppStartupParameters alloc]
      initWithExternalURL:gurl
              completeURL:gurl
          applicationMode:ApplicationModeForTabOpening::NORMAL];
  [[[connectionInformationMock stub] andReturn:startupParams]
      startupParameters];

  // Action.
  BOOL result =
      [UserActivityHandler continueUserActivity:userActivity
                            applicationIsActive:YES
                                      tabOpener:tabOpener
                          connectionInformation:connectionInformationMock
                             startupInformation:startupInformationMock
                                   browserState:nullptr
                                      initStage:InitStageFinal];

  // Test.
  EXPECT_EQ(gurl, tabOpener.urlLoadParams.web_params.url);
  EXPECT_TRUE(tabOpener.urlLoadParams.web_params.virtual_url.is_empty());
  EXPECT_TRUE(result);
}

// Tests that a new tab is created when application is started via handoff.
TEST_F(UserActivityHandlerTest, ContinueUserActivityBrowsingWeb) {
  NSUserActivity* userActivity = [[NSUserActivity alloc]
      initWithActivityType:NSUserActivityTypeBrowsingWeb];
  // This URL is passed to application by iOS but is not used in this part
  // of application logic.
  NSURL* nsurl = [NSURL URLWithString:@"http://goo.gl/foo/bar"];
  [userActivity setWebpageURL:nsurl];

  MockTabOpener* tabOpener = [[MockTabOpener alloc] init];

  // Use an object to capture the startup paramters set by UserActivityHandler.
  FakeStartupInformation* fakeStartupInformation =
      [[FakeStartupInformation alloc] init];
  FakeConnectionInformation* connectionInformationMock =
      [[FakeConnectionInformation alloc] init];

  BOOL result =
      [UserActivityHandler continueUserActivity:userActivity
                            applicationIsActive:YES
                                      tabOpener:tabOpener
                          connectionInformation:connectionInformationMock
                             startupInformation:fakeStartupInformation
                                   browserState:nullptr
                                      initStage:InitStageFinal];

  const GURL gurl = net::GURLWithNSURL(nsurl);
  EXPECT_EQ(gurl, tabOpener.urlLoadParams.web_params.url);
  EXPECT_TRUE(tabOpener.urlLoadParams.web_params.virtual_url.is_empty());
  // AppStartupParameters default to opening pages in non-Incognito mode.
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL, [tabOpener applicationMode]);
  EXPECT_TRUE(result);
}

// Tests that continueUserActivity sets startupParameters accordingly to the
// Spotlight action used.
TEST_F(UserActivityHandlerTest, ContinueUserActivityShortcutActions) {
  // Only test Spotlight if it is enabled and available on the device.
  if (!spotlight::IsSpotlightAvailable()) {
    return;
  }
  // Setup.
  GURL gurlNewTab(kChromeUINewTabURL);
  FakeStartupInformation* fakeStartupInformation =
      [[FakeStartupInformation alloc] init];
  FakeConnectionInformation* connectionInformationMock =
      [[FakeConnectionInformation alloc] init];

  NSArray* parametersToTest = @[
    @[
      base::SysUTF8ToNSString(spotlight::kSpotlightActionNewTab), @(NO_ACTION)
    ],
    @[
      base::SysUTF8ToNSString(spotlight::kSpotlightActionNewIncognitoTab),
      @(NO_ACTION)
    ],
    @[
      base::SysUTF8ToNSString(spotlight::kSpotlightActionVoiceSearch),
      @(START_VOICE_SEARCH)
    ],
    @[
      base::SysUTF8ToNSString(spotlight::kSpotlightActionQRScanner),
      @(START_QR_CODE_SCANNER)
    ]
  ];

  // Enable the Spotlight Actions experiment.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableSpotlightActions);

  for (id parameters in parametersToTest) {
    NSUserActivity* userActivity = [[NSUserActivity alloc]
        initWithActivityType:CSSearchableItemActionType];
    NSString* action = [NSString
        stringWithFormat:@"%@.%@", spotlight::StringFromSpotlightDomain(
                                       spotlight::DOMAIN_ACTIONS),
                         parameters[0]];
    NSDictionary* userInfo = @{CSSearchableItemActivityIdentifier : action};
    [userActivity addUserInfoEntriesFromDictionary:userInfo];

    id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];

    // Action.
    BOOL result =
        [UserActivityHandler continueUserActivity:userActivity
                              applicationIsActive:NO
                                        tabOpener:tabOpenerMock
                            connectionInformation:connectionInformationMock
                               startupInformation:fakeStartupInformation
                                     browserState:nullptr
                                        initStage:InitStageFinal];

    // Tests.
    EXPECT_TRUE(result);
    EXPECT_EQ(gurlNewTab,
              [connectionInformationMock startupParameters].externalURL);
    EXPECT_EQ([parameters[1] intValue],
              [connectionInformationMock startupParameters].postOpeningAction);
  }
}

// Tests that Chrome responds to open in incognito intent in the background
TEST_F(UserActivityHandlerTest, ContinueUserActivityIntentIncognitoBackground) {
  NSURL* url1 = [[NSURL alloc] initWithString:@"http://www.google.com"];
  NSURL* url2 = [[NSURL alloc] initWithString:@"http://www.apple.com"];
  NSURL* url3 = [[NSURL alloc] initWithString:@"http://www.espn.com"];
  NSArray<NSURL*>* urls = [NSArray arrayWithObjects:url1, url2, url3, nil];

  NSUserActivity* userActivity = [[NSUserActivity alloc]
      initWithActivityType:@"OpenInChromeIncognitoIntent"];

  OpenInChromeIncognitoIntent* intent =
      [[OpenInChromeIncognitoIntent alloc] init];

  intent.url = urls;

  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];

  id mock_user_activity = CreateMockNSUserActivity(userActivity, interaction);

  id startupInformationMock =
      [OCMockObject niceMockForProtocol:@protocol(StartupInformation)];

  id connectionInformationMock =
      [OCMockObject niceMockForProtocol:@protocol(ConnectionInformation)];

  [[connectionInformationMock expect]
      setStartupParameters:[OCMArg checkWithBlock:^BOOL(id value) {
        EXPECT_TRUE([value isKindOfClass:[AppStartupParameters class]] ||
                    value == nil);

        if (value != nil) {
          AppStartupParameters* startupParameters =
              (AppStartupParameters*)value;
          const GURL calledURL = startupParameters.externalURL;
          EXPECT_TRUE((int)[intent.url count] == 3);
          return [intent.url containsObject:(net::NSURLWithGURL(calledURL))];
        } else {
          return YES;
        }
      }]];

  MockTabOpener* tabOpener = [[MockTabOpener alloc] init];

  // Action.
  BOOL result =
      [UserActivityHandler continueUserActivity:mock_user_activity
                            applicationIsActive:NO
                                      tabOpener:tabOpener
                          connectionInformation:connectionInformationMock
                             startupInformation:startupInformationMock
                                   browserState:nullptr
                                      initStage:InitStageFinal];

  EXPECT_OCMOCK_VERIFY(startupInformationMock);
  EXPECT_TRUE(result);
}

// Tests that Chrome responds to open intents in the background.
TEST_F(UserActivityHandlerTest, ContinueUserActivityIntentBackground) {
  NSUserActivity* userActivity =
      [[NSUserActivity alloc] initWithActivityType:@"OpenInChromeIntent"];
  OpenInChromeIntent* intent = [[OpenInChromeIntent alloc] init];

  NSURL* url1 = [[NSURL alloc] initWithString:@"http://www.google.com"];
  NSURL* url2 = [[NSURL alloc] initWithString:@"http://www.apple.com"];
  NSURL* url3 = [[NSURL alloc] initWithString:@"http://www.espn.com"];
  NSArray<NSURL*>* urls = [NSArray arrayWithObjects:url1, url2, url3, nil];

  intent.url = urls;
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];

  id mock_user_activity = CreateMockNSUserActivity(userActivity, interaction);

  id startupInformationMock =
      [OCMockObject niceMockForProtocol:@protocol(StartupInformation)];

  id connectionInformationMock =
      [OCMockObject niceMockForProtocol:@protocol(ConnectionInformation)];

  [[connectionInformationMock expect]
      setStartupParameters:[OCMArg checkWithBlock:^BOOL(id value) {
        EXPECT_TRUE([value isKindOfClass:[AppStartupParameters class]] ||
                    value == nil);

        if (value != nil) {
          AppStartupParameters* startupParameters =
              (AppStartupParameters*)value;
          const GURL calledURL = startupParameters.externalURL;
          EXPECT_TRUE((int)[intent.url count] == 3);
          return [intent.url containsObject:(net::NSURLWithGURL(calledURL))];
        } else {
          return YES;
        }
      }]];

  // The test will fail if a method of this object is called.
  id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];

  // Action.
  BOOL result =
      [UserActivityHandler continueUserActivity:mock_user_activity
                            applicationIsActive:NO
                                      tabOpener:tabOpenerMock
                          connectionInformation:connectionInformationMock
                             startupInformation:startupInformationMock
                                   browserState:nullptr
                                      initStage:InitStageFinal];

  // Test.
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
  EXPECT_TRUE(result);
}

// Test that Chrome respond to open in incognito intent in the foreground.
TEST_F(UserActivityHandlerTest, ContinueUserActivityIntentIncognitoForeground) {
  NSURL* url1 = [[NSURL alloc] initWithString:@"http://www.google.com"];
  NSURL* url2 = [[NSURL alloc] initWithString:@"http://www.apple.com"];
  NSURL* url3 = [[NSURL alloc] initWithString:@"http://www.espn.com"];
  NSArray<NSURL*>* urls = [NSArray arrayWithObjects:url1, url2, url3, nil];

  NSUserActivity* userActivity = [[NSUserActivity alloc]
      initWithActivityType:@"OpenInChromeIncognitoIntent"];

  OpenInChromeIncognitoIntent* intent =
      [[OpenInChromeIncognitoIntent alloc] init];

  intent.url = urls;

  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];

  id mock_user_activity = CreateMockNSUserActivity(userActivity, interaction);

  id startupInformationMock =
      [OCMockObject niceMockForProtocol:@protocol(StartupInformation)];

  id connectionInformationMock =
      [OCMockObject niceMockForProtocol:@protocol(ConnectionInformation)];

  [[connectionInformationMock expect]
      setStartupParameters:[OCMArg checkWithBlock:^BOOL(id value) {
        EXPECT_TRUE([value isKindOfClass:[AppStartupParameters class]] ||
                    value == nil);

        if (value != nil) {
          AppStartupParameters* startupParameters =
              (AppStartupParameters*)value;
          const GURL calledURL = startupParameters.externalURL;
          EXPECT_TRUE((int)[intent.url count] == 3);
          return [intent.url containsObject:(net::NSURLWithGURL(calledURL))];
        } else {
          return YES;
        }
      }]];


  std::vector<GURL> URLs;
  for (NSURL* URL in urls) {
    URLs.push_back(net::GURLWithNSURL(URL));
  }

  AppStartupParameters* startupParams = [[AppStartupParameters alloc]
         initWithURLs:URLs
      applicationMode:ApplicationModeForTabOpening::NORMAL];
  [[[connectionInformationMock stub] andReturn:startupParams]
      startupParameters];

  MockTabOpener* tabOpener = [[MockTabOpener alloc] init];

  // Action.
  BOOL result =
      [UserActivityHandler continueUserActivity:mock_user_activity
                            applicationIsActive:YES
                                      tabOpener:tabOpener
                          connectionInformation:connectionInformationMock
                             startupInformation:startupInformationMock
                                   browserState:nullptr
                                      initStage:InitStageFinal];

  // Test.
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
  EXPECT_TRUE(result);
  EXPECT_EQ(3U, tabOpener.URLs.size());
}

// Tests that Chrome responds to open intents in the foreground.
TEST_F(UserActivityHandlerTest, ContinueUserActivityIntentForeground) {
  NSUserActivity* userActivity =
      [[NSUserActivity alloc] initWithActivityType:@"OpenInChromeIntent"];
  OpenInChromeIntent* intent = [[OpenInChromeIntent alloc] init];
  NSURL* url1 = [[NSURL alloc] initWithString:@"http://www.google.com"];
  NSURL* url2 = [[NSURL alloc] initWithString:@"http://www.apple.com"];
  NSURL* url3 = [[NSURL alloc] initWithString:@"http://www.espn.com"];
  NSArray<NSURL*>* urls = [NSArray arrayWithObjects:url1, url2, url3, nil];

  intent.url = urls;
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];

  id mock_user_activity = CreateMockNSUserActivity(userActivity, interaction);

  id startupInformationMock =
      [OCMockObject niceMockForProtocol:@protocol(StartupInformation)];

  id connectionInformationMock =
      [OCMockObject niceMockForProtocol:@protocol(ConnectionInformation)];

  [[connectionInformationMock expect]
      setStartupParameters:[OCMArg checkWithBlock:^BOOL(id value) {
        EXPECT_TRUE([value isKindOfClass:[AppStartupParameters class]] ||
                    value == nil);

        if (value != nil) {
          AppStartupParameters* startupParameters =
              (AppStartupParameters*)value;
          const GURL calledURL = startupParameters.externalURL;
          EXPECT_TRUE((int)[intent.url count] == 3);
          return [intent.url containsObject:(net::NSURLWithGURL(calledURL))];
        } else {
          return YES;
        }
      }]];

  MockTabOpener* tabOpener = [[MockTabOpener alloc] init];

  std::vector<GURL> URLs;
  for (NSURL* URL in urls) {
    URLs.push_back(net::GURLWithNSURL(URL));
  }

  AppStartupParameters* startupParams = [[AppStartupParameters alloc]
         initWithURLs:URLs
      applicationMode:ApplicationModeForTabOpening::NORMAL];
  [[[connectionInformationMock stub] andReturn:startupParams]
      startupParameters];

  // Action.
  BOOL result =
      [UserActivityHandler continueUserActivity:mock_user_activity
                            applicationIsActive:YES
                                      tabOpener:tabOpener
                          connectionInformation:connectionInformationMock
                             startupInformation:startupInformationMock
                                   browserState:nullptr
                                      initStage:InitStageFinal];

  // Test.
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
  EXPECT_TRUE(result);
  EXPECT_EQ(3U, tabOpener.URLs.size());
}

// Tests that handleStartupParameters with a file url. "external URL" gets
// rewritten to chrome://URL, while "complete URL" remains full local file URL.
TEST_F(UserActivityHandlerTest, HandleStartupParamsWithExternalFile) {
  // Setup.
  GURL externalURL("chrome://test.pdf");
  GURL completeURL("file://test.pdf");

  AppStartupParameters* startupParams = [[AppStartupParameters alloc]
      initWithExternalURL:externalURL
              completeURL:completeURL
          applicationMode:ApplicationModeForTabOpening::INCOGNITO];

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];

  id connectionInformationMock =
      [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];
  [[[connectionInformationMock stub] andReturn:startupParams]
      startupParameters];
  [[connectionInformationMock expect] setStartupParameters:nil];
  [[[connectionInformationMock expect] andReturnValue:@NO]
      startupParametersAreBeingHandled];
  [[connectionInformationMock expect] setStartupParametersAreBeingHandled:YES];

  MockTabOpener* tabOpener = [[MockTabOpener alloc] init];

  // Action.
  [UserActivityHandler
      handleStartupParametersWithTabOpener:tabOpener
                     connectionInformation:connectionInformationMock
                        startupInformation:startupInformationMock
                              browserState:nullptr
                                 initStage:InitStageFinal];
  [tabOpener completionBlock]();

  // Tests.
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
  // External file:// URL will be loaded by WebState, which expects complete
  // file:// URL. chrome:// URL is expected to be displayed in the omnibox,
  // and omnibox shows virtual URL.
  EXPECT_EQ(completeURL, tabOpener.urlLoadParams.web_params.url);
  EXPECT_EQ(externalURL, tabOpener.urlLoadParams.web_params.virtual_url);
  EXPECT_EQ(ApplicationModeForTabOpening::INCOGNITO,
            [tabOpener applicationMode]);
}

// Tests that performActionForShortcutItem set startupParameters accordingly to
// the shortcut used
// TODO(crbug.com/1172529): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_PerformActionForShortcutItemWithRealShortcut \
  PerformActionForShortcutItemWithRealShortcut
#else
#define MAYBE_PerformActionForShortcutItemWithRealShortcut \
  DISABLED_PerformActionForShortcutItemWithRealShortcut
#endif
TEST_F(UserActivityHandlerTest,
       MAYBE_PerformActionForShortcutItemWithRealShortcut) {
  // Setup.
  GURL gurlNewTab("chrome://newtab/");

  FakeStartupInformation* fakeStartupInformation =
      [[FakeStartupInformation alloc] init];

  FakeConnectionInformation* fakeConnectionInformation =
      [[FakeConnectionInformation alloc] init];

  // Set a list of parameter to test, where each entry has a post open action
  // name, whether or not it should open a new tab, whether or not to use
  // incognito, and the post open action enum value.
  NSArray* parametersToTest = @[
    @[ @"OpenNewSearch", @YES, @NO, @(FOCUS_OMNIBOX) ],
    @[ @"OpenIncognitoSearch", @YES, @YES, @(FOCUS_OMNIBOX) ],
    @[ @"OpenVoiceSearch", @YES, @NO, @(START_VOICE_SEARCH) ],
    @[ @"OpenQRScanner", @YES, @NO, @(START_QR_CODE_SCANNER) ],
    @[
      @"OpenLensFromAppIconLongPress", @NO, @NO,
      @(START_LENS_FROM_APP_ICON_LONG_PRESS)
    ],
    @[ @"OpenLensFromSpotlight", @NO, @NO, @(START_LENS_FROM_SPOTLIGHT) ]
  ];

  swizzleHandleStartupParameters();

  for (id parameters in parametersToTest) {
    UIApplicationShortcutItem* shortcut =
        [[UIApplicationShortcutItem alloc] initWithType:parameters[0]
                                         localizedTitle:parameters[0]];

    resetHandleStartupParametersHasBeenCalled();

    // The test will fail is a method of those objects is called.
    id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];

    // Action.
    [UserActivityHandler performActionForShortcutItem:shortcut
                                    completionHandler:getCompletionHandler()
                                            tabOpener:tabOpenerMock
                                connectionInformation:fakeConnectionInformation
                                   startupInformation:fakeStartupInformation
                                         browserState:nullptr
                                            initStage:InitStageFinal];

    // Tests.
    if ([[parameters objectAtIndex:1] boolValue]) {
      EXPECT_EQ(gurlNewTab,
                [fakeConnectionInformation startupParameters].externalURL);
    } else {
      EXPECT_TRUE(
          [fakeConnectionInformation startupParameters].externalURL.is_empty());
    }

    EXPECT_EQ([[parameters objectAtIndex:2] boolValue]
                  ? ApplicationModeForTabOpening::INCOGNITO
                  : ApplicationModeForTabOpening::NORMAL,
              [fakeConnectionInformation startupParameters].applicationMode);
    EXPECT_EQ([[parameters objectAtIndex:3] intValue],
              [fakeConnectionInformation startupParameters].postOpeningAction);
    EXPECT_TRUE(completionHandlerExecuted());
    EXPECT_TRUE(completionHandlerArgument());
    EXPECT_TRUE(getHandleStartupParametersHasBeenCalled());
  }
}

// Tests that performActionForShortcutItem just executes the completionHandler
// with NO if the firstRunUI is present.
TEST_F(UserActivityHandlerTest, PerformActionForShortcutItemWithFirstRunUI) {
  // Setup.
  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];

  id connectionInformationMock =
      [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];

  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:@"OpenNewSearch"
                                       localizedTitle:@""];

  swizzleHandleStartupParameters();

  // The test will fail is a method of those objects is called.
  id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];
  ChromeBrowserState* nullBrowserState = nullptr;

  // Action.
  [UserActivityHandler performActionForShortcutItem:shortcut
                                  completionHandler:getCompletionHandler()
                                          tabOpener:tabOpenerMock
                              connectionInformation:connectionInformationMock
                                 startupInformation:startupInformationMock
                                       browserState:nullBrowserState
                                          initStage:InitStageFirstRun];

  // Tests.
  EXPECT_TRUE(completionHandlerExecuted());
  EXPECT_FALSE(completionHandlerArgument());
  EXPECT_FALSE(getHandleStartupParametersHasBeenCalled());
}
