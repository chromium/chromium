// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"

#import "base/check.h"
#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/group_data.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_flow_configuration.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {
std::unique_ptr<KeyedService> BuildTestShareKitService(
    web::BrowserState* context) {
  ProfileIOS* profile = static_cast<ProfileIOS*>(context);
  data_sharing::DataSharingService* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);

  return std::make_unique<TestShareKitService>(data_sharing_service, nullptr,
                                               nullptr);
}

std::unique_ptr<KeyedService> BuildFakeTabGroupSyncService(
    web::BrowserState* context) {
  return std::make_unique<tab_groups::FakeTabGroupSyncService>();
}
}  // namespace

namespace collaboration {

// Test fixture for the iOS collaboration controller delegate.
class IOSCollaborationControllerDelegateTest : public PlatformTest {
 protected:
  IOSCollaborationControllerDelegateTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            kTabGroupSync,
            kTabGroupsIPad,
            kModernTabStrip,
            data_sharing::features::kDataSharingFeature,
        },
        /*disable_features=*/{});

    // Init the delegate parameters.
    TestProfileIOS::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    test_cbs_builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeTabGroupSyncService));
    test_cbs_builder.AddTestingFactory(
        ShareKitServiceFactory::GetInstance(),
        base::BindRepeating(&BuildTestShareKitService));
    profile_ = std::move(test_cbs_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    web_state_list_ = browser_->GetWebStateList();
    web_state_list_->InsertWebState(std::make_unique<web::FakeWebState>());
    web_state_list_->InsertWebState(std::make_unique<web::FakeWebState>());
    web_state_list_->InsertWebState(std::make_unique<web::FakeWebState>());

    web_state_list_->CreateGroup({1}, {},
                                 tab_groups::TabGroupId::GenerateNew());

    tab_group_ = web_state_list_->GetGroupOfWebStateAt(1);

    tab_groups::SavedTabGroup saved_group =
        tab_groups::test::CreateTestSavedTabGroup();
    saved_group.SetLocalGroupId(tab_group_->tab_group_id());
    tab_group_sync_service_ =
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile_.get());

    tab_group_sync_service_->AddGroup(saved_group);

    CommandDispatcher* command_dispatcher = browser_->GetCommandDispatcher();
    application_commands_mock_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [command_dispatcher
        startDispatchingToTarget:application_commands_mock_
                     forProtocol:@protocol(ApplicationCommands)];
    share_kit_service_ = ShareKitServiceFactory::GetForProfile(profile_.get());
    base_view_controller_ = [[FakeUIViewController alloc] init];
  }

  // Init the delegate for a share flow.
  void InitShareFlowDelegate() {
    delegate_ = std::make_unique<IOSCollaborationControllerDelegate>(
        browser_.get(), base_view_controller_,
        std::make_unique<CollaborationFlowConfigurationShareOrManage>());
  }

  // Init the delegate for a join flow.
  void InitJoinFlowDelegate() {
    delegate_ = std::make_unique<IOSCollaborationControllerDelegate>(
        browser_.get(), base_view_controller_,
        std::make_unique<CollaborationFlowConfigurationJoin>());
  }

  // Sign in in the authentication service with a fake identity.
  void SignIn() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationServiceFactory::GetForProfile(profile_.get())
        ->SignIn(identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;
  std::unique_ptr<IOSCollaborationControllerDelegate> delegate_;
  raw_ptr<WebStateList> web_state_list_;
  id<ApplicationCommands> application_commands_mock_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestProfileIOS> profile_;
  UIViewController* base_view_controller_;
  raw_ptr<const TabGroup> tab_group_;
  raw_ptr<ShareKitService> share_kit_service_;
};

// Tests `ShowShareDialog` with a valid tabGroup.
TEST_F(IOSCollaborationControllerDelegateTest, ShowShareDialogValid) {
  InitShareFlowDelegate();
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      completion_callback;
  delegate_->ShowShareDialog(tab_group_->tab_group_id(),
                             completion_callback.Get());
  EXPECT_TRUE(base_view_controller_.presentedViewController);
  // The callback is not expected to be called, as it is called when the
  // given ShareKit flow returns, i.e. when the presented view controller is
  // dismissed. Here, it's not dismissed.
}

// Tests `ShowShareDialog` with an invalid tabGroup.
TEST_F(IOSCollaborationControllerDelegateTest, ShowShareDialogInvalid) {
  InitShareFlowDelegate();

  tab_groups::TabGroupId tab_group_id = tab_group_->tab_group_id();

  // Delete the tabGroup.
  web_state_list_->DeleteGroup(tab_group_);

  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      completion_callback;
  EXPECT_CALL(completion_callback,
              Run(CollaborationControllerDelegate::Outcome::kFailure));
  delegate_->ShowShareDialog(tab_group_id, completion_callback.Get());
  EXPECT_FALSE(base_view_controller_.presentedViewController);
}

// Tests `ShowJoinDialog`.
TEST_F(IOSCollaborationControllerDelegateTest, ShowJoinDialog) {
  InitJoinFlowDelegate();
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      completion_callback;
  data_sharing::SharedDataPreview preview_data;
  delegate_->ShowJoinDialog(data_sharing::GroupToken(), preview_data,
                            completion_callback.Get());
  EXPECT_TRUE(base_view_controller_.presentedViewController);
  // The callback is not expected to be called, as it is called when the
  // given ShareKit flow returns, i.e. when the presented view controller is
  // dismissed. Here, it's not dismissed.
}

// Tests `ShowAuthenticationUi` from a share flow.
TEST_F(IOSCollaborationControllerDelegateTest, ShowAuthenticationUiShareFlow) {
  InitShareFlowDelegate();
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      completion_callback;
  OCMExpect([application_commands_mock_
              showSignin:[OCMArg checkWithBlock:^BOOL(
                                     ShowSigninCommand* command) {
                return command.operation ==
                       AuthenticationOperation::kSheetSigninAndHistorySync;
              }]
      baseViewController:base_view_controller_]);
  delegate_->ShowAuthenticationUi(completion_callback.Get());
}

// Tests `ShowAuthenticationUi` from a join flow.
TEST_F(IOSCollaborationControllerDelegateTest, ShowAuthenticationUiJoinFlow) {
  InitJoinFlowDelegate();
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      completion_callback;
  OCMExpect([application_commands_mock_
              showSignin:[OCMArg checkWithBlock:^BOOL(
                                     ShowSigninCommand* command) {
                return command.operation ==
                       AuthenticationOperation::kSheetSigninAndHistorySync;
              }]
      baseViewController:base_view_controller_]);
  delegate_->ShowAuthenticationUi(completion_callback.Get());
}

// Tests `ShowAuthenticationUi` from a join flow when being SignedIn only.
TEST_F(IOSCollaborationControllerDelegateTest,
       ShowAuthenticationUiSyncJoinFlow) {
  SignIn();
  InitJoinFlowDelegate();
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      completion_callback;
  OCMExpect([application_commands_mock_
              showSignin:[OCMArg
                             checkWithBlock:^BOOL(ShowSigninCommand* command) {
                               return command.operation ==
                                      AuthenticationOperation::kHistorySync;
                             }]
      baseViewController:base_view_controller_]);
  delegate_->ShowAuthenticationUi(completion_callback.Get());
}

// Tests `NotifySignInAndSyncStatusChange`.
TEST_F(IOSCollaborationControllerDelegateTest,
       NotifySignInAndSyncStatusChange) {
  InitJoinFlowDelegate();
  delegate_->NotifySignInAndSyncStatusChange();
}

}  // namespace collaboration
