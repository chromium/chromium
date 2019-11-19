// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/application_delegate/user_activity_handler.h"

#include <memory>

#import <CoreSpotlight/CoreSpotlight.h>

#include "base/mac/scoped_block.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "components/handoff/handoff_utility.h"
#import "ios/chrome/app/app_startup_parameters.h"
#include "ios/chrome/app/application_delegate/fake_startup_information.h"
#include "ios/chrome/app/application_delegate/mock_tab_opener.h"
#include "ios/chrome/app/application_delegate/startup_information.h"
#include "ios/chrome/app/application_delegate/tab_opening.h"
#include "ios/chrome/app/application_mode.h"
#include "ios/chrome/app/main_controller.h"
#include "ios/chrome/app/spotlight/actions_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/u2f/u2f_tab_helper.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface_provider.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "net/base/mac/url_conversions.h"
#include "net/test/gtest_util.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Substitutes U2FTabHelper for testing.
class FakeU2FTabHelper : public U2FTabHelper {
 public:
  static void CreateForWebState(web::WebState* web_state) {
    web_state->SetUserData(U2FTabHelper::UserDataKey(),
                           base::WrapUnique(new FakeU2FTabHelper(web_state)));
  }

  void EvaluateU2FResult(const GURL& url) override { url_ = url; }

  const GURL& url() const { return url_; }

 private:
  FakeU2FTabHelper(web::WebState* web_state) : U2FTabHelper(web_state) {}
  GURL url_;
  DISALLOW_COPY_AND_ASSIGN(FakeU2FTabHelper);
};

#pragma mark - TabModel Mock

// TabModel mock for using in UserActivity tests.
@interface UserActivityHandlerTabModelMock : NSObject

@end

@implementation UserActivityHandlerTabModelMock {
  FakeWebStateListDelegate _webStateListDelegate;
  std::unique_ptr<WebStateList> _webStateList;
}

- (instancetype)init {
  if ((self = [super init])) {
    _webStateList = std::make_unique<WebStateList>(&_webStateListDelegate);
  }
  return self;
}

- (web::WebState*)addWebState {
  auto testWebState = std::make_unique<web::TestWebState>();
  TabIdTabHelper::CreateForWebState(testWebState.get());
  FakeU2FTabHelper::CreateForWebState(testWebState.get());
  web::WebState* returnWebState = testWebState.get();
  _webStateList->InsertWebState(0, std::move(testWebState),
                                WebStateList::INSERT_NO_FLAGS,
                                WebStateOpener());

  return returnWebState;
}

- (WebStateList*)webStateList {
  return _webStateList.get();
}

@end

#pragma mark - Test class.

// A block that takes as arguments the caller and the arguments from
// UserActivityHandler +handleStartupParameters and returns nothing.
typedef void (^startupParameterBlock)(id,
                                      id<TabOpening>,
                                      id<StartupInformation>,
                                      id<BrowserInterfaceProvider>);

// A block that takes a BOOL argument and returns nothing.
typedef void (^conditionBlock)(BOOL);

class UserActivityHandlerTest : public PlatformTest {
 protected:
  void swizzleHandleStartupParameters() {
    handle_startup_parameters_has_been_called_ = NO;
    swizzle_block_ = [^(id self) {
      handle_startup_parameters_has_been_called_ = YES;
    } copy];
    user_activity_handler_swizzler_.reset(new ScopedBlockSwizzler(
        [UserActivityHandler class],
        @selector(handleStartupParametersWithTabOpener:
                                    startupInformation:interfaceProvider:),
        swizzle_block_));
  }

  BOOL getHandleStartupParametersHasBeenCalled() {
    return handle_startup_parameters_has_been_called_;
  }

  void resetHandleStartupParametersHasBeenCalled() {
    handle_startup_parameters_has_been_called_ = NO;
  }

  FakeU2FTabHelper* GetU2FTabHelperForWebState(web::WebState* web_state) {
    return static_cast<FakeU2FTabHelper*>(
        U2FTabHelper::FromWebState(web_state));
  }

  NSString* GetTabIdForWebState(web::WebState* web_state) {
    return TabIdTabHelper::FromWebState(web_state)->tab_id();
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

 private:
  __block BOOL block_executed_;
  __block BOOL block_argument_;
  std::unique_ptr<ScopedBlockSwizzler> user_activity_handler_swizzler_;
  startupParameterBlock swizzle_block_;
  conditionBlock completion_block_;
  __block BOOL handle_startup_parameters_has_been_called_;
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

    // Action.
    BOOL result =
        [UserActivityHandler continueUserActivity:userActivity
                              applicationIsActive:NO
                                        tabOpener:tabOpenerMock
                               startupInformation:startupInformationMock];

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
  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];

  // Action.
  BOOL result =
      [UserActivityHandler continueUserActivity:userActivity
                            applicationIsActive:NO
                                      tabOpener:tabOpenerMock
                             startupInformation:startupInformationMock];

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

  // Action.
  BOOL result =
      [UserActivityHandler continueUserActivity:userActivity
                            applicationIsActive:NO
                                      tabOpener:tabOpenerMock
                             startupInformation:startupInformationMock];

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
  [[startupInformationMock expect]
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
                             startupInformation:startupInformationMock];

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
  [[[startupInformationMock stub] andReturnValue:@NO] isPresentingFirstRunUI];

  AppStartupParameters* startupParams =
      [[AppStartupParameters alloc] initWithExternalURL:gurl completeURL:gurl];
  [[[startupInformationMock stub] andReturn:startupParams] startupParameters];

  // Action.
  BOOL result =
      [UserActivityHandler continueUserActivity:userActivity
                            applicationIsActive:YES
                                      tabOpener:tabOpener
                             startupInformation:startupInformationMock];

  // Test.
  EXPECT_EQ(gurl, tabOpener.urlLoadParams.web_params.url);
  EXPECT_TRUE(tabOpener.urlLoadParams.web_params.virtual_url.is_empty());
  EXPECT_TRUE(result);
}

// Tests that a new tab is created when application is started via Universal
// Link.
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
  [fakeStartupInformation setIsPresentingFirstRunUI:NO];

  BOOL result =
      [UserActivityHandler continueUserActivity:userActivity
                            applicationIsActive:YES
                                      tabOpener:tabOpener
                             startupInformation:fakeStartupInformation];

  GURL newTabURL(kChromeUINewTabURL);
  EXPECT_EQ(newTabURL, tabOpener.urlLoadParams.web_params.url);
  // AppStartupParameters default to opening pages in non-Incognito mode.
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL, [tabOpener applicationMode]);
  EXPECT_TRUE(result);
  // Verifies that a new tab is being requested.
  EXPECT_EQ(newTabURL,
            [[fakeStartupInformation startupParameters] externalURL]);
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
                               startupInformation:fakeStartupInformation];

    // Tests.
    EXPECT_TRUE(result);
    EXPECT_EQ(gurlNewTab,
              [fakeStartupInformation startupParameters].externalURL);
    EXPECT_EQ([parameters[1] intValue],
              [fakeStartupInformation startupParameters].postOpeningAction);
  }
}

// Tests that handleStartupParameters with a file url. "external URL" gets
// rewritten to chrome://URL, while "complete URL" remains full local file URL.
TEST_F(UserActivityHandlerTest, HandleStartupParamsWithExternalFile) {
  // Setup.
  GURL externalURL("chrome://test.pdf");
  GURL completeURL("file://test.pdf");

  AppStartupParameters* startupParams =
      [[AppStartupParameters alloc] initWithExternalURL:externalURL
                                            completeURL:completeURL];
  [startupParams setLaunchInIncognito:YES];

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[[startupInformationMock stub] andReturnValue:@NO] isPresentingFirstRunUI];
  [[[startupInformationMock stub] andReturn:startupParams] startupParameters];
  [[startupInformationMock expect] setStartupParameters:nil];

  MockTabOpener* tabOpener = [[MockTabOpener alloc] init];

  // The test will fail is a method of this object is called.
  id interfaceProviderMock =
      [OCMockObject mockForProtocol:@protocol(BrowserInterfaceProvider)];

  // Action.
  [UserActivityHandler
      handleStartupParametersWithTabOpener:tabOpener
                        startupInformation:startupInformationMock
                         interfaceProvider:interfaceProviderMock];
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

// Tests that handleStartupParameters with a non-U2F url opens a new tab.
TEST_F(UserActivityHandlerTest, HandleStartupParamsNonU2F) {
  // Setup.
  GURL gurl("http://www.google.com");

  AppStartupParameters* startupParams =
      [[AppStartupParameters alloc] initWithExternalURL:gurl completeURL:gurl];
  [startupParams setLaunchInIncognito:YES];

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[[startupInformationMock stub] andReturnValue:@NO] isPresentingFirstRunUI];
  [[[startupInformationMock stub] andReturn:startupParams] startupParameters];
  [[startupInformationMock expect] setStartupParameters:nil];

  MockTabOpener* tabOpener = [[MockTabOpener alloc] init];

  // The test will fail is a method of this object is called.
  id interfaceProviderMock =
      [OCMockObject mockForProtocol:@protocol(BrowserInterfaceProvider)];

  // Action.
  [UserActivityHandler
      handleStartupParametersWithTabOpener:tabOpener
                        startupInformation:startupInformationMock
                         interfaceProvider:interfaceProviderMock];
  [tabOpener completionBlock]();

  // Tests.
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
  EXPECT_EQ(gurl, tabOpener.urlLoadParams.web_params.url);
  EXPECT_TRUE(tabOpener.urlLoadParams.web_params.virtual_url.is_empty());
  EXPECT_EQ(ApplicationModeForTabOpening::INCOGNITO,
            [tabOpener applicationMode]);
}

// Tests that handleStartupParameters with a U2F url opens in the correct tab.
TEST_F(UserActivityHandlerTest, HandleStartupParamsU2F) {
  // Setup.
  UserActivityHandlerTabModelMock* mockTabModel =
      [[UserActivityHandlerTabModelMock alloc] init];
  web::WebState* web_state = [mockTabModel addWebState];
  id tabModel = static_cast<id>(mockTabModel);

  std::string urlRepresentation = base::StringPrintf(
      "chromium://u2f-callback?isU2F=1&tabID=%s",
      base::SysNSStringToUTF8(GetTabIdForWebState(web_state)).c_str());

  GURL gurl(urlRepresentation);
  AppStartupParameters* startupParams =
      [[AppStartupParameters alloc] initWithExternalURL:gurl completeURL:gurl];
  [startupParams setLaunchInIncognito:YES];

  id startupInformationMock =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[[startupInformationMock stub] andReturnValue:@NO] isPresentingFirstRunUI];
  [[[startupInformationMock stub] andReturn:startupParams] startupParameters];
  [[startupInformationMock expect] setStartupParameters:nil];

  StubBrowserInterfaceProvider* interfaceProvider =
      [[StubBrowserInterfaceProvider alloc] init];
  interfaceProvider.mainInterface.tabModel = tabModel;
  interfaceProvider.incognitoInterface.tabModel = tabModel;

  MockTabOpener* tabOpener = [[MockTabOpener alloc] init];

  // Action.
  [UserActivityHandler
      handleStartupParametersWithTabOpener:tabOpener
                        startupInformation:startupInformationMock
                         interfaceProvider:interfaceProvider];

  // Tests.
  EXPECT_OCMOCK_VERIFY(startupInformationMock);
  EXPECT_EQ(gurl, GetU2FTabHelperForWebState(web_state)->url());
  EXPECT_TRUE(tabOpener.urlLoadParams.web_params.url.is_empty());
  EXPECT_TRUE(tabOpener.urlLoadParams.web_params.virtual_url.is_empty());
}

// Tests that performActionForShortcutItem set startupParameters accordingly to
// the shortcut used
TEST_F(UserActivityHandlerTest, PerformActionForShortcutItemWithRealShortcut) {
  // Setup.
  GURL gurlNewTab("chrome://newtab/");

  FakeStartupInformation* fakeStartupInformation =
      [[FakeStartupInformation alloc] init];
  [fakeStartupInformation setIsPresentingFirstRunUI:NO];

  NSArray* parametersToTest = @[
    @[ @"OpenNewSearch", @NO, @(FOCUS_OMNIBOX) ],
    @[ @"OpenIncognitoSearch", @YES, @(FOCUS_OMNIBOX) ],
    @[ @"OpenVoiceSearch", @NO, @(START_VOICE_SEARCH) ],
    @[ @"OpenQRScanner", @NO, @(START_QR_CODE_SCANNER) ]
  ];

  swizzleHandleStartupParameters();

  for (id parameters in parametersToTest) {
    UIApplicationShortcutItem* shortcut =
        [[UIApplicationShortcutItem alloc] initWithType:parameters[0]
                                         localizedTitle:parameters[0]];

    resetHandleStartupParametersHasBeenCalled();

    // The test will fail is a method of those objects is called.
    id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];
    id interfaceProviderMock =
        [OCMockObject mockForProtocol:@protocol(BrowserInterfaceProvider)];

    // Action.
    [UserActivityHandler performActionForShortcutItem:shortcut
                                    completionHandler:getCompletionHandler()
                                            tabOpener:tabOpenerMock
                                   startupInformation:fakeStartupInformation
                                    interfaceProvider:interfaceProviderMock];

    // Tests.
    EXPECT_EQ(gurlNewTab,
              [fakeStartupInformation startupParameters].externalURL);
    EXPECT_EQ([[parameters objectAtIndex:1] boolValue],
              [fakeStartupInformation startupParameters].launchInIncognito);
    EXPECT_EQ([[parameters objectAtIndex:2] intValue],
              [fakeStartupInformation startupParameters].postOpeningAction);
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
  [[[startupInformationMock stub] andReturnValue:@YES] isPresentingFirstRunUI];

  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:@"OpenNewSearch"
                                       localizedTitle:@""];

  swizzleHandleStartupParameters();

  // The test will fail is a method of those objects is called.
  id tabOpenerMock = [OCMockObject mockForProtocol:@protocol(TabOpening)];
  id interfaceProviderMock =
      [OCMockObject mockForProtocol:@protocol(BrowserInterfaceProvider)];

  // Action.
  [UserActivityHandler performActionForShortcutItem:shortcut
                                  completionHandler:getCompletionHandler()
                                          tabOpener:tabOpenerMock
                                 startupInformation:startupInformationMock
                                  interfaceProvider:interfaceProviderMock];

  // Tests.
  EXPECT_TRUE(completionHandlerExecuted());
  EXPECT_FALSE(completionHandlerArgument());
  EXPECT_FALSE(getHandleStartupParametersHasBeenCalled());
}
