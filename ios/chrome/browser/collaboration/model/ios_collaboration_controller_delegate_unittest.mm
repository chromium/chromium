// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"

#import "base/check.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "components/collaboration/test_support/mock_collaboration_service.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/group_data.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/test_favicon_loader.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/fake_share_kit_flow_view_controller.h"
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
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using testing::_;
using testing::Return;

namespace collaboration {
namespace {

std::unique_ptr<KeyedService> BuildTestShareKitService(ProfileIOS* profile) {
  data_sharing::DataSharingService* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);
  TabGroupService* tab_group_service =
      TabGroupServiceFactory::GetForProfile(profile);

  return std::make_unique<TestShareKitService>(data_sharing_service, nullptr,
                                               nullptr, tab_group_service);
}

std::unique_ptr<KeyedService> BuildFakeTabGroupSyncService(
    ProfileIOS* profile) {
  return std::make_unique<tab_groups::FakeTabGroupSyncService>();
}

std::unique_ptr<KeyedService> BuildTestSyncService(ProfileIOS* profile) {
  return std::make_unique<syncer::TestSyncService>();
}

std::unique_ptr<KeyedService> BuildMockCollaborationService(
    ProfileIOS* profile) {
  ServiceStatus collaboration_status;
  collaboration_status.collaboration_status =
      CollaborationStatus::kEnabledCreateAndJoin;
  std::unique_ptr<MockCollaborationService> mock_collaboration_service =
      std::make_unique<MockCollaborationService>();
  ON_CALL(*mock_collaboration_service.get(), GetServiceStatus())
      .WillByDefault(Return(collaboration_status));
  return std::move(mock_collaboration_service);
}

std::unique_ptr<KeyedService> BuildTestFaviconLoader(ProfileIOS* profile) {
  return std::make_unique<TestFaviconLoader>();
}

}  // namespace

// Test fixture for the iOS collaboration controller delegate.
class IOSCollaborationControllerDelegateTest : public PlatformTest {
 protected:
  IOSCollaborationControllerDelegateTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            data_sharing::features::kDataSharingFeature,
        },
        /*disable_features=*/{});

    // Init the delegate parameters.
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    test_profile_builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildTestSyncService));
    test_profile_builder.AddTestingFactory(
        CollaborationServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockCollaborationService));
    test_profile_builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeTabGroupSyncService));
    test_profile_builder.AddTestingFactory(
        ShareKitServiceFactory::GetInstance(),
        base::BindRepeating(&BuildTestShareKitService));
    test_profile_builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        base::BindRepeating(&BuildTestFaviconLoader));

    profile_ = std::move(test_profile_builder).Build();
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
    signin_coordinator_mock_ = OCMStrictClassMock([SigninCoordinator class]);
    signin_coordinator_class_mock_ =
        OCMStrictClassMock([SigninCoordinator class]);
    share_kit_service_ = ShareKitServiceFactory::GetForProfile(profile_.get());
    base_view_controller_ = [[FakeUIViewController alloc] init];

    mock_collaboration_service_ = static_cast<MockCollaborationService*>(
        CollaborationServiceFactory::GetForProfile(profile_.get()));

    collaboration_status_.sync_status = SyncStatus::kSyncWithoutTabGroup;
  }

  // Init the delegate for a `flow_type` flow.
  void InitDelegate(FlowType flow_type) {
    delegate_ = std::make_unique<IOSCollaborationControllerDelegate>(
        browser_.get(), CreateControllerDelegateParamsFromProfile(
                            profile_.get(), base_view_controller_, flow_type));
  }

  // Sign in in the authentication service with a fake identity.
  void SignIn() {
    collaboration_status_.signin_status = SigninStatus::kSignedIn;
    ON_CALL(*mock_collaboration_service_, GetServiceStatus())
        .WillByDefault(Return(collaboration_status_));

    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationServiceFactory::GetForProfile(profile_.get())
        ->SignIn(identity, signin_metrics::AccessPoint::kUnknown);
  }

  // Updates the selected types to pretend that the user accepted to sync
  // everything.
  void AcceptSyncOptIn() {
    collaboration_status_.sync_status = SyncStatus::kSyncEnabled;
    ON_CALL(*mock_collaboration_service_, GetServiceStatus())
        .WillByDefault(Return(collaboration_status_));

    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForProfile(browser_->GetProfile());
    syncer::SyncUserSettings* user_settings = sync_service->GetUserSettings();
    user_settings->SetSelectedTypes(/*sync_everything=*/true,
                                    /*types=*/syncer::UserSelectableTypeSet());
  }

  // Updates the selected types to pretend that the user refused to sync
  // anything.
  void DenySyncOptIn() {
    collaboration_status_.sync_status = SyncStatus::kSyncWithoutTabGroup;
    ON_CALL(*mock_collaboration_service_, GetServiceStatus())
        .WillByDefault(Return(collaboration_status_));

    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForProfile(browser_->GetProfile());
    syncer::SyncUserSettings* user_settings = sync_service->GetUserSettings();
    user_settings->SetSelectedTypes(/*sync_everything=*/false,
                                    /*types=*/syncer::UserSelectableTypeSet());
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(application_commands_mock_);
    EXPECT_OCMOCK_VERIFY((id)signin_coordinator_mock_);
    EXPECT_OCMOCK_VERIFY(signin_coordinator_class_mock_);
    PlatformTest::TearDown();
  }

  // Returns a FakeShareKitFlowViewController if it is what is presented by
  // `base_view_controller`. Returns nil otherwise.
  FakeShareKitFlowViewController* ShareKitFlowFromBaseViewController(
      UIViewController* base_view_controller) {
    UIViewController* presented_view_controller =
        base_view_controller.presentedViewController;
    if (![presented_view_controller
            isKindOfClass:UINavigationController.class]) {
      return nil;
    }
    UINavigationController* navigation_controller =
        reinterpret_cast<UINavigationController*>(presented_view_controller);
    if (![[navigation_controller topViewController]
            isKindOfClass:FakeShareKitFlowViewController.class]) {
      return nil;
    }
    return reinterpret_cast<FakeShareKitFlowViewController*>(
        [navigation_controller topViewController]);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<tab_groups::TabGroupSyncService, DanglingUntriaged>
      tab_group_sync_service_;
  raw_ptr<MockCollaborationService, DanglingUntriaged>
      mock_collaboration_service_;
  std::unique_ptr<IOSCollaborationControllerDelegate> delegate_;
  raw_ptr<WebStateList, DanglingUntriaged> web_state_list_;
  id signin_coordinator_class_mock_;
  SigninCoordinator* signin_coordinator_mock_;
  id application_commands_mock_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestProfileIOS> profile_;
  UIViewController* base_view_controller_;
  raw_ptr<const TabGroup, DanglingUntriaged> tab_group_;
  raw_ptr<ShareKitService> share_kit_service_;
  ServiceStatus collaboration_status_;
};

// Tests `ShowShareDialog` with a valid tabGroup.
TEST_F(IOSCollaborationControllerDelegateTest, ShowShareDialogValid) {
  InitDelegate(FlowType::kShareOrManage);
  base::MockCallback<
      CollaborationControllerDelegate::ResultWithGroupTokenCallback>
      mock_callback;
  EXPECT_CALL(
      mock_callback,
      Run(CollaborationControllerDelegate::Outcome::kSuccess, testing::_));

  delegate_->ShowShareDialog(tab_group_->tab_group_id(), mock_callback.Get());

  FakeShareKitFlowViewController* share_kit_flow_view_controller =
      ShareKitFlowFromBaseViewController(base_view_controller_);
  EXPECT_TRUE(share_kit_flow_view_controller);

  [share_kit_flow_view_controller accept];
}

// Tests `ShowShareDialog` with an invalid tabGroup.
TEST_F(IOSCollaborationControllerDelegateTest, ShowShareDialogInvalid) {
  InitDelegate(FlowType::kShareOrManage);

  tab_groups::TabGroupId tab_group_id = tab_group_->tab_group_id();

  // Delete the tabGroup.
  web_state_list_->DeleteGroup(tab_group_);

  base::MockCallback<
      CollaborationControllerDelegate::ResultWithGroupTokenCallback>
      mock_callback;
  EXPECT_CALL(
      mock_callback,
      Run(CollaborationControllerDelegate::Outcome::kFailure, testing::_));
  delegate_->ShowShareDialog(tab_group_id, mock_callback.Get());
  EXPECT_FALSE(base_view_controller_.presentedViewController);
}

// Tests `ShowJoinDialog` and accept.
TEST_F(IOSCollaborationControllerDelegateTest, ShowJoinDialogAccept) {
  InitDelegate(FlowType::kJoin);
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      mock_callback;
  EXPECT_CALL(mock_callback,
              Run(CollaborationControllerDelegate::Outcome::kSuccess));

  data_sharing::SharedDataPreview preview_data;
  delegate_->ShowJoinDialog(data_sharing::GroupToken(), preview_data,
                            mock_callback.Get());

  FakeShareKitFlowViewController* share_kit_flow_view_controller =
      ShareKitFlowFromBaseViewController(base_view_controller_);
  EXPECT_TRUE(share_kit_flow_view_controller);

  [share_kit_flow_view_controller accept];
}

// Tests `ShowJoinDialog` and cancel.
TEST_F(IOSCollaborationControllerDelegateTest, ShowJoinDialogCancel) {
  InitDelegate(FlowType::kShareOrManage);
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      mock_callback;
  EXPECT_CALL(mock_callback,
              Run(CollaborationControllerDelegate::Outcome::kCancel));

  data_sharing::SharedDataPreview preview_data;
  delegate_->ShowJoinDialog(data_sharing::GroupToken(), preview_data,
                            mock_callback.Get());

  FakeShareKitFlowViewController* share_kit_flow_view_controller =
      ShareKitFlowFromBaseViewController(base_view_controller_);
  EXPECT_TRUE(share_kit_flow_view_controller);

  [share_kit_flow_view_controller cancel];
}

// Tests `ShowManageDialog` and accept.
TEST_F(IOSCollaborationControllerDelegateTest, ShowManageDialogAccept) {
  InitDelegate(FlowType::kShareOrManage);
  // Share the group.
  tab_group_sync_service_->MakeTabGroupShared(
      tab_group_->tab_group_id(), syncer::CollaborationId("collaboration"),
      tab_groups::TabGroupSyncService::TabGroupSharingCallback());
  // Prepare the callback.
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      mock_callback;
  EXPECT_CALL(mock_callback,
              Run(CollaborationControllerDelegate::Outcome::kSuccess));

  delegate_->ShowManageDialog(tab_group_->tab_group_id(), mock_callback.Get());

  FakeShareKitFlowViewController* share_kit_flow_view_controller =
      ShareKitFlowFromBaseViewController(base_view_controller_);
  EXPECT_TRUE(share_kit_flow_view_controller);

  [share_kit_flow_view_controller accept];
}

// Tests `ShowManageDialog` and cancel.
TEST_F(IOSCollaborationControllerDelegateTest, ShowManageDialogCancel) {
  InitDelegate(FlowType::kShareOrManage);
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      mock_callback;
  // Share the group.
  tab_group_sync_service_->MakeTabGroupShared(
      tab_group_->tab_group_id(), syncer::CollaborationId("collaboration"),
      tab_groups::TabGroupSyncService::TabGroupSharingCallback());
  // Prepare the callback.
  EXPECT_CALL(mock_callback,
              Run(CollaborationControllerDelegate::Outcome::kCancel));

  delegate_->ShowManageDialog(tab_group_->tab_group_id(), mock_callback.Get());

  FakeShareKitFlowViewController* share_kit_flow_view_controller =
      ShareKitFlowFromBaseViewController(base_view_controller_);
  EXPECT_TRUE(share_kit_flow_view_controller);

  [share_kit_flow_view_controller cancel];
}

// Tests `ShowAuthenticationUi` when the user chooses to cancel the sign in.
TEST_F(IOSCollaborationControllerDelegateTest,
       ShowAuthenticationUiSignInCanceled) {
  InitDelegate(FlowType::kJoin);
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      mock_callback;

  EXPECT_CALL(mock_callback,
              Run(CollaborationControllerDelegate::Outcome::kCancel));
  __block SigninCoordinatorCompletionCallback completion;

  OCMExpect(
      [signin_coordinator_class_mock_
          signinCoordinatorWithCommand:[OCMArg checkWithBlock:^BOOL(
                                                   ShowSigninCommand* command) {
            completion = command.completion;
            return command.operation ==
                   AuthenticationOperation::kSheetSigninAndHistorySync;
          }]
                               browser:browser_.get()
                    baseViewController:base_view_controller_])
      .andReturn(signin_coordinator_mock_);

  OCMExpect([signin_coordinator_mock_ start]).andDo(^(NSInvocation*) {
    completion(signin_coordinator_mock_, SigninCoordinatorResultCanceledByUser,
               nil);
  });
  OCMExpect([signin_coordinator_mock_ stop]);
  delegate_->ShowAuthenticationUi(FlowType::kJoin, mock_callback.Get());
}

// Tests `ShowAuthenticationUi` when the user sign in and accept the sync opt
// in.
TEST_F(IOSCollaborationControllerDelegateTest,
       ShowAuthenticationUiSyncAccepted) {
  InitDelegate(FlowType::kJoin);
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      mock_callback;

  EXPECT_CALL(mock_callback,
              Run(CollaborationControllerDelegate::Outcome::kSuccess));

  __block SigninCoordinatorCompletionCallback completion;
  OCMExpect(
      [signin_coordinator_class_mock_
          signinCoordinatorWithCommand:[OCMArg checkWithBlock:^BOOL(
                                                   ShowSigninCommand* command) {
            completion = command.completion;
            return command.operation ==
                   AuthenticationOperation::kSheetSigninAndHistorySync;
          }]
                               browser:browser_.get()
                    baseViewController:base_view_controller_])
      .andReturn(signin_coordinator_mock_);
  OCMExpect([signin_coordinator_mock_ start]).andDo(^(NSInvocation*) {
    AcceptSyncOptIn();
    completion(signin_coordinator_mock_, SigninCoordinatorResultSuccess,
               [FakeSystemIdentity fakeIdentity1]);
  });
  OCMExpect([signin_coordinator_mock_ stop]);

  delegate_->ShowAuthenticationUi(FlowType::kJoin, mock_callback.Get());
}

// Tests `ShowAuthenticationUi` when the user sign in but don't sync.
TEST_F(IOSCollaborationControllerDelegateTest, ShowAuthenticationUiSyncDenied) {
  InitDelegate(FlowType::kJoin);
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      mock_callback;

  EXPECT_CALL(mock_callback,
              Run(CollaborationControllerDelegate::Outcome::kFailure));
  __block SigninCoordinatorCompletionCallback completion;
  OCMExpect(
      [signin_coordinator_class_mock_
          signinCoordinatorWithCommand:[OCMArg checkWithBlock:^BOOL(
                                                   ShowSigninCommand* command) {
            completion = command.completion;
            return command.operation ==
                   AuthenticationOperation::kSheetSigninAndHistorySync;
          }]
                               browser:browser_.get()
                    baseViewController:base_view_controller_])
      .andReturn(signin_coordinator_mock_);
  OCMExpect([signin_coordinator_mock_ start]).andDo(^(NSInvocation*) {
    DenySyncOptIn();
    completion(signin_coordinator_mock_, SigninCoordinatorResultSuccess,
               [FakeSystemIdentity fakeIdentity1]);
  });
  OCMExpect([signin_coordinator_mock_ stop]);
  delegate_->ShowAuthenticationUi(FlowType::kJoin, mock_callback.Get());
}

// Tests `ShowAuthenticationUi` when the user is signed-in.
TEST_F(IOSCollaborationControllerDelegateTest, ShowAuthenticationUiWithSignIn) {
  SignIn();
  InitDelegate(FlowType::kJoin);
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      mock_callback;

  EXPECT_CALL(mock_callback,
              Run(CollaborationControllerDelegate::Outcome::kSuccess));

  __block SigninCoordinatorCompletionCallback completion;
  OCMExpect(
      [signin_coordinator_class_mock_
          signinCoordinatorWithCommand:[OCMArg checkWithBlock:^BOOL(
                                                   ShowSigninCommand* command) {
            completion = command.completion;
            return command.operation == AuthenticationOperation::kHistorySync;
          }]
                               browser:browser_.get()
                    baseViewController:base_view_controller_])
      .andReturn(signin_coordinator_mock_);
  OCMExpect([signin_coordinator_mock_ start]).andDo(^(NSInvocation*) {
    AcceptSyncOptIn();
    completion(signin_coordinator_mock_, SigninCoordinatorResultSuccess,
               [FakeSystemIdentity fakeIdentity1]);
  });
  OCMExpect([signin_coordinator_mock_ stop]);
  delegate_->ShowAuthenticationUi(FlowType::kJoin, mock_callback.Get());
}

// Tests `NotifySignInAndSyncStatusChange`.
TEST_F(IOSCollaborationControllerDelegateTest,
       NotifySignInAndSyncStatusChange) {
  InitDelegate(FlowType::kShareOrManage);
  delegate_->NotifySignInAndSyncStatusChange();
}

// Tests that showing the join screen records the correct user actions.
TEST_F(IOSCollaborationControllerDelegateTest, JoinUMA) {
  base::UserActionTester user_action_tester;
  InitDelegate(FlowType::kJoin);
  EXPECT_EQ(1, user_action_tester.GetActionCount("IOSCollaborationInitJoin"));
  data_sharing::SharedDataPreview preview_data;
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      mock_callback;

  delegate_->ShowJoinDialog(data_sharing::GroupToken(), preview_data,
                            mock_callback.Get());

  EXPECT_EQ(
      1, user_action_tester.GetActionCount("IOSCollaborationShowJoinDialog"));
}

// Tests that showing the share screen records the correct user actions.
TEST_F(IOSCollaborationControllerDelegateTest, ShareUMA) {
  base::UserActionTester user_action_tester;
  InitDelegate(FlowType::kShareOrManage);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOSCollaborationInitShareOrManage"));
  // Prepare the callback.
  base::MockCallback<
      CollaborationControllerDelegate::ResultWithGroupTokenCallback>
      mock_callback;

  delegate_->ShowShareDialog(tab_group_->tab_group_id(), mock_callback.Get());

  EXPECT_EQ(
      1, user_action_tester.GetActionCount("IOSCollaborationShowShareDialog"));
}

// Tests that showing the manage screen records the correct user actions.
TEST_F(IOSCollaborationControllerDelegateTest, ManageUMA) {
  base::UserActionTester user_action_tester;
  InitDelegate(FlowType::kShareOrManage);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOSCollaborationInitShareOrManage"));
  // Share the group.
  tab_group_sync_service_->MakeTabGroupShared(
      tab_group_->tab_group_id(), syncer::CollaborationId("collaboration"),
      tab_groups::TabGroupSyncService::TabGroupSharingCallback());
  // Prepare the callback.
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      mock_callback;

  delegate_->ShowManageDialog(tab_group_->tab_group_id(), mock_callback.Get());

  EXPECT_EQ(
      1, user_action_tester.GetActionCount("IOSCollaborationShowManageDialog"));
}

// Tests that showing the leave screen records the correct user actions.
TEST_F(IOSCollaborationControllerDelegateTest, LeaveUMA) {
  base::UserActionTester user_action_tester;
  InitDelegate(FlowType::kLeaveOrDelete);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOSCollaborationInitLeaveOrDelete"));
  base::MockCallback<
      base::OnceCallback<void(CollaborationControllerDelegate::ResultCallback)>>
      leave_completion_callback;
  delegate_->SetLeaveOrDeleteConfirmationCallback(
      leave_completion_callback.Get());
  // Share the group.
  tab_group_sync_service_->MakeTabGroupShared(
      tab_group_->tab_group_id(), syncer::CollaborationId("collaboration"),
      tab_groups::TabGroupSyncService::TabGroupSharingCallback());
  // Prepare the callback.
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      mock_callback;

  delegate_->ShowLeaveDialog(tab_group_->tab_group_id(), mock_callback.Get());

  EXPECT_EQ(
      1, user_action_tester.GetActionCount("IOSCollaborationShowLeaveDialog"));
}

// Tests that showing the delete screen records the correct user actions.
TEST_F(IOSCollaborationControllerDelegateTest, DeleteUMA) {
  base::UserActionTester user_action_tester;
  InitDelegate(FlowType::kLeaveOrDelete);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOSCollaborationInitLeaveOrDelete"));
  base::MockCallback<
      base::OnceCallback<void(CollaborationControllerDelegate::ResultCallback)>>
      delete_completion_callback;
  delegate_->SetLeaveOrDeleteConfirmationCallback(
      delete_completion_callback.Get());
  // Share the group.
  tab_group_sync_service_->MakeTabGroupShared(
      tab_group_->tab_group_id(), syncer::CollaborationId("collaboration"),
      tab_groups::TabGroupSyncService::TabGroupSharingCallback());
  // Prepare the callback.
  base::MockCallback<CollaborationControllerDelegate::ResultCallback>
      mock_callback;

  delegate_->ShowDeleteDialog(tab_group_->tab_group_id(), mock_callback.Get());

  EXPECT_EQ(
      1, user_action_tester.GetActionCount("IOSCollaborationShowDeleteDialog"));
}

}  // namespace collaboration
