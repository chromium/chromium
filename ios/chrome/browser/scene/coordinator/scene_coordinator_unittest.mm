// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/coordinator/scene_coordinator.h"

#import <memory>
#import <optional>
#import <string>
#import <utility>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/supervised_user/test_support/kids_chrome_management_test_utils.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signout_action_sheet/undo_signout/coordinator/undo_signout_coordinator.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/remote_actor_credential_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/passwords/model/remote_actor/proto/remote_actor_password_upload_request.pb.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_constants.h"
#import "ios/chrome/browser/scene/coordinator/signin_modal_host.h"
#import "ios/chrome/browser/scene/ui/scene_view_controller.h"
#import "ios/chrome/browser/scene/ui/scene_view_controller_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_sign_in_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_data.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using UserFeedbackDataCallback =
    base::RepeatingCallback<void(UserFeedbackData*)>;

@interface SceneCoordinator (Testing)
@property(nonatomic, readonly) Browser* currentBrowser;
- (void)presentReportAnIssueViewController:(UIViewController*)baseViewController
                                    sender:(UserFeedbackSender)sender
                          userFeedbackData:(UserFeedbackData*)userFeedbackData
                                   timeout:(base::TimeDelta)timeout
                                completion:(UserFeedbackDataCallback)completion;
@end

namespace {

// Returns whether `coordinator` handles the scene-level commands sent through
// `browser`'s dispatcher.
bool HandlesSceneCommands(SceneCoordinator* coordinator, Browser* browser) {
  // One required selector of each of `SceneCommands`, `SettingsCommands` and
  // `GeminiCommands`.
  const SEL selectors[] = {
    @selector(dismissModalDialogsWithCompletion:),
    @selector(showGeminiSettings),
    @selector(dismissGeminiFlowWithCompletion:),
  };
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  for (SEL selector : selectors) {
    if ([dispatcher forwardingTargetForSelector:selector] != coordinator) {
      return false;
    }
  }
  return true;
}

// Returns whether the `SceneSignInCommands` sent through `browser`'s dispatcher
// are handled by the sign-in modal host of `coordinator`, rather than by
// `coordinator`.
bool SigninModalHostHandlesSceneSignInCommands(SceneCoordinator* coordinator,
                                               Browser* browser) {
  id target = [browser->GetCommandDispatcher()
      forwardingTargetForSelector:
          @selector(stopSigninCoordinatorWithCompletionAnimated:)];
  return [target isKindOfClass:[SigninModalHost class]];
}

// Holds strong references to mock command handlers registered on the
// `CommandDispatcher` (which only holds weak references to targets).
struct ModalDismissalMocks {
  id snackbar_handler;
  id bookmarks_handler;
  id browser_coordinator_handler;
  id gemini_handler;
};

// Registers mock handlers needed by `dismissModalDialogsWithCompletion:` on
// `coordinator`'s command dispatcher and returns them.
ModalDismissalMocks SetUpModalDismissalMocks(SceneCoordinator* coordinator) {
  CommandDispatcher* dispatcher =
      coordinator.currentBrowser->GetCommandDispatcher();

  ModalDismissalMocks mocks;
  mocks.snackbar_handler = OCMProtocolMock(@protocol(SnackbarCommands));
  [dispatcher startDispatchingToTarget:mocks.snackbar_handler
                           forProtocol:@protocol(SnackbarCommands)];

  mocks.bookmarks_handler = OCMProtocolMock(@protocol(BookmarksCommands));
  [dispatcher startDispatchingToTarget:mocks.bookmarks_handler
                           forProtocol:@protocol(BookmarksCommands)];

  mocks.browser_coordinator_handler =
      OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
  OCMStub([mocks.browser_coordinator_handler
              clearPresentedStateWithCompletion:[OCMArg any]
                                 dismissOmnibox:YES])
      .andDo(^(NSInvocation* invocation) {
        void* ptr = nullptr;
        [invocation getArgument:&ptr atIndex:2];
        ProceduralBlock completionBlock = (__bridge ProceduralBlock)ptr;
        ASSERT_TRUE(completionBlock);
        completionBlock();
      });
  [dispatcher startDispatchingToTarget:mocks.browser_coordinator_handler
                           forProtocol:@protocol(BrowserCoordinatorCommands)];

  mocks.gemini_handler = OCMProtocolMock(@protocol(GeminiCommands));
  // Replace the scene coordinator, which handles `GeminiCommands`, by the mock.
  [dispatcher stopDispatchingForProtocol:@protocol(GeminiCommands)];
  [dispatcher startDispatchingToTarget:mocks.gemini_handler
                           forProtocol:@protocol(GeminiCommands)];
  return mocks;
}

class SceneCoordinatorTest : public PlatformTest {
 protected:
  SceneCoordinatorTest() {
    base_view_controller_ = [[UIViewController alloc] init];

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindOnce(&IdentityTestEnvironmentBrowserStateAdaptor::
                           BuildIdentityManagerForTests));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.SetSharedURLLoaderFactory(
        test_loader_factory_.GetSafeWeakWrapper());
    profile_ = std::move(builder).Build();

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
    profile_state_.profile = profile_.get();

    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.profileState = profile_state_;
    scene_state_.sceneSessionID = "scene";

    coordinator_ = [[SceneCoordinator alloc]
        initWithTabOpener:OCMProtocolMock(@protocol(TabOpening))];
    [coordinator_
        connectBrowsersFromProvider:scene_state_.browserProviderInterface];
  }

  void TearDown() override {
    @autoreleasepool {
      [partial_scene_coordinator_ stopMocking];
      partial_scene_coordinator_ = nil;
      [mock_application_ stopMocking];
      mock_application_ = nil;
      remote_actor_start_swizzler_.reset();
      started_remote_actor_coordinator_ = nil;
      scoped_key_window_.reset();
      [coordinator_ stop];
      [scene_state_ shutdown];
      mock_browser_coordinator_handler_ = nil;
      mock_bookmarks_handler_ = nil;
      mock_snackbar_handler_ = nil;
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

  // Registers mock `SnackbarCommands`, `BookmarksCommands`, and
  // `BrowserCoordinatorCommands` handlers required by
  // `dismissModalDialogsWithCompletion:`.
  void RegisterDismissModalDialogsHandlers() {
    CommandDispatcher* dispatcher =
        coordinator_.currentBrowser->GetCommandDispatcher();
    mock_snackbar_handler_ = OCMProtocolMock(@protocol(SnackbarCommands));
    [dispatcher startDispatchingToTarget:mock_snackbar_handler_
                             forProtocol:@protocol(SnackbarCommands)];
    mock_bookmarks_handler_ = OCMProtocolMock(@protocol(BookmarksCommands));
    [dispatcher startDispatchingToTarget:mock_bookmarks_handler_
                             forProtocol:@protocol(BookmarksCommands)];
    mock_browser_coordinator_handler_ =
        OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
    [dispatcher startDispatchingToTarget:mock_browser_coordinator_handler_
                             forProtocol:@protocol(BrowserCoordinatorCommands)];
  }

  // Registers mock command handlers for `dismissModalDialogsWithCompletion:`
  // and stubs `clearPresentedStateWithCompletion:dismissOmnibox:` to invoke its
  // completion block synchronously.
  void SetUpDismissModalDialogsHandlers() {
    RegisterDismissModalDialogsHandlers();
    OCMStub([mock_browser_coordinator_handler_
        clearPresentedStateWithCompletion:[OCMArg invokeBlock]
                           dismissOmnibox:YES]);
  }

  // Sets up the shared foreground key-window, `UIApplication` mock,
  // `activeViewController` partial mock,
  // `RemoteActorCredentialBottomSheetCoordinator` start swizzler, and
  // modal-dismissal command handlers used by deferred Remote Actor presentation
  // tests.
  void SetUpRemoteActorModalDismissalHarness() {
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;
    scoped_key_window_.emplace();
    scoped_key_window_->Get().rootViewController = base_view_controller_;
    ASSERT_NE(base_view_controller_.view.window, nil);

    RegisterDismissModalDialogsHandlers();

    mock_application_ = OCMClassMock([UIApplication class]);
    OCMStub([mock_application_ sharedApplication]).andReturn(mock_application_);

    partial_scene_coordinator_ = OCMPartialMock(coordinator_);
    OCMStub([partial_scene_coordinator_ activeViewController])
        .andReturn(base_view_controller_);

    RemoteActorCredentialBottomSheetCoordinator* __strong* started_slot =
        &started_remote_actor_coordinator_;
    remote_actor_start_swizzler_.emplace(
        [RemoteActorCredentialBottomSheetCoordinator class], @selector(start),
        ^(RemoteActorCredentialBottomSheetCoordinator* self_coordinator) {
          *started_slot = self_coordinator;
        });
  }

  // Dispatches a Remote Actor bottom sheet request and returns the captured
  // deferred modal-dismissal completion block.
  ProceduralBlock TriggerDeferredRemoteActorRequest() {
    EXPECT_NE(mock_browser_coordinator_handler_, nil);
    __block ProceduralBlock completion = nil;
    OCMExpect([mock_browser_coordinator_handler_
        clearPresentedStateWithCompletion:[OCMArg checkWithBlock:^BOOL(
                                                      ProceduralBlock block) {
          completion = [block copy];
          return YES;
        }]
                           dismissOmnibox:YES]);
    [coordinator_ showRemoteActorCredentialBottomSheetWithRequest:
                      MakeRemoteActorRequest()];
    EXPECT_OCMOCK_VERIFY(mock_browser_coordinator_handler_);
    return completion;
  }

  // Expects `mock_application_` to open the Gemini FAILED / UPLOAD_FAILED
  // callback URL for the default task ID.
  void ExpectGeminiFailureCallback() {
    NSURL* expected_url = [NSURL
        URLWithString:[NSString
                          stringWithFormat:@"%s?%s=task-123&%s=FAILED&%s="
                                           @"UPLOAD_FAILED",
                                           kRemoteActorGeminiCallbackBaseURL,
                                           kRemoteActorTaskIdQueryParam,
                                           kRemoteActorStatusQueryParam,
                                           kRemoteActorReasonQueryParam]];
    OCMExpect([mock_application_ openURL:expected_url
                                 options:@{}
                       completionHandler:[OCMArg any]]);
  }

  // Rejects any `openURL:` call on `mock_application_`.
  void RejectGeminiCallback() {
    OCMReject([mock_application_ openURL:[OCMArg any]
                                 options:[OCMArg any]
                       completionHandler:[OCMArg any]]);
  }

  // Builds a sample `RemoteActorPasswordUploadRequest` for `task_id`.
  remote_actor::proto::RemoteActorPasswordUploadRequest MakeRemoteActorRequest(
      std::string task_id = "task-123") {
    remote_actor::proto::RemoteActorPasswordUploadRequest request;
    request.set_task_id(std::move(task_id));
    request.set_gaia_id("gaia-id");
    request.set_domain("https://example.com");
    return request;
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
  std::optional<ScopedKeyWindow> scoped_key_window_;
  id mock_snackbar_handler_ = nil;
  id mock_bookmarks_handler_ = nil;
  id mock_browser_coordinator_handler_ = nil;
  id mock_application_ = nil;
  id partial_scene_coordinator_ = nil;
  RemoteActorCredentialBottomSheetCoordinator*
      started_remote_actor_coordinator_ = nil;
  std::optional<ScopedBlockSwizzler> remote_actor_start_swizzler_;
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

// Tests that the scene coordinator handles the scene-level commands of all the
// browsers it is given, and stops handling them when stopped.
TEST_F(SceneCoordinatorTest, HandlesSceneCommandsOfAllBrowsers) {
  Browser* regular_browser = coordinator_.currentBrowser;
  Browser* inactive_browser = regular_browser->GetInactiveBrowser();
  Browser* incognito_browser =
      scene_state_.browserProviderInterface.incognitoBrowser;

  EXPECT_TRUE(HandlesSceneCommands(coordinator_, regular_browser));
  EXPECT_TRUE(HandlesSceneCommands(coordinator_, inactive_browser));
  EXPECT_TRUE(HandlesSceneCommands(coordinator_, incognito_browser));
  EXPECT_TRUE(
      SigninModalHostHandlesSceneSignInCommands(coordinator_, regular_browser));
  EXPECT_TRUE(SigninModalHostHandlesSceneSignInCommands(coordinator_,
                                                        inactive_browser));
  EXPECT_TRUE(SigninModalHostHandlesSceneSignInCommands(coordinator_,
                                                        incognito_browser));

  [coordinator_ stop];
  EXPECT_FALSE(HandlesSceneCommands(coordinator_, regular_browser));
  EXPECT_FALSE(HandlesSceneCommands(coordinator_, inactive_browser));
  EXPECT_FALSE(HandlesSceneCommands(coordinator_, incognito_browser));
  EXPECT_FALSE(
      SigninModalHostHandlesSceneSignInCommands(coordinator_, regular_browser));
  EXPECT_FALSE(SigninModalHostHandlesSceneSignInCommands(coordinator_,
                                                         inactive_browser));
  EXPECT_FALSE(SigninModalHostHandlesSceneSignInCommands(coordinator_,
                                                         incognito_browser));
}

// Tests that disconnecting and connecting the incognito browser stops and
// starts the handling of its scene-level commands.
TEST_F(SceneCoordinatorTest, ConnectIncognitoBrowserHandlesSceneCommands) {
  Browser* incognito_browser =
      scene_state_.browserProviderInterface.incognitoBrowser;
  ASSERT_TRUE(HandlesSceneCommands(coordinator_, incognito_browser));
  ASSERT_TRUE(SigninModalHostHandlesSceneSignInCommands(coordinator_,
                                                        incognito_browser));

  [coordinator_ disconnectIncognitoBrowser];
  EXPECT_FALSE(HandlesSceneCommands(coordinator_, incognito_browser));
  EXPECT_FALSE(SigninModalHostHandlesSceneSignInCommands(coordinator_,
                                                         incognito_browser));

  [coordinator_ connectIncognitoBrowser:incognito_browser];
  EXPECT_TRUE(HandlesSceneCommands(coordinator_, incognito_browser));
  EXPECT_TRUE(SigninModalHostHandlesSceneSignInCommands(coordinator_,
                                                        incognito_browser));

  // Propagating the incognito browser does not register the handlers again.
  [coordinator_ propagateIncognitoBrowser];
  EXPECT_TRUE(HandlesSceneCommands(coordinator_, incognito_browser));
}

// Tests that SceneViewController hides the Gemini floaty when presenting a view
// controller.
TEST_F(SceneCoordinatorTest, PresentViewControllerHidesGeminiFloaty) {
  SceneViewController* scene_view_controller =
      [[SceneViewController alloc] init];
  scene_view_controller.layoutGuideCenter = [[LayoutGuideCenter alloc] init];
  id mock_delegate = OCMProtocolMock(@protocol(SceneViewControllerDelegate));
  scene_view_controller.delegate = mock_delegate;

  OCMExpect([mock_delegate
      sceneViewControllerHideGeminiFloatyIfInvoked:scene_view_controller]);

  [scene_view_controller presentViewController:[[UIViewController alloc] init]
                                      animated:NO
                                    completion:nil];

  EXPECT_OCMOCK_VERIFY(mock_delegate);
}

// Tests that dismissModalDialogsWithCompletion: completes without crashing and
// dismisses Gemini when bottom-sheet migration is disabled.
TEST_F(SceneCoordinatorTest, TestDismissModalDialogsWithCompletion) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kIOSGeminiBottomSheetMigration);

  ModalDismissalMocks mocks = SetUpModalDismissalMocks(coordinator_);
  OCMExpect([mocks.gemini_handler dismissGeminiFlowWithCompletion:nil]);
  [[mocks.gemini_handler reject] minimizeGeminiIfInvoked];

  __block bool completed = false;
  [coordinator_ dismissModalDialogsWithCompletion:^{
    completed = true;
  }];
  EXPECT_TRUE(completed);
  EXPECT_OCMOCK_VERIFY(mocks.gemini_handler);
}

// Tests that `dismissModalDialogsWithCompletion:` does not dismiss or minimize
// Gemini when `GeminiDismissalBehavior::kNone` is specified.
TEST_F(SceneCoordinatorTest,
       TestDismissModalDialogsWithCompletionDoesNotDismissGemini) {
  ModalDismissalMocks mocks = SetUpModalDismissalMocks(coordinator_);
  [[mocks.gemini_handler reject] dismissGeminiFlowWithCompletion:[OCMArg any]];
  [[mocks.gemini_handler reject] minimizeGeminiIfInvoked];

  __block bool completed = false;
  [coordinator_
      dismissModalDialogsWithCompletion:^{
        completed = true;
      }
                         dismissOmnibox:YES
                       dismissSnackbars:YES
                geminiDismissalBehavior:GeminiDismissalBehavior::kNone];
  EXPECT_TRUE(completed);
  EXPECT_OCMOCK_VERIFY(mocks.gemini_handler);
}

// Tests that `showRemoteActorCredentialBottomSheetWithRequest:` starts
// `RemoteActorCredentialBottomSheetCoordinator` and opens the Gemini callback
// URL with SUCCESS when the coordinator completes with `kSuccess`.
TEST_F(SceneCoordinatorTest,
       ShowRemoteActorCredentialBottomSheetSuccessCallback) {
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  SetUpDismissModalDialogsHandlers();
  ScopedKeyWindow scoped_key_window;
  scoped_key_window.Get().rootViewController = base_view_controller_;
  ASSERT_NE(base_view_controller_.view.window, nil);

  id mock_application = OCMClassMock([UIApplication class]);
  OCMStub([mock_application sharedApplication]).andReturn(mock_application);

  id partial_scene_coordinator = OCMPartialMock(coordinator_);
  OCMStub([partial_scene_coordinator activeViewController])
      .andReturn(base_view_controller_);

  __block RemoteActorCredentialBottomSheetCoordinator* started_coordinator =
      nil;
  auto start_swizzler = std::make_unique<ScopedBlockSwizzler>(
      [RemoteActorCredentialBottomSheetCoordinator class], @selector(start),
      ^(RemoteActorCredentialBottomSheetCoordinator* self_coordinator) {
        started_coordinator = self_coordinator;
      });

  [(id<SceneCommands>)coordinator_
      showRemoteActorCredentialBottomSheetWithRequest:MakeRemoteActorRequest()];
  ASSERT_NE(started_coordinator, nil);
  EXPECT_EQ([coordinator_
                valueForKey:@"_remoteActorCredentialBottomSheetCoordinator"],
            started_coordinator);

  RemoteActorCredentialBottomSheetCompletion completion =
      [started_coordinator valueForKey:@"completion"];
  ASSERT_NE(completion, nil);

  NSURL* expected_url = [NSURL
      URLWithString:[NSString
                        stringWithFormat:@"%s?%s=task-123&%s=SUCCESS",
                                         kRemoteActorGeminiCallbackBaseURL,
                                         kRemoteActorTaskIdQueryParam,
                                         kRemoteActorStatusQueryParam]];
  OCMExpect([mock_application openURL:expected_url
                              options:@{}
                    completionHandler:[OCMArg any]]);
  completion(password_manager::RemoteActorCredentialSharingResult::kSuccess);

  EXPECT_EQ([coordinator_
                valueForKey:@"_remoteActorCredentialBottomSheetCoordinator"],
            nil);
  EXPECT_OCMOCK_VERIFY(mock_application);

  [partial_scene_coordinator stopMocking];
  [mock_application stopMocking];
}

// Tests that `showRemoteActorCredentialBottomSheetWithRequest:` opens the
// Gemini callback URL with CANCELLED when the coordinator completes with
// `kUserCancelledDialog`.
TEST_F(SceneCoordinatorTest,
       ShowRemoteActorCredentialBottomSheetCancelledCallback) {
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  SetUpDismissModalDialogsHandlers();
  ScopedKeyWindow scoped_key_window;
  scoped_key_window.Get().rootViewController = base_view_controller_;
  ASSERT_NE(base_view_controller_.view.window, nil);

  id mock_application = OCMClassMock([UIApplication class]);
  OCMStub([mock_application sharedApplication]).andReturn(mock_application);

  id partial_scene_coordinator = OCMPartialMock(coordinator_);
  OCMStub([partial_scene_coordinator activeViewController])
      .andReturn(base_view_controller_);

  __block RemoteActorCredentialBottomSheetCoordinator* started_coordinator =
      nil;
  auto start_swizzler = std::make_unique<ScopedBlockSwizzler>(
      [RemoteActorCredentialBottomSheetCoordinator class], @selector(start),
      ^(RemoteActorCredentialBottomSheetCoordinator* self_coordinator) {
        started_coordinator = self_coordinator;
      });

  [(id<SceneCommands>)coordinator_
      showRemoteActorCredentialBottomSheetWithRequest:MakeRemoteActorRequest()];
  ASSERT_NE(started_coordinator, nil);

  RemoteActorCredentialBottomSheetCompletion completion =
      [started_coordinator valueForKey:@"completion"];
  ASSERT_NE(completion, nil);

  NSURL* expected_url = [NSURL
      URLWithString:[NSString
                        stringWithFormat:@"%s?%s=task-123&%s=CANCELLED",
                                         kRemoteActorGeminiCallbackBaseURL,
                                         kRemoteActorTaskIdQueryParam,
                                         kRemoteActorStatusQueryParam]];
  OCMExpect([mock_application openURL:expected_url
                              options:@{}
                    completionHandler:[OCMArg any]]);
  completion(password_manager::RemoteActorCredentialSharingResult::
                 kUserCancelledDialog);

  EXPECT_EQ([coordinator_
                valueForKey:@"_remoteActorCredentialBottomSheetCoordinator"],
            nil);
  EXPECT_OCMOCK_VERIFY(mock_application);

  [partial_scene_coordinator stopMocking];
  [mock_application stopMocking];
}

// Tests that `showRemoteActorCredentialBottomSheetWithRequest:` opens the
// Gemini callback URL with FAILED / UPLOAD_FAILED when the coordinator
// completes with an error result.
TEST_F(SceneCoordinatorTest,
       ShowRemoteActorCredentialBottomSheetFailureCallback) {
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  SetUpDismissModalDialogsHandlers();
  ScopedKeyWindow scoped_key_window;
  scoped_key_window.Get().rootViewController = base_view_controller_;
  ASSERT_NE(base_view_controller_.view.window, nil);

  id mock_application = OCMClassMock([UIApplication class]);
  OCMStub([mock_application sharedApplication]).andReturn(mock_application);

  id partial_scene_coordinator = OCMPartialMock(coordinator_);
  OCMStub([partial_scene_coordinator activeViewController])
      .andReturn(base_view_controller_);

  __block RemoteActorCredentialBottomSheetCoordinator* started_coordinator =
      nil;
  auto start_swizzler = std::make_unique<ScopedBlockSwizzler>(
      [RemoteActorCredentialBottomSheetCoordinator class], @selector(start),
      ^(RemoteActorCredentialBottomSheetCoordinator* self_coordinator) {
        started_coordinator = self_coordinator;
      });

  [(id<SceneCommands>)coordinator_
      showRemoteActorCredentialBottomSheetWithRequest:MakeRemoteActorRequest(
                                                          "task-456")];
  ASSERT_NE(started_coordinator, nil);

  RemoteActorCredentialBottomSheetCompletion completion =
      [started_coordinator valueForKey:@"completion"];
  ASSERT_NE(completion, nil);

  NSURL* expected_url = [NSURL
      URLWithString:[NSString stringWithFormat:
                                  @"%s?%s=task-456&%s=FAILED&%s=UPLOAD_FAILED",
                                  kRemoteActorGeminiCallbackBaseURL,
                                  kRemoteActorTaskIdQueryParam,
                                  kRemoteActorStatusQueryParam,
                                  kRemoteActorReasonQueryParam]];
  OCMExpect([mock_application openURL:expected_url
                              options:@{}
                    completionHandler:[OCMArg any]]);
  completion(
      password_manager::RemoteActorCredentialSharingResult::kSharingFailed);

  EXPECT_EQ([coordinator_
                valueForKey:@"_remoteActorCredentialBottomSheetCoordinator"],
            nil);
  EXPECT_OCMOCK_VERIFY(mock_application);

  [partial_scene_coordinator stopMocking];
  [mock_application stopMocking];
}

// Tests that `showRemoteActorCredentialBottomSheetWithRequest:` sends a failure
// callback via `OpenGeminiCallbackURL` when the bottom sheet cannot be
// presented (missing, detached, presenting, or dismissing base view controller,
// active incognito browser, or disconnected scene).
TEST_F(SceneCoordinatorTest,
       ShowRemoteActorCredentialBottomSheetFailsWhenUnavailable) {
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  SetUpDismissModalDialogsHandlers();
  ScopedKeyWindow scoped_key_window;

  id mock_application = OCMClassMock([UIApplication class]);
  OCMStub([mock_application sharedApplication]).andReturn(mock_application);

  __block RemoteActorCredentialBottomSheetCoordinator* started_coordinator =
      nil;
  auto start_swizzler = std::make_unique<ScopedBlockSwizzler>(
      [RemoteActorCredentialBottomSheetCoordinator class], @selector(start),
      ^(RemoteActorCredentialBottomSheetCoordinator* self_coordinator) {
        started_coordinator = self_coordinator;
      });

  const remote_actor::proto::RemoteActorPasswordUploadRequest request =
      MakeRemoteActorRequest("task-unavailable");

  NSURL* expected_url = [NSURL
      URLWithString:[NSString
                        stringWithFormat:@"%s?%s=task-unavailable&%s=FAILED&%s="
                                         @"UPLOAD_FAILED",
                                         kRemoteActorGeminiCallbackBaseURL,
                                         kRemoteActorTaskIdQueryParam,
                                         kRemoteActorStatusQueryParam,
                                         kRemoteActorReasonQueryParam]];

  // `activeViewController` is nil.
  OCMExpect([mock_application openURL:expected_url
                              options:@{}
                    completionHandler:[OCMArg any]]);
  [(id<SceneCommands>)coordinator_
      showRemoteActorCredentialBottomSheetWithRequest:request];
  EXPECT_EQ(started_coordinator, nil);
  EXPECT_OCMOCK_VERIFY(mock_application);

  // `activeViewController` is not in a window hierarchy.
  id partial_scene_coordinator = OCMPartialMock(coordinator_);
  OCMStub([partial_scene_coordinator activeViewController])
      .andReturn(base_view_controller_);
  ASSERT_EQ(base_view_controller_.view.window, nil);
  OCMExpect([mock_application openURL:expected_url
                              options:@{}
                    completionHandler:[OCMArg any]]);
  [(id<SceneCommands>)coordinator_
      showRemoteActorCredentialBottomSheetWithRequest:request];
  EXPECT_EQ(started_coordinator, nil);
  EXPECT_OCMOCK_VERIFY(mock_application);

  // `activeViewController` still has a presented view controller after modal
  // dismissal completes (e.g., dismissal timed out).
  scoped_key_window.Get().rootViewController = base_view_controller_;
  ASSERT_NE(base_view_controller_.view.window, nil);
  id partial_view_controller = OCMPartialMock(base_view_controller_);
  OCMStub([partial_view_controller presentedViewController])
      .andReturn([[UIViewController alloc] init]);
  OCMExpect([mock_application openURL:expected_url
                              options:@{}
                    completionHandler:[OCMArg any]]);
  [(id<SceneCommands>)coordinator_
      showRemoteActorCredentialBottomSheetWithRequest:request];
  EXPECT_EQ(started_coordinator, nil);
  EXPECT_OCMOCK_VERIFY(mock_application);
  [partial_view_controller stopMocking];

  // `activeViewController` is currently being dismissed.
  partial_view_controller = OCMPartialMock(base_view_controller_);
  OCMStub([partial_view_controller isBeingDismissed]).andReturn(YES);
  OCMExpect([mock_application openURL:expected_url
                              options:@{}
                    completionHandler:[OCMArg any]]);
  [coordinator_ showRemoteActorCredentialBottomSheetWithRequest:request];
  EXPECT_EQ(started_coordinator, nil);
  EXPECT_OCMOCK_VERIFY(mock_application);
  [partial_view_controller stopMocking];

  // Incognito browser is currently active.
  Browser* regular_browser = scene_state_.browserProviderInterface.mainBrowser;
  Browser* incognito_browser =
      scene_state_.browserProviderInterface.incognitoBrowser;
  [scene_state_ setCurrentBrowser:incognito_browser];
  OCMExpect([mock_application openURL:expected_url
                              options:@{}
                    completionHandler:[OCMArg any]]);
  [coordinator_ showRemoteActorCredentialBottomSheetWithRequest:request];
  EXPECT_EQ(started_coordinator, nil);
  EXPECT_OCMOCK_VERIFY(mock_application);
  [scene_state_ setCurrentBrowser:regular_browser];

  // Scene is disconnected.
  scene_state_.activationLevel = SceneActivationLevelDisconnected;
  OCMExpect([mock_application openURL:expected_url
                              options:@{}
                    completionHandler:[OCMArg any]]);
  [(id<SceneCommands>)coordinator_
      showRemoteActorCredentialBottomSheetWithRequest:request];
  EXPECT_EQ(started_coordinator, nil);
  EXPECT_OCMOCK_VERIFY(mock_application);

  [partial_scene_coordinator stopMocking];
  [mock_application stopMocking];
}

// Tests that `showRemoteActorCredentialBottomSheetWithRequest:` dismisses modal
// dialogs when UI is presented on `activeViewController`, and starts
// `RemoteActorCredentialBottomSheetCoordinator` once dismissal completes
// without triggering a failure callback.
TEST_F(SceneCoordinatorTest,
       ShowRemoteActorCredentialBottomSheetDismissesModalsBeforeStarting) {
  ASSERT_NO_FATAL_FAILURE(SetUpRemoteActorModalDismissalHarness());
  RejectGeminiCallback();

  [base_view_controller_ presentViewController:[[UIViewController alloc] init]
                                      animated:NO
                                    completion:nil];
  ASSERT_NE(base_view_controller_.presentedViewController, nil);

  ProceduralBlock dismissal_completion = TriggerDeferredRemoteActorRequest();
  ASSERT_NE(dismissal_completion, nil);
  EXPECT_EQ(started_remote_actor_coordinator_, nil);

  base::RunLoop run_loop;
  __block base::OnceClosure quit_closure = run_loop.QuitClosure();
  [base_view_controller_ dismissViewControllerAnimated:NO
                                            completion:^{
                                              dismissal_completion();
                                              std::move(quit_closure).Run();
                                            }];
  run_loop.Run();

  ASSERT_NE(started_remote_actor_coordinator_, nil);
  EXPECT_EQ([coordinator_
                valueForKey:@"_remoteActorCredentialBottomSheetCoordinator"],
            started_remote_actor_coordinator_);
  EXPECT_OCMOCK_VERIFY(mock_application_);
}

// Tests that `showRemoteActorCredentialBottomSheetWithRequest:` sends a failure
// callback via `OpenGeminiCallbackURL` if the scene switches to Incognito or
// disconnects while modal dialogs are dismissing.
TEST_F(SceneCoordinatorTest,
       ShowRemoteActorCredentialBottomSheetFailsIfUnavailableAfterDismissal) {
  ASSERT_NO_FATAL_FAILURE(SetUpRemoteActorModalDismissalHarness());

  Browser* regular_browser = scene_state_.browserProviderInterface.mainBrowser;
  Browser* incognito_browser =
      scene_state_.browserProviderInterface.incognitoBrowser;

  // Browser switches to Incognito while modal dialogs are dismissing.
  ProceduralBlock incognito_dismissal = TriggerDeferredRemoteActorRequest();
  ASSERT_NE(incognito_dismissal, nil);
  [scene_state_ setCurrentBrowser:incognito_browser];
  ExpectGeminiFailureCallback();
  incognito_dismissal();
  EXPECT_EQ(started_remote_actor_coordinator_, nil);
  EXPECT_OCMOCK_VERIFY(mock_application_);
  [scene_state_ setCurrentBrowser:regular_browser];

  // Scene disconnects while modal dialogs are dismissing.
  ProceduralBlock disconnect_dismissal = TriggerDeferredRemoteActorRequest();
  ASSERT_NE(disconnect_dismissal, nil);
  scene_state_.activationLevel = SceneActivationLevelDisconnected;
  ExpectGeminiFailureCallback();
  disconnect_dismissal();
  EXPECT_EQ(started_remote_actor_coordinator_, nil);
  EXPECT_OCMOCK_VERIFY(mock_application_);
}

// Tests that an in-flight Remote Actor modal dismissal is silently cancelled
// (without opening a callback URL or starting/disrupting a coordinator) when
// superseded by a newer request, when `closePresentedViews` is called (both
// asynchronously and synchronously during modal teardown), or when
// `SceneCoordinator` stops.
TEST_F(SceneCoordinatorTest,
       ShowRemoteActorCredentialBottomSheetCancelsInFlightRequest) {
  ASSERT_NO_FATAL_FAILURE(SetUpRemoteActorModalDismissalHarness());
  RejectGeminiCallback();

  // Superseded request: stale completion neither starts nor disrupts the sheet.
  ProceduralBlock stale_completion = TriggerDeferredRemoteActorRequest();
  ASSERT_NE(stale_completion, nil);
  ProceduralBlock latest_completion = TriggerDeferredRemoteActorRequest();
  ASSERT_NE(latest_completion, nil);

  stale_completion();
  EXPECT_EQ(started_remote_actor_coordinator_, nil);

  latest_completion();
  RemoteActorCredentialBottomSheetCoordinator* latest_coordinator =
      started_remote_actor_coordinator_;
  ASSERT_NE(latest_coordinator, nil);

  stale_completion();
  EXPECT_EQ(started_remote_actor_coordinator_, latest_coordinator);
  EXPECT_EQ([coordinator_
                valueForKey:@"_remoteActorCredentialBottomSheetCoordinator"],
            latest_coordinator);

  // `closePresentedViews` cancels an in-flight request.
  started_remote_actor_coordinator_ = nil;
  ProceduralBlock close_dismissal = TriggerDeferredRemoteActorRequest();
  ASSERT_NE(close_dismissal, nil);
  [coordinator_ closePresentedViews];
  close_dismissal();
  EXPECT_EQ(started_remote_actor_coordinator_, nil);

  // Synchronous re-entrant `closePresentedViews` during modal teardown cancels
  // the request because the one-shot preservation flag was already consumed by
  // the coordinator's own `-closePresentedViews:completion:` pass.
  __block ProceduralBlock reentrant_dismissal = nil;
  OCMExpect(
      [mock_browser_coordinator_handler_
          clearPresentedStateWithCompletion:[OCMArg checkWithBlock:^BOOL(
                                                        ProceduralBlock block) {
            reentrant_dismissal = [block copy];
            return YES;
          }]
                             dismissOmnibox:YES])
      .andDo(^(NSInvocation* invocation) {
        [coordinator_ closePresentedViews];
      });
  [coordinator_
      showRemoteActorCredentialBottomSheetWithRequest:MakeRemoteActorRequest()];
  EXPECT_OCMOCK_VERIFY(mock_browser_coordinator_handler_);
  ASSERT_NE(reentrant_dismissal, nil);
  reentrant_dismissal();
  EXPECT_EQ(started_remote_actor_coordinator_, nil);

  // `stop` cancels an in-flight request.
  ProceduralBlock stop_dismissal = TriggerDeferredRemoteActorRequest();
  ASSERT_NE(stop_dismissal, nil);
  [coordinator_ stop];
  stop_dismissal();
  EXPECT_EQ(started_remote_actor_coordinator_, nil);
  EXPECT_OCMOCK_VERIFY(mock_application_);
}

// Tests that stopping `SceneCoordinator` stops an active
// `RemoteActorCredentialBottomSheetCoordinator` without opening the Gemini
// callback URL.
TEST_F(SceneCoordinatorTest,
       StopStopsRemoteActorCredentialBottomSheetWithoutCallback) {
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  SetUpDismissModalDialogsHandlers();
  ScopedKeyWindow scoped_key_window;
  scoped_key_window.Get().rootViewController = base_view_controller_;
  ASSERT_NE(base_view_controller_.view.window, nil);

  id mock_application = OCMClassMock([UIApplication class]);
  OCMStub([mock_application sharedApplication]).andReturn(mock_application);

  id partial_scene_coordinator = OCMPartialMock(coordinator_);
  OCMStub([partial_scene_coordinator activeViewController])
      .andReturn(base_view_controller_);

  __block RemoteActorCredentialBottomSheetCoordinator* started_coordinator =
      nil;
  auto start_swizzler = std::make_unique<ScopedBlockSwizzler>(
      [RemoteActorCredentialBottomSheetCoordinator class], @selector(start),
      ^(RemoteActorCredentialBottomSheetCoordinator* self_coordinator) {
        started_coordinator = self_coordinator;
      });

  [(id<SceneCommands>)coordinator_
      showRemoteActorCredentialBottomSheetWithRequest:MakeRemoteActorRequest()];
  ASSERT_NE(started_coordinator, nil);
  EXPECT_EQ([coordinator_
                valueForKey:@"_remoteActorCredentialBottomSheetCoordinator"],
            started_coordinator);

  OCMReject([mock_application openURL:[OCMArg any]
                              options:[OCMArg any]
                    completionHandler:[OCMArg any]]);

  [coordinator_ stop];

  EXPECT_EQ([coordinator_
                valueForKey:@"_remoteActorCredentialBottomSheetCoordinator"],
            nil);
  EXPECT_OCMOCK_VERIFY(mock_application);

  [partial_scene_coordinator stopMocking];
  [mock_application stopMocking];
}

// Tests that closing presented views stops an active
// `RemoteActorCredentialBottomSheetCoordinator` without opening the Gemini
// callback URL.
TEST_F(SceneCoordinatorTest, ClosePresentedViewsStopsRemoteActorSheet) {
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  SetUpDismissModalDialogsHandlers();
  ScopedKeyWindow scoped_key_window;
  scoped_key_window.Get().rootViewController = base_view_controller_;
  ASSERT_NE(base_view_controller_.view.window, nil);

  id mock_application = OCMClassMock([UIApplication class]);
  OCMStub([mock_application sharedApplication]).andReturn(mock_application);

  id partial_scene_coordinator = OCMPartialMock(coordinator_);
  OCMStub([partial_scene_coordinator activeViewController])
      .andReturn(base_view_controller_);

  __block RemoteActorCredentialBottomSheetCoordinator* started_coordinator =
      nil;
  auto start_swizzler = std::make_unique<ScopedBlockSwizzler>(
      [RemoteActorCredentialBottomSheetCoordinator class], @selector(start),
      ^(RemoteActorCredentialBottomSheetCoordinator* self_coordinator) {
        started_coordinator = self_coordinator;
      });

  [(id<SceneCommands>)coordinator_
      showRemoteActorCredentialBottomSheetWithRequest:MakeRemoteActorRequest()];
  ASSERT_NE(started_coordinator, nil);
  EXPECT_EQ([coordinator_
                valueForKey:@"_remoteActorCredentialBottomSheetCoordinator"],
            started_coordinator);

  // `OpenGeminiCallbackURL()` notifies Gemini via `UIApplication` `openURL:`,
  // so rejecting it ensures no callback is sent when the sheet is torn down.
  OCMReject([mock_application openURL:[OCMArg any]
                              options:[OCMArg any]
                    completionHandler:[OCMArg any]]);

  [(id<SceneCommands>)coordinator_ closePresentedViews];

  EXPECT_EQ([coordinator_
                valueForKey:@"_remoteActorCredentialBottomSheetCoordinator"],
            nil);
  EXPECT_OCMOCK_VERIFY(mock_application);

  [partial_scene_coordinator stopMocking];
  [mock_application stopMocking];
}

// Tests that `showUndoSignoutFromSnackbarForIdentity:` starts the undo sign-out
// flow on the regular browser even when the incognito browser is current, as
// sign-in must never be performed on an incognito browser.
TEST_F(SceneCoordinatorTest,
       ShowUndoSignoutUsesRegularBrowserWhenIncognitoIsCurrent) {
  id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(identity);

  Browser* regular_browser = scene_state_.browserProviderInterface.mainBrowser;
  Browser* incognito_browser =
      scene_state_.browserProviderInterface.incognitoBrowser;
  [scene_state_ setCurrentBrowser:incognito_browser];
  ASSERT_EQ(coordinator_.currentBrowser, incognito_browser);

  id partial_scene_coordinator = OCMPartialMock(coordinator_);
  OCMStub([partial_scene_coordinator activeViewController])
      .andReturn(base_view_controller_);

  // Swizzles `start` so that no real `AuthenticationFlow` is created.
  __block UndoSignoutCoordinator* started_coordinator = nil;
  auto start_swizzler = std::make_unique<ScopedBlockSwizzler>(
      [UndoSignoutCoordinator class], @selector(start),
      ^(UndoSignoutCoordinator* self_coordinator) {
        started_coordinator = self_coordinator;
      });

  [(id<SceneCommands>)coordinator_
      showUndoSignoutFromSnackbarForIdentity:identity];

  ASSERT_NE(started_coordinator, nil);
  EXPECT_EQ([coordinator_ valueForKey:@"_undoSignoutCoordinator"],
            started_coordinator);
  EXPECT_EQ(started_coordinator.browser, regular_browser);
  EXPECT_EQ(started_coordinator.browser->type(), Browser::Type::kRegular);

  [partial_scene_coordinator stopMocking];
}

// Tests that `dismissModalDialogsWithCompletion:` minimizes Gemini instead of
// dismissing it when `IOSGeminiBottomSheetMigration` is enabled.
TEST_F(SceneCoordinatorTest,
       TestDismissModalDialogsWithCompletionMinimizesGeminiWhenMigrated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAssistantContainer, kIOSGeminiBottomSheetMigration}, {});

  ModalDismissalMocks mocks = SetUpModalDismissalMocks(coordinator_);
  [[mocks.gemini_handler reject] dismissGeminiFlowWithCompletion:[OCMArg any]];
  OCMExpect([mocks.gemini_handler minimizeGeminiIfInvoked]);

  __block bool completed = false;
  [coordinator_ dismissModalDialogsWithCompletion:^{
    completed = true;
  }];
  EXPECT_TRUE(completed);
  EXPECT_OCMOCK_VERIFY(mocks.gemini_handler);
}

// Tests that `dismissModalDialogsWithCompletion:` dismisses Gemini when
// `GeminiDismissalBehavior::kDismiss` is explicitly requested even if
// `IOSGeminiBottomSheetMigration` is enabled.
TEST_F(
    SceneCoordinatorTest,
    TestDismissModalDialogsWithCompletionDismissesGeminiWhenExplicitlyRequested) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAssistantContainer, kIOSGeminiBottomSheetMigration}, {});

  ModalDismissalMocks mocks = SetUpModalDismissalMocks(coordinator_);
  OCMExpect([mocks.gemini_handler dismissGeminiFlowWithCompletion:nil]);
  [[mocks.gemini_handler reject] minimizeGeminiIfInvoked];

  __block bool completed = false;
  [coordinator_
      dismissModalDialogsWithCompletion:^{
        completed = true;
      }
                         dismissOmnibox:YES
                       dismissSnackbars:YES
                geminiDismissalBehavior:GeminiDismissalBehavior::kDismiss];
  EXPECT_TRUE(completed);
  EXPECT_OCMOCK_VERIFY(mocks.gemini_handler);
}

}  // namespace
