// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_coordinator.h"

#import <memory>
#import <optional>
#import <vector>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/aim/model/mock_ios_chrome_aim_eligibility_service.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/cobrowse/shared/metrics/assistant_aim_metrics_recorder.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_view_controller.h"
#import "ios/chrome/browser/metrics/model/activity_reporter.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ssl/model/captive_portal_tab_helper.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

@interface AssistantAIMCoordinator (Testing)
- (std::unique_ptr<web::WebState>)createWebState;
@end

// Fake command handler for testing `AssistantContainerCommands` methods,
// including `setAssistantContainerDetents:` which takes a `std::vector` by
// value that `OCMock` cannot unbox.
@interface FakeAssistantContainerCommands
    : NSObject <AssistantContainerCommands>
@property(nonatomic, assign) std::vector<AssistantContainerDetent> detents;
@property(nonatomic, assign) std::optional<AssistantContainerDetent>
    lastAnimatedDetent;
@end

@implementation FakeAssistantContainerCommands

- (void)showAssistantContainerWithContent:(UIViewController*)viewController
                                 delegate:
                                     (id<AssistantContainerDelegate>)delegate {
}

- (void)dismissAssistantContainerAnimated:(BOOL)animated
                               completion:(ProceduralBlock)completion {
}

- (void)setAssistantContainerDetents:
    (std::vector<AssistantContainerDetent>)detents {
  _detents = std::move(detents);
}

- (void)setAssistantContainerLargestUndimmedDetent:
    (AssistantContainerDetent)detent {
}

- (void)animateAssistantContainerToDetent:(AssistantContainerDetent)detent {
  _lastAnimatedDetent = detent;
}

- (void)animateAssistantContainerToDetent:(AssistantContainerDetent)detent
                                 duration:(NSTimeInterval)duration
                                    curve:(UIViewAnimationCurve)curve {
  _lastAnimatedDetent = detent;
}

- (void)setAssistantContainerMinimizedDetentHeight:(NSInteger)height {
}

- (void)setAssistantContainerMediumDetentHeight:
    (std::optional<NSInteger>)height {
}

- (void)setAssistantContainerGrabberHidden:(BOOL)hidden
                                  animated:(BOOL)animated {
}

@end

namespace {

std::unique_ptr<KeyedService> BuildMockIOSChromeAimEligibilityService(
    ProfileIOS* profile) {
  return MockIOSChromeAimEligibilityService::CreateTestingProfileService(
      profile);
}

}  // namespace

class AssistantAIMCoordinatorTest : public PlatformTest {
 public:
  AssistantAIMCoordinatorTest() {
    scoped_feature_list_.InitWithFeatures({kAimCobrowse, kAssistantContainer},
                                          {});
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSChromeAimEligibilityServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockIOSChromeAimEligibilityService));
    // Services used by the tab helpers attached to the coordinator's WebState.
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = std::move(builder).Build();
    scene_state_ = [[SceneState alloc] init];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    mock_container_handler_ =
        OCMProtocolMock(@protocol(AssistantContainerCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_container_handler_
                     forProtocol:@protocol(AssistantContainerCommands)];
    mock_scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_scene_handler_
                     forProtocol:@protocol(SceneCommands)];
    mock_snackbar_handler_ = OCMProtocolMock(@protocol(SnackbarCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_snackbar_handler_
                     forProtocol:@protocol(SnackbarCommands)];
  }

  void SetUp() override {
    PlatformTest::SetUp();
    coordinator_ = [[AssistantAIMCoordinator alloc]
        initWithBaseViewController:nil
                           browser:browser_.get()];
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  SceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  id mock_container_handler_;
  id mock_scene_handler_;
  id mock_snackbar_handler_;
  AssistantAIMCoordinator* coordinator_ = nil;
};

TEST_F(AssistantAIMCoordinatorTest, ActivityReporting_SetVisible) {
  id mockInstance = OCMClassMock([ActivityReporter class]);
  [coordinator_ setValue:mockInstance forKey:@"activityReporter"];

  // setVisible:NO should report inactive even if _viewController is nil.
  OCMExpect([mockInstance reportInactive]);
  [coordinator_ setVisible:NO];
  [mockInstance verify];

  // Set up _viewController using KVC.
  id mockViewController = OCMClassMock([AssistantAIMViewController class]);
  [coordinator_ setValue:mockViewController forKey:@"viewController"];

  // setVisible:YES should report active when _viewController is non-nil.
  OCMExpect([mockInstance reportActive]);
  [coordinator_ setVisible:YES];
  [mockInstance verify];

  [coordinator_ setValue:nil forKey:@"activityReporter"];
  [mockInstance stopMocking];
  [mockViewController stopMocking];
}

// Tests that `AssistantAIMCoordinator` forwards visibility, detent change, and
// stop events to `AssistantAIMMetricsRecorder`.
TEST_F(AssistantAIMCoordinatorTest, MetricsReporting_VisibilityAndDetent) {
  id mock_recorder = OCMClassMock([AssistantAIMMetricsRecorder class]);
  [coordinator_ setValue:mock_recorder forKey:@"metricsRecorder"];

  id mock_view_controller = OCMClassMock([AssistantAIMViewController class]);
  [coordinator_ setValue:mock_view_controller forKey:@"viewController"];

  OCMExpect(
      [mock_recorder recordDetentChanged:AssistantContainerDetent::kMinimized]);
  OCMExpect([mock_recorder recordVisibilityChanged:YES]);
  [coordinator_ setVisible:YES inMinimizedState:YES];
  [mock_recorder verify];

  OCMExpect(
      [mock_recorder recordDetentChanged:AssistantContainerDetent::kLarge]);
  [static_cast<id<AssistantContainerDelegate>>(coordinator_)
      assistantContainer:nil
         didChangeDetent:AssistantContainerDetent::kLarge];
  [mock_recorder verify];

  OCMExpect([mock_recorder recordVisibilityChanged:NO]);
  [coordinator_ setVisible:NO];
  [mock_recorder verify];

  OCMExpect([mock_recorder recordSessionEnded]);
  [coordinator_ stop];
  [mock_recorder verify];
}

TEST_F(AssistantAIMCoordinatorTest,
       DidTapClose_ShowsSnackbarOnDismissalCompletion) {
  // Set up _viewController and _metricsRecorder using KVC.
  id mock_view_controller = OCMClassMock([AssistantAIMViewController class]);
  [coordinator_ setValue:mock_view_controller forKey:@"viewController"];
  id mock_recorder = OCMClassMock([AssistantAIMMetricsRecorder class]);
  [coordinator_ setValue:mock_recorder forKey:@"metricsRecorder"];

  // Expect the container dismissal to be triggered.
  // We want to capture the completion block passed to it.
  __block ProceduralBlock dismissal_completion = nil;
  OCMExpect([mock_recorder recordVisibilityChanged:NO]);
  OCMExpect([mock_container_handler_
      dismissAssistantContainerAnimated:YES
                             completion:[OCMArg checkWithBlock:^BOOL(id obj) {
                               dismissal_completion = obj;
                               return YES;
                             }]]);

  // Trigger close tap.
  [static_cast<id<AssistantAIMViewControllerDelegate>>(coordinator_)
      assistantAIMViewControllerDidTapClose:mock_view_controller];

  [mock_recorder verify];
  [mock_container_handler_ verify];
  [mock_snackbar_handler_ verify];

  // Now, the snackbar should be shown when the completion block is executed.
  __block SnackbarMessage* captured_message = nil;
  OCMExpect([mock_snackbar_handler_
      showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(SnackbarMessage* msg) {
        captured_message = msg;
        return YES;
      }]]);

  ASSERT_NE(nil, dismissal_completion);
  dismissal_completion();

  [mock_snackbar_handler_ verify];
  ASSERT_NE(nil, captured_message);
  ASSERT_NE(nil, captured_message.completionHandler);

  OCMExpect([mock_recorder recordClosedSnackbarResolvedWithUndo:NO]);
  OCMExpect([mock_recorder recordSessionEnded]);
  captured_message.completionHandler(YES);
  [mock_recorder verify];
}

// Tests that the captive portal login page requested from the coordinator's
// WebState, i.e. when tapping "Connect" on a captive portal interstitial shown
// in the sheet, opens in a new tab. Regression test for crbug.com/571963356.
TEST_F(AssistantAIMCoordinatorTest, CaptivePortalLoginPageOpensInNewTab) {
  TabInsertionBrowserAgent::CreateForBrowser(browser_.get());
  std::unique_ptr<web::WebState> web_state = [coordinator_ createWebState];
  ASSERT_TRUE(web_state);
  CaptivePortalTabHelper* captive_portal_tab_helper =
      CaptivePortalTabHelper::FromWebState(web_state.get());
  ASSERT_TRUE(captive_portal_tab_helper);

  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_TRUE(web_state_list->empty());

  const GURL landing_url("http://captive.portal.test/login");
  captive_portal_tab_helper->DisplayCaptivePortalLoginPage(landing_url);

  ASSERT_EQ(1, web_state_list->count());
  web::NavigationItem* pending_item = web_state_list->GetWebStateAt(0)
                                          ->GetNavigationManager()
                                          ->GetPendingItem();
  ASSERT_TRUE(pending_item);
  EXPECT_EQ(landing_url, pending_item->GetURL());
}

// Tests that transitioning traits to iPhone landscape removes the medium detent
// and expands the container from `kMedium` to `kLarge`.
TEST_F(AssistantAIMCoordinatorTest,
       DidChangeTraits_IPhoneLandscapeAnimatesMediumToLarge) {
  FakeAssistantContainerCommands* fake_container_handler =
      [[FakeAssistantContainerCommands alloc] init];
  [coordinator_ setValue:fake_container_handler forKey:@"containerHandler"];

  id mock_view_controller = OCMClassMock([AssistantAIMViewController class]);
  UITraitCollection* landscape_traits = [UITraitCollection
      traitCollectionWithTraits:^(id<UIMutableTraits> traits) {
        traits.userInterfaceIdiom = UIUserInterfaceIdiomPhone;
        traits.verticalSizeClass = UIUserInterfaceSizeClassCompact;
      }];
  OCMStub([mock_view_controller traitCollection]).andReturn(landscape_traits);
  [coordinator_ setValue:mock_view_controller forKey:@"viewController"];

  [static_cast<id<AssistantContainerDelegate>>(coordinator_)
      assistantContainer:nil
         didChangeDetent:AssistantContainerDetent::kMedium];

  [static_cast<id<AssistantAIMViewControllerDelegate>>(coordinator_)
      assistantAIMViewControllerDidChangeTraits:mock_view_controller];

  std::vector<AssistantContainerDetent> expected_detents = {
      AssistantContainerDetent::kMinimized,
      AssistantContainerDetent::kLarge,
  };
  EXPECT_EQ(expected_detents, fake_container_handler.detents);
  ASSERT_TRUE(fake_container_handler.lastAnimatedDetent.has_value());
  EXPECT_EQ(AssistantContainerDetent::kLarge,
            fake_container_handler.lastAnimatedDetent.value());

  [mock_view_controller stopMocking];
}

// Tests that showing the assistant in iPhone landscape when the previous detent
// was `kMedium` animates to `kLarge` instead of `kMedium`.
TEST_F(AssistantAIMCoordinatorTest,
       SetVisible_IPhoneLandscapeUsesLargeInsteadOfMedium) {
  FakeAssistantContainerCommands* fake_container_handler =
      [[FakeAssistantContainerCommands alloc] init];
  [coordinator_ setValue:fake_container_handler forKey:@"containerHandler"];

  id mock_view_controller = OCMClassMock([AssistantAIMViewController class]);
  UITraitCollection* landscape_traits = [UITraitCollection
      traitCollectionWithTraits:^(id<UIMutableTraits> traits) {
        traits.userInterfaceIdiom = UIUserInterfaceIdiomPhone;
        traits.verticalSizeClass = UIUserInterfaceSizeClassCompact;
      }];
  OCMStub([mock_view_controller traitCollection]).andReturn(landscape_traits);
  [coordinator_ setValue:mock_view_controller forKey:@"viewController"];

  [static_cast<id<AssistantContainerDelegate>>(coordinator_)
      assistantContainer:nil
         didChangeDetent:AssistantContainerDetent::kMedium];

  [coordinator_ setVisible:YES inMinimizedState:NO];

  ASSERT_TRUE(fake_container_handler.lastAnimatedDetent.has_value());
  EXPECT_EQ(AssistantContainerDetent::kLarge,
            fake_container_handler.lastAnimatedDetent.value());

  [mock_view_controller stopMocking];
}
