// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_url_context.h"

#import <UIKit/UIKit.h>

#import <optional>

#import "base/command_line.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/default_search_manager.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/custom_scheme_buildflags.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/startup/app_startup_utils.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/google_one_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test double for SceneController to verify tab opening from TaskRequests.
@interface TaskRequestURLContextTestTabOpener : SceneController <TabOpening>

// The mode in which the tab was requested to be opened.
@property(nonatomic, assign) ApplicationModeForTabOpening targetMode;
// The parameters used to load the opened tab.
@property(nonatomic, assign) UrlLoadParams urlLoadParams;
// Whether the omnibox was requested to be dismissed.
@property(nonatomic, assign) BOOL dismissOmnibox;
// The post-opening action requested for the opened tab.
@property(nonatomic, assign) TabOpeningPostOpeningAction completionAction;
// Whether the post-opening completion was executed.
@property(nonatomic, assign) BOOL completionActionExecuted;

@end

@implementation TaskRequestURLContextTestTabOpener

- (ProceduralBlock)completionBlockForTriggeringAction:
    (TabOpeningPostOpeningAction)action {
  self.completionAction = action;
  return ^{
    self.completionActionExecuted = YES;
  };
}

- (void)dismissModalsAndMaybeOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                                 withUrlLoadParams:(UrlLoadParams)urlLoadParams
                                    dismissOmnibox:(BOOL)dismissOmnibox
                                        completion:(ProceduralBlock)completion {
  self.targetMode = targetMode;
  self.urlLoadParams = urlLoadParams;
  self.dismissOmnibox = dismissOmnibox;
  if (completion) {
    completion();
  }
}

@end

class TaskRequestForURLContextTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    ClearDefaultBrowserPromoData();
    // Shared defaults persist across test runs on the simulator.
    [app_group::GetGroupUserDefaults()
        removeObjectForKey:AppGroupCommandPreference()];

    ResetEnableNewStartupFlowEnabledForTesting();
    scoped_feature_list_.InitAndEnableFeature(kEnableNewStartupFlow);
    SaveEnableNewStartupFlowForNextStart();

    mock_startup_information_ = OCMProtocolMock(@protocol(StartupInformation));
    app_state_ =
        [[AppState alloc] initWithStartupInformation:mock_startup_information_];
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    profile_state_ = [[ProfileState alloc] initWithAppState:app_state_];
    profile_state_.profile = profile_.get();
    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.profileState = profile_state_;
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
  }

  void TearDown() override {
    [app_group::GetGroupUserDefaults()
        removeObjectForKey:AppGroupCommandPreference()];
    app_state_ = nil;
    mock_startup_information_ = nil;
    browser_.reset();
    [scene_state_ shutdown];
    scene_state_ = nil;
    profile_state_ = nil;
    profile_.reset();
    ResetEnableNewStartupFlowEnabledForTesting();
    ClearDefaultBrowserPromoData();
    PlatformTest::TearDown();
  }

  UIOpenURLContext* CreateMockURLContext(NSURL* url,
                                         NSString* source_application = nil) {
    UIOpenURLContext* mock_context = OCMClassMock([UIOpenURLContext class]);
    OCMStub([mock_context URL]).andReturn(url);
    if (source_application) {
      UISceneOpenURLOptions* mock_options =
          OCMClassMock([UISceneOpenURLOptions class]);
      OCMStub([mock_options sourceApplication]).andReturn(source_application);
      OCMStub([mock_context options]).andReturn(mock_options);
    }
    return mock_context;
  }

  // Sets whether the app is in first run for testing metrics.
  void SetIsFirstRun(BOOL is_first_run) {
    OCMStub([mock_startup_information_ isFirstRun]).andReturn(is_first_run);
  }

  // Writes an App Group command from the share extension, issued at
  // `command_time`, to the shared defaults.
  void WriteShareExtensionCommand(NSDate* command_time) {
    WriteAppGroupCommand(@{
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandAppPreference) :
          app_group::kOpenCommandSourceShareExtension,
      base::SysUTF8ToNSString(
          app_group::kChromeAppGroupCommandCommandPreference) :
          base::SysUTF8ToNSString(app_group::kChromeAppGroupOpenURLCommand),
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference) :
          command_time,
    });
  }

  // Writes an App Group command only holding its `command_time` to the shared
  // defaults.
  void WriteAppGroupCommandWithOnlyTime(NSDate* command_time) {
    WriteAppGroupCommand(@{
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference) :
          command_time,
    });
  }

  void WriteAppGroupCommand(NSDictionary* command_dictionary) {
    [app_group::GetGroupUserDefaults() setObject:command_dictionary
                                          forKey:AppGroupCommandPreference()];
  }

  // Writes a recent App Group `command` sent by `caller_app`, with an optional
  // `external_text`, to the shared defaults.
  void WriteRecentAppGroupCommand(NSString* command,
                                  NSString* caller_app,
                                  NSString* external_text) {
    NSMutableDictionary* dictionary = [@{
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandAppPreference) :
          caller_app,
      base::SysUTF8ToNSString(
          app_group::kChromeAppGroupCommandCommandPreference) : command,
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference) :
          [NSDate date],
    } mutableCopy];
    if (external_text) {
      dictionary[base::SysUTF8ToNSString(
          app_group::kChromeAppGroupCommandTextPreference)] = external_text;
    }
    WriteAppGroupCommand(dictionary);
  }

  // Creates and executes an X-Callback App Group command request, returning
  // the tab opener that recorded the resulting tab opening, if any.
  TaskRequestURLContextTestTabOpener* ExecuteAppGroupCommandRequest() {
    TaskRequestForURLContext* request = [TaskRequestForURLContext
        taskRequestWithURLContext:CreateMockURLContext(
                                      [NSURL URLWithString:kAppGroupCommandURL])
                       sceneState:scene_state_
                      isColdStart:YES];
    TaskRequestURLContextTestTabOpener* tab_opener =
        [[TaskRequestURLContextTestTabOpener alloc]
            initWithSceneState:scene_state_];
    scene_state_.controller = tab_opener;
    [request execute];
    return tab_opener;
  }

  // Returns whether an App Group command is in the shared defaults.
  bool HasAppGroupCommand() {
    return [app_group::GetGroupUserDefaults()
               objectForKey:AppGroupCommandPreference()] != nil;
  }

  NSString* AppGroupCommandPreference() {
    return base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandPreference);
  }

  // Returns a request for `url_string`, with `kTestGaiaID` appended as the
  // `gaia_id` query item.
  TaskRequestForURLContext* CreateRequestWithGaiaID(NSString* url_string) {
    NSString* full_url_string = [NSString
        stringWithFormat:@"%@%@%s=%@", url_string,
                         [url_string containsString:@"?"] ? @"&" : @"?",
                         app_group::kGaiaIDQueryItemName, kTestGaiaID];
    return [TaskRequestForURLContext
        taskRequestWithURLContext:CreateMockURLContext(
                                      [NSURL URLWithString:full_url_string])
                       sceneState:scene_state_
                      isColdStart:YES];
  }

  // Sets a default search engine so that search URLs can be generated.
  void SetUpDefaultSearchEngine() {
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service->Load();
    TemplateURLData data;
    data.SetShortName(u"Test");
    data.SetKeyword(u"test");
    data.SetURL(kTestSearchURL);
    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

  NSString* const kTestGaiaID = @"test_gaia_id";
  // URL template of the default search engine used in tests.
  const std::string kTestSearchURL =
      "https://www.example.com/search?q={searchTerms}";
  // Text searched in tests, and its results URL with `kTestSearchURL`.
  NSString* const kTestSearchQuery = @"chromium";
  const GURL kTestSearchQueryResultsURL =
      GURL("https://www.example.com/search?q=chromium");
  NSString* const kAppGroupCommandURL =
      @"googlechrome://x-callback-url/app-group-command";

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  id mock_startup_information_;
  AppState* app_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* profile_state_;
  FakeSceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests that Startup.MobileSessionStartFromApps is logged during execute with
// the expected caller app, including uppercase schemes and first-party bundle
// IDs.
TEST_F(TaskRequestForURLContextTest, TestStartupMobileSessionStartFromApps) {
  struct TestCase {
    NSString* url_string;
    NSString* source_application;
    MobileSessionCallerApp expected_caller_app;
  } test_cases[] = {
      {@"https://www.example.com", nil, CALLER_APP_THIRD_PARTY},
      {@"HTTP://www.example.com", nil, CALLER_APP_THIRD_PARTY},
      {@"HTTPS://www.example.com", nil, CALLER_APP_THIRD_PARTY},
      {@"googlechrome://www.example.com", nil, CALLER_APP_NOT_AVAILABLE},
      {@"https://www.example.com", @"com.google.GooglePlus",
       CALLER_APP_GOOGLE_PLUS},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::SysNSStringToUTF8(test_case.url_string));
    base::HistogramTester histogram_tester;
    NSURL* url = [NSURL URLWithString:test_case.url_string];
    UIOpenURLContext* context =
        CreateMockURLContext(url, test_case.source_application);

    TaskRequestForURLContext* request =
        [TaskRequestForURLContext taskRequestWithURLContext:context
                                                 sceneState:scene_state_
                                                isColdStart:YES];
    EXPECT_NE(request, nil);
    histogram_tester.ExpectTotalCount("Startup.MobileSessionStartFromApps", 0);

    [request execute];

    histogram_tester.ExpectUniqueSample("Startup.MobileSessionStartFromApps",
                                        test_case.expected_caller_app, 1);
  }
}

// Tests that Startup.ShowDefaultPromoFromApps is logged during execute when URL
// contains the default browser settings query item.
TEST_F(TaskRequestForURLContextTest, TestStartupShowDefaultPromoFromApps) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL
      URLWithString:@"https://www.example.com?poa=default-browser-settings"];
  UIOpenURLContext* context = CreateMockURLContext(url);

  TaskRequestForURLContext* request =
      [TaskRequestForURLContext taskRequestWithURLContext:context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  EXPECT_NE(request, nil);
  histogram_tester.ExpectTotalCount("Startup.ShowDefaultPromoFromApps", 0);

  [request execute];

  histogram_tester.ExpectTotalCount("Startup.ShowDefaultPromoFromApps", 1);
  histogram_tester.ExpectBucketCount("Startup.ShowDefaultPromoFromApps",
                                     CALLER_APP_THIRD_PARTY, 1);
}

// Tests that FirstRun.LaunchSource is logged on first run.
TEST_F(TaskRequestForURLContextTest, TestFirstRunLaunchSource) {
  SetIsFirstRun(YES);
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL URLWithString:@"https://www.example.com"];
  UIOpenURLContext* context = CreateMockURLContext(url);

  TaskRequestForURLContext* request =
      [TaskRequestForURLContext taskRequestWithURLContext:context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  EXPECT_NE(request, nil);

  // ProfileState may be nil early during app startup, so these metrics should
  // be collected later when executing the request.
  histogram_tester.ExpectTotalCount("FirstRun.LaunchSource", 0);

  [request execute];

  histogram_tester.ExpectTotalCount("FirstRun.LaunchSource", 1);
  histogram_tester.ExpectBucketCount("FirstRun.LaunchSource",
                                     first_run::LAUNCH_BY_OTHERS, 1);
}

// Tests that WidgetKit URLs log the launch source and WidgetKit action metrics
// during execute.
TEST_F(TaskRequestForURLContextTest, TestWidgetKitActionMetrics) {
  struct TestCase {
    NSString* url_string;
    WidgetKitExtensionAction expected_action;
  } test_cases[] = {
      {@"chromewidgetkit://search-widget/search",
       WidgetKitExtensionAction::ACTION_SEARCH_WIDGET_SEARCH},
      {@"chromewidgetkit://quick-actions-widget/incognito",
       WidgetKitExtensionAction::ACTION_QUICK_ACTIONS_INCOGNITO},
      {@"chromewidgetkit://lockscreen-launcher-widget/search",
       WidgetKitExtensionAction::ACTION_LOCKSCREEN_LAUNCHER_SEARCH},
      {@"chromewidgetkit://shortcuts-widget/open",
       WidgetKitExtensionAction::ACTION_SHORTCUTS_OPEN},
  };

  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    NSURL* url = [NSURL URLWithString:test_case.url_string];
    UIOpenURLContext* context = CreateMockURLContext(url);

    TaskRequestForURLContext* request =
        [TaskRequestForURLContext taskRequestWithURLContext:context
                                                 sceneState:scene_state_
                                                isColdStart:YES];
    EXPECT_NE(request, nil);
    histogram_tester.ExpectTotalCount(kUMAMobileSessionStartActionHistogram, 0);
    histogram_tester.ExpectTotalCount(kAppLaunchSource, 0);
    histogram_tester.ExpectTotalCount(kWidgetKitActionHistogram, 0);

    [request execute];

    histogram_tester.ExpectUniqueSample(kUMAMobileSessionStartActionHistogram,
                                        START_ACTION_WIDGET_KIT_COMMAND, 1);
    histogram_tester.ExpectUniqueSample(kAppLaunchSource,
                                        AppLaunchSource::WIDGET, 1);
    histogram_tester.ExpectUniqueSample(kWidgetKitActionHistogram,
                                        test_case.expected_action, 1);
  }
}

// Tests that X-Callback URLs log the launch source and X-Callback action
// metrics during execute.
TEST_F(TaskRequestForURLContextTest, TestXCallbackURLMetrics) {
  struct TestCase {
    NSString* url_string;
    MobileSessionStartAction expected_action;
  } test_cases[] = {
      {@"googlechrome://x-callback-url/open?url=https://www.example.com",
       START_ACTION_XCALLBACK_OPEN},
      {@"googlechrome://x-callback-url/app-group-command",
       START_ACTION_XCALLBACK_APPGROUP_COMMAND},
      {@"googlechrome://x-callback-url/other", START_ACTION_XCALLBACK_OTHER},
  };

  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    NSURL* url = [NSURL URLWithString:test_case.url_string];
    UIOpenURLContext* context = CreateMockURLContext(url);

    TaskRequestForURLContext* request =
        [TaskRequestForURLContext taskRequestWithURLContext:context
                                                 sceneState:scene_state_
                                                isColdStart:YES];
    EXPECT_NE(request, nil);
    histogram_tester.ExpectTotalCount(kUMAMobileSessionStartActionHistogram, 0);
    histogram_tester.ExpectTotalCount(kAppLaunchSource, 0);

    [request execute];

    histogram_tester.ExpectUniqueSample(kUMAMobileSessionStartActionHistogram,
                                        test_case.expected_action, 1);
    histogram_tester.ExpectUniqueSample(kAppLaunchSource,
                                        AppLaunchSource::X_CALLBACK, 1);
  }
}

// Tests that X-Callback app-group-command URLs consume the App Group command
// on initialization, which logs the application group command delay metric
// once.
TEST_F(TaskRequestForURLContextTest, TestXCallbackAppGroupCommandDelayMetric) {
  base::HistogramTester histogram_tester;

  // Prepare app group command parameters in defaults.
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSString* commandDictionaryPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandPreference);
  NSString* commandTimePreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference);
  NSString* commandCallerPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandAppPreference);
  NSString* commandPreference = base::SysUTF8ToNSString(
      app_group::kChromeAppGroupCommandCommandPreference);

  NSDate* commandTime =
      [NSDate dateWithTimeIntervalSinceNow:-5.0];  // 5 seconds ago
  NSDictionary* commandDictionary = @{
    commandTimePreference : commandTime,
    commandCallerPreference : @"some-extension",
    commandPreference : @"open-url-command"
  };
  [sharedDefaults setObject:commandDictionary
                     forKey:commandDictionaryPreference];

  NSURL* url =
      [NSURL URLWithString:@"googlechrome://x-callback-url/app-group-command"];
  UIOpenURLContext* context = CreateMockURLContext(url);

  TaskRequestForURLContext* request =
      [TaskRequestForURLContext taskRequestWithURLContext:context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  EXPECT_NE(request, nil);
  histogram_tester.ExpectTotalCount(kUMAMobileSessionStartActionHistogram, 0);
  histogram_tester.ExpectTotalCount(kAppLaunchSource, 0);
  histogram_tester.ExpectUniqueSample("Startup.ApplicationGroupCommandDelay", 5,
                                      1);
  EXPECT_FALSE(HasAppGroupCommand());

  [request execute];

  histogram_tester.ExpectUniqueSample(kUMAMobileSessionStartActionHistogram,
                                      START_ACTION_XCALLBACK_APPGROUP_COMMAND,
                                      1);
  histogram_tester.ExpectUniqueSample(kAppLaunchSource,
                                      AppLaunchSource::X_CALLBACK, 1);

  // The command delay is not recorded again during execute.
  histogram_tester.ExpectUniqueSample("Startup.ApplicationGroupCommandDelay", 5,
                                      1);
}

// Tests that a valid share extension App Group command is consumed at
// initialization, marks the request as coming from the share extension and
// extracts the `gaia_id`.
TEST_F(TaskRequestForURLContextTest, TestShareExtensionCommandSetsGaiaID) {
  WriteShareExtensionCommand([NSDate date]);

  TaskRequestForURLContext* request =
      CreateRequestWithGaiaID(kAppGroupCommandURL);

  EXPECT_TRUE(request.isFromShareExtension);
  EXPECT_NSEQ(kTestGaiaID, request.gaiaID);
  EXPECT_FALSE(HasAppGroupCommand());
}

// Tests that the share extension origin is kept after the App Group command is
// no longer in the shared defaults, e.g. when the task is executed later.
TEST_F(TaskRequestForURLContextTest,
       TestShareExtensionOriginSurvivesCommandRemoval) {
  WriteShareExtensionCommand([NSDate date]);
  TaskRequestForURLContext* request =
      CreateRequestWithGaiaID(kAppGroupCommandURL);

  // A second request can't consume the command again.
  TaskRequestForURLContext* second_request =
      CreateRequestWithGaiaID(kAppGroupCommandURL);

  EXPECT_TRUE(request.isFromShareExtension);
  EXPECT_FALSE(second_request.isFromShareExtension);
  EXPECT_NSEQ(nil, second_request.gaiaID);
}

// Tests that an expired App Group command doesn't allow an account switch.
TEST_F(TaskRequestForURLContextTest, TestExpiredShareExtensionCommand) {
  WriteShareExtensionCommand([NSDate
      dateWithTimeIntervalSinceNow:-2 * app_group::kAppGroupCommandTimeout]);

  TaskRequestForURLContext* request =
      CreateRequestWithGaiaID(kAppGroupCommandURL);

  EXPECT_FALSE(request.isFromShareExtension);
  EXPECT_NSEQ(nil, request.gaiaID);
  EXPECT_FALSE(HasAppGroupCommand());
}

// Tests that an App Group command URL without a command in the shared defaults
// doesn't allow an account switch.
TEST_F(TaskRequestForURLContextTest, TestAppGroupURLWithoutCommand) {
  TaskRequestForURLContext* request =
      CreateRequestWithGaiaID(kAppGroupCommandURL);

  EXPECT_FALSE(request.isFromShareExtension);
  EXPECT_NSEQ(nil, request.gaiaID);
}

// Tests that a recent App Group command allows an account switch even if it
// can't be executed (no caller or command).
TEST_F(TaskRequestForURLContextTest, TestRecentCommandWithOnlyTimeSetsGaiaID) {
  WriteAppGroupCommandWithOnlyTime([NSDate date]);

  TaskRequestForURLContext* request =
      CreateRequestWithGaiaID(kAppGroupCommandURL);

  EXPECT_TRUE(request.isFromShareExtension);
  EXPECT_NSEQ(kTestGaiaID, request.gaiaID);
  EXPECT_FALSE(HasAppGroupCommand());
}

// Tests that a command dated in the future allows an account switch.
TEST_F(TaskRequestForURLContextTest, TestFutureShareExtensionCommand) {
  WriteShareExtensionCommand(
      [NSDate dateWithTimeIntervalSinceNow:app_group::kAppGroupCommandTimeout]);

  TaskRequestForURLContext* request =
      CreateRequestWithGaiaID(kAppGroupCommandURL);

  EXPECT_TRUE(request.isFromShareExtension);
  EXPECT_NSEQ(kTestGaiaID, request.gaiaID);
}

// Tests that an HTTPS URL mimicking an App Group command doesn't allow an
// account switch, as any app or webpage can open it.
TEST_F(TaskRequestForURLContextTest, TestHTTPSAppGroupURLIgnoresGaiaID) {
  WriteShareExtensionCommand([NSDate date]);

  TaskRequestForURLContext* request =
      CreateRequestWithGaiaID(@"https://x-callback-url/app-group-command");

  EXPECT_FALSE(request.isFromShareExtension);
  EXPECT_NSEQ(nil, request.gaiaID);
  // The command is still consumed, as X-Callback URLs are recognized by their
  // host only.
  EXPECT_FALSE(HasAppGroupCommand());
}

// Tests that widget URLs neither consume the App Group command nor are
// attributed to the share extension.
TEST_F(TaskRequestForURLContextTest, TestWidgetURLDoesNotConsumeCommand) {
  WriteShareExtensionCommand([NSDate date]);

  TaskRequestForURLContext* request =
      CreateRequestWithGaiaID(@"chromewidgetkit://search-widget/search");

  EXPECT_FALSE(request.isFromShareExtension);
  EXPECT_TRUE(HasAppGroupCommand());
}

// Tests that X-Callback URLs other than App Group commands neither consume the
// App Group command nor allow an account switch.
TEST_F(TaskRequestForURLContextTest, TestXCallbackOpenDoesNotConsumeCommand) {
  WriteShareExtensionCommand([NSDate date]);

  TaskRequestForURLContext* request = CreateRequestWithGaiaID(
      @"googlechrome://x-callback-url/open?url=https://www.example.com");

  EXPECT_FALSE(request.isFromShareExtension);
  EXPECT_NSEQ(nil, request.gaiaID);
  EXPECT_TRUE(HasAppGroupCommand());
}

// Tests that widget URLs allow an account switch without being attributed to
// the share extension.
TEST_F(TaskRequestForURLContextTest, TestWidgetURLSetsGaiaID) {
  TaskRequestForURLContext* request =
      CreateRequestWithGaiaID(@"chromewidgetkit://search-widget/search");

  EXPECT_FALSE(request.isFromShareExtension);
  EXPECT_NSEQ(kTestGaiaID, request.gaiaID);
}

// Tests that standard URLs don't allow an account switch.
TEST_F(TaskRequestForURLContextTest, TestStandardURLIgnoresGaiaID) {
  TaskRequestForURLContext* request =
      CreateRequestWithGaiaID(@"https://www.example.com");

  EXPECT_FALSE(request.isFromShareExtension);
  EXPECT_NSEQ(nil, request.gaiaID);
}

// Tests that the caller app of a valid App Group command is taken from the
// command, which is a trusted source.
TEST_F(TaskRequestForURLContextTest, TestShareExtensionCommandCallerApp) {
  base::HistogramTester histogram_tester;
  WriteShareExtensionCommand([NSDate date]);
  TaskRequestForURLContext* request =
      CreateRequestWithGaiaID(kAppGroupCommandURL);

  [request execute];

  histogram_tester.ExpectUniqueSample("Startup.MobileSessionStartFromApps",
                                      CALLER_APP_GOOGLE_CHROME_SHARE_EXTENSION,
                                      1);
}

// Tests that standard HTTP/HTTPS, File, and External Action URLs log launch
// source, startup metrics, and user actions during execute.
TEST_F(TaskRequestForURLContextTest, TestSimpleURLMetrics) {
  struct TestCase {
    NSString* url_string;
    MobileSessionStartAction expected_action;
    std::optional<AppLaunchSource> expected_launch_source;
    const char* expected_user_action;
  } test_cases[] = {
      {@"http://www.example.com", START_ACTION_OPEN_HTTP_FROM_OS,
       AppLaunchSource::LINK_OPENED_FROM_OS, "MobileDefaultBrowserViewIntent"},
      {@"https://www.example.com", START_ACTION_OPEN_HTTPS_FROM_OS,
       AppLaunchSource::LINK_OPENED_FROM_OS, "MobileDefaultBrowserViewIntent"},
      {@"googlechrome://www.example.com", START_ACTION_OPEN_HTTP,
       AppLaunchSource::LINK_OPENED_FROM_APP, "MobileFirstPartyViewIntent"},
      {@"googlechromes://www.example.com", START_ACTION_OPEN_HTTPS,
       AppLaunchSource::LINK_OPENED_FROM_APP, "MobileFirstPartyViewIntent"},
      {@"googlechrome://ChromeExternalAction", START_EXTERNAL_ACTION,
       AppLaunchSource::EXTERNAL_ACTION, "MobileExternalActionURLOpened"},
      {@"file:///path/to/file.txt", START_ACTION_OPEN_FILE, std::nullopt,
       nullptr},
  };

  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    NSURL* url = [NSURL URLWithString:test_case.url_string];
    UIOpenURLContext* context = CreateMockURLContext(url);

    TaskRequestForURLContext* request =
        [TaskRequestForURLContext taskRequestWithURLContext:context
                                                 sceneState:scene_state_
                                                isColdStart:YES];
    EXPECT_NE(request, nil);
    histogram_tester.ExpectTotalCount(kUMAMobileSessionStartActionHistogram, 0);
    histogram_tester.ExpectTotalCount(kAppLaunchSource, 0);
    if (test_case.expected_user_action) {
      EXPECT_EQ(
          user_action_tester.GetActionCount(test_case.expected_user_action), 0);
    }

    [request execute];

    histogram_tester.ExpectUniqueSample(kUMAMobileSessionStartActionHistogram,
                                        test_case.expected_action, 1);
    if (test_case.expected_launch_source.has_value()) {
      histogram_tester.ExpectUniqueSample(
          kAppLaunchSource, test_case.expected_launch_source.value(), 1);
    }
    if (test_case.expected_user_action) {
      EXPECT_EQ(
          user_action_tester.GetActionCount(test_case.expected_user_action), 1);
    }
  }
}

// Tests that External Action URLs correctly parse path components, handle
// feature flags, and log IOS.ExternalAction metrics and user actions during
// execute.
TEST_F(TaskRequestForURLContextTest, TestExternalActionMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kAppSwitcherAISummarization}, {});
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAppSwitcherAISummarizationTestingExternalURL);

  struct TestCase {
    NSString* url_string;
    IOSExternalAction expected_action;
    const char* expected_user_action;
  } test_cases[] = {
      {@"googlechrome://ChromeExternalAction",
       IOSExternalAction::ACTION_INVALID, nullptr},
      {@"googlechrome://ChromeExternalAction/OpenNTP",
       IOSExternalAction::ACTION_OPEN_NTP,
       "MobileExternalActionURLOpenedWithOpenNTP"},
      {@"googlechrome://ChromeExternalAction/DefaultBrowserSettings",
       IOSExternalAction::ACTION_DEFAULT_BROWSER_SETTINGS,
       "MobileExternalActionURLOpenedWithDefaultBrowserSettings"},
      {@"googlechrome://ChromeExternalAction/appstoregeminipromo",
       IOSExternalAction::ACTION_APP_STORE_GEMINI_PROMO,
       "MobileExternalActionURLOpenedWithAppStoreGeminiPromo"},
      {@"googlechrome://ChromeExternalAction/appswitchertesting",
       IOSExternalAction::ACTION_START_GEMINI_AI_SUMMARIZATION, nullptr},
  };

  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    NSURL* url = [NSURL URLWithString:test_case.url_string];
    UIOpenURLContext* context = CreateMockURLContext(url);

    TaskRequestForURLContext* request =
        [TaskRequestForURLContext taskRequestWithURLContext:context
                                                 sceneState:scene_state_
                                                isColdStart:YES];
    EXPECT_NE(request, nil);
    histogram_tester.ExpectTotalCount(kExternalActionHistogram, 0);
    histogram_tester.ExpectTotalCount(kAppLaunchSource, 0);
    EXPECT_EQ(
        user_action_tester.GetActionCount("MobileExternalActionURLOpened"), 0);

    [request execute];

    histogram_tester.ExpectUniqueSample(kExternalActionHistogram,
                                        test_case.expected_action, 1);
    histogram_tester.ExpectUniqueSample(kAppLaunchSource,
                                        AppLaunchSource::EXTERNAL_ACTION, 1);
    EXPECT_EQ(
        user_action_tester.GetActionCount("MobileExternalActionURLOpened"), 1);
    if (test_case.expected_user_action) {
      EXPECT_EQ(
          user_action_tester.GetActionCount(test_case.expected_user_action), 1);
    }
  }
}

// Tests that WidgetKit URL execution correctly triggers the scene controller.
TEST_F(TaskRequestForURLContextTest, TestWidgetURLContextExecution) {
  NSURL* url =
      [NSURL URLWithString:@"chromewidgetkit://quick-actions-widget/incognito"];
  UIOpenURLContext* context = CreateMockURLContext(url);

  TaskRequestForURLContext* request =
      [TaskRequestForURLContext taskRequestWithURLContext:context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  EXPECT_NE(request, nil);

  TaskRequestURLContextTestTabOpener* tab_opener =
      [[TaskRequestURLContextTestTabOpener alloc]
          initWithSceneState:scene_state_];
  scene_state_.controller = tab_opener;

  [request execute];

  EXPECT_EQ(tab_opener.targetMode, ApplicationModeForTabOpening::INCOGNITO);
  EXPECT_EQ(tab_opener.urlLoadParams.web_params.url, GURL("chrome://newtab/"));
  EXPECT_FALSE(tab_opener.dismissOmnibox);
  EXPECT_EQ(tab_opener.completionAction,
            TabOpeningPostOpeningAction::FOCUS_OMNIBOX);
  EXPECT_TRUE(tab_opener.completionActionExecuted);
}

// Tests that a file URL execution loads the physical file URL and sets the
// chrome://external-file/ virtual URL.
TEST_F(TaskRequestForURLContextTest, TestFileURLContextExecution) {
  NSURL* url = [NSURL URLWithString:@"file:///path/to/test.pdf"];
  UIOpenURLContext* context = CreateMockURLContext(url);

  TaskRequestForURLContext* request =
      [TaskRequestForURLContext taskRequestWithURLContext:context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  EXPECT_NE(request, nil);

  TaskRequestURLContextTestTabOpener* tab_opener =
      [[TaskRequestURLContextTestTabOpener alloc]
          initWithSceneState:scene_state_];
  scene_state_.controller = tab_opener;

  [request execute];

  EXPECT_EQ(tab_opener.targetMode, ApplicationModeForTabOpening::NORMAL);
  EXPECT_EQ(tab_opener.urlLoadParams.web_params.url,
            GURL("file:///path/to/test.pdf"));
  EXPECT_EQ(tab_opener.urlLoadParams.web_params.virtual_url,
            GURL("chrome://external-file/test.pdf"));
  EXPECT_TRUE(tab_opener.dismissOmnibox);
}

// Tests that an external action OpenNTP URL execution opens the NTP.
TEST_F(TaskRequestForURLContextTest, TestExternalActionOpenNTPExecution) {
  NSURL* url =
      [NSURL URLWithString:@"googlechrome://ChromeExternalAction/OpenNTP"];
  UIOpenURLContext* context = CreateMockURLContext(url);

  TaskRequestForURLContext* request =
      [TaskRequestForURLContext taskRequestWithURLContext:context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  EXPECT_NE(request, nil);

  TaskRequestURLContextTestTabOpener* tab_opener =
      [[TaskRequestURLContextTestTabOpener alloc]
          initWithSceneState:scene_state_];
  scene_state_.controller = tab_opener;

  [request execute];

  EXPECT_EQ(tab_opener.targetMode, ApplicationModeForTabOpening::UNDETERMINED);
  EXPECT_EQ(tab_opener.urlLoadParams.web_params.url, GURL(kChromeUINewTabURL));
  EXPECT_TRUE(tab_opener.urlLoadParams.web_params.virtual_url.is_empty());
  EXPECT_TRUE(tab_opener.dismissOmnibox);
}

// Tests that an external action DefaultBrowserSettings URL opens the NTP when
// Chrome is likely the default browser.
TEST_F(TaskRequestForURLContextTest,
       TestExternalActionDefaultBrowserSettingsWhenLikelyDefault) {
  LogOpenHTTPURLFromExternalURL();

  NSURL* url =
      [NSURL URLWithString:
                 @"googlechrome://ChromeExternalAction/DefaultBrowserSettings"];
  UIOpenURLContext* context = CreateMockURLContext(url);

  TaskRequestForURLContext* request =
      [TaskRequestForURLContext taskRequestWithURLContext:context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  EXPECT_NE(request, nil);

  TaskRequestURLContextTestTabOpener* tab_opener =
      [[TaskRequestURLContextTestTabOpener alloc]
          initWithSceneState:scene_state_];
  scene_state_.controller = tab_opener;

  [request execute];

  EXPECT_EQ(tab_opener.targetMode, ApplicationModeForTabOpening::UNDETERMINED);
  EXPECT_EQ(tab_opener.urlLoadParams.web_params.url, GURL(kChromeUINewTabURL));
  EXPECT_TRUE(tab_opener.urlLoadParams.web_params.virtual_url.is_empty());
  EXPECT_TRUE(tab_opener.dismissOmnibox);
}

// Tests that an external action DefaultBrowserSettings URL triggers the show
// browser settings completion action when Chrome is not the default browser.
TEST_F(TaskRequestForURLContextTest,
       TestExternalActionDefaultBrowserSettingsWhenNotDefault) {
  ClearDefaultBrowserPromoData();

  NSURL* url =
      [NSURL URLWithString:
                 @"googlechrome://ChromeExternalAction/DefaultBrowserSettings"];
  UIOpenURLContext* context = CreateMockURLContext(url);

  TaskRequestForURLContext* request =
      [TaskRequestForURLContext taskRequestWithURLContext:context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  EXPECT_NE(request, nil);

  TaskRequestURLContextTestTabOpener* tab_opener =
      [[TaskRequestURLContextTestTabOpener alloc]
          initWithSceneState:scene_state_];
  scene_state_.controller = tab_opener;

  [request execute];

  EXPECT_EQ(tab_opener.targetMode, ApplicationModeForTabOpening::UNDETERMINED);
  EXPECT_TRUE(tab_opener.urlLoadParams.web_params.url.is_empty());
  EXPECT_EQ(tab_opener.completionAction,
            TabOpeningPostOpeningAction::EXTERNAL_ACTION_SHOW_BROWSER_SETTINGS);
  EXPECT_TRUE(tab_opener.completionActionExecuted);
  EXPECT_TRUE(tab_opener.dismissOmnibox);
}

// Tests that an external action AppStoreGeminiPromo URL opens the Gemini promo
// URL, sets TRIGGER_GEMINI_PROMO, and sets the pref.
TEST_F(TaskRequestForURLContextTest,
       TestExternalActionAppStoreGeminiPromoExecution) {
  NSURL* url = [NSURL
      URLWithString:@"googlechrome://ChromeExternalAction/appstoregeminipromo"];
  UIOpenURLContext* context = CreateMockURLContext(url);

  TaskRequestForURLContext* request =
      [TaskRequestForURLContext taskRequestWithURLContext:context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  EXPECT_NE(request, nil);

  TaskRequestURLContextTestTabOpener* tab_opener =
      [[TaskRequestURLContextTestTabOpener alloc]
          initWithSceneState:scene_state_];
  scene_state_.controller = tab_opener;

  [request execute];

  EXPECT_EQ(tab_opener.targetMode, ApplicationModeForTabOpening::UNDETERMINED);
  EXPECT_EQ(tab_opener.urlLoadParams.web_params.url,
            GURL(kGeminiAppStorePromoURL));
  EXPECT_TRUE(tab_opener.urlLoadParams.web_params.virtual_url.is_empty());
  EXPECT_EQ(tab_opener.completionAction,
            TabOpeningPostOpeningAction::TRIGGER_GEMINI_PROMO);
  EXPECT_TRUE(tab_opener.completionActionExecuted);
  EXPECT_TRUE(tab_opener.dismissOmnibox);
  EXPECT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kAppStoreGeminiPromoTriggered));
}

// Tests that standard HTTP/HTTPS and first-party custom scheme URLs open a tab
// with the expected HTTP/HTTPS URL, and unrecognized schemes are ignored.
TEST_F(TaskRequestForURLContextTest, TestStandardURLContextExecution) {
  struct TestCase {
    NSString* url_string;
    GURL expected_url;
  } test_cases[] = {
      {@"http://www.example.com/page", GURL("http://www.example.com/page")},
      {@"https://www.example.com/page", GURL("https://www.example.com/page")},
      {[NSString stringWithFormat:@"%s://www.example.com/page",
                                  BUILDFLAG(INSECURE_CUSTOM_SCHEME)],
       GURL("http://www.example.com/page")},
      {[NSString stringWithFormat:@"%s://www.example.com/page",
                                  BUILDFLAG(SECURE_CUSTOM_SCHEME)],
       GURL("https://www.example.com/page")},
  };

  for (const auto& test_case : test_cases) {
    NSURL* url = [NSURL URLWithString:test_case.url_string];
    UIOpenURLContext* context = CreateMockURLContext(url);

    TaskRequestForURLContext* request =
        [TaskRequestForURLContext taskRequestWithURLContext:context
                                                 sceneState:scene_state_
                                                isColdStart:YES];
    ASSERT_NE(request, nil);

    TaskRequestURLContextTestTabOpener* tab_opener =
        [[TaskRequestURLContextTestTabOpener alloc]
            initWithSceneState:scene_state_];
    scene_state_.controller = tab_opener;

    [request execute];

    EXPECT_EQ(tab_opener.targetMode, ApplicationModeForTabOpening::UNDETERMINED)
        << "Failed for URL: " << base::SysNSStringToUTF8(test_case.url_string);
    EXPECT_EQ(tab_opener.urlLoadParams.web_params.url, test_case.expected_url)
        << "Failed for URL: " << base::SysNSStringToUTF8(test_case.url_string);
    EXPECT_TRUE(tab_opener.urlLoadParams.web_params.virtual_url.is_empty())
        << "Failed for URL: " << base::SysNSStringToUTF8(test_case.url_string);
    EXPECT_TRUE(tab_opener.dismissOmnibox)
        << "Failed for URL: " << base::SysNSStringToUTF8(test_case.url_string);
  }

  // Verify that an unregistered custom scheme does not open a tab.
  NSURL* unknown_url = [NSURL URLWithString:@"unknownscheme://www.example.com"];
  UIOpenURLContext* unknown_context = CreateMockURLContext(unknown_url);
  TaskRequestForURLContext* unknown_request =
      [TaskRequestForURLContext taskRequestWithURLContext:unknown_context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  ASSERT_NE(unknown_request, nil);
  TaskRequestURLContextTestTabOpener* tab_opener =
      [[TaskRequestURLContextTestTabOpener alloc]
          initWithSceneState:scene_state_];
  scene_state_.controller = tab_opener;
  [unknown_request execute];
  EXPECT_TRUE(tab_opener.urlLoadParams.web_params.url.is_empty());
}

// Tests that Google One deep links do not open the URL in a tab and invoke
// GoogleOneCommands with the normalized HTTPS URL when
// kSupportGoogleOneDeepLink is enabled.
TEST_F(TaskRequestForURLContextTest, TestGoogleOneDeepLinkEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSupportGoogleOneDeepLink);

  CommandDispatcher* dispatcher = scene_state_.browserProviderInterface
                                      .currentBrowser->GetCommandDispatcher();
  id mock_google_one_handler = OCMProtocolMock(@protocol(GoogleOneCommands));
  [dispatcher startDispatchingToTarget:mock_google_one_handler
                           forProtocol:@protocol(GoogleOneCommands)];

  NSArray<NSString*>* url_strings = @[
    @"https://one.google.com/deeplink",
    [NSString stringWithFormat:@"%s://one.google.com/deeplink",
                               BUILDFLAG(SECURE_CUSTOM_SCHEME)],
  ];

  for (NSString* url_string in url_strings) {
    SCOPED_TRACE(base::SysNSStringToUTF8(url_string));
    NSURL* url = [NSURL URLWithString:url_string];
    UIOpenURLContext* context = CreateMockURLContext(url);

    TaskRequestForURLContext* request =
        [TaskRequestForURLContext taskRequestWithURLContext:context
                                                 sceneState:scene_state_
                                                isColdStart:YES];
    ASSERT_NE(request, nil);

    TaskRequestURLContextTestTabOpener* tab_opener =
        [[TaskRequestURLContextTestTabOpener alloc]
            initWithSceneState:scene_state_];
    scene_state_.controller = tab_opener;

    __block GURL shown_url;
    OCMExpect([[mock_google_one_handler ignoringNonObjectArgs]
                  showGoogleOneForURL:GURL()])
        .andDo(^(NSInvocation* invocation) {
          const GURL* url_arg = nullptr;
          [invocation getArgument:&url_arg atIndex:2];
          shown_url = *url_arg;
        });

    [request execute];

    EXPECT_EQ(tab_opener.targetMode,
              ApplicationModeForTabOpening::UNDETERMINED);
    EXPECT_TRUE(tab_opener.urlLoadParams.web_params.url.is_empty());
    EXPECT_FALSE(tab_opener.completionActionExecuted);
    EXPECT_TRUE(tab_opener.dismissOmnibox);
    EXPECT_EQ(shown_url, GURL("https://one.google.com/deeplink"));
    EXPECT_OCMOCK_VERIFY(mock_google_one_handler);
  }

  [dispatcher stopDispatchingToTarget:mock_google_one_handler];
}

// Tests that Google One deep links open the URL normally when
// kSupportGoogleOneDeepLink is disabled.
TEST_F(TaskRequestForURLContextTest, TestGoogleOneDeepLinkDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kSupportGoogleOneDeepLink);

  NSURL* url = [NSURL URLWithString:@"https://one.google.com/deeplink"];
  UIOpenURLContext* context = CreateMockURLContext(url);

  TaskRequestForURLContext* request =
      [TaskRequestForURLContext taskRequestWithURLContext:context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  ASSERT_NE(request, nil);

  TaskRequestURLContextTestTabOpener* tab_opener =
      [[TaskRequestURLContextTestTabOpener alloc]
          initWithSceneState:scene_state_];
  scene_state_.controller = tab_opener;

  [request execute];

  EXPECT_EQ(tab_opener.targetMode, ApplicationModeForTabOpening::UNDETERMINED);
  EXPECT_EQ(tab_opener.urlLoadParams.web_params.url,
            GURL("https://one.google.com/deeplink"));
  EXPECT_EQ(tab_opener.completionAction,
            TabOpeningPostOpeningAction::NO_ACTION);
  EXPECT_TRUE(tab_opener.dismissOmnibox);
}

// Tests that X-Callback `/open` URLs open the `url` parameter in a new tab and
// map the `poa` parameter to the expected post-opening action.
TEST_F(TaskRequestForURLContextTest, TestXCallbackOpenCommandExecution) {
  struct TestCase {
    NSString* url_string;
    TabOpeningPostOpeningAction expected_action;
  } test_cases[] = {
      {@"googlechrome://x-callback-url/open?url=https%3A%2F%2Fwww.example.com",
       TabOpeningPostOpeningAction::NO_ACTION},
      {@"googlechrome://x-callback-url/open?url=https%3A%2F%2Fwww.example.com"
       @"&poa=default-browser-settings",
       TabOpeningPostOpeningAction::SHOW_DEFAULT_BROWSER_SETTINGS},
      {@"googlechrome://x-callback-url/open?url=https%3A%2F%2Fwww.example.com"
       @"&poa=unknown",
       TabOpeningPostOpeningAction::NO_ACTION},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::SysNSStringToUTF8(test_case.url_string));
    UIOpenURLContext* context =
        CreateMockURLContext([NSURL URLWithString:test_case.url_string]);
    TaskRequestForURLContext* request =
        [TaskRequestForURLContext taskRequestWithURLContext:context
                                                 sceneState:scene_state_
                                                isColdStart:YES];
    ASSERT_NE(request, nil);

    TaskRequestURLContextTestTabOpener* tab_opener =
        [[TaskRequestURLContextTestTabOpener alloc]
            initWithSceneState:scene_state_];
    scene_state_.controller = tab_opener;

    [request execute];

    EXPECT_EQ(tab_opener.targetMode,
              ApplicationModeForTabOpening::UNDETERMINED);
    EXPECT_EQ(tab_opener.urlLoadParams.web_params.url,
              GURL("https://www.example.com"));
    EXPECT_TRUE(tab_opener.urlLoadParams.web_params.virtual_url.is_empty());
    EXPECT_TRUE(tab_opener.dismissOmnibox);
    EXPECT_EQ(tab_opener.completionAction, test_case.expected_action);
    EXPECT_TRUE(tab_opener.completionActionExecuted);
  }
}

// Tests that X-Callback URLs with a missing or non-http(s) `url` parameter, or
// with an unsupported command, do not open a tab.
TEST_F(TaskRequestForURLContextTest, TestXCallbackInvalidCommandIgnored) {
  NSString* url_strings[] = {
      @"googlechrome://x-callback-url/open",
      @"googlechrome://x-callback-url/open?url=invalid",
      @"googlechrome://x-callback-url/open?url=chrome%3A%2F%2Fsettings",
      @"googlechrome://x-callback-url/open?url=javascript%3Aalert(1)",
      @"googlechrome://x-callback-url/other?url=https%3A%2F%2Fwww.example.com",
  };

  for (NSString* url_string : url_strings) {
    SCOPED_TRACE(base::SysNSStringToUTF8(url_string));
    UIOpenURLContext* context =
        CreateMockURLContext([NSURL URLWithString:url_string]);
    TaskRequestForURLContext* request =
        [TaskRequestForURLContext taskRequestWithURLContext:context
                                                 sceneState:scene_state_
                                                isColdStart:YES];
    ASSERT_NE(request, nil);

    TaskRequestURLContextTestTabOpener* tab_opener =
        [[TaskRequestURLContextTestTabOpener alloc]
            initWithSceneState:scene_state_];
    scene_state_.controller = tab_opener;

    [request execute];

    EXPECT_TRUE(tab_opener.urlLoadParams.web_params.url.is_empty());
    EXPECT_FALSE(tab_opener.completionActionExecuted);
  }
}

// Tests that valid App Group commands open the expected tab and trigger the
// expected post-opening action.
TEST_F(TaskRequestForURLContextTest, TestAppGroupCommandExecution) {
  NSString* const kExternalURL = @"https://www.example.com/";
  const GURL kNTPURL(kChromeUINewTabURL);
  struct TestCase {
    NSString* command;
    NSString* external_text;
    GURL expected_url;
    ApplicationModeForTabOpening expected_mode;
    TabOpeningPostOpeningAction expected_action;
    bool expected_dismiss_omnibox;
  } test_cases[] = {
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupOpenURLCommand),
       kExternalURL, GURL(base::SysNSStringToUTF8(kExternalURL)),
       ApplicationModeForTabOpening::UNDETERMINED,
       TabOpeningPostOpeningAction::NO_ACTION, true},
      {app_group::kChromeAppGroupOpenURLInIcognitoCommand, kExternalURL,
       GURL(base::SysNSStringToUTF8(kExternalURL)),
       ApplicationModeForTabOpening::INCOGNITO,
       TabOpeningPostOpeningAction::NO_ACTION, true},
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupVoiceSearchCommand),
       nil, kNTPURL, ApplicationModeForTabOpening::NORMAL,
       TabOpeningPostOpeningAction::START_VOICE_SEARCH, true},
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupNewTabCommand), nil,
       kNTPURL, ApplicationModeForTabOpening::NORMAL,
       TabOpeningPostOpeningAction::NO_ACTION, true},
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupFocusOmniboxCommand),
       nil, kNTPURL, ApplicationModeForTabOpening::NORMAL,
       TabOpeningPostOpeningAction::FOCUS_OMNIBOX, false},
      {base::SysUTF8ToNSString(
           app_group::kChromeAppGroupIncognitoSearchCommand),
       nil, kNTPURL, ApplicationModeForTabOpening::INCOGNITO,
       TabOpeningPostOpeningAction::FOCUS_OMNIBOX, false},
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupQRScannerCommand), nil,
       kNTPURL, ApplicationModeForTabOpening::NORMAL,
       TabOpeningPostOpeningAction::START_QR_CODE_SCANNER, true},
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupLensCommand), nil,
       GURL(), ApplicationModeForTabOpening::NORMAL,
       TabOpeningPostOpeningAction::START_LENS_FROM_HOME_SCREEN_WIDGET, true},
      {base::SysUTF8ToNSString(
           app_group::kChromeAppGroupSearchPasswordsCommand),
       nil, GURL(), ApplicationModeForTabOpening::NORMAL,
       TabOpeningPostOpeningAction::SEARCH_PASSWORDS, true},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::SysNSStringToUTF8(test_case.command));
    WriteRecentAppGroupCommand(test_case.command,
                               app_group::kOpenCommandSourceShareExtension,
                               test_case.external_text);

    TaskRequestURLContextTestTabOpener* tab_opener =
        ExecuteAppGroupCommandRequest();

    EXPECT_EQ(tab_opener.targetMode, test_case.expected_mode);
    EXPECT_EQ(tab_opener.urlLoadParams.web_params.url, test_case.expected_url);
    EXPECT_TRUE(tab_opener.urlLoadParams.web_params.virtual_url.is_empty());
    EXPECT_FALSE(tab_opener.urlLoadParams.from_widget_or_siri);
    EXPECT_EQ(tab_opener.dismissOmnibox, test_case.expected_dismiss_omnibox);
    EXPECT_EQ(tab_opener.completionAction, test_case.expected_action);
    EXPECT_TRUE(tab_opener.completionActionExecuted);
  }
}

// Tests that App Group open URL commands without an http(s) URL are discarded.
TEST_F(TaskRequestForURLContextTest, TestAppGroupOpenURLCommandWithoutHTTPURL) {
  NSString* const kOpenURLCommand =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupOpenURLCommand);
  NSString* external_texts[] = {
      nil,
      @"invalid",
      @"chrome://settings",
      @"javascript:alert(1)",
      @"file:///path/to/file.pdf",
  };

  for (NSString* external_text : external_texts) {
    SCOPED_TRACE(external_text ? base::SysNSStringToUTF8(external_text)
                               : "no text");
    WriteRecentAppGroupCommand(kOpenURLCommand,
                               app_group::kOpenCommandSourceShareExtension,
                               external_text);

    TaskRequestURLContextTestTabOpener* tab_opener =
        ExecuteAppGroupCommandRequest();

    EXPECT_FALSE(tab_opener.completionActionExecuted);
    EXPECT_TRUE(tab_opener.urlLoadParams.web_params.url.is_empty());
  }
}

// Tests that App Group text search commands open the results of the default
// search engine in the requested mode.
TEST_F(TaskRequestForURLContextTest, TestAppGroupTextSearchCommandExecution) {
  SetUpDefaultSearchEngine();
  struct TestCase {
    NSString* command;
    ApplicationModeForTabOpening expected_mode;
  } test_cases[] = {
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupSearchTextCommand),
       ApplicationModeForTabOpening::UNDETERMINED},
      {app_group::kChromeAppGroupIncognitoSearchTextCommand,
       ApplicationModeForTabOpening::INCOGNITO},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::SysNSStringToUTF8(test_case.command));
    WriteRecentAppGroupCommand(test_case.command,
                               app_group::kOpenCommandSourceShareExtension,
                               kTestSearchQuery);

    TaskRequestURLContextTestTabOpener* tab_opener =
        ExecuteAppGroupCommandRequest();

    EXPECT_EQ(tab_opener.targetMode, test_case.expected_mode);
    EXPECT_EQ(tab_opener.urlLoadParams.web_params.url,
              kTestSearchQueryResultsURL);
    EXPECT_TRUE(tab_opener.urlLoadParams.web_params.virtual_url.is_empty());
    EXPECT_FALSE(tab_opener.urlLoadParams.from_widget_or_siri);
    EXPECT_TRUE(tab_opener.dismissOmnibox);
    EXPECT_EQ(tab_opener.completionAction,
              TabOpeningPostOpeningAction::NO_ACTION);
    EXPECT_TRUE(tab_opener.completionActionExecuted);
  }
}

// Tests that an App Group text search command with an empty text still opens
// the search results.
TEST_F(TaskRequestForURLContextTest, TestAppGroupTextSearchCommandEmptyText) {
  SetUpDefaultSearchEngine();
  WriteRecentAppGroupCommand(
      base::SysUTF8ToNSString(app_group::kChromeAppGroupSearchTextCommand),
      app_group::kOpenCommandSourceShareExtension, @"");

  TaskRequestURLContextTestTabOpener* tab_opener =
      ExecuteAppGroupCommandRequest();

  EXPECT_EQ(tab_opener.urlLoadParams.web_params.url,
            GURL("https://www.example.com/search?q="));
  EXPECT_TRUE(tab_opener.completionActionExecuted);
}

// Tests that App Group text search commands without a text are discarded.
TEST_F(TaskRequestForURLContextTest,
       TestAppGroupTextSearchCommandWithoutTextIgnored) {
  SetUpDefaultSearchEngine();
  NSString* commands[] = {
      base::SysUTF8ToNSString(app_group::kChromeAppGroupSearchTextCommand),
      app_group::kChromeAppGroupIncognitoSearchTextCommand,
  };

  for (NSString* command : commands) {
    SCOPED_TRACE(base::SysNSStringToUTF8(command));
    WriteRecentAppGroupCommand(
        command, app_group::kOpenCommandSourceShareExtension, nil);

    TaskRequestURLContextTestTabOpener* tab_opener =
        ExecuteAppGroupCommandRequest();

    EXPECT_FALSE(tab_opener.completionActionExecuted);
    EXPECT_TRUE(tab_opener.urlLoadParams.web_params.url.is_empty());
  }
}

// Tests that an App Group text search command opens the NTP when the default
// search engine is disabled by policy.
TEST_F(TaskRequestForURLContextTest,
       TestAppGroupTextSearchCommandWithoutDefaultSearchEngine) {
  base::DictValue policy;
  policy.Set(DefaultSearchManager::kDisabledByPolicy, true);
  profile_->GetTestingPrefService()->SetManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      std::move(policy));
  ASSERT_FALSE(ios::TemplateURLServiceFactory::GetForProfile(profile_.get())
                   ->GetDefaultSearchProvider());
  WriteRecentAppGroupCommand(
      base::SysUTF8ToNSString(app_group::kChromeAppGroupSearchTextCommand),
      app_group::kOpenCommandSourceShareExtension, kTestSearchQuery);

  TaskRequestURLContextTestTabOpener* tab_opener =
      ExecuteAppGroupCommandRequest();

  EXPECT_EQ(tab_opener.urlLoadParams.web_params.url, GURL(kChromeUINewTabURL));
  EXPECT_EQ(tab_opener.targetMode, ApplicationModeForTabOpening::UNDETERMINED);
  EXPECT_TRUE(tab_opener.completionActionExecuted);
}

// Tests that unknown App Group commands, and the image search commands not yet
// supported by the new startup flow, don't open a tab.
TEST_F(TaskRequestForURLContextTest, TestUnhandledAppGroupCommandIgnored) {
  NSString* commands[] = {
      @"unknowncommand",
      base::SysUTF8ToNSString(app_group::kChromeAppGroupSearchImageCommand),
      app_group::kChromeAppGroupIncognitoSearchImageCommand,
  };

  for (NSString* command : commands) {
    SCOPED_TRACE(base::SysNSStringToUTF8(command));
    WriteRecentAppGroupCommand(
        command, app_group::kOpenCommandSourceShareExtension, @"text");

    TaskRequestURLContextTestTabOpener* tab_opener =
        ExecuteAppGroupCommandRequest();

    EXPECT_FALSE(tab_opener.completionActionExecuted);
    EXPECT_TRUE(tab_opener.urlLoadParams.web_params.url.is_empty());
  }
}

// Tests that an expired App Group command is discarded.
TEST_F(TaskRequestForURLContextTest, TestExpiredAppGroupCommandIgnored) {
  WriteShareExtensionCommand([NSDate
      dateWithTimeIntervalSinceNow:-app_group::kAppGroupCommandTimeout - 1]);

  EXPECT_FALSE(ExecuteAppGroupCommandRequest().completionActionExecuted);
}

// Tests that an App Group command without a command name is discarded.
TEST_F(TaskRequestForURLContextTest, TestAppGroupCommandWithoutNameIgnored) {
  WriteAppGroupCommandWithOnlyTime([NSDate date]);

  EXPECT_FALSE(ExecuteAppGroupCommandRequest().completionActionExecuted);
}

// Tests that an App Group command can't be executed twice.
TEST_F(TaskRequestForURLContextTest, TestAppGroupCommandExecutedOnce) {
  WriteRecentAppGroupCommand(
      base::SysUTF8ToNSString(app_group::kChromeAppGroupNewTabCommand),
      app_group::kOpenCommandSourceShareExtension, nil);
  EXPECT_TRUE(ExecuteAppGroupCommandRequest().completionActionExecuted);

  EXPECT_FALSE(ExecuteAppGroupCommandRequest().completionActionExecuted);
}

// Tests that Google One deep links with an email parameter resolve the matching
// device identity's gaiaID when kSupportGoogleOneDeepLink is enabled.
TEST_F(TaskRequestForURLContextTest, TestGoogleOneDeepLinkGaiaID) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSupportGoogleOneDeepLink);

  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(identity);

  NSString* matching_url_string = [NSString
      stringWithFormat:@"%s://one.google.com/deeplink?email=%@",
                       BUILDFLAG(SECURE_CUSTOM_SCHEME), identity.userEmail];
  UIOpenURLContext* matching_context =
      CreateMockURLContext([NSURL URLWithString:matching_url_string]);
  TaskRequestForURLContext* matching_request =
      [TaskRequestForURLContext taskRequestWithURLContext:matching_context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  ASSERT_NE(matching_request, nil);
  EXPECT_NSEQ(identity.gaiaId.ToNSString(), matching_request.gaiaID);

  // Unknown email returns nil gaiaID.
  NSURL* unknown_email_url = [NSURL
      URLWithString:@"https://one.google.com/deeplink?email=unknown@test.com"];
  UIOpenURLContext* unknown_context = CreateMockURLContext(unknown_email_url);
  TaskRequestForURLContext* unknown_request =
      [TaskRequestForURLContext taskRequestWithURLContext:unknown_context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  ASSERT_NE(unknown_request, nil);
  EXPECT_NSEQ(nil, unknown_request.gaiaID);

  // Disabled feature returns nil gaiaID even for a matching device identity.
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(kSupportGoogleOneDeepLink);
  TaskRequestForURLContext* disabled_request =
      [TaskRequestForURLContext taskRequestWithURLContext:matching_context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  ASSERT_NE(disabled_request, nil);
  EXPECT_NSEQ(nil, disabled_request.gaiaID);
}
