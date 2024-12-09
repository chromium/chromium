// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_flow_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_join_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/share_kit/model/share_kit_share_group_configuration.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

namespace collaboration {

IOSCollaborationControllerDelegate::IOSCollaborationControllerDelegate(
    std::unique_ptr<CollaborationFlowConfiguration> collaboration_flow)
    : collaboration_flow_(std::move(collaboration_flow)) {}

IOSCollaborationControllerDelegate::~IOSCollaborationControllerDelegate() {}

// CollaborationControllerDelegate.
void IOSCollaborationControllerDelegate::PrepareFlowUI(ResultCallback result) {
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::ShowError(ResultCallback result,
                                                   const ErrorInfo& error) {
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::Cancel(ResultCallback result) {
  if (session_id_) {
    collaboration_flow_->share_kit_service()->CancelSession(session_id_);
  }
  std::move(result).Run(CollaborationControllerDelegate::Outcome::kSuccess);
}

void IOSCollaborationControllerDelegate::ShowAuthenticationUi(
    ResultCallback result) {
  id<ApplicationCommands> aplication_handler = HandlerForProtocol(
      collaboration_flow_->command_dispatcher(), ApplicationCommands);
  UIViewController* base_view_controller =
      collaboration_flow_->base_view_controller();
  auto completion_block = base::CallbackToBlock(std::move(result));

  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kSheetSigninAndHistorySync
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

  [aplication_handler showSignin:command
              baseViewController:base_view_controller];
}

void IOSCollaborationControllerDelegate::NotifySignInAndSyncStatusChange() {
  collaboration_flow_->share_kit_service()->PrimaryAccountChanged();
}

void IOSCollaborationControllerDelegate::ShowJoinDialog(
    data_sharing::SharedDataPreview preview_data,
    ResultCallback result) {
  CHECK_EQ(collaboration_flow_->type(),
           CollaborationFlowConfiguration::Type::kJoin);
  const CollaborationFlowConfigurationJoin& join_flow =
      collaboration_flow_->As<CollaborationFlowConfigurationJoin>();

  ShareKitJoinConfiguration* config = [[ShareKitJoinConfiguration alloc] init];
  config.URL = join_flow.url();
  config.baseViewController = join_flow.base_view_controller();
  auto completion_block = base::CallbackToBlock(std::move(result));
  config.completionBlock = ^(BOOL completion_result) {
    CollaborationControllerDelegate::Outcome outcome =
        completion_result ? CollaborationControllerDelegate::Outcome::kSuccess
                          : CollaborationControllerDelegate::Outcome::kFailure;
    completion_block(outcome);
  };

  session_id_ = join_flow.share_kit_service()->JoinTabGroup(config);
}

void IOSCollaborationControllerDelegate::ShowShareDialog(
    ResultCallback result) {
  CHECK_EQ(collaboration_flow_->type(),
           CollaborationFlowConfiguration::Type::kShare);
  const CollaborationFlowConfigurationShare& share_flow =
      collaboration_flow_->As<CollaborationFlowConfigurationShare>();

  if (!share_flow.tab_group()) {
    std::move(result).Run(CollaborationControllerDelegate::Outcome::kFailure);
    return;
  }

  ShareKitShareGroupConfiguration* config =
      [[ShareKitShareGroupConfiguration alloc] init];
  config.tabGroup = share_flow.tab_group().get();
  config.baseViewController = share_flow.base_view_controller();
  config.applicationHandler =
      HandlerForProtocol(share_flow.command_dispatcher(), ApplicationCommands);
  auto completion_block = base::CallbackToBlock(std::move(result));
  config.completionBlock = ^(BOOL completion_result) {
    CollaborationControllerDelegate::Outcome outcome =
        completion_result ? CollaborationControllerDelegate::Outcome::kSuccess
                          : CollaborationControllerDelegate::Outcome::kFailure;
    completion_block(outcome);
  };

  session_id_ = share_flow.share_kit_service()->ShareTabGroup(config);
}

void IOSCollaborationControllerDelegate::PromoteTabGroup(
    ResultCallback result) {
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::PromoteCurrentScreen() {
  // TODO(crbug.com/377306986): Implement this.
}

}  // namespace collaboration
