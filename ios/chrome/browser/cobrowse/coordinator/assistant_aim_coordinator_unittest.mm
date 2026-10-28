// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_coordinator.h"

#import <memory>
#import <optional>
#import <vector>

#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/aim/model/mock_ios_chrome_aim_eligibility_service.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/cobrowse/shared/metrics/assistant_aim_metrics_constants.h"
#import "ios/chrome/browser/cobrowse/shared/metrics/assistant_aim_metrics_recorder.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_view_controller.h"
#import "ios/chrome/browser/metrics/model/activity_reporter.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
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
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/window_open_disposition.h"
#import "url/gurl.h"

@interface AssistantAIMCoordinator (Testing)
- (std::unique_ptr<web::WebState>)createWebState;
- (void)didAcceptTabSharingConsent;
- (void)didTapTabSharingConsentLearnMore;
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

// Tests that the tab sharing consent alert is presented with the expected
// title, message, and actions when the keyboard is shown while a view inside
// `_viewController` is the first responder and the consent pref is false.
TEST_F(AssistantAIMCoordinatorTest,
       TabSharingConsentAlert_PresentedWhenKeyboardShownWithFirstResponder) {
  id mock_view = OCMClassMock([UIView class]);
  OCMStub([mock_view isFirstResponder]).andReturn(YES);
  id mock_view_controller = OCMClassMock([AssistantAIMViewController class]);
  OCMStub([mock_view_controller view]).andReturn(mock_view);
  [coordinator_ setValue:mock_view_controller forKey:@"viewController"];

  PrefService* prefs = profile_->GetPrefs();
  ASSERT_FALSE(prefs->GetBoolean(prefs::kCobrowseTabSharingConsentAccepted));

  [static_cast<id<AssistantAIMViewControllerDelegate>>(coordinator_)
      assistantAIMViewControllerDidShowKeyboard:mock_view_controller];

  AlertCoordinator* alert_coordinator =
      [coordinator_ valueForKey:@"tabSharingConsentAlertCoordinator"];
  ASSERT_NE(nil, alert_coordinator);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_SHORT_TITLE),
              alert_coordinator.title);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_DESCRIPTION),
              alert_coordinator.message);
  ASSERT_EQ(2u, alert_coordinator.alertController.actions.count);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_ACCEPT_BUTTON),
              alert_coordinator.alertController.actions[0].title);
  EXPECT_EQ(alert_coordinator.alertController.actions[0],
            alert_coordinator.alertController.preferredAction);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_LEARN_MORE),
              alert_coordinator.alertController.actions[1].title);

  [mock_view stopMocking];
  [mock_view_controller stopMocking];
}

// Tests that the tab sharing consent alert is not presented when the keyboard
// is shown if no view inside `_viewController` is the first responder.
TEST_F(AssistantAIMCoordinatorTest,
       TabSharingConsentAlert_NotPresentedWhenNoFirstResponderInView) {
  id mock_view = OCMClassMock([UIView class]);
  OCMStub([mock_view isFirstResponder]).andReturn(NO);
  OCMStub([mock_view subviews]).andReturn(@[]);
  id mock_view_controller = OCMClassMock([AssistantAIMViewController class]);
  OCMStub([mock_view_controller view]).andReturn(mock_view);
  [coordinator_ setValue:mock_view_controller forKey:@"viewController"];

  [static_cast<id<AssistantAIMViewControllerDelegate>>(coordinator_)
      assistantAIMViewControllerDidShowKeyboard:mock_view_controller];

  AlertCoordinator* alert_coordinator =
      [coordinator_ valueForKey:@"tabSharingConsentAlertCoordinator"];
  EXPECT_EQ(nil, alert_coordinator);

  [mock_view stopMocking];
  [mock_view_controller stopMocking];
}

// Tests that accepting the tab sharing consent alert sets the
// `prefs::kCobrowseTabSharingConsentAccepted` pref, dismisses the alert, and
// prevents the alert from showing on subsequent keyboard notifications.
TEST_F(AssistantAIMCoordinatorTest,
       TabSharingConsentAlert_AcceptSetsPrefAndPreventsSubsequentAlert) {
  id mock_recorder = OCMClassMock([AssistantAIMMetricsRecorder class]);
  [coordinator_ setValue:mock_recorder forKey:@"metricsRecorder"];
  id mock_view = OCMClassMock([UIView class]);
  OCMStub([mock_view isFirstResponder]).andReturn(YES);
  id mock_view_controller = OCMClassMock([AssistantAIMViewController class]);
  OCMStub([mock_view_controller view]).andReturn(mock_view);
  [coordinator_ setValue:mock_view_controller forKey:@"viewController"];

  OCMExpect([mock_recorder
      recordFRENoticeAction:AssistantAimFRENoticeAction::kShown]);
  [static_cast<id<AssistantAIMViewControllerDelegate>>(coordinator_)
      assistantAIMViewControllerDidShowKeyboard:mock_view_controller];
  ASSERT_NE(nil,
            [coordinator_ valueForKey:@"tabSharingConsentAlertCoordinator"]);
  EXPECT_OCMOCK_VERIFY(mock_recorder);

  OCMExpect([mock_recorder
      recordFRENoticeAction:AssistantAimFRENoticeAction::kAccepted]);
  [coordinator_ didAcceptTabSharingConsent];
  EXPECT_OCMOCK_VERIFY(mock_recorder);

  PrefService* prefs = profile_->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kCobrowseTabSharingConsentAccepted));
  EXPECT_EQ(nil,
            [coordinator_ valueForKey:@"tabSharingConsentAlertCoordinator"]);

  // Subsequent keyboard notifications should not present the alert again.
  OCMReject([mock_recorder
      recordFRENoticeAction:AssistantAimFRENoticeAction::kShown]);
  [static_cast<id<AssistantAIMViewControllerDelegate>>(coordinator_)
      assistantAIMViewControllerDidShowKeyboard:mock_view_controller];
  EXPECT_EQ(nil,
            [coordinator_ valueForKey:@"tabSharingConsentAlertCoordinator"]);
  EXPECT_OCMOCK_VERIFY(mock_recorder);

  [coordinator_ setValue:nil forKey:@"metricsRecorder"];
  [mock_recorder stopMocking];
  [mock_view stopMocking];
  [mock_view_controller stopMocking];
}

// Tests that tapping "Learn More" on the tab sharing consent alert stops the
// alert, collapses the container to the minimized detent, opens the help center
// URL in a new tab, leaves the consent pref unset, and allows the alert to be
// presented again on next keyboard show.
TEST_F(AssistantAIMCoordinatorTest,
       TabSharingConsentAlert_LearnMoreMinimizesAndOpensHelpInNewTab) {
  UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
  FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
  FakeUrlLoadingBrowserAgent* url_loader =
      FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
          UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

  FakeAssistantContainerCommands* fake_container_handler =
      [[FakeAssistantContainerCommands alloc] init];
  [coordinator_ setValue:fake_container_handler forKey:@"containerHandler"];
  id mock_recorder = OCMClassMock([AssistantAIMMetricsRecorder class]);
  [coordinator_ setValue:mock_recorder forKey:@"metricsRecorder"];

  id mock_view = OCMClassMock([UIView class]);
  OCMStub([mock_view isFirstResponder]).andReturn(YES);
  id mock_view_controller = OCMClassMock([AssistantAIMViewController class]);
  OCMStub([mock_view_controller view]).andReturn(mock_view);
  [coordinator_ setValue:mock_view_controller forKey:@"viewController"];

  OCMExpect([mock_recorder
      recordFRENoticeAction:AssistantAimFRENoticeAction::kShown]);
  [static_cast<id<AssistantAIMViewControllerDelegate>>(coordinator_)
      assistantAIMViewControllerDidShowKeyboard:mock_view_controller];
  ASSERT_NE(nil,
            [coordinator_ valueForKey:@"tabSharingConsentAlertCoordinator"]);
  EXPECT_OCMOCK_VERIFY(mock_recorder);

  OCMExpect([mock_recorder
      recordFRENoticeAction:AssistantAimFRENoticeAction::kLearnMoreTapped]);
  [coordinator_ didTapTabSharingConsentLearnMore];
  EXPECT_OCMOCK_VERIFY(mock_recorder);

  PrefService* prefs = profile_->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kCobrowseTabSharingConsentAccepted));
  EXPECT_EQ(nil,
            [coordinator_ valueForKey:@"tabSharingConsentAlertCoordinator"]);
  ASSERT_TRUE(fake_container_handler.lastAnimatedDetent.has_value());
  EXPECT_EQ(AssistantContainerDetent::kMinimized,
            fake_container_handler.lastAnimatedDetent.value());
  EXPECT_EQ(GURL(kAssistantAIMHelpCenterURL),
            url_loader->last_params.web_params.url);
  EXPECT_EQ(WindowOpenDisposition::kNewForegroundTab,
            url_loader->last_params.disposition);

  // Showing the keyboard again should re-present the consent alert.
  OCMExpect([mock_recorder
      recordFRENoticeAction:AssistantAimFRENoticeAction::kShown]);
  [static_cast<id<AssistantAIMViewControllerDelegate>>(coordinator_)
      assistantAIMViewControllerDidShowKeyboard:mock_view_controller];
  EXPECT_NE(nil,
            [coordinator_ valueForKey:@"tabSharingConsentAlertCoordinator"]);
  EXPECT_OCMOCK_VERIFY(mock_recorder);

  [coordinator_ setValue:nil forKey:@"metricsRecorder"];
  [mock_recorder stopMocking];
  [mock_view stopMocking];
  [mock_view_controller stopMocking];
}
