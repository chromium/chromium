// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/app_state.h"

#include <memory>

#include "base/bind.h"
#include "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/application_delegate/app_state_testing.h"
#import "ios/chrome/app/application_delegate/browser_launcher.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/app/application_delegate/memory_warning_helper.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/mock_tab_opener.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_switching.h"
#import "ios/chrome/app/application_delegate/user_activity_handler.h"
#import "ios/chrome/app/main_application_delegate.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_config.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/connection_information.h"
#import "ios/chrome/browser/ui/main/test/fake_scene_state.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface_provider.h"
#import "ios/chrome/browser/ui/safe_mode/safe_mode_coordinator.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_provider.h"
#import "ios/chrome/test/scoped_key_window.h"
#include "ios/public/provider/chrome/browser/distribution/app_distribution_provider.h"
#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/user_feedback/test_user_feedback_provider.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/testing/scoped_block_swizzler.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_task_traits.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Exposes private safe mode start/stop methods.
@interface AppState (Private)
- (void)startSafeMode;
- (void)stopSafeMode;
@end

#pragma mark - Class definition.

namespace {

// A block that takes self as argument and return a BOOL.
typedef BOOL (^DecisionBlock)(id self);
// A block that takes the arguments of UserActivityHandler's
// +handleStartupParametersWithTabOpener.
typedef void (^HandleStartupParam)(
    id self,
    id<TabOpening> tabOpener,
    id<ConnectionInformation> connectionInformation,
    id<StartupInformation> startupInformation,
    ChromeBrowserState* browserState);
// A block ths returns values of AppState connectedScenes.
typedef NSArray<SceneState*>* (^ScenesBlock)(id self);

class FakeAppDistributionProvider : public AppDistributionProvider {
 public:
  FakeAppDistributionProvider() : cancel_called_(false) {}
  ~FakeAppDistributionProvider() override {}

  void CancelDistributionNotifications() override { cancel_called_ = true; }
  bool cancel_called() { return cancel_called_; }

 private:
  bool cancel_called_;
  DISALLOW_COPY_AND_ASSIGN(FakeAppDistributionProvider);
};

class FakeUserFeedbackProvider : public TestUserFeedbackProvider {
 public:
  FakeUserFeedbackProvider() : synchronize_called_(false) {}
  ~FakeUserFeedbackProvider() override {}
  void Synchronize() override { synchronize_called_ = true; }
  bool synchronize_called() { return synchronize_called_; }

 private:
  bool synchronize_called_;
  DISALLOW_COPY_AND_ASSIGN(FakeUserFeedbackProvider);
};

class FakeChromeBrowserProvider : public ios::TestChromeBrowserProvider {
 public:
  FakeChromeBrowserProvider()
      : app_distribution_provider_(
            std::make_unique<FakeAppDistributionProvider>()),
        user_feedback_provider_(std::make_unique<FakeUserFeedbackProvider>()) {}
  ~FakeChromeBrowserProvider() override {}

  AppDistributionProvider* GetAppDistributionProvider() const override {
    return app_distribution_provider_.get();
  }

  UserFeedbackProvider* GetUserFeedbackProvider() const override {
    return user_feedback_provider_.get();
  }

 private:
  std::unique_ptr<FakeAppDistributionProvider> app_distribution_provider_;
  std::unique_ptr<FakeUserFeedbackProvider> user_feedback_provider_;
  DISALLOW_COPY_AND_ASSIGN(FakeChromeBrowserProvider);
};

}  // namespace

class AppStateTest : public BlockCleanupTest {
 protected:
  AppStateTest() {
    browser_launcher_mock_ =
        [OCMockObject mockForProtocol:@protocol(BrowserLauncher)];
    startup_information_mock_ =
        [OCMockObject mockForProtocol:@protocol(StartupInformation)];
    connection_information_mock_ =
        [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];
    main_application_delegate_ =
        [OCMockObject mockForClass:[MainApplicationDelegate class]];
    window_ = [OCMockObject mockForClass:[UIWindow class]];

    interface_provider_ = [[StubBrowserInterfaceProvider alloc] init];
  }

  void SetUp() override {
    BlockCleanupTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        IOSChromeContentSuggestionsServiceFactory::GetInstance(),
        IOSChromeContentSuggestionsServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    browser_state_ = test_cbs_builder.Build();
  }

  void swizzleConnectedScenes(NSArray<SceneState*>* connectedScenes) {
    connected_scenes_swizzle_block_ = ^NSArray<SceneState*>*(id self) {
      return connectedScenes;
    };
    connected_scenes_swizzler_.reset(
        new ScopedBlockSwizzler([AppState class], @selector(connectedScenes),
                                connected_scenes_swizzle_block_));
  }

  void swizzleSafeModeShouldStart(BOOL shouldStart) {
    safe_mode_swizzle_block_ = ^BOOL(id self) {
      return shouldStart;
    };
    safe_mode_swizzler_.reset(new ScopedBlockSwizzler(
        [SafeModeCoordinator class], @selector(shouldStart),
        safe_mode_swizzle_block_));
  }

  void swizzleMetricsMediatorDisableReporting() {
    metrics_mediator_called_ = NO;

    metrics_mediator_swizzle_block_ = ^{
      metrics_mediator_called_ = YES;
    };

    metrics_mediator_swizzler_.reset(new ScopedBlockSwizzler(
        [MetricsMediator class], @selector(disableReporting),
        metrics_mediator_swizzle_block_));
  }

  void swizzleHandleStartupParameters(
      id<TabOpening> expectedTabOpener,
      ChromeBrowserState* expectedBrowserState) {
    handle_startup_swizzle_block_ =
        ^(id self, id<TabOpening> tabOpener,
          id<ConnectionInformation> connectionInformation,
          id<StartupInformation> startupInformation,
          ChromeBrowserState* browserState) {
          ASSERT_EQ(connection_information_mock_, connectionInformation);
          ASSERT_EQ(startup_information_mock_, startupInformation);
          ASSERT_EQ(expectedTabOpener, tabOpener);
          ASSERT_EQ(expectedBrowserState, browserState);
        };

    handle_startup_swizzler_.reset(new ScopedBlockSwizzler(
        [UserActivityHandler class],
        @selector
        (handleStartupParametersWithTabOpener:
                        connectionInformation:startupInformation:browserState:),
        handle_startup_swizzle_block_));
  }

  AppState* getAppStateWithOpenNTP(BOOL shouldOpenNTP, UIWindow* window) {
    AppState* appState = getAppStateWithRealWindow(window);

    id application = [OCMockObject mockForClass:[UIApplication class]];
    id metricsMediator = [OCMockObject mockForClass:[MetricsMediator class]];
    id memoryHelper = [OCMockObject mockForClass:[MemoryWarningHelper class]];
    id tabOpener = [OCMockObject mockForProtocol:@protocol(TabOpening)];
    Browser* browser = interface_provider_.currentInterface.browser;

    [[metricsMediator stub] updateMetricsStateBasedOnPrefsUserTriggered:NO];
    [[memoryHelper stub] resetForegroundMemoryWarningCount];
    [[[memoryHelper stub] andReturnValue:@0] foregroundMemoryWarningCount];
    [[[tabOpener stub] andReturnValue:@(shouldOpenNTP)]
        shouldOpenNTPTabOnActivationOfBrowser:browser];

    void (^swizzleBlock)() = ^{
    };

    ScopedBlockSwizzler swizzler(
        [MetricsMediator class],
        @selector(logLaunchMetricsWithStartupInformation:connectedScenes:),
        swizzleBlock);

    [appState applicationWillEnterForeground:application
                             metricsMediator:metricsMediator
                                memoryHelper:memoryHelper];

    return appState;
  }

  AppState* getAppStateWithMock() {
    if (!app_state_) {
      // The swizzle block needs the scene state before app_state is create, but
      // the scene state needs the app state. So this alloc before swizzling
      // and initiate after app state is created.
      main_scene_state_ = [FakeSceneState alloc];
      swizzleConnectedScenes(@[ main_scene_state_ ]);

      app_state_ =
          [[AppState alloc] initWithBrowserLauncher:browser_launcher_mock_
                                 startupInformation:startup_information_mock_
                                applicationDelegate:main_application_delegate_];
      app_state_.mainSceneState = main_scene_state_;

      main_scene_state_ = [main_scene_state_ initWithAppState:app_state_];
      main_scene_state_.window = getWindowMock();
    }
    return app_state_;
  }

  AppState* getAppStateWithRealWindow(UIWindow* window) {
    if (!app_state_) {
      // The swizzle block needs the scene state before app_state is create, but
      // the scene state needs the app state. So this alloc before swizzling
      // and initiate after app state is created.
      main_scene_state_ = [FakeSceneState alloc];
      swizzleConnectedScenes(@[ main_scene_state_ ]);

      app_state_ =
          [[AppState alloc] initWithBrowserLauncher:browser_launcher_mock_
                                 startupInformation:startup_information_mock_
                                applicationDelegate:main_application_delegate_];
      app_state_.mainSceneState = main_scene_state_;

      main_scene_state_ = [main_scene_state_ initWithAppState:app_state_];
      main_scene_state_.window = window;

      [window makeKeyAndVisible];
    }
    return app_state_;
  }

  id getBrowserLauncherMock() { return browser_launcher_mock_; }
  id getStartupInformationMock() { return startup_information_mock_; }
  id getConnectionInformationMock() { return connection_information_mock_; }
  id getApplicationDelegateMock() { return main_application_delegate_; }
  id getWindowMock() { return window_; }
  StubBrowserInterfaceProvider* getInterfaceProvider() {
    return interface_provider_;
  }
  ChromeBrowserState* getBrowserState() { return browser_state_.get(); }

  BOOL metricsMediatorHasBeenCalled() { return metrics_mediator_called_; }


 private:
  web::WebTaskEnvironment task_environment_;
  AppState* app_state_;
  FakeSceneState* main_scene_state_;
  id browser_launcher_mock_;
  id connection_information_mock_;
  id startup_information_mock_;
  id main_application_delegate_;
  id window_;
  StubBrowserInterfaceProvider* interface_provider_;
  ScenesBlock connected_scenes_swizzle_block_;
  DecisionBlock safe_mode_swizzle_block_;
  HandleStartupParam handle_startup_swizzle_block_;
  ProceduralBlock metrics_mediator_swizzle_block_;
  std::unique_ptr<ScopedBlockSwizzler> safe_mode_swizzler_;
  std::unique_ptr<ScopedBlockSwizzler> connected_scenes_swizzler_;
  std::unique_ptr<ScopedBlockSwizzler> handle_startup_swizzler_;
  std::unique_ptr<ScopedBlockSwizzler> metrics_mediator_swizzler_;
  __block BOOL metrics_mediator_called_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Used to have a thread handling the closing of the IO threads.
class AppStateWithThreadTest : public PlatformTest {
 protected:
  AppStateWithThreadTest()
      : task_environment_(web::WebTaskEnvironment::REAL_IO_THREAD) {}

 private:
  web::WebTaskEnvironment task_environment_;
};

#pragma mark - Tests.

// Tests that if the application is in background
// -requiresHandlingAfterLaunchWithOptions saves the launchOptions and returns
// YES (to handle the launch options later).
TEST_F(AppStateTest, requiresHandlingAfterLaunchWithOptionsBackground) {
  // Setup.
  NSString* sourceApplication = @"com.apple.mobilesafari";
  NSDictionary* launchOptions =
      @{UIApplicationLaunchOptionsSourceApplicationKey : sourceApplication};

  AppState* appState = getAppStateWithMock();

  id browserLauncherMock = getBrowserLauncherMock();
  BrowserInitializationStageType stageBasic = INITIALIZATION_STAGE_BASIC;
  [[browserLauncherMock expect] startUpBrowserToStage:stageBasic];
  [[browserLauncherMock expect] setLaunchOptions:launchOptions];

  // Action.
  BOOL result = [appState requiresHandlingAfterLaunchWithOptions:launchOptions
                                                 stateBackground:YES];

  // Test.
  EXPECT_TRUE(result);
  EXPECT_OCMOCK_VERIFY(browserLauncherMock);
}

// Tests that if the application is active and Safe Mode should be activated
// -requiresHandlingAfterLaunchWithOptions save the launch options and activate
// the Safe Mode.
TEST_F(AppStateTest, requiresHandlingAfterLaunchWithOptionsForegroundSafeMode) {
  // Setup.
  NSString* sourceApplication = @"com.apple.mobilesafari";
  NSDictionary* launchOptions =
      @{UIApplicationLaunchOptionsSourceApplicationKey : sourceApplication};

  base::TimeTicks now = base::TimeTicks::Now();
  [[[getStartupInformationMock() stub] andReturnValue:@YES] isColdStart];
  [[[getStartupInformationMock() stub] andDo:^(NSInvocation* invocation) {
    [invocation setReturnValue:(void*)&now];
  }] appLaunchTime];

  id windowMock = getWindowMock();
  [[[windowMock stub] andReturn:nil] rootViewController];
  [[windowMock expect] setRootViewController:[OCMArg any]];
  [[windowMock expect] makeKeyAndVisible];

  AppState* appState = getAppStateWithMock();

  id browserLauncherMock = getBrowserLauncherMock();
  BrowserInitializationStageType stageBasic = INITIALIZATION_STAGE_BASIC;
  [[browserLauncherMock expect] startUpBrowserToStage:stageBasic];
  [[browserLauncherMock expect] setLaunchOptions:launchOptions];

  swizzleSafeModeShouldStart(YES);

  ASSERT_FALSE([appState isInSafeMode]);

  appState.mainSceneState.activationLevel =
      SceneActivationLevelForegroundActive;

  // Action.
  BOOL result = [appState requiresHandlingAfterLaunchWithOptions:launchOptions
                                                 stateBackground:NO];

  if (base::ios::IsMultiwindowSupported()) {
    [appState startSafeMode];
  }

  // Test.
  EXPECT_TRUE(result);
  EXPECT_TRUE([appState isInSafeMode]);
  EXPECT_OCMOCK_VERIFY(browserLauncherMock);
  EXPECT_OCMOCK_VERIFY(windowMock);

  if (base::ios::IsMultiwindowSupported()) {
    [appState stopSafeMode];
  }
}

// Tests that if the application is active
// -requiresHandlingAfterLaunchWithOptions saves the launchOptions and start the
// application in foreground.
TEST_F(AppStateTest, requiresHandlingAfterLaunchWithOptionsForeground) {
  // Setup.
  NSString* sourceApplication = @"com.apple.mobilesafari";
  NSDictionary* launchOptions =
      @{UIApplicationLaunchOptionsSourceApplicationKey : sourceApplication};

  [[[getStartupInformationMock() stub] andReturnValue:@YES] isColdStart];

  [[[getWindowMock() stub] andReturn:nil] rootViewController];

  AppState* appState = getAppStateWithMock();

  id browserLauncherMock = getBrowserLauncherMock();
  BrowserInitializationStageType stageBasic = INITIALIZATION_STAGE_BASIC;
  [[browserLauncherMock expect] startUpBrowserToStage:stageBasic];
  BrowserInitializationStageType stageForeground =
      INITIALIZATION_STAGE_FOREGROUND;
  [[browserLauncherMock expect] startUpBrowserToStage:stageForeground];
  [[browserLauncherMock expect] setLaunchOptions:launchOptions];

  swizzleSafeModeShouldStart(NO);

  // Action.
  BOOL result = [appState requiresHandlingAfterLaunchWithOptions:launchOptions
                                                 stateBackground:NO];

  // Test.
  EXPECT_TRUE(result);
  EXPECT_OCMOCK_VERIFY(browserLauncherMock);
}

using AppStateNoFixtureTest = PlatformTest;

// Test that -willResignActive set cold start to NO and launch record.
TEST_F(AppStateNoFixtureTest, willResignActive) {
  // Setup.
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<Browser> browser = std::make_unique<TestBrowser>();

  StubBrowserInterfaceProvider* interfaceProvider =
      [[StubBrowserInterfaceProvider alloc] init];
  interfaceProvider.mainInterface.browser = browser.get();

  id browserLauncher =
      [OCMockObject mockForProtocol:@protocol(BrowserLauncher)];
  [[[browserLauncher stub] andReturnValue:@(INITIALIZATION_STAGE_FOREGROUND)]
      browserInitializationStage];
  [[[browserLauncher stub] andReturn:interfaceProvider] interfaceProvider];

  id applicationDelegate =
      [OCMockObject mockForClass:[MainApplicationDelegate class]];

  FakeStartupInformation* startupInformation =
      [[FakeStartupInformation alloc] init];
  [startupInformation setIsColdStart:YES];

  AppState* appState =
      [[AppState alloc] initWithBrowserLauncher:browserLauncher
                             startupInformation:startupInformation
                            applicationDelegate:applicationDelegate];

  ASSERT_TRUE([startupInformation isColdStart]);

  // Action.
  [appState willResignActiveTabModel];

  // Test.
  EXPECT_FALSE([startupInformation isColdStart]);
}

// Test that -applicationWillTerminate clears everything.
TEST_F(AppStateWithThreadTest, willTerminate) {
  // Setup.
  IOSChromeScopedTestingChromeBrowserProvider provider_(
      std::make_unique<FakeChromeBrowserProvider>());

  id browserLauncher =
      [OCMockObject mockForProtocol:@protocol(BrowserLauncher)];
  id applicationDelegate =
      [OCMockObject mockForClass:[MainApplicationDelegate class]];
  StubBrowserInterfaceProvider* interfaceProvider =
      [[StubBrowserInterfaceProvider alloc] init];
  interfaceProvider.mainInterface.userInteractionEnabled = YES;

  [[[browserLauncher stub] andReturnValue:@(INITIALIZATION_STAGE_FOREGROUND)]
      browserInitializationStage];
  [[[browserLauncher stub] andReturn:interfaceProvider] interfaceProvider];

  id startupInformation =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[startupInformation expect] stopChromeMain];

  AppState* appState =
      [[AppState alloc] initWithBrowserLauncher:browserLauncher
                             startupInformation:startupInformation
                            applicationDelegate:applicationDelegate];

  // Create a scene state so that full shutdown will run.
  if (!base::ios::IsSceneStartupSupported()) {
    appState.mainSceneState = [[SceneState alloc] initWithAppState:appState];
  }

  id application = [OCMockObject mockForClass:[UIApplication class]];

  // Action.
  [appState applicationWillTerminate:application];

  // Test.
  EXPECT_OCMOCK_VERIFY(startupInformation);
  EXPECT_OCMOCK_VERIFY(application);
  EXPECT_FALSE(interfaceProvider.mainInterface.userInteractionEnabled);
  FakeAppDistributionProvider* provider =
      static_cast<FakeAppDistributionProvider*>(
          ios::GetChromeBrowserProvider()->GetAppDistributionProvider());
  EXPECT_TRUE(provider->cancel_called());
}

// Test that -resumeSessionWithTabOpener
// restart metrics and launchs from StartupParameters if they exist.
TEST_F(AppStateTest, resumeSessionWithStartupParameters) {
  if (base::ios::IsSceneStartupSupported()) {
    // TODO(crbug.com/1045579): Session restoration not available yet in MW.
    return;
  }
  // Setup.

  // BrowserLauncher.
  StubBrowserInterfaceProvider* interfaceProvider = getInterfaceProvider();
  [[[getBrowserLauncherMock() stub]
      andReturnValue:@(INITIALIZATION_STAGE_FOREGROUND)]
      browserInitializationStage];
  [[[getBrowserLauncherMock() stub] andReturn:interfaceProvider]
      interfaceProvider];

  // StartupInformation.
  id appStartupParameters =
      [OCMockObject mockForClass:[AppStartupParameters class]];
  [[[getConnectionInformationMock() stub] andReturn:appStartupParameters]
      startupParameters];
  [[[getStartupInformationMock() stub] andReturnValue:@NO] isColdStart];

  // TabOpening.
  id tabOpener = [OCMockObject mockForProtocol:@protocol(TabOpening)];
  // TabSwitcher.
  id tabSwitcher = [OCMockObject mockForProtocol:@protocol(TabSwitching)];

  // BrowserViewInformation.
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(getBrowserState());
  interfaceProvider.mainInterface.browser = browser.get();
  interfaceProvider.mainInterface.browserState = getBrowserState();

  // Swizzle Startup Parameters.
  swizzleHandleStartupParameters(tabOpener, getBrowserState());

  ScopedKeyWindow scopedKeyWindow;
  AppState* appState = getAppStateWithOpenNTP(NO, scopedKeyWindow.Get());

  // Action.
  [appState resumeSessionWithTabOpener:tabOpener
                           tabSwitcher:tabSwitcher
                 connectionInformation:getConnectionInformationMock()];
}

// Test that -resumeSessionWithTabOpener
// restart metrics and creates a new tab from tab switcher if shouldOpenNTP is
// YES.
TEST_F(AppStateTest, resumeSessionShouldOpenNTPTabSwitcher) {
  if (base::ios::IsSceneStartupSupported()) {
    // TODO(crbug.com/1045579): Session restoration not available yet in MW.
    return;
  }

  // Setup.
  // BrowserLauncher.
  StubBrowserInterfaceProvider* interfaceProvider = getInterfaceProvider();
  [[[getBrowserLauncherMock() stub]
      andReturnValue:@(INITIALIZATION_STAGE_FOREGROUND)]
      browserInitializationStage];
  [[[getBrowserLauncherMock() stub] andReturn:interfaceProvider]
      interfaceProvider];

  // StartupInformation.
  [[[getConnectionInformationMock() stub] andReturn:nil] startupParameters];
  [[[getStartupInformationMock() stub] andReturnValue:@NO] isColdStart];

  // BrowserViewInformation.
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(getBrowserState());
  interfaceProvider.mainInterface.browser = browser.get();
  interfaceProvider.mainInterface.browserState = getBrowserState();

  // TabOpening.
  id tabOpener = [OCMockObject mockForProtocol:@protocol(TabOpening)];
  [[[tabOpener stub] andReturnValue:@YES]
      shouldOpenNTPTabOnActivationOfBrowser:browser.get()];

  // TabSwitcher.
  id tabSwitcher = [OCMockObject mockForProtocol:@protocol(TabSwitching)];
  [[[tabSwitcher stub] andReturnValue:@YES] openNewTabFromTabSwitcher];

  ScopedKeyWindow scopedKeyWindow;
  AppState* appState = getAppStateWithOpenNTP(YES, scopedKeyWindow.Get());

  // Action.
  [appState resumeSessionWithTabOpener:tabOpener
                           tabSwitcher:tabSwitcher
                 connectionInformation:getConnectionInformationMock()];

  // Test.
  EXPECT_EQ(NSUInteger(0), [scopedKeyWindow.Get() subviews].count);
}

// Test that -resumeSessionWithTabOpener,
// restart metrics and creates a new tab if shouldOpenNTP is YES.
TEST_F(AppStateTest, resumeSessionShouldOpenNTPNoTabSwitcher) {
  if (base::ios::IsSceneStartupSupported()) {
    // TODO(crbug.com/1045579): Session restoration not available yet in MW.
    return;
  }
  // Setup.
  // BrowserLauncher.
  StubBrowserInterfaceProvider* interfaceProvider = getInterfaceProvider();
  [[[getBrowserLauncherMock() stub]
      andReturnValue:@(INITIALIZATION_STAGE_FOREGROUND)]
      browserInitializationStage];
  [[[getBrowserLauncherMock() stub] andReturn:interfaceProvider]
      interfaceProvider];

  // StartupInformation.
  [[[getConnectionInformationMock() stub] andReturn:nil] startupParameters];
  [[[getStartupInformationMock() stub] andReturnValue:@NO] isColdStart];

  // BrowserViewInformation.
  id applicationCommandEndpoint =
      [OCMockObject mockForProtocol:@protocol(ApplicationCommands)];
  [((id<ApplicationCommands>)[applicationCommandEndpoint expect])
      openURLInNewTab:[OCMArg any]];

  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(getBrowserState());
  [browser->GetCommandDispatcher()
      startDispatchingToTarget:applicationCommandEndpoint
                   forProtocol:@protocol(ApplicationCommands)];
  // To fully conform to ApplicationCommands, the dispatcher needs to dispatch
  // for ApplicationSettingsCommands as well.
  id applicationSettingsCommandEndpoint =
      [OCMockObject mockForProtocol:@protocol(ApplicationSettingsCommands)];
  [browser->GetCommandDispatcher()
      startDispatchingToTarget:applicationSettingsCommandEndpoint
                   forProtocol:@protocol(ApplicationSettingsCommands)];
  interfaceProvider.mainInterface.browser = browser.get();
  interfaceProvider.mainInterface.browserState = getBrowserState();

  // TabOpening.
  id tabOpener = [OCMockObject mockForProtocol:@protocol(TabOpening)];
  [[[tabOpener stub] andReturnValue:@YES]
      shouldOpenNTPTabOnActivationOfBrowser:browser.get()];

  // TabSwitcher.
  id tabSwitcher = [OCMockObject mockForProtocol:@protocol(TabSwitching)];
  [[[tabSwitcher stub] andReturnValue:@NO] openNewTabFromTabSwitcher];

  ScopedKeyWindow scopedKeyWindow;
  AppState* appState = getAppStateWithOpenNTP(YES, scopedKeyWindow.Get());

  // Action.
  [appState resumeSessionWithTabOpener:tabOpener
                           tabSwitcher:tabSwitcher
                 connectionInformation:getConnectionInformationMock()];

  // Test.
  EXPECT_EQ(NSUInteger(0), [scopedKeyWindow.Get() subviews].count);
}

// Tests that -applicationWillEnterForeground resets components as needed.
TEST_F(AppStateTest, applicationWillEnterForeground) {
  // Setup.
  IOSChromeScopedTestingChromeBrowserProvider provider_(
      std::make_unique<FakeChromeBrowserProvider>());
  id application = [OCMockObject mockForClass:[UIApplication class]];
  id metricsMediator = [OCMockObject mockForClass:[MetricsMediator class]];
  id memoryHelper = [OCMockObject mockForClass:[MemoryWarningHelper class]];
  StubBrowserInterfaceProvider* interfaceProvider = getInterfaceProvider();
  id tabOpener = [OCMockObject mockForProtocol:@protocol(TabOpening)];
  std::unique_ptr<Browser> browser = std::make_unique<TestBrowser>();

  BrowserInitializationStageType stage = INITIALIZATION_STAGE_FOREGROUND;
  [[[getBrowserLauncherMock() stub] andReturnValue:@(stage)]
      browserInitializationStage];
  [[[getBrowserLauncherMock() stub] andReturn:interfaceProvider]
      interfaceProvider];
  interfaceProvider.mainInterface.browserState = getBrowserState();

  [[metricsMediator expect] updateMetricsStateBasedOnPrefsUserTriggered:NO];
  [[memoryHelper expect] resetForegroundMemoryWarningCount];
  [[[memoryHelper stub] andReturnValue:@0] foregroundMemoryWarningCount];
  [[[tabOpener stub] andReturnValue:@YES]
      shouldOpenNTPTabOnActivationOfBrowser:browser.get()];

  // Simulate background before going to foreground.
  [[getStartupInformationMock() expect] expireFirstUserActionRecorder];
  swizzleMetricsMediatorDisableReporting();
  [getAppStateWithMock() applicationDidEnterBackground:application
                                          memoryHelper:memoryHelper];

  void (^swizzleBlock)() = ^{
  };

  ScopedBlockSwizzler swizzler(
      [MetricsMediator class],
      @selector(logLaunchMetricsWithStartupInformation:connectedScenes:),
      swizzleBlock);

  // Actions.
  [getAppStateWithMock() applicationWillEnterForeground:application
                                        metricsMediator:metricsMediator
                                           memoryHelper:memoryHelper];

  // Tests.
  EXPECT_OCMOCK_VERIFY(metricsMediator);
  EXPECT_OCMOCK_VERIFY(memoryHelper);
  EXPECT_OCMOCK_VERIFY(getStartupInformationMock());
  FakeUserFeedbackProvider* user_feedback_provider =
      static_cast<FakeUserFeedbackProvider*>(
          ios::GetChromeBrowserProvider()->GetUserFeedbackProvider());
  EXPECT_TRUE(user_feedback_provider->synchronize_called());
}

// Tests that -applicationWillEnterForeground starts the browser if the
// application is in background.
TEST_F(AppStateTest, applicationWillEnterForegroundFromBackground) {
  // Setup.
  id application = [OCMockObject mockForClass:[UIApplication class]];
  id metricsMediator = [OCMockObject mockForClass:[MetricsMediator class]];
  id memoryHelper = [OCMockObject mockForClass:[MemoryWarningHelper class]];

  BrowserInitializationStageType stage = INITIALIZATION_STAGE_BACKGROUND;
  [[[getBrowserLauncherMock() stub] andReturnValue:@(stage)]
      browserInitializationStage];

  [[[getWindowMock() stub] andReturn:nil] rootViewController];
  swizzleSafeModeShouldStart(NO);

  [[[getStartupInformationMock() stub] andReturnValue:@YES] isColdStart];
  [[getBrowserLauncherMock() expect]
      startUpBrowserToStage:INITIALIZATION_STAGE_FOREGROUND];

  // Actions.
  [getAppStateWithMock() applicationWillEnterForeground:application
                                        metricsMediator:metricsMediator
                                           memoryHelper:memoryHelper];

  // Tests.
  EXPECT_OCMOCK_VERIFY(getBrowserLauncherMock());
}

// Tests that -applicationWillEnterForeground starts the safe mode if the
// application is in background.
TEST_F(AppStateTest,
       applicationWillEnterForegroundFromBackgroundShouldStartSafeMode) {
  if (base::ios::IsMultiwindowSupported()) {
    // In Multi Window, this is not the case. Skip this test.
    return;
  }
  // Setup.
  id application = [OCMockObject mockForClass:[UIApplication class]];
  id metricsMediator = [OCMockObject mockForClass:[MetricsMediator class]];
  id memoryHelper = [OCMockObject mockForClass:[MemoryWarningHelper class]];

  base::TimeTicks now = base::TimeTicks::Now();
  [[[getStartupInformationMock() stub] andReturnValue:@YES] isColdStart];
  [[[getStartupInformationMock() stub] andDo:^(NSInvocation* invocation) {
    [invocation setReturnValue:(void*)&now];
  }] appLaunchTime];

  id window = getWindowMock();

  BrowserInitializationStageType stage = INITIALIZATION_STAGE_BACKGROUND;
  [[[getBrowserLauncherMock() stub] andReturnValue:@(stage)]
      browserInitializationStage];

  [[[window stub] andReturn:nil] rootViewController];
  [[window stub] setRootViewController:[OCMArg any]];
  swizzleSafeModeShouldStart(YES);

  // The helper below calls makeKeyAndVisible.
  [[window expect] makeKeyAndVisible];
  AppState* appState = getAppStateWithRealWindow(window);

  // Starting safe mode will call makeKeyAndVisible on the window.
  [[window expect] makeKeyAndVisible];
  appState.mainSceneState.activationLevel =
      SceneActivationLevelForegroundActive;
  appState.mainSceneState.window = window;

  // Actions.
  [getAppStateWithMock() applicationWillEnterForeground:application
                                        metricsMediator:metricsMediator
                                           memoryHelper:memoryHelper];

  // Tests.
  EXPECT_OCMOCK_VERIFY(window);
  EXPECT_TRUE([getAppStateWithMock() isInSafeMode]);
}

// Tests that -applicationDidEnterBackground calls the metrics mediator.
TEST_F(AppStateTest, applicationDidEnterBackgroundIncognito) {
  // Setup.
  ScopedKeyWindow scopedKeyWindow;
  id application = [OCMockObject niceMockForClass:[UIApplication class]];
  id memoryHelper = [OCMockObject mockForClass:[MemoryWarningHelper class]];
  StubBrowserInterfaceProvider* interfaceProvider = getInterfaceProvider();

  std::unique_ptr<Browser> browser = std::make_unique<TestBrowser>();
  id startupInformation = getStartupInformationMock();
  id browserLauncher = getBrowserLauncherMock();
  BrowserInitializationStageType stage = INITIALIZATION_STAGE_FOREGROUND;

  AppState* appState = getAppStateWithRealWindow(scopedKeyWindow.Get());

  [[startupInformation expect] expireFirstUserActionRecorder];
  [[[memoryHelper stub] andReturnValue:@0] foregroundMemoryWarningCount];
  interfaceProvider.incognitoInterface.browser = browser.get();
  [[[browserLauncher stub] andReturnValue:@(stage)] browserInitializationStage];
  [[[browserLauncher stub] andReturn:interfaceProvider] interfaceProvider];

  swizzleMetricsMediatorDisableReporting();

  // Action.
  [appState applicationDidEnterBackground:application
                             memoryHelper:memoryHelper];

  // Tests.
  EXPECT_OCMOCK_VERIFY(startupInformation);
  EXPECT_TRUE(metricsMediatorHasBeenCalled());
}

// Tests that -applicationDidEnterBackground do nothing if the application has
// never been in a Foreground stage.
TEST_F(AppStateTest, applicationDidEnterBackgroundStageBackground) {
  // Setup.
  ScopedKeyWindow scopedKeyWindow;
  id application = [OCMockObject mockForClass:[UIApplication class]];
  id memoryHelper = [OCMockObject mockForClass:[MemoryWarningHelper class]];
  id browserLauncher = getBrowserLauncherMock();
  BrowserInitializationStageType stage = INITIALIZATION_STAGE_BACKGROUND;

  [[[browserLauncher stub] andReturnValue:@(stage)] browserInitializationStage];
  [[[browserLauncher stub] andReturn:nil] interfaceProvider];

  ASSERT_EQ(NSUInteger(0), [scopedKeyWindow.Get() subviews].count);

  // Action.
  [getAppStateWithRealWindow(scopedKeyWindow.Get())
      applicationDidEnterBackground:application
                       memoryHelper:memoryHelper];

  // Tests.
  EXPECT_EQ(NSUInteger(0), [scopedKeyWindow.Get() subviews].count);
}
