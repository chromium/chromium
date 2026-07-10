// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/coordinator/scene_coordinator.h"

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/supervised_user/test_support/kids_chrome_management_test_utils.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_options.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_data.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using UserFeedbackDataCallback =
    base::RepeatingCallback<void(UserFeedbackData*)>;

@interface SceneCoordinator (Testing)
- (void)presentReportAnIssueViewController:(UIViewController*)baseViewController
                                    sender:(UserFeedbackSender)sender
                          userFeedbackData:(UserFeedbackData*)userFeedbackData
                                   timeout:(base::TimeDelta)timeout
                                completion:(UserFeedbackDataCallback)completion;
@end

namespace {

class SceneCoordinatorTest : public PlatformTest {
 protected:
  SceneCoordinatorTest() {
    base_view_controller_ = [[UIViewController alloc] init];

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
    profile_state_.profile = profile_.get();

    scene_state_ = [[FakeSceneState alloc] initWithAppState:nil
                                                    profile:profile_.get()];
    [scene_state_ connectWithOptions:{.profile_state = profile_state_,
                                      .identifier = "scene"}];

    profile_->SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_loader_factory_));

    coordinator_ = [[SceneCoordinator alloc]
        initWithTabOpener:OCMProtocolMock(@protocol(TabOpening))];
    [coordinator_
        setBrowsersFromProvider:scene_state_.browserProviderInterface];
  }

  void TearDown() override {
    @autoreleasepool {
      [coordinator_ stop];
      [scene_state_ shutdown];
      coordinator_ = nil;
      scene_state_ = nil;
      profile_state_ = nil;
      base_view_controller_ = nil;
    }
  }

  void MakePrimaryAccountAvailable(const std::string& email) {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile_.get()), email,
        signin::ConsentLevel::kSignin);
  }

  void SetAutomaticIssueOfAccessTokens(bool grant) {
    signin::SetAutomaticIssueOfAccessTokens(
        IdentityManagerFactory::GetForProfile(profile_.get()), grant);
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::UI,
      web::WebTaskEnvironment::IOThreadType::REAL_THREAD,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  network::TestURLLoaderFactory test_loader_factory_;
  std::unique_ptr<TestProfileIOS> profile_;
  UIViewController* base_view_controller_;
  FakeSceneState* scene_state_;
  ProfileState* profile_state_;
  SceneCoordinator* coordinator_;
};

// Tests that "Report an issue" populates user feedback data with available
// information on the family member role for the signed-in user.
TEST_F(SceneCoordinatorTest,
       TestReportAnIssueViewControllerWithFamilyResponse) {
  MakePrimaryAccountAvailable("foo@gmail.com");
  SetAutomaticIssueOfAccessTokens(/*grant=*/true);

  base::RunLoop run_loop;
  UserFeedbackDataCallback completion =
      base::BindRepeating(^(UserFeedbackData* data) {
        EXPECT_EQ(UserFeedbackSender::ToolsMenu, data.origin);
        EXPECT_FALSE(data.currentPageIsIncognito);
        EXPECT_NSEQ(data.familyMemberRole, @"child");
      }).Then(run_loop.QuitClosure());

  [coordinator_
      presentReportAnIssueViewController:base_view_controller_
                                  sender:UserFeedbackSender::ToolsMenu
                        userFeedbackData:[[UserFeedbackData alloc] init]
                                 timeout:base::Seconds(1)
                              completion:std::move(completion)];

  // Create the family members fetch response.
  kidsmanagement::ListMembersResponse list_family_members_response;
  supervised_user::SetFamilyMemberAttributesForTesting(
      list_family_members_response.add_members(), kidsmanagement::CHILD, "foo");
  test_loader_factory_.SimulateResponseForPendingRequest(
      "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/families/"
      "mine/members?alt=proto&allow_empty_family=true",
      list_family_members_response.SerializeAsString());

  run_loop.Run();
}

// Tests that "Report an issue" populates user feedback data for signed-in user.
TEST_F(SceneCoordinatorTest, TestReportAnIssueViewControllerForSignedInUser) {
  MakePrimaryAccountAvailable("foo@gmail.com");

  base::RunLoop run_loop;
  UserFeedbackDataCallback completion =
      base::BindRepeating(^(UserFeedbackData* data) {
        EXPECT_EQ(UserFeedbackSender::ToolsMenu, data.origin);
        EXPECT_FALSE(data.currentPageIsIncognito);
        EXPECT_EQ(nil, data.familyMemberRole);
      }).Then(run_loop.QuitClosure());

  [coordinator_
      presentReportAnIssueViewController:base_view_controller_
                                  sender:UserFeedbackSender::ToolsMenu
                        userFeedbackData:[[UserFeedbackData alloc] init]
                                 timeout:base::Seconds(0)
                              completion:std::move(completion)];
  run_loop.Run();
}

// Tests that "Report an issue" populates user feedback data for signed-out
// user.
TEST_F(SceneCoordinatorTest, TestReportAnIssueViewControllerForSignedOutUser) {
  base::RunLoop run_loop;
  UserFeedbackDataCallback completion =
      base::BindRepeating(^(UserFeedbackData* data) {
        EXPECT_EQ(UserFeedbackSender::ToolsMenu, data.origin);
        EXPECT_FALSE(data.currentPageIsIncognito);
        EXPECT_EQ(nil, data.familyMemberRole);
      }).Then(run_loop.QuitClosure());

  [coordinator_
      presentReportAnIssueViewController:base_view_controller_
                                  sender:UserFeedbackSender::ToolsMenu
                        userFeedbackData:[[UserFeedbackData alloc] init]
                                 timeout:base::Seconds(1)
                              completion:std::move(completion)];
  run_loop.Run();
}

// Tests that scene coordinator updates scene state's incognitoContentVisible
// when the relevant scene commands is called.
TEST_F(SceneCoordinatorTest, UpdatesIncognitoContentVisibility) {
  [coordinator_ setIncognitoContentVisible:NO];
  EXPECT_FALSE(scene_state_.incognitoState.incognitoContentVisible);
  [coordinator_ setIncognitoContentVisible:YES];
  EXPECT_TRUE(scene_state_.incognitoState.incognitoContentVisible);
  [coordinator_ setIncognitoContentVisible:NO];
  EXPECT_FALSE(scene_state_.incognitoState.incognitoContentVisible);
}

}  // namespace
