// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_flow_configuration.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_join_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_manage_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_share_group_configuration.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

namespace collaboration {

IOSCollaborationControllerDelegate::IOSCollaborationControllerDelegate(
    Browser* browser,
    UIViewController* base_view_controller,
    std::unique_ptr<CollaborationFlowConfiguration> collaboration_flow)
    : browser_(browser),
      base_view_controller_(base_view_controller),
      flow_config_(std::move(collaboration_flow)) {
  CHECK(browser_);
  CHECK(base_view_controller_);
  share_kit_service_ =
      ShareKitServiceFactory::GetForProfile(browser_->GetProfile());
  CHECK(share_kit_service_);
}

IOSCollaborationControllerDelegate::~IOSCollaborationControllerDelegate() {}

// CollaborationControllerDelegate.
void IOSCollaborationControllerDelegate::PrepareFlowUI(ResultCallback result) {
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::ShowError(const ErrorInfo& error,
                                                   ResultCallback result) {
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::Cancel(ResultCallback result) {
  if (session_id_) {
    share_kit_service_->CancelSession(session_id_);
  }
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

void IOSCollaborationControllerDelegate::ShowAuthenticationUi(
    ResultCallback result) {
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(browser_->GetProfile());
  AuthenticationOperation operation =
      identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)
          ? AuthenticationOperation::kHistorySync
          : AuthenticationOperation::kSheetSigninAndHistorySync;

  id<ApplicationCommands> application_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), ApplicationCommands);
  auto completion_block = base::CallbackToBlock(std::move(result));

  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:operation
               identity:nil
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_COLLABORATION_TAB_GROUP
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
             completion:^(SigninCoordinatorResult sign_in_result,
                          id<SystemIdentity> completion_info) {
               bool completion_result =
                   sign_in_result == SigninCoordinatorResultSuccess;
               CollaborationControllerDelegate::Outcome outcome =
                   completion_result
                       ? CollaborationControllerDelegate::Outcome::kSuccess
                       : CollaborationControllerDelegate::Outcome::kFailure;
               completion_block(outcome);
             }];

  [application_handler showSignin:command
               baseViewController:base_view_controller_];
}

void IOSCollaborationControllerDelegate::NotifySignInAndSyncStatusChange() {
  share_kit_service_->PrimaryAccountChanged();
}

void IOSCollaborationControllerDelegate::ShowJoinDialog(
    const data_sharing::GroupToken& token,
    const data_sharing::SharedDataPreview& preview_data,
    ResultCallback result) {
  CHECK_EQ(flow_config_->type(), CollaborationFlowConfiguration::Type::kJoin);
  const CollaborationFlowConfigurationJoin& join_flow =
      flow_config_->As<CollaborationFlowConfigurationJoin>();

  ShareKitJoinConfiguration* config = [[ShareKitJoinConfiguration alloc] init];
  config.URL = join_flow.url();
  config.baseViewController = base_view_controller_;
  auto completion_block = base::CallbackToBlock(std::move(result));
  config.completionBlock = ^(BOOL completion_result) {
    CollaborationControllerDelegate::Outcome outcome =
        completion_result ? CollaborationControllerDelegate::Outcome::kSuccess
                          : CollaborationControllerDelegate::Outcome::kFailure;
    completion_block(outcome);
  };

  session_id_ = share_kit_service_->JoinTabGroup(config);
}

void IOSCollaborationControllerDelegate::ShowShareDialog(
    ResultCallback result) {
  CHECK_EQ(flow_config_->type(),
           CollaborationFlowConfiguration::Type::kShareOrManage);
  const CollaborationFlowConfigurationShareOrManage& share_flow =
      flow_config_->As<CollaborationFlowConfigurationShareOrManage>();

  base::WeakPtr<const TabGroup> tab_group = share_flow.tab_group();
  if (!tab_group) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure);
    return;
  }

  ShareKitShareGroupConfiguration* config =
      [[ShareKitShareGroupConfiguration alloc] init];
  config.tabGroup = tab_group.get();
  config.baseViewController = base_view_controller_;
  config.applicationHandler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), ApplicationCommands);
  auto completion_block = base::CallbackToBlock(std::move(result));
  config.completionBlock = ^(BOOL completion_result) {
    CollaborationControllerDelegate::Outcome outcome =
        completion_result ? CollaborationControllerDelegate::Outcome::kSuccess
                          : CollaborationControllerDelegate::Outcome::kFailure;
    completion_block(outcome);
  };

  session_id_ = share_kit_service_->ShareTabGroup(config);
}

void IOSCollaborationControllerDelegate::ShowManageDialog(
    ResultCallback result) {
  CHECK_EQ(flow_config_->type(),
           CollaborationFlowConfiguration::Type::kShareOrManage);
  const CollaborationFlowConfigurationShareOrManage& manage_configuration =
      flow_config_->As<CollaborationFlowConfigurationShareOrManage>();

  base::WeakPtr<const TabGroup> tab_group = manage_configuration.tab_group();
  if (!tab_group) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure);
    return;
  }

  tab_groups::TabGroupSyncService* sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser_->GetProfile());
  tab_groups::CollaborationId collaboration_id =
      tab_groups::utils::GetTabGroupCollabID(tab_group.get(), sync_service);
  if (collaboration_id->empty()) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure);
    return;
  }

  ShareKitManageConfiguration* config =
      [[ShareKitManageConfiguration alloc] init];
  config.baseViewController = base_view_controller_;
  config.collabID = base::SysUTF8ToNSString(collaboration_id.value());
  config.applicationHandler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), ApplicationCommands);
  share_kit_service_->ManageTabGroup(config);
}

void IOSCollaborationControllerDelegate::PromoteTabGroup(
    ResultCallback result) {
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::PromoteCurrentScreen() {
  // TODO(crbug.com/377306986): Implement this.
}

}  // namespace collaboration
