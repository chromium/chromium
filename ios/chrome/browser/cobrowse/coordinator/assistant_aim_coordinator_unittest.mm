// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_coordinator.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/aim/model/mock_ios_chrome_aim_eligibility_service.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_view_controller.h"
#import "ios/chrome/browser/metrics/model/activity_reporter.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

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
    profile_ = std::move(builder).Build();
    scene_state_ = [[SceneState alloc] init];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    mock_container_handler_ =
        OCMProtocolMock(@protocol(AssistantContainerCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_container_handler_
                     forProtocol:@protocol(AssistantContainerCommands)];
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
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  SceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  id mock_container_handler_;
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

TEST_F(AssistantAIMCoordinatorTest,
       DidTapClose_ShowsSnackbarOnDismissalCompletion) {
  // Set up _viewController using KVC.
  id mockViewController = OCMClassMock([AssistantAIMViewController class]);
  [coordinator_ setValue:mockViewController forKey:@"viewController"];

  // Expect the container dismissal to be triggered.
  // We want to capture the completion block passed to it.
  __block ProceduralBlock dismissal_completion = nil;
  OCMExpect([mock_container_handler_
      dismissAssistantContainerAnimated:YES
                             completion:[OCMArg checkWithBlock:^BOOL(id obj) {
                               dismissal_completion = obj;
                               return YES;
                             }]]);

  // Trigger close tap.
  [(id<AssistantAIMViewControllerDelegate>)coordinator_
      assistantAIMViewControllerDidTapClose:mockViewController];

  [mock_container_handler_ verify];
  [mock_snackbar_handler_ verify];

  // Now, the snackbar should be shown when the completion block is executed.
  OCMExpect([mock_snackbar_handler_ showSnackbarMessage:[OCMArg any]]);

  ASSERT_NE(nil, dismissal_completion);
  dismissal_completion();

  [mock_snackbar_handler_ verify];

  [mockViewController stopMocking];
}
