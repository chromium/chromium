// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/app_state.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/application_delegate/app_init_stage_test_utils.h"
#import "ios/chrome/app/application_delegate/app_state+Testing.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/app/application_delegate/memory_warning_helper.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/enterprise_app_agent.h"
#import "ios/chrome/app/safe_mode_app_state_agent+private.h"
#import "ios/chrome/app/safe_mode_app_state_agent.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager.h"
#import "ios/chrome/browser/safe_mode/ui_bundled/safe_mode_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/ui/device_orientation/scoped_force_portrait_orientation.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/providers/app_distribution/test_app_distribution.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/public/provider/chrome/browser/app_distribution/app_distribution_api.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"

// Subclass of AppState that allow returning a fake list of connected scenes.
@interface TestAppState : AppState

- (instancetype)
    initWithStartupInformation:(id<StartupInformation>)startupInformation
               connectedScenes:(NSArray<SceneState*>*)connectedScenes
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStartupInformation:
    (id<StartupInformation>)startupInformation NS_UNAVAILABLE;

@end

@implementation TestAppState {
  NSArray<SceneState*>* _connectedScenes;
}

- (instancetype)
    initWithStartupInformation:(id<StartupInformation>)startupInformation
               connectedScenes:(NSArray<SceneState*>*)connectedScenes {
  if ((self = [super initWithStartupInformation:startupInformation])) {
    _connectedScenes = connectedScenes ? [connectedScenes copy] : @[];
  }
  return self;
}

- (NSArray<SceneState*>*)connectedScenes {
  return _connectedScenes;
}

@end

// App state observer that is used to replace the main controller to transition
// through stages.
@interface AppStateObserverToMockMainController : NSObject <AppStateObserver>
@end
@implementation AppStateObserverToMockMainController
- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  switch (appState.initStage) {
    case AppInitStage::kStart:
      [appState queueTransitionToNextInitStage];
      break;
    case AppInitStage::kBrowserBasic:
      break;
    case AppInitStage::kSafeMode:
      break;
    case AppInitStage::kVariationsSeed:
      [appState queueTransitionToNextInitStage];
      break;
    case AppInitStage::kBrowserObjectsForBackgroundHandlers:
      [appState queueTransitionToNextInitStage];
      break;
    case AppInitStage::kEnterprise:
      break;
    case AppInitStage::kBrowserObjectsForUI:
      [appState queueTransitionToNextInitStage];
      break;
    case AppInitStage::kNormalUI:
      [appState queueTransitionToNextInitStage];
      break;
    case AppInitStage::kFirstRun:
      [appState queueTransitionToNextInitStage];
      break;
    case AppInitStage::kChoiceScreen:
      [appState queueTransitionToNextInitStage];
      break;
    case AppInitStage::kFinal:
      break;
  }
}
@end

// Trivial app agent used to test -connectedAgents and agent retrieval.
@interface TestAppAgent : ObservingAppAgent
@end
@implementation TestAppAgent
@end

#pragma mark - Class definition.

namespace {

// A block that takes self as argument and return a BOOL.
typedef BOOL (^DecisionBlock)(id self);
// A block ths returns values of AppState connectedScenes.
typedef NSArray<SceneState*>* (^ScenesBlock)(id self);

}  // namespace

// An app state observer that will call [AppState
// queueTransitionToNextInitStage] once (when a flag is set) from one of
// willTransitionToInitStage: and didTransitionFromInitStage: Defaults to
// willTransitioin.
@interface AppStateTransitioningObserver : NSObject <AppStateObserver>
// When set, will call queueTransitionToNextInitStage on
// didTransitionFromInitStage; otherwise, on willTransitionToInitStage
@property(nonatomic, assign) BOOL triggerOnDidTransition;
// Will do nothing when this is not set.
// Will call queueTransitionToNextInitStage on correct callback and reset this
// flag when it's set. The flag is init to YES when the object is created.
@property(nonatomic, assign) BOOL needsQueueTransition;
@end

@implementation AppStateTransitioningObserver

- (instancetype)init {
  self = [super init];
  if (self) {
    _needsQueueTransition = YES;
  }
  return self;
}

- (void)appState:(AppState*)appState
    willTransitionToInitStage:(AppInitStage)nextInitStage {
  if (self.needsQueueTransition && !self.triggerOnDidTransition) {
    [appState queueTransitionToNextInitStage];
    self.needsQueueTransition = NO;
  }
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  if (self.needsQueueTransition && self.triggerOnDidTransition) {
    [appState queueTransitionToNextInitStage];
    self.needsQueueTransition = NO;
  }
}
@end

class AppStateTest : public BlockCleanupTest {
 protected:
  AppStateTest() {
    // Init mocks.
    startup_information_mock_ =
        [OCMockObject mockForProtocol:@protocol(StartupInformation)];
    connection_information_mock_ =
        [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];
    window_ = [OCMockObject mockForClass:[UIWindow class]];
    app_state_observer_mock_ =
        [OCMockObject mockForProtocol:@protocol(AppStateObserver)];

    provider_interface_ = [[StubBrowserProviderInterface alloc] init];

    app_state_observer_to_mock_main_controller_ =
        [AppStateObserverToMockMainController alloc];
  }

  void SetUp() override {
    BlockCleanupTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
  }

  void TearDown() override {
    main_scene_state_ = nil;
    BlockCleanupTest::TearDown();
  }

  void SwizzleConnectedScenes(NSArray<SceneState*>* connectedScenes) {
    connected_scenes_swizzle_block_ = ^NSArray<SceneState*>*(id self) {
      return connectedScenes;
    };
    connected_scenes_swizzler_.reset(
        new ScopedBlockSwizzler([AppState class], @selector(connectedScenes),
                                connected_scenes_swizzle_block_));
  }

  void SwizzleSafeModeShouldStart(BOOL shouldStart) {
    safe_mode_swizzle_block_ = ^BOOL(id self) {
      return shouldStart;
    };
    safe_mode_swizzler_.reset(new ScopedBlockSwizzler(
        [SafeModeCoordinator class], @selector(shouldStart),
        safe_mode_swizzle_block_));
  }

  SafeModeAppAgent* GetSafeModeAppAgent() {
    if (!safe_mode_app_agent_) {
      safe_mode_app_agent_ = [[SafeModeAppAgent alloc] init];
    }
    return safe_mode_app_agent_;
  }

  EnterpriseAppAgent* GetEnterpriseAppAgent() {
    if (!enterprise_app_agent_) {
      enterprise_app_agent_ = [[EnterpriseAppAgent alloc] init];
    }
    return enterprise_app_agent_;
  }

  AppState* GetAppStateWithMock(bool with_safe_mode_agent) {
    if (!app_state_) {
      // The swizzle block needs the scene state before app_state is create, but
      // the scene state needs the app state. So this alloc before swizzling
      // and initiate after app state is created.
      main_scene_state_ = [FakeSceneState alloc];
      SwizzleConnectedScenes(@[ main_scene_state_ ]);

      app_state_ = [[TestAppState alloc]
          initWithStartupInformation:startup_information_mock_
                     connectedScenes:@[ main_scene_state_ ]];

      main_scene_state_ = [main_scene_state_ initWithAppState:app_state_
                                                      profile:GetProfile()];
      main_scene_state_.window = GetWindowMock();

      if (with_safe_mode_agent) {
        [app_state_ addAgent:GetSafeModeAppAgent()];
        // Retrigger a sceneConnected event for the safe mode agent. This is
        // needed because the sceneConnected event triggered by the app state is
        // done before resetting the scene state with initWithAppState which
        // clears the observers and agents.
        [GetSafeModeAppAgent() appState:app_state_
                         sceneConnected:main_scene_state_];
      }

      // Add the enterprise agent for the app to boot past the enterprise init
      // stage.
      [app_state_ addAgent:GetEnterpriseAppAgent()];

      [app_state_ addObserver:app_state_observer_to_mock_main_controller_];
    }
    return app_state_;
  }

  AppState* GetAppStateWithMock() {
    return GetAppStateWithMock(/*with_safe_mode_agent=*/true);
  }

  AppState* GetAppStateWithRealWindow(UIWindow* window) {
    if (!app_state_) {
      // The swizzle block needs the scene state before app_state is create, but
      // the scene state needs the app state. So this alloc before swizzling
      // and initiate after app state is created.
      main_scene_state_ = [FakeSceneState alloc];
      SwizzleConnectedScenes(@[ main_scene_state_ ]);

      app_state_ = [[TestAppState alloc]
          initWithStartupInformation:startup_information_mock_
                     connectedScenes:@[ main_scene_state_ ]];

      main_scene_state_ = [main_scene_state_ initWithAppState:app_state_
                                                      profile:GetProfile()];
      main_scene_state_.window = window;
      [window makeKeyAndVisible];

      [app_state_ addAgent:GetSafeModeAppAgent()];

      // Add the enterprise agent for the app to boot past the enterprise init
      // stage.
      [app_state_ addAgent:GetEnterpriseAppAgent()];

      [app_state_ addObserver:app_state_observer_to_mock_main_controller_];

      // Retrigger a sceneConnected event for the safe mode agent with the real
      // scene state. This is needed because the sceneConnected event triggered
      // by the app state is done before resetting the scene state with
      // initWithAppState which clears the observers and agents.
      [GetSafeModeAppAgent() appState:app_state_
                       sceneConnected:main_scene_state_];
    }
    return app_state_;
  }

  id GetStartupInformationMock() { return startup_information_mock_; }
  id GetConnectionInformationMock() { return connection_information_mock_; }
  id GetWindowMock() { return window_; }
  id GetAppStateObserverMock() { return app_state_observer_mock_; }
  ProfileIOS* GetProfile() { return profile_.get(); }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  TestAppState* app_state_;
  FakeSceneState* main_scene_state_;
  SafeModeAppAgent* safe_mode_app_agent_;
  EnterpriseAppAgent* enterprise_app_agent_;
  AppStateObserverToMockMainController*
      app_state_observer_to_mock_main_controller_;
  id connection_information_mock_;
  id startup_information_mock_;
  id window_;
  id app_state_observer_mock_;
  StubBrowserProviderInterface* provider_interface_;
  ScenesBlock connected_scenes_swizzle_block_;
  DecisionBlock safe_mode_swizzle_block_;
  std::unique_ptr<ScopedBlockSwizzler> safe_mode_swizzler_;
  std::unique_ptr<ScopedBlockSwizzler> connected_scenes_swizzler_;
  std::unique_ptr<ScopedBlockSwizzler> handle_startup_swizzler_;
  raw_ptr<ProfileIOS> profile_;
};

#pragma mark - Tests.

using AppStateNoFixtureTest = PlatformTest;

// Test that -willResignActive set cold start to NO and launch record.
TEST_F(AppStateNoFixtureTest, WillResignActive) {
  // Setup.
  base::test::TaskEnvironment task_environment;
  FakeStartupInformation* startupInformation =
      [[FakeStartupInformation alloc] init];
  [startupInformation setIsColdStart:YES];

  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestProfileManagerIOS profile_manager;
  profile_manager.AddProfileWithBuilder(TestProfileIOS::Builder());

  AppState* appState =
      [[AppState alloc] initWithStartupInformation:startupInformation];

  [appState addAgent:[[SafeModeAppAgent alloc] init]];
  AppStateObserverToMockMainController* observer =
      [AppStateObserverToMockMainController alloc];
  [appState addObserver:observer];

  // Start init stages.
  [appState startInitialization];
  [appState queueTransitionToNextInitStage];
  [appState queueTransitionToNextInitStage];

  ASSERT_TRUE([startupInformation isColdStart]);

  // Action.
  [appState willResignActive];

  // Test.
  EXPECT_FALSE([startupInformation isColdStart]);
}

// Test that -applicationWillTerminate clears everything.
TEST_F(AppStateTest, WillTerminate) {
  // Setup.
  ios::provider::test::ResetAppDistributionNotificationsState();
  ASSERT_FALSE(ios::provider::test::AreAppDistributionNotificationsCanceled());

  [[GetStartupInformationMock() stub] setIsFirstRun:YES];
  [[[GetStartupInformationMock() stub] andReturnValue:@YES] isFirstRun];

  [[GetStartupInformationMock() expect] stopChromeMain];

  AppState* appState = GetAppStateWithMock();

  id appStateMock = OCMPartialMock(GetAppStateWithMock());
  [[appStateMock expect] completeUIInitialization];

  // Start init stages.
  [appState startInitialization];
  [appState queueTransitionToNextInitStage];

  // Initialize the WebUsageEnablerBrowserAgent for all scenes.
  for (SceneState* connectedScene in appState.connectedScenes) {
    Browser* test_browser =
        connectedScene.browserProviderInterface.currentBrowserProvider.browser;
    WebUsageEnablerBrowserAgent::CreateForBrowser(test_browser);
  }

  id application = [OCMockObject mockForClass:[UIApplication class]];

  // Action.
  [appState applicationWillTerminate:application];

  // Test.
  EXPECT_OCMOCK_VERIFY(GetStartupInformationMock());
  EXPECT_OCMOCK_VERIFY(application);

  for (SceneState* connectedScene in appState.connectedScenes) {
    Browser* browser =
        connectedScene.browserProviderInterface.currentBrowserProvider.browser;
    EXPECT_FALSE(
        WebUsageEnablerBrowserAgent::FromBrowser(browser)->IsWebUsageEnabled());
  }

  EXPECT_TRUE(ios::provider::test::AreAppDistributionNotificationsCanceled());
}

// Tests that -applicationWillEnterForeground resets components as needed.
TEST_F(AppStateTest, ApplicationWillEnterForeground) {
  SwizzleSafeModeShouldStart(NO);
  [[GetStartupInformationMock() stub] setIsFirstRun:YES];
  [[[GetStartupInformationMock() stub] andReturnValue:@YES] isFirstRun];

  // Setup.
  id application = [OCMockObject mockForClass:[UIApplication class]];
  id metricsMediator = [OCMockObject mockForClass:[MetricsMediator class]];
  id memoryHelper = [OCMockObject mockForClass:[MemoryWarningHelper class]];
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(GetProfile());

  [[metricsMediator expect] updateMetricsStateBasedOnPrefsUserTriggered:NO];
  [[metricsMediator expect]
      notifyCredentialProviderWasUsed:static_cast<feature_engagement::Tracker*>(
                                          [OCMArg anyPointer])];
  [[memoryHelper expect] resetForegroundMemoryWarningCount];
  [[[memoryHelper stub] andReturnValue:@0] foregroundMemoryWarningCount];

  id appStateMock = OCMPartialMock(GetAppStateWithMock());
  [[appStateMock expect] completeUIInitialization];

  // Simulate finishing the initialization before going to background.
  [GetAppStateWithMock() startInitialization];
  [GetAppStateWithMock() queueTransitionToNextInitStage];

  // Simulate background before going to foreground.
  [[GetStartupInformationMock() expect] expireFirstUserActionRecorder];
  [GetAppStateWithMock() applicationDidEnterBackground:application
                                          memoryHelper:memoryHelper];

  void (^swizzleBlock)() = ^{
  };

  ScopedBlockSwizzler swizzler(
      [MetricsMediator class],
      @selector(logLaunchMetricsWithStartupInformation:connectedScenes:),
      swizzleBlock);

  // Actions.
  [GetAppStateWithMock() applicationWillEnterForeground:application
                                        metricsMediator:metricsMediator
                                           memoryHelper:memoryHelper];

  // Tests.
  EXPECT_OCMOCK_VERIFY(metricsMediator);
  EXPECT_OCMOCK_VERIFY(memoryHelper);
  EXPECT_OCMOCK_VERIFY(GetStartupInformationMock());
}

// Tests that -applicationWillEnterForeground starts the browser if the
// application is in background.
TEST_F(AppStateTest, ApplicationWillEnterForegroundFromBackground) {
  // Setup.
  id application = [OCMockObject mockForClass:[UIApplication class]];
  id metricsMediator = [OCMockObject mockForClass:[MetricsMediator class]];
  id memoryHelper = [OCMockObject mockForClass:[MemoryWarningHelper class]];

  [[[GetWindowMock() stub] andReturn:nil] rootViewController];
  SwizzleSafeModeShouldStart(NO);

  [[[GetStartupInformationMock() stub] andReturnValue:@YES] isColdStart];
  [[GetStartupInformationMock() stub] setIsFirstRun:YES];
  [[[GetStartupInformationMock() stub] andReturnValue:@YES] isFirstRun];

  // Simulate finishing the initialization before going to background.
  [GetAppStateWithMock() startInitialization];
  [GetAppStateWithMock() queueTransitionToNextInitStage];

  // Actions.
  [GetAppStateWithMock() applicationWillEnterForeground:application
                                        metricsMediator:metricsMediator
                                           memoryHelper:memoryHelper];
}

// Tests that -applicationDidEnterBackground do nothing if the application has
// never been in a Foreground stage.
TEST_F(AppStateTest, ApplicationDidEnterBackgroundStageBackground) {
  SwizzleSafeModeShouldStart(NO);
  [[GetStartupInformationMock() stub] setIsFirstRun:YES];
  [[[GetStartupInformationMock() stub] andReturnValue:@YES] isFirstRun];

  // Setup.
  ScopedKeyWindow scopedKeyWindow;
  id application = [OCMockObject mockForClass:[UIApplication class]];
  id memoryHelper = [OCMockObject mockForClass:[MemoryWarningHelper class]];

  ASSERT_EQ(NSUInteger(0), [scopedKeyWindow.Get() subviews].count);

  // Action.
  [GetAppStateWithRealWindow(scopedKeyWindow.Get())
      applicationDidEnterBackground:application
                       memoryHelper:memoryHelper];

  // Tests.
  EXPECT_EQ(NSUInteger(0), [scopedKeyWindow.Get() subviews].count);
}

// Tests that -queueTransitionToNextInitStage transitions to the next stage.
TEST_F(AppStateTest, queueTransitionToNextInitStage) {
  AppState* appState = GetAppStateWithMock();
  ASSERT_EQ(appState.initStage, AppInitStage::kStart);
  [appState queueTransitionToNextInitStage];
  ASSERT_EQ(appState.initStage, NextAppInitStage(AppInitStage::kStart));
}

// Tests that -queueTransitionToNextInitStage notifies observers.
TEST_F(AppStateTest, queueTransitionToNextInitStageNotifiesObservers) {
  // Setup.
  AppState* appState = GetAppStateWithMock();
  id observer = [OCMockObject mockForProtocol:@protocol(AppStateObserver)];
  AppInitStage secondStage = NextAppInitStage(AppInitStage::kStart);
  [appState addObserver:observer];

  [[[observer expect] andDo:^(NSInvocation*) {
    // Verify that the init stage isn't yet increased when calling
    // #willTransitionToInitStage.
    EXPECT_EQ(AppInitStage::kStart, appState.initStage);
  }] appState:appState willTransitionToInitStage:secondStage];
  [[[observer expect] andDo:^(NSInvocation*) {
    // Verify that the init stage is increased when calling
    // #didTransitionFromInitStage.
    EXPECT_EQ(secondStage, appState.initStage);
  }] appState:appState didTransitionFromInitStage:AppInitStage::kStart];

  [appState queueTransitionToNextInitStage];

  EXPECT_EQ(secondStage, appState.initStage);

  [observer verify];
}

// Tests that -queueTransitionToNextInitStage, when called from an observer's
// call, first completes sending previous updates and doesn't change the init
// stage, then transitions to the next init stage and sends updates.
TEST_F(AppStateTest,
       QueueTransitionToNextInitStageReentrantFromWillTransitionToInitStage) {
  // Setup.
  AppState* appState = GetAppStateWithMock(/*with_safe_mode_agent=*/false);
  id observer1 = [OCMockObject mockForProtocol:@protocol(AppStateObserver)];
  AppStateTransitioningObserver* transitioningObserver =
      [[AppStateTransitioningObserver alloc] init];
  id observer2 = [OCMockObject mockForProtocol:@protocol(AppStateObserver)];

  AppInitStage secondStage = NextAppInitStage(AppInitStage::kStart);
  AppInitStage thirdStage = NextAppInitStage(secondStage);

  // The order is important here.
  [appState addObserver:observer1];
  [appState addObserver:transitioningObserver];
  [appState addObserver:observer2];

  // The order is important here. We want to first receive all notifications for
  // the second stage, then all the notifications for the third stage, despite
  // transitioningObserver queueing a new transition from one of the callbacks.
  [[observer1 expect] appState:appState willTransitionToInitStage:secondStage];
  [[observer1 expect] appState:appState
      didTransitionFromInitStage:AppInitStage::kStart];
  [[observer2 expect] appState:appState willTransitionToInitStage:secondStage];
  [[observer2 expect] appState:appState
      didTransitionFromInitStage:AppInitStage::kStart];
  [[observer1 expect] appState:appState willTransitionToInitStage:thirdStage];
  [[observer1 expect] appState:appState didTransitionFromInitStage:secondStage];
  [[observer2 expect] appState:appState willTransitionToInitStage:thirdStage];
  [[observer2 expect] appState:appState didTransitionFromInitStage:secondStage];
  [observer1 setExpectationOrderMatters:YES];
  [observer2 setExpectationOrderMatters:YES];

  [appState queueTransitionToNextInitStage];
  [observer1 verify];
  [observer2 verify];
}

// Tests that -queueTransitionToNextInitStage, when called from an observer's
// call, first completes sending previous updates and doesn't change the init
// stage, then transitions to the next init stage and sends updates.
TEST_F(AppStateTest,
       QueueTransitionToNextInitStageReentrantFromDidTransitionFromInitStage) {
  // Setup.
  AppState* appState = GetAppStateWithMock(/*with_safe_mode_agent=*/false);
  id observer1 = [OCMockObject mockForProtocol:@protocol(AppStateObserver)];
  AppStateTransitioningObserver* transitioningObserver =
      [[AppStateTransitioningObserver alloc] init];
  transitioningObserver.triggerOnDidTransition = YES;
  id observer2 = [OCMockObject mockForProtocol:@protocol(AppStateObserver)];

  AppInitStage secondStage = NextAppInitStage(AppInitStage::kStart);
  AppInitStage thirdStage = NextAppInitStage(secondStage);

  // The order is important here.
  [appState addObserver:observer1];
  [appState addObserver:transitioningObserver];
  [appState addObserver:observer2];

  // The order is important here. We want to first receive all notifications for
  // the second stage, then all the notifications for the third stage, despite
  // transitioningObserver queueing a new transition from one of the callbacks.
  [[observer1 expect] appState:appState willTransitionToInitStage:secondStage];
  [[observer1 expect] appState:appState
      didTransitionFromInitStage:AppInitStage::kStart];
  [[observer2 expect] appState:appState willTransitionToInitStage:secondStage];
  [[observer2 expect] appState:appState
      didTransitionFromInitStage:AppInitStage::kStart];
  [[observer1 expect] appState:appState willTransitionToInitStage:thirdStage];
  [[observer1 expect] appState:appState didTransitionFromInitStage:secondStage];
  [[observer2 expect] appState:appState willTransitionToInitStage:thirdStage];
  [[observer2 expect] appState:appState didTransitionFromInitStage:secondStage];
  [observer1 setExpectationOrderMatters:YES];
  [observer2 setExpectationOrderMatters:YES];

  [appState queueTransitionToNextInitStage];
  [observer1 verify];
  [observer2 verify];
}

// Tests that when ScopedForcePortraitOrientation is created, `-portraitOnly`
// returns YES.
TEST_F(AppStateTest, ForcePortraitOrientation) {
  AppState* appState = GetAppStateWithMock();

  [[[GetWindowMock() stub] andReturn:nil] rootViewController];
  SwizzleSafeModeShouldStart(NO);

  [[[GetStartupInformationMock() stub] andReturnValue:@YES] isColdStart];
  [[GetStartupInformationMock() stub] setIsFirstRun:YES];
  [[[GetStartupInformationMock() stub] andReturnValue:@YES] isFirstRun];

  // Simulate finishing the initialization before going to background.
  [GetAppStateWithMock() startInitialization];
  [GetAppStateWithMock() queueTransitionToNextInitStage];

  ASSERT_FALSE(appState.portraitOnly);
  std::unique_ptr<ScopedForcePortraitOrientation>
      scopedForcePortraitOrientation =
          std::make_unique<ScopedForcePortraitOrientation>(appState);
  ASSERT_TRUE(appState.portraitOnly);

  scopedForcePortraitOrientation.reset();
  ASSERT_FALSE(appState.portraitOnly);
}

TEST_F(AppStateTest, AppAgentRetrieval) {
  AppState* appState = GetAppStateWithMock();
  // There should be the safe mode agent and enterprise agent connected.
  EXPECT_EQ(appState.connectedAgents.count, 2UL);

  TestAppAgent* agent = [[TestAppAgent alloc] init];
  [appState addAgent:agent];
  // `agent` should also now be added.
  EXPECT_EQ(appState.connectedAgents.count, 3UL);

  TestAppAgent* retrievedAgent = [TestAppAgent agentFromApp:appState];
  EXPECT_EQ(retrievedAgent, agent);
}

// Tests observers for UIBlockerManager
TEST_F(AppStateTest, AppAgentUIBlockerManagerObserver) {
  AppState* appState = GetAppStateWithMock();

  id<UIBlockerManagerObserver> observer =
      [OCMockObject mockForProtocol:@protocol(UIBlockerManagerObserver)];
  id<UIBlockerTarget> blocker_target =
      [OCMockObject mockForProtocol:@protocol(UIBlockerTarget)];

  [appState addUIBlockerManagerObserver:observer];
  EXPECT_EQ(appState.currentUIBlocker, nil);

  [appState incrementBlockingUICounterForTarget:blocker_target];
  EXPECT_NSEQ(appState.currentUIBlocker, blocker_target);
  EXPECT_OCMOCK_VERIFY(observer);

  OCMExpect([observer currentUIBlockerRemoved]);
  [appState decrementBlockingUICounterForTarget:blocker_target];
  EXPECT_NSEQ(appState.currentUIBlocker, nil);
  EXPECT_OCMOCK_VERIFY(observer);
}
