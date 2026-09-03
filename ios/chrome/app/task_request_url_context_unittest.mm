// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_url_context.h"

#import <UIKit/UIKit.h>

#import <optional>

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class TaskRequestForURLContextTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    ResetEnableNewStartupFlowEnabledForTesting();
    scoped_feature_list_.InitAndEnableFeature(kEnableNewStartupFlow);
    SaveEnableNewStartupFlowForNextStart();

    mock_startup_information_ = OCMProtocolMock(@protocol(StartupInformation));
    app_state_ =
        [[AppState alloc] initWithStartupInformation:mock_startup_information_];
    profile_ = TestProfileIOS::Builder().Build();
    profile_state_ = [[ProfileState alloc] initWithAppState:app_state_];
    profile_state_.profile = profile_.get();
    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.profileState = profile_state_;
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
  }

  void TearDown() override {
    app_state_ = nil;
    mock_startup_information_ = nil;
    browser_.reset();
    [scene_state_ shutdown];
    scene_state_ = nil;
    profile_state_ = nil;
    profile_.reset();
    ResetEnableNewStartupFlowEnabledForTesting();
    PlatformTest::TearDown();
  }

  UIOpenURLContext* CreateMockURLContext(NSURL* url) {
    UIOpenURLContext* mockContext = OCMClassMock([UIOpenURLContext class]);
    OCMStub([mockContext URL]).andReturn(url);
    return mockContext;
  }

  // Sets whether the app is in first run for testing metrics.
  void SetIsFirstRun(BOOL is_first_run) {
    OCMStub([mock_startup_information_ isFirstRun]).andReturn(is_first_run);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  id mock_startup_information_;
  AppState* app_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* profile_state_;
  FakeSceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests that Startup.MobileSessionStartFromApps is logged.
TEST_F(TaskRequestForURLContextTest, TestStartupMobileSessionStartFromApps) {
  base::HistogramTester histogram_tester;
  NSURL* url = [NSURL URLWithString:@"https://www.example.com"];
  UIOpenURLContext* context = CreateMockURLContext(url);

  TaskRequestForURLContext* request =
      [TaskRequestForURLContext taskRequestWithURLContext:context
                                               sceneState:scene_state_
                                              isColdStart:YES];
  EXPECT_NE(request, nil);

  histogram_tester.ExpectTotalCount("Startup.MobileSessionStartFromApps", 1);
}

// Tests that Startup.ShowDefaultPromoFromApps is logged when URL contains the
// default browser settings query item.
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

// Tests that WidgetKit URLs log the launch source and WidgetKit action metrics.
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

    histogram_tester.ExpectUniqueSample(kUMAMobileSessionStartActionHistogram,
                                        START_ACTION_WIDGET_KIT_COMMAND, 1);
    histogram_tester.ExpectUniqueSample(kAppLaunchSource,
                                        AppLaunchSource::WIDGET, 1);
    histogram_tester.ExpectUniqueSample(kWidgetKitActionHistogram,
                                        test_case.expected_action, 1);
  }
}

// Tests that X-Callback URLs log the launch source and X-Callback action
// metrics.
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

    histogram_tester.ExpectUniqueSample(kUMAMobileSessionStartActionHistogram,
                                        test_case.expected_action, 1);
    histogram_tester.ExpectUniqueSample(kAppLaunchSource,
                                        AppLaunchSource::X_CALLBACK, 1);
  }
}

// Tests that X-Callback app-group-command URLs log the application group
// command delay metric.
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

  histogram_tester.ExpectUniqueSample(kUMAMobileSessionStartActionHistogram,
                                      START_ACTION_XCALLBACK_APPGROUP_COMMAND,
                                      1);
  histogram_tester.ExpectUniqueSample(kAppLaunchSource,
                                      AppLaunchSource::X_CALLBACK, 1);

  // Verify that the command delay was recorded.
  histogram_tester.ExpectTotalCount("Startup.ApplicationGroupCommandDelay", 1);
  histogram_tester.ExpectUniqueSample("Startup.ApplicationGroupCommandDelay", 5,
                                      1);

  // Clean up.
  [sharedDefaults removeObjectForKey:commandDictionaryPreference];
}

// Tests that standard HTTP/HTTPS, File, and External Action URLs log launch
// source, startup metrics, and user actions.
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
// feature flags, and log IOS.ExternalAction metrics and user actions.
TEST_F(TaskRequestForURLContextTest, TestExternalActionMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kAppSwitcherAISummarization}, {});

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

// Test double for SceneController to verify tab opening from TaskRequests.
@interface TaskRequestURLContextTestTabOpener : SceneController <TabOpening>

@property(nonatomic, assign) ApplicationModeForTabOpening targetMode;
@property(nonatomic, assign) UrlLoadParams urlLoadParams;
@property(nonatomic, assign) BOOL dismissOmnibox;
@property(nonatomic, assign) TabOpeningPostOpeningAction completionAction;
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
