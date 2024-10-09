// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"

#import "base/test/task_environment.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/supervised_user/test_support/kids_chrome_management_test_utils.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/intents/intents_constants.h"
#import "ios/chrome/browser/intents/user_activity_browser_agent.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/model/test_session_restoration_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller_testing.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_util_test_support.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/ui/main/wrangled_browser.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_data.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@interface InternalFakeSceneController : SceneController
// Browser and ProfileIOS used to mock the currentInterface.
@property(nonatomic, assign) Browser* browser;
@property(nonatomic, assign) ProfileIOS* profile;
// Mocked currentInterface.
@property(nonatomic, assign) WrangledBrowser* currentInterface;
// BrowserViewWrangler to provide test setup for main coordinator and interface.
@property(nonatomic, strong) BrowserViewWrangler* browserViewWrangler;
// Argument for
// -dismissModalsAndMaybeOpenSelectedTabInMode:withUrlLoadParams:dismissOmnibox:
//  completion:.
@property(nonatomic, readonly) ApplicationModeForTabOpening applicationMode;
@end

@implementation InternalFakeSceneController

- (BOOL)isIncognitoForced {
  return NO;
}

- (void)dismissModalsAndMaybeOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                                 withUrlLoadParams:
                                     (const UrlLoadParams&)urlLoadParams
                                    dismissOmnibox:(BOOL)dismissOmnibox
                                        completion:(ProceduralBlock)completion {
  _applicationMode = targetMode;
}

- (BOOL)URLIsOpenedInRegularMode:(const GURL&)URL {
  return NO;
}
@end

namespace {

class SceneControllerTest : public PlatformTest {
 protected:
  SceneControllerTest() {
    base_view_controller_ = [[UIViewController alloc] init];

    // Required because UserActivityBrowserAgent uses the AppState's initStage
    // instead of ProfileState's initStage. Remove once this is no longer the
    // case.
    app_state_ = CreateMockAppState(AppInitStage::kFinal);

    fake_scene_ = FakeSceneWithIdentifier([[NSUUID UUID] UUIDString]);
    scene_state_ = [[SceneStateWithFakeScene alloc] initWithScene:fake_scene_
                                                         appState:app_state_];

    profile_state_ = CreateMockProfileState(ProfileInitStage::kFinal);
    scene_state_.profileState = profile_state_;

    scene_controller_ =
        [[InternalFakeSceneController alloc] initWithSceneState:scene_state_];
    scene_state_.controller = scene_controller_;

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(PrerenderServiceFactory::GetInstance(),
                              PrerenderServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        SendTabToSelfSyncServiceFactory::GetInstance(),
        SendTabToSelfSyncServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(ios::FaviconServiceFactory::GetInstance(),
                              ios::FaviconServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                              ios::HistoryServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(ios::BookmarkModelFactory::GetInstance(),
                              ios::BookmarkModelFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        SessionRestorationServiceFactory::GetInstance(),
        TestSessionRestorationService::GetTestingFactory());
    profile_ = std::move(builder).Build();

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());

    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    UserActivityBrowserAgent::CreateForBrowser(browser_.get());

    profile_->SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_loader_factory_));

    scene_controller_.browserViewWrangler =
        [[BrowserViewWrangler alloc] initWithProfile:profile_.get()
                                          sceneState:scene_state_
                                 applicationEndpoint:nil
                                    settingsEndpoint:nil];
    [scene_controller_.browserViewWrangler createMainCoordinatorAndInterface];

    scene_controller_.browser = browser_.get();
    scene_controller_.profile = profile_.get();
    scene_controller_.currentInterface = CreateMockCurrentInterface();
    connection_information_ = scene_state_.controller;
  }

  ~SceneControllerTest() override { [scene_controller_ teardownUI]; }

  // Mock & stub an AppState object with an arbitrary `init_stage` property.
  AppState* CreateMockAppState(AppInitStage init_stage) {
    AppState* mock_app_state = OCMClassMock([AppState class]);
    OCMStub([mock_app_state initStage]).andReturn(init_stage);
    return mock_app_state;
  }

  // Mock & stub a ProfileState object with an arbitrary `init_stage` property.
  ProfileState* CreateMockProfileState(ProfileInitStage init_stage) {
    ProfileState* mock_profile_state = OCMClassMock([ProfileState class]);
    OCMStub([mock_profile_state initStage]).andReturn(init_stage);
    OCMStub([mock_profile_state profile]).andReturn(profile_.get());
    return mock_profile_state;
  }

  // Mock & stub a WrangledBrowser object.
  id CreateMockCurrentInterface() {
    id mock_wrangled_browser = OCMClassMock(WrangledBrowser.class);
    OCMStub([mock_wrangled_browser profile]).andReturn(profile_.get());
    OCMStub([mock_wrangled_browser browser]).andReturn(browser_.get());
    return mock_wrangled_browser;
  }

  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(profile_.get());
  }

  void MakePrimaryAccountAvailable(const std::string& email) {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile_.get()), email,
        signin::ConsentLevel::kSignin);
  }

  void SetAutomaticIssueOfAccessTokens(bool grant) {
    signin::SetAutomaticIssueOfAccessTokens(GetIdentityManager(), grant);
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestProfileIOS> profile_;
  InternalFakeSceneController* scene_controller_;
  AppState* app_state_;
  SceneState* scene_state_;
  ProfileState* profile_state_;
  id fake_scene_;
  id<ConnectionInformation> connection_information_;

  UIViewController* base_view_controller_;
  network::TestURLLoaderFactory test_loader_factory_;
};

// TODO(crbug.com/40693350): Add a test for keeping validity of detecting a
// fresh open in new window coming from ios dock. 'Dock' is considered the
// default when the new window opening request is external to chrome and
// unknown.

// Tests that scene controller updates scene state's incognitoContentVisible
// when the relevant application command is called.
TEST_F(SceneControllerTest, UpdatesIncognitoContentVisibility) {
  [scene_controller_ setIncognitoContentVisible:NO];
  EXPECT_FALSE(scene_state_.incognitoContentVisible);
  [scene_controller_ setIncognitoContentVisible:YES];
  EXPECT_TRUE(scene_state_.incognitoContentVisible);
  [scene_controller_ setIncognitoContentVisible:NO];
  EXPECT_FALSE(scene_state_.incognitoContentVisible);
}

// Tests that scene controller correctly handles an external intent to
// OpenIncognitoSearch.
// TODO(crbug.com/40947630): re-enabled the test.
TEST_F(SceneControllerTest, DISABLED_TestOpenIncognitoSearchForShortcutItem) {
  UIApplicationShortcutItem* shortcut = [[UIApplicationShortcutItem alloc]
        initWithType:kShortcutNewIncognitoSearch
      localizedTitle:kShortcutNewIncognitoSearch];
  [scene_controller_ performActionForShortcutItem:shortcut
                                completionHandler:nil];
  EXPECT_TRUE(scene_state_.startupHadExternalIntent);
  EXPECT_EQ(ApplicationModeForTabOpening::INCOGNITO,
            [scene_controller_ applicationMode]);
  EXPECT_EQ(FOCUS_OMNIBOX,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that scene controller correctly handles an external intent to
// OpenNewSearch.
TEST_F(SceneControllerTest, TestOpenNewSearchForShortcutItem) {
  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:kShortcutNewSearch
                                       localizedTitle:kShortcutNewSearch];
  [scene_controller_ performActionForShortcutItem:shortcut
                                completionHandler:nil];
  EXPECT_TRUE(scene_state_.startupHadExternalIntent);
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL,
            [scene_controller_ applicationMode]);
  EXPECT_EQ(FOCUS_OMNIBOX,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that scene controller correctly handles an external intent to
// OpenVoiceSearch.
TEST_F(SceneControllerTest, TestOpenVoiceSearchForShortcutItem) {
  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:kShortcutVoiceSearch
                                       localizedTitle:kShortcutVoiceSearch];
  [scene_controller_ performActionForShortcutItem:shortcut
                                completionHandler:nil];
  EXPECT_TRUE(scene_state_.startupHadExternalIntent);
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL,
            [scene_controller_ applicationMode]);
  EXPECT_EQ(START_VOICE_SEARCH,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that scene controller correctly handles an external intent to
// OpenQRScanner.
TEST_F(SceneControllerTest, TestOpenQRScannerForShortcutItem) {
  UIApplicationShortcutItem* shortcut =
      [[UIApplicationShortcutItem alloc] initWithType:kShortcutQRScanner
                                       localizedTitle:kShortcutQRScanner];
  [scene_controller_ performActionForShortcutItem:shortcut
                                completionHandler:nil];
  EXPECT_TRUE(scene_state_.startupHadExternalIntent);
  EXPECT_EQ(ApplicationModeForTabOpening::NORMAL,
            [scene_controller_ applicationMode]);
  EXPECT_EQ(START_QR_CODE_SCANNER,
            [connection_information_ startupParameters].postOpeningAction);
}

// Tests that "Report an issue" populates user feedback data with available
// information on the family member role for the signed-in user.
TEST_F(SceneControllerTest, TestReportAnIssueViewControllerWithFamilyResponse) {
  MakePrimaryAccountAvailable("foo@gmail.com");
  SetAutomaticIssueOfAccessTokens(/*grant=*/true);

  base::RunLoop run_loop;
  UserFeedbackDataCallback completion =
      base::BindRepeating(^(UserFeedbackData* data) {
        EXPECT_EQ(UserFeedbackSender::ToolsMenu, data.origin);
        EXPECT_FALSE(data.currentPageIsIncognito);
        EXPECT_NSEQ(data.familyMemberRole, @"child");
      }).Then(run_loop.QuitClosure());

  [scene_controller_
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
TEST_F(SceneControllerTest, TestReportAnIssueViewControllerForSignedInUser) {
  MakePrimaryAccountAvailable("foo@gmail.com");

  base::RunLoop run_loop;
  UserFeedbackDataCallback completion =
      base::BindRepeating(^(UserFeedbackData* data) {
        EXPECT_EQ(UserFeedbackSender::ToolsMenu, data.origin);
        EXPECT_FALSE(data.currentPageIsIncognito);
        EXPECT_EQ(nil, data.familyMemberRole);
      }).Then(run_loop.QuitClosure());

  [scene_controller_
      presentReportAnIssueViewController:base_view_controller_
                                  sender:UserFeedbackSender::ToolsMenu
                        userFeedbackData:[[UserFeedbackData alloc] init]
                                 timeout:base::Seconds(0)
                              completion:std::move(completion)];
  run_loop.Run();
}

// Tests that "Report an issue" populates user feedback data for signed-out
// user.
TEST_F(SceneControllerTest, TestReportAnIssueViewControllerForSignedOutUser) {
  base::RunLoop run_loop;
  UserFeedbackDataCallback completion =
      base::BindRepeating(^(UserFeedbackData* data) {
        EXPECT_EQ(UserFeedbackSender::ToolsMenu, data.origin);
        EXPECT_FALSE(data.currentPageIsIncognito);
        EXPECT_EQ(nil, data.familyMemberRole);
      }).Then(run_loop.QuitClosure());

  [scene_controller_
      presentReportAnIssueViewController:base_view_controller_
                                  sender:UserFeedbackSender::ToolsMenu
                        userFeedbackData:[[UserFeedbackData alloc] init]
                                 timeout:base::Seconds(1)
                              completion:std::move(completion)];
  run_loop.Run();
}

}  // namespace
