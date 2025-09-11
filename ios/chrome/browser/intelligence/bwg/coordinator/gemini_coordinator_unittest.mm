// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_coordinator.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {
std::unique_ptr<KeyedService> CreateTestTracker(ProfileIOS* context) {
  return std::make_unique<
      testing::NiceMock<feature_engagement::test::MockTracker>>();
}
}  // namespace

class GeminiCoordinatorTest : public PlatformTest {
 public:
  GeminiCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(feature_engagement::TrackerFactory::GetInstance(),
                              base::BindOnce(&CreateTestTracker));
    builder.SetName("TestProfile");
    ProfileIOS* profile =
        profile_manager_.AddProfileWithBuilder(std::move(builder));

    browser_ = std::make_unique<TestBrowser>(profile);
    browser_list_ = BrowserListFactory::GetForProfile(profile);
    browser_list_->AddBrowser(browser_.get());
    TestFullscreenController::CreateForBrowser(browser_.get());

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    id mock_application_command_handler =
        OCMProtocolMock(@protocol(BWGCommands));
    id mock_settings_command_handler = OCMProtocolMock(@protocol(HelpCommands));
    id mock_application_commands_handler =
        OCMProtocolMock(@protocol(ApplicationCommands));

    [dispatcher startDispatchingToTarget:mock_application_command_handler
                             forProtocol:@protocol(BWGCommands)];
    [dispatcher startDispatchingToTarget:mock_settings_command_handler
                             forProtocol:@protocol(HelpCommands)];
    [dispatcher startDispatchingToTarget:mock_application_commands_handler
                             forProtocol:@protocol(ApplicationCommands)];
  }

  void StartCoordinatorWithEntryPoint(bwg::EntryPoint entryPoint) {
    base_view_controller_ = [[UIViewController alloc] init];
    coordinator_ =
        [[BWGCoordinator alloc] initWithBaseViewController:base_view_controller_
                                                   browser:browser_.get()
                                            fromEntryPoint:entryPoint];
    [coordinator_ start];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList feature_list_;
  TestProfileManagerIOS profile_manager_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<BrowserList> browser_list_;
  UIViewController* base_view_controller_;
  BWGCoordinator* coordinator_;
};

// Tests starting the coordinator from the AI Hub entry point.
TEST_F(GeminiCoordinatorTest, FullscreenNotExitedOnAIHubEntryPoint) {
  auto* tracker = static_cast<feature_engagement::test::MockTracker*>(
      feature_engagement::TrackerFactory::GetForProfile(
          profile_manager_.GetProfileWithName("TestProfile")));

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

  StartCoordinatorWithEntryPoint(bwg::EntryPoint::AIHub);

  // Check that fullscreen mode is still active.
  EXPECT_EQ(0.0, controller->GetProgress());
}
