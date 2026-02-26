// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_first_run_coordinator.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_first_run_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_utils.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
const CGFloat kPromoMaxImpressionCount = 3;

const std::string kFirstProfileName = "FirstProfile";

std::unique_ptr<KeyedService> CreateTestTracker(ProfileIOS* context) {
  return std::make_unique<
      testing::NiceMock<feature_engagement::test::MockTracker>>();
}
}  // namespace

class GeminiFirstRunCoordinatorTest : public PlatformTest {
 public:
  GeminiFirstRunCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(feature_engagement::TrackerFactory::GetInstance(),
                              base::BindOnce(&CreateTestTracker));
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    builder.SetName(kFirstProfileName);
    ProfileIOS* profile =
        profile_manager_.AddProfileWithBuilder(std::move(builder));

    browser_ = std::make_unique<TestBrowser>(profile);
    browser_list_ = BrowserListFactory::GetForProfile(profile);
    browser_list_->AddBrowser(browser_.get());
    TestFullscreenController::CreateForBrowser(browser_.get());
    GeminiBrowserAgent::CreateForBrowser(browser_.get());

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    mock_bwg_command_handler_ = OCMProtocolMock(@protocol(BWGCommands));
    mock_help_command_handler_ = OCMProtocolMock(@protocol(HelpCommands));
    id mock_scene_commands_handler = OCMProtocolMock(@protocol(SceneCommands));

    [dispatcher startDispatchingToTarget:mock_bwg_command_handler_
                             forProtocol:@protocol(BWGCommands)];
    [dispatcher startDispatchingToTarget:mock_help_command_handler_
                             forProtocol:@protocol(HelpCommands)];
    [dispatcher startDispatchingToTarget:mock_scene_commands_handler
                             forProtocol:@protocol(SceneCommands)];
  }

  void StartCoordinatorWithEntryPoint(gemini::EntryPoint entryPoint) {
    base_view_controller_ = [[UIViewController alloc] init];
    scoped_window_ = std::make_unique<ScopedKeyWindow>();
    [scoped_window_->Get() setRootViewController:base_view_controller_];
    [scoped_window_->Get() makeKeyAndVisible];

    coordinator_ = [[GeminiFirstRunCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                    fromEntryPoint:entryPoint
                 completionHandler:nil];
    [coordinator_ start];
    // Wait for the view controller to be presented.
    EXPECT_TRUE(
        base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(5), ^bool {
          return base_view_controller_.presentedViewController != nil;
        }));
    [base_view_controller_.presentedViewController viewDidAppear:NO];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList feature_list_;
  TestProfileManagerIOS profile_manager_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<BrowserList> browser_list_;
  UIViewController* base_view_controller_;
  GeminiFirstRunCoordinator* coordinator_;
  id mock_help_command_handler_;
  id mock_bwg_command_handler_;
  std::unique_ptr<ScopedKeyWindow> scoped_window_;
};

// Tests fullscreen mode not exiting when promo shows from the AI Hub entry
// point.
TEST_F(GeminiFirstRunCoordinatorTest, FullscreenNotExitedOnAIHubEntryPoint) {
  auto* tracker = static_cast<feature_engagement::test::MockTracker*>(
      feature_engagement::TrackerFactory::GetForProfile(
          profile_manager_.GetProfileWithName(kFirstProfileName)));

  TestFullscreenController* controller =
      TestFullscreenController::FromBrowser(browser_.get());
  controller->EnterFullscreen();
  ASSERT_EQ(0.0, controller->GetProgress());

  EXPECT_CALL(
      *tracker,
      NotifyEvent(feature_engagement::events::kIOSPageActionMenuIPHUsed));
  EXPECT_CALL(
      *tracker,
      NotifyEvent(feature_engagement::events::kIOSGeminiPromoFirstCompletion));
  EXPECT_CALL(
      *tracker,
      NotifyEvent(feature_engagement::events::kIOSGeminiFlowStartedNonPromo));

  StartCoordinatorWithEntryPoint(gemini::EntryPoint::AIHub);

  // Check that fullscreen mode is still active.
  EXPECT_EQ(0.0, controller->GetProgress());
}

// Tests fullscreen mode exiting when promo shows from the promo entry point.
TEST_F(GeminiFirstRunCoordinatorTest, FullscreenExitedOnPromoEntryPoint) {
  feature_list_.InitWithFeatures(
      {feature_engagement::kIPHiOSGeminiFullscreenPromoFeature,
       kGeminiNavigationPromo, kAskGeminiChip, kPageActionMenu},
      {});
  auto* tracker = static_cast<feature_engagement::test::MockTracker*>(
      feature_engagement::TrackerFactory::GetForProfile(
          profile_manager_.GetProfileWithName(kFirstProfileName)));
  EXPECT_CALL(*tracker, WouldTriggerHelpUI(testing::Ref(
                            feature_engagement::kIPHIOSPageActionMenu)))
      .WillRepeatedly(testing::Return(true));

  TestFullscreenController* controller =
      TestFullscreenController::FromBrowser(browser_.get());
  controller->EnterFullscreen();
  ASSERT_EQ(0.0, controller->GetProgress());
  EXPECT_CALL(
      *tracker,
      NotifyEvent(feature_engagement::events::kIOSGeminiPromoFirstCompletion));
  EXPECT_CALL(
      *tracker,
      NotifyEvent(
          feature_engagement::events::kIOSGeminiFullscreenPromoTriggered));

  EXPECT_CALL(
      *tracker,
      NotifyEvent(
          feature_engagement::events::kIOSFullscreenPromosGroupTrigger));

  StartCoordinatorWithEntryPoint(gemini::EntryPoint::Promo);

  // Check that fullscreen mode is deactivated.
  EXPECT_EQ(1.0, controller->GetProgress());
}

// Tests that the promo doesn't show after the maximum impression count.
TEST_F(GeminiFirstRunCoordinatorTest, GeminiPromoNotShown) {
  ProfileIOS* profile = profile_manager_.GetProfileWithName(kFirstProfileName);
  PrefService* prefs = profile->GetPrefs();
  prefs->SetInteger(prefs::kIOSBWGPromoImpressionCount,
                    kPromoMaxImpressionCount);

  auto* tracker = static_cast<feature_engagement::test::MockTracker*>(
      feature_engagement::TrackerFactory::GetForProfile(profile));

  // `kIOSGeminiPromoFirstCompletion` should not be notified if the promo is not
  // shown.
  EXPECT_CALL(
      *tracker,
      NotifyEvent(feature_engagement::events::kIOSGeminiPromoFirstCompletion))
      .Times(0);
  EXPECT_CALL(
      *tracker,
      NotifyEvent(
          feature_engagement::events::kIOSGeminiFullscreenPromoTriggered))
      .Times(0);
  EXPECT_CALL(
      *tracker,
      NotifyEvent(feature_engagement::events::kIOSFullscreenPromosGroupTrigger))
      .Times(0);

  StartCoordinatorWithEntryPoint(gemini::EntryPoint::Promo);

  // Checks that a promo didn't start and the impression count didn't
  // increase.
  EXPECT_EQ(kPromoMaxImpressionCount,
            prefs->GetInteger(prefs::kIOSBWGPromoImpressionCount));
}

// Tests AI Hub IPH starts when the user is shown the promo from initially
// tapping a non AI Hub entry point.
TEST_F(GeminiFirstRunCoordinatorTest, AIHubIPHWasTriggered) {
  OCMExpect([mock_help_command_handler_
      presentInProductHelpWithType:InProductHelpType::kPageActionMenu]);

  StartCoordinatorWithEntryPoint(gemini::EntryPoint::Promo);
  [coordinator_ stop];

  EXPECT_OCMOCK_VERIFY(mock_help_command_handler_);
}

// Tests AI Hub IPH doesn't start when the user is shown the promo from
// initially tapping the AI Hub entry point.
TEST_F(GeminiFirstRunCoordinatorTest, AIHubIPHNotTriggered) {
  OCMReject([mock_help_command_handler_
      presentInProductHelpWithType:InProductHelpType::kPageActionMenu]);

  StartCoordinatorWithEntryPoint(gemini::EntryPoint::AIHub);
  [coordinator_ stop];

  EXPECT_OCMOCK_VERIFY(mock_help_command_handler_);
}
