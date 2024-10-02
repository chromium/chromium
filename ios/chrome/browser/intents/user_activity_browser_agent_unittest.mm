// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intents/user_activity_browser_agent.h"

#import <CoreSpotlight/CoreSpotlight.h>

#import <memory>

#import "base/memory/ptr_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_command_line.h"
#import "base/test/task_environment.h"
#import "base/test/with_feature_override.h"
#import "base/values.h"
#import "components/handoff/handoff_utility.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/app/spotlight/actions_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/intents/intents_constants.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/intents/AddBookmarkToChromeIntent.h"
#import "ios/chrome/common/intents/AddReadingListItemToChromeIntent.h"
#import "ios/chrome/common/intents/ClearBrowsingDataIntent.h"
#import "ios/chrome/common/intents/ManagePasswordsIntent.h"
#import "ios/chrome/common/intents/ManagePaymentMethodsIntent.h"
#import "ios/chrome/common/intents/ManageSettingsIntent.h"
#import "ios/chrome/common/intents/OpenBookmarksIntent.h"
#import "ios/chrome/common/intents/OpenInChromeIncognitoIntent.h"
#import "ios/chrome/common/intents/OpenInChromeIntent.h"
#import "ios/chrome/common/intents/OpenLatestTabIntent.h"
#import "ios/chrome/common/intents/OpenLensIntent.h"
#import "ios/chrome/common/intents/OpenNewIncognitoTabIntent.h"
#import "ios/chrome/common/intents/OpenNewTabIntent.h"
#import "ios/chrome/common/intents/OpenReadingListIntent.h"
#import "ios/chrome/common/intents/OpenRecentTabsIntent.h"
#import "ios/chrome/common/intents/OpenTabGridIntent.h"
#import "ios/chrome/common/intents/RunSafetyCheckIntent.h"
#import "ios/chrome/common/intents/SearchInChromeIntent.h"
#import "ios/chrome/common/intents/SearchWithVoiceIntent.h"
#import "ios/chrome/common/intents/SetChromeDefaultBrowserIntent.h"
#import "ios/chrome/common/intents/ViewHistoryIntent.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/gtest_util.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

@interface FakeSceneController : SceneController
// Arguments for
// -dismissModalsAndMaybeOpenSelectedTabInMode:withUrlLoadParams:dismissOmnibox:
//  completion:.
@property(nonatomic, readonly) UrlLoadParams urlLoadParams;
@property(nonatomic, readonly) ApplicationModeForTabOpening applicationMode;
// Argument for
// -dismissModalsAndOpenMultipleTabsInMode:URLs:dismissOmnibox:completion:.
@property(nonatomic, readonly) std::vector<GURL>& URLs;
@end

@implementation FakeSceneController

- (BOOL)URLIsOpenedInRegularMode:(const GURL&)URL {
  return NO;
}

- (void)dismissModalsAndMaybeOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                                 withUrlLoadParams:
                                     (const UrlLoadParams&)urlLoadParams
                                    dismissOmnibox:(BOOL)dismissOmnibox
                                        completion:(ProceduralBlock)completion {
  _urlLoadParams = urlLoadParams;
  _applicationMode = targetMode;
}

- (void)dismissModalsAndOpenMultipleTabsWithURLs:(const std::vector<GURL>&)URLs
                                 inIncognitoMode:(BOOL)incognitoMode
                                  dismissOmnibox:(BOOL)dismissOmnibox
                                      completion:(ProceduralBlock)completion {
  _URLs = URLs;
}

@end

#pragma mark - Test class.

class UserActivityBrowserAgentTest : public PlatformTest {
 public:
  UserActivityBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();

    AppState* app_state = CreateMockAppState(AppInitStage::kFinal);

    scene_state_ = [[FakeSceneState alloc] initWithAppState:app_state
                                                    profile:profile_.get()];

    scene_state_.activationLevel = SceneActivationLevelForegroundActive;
    scene_controller_ =
        [[FakeSceneController alloc] initWithSceneState:scene_state_];
    scene_state_.controller = scene_controller_;
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    // Create the UserActivity Browser Agent.
    UserActivityBrowserAgent::CreateForBrowser(browser_.get());
    user_activity_browser_agent_ =
        UserActivityBrowserAgent::FromBrowser(browser_.get());

    connection_information_ = scene_state_.controller;
  }

  ~UserActivityBrowserAgentTest() override {}

 protected:
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

  // Mock & stub an AppState object with an arbitrary `init_stage` property.
  id CreateMockAppState(AppInitStage init_stage) {
    id mock_app_state = OCMClassMock([AppState class]);
    OCMStub([(AppState*)mock_app_state initStage]).andReturn(init_stage);
    return mock_app_state;
  }

  // Set pref kIncognitoModeAvailability to kForced and make it a managed pref.
  void ForceIncognitoMode() {
    PrefService* pref_service = profile_->GetPrefs();
    profile_->GetTestingPrefService()->SetManagedPref(
        policy::policy_prefs::kIncognitoModeAvailability,
        std::make_unique<base::Value>(true));

    EXPECT_TRUE(pref_service->IsManagedPreference(
        policy::policy_prefs::kIncognitoModeAvailability));

    pref_service->SetInteger(policy::policy_prefs::kIncognitoModeAvailability,
                             static_cast<int>(IncognitoModePrefs::kForced));
    EXPECT_TRUE(IsIncognitoModeForced(pref_service));
  }

  // Set pref kIncognitoModeAvailability to kDisabled and make it a managed
  // pref.
  void DisableIncognitoMode() {
    PrefService* pref_service = profile_->GetPrefs();
    profile_->GetTestingPrefService()->SetManagedPref(
        policy::policy_prefs::kIncognitoModeAvailability,
        std::make_unique<base::Value>(true));

    EXPECT_TRUE(pref_service->IsManagedPreference(
        policy::policy_prefs::kIncognitoModeAvailability));

    pref_service->SetInteger(policy::policy_prefs::kIncognitoModeAvailability,
                             static_cast<int>(IncognitoModePrefs::kDisabled));
    EXPECT_TRUE(IsIncognitoModeDisabled(pref_service));
  }

  raw_ptr<UserActivityBrowserAgent> user_activity_browser_agent_;
  FakeSceneState* scene_state_;
  FakeSceneController* scene_controller_;
  id<ConnectionInformation> connection_information_;

 private:
  std::unique_ptr<TestBrowser> browser_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

#pragma mark - Tests.

// Tests that method ProceedWithUserActivity returns true when incognito mode
// is forced and when userActivity supports incognito browser.
TEST_F(UserActivityBrowserAgentTest,
       ProceedWithUserActivityWithIncognitoBrowser) {
  // UserActivityTypes to test.
  NSArray* user_activity_types = @[
    kShortcutNewIncognitoSearch, kSiriShortcutOpenInIncognito,
    kShortcutLensFromSpotlight
  ];

  ForceIncognitoMode();

  for (NSString* user_activity_type in user_activity_types) {
    NSUserActivity* user_activity =
        [[NSUserActivity alloc] initWithActivityType:user_activity_type];

    EXPECT_TRUE(
        user_activity_browser_agent_->ProceedWithUserActivity(user_activity));
  }
}

// Tests that method canProceedWithUserActivity returns false when incognito
// mode is forced and when userActivity does not support incognito browser.
TEST_F(UserActivityBrowserAgentTest,
       ProceedWithWrongUserActivityWithIncognitoBrowser) {
  ForceIncognitoMode();

  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriShortcutOpenInChrome];
  EXPECT_FALSE(
      user_activity_browser_agent_->ProceedWithUserActivity(user_activity));
}

// Tests that method canProceedWithUserActivity returns true when incognito mode
// is disabled and when userActivity supports regular browser.
TEST_F(UserActivityBrowserAgentTest,
       CanProceedWithUserActivityWithRegularBrowser) {
  // UserActivityTypes to test.
  NSArray* user_activity_types = @[
    kShortcutNewSearch, kShortcutVoiceSearch, kSiriShortcutSearchInChrome,
    kSiriShortcutOpenInChrome
  ];

  DisableIncognitoMode();

  for (NSString* user_activity_type in user_activity_types) {
    NSUserActivity* user_activity =
        [[NSUserActivity alloc] initWithActivityType:user_activity_type];

    EXPECT_TRUE(
        user_activity_browser_agent_->ProceedWithUserActivity(user_activity));
  }
}

// Tests that method canProceedWithUserActivity returns false when incognito
// mode is disabled and when userActivity does not support regular browser.
TEST_F(UserActivityBrowserAgentTest,
       CanProceedWithWrongUserActivityWithRegularBrowser) {
  // UserActivityTypes to test.
  NSArray* user_activity_types =
      @[ kShortcutNewIncognitoSearch, kSiriShortcutOpenInIncognito ];

  DisableIncognitoMode();

  for (NSString* user_activity_type in user_activity_types) {
    NSUserActivity* user_activity =
        [[NSUserActivity alloc] initWithActivityType:user_activity_type];

    EXPECT_FALSE(
        user_activity_browser_agent_->ProceedWithUserActivity(user_activity));
  }
}

// Tests that method canProceedWithUserActivity returns false if the activity
// type is unknown.
TEST_F(UserActivityBrowserAgentTest,
       CanProceedWithUserActivityWithWrongActivityType) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:@"not_an_activity_type"];
  EXPECT_FALSE(
      user_activity_browser_agent_->ProceedWithUserActivity(user_activity));
}

// Tests that Chrome does not continue the activity if the activity type is
// random.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityFromGarbage) {
  // Setup.
  NSString* handoff_with_suffix =
      [handoff::kChromeHandoffActivityType stringByAppendingString:@"test"];
  NSString* handoff_with_prefix =
      [@"test" stringByAppendingString:handoff::kChromeHandoffActivityType];
  NSArray* user_activity_types = @[
    @"thisIsGarbage", @"it.does.not.work", handoff_with_suffix,
    handoff_with_prefix
  ];
  for (NSString* user_activity_type in user_activity_types) {
    NSUserActivity* user_activity =
        [[NSUserActivity alloc] initWithActivityType:user_activity_type];
    [user_activity
        setWebpageURL:[NSURL URLWithString:@"http://www.google.com"]];

    // Action.
    BOOL result =
        user_activity_browser_agent_->ContinueUserActivity(user_activity, NO);

    // Tests.
    EXPECT_FALSE(result);
  }
}

// Tests that Chrome does not continue the activity if the webpage url is not
// set.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityNoWebpage) {
  // Setup.
  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:handoff::kChromeHandoffActivityType];

  // Action.
  BOOL result =
      user_activity_browser_agent_->ContinueUserActivity(user_activity, NO);

  // Tests.
  EXPECT_FALSE(result);
}

// Tests that Chrome does not continue the activity if the activity is a
// Spotlight action of an unknown type.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivitySpotlightActionFromGarbage) {
  // Only test Spotlight if it is enabled and available on the device.
  if (!spotlight::IsSpotlightAvailable()) {
    return;
  }
  // Setup.
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:CSSearchableItemActionType];
  NSString* invalid_action =
      [NSString stringWithFormat:@"%@.invalidAction",
                                 spotlight::StringFromSpotlightDomain(
                                     spotlight::DOMAIN_ACTIONS)];
  NSDictionary* user_info =
      @{CSSearchableItemActivityIdentifier : invalid_action};
  [user_activity addUserInfoEntriesFromDictionary:user_info];

  // Enable the SpotlightActions experiment.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableSpotlightActions);

  // Action.
  BOOL result =
      user_activity_browser_agent_->ContinueUserActivity(user_activity, NO);

  // Tests.
  EXPECT_FALSE(result);
}

// Tests that ContinueUserActivity returns YES if the activity is a
// Spotlight action different from DOMAIN_ACTIONS and if there is no
// webpage_url.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivitySpotlightActionWithNoWebPageUrl) {
  // Only test Spotlight if it is enabled and available on the device.
  if (!spotlight::IsSpotlightAvailable()) {
    return;
  }
  // Setup.
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:CSSearchableItemActionType];
  NSString* invalid_action =
      [NSString stringWithFormat:@"%@.invalidAction",
                                 spotlight::StringFromSpotlightDomain(
                                     spotlight::DOMAIN_BOOKMARKS)];
  NSDictionary* user_info =
      @{CSSearchableItemActivityIdentifier : invalid_action};
  [user_activity addUserInfoEntriesFromDictionary:user_info];

  // Enable the SpotlightActions experiment.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableSpotlightActions);

  // Action.
  BOOL result =
      user_activity_browser_agent_->ContinueUserActivity(user_activity, NO);

  // Tests.
  EXPECT_TRUE(result);
}

// Tests that Chrome continues the activity if the application is in background
// by saving the url to startupParameters.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityBackground) {
  // Setup.
  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:handoff::kChromeHandoffActivityType];
  NSURL* nsurl = [NSURL URLWithString:@"http://www.google.com"];
  [user_activity setWebpageURL:nsurl];

  // Action.
  BOOL result =
      user_activity_browser_agent_->ContinueUserActivity(user_activity, NO);

  // Test.
  EXPECT_TRUE(result);
}

// Tests that Chrome continues the activity if the application is in foreground
// by opening a new tab.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityForeground) {
  // Setup.
  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:handoff::kChromeHandoffActivityType];
  GURL gurl("http://www.google.com");
  [user_activity setWebpageURL:net::NSURLWithGURL(gurl)];

  AppStartupParameters* startup_params = [[AppStartupParameters alloc]
      initWithExternalURL:gurl
              completeURL:gurl
          applicationMode:ApplicationModeForTabOpening::NORMAL];
  [connection_information_ setStartupParameters:startup_params];

  // Action.
  BOOL result =
      user_activity_browser_agent_->ContinueUserActivity(user_activity, YES);

  // Test.
  EXPECT_EQ(gurl, scene_controller_.urlLoadParams.web_params.url);
  EXPECT_TRUE(
      scene_controller_.urlLoadParams.web_params.virtual_url.is_empty());
  EXPECT_TRUE(result);
}

// Tests that a new tab is created when application is started via handoff.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityBrowsingWeb) {
  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:NSUserActivityTypeBrowsingWeb];
  // This URL is passed to application by iOS but is not used in this part
  // of application logic.
  NSURL* nsurl = [NSURL URLWithString:@"http://google.com/foo/bar"];
  [user_activity setWebpageURL:nsurl];

  BOOL result =
      user_activity_browser_agent_->ContinueUserActivity(user_activity, YES);

  const GURL gurl = net::GURLWithNSURL(nsurl);
  EXPECT_EQ(gurl, scene_controller_.urlLoadParams.web_params.url);
  EXPECT_TRUE(
      scene_controller_.urlLoadParams.web_params.virtual_url.is_empty());
  // AppStartupParameters default to opening pages in non-Incognito mode.
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL,
            [scene_controller_ applicationMode]);
  EXPECT_TRUE(result);
}

// Tests that continueUserActivity sets startupParameters accordingly to the
// Spotlight action used.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityShortcutActions) {
  // Only test Spotlight if it is enabled and available on the device.
  if (!spotlight::IsSpotlightAvailable()) {
    return;
  }
  // Setup.
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
    NSUserActivity* user_activity = [[NSUserActivity alloc]
        initWithActivityType:CSSearchableItemActionType];
    NSString* action =
        [NSString stringWithFormat:@"%@.%@",
                                   spotlight::StringFromSpotlightDomain(
                                       spotlight::DOMAIN_ACTIONS),
                                   parameters[0]];
    NSDictionary* user_info = @{CSSearchableItemActivityIdentifier : action};
    [user_activity addUserInfoEntriesFromDictionary:user_info];

    // Action.
    BOOL result =
        user_activity_browser_agent_->ContinueUserActivity(user_activity, NO);

    // Tests.
    EXPECT_TRUE(result);
    EXPECT_EQ(kChromeUINewTabURL,
              connection_information_.startupParameters.externalURL);
    EXPECT_EQ([parameters[1] intValue],
              [connection_information_ startupParameters].postOpeningAction);
  }
}

// Tests that Chrome responds to open in incognito intent in the background.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityIntentIncognitoBackground) {
  NSURL* url1 = [[NSURL alloc] initWithString:@"http://www.google.com/"];
  NSURL* url2 = [[NSURL alloc] initWithString:@"http://www.apple.com/"];
  NSURL* url3 = [[NSURL alloc] initWithString:@"http://www.espn.com/"];
  NSArray<NSURL*>* urls = [NSArray arrayWithObjects:url1, url2, url3, nil];

  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:kSiriShortcutOpenInIncognito];

  OpenInChromeIncognitoIntent* intent =
      [[OpenInChromeIncognitoIntent alloc] init];
  intent.url = urls;
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];

  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  // Action.
  BOOL result = user_activity_browser_agent_->ContinueUserActivity(
      mock_user_activity, NO);

  EXPECT_TRUE(result);
  NSURL* external_url = net::NSURLWithGURL(
      [connection_information_ startupParameters].externalURL);
  EXPECT_TRUE([intent.url containsObject:external_url]);
}

// Tests that Chrome responds to open intents in the background.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityIntentBackground) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriShortcutOpenInChrome];
  OpenInChromeIntent* intent = [[OpenInChromeIntent alloc] init];

  NSURL* url1 = [[NSURL alloc] initWithString:@"http://www.google.com/"];
  NSURL* url2 = [[NSURL alloc] initWithString:@"http://www.apple.com/"];
  NSURL* url3 = [[NSURL alloc] initWithString:@"http://www.espn.com/"];
  NSArray<NSURL*>* urls = [NSArray arrayWithObjects:url1, url2, url3, nil];

  intent.url = urls;
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];

  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  // Action.
  BOOL result = user_activity_browser_agent_->ContinueUserActivity(
      mock_user_activity, NO);

  // Test.
  EXPECT_TRUE(result);
  NSURL* external_url = net::NSURLWithGURL(
      [connection_information_ startupParameters].externalURL);
  EXPECT_TRUE([intent.url containsObject:external_url]);
}

// Tests that Chrome respond to open in incognito intent in the foreground.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityIntentIncognitoForeground) {
  NSURL* url1 = [[NSURL alloc] initWithString:@"http://www.google.com/"];
  NSURL* url2 = [[NSURL alloc] initWithString:@"http://www.apple.com/"];
  NSURL* url3 = [[NSURL alloc] initWithString:@"http://www.espn.com/"];
  NSArray<NSURL*>* urls = [NSArray arrayWithObjects:url1, url2, url3, nil];

  NSUserActivity* userActivity = [[NSUserActivity alloc]
      initWithActivityType:kSiriShortcutOpenInIncognito];

  OpenInChromeIncognitoIntent* intent =
      [[OpenInChromeIncognitoIntent alloc] init];

  intent.url = urls;

  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];

  id mock_user_activity = CreateMockNSUserActivity(userActivity, interaction);

  // Action.
  BOOL result = user_activity_browser_agent_->ContinueUserActivity(
      mock_user_activity, YES);

  // Test.
  EXPECT_TRUE(result);
  EXPECT_EQ(3U, scene_controller_.URLs.size());
  NSURL* external_url = net::NSURLWithGURL(
      [connection_information_ startupParameters].externalURL);
  EXPECT_TRUE([intent.url containsObject:external_url]);
}

// Tests that Chrome responds to open intents in the foreground.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityIntentForeground) {
  NSUserActivity* userActivity =
      [[NSUserActivity alloc] initWithActivityType:kSiriShortcutOpenInChrome];
  OpenInChromeIntent* intent = [[OpenInChromeIntent alloc] init];
  NSURL* url1 = [[NSURL alloc] initWithString:@"http://www.google.com/"];
  NSURL* url2 = [[NSURL alloc] initWithString:@"http://www.apple.com/"];
  NSURL* url3 = [[NSURL alloc] initWithString:@"http://www.espn.com/"];
  NSArray<NSURL*>* urls = [NSArray arrayWithObjects:url1, url2, url3, nil];

  intent.url = urls;
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(userActivity, interaction);

  std::vector<GURL> URLs;
  for (NSURL* URL in urls) {
    URLs.push_back(net::GURLWithNSURL(URL));
  }

  AppStartupParameters* startup_params = [[AppStartupParameters alloc]
         initWithURLs:URLs
      applicationMode:ApplicationModeForTabOpening::NORMAL];
  [connection_information_ setStartupParameters:startup_params];

  // Action.
  BOOL result = user_activity_browser_agent_->ContinueUserActivity(
      mock_user_activity, YES);

  // Test.
  EXPECT_TRUE(result);
  EXPECT_EQ(3U, scene_controller_.URLs.size());
  NSURL* external_url = net::NSURLWithGURL(
      [connection_information_ startupParameters].externalURL);
  EXPECT_TRUE([intent.url containsObject:external_url]);
}

// Tests that handleStartupParameters with a file url. "external URL" gets
// rewritten to chrome://URL, while "complete URL" remains full local file URL.
TEST_F(UserActivityBrowserAgentTest, HandleStartupParamsWithExternalFile) {
  // Setup.
  GURL external_url("chrome://test.pdf");
  GURL complete_url("file://test.pdf");

  AppStartupParameters* startup_params = [[AppStartupParameters alloc]
      initWithExternalURL:external_url
              completeURL:complete_url
          applicationMode:ApplicationModeForTabOpening::INCOGNITO];
  [connection_information_ setStartupParameters:startup_params];

  // Action.
  user_activity_browser_agent_->RouteToCorrectTab();

  // Tests.
  // External file:// URL will be loaded by WebState, which expects complete
  // file:// URL. chrome:// URL is expected to be displayed in the omnibox,
  // and omnibox shows virtual URL.
  EXPECT_EQ(complete_url, scene_controller_.urlLoadParams.web_params.url);
  EXPECT_EQ(external_url,
            scene_controller_.urlLoadParams.web_params.virtual_url);
  EXPECT_EQ(ApplicationModeForTabOpening::INCOGNITO,
            connection_information_.startupParameters.applicationMode);
}

// Tests that performActionForShortcutItem set startupParameters accordingly
// to the shortcut used
// TODO(crbug.com/40166681): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_PerformActionForShortcutItemWithRealShortcut \
  PerformActionForShortcutItemWithRealShortcut
#else
#define MAYBE_PerformActionForShortcutItemWithRealShortcut \
  DISABLED_PerformActionForShortcutItemWithRealShortcut
#endif
TEST_F(UserActivityBrowserAgentTest,
       MAYBE_PerformActionForShortcutItemWithRealShortcut) {
  // Setup.
  // Set a list of parameter to test, where each entry has a post open action
  // name, whether or not it should open a new tab, whether or not to use
  // incognito, and the post open action enum value.
  NSArray* parameters_to_test = @[
    @[ kShortcutNewSearch, @YES, @NO, @(FOCUS_OMNIBOX) ],
    @[ kShortcutNewIncognitoSearch, @YES, @YES, @(FOCUS_OMNIBOX) ],
    @[ kShortcutVoiceSearch, @YES, @NO, @(START_VOICE_SEARCH) ],
    @[ kShortcutQRScanner, @YES, @NO, @(START_QR_CODE_SCANNER) ],
    @[
      kShortcutLensFromAppIconLongPress, @NO, @NO,
      @(START_LENS_FROM_APP_ICON_LONG_PRESS)
    ],
    @[ kShortcutLensFromSpotlight, @NO, @NO, @(START_LENS_FROM_SPOTLIGHT) ]
  ];

  for (id parameters in parameters_to_test) {
    UIApplicationShortcutItem* shortcut =
        [[UIApplicationShortcutItem alloc] initWithType:parameters[0]
                                         localizedTitle:parameters[0]];

    // Action.
    user_activity_browser_agent_->Handle3DTouchApplicationShortcuts(shortcut);

    // Tests.
    if ([[parameters objectAtIndex:1] boolValue]) {
      EXPECT_EQ(kChromeUINewTabURL,
                [connection_information_ startupParameters].externalURL);
    } else {
      EXPECT_TRUE(
          [connection_information_ startupParameters].externalURL.is_empty());
    }

    ApplicationModeForTabOpening app_mode =
        [[parameters objectAtIndex:2] boolValue]
            ? ApplicationModeForTabOpening::INCOGNITO
            : ApplicationModeForTabOpening::NORMAL;
    EXPECT_EQ(app_mode,
              connection_information_.startupParameters.applicationMode);
    EXPECT_EQ([[parameters objectAtIndex:3] intValue],
              connection_information_.startupParameters.postOpeningAction);
  }
}

// Tests that Handle3DTouchApplicationShortcuts returns NO if it's the first
// run.
TEST_F(UserActivityBrowserAgentTest,
       PerformActionForShortcutItemWithFirstRunUI) {
  // Setup.
  scene_state_.appState = CreateMockAppState(AppInitStage::kFirstRun);
  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:kShortcutNewSearch
                                       localizedTitle:kShortcutNewSearch];

  // Action.
  bool result =
      user_activity_browser_agent_->Handle3DTouchApplicationShortcuts(shortcut);

  // Tests.
  EXPECT_FALSE(result);
}

// Tests that Chrome does continue the activity for the Add Bookmarks intent.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityBookmarks) {
  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:kSiriShortcutAddBookmarkToChrome];

  AddBookmarkToChromeIntent* intent = [[AddBookmarkToChromeIntent alloc] init];
  NSURL* url = [[NSURL alloc] initWithString:@"http://www.google.com"];
  NSArray<NSURL*>* urls = [NSArray arrayWithObjects:url, nil];
  intent.url = urls;
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(ADD_BOOKMARKS,
            connection_information_.startupParameters.postOpeningAction);
  EXPECT_EQ(urls.count,
            connection_information_.startupParameters.inputURLs.count);
}

// Tests that Chrome does not continue the activity for the Add Bookmarks intent
// due to still being in first run.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityBookmarksFailsFirstRun) {
  scene_state_.appState = CreateMockAppState(AppInitStage::kFirstRun);
  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:kSiriShortcutAddBookmarkToChrome];

  NSURL* url = [[NSURL alloc] initWithString:@"http://www.google.com/"];
  NSArray<NSURL*>* urls = [NSArray arrayWithObjects:url, nil];
  AddBookmarkToChromeIntent* intent = [[AddBookmarkToChromeIntent alloc] init];
  intent.url = urls;

  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  [user_activity setWebpageURL:url];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  BOOL result = user_activity_browser_agent_->ContinueUserActivity(
      mock_user_activity, NO);

  EXPECT_TRUE(result);
  EXPECT_EQ(urls.count,
            connection_information_.startupParameters.inputURLs.count);
  EXPECT_EQ(GURL(kChromeUINewTabURL),
            connection_information_.startupParameters.completeURL);
  EXPECT_EQ(GURL(kChromeUINewTabURL),
            connection_information_.startupParameters.externalURL);
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL,
            connection_information_.startupParameters.applicationMode);
}

// Tests that Chrome does not continue the activity if the intent URLs array is
// empty.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityBookmarksFailsURLsArrayEmpty) {
  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:kSiriShortcutAddBookmarkToChrome];

  NSArray<NSURL*>* urls = @[];
  AddBookmarkToChromeIntent* intent = [[AddBookmarkToChromeIntent alloc] init];
  intent.url = urls;
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  BOOL result = user_activity_browser_agent_->ContinueUserActivity(
      mock_user_activity, NO);

  EXPECT_FALSE(result);
  EXPECT_EQ(urls.count,
            connection_information_.startupParameters.inputURLs.count);
}

// Tests that Chrome does not continue the activity if the intent URLs are not
// set for the Add Bookmarks intent.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityBookmarksFailsNoURLs) {
  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:kSiriShortcutAddBookmarkToChrome];

  BOOL result =
      user_activity_browser_agent_->ContinueUserActivity(user_activity, NO);

  EXPECT_FALSE(result);
}

// Tests that Chrome does continue the activity for the Add Reading List items
// intent.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityAddToReadingList) {
  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:kSiriShortcutAddReadingListItemToChrome];

  AddReadingListItemToChromeIntent* intent =
      [[AddReadingListItemToChromeIntent alloc] init];
  NSArray<NSURL*>* urls =
      @[ [[NSURL alloc] initWithString:@"https://google.com/"] ];
  intent.url = urls;

  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(ADD_READING_LIST_ITEMS,
            [connection_information_ startupParameters].postOpeningAction);
}

// TBD Tests that Chrome does not continue the activity for the Add Reading List
// items intent due to still being in first run.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityAddToReadingListFailsFirstRun) {
  scene_state_.appState = CreateMockAppState(AppInitStage::kFirstRun);
  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:kSiriShortcutAddReadingListItemToChrome];

  NSURL* url = [[NSURL alloc] initWithString:@"http://www.google.com/"];
  NSArray<NSURL*>* urls = [NSArray arrayWithObjects:url, nil];
  AddReadingListItemToChromeIntent* intent =
      [[AddReadingListItemToChromeIntent alloc] init];
  intent.url = urls;

  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  [user_activity setWebpageURL:url];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  BOOL result = user_activity_browser_agent_->ContinueUserActivity(
      mock_user_activity, NO);

  EXPECT_TRUE(result);
}

// Tests that Chrome does not continue the activity if the intent URLs array
// is empty.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityAddToReadingListFailsURLsArrayEmpty) {
  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:kSiriShortcutAddReadingListItemToChrome];

  NSArray<NSURL*>* urls = @[];
  AddReadingListItemToChromeIntent* intent =
      [[AddReadingListItemToChromeIntent alloc] init];
  intent.url = urls;

  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];

  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  BOOL result = user_activity_browser_agent_->ContinueUserActivity(
      mock_user_activity, NO);

  EXPECT_FALSE(result);
}

// Tests that Chrome does not continue the activity if the intent URLs are not
// set for the Add Reading List items intent.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityAddToReadingListFailsNoURLs) {
  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:kSiriShortcutAddReadingListItemToChrome];

  BOOL result =
      user_activity_browser_agent_->ContinueUserActivity(user_activity, NO);

  EXPECT_FALSE(result);
}

// Tests that Chrome respond to open reading list intent.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityIntentOpenReadingList) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriOpenReadingList];

  OpenReadingListIntent* intent = [[OpenReadingListIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(OPEN_READING_LIST,
            connection_information_.startupParameters.postOpeningAction);
}

// Tests that Chrome respond to open bookmarks intent.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityIntentOpenBookmarks) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriOpenBookmarks];

  OpenBookmarksIntent* intent = [[OpenBookmarksIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(OPEN_BOOKMARKS,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to open recent tabs intent.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityIntentOpenRecentTabs) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriOpenRecentTabs];

  OpenRecentTabsIntent* intent = [[OpenRecentTabsIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(OPEN_RECENT_TABS,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to open tab grid intent.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityIntentOpenTabGrid) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriOpenTabGrid];

  OpenTabGridIntent* intent = [[OpenTabGridIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(OPEN_TAB_GRID,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to search with voice intent.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityIntentSearchWithVoice) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriVoiceSearch];

  SearchWithVoiceIntent* intent = [[SearchWithVoiceIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(START_VOICE_SEARCH,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to open new tab intent.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityIntentOpenNewTab) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriOpenNewTab];

  OpenNewTabIntent* intent = [[OpenNewTabIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(NO_ACTION,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to set chrome as default browser intent.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityIntentSetChromeDefaultBrowser) {
  NSUserActivity* user_activity = [[NSUserActivity alloc]
      initWithActivityType:kSiriSetChromeDefaultBrowser];

  SetChromeDefaultBrowserIntent* intent =
      [[SetChromeDefaultBrowserIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(SET_CHROME_DEFAULT_BROWSER,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to view chrome history intent.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityIntentViewChromeHistory) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriViewHistory];

  ViewHistoryIntent* intent = [[ViewHistoryIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(VIEW_HISTORY,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to open a new incognito tab.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityIntentOpenNewIncognitoTab) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriOpenNewIncognitoTab];

  OpenNewIncognitoTabIntent* intent = [[OpenNewIncognitoTabIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(ApplicationModeForTabOpening::INCOGNITO,
            [connection_information_ startupParameters].applicationMode);
  EXPECT_EQ(GURL(kChromeUINewTabURL),
            [connection_information_ startupParameters].completeURL);
  EXPECT_EQ(GURL(kChromeUINewTabURL),
            [connection_information_ startupParameters].externalURL);
  EXPECT_EQ(NO_ACTION,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to search in chrome.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityIntentSearchInChrome) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriShortcutSearchInChrome];

  SearchInChromeIntent* intent = [[SearchInChromeIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL,
            [connection_information_ startupParameters].applicationMode);
  EXPECT_EQ(GURL(kChromeUINewTabURL),
            [connection_information_ startupParameters].completeURL);
  EXPECT_EQ(GURL(kChromeUINewTabURL),
            [connection_information_ startupParameters].externalURL);
  EXPECT_EQ(FOCUS_OMNIBOX,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to manage payment methods intent.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityIntentManagePaymentMethods) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriManagePaymentMethods];

  ManagePaymentMethodsIntent* intent =
      [[ManagePaymentMethodsIntent alloc] init];

  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];

  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(OPEN_PAYMENT_METHODS,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to run safety check intent.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityIntentRunSafetyCheck) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriRunSafetyCheck];

  RunSafetyCheckIntent* intent = [[RunSafetyCheckIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(RUN_SAFETY_CHECK,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to run manage passwords intent.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityIntentManagePasswords) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriManagePasswords];

  ManagePasswordsIntent* intent = [[ManagePasswordsIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(MANAGE_PASSWORDS,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to manage settings intent.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityIntentManageSettings) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriManageSettings];

  ManageSettingsIntent* intent = [[ManageSettingsIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(MANAGE_SETTINGS,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to Open Latest Tab intent.
TEST_F(UserActivityBrowserAgentTest, ContinueUserActivityIntentOpenLatestTab) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriOpenLatestTab];

  OpenLatestTabIntent* intent = [[OpenLatestTabIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(OPEN_LATEST_TAB,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that Chrome respond to Open Lens intent.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityIntentOpenLensFromIntents) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriOpenLensFromIntents];

  OpenLensIntent* intent = [[OpenLensIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(START_LENS_FROM_INTENTS,
            [scene_state_.controller startupParameters].postOpeningAction);
}

// Tests that Chrome respond to Clear Browsing Data intent.
TEST_F(UserActivityBrowserAgentTest,
       ContinueUserActivityIntentClearBrowsingData) {
  NSUserActivity* user_activity =
      [[NSUserActivity alloc] initWithActivityType:kSiriClearBrowsingData];

  ClearBrowsingDataIntent* intent = [[ClearBrowsingDataIntent alloc] init];
  INInteraction* interaction = [[INInteraction alloc] initWithIntent:intent
                                                            response:nil];
  id mock_user_activity = CreateMockNSUserActivity(user_activity, interaction);

  user_activity_browser_agent_->ContinueUserActivity(mock_user_activity, YES);

  EXPECT_EQ(OPEN_CLEAR_BROWSING_DATA_DIALOG,
            [connection_information_ startupParameters].postOpeningAction);
}
